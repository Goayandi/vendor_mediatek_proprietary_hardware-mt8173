# --------------------------------------------------------------------
# configuration files for AOSP wpa_supplicant_8
# --------------------------------------------------------------------
ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_8_X)
WPA_SUPPLICANT_BUILD := yes
endif
ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_8_X_MTK)
WPA_SUPPLICANT_BUILD := yes
endif

ifeq ($(WPA_SUPPLICANT_BUILD), yes)
########################

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := wpa_supplicant.conf
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/wifi
LOCAL_SRC_FILES := mtk-wpa_supplicant.conf
include $(BUILD_PREBUILT)

#################Add overlay file################
include $(CLEAR_VARS)
LOCAL_MODULE := wpa_supplicant_overlay.conf
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/wifi
ifeq ($(findstring OP01, $(strip $(OPTR_SPEC_SEG_DEF))),OP01)
LOCAL_SRC_FILES := cmcc-wpa_supplicant-overlay.conf
else
LOCAL_SRC_FILES := mtk-wpa_supplicant-overlay.conf
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := p2p_supplicant_overlay.conf
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/wifi
LOCAL_SRC_FILES := mtk-p2p_wpa_supplicant-overlay.conf
include $(BUILD_PREBUILT)
endif #ifeq($(WPA_SUPPLICANT_VERSION),VER_0_8_X)
