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
 * @file DepthMapFlowOption_VSDOF.cpp
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
#include <drv/isp_reg.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
// Module header file

#include <vsdof/hal/stereo_tuning_provider.h>
// Local header file
#include "DepthMapFlowOption_VSDOF.h"
#include "../DepthMapPipe.h"
#include "../DepthMapPipeNode.h"
#include "./bufferPoolMgr/NodeBufferPoolMgr_VSDOF.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"

#define PIPE_MODULE_TAG "DepthMapPipe"
#define PIPE_CLASS_TAG "DepthMapFlowOption_VSDOF"
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
DepthMapFlowOption_VSDOF::
DepthMapFlowOption_VSDOF(const DepthMapPipeSetting& setting)
: miSensorIdx_Main1(setting.miSensorIdx_Main1)
, miSensorIdx_Main2(setting.miSensorIdx_Main2)
{
    // Record the frame index of the L/R-Side which is defined by ALGO.
    // L-Side could be main1 or main2 which can be identified by the sensors' locations.
    // Frame 0,3,5 is main1, 1,2,6 is main2.
    if(STEREO_SENSOR_REAR_MAIN_TOP == StereoSettingProvider::getSensorRelativePosition())
    {
        frameIdx_LSide[0] = 3;      // frame index of Left Side(Main1)
        frameIdx_LSide[1] = 5;
        frameIdx_RSide[0] = 2;      // frame index of Right Side(Main2)
        frameIdx_RSide[1] = 4;
    }
    else
    {
        // Main1: R, Main2: L
        frameIdx_LSide[0] = 2;
        frameIdx_LSide[1] = 4;
        frameIdx_RSide[0] = 3;
        frameIdx_RSide[1] = 5;
    }
}

DepthMapFlowOption_VSDOF::
~DepthMapFlowOption_VSDOF()
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
DepthMapFlowOption_VSDOF::
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
DepthMapFlowOption_VSDOF::
buildQParam(
    DepthMapRequestPtr pRequest,
    const Stereo3ATuningRes& tuningResult,
    QParams& rOutParam
)
{
    MBOOL bRet;
    DepthMapPipeOpState opState = pRequest->getRequestAttr().opState;
    if(opState == eSTATE_NORMAL)
        bRet = buildQParams_NORMAL(pRequest, tuningResult, rOutParam);
    else if(opState == eSTATE_CAPTURE)
        bRet = buildQParams_CAPTURE(pRequest, tuningResult, rOutParam);

    return bRet;

}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
DepthMapFlowOption_VSDOF::
init()
{
    // prepare template
    MBOOL bRet = prepareTemplateParams();
    return bRet;
}

MBOOL
DepthMapFlowOption_VSDOF::
generateQParamTemplate(
    DepthMapPipeOpState opState,
    QParamTemplate& rQTmplate
)
{
    MY_LOGD("+");
    using namespace NSCam::NSIoPipe::NSPostProc;
    QParams burstParams;

    // module rotation
    ENUM_ROTATION eRot = StereoSettingProvider::getModuleRotation();
    MINT32 iModuleTrans = -1;
    switch(eRot)
    {
        case eRotate_0:
            iModuleTrans = 0;
            break;
        case eRotate_90:
            iModuleTrans = eTransform_ROT_90;
            break;
        case eRotate_180:
            iModuleTrans = eTransform_ROT_180;
            break;
        case eRotate_270:
            iModuleTrans = eTransform_ROT_270;
            break;
        default:
            MY_LOGE("Not support module rotation =%d", eRot);
            return MFALSE;
    }

    // prepare template
    MBOOL bRet = prepareBurstQParamsTemplate(opState, iModuleTrans, rQTmplate);

    MY_LOGD("-");
    return bRet;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareBurstQParamsTemplate(
    DepthMapPipeOpState state,
    MINT32 iModuleTrans,
    QParamTemplate& rQTmplate
)
{
    MY_LOGD("+");
    using namespace NSCam::NSIoPipe::NSPostProc;

    MBOOL bSuccess = prepareQParam_frame0(iModuleTrans, rQTmplate);
    bSuccess &= prepareQParam_frame1(state, iModuleTrans, rQTmplate);
    bSuccess &= prepareQParam_frame2(rQTmplate);
    bSuccess &= prepareQParam_frame3(rQTmplate);
    bSuccess &= prepareQParam_frame4(rQTmplate);
    bSuccess &= prepareQParam_frame5(rQTmplate);
    bSuccess &= prepareQParam_frame6_9(rQTmplate);

    MY_LOGD("-");
    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame0(
    MINT32 iModuleTrans,
    QParamTemplate& rQTmplate
)
{
    MUINT iFrameNum = 0;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);

    MBOOL bSuccess =
        QParamTemplateGenerator(iFrameNum, miSensorIdx_Main2, ENormalStreamTag_Normal).
            addInput(PORT_IMGI).
            addCrop(eCROP_WROT, MPoint(0,0), MSize(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN2).     // WROT : FE2BO input
            addOutput(mapToPortID(BID_P2AFM_FE2B_INPUT), iModuleTrans).
            generate(rQTmplate);

    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame1(
    DepthMapPipeOpState state,
    MINT32 iModuleTrans,
    QParamTemplate& rQTmplate
)
{
    MUINT iFrameNum = 1;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);
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

    MBOOL bSuccess =
        generator.addCrop(eCROP_CRZ, MPoint(0,0), MSize(0,0), rP2AFMSize.mFD_IMG_SIZE).  // IMG2O : FD
            addOutput(mapToPortID(BID_P2AFM_OUT_FDIMG)).
            addCrop(eCROP_WDMA, MPoint(0,0), MSize(0,0), rP2AFMSize.mMAIN_IMAGE_SIZE).  // WDMA : MV_F
            addOutput(mapToPortID(BID_P2AFM_OUT_MV_F)).
            addCrop(eCROP_WROT, MPoint(0,0), MSize(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN1).  // WROT: FE1BO input
            addOutput(mapToPortID(BID_P2AFM_FE1B_INPUT), iModuleTrans).                // do module rotation
            generate(rQTmplate);

    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame2(QParamTemplate& rQTmplate)
{
    MUINT iFrameNum = 2;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);

    MVOID* srzInfo = reinterpret_cast<MVOID*>(mSrzSizeTemplateMap.valueFor(iFrameNum));

    MBOOL bSuccess =
        QParamTemplateGenerator(iFrameNum, miSensorIdx_Main2, ENormalStreamTag_Normal, &rQTmplate.mStats).
            addInput(PORT_IMGI).
            addCrop(eCROP_CRZ, MPoint(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN2, rP2AFMSize.mFEC_INPUT_SIZE_MAIN2).  // IMG2O: FE2CO input
            addOutput(mapToPortID(BID_P2AFM_FE2C_INPUT)).
            addCrop(eCROP_WDMA, MPoint(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN2, MSize(0,0)).  // WDMA : Rect_in2
            addOutput(mapToPortID(BID_P2AFM_OUT_RECT_IN2)).
            #ifdef GTEST
            addOutput(PORT_IMG3O).
            #endif
            addOutput(PORT_FEO).                           // FEO
            addModuleInfo(EDipModule_SRZ1, srzInfo).       // FEO SRZ1 config
            generate(rQTmplate);

    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame3(QParamTemplate& rQTmplate)
{
    MUINT iFrameNum = 3;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);

    MBOOL bSuccess =
    QParamTemplateGenerator(iFrameNum, miSensorIdx_Main1, ENormalStreamTag_Normal, &rQTmplate.mStats).
        addInput(PORT_IMGI).
        addCrop(eCROP_CRZ, MPoint(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN1, rP2AFMSize.mFEC_INPUT_SIZE_MAIN1).  // IMG2O: FE1CO input
        addOutput(mapToPortID(BID_P2AFM_FE1C_INPUT)).
        addCrop(eCROP_WDMA, MPoint(0,0), rP2AFMSize.mFEB_INPUT_SIZE_MAIN1, MSize(0,0)).  // WDMA : Rect_in1
        addOutput(mapToPortID(BID_P2AFM_OUT_RECT_IN1)).
        #ifdef GTEST
        addOutput(PORT_IMG3O).
        #endif
        addOutput(PORT_FEO).   // FEO
        generate(rQTmplate);

    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame4(QParamTemplate& rQTmplate)
{
    MUINT iFrameNum = 4;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);

    MVOID* srzInfo = reinterpret_cast<MVOID*>(mSrzSizeTemplateMap.valueFor(iFrameNum));
    MBOOL bSuccess =
    QParamTemplateGenerator(iFrameNum, miSensorIdx_Main2, ENormalStreamTag_Normal, &rQTmplate.mStats).
        addInput(PORT_IMGI).
        addCrop(eCROP_WDMA, MPoint(0,0), rP2AFMSize.mFEC_INPUT_SIZE_MAIN2, rP2AFMSize.mCCIN_SIZE).  // WDMA: CC_in2
        addOutput(mapToPortID(BID_P2AFM_OUT_CC_IN2)).
        #ifdef GTEST
        addOutput(PORT_IMG3O).
        #endif
        addOutput(PORT_FEO).                           // FEO
        addModuleInfo(EDipModule_SRZ1, srzInfo).       // FEO SRZ1 config
        generate(rQTmplate);

    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame5(QParamTemplate& rQTmplate)
{
    //--> frame 5
    MUINT iFrameNum = 5;
    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);

    MBOOL bSuccess =
    QParamTemplateGenerator(iFrameNum, miSensorIdx_Main1, ENormalStreamTag_Normal, &rQTmplate.mStats).   // frame 5
        addInput(PORT_IMGI).
        addCrop(eCROP_WDMA, MPoint(0,0), rP2AFMSize.mFEC_INPUT_SIZE_MAIN1, rP2AFMSize.mCCIN_SIZE).  // WDMA: CC_in1
        addOutput(mapToPortID(BID_P2AFM_OUT_CC_IN1)).
        #ifdef GTEST
        addOutput(PORT_IMG3O).
        #endif
        addOutput(PORT_FEO).                           // FEO
        generate(rQTmplate);

    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareQParam_frame6_9(QParamTemplate& rQTmplate)
{
    MBOOL bSuccess = MTRUE;
    //--> frame 6 ~ 9
    for(int frameID=6;frameID<10;++frameID)
    {
        bSuccess &=
        QParamTemplateGenerator(frameID, miSensorIdx_Main1, ENormalStreamTag_FM, &rQTmplate.mStats).
            addInput(PORT_DEPI).
            addInput(PORT_DMGI).
            addOutput(PORT_MFBO).
            generate(rQTmplate);
    }
    return bSuccess;
}

MBOOL
DepthMapFlowOption_VSDOF::
prepareTemplateParams()
{
    MY_LOGD("+");
    //SRZ: crop first, then resize.
    #define CONFIG_SRZINFO_TO_CROPAREA(SrzInfo, StereoArea) \
        SrzInfo->in_w =  StereoArea.size.w;\
        SrzInfo->in_h =  StereoArea.size.h;\
        SrzInfo->crop_w = StereoArea.size.w - StereoArea.padding.w;\
        SrzInfo->crop_h = StereoArea.size.h - StereoArea.padding.h;\
        SrzInfo->crop_x = StereoArea.startPt.x;\
        SrzInfo->crop_y = StereoArea.startPt.y;\
        SrzInfo->crop_floatX = 0;\
        SrzInfo->crop_floatY = 0;\
        SrzInfo->out_w = StereoArea.size.w - StereoArea.padding.w;\
        SrzInfo->out_h = StereoArea.size.h - StereoArea.padding.h;

    const P2AFMBufferSize& rP2AFMSize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);
    // FE SRZ template - frame 2/4 need FEO srz crop
    mSrzSizeTemplateMap.add(2,  new _SRZ_SIZE_INFO_());
    mSrzSizeTemplateMap.add(4,  new _SRZ_SIZE_INFO_());
    CONFIG_SRZINFO_TO_CROPAREA(mSrzSizeTemplateMap.valueFor(2), rP2AFMSize.mFEBO_AREA_MAIN2);
    CONFIG_SRZINFO_TO_CROPAREA(mSrzSizeTemplateMap.valueFor(4), rP2AFMSize.mFECO_AREA_MAIN2);

    // FM tuning template
    // frame 6/8 - forward +  frame 7/9 - backward
    for(int frameID=6;frameID<10;++frameID)
    {
        // allocate buffer
        dip_x_reg_t* pIspReg = new dip_x_reg_t();
        memset((void*)pIspReg, 0, sizeof(dip_x_reg_t));
        // setup
        setupFMTuningSetting(pIspReg, frameID);
        MY_LOGD("frameID=%d mFMTuningBufferMap add!!DIP_X_FM_SIZE =%x DIP_X_FM_TH_CON0=%x ", frameID,
                pIspReg->DIP_X_FM_SIZE.Raw, pIspReg->DIP_X_FM_TH_CON0.Raw);
        mFMTuningBufferMap.add(frameID, pIspReg);
    }

    // prepare FE tuning buffer
    for (int iStage=1;iStage<=2;++iStage)
    {
        // allocate buffer
        dip_x_reg_t* pIspReg = new dip_x_reg_t();
        memset((void*)pIspReg, 0, sizeof(dip_x_reg_t));
        // setup
        MUINT32 iBlockSize = StereoSettingProvider::fefmBlockSize(iStage);
        setupFETuningSetting(pIspReg, iBlockSize);
        mFETuningBufferMap.add(iStage, pIspReg);
    }

    // prepare QParams template
    MBOOL bRet = generateQParamTemplate(eSTATE_NORMAL, mQParamTemplate_NORMAL);
    bRet &= generateQParamTemplate(eSTATE_CAPTURE, mQParamTemplate_CAPTURE);
    if(!bRet)
    {
        MY_LOGE("generateQParamTemplate failed!!!");
    }

    MY_LOGD("-");
    return bRet;
}


MVOID
DepthMapFlowOption_VSDOF::
setupFETuningSetting(dip_x_reg_t* pTuningBuf, MUINT iBlockSize)
{
    pTuningBuf->DIP_X_CTL_YUV_EN.Raw = 0;
    pTuningBuf->DIP_X_CTL_YUV2_EN.Raw = 0;
    pTuningBuf->DIP_X_CTL_RGB_EN.Raw = 0;
    // query tuning from tuning provider
    StereoTuningProvider::getFETuningInfo(pTuningBuf, iBlockSize);
}

MVOID
DepthMapFlowOption_VSDOF::
setupFMTuningSetting(dip_x_reg_t* pTuningBuf, MUINT iFrameID)
{
    MY_LOGD("+");
    pTuningBuf->DIP_X_CTL_YUV_EN.Raw = 0;
    pTuningBuf->DIP_X_CTL_YUV2_EN.Raw = 0x00000001;
    pTuningBuf->DIP_X_CTL_RGB_EN.Raw = 0;

    MSize szFEBufSize = (iFrameID<=7) ? mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD).mFEB_INPUT_SIZE_MAIN1 :
                                        mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD).mFEC_INPUT_SIZE_MAIN1;
    MUINT32 iStage = (iFrameID<=7) ? 1 : 2;

    ENUM_FM_DIRECTION eDir = (iFrameID % 2 == 0) ? E_FM_L_TO_R : E_FM_R_TO_L;
    // query tuning parameter
    StereoTuningProvider::getFMTuningInfo(eDir, pTuningBuf->DIP_X_FM_SIZE, pTuningBuf->DIP_X_FM_TH_CON0);
    MUINT32 iBlockSize =  StereoSettingProvider::fefmBlockSize(iStage);
    // set width/height
    pTuningBuf->DIP_X_FM_SIZE.Bits.FM_WIDTH = szFEBufSize.w/iBlockSize;
    pTuningBuf->DIP_X_FM_SIZE.Bits.FM_HEIGHT = szFEBufSize.h/iBlockSize;

    MY_LOGD("frameID =%d DIP_X_FM_SIZE =%x DIP_X_FM_TH_CON0=%x", iFrameID, pTuningBuf->DIP_X_FM_SIZE.Raw, pTuningBuf->DIP_X_FM_TH_CON0.Raw);

    MY_LOGD("-");
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParams_NORMAL(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParams& rOutParm
)
{

    CAM_TRACE_NAME("P2AFMNode::buildQParams_NORMAL");
    VSDOF_PRFLOG("+, reqID=%d", rEffReqPtr->getRequestNo());
    // current node: P2AFMNode
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    // copy template
    rOutParm = mQParamTemplate_NORMAL.mTemplate;
    // filler
    QParamTemplateFiller qParamFiller(rOutParm, mQParamTemplate_NORMAL.mStats);

    MBOOL bRet = buildQParam_frame0(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame1(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame2(rEffReqPtr, tuningResult, qParamFiller);
    bRet &= buildQParam_frame3(rEffReqPtr, tuningResult, qParamFiller);
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
DepthMapFlowOption_VSDOF::
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
DepthMapFlowOption_VSDOF::
buildQParam_frame0(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    MUINT iFrameNum = 0;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;

    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    IImageBuffer* frameBuf_RSRAW2 = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_IN_RSRAW2);

    AAATuningResult tuningRes = tuningResult.tuningRes_main2;
    // output: FE2B input(WROT)
    IImageBuffer* pImgBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE2B_INPUT);
    // filler buffers and the raw size crop info
    rQFiller.insertTuningBuf(iFrameNum, tuningRes.tuningBuffer).  // insert tuning data
        insertInputBuf(iFrameNum, PORT_IMGI, frameBuf_RSRAW2). // input: Main2 RSRAW
        setCrop(iFrameNum, eCROP_WROT, MPoint(0,0), frameBuf_RSRAW2->getImgSize(),
                    mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD).mFEB_INPUT_SIZE_MAIN2).
        insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE2B_INPUT), pImgBuf);// output: FE2B input(WROT)
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame1(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    MUINT iFrameNum = 1;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    // input : RSRAW
    IImageBuffer* frameBuf_RSRAW1 = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_IN_RSRAW1);
    // tuning result
    AAATuningResult tuningRes = tuningResult.tuningRes_main1;

    // check exist Request FD buffer or not
    IImageBuffer* pFdBuf;
    MBOOL bExistFD = rEffReqPtr->getRequestImageBuffer({.bufferID=BID_P2AFM_OUT_FDIMG,
                                                        .ioType=eBUFFER_IOTYPE_OUTPUT}, pFdBuf);
    // fill buffer
    rQFiller.insertTuningBuf(iFrameNum, tuningRes.tuningBuffer). // insert tuning data
            insertInputBuf(iFrameNum, PORT_IMGI, frameBuf_RSRAW1); // input: Main1 RSRAW

    if(bExistFD)
    {
        // insert FD output
        rQFiller.insertOutputBuf(iFrameNum, PORT_IMG2O, pFdBuf).
                setCrop(iFrameNum, eCROP_CRZ, MPoint(0,0), frameBuf_RSRAW1->getImgSize(), pFdBuf->getImgSize());
    }
    else
    {   // no FD-> remove port in template.
        rQFiller.delOutputPort(iFrameNum, PORT_IMG2O, eCROP_CRZ);
    }
    // output: MV_F
    IImageBuffer* frameBuf_MV_F = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_MV_F);
    // check EIS on/off
    if (rEffReqPtr->getRequestAttr().isEISOn)
    {
        eis_region region;
        IMetadata* pMeta_InHal = pBufferHandler->requestMetadata(nodeID, BID_META_IN_HAL_MAIN1);
        // set MV_F crop for EIS
        if(queryEisRegion(pMeta_InHal, region))
        {
            rQFiller.setCrop(1, eCROP_WDMA, MPoint(region.x_int, region.y_int), region.s, frameBuf_MV_F->getImgSize());
        }
        else
        {
            MY_LOGE("Query EIS Region Failed! reqID=%d.", rEffReqPtr->getRequestNo());
            return MFALSE;
        }
    }
    else
    {   // MV_F crop
        rQFiller.setCrop(iFrameNum, eCROP_WDMA, MPoint(0,0), frameBuf_RSRAW1->getImgSize(), frameBuf_MV_F->getImgSize());
    }
    // ouput: FE1B_input
    IImageBuffer* pImgBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE1B_INPUT);

    rQFiller.insertOutputBuf(iFrameNum, PORT_WDMA, frameBuf_MV_F).  // MV_F
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE1B_INPUT), pImgBuf). //FE1B_input
            setCrop(iFrameNum, eCROP_WROT, MPoint(0,0), frameBuf_RSRAW1->getImgSize(), pImgBuf->getImgSize());  //FE1B_input

    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
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
    // output : FD
    IImageBuffer* pFDBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FDIMG);
    // output : MV_F_CAP
    IImageBuffer* pBuf_MV_F_CAP = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_MV_F_CAP);
    // output: FE1B_input
    IImageBuffer* pFE1BInBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE1B_INPUT);

    // make sure the output is 16:9, get crop size& point
    MSize cropSize;
    MPoint startPoint;
    calCropForScreen(pBuf_FSRAW1, startPoint, cropSize);
    //
    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, pBuf_FSRAW1). //FSRAW
            setCrop(iFrameNum, eCROP_CRZ, startPoint, cropSize, pFDBuf->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FDIMG), pFDBuf). //FD
            setCrop(iFrameNum, eCROP_WDMA, startPoint, cropSize, pBuf_MV_F_CAP->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_MV_F_CAP), pBuf_MV_F_CAP). //MV_F_CAP
            setCrop(iFrameNum, eCROP_WROT, startPoint, cropSize, pFE1BInBuf->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE1B_INPUT), pFE1BInBuf). //FE1B_input
            insertTuningBuf(iFrameNum, tuningRes.tuningBuffer);

    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
calCropForScreen(
    IImageBuffer* pImgBuf,
    MPoint &rCropStartPt,
    MSize& rCropSize
)
{
    MSize srcSize = pImgBuf->getImgSize();

    // calculate the required image hight according to image ratio
    int iHeight;
    switch(StereoSettingProvider::imageRatio())
    {
        case eRatio_4_3:
            iHeight = ((srcSize.w * 3 / 4) >> 1 ) <<1;
            break;
        case eRatio_16_9:
        default:
            iHeight = ((srcSize.w * 9 / 16) >> 1 ) <<1;
            break;
    }

    if(abs(iHeight-srcSize.h) == 0)
    {
        rCropStartPt = MPoint(0, 0);
        rCropSize = srcSize;
    }
    else
    {
        rCropStartPt.x = 0;
        rCropStartPt.y = (srcSize.h - iHeight)/2;
        rCropSize.w = srcSize.w;
        rCropSize.h = iHeight;
    }

    VSDOF_LOGD("calCropForScreen rCropStartPt: (%d, %d), \
                    rCropSize: %dx%d ", rCropStartPt.x, rCropStartPt.y, rCropSize.w, rCropSize.h);

    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame2(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    //--> frame 2
    MUINT iFrameNum = 2;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    // input: fe2bBuf_in
    IImageBuffer* fe2bBuf_in;
    MBOOL bRet=pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_FE2B_INPUT, fe2bBuf_in);
    // output: fe2cBuf_in
    IImageBuffer* fe2cBuf_in =  pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE2C_INPUT);
    // output: Rect_in2
    IImageBuffer* pRectIn2Buf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_RECT_IN2);
    // output: FE2BO
    IImageBuffer* pFE2BOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FE2BO);

    if(!bRet || !fe2cBuf_in || !pRectIn2Buf || !pFE2BOBuf)
        return MFALSE;

    // output: Rect_in2 NEEDs setCrop because the Rect_in size is change for each scenario
    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe2bBuf_in).  // fe2bBuf_in
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE2C_INPUT), fe2cBuf_in). // fe2cBuf_in
            setCrop(iFrameNum, eCROP_WDMA, MPoint(0,0), fe2bBuf_in->getImgSize(), pRectIn2Buf->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN2), pRectIn2Buf). // Rect_in2
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FE2BO), pFE2BOBuf). // FE2BO
            insertTuningBuf(iFrameNum, (MVOID*)mFETuningBufferMap.valueFor(1)); // tuning data
    #ifdef GTEST
    IImageBuffer* pFEInputBuf = pBufferHandler->requestBuffer(nodeID, BID_FE2_HWIN_MAIN2);
    rQFiller.insertOutputBuf(iFrameNum, mapToPortID(BID_FE2_HWIN_MAIN2), pFEInputBuf);
    #endif
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame2_capture(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    //--> frame 2
    MUINT iFrameNum = 2;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    // input: fe2bBuf_in
    IImageBuffer* fe2bBuf_in;
    MBOOL bRet=pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_FE2B_INPUT, fe2bBuf_in);
    // output: fe2cBuf_in
    IImageBuffer* fe2cBuf_in =  pBufferHandler->requestBuffer(nodeID, BID_P2AFM_FE2C_INPUT);
    // output: Rect_in2
    IImageBuffer* pRectIn2Buf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_RECT_IN2);
    // Rect_in2 : set active area
    pRectIn2Buf->setExtParam(mSizeMgr.getP2AFM(eSTEREO_SCENARIO_CAPTURE).mRECT_IN_CONTENT_SIZE);
    // output: FE2BO
    IImageBuffer* pFE2BOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FE2BO);

    if(!bRet || !fe2cBuf_in || !pRectIn2Buf || !pFE2BOBuf)
        return MFALSE;

    // output: Rect_in2 NEEDs setCrop because the Rect_in size is change for each scenario
    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe2bBuf_in).  // fe2bBuf_in
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_FE2C_INPUT), fe2cBuf_in). // fe2cBuf_in
            setCrop(iFrameNum, eCROP_WDMA, MPoint(0,0), fe2bBuf_in->getImgSize(), pRectIn2Buf->getImgSize()).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN2), pRectIn2Buf). // Rect_in2
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FE2BO), pFE2BOBuf). // FE2BO
            insertTuningBuf(iFrameNum, (MVOID*)mFETuningBufferMap.valueFor(1)); // tuning data
    #ifdef GTEST
    IImageBuffer* pFEInputBuf = pBufferHandler->requestBuffer(nodeID, BID_FE2_HWIN_MAIN2);
    rQFiller.insertOutputBuf(iFrameNum, mapToPortID(BID_FE2_HWIN_MAIN2), pFEInputBuf);
    #endif
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame3(
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

MBOOL
DepthMapFlowOption_VSDOF::
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
    // output: Rect_in1, write into the JPSMain1 frame buffer in Capture scenario
    IImageBuffer* pRectIn1Buf = pBufferHandler->requestBuffer(nodeID, BID_N3D_OUT_JPS_MAIN1);
    // Rect_in1 : set active area
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

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame4(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    MUINT iFrameNum = 4;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();
    // input: fe2coBuf_in
    IImageBuffer* pFE2CBuf_in;
    MBOOL bRet=pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_FE2C_INPUT, pFE2CBuf_in);
    // output: CC_in2
    IImageBuffer* pCCIn2Buf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_CC_IN2);
    // output: FE2CO
    IImageBuffer* pFE2COBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FE2CO);

    if(!bRet | !pCCIn2Buf | !pFE2COBuf)
        return MFALSE;

    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, pFE2CBuf_in). // FE2CBuf_in
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_CC_IN2), pCCIn2Buf). // CC_in2
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FE2CO), pFE2COBuf). // FE2COBuf
            insertTuningBuf(iFrameNum, (MVOID*)mFETuningBufferMap.valueFor(2));

    #ifdef GTEST
    IImageBuffer* pFEInputBuf = pBufferHandler->requestBuffer(nodeID, BID_FE3_HWIN_MAIN2);
    rQFiller.insertOutputBuf(iFrameNum, mapToPortID(BID_FE3_HWIN_MAIN2), pFEInputBuf);
    #endif
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame5(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    MUINT iFrameNum = 5;
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();

    // input: fe1coBuf_in
    IImageBuffer* pFE1CBuf_in;
    MBOOL bRet=pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_FE1C_INPUT, pFE1CBuf_in);
    // output: CC_in1
    IImageBuffer* pCCIn1Buf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_CC_IN1);
    // output: FE1CO
    IImageBuffer* pFE1COBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FE1CO);

    if(!bRet | !pCCIn1Buf | !pFE1COBuf)
        return MFALSE;

    rQFiller.insertInputBuf(iFrameNum, PORT_IMGI, pFE1CBuf_in). // FE2CBuf_in
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_CC_IN1), pCCIn1Buf). // CC_in1
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FE1CO), pFE1COBuf). // FE1CO
            insertTuningBuf(iFrameNum, (MVOID*)mFETuningBufferMap.valueFor(2));


    #ifdef GTEST
    IImageBuffer* pFEInputBuf = pBufferHandler->requestBuffer(nodeID, BID_FE3_HWIN_MAIN1);
    rQFiller.insertOutputBuf(iFrameNum, mapToPortID(BID_FE3_HWIN_MAIN1), pFEInputBuf);
    #endif
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildQParam_frame6to9(
    DepthMapRequestPtr& rEffReqPtr,
    const Stereo3ATuningRes& tuningResult,
    QParamTemplateFiller& rQFiller
)
{
    DepthMapPipeNodeID nodeID = eDPETHMAP_PIPE_NODEID_P2AFM;
    sp<BaseBufferHandler> pBufferHandler = rEffReqPtr->getBufferHandler();

    // prepare FEO buffers, frame 2~5 has FE output
    IImageBuffer* feoBuf[6] = {NULL};
    pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_OUT_FE2BO, feoBuf[2]);
    pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_OUT_FE1BO, feoBuf[3]);
    pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_OUT_FE2CO, feoBuf[4]);
    pBufferHandler->getEnqueBuffer(nodeID, BID_P2AFM_OUT_FE1CO, feoBuf[5]);

    //--> frame 6: FM - L to R
    int iFrameNum = 6;
    // output: FMBO_LR
    IImageBuffer* pFMBOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FMBO_LR);

    rQFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_LSide[0]]).
            insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_RSide[0]]).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FMBO_LR), pFMBOBuf).
            insertTuningBuf(iFrameNum,(MVOID*) mFMTuningBufferMap.valueFor(iFrameNum));


    //--> frame 7: FM - R to L
    iFrameNum = 7;
    // output: FMBO_RL
    pFMBOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FMBO_RL);

    rQFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_RSide[0]]).
            insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_LSide[0]]).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FMBO_RL), pFMBOBuf).
            insertTuningBuf(iFrameNum,(MVOID*) mFMTuningBufferMap.valueFor(iFrameNum));

    //--> frame 8: FM - L to R
    iFrameNum = 8;
    // output: FMCO_LR
    pFMBOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FMCO_LR);

    rQFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_LSide[1]]).
            insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_RSide[1]]).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FMCO_LR), pFMBOBuf).
            insertTuningBuf(iFrameNum,(MVOID*) mFMTuningBufferMap.valueFor(iFrameNum));

    ///--> frame 9: FM - R to L
    iFrameNum = 9;
    // output: FMCO_LR
    pFMBOBuf = pBufferHandler->requestBuffer(nodeID, BID_P2AFM_OUT_FMCO_RL);

    rQFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_RSide[1]]).
            insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_LSide[1]]).
            insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_FMCO_RL), pFMBOBuf).
            insertTuningBuf(iFrameNum,(MVOID*) mFMTuningBufferMap.valueFor(iFrameNum));

    return MTRUE;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
DepthMapFlowOption_VSDOF::
queryReqAttrs(
    sp<DepthMapEffectRequest> pRequest,
    EffectRequestAttrs& rReqAttrs
)
{
    // deccide EIS on/off
    IMetadata* pInAppMeta;
    pRequest->getRequestMetadata({.bufferID=BID_META_IN_APP, .ioType=eBUFFER_IOTYPE_INPUT}
                                   , pInAppMeta);

    rReqAttrs.isEISOn = isEISOn(pInAppMeta);
    // decide OpState and BufferScenario
    IImageBuffer* pImgBuf_MV_F_CAP;
    MBOOL bRet= pRequest->getRequestImageBuffer({.bufferID=BID_P2AFM_OUT_MV_F_CAP,
                                                    .ioType=eBUFFER_IOTYPE_OUTPUT}, pImgBuf_MV_F_CAP);
    if(bRet)
    {
        rReqAttrs.opState = eSTATE_CAPTURE;
        rReqAttrs.bufferScenario = eBUFFER_POOL_SCENARIO_CAPTURE;
    }
    else
    {
        rReqAttrs.opState = eSTATE_NORMAL;
        if(rReqAttrs.isEISOn)
            rReqAttrs.bufferScenario = eBUFFER_POOL_SCENARIO_RECORD;
        else
            rReqAttrs.bufferScenario = eBUFFER_POOL_SCENARIO_PREVIEW;
    }

    return MTRUE;
}


MBOOL
DepthMapFlowOption_VSDOF::
queryPipeNodeBitSet(PipeNodeBitSet& nodeBitSet)
{
    nodeBitSet.reset();
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_P2AFM, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_N3D, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_DPE, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_WMF, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_OCC, 1);
    nodeBitSet.set(eDPETHMAP_PIPE_NODEID_FD, 1);
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
buildPipeGraph(DepthMapPipe* pPipe, const DepthMapPipeNodeMap& nodeMap)
{
    DepthMapPipeNode* pP2AFMNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_P2AFM);
    DepthMapPipeNode* pN3DNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_N3D);
    DepthMapPipeNode* pDPENode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_DPE);
    DepthMapPipeNode* pOCCNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_OCC);
    DepthMapPipeNode* pWMFNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_WMF);
    DepthMapPipeNode* pFDNode = nodeMap.valueFor(eDPETHMAP_PIPE_NODEID_FD);

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
    // P2AFM to FD
    CONNECT_DATA(P2AFM_TO_FD_IMG, *pP2AFMNode, *pFDNode);
    // DPE to OCC
    CONNECT_DATA(DPE_TO_OCC_MVSV_DMP_CFM, *pDPENode, *pOCCNode);
    // P2AFM to WMF
    CONNECT_DATA(P2AFM_TO_WMF_MY_SL, *pP2AFMNode, *pWMFNode);
    // WMF output
    CONNECT_DATA(WMF_OUT_DMW_MY_S, *pWMFNode, pPipe);
    CONNECT_DATA(WMF_OUT_DEPTHMAP, *pWMFNode, pPipe);
    // N3D to DPE/OCC
    CONNECT_DATA(N3D_TO_DPE_MVSV_MASK, *pN3DNode, *pDPENode);
    CONNECT_DATA(N3D_TO_OCC_LDC, *pN3DNode, *pOCCNode);
    CONNECT_DATA(N3D_OUT_JPS_WARPING_MATRIX, *pN3DNode, pPipe);
    // N3D to FD
    CONNECT_DATA(N3D_TO_FD_EXTDATA_MASK, *pN3DNode, *pFDNode);
    // Hal Meta frame output
    CONNECT_DATA(DEPTHMAP_META_OUT, *pN3DNode, pPipe);

    //OCC to WMF + OCC_output
    CONNECT_DATA(OCC_TO_WMF_DMH_MY_S, *pOCCNode, *pWMFNode);
    //FDNode output
    CONNECT_DATA(FD_OUT_EXTRADATA, *pFDNode, pPipe);

    pPipe->setRootNode(pP2AFMNode);

    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
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
DepthMapFlowOption_VSDOF::
getFDBufferSize()
{
    const P2AFMBufferSize& P2ASize = mSizeMgr.getP2AFM(eSTEREO_SCENARIO_RECORD);
    return P2ASize.mFD_IMG_SIZE;
}

MBOOL
DepthMapFlowOption_VSDOF::
getCaptureFDEnable()
{
    return MTRUE;
}

MBOOL
DepthMapFlowOption_VSDOF::
config3ATuningMeta(
    StereoP2Path path,
    MetaSet_T& rMetaSet
)
{
    // no need to manully control
    return MTRUE;
}

DepthMapBufferID
DepthMapFlowOption_VSDOF::
reMapBufferID(
    const EffectRequestAttrs& reqAttr,
    DepthMapBufferID bufferID
)
{
    // RectIn1 when capture use JPSMain1 as output buffer
    if(reqAttr.opState==eSTATE_CAPTURE &&
        bufferID == BID_P2AFM_OUT_RECT_IN1)
    {
        return BID_N3D_OUT_JPS_MAIN1;
    }

    return bufferID;
}


}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam
