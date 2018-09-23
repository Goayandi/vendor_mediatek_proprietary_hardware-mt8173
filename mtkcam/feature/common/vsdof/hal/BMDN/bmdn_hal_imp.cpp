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

#define LOG_TAG "BMDN_HAL_IMP"

// Standard C header file
#include <sstream>

// Android system/core header file

// mtkcam custom header file
#include <camera_custom_stereo.h>
#include <n3d_sync2a_tuning_param.h>

// mtkcam global header file

// Module header file
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <vsdof/hal/AffinityUtil.h>

// Local header file
#include "bmdn_hal_imp.h"

#define SHADING_GAIN_CONFIG 200
#define SHADING_GAIN_WIDTH SHADING_GAIN_CONFIG
#define SHADING_GAIN_HEIGHT SHADING_GAIN_CONFIG

#define OB_OFFSET_SIZE 4
#define PRE_GAIN_SIZE 3
#define WARPING_MATRIX_SIZE 9
#define ALGO_PADDING 32

// using namespace std;
using namespace NS_BMDN_HAL;

/*******************************************************************************
* Global Define
********************************************************************************/
#ifndef GTEST
#define MY_LOGD(fmt, arg...)    CAM_LOGD("[%s][%s]" fmt, getUserName(), __func__, ##arg)
#define MY_LOGI(fmt, arg...)    CAM_LOGI("[%s][%s]" fmt, getUserName(), __func__, ##arg)
#define MY_LOGW(fmt, arg...)    CAM_LOGW("[%s][%s] WRN(%5d):" fmt, getUserName(), __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    CAM_LOGE("[%s][%s] %s ERROR(%5d):" fmt, getUserName(), __func__,__FILE__, __LINE__, ##arg)
#else
#define MY_LOGD(fmt, arg...)    printf("[D][%s][%s]" fmt"\n", getUserName(), __func__, ##arg)
#define MY_LOGI(fmt, arg...)    printf("[I][%s][%s]" fmt"\n", getUserName(), __func__, ##arg)
#define MY_LOGW(fmt, arg...)    printf("[W][%s][%s] WRN(%5d):" fmt"\n", getUserName(), __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    printf("[E][%s][%s] %s ERROR(%5d):" fmt"\n", getUserName(), __func__,__FILE__, __LINE__, ##arg)
#endif

#define MY_LOGE_WITHOUT_NAME(fmt, arg...)    printf("[E][%s] %s ERROR(%5d):" fmt"\n", __func__,__FILE__, __LINE__, ##arg)

#define MY_LOGV_IF(cond, arg...)    if (cond) { MY_LOGV(arg); }
#define MY_LOGD_IF(cond, arg...)    if (cond) { MY_LOGD(arg); }
#define MY_LOGI_IF(cond, arg...)    if (cond) { MY_LOGI(arg); }
#define MY_LOGW_IF(cond, arg...)    if (cond) { MY_LOGW(arg); }
#define MY_LOGE_IF(cond, arg...)    if (cond) { MY_LOGE(arg); }
    
#include <mtkcam/utils/std/Log.h>    

/*******************************************************************************
* External Function
********************************************************************************/



/*******************************************************************************
* Enum Define
********************************************************************************/



/*******************************************************************************
* Structure Define
********************************************************************************/



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
BMDN_HAL* BMDN_HAL::instance = nullptr;
Mutex BMDN_HAL::mLock;

BMDN_HAL*
BMDN_HAL::getInstance()
{
    Mutex::Autolock lock(mLock);
    if (!instance)
        instance = new BMDN_HAL_IMP("BMDN_HAL singleton");
    return instance;
}

bool
BMDN_HAL::destroyInstance()
{
    Mutex::Autolock lock(mLock);
    if (instance){
        delete instance;
        instance = nullptr;
        return true;
    }else{
        MY_LOGE_WITHOUT_NAME("delete before creating instance!");
        return false;
    }
}

BMDN_HAL_IMP::BMDN_HAL_IMP(char const* szCallerName )
    : mName(szCallerName)
    , LOG_ENABLED(StereoSettingProvider::isLogEnabled(PERPERTY_BMDENOISE_NODE_LOG))
{
}

BMDN_HAL_IMP::~BMDN_HAL_IMP()
{
    _clearAffinityControl();
    _clearBWDNData();
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  bmdn_hal Public interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool
BMDN_HAL_IMP::BMDNHalInit()
{
    MY_LOGD("BMDNHalInit +");

    StereoArea area = StereoSizeProvider::getInstance()->getBufferSize(E_BM_PREPROCESS_W_1);
    // set configure param
    BMDN_HAL_CONFIGURE_DATA         configureData;
    configureData.initWidth         = area.contentSize().w;
    configureData.initHeight        = area.contentSize().h;
    configureData.initPaddedWidth   = area.contentSize().w;
    configureData.initPaddedHeight  = area.contentSize().h;

    if(!BMDNHalConfigure(configureData)){
        MY_LOGE("configure BMDNHal failed!");
        return false;
    }

    MY_LOGD("BMDNHalInit -");
    return true;
}

bool
BMDN_HAL_IMP::BMDNHalConfigure(BMDN_HAL_CONFIGURE_DATA& configureData)
{
    MY_LOGD("BMDNHalConfigure +");
    Mutex::Autolock lock(mLock);
    _halConfigure(configureData);
    MY_LOGD("BMDNHalConfigure -");
    return true;
}

bool
BMDN_HAL_IMP::BMDNHalRun(BMDN_HAL_IN_DATA& inData)
{
    Mutex::Autolock lock(mLock);

    inData.paddedWidth = inData.paddedWidth + ALGO_PADDING;
    inData.paddedHeight = inData.paddedHeight + ALGO_PADDING;

    if(    inData.width != mCurrentSrcImgSize.w
        || inData.height != mCurrentSrcImgSize.h
        || inData.paddedWidth != mCurrentSrcImgPaddedSize.w
        || inData.paddedHeight != mCurrentSrcImgPaddedSize.h)
    {
        MY_LOGD("alg size changed (%d,%d,%d,%d)=>(%d,%d,%d,%d), auto re-configure bwdn",
            inData.width, inData.height, inData.paddedWidth, inData.paddedHeight,
            mCurrentSrcImgSize.w, mCurrentSrcImgSize.h, mCurrentSrcImgPaddedSize.w, mCurrentSrcImgPaddedSize.h
        );

        _clearAffinityControl();
        _clearBWDNData();

        BMDN_HAL_CONFIGURE_DATA configureData;
        configureData.initWidth = inData.width;
        configureData.initHeight = inData.height;
        configureData.initPaddedWidth = inData.paddedWidth - ALGO_PADDING;
        configureData.initPaddedHeight =inData.paddedHeight - ALGO_PADDING;

        _halConfigure(configureData);
    }

    if(_checkParams(inData)){
        // do alg
        _doAlg(inData);
        return true;
    }else{
        MY_LOGE("check parameters failed!");
        return false;
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  bmdn_hal_imp Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool
BMDN_HAL_IMP::_doAlg(BMDN_HAL_IN_DATA& inData)
{
    MY_LOGD("do BMDN alg +");

    BWDN_PARAMS input_params;
    BWDNBuffers input_buffers;

    input_params.width = mCurrentSrcImgSize.w;
    input_params.height = mCurrentSrcImgSize.h;

    input_params.isRotate = inData.isRotate;
    input_params.bayerOrder = inData.bayerOrder;
    input_params.RA = inData.RA;

    input_params.dwidth = inData.dwidth;
    input_params.dheight = inData.dheight;
    input_params.dPitch = inData.dPitch;
    input_params.dsH = inData.dsH;
    input_params.dsV = inData.dsV;

    input_params.FPREPROC = inData.FPREPROC;
    input_params.FSSTEP = inData.FSSTEP;

    input_params.OffsetX = inData.OffsetX;
    input_params.OffsetY = inData.OffsetY;

    //ISO dependent params
    input_params.BW_SingleRange = inData.BW_SingleRange;
    input_params.BW_OccRange = inData.BW_OccRange;
    input_params.BW_Range = inData.BW_Range;
    input_params.BW_Kernel = inData.BW_Kernel;
    input_params.B_Range = inData.B_Range;
    input_params.B_Kernel = inData.B_Kernel;
    input_params.W_Range = inData.W_Range;
    input_params.W_Kernel = inData.W_Kernel;

    for(int i=0 ; i < OB_OFFSET_SIZE ; i++){
        input_params.OBOffsetBayer[i] = inData.OBOffsetBayer[i];
        input_params.OBOffsetMono[i] = inData.OBOffsetMono[i];
    }

    input_params.sensorGainBayer = inData.sensorGainBayer;
    input_params.sensorGainMono = inData.sensorGainMono;
    input_params.ispGainBayer = inData.ispGainBayer;
    input_params.ispGainMono = inData.ispGainMono;

    for(int i=0 ; i < PRE_GAIN_SIZE ; i ++){
        input_params.preGainBayer[i] = inData.preGainBayer[i];
        input_params.preGainMono[i] = inData.preGainMono[i];
    }

    input_params.VSL = inData.VSL;
    input_params.VOFT = inData.VOFT;
    input_params.VGAIN = inData.VGAIN;

    input_params.dPadding[0] = inData.dPadding[0];
    input_params.dPadding[1] = inData.dPadding[1];

    for(int i=0 ; i < WARPING_MATRIX_SIZE ; i++){
        input_params.Trans[i] = inData.Trans[i];
    }

    input_buffers.width = mCurrentSrcImgSize.w;
    input_buffers.height = mCurrentSrcImgSize.h;

    input_buffers.MonoProcessedRaw = inData.MonoProcessedRaw;
    input_buffers.BayerProcessedRaw = inData.BayerProcessedRaw;
    input_buffers.BayerW = inData.BayerW;
    input_buffers.output = inData.output;
    input_buffers.depth = inData.depthL;
    input_buffers.BayerGain = inData.BayerGain; // shading gain
    input_buffers.MonoGain = inData.MonoGain;   // shading gain

    /* each frame needed */
    bwdn->BWDNFeatureCtrl(BWDN_FEATURE_SET_PARAMS, &input_params, NULL);
    /* each frame needed */
    bwdn->BWDNFeatureCtrl(BWDN_FEATURE_SET_IMAGES, &input_buffers, NULL);
    /* each frame needed */
    bwdn->BWDNMain();

    MY_LOGD("do BMDN alg -");
    return true;
}

bool
BMDN_HAL_IMP::_halConfigure(BMDN_HAL_CONFIGURE_DATA& configureData)
{
    mCurrentSrcImgSize.w = configureData.initWidth;
    mCurrentSrcImgSize.h = configureData.initHeight;
    mCurrentSrcImgPaddedSize.w = configureData.initPaddedWidth + ALGO_PADDING;
    mCurrentSrcImgPaddedSize.h = configureData.initPaddedHeight + ALGO_PADDING;

    const strSyncAEInitInfo* pSyncAEInitInfo = getSyncAEInitInfo();

    float convMat[3];
    convMat[0] = (static_cast<float>(pSyncAEInitInfo->RGB2YCoef_main[0]))/10000.0;
    convMat[1] = (static_cast<float>(pSyncAEInitInfo->RGB2YCoef_main[1]))/10000.0;
    convMat[2] = (static_cast<float>(pSyncAEInitInfo->RGB2YCoef_main[2]))/10000.0;

    // Prepare init params
    int WIDTH = mCurrentSrcImgSize.w;
    int HEIGHT = mCurrentSrcImgSize.h;
    int padWidth = mCurrentSrcImgPaddedSize.w;
    int padHeight = mCurrentSrcImgPaddedSize.h;

    std::vector<int> cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoBMDnoise, ENUM_STEREO_CAM_SCENARIO::eStereoCamCapture);
    int alg_threads = 0;
    for (auto & e : cpu_core) {
        alg_threads = alg_threads + e;
    }

    if(StereoSettingProvider::getModuleRotation() == eRotate_90 || StereoSettingProvider::getModuleRotation() == eRotate_270){
        mCurrentInit_params.splitSize = 336;
    }else{
        mCurrentInit_params.splitSize = 256;
    }

    mCurrentInit_params.threads = alg_threads;

    // sensor dependent
    mCurrentInit_params.convMat[0] = convMat[0];
    mCurrentInit_params.convMat[1] = convMat[1];
    mCurrentInit_params.convMat[2] = convMat[2];

    // shading gain size
    int gainWidth = SHADING_GAIN_WIDTH;
    int gainHeight = SHADING_GAIN_HEIGHT;

    // cltk part, must re-allocate when resolution changes
    mCurrentInit_params.ctx = cltk_bwdn_init();

    mCurrentInit_params.s_Buffer = cltk_image_alloc(mCurrentInit_params.ctx, WIDTH, HEIGHT, CLTK_UNORM_INT16, NULL);
    mCurrentInit_params.f_BufferDown = cltk_image_alloc(mCurrentInit_params.ctx, padWidth/16, padHeight/16, CLTK_FLOAT, NULL);
    mCurrentInit_params.us_Buffer1 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned short), NULL);
    mCurrentInit_params.us_Buffer2 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned short), NULL);
    mCurrentInit_params.us_Buffer3 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned short), NULL);

    mCurrentInit_params.f_BufferShad1 = cltk_image_alloc(mCurrentInit_params.ctx, gainWidth + 1, gainHeight + 1, CLTK_FLOAT, NULL);
    mCurrentInit_params.f_BufferShad2 = cltk_image_alloc(mCurrentInit_params.ctx, gainWidth + 1, gainHeight + 1, CLTK_FLOAT, NULL);
    mCurrentInit_params.f_BufferShad3 = cltk_image_alloc(mCurrentInit_params.ctx, gainWidth + 1, gainHeight + 1, CLTK_FLOAT, NULL);
    mCurrentInit_params.f_BufferShad4 = cltk_image_alloc(mCurrentInit_params.ctx, gainWidth + 1, gainHeight + 1, CLTK_FLOAT, NULL);
    mCurrentInit_params.us_Buffer4 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned short), NULL);

    mCurrentInit_params.us_Buffer5 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned short), NULL);
    mCurrentInit_params.us_Buffer6 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned short), NULL);
    mCurrentInit_params.uc_Buffer = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(unsigned char), NULL);
    mCurrentInit_params.f_Buffer1 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(float), NULL);
    mCurrentInit_params.f_Buffer2 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(float), NULL);

    mCurrentInit_params.f_Buffer3 = cltk_image_alloc(mCurrentInit_params.ctx, padWidth, padHeight, sizeof(float), NULL);
    mCurrentInit_params.f_BufferTrans = cltk_image_alloc(mCurrentInit_params.ctx, 9, 1, sizeof(float), NULL);

    MY_LOGD("srcSize: (%dx%d), srcPaddedSize: (%dx%d), shadingGainSize: (%dx%d), B2W:[%d,%d,%d]->[%f,%f,%f], threads:%d, splitSize:%d",
        mCurrentSrcImgSize.w,
        mCurrentSrcImgSize.h,
        mCurrentSrcImgPaddedSize.w,
        mCurrentSrcImgPaddedSize.h,
        SHADING_GAIN_WIDTH,
        SHADING_GAIN_HEIGHT,
        pSyncAEInitInfo->RGB2YCoef_main[0],
        pSyncAEInitInfo->RGB2YCoef_main[1],
        pSyncAEInitInfo->RGB2YCoef_main[2],
        mCurrentInit_params.convMat[0],
        mCurrentInit_params.convMat[1],
        mCurrentInit_params.convMat[2],
        mCurrentInit_params.threads,
        mCurrentInit_params.splitSize
    );

    // Affinity control
    _affinityControl();

    // init BWDN
    bwdn = MTKBWDN::createInstance(DRV_BWDN_OBJ_SW);
    bwdn->BWDNInit(&mCurrentInit_params, NULL);

    worksize = 0;
    bwdn->BWDNFeatureCtrl(BWDN_FEATURE_GET_WORKBUF_SIZE, &worksize, NULL);
    if(worksize){
        workbuf = new char[worksize];
        bwdn->BWDNFeatureCtrl(BWDN_FEATURE_SET_WORKBUF_ADDR, workbuf, NULL);
    }

    return true;
}

