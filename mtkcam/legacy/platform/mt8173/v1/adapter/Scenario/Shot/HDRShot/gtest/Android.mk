ifneq ($(MTK_BASIC_PACKAGE), yes)
# Build the unit tests,
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := hdreffecthal_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := \
    HdrEffectHal_test.cpp \
    main.cpp \
    camera3test_fixtures.cpp \
    
	# camera3test_fixtures.cpp add for start camera3 preview

LOCAL_CFLAGS := --coverage

LOCAL_LDFLAGS := --coverage

LOCAL_SHARED_LIBRARIES := \
	libcompiler_rt \
	libcutils \
	libutils \
	libcam.camadapter \
	libbinder \
	libcam_utils \
	libcamdrv \
	libhardware \
	libcamera_metadata \
	libdl  \
	
	# libhardware,libcamera_metadata, libdl add for start camera3 preview

ifneq ($(MTK_BASIC_PACKAGE), yes)
endif

LOCAL_C_INCLUDES := \
    external/gtest/include \

LOCAL_C_INCLUDES += $(MTKCAM_C_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/gralloc_extra/include
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/ext/include

LOCAL_C_INCLUDES += $(MY_ADAPTER_C_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/$(MTKCAM_C_INCLUDES)/..
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/include
#
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/external/aee/binary/inc
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/ext/include
LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/frameworks/av/services
LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/frameworks/av/services/mmsdk/include
#
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/v1/adapter/inc/Scenario
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/v1/adapter/inc/Scenario/Shot
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/v1/adapter/Scenario/Shot/inc
#
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/core/campipe/inc
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/core/camshot/inc
LOCAL_C_INCLUDES += $(TOP)/$(MTK_MTKCAM_PLATFORM)/include/mtkcam/algorithm/libhdr
#
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_CUSTOM)/hal/inc
#
LOCAL_C_INCLUDES += $(TOP)/$(MTKCAM_C_INCLUDES)/v1/camutils
LOCAL_C_INCLUDES += $(TOP)/system/media/camera/include
#

# Build the binary to $(TARGET_OUT_DATA_NATIVE_TESTS)/$(LOCAL_MODULE)
# to integrate with auto-test framework.
include $(BUILD_NATIVE_TEST)

# Include subdirectory makefiles
# ============================================================

# If we're building with ONE_SHOT_MAKEFILE (mm, mmm), then what the framework
# team really wants is to build the stuff defined by this makefile.
ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call first-makefiles-under,$(LOCAL_PATH))
endif

endif