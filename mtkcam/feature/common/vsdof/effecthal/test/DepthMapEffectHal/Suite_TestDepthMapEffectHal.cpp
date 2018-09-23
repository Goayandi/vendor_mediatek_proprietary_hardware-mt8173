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
#define LOG_TAG "TestDepthMapPipe"

#include <time.h>
#include <gtest/gtest.h>

#include <vector>
#include <mtkcam/aaa/IHal3A.h>
#include <utils/Vector.h>
#include <utils/String8.h>
#include <cutils/properties.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/feature/stereo/effecthal/DepthMapEffectHal.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/drv/IHalSensor.h>
#include "TestDepthMap_Common.h"

#include <mtkcam/utils/std/Log.h>


using android::sp;
using android::String8;
using namespace NSCam;

namespace NSCam{
namespace NSCamFeature{
namespace NSFeaturePipe{

typedef IImageBufferAllocator::ImgParam ImgParam;
StereoSizeProvider* gpSizePrvder = StereoSizeProvider::getInstance();


MVOID loadFullResizRawsImgBuf(sp<IImageBuffer>& rpFSMain1ImgBuf, sp<IImageBuffer>& rpRSMain1ImgBuf, sp<IImageBuffer>& rpRSMain2ImgBuf)
{
    const char * sMain1Filename = "/sdcard/vsdof/BID_P2AFM_IN_RSRAW1_2100x1180_3944.raw";
    const char * sMain2Filename = "/sdcard/vsdof/BID_P2AFM_IN_RSRAW2_1280x720_2400.raw";
    const char * sMain1FSFilename = "/sdcard/vsdof/BID_P2AFM_IN_FSRAW1_4208x3120_5260.raw";

    MUINT32 bufStridesInBytes[3] = {3944, 0, 0};
    MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
    IImageBufferAllocator::ImgParam imgParamRRZO_Main1 = IImageBufferAllocator::ImgParam((eImgFmt_FG_BAYER10), MSize(2100, 1180), bufStridesInBytes, bufBoundaryInBytes, 1);

    MUINT32 bufStridesInBytes2[3] = {2400, 0, 0};
    IImageBufferAllocator::ImgParam imgParamRRZO_Main2 = IImageBufferAllocator::ImgParam((eImgFmt_FG_BAYER10), MSize(1280, 720), bufStridesInBytes2, bufBoundaryInBytes, 1);

    MUINT32 bufStridesInBytesFS[3] = {5260, 0, 0};
    MINT32 bufBoundaryInBytesFS[3] = {0, 0, 0};
    IImageBufferAllocator::ImgParam imgParamIMGO = IImageBufferAllocator::ImgParam((eImgFmt_BAYER10), MSize(4208, 3120), bufStridesInBytesFS, bufBoundaryInBytesFS, 1);

    rpRSMain1ImgBuf = createImageBufferFromFile(imgParamRRZO_Main1, sMain1Filename, "RsRaw1",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
    rpRSMain2ImgBuf = createImageBufferFromFile(imgParamRRZO_Main2, sMain2Filename, "RsRaw2",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
    rpFSMain1ImgBuf = createImageBufferFromFile(imgParamIMGO, sMain1FSFilename, "FsRaw",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
}


#define DECLARE_MACRO()\
    sp<EffectFrameInfo> effectFrame;\
    MUINT32 reqIdx;

#define ADD_METABUF_FRAME(effectReq, frameMapName, frameId, rpMetaBuf) \
    rpMetaBuf = new IMetadata();\
    reqIdx = effectReq->getRequestNo();\
    effectFrame = new EffectFrameInfo(reqIdx, frameId); \
    {\
        sp<EffectParameter> effParam = new EffectParameter(); \
        effParam->setPtr(VSDOF_PARAMS_METADATA_KEY, (void*)rpMetaBuf); \
        effectFrame->setFrameParameter(effParam); \
        effectReq->frameMapName.add(frameId, effectFrame); \
    }

#define ADD_METABUF_INPUT_FRAME(effectReq, frameId, rpMetaBuf)  \
    ADD_METABUF_FRAME(effectReq, vInputFrameInfo, frameId, rpMetaBuf)

#define ADD_METABUF_OUTPUT_FRAME(effectReq, frameId, rpMetaBuf)  \
    ADD_METABUF_FRAME(effectReq, vOutputFrameInfo, frameId, rpMetaBuf)


#define USE_FD 1
#define EIS_ON 0

sp<EffectRequest> generateEffectRequest(int request_id, DepthNodeOpState eState)
{
    MY_LOGD("+: reqID = %d, request state=%d", request_id, eState);
    DECLARE_MACRO();

    sp<EffectRequest> pEffectReq = new EffectRequest(request_id, WaitingListener::CB);
    sp<EffectParameter> pEffectParam = new EffectParameter();
    pEffectParam->set(DEPTHMAP_REQUEST_STATE_KEY, (int)eState);
    pEffectReq->setRequestParameter(pEffectParam);

    sp<IImageBuffer> buf_fs_Main1, buf_rs_Main1, buf_rs_Main2, buf;
    // load resize raw
    loadFullResizRawsImgBuf(buf_fs_Main1, buf_rs_Main1, buf_rs_Main2);
    MY_LOGD("RRZ_RAW1=%x RRZ_RAW2=%x", buf_rs_Main1.get(), buf_rs_Main2.get());

    // prepare input frame info: RSRAW1
    sp<EffectFrameInfo> frameInfo = new EffectFrameInfo(request_id, BID_P2AFM_IN_RSRAW1);
    frameInfo->setFrameBuffer(buf_rs_Main1);
    pEffectReq->vInputFrameInfo.add(BID_P2AFM_IN_RSRAW1, frameInfo);

    // prepare input frame info: RSRAW2
    frameInfo = new EffectFrameInfo(request_id, BID_P2AFM_IN_RSRAW2);
    frameInfo->setFrameBuffer(buf_rs_Main2);
    pEffectReq->vInputFrameInfo.add(BID_P2AFM_IN_RSRAW2, frameInfo);

    // prepare input frame info: FSRAW
    frameInfo = new EffectFrameInfo(request_id, BID_P2AFM_IN_FSRAW1);
    frameInfo->setFrameBuffer(buf_fs_Main1);
    pEffectReq->vInputFrameInfo.add(BID_P2AFM_IN_FSRAW1, frameInfo);

    IMetadata* pMetadata;
    // prepare the metadata frame info
    // InAppMeta
    ADD_METABUF_INPUT_FRAME(pEffectReq, BID_META_IN_APP, pMetadata);
    // InAppMeta - EIS on
    if(EIS_ON)
    {
        trySetMetadata<MUINT8>(pMetadata, MTK_CONTROL_VIDEO_STABILIZATION_MODE, MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON);
    }
    else
    {
        trySetMetadata<MUINT8>(pMetadata, MTK_CONTROL_VIDEO_STABILIZATION_MODE, MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF);
    }
    // app meta
    trySetMetadata<MINT32>(pMetadata, MTK_STEREO_FEATURE_DOF_LEVEL, 0);
    trySetMetadata<MINT32>(pMetadata, MTK_JPEG_ORIENTATION, 0);
    trySetMetadata<MUINT8>(pMetadata, MTK_CONTROL_AF_TRIGGER, 0);
    // InHalMeta
    ADD_METABUF_INPUT_FRAME(pEffectReq, BID_META_IN_HAL, pMetadata);
    trySetMetadata<MINT32>(pMetadata, MTK_P1NODE_SENSOR_MODE, SENSOR_SCENARIO_ID_NORMAL_CAPTURE);
    trySetMetadata<MINT32>(pMetadata, MTK_P1NODE_PROCESSOR_MAGICNUM, 0);
    // InHalMeta-EIS region
    IMetadata::IEntry entry(MTK_EIS_REGION);
    entry.push_back(0, Type2Type< MINT32 >());
    entry.push_back(0, Type2Type< MINT32 >());
    entry.push_back(0, Type2Type< MINT32 >());
    entry.push_back(0, Type2Type< MINT32 >());
    entry.push_back(1920, Type2Type< MINT32 >());
    entry.push_back(1088, Type2Type< MINT32 >());
    // the following is  MVtoCenterX, MVtoCenterY, IsFromRRZ
    entry.push_back(0, Type2Type< MINT32 >());
    entry.push_back(0, Type2Type< MINT32 >());
    entry.push_back(MTRUE, Type2Type< MBOOL >());
    pMetadata->update(MTK_EIS_REGION, entry);
    // InHalMeta_Main2
    ADD_METABUF_INPUT_FRAME(pEffectReq, BID_META_IN_HAL_MAIN2, pMetadata);
    trySetMetadata<MINT32>(pMetadata, MTK_P1NODE_PROCESSOR_MAGICNUM, 0);
    // OutAppMeta
    ADD_METABUF_OUTPUT_FRAME(pEffectReq, BID_META_OUT_APP, pMetadata);
    // OutHalMeta
    ADD_METABUF_OUTPUT_FRAME(pEffectReq, BID_META_OUT_HAL, pMetadata);

    // prepare output frame: MV_F
    ImgParam imgParam_MainImageCAP = getImgParam_MV_F_CAP();
    ImgParam imgParam_MainImage = getImgParam_MV_F();
    ImgParam imgParam_DepthMap = getImgParam_DepthMap();
    ImgParam imgParam_DMBG = getImgParam_DMBG();
    ImgParam imgParam_JPSMain = getImgParam_JPSMain();
    ImgParam imgParam_LDC = getImgParam_LDC();

    if(eState == eSTATE_CAPTURE)
    {
        // MV_F_CAP
        frameInfo = new EffectFrameInfo(request_id, BID_P2AFM_OUT_MV_F_CAP);
        buf = createEmptyImageBuffer(imgParam_MainImageCAP, "MainImg",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_P2AFM_OUT_MV_F_CAP, frameInfo);
        // DepthMap
        frameInfo = new EffectFrameInfo(request_id, BID_GF_OUT_DEPTHMAP);
        buf = createEmptyImageBuffer(imgParam_DepthMap, "DepthMap",  eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_SW_READ_OFTEN);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_GF_OUT_DEPTHMAP, frameInfo);
        // JPS Main1/2
        frameInfo = new EffectFrameInfo(request_id, BID_N3D_OUT_JPS_MAIN1);
        buf = createEmptyGraphicBuffer(imgParam_JPSMain, "JPSMain1",  eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_SW_READ_OFTEN|eBUFFER_USAGE_HW_CAMERA_READWRITE);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_N3D_OUT_JPS_MAIN1, frameInfo);
        // JPS Main1/2
        frameInfo = new EffectFrameInfo(request_id, BID_N3D_OUT_JPS_MAIN2);
        buf = createEmptyGraphicBuffer(imgParam_JPSMain, "JPSMain2",  eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_SW_READ_OFTEN|eBUFFER_USAGE_HW_CAMERA_READWRITE);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_N3D_OUT_JPS_MAIN2, frameInfo);
        // LDC
        frameInfo = new EffectFrameInfo(request_id, BID_N3D_OUT_LDC);
        buf = createEmptyImageBuffer(imgParam_LDC, "LDC",  eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_SW_READ_OFTEN);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_N3D_OUT_LDC, frameInfo);

    }
    else
    {   // MV_F
        frameInfo = new EffectFrameInfo(request_id, BID_P2AFM_OUT_MV_F);
        buf = createEmptyImageBuffer(imgParam_MainImage, "MainImg",  eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_RARELY);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_P2AFM_OUT_MV_F, frameInfo);
    }

    // DMBG
    frameInfo = new EffectFrameInfo(request_id, BID_GF_OUT_DMBG);
    buf = createEmptyImageBuffer(imgParam_DMBG, "DMBG",  eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_SW_READ_OFTEN);
    frameInfo->setFrameBuffer(buf);
    pEffectReq->vOutputFrameInfo.add(BID_GF_OUT_DMBG, frameInfo);

    // prepare output frame: FD
    if(USE_FD)
    {
        frameInfo = new EffectFrameInfo(request_id, BID_P2AFM_OUT_FDIMG);
        ImgParam imgParam_FD = getImgParam_FD();
        buf = createEmptyImageBuffer(imgParam_FD, "FD", eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE |  eBUFFER_USAGE_SW_WRITE_RARELY);
        frameInfo->setFrameBuffer(buf);
        pEffectReq->vOutputFrameInfo.add(BID_P2AFM_OUT_FDIMG, frameInfo);
    }
    MY_LOGD("-");
    return pEffectReq;
}

MVOID setupDumpProperties()
{
    #define SET_ID2NAME_PROB(name, ID) \
        snprintf(prop_name, 256, "%s.%s", name, #ID); \
        property_set(prop_name, "1");

    char prop_name[256];
    // P2AFM Node section
    char* name = "P2AFM";
    property_set(name, "1");
    SET_ID2NAME_PROB(name, P2AFM_TO_N3D_FEFM_CCin);
    SET_ID2NAME_PROB(name, P2AFM_TO_FD_IMG);
    SET_ID2NAME_PROB(name, P2AFM_TO_GF_MY_SL);
    SET_ID2NAME_PROB(name, P2AFM_OUT_MV_F);
    SET_ID2NAME_PROB(name, P2AFM_OUT_FD);
    SET_ID2NAME_PROB(name, P2AFM_OUT_MV_F_CAP);
    SET_ID2NAME_PROB(name, UT_OUT_FE);
    //N3D Node section
    name = "N3DNode";
    property_set(name, "1");
    SET_ID2NAME_PROB(name, N3D_TO_DPE_MVSV_MASK);
    SET_ID2NAME_PROB(name, N3D_TO_OCC_LDC);
    SET_ID2NAME_PROB(name, N3D_TO_FD_EXTRADATA_MASK);
    SET_ID2NAME_PROB(name, N3D_OUT_JPS_WARPING_MATRIX);
    // DPE Node section
    name = "DPENode";
    property_set(name, "1");
    SET_ID2NAME_PROB(name, DPE_TO_OCC_MVSV_DMP_CFM);
    // OCC Node Section
    name = "OCCNode";
    property_set(name, "1");
    SET_ID2NAME_PROB(name, OCC_TO_WMF_DMH_MY_S);
    // WMF Node section
    name = "WMFNode";
    property_set(name, "1");
    SET_ID2NAME_PROB(name, WMF_TO_GF_DMW_MY_S);
    SET_ID2NAME_PROB(name, GF_OUT_DEPTHMAP);
    // FD Node section
    name = "FDNode";
    property_set(name, "1");
    SET_ID2NAME_PROB(name, FD_OUT_EXTRADATA);

    //======== Power on sensor ==========
    NSCam::IHalSensorList* pHalSensorList = MAKE_HalSensorList();
    pHalSensorList->searchSensors();
    int sensorCount = pHalSensorList->queryNumberOfSensors();

    int main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);
    MUINT pIndex[2] = { (MUINT)main1Idx, (MUINT)main2Idx };
    MUINT const main1Index = pIndex[0];
    MUINT const main2Index = pIndex[1];
    if(!pHalSensorList)
    {
        MY_LOGE("pHalSensorList == NULL");
    }
    //
    #define USER_NAME "N3DNodeUT"
    IHalSensor *pHalSensor = pHalSensorList->createSensor(
                                            USER_NAME,
                                            2,
                                            pIndex);
    if(pHalSensor == NULL)
    {
       MY_LOGE("pHalSensor is NULL");
    }
    // In stereo mode, Main1 needs power on first.
    // Power on main1 and main2 successively one after another.
    if( !pHalSensor->powerOn(USER_NAME, 1, &main1Index) )
    {
        MY_LOGE("sensor power on failed: %d", main1Index);
    }
    if( !pHalSensor->powerOn(USER_NAME, 1, &main2Index) )
    {
        MY_LOGE("sensor power on failed: %d", main2Index);
    }
}


TEST(DepthMapEffectHalTest, Standard)
{
    // setup dump buffer properties
    setupDumpProperties();
    StereoSettingProvider::setStereoFeatureMode(E_STEREO_FEATURE_VSDOF);

    MY_LOGD("Create DepthMapEffectHal...");
    DepthMapEffectHal* pDepthMapEffectHal = new DepthMapEffectHal();
    pDepthMapEffectHal->init();
    // main1/main2 sensor index
    int main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);

    // prepare sensor idx parameters
    sp<EffectParameter> effParam = new EffectParameter();
    effParam->set(SENSOR_IDX_MAIN1, main1Idx);
    effParam->set(SENSOR_IDX_MAIN2, main2Idx);
    pDepthMapEffectHal->setParameters(effParam);

    MY_LOGD("Prepare EffectRequests...");
    std::vector<sp<EffectRequest>> vEffectReqVec;
    int targetTime  = 5;
    for(int i=0;i<targetTime;i++)
    {
        sp<EffectRequest> pEffReq = generateEffectRequest(i, eSTATE_NORMAL);
        vEffectReqVec.push_back(pEffReq);
    }
    MY_LOGD("DepthMapEffectHal configure and start...");
    // configure and start effectHal
    pDepthMapEffectHal->configure();
    pDepthMapEffectHal->start();

    // test normal request
    for(int i=0;i<vEffectReqVec.size();i++)
    {
        sp<EffectRequest> pReq = vEffectReqVec[i];
        MY_LOGD("DepthMapEffectHal updateEffectRequest: %d", i);
        pDepthMapEffectHal->updateEffectRequest(pReq);
    }
    MY_LOGD("DepthMapEffectHal start to wait!!....");
    bool bRet = WaitingListener::waitRequestAtLeast(targetTime,5,1);
    MY_LOGD("Wait done!!....");
    EXPECT_TRUE(bRet);

    WaitingListener::resetCounter();
}

}
}
}



