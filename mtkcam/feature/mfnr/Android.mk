## Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.

# MediaTek Inc. (C) 2016. All rights reserved.
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
##
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# default disable MFLL, we will check if enable later
MFLL_ENABLE := no

# describes platform information
MFLL_PLATFORM := default

#------------------------------------------------------------------------------
# Default MFLL Configurations
#------------------------------------------------------------------------------
# MFLL core version, MFLL core is platform-independent
MFLL_CORE_VERSION_MAJOR                 := 1
MFLL_CORE_VERSION_MINOR                 := 5

# MFLL default version, for build pass usage
MFLL_MODULE_VERSION_BSS                 := default
MFLL_MODULE_VERSION_CAPTURER            := default
MFLL_MODULE_VERSION_EXIFINFO            := default
MFLL_MODULE_VERSION_PERFSERV            := default
MFLL_MODULE_VERSION_IMAGEBUF            := default
MFLL_MODULE_VERSION_MEMC                := default
MFLL_MODULE_VERSION_MFB                 := default
MFLL_MODULE_VERSION_NVRAM               := default
MFLL_MODULE_VERSION_STRATEGY            := default
MFLL_MODULE_VERSION_GYRO                := default

# To use parallel computing for MEMC or single thread
# 0: sequential MEMC
# 1: parallel MEMC
MFLL_MEMC_SUPPORT_MODE                  := 1

# MFLL core will create N threads to do ME and MC if MFLL_MEMC_SUPPORT_MODE = 1
MFLL_MEMC_THREADS_NUM                   := 3

# The MC algorithm also supports (if does) multi-threads computing. This define
# is describes how many threads that MC algorithm can create
MFLL_MC_THREAD_NUM                      := 4

# MfllImageBuffer 1.0 has a mechanism to handle image buffer for ZSD + DualPhse.
# This define describes if to enable or disable(set to 0) this feature. If to
# enable the feature, this define also describes the maximum number of image buffer
# that MfllImageBuffer can create.
MFLL_USING_IMAGEBUFFER_QUEUE            := 0

# Define is to 1 to always enable Multi Frame Blending
MFLL_MFB_ALWAYS_ON                      := 0

# Max available capture frame number
MFLL_MAX_FRAMES                         := 10

# Describes blending number
# Note: This is a default value and will be updated by NVRAM
MFLL_BLEND_FRAME                        := 4

# Describes capture frame number
# Note: This is a default value and will be updated by NVRAM
MFLL_CAPTURE_FRAME                      := $(MFLL_BLEND_FRAME)

# Describes MEMC type, if to use full size MC or not
# Note: This is a default value and will be updated by NVRAM
MFLL_FULL_SIZE_MC                       := 0

# Describes the size of GYRO information queue
MFLL_GYRO_QUEUE_SIZE                    := 30


#------------------------------------------------------------------------------
# Platform Configurations
#------------------------------------------------------------------------------
# MT6797
# {{{
ifeq ($(TARGET_BOARD_PLATFORM), $(filter $(TARGET_BOARD_PLATFORM), mt6797))
  MFLL_PLATFORM := mt6797

  MFLL_CORE_VERSION_MAJOR       := 1
  MFLL_CORE_VERSION_MINOR       := 5

  MFLL_MODULE_VERSION_BSS       := 1.0
  MFLL_MODULE_VERSION_CAPTURER  := 1.0
  MFLL_MODULE_VERSION_EXIFINFO  := 1.0
  MFLL_MODULE_VERSION_PERFSERV  := default
  MFLL_MODULE_VERSION_IMAGEBUF  := 1.0
  MFLL_MODULE_VERSION_MEMC      := 1.0
  MFLL_MODULE_VERSION_MFB       := 1.0
  MFLL_MODULE_VERSION_NVRAM     := 1.0
  MFLL_MODULE_VERSION_STRATEGY  := 1.0

  MFLL_MC_THREAD_NUM            := 10

  MFLL_USING_IMAGEBUFFER_QUEUE  := 12 # 12 full size YUV

  # workaroud to judge whether libperfservicenative exists nor not
  # remove this condition after MTK_PERFSERVICE_SUPPORT works as expected
  ifeq ($(MTK_BASIC_PACKAGE), yes)
    MFLL_MODULE_VERSION_PERFSERV := default
  endif
endif
# }}}

# MT6757
# {{{
ifeq ($(TARGET_BOARD_PLATFORM), $(filter $(TARGET_BOARD_PLATFORM), mt6757))
  MFLL_PLATFORM := mt6757

  MFLL_CORE_VERSION_MAJOR       := 1
  MFLL_CORE_VERSION_MINOR       := 6

  MFLL_MODULE_VERSION_BSS       := 1.1
  MFLL_MODULE_VERSION_CAPTURER  := 1.0
  MFLL_MODULE_VERSION_EXIFINFO  := 1.0
  MFLL_MODULE_VERSION_PERFSERV  := default
  MFLL_MODULE_VERSION_IMAGEBUF  := 1.0
  MFLL_MODULE_VERSION_MEMC      := 1.0
  MFLL_MODULE_VERSION_MFB       := 1.0
  MFLL_MODULE_VERSION_NVRAM     := 1.0
  MFLL_MODULE_VERSION_STRATEGY  := 1.1
  MFLL_MODULE_VERSION_GYRO      := 1.0

  MFLL_MC_THREAD_NUM            := 8

  MFLL_USING_IMAGEBUFFER_QUEUE  := 12 # 12 full size YUV

  # workaroud to judge whether libperfservicenative exists nor not
  # remove this condition after MTK_PERFSERVICE_SUPPORT works as expected
  ifeq ($(MTK_BASIC_PACKAGE), yes)
    MFLL_MODULE_VERSION_PERFSERV := default
  endif
endif
# }}}


# re-check module version
# for LDVT, do just for compile pass, hence we set MFLL_MODULE_VERSION to default
ifeq ($(BUILD_MTK_LDVT),yes)
  MFLL_MODULE_VERSION_BSS           := default
  MFLL_MODULE_VERSION_CAPTURER      := default
  MFLL_MODULE_VERSION_EXIFINFO      := default
  MFLL_MODULE_VERSION_PERFSERV      := default
  MFLL_MODULE_VERSION_IMAGEBUF      := default
  MFLL_MODULE_VERSION_MEMC          := default
  MFLL_MODULE_VERSION_MFB           := default
  MFLL_MODULE_VERSION_NVRAM         := default
  MFLL_MODULE_VERSION_STRATEGY      := default
  MFLL_MODULE_VERSION_GYRO          := default
endif

# check if enable MFLL by ProjectConfig.mk
ifneq ($(strip $(MTK_CAM_MFB_SUPPORT)),0)
  MFLL_ENABLE := yes
endif

#------------------------------------------------------------------------------
# MFLL config start from here.
#------------------------------------------------------------------------------
ifeq ($(strip $(MFLL_ENABLE)), yes)
# Define MFLL core (algorithm) version and dependent middleware version
MFLL_CORE_VERSION := $(MFLL_CORE_VERSION_MAJOR).$(MFLL_CORE_VERSION_MINOR)

# MFNR common interface location
MFLL_INCLUDE_PATH := $(MTK_PATH_SOURCE)/hardware/mtkcam/include/mtkcam/feature/mfnr
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/core/$(MFLL_CORE_VERSION)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/bss/$(MFLL_MODULE_VERSION_BSS)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/capturer/$(MFLL_MODULE_VERSION_CAPTURER)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/exifinfo/$(MFLL_MODULE_VERSION_EXIFINFO)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/imagebuffer/$(MFLL_MODULE_VERSION_IMAGEBUF)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/memc/$(MFLL_MODULE_VERSION_MEMC)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/mfb/$(MFLL_MODULE_VERSION_MFB)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/nvram/$(MFLL_MODULE_VERSION_NVRAM)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/perfserv/$(MFLL_MODULE_VERSION_PERFSERV)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/strategy/$(MFLL_MODULE_VERSION_STRATEGY)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/modules/gyro/$(MFLL_MODULE_VERSION_GYRO)

# MFNR version 2.x all needs 2.0 as base classes
ifeq ($(strip $(MFLL_CORE_VERSION_MAJOR)),2)
MFLL_INCLUDE_PATH += $(LOCAL_PATH)/core/2.0
endif

#
# MFLL core library name
#
MFLL_CORE_LIB_NAME := libmfllcore

#
# if defined, log will be displayed to stdout (yes or no)
#
MFLL_LOG_STDOUT := no

#------------------------------------------------------------------------------
# Define share library.
#------------------------------------------------------------------------------
MFLL_SHARED_LIBS := libutils
MFLL_SHARED_LIBS += libcutils
MFLL_SHARED_LIBS += libstdc++ # M toolchain

# bss
ifeq ($(strip $(MFLL_MODULE_VERSION_BSS)),1.0)
  MFLL_SHARED_LIBS += libcamalgo
  MFLL_SHARED_LIBS += libmtkcam_modulehelper
endif

# capturer
ifeq ($(strip $(MFLL_MODULE_VERSION_CAPTURER)),1.0)
  # no need any
endif

# exifInfo
ifeq ($(strip $(MFLL_MODULE_VERSION_EXIFINFO)),1.0)
  MFLL_SHARED_LIBS += libcameracustom
endif

# imagebuffer
ifeq ($(strip $(MFLL_MODULE_VERSION_IMAGEBUF)),1.0)
  MFLL_SHARED_LIBS += libm4u
  MFLL_SHARED_LIBS += libmedia
  MFLL_SHARED_LIBS += libmtkcam_stdutils
  MFLL_SHARED_LIBS += libmtkcam_imgbuf
endif

# memc
ifeq ($(strip $(MFLL_MODULE_VERSION_MEMC)),1.0)
  MFLL_SHARED_LIBS += libcamalgo
  MFLL_SHARED_LIBS += libmtkcam_modulehelper
endif

# mfb
ifeq ($(strip $(MFLL_MODULE_VERSION_MFB)),1.0)
  MFLL_SHARED_LIBS += libcam.iopipe
  MFLL_SHARED_LIBS += libmtkcam_modulehelper
  MFLL_SHARED_LIBS += libmtkcam_metadata
endif

# nvram
ifeq ($(strip $(MFLL_MODULE_VERSION_NVRAM)),1.0)
  MFLL_SHARED_LIBS += libcameracustom
  MFLL_SHARED_LIBS += libmtkcam_modulehelper
endif

# perfserv
ifeq ($(strip $(MFLL_MODULE_VERSION_PERFSERV)),mt6797)
  MFLL_SHARED_LIBS += libperfservicenative
endif

# strategy
ifeq ($(strip $(MFLL_MODULE_VERSION_STRATEGY)),1.0)
  MFLL_SHARED_LIBS += libcamalgo
endif

# gyro
ifeq ($(strip $(MFLL_MODULE_VERSION_GYRO)),1.0)
  MFLL_SHARED_LIBS += libmtkcam_sysutils
endif


#------------------------------------------------------------------------------
# MFLL module static link lib name, usually you don't need to modify this.
#------------------------------------------------------------------------------
MFLL_LIB_NAME_BSS       := libmfllbss
MFLL_LIB_NAME_CAPTURER  := libmfllcaptuer
MFLL_LIB_NAME_EXIFINFO  := libmfllexifinfo
MFLL_LIB_NAME_PERFSERV  := libmfllperfserv
MFLL_LIB_NAME_IMAGEBUF  := libmfllimagebuf
MFLL_LIB_NAME_MEMC      := libmfllmemc
MFLL_LIB_NAME_MFB       := libmfllmfb
MFLL_LIB_NAME_NVRAM     := libmfllnvram
MFLL_LIB_NAME_STRATEGY  := libmfllstrategy
MFLL_LIB_NAME_GYRO      := libmfllgyro


#------------------------------------------------------------------------------
# defines
#------------------------------------------------------------------------------
ifeq ($(strip $(MFLL_LOG_STDOUT)),yes)
  MFLL_CFLAGS += -DMFLL_LOG_STDOUT
endif
MFLL_CFLAGS += -DMFLL_CORE_VERSION_MAJOR=$(MFLL_CORE_VERSION_MAJOR)
MFLL_CFLAGS += -DMFLL_CORE_VERSION_MINOR=$(MFLL_CORE_VERSION_MINOR)
MFLL_CFLAGS += -DMFLL_MEMC_SUPPORT_MODE=$(MFLL_MEMC_SUPPORT_MODE)
MFLL_CFLAGS += -DMFLL_MEMC_THREADS_NUM=$(MFLL_MEMC_THREADS_NUM)
MFLL_CFLAGS += -DMFLL_MC_THREAD_NUM=$(MFLL_MC_THREAD_NUM)
MFLL_CFLAGS += -DMFLL_USING_IMAGEBUFFER_QUEUE=$(MFLL_USING_IMAGEBUFFER_QUEUE)
MFLL_CFLAGS += -DMFLL_MFB_ALWAYS_ON=$(MFLL_MFB_ALWAYS_ON)
MFLL_CFLAGS += -DMFLL_MAX_FRAMES=$(MFLL_MAX_FRAMES)
MFLL_CFLAGS += -DMFLL_BLEND_FRAME=$(MFLL_BLEND_FRAME)
MFLL_CFLAGS += -DMFLL_CAPTURE_FRAME=$(MFLL_CAPTURE_FRAME)
MFLL_CFLAGS += -DMFLL_FULL_SIZE_MC=$(MFLL_FULL_SIZE_MC)
MFLL_CFLAGS += -DMFLL_PLATFORM_$(shell echo $(MFLL_PLATFORM)|tr a-z A-Z)
MFLL_CFLAGS += -DMFLL_GYRO_QUEUE_SIZE=$(MFLL_GYRO_QUEUE_SIZE)

include $(call all-makefiles-under, $(LOCAL_PATH))
endif # MFLL_ENABLE
