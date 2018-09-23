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
 * @file IBMDepthMapPipe.h
 * @brief DepthMap feature pipe interfaces, classes and settings
 */

#ifndef _MTK_CAMERA_FEATURE_PIPE_INTERFACE_BMDEPTH_MAP_PIPE_H_
#define _MTK_CAMERA_FEATURE_PIPE_INTERFACE_BMDEPTH_MAP_PIPE_H_

// Standard C header file

// Android system/core header file
#include <utils/RefBase.h>

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>

// Module header file

// Local header file

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

#define DEPTHMAP_META_KEY_STRING "Metadata"
#define DEPTHMAP_COMPLETE_KEY "onComplete"
#define DEPTHMAP_FLUSH_KEY "onFlush"
#define DEPTHMAP_ERROR_KEY "onError"

/*******************************************************************************
* Enum Definition
********************************************************************************/

typedef enum DepthMapNode_BufferDataTypes{
    BID_INVALID = 0,
    // P2A input raw
    BID_P2AFM_IN_FSRAW1,
    BID_P2AFM_IN_FSRAW2,
    BID_P2AFM_IN_RSRAW1,
    BID_P2AFM_IN_RSRAW2,
    // internal P2A buffers
    BID_P2AFM_FE1B_INPUT,
    BID_P2AFM_FE2B_INPUT,
    BID_P2AFM_FE1C_INPUT,
    BID_P2AFM_FE2C_INPUT,
    BID_P2AFM_TUNING,
    // P2A output
    BID_P2AFM_OUT_FDIMG, //10
    BID_P2AFM_OUT_FE1AO,
    BID_P2AFM_OUT_FE2AO,
    BID_P2AFM_OUT_FE1BO,
    BID_P2AFM_OUT_FE2BO,
    BID_P2AFM_OUT_FE1CO,
    BID_P2AFM_OUT_FE2CO,
    BID_P2AFM_OUT_RECT_IN1,
    BID_P2AFM_OUT_RECT_IN2,
    BID_P2AFM_OUT_MV_F,
    BID_P2AFM_OUT_MV_F_CAP, //20
    BID_P2AFM_OUT_CC_IN1,
    BID_P2AFM_OUT_CC_IN2,
    BID_P2AFM_OUT_FMAO_LR,
    BID_P2AFM_OUT_FMAO_RL,
    BID_P2AFM_OUT_FMBO_LR,
    BID_P2AFM_OUT_FMBO_RL,
    BID_P2AFM_OUT_FMCO_LR,
    BID_P2AFM_OUT_FMCO_RL,
    // N3D output
    BID_N3D_OUT_MV_Y,
    BID_N3D_OUT_MASK_M,
    BID_N3D_OUT_SV_Y,
    BID_N3D_OUT_MASK_S,
    BID_N3D_OUT_LDC,
    BID_N3D_OUT_JPS_MAIN1,
    BID_N3D_OUT_JPS_MAIN2,
    BID_N3D_OUT_WARPING_MATRIX,
    // DPE output
    BID_DPE_OUT_CFM_L,
    BID_DPE_OUT_CFM_R,
    BID_DPE_OUT_DMP_L,
    BID_DPE_OUT_DMP_R,
    BID_DPE_OUT_RESPO_L,
    BID_DPE_OUT_RESPO_R,
    // Last DMP
    BID_DPE_INTERNAL_LAST_DMP,
    // OCC output
    BID_OCC_OUT_DMH,
    BID_OCC_OUT_MY_S,
    // WMF output
    BID_WMF_OUT_DEPTHMAP,
    BID_WMF_OUT_DMW,
    // WMF internal
    BID_WMF_DMW_INTERNAL,
    BID_WMF_DEPTHMAP_INTERNAL,
    BID_WMF_DEPTHMAP_ROTATION,
    // FD output
    BID_FD_OUT_EXTRADATA,
    // metadata
    BID_META_IN_APP,
    BID_META_IN_HAL_MAIN1,
    BID_META_IN_HAL_MAIN2,
    BID_META_OUT_APP,
    BID_META_OUT_HAL,

    #ifdef GTEST
    // UT output
    BID_FE2_HWIN_MAIN1,
    BID_FE2_HWIN_MAIN2,
    BID_FE3_HWIN_MAIN1,
    BID_FE3_HWIN_MAIN2,
#endif

    //alias section
    BID_P2AFM_OUT_MY_SL = BID_P2AFM_FE1C_INPUT


} DepthMapBufferID;

/**
 * @brief Operation Status of EffectRequest
 */
typedef enum eDepthMapPipeOpState {
    eSTATE_NORMAL = 1,
    eSTATE_CAPTURE = 2
} DepthMapPipeOpState;

/**
 * @brief Sensor type config
 */
typedef enum eStereoSensorState {
    eSENSOR_BAYER_BAYER,     /*!< Bayer+Bayer sensor */
    eSENSOR_BAYER_MONO       /*!< Bayer+Mono sensor */
} StereoSensorState;

/**
 * @brief DepthMapNode supoorted feature flow/mode
 */
typedef enum eDepthNodeFeatureMode {
    eDEPTHNODE_MODE_VSDOF,
    eDEPTHNODE_MODE_DENOISE,
    eDEPTHNODE_MODE_VSDOF_PLUS_DENOISE
} DepthNodeFeatureMode;

/*******************************************************************************
* Struct Definition
********************************************************************************/
/**
 * @brief DepthMapPipe functional option.
 */

struct DepthMapPipeOption
{
    StereoSensorState sensorState;      /*!< Indicate the current sensor config*/
    DepthNodeFeatureMode featureMode;   /*!< Indicate the feature mode`*/
};

/**
 * @brief DepthMapPipe config setting
 */

struct DepthMapPipeSetting
{
    MUINT32 miSensorIdx_Main1;
    MUINT32 miSensorIdx_Main2;
};

/*******************************************************************************
* Class Definition
********************************************************************************/
class IDepthMapEffectRequest;
/**
 * @class IDepthMapPipe
 * @brief DepthMapPipe interface
 */
class IDepthMapPipe
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief create a instance of DepthMapPipe
     * @param [in] setting DepthMapPipe control setting
     * @param [in] pipeOption the configurable settings of DepthMapPipe
     * @return
     * - instance of DepthMapPipe
     */
    static IDepthMapPipe* createInstance(DepthMapPipeSetting setting, DepthMapPipeOption pipeOption);
    MBOOL destroyInstance();
    /**
     * @brief DepthMapPipe destructor
     */
    virtual ~IDepthMapPipe() {};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipe Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief perform initial operation
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL init() = 0;
    /**
     * @brief perform uninitial operation
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL uninit() = 0;
    /**
     * @brief perform enque operation
     * @param [in] request to-be-performed EffectRequest
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL enque(android::sp<IDepthMapEffectRequest>& request) = 0;
    /**
     * @brief Flush all the operations inside the feature pipe
     */
    virtual MVOID flush() = 0;
};

}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam

#endif