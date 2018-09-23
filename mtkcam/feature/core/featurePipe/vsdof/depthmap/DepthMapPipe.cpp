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
#include "DepthMapPipe.h"

#include <utils/String8.h>
#include <utils/Vector.h>

#define PIPE_CLASS_TAG "DepthMapPipe"
#include <featurePipe/core/include/PipeLog.h>

/******************************************************************************
 *
 ******************************************************************************/

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

IDepthMapPipe*
IDepthMapPipe::createInstance(
    DepthFlowType flowType,
    MINT32 iSensorIdx_Main1,
    MINT32 iSensorIdx_Main2
)
{
    return new DepthMapPipe(flowType, iSensorIdx_Main1, iSensorIdx_Main2);
}

MBOOL
IDepthMapPipe::destroyInstance()
{
    delete this;
    return MTRUE;
}

DepthMapPipe::DepthMapPipe(
    DepthFlowType flowType,
    MINT32 iSensorIdx_Main1,
    MINT32 iSensorIdx_Main2
)
: CamPipe<DepthMapPipeNode>("DepthMapPipe"), mFlowType(flowType)
, mP2AFMNode("P2AFM", flowType, iSensorIdx_Main1, iSensorIdx_Main2)
, mN3DNode("N3DNode", flowType), mDPENode("DPENode", flowType)
, mOCCNode("OCCNode", flowType), mWMFNode("WMFNode", flowType)
, mFDNode("SFDNode", flowType), mGFNode("GFNode", flowType)
{
    mNodes.push(&mP2AFMNode);
    mNodes.push(&mN3DNode);
    mNodes.push(&mDPENode);
    mNodes.push(&mOCCNode);
    mNodes.push(&mWMFNode);
    mNodes.push(&mFDNode);
    mNodes.push(&mGFNode);
    // logging status
    mbDebugLog = StereoSettingProvider::isLogEnabled(PERPERTY_DEPTHMAP_NODE_LOG);
    mbProfileLog = StereoSettingProvider::isProfileLogEnabled();
    #ifdef GTEST
    mbDebugLog = MTRUE;
    mbProfileLog = MTRUE;
    #endif
    // depth map storage create and init
    mpDepthStorage = new DepthInfoStorage();
    mP2AFMNode.setDepthBufStorage(mpDepthStorage);
    mWMFNode.setDepthBufStorage(mpDepthStorage);

    MY_LOGD("DepthMapPipe created, flow type=%d", flowType);
}

DepthMapPipe::~DepthMapPipe()
{
    // must call dispose to free CamGraph
    this->dispose();
}

MBOOL DepthMapPipe::onInit()
{
    // prepare node connections
    // P2AFM to N3D + P2AFM_output
    this->connectData(P2AFM_TO_N3D_FEFM_CCin, P2AFM_TO_N3D_FEFM_CCin, mP2AFMNode, mN3DNode);
    this->connectData(P2AFM_OUT_MV_F, P2AFM_OUT_MV_F, mP2AFMNode, this);
    this->connectData(P2AFM_OUT_FD, P2AFM_OUT_FD, mP2AFMNode, this);
    this->connectData(P2AFM_OUT_MV_F_CAP, P2AFM_OUT_MV_F_CAP, mP2AFMNode, this);
    // P2AFM buffer dump
    this->connectData(TO_DUMP_BUFFERS, TO_DUMP_BUFFERS, mP2AFMNode, this);
    this->connectData(TO_DUMP_RAWS, TO_DUMP_RAWS, mP2AFMNode, this);
    // P2AFM to FD
    this->connectData(P2AFM_TO_FD_IMG, P2AFM_TO_FD_IMG, mP2AFMNode, mFDNode);
    // DPE to OCC
    this->connectData(DPE_TO_OCC_MVSV_DMP_CFM, DPE_TO_OCC_MVSV_DMP_CFM, mDPENode, mOCCNode);
    // P2AFM to GF
    this->connectData(P2AFM_TO_GF_MY_SL, P2AFM_TO_GF_MY_SL, mP2AFMNode, mGFNode);
    // N3D to DPE/OCC
    this->connectData(N3D_TO_DPE_MVSV_MASK, N3D_TO_DPE_MVSV_MASK, mN3DNode, mDPENode);
    this->connectData(N3D_TO_OCC_LDC, N3D_TO_OCC_LDC, mN3DNode, mOCCNode);
    this->connectData(N3D_OUT_JPS, N3D_OUT_JPS, mN3DNode, this);
    // N3D to FD
    this->connectData(N3D_TO_FD_EXTDATA_MASK, mN3DNode, mFDNode);
    // Hal Meta frame output
    this->connectData(DEPTHMAP_META_OUT, DEPTHMAP_META_OUT, mN3DNode, this);
    //OCC to WMF + OCC_output
    this->connectData(OCC_TO_WMF_DMH_MY_S, OCC_TO_WMF_DMH_MY_S, mOCCNode, mWMFNode);
    // WMF to GF
    this->connectData(WMF_TO_GF_DMW_MY_S, WMF_TO_GF_DMW_MY_S, mWMFNode, mGFNode);
    // GF output
    this->connectData(GF_OUT_DEPTHMAP, GF_OUT_DEPTHMAP, mGFNode, this);
    this->connectData(GF_OUT_DMBG, GF_OUT_DMBG, mGFNode, this);
    //FDNode output
    this->connectData(FD_OUT_EXTRADATA, FD_OUT_EXTRADATA, mFDNode, this);
    this->setRootNode(&mP2AFMNode);
    // Error handling
    for(ssize_t index=0;index<mNodes.size();++index)
    {
        this->connectData(ERROR_OCCUR_NOTIFY, ERROR_OCCUR_NOTIFY, *mNodes[index], this);
    }
    // Queued flow type -> connect P2AFM node to GF node
    if(mFlowType == eDEPTH_FLOW_TYPE_QUEUED_DEPTH)
    {
        this->connectData(P2AFM_TO_GF_DMW_MYS, P2AFM_TO_GF_DMW_MYS, mP2AFMNode, mGFNode);
        this->connectData(QUEUED_FLOW_DONE, QUEUED_FLOW_DONE, mWMFNode, this);
    }

    return MTRUE;
}

