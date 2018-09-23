#
# ANT Test
#
ifeq ($(MTK_ANT_SUPPORT), yes)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= $(LOCAL_PATH)/../src/inc

LOCAL_SRC_FILES := ant_app.c

LOCAL_MODULE:=antradio_app
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_SHARED_LIBRARIES := \
	libantradio \
	libcutils

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := optional

include $(MTK_EXECUTABLE)

endif

