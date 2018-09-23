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

#define LOG_TAG "HdrEffectHal_test"
//#define LOG_NDEBUG 0

#include <gtest/gtest.h>

#include <unistd.h>
#include <utils/String8.h>
#include <utils/threads.h>
#include <utils/Errors.h>
#include <utils/Vector.h>
#include <cutils/log.h>

#include <mtkcam/common.h>
#include <mtkcam/utils/Format.h>
#include <mtkcam/drv/imem_drv.h>    // link libcamdrv
#include <mtkcam/utils/ImageBufferHeap.h>
#include "../HdrEffectHal.h"

// camera3test_fixtures.h, hardware.h,  camera3.h add for start camera3 preview
#include "camera3test_fixtures.h"
#include <hardware/hardware.h>
#include <hardware/camera3.h>


using namespace android;
using namespace camera3;
using namespace tests;


extern int testVar1;
extern int testVar2;    

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


class HdrEffectTest : public ::testing::Test {
protected:

    HdrEffectTest() {}

    virtual void SetUp() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGD("Begin test: %s.%s", testInfo->test_case_name(), testInfo->name());

        //@todo implement this
        mpEffectHal = new HdrEffectHal;
        mpEffectHalClient = new EffectHalClient(mpEffectHal.get());
        mpCamera3Device = new android::camera3::tests::Camera3Device;

        //mListener = new EffectListener();
    	//1. init IMem
    	mpIMemDrv = IMemDrv::createInstance();
    	if(!mpIMemDrv) {
    		ALOGE("SetUp can't createInstance IMemDrv");
    	}
    	mpIMemDrv->init();	//check this, see fd

        mpIImageBufAllocator =  IImageBufferAllocator::getInstance();
        if (mpIImageBufAllocator == NULL)
        {
            ALOGE("mpIImageBufAllocator is NULL \n");
        }    
    }

    virtual void TearDown() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGD("End test:   %s.%s", testInfo->test_case_name(), testInfo->name());

        //delete mpEffectHal;
        //delete mpEffectHalClient;
    }
    MBOOL allocBuffer(IImageBuffer** ppBuf, MUINT32 w, MUINT32 h, MUINT32 fmt);
    void deallocBuffer(IImageBuffer* pBuf);
    // prevent to use loadFromFile (Fatal,lockBufLocked)
    // one lockBuf already in allocBuffer
    uint32_t loadFileToBuf(char const*const fname, MUINT8*  buf, MUINT32 size); 
    
protected:
    // IEffectHal *mpEffectHal;
    // IEffectHalClient *mpEffectHalClient;
    sp<IEffectHal> mpEffectHal;
    sp<IEffectHalClient> mpEffectHalClient;
    android::camera3::tests::Camera3Device * mpCamera3Device;
    //sp<EffectListener> mListener;

	IMemDrv             *mpIMemDrv;
    IImageBufferAllocator* mpIImageBufAllocator;
    typedef struct
    {
        IImageBuffer* pImgBuf;
        IMEM_BUF_INFO memBuf;
    } ImageBufferMap;
    std::vector<ImageBufferMap> mvImgBufMap;
};


TEST_F(HdrEffectTest, expectedCallSequence) {
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
    EXPECT_STREQ("Hdr", nameVersion.effectName);
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


TEST_F(HdrEffectTest, IEffectHalClientExpectedCallSequence) {
    EffectHalVersion nameVersion;
    
    sp<DummyEffectListener> listener = new DummyEffectListener();
    
    EffectParameter parameter;
    //EffectCaptureRequirement requirement;
    Vector<EffectCaptureRequirement> requirements;

    
    IImageBuffer *inputBuffer1;
    IImageBuffer *inputBuffer2;
    IImageBuffer *outputBuffer1;
    IImageBuffer *outputBuffer2;
    
	
    ASSERT_NE((void*)NULL, mpEffectHalClient.get());
    
    ASSERT_EQ(OK, mpEffectHalClient->getNameVersion(nameVersion));
    EXPECT_STREQ("Hdr", nameVersion.effectName);
    EXPECT_EQ(1, nameVersion.major);
    EXPECT_EQ(0, nameVersion.minor);

    // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHalClient->init());
    ASSERT_EQ(OK, mpEffectHalClient->setEffectListener(listener));
    //long int a = 1;

    String8 Key = String8("key1");
    String8 value = String8("1111");
    ASSERT_EQ(OK, mpEffectHalClient->setParameter(Key, value));
    Key = String8("key2");
    value = String8("2222");
    ASSERT_EQ(OK, mpEffectHalClient->setParameter(Key, value));
    
    // STATUS_CONFIGURED
    ASSERT_EQ(OK, mpEffectHalClient->configure());
    ASSERT_EQ(OK, mpEffectHalClient->getCaptureRequirement(&parameter, requirements));

    for(int i=0; i<requirements.size(); i++)
    {
        ALOGD("requirements index=%d", i);
        char str[32];
        sprintf(str, "%dx%d", (i+1)*100, (i+1)*100);   
        ASSERT_STREQ(str, requirements[i].get("picture-size"));
        ALOGD("requirements index=%d, size=%s, format=%s", i, 
                requirements[i].get("picture-size"), requirements[i].get("picture-format"));
    }
    ASSERT_EQ(OK, mpEffectHalClient->prepare());
    
    // // STATUS_START
    ASSERT_LT(0, mpEffectHalClient->start());
    
    // //@todo implement this
    // //add surface

    // // STATUS_CONFIGURED    
    ASSERT_EQ(OK, mpEffectHalClient->abort());        // must after start()
    ASSERT_EQ(OK, mpEffectHalClient->release());      // must after abort()

    // // STATUS_INIT
    ASSERT_EQ(OK, mpEffectHalClient->unconfigure());  // must after release()

    // // STATUS_UNINIT
    ASSERT_EQ(OK, mpEffectHalClient->uninit());       // must after init()
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
HdrEffectTest::
allocBuffer(IImageBuffer** ppBuf, MUINT32 w, MUINT32 h, MUINT32 fmt)
{
    MBOOL ret = MTRUE;

    IImageBuffer* pBuf = NULL;

    if( fmt != eImgFmt_JPEG )
    {
        /* To avoid non-continuous multi-plane memory, allocate ION memory and map it to ImageBuffer */
        MUINT32 plane = NSCam::Utils::Format::queryPlaneCount(fmt);
        ImageBufferMap bufMap;

        bufMap.memBuf.size = 0;
        for (MUINT32 i=0; i<plane; i++) {
            bufMap.memBuf.size += ((NSCam::Utils::Format::queryPlaneWidthInPixels(fmt,i, w) * NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt,i) + 7) / 8) * NSCam::Utils::Format::queryPlaneHeightInPixels(fmt, i, h);
        }

        if(mpIMemDrv == NULL) {
            ALOGE("null mpIMemDrv");
            return MFALSE;
        }        
        if (mpIMemDrv->allocVirtBuf(&bufMap.memBuf)) {
            ALOGE("g_pIMemDrv->allocVirtBuf() error \n");
            return MFALSE;
        }
        if (mpIMemDrv->mapPhyAddr(&bufMap.memBuf)) {
            ALOGE("mpIMemDrv->mapPhyAddr() error \n");
            return MFALSE;
        }
        ALOGD("allocBuffer at PA(%p) VA(%p) Size(0x%x)"
                , (void*)bufMap.memBuf.phyAddr
                , (void*)bufMap.memBuf.virtAddr
                , bufMap.memBuf.size
                );

        MINT32 bufBoundaryInBytes[3] = {0, 0, 0};

        MUINT32 strideInBytes[3] = {0};
        for (MUINT32 i = 0; i < plane; i++) {
            strideInBytes[i] = (NSCam::Utils::Format::queryPlaneWidthInPixels(fmt,i, w) * NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt, i) + 7) / 8;
        }
        IImageBufferAllocator::ImgParam imgParam(fmt
                                                , MSize(w,h)
                                                , strideInBytes
                                                , bufBoundaryInBytes
                                                , plane
                                                );

        PortBufInfo_v1 portBufInfo = PortBufInfo_v1(bufMap.memBuf.memID
                                                    , bufMap.memBuf.virtAddr
                                                    , bufMap.memBuf.useNoncache
                                                    , bufMap.memBuf.bufSecu
                                                    , bufMap.memBuf.bufCohe
                                                    );                                                    

        sp<ImageBufferHeap> pHeap = ImageBufferHeap::create(LOG_TAG
                                                            , imgParam
                                                            , portBufInfo
                                                            );
        if(pHeap == 0) {
            ALOGE("pHeap is NULL");
            return MFALSE;
        }

        //
        pBuf = pHeap->createImageBuffer();
        pBuf->incStrong(pBuf);

        bufMap.pImgBuf = pBuf;
        mvImgBufMap.push_back(bufMap);
    }
    else
    {
        MINT32 bufBoundaryInBytes = 0;
        IImageBufferAllocator::ImgParam imgParam(
                MSize(w,h),
                w * h * 6 / 5,  //FIXME
                bufBoundaryInBytes
                );

        if(mpIImageBufAllocator == NULL) {
            ALOGE("null mpIImageBufAllocator");
            return MFALSE;
        }      
        pBuf = mpIImageBufAllocator->alloc_ion(LOG_TAG, imgParam);
    }
    if (!pBuf || !pBuf->lockBuf( LOG_TAG, eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN ) )
    {
        ALOGE("Null allocated or lock Buffer failed\n");
        ret = MFALSE;
        
    }
    else
    {
        // flush
        pBuf->syncCache(eCACHECTRL_INVALID);  //hw->cpu
		ALOGD("allocBuffer addr(%p), width(%d), height(%d), format(0x%x)", pBuf, w, h, fmt);
        *ppBuf = pBuf;
    }
    // TODO remove this unlockbuf, if there is no loadFromFile() call behind
    //pBuf->unlockBuf(LOG_TAG);
lbExit:
    return ret;
}


/*******************************************************************************
*
*******************************************************************************/
void
HdrEffectTest::
deallocBuffer(IImageBuffer* pBuf)
{
    
    if(!pBuf) {
        ALOGD("free a null buffer");
        return;
    }

    pBuf->unlockBuf(LOG_TAG);
    switch (pBuf->getImgFormat())
    {
        case eImgFmt_JPEG:
            mpIImageBufAllocator->free(pBuf);
            break;
        //case eImgFmt_I422:
        //case eImgFmt_I420:
        default:
            pBuf->decStrong(pBuf);
            for (std::vector<ImageBufferMap>::iterator it = mvImgBufMap.begin();
                    it != mvImgBufMap.end();
                    it++)
            {
                if (it->pImgBuf == pBuf)
                {
                    mpIMemDrv->unmapPhyAddr(&it->memBuf);
                    if (mpIMemDrv->freeVirtBuf(&it->memBuf))
                    {
                        ALOGE("m_pIMemDrv->freeVirtBuf() error");
                    }
                    else
                    {
                        mvImgBufMap.erase(it);
                    }
                    break;
                }
            }
    }

    pBuf = NULL;

}
#if 1
uint32_t
HdrEffectTest::
loadFileToBuf(char const*const fname, MUINT8*  buf, MUINT32 size)
{
    int nr, cnt = 0;
    uint32_t readCnt = 0;

    ALOGD("opening file [%s]\n", fname);
    int fd = ::open(fname, O_RDONLY);
    if (fd < 0) {
        ALOGE("failed to create file [%s]: %s", fname, strerror(errno));
        return readCnt;
    }
    //
    if (size == 0) {
        size = ::lseek(fd, 0, SEEK_END);
        ::lseek(fd, 0, SEEK_SET);
    }
    //
    ALOGD("read %d bytes from file [%s]\n", size, fname);
    while (readCnt < size) {
        nr = ::read(fd,
                    buf + readCnt,
                    size - readCnt);
        if (nr < 0) {
            ALOGE("failed to read from file [%s]: %s",
                        fname, strerror(errno));
            break;
        }
        if (nr == 0) {
            ALOGE("can't read from file [%s]", fname);
            break;
        }
        readCnt += nr;
        cnt++;
    }
    ALOGD("done reading %d bytes to file [%s] in %d passes\n", size, fname, cnt);
    ::close(fd);

    return readCnt;
}
#endif


