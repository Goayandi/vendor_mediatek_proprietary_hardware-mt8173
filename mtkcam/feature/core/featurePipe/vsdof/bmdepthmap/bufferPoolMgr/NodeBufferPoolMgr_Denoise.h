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
 * @file NodeBufferPoolMgr_Denoise.h
 * @brief BufferPoolMgr for VSDOF
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_DENOISE_BUFFERPOOL_MGR_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_DENOISE_BUFFERPOOL_MGR_H_

// Standard C header file

// Android system/core header file
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>
// Module header file

// Local header file
#include "BaseBufferPoolMgr.h"
#include "NodeBufferSizeMgr.h"
#include "../DepthMapPipe_Common.h"
/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using android::sp;

/*******************************************************************************
* Struct Definition
********************************************************************************/

/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * @class NodeBufferPoolMgr_Denoise
 * @brief Base class of Denoise
 */
class NodeBufferPoolMgr_Denoise : public BaseBufferPoolMgr
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    NodeBufferPoolMgr_Denoise();
    virtual ~NodeBufferPoolMgr_Denoise();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BaseBufferPoolMgr Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual SmartImageBuffer request(
                            DepthMapBufferID id,
                            BufferPoolScenario scen
                            );
    virtual SmartGraphicBuffer requestGB(
                            DepthMapBufferID id,
                            BufferPoolScenario scen
                            );

    virtual SmartTuningBuffer requestTB(
                            DepthMapBufferID id,
                            BufferPoolScenario scen
                            );

    virtual sp<BaseBufferHandler> createBufferPoolHandler();

    virtual MBOOL queryBufferType(
                            DepthMapBufferID bid,
                            BufferPoolScenario scen,
                            DepthBufferType& rOutBufType);

    virtual sp<ImageBufferPool> getImageBufferPool(DepthMapBufferID bufferID);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_Denoise Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_Denoise Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    /**
     * @brief query the FEO buffer size from the input buffer size and block size
     * @param [in] iBufSize fe input buffer size
     * @param [in] iBlockSize FE block size
     * @param [out] rFEOSize FEO buffer size
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL queryFEOBufferSize(MSize iBufSize, MUINT iBlockSize, MSize& rFEOSize);

    /**
     * @brief query the FMO buffer size from the input FEO buffer size and block size
     * @param [in] szFEOBuffer feo buffer size
     * @param [out] rFMOSize FMO buffer size
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL queryFMOBufferSize(MSize szFEOBuffer, MSize& rFMOSize);

    /**
     * @brief initialize all buffer pool
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL initializeBufferPool();

    /**
     * @brief initialize node's buffer pools
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL initP2ABufferPool();
    MBOOL initN3DBufferPool();
    MBOOL initDPEBufferPool();

    /**
     * @brief initialize FEO/FMO buffer pools
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL initFEFMBufferPool();

    /**
     * @brief build the mapping between buffer id and image buffer pool
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL buildImageBufferPoolMap();
    /**
     * @brief build the buffer ID Map which shows the buffers are all
     *        inside the EffectRequest.
     * @param [out] bidToScneMap bufferID to scenario Map.
     *                if scenaio is eBUFFER_POOL_SCENARIO_PREVIEW
     *                  -> buffer is inside the EffectRequest for preview
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL buildRequestBufferMap();

    /**
     * @brief build the mapping between buffer id and its buffer type with specific scenario
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL buildBufScenarioToTypeMap();
    /**
     * @brief uninit function
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL uninit();


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_Denoise Public Member.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_Denoise Private Member.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    // buffer id to image buffer pool map
    KeyedVector<DepthMapBufferID, sp<ImageBufferPool> > mBIDtoImgBufPoolMap_Capture;
    // buffer id to graphic buffer pool map
    KeyedVector<DepthMapBufferID, sp<GraphicBufferPool> > mBIDtoGraphicBufPoolMap_Capture;

    typedef KeyedVector<BufferPoolScenario, DepthBufferType> BufScenarioToTypeMap;
    // Define the buffer type of all working buffers in each scenario inside this mgr
    KeyedVector<DepthMapBufferID, BufScenarioToTypeMap> mBIDToScenarioTypeMap;
    // size mgr
    NodeBufferSizeMgr mBufferSizeMgr;
    // requst bufffer map
    RequestBufferMap mRequestBufferIDMap;
    RequestBufferMap mRequestMetaIDMap;
    // ++++++++++++++++++++ Buffer Pools ++++++++++++++++++++++++++++++++//
    //----------------------P2AFM section--------------------------------//
    // Rectify_in/CC_in Buffer
    sp<GraphicBufferPool> mpRectInBufPool_CAP;
    sp<ImageBufferPool> mpCCInBufPool;
    // FEO buffer
    sp<ImageBufferPool> mpFEOB_BufPool;
    sp<ImageBufferPool> mpFEOC_BufPool;
    sp<ImageBufferPool> mpFMOB_BufPool;
    sp<ImageBufferPool> mpFMOC_BufPool;
    // FE Input Buffer
    sp<ImageBufferPool> mpFEBInBufPool_Main1;
    sp<ImageBufferPool> mpFEBInBufPool_Main2;
    sp<ImageBufferPool> mpFECInBufPool_Main1;
    sp<ImageBufferPool> mpFECInBufPool_Main2;
    // tuning buffer
    sp<TuningBufferPool> mpTuningBufferPool;
    #ifdef GTEST
    // For UT requirement - dump fe input buffer output at IMG3O
    sp<ImageBufferPool> mFEHWInput_StageB_Main1;
    sp<ImageBufferPool> mFEHWInput_StageB_Main2;
    sp<ImageBufferPool> mFEHWInput_StageC_Main1;
    sp<ImageBufferPool> mFEHWInput_StageC_Main2;
    #endif
    //----------------------N3D section--------------------------------//
    sp<ImageBufferPool> mN3DImgBufPool;
    sp<ImageBufferPool> mN3DMaskBufPool;
    //----------------------DPE section--------------------------------//
    sp<ImageBufferPool> mDMPBuffPool;
    sp<ImageBufferPool> mCFMBuffPool;
    sp<ImageBufferPool> mRespBuffPool;

};


}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif
