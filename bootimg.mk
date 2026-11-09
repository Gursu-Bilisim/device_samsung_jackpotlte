#
# SPDX-FileCopyrightText: 2017 The LineageOS Project
# SPDX-FileCopyrightText: 2026 Gursu Bilisim
# SPDX-License-Identifier: Apache-2.0
#
# Boot and recovery images for jackpotlte: header v0 with a Samsung DTBH
# image in the dt field and the SEANDROIDENFORCE trailer, as in the stock
# A530FXXULCUK6 boot.img. Based on hardware/samsung/mkbootimg.mk; the dt
# image is made by mkdtbh.py from the EUR revision 03-07 device trees only.
#

JACKPOTLTE_DTBH := $(DEVICE_PATH)/mkdtbh.py
JACKPOTLTE_DTB_DIR := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/arch/$(KERNEL_ARCH)/boot/dts/exynos/dtb
JACKPOTLTE_DTBS := $(foreach r,03 04 05 06 07,$(JACKPOTLTE_DTB_DIR)/exynos7885-jackpotlte_eur_open_$(r).dtb)

$(INSTALLED_DTIMAGE_TARGET): $(JACKPOTLTE_DTBH) $(INSTALLED_KERNEL_TARGET)
	$(call pretty,"Target dt image: $@")
	$(hide) python3 $(JACKPOTLTE_DTBH) -s $(BOARD_KERNEL_PAGESIZE) -o $@ $(JACKPOTLTE_DTBS)

.PHONY: dtimage
dtimage: $(INSTALLED_DTIMAGE_TARGET)

$(INSTALLED_BOOTIMAGE_TARGET): $(MKBOOTIMG) $(INTERNAL_BOOTIMAGE_FILES) $(BOOTIMAGE_EXTRA_DEPS) $(INSTALLED_KERNEL_TARGET)
	$(call pretty,"Target boot image: $@")
	$(hide) $(MKBOOTIMG) --kernel $(call bootimage-to-kernel,$(1)) $(INTERNAL_BOOTIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) $(BOARD_MKBOOTIMG_ARGS) --output $@
	$(hide) echo -n "SEANDROIDENFORCE" >> $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE),raw)
	@echo "Made boot image: $@"

$(INSTALLED_RECOVERYIMAGE_TARGET): $(MKBOOTIMG) $(recovery_ramdisk) $(recovery_kernel) $(RECOVERYIMAGE_EXTRA_DEPS)
	@echo "----- Making recovery image ------"
	$(hide) $(MKBOOTIMG) $(INTERNAL_RECOVERYIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) $(BOARD_MKBOOTIMG_ARGS) --output $@
	$(hide) echo -n "SEANDROIDENFORCE" >> $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE),raw)
	@echo "Made recovery image: $@"
