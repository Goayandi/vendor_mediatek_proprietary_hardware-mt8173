# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.
#
# MediaTek Inc. (C) 2010. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
# THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
# CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
# SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
# CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
#
# The following software/firmware and/or related documentation ("MediaTek Software")
# have been modified by MediaTek Inc. All revisions are subject to any receiver's
# applicable license agreements with MediaTek Inc.
#===============================================================================


LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

#-----------------------------------------------------------
-include $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk
LOCAL_CFLAGS += $(MTKCAM_CFLAGS)
#-----------------------------------------------------------

ifeq ($(BUILD_MTK_LDVT),true)
    LOCAL_CFLAGS += -DUSING_MTK_LDVT
endif

LOCAL_SRC_FILES += \
    autorama_hal_base.cpp \
    autorama_hal.cpp \
    utility_hal_base.cpp \
    utility_hal.cpp \



LOCAL_C_INCLUDES:= \
    $(TOP)/$(MTKCAM_ALGO_INCLUDE)/libmotion \
    $(TOP)/$(MTKCAM_ALGO_INCLUDE)/libautopano \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/aaa/include \
    $(TOP)/$(MTK_PATH_CUSTOM_PLATFORM)/hal/inc/isp_tuning \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/include \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/include/mtkcam/feature/autorama \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/jpeg/include \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/gralloc_extra/include \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/m4u/$(shell echo $(MTK_PLATFORM) | tr A-Z a-z ) \


LOCAL_STATIC_LIBRARIES := \

LOCAL_WHOLE_STATIC_LIBRARIES := \

LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libcamalgo
LOCAL_SHARED_LIBRARIES += libcamera_client
LOCAL_SHARED_LIBRARIES += libmtkcam_modulehelper
LOCAL_SHARED_LIBRARIES += libmtkcam_stdutils
LOCAL_SHARED_LIBRARIES += libmtkcam_exif
LOCAL_SHARED_LIBRARIES += libJpgEncPipe

LOCAL_MODULE := libcam.featureio.pipe.panorama
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

#include $(MTK_STATIC_LIBRARY)

SUPPORTED_PLATFORM=mt6797 mt6757 kiboplus
ifneq (,$(filter $(SUPPORTED_PLATFORM),$(TARGET_BOARD_PLATFORM)))
include $(MTK_SHARED_LIBRARY)
endif



