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
 * @file DepthMapFlowOption_VSDOF.h
 * @brief DepthMap Flow option template for VSDOF
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_FLOWOPTION_DEPTHMAPNODE_VSDOF_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_FLOWOPTION_DEPTHMAPNODE_VSDOF_H_

// Standard C header file

// Android system/core header file
#include <utils/KeyedVector.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
// Module header file
#include <bufferPoolMgr/NodeBufferPoolMgr_VSDOF.h>
// Local header file
#include "DepthMapFlowOption.h"


/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using NSCam::NSIoPipe::NSPostProc::QParams;
/*******************************************************************************
* Structure Define
********************************************************************************/

/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * @class DepthMapFlowOption_VSDOF
 * @brief flow optione class for VSDOF
 */
class DepthMapFlowOption_VSDOF : public DepthMapFlowOption
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    DepthMapFlowOption_VSDOF(const DepthMapPipeSetting& setting);
    virtual ~DepthMapFlowOption_VSDOF();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  P2AFMFlowOption Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual NSCam::NSIoPipe::PortID mapToPortID(DepthMapBufferID buffeID);

    virtual MBOOL buildQParam(
                    DepthMapRequestPtr pReq,
                    const Stereo3ATuningRes& tuningResult,
                    QParams& rOutParam);


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MBOOL init();
    virtual MBOOL queryReqAttrs(
                        sp<DepthMapEffectRequest> pRequest,
                        EffectRequestAttrs& rReqAttrs);

    virtual MBOOL queryPipeNodeBitSet(PipeNodeBitSet& nodeBitSet);

    virtual MBOOL buildPipeGraph(
                        DepthMapPipe* pPipe,
                        const DepthMapPipeNodeMap& nodeMap);

    virtual MBOOL checkConnected(
                        DepthMapDataID dataID);

    virtual MSize getFDBufferSize();

    virtual MBOOL getCaptureFDEnable();

    virtual MBOOL config3ATuningMeta(
                        StereoP2Path path,
                        MetaSet_T& rMetaSet);

    virtual DepthMapBufferID reMapBufferID(
                        const EffectRequestAttrs& reqAttr,
                        DepthMapBufferID bufferID
                        );
    /**
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    /**
     * @brief prepare the SRZ/FEFM/QParams tuning templates
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL prepareTemplateParams();

    /**
     * @brief generate QParams template for the operation state
     * @param [in] opState DepthMapNode Operation state
     * @param [out] rQTmplate QParams template for this operation state
     * @return
     * - requested buffer from buffer pool
     */
    MBOOL generateQParamTemplate(
                            DepthMapPipeOpState opState,
                            QParamTemplate& rQTmplate);

    /**
     * @brief prepare the QParams template for NORMAL operation state
     * @param [in] state DepthMap Pipe OP state
     * @param [in] iModuleTrans module rotation
     * @param [out] rQTmplate QParams template
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL prepareBurstQParamsTemplate(
                        DepthMapPipeOpState state,
                        MINT32 iModuleTrans,
                        QParamTemplate& rQTmplate);

    /**
     * @brief QParams template preparation function for each frame
     */
    virtual MBOOL prepareQParam_frame0(
                                MINT32 iModuleTrans,
                                QParamTemplate& rQTmplate);
    virtual MBOOL prepareQParam_frame2(QParamTemplate& rQTmplate);
    virtual MBOOL prepareQParam_frame3(QParamTemplate& rQTmplate);
    virtual MBOOL prepareQParam_frame4(QParamTemplate& rQTmplate);
    virtual MBOOL prepareQParam_frame5(QParamTemplate& rQTmplate);
    virtual MBOOL prepareQParam_frame6_9(QParamTemplate& rQTmplate);

    // frames: 1 are differnt between OpState
    virtual MBOOL prepareQParam_frame1(
                        DepthMapPipeOpState state,
                        MINT32 iModuleTrans,
                        QParamTemplate& rQTmplate);
    /**
     * @brief build the corresponding QParams in runtime of each OpState
     * @param [in] rEffReqPtr DepthMapRequestPtr
     * @param [in] tuningResult 3A tuning Result
     * @param [out] rOutParm QParams to enque into P2
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL buildQParams_NORMAL(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParams& rOutParm);

    virtual MBOOL buildQParams_CAPTURE(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParams& rOutParm);
    /**
     * @brief build QParams template for frame 0~9
     */
    virtual MBOOL buildQParam_frame0(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame1(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame2(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame3(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame4(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame5(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame6to9(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);

    // buildQParam for capture scenario
    virtual MBOOL buildQParam_frame1_capture(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame2_capture(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    virtual MBOOL buildQParam_frame3_capture(
                    DepthMapRequestPtr& rEffReqPtr,
                    const Stereo3ATuningRes& tuningResult,
                    QParamTemplateFiller& rQFiller);
    /**
     * @brief setup the FE/FM tuning buffer
     */
    MVOID setupFETuningSetting(dip_x_reg_t* pTuningBuf, MUINT iBlockSize);
    MVOID setupFMTuningSetting(dip_x_reg_t* pTuningBuf, MUINT iFrameID);
    /**
     * @brief Calaulate the crop region according to the screen ratio
     * @param [in] pImgBuf image buffer to crop
     * @param [out] rCropStartPt start point of the crop region
     * @param [out] rCropSize crop region size
     * @return operation result
     */
    MBOOL calCropForScreen(
                    IImageBuffer* pImgBuf,
                    MPoint &rCropStartPt,
                    MSize& rCropSize);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Public Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapFlowOption Protected Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    MUINT32 miSensorIdx_Main1;
    MUINT32 miSensorIdx_Main2;
    //
    NodeBufferSizeMgr mSizeMgr;
    // QParams template for normal OpState
    QParamTemplate mQParamTemplate_NORMAL;
    QParamTemplate mQParamTemplate_CAPTURE;

    // valid DepthMapDataID vector
    KeyedVector<DepthMapDataID, MBOOL> mvAllowDataIDMap;
    // SRZ CROP/RESIZE template, frameID to SRZInfo map
    KeyedVector<MUINT, _SRZ_SIZE_INFO_*> mSrzSizeTemplateMap;
    // FE Tuning Buffer Map, key=stage, value= tuning buffer
    KeyedVector<MUINT, dip_x_reg_t*> mFETuningBufferMap;
    // FM Tuning Buffer Map, key=frame ID, value= tuning buffer
    KeyedVector<MUINT, dip_x_reg_t*> mFMTuningBufferMap;
    // Record the frame index of the L/R-Side which is defined by ALGO.
    // L-Side could be main1 or main2 which can be identified by the sensors' locations.
    // Frame 0,3,5 is main1, 1,2,6 is main2.
    int frameIdx_LSide[2];
    int frameIdx_RSide[2];
};

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif