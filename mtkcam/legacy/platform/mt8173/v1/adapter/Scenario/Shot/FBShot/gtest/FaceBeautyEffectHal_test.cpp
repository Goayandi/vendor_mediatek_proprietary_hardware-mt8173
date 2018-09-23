/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "FaceBeautyEffectHal_test"
//#define LOG_NDEBUG 0

#include <gtest/gtest.h>

#include <unistd.h>
#include <utils/String8.h>
#include <utils/threads.h>
#include <utils/Errors.h>
#include <utils/Vector.h>
#include <cutils/log.h>

#include <mtkcam/utils/Format.h>
#include "../FaceBeautyEffectHal.h"



using namespace android;


extern int testVar1;
extern int testVar2;    
extern char testFileName[128];

namespace NSCam {

class DummyEffectListener : public EffectListener {
    virtual void    onPrepared(const IEffectHalClient* effectClient, const EffectResult& result) const {
        ALOGD("!!!!!!!!!!!!!!!!!!!  onPrepared");
        ASSERT_EQ(1, result.getInt("onPrepared"));
    };
    virtual void    onInputFrameProcessed(const IEffectHalClient* effectClient, const sp<EffectParameter> parameter, EffectResult partialResult) const {
        ALOGD("onInputFrameProcessed");
        //@todo implement this
    };
    virtual void    onOutputFrameProcessed(const IEffectHalClient* effectClient, const sp<EffectParameter> parameter, EffectResult partialResult) {
        ALOGD("onOutputFrameProcessed");
        //@todo implement this
    };
    virtual void    onCompleted(const IEffectHalClient* effectClient, const EffectResult& partialResult, uint64_t uid) const {
        ALOGD("onCompleted");
        ASSERT_EQ(1, partialResult.getInt("onCompleted"));
    };
    virtual void    onAborted(const IEffectHalClient* effectClient, const EffectResult& result) const {
        ALOGD("onAborted");
        ASSERT_EQ(1, result.getInt("onAborted"));
    };
    virtual void    onFailed(const IEffectHalClient* effectClient, const EffectResult& result) const {
        ALOGD("onFailed");
        //@todo implement this
    };
};

class FaceBeautyEffectTest : public ::testing::Test {
protected:

    FaceBeautyEffectTest() {}

    virtual void SetUp() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGD("Begin test: %s.%s", testInfo->test_case_name(), testInfo->name());

        //@todo implement this
        mpEffectHal = new FaceBeautyEffectHal;
        mpEffectHalClient = new EffectHalClient(mpEffectHal.get());
        //mListener = new EffectListener();
    }

    virtual void TearDown() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGD("End test:   %s.%s", testInfo->test_case_name(), testInfo->name());

        //delete mpEffectHal;
        //delete mpEffectHalClient;
    }
    
protected:
    // IEffectHal *mpEffectHal;
    // IEffectHalClient *mpEffectHalClient;
    sp<IEffectHal> mpEffectHal;
    sp<IEffectHalClient> mpEffectHalClient;

    //sp<EffectListener> mListener;
};


TEST_F(FaceBeautyEffectTest, expectedCallSequence) {
    EffectHalVersion nameVersion;
    
    sp<DummyEffectListener> listener = new DummyEffectListener();
    
    EffectParameter parameter;
    EffectCaptureRequirement requirement;
    Vector<EffectCaptureRequirement> requirements;

    IImageBuffer *inputBuffer1;
    IImageBuffer *inputBuffer2;
    IImageBuffer *outputBuffer1;
    IImageBuffer *outputBuffer2;

    ASSERT_NE((void*)NULL, mpEffectHal.get());
    
    ASSERT_EQ(OK, mpEffectHal->getNameVersion(nameVersion));
    EXPECT_STREQ("FaceBeauty", nameVersion.effectName);
    EXPECT_EQ(1, nameVersion.major);
    EXPECT_EQ(0, nameVersion.minor);
    
    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHal->init());
    ASSERT_EQ(OK, mpEffectHal->setEffectListener(listener));
    String8 Key = String8("key1");
    String8 value = String8("1111");
    ASSERT_EQ(OK, mpEffectHal->setParameter(Key, value));
    Key = String8("key2");
    value = String8("2222");
    ASSERT_EQ(OK, mpEffectHal->setParameter(Key, value));

    // STATUS_CONFIGURED
    ASSERT_EQ(OK, mpEffectHal->configure());
    ASSERT_EQ(OK, mpEffectHalClient->getCaptureRequirement(&parameter, requirements));
ASSERT_EQ(OK, mpEffectHal->prepare());
    
    // STATUS_START
    ASSERT_LT(0, mpEffectHal->start());

    // STATUS_CONFIGURED    
    ASSERT_EQ(OK, mpEffectHal->abort());        // must after start()
    ASSERT_EQ(OK, mpEffectHal->release());      // must after abort()

    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHal->unconfigure());  // must after release()

    // STATUS_UNINIT
    ASSERT_EQ(OK, mpEffectHal->uninit());       // must after init()
}


TEST_F(FaceBeautyEffectTest, IEffectHalClientExpectedCallSequence) {
    EffectHalVersion nameVersion;
    
    sp<DummyEffectListener> listener = new DummyEffectListener();
    
    EffectParameter parameter;
    EffectCaptureRequirement requirement;
    Vector<EffectCaptureRequirement> requirements;
    /*
    IImageBuffer *inputBuffer1;
    IImageBuffer *inputBuffer2;
    IImageBuffer *outputBuffer1;
    IImageBuffer *outputBuffer2;
    */

    ASSERT_NE((void*)NULL, mpEffectHalClient.get());
    
    ASSERT_EQ(OK, mpEffectHalClient->getNameVersion(nameVersion));
    EXPECT_STREQ("FaceBeauty", nameVersion.effectName);
    EXPECT_EQ(1, nameVersion.major);
    EXPECT_EQ(0, nameVersion.minor);

    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHalClient->init());
    ASSERT_EQ(OK, mpEffectHalClient->setEffectListener(listener));
    String8 Key = String8("key1");
    String8 value = String8("1111");
    ASSERT_EQ(OK, mpEffectHal->setParameter(Key, value));
    Key = String8("key2");
    value = String8("2222");
    ASSERT_EQ(OK, mpEffectHal->setParameter(Key, value));

    // STATUS_CONFIGURED
    ASSERT_EQ(OK, mpEffectHalClient->configure());
    ASSERT_EQ(OK, mpEffectHalClient->getCaptureRequirement(&parameter, requirements));
    ASSERT_EQ(OK, mpEffectHalClient->prepare());
    
    // STATUS_START
    ASSERT_LT(0, mpEffectHalClient->start());

    //@todo implement this
    //add surface

    // STATUS_CONFIGURED    
    ASSERT_EQ(OK, mpEffectHalClient->abort());        // must after start()
    ASSERT_EQ(OK, mpEffectHalClient->release());      // must after abort()

    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHalClient->unconfigure());  // must after release()

    // STATUS_UNINIT
    ASSERT_EQ(OK, mpEffectHalClient->uninit());       // must after init()
}


TEST_F(FaceBeautyEffectTest, FaceBeautyApplied) {
    
    sp<DummyEffectListener> listener = new DummyEffectListener();
    Vector<EffectCaptureRequirement> requirements;
    EImageFormat imageFormat = eImgFmt_YV12;
    //@todo implement this
    //if (argc > 4 && strncmp(argv[4], "i422", strlen("i422")))
    {
        imageFormat = eImgFmt_I422;
    }

    // Allocate IImageBuffer
    IImageBuffer* SrcImgBuffer;
    IImageBuffer* DstImgBuffer;
    MUINT32 width = 0;
    MUINT32 height = 0;
    ASSERT_NE(0, strlen(testFileName));
    int fd = ::open(testFileName, O_RDONLY);
    ASSERT_LE(0, fd);
    ::close(fd);
    sscanf(testFileName, "%*[^'_']_%dx%d", &width, &height);
    ASSERT_NE(0, width);
    ASSERT_NE(0, height);
    //
    IImageBufferAllocator* allocator = IImageBufferAllocator::getInstance();
    MUINT32 bufStridesInBytes[3] = {0};
    MUINT32 plane = NSCam::Utils::Format::queryPlaneCount(imageFormat);
    for (MUINT32 i = 0; i < plane; i++)
    {
        bufStridesInBytes[i] = NSCam::Utils::Format::queryPlaneWidthInPixels(imageFormat,i, width) * NSCam::Utils::Format::queryPlaneBitsPerPixel(imageFormat,i) / 8;
    }
    MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
    IImageBufferAllocator::ImgParam imgParam(
            imageFormat, MSize(width,height), bufStridesInBytes, bufBoundaryInBytes, plane
            );
    SrcImgBuffer = allocator->alloc_ion(LOG_TAG, imgParam);
    ASSERT_NE((void*)NULL, SrcImgBuffer);
    ASSERT_EQ(MTRUE, SrcImgBuffer->loadFromFile(testFileName));
    ASSERT_EQ(MTRUE, SrcImgBuffer->lockBuf( LOG_TAG, eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN ));
    DstImgBuffer = allocator->alloc_ion(LOG_TAG, imgParam);
    ASSERT_NE((void*)NULL, DstImgBuffer);
    ASSERT_EQ(MTRUE, DstImgBuffer->lockBuf( LOG_TAG, eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN ));

    //
    ASSERT_NE((void*)NULL, mpEffectHal.get());
    
    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHal->init());
    ASSERT_EQ(OK, mpEffectHal->setEffectListener(listener));
    String8 Key = String8("key1");
    String8 value = String8("1111");
    ASSERT_EQ(OK, mpEffectHal->setParameter(Key, value));
    Key = String8("key2");
    value = String8("2222");
    ASSERT_EQ(OK, mpEffectHal->setParameter(Key, value));

    // STATUS_CONFIGURED
    ASSERT_EQ(OK, mpEffectHal->configure());
    ASSERT_EQ(OK, mpEffectHal->prepare());
    
    // STATUS_START
    ASSERT_LT(0, mpEffectHal->start());
    ASSERT_EQ(OK, mpEffectHal->addInputFrame(SrcImgBuffer));
    sp<EffectParameter> parameter = new EffectParameter;
    parameter->set("fb-smooth-level", 9);
    parameter->set("fb-skin-color", 9);
    parameter->set("fb-enlarge-eye", 9);
	parameter->set("fb-slim-face", 9);
    ASSERT_EQ(OK, mpEffectHal->addOutputFrame(DstImgBuffer, parameter));

    DstImgBuffer->saveToFile("/sdcard/result.yuv");
    printf("Output result_%dx%d.yuv\n", width, height);

    // STATUS_CONFIGURED    
    ASSERT_EQ(OK, mpEffectHal->abort());        // must after start()
    ASSERT_EQ(OK, mpEffectHal->release());      // must after abort()

    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHal->unconfigure());  // must after release()

    // STATUS_UNINIT
    ASSERT_EQ(OK, mpEffectHal->uninit());       // must after init()

    allocator->free(SrcImgBuffer);
    allocator->free(DstImgBuffer);
}


TEST_F(FaceBeautyEffectTest, commandArgument) {
    printf("testVar1=%d\n", testVar1);
    printf("testVar2=%d\n", testVar2);
    printf("testFileName=%s\n", testFileName);
}


TEST_F(FaceBeautyEffectTest, testVector1) {
    Vector<int> v;
    {
        int a = 1;
        int b = 2;
        int c = 3;
        v.add(a);
        v.add(b);
        v.add(c);
        a = 3;
    }
    ASSERT_EQ(v[0], 1);
}


TEST_F(FaceBeautyEffectTest, testVector2) {
    Vector<int> v;
    int a = 1;
    int b = 2;
    int c = 3;
    v.add(a);
    v.add(b);
    v.add(c);
    a = 3;
    ASSERT_EQ(v[0], 1);
}


TEST_F(FaceBeautyEffectTest, testVector3) {
    Vector<String8> v;
    {
        String8 a("1");
        String8 b("2");
        String8 c("3");
        v.add(a);
        v.add(b);
        v.add(c);
        a = "4";
        EXPECT_STREQ("4", a);
    }
    EXPECT_STREQ("1", v[0]);
    EXPECT_STREQ("2", v[1]);
}


} // namespace NSCam


#if 0   //@todo reopen this
#include <gtest/gtest.h>

//#define LOG_TAG "CameraFrameTest"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "hardware/hardware.h"
#include "hardware/camera2.h"

//#include <common/CameraDeviceBase.h>
#include <utils/StrongPointer.h>
#include <gui/CpuConsumer.h>
#include <gui/Surface.h>

#include <unistd.h>

#include "CameraStreamFixture.h"
#include "TestExtensions.h"

#define CAMERA_FRAME_TIMEOUT    1000000000 //nsecs (1 secs)
#define CAMERA_HEAP_COUNT       2 //HALBUG: 1 means registerBuffers fails
#define CAMERA_FRAME_DEBUGGING  0

using namespace android;
using namespace android::camera2;

namespace android {
namespace camera2 {
namespace tests {

static CameraStreamParams STREAM_PARAMETERS = {
    /*mFormat*/     CAMERA_STREAM_AUTO_CPU_FORMAT,
    /*mHeapCount*/  CAMERA_HEAP_COUNT
};

class CameraFrameTest
    : public ::testing::TestWithParam<int>,
      public CameraStreamFixture {

public:
    CameraFrameTest() : CameraStreamFixture(STREAM_PARAMETERS) {
        TEST_EXTENSION_FORKING_CONSTRUCTOR;

        if (!HasFatalFailure()) {
            CreateStream();
        }
    }

    ~CameraFrameTest() {
        TEST_EXTENSION_FORKING_DESTRUCTOR;

        if (mDevice.get()) {
            mDevice->waitUntilDrained();
        }
    }

    virtual void SetUp() {
        TEST_EXTENSION_FORKING_SET_UP;
    }
    virtual void TearDown() {
        TEST_EXTENSION_FORKING_TEAR_DOWN;
    }

protected:

};

TEST_P(CameraFrameTest, GetFrame) {

    TEST_EXTENSION_FORKING_INIT;

    /* Submit a PREVIEW type request, then wait until we get the frame back */
    CameraMetadata previewRequest;
    ASSERT_EQ(OK, mDevice->createDefaultRequest(CAMERA2_TEMPLATE_PREVIEW,
                                                &previewRequest));
    {
        Vector<int32_t> outputStreamIds;
        outputStreamIds.push(mStreamId);
        ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_OUTPUT_STREAMS,
                                            outputStreamIds));
        if (CAMERA_FRAME_DEBUGGING) {
            int frameCount = 0;
            ASSERT_EQ(OK, previewRequest.update(ANDROID_REQUEST_FRAME_COUNT,
                                                &frameCount, 1));
        }
    }

    if (CAMERA_FRAME_DEBUGGING) {
        previewRequest.dump(STDOUT_FILENO);
    }

    for (int i = 0; i < GetParam(); ++i) {
        ALOGV("Submitting capture request %d", i);
        CameraMetadata tmpRequest = previewRequest;
        ASSERT_EQ(OK, mDevice->capture(tmpRequest));
    }

    for (int i = 0; i < GetParam(); ++i) {
        ALOGV("Reading capture request %d", i);
        ASSERT_EQ(OK, mDevice->waitForNextFrame(CAMERA_FRAME_TIMEOUT));

        CaptureResult result;
        ASSERT_EQ(OK, mDevice->getNextResult(&result));

        // wait for buffer to be available
        ASSERT_EQ(OK, mFrameListener->waitForFrame(CAMERA_FRAME_TIMEOUT));
        ALOGV("We got the frame now");

        // mark buffer consumed so producer can re-dequeue it
        CpuConsumer::LockedBuffer imgBuffer;
        ASSERT_EQ(OK, mCpuConsumer->lockNextBuffer(&imgBuffer));
        ASSERT_EQ(OK, mCpuConsumer->unlockBuffer(imgBuffer));
    }

}

//FIXME: dont hardcode stream params, and also test multistream
INSTANTIATE_TEST_CASE_P(FrameParameterCombinations, CameraFrameTest,
    testing::Range(1, 10));


}
}
}
#endif
