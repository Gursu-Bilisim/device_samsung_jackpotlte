#
# SPDX-FileCopyrightText: 2026 Gursu Bilisim
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/samsung/jackpotlte

# TEE: jackpotlte uses Trustonic (MobiCore), not TEEGRIS.
# Must be set before the common include, which reads it immediately.
BOARD_SEPOLICY_TEE_FLAVOR := mobicore

include device/samsung/exynos7885-common/BoardConfigCommon.mk

# Assert
TARGET_OTA_ASSERT_DEVICE := jackpotlte

# Boot image
# The bootloader (A530FXXULCUK6) reads a v0 header with a Samsung DTBH
# device tree image in the dt field, like the stock boot.img. There is no
# DTB or DTBO partition, so header v1 and recovery DTBO are not used.
BOARD_KERNEL_SEPARATED_DTBO :=
BOARD_DTBO_CFG :=
BOARD_INCLUDE_RECOVERY_DTBO :=
BOARD_DTBIMG_PARTITION_SIZE :=
BOARD_DTBOIMG_PARTITION_SIZE :=
BOARD_KERNEL_SEPARATED_DT := true
BOARD_CUSTOM_BOOTIMG := true
BOARD_CUSTOM_BOOTIMG_MK := $(DEVICE_PATH)/bootimg.mk
BOARD_MKBOOTIMG_ARGS := --base $(BOARD_KERNEL_BASE)
BOARD_MKBOOTIMG_ARGS += --pagesize $(BOARD_KERNEL_PAGESIZE)
BOARD_MKBOOTIMG_ARGS += --kernel_offset $(BOARD_KERNEL_OFFSET)
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset $(BOARD_RAMDISK_OFFSET)
BOARD_MKBOOTIMG_ARGS += --tags_offset $(BOARD_TAGS_OFFSET)

# Kernel: crDroid 15.0 (Eureka R24U) with the jackpotlte defconfig
TARGET_KERNEL_CONFIG := exynos7885-jackpotlte_defconfig
TARGET_KERNEL_ADDITIONAL_FLAGS += HOSTCFLAGS="-fuse-ld=lld -Wno-unused-command-line-argument"
BOARD_KERNEL_CMDLINE := androidboot.init_fatal_reboot_target=recovery

# Manifest: keymaster 3.0 (Trustonic) instead of the common keymaster 4.0
DEVICE_MANIFEST_FILE := $(filter-out $(COMMON_PATH)/manifest.xml,$(DEVICE_MANIFEST_FILE))
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/manifest.xml

# Partitions (bytes, Treble layout on this phone)
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 39845888
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 6081740800
BOARD_VENDORIMAGE_PARTITION_SIZE := 943718400
BOARD_CACHEIMAGE_PARTITION_SIZE := 209715200

# Properties
TARGET_VENDOR_PROP := $(filter-out $(COMMON_PATH)/vendor.prop,$(TARGET_VENDOR_PROP))
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.samsungexynos7885

# Releasetools: the common script writes dtb.img and dtbo.img, which this
# device does not have.
TARGET_RELEASETOOLS_EXTENSIONS :=

# Security patch of the vendor blobs (A530FXXULCUK6)
VENDOR_SECURITY_PATCH := 2021-11-01

# Sepolicy
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor
