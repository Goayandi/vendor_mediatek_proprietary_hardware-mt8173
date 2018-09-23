LOCAL_PATH := $(call my-dir)

########################################
# Build MTK FLP JNI shared library
########################################
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware

LOCAL_CFLAGS:= -DFLP_HARDWARE_SUPPORT
LOCAL_LDLIBS:= -llog
LOCAL_SRC_FILES :=  mtk_flp_hal_common.c

LOCAL_MODULE := flp.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := 64
LOCAL_MODULE_TAGS := optional

include $(MTK_SHARED_LIBRARY)
