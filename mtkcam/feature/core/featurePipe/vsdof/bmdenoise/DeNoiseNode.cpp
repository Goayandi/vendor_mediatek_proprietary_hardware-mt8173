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
//#include <met_tag.h>

#include "DeNoiseNode.h"

#define PIPE_MODULE_TAG "BMDeNoise"
#define PIPE_CLASS_TAG "DeNoiseNode"
#define PIPE_LOG_TAG PIPE_MODULE_TAG PIPE_CLASS_TAG

// MET tags
#define DO_BM_DENOISE "doBMDeNoise"
#define DO_MDP_ROT_BACK "doMDPRotBack"
#define DO_POST_PROCESS "doPostProcess"
#define DO_THUMBNAIL "doThumbnail"

// buffer alloc size
#define BUFFER_ALLOC_SIZE 1
#define TUNING_ALLOC_SIZE 3
#define SHADING_GAIN_SIZE 200

// debug settings
#define USE_DEFAULT_ISP 0
#define USE_DEFAULT_SHADING_GAIN 0
#define USE_DEFAULT_AMATRIX 0

#define DEFAULT_DYNAMIC_SHADING 32

// 2'comp, using ASL and ASR to do sign extension
#define TO_INT(a, bit) ((MINT32)((MINT32) (a)) << (32-(bit)) >> (32-(bit)))
#define OB_TO_INT(a) ((-(TO_INT((a), 13)))/4) /*10bit*/

#include <PipeLog.h>

#include <DpBlitStream.h>
#include "../util/vsdof_util.h"

#include <camera_custom_nvram.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <isp_tuning/isp_tuning_cam_info.h>
#include <isp_tuning/isp_tuning_idx.h>
#include <isp_tuning/isp_tuning_custom.h>

using namespace NSCam::NSCamFeature::NSFeaturePipe;
using namespace VSDOF::util;
using namespace NS_BMDN_HAL;
using namespace NS3Av3;
/*******************************************************************************
 *
 ********************************************************************************/
DeNoiseNode::BufferSizeConfig::
BufferSizeConfig()
{
    StereoSizeProvider* sizePrvider = StereoSizeProvider::getInstance();
    StereoArea area;

    {
        area = sizePrvider->getBufferSize(E_BM_DENOISE_HAL_OUT);
        BMDENOISE_BMDN_RESULT_SIZE = MSize(area.size.w, area.size.h);
    }

    {
        area = sizePrvider->getBufferSize(E_BM_DENOISE_HAL_OUT_ROT_BACK);
        BMDENOISE_BMDN_RESULT_ROT_BACK_SIZE = MSize(area.size.w, area.size.h);
    }

    debug();
}


MVOID
DeNoiseNode::BufferSizeConfig::debug() const
{
    MY_LOGD("DeNoiseNode debug size======>\n");
    #define DEBUG_MSIZE(sizeCons) \
        MY_LOGD("size: " #sizeCons " : %dx%d\n", sizeCons.w, sizeCons.h);

    DEBUG_MSIZE( BMDENOISE_BMDN_RESULT_SIZE);
    DEBUG_MSIZE( BMDENOISE_BMDN_RESULT_ROT_BACK_SIZE);
}

DeNoiseNode::BufferSizeConfig::
~BufferSizeConfig()
{}
/*******************************************************************************
 *
 ********************************************************************************/
DeNoiseNode::BufferPoolSet::
BufferPoolSet()
{}

DeNoiseNode::BufferPoolSet::
~BufferPoolSet()
{}

MBOOL
DeNoiseNode::BufferPoolSet::
init(const BufferSizeConfig& config, const MUINT32 tuningsize)
{
    MY_LOGD("BufferPoolSet init +");

    int allocateSize = BUFFER_ALLOC_SIZE;
    int allocateSize_tuning = TUNING_ALLOC_SIZE;

    CAM_TRACE_NAME("DeNoiseNode::BufferPoolSet::init");
    CAM_TRACE_BEGIN("DeNoiseNode::BufferPoolSet::init=>create buffer pools");

    mpBMDeNoiseResult_bufPool = ImageBufferPool::create(
        "mpBMDeNoiseResult_bufPool",
        config.BMDENOISE_BMDN_RESULT_SIZE.w,
        config.BMDENOISE_BMDN_RESULT_SIZE.h,
        eImgFmt_RGB48, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpBMDeNoiseResult_bufPool == nullptr){
        MY_LOGE("create mpBMDeNoiseResult_bufPool failed!");
        return MFALSE;
    }

    mpBMDeNoiseResultRotBack_bufPool = ImageBufferPool::create(
        "mpBMDeNoiseResultRotBack_bufPool",
        config.BMDENOISE_BMDN_RESULT_ROT_BACK_SIZE.w,
        config.BMDENOISE_BMDN_RESULT_ROT_BACK_SIZE.h,
        eImgFmt_RGB48, ImageBufferPool::USAGE_HW, MTRUE);
    if(mpBMDeNoiseResultRotBack_bufPool == nullptr){
        MY_LOGE("create mpBMDeNoiseResultRotBack_bufPool failed!");
        return MFALSE;
    }

    mpTuningBuffer_bufPool = TuningBufferPool::create("VSDOF_TUNING_DENOISE", tuningsize);
    if(mpTuningBuffer_bufPool == nullptr){
        MY_LOGE("create mpTuningBuffer_bufPool failed!");
        return MFALSE;
    }
    CAM_TRACE_END();

    CAM_TRACE_BEGIN("DeNoiseNode::BufferPoolSet::init=>allocate buffer pools");
    mpBMDeNoiseResult_bufPool->allocate(allocateSize);
    mpBMDeNoiseResultRotBack_bufPool->allocate(allocateSize);
    mpTuningBuffer_bufPool->allocate(allocateSize_tuning);
    CAM_TRACE_END();

    MY_LOGD("BufferPoolSet init -");
    return MTRUE;
}

MBOOL
DeNoiseNode::BufferPoolSet::
uninit()
{
    MY_LOGD("+");

    ImageBufferPool::destroy(mpBMDeNoiseResult_bufPool);
    ImageBufferPool::destroy(mpBMDeNoiseResultRotBack_bufPool);
    TuningBufferPool::destroy(mpTuningBuffer_bufPool);

    MY_LOGD("-");
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
/*******************************************************************************
 *
 ********************************************************************************/
DeNoiseNode::
DeNoiseNode(const char *name,
    Graph_T *graph,
    MINT32 openId)
    : BMDeNoisePipeNode(name, graph)
    , miOpenId(openId)
{
    MY_LOGD("ctor(0x%x)", this);
    this->addWaitQueue(&mRequests);

    miDumpShadingGain = ::property_get_int32("bmdenoise.pipe.dump.shading", 0);

    mDebugPerformanceQualityOption.dsH = ::property_get_int32("debug.bmdenoise.dsH", -1);
    mDebugPerformanceQualityOption.dsV = ::property_get_int32("debug.bmdenoise.dsV", -1);
    mDebugPerformanceQualityOption.FPREPROC = ::property_get_int32("debug.bmdenoise.FPREPROC", -1);
    mDebugPerformanceQualityOption.FSSTEP = ::property_get_int32("debug.bmdenoise.FSSTEP", -1);
}
/*******************************************************************************
 *
 ********************************************************************************/
DeNoiseNode::
~DeNoiseNode()
{
    MY_LOGD("dctor(0x%x)", this);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
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
DeNoiseNode::
onData(
    DataID id,
    ImgInfoMapPtr& pImgInfo)
{
    FUNC_START;
    MY_LOGE("not implemented!");
    FUNC_END;
    return MFALSE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
onInit()
{
    if(!BMDeNoisePipeNode::onInit()){
        MY_LOGE("BMDeNoisePipeNode::onInit() failed!");
        return MFALSE;
    }

    // MDP
    MY_LOGD("BMDeNoiseNode::onInit=>new DpBlitStream");
    CAM_TRACE_BEGIN("BMDeNoiseNode::onInit=>new DpBlitStream");
    mpDpStream = new DpBlitStream();
    CAM_TRACE_END();

    // normal stream
    MY_LOGD("BMDeNoiseNode::onInit=>create normalStream");
    CAM_TRACE_BEGIN("BMDeNoiseNode::onInit=>create normalStream");
    mpINormalStream = NSCam::NSIoPipe::NSPostProc::INormalStream::createInstance(miOpenId);

    if (mpINormalStream == NULL)
    {
        MY_LOGE("mpINormalStream create instance for BMDeNoiseNode Node failed!");
        cleanUp();
        return MFALSE;
    }
    mpINormalStream->init(PIPE_LOG_TAG);
    CAM_TRACE_END();

    // BufferPoolSet init
    MY_LOGD("BMDeNoiseNode::onInit=>BufferPoolSet::init");
    CAM_TRACE_BEGIN("BMDeNoiseNode::mBufPoolSet::init");

    const MUINT32 tuningsize = mpINormalStream->getRegTableSize();
    mBufPoolSet.init(mBufConfig, tuningsize);
    CAM_TRACE_END();

    MY_LOGD("BMDeNoiseNode::onInit=>create_3A_instance senosrIdx:(%d/%d)", mSensorIdx_Main1, mSensorIdx_Main2);
    CAM_TRACE_BEGIN("BMDeNoiseNode::onInit=>create_3A_instance");
    mp3AHal_Main1 = MAKE_Hal3A(mSensorIdx_Main1, "BMDENOISE_3A_MAIN1");
    mp3AHal_Main2 = MAKE_Hal3A(mSensorIdx_Main2, "BMDENOISE_3A_MAIN2");
    MY_LOGD("3A create instance, Main1: %x, Main2: %x", mp3AHal_Main1, mp3AHal_Main2);
    CAM_TRACE_END();

    if(!prepareModuleSettings()){
        MY_LOGE("prepareModuleSettings Error! Please check the error msg above!");
    }

    // query some sensor static info
    {
        IHalSensorList* sensorList = MAKE_HalSensorList();
        int sensorDev_Main1 = sensorList->querySensorDevIdx(mSensorIdx_Main1);

        SensorStaticInfo sensorStaticInfo;
        memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
        sensorList->querySensorStaticInfo(sensorDev_Main1, &sensorStaticInfo);

        mBayerOrder_main1 = sensorStaticInfo.sensorFormatOrder;
    }

    FUNC_END;
    TRACE_FUNC_EXIT();
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
onUninit()
{
    CAM_TRACE_NAME("DeNoiseNode::onUninit");
    FUNC_START;
    cleanUp();
    FUNC_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
DeNoiseNode::
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
DeNoiseNode::EnquedBufPool*
DeNoiseNode::
doBMDeNoise(EffectRequestPtr request)
{
    CAM_TRACE_NAME("DeNoiseNode::doBMDeNoise");
    MY_LOGD("+, reqID=%d, %d", request->getRequestNo());

    Timer localTimer;
    localTimer.start();
    //met_tag_start(0, DO_BM_DENOISE);

    // input image buffer
    FrameInfoPtr framePtr_inMFBO_final_1 = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_MFBO_FINAL_1);
    FrameInfoPtr framePtr_inMFBO_final_2 = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_MFBO_FINAL_2);
    FrameInfoPtr framePtr_inW_1 = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_W_1);
    FrameInfoPtr framePtr_inDisparityMap = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_DISPARITY_MAP_1);
    FrameInfoPtr framePtr_inWapingMatrix = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_WARPING_MATRIX);

    sp<IImageBuffer> frameBuf_inMFBO_final_1 = nullptr;
    sp<IImageBuffer> frameBuf_inMFBO_final_2 = nullptr;
    sp<IImageBuffer> frameBuf_inW_1 = nullptr;
    sp<IImageBuffer> frameBuf_inDisparityMap = nullptr;
    sp<IImageBuffer> frameBuf_inWapingMatrix = nullptr;

    framePtr_inMFBO_final_1->getFrameBuffer(frameBuf_inMFBO_final_1);
    framePtr_inMFBO_final_2->getFrameBuffer(frameBuf_inMFBO_final_2);
    framePtr_inW_1->getFrameBuffer(frameBuf_inW_1);
    framePtr_inDisparityMap->getFrameBuffer(frameBuf_inDisparityMap);
    framePtr_inWapingMatrix->getFrameBuffer(frameBuf_inWapingMatrix);

    if(framePtr_inMFBO_final_1 == nullptr){
        MY_LOGE("framePtr_inMFBO_final_1 == nullptr!");
        return MFALSE;
    }

    if(framePtr_inMFBO_final_2 == nullptr){
        MY_LOGE("framePtr_inMFBO_final_2 == nullptr!");
        return MFALSE;
    }

    if(framePtr_inW_1 == nullptr){
        MY_LOGE("framePtr_inW_1 == nullptr!");
        return MFALSE;
    }

    if(frameBuf_inWapingMatrix == nullptr){
        MY_LOGE("frameBuf_inWapingMatrix == nullptr!");
        return MFALSE;
    }

    if(frameBuf_inDisparityMap == nullptr){
        MY_LOGE("frameBuf_inDisparityMap == nullptr!");
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
    IMetadata* pMeta_InHal_main1 = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_main1);
    IMetadata* pMeta_InHal_main2 = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_main2);

    SmartTuningBuffer tuningBuf_main1 = mBufPoolSet.mpTuningBuffer_bufPool->request();
    SmartTuningBuffer tuningBuf_main2 = mBufPoolSet.mpTuningBuffer_bufPool->request();
    TuningParam rTuningParam_main1;
    TuningParam rTuningParam_main2;
    dip_x_reg_t* dip_reg_main1 = nullptr;
    dip_x_reg_t* dip_reg_main2 = nullptr;

    if(tuningBuf_main1 == nullptr){
        MY_LOGE("tuningBuf_main1 == nullptr!");
        return MFALSE;
    }

    if(tuningBuf_main2 == nullptr){
        MY_LOGE("tuningBuf_main2 == nullptr!");
        return MFALSE;
    }
    {
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_main1, mp3AHal_Main1, MFALSE};
        rTuningParam_main1 = applyISPTuning(tuningBuf_main1, ispConfig, USE_DEFAULT_ISP);
        dip_reg_main1 = (dip_x_reg_t*)(rTuningParam_main1.pRegBuf);
    }
    {
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_main2, mp3AHal_Main2, MFALSE};
        rTuningParam_main2 = applyISPTuning(tuningBuf_main2, ispConfig, USE_DEFAULT_ISP);
        dip_reg_main2 = (dip_x_reg_t*)(rTuningParam_main2.pRegBuf);
    }
    // output image buffer
    SmartImageBuffer smBMDN_HAL_out = mBufPoolSet.mpBMDeNoiseResult_bufPool->request();
    if(smBMDN_HAL_out == nullptr){
        MY_LOGE("smBMDN_HAL_out == nullptr!");
        return MFALSE;
    }

    // output meta buffer
    // if needed in future

    EnquedBufPool *BMDNResultData = new EnquedBufPool(request, this);
    BMDNResultData->addBuffData(BID_DENOISE_HAL_OUT, smBMDN_HAL_out);
    // setup BMDN in data
    BMDN_HAL_IN_DATA    inData;
    // shading Gain
    std::unique_ptr<ILscTable> outGain_main1(MAKE_LscTable(ILscTable::GAIN_FLOAT));
    std::unique_ptr<ILscTable> outGain_main2(MAKE_LscTable(ILscTable::GAIN_FLOAT));
    {
        // rotate and byaer order
        getIsRotateAndBayerOrder(inData);

        // OBOffset
        inData.OBOffsetBayer[0] = OB_TO_INT(dip_reg_main1->DIP_X_OBC2_OFFST0.Raw);
        inData.OBOffsetBayer[1] = OB_TO_INT(dip_reg_main1->DIP_X_OBC2_OFFST1.Raw);
        inData.OBOffsetBayer[2] = OB_TO_INT(dip_reg_main1->DIP_X_OBC2_OFFST2.Raw);
        inData.OBOffsetBayer[3] = OB_TO_INT(dip_reg_main1->DIP_X_OBC2_OFFST3.Raw);

        inData.OBOffsetMono[0] = OB_TO_INT(dip_reg_main2->DIP_X_OBC2_OFFST0.Raw);
        inData.OBOffsetMono[1] = OB_TO_INT(dip_reg_main2->DIP_X_OBC2_OFFST1.Raw);
        inData.OBOffsetMono[2] = OB_TO_INT(dip_reg_main2->DIP_X_OBC2_OFFST2.Raw);
        inData.OBOffsetMono[3] = OB_TO_INT(dip_reg_main2->DIP_X_OBC2_OFFST3.Raw);

        // IspGain, use GAIN0 directly
        inData.ispGainBayer = dip_reg_main1->DIP_X_OBC2_GAIN0.Raw;
        inData.ispGainMono = dip_reg_main2->DIP_X_OBC2_GAIN0.Raw;

        // PreGain
        inData.preGainBayer[0] = dip_reg_main1->DIP_X_PGN_GAIN_1.Bits.PGN_GAIN_B;
        inData.preGainBayer[1] = dip_reg_main1->DIP_X_PGN_GAIN_1.Bits.PGN_GAIN_GB;
        inData.preGainBayer[2] = dip_reg_main1->DIP_X_PGN_GAIN_2.Bits.PGN_GAIN_R;

        inData.preGainMono[0] = dip_reg_main2->DIP_X_PGN_GAIN_1.Bits.PGN_GAIN_B;
        inData.preGainMono[1] = dip_reg_main2->DIP_X_PGN_GAIN_1.Bits.PGN_GAIN_GB;
        inData.preGainMono[2] = dip_reg_main2->DIP_X_PGN_GAIN_2.Bits.PGN_GAIN_R;

        // Sensor gain
        getSensorGain(pMeta_InHal_main1, inData, MTRUE);
        getSensorGain(pMeta_InHal_main2, inData, MFALSE);

        StereoArea areaRatioCrop = mSizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);
        inData.OffsetX = areaRatioCrop.startPt.x;
        inData.OffsetY = areaRatioCrop.startPt.y;

        //ISO dependent params
        getISODependentParams(pMeta_InHal_main1, inData);

        getDynamicShadingRA(inData);

        getPerformanceQualityOption(inData);

        // Affine Matrix, the warping matrix from N3D
        getAffineMatrix((float*)frameBuf_inWapingMatrix->getBufVA(0), inData);

        // dPadding, currently always 0
        inData.dPadding[0] = 0;
        inData.dPadding[1] = 0;

        // Image sizes
        StereoArea area = mSizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_1);
        inData.width = area.contentSize().w;
        inData.height = area.contentSize().h;
        inData.paddedWidth = area.contentSize().w;
        inData.paddedHeight = area.contentSize().h;

        // we pass the ROI of disparityMap to ALG
        StereoArea areaDisparityMap = mSizePrvider->getBufferSize(E_DMP_H, eSTEREO_SCENARIO_CAPTURE);
        inData.dwidth = areaDisparityMap.size.w - areaDisparityMap.padding.w*2/2;
        inData.dheight = areaDisparityMap.size.h - areaDisparityMap.padding.h*2/2;
        inData.dPitch = frameBuf_inDisparityMap->getImgSize().w;
        // Add offset to point to the ROI
        inData.depthL = (short*)frameBuf_inDisparityMap->getBufVA(0);
        inData.depthL = inData.depthL + frameBuf_inDisparityMap->getImgSize().w*(areaDisparityMap.padding.h/2) + areaDisparityMap.padding.w/2;

        MY_LOGD("imageBufferSize(%dx%d), ROI(%d,%d,%d,%d), originPointer:%p, offsetPointer:%p",
            frameBuf_inDisparityMap->getImgSize().w,
            frameBuf_inDisparityMap->getImgSize().h,
            areaDisparityMap.padding.w,
            areaDisparityMap.padding.h,
            areaDisparityMap.size.w - areaDisparityMap.padding.w*2/2,
            areaDisparityMap.size.h - areaDisparityMap.padding.h*2/2,
            (short*)frameBuf_inDisparityMap->getBufVA(0),
            inData.depthL
        );

        #if USE_DEFAULT_SHADING_GAIN
            // use default shading gain
            MY_LOGD("Use default shading gain, filled with 1.0");
            {
                float* buffer = new float[SHADING_GAIN_SIZE*SHADING_GAIN_SIZE*4];
                if (buffer == nullptr){
                    MY_LOGE("failed allocating shading gain!");
                    return nullptr;
                }

                // filled with 1.0 for debug now
                fill(buffer,buffer+SHADING_GAIN_SIZE*SHADING_GAIN_SIZE*4-1,1.0);

                inData.BayerGain = buffer;
                inData.MonoGain = buffer;
            }
        #else
            getShadingGain(rTuningParam_main1, *outGain_main1, MTRUE);
            inData.BayerGain = (float*)outGain_main1->getData();

            getShadingGain(rTuningParam_main2, *outGain_main2, MFALSE);
            inData.MonoGain = (float*)outGain_main2->getData();
        #endif

        // Image Buffers
        inData.BayerProcessedRaw = (unsigned short*)frameBuf_inMFBO_final_1->getBufVA(0);
        inData.BayerW = (unsigned short*)frameBuf_inW_1->getBufVA(0);
        inData.MonoProcessedRaw = (unsigned short*)frameBuf_inMFBO_final_2->getBufVA(0);

        inData.output = (unsigned short*)smBMDN_HAL_out->mImageBuffer->getBufVA(0);
    }
    // run BMDN
    if(!BMDN_HAL::getInstance()->BMDNHalRun(inData)){
        MY_LOGE("Error runing BMDNHal, please check above error msgs!");
    }

    localTimer.stop();
    //met_tag_end(0, DO_BM_DENOISE);
    VSDOF_LOGD("BMDeNoise_Profile: featurePipe DeNoiseNode doBMDeNoise time(%d ms) reqID=%d",
        localTimer.getElapsed(),
        request->getRequestNo()
    );

    MY_LOGD("-, reqID=%d", request->getRequestNo());
    return BMDNResultData;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
doMDP(IImageBuffer* src, IImageBuffer* dst)
{
    FUNC_START;
    CAM_TRACE_NAME("DeNoiseNode::doMDP");

    sMDP_Config config;
    sMDP_extConfig extConfig_upper_left, extConfig_lower_right;

    // normal config
    config.pDpStream = mpDpStream;
    config.pSrcBuffer = src;
    config.pDstBuffer = dst;
    config.rotAngle = (360 - eTransformToDegree(mModuleTrans))%360;

    if(mModuleTrans == 0 || mModuleTrans == eTransform_ROT_180){
        // src left -> dst left
        // ext config source
        // *3 => ALG outputs is 3 times(48bits/pixel) to RGB565 data(16bits) in width
        // /2 => do left half only
        extConfig_upper_left.srcSize.w = src->getImgSize().w*3/2;
        extConfig_upper_left.srcSize.h = src->getImgSize().h;
        extConfig_upper_left.srcOffset = 0;
        // *3 => ALG outputs is 3 times to original data
        // *2 => 16bits==2bytes
        extConfig_upper_left.srcStride = src->getImgSize().w*3*2;
        extConfig_upper_left.srcBufSize = extConfig_upper_left.srcSize.h*extConfig_upper_left.srcStride;

        // ext config destination
        // *3 => rotate target is 3 times(48bits/pixel) to RGB565 data(16bits) in width
        // /2 => do left half only
        extConfig_upper_left.dstSize.w = dst->getImgSize().w*3/2;
        extConfig_upper_left.dstSize.h = dst->getImgSize().h;
        extConfig_upper_left.dstOffset = 0;
        // *3 => ALG outputs is 3 times to original data
        // *2 => 16bits==2bytes
        extConfig_upper_left.dstStride = dst->getImgSize().w*3*2;
        extConfig_upper_left.dstBufSize = extConfig_upper_left.dstSize.h*extConfig_upper_left.dstStride;

        // src right -> dst right
        extConfig_lower_right = extConfig_upper_left;
        extConfig_lower_right.srcOffset = extConfig_upper_left.srcStride/2;
        // *2, since extConfig_upper_left.dstSize is RGB565(16bits==2bytes)
        extConfig_lower_right.dstOffset = extConfig_upper_left.dstSize.w*2;
        extConfig_lower_right.srcBufSize = extConfig_upper_left.srcSize.h*extConfig_upper_left.srcStride - extConfig_lower_right.srcOffset;
        extConfig_lower_right.dstBufSize = extConfig_upper_left.dstSize.h*extConfig_upper_left.dstStride - extConfig_lower_right.dstOffset;
    }else if(mModuleTrans == eTransform_ROT_90 || mModuleTrans == eTransform_ROT_270){
        // src upper -> dst left
        // ext config source
        extConfig_upper_left.srcSize.w = src->getImgSize().w;
        // *3 => ALG outputs is 3 times(48bits/pixel) to RGB565 data(16bits) in height
        // /2 => do left half only
        extConfig_upper_left.srcSize.h = src->getImgSize().h*3/2;
        extConfig_upper_left.srcOffset = 0;
        // *2 => 16bits == 2bytes
        extConfig_upper_left.srcStride = src->getImgSize().w*2;
        extConfig_upper_left.srcBufSize = extConfig_upper_left.srcSize.h*extConfig_upper_left.srcStride;

        // ext config destination
        // *3 => rotate target is 3 times(48bits/pixel) to RGB565 data(16bits) in width
        // /2 => do left half only
        extConfig_upper_left.dstSize.w = dst->getImgSize().w*3/2;
        extConfig_upper_left.dstSize.h = dst->getImgSize().h;
        extConfig_upper_left.dstOffset = 0;
        // *3 => ALG outputs is 3 times to original data
        // *2 => 16bits==2bytes
        extConfig_upper_left.dstStride = dst->getImgSize().w*3*2;
        extConfig_upper_left.dstBufSize = extConfig_upper_left.dstSize.h*extConfig_upper_left.dstStride;

        // src lower -> dst right
        extConfig_lower_right = extConfig_upper_left;
        extConfig_lower_right.srcOffset = extConfig_upper_left.srcStride*extConfig_upper_left.srcSize.h;
        // *2, since extConfig_upper_left.dstSize is RGB565(16bits==2bytes)
        extConfig_lower_right.dstOffset = extConfig_upper_left.dstSize.w*2;
        extConfig_lower_right.srcBufSize = extConfig_upper_left.srcSize.h*extConfig_upper_left.srcStride;
        extConfig_lower_right.dstBufSize = extConfig_upper_left.dstSize.h*extConfig_upper_left.dstStride - extConfig_lower_right.dstOffset;
    }else{
        MY_LOGE("currently not support this transform => %d!", mModuleTrans);
        return MFALSE;
    }

    if(!excuteMDPSubProcess(config, extConfig_upper_left)){
        MY_LOGE("failed excuteMDPSubProcess upper->left part");
        return MFALSE;
    }

    if(!excuteMDPSubProcess(config, extConfig_lower_right)){
        MY_LOGE("failed excuteMDPSubProcess lower->right part");
        return MFALSE;
    }

    FUNC_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
doPostProcess(EffectRequestPtr request, EnquedBufPool* pBMDNResultData)
{
    CAM_TRACE_NAME("DeNoiseNode::doPostProcess");
    MY_LOGD("+, reqID=%d", request->getRequestNo());

    // input buffer
    SmartImageBuffer smBMDN_HAL_out = pBMDNResultData->mEnquedSmartImgBufMap.valueFor(BID_DENOISE_HAL_OUT);

    if(smBMDN_HAL_out == nullptr){
        MY_LOGE("smBMDN_HAL_out == nullptr!");
        return MFALSE;
    }

    // input metadata
    FrameInfoPtr framePtr_inAppMeta = request->vInputFrameInfo.valueFor(BID_META_IN_APP);
    FrameInfoPtr framePtr_inHalMeta_main1 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL);

    if(framePtr_inAppMeta == nullptr){
        MY_LOGE("framePtr_inAppMeta == nullptr!");
        return MFALSE;
    }

    if(framePtr_inHalMeta_main1 == nullptr){
        MY_LOGE("framePtr_inHalMeta_main1 == nullptr!");
        return MFALSE;
    }

    IMetadata* pMeta_InApp = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);
    IMetadata* pMeta_InHal_main1 = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_main1);

    if(pMeta_InApp == nullptr){
        MY_LOGE("pMeta_InApp == nullptr!");
        return MFALSE;
    }

    if(pMeta_InHal_main1 == nullptr){
        MY_LOGE("pMeta_InHal_main1 == nullptr!");
        return MFALSE;
    }

    // step 1. use MDP to revert ALG output with module rotation
    // output buffer
    SmartImageBuffer smDenoiseRotBack = nullptr;
    if(mModuleTrans == 0){
        MY_LOGD("no need to do MDP rotate back");
        smDenoiseRotBack = smBMDN_HAL_out;
        // remove padding sizes
        StereoArea hal_out_size;
        hal_out_size = mSizePrvider->getBufferSize(E_BM_DENOISE_HAL_OUT);
        smDenoiseRotBack->mImageBuffer->setExtParam(hal_out_size.contentSize());
    }else{
        smDenoiseRotBack = mBufPoolSet.mpBMDeNoiseResultRotBack_bufPool->request();
        // remove padding sizes
        StereoArea hal_out_size;
        hal_out_size = mSizePrvider->getBufferSize(E_BM_DENOISE_HAL_OUT);
        smBMDN_HAL_out->mImageBuffer->setExtParam(hal_out_size.contentSize());

        Timer localTimer;
        localTimer.start();
        //met_tag_start(0, DO_MDP_ROT_BACK);

        smBMDN_HAL_out->mImageBuffer->syncCache(eCACHECTRL_FLUSH);
        doMDP(smBMDN_HAL_out->mImageBuffer.get(), smDenoiseRotBack->mImageBuffer.get());

        localTimer.stop();
        //met_tag_end(0, DO_MDP_ROT_BACK);
        VSDOF_LOGD("BMDeNoise_Profile: featurePipe DeNoiseNode doMDP rotate back time(%d ms) reqID=%d",
            localTimer.getElapsed(),
            request->getRequestNo()
        );
    }

    // step 2. pass2 output
    // output buffer
    FrameInfoPtr framePtr_denoise_final_result = request->vOutputFrameInfo.valueFor(BID_DENOISE_FINAL_RESULT);
    FrameInfoPtr framePtr_denoise_final_result_thumb = request->vOutputFrameInfo.valueFor(BID_DENOISE_FINAL_RESULT_THUMBNAIL);
    sp<IImageBuffer> frameBuf_denoise_final_result = nullptr;
    sp<IImageBuffer> frameBuf_denoise_final_result_thumb = nullptr;

    framePtr_denoise_final_result->getFrameBuffer(frameBuf_denoise_final_result);
    if(frameBuf_denoise_final_result == nullptr){
        MY_LOGE("frameBuf_denoise_final_result == nullptr!");
        return MFALSE;
    }

    framePtr_denoise_final_result_thumb->getFrameBuffer(frameBuf_denoise_final_result_thumb);
    if(frameBuf_denoise_final_result_thumb == nullptr){
        MY_LOGE("frameBuf_denoise_final_result_thumb == nullptr!");
        return MFALSE;
    }

    // output meta

    // tuning buffer
    SmartTuningBuffer tuningBuf_main1 = mBufPoolSet.mpTuningBuffer_bufPool->request();

    if(tuningBuf_main1 == nullptr){
        MY_LOGE("tuningBuf_main1 == nullptr!");
        return MFALSE;
    }

    {
        // Normally fullraw are not cropped before enque to pass2, but we DO crop the MFBO, so that we have to
        // tell 3A ISP the cropped region for correct shading link
        StereoArea fullraw_crop;
        fullraw_crop = mSizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);
        trySetMetadata<MRect>(pMeta_InHal_main1, MTK_P1NODE_SCALAR_CROP_REGION, MRect(fullraw_crop.startPt, fullraw_crop.size));
        trySetMetadata<MSize>(pMeta_InHal_main1, MTK_P1NODE_RESIZER_SIZE, fullraw_crop.size);

      	trySetMetadata<MUINT8>(pMeta_InHal_main1, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Denoise);
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_main1, mp3AHal_Main1, MFALSE};
        TuningParam rTuningParam = applyISPTuning(tuningBuf_main1, ispConfig);
    }

    // prepare P2 config
    MINT32 eTransJpeg = getJpegRotation(pMeta_InApp);

    EnquedBufPool *enquedData = new EnquedBufPool(request, this);
    enquedData->start();
    enquedData->addBuffData(BID_DENOISE_HAL_OUT, smBMDN_HAL_out);
    enquedData->addBuffData(BID_DENOISE_HAL_OUT_ROT_BACK, smDenoiseRotBack);
    enquedData->addIImageBuffData(BID_DENOISE_FINAL_RESULT, frameBuf_denoise_final_result);
    enquedData->addIImageBuffData(BID_DENOISE_FINAL_RESULT_THUMBNAIL, frameBuf_denoise_final_result_thumb);
    enquedData->addTuningData(tuningBuf_main1);

    // do P2 post-process
    QParams enqueParams;
    {
        int frameGroup = 0;

        enqueParams.mvStreamTag.push_back(ENormalStreamTag_Normal);
        enqueParams.mvSensorIdx.push_back(mSensorIdx_Main1);
        enqueParams.mvTuningData.push_back(tuningBuf_main1->mpVA);

        Input src;
        src.mPortID = PORT_IMGI;
        src.mPortID.group = frameGroup;
        src.mBuffer = smDenoiseRotBack->mImageBuffer.get();
        enqueParams.mvIn.push_back(src);

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

        // WROT, for result YUV
        crop3.mGroupID = 3;
        crop3.mFrameGroup = frameGroup;
        crop3.mCropRect.p_fractional.x=0;
        crop3.mCropRect.p_fractional.y=0;
        crop3.mCropRect.p_integral.x=0;
        crop3.mCropRect.p_integral.y=0;
        crop3.mCropRect.s.w=src.mBuffer->getImgSize().w;
        crop3.mCropRect.s.h=src.mBuffer->getImgSize().h;
        crop3.mResizeDst.w=frameBuf_denoise_final_result->getImgSize().w;
        crop3.mResizeDst.h=frameBuf_denoise_final_result->getImgSize().h;
        enqueParams.mvCropRsInfo.push_back(crop3);

        Output out;
        out.mPortID = PORT_WROT;
        out.mPortID.group = frameGroup;
        out.mBuffer = frameBuf_denoise_final_result.get();
        out.mTransform = eTransJpeg;
        enqueParams.mvOut.push_back(out);
    }

    enqueParams.mpfnCallback = onP2SuccessCallback;
    enqueParams.mpfnEnQFailCallback = onP2FailedCallback;
    enqueParams.mpCookie = (MVOID*) enquedData;

    // run P2
    MY_LOGD("mpINormalStream->enque start! reqID=%d", request->getRequestNo());
    enquedData->start();
    //met_tag_start(0, DO_POST_PROCESS);

    if(!mpINormalStream->enque(enqueParams))
    {
        MY_LOGE("mpINormalStream enque failed! reqID=%d", request->getRequestNo());
        return MFALSE;
    }

    MY_LOGD("mpINormalStream->enque done! reqID=%d", request->getRequestNo());

    delete pBMDNResultData;

    MY_LOGD("-, reqID=%d", request->getRequestNo());
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
doNonDeNoiseCapture(EffectRequestPtr request)
{
    CAM_TRACE_NAME("DeNoiseNode::doNonDeNoiseCapture");
    MY_LOGD("+, reqID=%d", request->getRequestNo());

    FrameInfoPtr framePtr_inMain1FSRAW = request->vInputFrameInfo.valueFor(BID_PRE_PROCESS_IN_FULLRAW_1);
    sp<IImageBuffer> frameBuf_inFSRAW1 = nullptr;
    framePtr_inMain1FSRAW->getFrameBuffer(frameBuf_inFSRAW1);

    if(frameBuf_inFSRAW1 == nullptr){
        MY_LOGE("frameBuf_inFSRAW1 == nullptr!");
        return MFALSE;
    }

    // input metadata
    FrameInfoPtr framePtr_inAppMeta = request->vInputFrameInfo.valueFor(BID_META_IN_APP);
    FrameInfoPtr framePtr_inHalMeta_main1 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL);

    if(framePtr_inAppMeta == nullptr){
        MY_LOGE("framePtr_inAppMeta == nullptr!");
        return MFALSE;
    }

    if(framePtr_inHalMeta_main1 == nullptr){
        MY_LOGE("framePtr_inHalMeta_main1 == nullptr!");
        return MFALSE;
    }

    IMetadata* pMeta_InApp = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);
    IMetadata* pMeta_InHal_main1 = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_main1);

    if(pMeta_InApp == nullptr){
        MY_LOGE("pMeta_InApp == nullptr!");
        return MFALSE;
    }

    if(pMeta_InHal_main1 == nullptr){
        MY_LOGE("pMeta_InHal_main1 == nullptr!");
        return MFALSE;
    }

    // output buffer
    FrameInfoPtr framePtr_denoise_final_result = request->vOutputFrameInfo.valueFor(BID_DENOISE_FINAL_RESULT);
    FrameInfoPtr framePtr_denoise_final_result_thumb = request->vOutputFrameInfo.valueFor(BID_DENOISE_FINAL_RESULT_THUMBNAIL);
    sp<IImageBuffer> frameBuf_denoise_final_result = nullptr;
    sp<IImageBuffer> frameBuf_denoise_final_result_thumb = nullptr;

    framePtr_denoise_final_result->getFrameBuffer(frameBuf_denoise_final_result);
    if(frameBuf_denoise_final_result == nullptr){
        MY_LOGE("frameBuf_denoise_final_result == nullptr!");
        return MFALSE;
    }

    framePtr_denoise_final_result_thumb->getFrameBuffer(frameBuf_denoise_final_result_thumb);
    if(frameBuf_denoise_final_result_thumb == nullptr){
        MY_LOGE("frameBuf_denoise_final_result_thumb == nullptr!");
        return MFALSE;
    }

    TuningParam rTuningParam_main1;
    SmartTuningBuffer tuningBuf_main1 = mBufPoolSet.mpTuningBuffer_bufPool->request();

    if(tuningBuf_main1 == nullptr){
        MY_LOGE("tuningBuf_main1 == nullptr!");
        return MFALSE;
    }

    {
        trySetMetadata<MUINT8>(pMeta_InHal_main1, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Capture);
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_main1, mp3AHal_Main1, MFALSE};
        rTuningParam_main1 = applyISPTuning(tuningBuf_main1, ispConfig, USE_DEFAULT_ISP);
    }

    EnquedBufPool *enquedData = new EnquedBufPool(request, this);
    enquedData->start();
    enquedData->addIImageBuffData(BID_DENOISE_FINAL_RESULT, frameBuf_denoise_final_result);
    enquedData->addIImageBuffData(BID_DENOISE_FINAL_RESULT_THUMBNAIL, frameBuf_denoise_final_result_thumb);
    enquedData->addTuningData(tuningBuf_main1);

    StereoSizeProvider* sizePrvider = StereoSizeProvider::getInstance();
    StereoArea area;

    QParams enqueParams, dequeParams;
    // main1_fullraw to capture YUV
    {
        int frameGroup = 0;

        enqueParams.mvStreamTag.push_back(ENormalStreamTag_Normal);
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
        fullraw_crop = sizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);

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
        crop3.mGroupID = 3;
        crop3.mFrameGroup = frameGroup;
        crop3.mCropRect.p_fractional.x=0;
        crop3.mCropRect.p_fractional.y=0;
        crop3.mCropRect.p_integral.x=fullraw_crop.startPt.x;
        crop3.mCropRect.p_integral.y=fullraw_crop.startPt.y;
        crop3.mCropRect.s = fullraw_crop.size;
        crop3.mResizeDst = frameBuf_denoise_final_result->getImgSize();
        enqueParams.mvCropRsInfo.push_back(crop3);

        Output out;
        out.mPortID = PORT_WROT;
        out.mPortID.group = frameGroup;
        out.mBuffer = frameBuf_denoise_final_result.get();
        out.mTransform = getJpegRotation(pMeta_InApp);
        enqueParams.mvOut.push_back(out);
    }

    enqueParams.mpfnCallback = onP2SuccessCallback;
    enqueParams.mpfnEnQFailCallback = onP2FailedCallback;
    enqueParams.mpCookie = (MVOID*) enquedData;

    // run P2
    MY_LOGD("mpINormalStream->enque normal pass2 start! reqID=%d", request->getRequestNo());
    //met_tag_start(0, DO_POST_PROCESS);

    if(!mpINormalStream->enque(enqueParams))
    {
        MY_LOGE("mpINormalStream enque normal pass2 failed! reqID=%d", request->getRequestNo());
        return MFALSE;
    }

    MY_LOGD("mpINormalStream->enque normal pass2 done! reqID=%d", request->getRequestNo());
    MY_LOGD("-, reqID=%d", request->getRequestNo());
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
onThreadStart()
{
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
onThreadStop()
{
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
onThreadLoop()
{
    FUNC_START;
    EffectRequestPtr effectRequest = nullptr;

    if( !waitAllQueue() )// block until queue ready, or flush() breaks the blocking state too.
    {
        return MFALSE;
    }
    if( !mRequests.deque(effectRequest) )
    {
        MY_LOGD("mRequests.deque() failed");
        return MFALSE;
    }

    CAM_TRACE_NAME("DeNoiseNode::onThreadLoop");

    // get request type
    const sp<EffectParameter> params = effectRequest->getRequestParameter();
    MINT32 requestType = params->getInt(BMDENOISE_REQUEST_TYPE_KEY);

    doInputDataDump(effectRequest);

    //check if we need to do denoise
    if(requestType == TYPE_BM_DENOISE){
        EnquedBufPool* BMDN_HAL_result = doBMDeNoise(effectRequest);
        if(BMDN_HAL_result != nullptr){
            MY_LOGD("doBMDeNoise success");
        }else{
            MY_LOGE("doBMDeNoise failed!");
        }

        if(doPostProcess(effectRequest, BMDN_HAL_result)){
            MY_LOGD("doPostProcess success");
        }else{
            MY_LOGE("doPostProcess failed!");
        }
    }else{
        if(doNonDeNoiseCapture(effectRequest)){
            MY_LOGD("doNonDeNoiseCapture success");
        }else{
            MY_LOGE("doNonDeNoiseCapture failed!");
        }
    }

    FUNC_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
prepareModuleSettings()
{
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
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
IMetadata*
DeNoiseNode::
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
DeNoiseNode::
applyISPTuning(
        SmartTuningBuffer& targetTuningBuf,
        const ISPTuningConfig& ispConfig,
        MBOOL useDefault
)
{
    CAM_TRACE_NAME("DeNoiseNode::applyISPTuning");
    VSDOF_LOGD("+, reqID=%d bIsResized=%d", ispConfig.pInAppMetaFrame->getRequestNo(), ispConfig.bInputResizeRaw);

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
DeNoiseNode::
onP2SuccessCallback(QParams& rParams)
{
    EnquedBufPool* pEnqueData = (EnquedBufPool*) (rParams.mpCookie);
    DeNoiseNode* pNode = (DeNoiseNode*) (pEnqueData->mpNode);

    pNode->handleP2Done(rParams, pEnqueData);
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
DeNoiseNode::
onP2FailedCallback(QParams& rParams)
{
    MY_LOGE("DeNoiseNode operations failed!!Check the following log:");
    EnquedBufPool* pEnqueData = (EnquedBufPool*) (rParams.mpCookie);
    DeNoiseNode* pNode = (DeNoiseNode*) (pEnqueData->mpNode);
    pNode->handleData(ERROR_OCCUR_NOTIFY, pEnqueData->mRequest);
    delete pEnqueData;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
DeNoiseNode::
handleP2Done(QParams& rParams, EnquedBufPool* pEnqueBufPool)
{
    CAM_TRACE_NAME("DeNoiseNode::handleP2Done");
    EffectRequestPtr request = pEnqueBufPool->mRequest;
    VSDOF_LOGD("+ :reqID=%d", request->getRequestNo());

    pEnqueBufPool->stop();
    //met_tag_end(0, DO_POST_PROCESS);
    VSDOF_LOGD("BMDeNoise_Profile: featurePipe p2 post-process time(%d ms) reqID=%d",
        pEnqueBufPool->getElapsed(),
        request->getRequestNo()
    );

    Timer localTimer;
    localTimer.start();
    //met_tag_start(0, DO_THUMBNAIL);
    {
        CAM_TRACE_NAME("DeNoiseNode::MDP for Thumbnail");
        // do MDP rotate for thumbnail
        {
            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = pEnqueBufPool->mEnquedIImgBufMap.valueFor(BID_DENOISE_FINAL_RESULT).get();
            config.pDstBuffer = pEnqueBufPool->mEnquedIImgBufMap.valueFor(BID_DENOISE_FINAL_RESULT_THUMBNAIL).get();
            config.rotAngle = 0;

            MY_LOGD("src %dx%d  dst:%dx%d",
                config.pSrcBuffer->getImgSize().w,config.pSrcBuffer->getImgSize().h,
                config.pDstBuffer->getImgSize().w,config.pDstBuffer->getImgSize().h
            );

            if(!excuteMDP(config))
            {
                MY_LOGE("excuteMDP for Thumbnail failed.");
            }
        }
    }
    localTimer.stop();
    //met_tag_end(0, DO_THUMBNAIL);
    VSDOF_LOGD("BMDeNoise_Profile: featurePipe DeNoiseNode thumbnail MDP time(%d ms) reqID=%d",
        localTimer.getElapsed(),
        request->getRequestNo()
    );

    doOutputDataDump(request, pEnqueBufPool);

    handleData(DENOISE_RESULT_OUT, request);

    delete pEnqueBufPool;

     VSDOF_LOGD("- :reqID=%d", request->getRequestNo());
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
DeNoiseNode::
doDataDump(IImageBuffer* pBuf, BMDeNoiseBufferID BID, MUINT32 iReqIdx)
{
    VSDOF_LOGD("doDataDump: BID:%d +", BID);

    char filepath[1024];
    snprintf(filepath, 1024, "/sdcard/bmdenoise/%d/%s", iReqIdx, getName());

    // make path
    VSDOF_LOGD("makePath: %s", filepath);
    makePath(filepath, 0660);

    const char* writeFileName = onDumpBIDToName(BID);

    char writepath[1024];
    snprintf(writepath,
        1024, "%s/%s_%dx%d.yuv", filepath, writeFileName,
        pBuf->getImgSize().w, pBuf->getImgSize().h);

    pBuf->saveToFile(writepath);

    VSDOF_LOGD("doDataDump: BID:%d -", BID);
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
DeNoiseNode::
doInputDataDump(EffectRequestPtr request)
{
    MUINT32 iReqIdx = request->getRequestNo();

    // get request type
    const sp<EffectParameter> params = request->getRequestParameter();
    MINT32 requestType = params->getInt(BMDENOISE_REQUEST_TYPE_KEY);
    if(requestType != TYPE_BM_DENOISE){
        return;
    }

    if(miTuningDump == 0){
        if(iReqIdx < miDumpStartIdx || iReqIdx >= miDumpStartIdx + miDumpBufSize)
            return;

        // input image buffer
        FrameInfoPtr framePtr_inMFBO_final_1 = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_MFBO_FINAL_1);
        FrameInfoPtr framePtr_inMFBO_final_2 = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_MFBO_FINAL_2);
        FrameInfoPtr framePtr_inW_1 = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_W_1);
        FrameInfoPtr framePtr_inDisparityMap = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_DISPARITY_MAP_1);
        FrameInfoPtr framePtr_inWapingMatrix = request->vInputFrameInfo.valueFor(BID_DENOISE_IN_WARPING_MATRIX);

        sp<IImageBuffer> frameBuf_inMFBO_final_1 = nullptr;
        sp<IImageBuffer> frameBuf_inMFBO_final_2 = nullptr;
        sp<IImageBuffer> frameBuf_inW_1 = nullptr;
        sp<IImageBuffer> frameBuf_inDisparityMap = nullptr;
        sp<IImageBuffer> frameBuf_inWapingMatrix = nullptr;

        framePtr_inMFBO_final_1->getFrameBuffer(frameBuf_inMFBO_final_1);
        framePtr_inMFBO_final_2->getFrameBuffer(frameBuf_inMFBO_final_2);
        framePtr_inW_1->getFrameBuffer(frameBuf_inW_1);
        framePtr_inDisparityMap->getFrameBuffer(frameBuf_inDisparityMap);
        framePtr_inWapingMatrix->getFrameBuffer(frameBuf_inWapingMatrix);

        doDataDump(
            frameBuf_inMFBO_final_1.get(),
            BID_DENOISE_IN_MFBO_FINAL_1,
            iReqIdx
        );

        doDataDump(
            frameBuf_inMFBO_final_2.get(),
            BID_DENOISE_IN_MFBO_FINAL_2,
            iReqIdx
        );

        doDataDump(
            frameBuf_inW_1.get(),
            BID_DENOISE_IN_W_1,
            iReqIdx
        );

        doDataDump(
            frameBuf_inDisparityMap.get(),
            BID_DENOISE_IN_DISPARITY_MAP_1,
            iReqIdx
        );
    }else{
        MY_LOGD("do tuning dump for req(%d) inputs", iReqIdx);
        // add doDataDump() here...
    }

    // make path for ALG
    char filepathAlg[1024];
    snprintf(filepathAlg, 1024, "/sdcard/bmdenoise/CModelData");
    VSDOF_LOGD("makePath: %s", filepathAlg);
    makePath(filepathAlg, 0777);
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
DeNoiseNode::
doOutputDataDump(EffectRequestPtr request, EnquedBufPool* pEnqueBufPool)
{
    MUINT32 iReqIdx = request->getRequestNo();

    // get request type
    const sp<EffectParameter> params = request->getRequestParameter();
    MINT32 requestType = params->getInt(BMDENOISE_REQUEST_TYPE_KEY);
    if(requestType != TYPE_BM_DENOISE){
        system("rm /sdcard/bmdenoise/*.raw");
        system("rm /sdcard/bmdenoise/*.bmp");
        system("rm -rf /sdcard/bmdenoise/CModelData");
        system("rm -rf /sdcard/debug");
        system("mkdir /sdcard/debug");
        return;
    }

    if(miTuningDump == 0){
        if(iReqIdx < miDumpStartIdx || iReqIdx >= miDumpStartIdx + miDumpBufSize)
            return;

        doDataDump(
            pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_DENOISE_HAL_OUT)->mImageBuffer.get(),
            BID_DENOISE_HAL_OUT,
            iReqIdx
        );
        doDataDump(
            pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_DENOISE_HAL_OUT_ROT_BACK)->mImageBuffer.get(),
            BID_DENOISE_HAL_OUT_ROT_BACK,
            iReqIdx
        );
        doDataDump(
            pEnqueBufPool->mEnquedIImgBufMap.valueFor(BID_DENOISE_FINAL_RESULT).get(),
            BID_DENOISE_FINAL_RESULT,
            iReqIdx
        );
        doDataDump(
            pEnqueBufPool->mEnquedIImgBufMap.valueFor(BID_DENOISE_FINAL_RESULT_THUMBNAIL).get(),
            BID_DENOISE_FINAL_RESULT_THUMBNAIL,
            iReqIdx
        );
    }else{
        MY_LOGD("do tuning dump for req(%d) ouputs", iReqIdx);
        doDataDump(
            pEnqueBufPool->mEnquedSmartImgBufMap.valueFor(BID_DENOISE_HAL_OUT_ROT_BACK)->mImageBuffer.get(),
            BID_DENOISE_HAL_OUT_ROT_BACK,
            iReqIdx
        );
    }

    // move alg debug info and 3A debug info into MW debug folder
    char targetPath[1024];
    snprintf(targetPath, 1024, "/sdcard/bmdenoise/%d/%s", iReqIdx, getName());

    // P1 full-raw
    char cmd[1024];
    snprintf(cmd, 1024, "cp -R /sdcard/vsdof/capture/Rear/%d/P2AFM/*.raw %s/", iReqIdx, targetPath);
    VSDOF_LOGD("%s", cmd);
    system(cmd);

    snprintf(cmd, 1024, "cp -R /sdcard/vsdof/capture/Front/%d/P2AFM/*.raw %s/", iReqIdx, targetPath);
    VSDOF_LOGD("%s", cmd);
    system(cmd);

    // ALG
    snprintf(cmd, 1024, "cp -R /sdcard/bmdenoise/CModelData %s/", targetPath);
    VSDOF_LOGD("%s", cmd);
    system(cmd);

    snprintf(cmd, 1024, "cp /sdcard/bmdenoise/*.raw %s/", targetPath);
    VSDOF_LOGD("%s", cmd);
    system(cmd);

    snprintf(cmd, 1024, "cp /sdcard/bmdenoise/*.bmp %s/", targetPath);
    VSDOF_LOGD("%s", cmd);
    system(cmd);

    // 3A
    snprintf(cmd, 1024, "cp -R /sdcard/debug %s/", targetPath);
    VSDOF_LOGD("%s", cmd);
    system(cmd);

    VSDOF_LOGD("rm /sdcard/bmdenoise/*.raw");
    system("rm /sdcard/bmdenoise/*.raw");

    VSDOF_LOGD("rm /sdcard/bmdenoise/*.bmp");
    system("rm /sdcard/bmdenoise/*.bmp");

    VSDOF_LOGD("rm -rf /sdcard/bmdenoise/CModelData");
    system("rm -rf /sdcard/bmdenoise/CModelData");

    VSDOF_LOGD("rm -rf /sdcard/debug");
    system("rm -rf /sdcard/debug");

    VSDOF_LOGD("mkdir /sdcard/debug");
    system("mkdir /sdcard/debug");
}
/*******************************************************************************
 *
 ********************************************************************************/
const char*
DeNoiseNode::
onDumpBIDToName(BMDeNoiseBufferID BID)
{
    #define MAKE_NAME_CASE(name) \
        case name: return #name;

    switch(BID)
    {
        MAKE_NAME_CASE(BID_DENOISE_IN_MFBO_FINAL_1);
        MAKE_NAME_CASE(BID_DENOISE_IN_MFBO_FINAL_2);
        MAKE_NAME_CASE(BID_DENOISE_IN_W_1);
        MAKE_NAME_CASE(BID_DENOISE_IN_DISPARITY_MAP_1);
        MAKE_NAME_CASE(BID_DENOISE_HAL_OUT);
        MAKE_NAME_CASE(BID_DENOISE_HAL_OUT_ROT_BACK);
        MAKE_NAME_CASE(BID_DENOISE_FINAL_RESULT);
        MAKE_NAME_CASE(BID_DENOISE_FINAL_RESULT_THUMBNAIL);
    }
    MY_LOGW("unknown BID:%d", BID);

    return "unknown";
    #undef MAKE_NAME_CASE
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getIsRotateAndBayerOrder(BMDN_HAL_IN_DATA &rInData)
{
    // bayer order
    rInData.bayerOrder = mBayerOrder_main1;

    // isRotate
    switch(mModuleTrans)
    {
        case 0:
            rInData.isRotate = 0;
            break;
        case eTransform_ROT_90:
            rInData.isRotate = 1;
            break;
        // currently not supported rotation
        // case eTransform_ROT_180:
        //     inData.isRotate = 1;
        //     break;
        // case eTransform_ROT_270:
        //     inData.isRotate = 1;
        //     break;
        default:
            MY_LOGE("ALG not support module rotation(%d) for now!", mModuleTrans);
            return MFALSE;
    }

    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getSensorGain(IMetadata* pMeta, BMDN_HAL_IN_DATA &rInData, MBOOL isBayer)
{
    CAM_TRACE_NAME("DeNoiseNode::getSensorGain");
    IMetadata::Memory meta;
    if(tryGetMetadata<IMetadata::Memory>(pMeta, MTK_PROCESSOR_CAMINFO, meta))
    {
        RAWIspCamInfo pCamInfo;
        ::memcpy(&pCamInfo, meta.array(), sizeof(NSIspTuning::RAWIspCamInfo));

        if(isBayer){
            rInData.sensorGainBayer = pCamInfo.rAEInfo.u4AfeGain;
        }else{
            rInData.sensorGainMono = pCamInfo.rAEInfo.u4AfeGain;
        }
        return MTRUE;
    }else{
        MY_LOGE("Get MTK_PROCESSOR_CAMINFO failed.");
        return MFALSE;
    }
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getISODependentParams(IMetadata* pMeta, BMDN_HAL_IN_DATA &rInData)
{
    MINT32 ISO = 0;
    MINT32 debugISO = 0;
    IMetadata exifMeta;

    if( tryGetMetadata<IMetadata>(pMeta, MTK_3A_EXIF_METADATA, exifMeta) ) {
        if(!tryGetMetadata<MINT32>(&exifMeta, MTK_3A_EXIF_AE_ISO_SPEED, ISO)){
            MY_LOGE("Get ISO from meta failed, use default value:%d", ISO);
        }
    }
    else {
        MY_LOGE("no tag: MTK_3A_EXIF_METADATA, use default value:%d", ISO);
    }

    debugISO = ::property_get_int32("debug.bmdenoise.iso", -1);

    MY_LOGD("metaISO:%d/debugISO:%d", ISO, debugISO);

    if(debugISO != -1 && debugISO >= 0){
        ISO = debugISO;
    }

    BMDeNoiseISODependentParam ISODependentParams = getBMDeNoiseISODePendentParams(ISO);

    rInData.BW_SingleRange = ISODependentParams.BW_SingleRange;
    rInData.BW_OccRange = ISODependentParams.BW_OccRange;
    rInData.BW_Range = ISODependentParams.BW_Range;
    rInData.BW_Kernel = ISODependentParams.BW_Kernel;
    rInData.B_Range = ISODependentParams.B_Range;
    rInData.B_Kernel = ISODependentParams.B_Kernel;
    rInData.W_Range = ISODependentParams.W_Range;
    rInData.W_Kernel = ISODependentParams.W_Kernel;
    rInData.VSL = ISODependentParams.VSL;
    rInData.VOFT = ISODependentParams.VOFT;
    rInData.VGAIN = ISODependentParams.VGAIN;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getDynamicShadingRA(BMDN_HAL_IN_DATA &rInData)
{
    IspTuningCustom* pTuningCustom = IspTuningCustom::createInstance(ESensorDev_Main, mSensorIdx_Main1);;
    MUINT32 ret = -1;

    if(pTuningCustom != nullptr){
        ret = pTuningCustom->dynamic_Rto;
    }else{
        ret = DEFAULT_DYNAMIC_SHADING;
        MY_LOGE("Failed to create IspTuningCustom Instance. use default value");
    }

    MY_LOGD("get dynamic shading RA:%d", ret);
    rInData.RA = ret;

    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getPerformanceQualityOption(BMDN_HAL_IN_DATA &rInData)
{
    BMDeNoiseQualityPerformanceParam param = getBMDeNoiseQualityPerformanceParam();

    rInData.dsH = (mDebugPerformanceQualityOption.dsH == -1) ? param.dsH : mDebugPerformanceQualityOption.dsH;
    rInData.dsV = (mDebugPerformanceQualityOption.dsV == -1) ? param.dsV : mDebugPerformanceQualityOption.dsV;
    rInData.FPREPROC = (mDebugPerformanceQualityOption.FPREPROC == -1) ? param.FPREPROC : mDebugPerformanceQualityOption.FPREPROC;
    rInData.FSSTEP = (mDebugPerformanceQualityOption.FSSTEP == -1) ? param.FSSTEP : mDebugPerformanceQualityOption.FSSTEP;

    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getAffineMatrix(float* warpingMatrix, BMDN_HAL_IN_DATA &rInData)
{
    #if USE_DEFAULT_AMATRIX
        rInData.Trans[0] = 1;
        rInData.Trans[1] = 0;
        rInData.Trans[2] = 0;
        rInData.Trans[3] = 0;
        rInData.Trans[4] = 1;
        rInData.Trans[5] = 0;
        rInData.Trans[6] = 0;
        rInData.Trans[7] = 0;
        rInData.Trans[8] = 1;
        MY_LOGD("Test mode, use identity matrix");
    #else
        rInData.Trans[0] = warpingMatrix[0];
        rInData.Trans[1] = warpingMatrix[1];
        rInData.Trans[2] = warpingMatrix[2];
        rInData.Trans[3] = warpingMatrix[3];
        rInData.Trans[4] = warpingMatrix[4];
        rInData.Trans[5] = warpingMatrix[5];
        rInData.Trans[6] = warpingMatrix[6];
        rInData.Trans[7] = warpingMatrix[7];
        rInData.Trans[8] = warpingMatrix[8];
    #endif

    MY_LOGD("warpingMatrix: [%f,%f,%f][%f,%f,%f][%f,%f,%f]",
        rInData.Trans[0],
        rInData.Trans[1],
        rInData.Trans[2],
        rInData.Trans[3],
        rInData.Trans[4],
        rInData.Trans[5],
        rInData.Trans[6],
        rInData.Trans[7],
        rInData.Trans[8]
    );
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
DeNoiseNode::
getShadingGain(TuningParam& tuningParam, ILscTable& outGain, MBOOL isBayer)
{
    CAM_TRACE_NAME("DeNoiseNode::getShadingGain");
    StereoArea area_src;
    StereoArea area_dst;
    std::unique_ptr<ILscTable> tmpOutGain(MAKE_LscTable(ILscTable::GAIN_FIXED));

    if(isBayer){
        area_src = mSizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_1);
        area_dst = mSizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);
    }else{
        area_src = mSizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_2);
        area_dst = mSizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_2);
    }

    IImageBuffer* pLsc2Buf = (IImageBuffer*)tuningParam.pLsc2Buf;
    std::unique_ptr<ILscTable> inputHw(MAKE_LscTable(ILscTable::HWTBL), area_src.size.w, area_src.size.h, 17, 17); //17x17 is hw limitation, do not change it
    inputHw->setData((MVOID*)pLsc2Buf->getBufVA(0), inputHw->getSize());

    MY_LOGD("ShadingGain srcResize:%d,%d, dstCrop:%d,%d,%d,%d, gridSize:%d,%d",
        // step1. src resize, the same as p1 full out
        area_src.size.w, area_src.size.h,
        // step2. dst crop to be 16:9 or 4:3, the same as alg size
        area_dst.startPt.x, area_dst.startPt.y, area_dst.size.w, area_dst.size.h,
        // step3. result shading gain table grid. No matter what the aspect ratio of dst crop is, it is always a 1:1 square
        SHADING_GAIN_SIZE, SHADING_GAIN_SIZE
    );
    inputHw->cropOut(
        ILscTable::TransformCfg_T(
            area_src.size.w, area_src.size.h,
            SHADING_GAIN_SIZE, SHADING_GAIN_SIZE,
            area_dst.startPt.x, area_dst.startPt.y, area_dst.size.w, area_dst.size.h
        ),
        *tmpOutGain
    );

    tmpOutGain->convert(outGain);

    if(miDumpShadingGain == 1){
        if(isBayer){
            tmpOutGain->dump("sdcard/bmdenoise/shadingBayer_fixed.raw");
            outGain.dump("sdcard/bmdenoise/shadingBayer_float.raw");

            float* pTmpOutGain = (float*)tmpOutGain->getData();
            float* pOutGain = (float*)outGain.getData();

            MY_LOGD("shadingBayer Fixed:%f,%f,%f,%f,%f  Float:%f,%f,%f,%f,%f",
                pTmpOutGain[0], pTmpOutGain[1], pTmpOutGain[2], pTmpOutGain[3], pTmpOutGain[4],
                pOutGain[0], pOutGain[1], pOutGain[2], pOutGain[3], pOutGain[4]
            );

        }else{
            tmpOutGain->dump("sdcard/bmdenoise/shadingMono_fixed.raw");
            outGain.dump("sdcard/bmdenoise/shadingMono_float.raw");

            float* pTmpOutGain = (float*)tmpOutGain->getData();
            float* pOutGain = (float*)outGain.getData();

            MY_LOGD("shadingMono Fixed:%f,%f,%f,%f,%f  Float:%f,%f,%f,%f,%f",
                pTmpOutGain[0], pTmpOutGain[1], pTmpOutGain[2], pTmpOutGain[3], pTmpOutGain[4],
                pOutGain[0], pOutGain[1], pOutGain[2], pOutGain[3], pOutGain[4]
            );
        }
    }
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MINT32
DeNoiseNode::
eTransformToDegree(MINT32 eTrans)
{
    switch(eTrans)
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
MINT32
DeNoiseNode::
DegreeToeTransform(MINT32 degree)
{
    switch(degree)
    {
        case 0:
            return 0;
        case 90:
            return eTransform_ROT_90;
        case 180:
            return eTransform_ROT_180;
        case 270:
            return eTransform_ROT_270;
        default:
            MY_LOGE("Not support degree =%d", degree);
            return -1;
    }
}

/*******************************************************************************
 *
 ********************************************************************************/
MINT32
DeNoiseNode::
getJpegRotation(IMetadata* pMeta)
{
    MINT32 jpegRotationApp = 0;
    if(!tryGetMetadata<MINT32>(pMeta, MTK_JPEG_ORIENTATION, jpegRotationApp))
    {
        MY_LOGE("Get jpegRotationApp failed!");
    }

    MINT32 rotDegreeJpeg = jpegRotationApp;
    if(rotDegreeJpeg < 0){
        rotDegreeJpeg = rotDegreeJpeg + 360;
    }

    MY_LOGD("jpegRotationApp:%d, rotDegreeJpeg:%d", jpegRotationApp, rotDegreeJpeg);

    return DegreeToeTransform(rotDegreeJpeg);;
}
