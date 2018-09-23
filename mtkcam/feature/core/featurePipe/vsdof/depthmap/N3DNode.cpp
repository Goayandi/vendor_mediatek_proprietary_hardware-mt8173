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
#include "N3DNode.h"
#include "DepthMapPipe_Common.h"
#include "DepthMapPipeUtils.h"

#include <ui/gralloc_extra.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>

#define PIPE_CLASS_TAG "N3DNode"
#include <featurePipe/core/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

N3DNode::BufferSizeConfig::
BufferSizeConfig()
{
     // setup constants
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    // img size is independent to scenarios
    VSDOF_N3D_OUTPUT_IMG_SIZE = pSizeProvder->getBufferSize(E_MV_Y, eSTEREO_SCENARIO_RECORD);
    // mask size is independent to scenarios
    VSDOF_N3D_OUTPUT_MASK_SIZE = pSizeProvder->getBufferSize(E_MASK_M_Y, eSTEREO_SCENARIO_RECORD);
    VSDOF_N3D_LDC_SIZE = pSizeProvder->getBufferSize(E_LDC, eSTEREO_SCENARIO_RECORD);
}

N3DNode::BufferSizeConfig::
~BufferSizeConfig()
{}

N3DNode::BufferPoolSet::
BufferPoolSet()
{}

N3DNode::BufferPoolSet::
~BufferPoolSet()
{}

MBOOL
N3DNode::BufferPoolSet::
init(const BufferSizeConfig& config)
{
    MY_LOGD("+");
    // SW read/write, hw read
    MUINT32 usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ;

    // INIT N3D output buffer pool - for MV/SV
    mN3DImgBufPool = ImageBufferPool::create("N3DImgBufPool", config.VSDOF_N3D_OUTPUT_IMG_SIZE.w,
                                        config.VSDOF_N3D_OUTPUT_IMG_SIZE.h, eImgFmt_YV12, usage);
    // INIT N3D output buffer pool - for MASK_M/MASK_Y
    mN3DMaskBufPool = ImageBufferPool::create("N3DMaskBufPool", config.VSDOF_N3D_OUTPUT_MASK_SIZE.w,
                                        config.VSDOF_N3D_OUTPUT_MASK_SIZE.h, eImgFmt_Y8, usage);
    // INIT N3D output buffer - for LDC
    mLDCBufPool = ImageBufferPool::create("LDCBufPool", config.VSDOF_N3D_LDC_SIZE.w,
                                            config.VSDOF_N3D_LDC_SIZE.h, eImgFmt_Y8, ImageBufferPool::USAGE_SW);
    // for MV/SV -> double size
    ALLOCATE_BUFFER_POOL(mN3DImgBufPool, VSDOF_WORKING_BUF_SET*2);
    // for MASK_M/MASK_Y -> double size
    ALLOCATE_BUFFER_POOL(mN3DMaskBufPool, VSDOF_WORKING_BUF_SET*2);
    // for LDC
    ALLOCATE_BUFFER_POOL(mLDCBufPool, VSDOF_WORKING_BUF_SET);
    MY_LOGD("-");
    return MTRUE;
}

MBOOL
N3DNode::BufferPoolSet::
uninit()
{
    ImageBufferPool::destroy(mN3DImgBufPool);
    ImageBufferPool::destroy(mN3DMaskBufPool);
    ImageBufferPool::destroy(mLDCBufPool);
    return MTRUE;
}

N3DNode::
N3DNode(const char* name, DepthFlowType flowType)
:DepthMapPipeNode(name, flowType)
{
    this->addWaitQueue(&mJobQueue);
}

N3DNode::
~N3DNode()
{
}

MVOID
N3DNode::
cleanUp()
{
    VSDOF_LOGD("+");

    if(mpN3DHAL_CAP)
    {
        delete mpN3DHAL_CAP;
        mpN3DHAL_CAP = nullptr;
    }

    if(mpN3DHAL_VRPV)
    {
        delete mpN3DHAL_VRPV;
        mpN3DHAL_VRPV = nullptr;
    }
    mJobQueue.clear();
    mBufPoolSet.uninit();
    VSDOF_LOGD("-");
}

MBOOL
N3DNode::
onInit()
{
    VSDOF_LOGD("+");
    CAM_TRACE_NAME("N3DNode::onInit");
    // init the buffer pool
    mBufPoolSet.init(mBufConfig);
    // create N3DHAL instance for scenarios
    N3D_HAL_INIT_PARAM initParam_CAP;
    initParam_CAP.eScenario  = eSTEREO_SCENARIO_CAPTURE;
    initParam_CAP.fefmRound  = VSDOF_CONST_FE_EXEC_TIMES;
    mpN3DHAL_CAP = N3D_HAL::createInstance(initParam_CAP);

    N3D_HAL_INIT_PARAM initParam_VRPV;
    initParam_VRPV.eScenario = eSTEREO_SCENARIO_RECORD;
    initParam_VRPV.fefmRound = VSDOF_CONST_FE_EXEC_TIMES;
    mpN3DHAL_VRPV  = N3D_HAL::createInstance(initParam_VRPV);

    VSDOF_LOGD("-");
    return MTRUE;
}

MBOOL
N3DNode::
onUninit()
{
    cleanUp();
    return MTRUE;
}

MBOOL
N3DNode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
N3DNode::
onThreadStop()
{
    return MTRUE;
}


MBOOL
N3DNode::
onData(DataID data, ImgInfoMapPtr& P2ImgInfo)
{
    MBOOL ret = MTRUE;
    VSDOF_PRFLOG("+ : dataID=%d reqId=%d", data, P2ImgInfo->getRequestPtr()->getRequestNo());

    switch(data)
    {
        case P2AFM_TO_N3D_FEFM_CCin:
            mJobQueue.enque(P2ImgInfo);
            break;
        default:
            MY_LOGW("Unrecongnized DataID=%d", data);
            ret = MFALSE;
            break;
    }

    VSDOF_LOGD("-");
    return ret;
}

MBOOL
N3DNode::
onThreadLoop()
{
    ImgInfoMapPtr P2ImgInfo;
    EffectRequestPtr reqPtr;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }

    if( !mJobQueue.deque(P2ImgInfo) )
    {
        MY_LOGE("N3DNode mJobQueue.deque() failed");
        return MFALSE;
    }
    // mark on-going-request start
    this->incExtThreadDependency();

    reqPtr = P2ImgInfo->getRequestPtr();
    VSDOF_PRFLOG("threadLoop start, reqID=%d", reqPtr->getRequestNo());
    CAM_TRACE_NAME("N3DNode::onThreadLoop");
    DepthNodeOpState state = getRequestState(reqPtr);

    MBOOL ret;

    if(state == eSTATE_CAPTURE)
    {
        ret = performN3DALGO_CAP(P2ImgInfo);
    }
    else
    {
        ret = performN3DALGO_VRPV(P2ImgInfo);
    }

    VSDOF_PRFLOG("threadLoop end! reqID=%d", reqPtr->getRequestNo());
    // mark on-going-request end
    this->decExtThreadDependency();

    return ret;
}

MBOOL
N3DNode::
performN3DALGO_VRPV(ImgInfoMapPtr& rP2ImgInfo)
{
    CAM_TRACE_NAME("N3DNode::performN3DALGO_VRPV");
    EffectRequestPtr pRequest = rP2ImgInfo->getRequestPtr();
    N3D_HAL_PARAM n3dParams;
    N3D_HAL_OUTPUT n3dOutput;
    // prepare input params
    prepareN3DInputParam(rP2ImgInfo, n3dParams);

    //output ImgInfoMap
    ImgInfoMapPtr pToDPEImgInfo = new ImageBufInfoMap(pRequest);
    ImgInfoMapPtr pToOCCImgInfo = new ImageBufInfoMap(pRequest);

    // prepare output params
    prepareN3DOutputParam(n3dOutput, pToDPEImgInfo, pToOCCImgInfo);

    debugN3DParams(n3dParams, n3dOutput);


    SimpleTimer timer;
    VSDOF_PRFLOG("start N3D(PV/VR) ALGO, reqID=%d t=%d", pRequest->getRequestNo(), timer.startTimer());

    CAM_TRACE_BEGIN("N3DNode::N3DHALRun");
    bool ret = mpN3DHAL_VRPV->N3DHALRun(n3dParams, n3dOutput);
    CAM_TRACE_END();

    if(ret)
    {
        VSDOF_PRFLOG("finsished N3D(PV/VR) ALGO, reqID=%d, exec-time=%f msec", pRequest->getRequestNo(), timer.countTimer());
        // write meta when normal flow type
        if(mFlowType != eDEPTH_FLOW_TYPE_QUEUED_DEPTH)
            writeN3DResultToMeta(n3dOutput, pRequest);
        handleDataAndDump(N3D_TO_DPE_MVSV_MASK, pToDPEImgInfo);
        handleDataAndDump(N3D_TO_OCC_LDC, pToOCCImgInfo);
    }
    else
    {
        MY_LOGE("N3D ALGO(PV/VR) failed: reqID=%d", rP2ImgInfo->getRequestPtr()->getRequestNo());
    }

    return ret;

}

MBOOL
N3DNode::
writeN3DResultToMeta(const N3D_HAL_OUTPUT& n3dOutput, sp<EffectRequest> pRequest)
{
    DepthMapBufferID metaBID_outApp = mapQueuedBufferID(pRequest, BID_META_OUT_APP);
    DepthMapBufferID metaBID_outHal = mapQueuedBufferID(pRequest, BID_META_OUT_HAL);
    //write distance to app meta
    FrameInfoPtr outMeta_frameInfo = pRequest->vOutputFrameInfo.valueFor(metaBID_outApp);
    IMetadata* pMetadata = getMetadataFromFrameInfoPtr(outMeta_frameInfo);
    VSDOF_LOGD("output distance:%d", n3dOutput.distance);
    trySetMetadata<MFLOAT>(pMetadata, MTK_STEREO_FEATURE_RESULT_DISTANCE, n3dOutput.distance);
    // set outAppMeta ready
    outMeta_frameInfo->setFrameReady(true);

    outMeta_frameInfo = pRequest->vOutputFrameInfo.valueFor(metaBID_outHal);
    pMetadata = getMetadataFromFrameInfoPtr(outMeta_frameInfo);
    VSDOF_LOGD("output convOffset:%f", n3dOutput.convOffset);
    trySetMetadata<MFLOAT>(pMetadata, MTK_CONVERGENCE_DEPTH_OFFSET, n3dOutput.convOffset);

    // set outHalMeta ready
    outMeta_frameInfo->setFrameReady(true);
    // pass data finish
    handleData(DEPTHMAP_META_OUT, outMeta_frameInfo);
    return MTRUE;
}

MBOOL
N3DNode::
performN3DALGO_CAP(ImgInfoMapPtr& rP2ImgInfo)
{
    CAM_TRACE_NAME("N3DNode::performN3DALGO_CAP");

    N3D_HAL_PARAM_CAPTURE n3dParams;
    N3D_HAL_OUTPUT_CAPTURE n3dOutput;
    EffectRequestPtr pEffReq = rP2ImgInfo->getRequestPtr();
    // prepare input params
    prepareN3DInputParam_CAP(rP2ImgInfo, n3dParams);
    //output ImgInfoMap
    ImgInfoMapPtr pToDPEImgInfo = new ImageBufInfoMap(pEffReq);
    // prepare output params
    prepareN3DOutputParam_CAP(n3dOutput, pToDPEImgInfo);
    //
    debugN3DParams_Cap(n3dParams, n3dOutput);

    SimpleTimer timer;
    VSDOF_PRFLOG("start N3D(CAP) ALGO, reqID=%d t=%d", pEffReq->getRequestNo(), timer.startTimer());

    CAM_TRACE_BEGIN("N3DNode::N3DHALRun");
    bool ret = mpN3DHAL_CAP->N3DHALRun(n3dParams, n3dOutput);
    CAM_TRACE_END();

    if(ret)
    {
        VSDOF_PRFLOG("finsished N3D(CAP) ALGO, reqID=%d, exe-time=%f msec", pEffReq->getRequestNo(), timer.countTimer());

        // mark LDC buffer ready
        FrameInfoPtr pFrameInfo;
        RETRIEVE_OFRAMEINFO(pEffReq, pFrameInfo, BID_N3D_OUT_LDC);
        pFrameInfo->setFrameReady(true);
        // Capture flow - LDC buffer locates at EffectRequest
        ImgInfoMapPtr pToOCCImgInfo = new ImageBufInfoMap(pEffReq);
        handleData(N3D_TO_OCC_LDC, pToOCCImgInfo);
        handleDump(N3D_TO_OCC_LDC, pFrameInfo, eSTATE_CAPTURE);
        // write meta
        writeN3DResultToMeta(n3dOutput, pEffReq);
        handleDataAndDump(N3D_TO_DPE_MVSV_MASK, pToDPEImgInfo);
        // output the JSPMain2
        FrameInfoPtr frameInfo_JPSMain2;
        RETRIEVE_OFRAMEINFO(rP2ImgInfo->getRequestPtr(), frameInfo_JPSMain2, BID_N3D_OUT_JPS_MAIN2);
        if(frameInfo_JPSMain2.get())
        {
            frameInfo_JPSMain2->setFrameReady(true);
        }
        else
        {
            MY_LOGE("Cannot find the frameInfo:JSPMain2, reqID=%d", pEffReq->getRequestNo());
            return MFALSE;
        }
        handleDump(N3D_OUT_JPS, frameInfo_JPSMain2, eSTATE_CAPTURE, "N3D_OUT_JPS_MAIN2");

        // output ExtraData
        char* sExtraData = mpN3DHAL_CAP->getStereoExtraData();
        ExtraDataInfoPtr pExtraData = new ExtraDataInfo(pEffReq,sExtraData);
        handleData(N3D_TO_FD_EXTDATA_MASK, pExtraData);

    }
    else
    {
        MY_LOGE("N3D ALGO(CAP) failed: reqID=%d", pEffReq->getRequestNo());
        handleData(ERROR_OCCUR_NOTIFY, pEffReq);
        return MFALSE;
    }
    return MTRUE;
}

