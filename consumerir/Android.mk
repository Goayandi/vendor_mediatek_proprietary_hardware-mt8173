# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(strip $(MTK_IRTX_SUPPORT)),yes)
        install_consumerir := yes
else
ifeq ($(strip $(MTK_IRTX_PWM_SUPPORT)),yes)
        install_consumerir := yes
else
ifeq ($(strip $(MTK_IR_LEARNING_SUPPORT)),yes)
        install_consumerir := yes
else
        install_consumerir := no
endif
endif
endif

ifeq ($(strip $(install_consumerir)), yes)

$(warning consumerir.$(TARGET_BOARD_PLATFORM) is built)

LOCAL_C_INCLUDES := $(TOPDIR)/hardware/libhardware_legacy/include \
                    $(TOPDIR)/hardware/libhardware/include \
                    $(LOCAL_PATH)/include \

LOCAL_MODULE := consumerir.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := src/consumerir.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := both

include $(MTK_SHARED_LIBRARY)

endif