# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.

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


################################################################################
#
################################################################################

-include $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk
-include $(TOP)/$(MTK_PATH_CUSTOM)/hal/mtkcam/mtkcam.mk

PLATFORM := $(shell echo $(MTK_PLATFORM) | tr A-Z a-z)

#-----------------------------------------------------------
# MTK Hal Standard Include Path
MTKCAM_C_INCLUDES += $(TOP)/$(MTKCAM_C_INCLUDES)/..

MTKCAM_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/include
MTKCAM_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/include/mtkcam/drv

# add begin
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/include
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/include/mtkcam
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/gralloc_extra/include
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/ext/include
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/include

LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/v1/adapter/inc
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/v1/adapter/inc/Scenario
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/v1/adapter/inc/Scenario/Shot
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/v1/adapter/Scenario/Shot/inc

#
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/core/campipe/inc
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/core/camshot/inc
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/include/mtkcam/algorithm/libhdr
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/include/mtkcam/algorithm/libstereocam
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/include/mtkcam/algorithm/libutility
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/include/mtkcam/drv
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/include/mtkcam/featureio
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(MTK_PLATFORM_DIR)/include
#
# add end

LOCAL_C_INCLUDES += $(MTK_PATH_CUSTOM_PLATFORM)/hal/inc/aaa

ifneq ($(strip $(MTK_ALPS_BOX_SUPPORT)),yes)

# enable or disable dngop
MTK_TEMP_DNGOP_ENABLE := 1

# enable fake lib3a
MTKCAM_TEMP_ENABLE_FAKE_LIB3A := 0

# mdp_mgr.cpp call setSharpness or not 
# seems as if on 8173, don't call this fuction
MTKCAM_TEMP_MDP_MGR_CALL_SETSHARPNESS := 0
LOCAL_CFLAGS += -DMTKCAM_TEMP_MDP_MGR_CALL_SETSHARPNESS=$(MTKCAM_TEMP_MDP_MGR_CALL_SETSHARPNESS)

# af_mgr.cpp IAfAlgo call initAF or not
# porting D1 camera MW/HAL to 8173, now use real lib3a, not crash
MTKCAM_TEMP_AFMGR_AFALGO_CALL_INITAF := 1
LOCAL_CFLAGS += -DMTKCAM_TEMP_AFMGR_AFALGO_CALL_INITAF=$(MTKCAM_TEMP_AFMGR_AFALGO_CALL_INITAF)

# ae_mgr.cpp IAeAlgo call initAE or not
# porting D1 camera MW/HAL to 8173, now use real lib3a, not crash
MTKCAM_TEMP_AEMGR_AEALGO_CALL_INITAE := 1
LOCAL_CFLAGS += -DMTKCAM_TEMP_AEMGR_AEALGO_CALL_INITAE=$(MTKCAM_TEMP_AEMGR_AEALGO_CALL_INITAE)

# whether compile cpu ctrl or not : CpuCtrl.cpp
# this is for particial build pass, because on tk branch, it will use, on aosp 8173, won't use it
MTKCAM_TEMP_CPUCTRL_DISABLE := 0
LOCAL_CFLAGS += -DMTKCAM_TEMP_CPUCTRL_DISABLE=$(MTKCAM_TEMP_CPUCTRL_DISABLE)

# whether the Image Buffer ignore the stride/width/height error, eg: stride is 0
MTKCAM_TEMP_IMAGEBUFFER_FORCE_IGNORE_ERR := 0
LOCAL_CFLAGS += -DMTKCAM_TEMP_IMAGEBUFFER_FORCE_IGNORE_ERR=$(MTKCAM_TEMP_IMAGEBUFFER_FORCE_IGNORE_ERR)
endif 

# SDK Client
# set it here, for every platform can set it, then particial compile legacy/v1/client/SdkClient using mmm won't fail
ifeq ($(strip $(MTK_CAM_MMSDK_SUPPORT)),yes)
MTKSDK_HAVE_GESTURE_CLIENT := 1
else
MTKSDK_HAVE_GESTURE_CLIENT := 0
endif
#

ifneq ($(strip $(MTK_ALPS_BOX_SUPPORT)),yes)

PRINT_ONCE_GESTURE_CLIENT ?= 1
ifeq ($(PRINT_ONCE_GESTURE_CLIENT), 1)
$(warning "MTKSDK_HAVE_GESTURE_CLIENT:$(MTKSDK_HAVE_GESTURE_CLIENT)")
PRINT_ONCE_GESTURE_CLIENT := 2
endif

PRINT_ONCE_MTKCAM_C_INCLUDES ?= 1
ifeq ($(PRINT_ONCE_MTKCAM_C_INCLUDES), 1)
$(warning "MTKCAM_C_INCLUDES:" $(MTKCAM_C_INCLUDES))
PRINT_ONCE_MTKCAM_C_INCLUDES := 2
endif

endif
#



