#
# SPDX-FileCopyrightText: 2026 Gursu Bilisim
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/samsung/jackpotlte

# Read by exynos7885-common.mk (Gursu-Bilisim fork) while it is inherited:
# Trustonic TEE instead of TEEGRIS, and rootdir files from this tree.
TARGET_EXYNOS7885_TEE := mobicore
TARGET_EXYNOS7885_DEVICE_ROOTDIR := true

# Files the common tree or common vendor also copy. PRODUCT_COPY_FILES
# keeps the first entry for a destination, and values added after an
# inherit come after the inherited ones, so these stay above the inherit
# lines (the common a30 init.baseband.rc uses lower case by-name paths).
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/init/init.baseband.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.baseband.rc \
    $(DEVICE_PATH)/configs/init/init.device.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.device.rc \
    $(DEVICE_PATH)/configs/media/media_profiles_V1_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml

# Inherit proprietary files setup. Listed before the common tree so that
# for files both copy (audio firmware, configs) the jackpotlte copy wins.
# Optional only so that kernel and boot image can be built before the blobs
# are extracted; a full build without them is not usable.
ifeq ($(wildcard vendor/samsung/jackpotlte/jackpotlte-vendor.mk),)
$(warning vendor/samsung/jackpotlte is missing: run device/samsung/jackpotlte/extract-files.sh)
endif
$(call inherit-product-if-exists, vendor/samsung/jackpotlte/jackpotlte-vendor.mk)

# Inherit common device configuration
$(call inherit-product, device/samsung/exynos7885-common/exynos7885-common.mk)

# Inherit dalvik config (4 GB RAM)
$(call inherit-product, frameworks/native/build/phone-xhdpi-4096-dalvik-heap.mk)

# Target Info
TARGET_DEVICE := jackpotlte
TARGET_SOC := exynos7885

# Display: 1080x2220, stock ro.sf.lcd_density=480
TARGET_SCREEN_DENSITY := 480
TARGET_SCREEN_HEIGHT := 2220
TARGET_SCREEN_WIDTH := 1080

# Fingerprint
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint-service.samsung

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml \
    $(DEVICE_PATH)/configs/keylayout/uinput-sec-fp.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/uinput-sec-fp.kl

PRODUCT_PACKAGES += \
    fstab.exynos7885 \
    init.exynos7885.rc \
    init.exynos7885.usb.rc \
    ueventd.jackpotlte.rc

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/rootdir/etc/fstab.samsungexynos7885:$(TARGET_COPY_OUT_RAMDISK)/vendor/etc/fstab.samsungexynos7885

# Keymaster 3.0 (loads keystore.mdfpp.so from the vendor blobs)
PRODUCT_PACKAGES += \
    android.hardware.keymaster@3.0-impl \
    android.hardware.keymaster@3.0-service

# Runtime dependencies of the 32-bit prebuilts that nothing else
# installs (camera, audio and keymaster HALs from 18.1 and CUK6)
PRODUCT_PACKAGES += \
    android.hardware.keymaster@4.0.vendor:32 \
    libcamera_metadata_helper:32 \
    libexynosgscaler:32 \
    libgui_vendor:32 \
    libhidltransport.vendor:32 \
    libhwbinder.vendor:32 \
    libkeymaster4support.vendor:32 \
    libtinycompress:32

# NFC (Samsung S3NRN81)
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/nfc/libnfc-sec-vendor.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-sec-vendor.conf

# Overlays
DEVICE_PACKAGE_OVERLAYS += $(DEVICE_PATH)/overlay

PRODUCT_PACKAGES += \
    WifiOverlayA8

# Shipping API level (same choice as the LineageOS 18.1 jackpotlte tree;
# selects the treble MobiCore sepolicy)
$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_p.mk)

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += $(DEVICE_PATH)
