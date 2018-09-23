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
 * @file P2AFMFlowOption.h
 * @brief Flow option template for P2AFMNode
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_FLOWOPTION_P2AFMNODE_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_FLOWOPTION_P2AFMNODE_H_

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <mtkcam/aaa/aaa_hal_common.h>
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>
// Module header file
// Local header file
#include "../DepthMapEffectRequest.h"
#include "../DepthMapPipeUtils.h"

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using namespace NS3Av3;

/*******************************************************************************
* Const Definition
********************************************************************************/

// DMP Port - input
static const NSCam::NSIoPipe::PortID PORT_DEPI( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_DEPI, 0);
static const NSCam::NSIoPipe::PortID PORT_DMGI( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_DMGI , 0);
static const NSCam::NSIoPipe::PortID PORT_IMGI( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_IMGI, 0);
// DMP Port -  output
static const NSCam::NSIoPipe::PortID PORT_IMG2O( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_IMG2O, 1);
static const NSCam::NSIoPipe::PortID PORT_IMG3O( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_IMG3O, 1);
static const NSCam::NSIoPipe::PortID PORT_WDMA( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_WDMAO, 1);
static const NSCam::NSIoPipe::PortID PORT_WROT( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_WROTO, 1);
static const NSCam::NSIoPipe::PortID PORT_FEO( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_FEO, 1);
static const NSCam::NSIoPipe::PortID PORT_MFBO( NSIoPipe::EPortType_Memory, NSImageio::NSIspio::EPortIndex_MFBO, 1);

/*******************************************************************************
* Struct Definition
********************************************************************************/
struct AAATuningResult
{
    TuningParam tuningResult;
    MVOID*  tuningBuffer;
};

struct Stereo3ATuningRes
{
    AAATuningResult tuningRes_main1;
    AAATuningResult tuningRes_main2;
};

/*******************************************************************************
* Class Definition
********************************************************************************/
/**
 * @class P2AFMFlowOption
 * @brief Flow option template class for P2AFM Node
 */

class P2AFMFlowOption
{
public:
    P2AFMFlowOption() {}
    virtual ~P2AFMFlowOption() {}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  P2AFMFlowOption Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief map buffer id to its P2 output port
     * @param [in] buffeID DepthMapNode buffer id
     * @return
     * - DMA output port id
     */
    virtual NSCam::NSIoPipe::PortID mapToPortID(DepthMapBufferID buffeID) = 0;

    /**
     * @brief build the enque QParams
     * @param [in] pReq DepthMap EffectRequest
     * @param [in] tuningResult tuning result after applied the 3A setIsp
     * @param [out] rOutParam output QParams
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL buildQParam(
                    sp<DepthMapEffectRequest> pReq,
                    const Stereo3ATuningRes& tuningResult,
                    QParams& rOutParam) = 0;

}; //P2AFMFlowOption

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif
