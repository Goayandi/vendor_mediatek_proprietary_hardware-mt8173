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
 * @file NodeBufferPoolMgr.cpp
 * @brief BufferPoolMgr for VSDOF
*/

// Standard C header file

// Android system/core header file
#include <ui/gralloc_extra.h>
// mtkcam custom header file

// mtkcam global header file
#include <drv/isp_reg.h>
#include <mtkcam/drv/def/dpecommon.h>
#include <mtkcam/drv/iopipe/PostProc/DpeUtility.h>
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
// Module header file

// Local header file
#include "NodeBufferHandler.h"
#include "NodeBufferSizeMgr.h"
#include "../DepthMapPipe_Common.h"
#include "../bufferConfig/BufferConfig_VSDOF.h"
// Logging header file
#define PIPE_CLASS_TAG "NodeBufferSizeMgr"
#include <featurePipe/core/include/PipeLog.h>

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using namespace NSCam::NSIoPipe;
/*******************************************************************************
* Global Define
********************************************************************************/
#define DEBUG_MSIZE(sizeCons) \
    MY_LOGD("size: " #sizeCons " : %dx%d\n", sizeCons.w, sizeCons.h);

#define DEBUG_AREA(areaCons) \
    MY_LOGD("Area: " #areaCons " : size=%dx%d padd=%dx%d startPt=(%d, %d)\n", areaCons.size.w, areaCons.size.h, areaCons.padding.w, areaCons.padding.h, areaCons.startPt.x, areaCons.startPt.x);
/*******************************************************************************
* External Function
********************************************************************************/

/*******************************************************************************
* Enum Define
********************************************************************************/


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferSizeMgr::P2AFMBufferSize class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferSizeMgr::P2AFMBufferSize::
P2AFMBufferSize(ENUM_STEREO_SCENARIO scenario)
: BaseBufferSize(scenario)
{
    MY_LOGD("+");
    StereoSizeProvider* pSizePrvder = StereoSizeProvider::getInstance();

    Pass2SizeInfo pass2SizeInfo;
    pSizePrvder->getPass2SizeInfo(PASS2A_P, scenario, pass2SizeInfo);
    mFEB_INPUT_SIZE_MAIN2 = pass2SizeInfo.areaWROT;
    mFEAO_AREA_MAIN2 = pass2SizeInfo.areaFEO;

    // frame 1
    pSizePrvder->getPass2SizeInfo(PASS2A, scenario, pass2SizeInfo);
    mFD_IMG_SIZE = pass2SizeInfo.areaIMG2O;
    mMAIN_IMAGE_SIZE = pass2SizeInfo.areaWDMA.size;
    mFEB_INPUT_SIZE_MAIN1 = pass2SizeInfo.areaWROT;

    // frame 2
    pSizePrvder->getPass2SizeInfo(PASS2A_P_2, scenario, pass2SizeInfo);
    mFEC_INPUT_SIZE_MAIN2 = pass2SizeInfo.areaIMG2O;
    mRECT_IN_SIZE_MAIN2 = pass2SizeInfo.areaWDMA.size;
    mFEBO_AREA_MAIN2 = pass2SizeInfo.areaFEO;

    // frame 3
    pSizePrvder->getPass2SizeInfo(PASS2A_2, scenario, pass2SizeInfo);
    mFEC_INPUT_SIZE_MAIN1 = pass2SizeInfo.areaIMG2O;
    mRECT_IN_SIZE_MAIN1 = pass2SizeInfo.areaWDMA.size;
    mRECT_IN_CONTENT_SIZE = pass2SizeInfo.areaWDMA.size - pass2SizeInfo.areaWDMA.padding;

    // frame 4
    pSizePrvder->getPass2SizeInfo(PASS2A_P_3, scenario, pass2SizeInfo);
    mFECO_AREA_MAIN2 = pass2SizeInfo.areaFEO;
    // cc_in
    mCCIN_SIZE = pass2SizeInfo.areaIMG2O;

    debug();
    MY_LOGD("-");
}

MVOID
NodeBufferSizeMgr::P2AFMBufferSize::
debug() const
{
    MY_LOGD("P2AFM debug size: scenario=%d, ", mScenario);

    DEBUG_MSIZE(mFD_IMG_SIZE);
    DEBUG_MSIZE(mFEB_INPUT_SIZE_MAIN1);
    DEBUG_MSIZE(mFEB_INPUT_SIZE_MAIN2);
    DEBUG_MSIZE(mFEC_INPUT_SIZE_MAIN1);
    DEBUG_MSIZE(mFEC_INPUT_SIZE_MAIN2);
    DEBUG_MSIZE(mRECT_IN_SIZE_MAIN1);
    DEBUG_MSIZE(mRECT_IN_SIZE_MAIN2);
    DEBUG_MSIZE(mRECT_IN_CONTENT_SIZE);
    DEBUG_MSIZE(mMAIN_IMAGE_SIZE);
    DEBUG_MSIZE(mCCIN_SIZE);

    DEBUG_AREA(mFEAO_AREA_MAIN2);
    DEBUG_AREA(mFEBO_AREA_MAIN2);
    DEBUG_AREA(mFECO_AREA_MAIN2);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferSizeMgr::N3DBufferSize class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferSizeMgr::N3DBufferSize::
N3DBufferSize(ENUM_STEREO_SCENARIO scenario)
: BaseBufferSize(scenario)
{
   MY_LOGD("+");
    // setup constants
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    mWARP_IMG_SIZE = pSizeProvder->getBufferSize(E_MV_Y, scenario);
    mWARP_MASK_SIZE = pSizeProvder->getBufferSize(E_MASK_M_Y, scenario);
    // LDC
    mLDC_SIZE = pSizeProvder->getBufferSize(E_LDC, scenario);
    debug();
    MY_LOGD("-");
}


MVOID
NodeBufferSizeMgr::N3DBufferSize::
debug() const
{
    MY_LOGD("N3D debug size: scenario=%d, ", mScenario);

    DEBUG_MSIZE(mWARP_IMG_SIZE);
    DEBUG_MSIZE(mWARP_MASK_SIZE);
    DEBUG_MSIZE(mLDC_SIZE);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferSizeMgr::DPEBufferSize class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferSizeMgr::DPEBufferSize::
DPEBufferSize(ENUM_STEREO_SCENARIO scenario)
: BaseBufferSize(scenario)
{
    MY_LOGD("+");
    // setup constans
    StereoSizeProvider* pSizePrvder = StereoSizeProvider::getInstance();
    mDMP_SIZE = pSizePrvder->getBufferSize(E_DMP_H, scenario);
    mCFM_SIZE = pSizePrvder->getBufferSize(E_CFM_H, scenario);
    mRESPO_SIZE = pSizePrvder->getBufferSize(E_RESPO, scenario);

    // original image width is the size before N3D in Cap
    Pass2SizeInfo pass2SizeInfo;
    pSizePrvder->getPass2SizeInfo(PASS2A_P_2, eSTEREO_SCENARIO_CAPTURE, pass2SizeInfo);
    mTARGET_IMG_WIDTH = pass2SizeInfo.areaWDMA.size.w;
    mTARGET_IMG_HEIGHT = pass2SizeInfo.areaWDMA.size.h;


    debug();
    MY_LOGD("+");
}


MVOID
NodeBufferSizeMgr::DPEBufferSize::
debug() const
{
    MY_LOGD("DPE debug size: scenario=%d, ", mScenario);

    DEBUG_MSIZE(mDMP_SIZE);
    DEBUG_MSIZE(mCFM_SIZE);
    DEBUG_MSIZE(mRESPO_SIZE);

    MY_LOGD("mTARGET_IMG_WIDTH=%d, mTARGET_IMG_HEIGHT=%d",
            mTARGET_IMG_WIDTH, mTARGET_IMG_HEIGHT);

}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferSizeMgr::OCCBufferSize class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferSizeMgr::OCCBufferSize::
OCCBufferSize(ENUM_STEREO_SCENARIO scenario)
: BaseBufferSize(scenario)
{
    MY_LOGD("+");
    // setup constants
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    mDMH_SIZE = pSizeProvder->getBufferSize(E_DMH, scenario);
    mMYS_SIZE = pSizeProvder->getBufferSize(E_MY_S, scenario);
    debug();
    MY_LOGD("-");
}

MVOID
NodeBufferSizeMgr::OCCBufferSize::
debug() const
{
    MY_LOGD("OCC debug size: scenario=%d, ", mScenario);

    DEBUG_MSIZE(mDMH_SIZE);
    DEBUG_MSIZE(mMYS_SIZE);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferSizeMgr::WMFBufferSize class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferSizeMgr::WMFBufferSize::
WMFBufferSize(ENUM_STEREO_SCENARIO scenario)
: BaseBufferSize(scenario)
{
    MY_LOGD("+");
    // setup constants
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    mDMW_SIZE = pSizeProvder->getBufferSize(E_DMW, scenario);
    mDMW_DEPTHMAP_SIZE = pSizeProvder->getBufferSize(E_DEPTH_MAP, scenario);
    debug();
    MY_LOGD("-");
}

MVOID
NodeBufferSizeMgr::WMFBufferSize::
debug() const
{
    MY_LOGD("WMFBufferSize size======>\n");

    DEBUG_MSIZE(mDMW_SIZE);
    DEBUG_MSIZE(mDMW_DEPTHMAP_SIZE);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferSizeMgr class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferSizeMgr::
NodeBufferSizeMgr()
{
    ENUM_STEREO_SCENARIO scen[] = {eSTEREO_SCENARIO_RECORD, eSTEREO_SCENARIO_CAPTURE, eSTEREO_SCENARIO_PREVIEW};

    for(int index=0;index<3;++index)
    {
        mP2ASize.add(scen[index], P2AFMBufferSize(scen[index]));
        mN3DSize.add(scen[index], N3DBufferSize(scen[index]));
        mDPESize.add(scen[index], DPEBufferSize(scen[index]));
        mOCCSize.add(scen[index], OCCBufferSize(scen[index]));
        mWMFSize.add(scen[index], WMFBufferSize(scen[index]));
    }
}

NodeBufferSizeMgr::
~NodeBufferSizeMgr()
{
}

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam



