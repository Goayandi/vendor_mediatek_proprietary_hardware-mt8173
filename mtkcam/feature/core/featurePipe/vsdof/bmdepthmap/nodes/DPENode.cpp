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

// mtkcam custom header file
#include <camera_custom_stereo.h>
// mtkcam global header file
#include <mtkcam/drv/iopipe/PostProc/DpeUtility.h>
// Module header file
#include <vsdof/hal/stereo_tuning_provider.h>
// Local header file
#include "DPENode.h"
#include "../DepthMapPipe_Common.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"
// Logging header
#define PIPE_CLASS_TAG "DPENode"
#include <featurePipe/core/include/PipeLog.h>


namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using namespace NSCam::NSIoPipe::NSDpe;

DPENode::
DPENode(
    const char *name,
    DepthMapPipeNodeID nodeID,
    sp<DepthMapFlowOption> pFlowOpt,
    const DepthMapPipeSetting& setting
)
: DepthMapPipeNode(name, nodeID, pFlowOpt, setting), mbFirst(MTRUE)
{
    this->addWaitQueue(&mJobQueue);
}

DPENode::
~DPENode()
{
    MY_LOGD("[Destructor]");
}

MVOID
DPENode::
setDMPBufferPool(sp<ImageBufferPool> pDMPBufPool)
{
    MY_LOGD("+");
    mpDMPBufferPool = pDMPBufPool;
    MY_LOGD("-");
}

MVOID
DPENode::
cleanUp()
{
    VSDOF_LOGD("+");
    //
    if(mpDPEStream != NULL)
    {
        mpDPEStream->uninit();
        mpDPEStream = NULL;
    }
    mJobQueue.clear();
    mbFirst = MTRUE;
    mLastDMP_L = NULL;
    mLastDMP_R = NULL;
    //
    VSDOF_LOGD("-");
}

MBOOL
DPENode::
onInit()
{
    VSDOF_LOGD("+");
    CAM_TRACE_NAME("DPENode::onInit");

    // init DPEStream
    mpDPEStream = NSCam::NSIoPipe::NSDpe::IDpeStream::createInstance("VSDOF_DPE");
    if(mpDPEStream == NULL)
    {
        MY_LOGE("DPE Stream create instance failed!");
        return MFALSE;
    }
    mpDPEStream->init();
    // init the last DMP
    mLastDMP_L = mpDMPBufferPool->request();
    mLastDMP_R = mpDMPBufferPool->request();

    VSDOF_LOGD("-");
    return MTRUE;
}

MBOOL
DPENode::
onUninit()
{
    VSDOF_LOGD("+");
    CAM_TRACE_NAME("DPENode::onUninit");

    cleanUp();
    VSDOF_LOGD("-");
    return MTRUE;
}

MBOOL
DPENode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
DPENode::
onThreadStop()
{
    return MTRUE;
}


MBOOL
DPENode::
onData(DataID data, DepthMapRequestPtr& pRequest)
{
    MBOOL ret = MTRUE;
    VSDOF_LOGD("+ : reqId=%d", pRequest->getRequestNo());

    switch(data)
    {
        case N3D_TO_DPE_MVSV_MASK:
            mJobQueue.enque(pRequest);
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
DPENode::
onThreadLoop()
{
    DepthMapRequestPtr pRequest;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }

    if( !mJobQueue.deque(pRequest) )
    {
        MY_LOGE("mJobQueue.deque() failed");
        return MFALSE;
    }
    // mark on-going-request start
    this->incExtThreadDependency();

    MUINT32 iReqIdx = pRequest->getRequestNo();

    VSDOF_PRFLOG("threadLoop start, reqID=%d", iReqIdx);
    CAM_TRACE_NAME("DPENode::onThreadLoop");

    DVEParams enqueParams;
    DVEConfig enqueConfig;

    // prepare the enque configs
    if(!prepareDPEEnqueConfig(pRequest, enqueConfig))
    {
        MY_LOGE("Failed to prepare DPE enque paramters!");
        return MFALSE;
    }
    //
    debugDPEConfig(enqueConfig);

    // enque cookie instance
    EnqueCookieContainer *pCookieIns = new EnqueCookieContainer(pRequest, this);

    enqueParams.mDVEConfigVec.push_back(enqueConfig);
    enqueParams.mpfnCallback = onDPEEnqueDone;
    enqueParams.mpCookie = (void*) pCookieIns;
    // timer
    pRequest->mTimer.startDPE();
    pRequest->mTimer.startDPEEnque();
    CAM_TRACE_BEGIN("DPENODE::DPEStream::enque");
    VSDOF_PRFLOG("DVE Enque start, reqID=%d", iReqIdx);
    MBOOL bRet = mpDPEStream->DVEenque(enqueParams);

    pRequest->mTimer.stopDPEEnque();
    VSDOF_PRFLOG("DVE Enque, reqID=%d, config time=%d ms", iReqIdx, pRequest->mTimer.getElapsedDPEEnque());

    CAM_TRACE_END();

    if(!bRet)
    {
        MY_LOGE("DPE enque failed!!");
        goto lbExit;
    }

    VSDOF_PRFLOG("threadLoop end! reqID=%d", iReqIdx);

    return MTRUE;

lbExit:
    delete pCookieIns;
    // mark on-going-request end
    this->decExtThreadDependency();
    return MFALSE;
}

MVOID
DPENode::
debugDPEConfig(DVEConfig& config)
{
    if(!DepthPipeLoggingSetup::mbDebugLog)
        return;

    MY_LOGD("DVEConfig.Dve_Skp_Pre_Dv=%d", config.Dve_Skp_Pre_Dv);
    MY_LOGD("DVEConfig.Dve_Mask_En=%d", config.Dve_Mask_En);
    MY_LOGD("DVEConfig.Dve_l_Bbox_En=%d", config.Dve_l_Bbox_En);
    MY_LOGD("DVEConfig.Dve_r_Bbox_En=%d", config.Dve_r_Bbox_En);
    MY_LOGD("DVEConfig.Dve_Horz_Ds_Mode=%d", config.Dve_Horz_Ds_Mode);
    MY_LOGD("DVEConfig.Dve_Vert_Ds_Mode=%d", config.Dve_Vert_Ds_Mode);
    MY_LOGD("DVEConfig.Dve_Imgi_l_Fmt=%d", config.Dve_Imgi_l_Fmt);
    MY_LOGD("DVEConfig.Dve_Imgi_r_Fmt=%d", config.Dve_Imgi_r_Fmt);
    MY_LOGD("DVEConfig.Dve_Org_l_Bbox (left, right, top, down)=(%d, %d, %d, %d)",
                config.Dve_Org_l_Bbox.DVE_ORG_BBOX_LEFT, config.Dve_Org_l_Bbox.DVE_ORG_BBOX_RIGHT,
                config.Dve_Org_l_Bbox.DVE_ORG_BBOX_TOP, config.Dve_Org_l_Bbox.DVE_ORG_BBOX_BOTTOM);

    MY_LOGD("DVEConfig.Dve_Org_r_Bbox (left, right, top, down)=(%d, %d, %d, %d)",
                config.Dve_Org_r_Bbox.DVE_ORG_BBOX_LEFT, config.Dve_Org_r_Bbox.DVE_ORG_BBOX_RIGHT,
                config.Dve_Org_r_Bbox.DVE_ORG_BBOX_TOP, config.Dve_Org_r_Bbox.DVE_ORG_BBOX_BOTTOM);


    MY_LOGD("DVEConfig.Dve_Org_Width=%d", config.Dve_Org_Width);
    MY_LOGD("DVEConfig.Dve_Org_Height=%d", config.Dve_Org_Height);

    MY_LOGD("DVEConfig.Dve_Org_Horz_Sr_0=%d", config.Dve_Org_Horz_Sr_0);
    MY_LOGD("DVEConfig.Dve_Org_Horz_Sr_1=%d", config.Dve_Org_Horz_Sr_1);
    MY_LOGD("DVEConfig.Dve_Org_Vert_Sr_0=%d", config.Dve_Org_Vert_Sr_0);
    MY_LOGD("DVEConfig.Dve_Org_Start_Vert_Sv=%d", config.Dve_Org_Start_Vert_Sv);
    MY_LOGD("DVEConfig.Dve_Org_Start_Horz_Sv=%d", config.Dve_Org_Start_Horz_Sv);
    MY_LOGD("DVEConfig.Dve_Cand_Num=%d", config.Dve_Cand_Num);

    #define LOG_CANDIDATE(cand) \
        MY_LOGD("DVEConfig." #cand ".DVE_CAND_SEL = %d", config.cand.DVE_CAND_SEL); \
        MY_LOGD("DVEConfig." #cand ".DVE_CAND_TYPE = %d", config.cand.DVE_CAND_TYPE);

    LOG_CANDIDATE(Dve_Cand_0);
    LOG_CANDIDATE(Dve_Cand_1);
    LOG_CANDIDATE(Dve_Cand_2);
    LOG_CANDIDATE(Dve_Cand_3);
    LOG_CANDIDATE(Dve_Cand_4);
    LOG_CANDIDATE(Dve_Cand_5);
    LOG_CANDIDATE(Dve_Cand_6);
    LOG_CANDIDATE(Dve_Cand_7);

    MY_LOGD("DVEConfig.Dve_Rand_Lut_0=%d", config.Dve_Rand_Lut_0);
    MY_LOGD("DVEConfig.Dve_Rand_Lut_1=%d", config.Dve_Rand_Lut_1);
    MY_LOGD("DVEConfig.Dve_Rand_Lut_2=%d", config.Dve_Rand_Lut_2);
    MY_LOGD("DVEConfig.Dve_Rand_Lut_3=%d", config.Dve_Rand_Lut_3);
    MY_LOGD("DVEConfig.DVE_VERT_GMV=%d", config.DVE_VERT_GMV);
    MY_LOGD("DVEConfig.DVE_HORZ_GMV=%d", config.DVE_HORZ_GMV);
    MY_LOGD("DVEConfig.Dve_Horz_Dv_Ini=%d", config.Dve_Horz_Dv_Ini);
    MY_LOGD("DVEConfig.Dve_Coft_Shift=%d", config.Dve_Coft_Shift);
    MY_LOGD("DVEConfig.Dve_Corner_Th=%d", config.Dve_Corner_Th);
    MY_LOGD("DVEConfig.Dve_Smth_Luma_Th_1=%d", config.Dve_Smth_Luma_Th_1);
    MY_LOGD("DVEConfig.Dve_Smth_Luma_Th_0=%d", config.Dve_Smth_Luma_Th_0);
    MY_LOGD("DVEConfig.Dve_Smth_Luma_Ada_Base=%d", config.Dve_Smth_Luma_Ada_Base);
    MY_LOGD("DVEConfig.Dve_Smth_Luma_Horz_Pnlty_Sel=%d", config.Dve_Smth_Luma_Horz_Pnlty_Sel);
    MY_LOGD("DVEConfig.Dve_Smth_Dv_Mode=%d", config.Dve_Smth_Dv_Mode);
    MY_LOGD("DVEConfig.Dve_Smth_Dv_Th_1=%d", config.Dve_Smth_Dv_Th_1);
    MY_LOGD("DVEConfig.Dve_Smth_Dv_Th_0=%d", config.Dve_Smth_Dv_Th_0);
    MY_LOGD("DVEConfig.Dve_Smth_Dv_Ada_Base=%d", config.Dve_Smth_Dv_Ada_Base);
    MY_LOGD("DVEConfig.Dve_Smth_Dv_Vert_Pnlty_Sel=%d", config.Dve_Smth_Dv_Vert_Pnlty_Sel);
    MY_LOGD("DVEConfig.Dve_Smth_Dv_Horz_Pnlty_Sel=%d", config.Dve_Smth_Dv_Horz_Pnlty_Sel);
    MY_LOGD("DVEConfig.Dve_Ord_Pnlty_Sel=%d", config.Dve_Ord_Pnlty_Sel);
    MY_LOGD("DVEConfig.Dve_Ord_Coring=%d", config.Dve_Ord_Coring);
    MY_LOGD("DVEConfig.Dve_Ord_Th=%d", config.Dve_Ord_Th);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_REFINE_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_REFINE_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_GMV_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_GMV_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_PREV_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_PREV_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_NBR_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_NBR_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_RAND_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_RAND_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_TMPR_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_TMPR_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_SPTL_PNLTY_SEL=%d",
                config.Dve_Type_Penality_Ctrl.DVE_SPTL_PNLTY_SEL);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_RAND_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_RAND_COST);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_GMV_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_GMV_COST);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_PREV_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_PREV_COST);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_NBR_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_NBR_COST);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_REFINE_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_REFINE_COST);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_TMPR_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_TMPR_COST);
    MY_LOGD("DVEConfig.Dve_Type_Penality_Ctrl.DVE_SPTL_COST=%d",
                config.Dve_Type_Penality_Ctrl.DVE_SPTL_COST);


    #define LOG_DPEBuf(bufInfo) \
        MY_LOGD("DVEConfig." #bufInfo ".dmaport=%d", config.bufInfo.dmaport);\
        MY_LOGD("DVEConfig." #bufInfo ".memID=%d", config.bufInfo.memID); \
        MY_LOGD("DVEConfig." #bufInfo ".u4BufVA=%x", config.bufInfo.u4BufVA); \
        MY_LOGD("DVEConfig." #bufInfo ".u4BufPA=%x", config.bufInfo.u4BufPA); \
        MY_LOGD("DVEConfig." #bufInfo ".u4BufSize=%d", config.bufInfo.u4BufSize); \
        MY_LOGD("DVEConfig." #bufInfo ".u4Stride=%d", config.bufInfo.u4Stride);

    LOG_DPEBuf(Dve_Imgi_l);
    LOG_DPEBuf(Dve_Imgi_r);
    LOG_DPEBuf(Dve_Dvi_l);
    LOG_DPEBuf(Dve_Dvi_r);
    LOG_DPEBuf(Dve_Maski_l);
    LOG_DPEBuf(Dve_Maski_r);
    LOG_DPEBuf(Dve_Dvo_l);
    LOG_DPEBuf(Dve_Dvo_r);
    LOG_DPEBuf(Dve_Confo_l);
    LOG_DPEBuf(Dve_Confo_r);
    LOG_DPEBuf(Dve_Respo_l);
    LOG_DPEBuf(Dve_Respo_r);

    MY_LOGD("DVEConfig.Dve_Vert_Sv=%d", config.Dve_Vert_Sv);
    MY_LOGD("DVEConfig.Dve_Horz_Sv=%d", config.Dve_Horz_Sv);
}

MVOID
DPENode::
onDPEEnqueDone(DVEParams& rParams)
{
    EnqueCookieContainer* pEnqueCookie = reinterpret_cast<EnqueCookieContainer*>(rParams.mpCookie);
    DPENode* pDPENode = reinterpret_cast<DPENode*>(pEnqueCookie->mpNode);
    pDPENode->handleDPEEnqueDone(rParams, pEnqueCookie);
}

MVOID
DPENode::
handleDPEEnqueDone(DVEParams& rParams, EnqueCookieContainer* pEnqueCookie)
{
    DepthMapRequestPtr pRequest = pEnqueCookie->mRequest;
    //
    pRequest->mTimer.stopDPE();
    CAM_TRACE_NAME("DPENode::handleDPEEnqueDone");
    VSDOF_PRFLOG("+, reqID=%d, DPE exec-time=%d msec",
            pRequest->getRequestNo(), pRequest->mTimer.getElapsedDPE());

    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    IImageBuffer *pImgBuf_DMP_L, *pImgBuf_DMP_R, *pImgBuf_CFM_L, *pImgBuf_CFM_R;
    IImageBuffer *pImgBuf_RESPO_L, *pImgBuf_RESPO_R;

    MBOOL bRet = MTRUE;
    // get the output buffers and invalid
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_DMP_L, pImgBuf_DMP_L);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_DMP_R, pImgBuf_DMP_R);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_CFM_L, pImgBuf_CFM_L);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_CFM_R, pImgBuf_CFM_R);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_RESPO_L, pImgBuf_RESPO_L);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_DPE_OUT_RESPO_R, pImgBuf_RESPO_R);

    if(!bRet)
    {
        MY_LOGE("Failed to get DPE output buffers");
    }
    // invalidate
    pImgBuf_DMP_L->syncCache(eCACHECTRL_INVALID);
    pImgBuf_DMP_R->syncCache(eCACHECTRL_INVALID);
    pImgBuf_CFM_L->syncCache(eCACHECTRL_INVALID);
    pImgBuf_CFM_R->syncCache(eCACHECTRL_INVALID);
    pImgBuf_RESPO_L->syncCache(eCACHECTRL_INVALID);
    pImgBuf_RESPO_R->syncCache(eCACHECTRL_INVALID);

    // update last DMP, record SmartImageBuffer to avoid release
    pBufferHandler->getEnquedSmartBuffer(getNodeId(), BID_DPE_OUT_DMP_L, mLastDMP_L);
    pBufferHandler->getEnquedSmartBuffer(getNodeId(), BID_DPE_OUT_DMP_R, mLastDMP_R);

    // prepare input for OCC
    pBufferHandler->configOutBuffer(getNodeId(), BID_DPE_OUT_DMP_L, eDPETHMAP_PIPE_NODEID_OCC);
    pBufferHandler->configOutBuffer(getNodeId(), BID_DPE_OUT_DMP_R, eDPETHMAP_PIPE_NODEID_OCC);
    // set ready for DMP output
    pRequest->setOutputBufferReady(BID_DPE_OUT_DMP_L);
    pRequest->setOutputBufferReady(BID_DPE_OUT_DMP_R);
    handleDataAndDump(DPE_OUT_DISPARITY, pRequest);
    // pass MV/SV to OCC
    pBufferHandler->configOutBuffer(getNodeId(), BID_N3D_OUT_MV_Y, eDPETHMAP_PIPE_NODEID_OCC);
    pBufferHandler->configOutBuffer(getNodeId(), BID_N3D_OUT_SV_Y, eDPETHMAP_PIPE_NODEID_OCC);
    // post to OCC node
    handleDataAndDump(DPE_TO_OCC_MVSV_DMP_CFM, pRequest);
    delete pEnqueCookie;
    // launch onProcessDone
    pBufferHandler->onProcessDone(getNodeId());

    VSDOF_PRFLOG("-, reqID=%d", pRequest->getRequestNo());
    // mark on-going-request end
    this->decExtThreadDependency();
}

MBOOL
DPENode::
setupDPEBufInfo(
    DPEDMAPort dmaPort,
    IImageBuffer* pImgBuf,
    DPEBufInfo& rBufInfo
)
{
    // plane 0 address
    rBufInfo.memID = pImgBuf->getFD(0);
    rBufInfo.dmaport = dmaPort;
    rBufInfo.u4BufVA = pImgBuf->getBufVA(0);
    rBufInfo.u4BufPA = pImgBuf->getBufPA(0);
    rBufInfo.u4BufSize = pImgBuf->getBufSizeInBytes(0);
    rBufInfo.u4Stride = pImgBuf->getBufStridesInBytes(0);
    return MTRUE;
}


MBOOL
DPENode::
prepareDPEEnqueConfig(
    DepthMapRequestPtr pRequest,
    DVEConfig& rDPEConfig
)
{
    VSDOF_LOGD("+");
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();

    // read the tuning data from tuning provider
    StereoTuningProvider::getDPETuningInfo(&rDPEConfig);
    // insert the dynamic data
    IImageBuffer *pImgBuf_MV_Y, *pImgBuf_SV_Y;
    IImageBuffer *pImgBuf_MASK_M , *pImgBuf_MASK_S;

    MBOOL bRet = MTRUE;
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_MV_Y, pImgBuf_MV_Y);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_SV_Y, pImgBuf_SV_Y);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_MASK_M, pImgBuf_MASK_M);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), BID_N3D_OUT_MASK_S, pImgBuf_MASK_S);

    if(!bRet)
        return MFALSE;

    // imgi format: YV12 -> DPE_IMGI_Y_FMT
    if(pImgBuf_MV_Y->getImgFormat() == eImgFmt_YV12)
    {
        rDPEConfig.Dve_Imgi_l_Fmt = DPE_IMGI_Y_FMT;
        rDPEConfig.Dve_Imgi_r_Fmt = DPE_IMGI_Y_FMT;
    }
    else
    {
        MY_LOGE("Not supported IMGI format!");
        return MFALSE;
    }
    // mask flag: use the tuning provider firstly.
    rDPEConfig.Dve_Mask_En &= true;

    // flush input image buffers
    pImgBuf_SV_Y->syncCache(eCACHECTRL_FLUSH);
    pImgBuf_MV_Y->syncCache(eCACHECTRL_FLUSH);
    pImgBuf_MASK_M->syncCache(eCACHECTRL_FLUSH);
    pImgBuf_MASK_S->syncCache(eCACHECTRL_FLUSH);

    // setup input enque buffers
    if(STEREO_SENSOR_REAR_MAIN_TOP == StereoSettingProvider::getSensorRelativePosition())
    {
        // Main1 locations: L
        setupDPEBufInfo(DMA_DVE_IMGI, pImgBuf_MV_Y, rDPEConfig.Dve_Imgi_l);
        setupDPEBufInfo(DMA_DVE_IMGI, pImgBuf_SV_Y, rDPEConfig.Dve_Imgi_r);
        setupDPEBufInfo(DMA_DVE_MASKI, pImgBuf_MASK_M, rDPEConfig.Dve_Maski_l);
        setupDPEBufInfo(DMA_DVE_MASKI, pImgBuf_MASK_S, rDPEConfig.Dve_Maski_r);
    }
    else
    {   // Main1 location: R
        setupDPEBufInfo(DMA_DVE_IMGI, pImgBuf_SV_Y, rDPEConfig.Dve_Imgi_l);
        setupDPEBufInfo(DMA_DVE_IMGI, pImgBuf_MV_Y, rDPEConfig.Dve_Imgi_r);
        setupDPEBufInfo(DMA_DVE_MASKI, pImgBuf_MASK_S, rDPEConfig.Dve_Maski_l);
        setupDPEBufInfo(DMA_DVE_MASKI, pImgBuf_MASK_M, rDPEConfig.Dve_Maski_r);
    }

    // check the first enque or not
    if(mbFirst)
    {
        mbFirst = MFALSE;
        rDPEConfig.Dve_Skp_Pre_Dv = true;
    }
    else
    {
        rDPEConfig.Dve_Skp_Pre_Dv = false;
    }
    // flush and setup DVE buffer
    mLastDMP_L->mImageBuffer->syncCache(eCACHECTRL_FLUSH);
    mLastDMP_R->mImageBuffer->syncCache(eCACHECTRL_FLUSH);
    setupDPEBufInfo(DMA_DVE_DVI, mLastDMP_L->mImageBuffer.get(), rDPEConfig.Dve_Dvi_l);
    setupDPEBufInfo(DMA_DVE_DVI, mLastDMP_R->mImageBuffer.get(), rDPEConfig.Dve_Dvi_r);

    // output buffers
    IImageBuffer* pImgBuf_DMP_L =  pBufferHandler->requestBuffer(getNodeId(), BID_DPE_OUT_DMP_L);
    IImageBuffer* pImgBuf_DMP_R =  pBufferHandler->requestBuffer(getNodeId(), BID_DPE_OUT_DMP_R);
    IImageBuffer* pImgBuf_CFM_L =  pBufferHandler->requestBuffer(getNodeId(), BID_DPE_OUT_CFM_L);
    IImageBuffer* pImgBuf_CFM_R =  pBufferHandler->requestBuffer(getNodeId(), BID_DPE_OUT_CFM_R);
    IImageBuffer* pImgBuf_RESPO_L =  pBufferHandler->requestBuffer(getNodeId(), BID_DPE_OUT_RESPO_L);
    IImageBuffer* pImgBuf_RESPO_R =  pBufferHandler->requestBuffer(getNodeId(), BID_DPE_OUT_RESPO_R);

    // setup output buffers and store to pEnqueBufPool
    setupDPEBufInfo(DMA_DVE_DVO, pImgBuf_DMP_L, rDPEConfig.Dve_Dvo_l);
    setupDPEBufInfo(DMA_DVE_DVO, pImgBuf_DMP_R, rDPEConfig.Dve_Dvo_r);
    setupDPEBufInfo(DMA_DVE_CONFO, pImgBuf_CFM_L, rDPEConfig.Dve_Confo_l);
    setupDPEBufInfo(DMA_DVE_CONFO, pImgBuf_CFM_R, rDPEConfig.Dve_Confo_r);
    setupDPEBufInfo(DMA_DVE_RESPO, pImgBuf_RESPO_L, rDPEConfig.Dve_Respo_l);
    setupDPEBufInfo(DMA_DVE_RESPO, pImgBuf_RESPO_R, rDPEConfig.Dve_Respo_r);

    #define DEBUG_BUFFER_SETUP(buf) \
        MY_LOGD("DPE buf:" # buf);\
        MY_LOGD("Image buffer size=%dx%d:", buf->getImgSize().w, buf->getImgSize().h);\
        MY_LOGD("Image buffer format=%x", buf->getImgFormat());\

    // debug section
    if(DepthPipeLoggingSetup::mbDebugLog)
    {
        MY_LOGD("STEREO_SENSOR_REAR_MAIN_TOP == getSensorRelativePosition()",
            STEREO_SENSOR_REAR_MAIN_TOP == StereoSettingProvider::getSensorRelativePosition());

        DEBUG_BUFFER_SETUP(pImgBuf_MV_Y);
        DEBUG_BUFFER_SETUP(pImgBuf_SV_Y);
        DEBUG_BUFFER_SETUP(pImgBuf_MASK_M);
        DEBUG_BUFFER_SETUP(pImgBuf_MASK_S);
        DEBUG_BUFFER_SETUP(mLastDMP_L->mImageBuffer);
        DEBUG_BUFFER_SETUP(mLastDMP_R->mImageBuffer);
        DEBUG_BUFFER_SETUP(pImgBuf_DMP_L);
        DEBUG_BUFFER_SETUP(pImgBuf_DMP_R);
        DEBUG_BUFFER_SETUP(pImgBuf_CFM_L);
        DEBUG_BUFFER_SETUP(pImgBuf_CFM_R);
        DEBUG_BUFFER_SETUP(pImgBuf_RESPO_L);
        DEBUG_BUFFER_SETUP(pImgBuf_RESPO_R);
    }

    #undef DEBUG_BUFFER_SETUP

    VSDOF_LOGD("-");
    return MTRUE;
}

const char*
DPENode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_DPE_OUT_DMP_L);
        MAKE_NAME_CASE(BID_DPE_OUT_DMP_R);
        MAKE_NAME_CASE(BID_DPE_OUT_CFM_L);
        MAKE_NAME_CASE(BID_DPE_OUT_CFM_R);
        MAKE_NAME_CASE(BID_DPE_OUT_RESPO_L);
        MAKE_NAME_CASE(BID_DPE_OUT_RESPO_R);
        MAKE_NAME_CASE(BID_N3D_OUT_MV_Y);
        MAKE_NAME_CASE(BID_N3D_OUT_SV_Y);

    }
    MY_LOGW("unknown BID:%d", BID);

    return "unknown";
#undef MAKE_NAME_CASE
}

}; //NSFeaturePipe_DepthMap_BM
}; //NSCamFeature
}; //NSCam
