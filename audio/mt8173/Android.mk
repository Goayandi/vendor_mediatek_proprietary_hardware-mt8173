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

#  only if use yusu audio will build this.
#ifneq ($(TARGET_BUILD_PDK),true)
ifeq ($(strip $(BOARD_USES_MTK_AUDIO)),true)

LOCAL_PATH:= $(call my-dir)
LOCAL_COMMON_PATH:=../common

include $(CLEAR_VARS)

ifeq ($(MTK_PLATFORM),MT8173)

LOCAL_SRC_FILES := \
    $(LOCAL_COMMON_PATH)/aud_drv/AudioMTKFilter.cpp \
    $(LOCAL_COMMON_PATH)/aud_drv/AudioMTKHeadsetMessager.cpp \
    $(LOCAL_COMMON_PATH)/aud_drv/AudioUtility.cpp \
    $(LOCAL_COMMON_PATH)/aud_drv/AudioFtmBase.cpp \
    $(LOCAL_COMMON_PATH)/aud_drv/WCNChipController.cpp \
    $(LOCAL_COMMON_PATH)/aud_drv/AudioVIBSPKControl.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioSpeechEnhanceInfo.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioSpeechEnhLayer.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioPreProcess.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioALSADriverUtility.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioALSASampleRateController.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioALSADeviceParser.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioVolumeFactory.cpp \
    $(LOCAL_COMMON_PATH)/V3/aud_drv/AudioALSAFMController.cpp

LOCAL_C_INCLUDES := \
    $(MTK_PATH_SOURCE)/hardware/audio/$(shell echo $(MTK_PLATFORM) | tr A-Z a-z)/include \
    $(MTK_PATH_SOURCE)/hardware/audio/common/include \
    $(MTK_PATH_SOURCE)/hardware/audio/common/V3/include \
    $(MTK_PATH_SOURCE)/external/AudioCompensationFilter \
    $(MTK_PATH_SOURCE)/external/AudioComponentEngine \
    $(MTK_PATH_SOURCE)/external/audiocustparam \
    $(MTK_PATH_SOURCE)/external/AudioSpeechEnhancement/V3/inc \
    $(MTK_PATH_CUSTOM)/hal/audioflinger/audio \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects) \
    $(call include-path-for, audio-route) \
    external/tinyalsa/include \
    external/tinycompress/include

LOCAL_CFLAGS += -DMTK_SUPPORT_AUDIO_DEVICE_API3
LOCAL_CFLAGS += -DUPLINK_LOW_LATENCY -DDOWNLINK_LOW_LATENCY

ifeq ($(MTK_AUDIO_BLOUD_CUSTOMPARAMETER_REV),MTK_AUDIO_BLOUD_CUSTOMPARAMETER_V5)
  LOCAL_CFLAGS += -DMTK_AUDIO_BLOUD_CUSTOMPARAMETER_V5
else
  ifeq ($(strip $(MTK_AUDIO_BLOUD_CUSTOMPARAMETER_REV)),MTK_AUDIO_BLOUD_CUSTOMPARAMETER_V4)
    LOCAL_CFLAGS += -DMTK_AUDIO_BLOUD_CUSTOMPARAMETER_V4
  endif
endif

ifeq ($(findstring MTK_AOSP_ENHANCEMENT,  $(MTK_GLOBAL_CFLAGS)),)
  LOCAL_CFLAGS += -DMTK_BASIC_PACKAGE
endif

ifeq ($(MTK_DIGITAL_MIC_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_DIGITAL_MIC_SUPPORT
endif

ifeq ($(strip $(MTK_2IN1_SPK_SUPPORT)),yes)
  LOCAL_CFLAGS += -DUSING_2IN1_SPEAKER
endif

ifeq ($(strip $(MTK_EXTERNAL_BUILTIN_MIC_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_EXTERNAL_BUILTIN_MIC_SUPPORT
endif

ifeq ($(strip $(MTK_EXTERNAL_SPEAKER_DAC_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_EXTERNAL_SPEAKER_DAC_SUPPORT
endif

ifeq ($(MTK_BSP_PACKAGE),yes)
 LOCAL_CFLAGS += -DMTK_BSP_PACKAGE
endif

LOCAL_MODULE := libaudio_hal_common
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_TAGS := optional

include $(MTK_STATIC_LIBRARY)

# MediaTek Audio HAL
include $(CLEAR_VARS)

LOCAL_CFLAGS += -DMTK_AUDIO -DMTK_HDMI_FORCE_AUDIO_CLK
LOCAL_CFLAGS += -DUPLINK_LOW_LATENCY -DDOWNLINK_LOW_LATENCY

LOCAL_C_INCLUDES := \
    hardware/libhardware_legacy/include \
    hardware/libhardware/include \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects) \
    $(call include-path-for, audio-route) \
    $(MTK_PATH_SOURCE)/hardware/ccci/include \
    $(MTK_PATH_SOURCE)/hardware/audio/$(shell echo $(MTK_PLATFORM) | tr A-Z a-z)/include \
    $(MTK_PATH_SOURCE)/hardware/audio/$(shell echo $(MTK_PLATFORM) | tr A-Z a-z)/include/symlink \
    $(MTK_PATH_SOURCE)/hardware/audio/common \
    $(MTK_PATH_SOURCE)/external/AudioCompensationFilter \
    $(MTK_PATH_SOURCE)/external/AudioComponentEngine \
    $(MTK_PATH_SOURCE)/external/audiocustparam \
    $(MTK_PATH_SOURCE)/external/AudioSpeechEnhancement/V3/inc \
    $(MTK_PATH_CUSTOM)/hal/audioflinger/audio \
    external/tinyalsa/include \
    external/tinycompress/include

LOCAL_SRC_FILES += \
    $(LOCAL_COMMON_PATH)/aud_drv/audio_hw_hal.cpp \
    aud_drv/AudioALSAStreamManager.cpp \
    aud_drv/AudioALSAHardware.cpp \
    aud_drv/AudioALSAStreamOut.cpp \
    aud_drv/AudioALSAStreamIn.cpp \
    aud_drv/AudioALSAPlaybackHandlerBase.cpp \
    aud_drv/AudioALSAPlaybackHandlerNormal.cpp \
    aud_drv/AudioALSAPlaybackHandlerBTSCO.cpp \
    aud_drv/AudioALSAPlaybackHandlerHDMI.cpp \
    aud_drv/AudioALSAPlaybackHandlerHDMIRaw.cpp \
    aud_drv/AudioALSAPlaybackHandlerFast.cpp \
    aud_drv/AudioALSACaptureHandlerBase.cpp \
    aud_drv/AudioALSACaptureHandlerNormal.cpp \
    aud_drv/AudioALSACaptureHandlerBTSCO.cpp \
    aud_drv/AudioALSACaptureHandlerAEC.cpp \
    aud_drv/AudioALSACaptureHandlerFMRadio.cpp \
    aud_drv/AudioALSACaptureDataClient.cpp \
    aud_drv/AudioALSACaptureDataProviderBase.cpp \
    aud_drv/AudioALSACaptureDataProviderNormal.cpp \
    aud_drv/AudioALSACaptureDataProviderBTSCO.cpp \
    aud_drv/AudioALSACaptureDataProviderExternal.cpp \
    aud_drv/AudioALSACaptureDataProviderFMRadio.cpp \
    aud_drv/AudioALSACaptureDataProviderEchoRef.cpp \
    aud_drv/AudioALSACaptureDataProviderEchoRefBTSCO.cpp \
    aud_drv/AudioALSACaptureDataProviderEchoRefExt.cpp \
    aud_drv/AudioALSAHardwareResourceManager.cpp \
    aud_drv/AudioALSAVolumeController.cpp \
    aud_drv/AudioFtm.cpp \
    aud_drv/AudioALSAParamTuner.cpp \
    aud_drv/LoopbackManager.cpp \
    aud_drv/AudioALSALoopbackController.cpp \
    aud_drv/HDMITxController.cpp

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper \
    libaudio_hal_common

LOCAL_SHARED_LIBRARIES += \
    libmedia \
    libcutils \
    libutils \
    libhardware_legacy \
    libhardware \
    libdl \
    libaudiocustparam \
    libaudioutils \
    libtinyalsa \
    libtinycompress \
    libaudioroute \
    libcustom_nvram \
    libaudiospdif \
    libaudiotoolkit

ifeq ($(strip $(MTK_HIGH_RESOLUTION_AUDIO_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_HD_AUDIO_ARCHITECTURE
endif

ifeq ($(AUDIO_POLICY_TEST),true)
  ENABLE_AUDIO_DUMP := true
endif

ifeq ($(strip $(TARGET_BUILD_VARIANT)),eng)
  LOCAL_CFLAGS += -DDEBUG_AUDIO_PCM
endif

ifeq ($(MTK_DIGITAL_MIC_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_DIGITAL_MIC_SUPPORT
endif

ifeq ($(strip $(MTK_AUDENH_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_AUDENH_SUPPORT
endif

ifeq ($(strip $(MTK_BESLOUDNESS_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_BESLOUDNESS_SUPPORT
endif

ifeq ($(strip $(MTK_BESSURROUND_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_BESSURROUND_SUPPORT
endif

ifeq ($(strip $(MTK_HDMI_MULTI_CHANNEL_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_HDMI_MULTI_CHANNEL_SUPPORT
endif

ifeq ($(strip $(MTK_AUDIO_MIC_INVERSE)),yes)
  LOCAL_CFLAGS += -DDEN_PHONE_MIC_INVERSE
endif

ifeq ($(strip $(MTK_2IN1_SPK_SUPPORT)),yes)
  LOCAL_CFLAGS += -DUSING_2IN1_SPEAKER
endif

ifeq ($(strip $(MTK_USE_ANDROID_MM_DEFAULT_CODE)),yes)
  LOCAL_CFLAGS += -DANDROID_DEFAULT_CODE
endif

ifeq ($(strip $(DMNR_TUNNING_AT_MODEMSIDE)),yes)
  LOCAL_CFLAGS += -DDMNR_TUNNING_AT_MODEMSIDE
endif

ifeq ($(MTK_DUAL_MIC_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_DUAL_MIC_SUPPORT
endif

ifeq ($(MTK_VOW_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_VOW_SUPPORT
endif

ifeq ($(MTK_MAGICONFERENCE_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_MAGICONFERENCE_SUPPORT
endif

ifeq ($(MTK_HAC_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_HAC_SUPPORT
endif

ifeq ($(MTK_VIBSPK_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_VIBSPK_SUPPORT
endif

ifeq ($(NXP_SMARTPA_SUPPORT),yes)
  LOCAL_CFLAGS += -DNXP_SMARTPA_SUPPORT
  LOCAL_CFLAGS += -DEXT_SPK_SUPPORT
endif

LOCAL_CFLAGS += -DMTK_INCALL_MODE_REFACTORY_SUPPORT

LOCAL_CFLAGS += -DMTK_SUPPORT_AUDIO_DEVICE_API3

ifeq ($(MTK_AUDIO_BLOUD_CUSTOMPARAMETER_REV),MTK_AUDIO_BLOUD_CUSTOMPARAMETER_V5)
  LOCAL_CFLAGS += -DMTK_AUDIO_BLOUD_CUSTOMPARAMETER_V5
else
  ifeq ($(strip $(MTK_AUDIO_BLOUD_CUSTOMPARAMETER_REV)),MTK_AUDIO_BLOUD_CUSTOMPARAMETER_V4)
    LOCAL_CFLAGS += -DMTK_AUDIO_BLOUD_CUSTOMPARAMETER_V4
  endif
endif

ifeq ($(MTK_SUPPORT_TC1_TUNNING),yes)
  LOCAL_CFLAGS += -DMTK_AUDIO_GAIN_TABLE
endif

ifeq ($(strip $(MTK_TTY_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_TTY_SUPPORT
endif

ifeq ($(findstring MTK_AOSP_ENHANCEMENT,  $(MTK_GLOBAL_CFLAGS)),)
  LOCAL_CFLAGS += -DMTK_BASIC_PACKAGE
endif

ifeq ($(strip $(MTK_ENABLE_MD1)),yes)
  LOCAL_CFLAGS += -D__MTK_ENABLE_MD1__
endif

ifeq ($(strip $(MTK_ENABLE_MD2)),yes)
  LOCAL_CFLAGS += -D__MTK_ENABLE_MD2__
endif

ifeq ($(strip $(MTK_ENABLE_MD5)),yes)
  LOCAL_CFLAGS += -D__MTK_ENABLE_MD5__
endif

ifeq ($(HAVE_AEE_FEATURE),yes)
  LOCAL_SHARED_LIBRARIES += libaed
  LOCAL_C_INCLUDES += \
    $(MTK_PATH_SOURCE)/external/aee/binary/inc
  LOCAL_CFLAGS += -DHAVE_AEE_FEATURE
endif

ifeq ($(MTK_WB_SPEECH_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_WB_SPEECH_SUPPORT
endif

# Audio HD Record
ifeq ($(MTK_AUDIO_HD_REC_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_AUDIO_HD_REC_SUPPORT
endif
# Audio HD Record

# MTK VoIP
ifeq ($(MTK_VOIP_ENHANCEMENT_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_VOIP_ENHANCEMENT_SUPPORT
endif
# MTK VoIP

# DMNR 3.0
ifeq ($(strip $(MTK_HANDSFREE_DMNR_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_HANDSFREE_DMNR_SUPPORT
endif
# DMNR 3.0

# Native Audio Preprocess
ifeq ($(strip $(NATIVE_AUDIO_PREPROCESS_ENABLE)),yes)
  LOCAL_CFLAGS += -DNATIVE_AUDIO_PREPROCESS_ENABLE
endif

ifeq ($(MTK_INTERNAL_HDMI_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_INTERNAL_HDMI_SUPPORT
endif

ifeq ($(MTK_INTERNAL_MHL_SUPPORT),yes)
  LOCAL_CFLAGS += -DMTK_INTERNAL_MHL_SUPPORT
endif

ifeq ($(strip $(MTK_EXTERNAL_BUILTIN_MIC_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_EXTERNAL_BUILTIN_MIC_SUPPORT
endif

ifeq ($(strip $(MTK_EXTERNAL_SPEAKER_DAC_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_EXTERNAL_SPEAKER_DAC_SUPPORT
endif

ifeq ($(strip $(MTK_HIRES_192K_AUDIO_SUPPORT)),yes)
  LOCAL_CFLAGS += -DMTK_HIRES_192K_AUDIO_SUPPORT
endif

LOCAL_ARM_MODE := arm
LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := both
include $(MTK_SHARED_LIBRARY)

#========================================================================

include $(CLEAR_VARS)
LOCAL_CLANG := false
LOCAL_SRC_FILES:= \
    $(LOCAL_COMMON_PATH)/aud_drv/AudioToolkit.cpp
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils


LOCAL_STATIC_LIBRARIES := \

LOCAL_C_INCLUDES:= \
    $(MTK_PATH_SOURCE)/hardware/audio/common/include \

LOCAL_MODULE:= libaudiotoolkit
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

#ifeq ($(MTK_AUDIO_A64_SUPPORT),yes)
#LOCAL_MULTILIB := both
#else
#LOCAL_MULTILIB := 32
#endif

LOCAL_ARM_MODE := arm

include $(BUILD_SHARED_LIBRARY)
#========================================================================
ifeq ($(findstring MTK_AOSP_ENHANCEMENT,  $(MTK_GLOBAL_CFLAGS)),)
ifneq ($(USE_LEGACY_AUDIO_POLICY), 1)
ifeq ($(USE_CUSTOM_AUDIO_POLICY), 1)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    $(LOCAL_COMMON_PATH)/aud_policy/AudioPolicyManagerCustom.cpp

LOCAL_CFLAGS := -DMTK_AUDIO -DMTK_BASIC_PACKAGE

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    liblog \
    libmedia \
    libaudiopolicymanagerdefault

LOCAL_C_INCLUDES := \
    $(TOPDIR)/hardware/libhardware_legacy/include \
    $(TOPDIR)/hardware/libhardware/include \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects) \
    $(MTK_PATH_SOURCE)/hardware/audio/common/include \
    $(MTK_PATH_SOURCE)/hardware/audio/common/include/hardware \
    $(MTK_PATH_SOURCE)/hardware/audio/common/aud_policy \
    $(MTK_PATH_SOURCE)/external/AudioCompensationFilter \
    $(MTK_PATH_SOURCE)/external/AudioComponentEngine \
    $(MTK_PATH_SOURCE)/external/cvsd_plc_codec \
    $(MTK_PATH_SOURCE)/external/msbc_codec \
    $(MTK_PATH_SOURCE)/external/audiocustparam \
    $(MTK_PATH_SOURCE)/external/AudioSpeechEnhancement/V3/inc \
    $(MTK_PATH_SOURCE)/external/blisrc \
    $(MTK_PATH_SOURCE)/external/bessound_HD \
    $(MTK_PATH_SOURCE)/external/bessound \
    $(MTK_PATH_SOURCE)/external/shifter \
    $(MTK_PATH_SOURCE)/external/limiter \
    $(MTK_PATH_SOURCE)/external/audiodcremoveflt \
    $(MTK_PATH_CUSTOM)/custom \
    $(MTK_PATH_CUSTOM)/custom/audio \
    $(MTK_PATH_CUSTOM)/cgen/inc \
    $(MTK_PATH_CUSTOM)/cgen/cfgfileinc \
    $(MTK_PATH_CUSTOM)/cgen/cfgdefault \
    $(MTK_PATH_CUSTOM)/hal/audioflinger/audio \
    $(TOPDIR)frameworks/av/services/audiopolicy \
    $(TOPDIR)frameworks/av/services/audiopolicy/engine/interface \
    $(TOPDIR)frameworks/av/services/audiopolicy/common/include \
    $(TOPDIR)frameworks/av/services/audiopolicy/common/managerdefinitions/include

LOCAL_MODULE := libaudiopolicymanagercustom
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(MTK_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    $(LOCAL_COMMON_PATH)/aud_policy/AudioPolicyFactory.cpp

LOCAL_CFLAGS := -DMTK_AUDIO -DMTK_BASIC_PACKAGE

LOCAL_SHARED_LIBRARIES := libaudiopolicymanagercustom

LOCAL_C_INCLUDES := \
    $(TOPDIR)frameworks/av/services/audiopolicy \
    $(TOPDIR)frameworks/av/services/audiopolicy/engine/interface \
    $(TOPDIR)frameworks/av/services/audiopolicy/common/include \
    $(TOPDIR)frameworks/av/services/audiopolicy/common/managerdefinitions/include \
    $(MTK_PATH_SOURCE)/hardware/audio/common/include \
    $(MTK_PATH_SOURCE)/hardware/audio/common/include/hardware \
    $(MTK_PATH_SOURCE)/hardware/audio/common/aud_policy

LOCAL_MODULE := libaudiopolicymanager
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(MTK_SHARED_LIBRARY)

endif
endif
endif

endif
endif
