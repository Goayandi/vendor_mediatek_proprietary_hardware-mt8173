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
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/aaa/aaa_hal_common.h>
#include <libion_mtk/include/ion.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
// Module header file
#include <vsdof/hal/stereo_tuning_provider.h>
// Local header file
#include "P2AFMNode.h"
#include "../DepthMapPipe_Common.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"

// Logging
#define PIPE_CLASS_TAG "P2A_FMNode"
#include <featurePipe/core/include/PipeLog.h>

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
P2AFMNode::
P2AFMNode(
    const char *name,
    DepthMapPipeNodeID nodeID,
    sp<DepthMapFlowOption> pFlowOpt,
    const DepthMapPipeSetting& setting
)
: DepthMapPipeNode(name, nodeID, pFlowOpt, setting)
, miSensorIdx_Main1(setting.miSensorIdx_Main1)
, miSensorIdx_Main2(setting.miSensorIdx_Main2)
{
    this->addWaitQueue(&mRequestQue);
}

P2AFMNode::
~P2AFMNode()
{
    MY_LOGD("[Destructor]");
}

MVOID
P2AFMNode::
cleanUp()
{
    MY_LOGD("+");
    if(mpINormalStream != NULL)
    {
        mpINormalStream->uninit("VSDOF_P2AFM");
        mpINormalStream->destroyInstance();
        mpINormalStream = NULL;
    }

    if(mp3AHal_Main1)
    {
        mp3AHal_Main1->destroyInstance("VSDOF_3A_MAIN1");
        mp3AHal_Main1 = NULL;
    }

    if(mp3AHal_Main2)
    {
        mp3AHal_Main2->destroyInstance("VSDOF_3A_MAIN2");
        mp3AHal_Main2 = NULL;
    }

    MY_LOGD("-");
}


MBOOL
P2AFMNode::
onInit()
{
    CAM_TRACE_NAME("P2AFMNode::onInit");
    MY_LOGD("+");
    // Create NormalStream
    VSDOF_LOGD("NormalStream create instance: idx=%d", miSensorIdx_Main1);
    CAM_TRACE_BEGIN("P2AFMNode::NormalStream::createInstance+init");
    mpINormalStream = NSCam::NSIoPipe::NSPostProc::INormalStream::createInstance(miSensorIdx_Main1);

    if (mpINormalStream == NULL)
    {
        MY_LOGE("mpINormalStream create instance for P2AFM Node failed!");
        cleanUp();
        return MFALSE;
    }
    mpINormalStream->init("VSDOF_P2AFM");
    CAM_TRACE_END();

    // 3A: create instance
    // UT does not test 3A
    CAM_TRACE_BEGIN("P2AFMNode::create_3A_instance");
    #ifndef GTEST
    mp3AHal_Main1 = MAKE_Hal3A(miSensorIdx_Main1, "VSDOF_3A_MAIN1");
    mp3AHal_Main2 = MAKE_Hal3A(miSensorIdx_Main2, "VSDOF_3A_MAIN2");
    MY_LOGD("3A create instance, Main1: %x Main2: %x", mp3AHal_Main1, mp3AHal_Main2);
    #endif
    CAM_TRACE_END();

    MY_LOGD("-");
    return MTRUE;
}

MBOOL
P2AFMNode::
onUninit()
{
    CAM_TRACE_NAME("P2AFMNode::onUninit");
    cleanUp();
    return MTRUE;
}

MBOOL
P2AFMNode::
onData(DataID data, DepthMapRequestPtr &request)
{
  MBOOL ret = MTRUE;
  VSDOF_LOGD("+ : reqID=%d", request->getRequestNo());
  CAM_TRACE_NAME("P2AFMNode::onData");

  switch(data)
  {
  case ROOT_ENQUE:
    mRequestQue.enque(request);
    break;
  default:
    MY_LOGW("Un-recognized data ID, id=%d reqID=%d", data, request->getRequestNo());
    ret = MFALSE;
    break;
  }

  VSDOF_LOGD("-");
  return ret;
}

const char*
P2AFMNode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_P2AFM_IN_FSRAW1);
        MAKE_NAME_CASE(BID_P2AFM_IN_FSRAW2);
        MAKE_NAME_CASE(BID_P2AFM_IN_RSRAW1);
        MAKE_NAME_CASE(BID_P2AFM_IN_RSRAW2);

        MAKE_NAME_CASE(BID_P2AFM_FE1B_INPUT);
        MAKE_NAME_CASE(BID_P2AFM_FE2B_INPUT);
        MAKE_NAME_CASE(BID_P2AFM_FE1C_INPUT);
        MAKE_NAME_CASE(BID_P2AFM_FE2C_INPUT);

        MAKE_NAME_CASE(BID_P2AFM_OUT_FDIMG);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE1AO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE2AO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE1BO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE2BO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE1CO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE2CO);

        MAKE_NAME_CASE(BID_P2AFM_OUT_RECT_IN1);
        MAKE_NAME_CASE(BID_P2AFM_OUT_RECT_IN2);
        MAKE_NAME_CASE(BID_P2AFM_OUT_MV_F);
        MAKE_NAME_CASE(BID_P2AFM_OUT_MV_F_CAP);
        MAKE_NAME_CASE(BID_P2AFM_OUT_CC_IN1);
        MAKE_NAME_CASE(BID_P2AFM_OUT_CC_IN2);

        MAKE_NAME_CASE(BID_P2AFM_OUT_FMAO_LR);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMAO_RL);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMBO_LR);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMBO_RL);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMCO_LR);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMCO_RL);

        #ifdef GTEST
        MAKE_NAME_CASE(BID_FE2_HWIN_MAIN1);
        MAKE_NAME_CASE(BID_FE2_HWIN_MAIN2);
        MAKE_NAME_CASE(BID_FE3_HWIN_MAIN1);
        MAKE_NAME_CASE(BID_FE3_HWIN_MAIN2);
        #endif

    }
    MY_LOGW("unknown BID:%d", BID);

    return "unknown";
#undef MAKE_NAME_CASE
}

MBOOL
P2AFMNode::
perform3AIspTuning(
    DepthMapRequestPtr& rEffectReqPtr,
    Stereo3ATuningRes& rOutTuningRes
)
{
    rOutTuningRes.tuningRes_main1 = applyISPTuning(rEffectReqPtr, eP2APATH_MAIN1);
    rOutTuningRes.tuningRes_main2 = applyISPTuning(rEffectReqPtr, eP2APATH_MAIN2);
    return MTRUE;
}

AAATuningResult
P2AFMNode::
applyISPTuning(
    DepthMapRequestPtr& rEffectReqPtr,
    StereoP2Path p2aPath
)
{
    CAM_TRACE_NAME("P2AFMNode::applyISPTuning");
    MBOOL bIsMain1Path = (p2aPath == eP2APATH_MAIN1);
    VSDOF_PRFLOG("+, reqID=%d bIsMain1Path=%d", rEffectReqPtr->getRequestNo(), bIsMain1Path);

    AAATuningResult result;
    sp<BaseBufferHandler> pBufferHandler = rEffectReqPtr->getBufferHandler();
    MVOID* pTuningBuf = pBufferHandler->requestTuningBuf(getNodeId(), BID_P2AFM_TUNING);
    result.tuningResult = {NULL, NULL};
    result.tuningResult.pRegBuf = pTuningBuf;
    result.tuningBuffer = pTuningBuf;

    // get APP/HAL meta
    DepthMapBufferID halMetaBID = bIsMain1Path ? BID_META_IN_HAL_MAIN1 : BID_META_IN_HAL_MAIN2;
    IMetadata* pMeta_InApp  = pBufferHandler->requestMetadata(getNodeId(), BID_META_IN_APP);
    IMetadata* pMeta_InHal  = pBufferHandler->requestMetadata(getNodeId(), halMetaBID);
    // pack into MetaSet_T
    MetaSet_T inMetaSet = {.appMeta=*pMeta_InApp, .halMeta = *pMeta_InHal};
    // capture+Main1Path is full raw, others is resize raw.
    MBOOL bIsInputResizedRaw = (rEffectReqPtr->getRequestAttr().opState != eSTATE_CAPTURE) || !bIsMain1Path;

    MY_LOGD("bIsInputResizedRaw %d", bIsInputResizedRaw);
    // USE resize raw-->set PGN 0
    if(bIsInputResizedRaw)
        updateEntry<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 0);
    else
        updateEntry<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 1);

    // manully tuning for current flow option
    mpFlowOption->config3ATuningMeta(p2aPath, inMetaSet);

    // UT do not test setIsp
    #ifndef GTEST
    IHal3A* p3AHAL = bIsMain1Path ? mp3AHal_Main1 : mp3AHal_Main2;
    // 3A result
    MetaSet_T resultMeta;
    // only capture need to get result
    MetaSet_T *resultPtr = (rEffectReqPtr->getRequestAttr().opState == eSTATE_CAPTURE) ? &resultMeta : NULL;
    p3AHAL->setIsp(0, inMetaSet, &result.tuningResult, resultPtr);
    // DO NOT write ISP resultMeta back into input hal Meta since there are other node doing this concurrently
   // (*pMeta_InHal) += resultMeta.halMeta;
    #else
    {   // UT: use default tuning
        SetDefaultTuning((dip_x_reg_t*)result.tuningResult.pRegBuf,
                        (MUINT32*)result.tuningResult.pRegBuf, tuning_tag_G2G, 0);
        SetDefaultTuning((dip_x_reg_t*)result.tuningResult.pRegBuf,
                        (MUINT32*)result.tuningResult.pRegBuf, tuning_tag_G2C, 0);
        SetDefaultTuning((dip_x_reg_t*)result.tuningResult.pRegBuf,
                        (MUINT32*)result.tuningResult.pRegBuf, tuning_tag_GGM, 0);
        SetDefaultTuning((dip_x_reg_t*)result.tuningResult.pRegBuf,
                        (MUINT32*)result.tuningResult.pRegBuf, tuning_tag_UDM, 0);
    }
    #endif

    VSDOF_PRFLOG("-, reqID=%d", rEffectReqPtr->getRequestNo());

    return result;
}

MBOOL
P2AFMNode::
onThreadLoop()
{
    DepthMapRequestPtr request;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }

    if( !mRequestQue.deque(request) )
    {
        MY_LOGE("mRequestQue.deque() failed");
        return MFALSE;
    }

    CAM_TRACE_NAME("P2AFMNode::onThreadLoop");

    // mark on-going-request start
    this->incExtThreadDependency();

    VSDOF_PRFLOG("threadLoop start, reqID=%d eState=%d",
                    request->getRequestNo(), request->getRequestAttr().opState);

    // enque QParams
    QParams enqueParams;
    // enque cookie instance
    EnqueCookieContainer *pCookieIns = new EnqueCookieContainer(request, this);

    // apply 3A Isp tuning
    Stereo3ATuningRes tuningRes;
    MBOOL bRet = perform3AIspTuning(request, tuningRes);

    if(!bRet)
        goto lbExit;

    // call flow option to build QParams
    bRet = mpFlowOption->buildQParam(request, tuningRes, enqueParams);
    //
    debugQParams(enqueParams);
    if(!bRet)
    {
        MY_LOGE("Failed to build P2 enque parametes.");
        goto lbExit;
    }

    // callback
    enqueParams.mpfnCallback = onP2Callback;
    enqueParams.mpfnEnQFailCallback = onP2FailedCallback;
    enqueParams.mpCookie = (MVOID*) pCookieIns;

    VSDOF_PRFLOG("mpINormalStream enque start! reqID=%d", request->getRequestNo());
    // start P2A timer
    request->mTimer.startP2AFM();
    request->mTimer.startP2AFMEnque();
    // enque
    CAM_TRACE_BEGIN("P2AFMNode::NormalStream::enque");
    bRet = mpINormalStream->enque(enqueParams);
    CAM_TRACE_END();
    // stop P2A Enque timer
    request->mTimer.stopP2AFMEnque();
    VSDOF_PRFLOG("mpINormalStream enque end! reqID=%d, exec-time(enque)=%d msec",
                    request->getRequestNo(), request->mTimer.getElapsedP2AFMEnque());
    if(!bRet)
    {
        MY_LOGE("mpINormalStream enque failed! reqID=%d", request->getRequestNo());
        goto lbExit;
    }

    VSDOF_PRFLOG("threadLoop end! reqID=%d", request->getRequestNo());

    return MTRUE;

lbExit:
    delete pCookieIns;
    // mark on-going-request end
    this->decExtThreadDependency();
    return MFALSE;

}

MVOID
P2AFMNode::
onP2Callback(QParams& rParams)
{
    EnqueCookieContainer* pEnqueData = (EnqueCookieContainer*) (rParams.mpCookie);
    P2AFMNode* pP2AFMNode = (P2AFMNode*) (pEnqueData->mpNode);
    pP2AFMNode->handleP2Done(rParams, pEnqueData);
}

MVOID
P2AFMNode::
onP2FailedCallback(QParams& rParams)
{
    MY_LOGE("P2A operations failed!!Check the following log:");
    EnqueCookieContainer* pEnqueData = (EnqueCookieContainer*) (rParams.mpCookie);
    P2AFMNode* pP2AFMNode = (P2AFMNode*) (pEnqueData->mpNode);
    debugQParams(rParams);
    pP2AFMNode->handleData(ERROR_OCCUR_NOTIFY, pEnqueData->mRequest);
    // launch onProcessDone
    pEnqueData->mRequest->getBufferHandler()->onProcessDone(pP2AFMNode->getNodeId());
    // mark on-going-request end
    pP2AFMNode->decExtThreadDependency();
    delete pEnqueData;
}

MVOID
P2AFMNode::
handleP2Done(QParams& rParams, EnqueCookieContainer* pEnqueCookie)
{
    CAM_TRACE_NAME("P2AFMNode::handleP2Done");
    DepthMapRequestPtr pRequest = pEnqueCookie->mRequest;
    // stop timer
    pRequest->mTimer.stopP2AFM();
    VSDOF_PRFLOG("+ :reqID=%d , p2 exec-time=%d ms", pRequest->getRequestNo(), pRequest->mTimer.getElapsedP2AFM());
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();

    // mark buffer ready and handle data
    // FD
    pRequest->setOutputBufferReady(BID_P2AFM_OUT_FDIMG);
    // config for FD input
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FDIMG, eDPETHMAP_PIPE_NODEID_FD);
    this->handleDataAndDump(P2AFM_OUT_FD, pRequest);
    // MV_F
    pRequest->setOutputBufferReady(BID_P2AFM_OUT_MV_F);
    this->handleDataAndDump(P2AFM_OUT_MV_F, pRequest);
    // MV_F_CAP
    pRequest->setOutputBufferReady(BID_P2AFM_OUT_MV_F_CAP);
    this->handleDataAndDump(P2AFM_OUT_MV_F_CAP, pRequest);

    // config output buffers for N3D input
    // FEO
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FE1BO, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FE2BO, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FE1CO, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FE2CO, eDPETHMAP_PIPE_NODEID_N3D);
    // FMO
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FMBO_LR, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FMBO_RL, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FMCO_LR, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_FMCO_RL, eDPETHMAP_PIPE_NODEID_N3D);
    // CC_in/Rect_in
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_CC_IN1, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_CC_IN2, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_RECT_IN1, eDPETHMAP_PIPE_NODEID_N3D);
    pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_RECT_IN2, eDPETHMAP_PIPE_NODEID_N3D);
    // Dump P1Raw
    DumpConfig config(NULL, ".raw", MTRUE);
    this->handleDump(TO_DUMP_RAWS, pRequest, &config);
    // pass to N3D
    this->handleDataAndDump(P2AFM_TO_N3D_FEFM_CCin, pRequest);
    #ifdef GTEST
    //dump HWFE input buffers
    this->handleDump(UT_OUT_FE, pRequest);
    #endif

    // if capture
    if(pRequest->getRequestAttr().opState == eSTATE_CAPTURE)
    {
        // pass to WMF node, BID_P2AFM_OUT_MY_SL is the alias of BID_P2AFM_FE1C_INPUT
        pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_MY_SL, eDPETHMAP_PIPE_NODEID_WMF);
        this->handleDataAndDump(P2AFM_TO_WMF_MY_SL, pRequest);
        // pass to FD node
        pBufferHandler->configOutBuffer(getNodeId(), BID_P2AFM_OUT_MY_SL, eDPETHMAP_PIPE_NODEID_FD);
        this->handleDataAndDump(P2AFM_TO_FD_IMG, pRequest);
    }

    // dump extra buffers
    config = DumpConfig(NULL, NULL, MFALSE);
    this->handleDump(TO_DUMP_BUFFERS, pRequest, &config);
    // launch onProcessDone
    pBufferHandler->onProcessDone(getNodeId());

    delete pEnqueCookie;
    VSDOF_PRFLOG("- :reqID=%d", pRequest->getRequestNo());

    // mark on-going-request end
    this->decExtThreadDependency();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2AFMNode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
P2AFMNode::
onThreadStop()
{
    return MTRUE;
}

}; //NSFeaturePipe_DepthMap_BM
}; //NSCamFeature
}; //NSCam

