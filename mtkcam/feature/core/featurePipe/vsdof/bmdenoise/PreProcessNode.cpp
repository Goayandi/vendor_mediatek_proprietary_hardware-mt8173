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
#include <mtkcam/feature/stereo/pipe/vsdof_common.h>
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>

#include "PreProcessNode.h"

#define PIPE_MODULE_TAG "BMDeNoise"
#define PIPE_CLASS_TAG "PreProcessNode"
#define PIPE_LOG_TAG PIPE_MODULE_TAG PIPE_CLASS_TAG

// buffer alloc size
#define BUFFER_ALLOC_SIZE 1
#define TUNING_ALLOC_SIZE 3

// debug settings
#define USE_DEFAULT_ISP 0

#include <PipeLog.h>

#include <DpBlitStream.h>
#include "../util/vsdof_util.h"

#include <mtkcam/feature/stereo/hal/stereo_common.h>
using namespace NSCam::NSCamFeature::NSFeaturePipe;
using namespace VSDOF::util;
/*******************************************************************************
 *
 ********************************************************************************/
PreProcessNode::BufferSizeConfig::
BufferSizeConfig()
{
    StereoSizeProvider* sizePrvider = StereoSizeProvider::getInstance();
    StereoArea area;

    {
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_W_1);
        BMPREPROCESS_MAIN1_W_SIZE = MSize(area.size.w, area.size.h);
    }
    {
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_W_2);
        BMPREPROCESS_MAIN2_W_SIZE = MSize(area.size.w, area.size.h);
    }

    {
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_1);
        BMPREPROCESS_MAIN1_MFBO_SIZE = MSize(area.size.w, area.size.h);
    }
    {
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_2);
        BMPREPROCESS_MAIN2_MFBO_SIZE = MSize(area.size.w, area.size.h);
    }
    {
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_1);
        BMPREPROCESS_MAIN1_MFBO_FINAL_SIZE = MSize(area.size.w, area.size.h);
    }
    {
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_2);
        BMPREPROCESS_MAIN2_MFBO_FINAL_SIZE = MSize(area.size.w, area.size.h);
    }

    debug();
}


MVOID
PreProcessNode::BufferSizeConfig::debug() const
{
    MY_LOGD("PreProcessNode debug size======>\n");
    #define DEBUG_MSIZE(sizeCons) \
        MY_LOGD("size: " #sizeCons " : %dx%d\n", sizeCons.w, sizeCons.h);

    DEBUG_MSIZE( BMPREPROCESS_MAIN1_W_SIZE);
    DEBUG_MSIZE( BMPREPROCESS_MAIN2_W_SIZE);

    DEBUG_MSIZE( BMPREPROCESS_MAIN1_MFBO_SIZE);
    DEBUG_MSIZE( BMPREPROCESS_MAIN2_MFBO_SIZE);

    DEBUG_MSIZE( BMPREPROCESS_MAIN1_MFBO_FINAL_SIZE);
    DEBUG_MSIZE( BMPREPROCESS_MAIN2_MFBO_FINAL_SIZE);
}

PreProcessNode::BufferSizeConfig::
~BufferSizeConfig()
{}
/*******************************************************************************
 *
 ********************************************************************************/
PreProcessNode::BufferPoolSet::
BufferPoolSet()
{}

PreProcessNode::BufferPoolSet::
~BufferPoolSet()
{}

MBOOL
PreProcessNode::BufferPoolSet::
init(const BufferSizeConfig& config, const MUINT32 tuningsize)
{
    MY_LOGD("BufferPoolSet init +");

    int allocateSize = BUFFER_ALLOC_SIZE;
    int allocateSize_tuning = TUNING_ALLOC_SIZE;

    CAM_TRACE_NAME("PreProcessNode::BufferPoolSet::init");
    CAM_TRACE_BEGIN("PreProcessNode::BufferPoolSet::init=>create buffer pools");

    mpMain1_w_bufPool = ImageBufferPool::create(
        "mpMain1_w_bufPool",
        config.BMPREPROCESS_MAIN1_W_SIZE.w,
        config.BMPREPROCESS_MAIN1_W_SIZE.h,
        eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpMain1_w_bufPool == nullptr){
        MY_LOGE("create mpMain1_w_bufPool failed!");
        return MFALSE;
    }

    mpMain2_w_bufPool = ImageBufferPool::create(
        "mpMain2_w_bufPool",
        config.BMPREPROCESS_MAIN2_W_SIZE.w,
        config.BMPREPROCESS_MAIN2_W_SIZE.h,
        eImgFmt_YV12, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpMain2_w_bufPool == nullptr){
        MY_LOGE("create mpMain2_w_bufPool failed!");
        return MFALSE;
    }

    mpMain1_mfbo_bufPool = ImageBufferPool::create(
        "mpMain1_mfbo_bufPool",
        config.BMPREPROCESS_MAIN1_MFBO_SIZE.w,
        config.BMPREPROCESS_MAIN1_MFBO_SIZE.h,
        eImgFmt_BAYER12_UNPAK, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpMain1_mfbo_bufPool == nullptr){
        MY_LOGE("create mpMain1_mfbo_bufPool failed!");
        return MFALSE;
    }

    mpMain2_mfbo_bufPool = ImageBufferPool::create(
        "mpMain2_mfbo_bufPool",
        config.BMPREPROCESS_MAIN2_MFBO_SIZE.w,
        config.BMPREPROCESS_MAIN2_MFBO_SIZE.h,
        eImgFmt_BAYER12_UNPAK, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpMain2_mfbo_bufPool == nullptr){
        MY_LOGE("create mpMain2_mfbo_bufPool failed!");
        return MFALSE;
    }

    mpMain1_mfbo_final_bufPool = ImageBufferPool::create(
        "mpMain1_mfbo_final_bufPool",
        config.BMPREPROCESS_MAIN1_MFBO_FINAL_SIZE.w,
        config.BMPREPROCESS_MAIN1_MFBO_FINAL_SIZE.h,
        eImgFmt_BAYER12_UNPAK, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpMain1_mfbo_final_bufPool == nullptr){
        MY_LOGE("create mpMain1_mfbo_final_bufPool failed!");
        return MFALSE;
    }

    mpMain2_mfbo_final_bufPool = ImageBufferPool::create(
        "mpMain2_mfbo_final_bufPool",
        config.BMPREPROCESS_MAIN2_MFBO_FINAL_SIZE.w,
        config.BMPREPROCESS_MAIN2_MFBO_FINAL_SIZE.h,
        eImgFmt_BAYER12_UNPAK, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpMain2_mfbo_final_bufPool == nullptr){
        MY_LOGE("create mpMain2_mfbo_final_bufPool failed!");
        return MFALSE;
    }

    mpTuningBuffer_bufPool = TuningBufferPool::create("VSDOF_TUNING_PREPROCESS", tuningsize);
    if(mpTuningBuffer_bufPool == nullptr){
        MY_LOGE("create mpTuningBuffer_bufPool failed!");
        return MFALSE;
    }
    CAM_TRACE_END();

    CAM_TRACE_BEGIN("PreProcessNode::BufferPoolSet::init=>allocate buffer pools");
    mpMain1_w_bufPool->allocate(allocateSize);
    mpMain2_w_bufPool->allocate(allocateSize);
    mpMain1_mfbo_bufPool->allocate(allocateSize);
    mpMain2_mfbo_bufPool->allocate(allocateSize);
    mpMain1_mfbo_final_bufPool->allocate(allocateSize);
    mpMain2_mfbo_final_bufPool->allocate(allocateSize);
    mpTuningBuffer_bufPool->allocate(allocateSize_tuning);
    CAM_TRACE_END();

    MY_LOGD("BufferPoolSet init -");
    return MTRUE;
}

MBOOL
PreProcessNode::BufferPoolSet::
uninit()
{
    MY_LOGD("+");

    ImageBufferPool::destroy(mpMain1_w_bufPool);
    ImageBufferPool::destroy(mpMain2_w_bufPool);
    ImageBufferPool::destroy(mpMain1_mfbo_bufPool);
    ImageBufferPool::destroy(mpMain2_mfbo_bufPool);
    ImageBufferPool::destroy(mpMain1_mfbo_final_bufPool);
    ImageBufferPool::destroy(mpMain2_mfbo_final_bufPool);
    TuningBufferPool::destroy(mpTuningBuffer_bufPool);

    MY_LOGD("-");
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
PreProcessNode::
PreProcessNode(const char *name,
    Graph_T *graph,
    MINT32 openId)
    : BMDeNoisePipeNode(name, graph)
    , miOpenId(openId)
{
    MY_LOGD("ctor(0x%x)", this);
    this->addWaitQueue(&mRequests);
}
/*******************************************************************************
 *
 ********************************************************************************/
PreProcessNode::
~PreProcessNode()
{
    MY_LOGD("dctor(0x%x)", this);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
onData(
    DataID id,
    EffectRequestPtr &request)
{
    FUNC_START;
    TRACE_FUNC_ENTER();

    MBOOL ret = MFALSE;
    switch(id)
    {
        case ROOT_ENQUE:
            mRequests.enque(request);
            ret = MTRUE;
            break;
        default:
            ret = MFALSE;
            MY_LOGE("unknown data id :%d", id);
            break;
    }

    TRACE_FUNC_EXIT();
    FUNC_END;
    return ret;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
onInit()
{
    CAM_TRACE_NAME("PreProcessNode::onInit");
    TRACE_FUNC_ENTER();
    FUNC_START;

    if(!BMDeNoisePipeNode::onInit()){
        MY_LOGE("BMDeNoisePipeNode::onInit() failed!");
        return MFALSE;
    }

    // MDP
    MY_LOGD("PreProcessNode::onInit=>new DpBlitStream");
    CAM_TRACE_BEGIN("PreProcessNode::onInit=>new DpBlitStream");
    mpDpStream = new DpBlitStream();
    CAM_TRACE_END();

    // normal stream
    MY_LOGD("PreProcessNode::onInit=>create normalStream");
    CAM_TRACE_BEGIN("PreProcessNode::onInit=>create normalStream");
    mpINormalStream = NSCam::NSIoPipe::NSPostProc::INormalStream::createInstance(miOpenId);

    if (mpINormalStream == NULL)
    {
        MY_LOGE("mpINormalStream create instance for PreProcessNode Node failed!");
        cleanUp();
        return MFALSE;
    }
    mpINormalStream->init(PIPE_LOG_TAG);
    CAM_TRACE_END();

    // BufferPoolSet init
    MY_LOGD("PreProcessNode::onInit=>BufferPoolSet::init");
    CAM_TRACE_BEGIN("PreProcessNode::mBufPoolSet::init");

    const MUINT32 tuningsize = mpINormalStream->getRegTableSize();
    mBufPoolSet.init(mBufConfig, tuningsize);
    CAM_TRACE_END();

    MY_LOGD("PreProcessNode::onInit=>create_3A_instance senosrIdx:(%d/%d)", mSensorIdx_Main1, mSensorIdx_Main2);
    CAM_TRACE_BEGIN("PreProcessNode::onInit=>create_3A_instance");
    mp3AHal_Main1 = MAKE_Hal3A(mSensorIdx_Main1, "BMDENOISE_3A_MAIN1");
    mp3AHal_Main2 = MAKE_Hal3A(mSensorIdx_Main2, "BMDENOISE_3A_MAIN2");
    MY_LOGD("3A create instance, Main1: %x Main2: %x", mp3AHal_Main1, mp3AHal_Main2);
    CAM_TRACE_END();

    if(!prepareModuleSettings()){
        MY_LOGE("prepareModuleSettings Error! Please check the error msg above!");
    }

    mSizePrvider = StereoSizeProvider::getInstance();

    FUNC_END;
    TRACE_FUNC_EXIT();
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
onUninit()
{
    CAM_TRACE_NAME("PreProcessNode::onUninit");
    FUNC_START;
    cleanUp();
    FUNC_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
PreProcessNode::
cleanUp()
{
    FUNC_START;
    if(mpDpStream!= nullptr)
        delete mpDpStream;
    if(mpINormalStream != nullptr)
    {
        mpINormalStream->uninit(PIPE_LOG_TAG);
        mpINormalStream->destroyInstance();
        mpINormalStream = nullptr;
    }
    if(mp3AHal_Main1)
    {
        mp3AHal_Main1->destroyInstance("BMDENOISE_3A_MAIN1");
        mp3AHal_Main1 = NULL;
    }

    if(mp3AHal_Main2)
    {
        mp3AHal_Main2->destroyInstance("BMDENOISE_3A_MAIN2");
        mp3AHal_Main2 = NULL;
    }

    mBufPoolSet.uninit();
    FUNC_END;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
onThreadStart()
{
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
onThreadStop()
{
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
onThreadLoop()
{
    FUNC_START;
    EffectRequestPtr request = nullptr;
    SmartImageBuffer wdma_img = nullptr;

    if( !waitAllQueue() )// block until queue ready, or flush() breaks the blocking state too.
    {
        return MFALSE;
    }
    if( !mRequests.deque(request) )
    {
        MY_LOGD("mRequests.deque() failed");
        return MFALSE;
    }
    CAM_TRACE_NAME("PreProcessNode::onThreadLoop");

    // check request type
    const sp<EffectParameter> params = request->getRequestParameter();
    MINT32 requestType = params->getInt(BMDENOISE_REQUEST_TYPE_KEY);

    MBOOL ret = MFALSE;
    switch(requestType)
    {
        case TYPE_PREPROCESS:
            VSDOF_LOGD("do pre-process");
            doPreProcessRequest(request);
            ret = MTRUE;
            break;
        case TYPE_WARPING_MATRIX:
            VSDOF_LOGD("handle waping matrix to next node directly");
            handleData(RREPROCESS_TO_DENOISE_WARPING, request);
            ret = MTRUE;
            break;
        default:
            ret = MFALSE;
            MY_LOGE("unknown requestType :%d", requestType);
            break;
    }

    FUNC_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
doPreProcessRequest(
    EffectRequestPtr request)
{
    CAM_TRACE_NAME("PreProcessNode::doPreProcessRequest");
    MY_LOGD("+, reqID=%d", request->getRequestNo());

    // input image buffer
    FrameInfoPtr framePtr_inMain1FSRAW = request->vInputFrameInfo.valueFor(BID_PRE_PROCESS_IN_FULLRAW_1);
    FrameInfoPtr framePtr_inMain2FSRAW = request->vInputFrameInfo.valueFor(BID_PRE_PROCESS_IN_FULLRAW_2);
    sp<IImageBuffer> frameBuf_inFSRAW1 = nullptr;
    sp<IImageBuffer> frameBuf_inFSRAW2 = nullptr;
    framePtr_inMain1FSRAW->getFrameBuffer(frameBuf_inFSRAW1);
    framePtr_inMain2FSRAW->getFrameBuffer(frameBuf_inFSRAW2);

    if(frameBuf_inFSRAW1 == nullptr){
        MY_LOGE("frameBuf_inFSRAW1 == nullptr!");
        return MFALSE;
    }

    if(frameBuf_inFSRAW2 == nullptr){
        MY_LOGE("frameBuf_inFSRAW2 == nullptr!");
        return MFALSE;
    }

    // input metadata
    FrameInfoPtr framePtr_inAppMeta = request->vInputFrameInfo.valueFor(BID_META_IN_APP);
    FrameInfoPtr framePtr_inHalMeta_main1 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL);
    FrameInfoPtr framePtr_inHalMeta_main2 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);

    if(framePtr_inAppMeta == nullptr){
        MY_LOGE("framePtr_inAppMeta == nullptr!");
        return MFALSE;
    }

    if(framePtr_inHalMeta_main1 == nullptr){
        MY_LOGE("framePtr_inHalMeta_main1 == nullptr!");
        return MFALSE;
    }

    if(framePtr_inHalMeta_main2 == nullptr){
        MY_LOGE("framePtr_inHalMeta_main2 == nullptr!");
        return MFALSE;
    }

    IMetadata* pMeta_InApp = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);
    IMetadata* pMeta_InHal_main1 = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_main1);
    IMetadata* pMeta_InHal_main2 = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_main2);

    if(pMeta_InApp == nullptr){
        MY_LOGE("pMeta_InApp == nullptr!");
        return MFALSE;
    }

    if(pMeta_InHal_main1 == nullptr){
        MY_LOGE("pMeta_InHal_main1 == nullptr!");
        return MFALSE;
    }

    if(pMeta_InHal_main2 == nullptr){
        MY_LOGE("pMeta_InHal_main2 == nullptr!");
        return MFALSE;
    }

    // output image buffer
    SmartImageBuffer mpMain1_w = mBufPoolSet.mpMain1_w_bufPool->request();
    SmartImageBuffer mpMain2_w = mBufPoolSet.mpMain2_w_bufPool->request();
    SmartImageBuffer mpMain1_mfbo = mBufPoolSet.mpMain1_mfbo_bufPool->request();
    SmartImageBuffer mpMain2_mfbo = mBufPoolSet.mpMain2_mfbo_bufPool->request();
    SmartImageBuffer mpMain1_mfbo_final = mBufPoolSet.mpMain1_mfbo_final_bufPool->request();
    SmartImageBuffer mpMain2_mfbo_final = mBufPoolSet.mpMain2_mfbo_final_bufPool->request();

    // tuning buffer
    TuningParam rTuningParam_main1;
    TuningParam rTuningParam_main2;
    SmartTuningBuffer tuningBuf_main1 = mBufPoolSet.mpTuningBuffer_bufPool->request();
    SmartTuningBuffer tuningBuf_main2 = mBufPoolSet.mpTuningBuffer_bufPool->request();

    if(tuningBuf_main1 == nullptr){
        MY_LOGE("tuningBuf_main1 == nullptr!");
        return MFALSE;
    }

    if(tuningBuf_main2 == nullptr){
        MY_LOGE("tuningBuf_main2 == nullptr!");
        return MFALSE;
    }

    {
      	trySetMetadata<MUINT8>(pMeta_InHal_main1, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Denoise_toW);

        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_main1, mp3AHal_Main1, MFALSE};
        rTuningParam_main1 = applyISPTuning(tuningBuf_main1, ispConfig, USE_DEFAULT_ISP);
    }

    {
      	trySetMetadata<MUINT8>(pMeta_InHal_main2, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Denoise_toW);

        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_main2, mp3AHal_Main2, MFALSE};
        rTuningParam_main2 = applyISPTuning(tuningBuf_main2, ispConfig, USE_DEFAULT_ISP);
    }

    EnquedBufPool *enquedData = new EnquedBufPool(request, this);

    enquedData->addBuffData(BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_1, mpMain1_mfbo);
    enquedData->addBuffData(BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_2, mpMain2_mfbo);
    enquedData->addBuffData(BID_PRE_PROCESS_OUT_W_1, mpMain1_w);
    enquedData->addBuffData(BID_PRE_PROCESS_OUT_W_2, mpMain2_w);
    enquedData->addBuffData(BID_PRE_PROCESS_OUT_MFBO_FINAL_1, mpMain1_mfbo_final);
    enquedData->addBuffData(BID_PRE_PROCESS_OUT_MFBO_FINAL_2, mpMain2_mfbo_final);

    enquedData->addTuningData(tuningBuf_main1);
    enquedData->addTuningData(tuningBuf_main2);

    StereoSizeProvider* sizePrvider = StereoSizeProvider::getInstance();
    StereoArea area;

    QParams enqueParams;

    // main1_w & main1_mfbo
    {
        int frameGroup = 0;

        enqueParams.mvStreamTag.push_back(ENormalStreamTag_DeNoise);
        enqueParams.mvSensorIdx.push_back(mSensorIdx_Main1);
        enqueParams.mvTuningData.push_back(tuningBuf_main1->mpVA);

        Input src;
        src.mPortID = PORT_IMGI;
        src.mPortID.group = frameGroup;
        src.mBuffer = frameBuf_inFSRAW1.get();
        enqueParams.mvIn.push_back(src);

        Input LSCIn;
        LSCIn.mPortID = PORT_DEPI;
        LSCIn.mPortID.group = frameGroup;
        LSCIn.mBuffer = static_cast<IImageBuffer*>(rTuningParam_main1.pLsc2Buf);
        enqueParams.mvIn.push_back(LSCIn);

        StereoArea fullraw_crop;
        fullraw_crop = mSizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);

        MCrpRsInfo crop1, crop2, crop3;
        // CRZ
        crop1.mGroupID = 1;
        crop1.mFrameGroup = frameGroup;
        crop1.mCropRect.p_fractional.x=0;
        crop1.mCropRect.p_fractional.y=0;
        crop1.mCropRect.p_integral.x=0;
        crop1.mCropRect.p_integral.y=0;
        crop1.mCropRect.s.w=src.mBuffer->getImgSize().w;
        crop1.mCropRect.s.h=src.mBuffer->getImgSize().h;
        crop1.mResizeDst.w=src.mBuffer->getImgSize().w;
        crop1.mResizeDst.h=src.mBuffer->getImgSize().h;
        enqueParams.mvCropRsInfo.push_back(crop1);

        // WROT
        StereoArea area;
        area = mSizePrvider->getBufferSize(E_BM_PREPROCESS_W_1);
        mpMain1_w->mImageBuffer->setExtParam(area.contentSize());

        crop3.mGroupID = 3;
        crop3.mFrameGroup = frameGroup;
        crop3.mCropRect.p_fractional.x=0;
        crop3.mCropRect.p_fractional.y=0;
        crop3.mCropRect.p_integral.x=fullraw_crop.startPt.x;
        crop3.mCropRect.p_integral.y=fullraw_crop.startPt.y;
        crop3.mCropRect.s = fullraw_crop.size;
        crop3.mResizeDst = fullraw_crop.size;
        enqueParams.mvCropRsInfo.push_back(crop3);

        Output out;
        out.mPortID = PORT_WROT;
        out.mPortID.group = frameGroup;
        out.mBuffer = mpMain1_w->mImageBuffer.get();
        out.mTransform = mModuleTrans;
        enqueParams.mvOut.push_back(out);

        // MFBO
        Output out2;
        out2.mPortID = PORT_MFBO;
        out2.mPortID.group = frameGroup;
        out2.mBuffer = mpMain1_mfbo->mImageBuffer.get();
        // A trick to make MFBO destination crop
        mpMain1_mfbo->mImageBuffer->setExtParam(
            fullraw_crop.size,
            fullraw_crop.size.w*mpMain1_mfbo->mImageBuffer->getPlaneBitsPerPixel(0)/8*fullraw_crop.startPt.y
        );
        enqueParams.mvOut.push_back(out2);
    }

    // main2_mfbo
    {
        int frameGroup = 1;

        enqueParams.mvStreamTag.push_back(ENormalStreamTag_DeNoise);
        enqueParams.mvSensorIdx.push_back(mSensorIdx_Main2);
        enqueParams.mvTuningData.push_back(tuningBuf_main2->mpVA);

        Input src;
        src.mPortID = PORT_IMGI;
        src.mPortID.group = frameGroup;
        src.mBuffer = frameBuf_inFSRAW2.get();
        enqueParams.mvIn.push_back(src);

        Input LSCIn;
        LSCIn.mPortID = PORT_DEPI;
        LSCIn.mPortID.group = frameGroup;
        LSCIn.mBuffer = static_cast<IImageBuffer*>(rTuningParam_main2.pLsc2Buf);
        enqueParams.mvIn.push_back(LSCIn);

        StereoArea fullraw_crop;
        fullraw_crop = mSizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_2);

        MCrpRsInfo crop1, crop2, crop3;
        // CRZ
        crop1.mGroupID = 1;
        crop1.mFrameGroup = frameGroup;
        crop1.mCropRect.p_fractional.x=0;
        crop1.mCropRect.p_fractional.y=0;
        crop1.mCropRect.p_integral.x=0;
        crop1.mCropRect.p_integral.y=0;
        crop1.mCropRect.s.w=src.mBuffer->getImgSize().w;
        crop1.mCropRect.s.h=src.mBuffer->getImgSize().h;
        crop1.mResizeDst.w=src.mBuffer->getImgSize().w;
        crop1.mResizeDst.h=src.mBuffer->getImgSize().h;
        enqueParams.mvCropRsInfo.push_back(crop1);

        // WROT
        StereoArea area;
        area = mSizePrvider->getBufferSize(E_BM_PREPROCESS_W_2);
        mpMain2_w->mImageBuffer->setExtParam(area.contentSize());
        crop3.mGroupID = 3;
        crop3.mFrameGroup = frameGroup;
        crop3.mCropRect.p_fractional.x=0;
        crop3.mCropRect.p_fractional.y=0;
        crop3.mCropRect.p_integral.x=fullraw_crop.startPt.x;
        crop3.mCropRect.p_integral.y=fullraw_crop.startPt.y;
        crop3.mCropRect.s = fullraw_crop.size;
        crop3.mResizeDst = fullraw_crop.size;
        enqueParams.mvCropRsInfo.push_back(crop3);

        Output out;
        out.mPortID = PORT_WROT;
        out.mPortID.group = frameGroup;
        out.mBuffer = mpMain2_w->mImageBuffer.get();
        out.mTransform = mModuleTrans;
        enqueParams.mvOut.push_back(out);

        // MFBO
        Output out2;
        out2.mPortID = PORT_MFBO;
        out2.mPortID.group = frameGroup;
        out2.mBuffer = mpMain2_mfbo->mImageBuffer.get();
        // A trick to make MFBO destination crop
        mpMain2_mfbo->mImageBuffer->setExtParam(
            fullraw_crop.size,
            fullraw_crop.size.w*mpMain2_mfbo->mImageBuffer->getPlaneBitsPerPixel(0)/8*fullraw_crop.startPt.y
        );
        enqueParams.mvOut.push_back(out2);
    }

    enqueParams.mpfnCallback = onP2SuccessCallback;
    enqueParams.mpfnEnQFailCallback = onP2FailedCallback;
    enqueParams.mpCookie = (MVOID*) enquedData;

    // run P2
    MY_LOGD("mpINormalStream->enque start! reqID=%d", request->getRequestNo());
    enquedData->start();

    if(!mpINormalStream->enque(enqueParams))
    {
        MY_LOGE("mpINormalStream enque failed! reqID=%d", request->getRequestNo());
        return MFALSE;
    }

    MY_LOGD("mpINormalStream->enque done! reqID=%d", request->getRequestNo());

    MY_LOGD("-, reqID=%d", request->getRequestNo());
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
PreProcessNode::
prepareModuleSettings()
{
    MY_LOGD("+");

    // module rotation
    ENUM_ROTATION eRot = StereoSettingProvider::getModuleRotation();
    switch(eRot)
    {
        case eRotate_0:
            mModuleTrans = 0;
            break;
        case eRotate_90:
            mModuleTrans = eTransform_ROT_90;
            break;
        case eRotate_180:
            mModuleTrans = eTransform_ROT_180;
            break;
        case eRotate_270:
            mModuleTrans = eTransform_ROT_270;
            break;
        default:
            MY_LOGE("Not support module rotation =%d", eRot);
            return MFALSE;
    }

    MY_LOGD("mModuleTrans=%d", mModuleTrans);

    MY_LOGD("-");
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MINT32
PreProcessNode::
eTransformToDegree(MINT32 eTrans)
{
    switch(mModuleTrans)
    {
        case 0:
            return 0;
        case eTransform_ROT_90:
            return 90;
        case eTransform_ROT_180:
            return 180;
        case eTransform_ROT_270:
            return 270;
        default:
            MY_LOGE("Not support eTransform =%d", eTrans);
            return -1;
    }
}
/*******************************************************************************
 *
 ********************************************************************************/
IMetadata*
PreProcessNode::
getMetadataFromFrameInfoPtr(sp<EffectFrameInfo> pFrameInfo)
{
    IMetadata* result;
    sp<EffectParameter> effParam = pFrameInfo->getFrameParameter();
    result = reinterpret_cast<IMetadata*>(effParam->getPtr(BMDENOISE_EFFECT_PARAMS_KEY));
    return result;
}
/*******************************************************************************
 *
 ********************************************************************************/
TuningParam
PreProcessNode::
applyISPTuning(
        SmartTuningBuffer& targetTuningBuf,
        const ISPTuningConfig& ispConfig,
        MBOOL useDefault
)
{
    CAM_TRACE_NAME("PreProcessNode::applyISPTuning");
    VSDOF_LOGD("+, reqID=%d bIsResized=%d", ispConfig.pInAppMetaFrame->getRequestNo(), ispConfig.bInputResizeRaw);

    TuningParam tuningParam = {NULL, NULL, NULL};
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

    if(useDefault){
        MY_LOGE("not supported");
    }else{
        MetaSet_T resultMeta;
        ispConfig.p3AHAL->setIsp(0, inMetaSet, &tuningParam, &resultMeta);

        // write ISP resultMeta to input hal Meta
        (*pMeta_InHal) += resultMeta.halMeta;
    }

    VSDOF_LOGD("-, reqID=%d", ispConfig.pInAppMetaFrame->getRequestNo());

    return tuningParam;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
PreProcessNode::
onP2SuccessCallback(QParams& rParams)
{
    EnquedBufPool* pEnqueData = (EnquedBufPool*) (rParams.mpCookie);
    PreProcessNode* pPreProcessNode = (PreProcessNode*) (pEnqueData->mpNode);
    pPreProcessNode->handleP2Done(rParams, pEnqueData);
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
PreProcessNode::
onP2FailedCallback(QParams& rParams)
{
    MY_LOGE("PreProcessNode operations failed!!Check the following log:");
    EnquedBufPool* pEnqueData = (EnquedBufPool*) (rParams.mpCookie);
    PreProcessNode* pPreProcessNode = (PreProcessNode*) (pEnqueData->mpNode);
    // pPreProcessNode->debugQParams(rParams);
    pPreProcessNode->handleData(ERROR_OCCUR_NOTIFY, pEnqueData->mRequest);
    delete pEnqueData;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
PreProcessNode::
handleP2Done(QParams& rParams, EnquedBufPool* pEnqueBufPool)
{
    EffectRequestPtr request = pEnqueBufPool->mRequest;
    VSDOF_LOGD("+ :reqID=%d", request->getRequestNo());

    pEnqueBufPool->stop();
    VSDOF_LOGD("BMDeNoise_Profile: featurePipe PreProcessNode P1 Pre-Process time(%d ms) reqID=%d",
        pEnqueBufPool->getElapsed(),
        request->getRequestNo()
    );

    //revert setExtParam to W1
    {
        StereoArea area = mSizePrvider->getBufferSize(E_BM_PREPROCESS_W_1);
        SmartImageBuffer pBuf = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_PRE_PROCESS_OUT_W_1);
        pBuf->mImageBuffer->setExtParam(area.size);
    }

    //revert setExtParam to W2
    {
        StereoArea area = mSizePrvider->getBufferSize(E_BM_PREPROCESS_W_2);
        SmartImageBuffer pBuf = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_PRE_PROCESS_OUT_W_2);
        pBuf->mImageBuffer->setExtParam(area.size);
    }


    Timer localTimer;
    localTimer.start();
    {
        CAM_TRACE_NAME("PreProcessNode::MDP");
        // do MDP rotate for main1 mfbo
        {
            StereoArea area_dst;
            SmartImageBuffer srcBuf = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_1);
            SmartImageBuffer dstBuf = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_PRE_PROCESS_OUT_MFBO_FINAL_1);
            area_dst = mSizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_1);

            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = srcBuf->mImageBuffer.get();
            config.pDstBuffer = dstBuf->mImageBuffer.get();
            config.rotAngle = eTransformToDegree(mModuleTrans);
            config.customizedDstSize = area_dst.contentSize();

            if(!excuteMDPBayer12(config))
            {
                MY_LOGE("excuteMDPBayer12 for main1 mfbo failed.");
            }
        }

        // do MDP rotate for main2 mfbo
        {
            StereoArea area_dst;
            SmartImageBuffer srcBuf = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_2);
            SmartImageBuffer dstBuf = pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_PRE_PROCESS_OUT_MFBO_FINAL_2);
            area_dst = mSizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_2);

            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = srcBuf->mImageBuffer.get();
            config.pDstBuffer = dstBuf->mImageBuffer.get();
            config.rotAngle = eTransformToDegree(mModuleTrans);
            config.customizedDstSize = area_dst.contentSize();

            if(!excuteMDPBayer12(config))
            {
                MY_LOGE("excuteMDPBayer12 for main1 mfbo failed.");
            }
        }
    }

    localTimer.stop();
    VSDOF_LOGD("BMDeNoise_Profile: featurePipe PreProcessNode P1 Pre-Process MDP rotate time(%d ms) reqID=%d",
        localTimer.getElapsed(),
        request->getRequestNo()
    );

    {
        CAM_TRACE_NAME("PreProcessNode::handleP2Done");
        SmartImageBuffer smBuf;

        #define ADD_ENQUE_BUFF_TO_INFO_MAP(infoMap, enqueBufPool, BID)\
            smBuf = enqueBufPool->mEnquedSmartImgBufMap.valueFor(BID); \
            infoMap->addImageBuffer(BID, smBuf);

        sp<ImageBufInfoMap> TO_DeNoise_ImgBufInfo = new ImageBufInfoMap(request);

        // really used by DeNoiseNode
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_DeNoise_ImgBufInfo, pEnqueBufPool, BID_PRE_PROCESS_OUT_W_1);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_DeNoise_ImgBufInfo, pEnqueBufPool, BID_PRE_PROCESS_OUT_MFBO_FINAL_1);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_DeNoise_ImgBufInfo, pEnqueBufPool, BID_PRE_PROCESS_OUT_MFBO_FINAL_2);

        // not used by DeNoiseNode, just for buffer dump
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_DeNoise_ImgBufInfo, pEnqueBufPool, BID_PRE_PROCESS_OUT_W_2);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_DeNoise_ImgBufInfo, pEnqueBufPool, BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_1);
        ADD_ENQUE_BUFF_TO_INFO_MAP(TO_DeNoise_ImgBufInfo, pEnqueBufPool, BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_2);

        // p1 raw dump? Ask DepthMapNode!

        handleDataAndDump(RREPROCESS_TO_DENOISE_RAW, TO_DeNoise_ImgBufInfo);

        delete pEnqueBufPool;
    }
    VSDOF_LOGD("- :reqID=%d", request->getRequestNo());
}
/*******************************************************************************
 *
 ********************************************************************************/
const char*
PreProcessNode::
onDumpBIDToName(BMDeNoiseBufferID BID)
{
    #define MAKE_NAME_CASE(name) \
        case name: return #name;

    switch(BID)
    {
        MAKE_NAME_CASE(BID_PRE_PROCESS_IN_FULLRAW_1);
        MAKE_NAME_CASE(BID_PRE_PROCESS_IN_FULLRAW_2);
        MAKE_NAME_CASE(BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_1);
        MAKE_NAME_CASE(BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_2);
        MAKE_NAME_CASE(BID_PRE_PROCESS_OUT_W_1);
        MAKE_NAME_CASE(BID_PRE_PROCESS_OUT_W_2);
        MAKE_NAME_CASE(BID_PRE_PROCESS_OUT_MFBO_FINAL_1);
        MAKE_NAME_CASE(BID_PRE_PROCESS_OUT_MFBO_FINAL_2);
    }
    MY_LOGW("unknown BID:%d", BID);

    return "unknown";
    #undef MAKE_NAME_CASE
}