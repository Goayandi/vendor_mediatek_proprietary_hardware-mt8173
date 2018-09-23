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

#include "P2AFMNode.h"
#include "DepthMapPipe_Common.h"

#include <mtkcam/drv/IHalSensor.h>
#include <libion_mtk/include/ion.h>
#include <camera_custom_stereo.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <vsdof/hal/stereo_tuning_provider.h>

#define PIPE_CLASS_TAG "P2A_FMNode"
#include <featurePipe/core/include/PipeLog.h>


// FM frame 6~9, template 0~3
#define GET_FM_STAGE(FRAME_ID) FRAME_ID-6

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

P2AFMNode::BufferSizeConfig::
BufferSizeConfig()
{
    StereoSizeProvider* pSizePrvder = StereoSizeProvider::getInstance();

    // PV/VR - frame 0
    Pass2SizeInfo pass2SizeInfo;
    pSizePrvder->getPass2SizeInfo(PASS2A_P, eSTEREO_SCENARIO_RECORD, pass2SizeInfo);
    P2AFM_FE2BO_INPUT_SIZE = pass2SizeInfo.areaWROT;
    P2AFM_MAIN2_FEAO_AREA = pass2SizeInfo.areaFEO;
    P2AFM_FEAO_INPUT_SIZE = pass2SizeInfo.areaFEO.size;

    // frame 1
    pSizePrvder->getPass2SizeInfo(PASS2A, eSTEREO_SCENARIO_RECORD, pass2SizeInfo);
    P2AFM_FD_IMG_SIZE = pass2SizeInfo.areaIMG2O;
    P2AFM_MAIN_IMAGE_SIZE = pass2SizeInfo.areaWDMA.size;
    P2AFM_FE1BO_INPUT_SIZE = pass2SizeInfo.areaWROT;

    // frame 2
    pSizePrvder->getPass2SizeInfo(PASS2A_P_2, eSTEREO_SCENARIO_RECORD, pass2SizeInfo);
    P2AFM_FE2CO_INPUT_SIZE = pass2SizeInfo.areaIMG2O;
    P2AFM_RECT_IN_VR_SIZE_MAIN2 = pass2SizeInfo.areaWDMA.size;
    P2AFM_MAIN2_FEBO_AREA = pass2SizeInfo.areaFEO;

    // frame 3
    pSizePrvder->getPass2SizeInfo(PASS2A_2, eSTEREO_SCENARIO_RECORD, pass2SizeInfo);
    P2AFM_FE1CO_INPUT_SIZE = pass2SizeInfo.areaIMG2O;
    P2AFM_RECT_IN_VR_SIZE_MAIN1 = pass2SizeInfo.areaWDMA.size;

    // frame 4
    pSizePrvder->getPass2SizeInfo(PASS2A_P_3, eSTEREO_SCENARIO_RECORD, pass2SizeInfo);
    P2AFM_MAIN2_FECO_AREA = pass2SizeInfo.areaFEO;
    // cc_in
    P2AFM_CCIN_SIZE = pass2SizeInfo.areaIMG2O;

    // PV - frame 2
    pSizePrvder->getPass2SizeInfo(PASS2A_P_2, eSTEREO_SCENARIO_PREVIEW, pass2SizeInfo);
    P2AFM_RECT_IN_PV_SIZE_MAIN2 = pass2SizeInfo.areaWDMA.size;
    // frame 3
    pSizePrvder->getPass2SizeInfo(PASS2A_2, eSTEREO_SCENARIO_PREVIEW, pass2SizeInfo);
    P2AFM_RECT_IN_PV_SIZE_MAIN1 = pass2SizeInfo.areaWDMA.size;

    // CAP
    // frame 1
    pSizePrvder->getPass2SizeInfo(PASS2A, eSTEREO_SCENARIO_CAPTURE, pass2SizeInfo);
    P2AFM_MAIN_IMAGE_CAP_SIZE = pass2SizeInfo.areaWDMA.size;

    // frame 2
    pSizePrvder->getPass2SizeInfo(PASS2A_P_2, eSTEREO_SCENARIO_CAPTURE, pass2SizeInfo);
    P2AFM_RECT_IN_CAP_SIZE = pass2SizeInfo.areaWDMA.size;
    P2AFM_RECT_IN_CAP_IMG_SIZE = pass2SizeInfo.areaWDMA.size - pass2SizeInfo.areaWDMA.padding;

    debug();

}


MVOID
P2AFMNode::BufferSizeConfig::debug() const
{
    MY_LOGD("P2AFM debug size======>\n");
    #define DEBUG_MSIZE(sizeCons) \
        MY_LOGD("size: " #sizeCons " : %dx%d\n", sizeCons.w, sizeCons.h);

    DEBUG_MSIZE( P2AFM_FD_IMG_SIZE);
    DEBUG_MSIZE( P2AFM_FEAO_INPUT_SIZE);
    DEBUG_MSIZE( P2AFM_FE1BO_INPUT_SIZE);
    DEBUG_MSIZE( P2AFM_FE2BO_INPUT_SIZE);
    DEBUG_MSIZE( P2AFM_FE1CO_INPUT_SIZE);
    DEBUG_MSIZE( P2AFM_FE2CO_INPUT_SIZE);
    DEBUG_MSIZE( P2AFM_RECT_IN_VR_SIZE_MAIN1);
    DEBUG_MSIZE( P2AFM_RECT_IN_VR_SIZE_MAIN2);
    DEBUG_MSIZE( P2AFM_RECT_IN_PV_SIZE_MAIN1);
    DEBUG_MSIZE( P2AFM_RECT_IN_PV_SIZE_MAIN2);
    DEBUG_MSIZE( P2AFM_RECT_IN_CAP_SIZE);
    DEBUG_MSIZE( P2AFM_RECT_IN_CAP_IMG_SIZE);
    DEBUG_MSIZE( P2AFM_MAIN_IMAGE_SIZE);
    DEBUG_MSIZE( P2AFM_MAIN_IMAGE_CAP_SIZE);
    DEBUG_MSIZE( P2AFM_CCIN_SIZE);

    #define DEBUG_AREA(areaCons) \
        MY_LOGD("Area: " #areaCons " : size=%dx%d padd=%dx%d startPt=(%d, %d)\n", areaCons.size.w, areaCons.size.h, areaCons.padding.w, areaCons.padding.h, areaCons.startPt.x, areaCons.startPt.x);

    DEBUG_AREA(P2AFM_MAIN2_FEAO_AREA);
    DEBUG_AREA(P2AFM_MAIN2_FEBO_AREA);
    DEBUG_AREA(P2AFM_MAIN2_FECO_AREA);

    MY_LOGD("StereoSettingProvider::fefmBlockSize(1)=%d StereoSettingProvider::fefmBlockSize(2)=%d",
                StereoSettingProvider::fefmBlockSize(1), StereoSettingProvider::fefmBlockSize(2));


    MY_LOGD("StereoSettingProvider::getStereoCameraFOVRatio=%f",StereoSettingProvider::getStereoCameraFOVRatio());
}

P2AFMNode::BufferSizeConfig::
~BufferSizeConfig()
{}

P2AFMNode::BufferPoolSet::
BufferPoolSet()
{}

P2AFMNode::BufferPoolSet::
~BufferPoolSet()
{}

MVOID
P2AFMNode::BufferPoolSet::
queryFEOBufferSize(MSize iBufSize, MUINT iBlockSize, MUINT32 &riFEOWidth, MUINT32 &riFEOHeight)
{
    riFEOWidth = iBufSize.w/iBlockSize*40;
    riFEOHeight = iBufSize.h/iBlockSize;
    MY_LOGD("queryFEOBufferSize: iBufSize=%dx%d  ouput=%dx%d", iBufSize.w, iBufSize.h, riFEOWidth, riFEOHeight);
}

MVOID
P2AFMNode::BufferPoolSet::
queryFMOBufferSize(MUINT32 iFEOWidth, MUINT32 iFEOHeight, MUINT32 &riFMOWidth, MUINT32 &riFMOHeight)
{
    riFMOWidth = (iFEOWidth/40) * 2;
    riFMOHeight = iFEOHeight;

    MY_LOGD("queryFMOBufferSize: iFEOWidth=%d iFEOHeight=%d  ouput=%dx%d", iFEOWidth, iFEOHeight, riFMOWidth, riFMOHeight);
}

MBOOL
P2AFMNode::BufferPoolSet::
init(
    const BufferSizeConfig& config,
    DepthFlowType flowType
)
{
    MY_LOGD("+ : BufferPoolSet init ");
    //FE FM Buffer pool
    MUINT32 fe_width=0, fe_height=0;
    MUINT32 fm_width=0, fm_height=0;

    MUINT32 iBlockSize;

    if(VSDOF_CONST_FE_EXEC_TIMES== 2)
    {
        // stage 2 - FEFM
        MUINT32 iBlockSize = StereoSettingProvider::fefmBlockSize(1);
        // calculate the buffers which is the input for FE HW, use Main1 version/because Main2 has SRC CROP
        MSize szFEBufSize = config.P2AFM_FE1BO_INPUT_SIZE;
        queryFEOBufferSize(szFEBufSize, iBlockSize, fe_width, fe_height);
        queryFMOBufferSize(fe_width, fe_height, fm_width, fm_height);
        // create image buffer pool
        CREATE_IMGBUF_POOL(mpFMOB_BufPool, "FMB_BufPool", MSize(fm_width, fm_height),
                            eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);
        CREATE_IMGBUF_POOL(mpFEOB_BufPool, "FEB_BufPoll", MSize(fe_width, fe_height),
                            eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);
        // stage 3 - FEFM
        iBlockSize = StereoSettingProvider::fefmBlockSize(2);
        // calculate the buffers which is the input for FE HW, use Main1 version/because Main2 has SRC CROP
        szFEBufSize = config.P2AFM_FE1CO_INPUT_SIZE;
        queryFEOBufferSize(szFEBufSize, iBlockSize, fe_width, fe_height);
        queryFMOBufferSize(fe_width, fe_height, fm_width, fm_height);
        CREATE_IMGBUF_POOL(mpFMOC_BufPool, "FMC_BufPool", MSize(fm_width, fm_height), eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);
        CREATE_IMGBUF_POOL(mpFEOC_BufPool, "FEC_BufPool", MSize(fe_width, fe_height), eImgFmt_STA_BYTE, ImageBufferPool::USAGE_HW, MTRUE);
    }
    else
    {
        MY_LOGE("VSDOF_CONST_FE_EXEC_TIMES const error! val = %d", VSDOF_CONST_FE_EXEC_TIMES);
        return MFALSE;
    }

    android::sp<ImageBufferPool> FE_POOLS[2] = {mpFEOC_BufPool, mpFEOB_BufPool};
    android::sp<ImageBufferPool> FM_POOLS[2] = {mpFMOC_BufPool, mpFMOB_BufPool};

    for (MUINT8 index=0; index<VSDOF_CONST_FE_EXEC_TIMES;++index)
    {
        android::sp<ImageBufferPool> fe_pool = FE_POOLS[index];
        android::sp<ImageBufferPool> fm_pool = FM_POOLS[index];
        // Main1+Main2 -->?2 * VSDOF_WORKING_BUF_SET
        ALLOCATE_BUFFER_POOL(fe_pool, 2*VSDOF_WORKING_BUF_SET);
        ALLOCATE_BUFFER_POOL(fm_pool, 2*VSDOF_WORKING_BUF_SET);
    }

    // VR: Rect_in1
    CREATE_IMGBUF_POOL(mpRectInVRBufPool, "RectInVR_BufPool", config.P2AFM_RECT_IN_VR_SIZE_MAIN1, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    // Rect_in2
    CREATE_IMGBUF_POOL(mpRectInVRBufPool_Main2, "RectInVR_BufPool_Main2", config.P2AFM_RECT_IN_VR_SIZE_MAIN2, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);

    // Capture
    // Rect_in2 : graphic buffer
    mpRectInCapBufPool_Gra_Main2 = GraphicBufferPool::create("RectInGra_BufPool", config.P2AFM_RECT_IN_CAP_SIZE.w,
                                            config.P2AFM_RECT_IN_CAP_SIZE.h, eImgFmt_YV12, GraphicBufferPool::USAGE_HW_TEXTURE);
    // PV: Rect_in1
    CREATE_IMGBUF_POOL(mpRectInPVBufPool, "RectInPV_BufPool", config.P2AFM_RECT_IN_PV_SIZE_MAIN1, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    // Rect_in2
    CREATE_IMGBUF_POOL(mpRectInPVBufPool_Main2, "RectInPV_BufPool_Main2", config.P2AFM_RECT_IN_PV_SIZE_MAIN2, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);


    //FEBO_Main1 input
    CREATE_IMGBUF_POOL(mpFEBOInBufPool_Main1, "FE1BO_InputBufPool", config.P2AFM_FE1BO_INPUT_SIZE, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);

    //FEBO_Main2 input
    CREATE_IMGBUF_POOL(mpFEBOInBufPool_Main2, "FE2BO_InputBufPool", config.P2AFM_FE2BO_INPUT_SIZE, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);

    //FECO_Main1 input
    CREATE_IMGBUF_POOL(mpFECOInBufPool_Main1, "FE1CO_InputBufPool", config.P2AFM_FE1CO_INPUT_SIZE, eImgFmt_YUY2, ImageBufferPool::USAGE_HW, MTRUE);

    //FECO_Main2 input
    CREATE_IMGBUF_POOL(mpFECOInBufPool_Main2, "FE2CO_InputBufPool", config.P2AFM_FE2CO_INPUT_SIZE, eImgFmt_YUY2, ImageBufferPool::USAGE_HW, MTRUE);

    // CC_in
    CREATE_IMGBUF_POOL(mpCCInBufPool, "CCin_BufPool", config.P2AFM_CCIN_SIZE, eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);

    // FD when Capture
    CREATE_IMGBUF_POOL(mpFDBufPool, "FDBufPool", config.P2AFM_FD_IMG_SIZE, eImgFmt_YUY2, ImageBufferPool::USAGE_HW, MTRUE);

    #ifdef GTEST
    // FEInput For UT
    CREATE_IMGBUF_POOL(mFEInput_Stage2_Main1, "mFEInput_Stage2_Main1", config.P2AFM_FE1BO_INPUT_SIZE, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mFEInput_Stage2_Main2, "mFEInput_Stage2_Main2", config.P2AFM_MAIN2_FEBO_AREA.size, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mFEInput_Stage3_Main1, "mFEInput_Stage3_Main1", config.P2AFM_FE1CO_INPUT_SIZE, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    CREATE_IMGBUF_POOL(mFEInput_Stage3_Main2, "mFEInput_Stage3_Main2", config.P2AFM_MAIN2_FECO_AREA.size, eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    ALLOCATE_BUFFER_POOL(mFEInput_Stage2_Main1, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mFEInput_Stage2_Main2, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mFEInput_Stage3_Main1, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mFEInput_Stage3_Main2, VSDOF_WORKING_BUF_SET);
    #endif

    // Rect_in : PV/VR
    ALLOCATE_BUFFER_POOL(mpRectInVRBufPool, VSDOF_WORKING_BUF_SET);
    ALLOCATE_BUFFER_POOL(mpRectInVRBufPool_Main2, VSDOF_WORKING_BUF_SET);
    if(flowType == eDEPTH_FLOW_TYPE_QUEUED_DEPTH) {
        //extra set for BID_P2AFM_OUT_MY_S
        ALLOCATE_BUFFER_POOL(mpRectInPVBufPool, VSDOF_WORKING_BUF_SET * 2);
    }
    else {
        ALLOCATE_BUFFER_POOL(mpRectInPVBufPool, VSDOF_WORKING_BUF_SET); }

    ALLOCATE_BUFFER_POOL(mpRectInPVBufPool_Main2, VSDOF_WORKING_BUF_SET);
    // FEOIn
    ALLOCATE_BUFFER_POOL(mpFEBOInBufPool_Main1, 2);
    ALLOCATE_BUFFER_POOL(mpFEBOInBufPool_Main2, 2);
    ALLOCATE_BUFFER_POOL(mpFECOInBufPool_Main1, 2);
    ALLOCATE_BUFFER_POOL(mpFECOInBufPool_Main2, 2);

    // cc_in --> 2 sets
    ALLOCATE_BUFFER_POOL(mpCCInBufPool, 2 * VSDOF_WORKING_BUF_SET);
    // Rect_in : Capture
    ALLOCATE_BUFFER_POOL(mpRectInCapBufPool_Gra_Main2, VSDOF_WORKING_BUF_SET);
    // FD
    ALLOCATE_BUFFER_POOL(mpFDBufPool, VSDOF_WORKING_BUF_SET);
    // TuningBufferPool creation
    mpTuningBufferPool = TuningBufferPool::create("VSDOF_TUNING_P2A", sizeof(dip_x_reg_t));
    ALLOCATE_BUFFER_POOL(mpTuningBufferPool, VSDOF_BURST_FRAME_SIZE);


    MY_LOGD("mpFEOC_BufPool=%x mpRectInVRBufPool=%x mpFDBufPool=%x mpTuningBufferPool=%x",
    mpFEOC_BufPool.get(), mpRectInVRBufPool.get(), mpFDBufPool.get(), mpTuningBufferPool.get());

    MY_LOGD("- : BufferPoolSet init ");
    config.debug();

    return MTRUE;
}

MBOOL
P2AFMNode::BufferPoolSet::
uninit()
{
    MY_LOGD("+");

    //clear data
    ImageBufferPool::destroy(mpFEOB_BufPool);
    ImageBufferPool::destroy(mpFEOC_BufPool);
    ImageBufferPool::destroy(mpFMOB_BufPool);
    ImageBufferPool::destroy(mpFMOC_BufPool);
    ImageBufferPool::destroy(mpRectInVRBufPool);
    ImageBufferPool::destroy(mpRectInVRBufPool_Main2);
    ImageBufferPool::destroy(mpRectInPVBufPool);
    ImageBufferPool::destroy(mpRectInPVBufPool_Main2);
    GraphicBufferPool::destroy(mpRectInCapBufPool_Gra_Main2);

    ImageBufferPool::destroy(mpCCInBufPool);
    ImageBufferPool::destroy(mpFEBOInBufPool_Main1);
    ImageBufferPool::destroy(mpFEBOInBufPool_Main2);
    ImageBufferPool::destroy(mpFECOInBufPool_Main1);
    ImageBufferPool::destroy(mpFECOInBufPool_Main2);
    ImageBufferPool::destroy(mpFDBufPool);
    TuningBufferPool::destroy(mpTuningBufferPool);

    #ifdef GTEST
    ImageBufferPool::destroy(mFEInput_Stage2_Main1);
    ImageBufferPool::destroy(mFEInput_Stage2_Main2);
    ImageBufferPool::destroy(mFEInput_Stage3_Main1);
    ImageBufferPool::destroy(mFEInput_Stage3_Main2);
    #endif
    MY_LOGD("-");

    return MTRUE;
}


P2AFMNode::
P2AFMNode(
    const char* name,
    DepthFlowType flowType,
    MUINT32 iSensorIdx_Main1,
    MUINT32 iSensorIdx_Main2
)
: DepthMapPipeNode(name, flowType), miSensorIdx_Main1(iSensorIdx_Main1)
, miSensorIdx_Main2(iSensorIdx_Main2)
{
    VSDOF_LOGD("Sensor index, Main1: %d, Main2: %d, flowType=%d",
        miSensorIdx_Main1, miSensorIdx_Main2, flowType);
    this->addWaitQueue(&mRequestQue);

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

MBOOL
P2AFMNode::
setDepthBufStorage(sp<DepthInfoStorage> pDepthStorage)
{
    mpDepthStorage=pDepthStorage;
    return MTRUE;
}

P2AFMNode::
~P2AFMNode()
{
}

MVOID
P2AFMNode::
cleanUp()
{
    MY_LOGD("+");
    if(mpINormalStream != NULL)
    {
        mpINormalStream->uninit("VSDOF_P2AFM");
        mpINormalStream->destroyInstance();
        mpINormalStream = NULL;
    }

    if(mp3AHal_Main1)
    {
        mp3AHal_Main1->destroyInstance("VSDOF_3A_MAIN1");
        mp3AHal_Main1 = NULL;
    }

    if(mp3AHal_Main2)
    {
        mp3AHal_Main2->destroyInstance("VSDOF_3A_MAIN2");
        mp3AHal_Main2 = NULL;
    }

    mFETuningBufferMap.clear();
    mFMTuningBufferMap.clear();
    mBufPoolSet.uninit();

    // free the item inside mSrzSizeTemplateMap
    for(size_t index=0;index<mSrzSizeTemplateMap.size();++index)
    {
        _SRZ_SIZE_INFO_* pData = mSrzSizeTemplateMap.valueAt(index);
        delete pData;
    }
    mSrzSizeTemplateMap.clear();

    MY_LOGD("-");
}


MBOOL
P2AFMNode::
onInit()
{
    CAM_TRACE_NAME("P2AFMNode::onInit");
    MY_LOGD("+");
    // Create NormalStream
    VSDOF_LOGD("NormalStream create instance: idx=%d", miSensorIdx_Main1);
    CAM_TRACE_BEGIN("P2AFMNode::NormalStream::createInstance+init");
    mpINormalStream = NSCam::NSIoPipe::NSPostProc::INormalStream::createInstance(miSensorIdx_Main1);

    if (mpINormalStream == NULL)
    {
        MY_LOGE("mpINormalStream create instance for P2AFM Node failed!");
        cleanUp();
        return MFALSE;
    }
    mpINormalStream->init("VSDOF_P2AFM");
    CAM_TRACE_END();

    // BufferPoolSet init
    VSDOF_LOGD("BufferPoolSet init");
    CAM_TRACE_BEGIN("P2AFMNode::mBufPoolSet::init");
    mBufPoolSet.init(mBufConfig, mFlowType);
    CAM_TRACE_END();

    // 3A: create instance
    // UT does not test 3A
    CAM_TRACE_BEGIN("P2AFMNode::create_3A_instance");
    #ifndef GTEST
    mp3AHal_Main1 = MAKE_Hal3A(miSensorIdx_Main1, "VSDOF_3A_MAIN1");
    mp3AHal_Main2 = MAKE_Hal3A(miSensorIdx_Main2, "VSDOF_3A_MAIN2");
    MY_LOGD("3A create instance, Main1: %x Main2: %x", mp3AHal_Main1, mp3AHal_Main2);
    #endif
    CAM_TRACE_END();

    // prepare templates
    prepareTemplateParams();

    // prepare burst trigger QParams
    MBOOL bRet = prepareBurstQParams();
    if(!bRet)
        MY_LOGE("prepareBurstQParams Error! Please check the error msg above!");

    MY_LOGD("-");
    return MTRUE;
}

MVOID
P2AFMNode::
prepareTemplateParams()
{
    VSDOF_LOGD("+");
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

    // FE SRZ template - frame 2/4 need FEO srz crop
    mSrzSizeTemplateMap.add(2,  new _SRZ_SIZE_INFO_());
    mSrzSizeTemplateMap.add(4,  new _SRZ_SIZE_INFO_());
    CONFIG_SRZINFO_TO_CROPAREA(mSrzSizeTemplateMap.valueFor(2), mBufConfig.P2AFM_MAIN2_FEBO_AREA);
    CONFIG_SRZINFO_TO_CROPAREA(mSrzSizeTemplateMap.valueFor(4), mBufConfig.P2AFM_MAIN2_FECO_AREA);

    // FM tuning template
    // frame 6/8 - forward +  frame 7/9 - backward
    for(int frameID=6;frameID<10;++frameID)
    {
        // prepare FM tuning buffer
        SmartTuningBuffer smFMTuningBuffer = mBufPoolSet.mpTuningBufferPool->request();
        setupEmptyTuningWithFM(smFMTuningBuffer, frameID);
        dip_x_reg_t * pIspReg = (dip_x_reg_t *)smFMTuningBuffer->mpVA;
        MY_LOGD("frameID=%d mFMTuningBufferMap add!!DIP_X_FM_SIZE =%x DIP_X_FM_TH_CON0=%x ", frameID,
                pIspReg->DIP_X_FM_SIZE.Raw, pIspReg->DIP_X_FM_TH_CON0.Raw);
        mFMTuningBufferMap.add(frameID, smFMTuningBuffer);
    }

    // prepare FE tuning buffer
    for (int iStage=1;iStage<=2;++iStage)
    {
        SmartTuningBuffer smFETuningBuffer = mBufPoolSet.mpTuningBufferPool->request();
        MUINT32 iBlockSize = StereoSettingProvider::fefmBlockSize(iStage);
        setupEmptyTuningWithFE(smFETuningBuffer, iBlockSize);
        mFETuningBufferMap.add(iStage, smFETuningBuffer);
    }

    VSDOF_LOGD("-");
}

MVOID
P2AFMNode::
setupEmptyTuningWithFE(SmartTuningBuffer& targetTuningBuf, MUINT iBlockSize)
{
    dip_x_reg_t* isp_reg = reinterpret_cast<dip_x_reg_t*>(targetTuningBuf->mpVA);
    isp_reg->DIP_X_CTL_YUV_EN.Raw = 0;
    isp_reg->DIP_X_CTL_YUV2_EN.Raw = 0;
    isp_reg->DIP_X_CTL_RGB_EN.Raw = 0;
    // query tuning from tuning provider
    StereoTuningProvider::getFETuningInfo(isp_reg, iBlockSize);
}

MVOID
P2AFMNode::
setupEmptyTuningWithFM(SmartTuningBuffer& targetTuningBuf, MUINT iFrameID)
{
    MY_LOGD("+");
    dip_x_reg_t* isp_reg = reinterpret_cast<dip_x_reg_t*>(targetTuningBuf->mpVA);
    isp_reg->DIP_X_CTL_YUV_EN.Raw = 0;
    isp_reg->DIP_X_CTL_YUV2_EN.Raw = 0x00000001;
    isp_reg->DIP_X_CTL_RGB_EN.Raw = 0;

    MSize szFEBufSize = (iFrameID<=7) ? mBufConfig.P2AFM_FE1BO_INPUT_SIZE : mBufConfig.P2AFM_FE1CO_INPUT_SIZE;;
    MUINT32 iStage = (iFrameID<=7) ? 1 : 2;

    ENUM_FM_DIRECTION eDir = (iFrameID % 2 == 0) ? E_FM_L_TO_R : E_FM_R_TO_L;
    // query tuning parameter
    StereoTuningProvider::getFMTuningInfo(eDir, isp_reg->DIP_X_FM_SIZE, isp_reg->DIP_X_FM_TH_CON0);
    MUINT32 iBlockSize =  StereoSettingProvider::fefmBlockSize(iStage);
    // set width/height
    isp_reg->DIP_X_FM_SIZE.Bits.FM_WIDTH = szFEBufSize.w/iBlockSize;
    isp_reg->DIP_X_FM_SIZE.Bits.FM_HEIGHT = szFEBufSize.h/iBlockSize;

    MY_LOGD("frameID =%d DIP_X_FM_SIZE =%x DIP_X_FM_TH_CON0=%x", iFrameID, isp_reg->DIP_X_FM_SIZE.Raw, isp_reg->DIP_X_FM_TH_CON0.Raw);

    MY_LOGD("-");
}


MBOOL
P2AFMNode::
onUninit()
{
    CAM_TRACE_NAME("P2AFMNode::onUninit");
    cleanUp();
    return MTRUE;
}

MBOOL
P2AFMNode::
onData(DataID data, EffectRequestPtr &request)
{
  MBOOL ret = MTRUE;
  VSDOF_PRFLOG("+ : reqID=%d", request->getRequestNo());
  CAM_TRACE_NAME("P2AFMNode::onData");

  switch(data)
  {
  case ROOT_ENQUE:
    mRequestQue.enque(request);
    break;
  default:
    MY_LOGW("Un-recognized data ID, id=%d reqID=%d", data, request->getRequestNo());
    ret = MFALSE;
    break;
  }

  VSDOF_LOGD("-");
  return ret;
}

const char*
P2AFMNode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE1BO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE2BO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE1CO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FE2CO);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMBO_LR);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMBO_RL);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMCO_LR);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FMCO_RL);
        MAKE_NAME_CASE(BID_P2AFM_OUT_CC_IN1);
        MAKE_NAME_CASE(BID_P2AFM_OUT_CC_IN2);
        MAKE_NAME_CASE(BID_P2AFM_OUT_RECT_IN1_CAP);
        MAKE_NAME_CASE(BID_P2AFM_OUT_RECT_IN2_CAP);
        MAKE_NAME_CASE(BID_P2AFM_OUT_RECT_IN1);
        MAKE_NAME_CASE(BID_P2AFM_OUT_RECT_IN2);
        MAKE_NAME_CASE(BID_P2AFM_OUT_FDIMG);
        // MY_SLL
        MAKE_NAME_CASE(BID_TO_GF_MY_SL);
        MAKE_NAME_CASE(BID_TO_GF_MY_SLL);
        MAKE_NAME_CASE(BID_WMF_OUT_DMW);
        MAKE_NAME_CASE(BID_P2AFM_OUT_MY_S);
        #ifdef GTEST
        MAKE_NAME_CASE(BID_FE2_HWIN_MAIN1);
        MAKE_NAME_CASE(BID_FE2_HWIN_MAIN2);
        MAKE_NAME_CASE(BID_FE3_HWIN_MAIN1);
        MAKE_NAME_CASE(BID_FE3_HWIN_MAIN2);
        #endif


    }
    MY_LOGW("unknown BID:%d", BID);

    return "unknown";
#undef MAKE_NAME_CASE
}


#define SET_FULL_CROP_RS_INFO(cropInfo, inputBufSize, frameID) \
    cropInfo.mCropRect.p_fractional.x=0; \
    cropInfo.mCropRect.p_fractional.y=0; \
    cropInfo.mCropRect.p_integral.x=0; \
    cropInfo.mCropRect.p_integral.y=0; \
    cropInfo.mCropRect.s=inputBufSize; \
    cropInfo.mResizeDst=inputBufSize; \
    cropInfo.mFrameGroup = frameID;

#define SET_ZERO_CROP_RS_INFO(cropInfo, cropSize, inputBufSize, frameID) \
    cropInfo.mCropRect.p_fractional.x=0; \
    cropInfo.mCropRect.p_fractional.y=0; \
    cropInfo.mCropRect.p_integral.x=0; \
    cropInfo.mCropRect.p_integral.y=0; \
    cropInfo.mCropRect.s=cropSize; \
    cropInfo.mResizeDst=inputBufSize; \
    cropInfo.mFrameGroup = frameID;

#define SET_START_CROP_RS_INFO(cropInfo, startPoint, cropSize, inputBufSize, frameID) \
    cropInfo.mCropRect.p_fractional.x=0; \
    cropInfo.mCropRect.p_fractional.y=0; \
    cropInfo.mCropRect.p_integral.x=startPoint.x; \
    cropInfo.mCropRect.p_integral.y=startPoint.y; \
    cropInfo.mCropRect.s=cropSize; \
    cropInfo.mResizeDst=inputBufSize; \
    cropInfo.mFrameGroup = frameID;

MBOOL
P2AFMNode::
prepareBurstQParams()
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

    MBOOL bRet = prepareBurstQParams_CAP(iModuleTrans);
    bRet &= prepareBurstQParams_NORMAL(iModuleTrans);
    MY_LOGD("-");
    return bRet;
}


MBOOL
P2AFMNode::
prepareBurstQParams_NORMAL(MINT32 iModuleTrans)
{
    MY_LOGD("+");
    using namespace NSCam::NSIoPipe::NSPostProc;

    MBOOL bSuccess;
    //--> frame 0
    // dummy size will be filled with the correct value when runtime.
    MSize dummySize(0,0);
    MPoint zeroPos(0,0);
    {
        bSuccess =
            QParamTemplateGenerator(0, miSensorIdx_Main2, ENormalStreamTag_Normal).   // frame 0
                addInput(PORT_IMGI).
                addCrop(eCROP_WROT, zeroPos, dummySize, mBufConfig.P2AFM_FE2BO_INPUT_SIZE).     // WROT : FE2BO input
                addOutput(mapToPortID(BID_P2AFM_FE2B_INPUT), iModuleTrans).
                generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }

    //--> frame 1
    {
        bSuccess &=
            QParamTemplateGenerator(1, miSensorIdx_Main1, ENormalStreamTag_Normal, &mTemplStats_NORMAL).   // frame 1
                addInput(PORT_IMGI).
                addCrop(eCROP_CRZ, zeroPos, dummySize, mBufConfig.P2AFM_FD_IMG_SIZE).  // IMG2O : FD
                addOutput(mapToPortID(BID_P2AFM_OUT_FDIMG)).
                addCrop(eCROP_WDMA, zeroPos, dummySize, mBufConfig.P2AFM_MAIN_IMAGE_SIZE).  // WDMA : MV_F
                addOutput(mapToPortID(BID_P2AFM_OUT_MV_F)).
                addCrop(eCROP_WROT, zeroPos, dummySize, mBufConfig.P2AFM_FE1BO_INPUT_SIZE).  // WROT: FE1BO input
                addOutput(mapToPortID(BID_P2AFM_FE1B_INPUT), iModuleTrans).                // do module rotation
                generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }

    //--> frame 2
    {
        MVOID* srzInfo = reinterpret_cast<MVOID*>(mSrzSizeTemplateMap.valueFor(2));

        bSuccess &=
            QParamTemplateGenerator(2, miSensorIdx_Main2, ENormalStreamTag_Normal, &mTemplStats_NORMAL).   // frame 2
                addInput(PORT_IMGI).
                addCrop(eCROP_CRZ, zeroPos, mBufConfig.P2AFM_FE2BO_INPUT_SIZE, mBufConfig.P2AFM_FE2CO_INPUT_SIZE).  // IMG2O: FE2CO input
                addOutput(mapToPortID(BID_P2AFM_FE2C_INPUT)).
                addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE2BO_INPUT_SIZE, dummySize).  // WDMA : Rect_in2
                addOutput(mapToPortID(BID_P2AFM_OUT_RECT_IN2)).
                #ifdef GTEST
                addOutput(PORT_IMG3O).
                #endif
                addOutput(PORT_FEO).                           // FEO
                addModuleInfo(EDipModule_SRZ1, srzInfo).       // FEO SRZ1 config
                generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }

    //--> frame 3
    {
        QParamTemplateGenerator templateGen =
            QParamTemplateGenerator(3, miSensorIdx_Main1, ENormalStreamTag_Normal, &mTemplStats_NORMAL).   // frame 3
                addInput(PORT_IMGI).
                addCrop(eCROP_CRZ, zeroPos, mBufConfig.P2AFM_FE1BO_INPUT_SIZE, mBufConfig.P2AFM_FE1CO_INPUT_SIZE).  // IMG2O: FE1CO input
                addOutput(mapToPortID(BID_P2AFM_FE1C_INPUT)).
                addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE1BO_INPUT_SIZE, dummySize).  // WDMA : Rect_in1
                addOutput(mapToPortID(BID_P2AFM_OUT_RECT_IN1)).
                #ifdef GTEST
                addOutput(PORT_IMG3O).
                #endif
                addOutput(PORT_FEO);  // FEO

        // Queued flow type
        if(mFlowType == eDEPTH_FLOW_TYPE_QUEUED_DEPTH)
        {
            // add extra WROT output: EIS cropped MY_S for queued flow type
            templateGen.addCrop(eCROP_WROT, zeroPos, mBufConfig.P2AFM_FE1BO_INPUT_SIZE, dummySize).
                        addOutput(mapToPortID(BID_P2AFM_OUT_MY_S));
        }

        bSuccess &= templateGen.generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }

    //--> frame 4
    {
        MVOID* srzInfo = reinterpret_cast<MVOID*>(mSrzSizeTemplateMap.valueFor(4));
        bSuccess &=
        QParamTemplateGenerator(4, miSensorIdx_Main2, ENormalStreamTag_Normal, &mTemplStats_NORMAL).   // frame 4
            addInput(PORT_IMGI).
            addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE2CO_INPUT_SIZE, mBufConfig.P2AFM_CCIN_SIZE).  // WDMA: CC_in2
            addOutput(mapToPortID(BID_P2AFM_OUT_CC_IN2)).
            #ifdef GTEST
            addOutput(PORT_IMG3O).
            #endif
            addOutput(PORT_FEO).                           // FEO
            addModuleInfo(EDipModule_SRZ1, srzInfo).       // FEO SRZ1 config
            generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }


    //--> frame 5
    {
        bSuccess &=
        QParamTemplateGenerator(5, miSensorIdx_Main1, ENormalStreamTag_Normal, &mTemplStats_NORMAL).   // frame 5
            addInput(PORT_IMGI).
            addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE1CO_INPUT_SIZE, mBufConfig.P2AFM_CCIN_SIZE).  // WDMA: CC_in1
            addOutput(mapToPortID(BID_P2AFM_OUT_CC_IN1)).
            #ifdef GTEST
            addOutput(PORT_IMG3O).
            #endif
            addOutput(PORT_FEO).                           // FEO
            generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }

    //--> frame 6 ~ 9
    for(int frameID=6;frameID<10;++frameID)
    {
        bSuccess &=
        QParamTemplateGenerator(frameID, miSensorIdx_Main1, ENormalStreamTag_FM, &mTemplStats_NORMAL).
            addInput(PORT_DEPI).
            addInput(PORT_DMGI).
            addOutput(PORT_MFBO).
            generate(mBurstParamTmplate_NORMAL, mTemplStats_NORMAL);
    }

    //MY_LOGD("debugQParams mBurstParamTmplate_NORMAL:");
    //debugQParams(mBurstParamTmplate_NORMAL);

    MY_LOGD("-");
    return bSuccess;

}


MBOOL
P2AFMNode::
prepareBurstQParams_CAP(MINT32 iModuleTrans)
{
    MY_LOGD("+");
    using namespace NSCam::NSIoPipe::NSPostProc;

    // difference with PV : extra LSC2 buffer, different MV_F size, different Rect_in size
    bool bSuccess;
    //--> frame 0
    // dummy size will be filled with the correct value when runtime.
    MSize dummySize(0,0);
    MPoint zeroPos(0,0);
    {
        bSuccess =
        QParamTemplateGenerator(0, miSensorIdx_Main2, ENormalStreamTag_Normal).   // frame 0
            addInput(PORT_IMGI).
            addCrop(eCROP_WROT, zeroPos, dummySize, mBufConfig.P2AFM_FE2BO_INPUT_SIZE).  // WROT : FE2BO input
            addOutput(mapToPortID(BID_P2AFM_FE2B_INPUT), iModuleTrans).
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }

    //--> frame 1
    {

        bSuccess &=
        QParamTemplateGenerator(1, miSensorIdx_Main1, ENormalStreamTag_Normal, &mTemplStats_Cap).   // frame 1
            addInput(PORT_IMGI).
            #ifndef GTEST
            addInput(PORT_DEPI).    // Capture need extra LSC2 buffer input
            #endif
            addCrop(eCROP_CRZ, zeroPos, dummySize, mBufConfig.P2AFM_FD_IMG_SIZE).  // IMG2O : FD
            addOutput(mapToPortID(BID_P2AFM_OUT_FDIMG)).
            addCrop(eCROP_WDMA, zeroPos, dummySize, mBufConfig.P2AFM_MAIN_IMAGE_CAP_SIZE).  // WDMA : MV_F
            addOutput(mapToPortID(BID_P2AFM_OUT_MV_F)).
            addCrop(eCROP_WROT, zeroPos, dummySize, mBufConfig.P2AFM_FE1BO_INPUT_SIZE).  // WROT: FE1BO input
            addOutput(mapToPortID(BID_P2AFM_FE1B_INPUT), iModuleTrans).                // do module rotation
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }

    //--> frame 2
    {
        MVOID* srzInfo = reinterpret_cast<MVOID*>(mSrzSizeTemplateMap.valueFor(2));

        bSuccess &=
        QParamTemplateGenerator(2, miSensorIdx_Main2, ENormalStreamTag_Normal, &mTemplStats_Cap).   // frame 2
            addInput(PORT_IMGI).
            addCrop(eCROP_CRZ, zeroPos, mBufConfig.P2AFM_FE2BO_INPUT_SIZE, mBufConfig.P2AFM_FE2CO_INPUT_SIZE).  // IMG2O: FE2CO input
            addOutput(mapToPortID(BID_P2AFM_FE2C_INPUT)).
            addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE2BO_INPUT_SIZE, mBufConfig.P2AFM_RECT_IN_CAP_SIZE).  // WDMA : Rect_in2
            addOutput(mapToPortID(BID_P2AFM_OUT_RECT_IN2_CAP)).
            addOutput(PORT_FEO).                           // FEO
            addModuleInfo(EDipModule_SRZ1, srzInfo).       // FEO SRZ1 config
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }

    //--> frame 3
    {
        bSuccess &=
        QParamTemplateGenerator(3, miSensorIdx_Main1, ENormalStreamTag_Normal, &mTemplStats_Cap).   // frame 3
            addInput(PORT_IMGI).
            addCrop(eCROP_CRZ, zeroPos, mBufConfig.P2AFM_FE1BO_INPUT_SIZE, mBufConfig.P2AFM_FE1CO_INPUT_SIZE).  // IMG2O: FE1CO input
            addOutput(mapToPortID(BID_P2AFM_FE1C_INPUT)).
            addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE1BO_INPUT_SIZE, mBufConfig.P2AFM_RECT_IN_CAP_SIZE).  // WDMA : Rect_in1
            addOutput(mapToPortID(BID_P2AFM_OUT_RECT_IN1_CAP)).
            addOutput(PORT_FEO).                           // FEO
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }

    //--> frame 4
    {
        MVOID* srzInfo = reinterpret_cast<MVOID*>(mSrzSizeTemplateMap.valueFor(4));
        bSuccess &=
        QParamTemplateGenerator(4, miSensorIdx_Main2, ENormalStreamTag_Normal, &mTemplStats_Cap).   // frame 4
            addInput(PORT_IMGI).
            // CC_in change to WDMA
            //addCrop(eCROP_CRZ, zeroPos, mBufConfig.P2AFM_FE2CO_INPUT_SIZE, mBufConfig.P2AFM_CCIN_SIZE).  // IMG2O: CC_in2
            addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE2CO_INPUT_SIZE, mBufConfig.P2AFM_CCIN_SIZE).  // IMG2O: CC_in2
            addOutput(mapToPortID(BID_P2AFM_OUT_CC_IN2)).
            addOutput(PORT_FEO).                           // FEO
            addModuleInfo(EDipModule_SRZ1, srzInfo).       // FEO SRZ1 config
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }


    //--> frame 5
    {
        bSuccess &=
        QParamTemplateGenerator(5, miSensorIdx_Main1, ENormalStreamTag_Normal, &mTemplStats_Cap).   // frame 5
            addInput(PORT_IMGI).
            //addCrop(eCROP_CRZ, zeroPos, mBufConfig.P2AFM_FE1CO_INPUT_SIZE, mBufConfig.P2AFM_CCIN_SIZE).  // IMG2O: CC_in1
            addCrop(eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE1CO_INPUT_SIZE, mBufConfig.P2AFM_CCIN_SIZE).  // IMG2O: CC_in1
            addOutput(mapToPortID(BID_P2AFM_OUT_CC_IN1)).
            addOutput(PORT_FEO).                           // FEO
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }

    //--> frame 6 ~ 9
    for(int frameID=6;frameID<10;++frameID)
    {
        bSuccess &=
        QParamTemplateGenerator(frameID, miSensorIdx_Main1, ENormalStreamTag_FM, &mTemplStats_Cap).
            addInput(PORT_DEPI).
            addInput(PORT_DMGI).
            addOutput(PORT_MFBO).
            generate(mBurstParamTmplate_Cap, mTemplStats_Cap);
    }

    MY_LOGD("-");

    //MY_LOGD("debugQParams mBurstParamTmplate_Cap:");
    //debugQParams(mBurstParamTmplate_Cap);

    return bSuccess;
}

MBOOL
P2AFMNode::
buildQParams_NORMAL(EffectRequestPtr& rEffReqPtr, QParams& rEnqueParam, EnquedBufPool* pEnquePoolData, const TemplateStatistics& tmplStats)
{
    CAM_TRACE_NAME("P2AFMNode::buildQParams_NORMAL");
    VSDOF_PRFLOG("+, reqID=%d", rEffReqPtr->getRequestNo());
    // Get the input/output buffer inside the request
    FrameInfoPtr framePtr_inMain1RSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_P2AFM_IN_RSRAW1);
    FrameInfoPtr framePtr_inMain2RSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_P2AFM_IN_RSRAW2);
    FrameInfoPtr pFramePtr;
    sp<IImageBuffer> frameBuf_RSRAW1, frameBuf_RSRAW2, frameBuf_FD;
    SmartImageBuffer poolBuf, fe2boBuf_in, fe1boBuf_in,  fe2coBuf_in, fe1coBuf_in;
    SmartImageBuffer rectIn1Buf, rectIn2Buf, ccIn1Buf, ccIn2Buf, fDBuf;
    SmartImageBuffer feoBuf[6];
    SmartTuningBuffer tuningBuf;

    framePtr_inMain1RSRAW->getFrameBuffer(frameBuf_RSRAW1);
    framePtr_inMain2RSRAW->getFrameBuffer(frameBuf_RSRAW2);

    #define ADD_REQUEST_BUFFER_TO_MVOUT(bufID, reqBuf, filler, frameID, portID) \
        pEnquePoolData->addBuffData(bufID, reqBuf); \
        filler.insertOutputBuf(frameID, portID, reqBuf->mImageBuffer.get());

    #define ADD_REQUEST_BUFFER_WITH_AREA_TO_MVOUT(bufID, reqBuf, validSize, filler, frameID, portID) \
        pEnquePoolData->addBuffData(bufID, reqBuf); \
        reqBuf->mImageBuffer->setExtParam(validSize); \
        filler.insertOutputBuf(frameID, portID, reqBuf->mImageBuffer.get());

    QParamTemplateFiller qParamFiller(rEnqueParam, tmplStats);
    MPoint zeroPos(0,0);
    // Make sure the ordering inside the mvIn mvOut
    int mvInIndex = 0, mvOutIndex = 0;
    MUINT iFrameNum = 0;

    FrameInfoPtr framePtr_inAppMeta = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_APP);
    IMetadata* pMeta_InApp = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);
    FrameInfoPtr framePtr_inHalMeta_Main1 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL);

    FrameInfoPtr framePtr_inHalMeta;
    //--> frame 0
    {
        FrameInfoPtr framePtr_inHalMeta_Main2 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
         // Apply tuning data
        tuningBuf = mBufPoolSet.mpTuningBufferPool->request();
        pEnquePoolData->addTuningData(tuningBuf);
        // apply ISP tuning
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main2, mp3AHal_Main2, MTRUE};
        TuningParam rTuningParam = applyISPTuning(tuningBuf, ispConfig);

        qParamFiller.insertTuningBuf(iFrameNum, tuningBuf->mpVA).  // insert tuning data
                insertInputBuf(iFrameNum, PORT_IMGI, framePtr_inMain2RSRAW). // input: Main2 RSRAW
                setCrop(iFrameNum, eCROP_WROT, zeroPos, frameBuf_RSRAW2->getImgSize(), mBufConfig.P2AFM_FE2BO_INPUT_SIZE);

        // output: FE2BO input(WROT)
        fe2boBuf_in = mBufPoolSet.mpFEBOInBufPool_Main2->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE2B_INPUT, fe2boBuf_in, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_FE2B_INPUT));
    }

    //--> frame 1
    {
        iFrameNum = 1;

        // Apply tuning data
        tuningBuf = mBufPoolSet.mpTuningBufferPool->request();
        pEnquePoolData->addTuningData(tuningBuf);
        sp<IImageBuffer> frameBuf_MV_F;
        // Apply ISP tuning
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main1, mp3AHal_Main1, MTRUE};
        TuningParam rTuningParam = applyISPTuning(tuningBuf, ispConfig);

        qParamFiller.insertTuningBuf(iFrameNum, tuningBuf->mpVA). // insert tuning data
                insertInputBuf(iFrameNum, PORT_IMGI, framePtr_inMain1RSRAW); // input: Main1 RSRAW

        // output FD
        if(rEffReqPtr->vOutputFrameInfo.indexOfKey(BID_P2AFM_OUT_FDIMG) >= 0)
        {
             // output: FD
            RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(rEffReqPtr, pFramePtr, BID_P2AFM_OUT_FDIMG, frameBuf_FD);
            // insert FD output
            qParamFiller.insertOutputBuf(iFrameNum, PORT_IMG2O, frameBuf_FD.get()).
                    setCrop(iFrameNum, eCROP_CRZ, zeroPos, frameBuf_RSRAW1->getImgSize(), frameBuf_FD->getImgSize());
        }
        else
        {   // no FD-> remove port in template.
            qParamFiller.delOutputPort(iFrameNum, PORT_IMG2O, eCROP_CRZ);
        }
        // output: MV_F
        RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(rEffReqPtr, pFramePtr, BID_P2AFM_OUT_MV_F, frameBuf_MV_F);
        // check EIS on/off
        if (isEISOn(pMeta_InApp))
        {
            IMetadata* pMeta_InHal = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_Main1);
            eis_region region;
            // set MV_F crop for EIS
            if(queryEisRegion(pMeta_InHal, region, rEffReqPtr))
            {
                qParamFiller.setCrop(iFrameNum, eCROP_WDMA, MPoint(region.x_int, region.y_int), region.s, frameBuf_MV_F->getImgSize());
            }
            else
            {
                MY_LOGE("Query EIS Region Failed! reqID=%d.", rEffReqPtr->getRequestNo());
                return MFALSE;
            }
        }
        else
        {
            // MV_F crop
            qParamFiller.setCrop(iFrameNum, eCROP_WDMA, zeroPos, frameBuf_RSRAW1->getImgSize(), frameBuf_MV_F->getImgSize());
        }
        qParamFiller.insertOutputBuf(iFrameNum, PORT_WDMA, frameBuf_MV_F.get());
        // ouput: FE1BO_input
        fe1boBuf_in = mBufPoolSet.mpFEBOInBufPool_Main1->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE1B_INPUT, fe1boBuf_in, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_FE1B_INPUT));
        // FE1BO input crop
        qParamFiller.setCrop(iFrameNum, eCROP_WROT, zeroPos, frameBuf_RSRAW1->getImgSize(), mBufConfig.P2AFM_FE1BO_INPUT_SIZE);
    }

    //--> frame 2
    {
        iFrameNum = 2;
        // input: fe2boBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe2boBuf_in->mImageBuffer.get());
        // output: FE2CO input
        fe2coBuf_in = mBufPoolSet.mpFECOInBufPool_Main2->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE2C_INPUT, fe2coBuf_in, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_FE2C_INPUT));

         // output: Rect_in2
        {
            rectIn2Buf = isEISOn(pMeta_InApp) ?
                            mBufPoolSet.mpRectInVRBufPool_Main2->request():
                            mBufPoolSet.mpRectInPVBufPool_Main2->request();
            // Rect_in2 CROP
            qParamFiller.setCrop(iFrameNum, eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE2BO_INPUT_SIZE, rectIn2Buf->mImageBuffer->getImgSize());
            // Rect_in2
            ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_RECT_IN2, rectIn2Buf, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN2));
            #ifdef GTEST
            SmartImageBuffer smBuf_UT = mBufPoolSet.mFEInput_Stage2_Main2->request();
            ADD_REQUEST_BUFFER_TO_MVOUT(BID_FE2_HWIN_MAIN2, smBuf_UT, qParamFiller, iFrameNum, PORT_IMG3O);
            #endif
        }
        // output: FE2BO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOB_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE2BO, feoBuf[iFrameNum], qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FE2BO));

        // tuning data - apply Stage_1 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(1)->mpVA);
    }

    //--> frame 3
    {
        iFrameNum = 3;
        // input: fe1boBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe1boBuf_in->mImageBuffer.get());
        // output: FE1CO input
        fe1coBuf_in = mBufPoolSet.mpFECOInBufPool_Main1->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE1C_INPUT, fe1coBuf_in, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_FE1C_INPUT));

        // output: Rect_in1
        {
            rectIn1Buf = isEISOn(pMeta_InApp)?
                            mBufPoolSet.mpRectInVRBufPool->request():
                            mBufPoolSet.mpRectInPVBufPool->request();
            // Rect_in1 CROP
            qParamFiller.setCrop(iFrameNum, eCROP_WDMA, zeroPos, mBufConfig.P2AFM_FE1BO_INPUT_SIZE, rectIn1Buf->mImageBuffer->getImgSize());
            ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_RECT_IN1, rectIn1Buf, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN1));
        }
        // for QUEUED flow type
        if(mFlowType == eDEPTH_FLOW_TYPE_QUEUED_DEPTH)
        {
            // output: MY_S with EIS cropped for GF input
            if(isEISOn(pMeta_InApp))
            {
                eis_region region;
                IMetadata* pMeta_InHal = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_Main1);
                MBOOL bRes = queryRemappedEISRegion(rEffReqPtr, pMeta_InHal,
                                frameBuf_RSRAW1->getImgSize(), fe1boBuf_in->mImageBuffer->getImgSize(), region);
                if(!bRes)
                    return MFALSE;
                // MY_S buffer(240x136)
                SmartImageBuffer smMYSBuf = mBufPoolSet.mpRectInPVBufPool->request();
                qParamFiller.setCrop(iFrameNum, eCROP_WROT, MPoint(region.x_int, region.y_int), region.s, smMYSBuf->mImageBuffer->getImgSize());
                ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_MY_S, smMYSBuf, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_MY_S));
            }
            else
            {
                qParamFiller.delOutputPort(iFrameNum, PORT_WROT, eCROP_WROT);
            }
        }
        // output: FE1BO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOB_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE1BO, feoBuf[iFrameNum], qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FE1BO));

        #ifdef GTEST
        SmartImageBuffer smBuf_UT = mBufPoolSet.mFEInput_Stage2_Main1->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_FE2_HWIN_MAIN1, smBuf_UT, qParamFiller, iFrameNum, PORT_IMG3O);
        #endif
        // tuning data - apply Stage_1 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(1)->mpVA);
    }

    //--> frame 4
    {
        iFrameNum = 4;
        // input: fe2coBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe2coBuf_in->mImageBuffer.get());
        // output: CC_in
        ccIn2Buf = mBufPoolSet.mpCCInBufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_CC_IN2, ccIn2Buf, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_OUT_CC_IN2));
        // output: FE2CO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOC_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE2CO, feoBuf[iFrameNum], qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_OUT_FE2CO));

        #ifdef GTEST
        SmartImageBuffer smBuf_UT = mBufPoolSet.mFEInput_Stage3_Main2->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_FE3_HWIN_MAIN2, smBuf_UT, qParamFiller, iFrameNum, PORT_IMG3O);
        #endif
        // tuning data - apply Stage_2 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(2)->mpVA);
    }
    //--> frame 5
    {
        iFrameNum = 5;
        // input: fe1coBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe1coBuf_in->mImageBuffer.get());
        // output: CC_in
        ccIn1Buf = mBufPoolSet.mpCCInBufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_CC_IN1, ccIn1Buf, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_CC_IN1));
        // output: FE1CO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOC_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE1CO, feoBuf[iFrameNum], qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FE1CO));

        #ifdef GTEST
        SmartImageBuffer smBuf_UT = mBufPoolSet.mFEInput_Stage3_Main1->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_FE3_HWIN_MAIN1, smBuf_UT, qParamFiller, iFrameNum, PORT_IMG3O);
        #endif
        // tuning data - apply Stage_2 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(2)->mpVA);
    }

    // build FE/FM frame
    _buildFEFMFrame(qParamFiller, feoBuf, pEnquePoolData);

    VSDOF_PRFLOG("-, reqID=%d", rEffReqPtr->getRequestNo());

    MBOOL bRet = qParamFiller.validate();

    return bRet;
}
#undef ADD_REQUEST_BUFFER_WITH_AREA_TO_MVOUT
#undef ADD_REQUEST_BUFFER_TO_MVOUT

MBOOL
P2AFMNode::
queryRemappedEISRegion(
    EffectRequestPtr& rEffReqPtr,
    IMetadata* pMeta_InHal,
    MSize szEISDomain,
    MSize szRemappedDomain,
    eis_region& rOutRegion
)
{
    if(!queryEisRegion(pMeta_InHal, rOutRegion, rEffReqPtr))
    {
        MY_LOGE("Failed to query EIS region!");
        return MFALSE;
    }


    VSDOF_LOGD("Original query-EIS region: startPt=(%d.%d, %d.%d) Size=(%d, %d)",
            rOutRegion.x_int, rOutRegion.x_float, rOutRegion.y_int, rOutRegion.y_float,
            rOutRegion.s.w, rOutRegion.s.h);

    float ratio_width = szRemappedDomain.w * 1.0 / szEISDomain.w;
    float ratio_height = szRemappedDomain.h * 1.0 / szEISDomain.h;

    rOutRegion.x_int = (MUINT32) ceil(rOutRegion.x_int * ratio_width);
    rOutRegion.x_float = (MUINT32) ceil(rOutRegion.x_int * ratio_width);
    rOutRegion.y_int = (MUINT32) ceil(rOutRegion.y_int * ratio_height);
    rOutRegion.y_float = (MUINT32) ceil(rOutRegion.y_float * ratio_height);
    rOutRegion.s.w = ceil(rOutRegion.s.w * ratio_width);
    rOutRegion.s.h = ceil(rOutRegion.s.h * ratio_height);

    VSDOF_LOGD("Remapped query-EIS region: startPt=(%d.%d, %d.%d) Size=(%d, %d)",
            rOutRegion.x_int, rOutRegion.x_float, rOutRegion.y_int, rOutRegion.y_float,
            rOutRegion.s.w, rOutRegion.s.h);

    return MTRUE;
}



MBOOL
P2AFMNode::
buildQParams_CAP(EffectRequestPtr& rEffReqPtr, QParams& rEnqueParam, EnquedBufPool* pEnquePoolData, const TemplateStatistics& tmplStats)
{
    CAM_TRACE_NAME("P2AFMNode::buildQParams_CAP");
    VSDOF_PRFLOG("+, reqID=%d", rEffReqPtr->getRequestNo());
    // Get the input/output buffer inside the request
    FrameInfoPtr framePtr_inMain1FSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_P2AFM_IN_FSRAW1);
    FrameInfoPtr framePtr_inMain2RSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_P2AFM_IN_RSRAW2);
    FrameInfoPtr pFramePtr;
    sp<IImageBuffer> frameBuf_RSRAW2, frameBuf_FSRAW1, frameBuf_FD, frameBuf_JPSMain1;
    SmartImageBuffer poolBuf, fe2boBuf_in, fe1boBuf_in,  fe2coBuf_in, fe1coBuf_in;
    SmartImageBuffer rectIn1Buf, rectIn2Buf, ccIn1Buf, ccIn2Buf, fDBuf;
    SmartGraphicBuffer rectIn2Buf_gra;
    SmartImageBuffer feoBuf[6];
    SmartTuningBuffer tuningBuf;


    framePtr_inMain2RSRAW->getFrameBuffer(frameBuf_RSRAW2);
    framePtr_inMain1FSRAW->getFrameBuffer(frameBuf_FSRAW1);

    #define ADD_REQUEST_BUFFER_TO_MVOUT(bufID, reqBuf, filler, frameID, portID) \
        pEnquePoolData->addBuffData(bufID, reqBuf); \
        filler.insertOutputBuf(frameID, portID, reqBuf->mImageBuffer.get());

    #define ADD_REQUEST_BUFFER_WITH_AREA_TO_MVOUT(bufID, reqBuf, validSize, filler, frameID, portID) \
        pEnquePoolData->addBuffData(bufID, reqBuf); \
        reqBuf->mImageBuffer->setExtParam(validSize); \
        filler.insertOutputBuf(frameID, portID, reqBuf->mImageBuffer.get());

    QParamTemplateFiller qParamFiller(rEnqueParam, tmplStats);
    MPoint zeroPos(0,0);
    // Make sure the ordering inside the mvIn mvOut
    int mvInIndex = 0, mvOutIndex = 0;
    MUINT iFrameNum = 0;

    FrameInfoPtr framePtr_inAppMeta = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_APP);
    IMetadata* pMeta_InApp = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);

    FrameInfoPtr framePtr_inHalMeta;
    //--> frame 0
    {
        FrameInfoPtr framePtr_inHalMeta_Main2 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
         // Apply tuning data
        VSDOF_PRFLOG("+, reqID=%d  frame 0, request tuning buffer!", rEffReqPtr->getRequestNo());

        tuningBuf = mBufPoolSet.mpTuningBufferPool->request();

        VSDOF_PRFLOG("+, reqID=%d  frame 0, add tuning buffer!", rEffReqPtr->getRequestNo());
        pEnquePoolData->addTuningData(tuningBuf);
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main2, mp3AHal_Main2, MTRUE};
        TuningParam rTuningParam = applyISPTuning(tuningBuf, ispConfig);
        // insert tuning data
        qParamFiller.insertTuningBuf(iFrameNum, tuningBuf->mpVA).
            insertInputBuf(iFrameNum, PORT_IMGI, framePtr_inMain2RSRAW). // input: Main2 RSRAW
            setCrop(iFrameNum, eCROP_WROT, zeroPos, frameBuf_RSRAW2->getImgSize(), mBufConfig.P2AFM_FE2BO_INPUT_SIZE); // WROT crop

        // output: FE2BO input(WROT)
        fe2boBuf_in = mBufPoolSet.mpFEBOInBufPool_Main2->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE2B_INPUT, fe2boBuf_in, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_FE2B_INPUT));
    }

    //--> frame 1
    {
        iFrameNum = 1;
        // Apply tuning data
        tuningBuf = mBufPoolSet.mpTuningBufferPool->request();
        pEnquePoolData->addTuningData(tuningBuf);
        sp<IImageBuffer> frameBuf_MV_F;
        // Apply ISP tuning
        FrameInfoPtr framePtr_inHalMeta_Main1 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL);
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main1, mp3AHal_Main1, MFALSE};
        TuningParam rTuningParam = applyISPTuning(tuningBuf, ispConfig);
        // insert tuning data
        qParamFiller.insertTuningBuf(iFrameNum, tuningBuf->mpVA);

        // UT does not test 3A
        #ifndef GTEST
        if(rTuningParam.pLsc2Buf != NULL)
        {
            // input: LSC2 buffer (for tuning)
            IImageBuffer* pLSC2Src = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);
            qParamFiller.insertInputBuf(iFrameNum, PORT_DEPI, pLSC2Src);
        }
        else
        {
            MY_LOGE("LSC2 buffer from 3A is NULL!!");
            return MFALSE;
        }
        #endif

        // make sure the output is 16:9, get crop size& point
        MSize cropSize;
        MPoint startPoint;
        calCropForScreen(framePtr_inMain1FSRAW, startPoint, cropSize);

        // input: Main1 Fullsize RAW
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, framePtr_inMain1FSRAW);
        // output: FD - working buf
        fDBuf = mBufPoolSet.mpFDBufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FDIMG, fDBuf, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FDIMG));
        // FD crop
        qParamFiller.setCrop(iFrameNum, eCROP_CRZ, startPoint, cropSize, mBufConfig.P2AFM_FD_IMG_SIZE);
        // output : MV_F_CAP
        RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(rEffReqPtr, pFramePtr, BID_P2AFM_OUT_MV_F_CAP, frameBuf_MV_F);
        qParamFiller.insertOutputBuf(iFrameNum, PORT_WDMA, frameBuf_MV_F.get()).
                        setCrop(iFrameNum, eCROP_WDMA, startPoint, cropSize, frameBuf_MV_F->getImgSize());

        // ouput: FE1BO_input
        fe1boBuf_in = mBufPoolSet.mpFEBOInBufPool_Main1->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE1B_INPUT, fe1boBuf_in, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_FE1B_INPUT));
        // FE1BO input crop
        qParamFiller.setCrop(iFrameNum, eCROP_WROT, startPoint, cropSize, mBufConfig.P2AFM_FE1BO_INPUT_SIZE);
    }

    //--> frame 2
    {
        iFrameNum = 2;
        // input: fe2boBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe2boBuf_in->mImageBuffer.get());
        // output: FE2CO input
        fe2coBuf_in = mBufPoolSet.mpFECOInBufPool_Main2->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE2C_INPUT, fe2coBuf_in, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_FE2C_INPUT));
         // output: Rect_in2
        rectIn2Buf_gra = mBufPoolSet.mpRectInCapBufPool_Gra_Main2->request();
        ADD_REQUEST_BUFFER_WITH_AREA_TO_MVOUT(BID_P2AFM_OUT_RECT_IN2_CAP, rectIn2Buf_gra,
                            mBufConfig.P2AFM_RECT_IN_CAP_IMG_SIZE, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN2_CAP));
        // output: FE2BO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOB_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE2BO, feoBuf[iFrameNum], qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FE2BO));
        // tuning data - apply Stage_1 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(1)->mpVA);
    }

    //--> frame 3
    {
        iFrameNum = 3;
        // input: fe1boBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe1boBuf_in->mImageBuffer.get());
        // output: FE1CO input
        fe1coBuf_in = mBufPoolSet.mpFECOInBufPool_Main1->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_FE1C_INPUT, fe1coBuf_in, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_FE1C_INPUT));

        // output: Rect_in1
        // Capture: Rect_in1 use the JPSMain1 frame buffer
        RETRIEVE_OFRAMEINFO_IMGBUF_WITH_ERROR(rEffReqPtr, pFramePtr, BID_N3D_OUT_JPS_MAIN1, frameBuf_JPSMain1);
        // set active area
        frameBuf_JPSMain1->setExtParam(mBufConfig.P2AFM_RECT_IN_CAP_IMG_SIZE);
        qParamFiller.insertOutputBuf(iFrameNum, mapToPortID(BID_P2AFM_OUT_RECT_IN1_CAP), frameBuf_JPSMain1.get());

        // output: FE1BO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOB_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE1BO, feoBuf[iFrameNum], qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FE1BO));

        // tuning data - apply Stage_1 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(1)->mpVA);
    }

    //--> frame 4
    {
        iFrameNum = 4;
        // input: fe2coBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe2coBuf_in->mImageBuffer.get());
        // output: CC_in
        ccIn2Buf = mBufPoolSet.mpCCInBufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_CC_IN2, ccIn2Buf, qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_OUT_CC_IN2));
        // output: FE2CO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOC_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE2CO, feoBuf[iFrameNum], qParamFiller, iFrameNum,
                                    mapToPortID(BID_P2AFM_OUT_FE2CO));

        // tuning data - apply Stage_2 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(2)->mpVA);
    }
    //--> frame 5
    {
        iFrameNum = 5;
        // input: fe1coBuf_in
        qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, fe1coBuf_in->mImageBuffer.get());
        // output: CC_in
        ccIn1Buf = mBufPoolSet.mpCCInBufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_CC_IN1, ccIn1Buf, qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_CC_IN1));
        // output: FE1CO
        feoBuf[iFrameNum] = mBufPoolSet.mpFEOC_BufPool->request();
        ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FE1CO, feoBuf[iFrameNum], qParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FE1CO));

        // tuning data - apply Stage_2 cached FE tuning buffer
        qParamFiller.insertTuningBuf(iFrameNum, mFETuningBufferMap.valueFor(2)->mpVA);
    }

    // build FE/FM frame
    _buildFEFMFrame(qParamFiller, feoBuf, pEnquePoolData);

    VSDOF_PRFLOG("-, reqID=%d", rEffReqPtr->getRequestNo());

    MBOOL bRet = qParamFiller.validate();

    return bRet;
}


MVOID
P2AFMNode::
_buildFEFMFrame(QParamTemplateFiller& rQParamFiller, SmartImageBuffer feoBuf[], EnquedBufPool* pEnquePoolData)
{
    #define ADD_REQUEST_BUFFER_TO_MVOUT(bufID, reqBuf, filler, frameID, portID) \
        pEnquePoolData->addBuffData(bufID, reqBuf); \
        filler.insertOutputBuf(frameID, portID, reqBuf->mImageBuffer.get());

    #define ADD_REQUEST_BUFFER_WITH_AREA_TO_MVOUT(bufID, reqBuf, validSize, filler, frameID, portID) \
        pEnquePoolData->addBuffData(bufID, reqBuf); \
        reqBuf->mImageBuffer->setExtParam(validSize); \
        filler.insertOutputBuf(frameID, portID, reqBuf->mImageBuffer.get());

    //--> frame 6: FM - L to R
    int iFrameNum = 6;
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_LSide[0]]->mImageBuffer.get());
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_RSide[0]]->mImageBuffer.get());
    SmartImageBuffer fmoBuf = mBufPoolSet.mpFMOB_BufPool->request();
    ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FMBO_LR, fmoBuf, rQParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FMBO_LR));
    SmartTuningBuffer smFMTuningBuf = mFMTuningBufferMap.valueFor(iFrameNum);
    rQParamFiller.insertTuningBuf(iFrameNum, smFMTuningBuf->mpVA);

    //--> frame 7: FM - R to L
    iFrameNum = 7;
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_RSide[0]]->mImageBuffer.get());
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_LSide[0]]->mImageBuffer.get());
    fmoBuf = mBufPoolSet.mpFMOB_BufPool->request();
    ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FMBO_RL, fmoBuf, rQParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FMBO_RL));
    smFMTuningBuf = mFMTuningBufferMap.valueFor(iFrameNum);
    rQParamFiller.insertTuningBuf(iFrameNum, smFMTuningBuf->mpVA);

    //--> frame 8: FM - L to R
    iFrameNum = 8;
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_LSide[1]]->mImageBuffer.get());
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_RSide[1]]->mImageBuffer.get());
    fmoBuf = mBufPoolSet.mpFMOC_BufPool->request();
    ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FMCO_LR, fmoBuf, rQParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FMCO_LR));
    smFMTuningBuf = mFMTuningBufferMap.valueFor(iFrameNum);
    rQParamFiller.insertTuningBuf(iFrameNum, smFMTuningBuf->mpVA);

    ///--> frame 9: FM
    iFrameNum = 9;
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DEPI, feoBuf[frameIdx_RSide[1]]->mImageBuffer.get());
    rQParamFiller.insertInputBuf(iFrameNum, PORT_DMGI, feoBuf[frameIdx_LSide[1]]->mImageBuffer.get());
    fmoBuf = mBufPoolSet.mpFMOC_BufPool->request();
    ADD_REQUEST_BUFFER_TO_MVOUT(BID_P2AFM_OUT_FMCO_RL, fmoBuf, rQParamFiller, iFrameNum, mapToPortID(BID_P2AFM_OUT_FMCO_RL));
    smFMTuningBuf = mFMTuningBufferMap.valueFor(iFrameNum);
    rQParamFiller.insertTuningBuf(iFrameNum, smFMTuningBuf->mpVA);
}

MBOOL
P2AFMNode::
calCropForScreen(FrameInfoPtr& pFrameInfo, MPoint &rCropStartPt, MSize& rCropSize )
{
    sp<IImageBuffer> pImgBuf;
    pFrameInfo->getFrameBuffer(pImgBuf);
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

TuningParam
P2AFMNode::
applyISPTuning(
        SmartTuningBuffer& targetTuningBuf,
        const ISPTuningConfig& ispConfig
)
{
    CAM_TRACE_NAME("P2AFMNode::applyISPTuning");
    VSDOF_PRFLOG("+, reqID=%d bIsResized=%d", ispConfig.pInAppMetaFrame->getRequestNo(), ispConfig.bInputResizeRaw);

    TuningParam tuningParam = {NULL, NULL};
    tuningParam.pRegBuf = reinterpret_cast<void*>(targetTuningBuf->mpVA);

    MetaSet_T inMetaSet;
    IMetadata* pMeta_InApp  = getMetadataFromFrameInfoPtr(ispConfig.pInAppMetaFrame);
    IMetadata* pMeta_InHal  = getMetadataFromFrameInfoPtr(ispConfig.pInHalMetaFrame);

    inMetaSet.appMeta = *pMeta_InApp;
    inMetaSet.halMeta = *pMeta_InHal;

    // USE resize raw-->set PGN 0
    if(ispConfig.bInputResizeRaw)
        updateEntry<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 0);
    else
        updateEntry<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 1);

    SimpleTimer timer(true);
    // UT do not test setIsp
    #ifndef GTEST
    MetaSet_T resultMeta;
    ispConfig.p3AHAL->setIsp(0, inMetaSet, &tuningParam, &resultMeta);
    // write ISP resultMeta to input hal Meta
   (*pMeta_InHal) += resultMeta.halMeta;
    #else
    {   // UT: use default tuning
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_G2G, 0);
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_G2C, 0);
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_GGM, 0);
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_UDM, 0);
    }
    #endif

    VSDOF_PRFLOG("-, reqID=%d setIsp_time=%f", ispConfig.pInAppMetaFrame->getRequestNo(), timer.countTimer());

    return tuningParam;
}

MBOOL
P2AFMNode::
onThreadLoop()
{
    EffectRequestPtr request;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }

    if( !mRequestQue.deque(request) )
    {
        MY_LOGE("mRequestQue.deque() failed");
        return MFALSE;
    }

    CAM_TRACE_NAME("P2AFMNode::onThreadLoop");

    // mark on-going-request start
    this->incExtThreadDependency();
    //prepare enque parameter
    QParams enqueParams;
    EnquedBufPool *enquedData = new EnquedBufPool(request, this);

    // get current eState
    DepthNodeOpState eState = getRequestState(request);

    VSDOF_PRFLOG("threadLoop start, reqID=%d eState=%d", request->getRequestNo(), eState);

    TemplateStatistics tmplStats;
    MBOOL bRet;

    // copy the corresponding QParams template
    if(eState == eSTATE_CAPTURE)
    {
        enqueParams = mBurstParamTmplate_Cap;
        tmplStats = mTemplStats_Cap;
        bRet = buildQParams_CAP(request, enqueParams, enquedData, tmplStats);
    }
    else
    {
        enqueParams = mBurstParamTmplate_NORMAL;
        tmplStats = mTemplStats_NORMAL;
        bRet = buildQParams_NORMAL(request, enqueParams, enquedData, tmplStats);
    }
    debugQParams(enqueParams);
    if(!bRet)
    {
        MY_LOGE("Failed to build P2 enque parametes.");
        goto lbExit;
    }

    // callback
    enqueParams.mpfnCallback = onP2Callback;
    enqueParams.mpfnEnQFailCallback = onP2FailedCallback;
    enqueParams.mpCookie = (MVOID*) enquedData;

    VSDOF_PRFLOG("mpINormalStream enque start! reqID=%d t=%d", request->getRequestNo(), enquedData->startTimer());

    CAM_TRACE_BEGIN("P2AFMNode::NormalStream::enque");
    bRet = mpINormalStream->enque(enqueParams);
    CAM_TRACE_END();

    VSDOF_PRFLOG("mpINormalStream enque end! reqID=%d, exec-time(enque)=%f msec", request->getRequestNo(), enquedData->countTimer());
    if(!bRet)
    {
        MY_LOGE("mpINormalStream enque failed! reqID=%d", request->getRequestNo());
        goto lbExit;
    }

    VSDOF_PRFLOG("threadLoop end! reqID=%d", request->getRequestNo());

    return MTRUE;

lbExit:
    delete enquedData;
    // mark on-going-request end
    this->decExtThreadDependency();
    return MFALSE;

}

MVOID
P2AFMNode::
onP2Callback(QParams& rParams)
{
    EnquedBufPool* pEnqueData = (EnquedBufPool*) (rParams.mpCookie);
    P2AFMNode* pP2AFMNode = (P2AFMNode*) (pEnqueData->mpNode);
    pP2AFMNode->handleP2Done(rParams, pEnqueData);
}

MVOID
P2AFMNode::
onP2FailedCallback(QParams& rParams)
{
    MY_LOGE("P2A operations failed!!Check the following log:");
    EnquedBufPool* pEnqueData = (EnquedBufPool*) (rParams.mpCookie);
    P2AFMNode* pP2AFMNode = (P2AFMNode*) (pEnqueData->mpNode);
    pP2AFMNode->debugQParams(rParams);
    pP2AFMNode->handleData(ERROR_OCCUR_NOTIFY, pEnqueData->mRequest);
    delete pEnqueData;
}

MVOID
P2AFMNode::
handleP2Done(QParams& rParams, EnquedBufPool* pEnqueBufPool)
{
    CAM_TRACE_NAME("P2AFMNode::handleP2Done");
    EffectRequestPtr request = pEnqueBufPool->mRequest;
    VSDOF_PRFLOG("+ :reqID=%d , p2 exec-time=%f", request->getRequestNo(), pEnqueBufPool->countTimer());
    DepthNodeOpState eState = getRequestState(request);
    FrameInfoPtr framePtr;
    // launch flow type handle function
    onHandleFlowTypeP2Done(pEnqueBufPool);

    #define MARK_OUTFRAME_READY_AND_HANDLEDATA(BID, DataID)\
        if (request->vOutputFrameInfo.indexOfKey(BID) >= 0)\
        {\
            framePtr = request->vOutputFrameInfo.valueFor(BID);\
            framePtr->setFrameReady(true);\
            handleDataAndDump(DataID, framePtr, eState);\
        }
    // FD
    MARK_OUTFRAME_READY_AND_HANDLEDATA(BID_P2AFM_OUT_FDIMG, P2AFM_OUT_FD);
    // MainImage
    MARK_OUTFRAME_READY_AND_HANDLEDATA(BID_P2AFM_OUT_MV_F, P2AFM_OUT_MV_F);
    // MainImage Capture
    MARK_OUTFRAME_READY_AND_HANDLEDATA(BID_P2AFM_OUT_MV_F_CAP, P2AFM_OUT_MV_F_CAP);

    // prepare data for N3D input
    SmartImageBuffer smBuf;
    #define ADD_ENQUE_BUFF_TO_INFO_MAP(infoMap, enqueBufPool, BID)\
        smBuf = enqueBufPool->mEnquedSmartImgBufMap.valueFor(BID); \
        infoMap->addImageBuffer(BID, smBuf);
    // FEO/FMO
    sp<ImageBufInfoMap> TON3D_ImgBufInfo = new ImageBufInfoMap(request);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FE1BO);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FE2BO);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FE1CO);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FE2CO);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FMBO_LR);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FMBO_RL);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FMCO_LR);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FMCO_RL);
    // CC_in
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_CC_IN1);
    ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_CC_IN2);
    // Rect_in
    if(eState == eSTATE_CAPTURE)
    {
        // Rect_in1: use JPSMain1, mark ready
        RETRIEVE_OFRAMEINFO(request, framePtr, BID_N3D_OUT_JPS_MAIN1);
        framePtr->setFrameReady(true);
        handleDump(P2AFM_TO_N3D_FEFM_CCin, framePtr, eState, "BID_P2AFM_OUT_RECT_IN1_CAP");
        // Rect_in2: graphic buffers
        SmartGraphicBuffer pGraBuf = pEnqueBufPool->mEnquedSmartGraImgBufMap.valueFor(BID_P2AFM_OUT_RECT_IN2_CAP);
        TON3D_ImgBufInfo->addGraphicBuffer(BID_P2AFM_OUT_RECT_IN2_CAP, pGraBuf);
    }
    else
    {
        ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_RECT_IN1);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TON3D_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_RECT_IN2);
    }
    // pass to N3D
    handleDataAndDump(P2AFM_TO_N3D_FEFM_CCin, TON3D_ImgBufInfo);
    // if capture
    if(eState == eSTATE_CAPTURE)
    {
        // pass to WMF node
        sp<ImageBufInfoMap> TO_GF_ImgBufInfo = new ImageBufInfoMap(request);
        // BID_P2AFM_FE1C_INPUT is BID_TO_GF_MY_SL, BID_P2AFM_FE1B_INPUT is BID_TO_GF_MY_SLL
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_GF_ImgBufInfo, pEnqueBufPool, BID_TO_GF_MY_SLL);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_GF_ImgBufInfo, pEnqueBufPool, BID_TO_GF_MY_SL);
        handleDataAndDump(P2AFM_TO_GF_MY_SL, TO_GF_ImgBufInfo);
        // pass to FD node
        sp<ImageBufInfoMap> TO_FD_ImgBufInfo = new ImageBufInfoMap(request);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_FD_ImgBufInfo, pEnqueBufPool, BID_P2AFM_OUT_FDIMG);
        handleDataAndDump(P2AFM_TO_FD_IMG, TO_FD_ImgBufInfo);
    }

    // dumping buffers
    dumpingP2Buffers(pEnqueBufPool);
    // release enque cookie
    delete pEnqueBufPool;
    VSDOF_PRFLOG("- :reqID=%d", request->getRequestNo());
    // mark on-going-request end
    this->decExtThreadDependency();

    #undef MARK_OUTFRAME_READY_AND_HANDLEDATA
    #undef ADD_ENQUE_BUFF_TO_INFO_MAP
}

MVOID
P2AFMNode::
onHandleFlowTypeP2Done(EnquedBufPool* pEnqueBufPool)
{
    // pass to GF if QUEUED flow and BID_P2AFM_OUT_MY_S
    ssize_t idx;
    EffectRequestPtr pRequest = pEnqueBufPool->mRequest;
    if(this->isQueuedDepthRequest(pRequest))
    {
        // get queue depth info
        DepthBufferInfo depthInfo;
        if(mpDepthStorage->pop_front(depthInfo)) {
            mLastQueueDepthInfo = depthInfo;
            VSDOF_LOGD("reqID=%d use queued depth info!",
                        pRequest->getRequestNo());
        }else
        {
            depthInfo = mLastQueueDepthInfo;
            VSDOF_LOGD("reqID=%d, no queued depth, use last depth info!",
                        pRequest->getRequestNo());
        }

        // create to GF image buffer info
        sp<ImageBufInfoMap> TOGF_ImgBufInfo = new ImageBufInfoMap(pRequest);
        // add Depth
        TOGF_ImgBufInfo->addImageBuffer(BID_WMF_OUT_DMW, depthInfo.mpDepthBuffer);
        // get GF input MY_S buffer BID, depend on the EIS status
        FrameInfoPtr framePtr = pRequest->vInputFrameInfo.valueFor(BID_META_IN_APP);
        IMetadata* pMetadata = getMetadataFromFrameInfoPtr(framePtr);
        DepthMapBufferID bid_GfInputMYS;
        if(isEISOn(pMetadata))
            bid_GfInputMYS = BID_P2AFM_OUT_MY_S;
        else
            bid_GfInputMYS = BID_P2AFM_OUT_RECT_IN1;
        // retrieve buffer
        SmartImageBuffer smMYS = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(bid_GfInputMYS);
        TOGF_ImgBufInfo->addImageBuffer(BID_P2AFM_OUT_MY_S, smMYS);
        // write meta
        {
            FrameInfoPtr outMeta_frameInfo = pRequest->vOutputFrameInfo.valueFor(BID_META_OUT_HAL);
            pMetadata = getMetadataFromFrameInfoPtr(outMeta_frameInfo);
            VSDOF_LOGD("output convOffset:%f", depthInfo.mfConvOffset);
            trySetMetadata<MFLOAT>(pMetadata, MTK_CONVERGENCE_DEPTH_OFFSET, depthInfo.mfConvOffset);
        }
        // set out meta ready
        pRequest->vOutputFrameInfo.valueFor(BID_META_OUT_APP)->setFrameReady(true);
        pRequest->vOutputFrameInfo.valueFor(BID_META_OUT_HAL)->setFrameReady(true);
        pRequest->vOutputFrameInfo.valueFor(BID_META_OUT_APP_QUEUED)->setFrameReady(true);
        pRequest->vOutputFrameInfo.valueFor(BID_META_OUT_HAL_QUEUED)->setFrameReady(true);
        // pass to GF
        handleDataAndDump(P2AFM_TO_GF_DMW_MYS, TOGF_ImgBufInfo);
    }
}

MVOID
P2AFMNode::
dumpingP2Buffers(EnquedBufPool* pEnqueBufPool)
{
    EffectRequestPtr request = pEnqueBufPool->mRequest;
    DepthNodeOpState eState = getRequestState(request);
    SmartImageBuffer smBuf;

    #define ADD_ENQUE_BUFF_TO_INFO_MAP(infoMap, enqueBufPool, BID)\
        smBuf = enqueBufPool->mEnquedSmartImgBufMap.valueFor(BID); \
        infoMap->addImageBuffer(BID, smBuf);

    // Dump P1Raw
    if(checkToDump(TO_DUMP_RAWS))
    {
        FrameInfoPtr framePtr;
        sp<IImageBuffer> frameBuf;
        char postfix[50]={'\0'};
        if(request->vInputFrameInfo.indexOfKey(BID_P2AFM_IN_FSRAW1)>=0)
        {
            framePtr = request->vInputFrameInfo.valueFor(BID_P2AFM_IN_FSRAW1);
            framePtr->getFrameBuffer(frameBuf);
            snprintf(postfix, 50, "_%d.raw", frameBuf->getBufStridesInBytes(0));
            handleDump(TO_DUMP_RAWS, framePtr, eState, "BID_P2AFM_IN_FSRAW1", postfix);
        }
        if(request->vInputFrameInfo.indexOfKey(BID_P2AFM_IN_FSRAW2)>=0)
        {
            framePtr = request->vInputFrameInfo.valueFor(BID_P2AFM_IN_FSRAW2);
            framePtr->getFrameBuffer(frameBuf);
            snprintf(postfix, 50, "_%d.raw", frameBuf->getBufStridesInBytes(0));
            handleDump(TO_DUMP_RAWS, framePtr, eState, "BID_P2AFM_IN_FSRAW2", postfix);
        }
        if(request->vInputFrameInfo.indexOfKey(BID_P2AFM_IN_RSRAW1)>=0)
        {
            framePtr = request->vInputFrameInfo.valueFor(BID_P2AFM_IN_RSRAW1);
            framePtr->getFrameBuffer(frameBuf);
            snprintf(postfix, 50, "_%d.raw", frameBuf->getBufStridesInBytes(0));
            handleDump(TO_DUMP_RAWS, framePtr, eState, "BID_P2AFM_IN_RSRAW1", postfix);
        }
        if(request->vInputFrameInfo.indexOfKey(BID_P2AFM_IN_RSRAW2)>=0)
        {
            framePtr = request->vInputFrameInfo.valueFor(BID_P2AFM_IN_RSRAW2);
            framePtr->getFrameBuffer(frameBuf);
            snprintf(postfix, 50, "_%d.raw", frameBuf->getBufStridesInBytes(0));
            handleDump(TO_DUMP_RAWS, framePtr, eState, "BID_P2AFM_IN_RSRAW2", postfix);
        }
    }

    #ifdef GTEST
    if(eState == eSTATE_NORMAL)
    {
        //dump buffers
        sp<ImageBufInfoMap> UTDump_ImgBufInfo = new ImageBufInfoMap(request);
        ADD_ENQUE_BUFF_TO_INFO_MAP(UTDump_ImgBufInfo, pEnqueBufPool, BID_FE2_HWIN_MAIN1);
        ADD_ENQUE_BUFF_TO_INFO_MAP(UTDump_ImgBufInfo, pEnqueBufPool, BID_FE2_HWIN_MAIN2);
        ADD_ENQUE_BUFF_TO_INFO_MAP(UTDump_ImgBufInfo, pEnqueBufPool, BID_FE3_HWIN_MAIN1);
        ADD_ENQUE_BUFF_TO_INFO_MAP(UTDump_ImgBufInfo, pEnqueBufPool, BID_FE3_HWIN_MAIN2);
        handleDump(UT_OUT_FE, UTDump_ImgBufInfo);
    }
    #endif

     // dump extra buffers
    sp<ImageBufInfoMap> ToDump_ImgBufInfo = new ImageBufInfoMap(request);
    ADD_ENQUE_BUFF_TO_INFO_MAP(ToDump_ImgBufInfo, pEnqueBufPool, BID_P2AFM_FE2B_INPUT);
    handleDump(TO_DUMP_BUFFERS, ToDump_ImgBufInfo, "P2AFM_OUT_MV_F_MAIN2");

    #undef ADD_ENQUE_BUFF_TO_INFO_MAP
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2AFMNode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
P2AFMNode::
onThreadStop()
{
    return MTRUE;
}

MVOID
P2AFMNode::
setSensorIdx(MUINT32 main1_idx, MUINT32 main2_idx)
{
    VSDOF_LOGD("setSensorIdx main1=%d main2=%d", main1_idx, main2_idx);
    miSensorIdx_Main1 = main1_idx;
    miSensorIdx_Main2 = main2_idx;
}

NSCam::NSIoPipe::PortID
P2AFMNode::
mapToPortID(const MUINT32 nodeDataType)
{
    switch(nodeDataType)
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

        case BID_P2AFM_OUT_FE1BO:
        case BID_P2AFM_OUT_FE2BO:
        case BID_P2AFM_OUT_FE1CO:
        case BID_P2AFM_OUT_FE2CO:
            return PORT_FEO;

        case BID_P2AFM_OUT_RECT_IN1:
        case BID_P2AFM_OUT_RECT_IN2:
        case BID_P2AFM_OUT_RECT_IN2_CAP:
        case BID_P2AFM_OUT_RECT_IN1_CAP:
        case BID_P2AFM_OUT_MV_F:
        case BID_P2AFM_OUT_MV_F_CAP:
            return PORT_WDMA;

        case BID_P2AFM_FE1B_INPUT:
        case BID_P2AFM_FE2B_INPUT:
        case BID_P2AFM_OUT_MY_S:
            return PORT_WROT;

        case BID_P2AFM_OUT_CC_IN1:
        case BID_P2AFM_OUT_CC_IN2:
            return PORT_WDMA;

        case BID_P2AFM_OUT_FMBO_LR:
        case BID_P2AFM_OUT_FMBO_RL:
        case BID_P2AFM_OUT_FMCO_LR:
        case BID_P2AFM_OUT_FMCO_RL:
            return PORT_MFBO;

        default:
            MY_LOGE("mapToPortID: not exist nodeDataType=%d", nodeDataType);
            break;
    }

    return NSCam::NSIoPipe::PortID();
}

MVOID P2AFMNode::debugQParams(const QParams& rInputQParam)
{
    if(!mbDebugLog)
        return;

    MY_LOGD("debugQParams start");
    MY_LOGD("Size of mvStreamTag=%d, mvTuningData=%d mvIn=%d mvOut=%d, mvCropRsInfo=%d, mvModuleData=%d\n",
                rInputQParam.mvStreamTag.size(), rInputQParam.mvTuningData.size(), rInputQParam.mvIn.size(),
                rInputQParam.mvOut.size(), rInputQParam.mvCropRsInfo.size(), rInputQParam.mvModuleData.size());

    MY_LOGD("mvStreamTag section");
    for(size_t i=0;i<rInputQParam.mvStreamTag.size(); i++)
    {
        MY_LOGD("Index = %d mvStreamTag = %d", i, rInputQParam.mvStreamTag[i]);
    }

    MY_LOGD("mvTuningData section");
    for(size_t i=0;i<rInputQParam.mvTuningData.size(); i++)
    {
        dip_x_reg_t* data = (dip_x_reg_t*) rInputQParam.mvTuningData[i];
        MY_LOGD("========\nIndex = %d", i);

        MY_LOGD("DIP_X_FE_CTRL1.Raw = %x", data->DIP_X_FE_CTRL1.Raw);
        MY_LOGD("DIP_X_FE_IDX_CTRL.Raw = %x", data->DIP_X_FE_IDX_CTRL.Raw);
        MY_LOGD("DIP_X_FE_CROP_CTRL1.Raw = %x", data->DIP_X_FE_CROP_CTRL1.Raw);
        MY_LOGD("DIP_X_FE_CROP_CTRL2.Raw = %x", data->DIP_X_FE_CROP_CTRL2.Raw);
        MY_LOGD("DIP_X_FE_CTRL2.Raw = %x", data->DIP_X_FE_CTRL2.Raw);
        MY_LOGD("DIP_X_FM_SIZE.Raw = %x", data->DIP_X_FM_SIZE.Raw);
        MY_LOGD("DIP_X_FM_TH_CON0.Raw = %x", data->DIP_X_FM_TH_CON0.Raw);
    }

    MY_LOGD("mvIn section");
    for(size_t i=0;i<rInputQParam.mvIn.size(); i++)
    {
        Input data = rInputQParam.mvIn[i];
        MY_LOGD("========\nIndex = %d", i);

        MY_LOGD("mvIn.PortID.index = %d", data.mPortID.index);
        MY_LOGD("mvIn.PortID.type = %d", data.mPortID.type);
        MY_LOGD("mvIn.PortID.inout = %d", data.mPortID.inout);
        MY_LOGD("mvIn.PortID.group = %d", data.mPortID.group);

        MY_LOGD("mvIn.mBuffer=%x", data.mBuffer);
        if(data.mBuffer !=NULL)
        {
            MY_LOGD("mvIn.mBuffer->getImgSize = %dx%d", data.mBuffer->getImgSize().w,
                                                data.mBuffer->getImgSize().h);

            MY_LOGD("mvIn.mBuffer->getImgFormat = %x", data.mBuffer->getImgFormat());
            MY_LOGD("mvIn.mBuffer->getPlaneCount = %d", data.mBuffer->getPlaneCount());
            for(int j=0;j<data.mBuffer->getPlaneCount();j++)
            {
                MY_LOGD("mvIn.mBuffer->getBufVA(%d) = %X", j, data.mBuffer->getBufVA(j));
                MY_LOGD("mvIn.mBuffer->getBufStridesInBytes(%d) = %d", j, data.mBuffer->getBufStridesInBytes(j));
            }
        }
        else
        {
            MY_LOGD("mvIn.mBuffer is NULL!!");
        }


        MY_LOGD("mvIn.mTransform = %d", data.mTransform);

    }

    MY_LOGD("mvOut section");
    for(size_t i=0;i<rInputQParam.mvOut.size(); i++)
    {
        Output data = rInputQParam.mvOut[i];
        MY_LOGD("========\nIndex = %d", i);

        MY_LOGD("mvOut.PortID.index = %d", data.mPortID.index);
        MY_LOGD("mvOut.PortID.type = %d", data.mPortID.type);
        MY_LOGD("mvOut.PortID.inout = %d", data.mPortID.inout);
        MY_LOGD("mvOut.PortID.group = %d", data.mPortID.group);

        MY_LOGD("mvOut.mBuffer=%x", data.mBuffer);
        if(data.mBuffer != NULL)
        {
            MY_LOGD("mvOut.mBuffer->getImgSize = %dx%d", data.mBuffer->getImgSize().w,
                                                data.mBuffer->getImgSize().h);

            MY_LOGD("mvOut.mBuffer->getImgFormat = %x", data.mBuffer->getImgFormat());
            MY_LOGD("mvOut.mBuffer->getPlaneCount = %d", data.mBuffer->getPlaneCount());
            for(size_t j=0;j<data.mBuffer->getPlaneCount();j++)
            {
                MY_LOGD("mvOut.mBuffer->getBufVA(%d) = %X", j, data.mBuffer->getBufVA(j));
                MY_LOGD("mvOut.mBuffer->getBufStridesInBytes(%d) = %d", j, data.mBuffer->getBufStridesInBytes(j));
            }
        }
        else
        {
            MY_LOGD("mvOut.mBuffer is NULL!!");
        }
        MY_LOGD("mvOut.mTransform = %d", data.mTransform);
    }

    MY_LOGD("mvCropRsInfo section");
    for(size_t i=0;i<rInputQParam.mvCropRsInfo.size(); i++)
    {
        MCrpRsInfo data = rInputQParam.mvCropRsInfo[i];
        MY_LOGD("========\nIndex = %d", i);

        MY_LOGD("CropRsInfo.mGroupID=%d", data.mGroupID);
        MY_LOGD("CropRsInfo.mFrameGroup=%d", data.mFrameGroup);
        MY_LOGD("CropRsInfo.mResizeDst=%dx%d", data.mResizeDst.w, data.mResizeDst.h);
        MY_LOGD("CropRsInfo.mCropRect.p_fractional=(%d,%d) ", data.mCropRect.p_fractional.x, data.mCropRect.p_fractional.y);
        MY_LOGD("CropRsInfo.mCropRect.p_integral=(%d,%d) ", data.mCropRect.p_integral.x, data.mCropRect.p_integral.y);
        MY_LOGD("CropRsInfo.mCropRect.s=%dx%d ", data.mCropRect.s.w, data.mCropRect.s.h);
    }

    MY_LOGD("mvModuleData section");
    for(size_t i=0;i<rInputQParam.mvModuleData.size(); i++)
    {
        ModuleInfo data = rInputQParam.mvModuleData[i];
        MY_LOGD("========\nIndex = %d", i);

        MY_LOGD("ModuleData.moduleTag=%d", data.moduleTag);
        MY_LOGD("ModuleData.frameGroup=%d", data.frameGroup);

        _SRZ_SIZE_INFO_ *SrzInfo = (_SRZ_SIZE_INFO_ *) data.moduleStruct;
        MY_LOGD("SrzInfo->in_w=%d", SrzInfo->in_w);
        MY_LOGD("SrzInfo->in_h=%d", SrzInfo->in_h);
        MY_LOGD("SrzInfo->crop_w=%d", SrzInfo->crop_w);
        MY_LOGD("SrzInfo->crop_h=%d", SrzInfo->crop_h);
        MY_LOGD("SrzInfo->crop_x=%d", SrzInfo->crop_x);
        MY_LOGD("SrzInfo->crop_y=%d", SrzInfo->crop_y);
        MY_LOGD("SrzInfo->crop_floatX=%d", SrzInfo->crop_floatX);
        MY_LOGD("SrzInfo->crop_floatY=%d", SrzInfo->crop_floatY);
        MY_LOGD("SrzInfo->out_w=%d", SrzInfo->out_w);
        MY_LOGD("SrzInfo->out_h=%d", SrzInfo->out_h);
    }
}

}; //NSFeaturePipe
}; //NSCamFeature
}; //NSCam

