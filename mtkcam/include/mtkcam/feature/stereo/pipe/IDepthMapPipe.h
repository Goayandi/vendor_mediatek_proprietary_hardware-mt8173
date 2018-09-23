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

#ifndef _MTK_CAMERA_FEATURE_PIPE_INTERFACE_DEPTH_MAP_PIPE_H_
#define _MTK_CAMERA_FEATURE_PIPE_INTERFACE_DEPTH_MAP_PIPE_H_

#include <utils/RefBase.h>
#include <mtkcam/def/common.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

#define VSDOF_PARAMS_METADATA_KEY "Metadata"

#define DEPTHMAP_COMPLETE_KEY "onComplete"
#define DEPTHMAP_FLUSH_KEY "onFlush"
#define DEPTHMAP_ERROR_KEY "onError"
#define DEPTHMAP_REQUEST_STATE_KEY "ReqStatus"
#define DEPTHMAP_REQUEST_TIMEKEY "TimeStamp"

typedef enum DepthMapNode_BufferDataTypes{
    BID_DEPTHMAPPIPE_INVALID = 0,
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
    // p2a output
    BID_P2AFM_OUT_FDIMG,
    BID_P2AFM_OUT_FE1BO,//10
    BID_P2AFM_OUT_FE2BO,
    BID_P2AFM_OUT_FE1CO,
    BID_P2AFM_OUT_FE2CO,
    BID_P2AFM_OUT_RECT_IN1,
    BID_P2AFM_OUT_RECT_IN2,
    BID_P2AFM_OUT_RECT_IN1_CAP,
    BID_P2AFM_OUT_RECT_IN2_CAP,
    BID_P2AFM_OUT_CC_IN1,
    BID_P2AFM_OUT_CC_IN2,
    BID_P2AFM_OUT_FMBO_LR,//20
    BID_P2AFM_OUT_FMBO_RL,
    BID_P2AFM_OUT_FMCO_LR,
    BID_P2AFM_OUT_FMCO_RL,
    BID_P2AFM_OUT_MV_F,
    BID_P2AFM_OUT_MV_F_CAP,

    // N3D output
    BID_N3D_OUT_MV_Y,
    BID_N3D_OUT_MASK_M,
    BID_N3D_OUT_SV_Y,
    BID_N3D_OUT_MASK_S,
    BID_N3D_OUT_LDC,//30
    BID_N3D_OUT_JPS_MAIN1,
    BID_N3D_OUT_JPS_MAIN2,
    // DPE output
    BID_DPE_OUT_CFM_L,
    BID_DPE_OUT_CFM_R,
    BID_DPE_OUT_DMP_L,
    BID_DPE_OUT_DMP_R,
    BID_DPE_OUT_RESPO_L,
    BID_DPE_OUT_RESPO_R,
    // OCC output
    BID_OCC_OUT_DMH,
    BID_OCC_OUT_MY_S,//40
    // WMF
    BID_WMF_OUT_DMW,
    BID_WMF_DMW_INTERNAL,
    // GF
    BID_GF_INTERAL_DEPTHMAP,
    BID_GF_OUT_DMBG,
    BID_GF_OUT_DEPTHMAP,
    // FD
    BID_FD_OUT_EXTRADATA,
    // metadata
    BID_META_IN_APP,
    BID_META_IN_HAL,
    BID_META_IN_HAL_MAIN2,
    BID_META_OUT_APP,
    BID_META_OUT_HAL,
    // queued meta
    BID_META_IN_APP_QUEUED,
    BID_META_IN_HAL_QUEUED,
    BID_META_IN_HAL_MAIN2_QUEUED,
    BID_META_OUT_APP_QUEUED,
    BID_META_OUT_HAL_QUEUED,

#ifdef GTEST
    // UT output
    BID_FE2_HWIN_MAIN1,
    BID_FE2_HWIN_MAIN2,
    BID_FE3_HWIN_MAIN1,
    BID_FE3_HWIN_MAIN2,
#endif

    // alias section(must be placed bottom)
    BID_TO_GF_MY_SLL = BID_P2AFM_FE1B_INPUT,
    BID_TO_GF_MY_SL = BID_P2AFM_FE1C_INPUT,
    BID_P2AFM_OUT_MY_S = BID_OCC_OUT_MY_S

} DepthMapBufferID;

typedef enum eDepthNodeOpState {
    eSTATE_NORMAL = 1,
    eSTATE_CAPTURE = 2
} DepthNodeOpState;


typedef enum eDepthFlowType {
    eDEPTH_FLOW_TYPE_STANDARD,
    eDEPTH_FLOW_TYPE_QUEUED_DEPTH
} DepthFlowType;

const MUINT8 DEPTH_FLOW_QUEUE_SIZE = 3;

const DepthMapBufferID INPUT_METADATA_BID_LIST[] = {BID_META_IN_APP, BID_META_IN_HAL, BID_META_IN_HAL_MAIN2};
const DepthMapBufferID OUTPUT_METADATA_BID_LIST[] = {BID_META_OUT_APP, BID_META_OUT_HAL};

/******************************************************************************
 * The Interface of DepthMapNode's FeaturePipe
 ******************************************************************************/
class IDepthMapPipe
{
public:
    /**
     * @brief DepthMapPipe creation
     * @param [in] flowType controlling depth flow
     * @param [in] iSensorIdx_Main1  sensor index of main1
     * @param [in] iSensorIdx_Main2  sensor index of main2
     * @return
     * - MTRUE indicates success
     * - MFALSE indicates failure
     */
    static IDepthMapPipe* createInstance(
                            DepthFlowType flowType,
                            MINT32 iSensorIdx_Main1,
                            MINT32 iSensorIdx_Main2);
    MBOOL destroyInstance();
    // destructor
    virtual ~IDepthMapPipe() {};
public:
    virtual MBOOL init() = 0;
    virtual MBOOL uninit() = 0;
    virtual MBOOL enque(android::sp<EffectRequest>& request) = 0;
    virtual MVOID flush() = 0;
    virtual MVOID setFlushOnStop(MBOOL flushOnStop) = 0;
};

}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam

#endif