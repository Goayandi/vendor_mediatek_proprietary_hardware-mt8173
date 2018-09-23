LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

-include $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk
LOCAL_CFLAGS += $(MTKCAM_CFLAGS)

ifneq ($(strip $(MTKCAM_HAVE_MFB_SUPPORT)),0)

LOCAL_MODULE := libcam.camadapter.scenario.shot.mfllshot
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

MTK_PATH_CAM := $(MTK_PATH_SOURCE)/hardware/mtkcam

LOCAL_SRC_FILES := MfllShot.cpp
LOCAL_SRC_FILES += MfllShotEng.cpp

LOCAL_C_INCLUDES := \
	system/media/camera/include \
	$(MTKCAM_C_INCLUDES) \
    $(MTK_PATH_SOURCE)/hardware/mtkcam/include \
    $(LOCAL_PATH)/../inc \
    $(LOCAL_PATH)/../../../inc/Scenario/Shot \
	$(MTK_PATH_CAM) \
	$(MTK_PATH_CAM)/include/$(PLATFORM) \
	$(MTK_PATH_SOURCE)/hardware/mtkcam/feature/include \
	$(MTK_PATH_SOURCE)/hardware/gralloc_extra/include \
	$(MTK_PATH_SOURCE)/custom/$(TARGET_BOARD_PLATFORM)/hal/inc/isp_tuning \
	$(LOCAL_PATH)/../EngShot/

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"mfllshot\"
LOCAL_CFLAGS += $(MTKCAM_CFLAGS)

LOCAL_CFLAGS += -DUSE_SYSTRACE

include $(MTK_STATIC_LIBRARY)

endif # MTK_CAM_MFB_SUPPORT