bool
BMDN_HAL_IMP::_checkParams(BMDN_HAL_IN_DATA& inData)
{
    MY_LOGD("isRotate [%d], bayerOrder [%d]", inData.isRotate, inData.bayerOrder);

    MY_LOGD("RA [%d]", inData.RA);

    MY_LOGD("OBOffsetBayer [%d,%d,%d,%d]",
        inData.OBOffsetBayer[0],
        inData.OBOffsetBayer[1],
        inData.OBOffsetBayer[2],
        inData.OBOffsetBayer[3]
    );

    MY_LOGD("OBOffsetMono [%d,%d,%d,%d]",
        inData.OBOffsetMono[0],
        inData.OBOffsetMono[1],
        inData.OBOffsetMono[2],
        inData.OBOffsetMono[3]
    );

    MY_LOGD("sensorGainBayer [%d]", inData.sensorGainBayer);
    MY_LOGD("sensorGainMono [%d]", inData.sensorGainMono);
    MY_LOGD("ispGainBayer [%d]", inData.ispGainBayer);
    MY_LOGD("ispGainMono [%d]", inData.ispGainMono);

    MY_LOGD("preGainBayer [%d,%d,%d]",
        inData.preGainBayer[0],
        inData.preGainBayer[1],
        inData.preGainBayer[2]
    );

    MY_LOGD("preGainMono [%d,%d,%d]",
        inData.preGainMono[0],
        inData.preGainMono[1],
        inData.preGainMono[2]
    );

    MY_LOGD("OffsetX:%d OffsetY:%d",
        inData.OffsetX,
        inData.OffsetY
    );

    MY_LOGD("BW_SingleRange [%d]", inData.BW_SingleRange);
    MY_LOGD("BW_OccRange [%d]", inData.BW_OccRange);
    MY_LOGD("BW_Range [%d]", inData.BW_Range);
    MY_LOGD("BW_Kernel [%d]", inData.BW_Kernel);
    MY_LOGD("B_Range [%d]", inData.B_Range);
    MY_LOGD("B_Kernel [%d]", inData.B_Kernel);
    MY_LOGD("W_Range [%d]", inData.W_Range);
    MY_LOGD("W_Kernel [%d]", inData.W_Kernel);

    MY_LOGD("VSL [%d]", inData.VSL);
    MY_LOGD("VOFT [%d]", inData.VOFT);
    MY_LOGD("VGAIN [%d]", inData.VGAIN);

    MY_LOGD("Trans [%f,%f,%f,%f,%f,%f,%f,%f,%f]",
        inData.Trans[0],
        inData.Trans[1],
        inData.Trans[2],
        inData.Trans[3],
        inData.Trans[4],
        inData.Trans[5],
        inData.Trans[6],
        inData.Trans[7],
        inData.Trans[8]
    );

    MY_LOGD("dPadding [%d,%d]",
        inData.dPadding[0],
        inData.dPadding[1]
    );

    MY_LOGD("unpadded width x height [%d,%d]",
        inData.width,
        inData.height
    );

    MY_LOGD("padded width x height [%d,%d]",
        inData.paddedWidth,
        inData.paddedHeight
    );

    MY_LOGD("MonoProcessedRaw [0x%x]", inData.MonoProcessedRaw);
    if(inData.MonoProcessedRaw == nullptr){
        MY_LOGE("MonoProcessedRaw is nullptr!");
        return false;
    }

    MY_LOGD("BayerProcessedRaw [0x%x]", inData.BayerProcessedRaw);
    MY_LOGD("BayerW [0x%x]", inData.BayerW);

    MY_LOGD("depthSize [%dx%d] pitch:%d", inData.dwidth, inData.dheight, inData.dPitch);
    MY_LOGD("depthL [0x%x]", inData.depthL);
    MY_LOGD("depthR [0x%x]", inData.depthR);
    MY_LOGD("depthHV [%dx%d]", inData.dsH, inData.dsV);

    MY_LOGD("FPREPROC [%d], FSSTEP [%d]", inData.FPREPROC, inData.FSSTEP);

    MY_LOGD("BayerGain [0x%x]", inData.BayerGain);
    MY_LOGD("MonoGain [0x%x]", inData.MonoGain);

    MY_LOGD("output [0x%x]", inData.output);

    MY_LOGD("shadingGain Bayer: %f %f %f %f %f",
        inData.BayerGain[0],
        inData.BayerGain[1],
        inData.BayerGain[2],
        inData.BayerGain[3],
        inData.BayerGain[4]
    );

    MY_LOGD("shadingGain Mono: %f %f %f %f %f",
        inData.MonoGain[0],
        inData.MonoGain[1],
        inData.MonoGain[2],
        inData.MonoGain[3],
        inData.MonoGain[4]
    );

    return true;
}

bool
BMDN_HAL_IMP::_clearBWDNData(){

    cltk_image_release(mCurrentInit_params.s_Buffer);
    cltk_image_release(mCurrentInit_params.f_BufferDown);
    cltk_image_release(mCurrentInit_params.us_Buffer1);
    cltk_image_release(mCurrentInit_params.us_Buffer2);
    cltk_image_release(mCurrentInit_params.us_Buffer3);

    cltk_image_release(mCurrentInit_params.f_BufferShad1);
    cltk_image_release(mCurrentInit_params.f_BufferShad2);
    cltk_image_release(mCurrentInit_params.f_BufferShad3);
    cltk_image_release(mCurrentInit_params.f_BufferShad4);
    cltk_image_release(mCurrentInit_params.us_Buffer4);

    cltk_image_release(mCurrentInit_params.us_Buffer5);
    cltk_image_release(mCurrentInit_params.us_Buffer6);
    cltk_image_release(mCurrentInit_params.uc_Buffer);
    cltk_image_release(mCurrentInit_params.f_Buffer1);
    cltk_image_release(mCurrentInit_params.f_Buffer2);

    cltk_image_release(mCurrentInit_params.f_Buffer3);
    cltk_image_release(mCurrentInit_params.f_BufferTrans);

    cltk_end(mCurrentInit_params.ctx);

    bwdn->destroyInstance(bwdn);

    if(worksize)
        delete [] workbuf;

    return true;
}

