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




#ifeq ($(MTK_PLATFORM),$(filter $(MTK_PLATFORM),MT6593))

LOCAL_PATH:= $(call my-dir)

COMMON_PATH:= common
INCLUDE_PATH:= inc

include $(CLEAR_VARS)

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6735m)
LOCAL_CFLAGS += -DBWC_D2
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6737)
LOCAL_CFLAGS += -DBWC_D2_P
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6737m)
LOCAL_CFLAGS += -DBWC_D2_M
else ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6735 mt6737t))
LOCAL_CFLAGS += -DBWC_D1
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6753)
LOCAL_CFLAGS += -DBWC_D3
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6755)
LOCAL_CFLAGS += -DBWC_J
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6750)
LOCAL_CFLAGS += -DBWC_J
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6795)
LOCAL_CFLAGS += -DBWC_RO
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6580)
LOCAL_CFLAGS += -DBWC_R
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6797)
LOCAL_CFLAGS += -DBWC_EV
else ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6757)
LOCAL_CFLAGS += -DBWC_OLY
endif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)),mt6735m)
#include $(LOCAL_PATH)/$(shell echo $(MTK_PLATFORM) | tr A-Z a-z )/Android.mk
#else
LOCAL_SRC_FILES:= \
    $(COMMON_PATH)/bandwidth_control.cpp \
    $(COMMON_PATH)/bandwidth_control_port.cpp \
    $(COMMON_PATH)/IBWCService.cpp \
    $(COMMON_PATH)/BWCService.cpp \
    $(COMMON_PATH)/BWCClient.cpp \
    $(COMMON_PATH)/BWManager.cpp \
    $(COMMON_PATH)/BWCProfileAdapter.cpp

LOCAL_C_INCLUDES:= \
  $(MTK_PATH_SOURCE)/hardware/bwc/inc \
  #$(TOP)/frameworks/base/include/ \
  #$(MTK_PATH_SOURCE)/hardware/bwc/inc \
  #$(TOP)/$(MTK_PATH_PLATFORM)/kernel/core/include/mach \
  #$(TOP)/$(MTK_PATH_PLATFORM)/hardware/bwc/inc \

LOCAL_SHARED_LIBRARIES := \
     libcutils \
     liblog \
     libnetutils \
     libutils \
     libbinder

ifeq ($(MTK_DISPLAY_120HZ_SUPPORT), yes)
LOCAL_CFLAGS += -DBWC_RRC_SUPPORT
LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/rrc/inc
LOCAL_SHARED_LIBRARIES += librrc
endif

LOCAL_MODULE := libbwc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := both

LOCAL_MODULE_TAGS := optional

#LOCAL_PRELINK_MODULE := false

include $(MTK_SHARED_LIBRARY)

include $(CLEAR_VARS)
#include $(call all-makefiles-under,$(LOCAL_PATH))


#endif
#endif

