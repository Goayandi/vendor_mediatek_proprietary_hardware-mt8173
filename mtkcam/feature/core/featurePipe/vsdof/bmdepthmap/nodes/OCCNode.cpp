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

// Local header file
#include "OCCNode.h"
#include "../DepthMapPipe_Common.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"

// Logging header file
#define PIPE_CLASS_TAG "OCCNode"
#include <featurePipe/core/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

OCCNode::
OCCNode(
   const char *name,
    DepthMapPipeNodeID nodeID,
    sp<DepthMapFlowOption> pFlowOpt,
    const DepthMapPipeSetting& setting
)
: DepthMapPipeNode(name, nodeID, pFlowOpt, setting)
{
    this->addWaitQueue(&mJobQueue);
    this->addWaitQueue(&mLDCReqIDQueue);
}

OCCNode::
~OCCNode()
{
    MY_LOGD("[Destructor]");
}

MVOID
OCCNode::
cleanUp()
{
    VSDOF_LOGD("+");
    if(mpOCCHAL!=NULL)
    {
        delete mpOCCHAL;
        mpOCCHAL = NULL;
    }
    mJobQueue.clear();
    VSDOF_LOGD("-");
}

MBOOL
OCCNode::
onInit()
{
    VSDOF_LOGD("+");
    CAM_TRACE_NAME("OCCNode::onInit");
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
onData(DataID data, DepthMapRequestPtr& pRequest)
{
    MBOOL bRet = MTRUE;
    VSDOF_LOGD("+, DataID=%d reqId=%d", data, pRequest->getRequestNo());

    switch(data)
    {
        case N3D_TO_OCC_LDC:
        {
            mLDCReqIDQueue.enque(pRequest->getRequestNo());
            break;
        }
        case DPE_TO_OCC_MVSV_DMP_CFM:
            mJobQueue.enque(pRequest);
            break;
        default:
            MY_LOGW("Unrecongnized DataID=%d", data);
            bRet = MFALSE;
            break;
    }

    TRACE_FUNC_EXIT();
    return bRet;
}

MBOOL
OCCNode::
onThreadLoop()
{
    MUINT32 iLDCReadyReqID;
    DepthMapRequestPtr pRequest;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }
    // get request
    if( !mJobQueue.deque(pRequest) )
    {
        MY_LOGE("mJobQueue.deque() failed");
        return MFALSE;
    }
    // get LDC request id
    if( !mLDCReqIDQueue.deque(iLDCReadyReqID) )
    {
        MY_LOGE("mLDCReqIDQueue.deque() failed");
        return MFALSE;
    }

    MUINT32 iReqNo = pRequest->getRequestNo();

    if(iReqNo != iLDCReadyReqID)
    {
        MY_LOGE("The deque request is not the LDC-ready one. Please check!iReqNo=%d  iLDCReadyReqID=%d", iReqNo, iLDCReadyReqID);
        return MFALSE;
    }

    CAM_TRACE_NAME("OCCNode::onThreadLoop");
    // mark on-going-request start
    this->incExtThreadDependency();
    VSDOF_PRFLOG("OCC threadloop start, reqID=%d", iReqNo);

    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    OCC_HAL_PARAMS occInputParams;
    OCC_HAL_OUTPUT occOutputParams;
    // prepare in/out params
    MBOOL bRet = prepareOCCParams(pRequest, occInputParams, occOutputParams);
    if(!bRet)
    {
        MY_LOGE("OCC ALGO stopped because of the enque parameter error.");
        // mark on-going-request end
        this->decExtThreadDependency();
        return MFALSE;
    }

    VSDOF_PRFLOG("OCC ALGO start, reqID=%d", pRequest->getRequestNo());
    // timer
    pRequest->mTimer.startOCC();
    CAM_TRACE_BEGIN("OCCNode::OCCHALRun");
    bRet = mpOCCHAL->OCCHALRun(occInputParams, occOutputParams);
    CAM_TRACE_END();
    // timer
    pRequest->mTimer.stopOCC();
    VSDOF_PRFLOG("OCC ALGO end, reqID=%d, exec-time=%d msec",
                pRequest->getRequestNo(), pRequest->mTimer.getElapsedOCC());

    if(bRet)
    {
        // config output to WMF node
        pBufferHandler->configOutBuffer(getNodeId(), BID_OCC_OUT_MY_S, eDPETHMAP_PIPE_NODEID_WMF);
        pBufferHandler->configOutBuffer(getNodeId(), BID_OCC_OUT_DMH, eDPETHMAP_PIPE_NODEID_WMF);
        // pass data
        handleDataAndDump(OCC_TO_WMF_DMH_MY_S, pRequest);
    }
    else
    {
        MY_LOGE("OCC ALGO failed: reqID=%d", pRequest->getRequestNo());
        handleData(ERROR_OCCUR_NOTIFY, pRequest);
    }
    // launch onProcessDone
    pBufferHandler->onProcessDone(getNodeId());
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
    DepthMapRequestPtr& pRequest,
    OCC_HAL_PARAMS& rOCCParams,
    OCC_HAL_OUTPUT& rOutParams
)
{
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();

    MBOOL bRet = MTRUE;
    IImageBuffer *pImgBuf_MV_Y, *pImgBuf_SV_Y, *pImgBuf_DMP_L,  *pImgBuf_DMP_R;
    IImageBuffer *pImgBuf_LDC;
    // input buffers
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_MV_Y, pImgBuf_MV_Y);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_SV_Y, pImgBuf_SV_Y);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_DMP_L, pImgBuf_DMP_L);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_DMP_R, pImgBuf_DMP_R);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_LDC, pImgBuf_LDC);

    if(!bRet)
    {
        MY_LOGE("Cannot get enque buffers!");
        return MFALSE;
    }
    // insert input buffer
    rOCCParams.imageMain1 = pImgBuf_MV_Y;
    rOCCParams.imageMain2 = pImgBuf_SV_Y;
    rOCCParams.disparityLeftToRight = (MUINT16*) pImgBuf_DMP_L->getBufVA(0);
    rOCCParams.disparityRightToLEft = (MUINT16*) pImgBuf_DMP_R->getBufVA(0);
    rOCCParams.confidenceMap = NULL;
    rOCCParams.requestNumber = pRequest->getRequestNo();
    rOCCParams.ldcMain1 = (MUINT8*)pImgBuf_LDC->getBufVA(0);

    // get output MY_S, DMH
    IImageBuffer *pOutBuf_MY_S = pBufferHandler->requestBuffer(getNodeId(), BID_OCC_OUT_MY_S);
    IImageBuffer *pOutBuf_DMH = pBufferHandler->requestBuffer(getNodeId(), BID_OCC_OUT_DMH);
    // MY_S
    rOutParams.downScaledImg = pOutBuf_MY_S;
    // DMH
    rOutParams.depthMap = (MUINT8*) pOutBuf_DMH->getBufVA(0);

    debugOCCParams({pImgBuf_MV_Y, pImgBuf_SV_Y,
                    pImgBuf_DMP_L, pImgBuf_DMP_R,pOutBuf_MY_S, pOutBuf_DMH});

    return MTRUE;
}

MVOID
OCCNode::
debugOCCParams(DebugBufParam param)
{
    if(!DepthPipeLoggingSetup::mbDebugLog)
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
        }\
        else\
            MY_LOGD("=======================:" # imageBuf " is NULL!!!!");\


    MY_LOGD("Input::");
    OUTPUT_IMG_BUFF(param.imgBuf_MV_Y);
    OUTPUT_IMG_BUFF(param.imgBuf_SV_Y);
    OUTPUT_IMG_BUFF(param.imgBuf_DMP_L);
    OUTPUT_IMG_BUFF(param.imgBuf_DMP_R);

    MY_LOGD("Output::");
    OUTPUT_IMG_BUFF(param.downScaledImg);
    OUTPUT_IMG_BUFF(param.depthMap);

    #undef OUTPUT_IMG_BUFF
}


}; //NSFeaturePipe_DepthMap_BM
}; //NSCamFeature
}; //NSCam
