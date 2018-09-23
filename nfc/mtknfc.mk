ifeq ($(strip $(MTK_NFC_SUPPORT)), yes)

########################################
# MTK NFC Package Configuration
# MTK_NFC_PACKAGE = MTK , MTK hal (for TK) 
# MTK_NFC_PACKAGE = AOSP_B , AOSP hal (for Basic,Bsp)
########################################
ifeq ($(strip $(MTK_NFC_PACKAGE)), AOSP_B)
    LOCAL_PATH:= vendor/mediatek/proprietary/hardware/nfc
else
    LOCAL_PATH:= vendor/mediatek/proprietary/external/mtknfc
endif

########################################
# MTK NFC Clock Type & Rate Configuration
########################################

ifeq ($(wildcard device/mediatek/$(MTK_TARGET_PROJECT)/nfc.cfg),)
    PRODUCT_COPY_FILES += $(LOCAL_PATH)/nfc.cfg:system/etc/nfc.cfg
else
    PRODUCT_COPY_FILES += device/mediatek/$(MTK_TARGET_PROJECT)/nfc.cfg:system/etc/nfc.cfg
endif

ifneq  ($(wildcard device/mediatek/$(MTK_TARGET_PROJECT)/nfcbooster.cfg),)
    PRODUCT_COPY_FILES += device/mediatek/$(MTK_TARGET_PROJECT)/nfcbooster.cfg:system/etc/nfcbooster.cfg	
endif

#Copy Mifare lincense file
PRODUCT_COPY_FILES += $(LOCAL_PATH)/MTKNfclicense.lic:$(TARGET_COPY_OUT_VENDOR)/etc/MTKNfclicense.lic:mtk

ifeq ($(MTK_NFC_PACKAGE), AOSP_B)
    PRODUCT_PACKAGES += nfc_nci.mt6605.default
    PRODUCT_PACKAGES += NfcNci
    PRODUCT_PACKAGES += libmtknfc
    PRODUCT_PACKAGES += libnfc_nci_jni
    PRODUCT_PACKAGES += libnfc-nci
    ifeq ($(strip $(MTK_BSP_PACKAGE)), yes)
        PRODUCT_PACKAGES += nfcstackp
    endif
# NFC configure file
    PRODUCT_COPY_FILES += $(LOCAL_PATH)/nfcse.cfg:system/etc/nfcse.cfg
    PRODUCT_COPY_FILES += $(LOCAL_PATH)/halimpl/libnfc-brcm.conf:system/etc/libnfc-brcm.conf

endif

ifeq ($(MTK_NFC_PACKAGE), MTK)
      PRODUCT_PACKAGES += nfcstackp
      PRODUCT_PACKAGES += DeviceTestApp
      PRODUCT_PACKAGES += libdta_mt6605_jni
      PRODUCT_PACKAGES += libmtknfc_dynamic_load_jni
      PRODUCT_PACKAGES += libnfc_mt6605_jni
      $(call inherit-product-if-exists, vendor/mediatek/proprietary/packages/apps/DeviceTestApp/DeviceTestApp.mk)

endif

endif  # MTK_NFC_SUPPORT

