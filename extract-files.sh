#!/bin/bash
#
# SPDX-FileCopyrightText: 2016 The CyanogenMod Project
# SPDX-FileCopyrightText: 2017-2025 The LineageOS Project
# SPDX-FileCopyrightText: 2026 Gursu Bilisim
# SPDX-License-Identifier: Apache-2.0
#
# Extracts only the jackpotlte blobs into vendor/samsung/jackpotlte.
# vendor/samsung/exynos7885-common is not touched (its blobs come from
# a30 Android 11 firmware, not from this phone).
#
# Source: an unpacked stock A530FXXULCUK6 system image, whose vendor files
# sit under system/vendor (see README for how to get there), e.g.
#   ./extract-files.sh <path to the unpacked system image root>
#

set -e

DEVICE=jackpotlte
VENDOR=samsung

MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."

HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

CLEAN_VENDOR=true
KANG=
SECTION=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n | --no-cleanup )
                CLEAN_VENDOR=false
                ;;
        -k | --kang )
                KANG="--kang"
                ;;
        -s | --section )
                SECTION="${2}"; shift
                CLEAN_VENDOR=false
                ;;
        * )
                SRC="${1}"
                ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    echo "Usage: ${0} <path to the unpacked system image root>"
    exit 1
fi

function blob_fixup() {
    case "${1}" in
        lib/hw/audio.primary.universal7885.so | vendor/lib/hw/audio.primary.exynos7885.so)
            # Stock libaudioroute under its own name; tinyalsa from the common vendor
            "${PATCHELF}" --replace-needed libaudioroute.so libaudioroute.universal7885.so "${2}"
            "${PATCHELF}" --replace-needed libtinyalsa.so libtinyalsa.exynos7885.so "${2}"
            # set_sched_policy moved from libcutils to libprocessgroup after Android 9
            "${PATCHELF}" --add-needed libprocessgroup.so "${2}"
            ;;
        lib/libaudioroute.so | vendor/lib/libaudioroute.universal7885.so)
            # Same length paths; the stock build kept the mixer files in /system/etc
            sed -i 's|/system/etc/mixer_paths.xml|/vendor/etc/mixer_paths.xml|g' "${2}"
            sed -i 's|/system/etc/mixer_gains.xml|/vendor/etc/mixer_gains.xml|g' "${2}"
            "${PATCHELF}" --set-soname libaudioroute.universal7885.so "${2}"
            "${PATCHELF}" --replace-needed libtinyalsa.so libtinyalsa.exynos7885.so "${2}"
            ;;
        lib/libaboxpcmdump.so | vendor/lib/libaboxpcmdump.so)
            sed -i 's|/system/etc/abox_debug.xml|/vendor/etc/abox_debug.xml|g' "${2}"
            "${PATCHELF}" --replace-needed libtinyalsa.so libtinyalsa.exynos7885.so "${2}"
            ;;
        vendor/lib/libwvhidl.so | vendor/lib/mediadrm/libwvdrmengine.so)
            # Same shim the common (a30) Widevine blobs get
            "${PATCHELF}" --add-needed libcrypto_shim.so "${2}"
            # Android 9 VNDK protobuf to the 3.9.1 build (universal7880 and
            # universal8890 move these blobs to a 3.9.1 build as well)
            "${PATCHELF}" --replace-needed libprotobuf-cpp-lite.so libprotobuf-cpp-lite-3.9.1.so "${2}"
            ;;
    esac
}

setup_vendor "${DEVICE}" "${VENDOR}" "${ANDROID_ROOT}" false "${CLEAN_VENDOR}"

extract "${MY_DIR}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"

"${MY_DIR}/setup-makefiles.sh"
