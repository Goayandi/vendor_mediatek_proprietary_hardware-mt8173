LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    cust_aal_main.cpp

LOCAL_STATIC_LIBRARIES := \
    libaal_config

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdpframework

LC_MTK_PLATFORM := $(shell echo $(MTK_PLATFORM) | tr A-Z a-z )

LOCAL_C_INCLUDES := \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/aal/include \
    $(TOP)/$(MTK_PATH_SOURCE)/platform/$(LC_MTK_PLATFORM)/kernel/drivers/dispsys

ifeq ($(strip $(MTK_ULTRA_DIMMING_SUPPORT)),yes)
    LOCAL_CFLAGS += -DMTK_ULTRA_DIMMING_SUPPORT
endif

LOCAL_MODULE:= libaal_cust
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(MTK_SHARED_LIBRARY)
