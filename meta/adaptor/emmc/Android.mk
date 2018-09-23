ifeq ($(MTK_EMMC_SUPPORT), yes)
##### EMMC META library #####
BUILD_CLR_EMMC := true

ifeq ($(BUILD_CLR_EMMC), true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := meta_clr_emmc.c
LOCAL_C_INCLUDES := $(MTK_PATH_SOURCE)/hardware/meta/common/inc \
                    $(MTK_PATH_SOURCE)/hardware/meta/adaptor/storageutil
LOCAL_SHARED_LIBRARIES := libcutils libc
LOCAL_STATIC_LIBRARIES := libstorageutil
LOCAL_CFLAGS += -DUSE_EXT4
LOCAL_C_INCLUDES += system/extras/ext4_utils
LOCAL_C_INCLUDES += system/core/fs_mgr/include
LOCAL_C_INCLUDES += external/libselinux/include
LOCAL_STATIC_LIBRARIES += libext4_utils_static libz libft
LOCAL_STATIC_LIBRARIES += libfs_mgr
LOCAL_MODULE := libmeta_clr_emmc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_PRELINK_MODULE := false
include $(MTK_STATIC_LIBRARY)
endif
endif

ifeq ($(MTK_EMMC_SUPPORT), no)
##### NAND META library #####
BUILD_CLR_NAND := true

ifeq ($(BUILD_CLR_NAND), true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := meta_clr_nand.c
LOCAL_C_INCLUDES := $(MTK_PATH_SOURCE)/hardware/meta/common/inc \
                    $(MTK_PATH_SOURCE)/hardware/meta/adaptor/storageutil
LOCAL_SHARED_LIBRARIES := libcutils libc
LOCAL_STATIC_LIBRARIES := libstorageutil libft
LOCAL_MODULE := libmeta_clr_emmc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_PRELINK_MODULE := false
include $(MTK_STATIC_LIBRARY)
endif
endif
