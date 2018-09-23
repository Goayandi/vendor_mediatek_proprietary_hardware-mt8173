/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */
#define LOG_TAG "TestBMDeNoisePipe"
#include <mtkcam/utils/std/Log.h>

#include <time.h>
#include <gtest/gtest.h>

#include <vector>
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <utils/Vector.h>
#include <utils/String8.h>
#include <cutils/properties.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/feature/stereo/effecthal/BMDeNoiseEffectHal.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>

#include "TestBMDeNoise_Common.h"



using android::sp;
using android::String8;
using namespace NSCam;

namespace NSCam{
namespace NSCamFeature{
namespace NSFeaturePipe{

typedef IImageBufferAllocator::ImgParam ImgParam;
StereoSizeProvider* gpSizePrvder = StereoSizeProvider::getInstance();

#define DECLARE_MACRO()\
    sp<EffectFrameInfo> effectFrame;\
    MUINT32 reqIdx;

#define ADD_METABUF_FRAME(effectReq, frameMapName, frameId, rpMetaBuf) \
    rpMetaBuf = new IMetadata();\
    reqIdx = effectReq->getRequestNo();\
    effectFrame = new EffectFrameInfo(reqIdx, frameId); \
    {\
        sp<EffectParameter> effParam = new EffectParameter(); \
        effParam->setPtr(BMDENOISE_EFFECT_PARAMS_KEY, (void*)rpMetaBuf); \
        effectFrame->setFrameParameter(effParam); \
        effectReq->frameMapName.add(frameId, effectFrame); \
    }

#define ADD_METABUF_INPUT_FRAME(effectReq, frameId, rpMetaBuf)  \
    ADD_METABUF_FRAME(effectReq, vInputFrameInfo, frameId, rpMetaBuf)

#define ADD_METABUF_OUTPUT_FRAME(effectReq, frameId, rpMetaBuf)  \
    ADD_METABUF_FRAME(effectReq, vOutputFrameInfo, frameId, rpMetaBuf)


MVOID loadImgBuf(
    sp<IImageBuffer>& rpFSMain1ImgBuf,
    sp<IImageBuffer>& rpFSMain2ImgBuf,
    sp<IImageBuffer>& rpDisparityMain1Imgbuf,
    sp<IImageBuffer>& rpDisparityMain2Imgbuf,
    sp<IImageBuffer>& rpWarppingMatrixImgBuf)
{
    const char * sMain1FSFilename = "/sdcard/vsdof/test/full_raw_4208x3120_5260.raw";
    const char * sMain2FSFilename = "/sdcard/vsdof/test/full_raw_4208x3120_5260.raw";
    const char * sMain1DisparityFilename = "/sdcard/vsdof/test/full_raw_4208x3120_5260.raw";
    const char * sMain2DisparityFilename = "/sdcard/vsdof/test/full_raw_4208x3120_5260.raw";
    const char * sWarppingMatrixFilename = "/sdcard/vsdof/test/full_raw_4208x3120_5260.raw";

    {
        MUINT32 bufStridesInBytes[3] = {5260, 0, 0};
        MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
        IImageBufferAllocator::ImgParam imgParam = IImageBufferAllocator::ImgParam((eImgFmt_BAYER10), MSize(4208, 3120), bufStridesInBytes, bufBoundaryInBytes, 1);

        rpFSMain1ImgBuf = createImageBufferFromFile(imgParam, sMain1FSFilename, "FSRaw1",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        EXPECT_TRUE(rpFSMain1ImgBuf != nullptr)<< "rpFSMain1ImgBuf != nullptr";
    }

    {
        MUINT32 bufStridesInBytes[3] = {5260, 0, 0};
        MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
        IImageBufferAllocator::ImgParam imgParam = IImageBufferAllocator::ImgParam((eImgFmt_BAYER10), MSize(4208, 3120), bufStridesInBytes, bufBoundaryInBytes, 1);

        rpFSMain2ImgBuf = createImageBufferFromFile(imgParam, sMain2FSFilename, "FSRaw2",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        EXPECT_TRUE(rpFSMain2ImgBuf != nullptr)<< "rpFSMain2ImgBuf != nullptr";
    }

    {
        MUINT32 bufStridesInBytes[3] = {5260, 0, 0};
        MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
        IImageBufferAllocator::ImgParam imgParam = IImageBufferAllocator::ImgParam((eImgFmt_BAYER10), MSize(4208, 3120), bufStridesInBytes, bufBoundaryInBytes, 1);

        rpDisparityMain1Imgbuf = createImageBufferFromFile(imgParam, sMain1DisparityFilename, "Disparity1",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        EXPECT_TRUE(rpDisparityMain1Imgbuf != nullptr)<< "rpDisparityMain1Imgbuf != nullptr";
    }

    {
        MUINT32 bufStridesInBytes[3] = {5260, 0, 0};
        MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
        IImageBufferAllocator::ImgParam imgParam = IImageBufferAllocator::ImgParam((eImgFmt_BAYER10), MSize(4208, 3120), bufStridesInBytes, bufBoundaryInBytes, 1);

        rpDisparityMain2Imgbuf = createImageBufferFromFile(imgParam, sMain2DisparityFilename, "Disparity2",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        EXPECT_TRUE(rpDisparityMain2Imgbuf != nullptr)<< "rpDisparityMain2Imgbuf != nullptr";
    }

    {
        MUINT32 bufStridesInBytes[3] = {5260, 0, 0};
        MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
        IImageBufferAllocator::ImgParam imgParam = IImageBufferAllocator::ImgParam((eImgFmt_BAYER10), MSize(4208, 3120), bufStridesInBytes, bufBoundaryInBytes, 1);

        rpWarppingMatrixImgBuf = createImageBufferFromFile(imgParam, sWarppingMatrixFilename, "warppingMatrix",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        EXPECT_TRUE(rpWarppingMatrixImgBuf != nullptr)<< "rpWarppingMatrixImgBuf != nullptr";
    }
}


sp<EffectRequest> generateEffectRequest(int request_id, BMDeNoisePipeRequestType eType, MINT32 gSensorRotation)
{
    MY_LOGD("+: reqID = %d, request state=%d", request_id, eType);
    DECLARE_MACRO();

    sp<EffectRequest> pEffectReq = new EffectRequest(request_id, WaitingListener::CB);
    sp<EffectParameter> pEffectParam = new EffectParameter();

    pEffectParam->set(BMDENOISE_REQUEST_TYPE_KEY, (int)eType);
    // pEffectParam->set(BMDENOISE_G_SENSOR_KEY, (int)gSensorRotation); => no need, simply get this value from metadata

    pEffectReq->setRequestParameter(pEffectParam);

    sp<IImageBuffer> buf_fs_Main1;
    sp<IImageBuffer> buf_fs_Main2;
    sp<IImageBuffer> buf_disparityMap_Main1;
    sp<IImageBuffer> buf_disparityMap_Main2;
    sp<IImageBuffer> buf_warppingMatrix;

    // load image buffers
    loadImgBuf(buf_fs_Main1, buf_fs_Main2, buf_disparityMap_Main1, buf_disparityMap_Main2, buf_warppingMatrix);
    MY_LOGD("buf_fs_Main1=%p, buf_fs_Main2=%p, buf_disparityMap_Main1=%p, buf_disparityMap_Main2=%p, buf_warppingMatrix=%p",
        buf_fs_Main1.get(),
        buf_fs_Main2.get(),
        buf_disparityMap_Main1.get(),
        buf_disparityMap_Main2.get(),
        buf_warppingMatrix.get()
    );

    EXPECT_EQ(true, buf_fs_Main1 != nullptr) << "buf_fs_Main1";
    EXPECT_EQ(true, buf_fs_Main2 != nullptr) << "buf_fs_Main2";
    EXPECT_EQ(true, buf_disparityMap_Main1 != nullptr) << "buf_disparityMap_Main1";
    EXPECT_EQ(true, buf_disparityMap_Main2 != nullptr) << "buf_disparityMap_Main2";
    EXPECT_EQ(true, buf_warppingMatrix != nullptr) << "buf_warppingMatrix";

    // prepare input frame info: FSRAW1
    sp<EffectFrameInfo> frameInfo = new EffectFrameInfo(request_id, BID_PRE_PROCESS_IN_FULLRAW_1);
    frameInfo->setFrameBuffer(buf_fs_Main1);
    pEffectReq->vInputFrameInfo.add(BID_PRE_PROCESS_IN_FULLRAW_1, frameInfo);

    // prepare input frame info: FSRAW2
    frameInfo = new EffectFrameInfo(request_id, BID_PRE_PROCESS_IN_FULLRAW_2);
    frameInfo->setFrameBuffer(buf_fs_Main2);
    pEffectReq->vInputFrameInfo.add(BID_PRE_PROCESS_IN_FULLRAW_2, frameInfo);

    // prepare input frame info: disparity_map
    frameInfo = new EffectFrameInfo(request_id, BID_DENOISE_IN_DISPARITY_MAP_1);
    frameInfo->setFrameBuffer(buf_disparityMap_Main1);
    pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_DISPARITY_MAP_1, frameInfo);

    // prepare input frame info: waping_matrix
    frameInfo = new EffectFrameInfo(request_id, BID_DENOISE_IN_WARPING_MATRIX);
    frameInfo->setFrameBuffer(buf_warppingMatrix);
    pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_WARPING_MATRIX, frameInfo);

    {
        IMetadata* pMetadata;
        ADD_METABUF_INPUT_FRAME(pEffectReq, BID_META_IN_APP, pMetadata);
        trySetMetadata<MINT32>(pMetadata, MTK_JPEG_ORIENTATION, 0);
    }

    {
        IMetadata* pMetadata;
        ADD_METABUF_INPUT_FRAME(pEffectReq, BID_META_IN_HAL, pMetadata);
        trySetMetadata<MINT32>(pMetadata, MTK_JPEG_ORIENTATION, 0);
    }

    {
        IMetadata* pMetadata;
        ADD_METABUF_INPUT_FRAME(pEffectReq, BID_META_IN_HAL_MAIN2, pMetadata);
        trySetMetadata<MINT32>(pMetadata, MTK_JPEG_ORIENTATION, 0);
    }

    // // InHalMeta-EIS region
    // IMetadata::IEntry entry(MTK_EIS_REGION);
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(0, Type2Type< MINT32 >());
    // // the following is  MVtoCenterX, MVtoCenterY, IsFromRRZ
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(0, Type2Type< MINT32 >());
    // entry.push_back(MTRUE, Type2Type< MBOOL >());
    // pMetadata->update(MTK_EIS_REGION, entry);

    // prepare output frame
    ImgParam imgParam_DeNoiseResultYUV = getImgParam_BMDENOISE_final_result();
    // ImgParam imgParam_DeNoiseResultThumb = getImgParam_MV_F_CAP();

    frameInfo = new EffectFrameInfo(request_id, BID_DENOISE_FINAL_RESULT);
    sp<IImageBuffer> buf = createEmptyImageBuffer(imgParam_DeNoiseResultYUV, "BMDeNoiseResult",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
    frameInfo->setFrameBuffer(buf);
    pEffectReq->vOutputFrameInfo.add(BID_DENOISE_FINAL_RESULT, frameInfo);

    MY_LOGD("-");
    return pEffectReq;
}

void HalSensorInit(){
    NSCam::IHalSensorList* pHalSensorList = MAKE_HalSensorList();
    pHalSensorList->searchSensors();
    int sensorCount = pHalSensorList->queryNumberOfSensors();

    MY_LOGD("sensorCount:%d", sensorCount);
    printf("sensorCount:%d", sensorCount);
}

TEST(BMDeNoiseEffectHalTest, StandardTest)
{
    printf("StandardTest +\n");

    HalSensorInit();

    MY_LOGD("StereoSettingProvider::stereoProfile() = STEREO_SENSOR_PROFILE_REAR_FRONT...");
    printf("StereoSettingProvider::stereoProfile() = STEREO_SENSOR_PROFILE_REAR_FRONT...\n");
    //StereoSettingProvider::stereoProfile() = STEREO_SENSOR_PROFILE_REAR_FRONT;
    StereoSettingProvider::setStereoProfile(STEREO_SENSOR_PROFILE_REAR_FRONT);

    MY_LOGD("Create BMDeNoiseEffectHal...");
    printf("Create BMDeNoiseEffectHal...\n");

    BMDeNoiseEffectHal* pBMDeNoiseEffectHal = new BMDeNoiseEffectHal();
    pBMDeNoiseEffectHal->init();

    int main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);

    MY_LOGD("getStereoSensorIndex: (%d, %d)", main1Idx, main2Idx);
    printf("getStereoSensorIndex: (%d, %d)\n", main1Idx, main2Idx);

    sp<EffectParameter> effParam = new EffectParameter();
    effParam->set(SENSOR_IDX_MAIN1, main1Idx); // string to value pair
    effParam->set(SENSOR_IDX_MAIN2, main2Idx);
    pBMDeNoiseEffectHal->setParameters(effParam);

    MY_LOGD("Prepare EffectRequests...");
    printf("Prepare EffectRequests...\n");

    MINT32 regIdx = 0;
    sp<EffectRequest> pEffReqRaw    = generateEffectRequest(regIdx, TYPE_PREPROCESS, 0);
    sp<EffectRequest> pEffReqWaping = generateEffectRequest(regIdx+1000, TYPE_WARPING_MATRIX, 0);

    MY_LOGD("BMDeNoiseEffectHal configure...");
    printf("BMDeNoiseEffectHal configure...\n");
    EXPECT_EQ(OK, pBMDeNoiseEffectHal->configure()) << "pBMDeNoiseEffectHal->configure";

    MY_LOGD("BMDeNoiseEffectHal start...");
    printf("BMDeNoiseEffectHal start...\n");
    pBMDeNoiseEffectHal->start();

    MY_LOGD("BMDeNoiseEffectHal updateEffectRequest...");
    printf("BMDeNoiseEffectHal updateEffectRequest...\n");
    pBMDeNoiseEffectHal->updateEffectRequest(pEffReqRaw);
    pBMDeNoiseEffectHal->updateEffectRequest(pEffReqWaping);

    MY_LOGD("BMDeNoiseEffectHal start to wait!!....");
    printf("BMDeNoiseEffectHal start to wait!!....\n");
    bool bRet = WaitingListener::waitRequest(2,30);
    MY_LOGD("Wait done!!....");
    EXPECT_TRUE(bRet);

    WaitingListener::resetCounter();

    printf("StandardTest -\n");
}

}
}
}



