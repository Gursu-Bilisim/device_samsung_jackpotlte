#
# SPDX-FileCopyrightText: 2026 Gursu Bilisim
# SPDX-License-Identifier: Apache-2.0
#

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/non_ab_device.mk)

# Inherit common Lineage phone (vendor/lineage is BaikalOS' vendor_baikalos)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit device configuration
$(call inherit-product, device/samsung/jackpotlte/device.mk)

PRODUCT_DEVICE := jackpotlte
PRODUCT_NAME := lineage_jackpotlte
PRODUCT_MODEL := SM-A530F
PRODUCT_BRAND := samsung
PRODUCT_MANUFACTURER := samsung
PRODUCT_GMS_CLIENTID_BASE := android-samsung

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="jackpotltexx-user 9 PPR1.180610.011 A530FXXULCUK6 release-keys" \
    BuildFingerprint=samsung/jackpotltexx/jackpotlte:9/PPR1.180610.011/A530FXXULCUK6:user/release-keys