TEST_F(HdrEffectTest, HDRApplied) {
    
    ALOGD("HDRApplied +");

    EffectHalVersion nameVersion;
    sp<DummyEffectListener> listener = new DummyEffectListener();
    EffectParameter parameter;
    Vector<EffectCaptureRequirement> vCapRrequirements;
    char szSrcFileName[6][100];
    char szResultFileName[100];

//  camera 3 preview start
    if(mpCamera3Device == NULL){
        ALOGE("mpCamera3Device is NULL \n");
    } else {
        ALOGD("mpCamera3Device SetUp");
        mpCamera3Device->SetUp();
        
        ALOGD("mpCamera3Device getNumOfCams:%d",mpCamera3Device->getNumOfCams());
        int id = 0;
        for (id = 0; id < mpCamera3Device->getNumOfCams(); id++) {
            if (!mpCamera3Device->isHal3Supported(id)) 
                break;
        } 
        ALOGD("mpCamera3Device id:%d",id);
            mpCamera3Device->openCamera(0);
            //Camera init with callback
        ALOGD("mpCamera3Device init");
            mpCamera3Device->init();

        for (int i = CAMERA3_TEMPLATE_PREVIEW; i < CAMERA3_TEMPLATE_COUNT; i++) {
            const camera_metadata_t *request = /*mpCamera3Device->getCam3Device()*/mpCamera3Device->mDevice->ops->construct_default_request_settings(mpCamera3Device->mDevice, i);
            EXPECT_TRUE(request != NULL);
            EXPECT_LT((size_t)0, get_camera_metadata_entry_count(request));
            EXPECT_LT((size_t)0, get_camera_metadata_data_count(request));
            
            ALOGD("Template type %d:",i);
            dump_indented_camera_metadata(request, 0, 2, 4);
        }
                   
    }
// camera 3 preview end


    ASSERT_NE((void*)NULL, mpEffectHal.get());
        
    ASSERT_EQ(OK, mpEffectHal->init());    
    #if 0
    mpEffectHal->getNameVersion(nameVersion);
    ALOGD("getNameVersion effectName:%s",nameVersion.effectName.string());
    ALOGD("getNameVersion major:%d",nameVersion.major);
    ALOGD("getNameVersion minor:%d",nameVersion.minor);
    #endif
    
    ALOGD("HdrEffectTest setEffectListener");
    ASSERT_EQ(OK, mpEffectHalClient->setEffectListener(listener));

    String8 pic_w = String8("4096");
    String8 pic_h = String8("2304");
    String8 pic_format = String8("jpeg");
    String8 th_w = String8("320");
    String8 th_h = String8("240");
    String8 th_q = String8("90");
    String8 jpeg_q = String8("90");
    String8 rot = String8("0");
    String8 zoom = String8("100");
    String8 sensor_capture_width = String8("4192");
    String8 sensor_capture_height = String8("3104");

    android::String8 invaludeparam("invaludeparam");
    android::String8 EFFECTHAL_KEY_PICTURE_WIDTH("picture-width");
    android::String8 EFFECTHAL_KEY_PICTURE_HEIGHT("picture-height");
    // Supported image formats for captured pictures.
    // Example value: "jpeg,rgb565". Read only.
    android::String8 EFFECTHAL_KEY_PICTURE_FORMAT("picture-format");
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_WIDTH("jpeg-thumbnail-width");
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_HEIGHT("jpeg-thumbnail-height");
    // Thumbnail quality of captured picture. The range is 1 to 100, with 100 being
    // the best.
    // Example value: "90". Read/write.
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_QUALITY("jpeg-thumbnail-quality");
    // Jpeg quality of captured picture. The range is 1 to 100, with 100 being
    // the best.
    // Example value: "90". Read/write.
    android::String8 EFFECTHAL_KEY_JPEG_QUALITY("jpeg-quality");
    android::String8 EFFECTHAL_KEY_ROTATION("rotation");
    // Current zoom value, with 100 being zoom x1
    android::String8 EFFECTHAL_KEY_ZOOM("zoom");
    android::String8 EFFECTHAL_KEY_SENSOR_TYPE("sensor-type");
    android::String8 EFFECTHAL_KEY_SENSOR_CAPTURE_WIDTH("sensor_capture_width");   
    android::String8 EFFECTHAL_KEY_SENSOR_CAPTURE_HEIGHT("sensor_capture_height"); 

    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_PICTURE_WIDTH, pic_w));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_PICTURE_HEIGHT, pic_h));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_PICTURE_FORMAT, pic_format));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_JPEG_THUMBNAIL_WIDTH, th_w));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_JPEG_THUMBNAIL_HEIGHT, th_h));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_JPEG_THUMBNAIL_QUALITY, th_q));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_JPEG_QUALITY, jpeg_q));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_ROTATION, rot));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_ZOOM, zoom));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_SENSOR_TYPE, zoom));
    //ASSERT_EQ(OK, mpEffectHal->setParameter(invaludeparam, (uintptr_t)&zoom));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_SENSOR_CAPTURE_WIDTH, sensor_capture_width));
    ASSERT_EQ(OK, mpEffectHal->setParameter(EFFECTHAL_KEY_SENSOR_CAPTURE_HEIGHT, sensor_capture_height));
        
    ASSERT_EQ(OK, mpEffectHal->configure());
    ASSERT_EQ(OK, mpEffectHal->prepare());
    
    
    //android::String8 EFFECTHAL_KEY_HDR_FRAME_INDEX("hdr-frame-index");  
    android::String8 EFFECTHAL_KEY_EXPOSURE_TIME("exposure-time");  
    android::String8 EFFECTHAL_KEY_ISO("iso");  
    android::String8 EFFECTHAL_KEY_FLARE_OFFSET("flare-offset");
    
    android::String8 EFFECTHAL_KEY_SURFACE_ID("surface-id");          
    android::String8 EFFECTHAL_KEY_SURFACE_FORMAT("surface-format");
    android::String8 EFFECTHAL_KEY_SURFACE_WIDTH("surface-width");  
    android::String8 EFFECTHAL_KEY_SURFACE_HEIGHT("surface-height"); 
    
    ALOGD("HdrEffectTest getCaptureRequirement for surface info");
    // TODO, the EV0 information should from META data.
    //parameter.set(EFFECTHAL_KEY_ISO.string(),199);
    //parameter.set(EFFECTHAL_KEY_EXPOSURE_TIME.string(),20000);
    //ASSERT_EQ(OK, mpEffectHal->getCaptureRequirement(requirements));
    ASSERT_EQ(OK, mpEffectHalClient->getCaptureRequirement(NULL, vCapRrequirements));
    ALOGD("HdrEffectTest getCaptureRequirement size:%d",vCapRrequirements.size());
    // TODO: get capture should reply: large , small wxh, and format


    MUINT32 u4SurfaceIndex[6];
    EImageFormat InputImageFormat[6];
    MUINT32 InputImageWidth[6] ;
    MUINT32 InputImageHeight[6] ;

    for (MUINT32 i = 0; i < vCapRrequirements.size(); i++)
    {        
        u4SurfaceIndex[i]     =  vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_ID.string());
        InputImageFormat[i] = (EImageFormat)vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_FORMAT.string());
        InputImageWidth[i]  =  vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_WIDTH.string());
        InputImageHeight[i] =  vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_HEIGHT.string());
        ALOGD("Surface[%d] Format[%d] size (%dX%d)",u4SurfaceIndex[i],InputImageFormat[i],InputImageWidth[i],InputImageHeight[i]);
    }
    #if 0
    for (Vector<EffectCaptureRequirement>::iterator it = vCapRrequirements.begin();it != vCapRrequirements.end();it++)
    {
        it->dump();
    } 
    #endif
    vCapRrequirements.clear();
    ALOGD("HdrEffectTest getCaptureRequirement");
    ASSERT_EQ(OK, mpEffectHalClient->getCaptureRequirement(NULL, vCapRrequirements));
    ALOGD("HdrEffectTest getCaptureRequirement size:%d",vCapRrequirements.size());

    MUINT32 u4Eposuretime[6];
    MUINT32 u4RealISO[6];
    MUINT32 u4FlareOffset[6];

    for (MUINT32 i = 0; i < vCapRrequirements.size(); i++)
    {        
        u4SurfaceIndex[i]  =  vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_ID.string());
        u4Eposuretime[i] = vCapRrequirements[i].getInt(EFFECTHAL_KEY_EXPOSURE_TIME.string());
        u4RealISO[i]     = vCapRrequirements[i].getInt(EFFECTHAL_KEY_ISO.string());
        u4FlareOffset[i] = vCapRrequirements[i].getInt(EFFECTHAL_KEY_FLARE_OFFSET.string());
        InputImageFormat[i] = (EImageFormat)vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_FORMAT.string());
        InputImageWidth[i]  =  vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_WIDTH.string());
        InputImageHeight[i] =  vCapRrequirements[i].getInt(EFFECTHAL_KEY_SURFACE_HEIGHT.string());
        ALOGD("u4SurfaceIndex[%d]: exptime:%d, ISO:%d, FlareOffset:%d ",u4SurfaceIndex[i],u4Eposuretime[i],u4RealISO[i],u4FlareOffset[i]);
        ALOGD("Surface[%d] Format[%d] size (%dX%d)",u4SurfaceIndex[i],InputImageFormat[i],InputImageWidth[i],InputImageHeight[i]);
    }    
    IImageBuffer* SrcImgBuffer[6];
    EImageFormat DstimageFormat = eImgFmt_YUY2;
    MUINT32 DstimageWidth = 4096;
    MUINT32 DstimageHeight = 2304;
    IImageBuffer* DstImgBuffer;
 
    ALOGD("allocate source buffer");

    for (MUINT32 i = 0; i < vCapRrequirements.size(); i++)
    {        
        ALOGD("allocBuffer [%d] , InputImageWidth[%d], InputImageHeight[%d], InputImageFormat[%d]",i, InputImageWidth[i], InputImageHeight[i], InputImageFormat[i]);
   
        allocBuffer(&SrcImgBuffer[i], InputImageWidth[i], InputImageHeight[i], InputImageFormat[i]);
        ASSERT_NE((void*)NULL, SrcImgBuffer[i]);
        if((i%2) == 0){
            sprintf(szSrcFileName[i], "/sdcard/input/mpSourceImgBuf[%d]_%dx%d.i420",i,InputImageWidth[i],InputImageHeight[i]);
        } else {
            sprintf(szSrcFileName[i], "/sdcard/input/mpSourceImgBuf[%d]_%dx%d.y",i,InputImageWidth[i],InputImageHeight[i]);
        }
        
    }
    
    // fill src buffer
    for (MUINT32 i = 0; i < vCapRrequirements.size(); i++)
    {               
        if((i%2) == 0){
            loadFileToBuf(szSrcFileName[i],(MUINT8*) SrcImgBuffer[i]->getBufVA(0),SrcImgBuffer[i]->getBufSizeInBytes(0)+SrcImgBuffer[i]->getBufSizeInBytes(1)+SrcImgBuffer[i]->getBufSizeInBytes(2));
        }else{
            loadFileToBuf(szSrcFileName[i],(MUINT8*) SrcImgBuffer[i]->getBufVA(0),SrcImgBuffer[i]->getBufSizeInBytes(0));
        }
    }
    
    ALOGD("allocate dst buffer");
    allocBuffer(&DstImgBuffer, DstimageWidth, DstimageHeight, DstimageFormat);
    //ASSERT_EQ(MTRUE, DstImgBuffer->lockBuf( LOG_TAG, eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN ));
    ASSERT_NE((void*)NULL, DstImgBuffer);
    //
    // TODO: should set output format in parameter



    ALOGD("HdrEffectTest start");
    //ASSERT_EQ(OK, mpEffectHal->start()); 
    ASSERT_LT(0, mpEffectHal->start());

    // add dymmy_OutputFrame_param to prevent NE in onOutputFrameProcessed()
    sp<EffectParameter> dymmy_OutputFrame_param = new EffectParameter;  
    ALOGD("HdrEffectTest addOutputFrame");
    ASSERT_EQ(OK, mpEffectHal->addOutputFrame(DstImgBuffer,dymmy_OutputFrame_param));

#if 1  // TEST

    ALOGD("HdrEffectTest addInputFrame");

    // EFFECTHAL_KEY_HDR_FRAME_INDEX: 0 , 1 , 2 by sequence

    sp<EffectParameter> surface_id = new EffectParameter;

    for (MUINT32 i = 0; i < vCapRrequirements.size(); i++)
    {        
        surface_id->set(EFFECTHAL_KEY_SURFACE_ID.string(),i);  
        ASSERT_EQ(OK, mpEffectHal->addInputFrame(SrcImgBuffer[i],surface_id));
    }
#endif
    
    
    ALOGD("HdrEffectTest sleep");
    sleep(5); //wait 1 sec for AE stable
    

    ::sprintf(szResultFileName, "/sdcard/0000_10_DstImgBuffer_%dx%d.yuy2", DstimageWidth, DstimageHeight);
    DstImgBuffer->saveToFile(szResultFileName);
    
    mpEffectHal->abort();
    mpEffectHal->release();
    mpEffectHal->unconfigure();  // must after release()
    mpEffectHal->uninit();

#if 1 //TODO, should free
    ALOGD("deallocBuffer DstImgBuffer");
    for (MUINT32 i = 0; i < vCapRrequirements.size(); i++)
    {        
        deallocBuffer(SrcImgBuffer[i]);
    }
    ALOGD("deallocBuffer DstImgBuffer");
    deallocBuffer(DstImgBuffer);
#endif
    
    //  camera 3 preview start

    if(mpCamera3Device){
    ALOGD("closeCamera");
        mpCamera3Device->closeCamera();
    //ALOGD("TearDown");
        //mpCamera3Device->TearDown();
    }
    
    //  camera 3 preview end

    ALOGD("HDRApplied -");
}


TEST_F(HdrEffectTest, commandArgument) {
    printf("testVar1=%d\n", testVar1);
    printf("testVar2=%d\n", testVar2);
}


TEST_F(HdrEffectTest, testVector1) {
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


TEST_F(HdrEffectTest, testVector2) {
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


TEST_F(HdrEffectTest, testVector3) {
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
        ALOGD("Submitting capture request %d", i);
        CameraMetadata tmpRequest = previewRequest;
        ASSERT_EQ(OK, mDevice->capture(tmpRequest));
    }

    for (int i = 0; i < GetParam(); ++i) {
        ALOGD("Reading capture request %d", i);
        ASSERT_EQ(OK, mDevice->waitForNextFrame(CAMERA_FRAME_TIMEOUT));

        CaptureResult result;
        ASSERT_EQ(OK, mDevice->getNextResult(&result));

        // wait for buffer to be available
        ASSERT_EQ(OK, mFrameListener->waitForFrame(CAMERA_FRAME_TIMEOUT));
        ALOGD("We got the frame now");

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
