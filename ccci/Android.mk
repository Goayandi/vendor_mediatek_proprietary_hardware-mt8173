LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifdef MTK_MD_SBP_CUSTOM_VALUE
ifneq ($(MTK_MD_SBP_CUSTOM_VALUE), "")
LOCAL_CFLAGS += -DMD_SBP_CUSTOM_VALUE=$(subst ",,$(MTK_MD_SBP_CUSTOM_VALUE))
endif
endif

ifdef MTK_MD2_SBP_CUSTOM_VALUE
ifneq ($(MTK_MD2_SBP_CUSTOM_VALUE), "")
LOCAL_CFLAGS += -DMD2_SBP_CUSTOM_VALUE=$(subst ",,$(MTK_MD2_SBP_CUSTOM_VALUE))
endif
endif

ifeq ($(MTK_ECCCI_C2K),yes)
LOCAL_CFLAGS += -DMTK_ECCCI_C2K
endif

LOCAL_SRC_FILES := ccci_lib.c

LOCAL_STATIC_LIBRARIES := libcutils liblog

LOCAL_C_INCLUDES := \
	$(MTK_PATH_SOURCE)/external/nvram/libnvram \
	$(MTK_PATH_SOURCE)/hardware/ccci/include

LOCAL_SHARED_LIBRARIES := libc

LOCAL_MODULE := libccci_util
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := both

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE:=false

include $(MTK_SHARED_LIBRARY)