MBOOL
N3DNode::
copyImageBuffer(sp<IImageBuffer> srcImgBuf, sp<IImageBuffer> dstImgBuf)
{
    if(srcImgBuf->getPlaneCount() != dstImgBuf->getPlaneCount())
    {
        MY_LOGE("source/destination image buffer has different plane count! cannot copy!");
        return MFALSE;
    }
    for(int index=0;index<srcImgBuf->getPlaneCount();++index)
    {
        if(srcImgBuf->getBufSizeInBytes(index) != dstImgBuf->getBufSizeInBytes(index))
        {
            MY_LOGE("The %d-st plane of source/destination image buffer has different buffer size! cannot copy!", index);
            return MFALSE;
        }
        // memory copy
        memcpy((MUINT8*)dstImgBuf->getBufVA(index), (MUINT8*)srcImgBuf->getBufVA(index), srcImgBuf->getBufSizeInBytes(index));
    }
    return MTRUE;
}

MBOOL
N3DNode::
prepareN3DInputParam(ImgInfoMapPtr& rP2ImgInfo, N3D_HAL_PARAM& rN3dParam)
{
    VSDOF_PRFLOG("prepareN3DInputParam +");
    CAM_TRACE_NAME("prepareN3DInputParam");
    // prepare FEFM input
    prepareFEFMData(rP2ImgInfo, rN3dParam.hwfefmData);

    // prepare Rect/CC input
    SmartImageBuffer smCCIn1 = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_CC_IN1);
    SmartImageBuffer smCCIn2 = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_CC_IN2);
    SmartImageBuffer smRectIn1 = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_RECT_IN1);
    SmartImageBuffer smRectIn2 = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_RECT_IN2);

    smRectIn1->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smRectIn2->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smCCIn1->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smCCIn2->mImageBuffer->syncCache(eCACHECTRL_INVALID);

    rN3dParam.rectifyImgMain1 = smRectIn1->mImageBuffer.get();
    rN3dParam.rectifyImgMain2 = smRectIn2->mImageBuffer.get();
    rN3dParam.ccImage[0] = smCCIn1->mImageBuffer.get();
    rN3dParam.ccImage[1] = smCCIn2->mImageBuffer.get();

    EffectRequestPtr pRequest = rP2ImgInfo->getRequestPtr();
    MBOOL bRet = MFALSE;
    // meta id
    DepthMapBufferID metaBID_InApp = mapQueuedBufferID(pRequest, BID_META_IN_APP);
    DepthMapBufferID metaBID_InHal = mapQueuedBufferID(pRequest, BID_META_IN_HAL);
    DepthMapBufferID metaBID_InHal_Main2 = mapQueuedBufferID(pRequest, BID_META_IN_HAL_MAIN2);
    //
    MINT32 depthAFON = 0;
    MINT32 disMeasureFON = 0;
    MUINT8 isAFTrigger = 0;
    MINT32 magicNum = 0;

    bRet = tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                                        metaBID_InApp, MTK_STEREO_FEATURE_DEPTH_AF_ON, depthAFON);
    if(!bRet)
        VSDOF_LOGD("reqID=%d, Cannot find MTK_STEREO_FEATURE_DEPTH_AF_ON meta!", pRequest->getRequestNo());

    bRet = tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                                        metaBID_InApp, MTK_STEREO_FEATURE_DISTANCE_MEASURE_ON, disMeasureFON);
    if(!bRet)
        VSDOF_LOGD("reqID=%d, Cannot find MTK_STEREO_FEATURE_DISTANCE_MEASURE_ON meta!", pRequest->getRequestNo());

    bRet = tryGetMetadataInFrame<MUINT8>(pRequest, FRAME_INPUT,
                                        metaBID_InApp, MTK_CONTROL_AF_TRIGGER, isAFTrigger);
    if(!bRet)
        VSDOF_LOGD("reqID=%d, Cannot find MTK_CONTROL_AF_TRIGGER meta!", pRequest->getRequestNo());

    bRet = tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                                        metaBID_InHal, MTK_P1NODE_PROCESSOR_MAGICNUM, magicNum);
    if(bRet) {
        rN3dParam.magicNumber[0] = magicNum;
    } else {
        MY_LOGE("reqID=%d, Cannot find MTK_P1NODE_PROCESSOR_MAGICNUM meta(0)!", pRequest->getRequestNo());
        return MFALSE;
    }

    bRet = tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                                        metaBID_InHal_Main2, MTK_P1NODE_PROCESSOR_MAGICNUM, magicNum);
    if(bRet) {
        rN3dParam.magicNumber[1] = magicNum;
    } else {
        MY_LOGE("reqID=%d, Cannot find MTK_P1NODE_PROCESSOR_MAGICNUM meta(1)!", pRequest->getRequestNo());
        return MFALSE;
    }

    rN3dParam.requestNumber = pRequest->getRequestNo();
    rN3dParam.isAFTrigger = isAFTrigger;
    rN3dParam.isDepthAFON = depthAFON;
    rN3dParam.isDistanceMeasurementON = disMeasureFON;

    // prepare EIS data
    prepareEISData(rP2ImgInfo, rN3dParam.eisData);

    VSDOF_PRFLOG("prepareN3DInputParam -");
    return MTRUE;
}

MBOOL
N3DNode::
prepareN3DInputParam_CAP(ImgInfoMapPtr& rP2ImgInfo, N3D_HAL_PARAM_CAPTURE& rN3dParam)
{
    CAM_TRACE_NAME("prepareN3DInputParam_CAP");
    VSDOF_PRFLOG("prepareN3DInputParam_CAP +");
    // prepare FEFM input
    prepareFEFMData(rP2ImgInfo, rN3dParam.hwfefmData);

    // prepare Rect/CC input
    SmartImageBuffer smCCIn1 = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_CC_IN1);
    SmartImageBuffer smCCIn2 = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_CC_IN2);

     // Rect_in1 is the JSPMain1 when capture
    FrameInfoPtr pFrameInfo;
    sp<IImageBuffer> pRectImgBuf;
    RETRIEVE_OFRAMEINFO_IMGBUF(rP2ImgInfo->getRequestPtr(), pFrameInfo, BID_N3D_OUT_JPS_MAIN1, pRectImgBuf);
    rN3dParam.rectifyImgMain1 = pRectImgBuf.get();
    //
    rN3dParam.ccImage[0] = smCCIn1->mImageBuffer.get();
    rN3dParam.ccImage[1] = smCCIn2->mImageBuffer.get();

    // main2 graphic buffer for rectify
    SmartGraphicBuffer smRectIn2 = rP2ImgInfo->getGraphicBuffer(BID_P2AFM_OUT_RECT_IN2_CAP);
    android::sp<android::GraphicBuffer> pRectInGraBuf = smRectIn2->mGraphicBuffer;
    // config graphic buffer to BT601_FULL
    gralloc_extra_ion_sf_info_t info;
    gralloc_extra_query(pRectInGraBuf->handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &info);
    gralloc_extra_sf_set_status(&info, GRALLOC_EXTRA_MASK_YUV_COLORSPACE, GRALLOC_EXTRA_BIT_YUV_BT601_FULL);
    gralloc_extra_perform(pRectInGraBuf->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &info);
    rN3dParam.rectifyGBMain2 = smRectIn2->mImageBuffer.get();
    // input magic number/orientation
    MBOOL bRet;
    MINT32 magicNum = 0;
    MINT32 jpegOrientation = 0;

    bRet = tryGetMetadataInFrame<MINT32>(rP2ImgInfo->getRequestPtr(), FRAME_INPUT,
                                        BID_META_IN_HAL, MTK_P1NODE_PROCESSOR_MAGICNUM, magicNum);
    if(bRet) {
        rN3dParam.magicNumber[0] = magicNum;
    } else {
        MY_LOGE("Cannot find MTK_P1NODE_PROCESSOR_MAGICNUM meta(0)!");
    }

    bRet = tryGetMetadataInFrame<MINT32>(rP2ImgInfo->getRequestPtr(), FRAME_INPUT,
                                        BID_META_IN_HAL_MAIN2, MTK_P1NODE_PROCESSOR_MAGICNUM, magicNum);
    if(bRet) {
        rN3dParam.magicNumber[1] = magicNum;
    } else {
        MY_LOGE("Cannot find MTK_P1NODE_PROCESSOR_MAGICNUM meta(1)!");
    }

    bRet = tryGetMetadataInFrame<MINT32>(rP2ImgInfo->getRequestPtr(), FRAME_INPUT,
                                        BID_META_IN_APP, MTK_JPEG_ORIENTATION, jpegOrientation);
    if(!bRet) {
        MY_LOGE("Cannot find MTK_JPEG_ORIENTATION meta!");
    }
    rN3dParam.captureOrientation = jpegOrientation;

    VSDOF_PRFLOG("prepareN3DInputParam_CAP -");

    return MTRUE;
}

MVOID
N3DNode::
debugN3DParams(N3D_HAL_PARAM& inParam, N3D_HAL_OUTPUT& output)
{
    if(!mbDebugLog)
        return;

    MY_LOGD("+");
    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataMain1[%d]=%x",i, inParam.hwfefmData.geoDataMain1[i]);
    }

    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataMain2[%d]=%x",i, inParam.hwfefmData.geoDataMain2[i]);
    }

    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataLeftToRight[%d]=%x",i, inParam.hwfefmData.geoDataLeftToRight[i]);
    }

    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataRightToLeft[%d]=%x",i, inParam.hwfefmData.geoDataRightToLeft[i]);
    }

    MY_LOGD("inParam.rectifyImgMain1=%x",inParam.rectifyImgMain1);
    MY_LOGD("inParam.rectifyImgMain2=%x",inParam.rectifyImgMain2);

    MY_LOGD("inParam.ccImage[0]=%x",inParam.ccImage[0]);
    MY_LOGD("inParam.ccImage[1]=%x", inParam.ccImage[1]);
    MY_LOGD("inParam.magicNumber=%d, %d", inParam.magicNumber[0], inParam.magicNumber[1]);
    MY_LOGD("inParam.requestNumber=%d", inParam.requestNumber);
    MY_LOGD("inParam.isAFTrigger=%d", inParam.isAFTrigger);
    MY_LOGD("inParam.isDepthAFON=%d", inParam.isDepthAFON);
    MY_LOGD("inParam.isDistanceMeasurementON=%d", inParam.isDistanceMeasurementON);

    MY_LOGD("inParam.eisData.isON=%d", inParam.eisData.isON);
    MY_LOGD("inParam.eisData.eisOffset=%d", inParam.eisData.eisOffset);
    MY_LOGD("inParam.eisData.eisImgSize=%dx%d", inParam.eisData.eisImgSize.w, inParam.eisData.eisImgSize.h);

    MY_LOGD("output.rectifyImgMain1=%x",output.rectifyImgMain1);
    MY_LOGD("output.maskMain1=%x",output.maskMain1);
    MY_LOGD("output.rectifyImgMain2=%x",output.rectifyImgMain2);
    MY_LOGD("output.maskMain2=%x",output.maskMain2);
    MY_LOGD("output.ldcMain1=%x",output.ldcMain1);
    MY_LOGD("-");
}

MVOID
N3DNode::
debugN3DParams_Cap(N3D_HAL_PARAM_CAPTURE& inParam, N3D_HAL_OUTPUT_CAPTURE& output)
{
    if(!mbDebugLog)
        return;

    MY_LOGD("+");
    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataMain1[%d]=%x",i, inParam.hwfefmData.geoDataMain1[i]);
    }

    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataMain2[%d]=%x",i, inParam.hwfefmData.geoDataMain2[i]);
    }

    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataLeftToRight[%d]=%x",i, inParam.hwfefmData.geoDataLeftToRight[i]);
    }

    for(int i=0;i<3;i++)
    {
        MY_LOGD("inParam.hwfefmData.geoDataRightToLeft[%d]=%x",i, inParam.hwfefmData.geoDataRightToLeft[i]);
    }

    #define LOG_IMGBUFFER(imgbuf)\
        if(imgbuf!=NULL)\
        {\
            MY_LOGD(#imgbuf "=%x size=%dx%d format=%x",imgbuf, imgbuf->getImgSize().w \
                            , imgbuf->getImgSize().h, imgbuf->getImgFormat());\
        }

    LOG_IMGBUFFER(inParam.rectifyImgMain1);
    LOG_IMGBUFFER(inParam.rectifyGBMain2);
    LOG_IMGBUFFER(inParam.ccImage[0]);
    LOG_IMGBUFFER(inParam.ccImage[1]);
    MY_LOGD("inParam.magicNumber=%d, %d", inParam.magicNumber[0], inParam.magicNumber[1]);
    MY_LOGD("inParam.captureOrientation=%x", inParam.captureOrientation);

    LOG_IMGBUFFER(output.rectifyImgMain1);
    LOG_IMGBUFFER(output.rectifyImgMain2);
    LOG_IMGBUFFER(output.jpsImgMain2);
    MY_LOGD("output.maskMain1=%x",output.maskMain1);
    MY_LOGD("output.maskMain2=%x",output.maskMain2);
    MY_LOGD("output.ldcMain1=%x",output.ldcMain1);
    MY_LOGD("output.warpingMatrix=%x",output.warpingMatrix);
    MY_LOGD("output.warpingMatrixSize=%d",output.warpingMatrixSize);


    MY_LOGD("-");

    #undef LOG_IMGBUFFER
}

MBOOL
N3DNode::
prepareN3DOutputParam(N3D_HAL_OUTPUT& rN3dParam, ImgInfoMapPtr pToDPEImgInfo, ImgInfoMapPtr pToOCCImgInfo)
{
    // prepare YUV/MASK
    prepareN3DOutputYUVMask(rN3dParam, pToDPEImgInfo);

    // prepare LDC buffer
    SmartImageBuffer smImgBuf_LDC = mBufPoolSet.mLDCBufPool->request();
    pToOCCImgInfo->addImageBuffer(BID_N3D_OUT_LDC, smImgBuf_LDC);
    rN3dParam.ldcMain1 = (MUINT8*) smImgBuf_LDC->mImageBuffer->getBufVA(0);

    VSDOF_LOGD("rN3dParam.ldcMain1 size=%dx%d plane_bytes=(%d)", smImgBuf_LDC->mImageBuffer->getImgSize().w,
                smImgBuf_LDC->mImageBuffer->getImgSize().h, smImgBuf_LDC->mImageBuffer->getBufSizeInBytes(0));

    return MTRUE;
}

const char*
N3DNode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_N3D_OUT_MV_Y);
        MAKE_NAME_CASE(BID_N3D_OUT_SV_Y);
        MAKE_NAME_CASE(BID_N3D_OUT_MASK_M);
        MAKE_NAME_CASE(BID_N3D_OUT_MASK_S);
        MAKE_NAME_CASE(BID_N3D_OUT_LDC);
        default:
            MY_LOGW("unknown BID:%d", BID);
            return "unknown";
    }

#undef MAKE_NAME_CASE
}

MBOOL
N3DNode::
prepareN3DOutputYUVMask(
    N3D_HAL_OUTPUT& rN3dParam,
    ImgInfoMapPtr pToDPEImgInfo)
{
    // request output buffers
    SmartImageBuffer smImgBuf_MV_Y = mBufPoolSet.mN3DImgBufPool->request();
    SmartImageBuffer smImgBuf_SV_Y = mBufPoolSet.mN3DImgBufPool->request();
    SmartImageBuffer smImgBuf_MASK_S = mBufPoolSet.mN3DMaskBufPool->request();
    SmartImageBuffer smImgBuf_MASK_M = mBufPoolSet.mN3DMaskBufPool->request();

    // insert to OutputInfoMap and fill in the N3D output params
    pToDPEImgInfo->addImageBuffer(BID_N3D_OUT_MV_Y, smImgBuf_MV_Y);
    rN3dParam.rectifyImgMain1 = smImgBuf_MV_Y->mImageBuffer.get();

    pToDPEImgInfo->addImageBuffer(BID_N3D_OUT_SV_Y, smImgBuf_SV_Y);
    rN3dParam.rectifyImgMain2 = smImgBuf_SV_Y->mImageBuffer.get();

    pToDPEImgInfo->addImageBuffer(BID_N3D_OUT_MASK_M, smImgBuf_MASK_M);
    rN3dParam.maskMain1 = (MUINT8*) smImgBuf_MASK_M->mImageBuffer->getBufVA(0);

    pToDPEImgInfo->addImageBuffer(BID_N3D_OUT_MASK_S, smImgBuf_MASK_S);
    rN3dParam.maskMain2 = (MUINT8*) smImgBuf_MASK_S->mImageBuffer->getBufVA(0);

    VSDOF_LOGD("rN3dParam.rectifyImgMain1 size=%dx%d format=%x",
                smImgBuf_MV_Y->mImageBuffer->getImgSize().w, smImgBuf_MV_Y->mImageBuffer->getImgSize().h,
                smImgBuf_MV_Y->mImageBuffer->getImgFormat());

    VSDOF_LOGD("rN3dParam.rectifyImgMain2 size=%dx%d format=%x",
                smImgBuf_SV_Y->mImageBuffer->getImgSize().w, smImgBuf_SV_Y->mImageBuffer->getImgSize().h,
                smImgBuf_SV_Y->mImageBuffer->getImgFormat());

    VSDOF_LOGD("rN3dParam.maskMain1 size=%dx%d format=%x", smImgBuf_MASK_M->mImageBuffer->getImgSize().w,
                smImgBuf_MASK_M->mImageBuffer->getImgSize().h, smImgBuf_MASK_M->mImageBuffer->getImgFormat());

    VSDOF_LOGD("rN3dParam.maskMain2 size=%dx%d format=%x", smImgBuf_MASK_S->mImageBuffer->getImgSize().w,
                smImgBuf_MASK_S->mImageBuffer->getImgSize().h, smImgBuf_MASK_S->mImageBuffer->getImgFormat());

    return MTRUE;
}

MBOOL
N3DNode::
prepareN3DOutputParam_CAP(
    N3D_HAL_OUTPUT_CAPTURE& rN3dParam,
    ImgInfoMapPtr pToDPEImgInfo)
{
    // prepare YUV/MASK
    prepareN3DOutputYUVMask(rN3dParam, pToDPEImgInfo);
    // get LDC buffer from EffectRequest in Capture
    EffectRequestPtr pRequest = pToDPEImgInfo->getRequestPtr();
    FrameInfoPtr pFramePtr;
    sp<IImageBuffer> pImgBuf_LDC;
    RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(pRequest, pFramePtr, BID_N3D_OUT_LDC, pImgBuf_LDC);
    if(pImgBuf_LDC != NULL)
        rN3dParam.ldcMain1 = (MUINT8*) pImgBuf_LDC->getBufVA(0);

    // prepare the JPSMain2
    sp<IImageBuffer> pImgBuf_JPS;
    RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(pRequest, pFramePtr, BID_N3D_OUT_JPS_MAIN2, pImgBuf_JPS);
    // warping matrix
    rN3dParam.warpingMatrix = NULL;

    MBOOL ret = MTRUE;
    if(pImgBuf_JPS.get() && pImgBuf_LDC.get())
    {
        rN3dParam.jpsImgMain2 = pImgBuf_JPS.get();
        rN3dParam.ldcMain1 = (MUINT8*) pImgBuf_LDC->getBufVA(0);
        return MTRUE;
    }

    return MFALSE;
}

MBOOL
N3DNode::
prepareEISData(ImgInfoMapPtr& rP2ImgInfo, EIS_DATA& rEISData)
{
    EffectRequestPtr pRequest = rP2ImgInfo->getRequestPtr();
    DepthMapBufferID metaBID_InApp = mapQueuedBufferID(pRequest, BID_META_IN_APP);
    FrameInfoPtr framePtr_inAppMeta = pRequest->vInputFrameInfo.valueFor(metaBID_InApp);
    IMetadata* pMeta_InApp  = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);

    if(isEISOn(pMeta_InApp))
    {
        DepthMapBufferID metaBID_InHal = mapQueuedBufferID(pRequest, BID_META_IN_HAL);
        FrameInfoPtr framePtr_inHalMeta = pRequest->vInputFrameInfo.valueFor(metaBID_InHal);
        IMetadata* pMeta_InHal  = getMetadataFromFrameInfoPtr(framePtr_inHalMeta);

        eis_region region;
        queryEisRegion(pMeta_InHal, region, pRequest);

        rEISData.isON = true;
        rEISData.eisOffset.x = region.x_int;
        rEISData.eisOffset.y = region.y_int;
        rEISData.eisImgSize = region.s;
    }
    else
    {
        rEISData.isON = false;
    }
    return MTRUE;
}

MBOOL
N3DNode::
prepareFEFMData(ImgInfoMapPtr& rP2ImgInfo, HWFEFM_DATA& rFefmData)
{
    CAM_TRACE_NAME("prepareFEFMData");
    VSDOF_PRFLOG("prepareFEFMData +");
    // N3D input FEO/FMO data
    SmartImageBuffer smFe1bo, smFe2bo, smFe1co, smFe2co;
    SmartImageBuffer smFmboLR, smFmboRL, smFmcoLR, smFmcoRL;

    smFe1bo = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FE1BO);
    smFe2bo = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FE2BO);
    smFe1co = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FE1CO);
    smFe2co = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FE2CO);
    smFe1bo->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smFe2bo->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smFe1co->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smFe2co->mImageBuffer->syncCache(eCACHECTRL_INVALID);


    rFefmData.geoDataMain1[0] = (MUINT16*)smFe1bo->mImageBuffer->getBufVA(0);
    rFefmData.geoDataMain1[1] = (MUINT16*)smFe1co->mImageBuffer->getBufVA(0);
    rFefmData.geoDataMain1[2] = NULL;

    rFefmData.geoDataMain2[0] = (MUINT16*)smFe2bo->mImageBuffer->getBufVA(0);
    rFefmData.geoDataMain2[1] = (MUINT16*)smFe2co->mImageBuffer->getBufVA(0);
    rFefmData.geoDataMain2[2] = NULL;

    smFmboLR = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FMBO_LR);
    smFmboRL = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FMBO_RL);
    smFmcoLR = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FMCO_LR);
    smFmcoRL = rP2ImgInfo->getImageBuffer(BID_P2AFM_OUT_FMCO_RL);
    smFmboLR->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smFmboRL->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smFmcoLR->mImageBuffer->syncCache(eCACHECTRL_INVALID);
    smFmcoRL->mImageBuffer->syncCache(eCACHECTRL_INVALID);

    rFefmData.geoDataLeftToRight[0] = (MUINT16*) smFmboLR->mImageBuffer->getBufVA(0);
    rFefmData.geoDataLeftToRight[1] = (MUINT16*) smFmcoLR->mImageBuffer->getBufVA(0);
    rFefmData.geoDataLeftToRight[2] = NULL;

    rFefmData.geoDataRightToLeft[0] = (MUINT16*) smFmboRL->mImageBuffer->getBufVA(0);
    rFefmData.geoDataRightToLeft[1] = (MUINT16*) smFmcoRL->mImageBuffer->getBufVA(0);
    rFefmData.geoDataRightToLeft[2] = NULL;

    VSDOF_PRFLOG("prepareFEFMData -");
    return MTRUE;
}

}; //NSFeaturePipe
}; //NSCamFeature
}; //NSCam



