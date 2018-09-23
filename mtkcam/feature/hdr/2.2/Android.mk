LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MTK_PATH_GLOBAL_HDR_PLATFORM := $(LOCAL_PATH)/../$(TARGET_BOARD_PLATFORM)
MTK_PATH_LOCAL_HDR_PLATFORM := ../$(TARGET_BOARD_PLATFORM)

$(info MTK HDR platform path is $(MTK_PATH_GLOBAL_HDR_PLATFORM))

ifeq (,$(wildcard $(MTK_PATH_GLOBAL_HDR_PLATFORM)))
# Skeleton for the platform dependent implementation.
# They work as *stubs* for yet-to-be-developed code,
# and are most useful in porting stage of a new platform.
$(info use default platform parts)
MTK_PATH_LOCAL_HDR_PLATFORM := default
else
$(info use real platform parts)
endif

ifeq ($(MTK_CAM_HDR_SUPPORT), yes)

ifeq ($(TARGET_BOARD_PLATFORM), $(filter $(TARGET_BOARD_PLATFORM), kiboplus mt6757 mt6797))

-include $(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk

LOCAL_MODULE := libhdrproc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

MTK_PATH_CAM := $(MTK_PATH_SOURCE)/hardware/mtkcam

LOCAL_SRC_FILES := \
	HDRProc2.cpp \
	HDRAlgo.cpp \
	HDRUtils.cpp \
	hal/HDRHAL.cpp \
	$(MTK_PATH_LOCAL_HDR_PLATFORM)/Platform.cpp

LOCAL_C_INCLUDES := \
	system/media/camera/include \
	$(MTK_PATH_CAM) \
	$(MTK_PATH_SOURCE)/hardware/mtkcam/include \
	$(MTK_PATH_CUSTOM_PLATFORM)/hal/inc \
	$(MTK_PATH_CUSTOM_PLATFORM)/hal/inc/isp_tuning \
	$(MTK_PATH_CAM)/include/algorithm/$(TARGET_BOARD_PLATFORM) \
	$(MTK_PATH_LOCAL_HDR_PLATFORM)

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libcutils \
	libcamalgo \
	libcam.feature_utils \
	libmtkcam_stdutils \
	libcam.iopipe \
	libcameracustom \
	libmtkcam_modulehelper \

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"hdrproc\"
LOCAL_CFLAGS += $(MTKCAM_CFLAGS)
LOCAL_CFLAGS += -DUSE_SYSTRACE

# workaroud to judge whether libperfservicenative exists nor not
# remove this condition after MTK_PERFSERVICE_SUPPORT works as expected
ifneq ($(MTK_BASIC_PACKAGE), yes)
LOCAL_CFLAGS += -DUSE_PERFSERVICE
LOCAL_CFLAGS += -DUSE_AFFINITY
LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/perfservice
LOCAL_SHARED_LIBRARIES += libperfservicenative
endif

include $(MTK_SHARED_LIBRARY)

endif # TARGET_BOARD_PLATFORM

endif # MTK_CAM_HDR_SUPPORT
