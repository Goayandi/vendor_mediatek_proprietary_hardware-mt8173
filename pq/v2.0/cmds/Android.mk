LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    main_pq.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libdpframework \
    libbinder \
    libdl \
    libpqservice

LOCAL_C_INCLUDES += \
    $(TOP)/frameworks/base/include \
    $(MTK_PATH_SOURCE)/hardware/pq/v2.0/include \
    $(TOP)/$(MTK_PATH_SOURCE)/platform/$(MTK_PLATFORM_DIR)/kernel/drivers/dispsys \
    $(TOP)/$(MTK_ROOT)/frameworks-ext/native/include

LOCAL_MODULE:= pq
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_CLASS := EXECUTABLES

include $(MTK_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
