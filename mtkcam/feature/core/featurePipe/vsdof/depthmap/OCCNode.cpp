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

#include "OCCNode.h"
#include "DepthMapPipe_Common.h"

#define PIPE_CLASS_TAG "OCCNode"
#include <featurePipe/core/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {


OCCNode::BufferSizeConfig::
BufferSizeConfig()
{
    // setup constants
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    VSDOF_DMH_SIZE = pSizeProvder->getBufferSize(E_DMH, eSTEREO_SCENARIO_RECORD);
    VSDOF_MYS_SIZE = pSizeProvder->getBufferSize(E_MY_S, eSTEREO_SCENARIO_RECORD);
}

OCCNode::BufferSizeConfig::
~BufferSizeConfig()
{
}

OCCNode::BufferPoolSet::
BufferPoolSet()
{}

OCCNode::BufferPoolSet::
~BufferPoolSet()
{}

MBOOL
OCCNode::BufferPoolSet::
init(const BufferSizeConfig& config)
{
    // buffer usage
    MUINT32 usage = eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ;
    // DMH
    CREATE_IMGBUF_POOL(mDMHImgBufPool, "DMHBufferPool", config.VSDOF_DMH_SIZE, eImgFmt_Y8, usage, MTRUE);
    ALLOCATE_BUFFER_POOL(mDMHImgBufPool, VSDOF_WORKING_BUF_SET);
    // MY_S
    CREATE_IMGBUF_POOL(mMYSImgBufPool, "MYSBufferPool", config.VSDOF_MYS_SIZE, eImgFmt_YV12, usage, MTRUE);
    ALLOCATE_BUFFER_POOL(mMYSImgBufPool, VSDOF_WORKING_BUF_SET);

    return MTRUE;
}

MBOOL
OCCNode::BufferPoolSet::
uninit()
{
    ImageBufferPool::destroy(mDMHImgBufPool);
    ImageBufferPool::destroy(mMYSImgBufPool);
    return MTRUE;
}

OCCNode::
OCCNode(const char* name, DepthFlowType flowType)
: DepthMapPipeNode(name, flowType)
{
    this->addWaitQueue(&mJobQueue);
    this->addWaitQueue(&mLDCBufQueue);


}

OCCNode::
~OCCNode()
{
}

MVOID
OCCNode::
cleanUp()
{
    VSDOF_LOGD("+");
    if(mpOCCHAL!=nullptr)
    {
        delete mpOCCHAL;
        mpOCCHAL = nullptr;
    }
    mJobQueue.clear();
    mLDCBufQueue.clear();
    mLDCImageBufMap.clear();
    mBufPoolSet.uninit();
    VSDOF_LOGD("-");
}

MBOOL
OCCNode::
onInit()
{
    VSDOF_LOGD("+");
    CAM_TRACE_NAME("OCCNode::onInit");
    // buffer pool set init
    mBufPoolSet.init(mSizConfig);
    // OCC HAL initialization
    mpOCCHAL = OCC_HAL::createInstance();
    VSDOF_LOGD("-");

    return MTRUE;
}

MBOOL
OCCNode::
onUninit()
{
    CAM_TRACE_NAME("OCCNode::onUninit");
    cleanUp();
    return MTRUE;
}

MBOOL
OCCNode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
OCCNode::
onThreadStop()
{
    return MTRUE;
}


MBOOL
OCCNode::
onData(DataID data, ImgInfoMapPtr& pImgInfo)
{
    MBOOL ret = MTRUE;
    VSDOF_LOGD("+, DataID=%d reqId=%d", data, pImgInfo->getRequestPtr()->getRequestNo());

    switch(data)
    {
        case N3D_TO_OCC_LDC:
        {
            mLDCBufQueue.enque(pImgInfo);
            break;
        }
        case DPE_TO_OCC_MVSV_DMP_CFM:
            mJobQueue.enque(pImgInfo);
            break;
        default:
            MY_LOGW("Unrecongnized DataID=%d", data);
            ret = MFALSE;
            break;
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL
OCCNode::
onThreadLoop()
{
    ImgInfoMapPtr pDPEImgInfo;
    ImgInfoMapPtr pN3DImgInfo;
    EffectRequestPtr reqPtr;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }

    if( !mJobQueue.deque(pDPEImgInfo) )
    {
        MY_LOGE("mJobQueue.deque() failed");
        return MFALSE;
    }

    if( !mLDCBufQueue.deque(pN3DImgInfo) )
    {
        MY_LOGE("mLDCBufQueue.deque() failed");
        return MFALSE;
    }

    if(pDPEImgInfo->getRequestPtr()->getRequestNo() != pN3DImgInfo->getRequestPtr()->getRequestNo())
    {
        MY_LOGE("The DPE ImgInfo is different fron N3D ImgInfo.");
        return MFALSE;
    }

    CAM_TRACE_NAME("OCCNode::onThreadLoop");
    // mark on-going-request start
    this->incExtThreadDependency();

    reqPtr = pDPEImgInfo->getRequestPtr();
    MUINT32 iReqNo = reqPtr->getRequestNo();
    VSDOF_PRFLOG("OCC threadloop start, reqID=%d", iReqNo);

    OCC_HAL_PARAMS occInputParams;
    OCC_HAL_OUTPUT occOutputParams;
    ImgInfoMapPtr toWMFImgInfo = new ImageBufInfoMap(reqPtr);
    // prepare in/out params
    bool ret = prepareOCCParams(pDPEImgInfo, pN3DImgInfo, occInputParams, occOutputParams, toWMFImgInfo);

    if(!ret)
    {
        MY_LOGE("OCC ALGO stopped because of the enque parameter error.");
        // mark on-going-request end
        this->decExtThreadDependency();
        return MFALSE;
    }
    SimpleTimer timer;
    VSDOF_PRFLOG("OCC ALGO start, reqID=%d t=%d", reqPtr->getRequestNo(), timer.startTimer());
    CAM_TRACE_BEGIN("OCCNode::OCCHALRun");
    ret = mpOCCHAL->OCCHALRun(occInputParams, occOutputParams);
    CAM_TRACE_END();
    VSDOF_PRFLOG("OCC ALGO end, reqID=%d, exec-time=%f msec", reqPtr->getRequestNo(), timer.countTimer());

    if(ret)
    {
        // output to WMF node
        handleDataAndDump(OCC_TO_WMF_DMH_MY_S, toWMFImgInfo);
    }
    else
    {
        MY_LOGE("OCC ALGO failed: reqID=%d", reqPtr->getRequestNo());
        handleData(ERROR_OCCUR_NOTIFY, reqPtr);
    }

    // mark on-going-request end
    this->decExtThreadDependency();

    return MTRUE;
}

const char*
OCCNode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_OCC_OUT_DMH);
        MAKE_NAME_CASE(BID_OCC_OUT_MY_S);
        default:
            MY_LOGW("unknown BID:%d", BID);
            return "unknown";
    }

#undef MAKE_NAME_CASE
}

