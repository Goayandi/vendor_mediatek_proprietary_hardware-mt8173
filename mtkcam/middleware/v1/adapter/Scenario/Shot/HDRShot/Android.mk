LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(MTK_CAM_HDR_SUPPORT), yes)

-include $(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk

LOCAL_MODULE := libcam.camadapter.scenario.shot.hdrshot
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

MTK_PATH_CAM := $(MTK_PATH_SOURCE)/hardware/mtkcam

LOCAL_SRC_FILES := \
	HDRShot.cpp \
	VHDRShot.cpp \
	utils/WorkerThread.cpp

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
	$(MTK_PATH_SOURCE)/custom/$(TARGET_BOARD_PLATFORM)/hal/inc/isp_tuning

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"hdrshot\"
LOCAL_CFLAGS += $(MTKCAM_CFLAGS)
LOCAL_CFLAGS += -DUSE_SYSTRACE

include $(MTK_STATIC_LIBRARY)

endif # MTK_CAM_HDR_SUPPORT
