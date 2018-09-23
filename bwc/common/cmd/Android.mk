LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main_bwc.cpp 

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    liblog \
    libbwc \

LOCAL_C_INCLUDES := \
    $(MTK_PATH_SOURCE)/hardware/bwc/inc \
    $(TOP)/$(MTK_PATH_PLATFORM)/hardware/bwc/inc
  
LOCAL_MODULE:= bwc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_TAGS := optional

include $(MTK_EXECUTABLE)
