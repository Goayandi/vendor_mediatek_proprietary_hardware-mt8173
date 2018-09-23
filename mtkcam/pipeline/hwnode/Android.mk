# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.

# MediaTek Inc. (C) 2015. All rights reserved.
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

LOCAL_PATH := $(call my-dir)

################################################################################
#
################################################################################
include $(CLEAR_VARS)

#-----------------------------------------------------------
-include $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk
-include $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/feature/effectHal.mk

#-----------------------------------------------------------
ifeq ($(BUILD_MTK_LDVT),yes)
    LOCAL_CFLAGS += -DUSING_MTK_LDVT
    LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/ldvt/$(PLATFORM)/include
    LOCAL_WHOLE_STATIC_LIBRARIES += libuvvf
endif

#-----------------------------------------------------------
LOCAL_SRC_FILES += DummyNode.cpp
LOCAL_SRC_FILES += BaseNode.cpp
LOCAL_SRC_FILES += P1Node.cpp
#LOCAL_SRC_FILES += P2Node.cpp
LOCAL_SRC_FILES += p2node/P2Node.cpp
LOCAL_SRC_FILES += p2node/FrameUtils.cpp
LOCAL_SRC_FILES += p2node/proc/P2Procedure.cpp
LOCAL_SRC_FILES += p2node/proc/SwnrProcedure.cpp
LOCAL_SRC_FILES += p2node/proc/MdpProcedure.cpp
LOCAL_SRC_FILES += p2node/proc/PluginProcedure.cpp
#LOCAL_SRC_FILES += p2node/proc/plugin/RawPostProcessing.cpp
#LOCAL_SRC_FILES += p2node/proc/plugin/YuvPostProcessing.cpp
LOCAL_SRC_FILES += JpegNode.cpp
LOCAL_SRC_FILES += RAW16Node.cpp
LOCAL_SRC_FILES += P2_utils.cpp

ifneq ($(MTK_BASIC_PACKAGE),yes)
LOCAL_SRC_FILES += P2FeatureNode.cpp
endif

ifeq ($(MTK_CAM_HDR_SUPPORT), yes)
LOCAL_SRC_FILES += HDRNode.cpp
endif

ifneq ($(strip $(MTKCAM_HAVE_MFB_SUPPORT)),0)
LOCAL_SRC_FILES += MfllNode.cpp
endif

LOCAL_SRC_FILES += MFCNode.cpp

ifeq ($(MTK_CROSSMOUNT_SUPPORT),yes)
LOCAL_SRC_FILES += BufferReceiveNode.cpp
endif

ifeq ($(MTK_CAM_STEREO_CAMERA_SUPPORT),yes)
LOCAL_CFLAGS += -DENABLE_STEREO_PERFSERVICE
LOCAL_SRC_FILES += StereoRootNode.cpp
LOCAL_SRC_FILES += DepthMapNode.cpp
LOCAL_SRC_FILES += BokehNode.cpp
LOCAL_SRC_FILES += DualImageTransformNode.cpp
LOCAL_SRC_FILES += DualYUVNode.cpp
ifeq ($(MTK_CAM_STEREO_DENOISE_SUPPORT), yes)
LOCAL_SRC_FILES += BMDeNoiseNode.cpp
LOCAL_SRC_FILES += BMDepthMapNode.cpp
LOCAL_SRC_FILES += BMPreProcessNode.cpp
endif
endif
#-----------------------------------------------------------
LOCAL_C_INCLUDES += $(MTKCAM_C_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/include
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/feature/include
#
LOCAL_C_INCLUDES += $(TOP)/$(MTKCAM_ALGO_INCLUDE)
LOCAL_C_INCLUDES += $(TOP)/$(MTKCAM_DRV_INCLUDE)

LOCAL_C_INCLUDES += $(TOP)/$(MTKCAM_DRV_INCLUDE)/drv

#
LOCAL_C_INCLUDES += $(TOP)/system/media/camera/include
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/external/mtkjpeg
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/external/libion_mtk/include
#
LOCAL_C_INCLUDES += $(MTK_PATH_CUSTOM_PLATFORM)/hal/inc
LOCAL_C_INCLUDES += $(MTK_PATH_CUSTOM_PLATFORM)/hal/inc/isp_tuning

# AEE
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/external/aee/binary/inc

ifneq ($(MTK_BASIC_PACKAGE),yes)
LOCAL_C_INCLUDES += $(EFFECTHAL_C_INCLUDE)
endif

#-----------------------------------------------------------
LOCAL_CFLAGS += $(MTKCAM_CFLAGS)
#
ifeq ($(HAVE_AEE_FEATURE),yes)
LOCAL_CFLAGS += -DHWNODE_HAVE_AEE_FEATURE=1
else
LOCAL_CFLAGS += -DHWNODE_HAVE_AEE_FEATURE=0
endif

#-----------------------------------------------------------
LOCAL_STATIC_LIBRARIES +=
#
LOCAL_WHOLE_STATIC_LIBRARIES += libmtkcam_hwnode.fdNode
ifeq ($(MTK_CAM_STEREO_CAMERA_SUPPORT), yes)
LOCAL_CFLAGS += -DMET_USER_EVENT_SUPPORT
LOCAL_WHOLE_STATIC_LIBRARIES += libfeature.vsdof.effecthal
endif

ifneq ($(MTK_BASIC_PACKAGE), yes)
LOCAL_CFLAGS += -DENABLE_PERFSERVICE
LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/perfservice
LOCAL_SHARED_LIBRARIES += libperfservicenative
endif

#-----------------------------------------------------------
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libcutils
#
LOCAL_SHARED_LIBRARIES += libmtkcam_modulehelper
LOCAL_SHARED_LIBRARIES += libmtkcam_stdutils libmtkcam_imgbuf
LOCAL_SHARED_LIBRARIES += libmtkcam_streamutils
LOCAL_SHARED_LIBRARIES += libmtkcam_hwutils
LOCAL_SHARED_LIBRARIES += libmtkcam_metadata
LOCAL_SHARED_LIBRARIES += libmtkcam_metastore
LOCAL_SHARED_LIBRARIES += libcam.feature_utils
LOCAL_SHARED_LIBRARIES += libcameracustom
#
LOCAL_SHARED_LIBRARIES += libcam.iopipe
LOCAL_SHARED_LIBRARIES += libfeature_eis
LOCAL_SHARED_LIBRARIES += libion_mtk
LOCAL_SHARED_LIBRARIES += libion
# AEE
ifeq ($(HAVE_AEE_FEATURE),yes)
LOCAL_SHARED_LIBRARIES += libaed
endif
# JpegNode
LOCAL_SHARED_LIBRARIES += libcam.iopipe
LOCAL_SHARED_LIBRARIES += libmtkcam_exif
LOCAL_SHARED_LIBRARIES += libmtkjpeg
# sensor
LOCAL_SHARED_LIBRARIES += libmtkcam_sysutils
ifeq ($(MTK_CAM_VHDR_SUPPORT), yes)
# vhdr
LOCAL_SHARED_LIBRARIES += libcam.vhdr
endif
# mfnr core
ifneq ($(strip $(MTKCAM_HAVE_MFB_SUPPORT)),0)
LOCAL_SHARED_LIBRARIES += libmfllcore
endif
# RAW16
LOCAL_SHARED_LIBRARIES += libdngop
# face feature
LOCAL_SHARED_LIBRARIES += libfeature.face
ifeq ($(MTK_CROSSMOUNT_SUPPORT),yes)
LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcam.external_device
endif

LOCAL_SHARED_LIBRARIES += libdpframework

ifeq ($(MTK_CAM_STEREO_CAMERA_SUPPORT), yes)
LOCAL_SHARED_LIBRARIES += libmtkcam.featurepipe.depthmap
LOCAL_SHARED_LIBRARIES += libmtkcam.featurepipe.bokeh
LOCAL_SHARED_LIBRARIES += libmtkcam.featurepipe.vsdof_util
LOCAL_SHARED_LIBRARIES += libfeature.vsdof.hal
LOCAL_SHARED_LIBRARIES += libcameracustom
#LOCAL_SHARED_LIBRARIES += libmet-tag
ifeq ($(MTK_CAM_STEREO_DENOISE_SUPPORT), yes)
LOCAL_SHARED_LIBRARIES += libmtkcam.featurepipe.bmdepthmap
LOCAL_SHARED_LIBRARIES += libmtkcam.featurepipe.bmdenoise
endif
endif
# effecthal
ifneq ($(MTK_BASIC_PACKAGE),yes)
LOCAL_SHARED_LIBRARIES += libeffecthal.base
endif
ifneq ($(MTK_BASIC_PACKAGE),yes)
LOCAL_SHARED_LIBRARIES += libmtkcam.featurepipe.streaming
endif
#-----------------------------------------------------------
# aaa
MTKCAM_3A_SIMULATOR := no
#$(info MTKCAM_3A_SIMULATOR = "$(MTKCAM_3A_SIMULATOR)")

ifeq ($(MTKCAM_3A_SIMULATOR),yes)
LOCAL_CFLAGS += -DUSING_3A_SIMULATOR=1
LOCAL_CFLAGS += -DUSING_3A_SIMULATOR_SOF=1
else
LOCAL_CFLAGS += -DUSING_3A_SIMULATOR=0
LOCAL_CFLAGS += -DUSING_3A_SIMULATOR_SOF=0
endif

# mtkcam hw node Log Level (for compile-time loglevel control)
ifndef MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT
ifeq ($(TARGET_BUILD_VARIANT),user)
    MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT   := 2
else ifeq ($(TARGET_BUILD_VARIANT),userdebug)
    MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT   := 3
else
    MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT   := 4
endif
endif
LOCAL_CFLAGS += -DMTKCAM_HW_NODE_LOG_LEVEL_DEFAULT=$(MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT)

#-----------------------------------------------------------
LOCAL_MODULE := libmtkcam_hwnode
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

#-----------------------------------------------------------
include $(MTK_SHARED_LIBRARY)


################################################################################
#
################################################################################
include $(call all-makefiles-under,$(LOCAL_PATH))