bool
BMDN_HAL_IMP::_affinityControl(){
    if(mCPUAffinity == nullptr){
        mCPUAffinity = new CPUAffinity();
    }else{
        mCPUAffinity->disable();
    }

    std::vector<int> cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoBMDnoise, ENUM_STEREO_CAM_SCENARIO::eStereoCamCapture);
    MINT32 value;

    value = property_get_int32("debug.bmdenoise.ALG.coreLL", -1);
    if(value != -1){
        cpu_core[0] = value;
    }
    value = property_get_int32("debug.bmdenoise.ALG.coreL", -1);
    if(value != -1){
        cpu_core[1] = value;
    }
    value = property_get_int32("debug.bmdenoise.ALG.coreB", -1);
    if(value != -1){
        cpu_core[2] = value;
    }

    MY_LOGD("enable CPUAffinity (%d/%d/%d)", cpu_core[0], cpu_core[1], cpu_core[2]);

    CPUMask cpuMask;
    cpuMask.setCore(CPUCoreLL, cpu_core[0]);
    cpuMask.setCore(CPUCoreL,  cpu_core[1]);
    cpuMask.setCore(CPUCoreB,  cpu_core[2]);

    mCPUAffinity->enable(cpuMask);
    return true;
}

bool
BMDN_HAL_IMP::_clearAffinityControl(){
    if(mCPUAffinity != nullptr){
        MY_LOGD("disable and clear CPUAffinity");
        mCPUAffinity->disable();
        delete mCPUAffinity;
    }
    return true;
}