MBOOL
OCCNode::
prepareOCCParams(
    ImgInfoMapPtr& pDPEImgInfo,
    ImgInfoMapPtr& pN3DImgInfo,
    OCC_HAL_PARAMS& enqueParams,
    OCC_HAL_OUTPUT& outputParams,
    ImgInfoMapPtr& toWMFImgInfo
)
{
    EffectRequestPtr pEffectReq = pDPEImgInfo->getRequestPtr();
    //
    SmartImageBuffer smMV_Y = pDPEImgInfo->getImageBuffer(BID_N3D_OUT_MV_Y);
    SmartImageBuffer smSV_Y = pDPEImgInfo->getImageBuffer(BID_N3D_OUT_SV_Y);
    SmartImageBuffer smDMP_L = pDPEImgInfo->getImageBuffer(BID_DPE_OUT_DMP_L);
    SmartImageBuffer smDMP_R = pDPEImgInfo->getImageBuffer(BID_DPE_OUT_DMP_R);
    //
    enqueParams.imageMain1 = smMV_Y->mImageBuffer.get();
    enqueParams.imageMain2 = smSV_Y->mImageBuffer.get();
    enqueParams.disparityLeftToRight = (MUINT16*) smDMP_L->mImageBuffer->getBufVA(0);
    enqueParams.disparityRightToLEft = (MUINT16*) smDMP_R->mImageBuffer->getBufVA(0);
    enqueParams.confidenceMap = NULL;
    enqueParams.requestNumber = pEffectReq->getRequestNo();

    // push LDC info
    sp<IImageBuffer> pLDCImgBuf;
    DepthNodeOpState eState = getRequestState(pEffectReq);
    if(eState == eSTATE_NORMAL)
    {
        SmartImageBuffer smLDC = pN3DImgInfo->getImageBuffer(BID_N3D_OUT_LDC);
        pLDCImgBuf = smLDC->mImageBuffer;
        enqueParams.ldcMain1 = (MUINT8*) smLDC->mImageBuffer->getBufVA(0);
    }
    else
    {
        FrameInfoPtr pFrameInfo;
        RETRIEVE_OFRAMEINFO_IMGBUF(pEffectReq, pFrameInfo, BID_N3D_OUT_LDC, pLDCImgBuf);

        if(!pLDCImgBuf.get())
        {
            MY_LOGE("(Capture) Cannot get LDC buffer.");
            return MFALSE;
        }
        else
        {
            enqueParams.ldcMain1 = (MUINT8*) pLDCImgBuf->getBufVA(0);
        }
    }
    // MY_S
    SmartImageBuffer smMY_S = mBufPoolSet.mMYSImgBufPool->request();
    toWMFImgInfo->addImageBuffer(BID_OCC_OUT_MY_S, smMY_S);
    outputParams.downScaledImg = smMY_S->mImageBuffer.get();
    // DMH
    SmartImageBuffer smDMH = mBufPoolSet.mDMHImgBufPool->request();
    toWMFImgInfo->addImageBuffer(BID_OCC_OUT_DMH, smDMH);
    outputParams.depthMap = (MUINT8*) smDMH->mImageBuffer->getBufVA(0);

    debugOCCParams({smMV_Y->mImageBuffer, smSV_Y->mImageBuffer, smDMP_L->mImageBuffer,
                    smDMP_R->mImageBuffer, pLDCImgBuf, smMY_S->mImageBuffer, smDMH->mImageBuffer});

    return MTRUE;
}

MVOID
OCCNode::
debugOCCParams(const DebugBufParam& param)
{
    if(!mbDebugLog)
        return;
    #define OUTPUT_IMG_BUFF(imageBuf)\
        if(imageBuf!=NULL)\
        {\
            MY_LOGD("=======================:" # imageBuf);\
            MY_LOGD("imageBuff size=%dx%d", imageBuf->getImgSize().w, imageBuf->getImgSize().h);\
            MY_LOGD("imageBuff plane count=%d", imageBuf->getPlaneCount());\
            MY_LOGD("imageBuff format=%x", imageBuf->getImgFormat());\
            MY_LOGD("imageBuff getImgBitsPerPixel=%d", imageBuf->getImgBitsPerPixel());\
            MY_LOGD("=======================");\
        }

    MY_LOGD("Input::");
    OUTPUT_IMG_BUFF(param.imgBuf_MV_Y);
    OUTPUT_IMG_BUFF(param.imgBuf_SV_Y);
    OUTPUT_IMG_BUFF(param.imgBuf_DMP_L);
    OUTPUT_IMG_BUFF(param.imgBuf_DMP_R);
    OUTPUT_IMG_BUFF(param.imgBuf_LDC);

    MY_LOGD("Output::");
    OUTPUT_IMG_BUFF(param.downScaledImg);
    OUTPUT_IMG_BUFF(param.depthMap);

    #undef OUTPUT_IMG_BUFF
}


}; //NSFeaturePipe
}; //NSCamFeature
}; //NSCam
