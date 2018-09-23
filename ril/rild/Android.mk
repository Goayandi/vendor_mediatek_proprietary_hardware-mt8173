#
# Copyright (C) 2014 MediaTek Inc.
#
# Modification based on code covered by the below mentioned copyright
# and/or permission notice(s).
#

# Copyright 2014 The Android Open Source Project


ifneq ($(GOOGLE_RELEASE_RIL), yes)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    rild.c

LOCAL_SHARED_LIBRARIES := \
  liblog \
  libcutils \
  librilmtk \
  libdl

LOCAL_STATIC_LIBRARIES := \
    rild-prop-md1

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include

#ifeq ($(TARGET_ARCH),arm)
    LOCAL_SHARED_LIBRARIES += libdl
#endif # arm

LOCAL_CFLAGS := -DRIL_SHLIB -DMTK_RIL

ifneq ($(MTK_NUM_MODEM_PROTOCOL),1)
    LOCAL_CFLAGS += -DMTK_GEMINI -DANDROID_MULTI_SIM
endif

ifeq ($(MTK_NUM_MODEM_PROTOCOL), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

ifeq ($(MTK_NUM_MODEM_PROTOCOL), 3)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_3
endif

ifeq ($(MTK_NUM_MODEM_PROTOCOL), 4)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_4
endif

LOCAL_CFLAGS += -DMTK_RIL_MD1

LOCAL_MODULE:= mtkrild
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_INIT_RC := rild.rc

include $(MTK_EXECUTABLE)


# For radiooptions binary
# =======================
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        radiooptions.c

LOCAL_SHARED_LIBRARIES := \
        liblog \
        libcutils \

LOCAL_CFLAGS := \

LOCAL_MODULE:= mtkradiooptions
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_TAGS := debug

include $(MTK_EXECUTABLE)

endif # GOOGLE_RELEASE_RIL
