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

// Standard C header file

// Android system/core header file
#include <utils/String8.h>
#include <utils/Vector.h>
// mtkcam custom header file

// mtkcam global header file

// Module header file

// Local header file
#include "DepthMapPipe.h"
#include "DepthMapFactory.h"
#include "DepthMapEffectRequest.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"

#define PIPE_CLASS_TAG "DepthMapPipe"
#include <featurePipe/core/include/PipeLog.h>

/******************************************************************************
 *
 ******************************************************************************/

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

IDepthMapPipe*
IDepthMapPipe::createInstance(DepthMapPipeSetting setting, DepthMapPipeOption pipeOption)
{
    DepthPipeLoggingSetup::mbProfileLog = StereoSettingProvider::isProfileLogEnabled();
    DepthPipeLoggingSetup::mbDebugLog = StereoSettingProvider::isLogEnabled(PERPERTY_DEPTHMAP_NODE_LOG);
    MY_LOGD("LoggingSetup: mbProfileLog =%d mbDebugLog=%d ",
            DepthPipeLoggingSetup::mbProfileLog, DepthPipeLoggingSetup::mbDebugLog);

    return new DepthMapPipe(setting, pipeOption);
}

MBOOL
IDepthMapPipe::destroyInstance()
{
    delete this;
    return MTRUE;
}

DepthMapPipe::DepthMapPipe(DepthMapPipeSetting setting, DepthMapPipeOption pipeOption)
: CamPipe<DepthMapPipeNode>("DepthMapPipe")
, mSetting(setting), mPipeOption(pipeOption)
{
    DepthMapPipeNode::miTimestamp = time(NULL);
}

DepthMapPipe::~DepthMapPipe()
{
    MY_LOGD("[Destructor] +");
    // must call dispose to free CamGraph
    this->dispose();
    // free all nodes
    for(ssize_t idx=0;idx<mNodeMap.size();++idx)
    {
        DepthMapPipeNode *pNode = mNodeMap.valueAt(idx);
        delete pNode;
    }
    mNodeMap.clear();
    MY_LOGD("[Destructor] -");
}

MBOOL DepthMapPipe::onInit()
{
    MY_LOGD("+");
    // query FlowOption
    MBOOL bRet = queryDepthMapFlowOption(mSetting, mPipeOption, mpFlowOption);
    // query buffer pool mgr
    bRet &= queryBufferPoolMgr(mSetting, mPipeOption, mpFlowOption, mpBuffPoolMgr);

    if(!bRet)
    {
        MY_LOGE("Failed to query DepthMapFlowOption and BufferPoolMgr!!");
        return MFALSE;
    }

    DepthMapPipeNode* pPipeNode;
    PipeNodeBitSet activeNodeBit;
    //query the active pipe nodes
    mpFlowOption->queryPipeNodeBitSet(activeNodeBit);

    #define TEST_AND_CREATE_NODE(nodeIDEnum, nodeClass, nodeName)\
        if(activeNodeBit.test(nodeIDEnum)) { \
            pPipeNode = new nodeClass(nodeName, nodeIDEnum, \
                                        mpFlowOption, mSetting);\
            mNodeMap.add(nodeIDEnum, pPipeNode);}

    TEST_AND_CREATE_NODE(eDPETHMAP_PIPE_NODEID_P2AFM, P2AFMNode, "P2AFM");
    TEST_AND_CREATE_NODE(eDPETHMAP_PIPE_NODEID_N3D, N3DNode, "N3DNode");
    TEST_AND_CREATE_NODE(eDPETHMAP_PIPE_NODEID_DPE, DPENode, "DPENode");
    TEST_AND_CREATE_NODE(eDPETHMAP_PIPE_NODEID_WMF, WMFNode, "WMFNode");
    TEST_AND_CREATE_NODE(eDPETHMAP_PIPE_NODEID_OCC, OCCNode, "OCCNode");
    TEST_AND_CREATE_NODE(eDPETHMAP_PIPE_NODEID_FD, StereoFDNode, "SFDNode");

    #undef TEST_AND_CREATE_NODE

    // set Last DMP buffer pool for DPENode
    DPENode *pDPENode = reinterpret_cast<DPENode*>(mNodeMap.valueFor(eDPETHMAP_PIPE_NODEID_DPE));
    sp<ImageBufferPool> pPool = mpBuffPoolMgr->getImageBufferPool(BID_DPE_INTERNAL_LAST_DMP);
    pDPENode->setDMPBufferPool(pPool);

    // build nodes into graph
    mpFlowOption->buildPipeGraph(this, mNodeMap);
    // default node graph - Error handling
    for(ssize_t index=0;index<mNodeMap.size();++index)
    {
        this->connectData(ERROR_OCCUR_NOTIFY, ERROR_OCCUR_NOTIFY, *mNodeMap.valueAt(index), this);
    }
    MY_LOGD("-");

    return MTRUE;
}

MVOID DepthMapPipe::onUninit()
{
    MY_LOGD("+");

    //
    mpFlowOption = NULL;
    mpBuffPoolMgr = NULL;
    MY_LOGD("-");
}

MBOOL
DepthMapPipe::
init()
{
    CAM_TRACE_NAME("DepthMapPipe::init");
    MBOOL bRet = CamPipe<DepthMapPipeNode>::init();
    // Use flush on stop by default
    CamPipe<DepthMapPipeNode>::setFlushOnStop(MTRUE);
    return bRet;
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

        DepthMapRequestPtr pEffectReq = mvRequestPtrMap.valueAt(index);
        sp<EffectRequest> pReq = (EffectRequest*) pEffectReq.get();
        // callback to pipeline node with FLUSH KEY
        pEffectReq->mpOnRequestProcessed(pEffectReq->mpTag, String8( DEPTHMAP_FLUSH_KEY ), pReq);
    }
    // clear all request map
    mvRequestPtrMap.clear();

    VSDOF_PRFLOG("-");
}

MBOOL
DepthMapPipe::
enque(sp<IDepthMapEffectRequest>& request)
{
    CAM_TRACE_NAME("DepthMapPipe::enque");
    VSDOF_PRFLOG("+");

    DepthMapRequestPtr pDpeEffReq = static_cast<DepthMapEffectRequest*>(request.get());

    MUINT32 reqID = pDpeEffReq->getRequestNo();
    // autolock for request map
    {
        VSDOF_PRFLOG("request map add reqId=%d ", reqID);
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.add(reqID, pDpeEffReq);
    }
    // warning msg
    if(mvRequestPtrMap.size()>6)
        MY_LOGW("The size of queued request inside DepthMapPipe is over 6.");

    // create BufferHandler for each request
    sp<BaseBufferHandler> pBufferPoolHandler = mpBuffPoolMgr->createBufferPoolHandler();
    EffectRequestAttrs reqAttrs;
    if(!mpFlowOption->queryReqAttrs(pDpeEffReq, reqAttrs))
    {
        MY_LOGE("Failed to query request attributes!");
        return MFALSE;
    }
    pDpeEffReq->init(pBufferPoolHandler, reqAttrs);

    // call parent class:FeaturePipe enque function
    MBOOL ret = CamPipe::enque(ROOT_ENQUE, pDpeEffReq);

    VSDOF_PRFLOG("-");
    return ret;
}


MBOOL DepthMapPipe::onData(DataID id, DepthMapRequestPtr &request)
{
    VSDOF_LOGD("+ : reqID=%d DataID=%d", request->getRequestNo(), id);

    MBOOL bRet;
    switch(id)
    {
        case P2AFM_OUT_MV_F:
        case P2AFM_OUT_MV_F_CAP:
        case P2AFM_OUT_FD:
        case WMF_OUT_DMW_MY_S:
        case WMF_OUT_DEPTHMAP:
        case DEPTHMAP_META_OUT:
        case N3D_OUT_JPS_WARPING_MATRIX:
        case FD_OUT_EXTRADATA:
        case DPE_OUT_DISPARITY:
        {
            bRet = onDataSuccess(request);
            break;
        }
        case ERROR_OCCUR_NOTIFY:
        {
            bRet = onErrorNotity(request);
            break;
        }
        default:
            MY_LOGW("FeaturePipe_DepthMap: onData non-avaiilable id=%d", id);
            return MFALSE;
    }


    VSDOF_LOGD("-");
    return bRet;
}

MBOOL
DepthMapPipe::
onErrorNotity(DepthMapRequestPtr &request)
{
    // error handling
    MUINT32 reqID = request->getRequestNo();
    VSDOF_LOGD("DepthMapPipie request occur error! req_id=%d", request->getRequestNo());
    sp<EffectRequest> pReq = (EffectRequest*) request.get();
    request->mpOnRequestProcessed(request->mpTag, String8( DEPTHMAP_ERROR_KEY ), pReq);

    // remove request
    {
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.removeItem(reqID);
    }
    return MTRUE;
}

MBOOL
DepthMapPipe::
onDataSuccess(DepthMapRequestPtr &request)
{
    VSDOF_LOGD("+");
    MUINT32 reqID = request->getRequestNo();

    // autolock for request map
    {
        android::Mutex::Autolock lock(mReqMapLock);
        if(mvRequestPtrMap.indexOfKey(reqID)<0)
        {
            MY_LOGW("FeaturePipe_DepthMap: onGraphCB non-existed req_id=%d, might already return.", reqID);
            return MFALSE;
        }
    }

    // if ready callback to client
    if(request->checkAllOutputReady())
    {
        sp<EffectRequest> pReq = (EffectRequest*) request.get();
        request->mpOnRequestProcessed(request->mpTag, String8( DEPTHMAP_COMPLETE_KEY ), pReq);
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.removeItem(reqID);
    }

    VSDOF_LOGD("-");

    return MTRUE;
}

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam
