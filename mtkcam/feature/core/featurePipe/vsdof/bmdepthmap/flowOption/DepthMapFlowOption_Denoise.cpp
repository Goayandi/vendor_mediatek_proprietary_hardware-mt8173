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

/**
 * @file DepthMapFlowOption_Denoise.cpp
 * @brief DepthMapFlowOption for VSDOF
 */

// Standard C header file

// Android system/core header file

// mtkcam custom header file
#include <camera_custom_stereo.h>
// mtkcam global header file
#include <mtkcam/def/common.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <drv/isp_reg.h>
#include <isp_tuning.h>
// Module header file
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <vsdof/hal/stereo_tuning_provider.h>
// Local header file
#include "DepthMapFlowOption_Denoise.h"
#include "../DepthMapPipe.h"
#include "../DepthMapPipeNode.h"
#include "./bufferPoolMgr/NodeBufferPoolMgr_Denoise.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"

#define PIPE_MODULE_TAG "DepthMapPipe"
#define PIPE_CLASS_TAG "DepthMapFlowOption_Denoise"
#include <featurePipe/core/include/PipeLog.h>

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

typedef NodeBufferSizeMgr::DPEBufferSize DPEBufferSize;
typedef NodeBufferSizeMgr::P2AFMBufferSize P2AFMBufferSize;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
DepthMapFlowOption_Denoise::
DepthMapFlowOption_Denoise(const DepthMapPipeSetting& setting)
: DepthMapFlowOption_VSDOF(setting)
{
}

DepthMapFlowOption_Denoise::
~DepthMapFlowOption_Denoise()
{
    MY_LOGD("[Destructor] +");
    #define CLEAR_KEYEDVECTOR_VALUE(keyedVec)\
        for(size_t index=0;index<keyedVec.size();++index)\
        {\
            delete keyedVec.valueAt(index);\
        }\
        keyedVec.clear();

    CLEAR_KEYEDVECTOR_VALUE(mSrzSizeTemplateMap);
    CLEAR_KEYEDVECTOR_VALUE(mFETuningBufferMap);
    CLEAR_KEYEDVECTOR_VALUE(mFMTuningBufferMap);
    MY_LOGD("[Destructor] -");
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  P2AFMFlowOption Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NSCam::NSIoPipe::PortID
DepthMapFlowOption_Denoise::
mapToPortID(DepthMapBufferID buffeID)
{
    switch(buffeID)
    {
        case BID_P2AFM_IN_RSRAW1:
        case BID_P2AFM_IN_RSRAW2:
        case BID_P2AFM_IN_FSRAW1:
        case BID_P2AFM_IN_FSRAW2:
            return PORT_IMGI;

        case BID_P2AFM_OUT_FDIMG:
        case BID_P2AFM_FE1C_INPUT:
        case BID_P2AFM_FE2C_INPUT:
            return PORT_IMG2O;

        case BID_P2AFM_OUT_FE1AO:
        case BID_P2AFM_OUT_FE2AO:
        case BID_P2AFM_OUT_FE1BO:
        case BID_P2AFM_OUT_FE2BO:
        case BID_P2AFM_OUT_FE1CO:
        case BID_P2AFM_OUT_FE2CO:
            return PORT_FEO;

        case BID_P2AFM_OUT_RECT_IN1:
        case BID_P2AFM_OUT_RECT_IN2:
        case BID_P2AFM_OUT_MV_F:
        case BID_P2AFM_OUT_MV_F_CAP:
            return PORT_WDMA;

        case BID_P2AFM_FE1B_INPUT:
        case BID_P2AFM_FE2B_INPUT:
            return PORT_WROT;

        case BID_P2AFM_OUT_CC_IN1:
        case BID_P2AFM_OUT_CC_IN2:
            return PORT_WDMA;

        case BID_P2AFM_OUT_FMBO_LR:
        case BID_P2AFM_OUT_FMBO_RL:
        case BID_P2AFM_OUT_FMCO_LR:
        case BID_P2AFM_OUT_FMCO_RL:
            return PORT_MFBO;

        #ifdef GTEST
        case BID_FE2_HWIN_MAIN1:
        case BID_FE2_HWIN_MAIN2:
        case BID_FE3_HWIN_MAIN1:
        case BID_FE3_HWIN_MAIN2:
            return PORT_IMG3O;
        #endif

        default:
            MY_LOGE("mapToPortID: not exist buffeID=%d", buffeID);
            break;
    }

    return NSCam::NSIoPipe::PortID();

}

MBOOL
DepthMapFlowOption_Denoise::
buildQParam(
    DepthMapRequestPtr pReq,
    const Stereo3ATuningRes& tuningResult,
    QParams& rOutParam
)
{
    MBOOL bRet;
    DepthMapPipeOpState opState = pReq->getRequestAttr().opState;
    if(opState == eSTATE_CAPTURE)
        bRet = buildQParams_CAPTURE(pReq, tuningResult, rOutParam);
    else
    {
        MY_LOGE("Denoise cannot support NORMAL mode.");
        return MFALSE;
    }


    return bRet;

}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

MBOOL
DepthMapFlowOption_Denoise::
prepareBurstQParamsTemplate(
    DepthMapPipeOpState state,
    MINT32 iModuleTrans,
    QParamTemplate& rQTmplate
)
{
    MY_LOGD("+");

    // only capture need QParms template
    if(state != eSTATE_CAPTURE)
        return MTRUE;

    MBOOL bRet = DepthMapFlowOption_VSDOF::prepareBurstQParamsTemplate(state, iModuleTrans, rQTmplate);
    MY_LOGD("-");
    return bRet;
}

MBOOL
DepthMapFlowOption_Denoise::
prepareQParam_frame1(
    DepthMapPipeOpState state,
    MINT32 iModuleTrans,
    QParamTemplate& rQTmplate
)
{
    // Denoise frame 1 is different from VSDOF.
    MUINT iFrameNum = 1;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_CAPTURE);
    //
    QParamTemplateGenerator generator =
        QParamTemplateGenerator(iFrameNum, miSensorIdx_Main1, ENormalStreamTag_Normal, &rQTmplate.mStats).
        addInput(PORT_IMGI);

    // capture request need extra input port for LSC buffer
    if(state == eSTATE_CAPTURE)
    {
        #ifndef GTEST
        generator.addInput(PORT_DEPI);
        #endif
    }

    // no FD and MV_F buffer for Denoise mode
    MBOOL bSuccess =
        // WROT: FE1BO input
        generator.addCrop(eCROP_WROT, MPoint(0,0), MSize(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN1).
            addOutput(mapToPortID(BID_P2AFM_FE1B_INPUT), iModuleTrans).                // do module rotation
            generate(rQTmplate);

    return bSuccess;
}


MBOOL
DepthMapFlowOption_Denoise::
buildQParams_CAPTURE(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParams& rOutParm
)
{
    CAM_TRACE_NAME("P2AFMNode::buildQParams_CAPTURE");
    VSDOF_PRFLOG("+, reqID=%d", rEffReqPtr->getRequestNo());
    // current node: P2AFMNode
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    // copy template
    rOutParm = mQParamTemplate_CAPTURE.mTemplate;
    // filler
    QParamTemplateFiller qParamFiller(rOutParm, mQParamTemplate_CAPTURE.mStats);

    MBOOL bRet = buildQParam_frame0(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame1_capture(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame2_capture(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame3_capture(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame4(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame5(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame6to9(rEffReqPtr, tuningResult, qParamFiller);

    if(!bRet)
        return MFALSE;

    bRet = qParamFiller.validate();
    VSDOF_PRFLOG("-, reqID=%d", rEffReqPtr->getRequestNo());
    return bRet;
}

MBOOL
DepthMapFlowOption_Denoise::
buildQParam_frame1_capture(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    MUINT iFrameNum = 1;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    // tuning result
    AAATuningResult tuningRes = tuningResult.tuningRes_main1;
    // UT does not test 3A
    #ifndef GTEST
    // LSC
    IImageBuffer* pLSC2Src;
    if(tuningRes.tuningResult.pLsc2Buf != NULL)
    {
        // input: LSC2 buffer (for tuning)
        pLSC2Src = static_cast<IImageBuffer*>(tuningRes.tuningResult.pLsc2Buf);
        rQFiller.insertInputBuf(iFrameNum, PORT_DEPI, pLSC2Src);
    }
    else
    {
        MY_LOGE("LSC2 buffer from 3A is NULL!!");
        return MFALSE;
    }
    #endif

    // input : FSRAW
    IImageBuffer* pBuf_FSRAW1 = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_IN_FSRAW1);
    // output: FE1B_input
    IImageBuffer* pFE1BInBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE1B_INPUT);

    // make sure the output is 16:9, get crop size& point
    MSize cropSize;
    MPoint startPoint;
    calCropForScreen(pBuf_FSRAW1, startPoint, cropSize);
    //
    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, pBuf_FSRAW1). //FSRAW
            setCrop(iFrameNum, eCROP_WROT, startPoint, cropSize, pFE1BInBuf->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE1B_INPUT), pFE1BInBuf). //FE1B_input
            insertTuningBuf(iFrameNum, tuningRes.tuningBuffer);

    return MTRUE;
}


MBOOL
DepthMapFlowOption_Denoise::
buildQParam_frame3_capture(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    MUINT iFrameNum = 3;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    // input: FE1BBuf_in
    IImageBuffer* pFE1BImgBuf;
    MBOOL bRet=pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_FE1B_INPUT, pFE1BImgBuf);
    // output: FE1C_input
    IImageBuffer* pFE1CBuf_in =pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE1C_INPUT);
    // output: Rect_in1
    IImageBuffer* pRectIn1Buf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_RECT_IN1);
    // set active area
    pRectIn1Buf->setExtParam(mSizeMgr.getP2AFM(eSTEREO_SCENARIO_CAPTURE).mRECT_IN_CONTENT_SIZE);
    // output: FE1BO
    IImageBuffer* pFE1BOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FE1BO);

    if(!bRet | !pFE1CBuf_in | !pRectIn1Buf | !pFE1BOBuf)
        return MFALSE;

    // Rect_in1 need setCrop. bcz its size is not constant between scenario
    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, pFE1BImgBuf). // FE1BBuf_in
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE1C_INPUT), pFE1CBuf_in). // FE1C_input
            setCrop(iFrameNum, eCROP_WDMA, MPoint(0,0), pFE1BImgBuf->getImgSize(), pRectIn1Buf->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN1), pRectIn1Buf). // Rect_in1
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FE1BO), pFE1BOBuf). // FE1BO
            insertTuningBuf(iFrameNum, (MVOID*)mFETuningBufferMap.valueFor(1));

    #ifdef GTEST
    IImageBuffer* pFEInputBuf = pBufferHandler->requestBuffer(nodeID, BID_FE2_HWIN_MAIN1);
    rQFiller.insertOutputBuf(iFrameNum, mapToPortID(BID_FE2_HWIN_MAIN1), pFEInputBuf);
    #endif
    return MTRUE;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

MBOOL
DepthMapFlowOption_Denoise::
queryReqAttrs(
    sp<DepthMapEffectRequest> pRequest,
    EffectRequestAttrs& rReqAttrs
)
{
    // Denoise only has capture
    rReqAttrs.isEISOn = MFALSE;
    rReqAttrs.opState = eSTATE_CAPTURE;
    rReqAttrs.bufferScenario = eBUFFER_POOL_SCENARIO_CAPTURE;

    return MTRUE;
}


MBOOL
DepthMapFlowOption_Denoise::
queryPipeNodeBitSet(PipeNodeBitSet& nodeBitSet)
{
    nodeBitSet.reset();
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_P2AFM, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_N3D, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_DPE, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_WMF, 0);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_OCC, 0);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_FD, 0);
    return MTRUE;
}

MBOOL
DepthMapFlowOption_Denoise::
buildPipeGraph(DepthMapPipe* pPipe, const DepthMapPipeNodeMap& nodeMap)
{
    DepthMapPipeNode* pP2AFMNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_P2AFM);
    DepthMapPipeNode* pN3DNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_N3D);
    DepthMapPipeNode* pDPENode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_DPE);

    #define CONNECT_DATA(DataID, src, dst)\
        pPipe->connectData(DataID, DataID, src, dst);\
        mvAllowDataIDMap.add(DataID, MTRUE);

    // P2AFM to N3D + P2AFM_output
    CONNECT_DATA(P2AFM_TO_N3D_FEFM_CCin, *pP2AFMNode, *pN3DNode);
    CONNECT_DATA(P2AFM_OUT_MV_F, *pP2AFMNode, pPipe);
    CONNECT_DATA(P2AFM_OUT_FD, *pP2AFMNode, pPipe);
    CONNECT_DATA(P2AFM_OUT_MV_F_CAP, *pP2AFMNode, pPipe);
    CONNECT_DATA(TO_DUMP_BUFFERS, *pP2AFMNode, pPipe);
    // P2AFM buffer dump
    CONNECT_DATA(TO_DUMP_RAWS, *pP2AFMNode, pPipe);
    CONNECT_DATA(DEPTHMAP_META_OUT, *pP2AFMNode, pPipe);
    // Hal Meta frame output
    CONNECT_DATA(P2AFM_TO_N3D_FEFM_CCin, *pP2AFMNode, *pN3DNode);
    // N3D to DPE/OCC
    CONNECT_DATA(N3D_TO_DPE_MVSV_MASK, *pN3DNode, *pDPENode);
    CONNECT_DATA(N3D_OUT_JPS_WARPING_MATRIX, *pN3DNode, pPipe);
    // DPE output
    CONNECT_DATA(DPE_OUT_DISPARITY, *pDPENode, pPipe);

    // Hal Meta frame output
    CONNECT_DATA(DEPTHMAP_META_OUT, *pN3DNode, pPipe);

    pPipe->setRootNode(pP2AFMNode);

    return MTRUE;
}

MBOOL
DepthMapFlowOption_Denoise::
checkConnected(
    DepthMapDataID dataID
)
{
    // check data id is allowed to handledata or not.
    if(mvAllowDataIDMap.indexOfKey(dataID)>=0)
    {
        return MTRUE;
    }
    return MFALSE;
}

MSize
DepthMapFlowOption_Denoise::
getFDBufferSize()
{
    // no fd buffer
    return MSize(0,0);
}

MBOOL
DepthMapFlowOption_Denoise::
getCaptureFDEnable()
{
    return MFALSE;
}

MBOOL
DepthMapFlowOption_Denoise::
config3ATuningMeta(
    StereoP2Path path,
    MetaSet_T& rMetaSet
)
{
    // main1 path need to-W profile
    if(path == eP2APATH_MAIN1)
    {
        trySetMetadata<MUINT8>(&rMetaSet.halMeta, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Capture_toW);
    }
    else
    {
        trySetMetadata<MUINT8>(&rMetaSet.halMeta, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Capture);
    }

    return MTRUE;
}

DepthMapBufferID
DepthMapFlowOption_Denoise::
reMapBufferID(
    const EffectRequestAttrs& reqAttr,
    DepthMapBufferID bufferID
)
{   // no remapped
    return bufferID;
}

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam
