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
 * @file NodeBufferPoolMgr_Denoise.cpp
 * @brief BufferPoolMgr for Denoise
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
// Module header file
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
// Local header file
#include "NodeBufferPoolMgr_Denoise.h"
#include "NodeBufferHandler.h"
#include "../DepthMapPipe_Common.h"
#include "../bufferConfig/BufferConfig_Denoise.h"
// Logging header file
#define PIPE_CLASS_TAG "NodeBufferPoolMgr_Denoise"
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
typedef NodeBufferSizeMgr::P2AFMBufferSize P2AFMBufferSize;
typedef NodeBufferSizeMgr::N3DBufferSize N3DBufferSize;
typedef NodeBufferSizeMgr::DPEBufferSize DPEBufferSize;
typedef NodeBufferSizeMgr::OCCBufferSize OCCBufferSize;
typedef NodeBufferSizeMgr::WMFBufferSize WMFBufferSize;
/*******************************************************************************
* External Function
********************************************************************************/

/*******************************************************************************
* Enum Define
********************************************************************************/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_Denoise class - Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NodeBufferPoolMgr_Denoise::
NodeBufferPoolMgr_Denoise()
{
    MBOOL bRet = this->initializeBufferPool();

    if(!bRet)
    {
        MY_LOGE("Failed to initialize buffer pool set! Cannot continue..");
        uninit();
        return;
    }

    this->buildImageBufferPoolMap();
    this->buildRequestBufferMap();
    this->buildBufScenarioToTypeMap();

}

NodeBufferPoolMgr_Denoise::
~NodeBufferPoolMgr_Denoise()
{
    MY_LOGD("[Destructor] +");
    uninit();
    MY_LOGD("[Destructor] -");
}

MBOOL
NodeBufferPoolMgr_Denoise::
uninit()
{
    // destroy buffer pools
    ImageBufferPool::destroy(mpCCInBufPool);
    ImageBufferPool::destroy(mpFEOB_BufPool);
    ImageBufferPool::destroy(mpFEOC_BufPool);
    ImageBufferPool::destroy(mpFMOB_BufPool);
    ImageBufferPool::destroy(mpFMOC_BufPool);
    ImageBufferPool::destroy(mpFEBInBufPool_Main1);
    ImageBufferPool::destroy(mpFEBInBufPool_Main2);
    ImageBufferPool::destroy(mpFECInBufPool_Main1);
    ImageBufferPool::destroy(mpFECInBufPool_Main2);
    GraphicBufferPool::destroy(mpRectInBufPool_CAP);
    TuningBufferPool::destroy(mpTuningBufferPool);
    #ifdef GTEST
    ImageBufferPool::destroy(mFEHWInput_StageB_Main1);
    ImageBufferPool::destroy(mFEHWInput_StageB_Main2);
    ImageBufferPool::destroy(mFEHWInput_StageC_Main1);
    ImageBufferPool::destroy(mFEHWInput_StageC_Main2);
    #endif
    //----------------------N3D section--------------------------------//
    ImageBufferPool::destroy(mN3DImgBufPool);
    ImageBufferPool::destroy(mN3DMaskBufPool);
    //----------------------DPE section--------------------------------//
    ImageBufferPool::destroy(mDMPBuffPool);
    ImageBufferPool::destroy(mCFMBuffPool);
    ImageBufferPool::destroy(mRespBuffPool);
    return MTRUE;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BaseBufferPoolMgr Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
SmartImageBuffer
NodeBufferPoolMgr_Denoise::
request(DepthMapBufferID id, BufferPoolScenario scen)
{
    ssize_t index;
    if((index=mBIDtoImgBufPoolMap_Capture.indexOfKey(id)) >= 0)
    {
        sp<ImageBufferPool> pBufferPool = mBIDtoImgBufPoolMap_Capture.valueAt(index);
        return pBufferPool->request();
    }
    else
    {
        return NULL;
    }
}

SmartGraphicBuffer
NodeBufferPoolMgr_Denoise::
requestGB(DepthMapBufferID id, BufferPoolScenario scen)
{
     ssize_t index;
    if((index=mBIDtoGraphicBufPoolMap_Capture.indexOfKey(id)) >= 0)
    {
        sp<GraphicBufferPool> pBufferPool = mBIDtoGraphicBufPoolMap_Capture.valueAt(index);

        SmartGraphicBuffer smGraBuf = pBufferPool->request();
        // config
        android::sp<android::GraphicBuffer> pRectInGraBuf = smGraBuf->mGraphicBuffer;
        // config graphic buffer to BT601_FULL
        gralloc_extra_ion_sf_info_t info;
        gralloc_extra_query(pRectInGraBuf->handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &info);
        gralloc_extra_sf_set_status(&info, GRALLOC_EXTRA_MASK_YUV_COLORSPACE, GRALLOC_EXTRA_BIT_YUV_BT601_FULL);
        gralloc_extra_perform(pRectInGraBuf->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &info);
        return smGraBuf;
    }
    else
    {
        return NULL;
    }
}

SmartTuningBuffer
NodeBufferPoolMgr_Denoise::
requestTB(DepthMapBufferID id, BufferPoolScenario scen)
{
    if(id == BID_P2AFM_TUNING)
    {
        return mpTuningBufferPool->request();
    }
    else
    {
        MY_LOGE("Cannot find the TuningBufferPool with scenario:%d of buffer id:%d!!", scen, id);
            return NULL;
    }
}

BufferPoolHandlerPtr
NodeBufferPoolMgr_Denoise::
createBufferPoolHandler()
{
    BaseBufferHandler* pPtr = new NodeBufferHandler(this, mRequestBufferIDMap, mRequestMetaIDMap);
    return pPtr;
}

MBOOL
NodeBufferPoolMgr_Denoise::
queryBufferType(
    DepthMapBufferID bid,
    BufferPoolScenario scen,
    DepthBufferType& rOutBufType
)
{
    ssize_t index;

    if((index=mBIDToScenarioTypeMap.indexOfKey(bid))>=0)
    {
        BufScenarioToTypeMap scenMap = mBIDToScenarioTypeMap.valueAt(index);
        if((index=scenMap.indexOfKey(scen))>=0)
        {
            rOutBufType = scenMap.valueAt(index);
            return MTRUE;
        }
    }
    return MFALSE;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  NodeBufferPoolMgr_Denoise class - Private Functinos
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

MBOOL
NodeBufferPoolMgr_Denoise::
queryFEOBufferSize(MSize iBufSize, MUINT iBlockSize, MSize& rFEOSize)
{
    rFEOSize.w = iBufSize.w/iBlockSize*40;
    rFEOSize.h = iBufSize.h/iBlockSize;
    MY_LOGD("queryFEOBufferSize: iBufSize=%dx%d  ouput=%dx%d", iBufSize.w, iBufSize.h, rFEOSize.w, rFEOSize.h);
    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
queryFMOBufferSize(MSize szFEOBuffer, MSize& rFMOSize)
{
    rFMOSize.w = (szFEOBuffer.w/40) * 2;
    rFMOSize.h = szFEOBuffer.h;

    MY_LOGD("queryFMOBufferSize: FEOWidth=%dx%d  ouput FMOSize=%dx%d", szFEOBuffer.w, szFEOBuffer.h, rFMOSize.w, rFMOSize.h);
    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
initializeBufferPool()
{
    CAM_TRACE_BEGIN("NodeBufferPoolMgr_Denoise::initializeBufferPool");
    MY_LOGD("+");

    MBOOL bRet = MTRUE;
    bRet &= this->initP2ABufferPool();
    bRet &= this->initN3DBufferPool();
    bRet &= this->initDPEBufferPool();

    MY_LOGD("-");
    return bRet;
}

MBOOL
NodeBufferPoolMgr_Denoise::
initP2ABufferPool()
{
    MY_LOGD("+");
    const P2AFMBufferSize& rP2AFM_CAP=mBufferSizeMgr.getP2AFM(eSTEREO_SCENARIO_CAPTURE);

    /**********************************************************************
     * Rectify_in/CC_in/FD/Tuning/FE/FM buffer pools
     **********************************************************************/

    // Capture, Rect_in, main1+main2
    mpRectInBufPool_CAP = GraphicBufferPool::create("RectInGra_BufPool",
                                    rP2AFM_CAP.mRECT_IN_SIZE_MAIN1.w, rP2AFM_CAP.mRECT_IN_SIZE_MAIN1.h,
                                    eImgFmt_YV12, GraphicBufferPool::USAGE_HW_TEXTURE);

    // CC_in
    CREATE_IMGBUF_POOL(mpCCInBufPool, "CCin_BufPool", rP2AFM_CAP.mCCIN_SIZE,
                        eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);

    // TuningBufferPool creation
    mpTuningBufferPool = TuningBufferPool::create("VSDOF_TUNING_P2A", sizeof(dip_x_reg_t));

    // allcate buffer
    ALLOCATE_BUFFER_POOL(mpRectInBufPool_CAP, 2 * VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mpCCInBufPool, 2 * VSDOF_WORKING_BUF_SET);      // CC_in : main1+main2
    ALLOCATE_BUFFER_POOL(mpTuningBufferPool, VSDOF_BURST_FRAME_SIZE);
    // initialize the FE/FE related pools
    this->initFEFMBufferPool();

    MY_LOGD("-");
    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
initN3DBufferPool()
{
    MY_LOGD("+");
    // SW read/write, hw read
    MUINT32 usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ;

    const N3DBufferSize& rN3DSize = mBufferSizeMgr.getN3D(eSTEREO_SCENARIO_CAPTURE);
    // INIT N3D output buffer pool - for MV/SV
    CREATE_IMGBUF_POOL(mN3DImgBufPool, "N3DImgBufPool", rN3DSize.mWARP_IMG_SIZE,
                        eImgFmt_YV12, usage, MTRUE);
    // INIT N3D output buffer pool - for MASK_M/MASK_Y
    CREATE_IMGBUF_POOL(mN3DMaskBufPool, "N3DMaskBufPool", rN3DSize.mWARP_MASK_SIZE,
                        eImgFmt_Y8, usage, MTRUE);
    // for MV+SV -> double size
    ALLOCATE_BUFFER_POOL(mN3DImgBufPool, VSDOF_WORKING_BUF_SET*2);
    // for MASK_M+MASK_Y -> double size
    ALLOCATE_BUFFER_POOL(mN3DMaskBufPool, VSDOF_WORKING_BUF_SET*2);
    MY_LOGD("-");
    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
initDPEBufferPool()
{
    MY_LOGD("+");
    #define BUFFRPOOL_EXTRA_SIZE_FOR_LAST_DMP 2

    const DPEBufferSize& rDPESize = mBufferSizeMgr.getDPE(eSTEREO_SCENARIO_CAPTURE);
    // DMP buffer pool are ONLY used for the DPENode's Last DMP_L/R
    CREATE_IMGBUF_POOL(mDMPBuffPool, "DMPBufPool", rDPESize.mDMP_SIZE,
                                        eImgFmt_Y16, ImageBufferPool::USAGE_HW, MTRUE);

    ALLOCATE_BUFFER_POOL(mDMPBuffPool, BUFFRPOOL_EXTRA_SIZE_FOR_LAST_DMP);

    // allocate with the stride size as the width,
    CREATE_IMGBUF_POOL(mCFMBuffPool, "CFMBufPool", rDPESize.mCFM_SIZE,
                                        eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mRespBuffPool, "RESPOBufPool", rDPESize.mRESPO_SIZE,
                                        eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);
    //CFM, RESPO have all Left/Right side ->  double size
    ALLOCATE_BUFFER_POOL(mCFMBuffPool, VSDOF_WORKING_BUF_SET*2);
    ALLOCATE_BUFFER_POOL(mRespBuffPool, VSDOF_WORKING_BUF_SET*2);

    MY_LOGD("-");
    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
initFEFMBufferPool()
{
    const P2AFMBufferSize& rP2aSize = mBufferSizeMgr.getP2AFM(eSTEREO_SCENARIO_CAPTURE);
    /**********************************************************************
     * FE/FM has 3 stage A,B,C, currently only apply 2 stage FEFM: stage=B(1),C(2)
     **********************************************************************/
    // FE/FM buffer pool - stage B
    MUINT32 iBlockSize = StereoSettingProvider::fefmBlockSize(1);
    // query the FEO buffer size from FE input buffer size
    MSize szFEBufSize = rP2aSize.mFEB_INPUT_SIZE_MAIN1;
    MSize szFEOBufferSize, szFMOBufferSize;
    queryFEOBufferSize(szFEBufSize, iBlockSize, szFEOBufferSize);
    // create buffer pool
    CREATE_IMGBUF_POOL(mpFEOB_BufPool, "FEB_BufPoll", szFEOBufferSize,
                        eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);
    // query the FMO buffer size from FEO size
    queryFMOBufferSize(szFEOBufferSize, szFMOBufferSize);
    // create buffer pool
    CREATE_IMGBUF_POOL(mpFMOB_BufPool, "FMB_BufPool", szFMOBufferSize,
                        eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);

    // FE/FM buffer pool - stage C
    iBlockSize = StereoSettingProvider::fefmBlockSize(2);
    // query FEO/FMO size and create pool
    szFEBufSize = rP2aSize.mFEC_INPUT_SIZE_MAIN1;
    queryFEOBufferSize(szFEBufSize, iBlockSize, szFEOBufferSize);
    CREATE_IMGBUF_POOL(mpFEOC_BufPool, "FEC_BufPool", szFEOBufferSize,
                        eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);
    queryFMOBufferSize(szFEOBufferSize, szFMOBufferSize);
    CREATE_IMGBUF_POOL(mpFMOC_BufPool, "FMC_BufPool", szFMOBufferSize,
                        eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);

    // create the FE input buffer pool - stage B (the seocond FE input buffer)
    //FEB Main1 input
    CREATE_IMGBUF_POOL(mpFEBInBufPool_Main1, "FE1BInputBufPool", rP2aSize.mFEB_INPUT_SIZE_MAIN1,
                    eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);

    //FEB Main2 input
    CREATE_IMGBUF_POOL(mpFEBInBufPool_Main2, "FE2BInputBufPool", rP2aSize.mFEB_INPUT_SIZE_MAIN2,
                    eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);

    //FEC Main1 input
    CREATE_IMGBUF_POOL(mpFECInBufPool_Main1, "FE1CInputBufPool", rP2aSize.mFEC_INPUT_SIZE_MAIN1,
                    eImgFmt_YUY2, ImageBufferPool::USAGE_HW, MTRUE);

    //FEC Main2 input
    CREATE_IMGBUF_POOL(mpFECInBufPool_Main2, "FE2CInputBufPool", rP2aSize.mFEC_INPUT_SIZE_MAIN2,
                    eImgFmt_YUY2, ImageBufferPool::USAGE_HW, MTRUE);

    // FEO/FMO buffer pool- ALLOCATE buffers : Main1+Main2 -> two working set
    ALLOCATE_BUFFER_POOL(mpFEOB_BufPool, 2*VSDOF_WORKING_BUF_SET)
    ALLOCATE_BUFFER_POOL(mpFEOC_BufPool, 2*VSDOF_WORKING_BUF_SET)
    ALLOCATE_BUFFER_POOL(mpFMOB_BufPool, 2*VSDOF_WORKING_BUF_SET)
    ALLOCATE_BUFFER_POOL(mpFMOC_BufPool, 2*VSDOF_WORKING_BUF_SET)

    // FEB/FEC_Input buffer pool- ALLOCATE buffers : 2 (internal working buffer in Burst trigger)
    ALLOCATE_BUFFER_POOL(mpFEBInBufPool_Main1, 2)
    ALLOCATE_BUFFER_POOL(mpFEBInBufPool_Main2, 2)
    ALLOCATE_BUFFER_POOL(mpFECInBufPool_Main1, 2)
    ALLOCATE_BUFFER_POOL(mpFECInBufPool_Main2, 2)

    #ifdef GTEST
    // FE HW Input For UT
    CREATE_IMGBUF_POOL(mFEHWInput_StageB_Main1, "mFEHWInput_StageB_Main1", rP2aSize.mFEB_INPUT_SIZE_MAIN1, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mFEHWInput_StageB_Main2, "mFEHWInput_StageB_Main2", rP2aSize.mFEBO_AREA_MAIN2.size, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mFEHWInput_StageC_Main1, "mFEHWInput_StageC_Main1", rP2aSize.mFEC_INPUT_SIZE_MAIN1, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mFEHWInput_StageC_Main2, "mFEHWInput_StageC_Main2", rP2aSize.mFECO_AREA_MAIN2.size, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    ALLOCATE_BUFFER_POOL(mFEHWInput_StageB_Main1, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mFEHWInput_StageB_Main2, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mFEHWInput_StageC_Main1, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mFEHWInput_StageC_Main2, VSDOF_WORKING_BUF_SET);
    #endif

    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
buildImageBufferPoolMap()
{
    MY_LOGD("+");

    //---- build the buffer pool map for Capture scenario ----

    // P2A Part
    {
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_CC_IN1, mpCCInBufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_CC_IN2, mpCCInBufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FE1BO, mpFEOB_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FE2BO, mpFEOB_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FE1CO, mpFEOC_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FE2CO, mpFEOC_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FMBO_RL, mpFMOB_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FMBO_LR, mpFMOB_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FMCO_LR, mpFMOC_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_OUT_FMCO_RL, mpFMOC_BufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_FE1B_INPUT, mpFEBInBufPool_Main1);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_FE2B_INPUT, mpFEBInBufPool_Main2);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_FE1C_INPUT, mpFECInBufPool_Main1);
        mBIDtoImgBufPoolMap_Capture.add(BID_P2AFM_FE2C_INPUT, mpFECInBufPool_Main2);
        #ifdef GTEST
        mBIDtoImgBufPoolMap_Capture.add(BID_FE2_HWIN_MAIN1, mFEHWInput_StageB_Main1);
        mBIDtoImgBufPoolMap_Capture.add(BID_FE2_HWIN_MAIN2, mFEHWInput_StageB_Main2);
        mBIDtoImgBufPoolMap_Capture.add(BID_FE3_HWIN_MAIN1, mFEHWInput_StageC_Main1);
        mBIDtoImgBufPoolMap_Capture.add(BID_FE3_HWIN_MAIN2, mFEHWInput_StageC_Main2);
        #endif
        // graphic buffer
        mBIDtoGraphicBufPoolMap_Capture.add(BID_P2AFM_OUT_RECT_IN1, mpRectInBufPool_CAP);
        mBIDtoGraphicBufPoolMap_Capture.add(BID_P2AFM_OUT_RECT_IN2, mpRectInBufPool_CAP);
    }

    //N3D Part
    {
        mBIDtoImgBufPoolMap_Capture.add(BID_N3D_OUT_MV_Y, mN3DImgBufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_N3D_OUT_SV_Y, mN3DImgBufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_N3D_OUT_MASK_M, mN3DMaskBufPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_N3D_OUT_MASK_S, mN3DMaskBufPool);
    }

    //DPE Part
    {
        // DMP buffer use the ones inside requests
        mBIDtoImgBufPoolMap_Capture.add(BID_DPE_INTERNAL_LAST_DMP, mDMPBuffPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_DPE_OUT_CFM_R, mCFMBuffPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_DPE_OUT_CFM_L, mCFMBuffPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_DPE_OUT_RESPO_L, mRespBuffPool);
        mBIDtoImgBufPoolMap_Capture.add(BID_DPE_OUT_RESPO_R, mRespBuffPool);
    }

    MY_LOGD("-");
    return MTRUE;
}

sp<ImageBufferPool>
NodeBufferPoolMgr_Denoise::
getImageBufferPool(DepthMapBufferID bufferID)
{
    ssize_t index=mBIDtoImgBufPoolMap_Capture.indexOfKey(bufferID);
    if(index >= 0)
    {
        return mBIDtoImgBufPoolMap_Capture.valueAt(index);
    }

    return NULL;
}

MBOOL
NodeBufferPoolMgr_Denoise::
buildRequestBufferMap()
{
    MY_LOGD("+");
    BufferIDMap capBufferMap;
    // only capture
    const NodeBufferSetting* pConfig = capture_buffer_config;
    while(pConfig->bufferID!=ID_INVALID)
    {
        MY_LOGD("Add request buffer for capture: bufferID=%d  ioType=%d",
                    pConfig->bufferID, pConfig->ioType);

        capBufferMap.add(pConfig->bufferID, pConfig->ioType);
        pConfig++;
    }
    mRequestBufferIDMap.add(eBUFFER_POOL_SCENARIO_CAPTURE, capBufferMap);

    BufferIDMap pvMetaBufferMap, vrMetaBufferMap, capMetaBufferMap;
    pConfig =metadata_config;
    while(pConfig->bufferID!=ID_INVALID)
    {
        pvMetaBufferMap.add(pConfig->bufferID, pConfig->ioType);
        vrMetaBufferMap.add(pConfig->bufferID, pConfig->ioType);
        capMetaBufferMap.add(pConfig->bufferID, pConfig->ioType);
        pConfig++;
    }
    mRequestMetaIDMap.add(eBUFFER_POOL_SCENARIO_CAPTURE, capMetaBufferMap);

    MY_LOGD("-");
    return MTRUE;
}

MBOOL
NodeBufferPoolMgr_Denoise::
buildBufScenarioToTypeMap()
{
    // ---------------- only capture -------------------
    // buffer id in Capture imgBuf pool map
    for(ssize_t idx=0;idx<mBIDtoImgBufPoolMap_Capture.size();++idx)
    {
        DepthMapBufferID bid = mBIDtoImgBufPoolMap_Capture.keyAt(idx);
        BufScenarioToTypeMap typeMap;
        typeMap.add(eBUFFER_POOL_SCENARIO_CAPTURE, eBUFFER_IMAGE);
        mBIDToScenarioTypeMap.add(bid, typeMap);
    }

    // graphic buffer section
    for(ssize_t idx=0;idx<mBIDtoGraphicBufPoolMap_Capture.size();++idx)
    {
        DepthMapBufferID bid = mBIDtoGraphicBufPoolMap_Capture.keyAt(idx);
        BufScenarioToTypeMap typeMap;
        typeMap.add(eBUFFER_POOL_SCENARIO_CAPTURE, eBUFFER_GRAPHIC);
        mBIDToScenarioTypeMap.add(bid, typeMap);
    }

    // tuning buffer section
    BufScenarioToTypeMap typeMap;
    typeMap.add(eBUFFER_POOL_SCENARIO_CAPTURE, eBUFFER_TUNING);
    mBIDToScenarioTypeMap.add(BID_P2AFM_TUNING, typeMap);
    return MTRUE;

}

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam



