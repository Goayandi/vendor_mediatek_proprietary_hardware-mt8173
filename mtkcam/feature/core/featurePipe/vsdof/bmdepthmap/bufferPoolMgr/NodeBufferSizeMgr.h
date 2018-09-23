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
 * @file NodeBufferSizeMgr.h
 * @brief BufferPoolMgr for Stereo features, ex.VSDOF
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_BUFFERSIZE_MGR_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_BUFFERSIZE_MGR_H_

// Standard C header file

// Android system/core header file
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>
// Module header file
#include <mtkcam/feature/stereo/hal/stereo_common.h>
// Local header file

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using android::sp;
using namespace StereoHAL;

/*******************************************************************************
* Struct Definition
********************************************************************************/

/*******************************************************************************
* Type Definition
********************************************************************************/

/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * @class NodeBufferSizeMgr
 * @brief control all the buffer size in VSDOF
 */
class NodeBufferSizeMgr
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual ~NodeBufferSizeMgr();
    NodeBufferSizeMgr();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_VSDOF Public Structure
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    struct BaseBufferSize
    {
        BaseBufferSize()
        : mScenario(eSTEREO_SCENARIO_RECORD) {}

        BaseBufferSize(ENUM_STEREO_SCENARIO scenario)
        : mScenario(scenario) {}
        // scenario
        ENUM_STEREO_SCENARIO  mScenario;
    };

    struct P2AFMBufferSize : public BaseBufferSize
    {
        P2AFMBufferSize() {}
        P2AFMBufferSize(ENUM_STEREO_SCENARIO scenario);
        MVOID debug() const;
        // size
        MSize mFD_IMG_SIZE;
        MSize mFEB_INPUT_SIZE_MAIN1;
        MSize mFEB_INPUT_SIZE_MAIN2;
        MSize mFEC_INPUT_SIZE_MAIN1;
        MSize mFEC_INPUT_SIZE_MAIN2;
        MSize mRECT_IN_SIZE_MAIN1;
        MSize mRECT_IN_SIZE_MAIN2;
        MSize mRECT_IN_CONTENT_SIZE;
        MSize mMAIN_IMAGE_SIZE;
        MSize mCCIN_SIZE;
        // area
        StereoArea mFEAO_AREA_MAIN2;
        StereoArea mFEBO_AREA_MAIN2;
        StereoArea mFECO_AREA_MAIN2;
    };

    struct N3DBufferSize : public BaseBufferSize
    {
        N3DBufferSize() {}
        N3DBufferSize(ENUM_STEREO_SCENARIO scenario);
        MVOID debug() const;
        // buffer size const
        MSize mWARP_IMG_SIZE;
        MSize mWARP_MASK_SIZE;
        MSize mLDC_SIZE;
    };

    struct DPEBufferSize : public BaseBufferSize
    {
        DPEBufferSize() {}
        DPEBufferSize(ENUM_STEREO_SCENARIO scenario);
        MVOID debug() const;
        // size
        MSize mDMP_SIZE;
        MSize mCFM_SIZE;
        MSize mRESPO_SIZE;
        // target spec size
        MUINT32 mTARGET_IMG_WIDTH;
        MUINT32 mTARGET_IMG_HEIGHT;
    };

    struct OCCBufferSize : public BaseBufferSize
    {
        OCCBufferSize() {}
        OCCBufferSize(ENUM_STEREO_SCENARIO scenario);
        MVOID debug() const;
        //
        MSize mDMH_SIZE;
        MSize mMYS_SIZE;
    };

    struct WMFBufferSize : public BaseBufferSize
    {
        WMFBufferSize() {}
        WMFBufferSize(ENUM_STEREO_SCENARIO scenario);
        MVOID debug() const;
        //
        MSize mDMW_SIZE;
        MSize mDMW_DEPTHMAP_SIZE;
    };
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_VSDOF Public Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    const P2AFMBufferSize& getP2AFM(ENUM_STEREO_SCENARIO scen) {return mP2ASize.valueFor(scen);}
    const N3DBufferSize& getN3D(ENUM_STEREO_SCENARIO scen) {return mN3DSize.valueFor(scen);}
    const DPEBufferSize& getDPE(ENUM_STEREO_SCENARIO scen) {return mDPESize.valueFor(scen);}
    const OCCBufferSize& getOCC(ENUM_STEREO_SCENARIO scen) {return mOCCSize.valueFor(scen);}
    const WMFBufferSize& getWMF(ENUM_STEREO_SCENARIO scen) {return mWMFSize.valueFor(scen);}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_VSDOF Public Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    KeyedVector<ENUM_STEREO_SCENARIO, P2AFMBufferSize> mP2ASize;
    KeyedVector<ENUM_STEREO_SCENARIO, N3DBufferSize> mN3DSize;
    KeyedVector<ENUM_STEREO_SCENARIO, DPEBufferSize> mDPESize;
    KeyedVector<ENUM_STEREO_SCENARIO, OCCBufferSize> mOCCSize;
    KeyedVector<ENUM_STEREO_SCENARIO, WMFBufferSize> mWMFSize;
};

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif
