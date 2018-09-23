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
//

#include <mtkcam/feature/stereo/pipe/vsdof_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
//
#define PIPE_MODULE_TAG "BokehPipe"
#define PIPE_CLASS_TAG "GFNode"
//
#include "GFNode.h"
#include "DepthMapPipeUtils.h"
#include "../util/vsdof_util.h"
//
#include <string>
#include <chrono>
#include <DpBlitStream.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#define PIPE_CLASS_TAG "GFNode"
#include <featurePipe/core/include/PipeLog.h>
/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam{
namespace NSCamFeature{
namespace NSFeaturePipe{

using namespace std;
using namespace VSDOF::util;

//************************************************************************
//
//************************************************************************
GFNode::
GFNode(
    const char *name,
    DepthFlowType flowType)
    : DepthMapPipeNode(name, flowType)
{
    MY_LOGD("ctor(0x%x)", this);
    this->addWaitQueue(&mJobQueue);
}
//************************************************************************
//
//************************************************************************
GFNode::
~GFNode()
{
    MY_LOGD("dctor(0x%x)", this);
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
onInit()
{
    CAM_TRACE_NAME("GFNode::onInit");
    MY_LOGD("+");

    // create gf_hal for PV/VR
    mpGf_Hal = GF_HAL::createInstance(eSTEREO_SCENARIO_PREVIEW);
    // create gf_hal for CAPTURE
    mpGf_Hal_Cap = GF_HAL::createInstance(eSTEREO_SCENARIO_CAPTURE);
    if(!mpGf_Hal || !mpGf_Hal_Cap)
    {
        MY_LOGE("Create GF_HAL fail.");
        cleanUp();
    }

    // internal depthmap : for GF - SW usage, for MDP rotate: HW read
    MUINT32 usage = ImageBufferPool::USAGE_SW | eBUFFER_USAGE_HW_CAMERA_READ;
    // create depthmap internal buffer pool for g-sensor rotation
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    MSize szDepthMapSize = pSizeProvder->getBufferSize(E_DEPTH_MAP, eSTEREO_SCENARIO_CAPTURE).size;
    CREATE_IMGBUF_POOL(mpInternalDepthMapImgBufPool, "InternalDepthMapImgBufPool", szDepthMapSize,
                        eImgFmt_Y8, usage, MTRUE);
    ALLOCATE_BUFFER_POOL(mpInternalDepthMapImgBufPool, 2);
    // init DpStream
    mpDpStream = new DpBlitStream();

    MY_LOGD("-");
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
createBufferPool(
    android::sp<ImageBufferPool> &pPool,
    MUINT32 width,
    MUINT32 height,
    NSCam::EImageFormat format,
    MUINT32 bufCount,
    const char* caller,
    MUINT32 bufUsage)
{
    MY_LOGD("+");
    MBOOL ret = MFALSE;
    pPool = ImageBufferPool::create(caller, width, height, format, bufUsage);
    if(pPool == nullptr)
    {
        ret = MFALSE;
        MY_LOGE("Create [%s] failed.", caller);
        goto lbExit;
    }
    for(MUINT32 i=0;i<bufCount;++i)
    {
        if(!pPool->allocate())
        {
            ret = MFALSE;
            MY_LOGE("Allocate [%s] working buffer failed.", caller);
            goto lbExit;
        }
    }
    ret = MTRUE;
    MY_LOGD("-");
lbExit:
    return ret;
}
//************************************************************************
//
//************************************************************************
MVOID
GFNode::
setImageBufferValue(SmartImageBuffer& buffer,
                MINT32 width,
                MINT32 height,
                MINT32 value)
{
    MUINT8* data = (MUINT8*)buffer->mImageBuffer->getBufVA(0);
    memset(data, value, sizeof(MUINT8) * width * height);
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
onUninit()
{
    MY_LOGD("+");
    cleanUp();
    MY_LOGD("-");
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MVOID
GFNode::
cleanUp()
{
    MY_LOGD("+");
    mJobQueue.clear();
    // release gf_hal
    delete mpGf_Hal;
    delete mpGf_Hal_Cap;
    mpGf_Hal = nullptr;
    mpGf_Hal_Cap = nullptr;
    // delete DPStream
    if(mpDpStream != NULL)
        delete mpDpStream;
    // release pool
    ImageBufferPool::destroy(mpInternalDepthMapImgBufPool);
    MY_LOGD("-");
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
onThreadStart()
{
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
onThreadStop()
{
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
onData(DataID id, ImgInfoMapPtr &data)
{
    TRACE_FUNC_ENTER();
    EffectRequestPtr pRequest = data->getRequestPtr();
    VSDOF_PRFLOG("reqID=%d + dataID=%d", pRequest->getRequestNo(), id);
    MBOOL ret = MFALSE;
    FrameInfoPtr frameInfo;
    //
    switch(id)
    {
        case P2AFM_TO_GF_MY_SL:
        {
            Mutex::Autolock _l(mMYSLMutex);
            mMYSLInfoMap.add(pRequest->getRequestNo(), data);
            ret = MTRUE;
            break;
        }
        case P2AFM_TO_GF_DMW_MYS:
            // insert into Job queue
            mJobQueue.enque(data);
            ret = MTRUE;
            break;
        case WMF_TO_GF_DMW_MY_S:
            mJobQueue.enque(data);
            ret = MTRUE;
            break;
        default:
            ret = MFALSE;
            break;
    }
    //
    VSDOF_PRFLOG("-");
    TRACE_FUNC_EXIT();
    return ret;
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
onThreadLoop()
{
    ImgInfoMapPtr pImgInfoMap;;
    //
    if( !waitAllQueue() )
    {
        return MFALSE;
    }
    //
    if( !mJobQueue.deque(pImgInfoMap) )
    {
        return MFALSE;
    }
    CAM_TRACE_NAME("GFNode::onThreadLoop");
    VSDOF_PRFLOG("GFNode onThreadLoop: reqID=%d",
                pImgInfoMap->getRequestPtr()->getRequestNo());
    //
    if(!executeAlgo(pImgInfoMap))
    {
        return MFALSE;
    }
    //
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
executeAlgo(
    ImgInfoMapPtr pSrcImgInfoMap)
{
    MBOOL ret = MFALSE;
    EffectRequestPtr pRequest = pSrcImgInfoMap->getRequestPtr();
    DepthNodeOpState eState = getRequestState(pRequest);
    VSDOF_LOGD("+ reqId=%d", pRequest->getRequestNo());
    // Normal pass
    if(!runNormalPass(pSrcImgInfoMap, eState))
    {
        MY_LOGE("GF NormalPass failed!");
        goto lbExit;
    }
    // Capture pass
    if(eState == eSTATE_CAPTURE && !runCapturePass(pSrcImgInfoMap, eState))
    {
        MY_LOGE("GF CapturePass failed!");
        goto lbExit;
    }
    //
    ret = MTRUE;
    VSDOF_LOGD("- reqId=%d", pRequest->getRequestNo());
    CAM_TRACE_END();
lbExit:
    return ret;
}
//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
runNormalPass(
    ImgInfoMapPtr pSrcImgInfoMap,
    DepthNodeOpState eOpState
)
{
    CAM_TRACE_BEGIN("GFNode::runNormalPass");
    EffectRequestPtr pRequest = pSrcImgInfoMap->getRequestPtr();

    GF_HAL_IN_DATA sInData;
    GF_HAL_OUT_DATA sOutData;

    sInData.isCapture = (eSTATE_CAPTURE == getRequestState(pRequest));

    MINT32 iReqIdx = pRequest->getRequestNo();

    if(!requireAlgoDataFromRequest(pSrcImgInfoMap, eGF_PASS_NORMAL, sInData, sOutData))
    {
        MY_LOGE("get algo buffer fail, reqID=%d", iReqIdx);
        return MFALSE;
    }
    //
    debugGFParams(sInData, sOutData);
    //
    SimpleTimer timer(MTRUE);
    if(!mpGf_Hal->GFHALRun(sInData, sOutData))
    {
        MY_LOGE("GFHalRun fail, reqID=%d", iReqIdx);
        return MFALSE;
    }
    VSDOF_PRFLOG("[NormalPass]gf algo processing time(%lf ms) reqID=%d",
                                timer.countTimer(), iReqIdx);
    FrameInfoPtr pFrameInfo;
    // mark DMBG ready
    RETRIEVE_OFRAMEINFO(pRequest, pFrameInfo, BID_GF_OUT_DMBG);
    pFrameInfo->setFrameReady(true);
    handleDataAndDump(GF_OUT_DMBG, pFrameInfo, eOpState);
    CAM_TRACE_END();
    return MTRUE;
}

//************************************************************************
//
//************************************************************************
const char*
GFNode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_GF_OUT_DMBG);
        MAKE_NAME_CASE(BID_GF_OUT_DEPTHMAP);
        default:
            MY_LOGW("unknown BID:%d", BID);
            return "unknown";
    }

#undef MAKE_NAME_CASE
}

//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
runCapturePass(
    ImgInfoMapPtr pSrcImgInfoMap,
    DepthNodeOpState eOpState
)
{
    CAM_TRACE_BEGIN("GFNode::runCapturePass");
    EffectRequestPtr pRequest = pSrcImgInfoMap->getRequestPtr();
    GF_HAL_IN_DATA sInData;
    GF_HAL_OUT_DATA sOutData;

    if(!requireAlgoDataFromRequest(pSrcImgInfoMap, eGF_PASS_CAPTURE, sInData, sOutData))
    {
        MY_LOGE("get algo buffer fail.");
        return MFALSE;
    }
    //
    debugGFParams(sInData, sOutData);
    //
    SimpleTimer timer(MTRUE);
    if(!mpGf_Hal_Cap->GFHALRun(sInData, sOutData))
    {
        MY_LOGE("GFHalRun fail");
        return MFALSE;
    }
    VSDOF_PRFLOG("[CapturePass]gf algo processing time(%lf ms) reqID=%d",
                                timer.countTimer(), pRequest->getRequestNo());
    // release MYSL data
    {
        Mutex::Autolock _l(mMYSLMutex);
        mMYSLInfoMap.removeItem(pRequest->getRequestNo());
    }

    // jpeg rotation
    if(jpegRotationOnDepthMap(pSrcImgInfoMap))
    {
        // depth map frame info
        FrameInfoPtr pDepthMapFrameInfo;
        RETRIEVE_OFRAMEINFO(pRequest, pDepthMapFrameInfo, BID_GF_OUT_DEPTHMAP);
        // mark DepthMap ready
        pDepthMapFrameInfo->setFrameReady(true);
        handleDataAndDump(GF_OUT_DEPTHMAP, pDepthMapFrameInfo, eOpState );
    }
    else
        MY_LOGE("Failed to rotate DepthMap according to jpegOrientation, please check!!! reqID=%d",
                pRequest->getRequestNo());

    CAM_TRACE_END();
    return MTRUE;
}


//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
jpegRotationOnDepthMap(
    const ImgInfoMapPtr pSrcImgInfoMap
)
{
    EffectRequestPtr pRequest = pSrcImgInfoMap->getRequestPtr();

    // get gsensor rotation
    MINT32 jpegOrientation = 0;
    MBOOL bRet = tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                                        BID_META_IN_APP, MTK_JPEG_ORIENTATION, jpegOrientation);

    //do roataion
    if(jpegOrientation > 0)
    {
        VSDOF_LOGD("Do g-sensor rotation: %d degree", jpegOrientation);
        // get source DepthMap buffer
        SmartImageBuffer sm_DepthMap = pSrcImgInfoMap->getImageBuffer(BID_GF_INTERAL_DEPTHMAP);

        if(sm_DepthMap.get() == NULL)
            MY_LOGE("Failed to get DepthMap buffer");
        else
        {
            // depthMap frameInfo
            FrameInfoPtr pDepthMapFrameInfo;
            RETRIEVE_OFRAMEINFO(pRequest, pDepthMapFrameInfo, BID_GF_OUT_DEPTHMAP);
            // output DepthMap buffer
            sp<IImageBuffer> pImgBuf;
            pDepthMapFrameInfo->getFrameBuffer(pImgBuf);

            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = sm_DepthMap->mImageBuffer.get();
            config.pDstBuffer = pImgBuf.get();
            config.rotAngle = jpegOrientation;

            if(!excuteMDP(config))
            {
                MY_LOGE("excuteMDP fail: Cannot roatate depth map.");
                return MFALSE;
            }
        }
    }

    return MTRUE;

}

//************************************************************************
//
//************************************************************************
MVOID
GFNode::
debugGFParams(
    const GF_HAL_IN_DATA& inData,
    const GF_HAL_OUT_DATA& outData
)
{
    if(!mbDebugLog)
        return;

    MY_LOGD("Input GFParam: GF_HAL_IN_DATA");
    MY_LOGD("magicNumber=%d", inData.magicNumber);
    MY_LOGD("scenario=%d", inData.scenario);
    MY_LOGD("dofLevel=%d", inData.dofLevel);
    MY_LOGD("depthMap=%x", inData.depthMap);
    MY_LOGD("images.size()=%d", inData.images.size());
    for(ssize_t idx=0;idx<inData.images.size();++idx)
    {
        MY_LOGD("images[%d]=%x", idx, inData.images[idx]);
        if(inData.images[idx])
        {
            MY_LOGD("images[%d], image size=%dx%d", idx,
                inData.images[idx]->getImgSize().w, inData.images[idx]->getImgSize().h);
        }
    }

    MY_LOGD("convOffset=%f", inData.convOffset);

}

//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
requireInputMetaFromRequest(
    const ImgInfoMapPtr pSrcImgInfoMap,
    GF_HAL_IN_DATA& inData
)
{
    EffectRequestPtr pRequest = pSrcImgInfoMap->getRequestPtr();
    MINT32 iReqIdx = pRequest->getRequestNo();
    DepthNodeOpState eState = getRequestState(pRequest);
    // config DOF level
    if(!tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
            BID_META_IN_APP, MTK_STEREO_FEATURE_DOF_LEVEL, inData.dofLevel))
    {
        MY_LOGE("reqID=%d Cannot find MTK_STEREO_FEATURE_DOF_LEVEL meta in AppMeta!", iReqIdx);
        return MFALSE;
    }

    // config touch info
    sp<EffectFrameInfo> pFrameInfo;
    pFrameInfo = pRequest->vInputFrameInfo.valueFor(BID_META_IN_APP);
    IMetadata* pMeta;
    sp<EffectParameter> effParam = pFrameInfo->getFrameParameter();
    pMeta = reinterpret_cast<IMetadata*>(effParam->getPtr(VSDOF_PARAMS_METADATA_KEY));
    if( pMeta != NULL ) {
        IMetadata::IEntry entry = pMeta->entryFor(MTK_STEREO_FEATURE_TOUCH_POSITION);
        if( !entry.isEmpty() ) {
            inData.touchPosX = entry.itemAt(0, Type2Type<MINT32>());
            inData.touchPosY = entry.itemAt(1, Type2Type<MINT32>());
        }
    }

    MINT32 iSensorMode;
    // config scenario
    if(!tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                            BID_META_IN_HAL, MTK_P1NODE_SENSOR_MODE, iSensorMode))
    {
        MY_LOGE("reqID=%d Cannot find MTK_P1NODE_SENSOR_MODE meta in HalMeta!", iReqIdx);
        return MFALSE;
    }
    inData.scenario = getStereoSenario(iSensorMode);

    // config magic number
    if(!tryGetMetadataInFrame<MINT32>(pRequest, FRAME_INPUT,
                            BID_META_IN_HAL, MTK_P1NODE_PROCESSOR_MAGICNUM, inData.magicNumber))
    {
        MY_LOGE("reqID=%d Cannot find MTK_P1NODE_PROCESSOR_MAGICNUM meta in HalMeta!", iReqIdx);
        return MFALSE;
    }

    // read convergence from metedata of cuttrnt frame,
    if(!tryGetMetadataInFrame<MFLOAT>(pRequest, FRAME_OUTPUT,
                        BID_META_OUT_HAL, MTK_CONVERGENCE_DEPTH_OFFSET, inData.convOffset))
    {
        MY_LOGE("reqID=%d Cannot find MTK_CONVERGENCE_DEPTH_OFFSET meta in outHalMeta!", iReqIdx);
        return MFALSE;
    }

    return MTRUE;
}

//************************************************************************
//
//************************************************************************
MBOOL
GFNode::
requireAlgoDataFromRequest(
    const ImgInfoMapPtr pSrcImgInfoMap,
    GuidedFilterPass gfPass,
    GF_HAL_IN_DATA& inData,
    GF_HAL_OUT_DATA& outData)
{
    FrameInfoPtr pFrameInfo;
    sp<IImageBuffer> pImgBuf_DMBG = NULL, pImgBuf_DepthMap = NULL;
    EffectRequestPtr pRequest = pSrcImgInfoMap->getRequestPtr();
    MINT32 iReqIdx = pRequest->getRequestNo();
    VSDOF_LOGD("reqID=%d gfPass=%d", pRequest->getRequestNo(), gfPass);

    // require input meta
    if(!requireInputMetaFromRequest(pSrcImgInfoMap, inData))
        return MFALSE;

    // config MYS buffer.
    SmartImageBuffer smMYS = pSrcImgInfoMap->getImageBuffer(BID_OCC_OUT_MY_S);
    inData.images.push_back(smMYS->mImageBuffer.get());
    // config DMW buffer.
    SmartImageBuffer smDMW = pSrcImgInfoMap->getImageBuffer(BID_WMF_OUT_DMW);
    if(smDMW == nullptr && this->isQueuedDepthRequest(pRequest))
        inData.depthMap = nullptr;
    else
        inData.depthMap = (MUINT8*)smDMW->mImageBuffer->getBufVA(0);

    // check capture pass
    if(gfPass==eGF_PASS_CAPTURE)
    {

        ImgInfoMapPtr pMYSLInfoMap;
        // check MY_SL exist
        {
            ssize_t index;
            Mutex::Autolock _l(mMYSLMutex);
            if((index=mMYSLInfoMap.indexOfKey(pRequest->getRequestNo()))>=0)
                pMYSLInfoMap = mMYSLInfoMap.valueAt(index);
            else
            {
                MY_LOGE("reqID=%d Cannot find MYSL ImgInfoMap!", iReqIdx);
                return MFALSE;
            }
        }
        // config extra input image
        SmartImageBuffer smBuf = pMYSLInfoMap->getImageBuffer(BID_TO_GF_MY_SLL);
        inData.images.push_back(smBuf->mImageBuffer.get());
        smBuf = pMYSLInfoMap->getImageBuffer(BID_TO_GF_MY_SL);
        inData.images.push_back(smBuf->mImageBuffer.get());

        // get rotation
        MINT32 jpegOrientation = 0;
        MBOOL bRet = tryGetMetadataInFrame<MINT32>(pSrcImgInfoMap->getRequestPtr(), FRAME_INPUT,
                                            BID_META_IN_APP, MTK_JPEG_ORIENTATION, jpegOrientation);
        // config output buffer: depthMap
        // no need to rotate
        if(jpegOrientation == 0)
        {
            RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(pRequest, pFrameInfo, BID_GF_OUT_DEPTHMAP, pImgBuf_DepthMap);
            if(pImgBuf_DepthMap == NULL)
                return MFALSE;
        }
        else
        {
            // request working buffer as DepthMap output buffer
            SmartImageBuffer sm_DepthMap = mpInternalDepthMapImgBufPool->request();
            // record requested buffer into pSrcImgInfoMap
            pSrcImgInfoMap->addImageBuffer(BID_GF_INTERAL_DEPTHMAP, sm_DepthMap);
            pImgBuf_DepthMap = sm_DepthMap->mImageBuffer;
        }

        VSDOF_LOGD("reqID=%d depthMap size=%dx%d", pRequest->getRequestNo(),
                        pImgBuf_DepthMap->getImgSize().w, pImgBuf_DepthMap->getImgSize().h);
        outData.depthMap = (MUINT8*) pImgBuf_DepthMap->getBufVA(0);
        outData.dmbg = NULL;
    }
    else
    {
        // config output buffers: DMBG
        RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(pRequest, pFrameInfo, BID_GF_OUT_DMBG, pImgBuf_DMBG);
        if(pImgBuf_DMBG == NULL)
            return MFALSE;
        outData.dmbg = (MUINT8*) pImgBuf_DMBG->getBufVA(0);
        outData.depthMap = NULL;
    }

    return MTRUE;
}

};
};
};
