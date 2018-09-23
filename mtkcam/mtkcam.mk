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
-include $(TOP)/$(MTK_PATH_CUSTOM)/hal/mtkcam/mtkcam.mk
MTKCAM_MTK_PLATFORM := $(shell echo $(MTK_PLATFORM) | tr A-Z a-z)
#MTKCAM_TARGET_BOARD_PLATFORM := $(TARGET_BOARD_PLATFORM)

#-----------------------------------------------------------
# LTM module on/off option
#         0: disabled
# otherwise: enabled
#-----------------------------------------------------------
MTK_CAM_LTM_SUPPORT := 1

#-----------------------------------------------------------
# Camera ip base design version control
#-----------------------------------------------------------
ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6797 mt6757))
    MTKCAM_IP_BASE := 1
else ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), kiboplus))
    MTKCAM_IP_BASE := 1
else
    MTKCAM_IP_BASE := 0
endif

#-----------------------------------------------------------
# Camera ip base design include path
#-----------------------------------------------------------
ifeq ($(MTKCAM_IP_BASE),1)
#### ip-based chips ########################################
IS_LEGACY := 0

MTKCAM_DRV_INCLUDE := $(MTK_PATH_SOURCE)/hardware/mtkcam/drv/include/$(TARGET_BOARD_PLATFORM)
MTKCAM_ALGO_INCLUDE := $(MTK_PATH_SOURCE)/hardware/mtkcam/include/algorithm/$(TARGET_BOARD_PLATFORM)
MTKCAM_PUBLIC_INCLUDE := $(MTK_PATH_SOURCE)/hardware/mtkcam/include

MTKCAM_C_INCLUDES := $(MTKCAM_PUBLIC_INCLUDE)
MTKCAM_C_INCLUDES += $(MTK_PATH_SOURCE)/external/aee/binary/inc
ifeq ($(BUILD_MTK_LDVT),true)
MTKCAM_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/ldvt/$(MTKCAM_MTK_PLATFORM)/include
endif

else
#### platform-based (legacy) chips #########################
IS_LEGACY := 1

MTKCAM_C_INCLUDES := $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/include/mtkcam

# path for legacy platform
MTK_MTKCAM_PLATFORM    := $(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/$(shell echo $(MTK_PLATFORM) | tr A-Z a-z)
ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6735m mt6737m))
MTK_MTKCAM_PLATFORM    := $(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/mt6735m
endif
ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6737t))
MTK_MTKCAM_PLATFORM    := $(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/platform/mt6735
endif


# MTKCAM_LOG_LEVEL_DEFAULT for compile-time loglevel control
MTKCAM_LOG_LEVEL_DEFAULT   := 4
LOG_EXCEPTION_LIST :=
ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(LOG_EXCEPTION_LIST)),$(TARGET_BOARD_PLATFORM))
ifeq ($(TARGET_BUILD_VARIANT), user)
    MTKCAM_LOG_LEVEL_DEFAULT   := 2
else ifeq ($(TARGET_BUILD_VARIANT), userdebug)
# for user debug load & MTKCAM_USER_DBG_LOG_OFF (depends on special customer's request)
# change default log level to ALOGI
ifeq ($(MTK_CAM_USER_DBG_LOG_OFF), yes)
    MTKCAM_LOG_LEVEL_DEFAULT   := 2
else
    MTKCAM_LOG_LEVEL_DEFAULT   := 3
endif
endif
endif
MTKCAM_CFLAGS += -DMTKCAM_LOG_LEVEL_DEFAULT=$(MTKCAM_LOG_LEVEL_DEFAULT)

endif ######################################################

#-----------------------------------------------------------
# MFLL default ON, makes MFLL could be triggered in normal scene mode
# 0: do not enable buildin MFLL
# 1: enable buildin MFLL as MFLL mode
# 2: enable buuldin MFLL as AIS
#-----------------------------------------------------------
MTK_CAM_MFB_BUILDIN_SUPPORT := 0

#-----------------------------------------------------------
# MTKCAM_CFLAGS define
# In Android.mk, add followed words to use it:
# LOCAL_CFLAGS + = MTKCAM_CFLAGS
#-----------------------------------------------------------
# MTKCAM_HAVE_AEE_FEATURE
ifeq "yes" "$(strip $(HAVE_AEE_FEATURE))"
    MTKCAM_HAVE_AEE_FEATURE ?= 1
else
    MTKCAM_HAVE_AEE_FEATURE := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_AEE_FEATURE=$(MTKCAM_HAVE_AEE_FEATURE)
#-----------------------------------------------------------
# MTK_BASIC_PACKAGE
ifneq ($(MTK_BASIC_PACKAGE), yes)
    MTKCAM_BASIC_PACKAGE := 0
else
    MTKCAM_BASIC_PACKAGE := 1
endif
MTKCAM_CFLAGS += -DMTKCAM_BASIC_PACKAGE=$(MTKCAM_BASIC_PACKAGE)
#-----------------------------------------------------------
# MFLL option
ifneq ($(strip $(MTK_CAM_MFB_SUPPORT)),0)
    MTKCAM_HAVE_MFB_SUPPORT := $(MTK_CAM_MFB_SUPPORT)
else
    MTKCAM_HAVE_MFB_SUPPORT := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_MFB_SUPPORT=$(MTKCAM_HAVE_MFB_SUPPORT)
#-----------------------------------------------------------
# ZSD+MFLL
ifeq "yes" "$(strip $(MTK_CAM_ZSDMFB_SUPPORT))"
    MTKCAM_HAVE_ZSDMFB_SUPPORT := 1
else
    MTKCAM_HAVE_ZSDMFB_SUPPORT := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_ZSDMFB_SUPPORT=$(MTKCAM_HAVE_ZSDMFB_SUPPORT)
#-----------------------------------------------------------
# build in MFLL option, which means MFLL may be triggered even in normal scene mode
ifneq ($(strip $(MTK_CAM_MFB_BUILDIN_SUPPORT)),0)
    MTKCAM_HAVE_MFB_BUILDIN_SUPPORT := $(MTK_CAM_MFB_BUILDIN_SUPPORT)
else
    MTKCAM_HAVE_MFB_BUILDIN_SUPPORT := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_MFB_BUILDIN_SUPPORT=$(MTKCAM_HAVE_MFB_BUILDIN_SUPPORT)
#-----------------------------------------------------------
# ZSD+HDR
ifeq "yes" "$(strip $(MTK_CAM_ZSDHDR_SUPPORT))"
    MTK_CAM_HAVE_ZSDHDR_SUPPORT := 1
else
    MTK_CAM_HAVE_ZSDHDR_SUPPORT := 0
endif
MTKCAM_CFLAGS += -DMTK_CAM_HAVE_ZSDHDR_SUPPORT=$(MTK_CAM_HAVE_ZSDHDR_SUPPORT)
#-----------------------------------------------------------
# VHDR
ifeq ($(MTK_CAM_VHDR_SUPPORT),yes)
    MTKCAM_HAVE_VHDR_SUPPORT := 1
else
    MTKCAM_HAVE_VHDR_SUPPORT := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_VHDR_SUPPORT=$(MTKCAM_HAVE_VHDR_SUPPORT)
#-----------------------------------------------------------
# HDR
ifeq ($(MTK_CAM_HDR_SUPPORT),yes)
    MTKCAM_HAVE_HDR_SUPPORT := 1
else
    MTKCAM_HAVE_HDR_SUPPORT := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_HDR_SUPPORT=$(MTKCAM_HAVE_HDR_SUPPORT)
#-----------------------------------------------------------
# HDR Detection
MTK_CAM_HDR_DETECTION_MODE ?= 0
MTKCAM_CFLAGS += -DMTKCAM_HDR_DETECTION_MODE=$(MTK_CAM_HDR_DETECTION_MODE)
#-----------------------------------------------------------
# camera mount
ifeq "yes" "$(strip $(MTK_CROSSMOUNT_SUPPORT))"
    MTKCAM_HAVE_CAMERAMOUNT := 1
else
    MTKCAM_HAVE_CAMERAMOUNT := 0
endif
MTKCAM_CFLAGS += -DMTKCAM_HAVE_CAMERAMOUNT=$(MTKCAM_HAVE_CAMERAMOUNT)
#-----------------------------------------------------------
# MTKCAM_HAVE_RR_PRIORITY
MTKCAM_HAVE_RR_PRIORITY      ?= 0  # built-in if '1' ; otherwise not built-in
MTKCAM_CFLAGS += -DMTKCAM_HAVE_RR_PRIORITY=$(MTKCAM_HAVE_RR_PRIORITY)
#-----------------------------------------------------------
# L1_CACHE_BYTES for 32-byte cache line
MTKCAM_CFLAGS += -DL1_CACHE_BYTES=32
#-----------------------------------------------------------
ifeq ($(strip $(MTK_CAM_MAX_NUMBER_OF_CAMERA)), 4)
    MTKCAM_CFLAGS += -DMTK_SUB2_IMGSENSOR
endif
#----------------------------------------------------------
# LTM enable define
MTKCAM_CFLAGS += -DMTKCAM_LTM_SUPPORT=$(MTK_CAM_LTM_SUPPORT)