MVOID DepthMapPipe::onUninit()
{
}

MBOOL
DepthMapPipe::
init()
{
    CAM_TRACE_NAME("DepthMapPipe::init");
    return CamPipe<DepthMapPipeNode>::init();
}

MBOOL
DepthMapPipe::
uninit()
{
    CAM_TRACE_NAME("DepthMapPipe::uninit");
    return CamPipe<DepthMapPipeNode>::uninit();
}

MVOID
DepthMapPipe::
flush()
{
    CAM_TRACE_NAME("DepthMapPipe::flush");
    VSDOF_PRFLOG("+");
    // lauch the default flush operations
    CamPipe::flush();

    // relase all the effectRequest
    android::Mutex::Autolock lock(mReqMapLock);
    for(size_t index=0;index<mvRequestPtrMap.size();++index)
    {
        MUINT32 iFlushReqID = mvRequestPtrMap.keyAt(index);
        VSDOF_LOGD("flush ReqID = %d", iFlushReqID);

        EffectRequestPtr pEffectReq = mvRequestPtrMap.valueAt(index);
        // callback to pipeline node with FLUSH KEY
        pEffectReq->mpOnRequestProcessed(pEffectReq->mpTag, String8( DEPTHMAP_FLUSH_KEY ), pEffectReq);
    }
    // clear all request map
    mvRequestPtrMap.clear();

    VSDOF_PRFLOG("-");
}

MBOOL
DepthMapPipe::
enque(EffectRequestPtr& request)
{
    CAM_TRACE_NAME("DepthMapPipe::enque");
    VSDOF_PRFLOG("+");
    MUINT32 reqID = request->getRequestNo();
    // autolock for request map
    {
        VSDOF_PRFLOG("request map add reqId=%d ", reqID);
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.add(reqID, request);
    }
    if(mvRequestPtrMap.size()>6)
        MY_LOGW("The size of queued request inside DepthMapPipe is over 6.");

    MBOOL ret = handleFlowTypeOnEnque(request);
    // call parent class:FeaturePipe enque function
    ret = ret && CamPipe::enque(ROOT_ENQUE, request);

    VSDOF_PRFLOG("-");
    return ret;
}

MBOOL
DepthMapPipe::
handleFlowTypeOnEnque(EffectRequestPtr request)
{
    DepthNodeOpState eState = getRequestState(request);
    // if queued depth request
    if(isQueueDepthFlow(eState, mFlowType))
    {
        MINT32 iReqID= request->getRequestNo();
        // create new metadata of the metadata with queued buffer
        for(auto metaBID : INPUT_METADATA_BID_LIST)
        {
            sp<EffectFrameInfo> pFrame = request->vInputFrameInfo.valueFor(metaBID);
            IMetadata* pMeta = getMetadataFromFrameInfoPtr(pFrame);
            // create new meta
            IMetadata* pCopyMeta = new IMetadata(*pMeta);
            DepthMapBufferID queuedMetaBID = mapQueuedBID(metaBID);
            pFrame = new EffectFrameInfo(iReqID, queuedMetaBID);
            sp<EffectParameter> pEffParam = new EffectParameter();
            pEffParam->setPtr(VSDOF_PARAMS_METADATA_KEY, (void*)pCopyMeta);
            pFrame->setFrameParameter(pEffParam);
            request->vInputFrameInfo.add(queuedMetaBID, pFrame);
        }
        // output
        for(auto metaBID : OUTPUT_METADATA_BID_LIST)
        {
            sp<EffectFrameInfo> pFrame = request->vOutputFrameInfo.valueFor(metaBID);
            IMetadata* pMeta = getMetadataFromFrameInfoPtr(pFrame);
            // create new meta
            IMetadata* pNewMeta = new IMetadata(*pMeta);
            DepthMapBufferID queuedMetaBID = mapQueuedBID(metaBID);
            pFrame = new EffectFrameInfo(iReqID, queuedMetaBID);
            sp<EffectParameter> pEffParam = new EffectParameter();
            pEffParam->setPtr(VSDOF_PARAMS_METADATA_KEY, (void*)pNewMeta);
            pFrame->setFrameParameter(pEffParam);
            request->vOutputFrameInfo.add(queuedMetaBID, pFrame);
        }
    }
    return MTRUE;
}


MBOOL DepthMapPipe::onData(DataID id, EffectRequestPtr &request)
{
    VSDOF_LOGD("+ : reqID=%d DataID=%d", request->getRequestNo(), id);

    switch(id)
    {
        case QUEUED_FLOW_DONE:
            onQueuedDepthFlowDone(request);
            return MTRUE;
        case ERROR_OCCUR_NOTIFY:
            onErrorNotify(request);
            break;
        default:
            MY_LOGW("FeaturePipe_DepthMap: onData non-avaiilable id=%d", id);
            return MFALSE;
    }
    VSDOF_LOGD("-");
    return MTRUE;
}

MBOOL
DepthMapPipe::
onQueuedDepthFlowDone(EffectRequestPtr pRequest)
{
    // release the queued metadata instance(input)
    for(auto metaBID : INPUT_METADATA_BID_LIST)
    {
        DepthMapBufferID queuedMetaBID = mapQueuedBID(metaBID);
        sp<EffectFrameInfo> pFrame = pRequest->vInputFrameInfo.valueFor(queuedMetaBID);
        IMetadata* pMeta = getMetadataFromFrameInfoPtr(pFrame);
        delete pMeta;
    }
    // release the queued metadata instance(output)
    for(auto metaBID : OUTPUT_METADATA_BID_LIST)
    {
        DepthMapBufferID queuedMetaBID = mapQueuedBID(metaBID);
        sp<EffectFrameInfo> pFrame = pRequest->vOutputFrameInfo.valueFor(queuedMetaBID);
        IMetadata* pMeta = getMetadataFromFrameInfoPtr(pFrame);
        delete pMeta;
    }

    return MTRUE;
}

MBOOL
DepthMapPipe::
onErrorNotify(EffectRequestPtr pRequest)
{
    // error handling
    MUINT32 reqID = pRequest->getRequestNo();
    VSDOF_LOGD("DepthMapPipie request occur error! req_id=%d", pRequest->getRequestNo());
    pRequest->mpOnRequestProcessed(pRequest->mpTag, String8( DEPTHMAP_ERROR_KEY ), pRequest);
    // remove request
    {
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.removeItem(reqID);
    }
    return MTRUE;
}

MBOOL
DepthMapPipe::
onData(DataID id, FrameInfoPtr &data)
{
    VSDOF_LOGD("+ :DepthMapPipe onData: reqID=%d DataID=%d", data->getRequestNo(), id);

    MUINT32 reqID;
    switch(id)
    {
        case P2AFM_OUT_MV_F:
        case P2AFM_OUT_MV_F_CAP:
        case P2AFM_OUT_FD:
        case WMF_TO_GF_DMW_MY_S:
        case DEPTHMAP_META_OUT:
        case N3D_OUT_JPS:
        case FD_OUT_EXTRADATA:
        case GF_OUT_DMBG:
        case GF_OUT_DEPTHMAP:
            reqID = data->getRequestNo();
            break;
        default:
            MY_LOGW("FeaturePipe_DepthMap: onData non-avaiilable id=%d", id);
            return MFALSE;
    }
    sp<EffectRequest> request;
    // autolock for request map
    {
        android::Mutex::Autolock lock(mReqMapLock);
        if(mvRequestPtrMap.indexOfKey(reqID)<0)
        {
            MY_LOGW("FeaturePipe_DepthMap: onGraphCB non-existed req_id=%d, might already return.", reqID);
            return MFALSE;
        }
        request = mvRequestPtrMap.valueFor(reqID);
        size_t ouputBufSize = request->vOutputFrameInfo.size();
        MBOOL bReady=MTRUE;
        // make sure all output frame are ready
        for(size_t index=0;index<ouputBufSize;index++)
        {
            if(!request->vOutputFrameInfo[index]->isFrameBufferReady())
            {
                VSDOF_LOGD("req_id=%d Data not ready!! index=%d key=%d", reqID, index, request->vOutputFrameInfo.keyAt(index));
                bReady = MFALSE;
                break;
            }
        }
        // if ready callback to client
        if(bReady)
        {
            VSDOF_PRFLOG("Request data ready! req_id=%d", request->getRequestNo());
            request->mpOnRequestProcessed(request->mpTag, String8( DEPTHMAP_COMPLETE_KEY ), request);
            mvRequestPtrMap.removeItem(reqID);
        }
    }

    VSDOF_LOGD("-");

    return MTRUE;
}


MBOOL DepthMapPipe::onData(DataID id, SmartImageBuffer &data)
{
  return MTRUE;
}

}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam
