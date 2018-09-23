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

#define LOG_TAG "MtkCam/FeatureFlowControl"
//
#include "MyUtils.h"
//
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>

#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
#include <mtkcam/middleware/v1/LegacyPipeline/NodeId.h>
#include <mtkcam/middleware/v1/IParamsManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/ILegacyPipeline.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineBuilder.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProviderFactory.h>
#include "FeatureFlowControl.h"
#include <buffer/ClientBufferPool.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/BufferPoolImp.h>

#warning "FIXME"
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/Selector.h>

#ifdef FEATURE_MODIFY
#include <mtkcam/feature/eis/eis_ext.h>
#include <camera_custom_eis.h>
#endif // FEATURE_MODIFY

#include <math.h>
#include <camera_custom_isp_limitation.h>

#include <mtkcam/utils/hw/CamManager.h>
using namespace NSCam::Utils;

using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace android;
using namespace NSCam::v3;
using namespace NSCamHW;

/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
//
#define MY_LOGD1(...)               MY_LOGD_IF((mLogLevel>=1),__VA_ARGS__)
#define MY_LOGD2(...)               MY_LOGD_IF((mLogLevel>=2),__VA_ARGS__)
#define MY_LOGD3(...)               MY_LOGD_IF((mLogLevel>=3),__VA_ARGS__)
//
#define FUNC_START                  MY_LOGD1("+")
#define FUNC_END                    MY_LOGD1("-")
//
#define TEST_RAW_ALLOCATE (0)
#define SUPPORT_VSS (1)
//
/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
decideSensorModeByVHDR(
    HwInfoHelper&   helper,
    MUINT&          sensorMode,
    MUINT32 const   vhdrMode
)
{
    MUINT32 supportHDRMode = 0;
    if (! helper.querySupportVHDRMode(sensorMode, supportHDRMode))
        return BAD_VALUE;
    if(vhdrMode == supportHDRMode)
        return OK;

    MUINT origSensorMode = sensorMode;
    // Find acceptable sensor mode for this vhdrMode
    // 1. Try use VIDEO mode
    if ( sensorMode == SENSOR_SCENARIO_ID_NORMAL_PREVIEW )
    {
        MY_LOGI("VHDR Mode(%d) is not support in sensor preview mode, try video mode", vhdrMode);
        sensorMode = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
        if ( ! helper.querySupportVHDRMode(sensorMode, supportHDRMode) )
        {
            sensorMode = origSensorMode;
            return BAD_VALUE;
        }
        if (vhdrMode == supportHDRMode)
            return OK;
    }

    // 2. VIDEO & PREVIEW mode are all not acceptable -> use capture mode directly
    MY_LOGW("Directly use sensor capture mode to do VHDR ");
    sensorMode = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
decideRrzoImage(
    HwInfoHelper& helper,
    MUINT32 const bitDepth,
    MSize         referenceSize,
    MUINT const   usage,
    MINT32 const  minBuffer,
    MINT32 const  maxBuffer,
    sp<IImageStreamInfo>& rpImageStreamInfo
)
{
    MSize autualSize;
    size_t stride;
    MINT format;
    if( ! helper.getRrzoFmt(bitDepth, format) ||
        ! helper.alignRrzoHwLimitation(referenceSize, mSensorParam.size, autualSize) ||
        ! helper.alignPass1HwLimitation(mSensorParam.pixelMode, format, false, autualSize, stride) )
    {
        MY_LOGE("wrong params about rrzo");
        return BAD_VALUE;
    }
    //
    MY_LOGI("rrzo num:%d-%d bitDepth:%d format:%d referenceSize:%dx%d, actual size:%dx%d, stride:%d",
                minBuffer,
                maxBuffer,
                bitDepth,
                format,
                referenceSize.w, referenceSize.h,
                autualSize.w, autualSize.h,
                stride
            );
    rpImageStreamInfo =
        createRawImageStreamInfo(
            "Hal:Image:Resiedraw",
            eSTREAMID_IMAGE_PIPE_RAW_RESIZER,
            eSTREAMTYPE_IMAGE_INOUT,
            maxBuffer, minBuffer,
            usage, format, autualSize, stride
            );

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
decideImgoImage(
    HwInfoHelper& helper,
    MUINT32 const bitDepth,
    MSize         referenceSize,
    MUINT  const  usage,
    MINT32 const  minBuffer,
    MINT32 const  maxBuffer,
    sp<IImageStreamInfo>& rpImageStreamInfo
)
{
    MSize autualSize = referenceSize;
    size_t stride;
    MINT format;
    if( ! helper.getImgoFmt(bitDepth, format) ||
        ! helper.alignPass1HwLimitation(mSensorParam.pixelMode, format, true, autualSize, stride) )
    {
        MY_LOGE("wrong params about imgo");
        return BAD_VALUE;
    }
    //
    MY_LOGD("imgo num:%d-%d bitDepth:%d format:%d referenceSize:%dx%d, actual size:%dx%d, stride:%d",
                minBuffer,
                maxBuffer,
                bitDepth,
                format,
                referenceSize.w, referenceSize.h,
                autualSize.w, autualSize.h,
                stride
            );
    rpImageStreamInfo =
        createRawImageStreamInfo(
            "Hal:Image:Fullraw",
            eSTREAMID_IMAGE_PIPE_RAW_OPAQUE,
            eSTREAMTYPE_IMAGE_INOUT,
            maxBuffer, minBuffer,
            usage, format, autualSize, stride
            );

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
setCamClient(
    const char* name,
    StreamId streamId,
    Vector<PipelineImageParam>& vImageParam,
    Vector<MUINT32> clientMode,
    MUINT usage,
    MBOOL useTransform
)
{
    MSize const& size = MSize(-1,-1);
    MINT const format = eImgFmt_YUY2;
    size_t const stride = 1280;
    sp<IImageStreamInfo> pImage_Yuv =
        createImageStreamInfo(
            name,
            streamId,
            eSTREAMTYPE_IMAGE_INOUT,
            5, 1,
            usage, format, size, 0
            );
    sp<ClientBufferPool> pClient = new ClientBufferPool(getOpenId(), useTransform);
    pClient->setCamClient(
                        name,
                        mpImgBufProvidersMgr,
                        clientMode[0]
                    );
    for ( size_t i = 1; i < clientMode.size(); ++i ) {
        pClient->setCamClient( clientMode[i] );
    }

    sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
    pFactory->setImageStreamInfo(pImage_Yuv);
    pFactory->setUsersPool(pClient);

    vImageParam.push_back(
        PipelineImageParam{
            pImage_Yuv,
            pFactory->create(),
            0
        }
    );

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
constructNormalPreviewPipeline()
{
    FUNC_START;

    CAM_TRACE_NAME("DFC:constructFeaturePreviewPipeline");

    mLPBConfigParams.mode = LegacyPipelineMode_T::PipelineMode_Feature_Preview;

    HwInfoHelper helper(mOpenId);
    if( ! helper.updateInfos() ) {
        MY_LOGE("cannot properly update infos");
        return BAD_VALUE;
    }
    //
    mSensorParam.mode = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;

#ifdef FEATURE_MODIFY
    MUINT32 vhdrMode = mpParamsManagerV3->getParamsMgr()->getVHdr();
    if(vhdrMode != SENSOR_VHDR_MODE_NONE
        && decideSensorModeByVHDR(helper, mSensorParam.mode, vhdrMode) != OK) {
        MY_LOGE("cannot get vhdr supported sensor mode.");
        return BAD_VALUE;
    }
#endif // FEATURE_MODIFY

    if( ! helper.getSensorSize( mSensorParam.mode, mSensorParam.size) ||
        ! helper.getSensorFps( (MUINT32)mSensorParam.mode, (MINT32&)mSensorParam.fps) ||
        ! helper.queryPixelMode( mSensorParam.mode, mSensorParam.fps, mSensorParam.pixelMode)
      ) {
        MY_LOGE("cannot get params about sensor");
        return BAD_VALUE;
    }

    if (helper.getDualPDAFSupported(mSensorParam.mode))
    {
        mLPBConfigParams.enableDualPD = MTRUE;
    }

#ifdef FEATURE_MODIFY
    // Sensor
    mSensorParam.vhdrMode = mpParamsManagerV3->getParamsMgr()->getVHdr();
    MY_LOGD("VHDR sensor mode:%d, rawType:%d, size:%dx%d, fps:%d pixel:%d vhdrMode:%d",
        mSensorParam.mode,
        mSensorParam.rawType,
        mSensorParam.size.w, mSensorParam.size.h,
        mSensorParam.fps,
        mSensorParam.pixelMode,
        mSensorParam.vhdrMode);
#else
    // Sensor
    MY_LOGD("sensor mode:%d, rawType:%d, size:%dx%d, fps:%d pixel:%d",
        mSensorParam.mode,
        mSensorParam.rawType,
        mSensorParam.size.w, mSensorParam.size.h,
        mSensorParam.fps,
        mSensorParam.pixelMode);
#endif // FEATURE_MODIFY
    //
    sp<LegacyPipelineBuilder> pBuilder =
        LegacyPipelineBuilder::createInstance(
                                    mOpenId,
                                    "FeaturePreview",
                                    mLPBConfigParams);
    if ( pBuilder == 0 ) {
        MY_LOGE("Cannot create LegacyPipelineBuilder.");
        return BAD_VALUE;
    }
    //
    pBuilder->setSrc(mSensorParam);
    //
    MUINT featureCFG = 0;
    if (mLPBConfigParams.enableDualPD)
    {
        FEATURE_CFG_ENABLE_MASK(featureCFG,IScenarioControl::FEATURE_DUAL_PD);
    }
    //
    sp<IScenarioControl> pScenarioCtrl = enterScenarioControl(getScenario(), mSensorParam.size, mSensorParam.fps, featureCFG);
    if( pScenarioCtrl.get() == NULL )
    {
        MY_LOGE("get Scenario Control fail");
        return BAD_VALUE;
    }
    pBuilder->setScenarioControl(pScenarioCtrl);
    //
    sp<IImageStreamInfo> pImage_FullRaw;
    sp<PairMetadata>  pFullPair;
    sp<BufferPoolImp> pFullRawPool;
    sp<IImageStreamInfo> pImage_ResizedRaw;
    sp<PairMetadata>  pResizedPair;
    sp<BufferPoolImp> pResizedRawPool;
    // Image
    {
        CAM_TRACE_NAME("DFC:SetImageDst");
        Vector<PipelineImageParam> vImageParam;
        // RAW (RRZO)
        {
            MUINT32 const bitDepth = getPreviewRawBitDepth();
            MSize previewsize;
            MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE; //not necessary here
            mpParamsManagerV3->getParamsMgr()->getPreviewSize(&previewsize.w, &previewsize.h);

            if ( OK != decideRrzoImage(
                            helper, bitDepth,
                            previewsize, usage,
                            2, 8,
                            pImage_ResizedRaw
                        ))
            {
                MY_LOGE("No rrzo image");
                return BAD_VALUE;
            }

            if(mNeedDumpRRZO)
            {
                MY_LOGD("Open RRZO for dump mNeedDumpRRZO(%d)",mNeedDumpRRZO);
                pResizedPair = PairMetadata::createInstance(pImage_ResizedRaw->getStreamName());
                pResizedRawPool = new BufferPoolImp(pImage_ResizedRaw);
                //
                sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                pFactory->setImageStreamInfo(pImage_ResizedRaw);
                pFactory->setPairRule(pResizedPair, 1);
                pFactory->setUsersPool(pResizedRawPool);
                sp<StreamBufferProvider> pProducer = pFactory->create();
                //
                mpResourceContainer->setConsumer( pImage_ResizedRaw->getStreamId(), pProducer );
                //
                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_ResizedRaw,
                        pProducer,
                        0
                    }
                );
            }
            else
            {
                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_ResizedRaw,
                        NULL,
                        0
                    }
                );
            }
            // ===== For Dump IMGO raw  ============
            if(mForceEnableIMGO || mNeedDumpIMGO){

                MY_LOGD("Open IMGO for dump mForceEnableIMGO(%d) mNeedDumpIMGO(%d)",mForceEnableIMGO,mNeedDumpIMGO);

                if ( OK != decideImgoImage(
                            helper, bitDepth,
                            mSensorParam.size, usage,
                            4, 8,
                            pImage_FullRaw
                            ))
                {
                    MY_LOGE("No imgo image");
                    return BAD_VALUE;
                }

                if(mNeedDumpIMGO)
                {
                    pFullPair = PairMetadata::createInstance(pImage_FullRaw->getStreamName());
                    pFullRawPool = new BufferPoolImp(pImage_FullRaw);
                    //
                    sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                    pFactory->setImageStreamInfo(pImage_FullRaw);
                    pFactory->setPairRule(pFullPair, 1);
                    pFactory->setUsersPool(pFullRawPool);
                    sp<StreamBufferProvider> pProducer = pFactory->create();
                    //
                    mpResourceContainer->setConsumer( pImage_FullRaw->getStreamId(), pProducer );
                    //
                    vImageParam.push_back(
                            PipelineImageParam{
                            pImage_FullRaw,
                            pProducer,
                            0
                            }
                            );
                }
                else
                {
                    vImageParam.push_back(
                            PipelineImageParam{
                            pImage_FullRaw,
                            NULL,
                            0
                            }
                            );
                }
            }
        }
        // ===============================
        // RAW (LCSO with pure pool)
        if (mLPBConfigParams.enableLCS)
        {
            MUINT32 const bitDepth = 10; // no use
            MSize anySize; // no use
            MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE; //not necessary here
            sp<IImageStreamInfo> pImage_Raw;

            if ( OK != decideLcsoImage(
                            helper, bitDepth,
                            anySize, usage,
                            2, 8,
                            pImage_Raw
                        ))
            {
                MY_LOGE("No lcso image");
                return BAD_VALUE;
            }

            vImageParam.push_back(
                PipelineImageParam{
                    pImage_Raw,
                    NULL,
                    0
                }
            );
        }
        // YUV preview -> display client
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_DISPLAY);

            setCamClient(
                "Hal:Image:yuvDisp",
                eSTREAMID_IMAGE_PIPE_YUV_00,
                vImageParam,
                clientMode,
                GRALLOC_USAGE_HW_COMPOSER,
                MTRUE
            );
        }
        // YUV preview callback -> preview callback client
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_PRV_CB);
            clientMode.push_back(IImgBufProvider::eID_GENERIC);

            setCamClient(
                "Hal:Image:yuvPrvCB",
                eSTREAMID_IMAGE_PIPE_YUV_01,
                vImageParam,
                clientMode
            );
        }
        // YUV FD & OT
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_FD);
            clientMode.push_back(IImgBufProvider::eID_OT);

            setCamClient(
                "Hal:Image:yuvFD",
                eSTREAMID_IMAGE_YUV_FD,
                vImageParam,
                clientMode
            );
        }

        pBuilder->setDst(vImageParam);
    }

    mpPipeline = pBuilder->create();

    if ( mpPipeline == 0) {
        MY_LOGE("Fail to create Legacy Pipeline.");
        return BAD_VALUE;
    }
    //
    sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
    sp<FrameInfo> pFrameInfo = new FrameInfo();
    mpResourceContainer->setLatestFrameInfo(pFrameInfo);
    pProcessor->registerListener(
                    eSTREAMID_META_APP_DYNAMIC_P1,
                    pFrameInfo
                    );
    //
    if ( mNeedDumpIMGO )
    {
        pFullRawPool->allocateBuffer(
        pImage_FullRaw->getStreamName(),
        pImage_FullRaw->getMaxBufNum(),
        pImage_FullRaw->getMinInitBufNum()
                );

        sp<StreamBufferProvider> pTempConsumer =
            mpResourceContainer->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );

        if ( pTempConsumer != 0 ) {
            sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
            pProcessor->registerListener(
                            eSTREAMID_META_HAL_DYNAMIC_P2,
                            pFullPair
                            );
            // Need set Selector
            sp<DumpBufferSelector> pSelector = new DumpBufferSelector();
            pSelector->setDumpConfig( mOpenId );
            pTempConsumer->setSelector(pSelector);
            //
        }
    }
    //
    if ( mNeedDumpRRZO )
    {
        pResizedRawPool->allocateBuffer(
        pImage_ResizedRaw->getStreamName(),
        pImage_ResizedRaw->getMaxBufNum(),
        pImage_ResizedRaw->getMinInitBufNum()
                );

        sp<StreamBufferProvider> pTempConsumer =
            mpResourceContainer->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_RESIZER );

        if ( pTempConsumer != 0 ) {
            sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
            pProcessor->registerListener(
                            eSTREAMID_META_HAL_DYNAMIC_P2,
                            pResizedPair
                            );
            // Need set Selector
            sp<DumpBufferSelector> pSelector = new DumpBufferSelector();
            pSelector->setDumpConfig( mOpenId );
            pTempConsumer->setSelector(pSelector);
            //
        }
    }
    //
    FUNC_END;

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
constructRecordingPipeline()
{
    FUNC_START;
    MBOOL bHighSpeedMode = MFALSE;
    MUINT previewMaxFps = 0;
    HwInfoHelper helper(mOpenId);

    #ifdef FEATURE_MODIFY
    MBOOL bAdvEISEnabled = MFALSE;
    #endif // FEATURE_MODIFY

    CAM_TRACE_NAME("DFC:constructRecordingPipeline");
    //
    if( ! helper.updateInfos() ) {
        MY_LOGE("cannot properly update infos");
        return BAD_VALUE;
    }
    //
    if(mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) > HighSpeedVideoFpsBase)
    {
        selectHighSpeedSensorScen(
            mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE),
            mSensorParam.mode);
    }
    else
    {
        mSensorParam.mode = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
    }
    //
    if (helper.getDualPDAFSupported(mSensorParam.mode))
    {
        mLPBConfigParams.enableDualPD = MTRUE;
    }
    //
    if(!helper.getSensorFps((MUINT32)mSensorParam.mode, (MINT32&)mSensorParam.fps))
    {
        MY_LOGE("cannot get params about sensor");
        return BAD_VALUE;
    }
    //
    previewMaxFps =     (mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) <= mSensorParam.fps) ?
                        mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) : mSensorParam.fps;
    mpParamsManagerV3->setPreviewMaxFps(previewMaxFps);
    if(previewMaxFps > HighSpeedVideoFpsBase)
    {
        bHighSpeedMode = MTRUE;
    }
    //
    MSize recordsize = MSize(0,0);
    if( decideSensorModeAndRrzo(recordsize, mSensorParam.mode, bHighSpeedMode) != OK )
    {
        return BAD_VALUE;
    }

#ifdef FEATURE_MODIFY
    MUINT32 vhdrMode = mpParamsManagerV3->getParamsMgr()->getVHdr();
    if(vhdrMode != SENSOR_VHDR_MODE_NONE
        && decideSensorModeByVHDR(helper, mSensorParam.mode, vhdrMode) != OK) {
        MY_LOGE("cannot get vhdr supported sensor mode.");
        return BAD_VALUE;
    }
#endif // FEATURE_MODIFY

    //
    if( ! helper.getSensorSize( mSensorParam.mode, mSensorParam.size) ||
        ! helper.getSensorFps( (MUINT32)mSensorParam.mode, (MINT32&)mSensorParam.fps) ||
        ! helper.queryPixelMode( mSensorParam.mode, mSensorParam.fps, mSensorParam.pixelMode)
      ) {
        MY_LOGE("cannot get params about sensor");
        return BAD_VALUE;
    }
    //
    MUINT32 eis_supFrms = 0;
    if(bHighSpeedMode)
    {
        mLPBConfigParams.mode = LegacyPipelineMode_T::PipelineMode_HighSpeedVideo;
        mLPBConfigParams.enableEIS = MFALSE;
        mLPBConfigParams.enableLCS = MFALSE;
        mpRequestThreadLoopCnt = previewMaxFps/HighSpeedVideoFpsBase;
    }
    else
    {
        mLPBConfigParams.mode = LegacyPipelineMode_T::PipelineMode_Feature_VideoRecord;

        MUINT32 eis_mode    = 0;

        #ifdef FEATURE_MODIFY
        if (mpParamsManagerV3->getParamsMgr()->getVideoStabilization())
        {
            MINT32 videoWidth = 0,videoHeight = 0;

            mpParamsManagerV3->getParamsMgr()->getVideoSize(&videoWidth,&videoHeight);

            if (EISCustom::isForcedEIS12())
            {
                EIS_MODE_ENABLE_EIS_12(mEisMode);
                MY_LOGD("Force run EIS 1.2.");
            }
            else if (((videoWidth*videoHeight) <= (VR_1080P_W*VR_1080P_H)) &&
                     (mpParamsManagerV3->getParamsMgr()->getVHdr() == SENSOR_VHDR_MODE_NONE))
            {
                //Enable Advanced EIS for FHD VR
                eis_mode    = EISCustom::getEIS25_FHD_Mode();
                eis_supFrms = EISCustom::getEIS25_FHD_Support_Frames();

                mEisCounter = 0;
                EIS_EANBLE_ADVANCED_EIS(mEisMode);
                bAdvEISEnabled = MTRUE;
                MY_LOGD("Run EIS 2.5 Fusion.");
            }
            else if (((videoWidth*videoHeight) <= (VR_1080P_W*VR_1080P_H)) &&
                     (mpParamsManagerV3->getParamsMgr()->getVHdr() != SENSOR_VHDR_MODE_NONE))
            {
                //Enable EIS 2.5 with vHDR VR
                if (EISCustom::isEnabledEIS25_ImageMode())
                {
                    eis_mode    = EISCustom::getEIS25_FHD_Mode();
                    eis_supFrms = EISCustom::getEIS25_FHD_Support_Frames();

                    mEisCounter = 0;
                    EIS_EANBLE_vHDR_EIS25(mEisMode);
                    EIS_MODE_ENABLE_EIS_25_IMAGE_ONLY(mEisMode);
                    bAdvEISEnabled = MTRUE;
                    MY_LOGD("Run EIS 2.5 Image-based with vHDR");
                }
                else
                {
                    EIS_MODE_ENABLE_EIS_12(mEisMode);
                    MY_LOGD("EIS 2.5 only supply Image-based with vHDR! Run EIS 1.2.");
                }
            }
            else if (((videoWidth*videoHeight) >= (VR_UHD_W*VR_UHD_H)) &&
                     (mpParamsManagerV3->getParamsMgr()->getVHdr() == SENSOR_VHDR_MODE_NONE))
            {
                //SET 4K FLAG
                //Enable EIS 2.5 for 4K VR without vHDR
                mEisCounter = 0;
                if (EISCustom::isEnabledEIS25_4k2k())
                {
                    eis_mode    = EISCustom::getEIS25_4k2k_Mode();
                    eis_supFrms = EISCustom::getEIS25_4k2k_Support_Frames();

                    EIS_MODE_ENABLE_EIS_25(mEisMode);
                    bAdvEISEnabled = MTRUE;
                    MY_LOGD("Run EIS 2.5 Gyro-based with 4k2k.");
                }
                else
                {
                    EIS_MODE_ENABLE_EIS_12(mEisMode);
                    MY_LOGD("EIS 2.5 do not support 4k2k! Run EIS 1.2.");
                }
            }
            else
            {
                //Fallthrough
                EIS_MODE_ENABLE_EIS_12(mEisMode);
                MY_LOGE("Unknown EIS state! Run EIS 1.2.");
            }
            MY_LOGD("start recording WxH (%dx%d), eisMode %d",videoWidth,videoHeight,mEisMode);
        }
        else
        {
            //DoNothing
            mEisMode = EIS_MODE_OFF;
        }

        mpParamsManagerV3->getParamsMgr()->set(MtkCameraParameters::KEY_EIS25_MODE, eis_mode);
        mpParamsManagerV3->getParamsMgr()->set(MtkCameraParameters::KEY_EIS_SUPPORTED_FRAMES, eis_supFrms);

        mLPBConfigParams.eisMode = mEisMode;
        #endif // FEATURE_MODIFY
    }

    sp<LegacyPipelineBuilder> pBuilder =
        LegacyPipelineBuilder::createInstance(
                                    mOpenId,
                                    "FeatureRecord",
                                    mLPBConfigParams);
    if ( pBuilder == 0 ) {
        MY_LOGE("Cannot create LegacyPipelineBuilder.");
        return BAD_VALUE;
    }

    #ifdef FEATURE_MODIFY
    // Sensor
    mSensorParam.vhdrMode = mpParamsManagerV3->getParamsMgr()->getVHdr();
    MY_LOGD("sensor mode:%d, rawType:%d, size:%dx%d, fps:%d pixel:%d vhdrMode:%d",
        mSensorParam.mode,
        mSensorParam.rawType,
        mSensorParam.size.w, mSensorParam.size.h,
        mSensorParam.fps,
        mSensorParam.pixelMode,
        mSensorParam.vhdrMode);
    #else
    // Sensor
    MY_LOGD("sensor mode:%d, rawType:%d, size:%dx%d, fps:%d pixel:%d",
        mSensorParam.mode,
        mSensorParam.rawType,
        mSensorParam.size.w, mSensorParam.size.h,
        mSensorParam.fps,
        mSensorParam.pixelMode);
    #endif // FEATURE_MODIFY

    pBuilder->setSrc(mSensorParam);
    //

    #ifdef FEATURE_MODIFY
    MUINT featureCFG = 0;
    if (bAdvEISEnabled)
    {
        FEATURE_CFG_ENABLE_MASK(featureCFG,IScenarioControl::FEATURE_ADV_EIS);
    }
    if (mLPBConfigParams.enableDualPD)
    {
        FEATURE_CFG_ENABLE_MASK(featureCFG,IScenarioControl::FEATURE_DUAL_PD);
    }
    //
    MSize videoSize = MSize(0,0);
    mpParamsManagerV3->getParamsMgr()->getVideoSize(&videoSize.w,&videoSize.h);
    //
    sp<IScenarioControl> pScenarioCtrl = enterScenarioControl(getScenario(), mSensorParam.size, mSensorParam.fps, featureCFG, videoSize);
    #endif // FEATURE_MODIFY

    if( pScenarioCtrl.get() == NULL )
    {
        MY_LOGE("get Scenario Control fail");
        return BAD_VALUE;
    }
    pBuilder->setScenarioControl(pScenarioCtrl);

    // Image
    //
    sp<IImageStreamInfo> pImage_FullRaw;
    sp<PairMetadata>  pFullPair;
    sp<BufferPoolImp> pFullRawPool;
    sp<IImageStreamInfo> pImage_ResizedRaw;
    sp<PairMetadata>  pResizedPair;
    sp<BufferPoolImp> pResizedRawPool;
    sp<IImageStreamInfo> pImage_LcsoRaw;

    sp<VssSelector> pVssSelector = new VssSelector();
//	    sp<PairMetadata> pPair;
    {
        CAM_TRACE_NAME("DFC:SetImageDst");
        Vector<PipelineImageParam> vImageParam;
        // RAW
        {
            MUINT32 const bitDepth = getPreviewRawBitDepth();
            MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE; //not necessary here
//	            sp<IImageStreamInfo> pImage_Raw;

            int mrrzoSize_Max, mrrzoSize_Min;

            if(bHighSpeedMode)
            {
                #warning "TODO: Check how to decide RRZO buffer count for high speed mode"
                mrrzoSize_Max = mpRequestThreadLoopCnt * 8; // 6 for p1, 2 for p2 depth
                mrrzoSize_Min = mrrzoSize_Max/2;
            }
            else
            {
                mrrzoSize_Max = 8;
                mrrzoSize_Min = 4;
            }
            // for debug use for modify RRZO buffer count
            {
                char rrzoSize[PROPERTY_VALUE_MAX] = {'\0'};
                int tempMrrzoSize;
                ::property_get("debug.camera.rrzosize", rrzoSize, "-1");
                tempMrrzoSize = ::atoi(rrzoSize);
                if (tempMrrzoSize != -1)
                {
                    mrrzoSize_Max = tempMrrzoSize;
                    // incase of set RRZO count as 0
                    if (mrrzoSize_Max == 0)
                    {
                        mrrzoSize_Max = 8;
                    }
                    mrrzoSize_Min = mrrzoSize_Max/2;
                }
            }
            MY_LOGI("RRZO Count %d/%d",mrrzoSize_Min, mrrzoSize_Max);
            if(mNeedDumpRRZO)
            {
                if ( OK != decideRrzoImage(
                                helper, bitDepth,
                                recordsize, usage,
                                2, 10,
                                pImage_ResizedRaw
                            ))
                {
                    MY_LOGE("No rrzo image");
                    return BAD_VALUE;
                }
            }
            else
            {
                if ( OK != decideRrzoImage(
                                helper, bitDepth,
                                recordsize, usage,
                                mrrzoSize_Min, mrrzoSize_Max,
                                pImage_ResizedRaw
                            ))
                {
                    MY_LOGE("No rrzo image");
                    return BAD_VALUE;
                }
            }
            //
            if(mNeedDumpRRZO)
            {
                MY_LOGD("Open RRZO for dump mNeedDumpRRZO(%d)",mNeedDumpRRZO);
                pResizedPair = PairMetadata::createInstance(pImage_ResizedRaw->getStreamName());
                pResizedRawPool = new BufferPoolImp(pImage_ResizedRaw);
                //
                sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                pFactory->setImageStreamInfo(pImage_ResizedRaw);
                pFactory->setPairRule(pResizedPair, 1);
                pFactory->setUsersPool(pResizedRawPool);
                sp<StreamBufferProvider> pProducer = pFactory->create();
                //
                mpResourceContainer->setConsumer( pImage_ResizedRaw->getStreamId(), pProducer );
                //
                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_ResizedRaw,
                        pProducer,
                        0
                    }
                );
            }
            else
            {
                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_ResizedRaw,
                        NULL,
                        0
                    }
                );
            }

            if(!bHighSpeedMode)
            {
                if(mNeedDumpIMGO)
                {
                    MY_LOGD("Open IMGO for dump mNeedDumpIMGO(%d)",mNeedDumpIMGO);
                    // for Debug Dump
                    if ( OK != decideImgoImage(
                                    helper, bitDepth,
                                    mSensorParam.size, usage,
                                    4, 10,
                                    pImage_FullRaw
                                ))
                    {
                        MY_LOGE("No imgo image");
                        return BAD_VALUE;
                    }
                }
                else
                {
                    // for VSS
                    if ( OK != decideImgoImage(
                                    helper, bitDepth,
                                    mSensorParam.size, usage,
                                    1, 1,
                                    pImage_FullRaw
                                ))
                    {
                        MY_LOGE("No imgo image");
                        return BAD_VALUE;
                    }
                }
                pFullPair = PairMetadata::createInstance(pImage_FullRaw->getStreamName());

                sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                pFactory->setImageStreamInfo(pImage_FullRaw);
                if (mNeedDumpIMGO)
                {
                    pFullRawPool = new BufferPoolImp(pImage_FullRaw);
                    pFactory->setPairRule(pFullPair, 3);
                    pFactory->setUsersPool(pFullRawPool);
                    pVssSelector->setDumpConfig(mOpenId);
                }
                else
                {
                    pFactory->setPairRule(pFullPair, 2);
                }
                sp<StreamBufferProvider> pProducer = pFactory->create();
                pProducer->setSelector(pVssSelector);
                mpResourceContainer->setConsumer(pImage_FullRaw->getStreamId(),pProducer);

                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_FullRaw,
                        pProducer,
                        0
                    }
                );
            }

            // RAW (LCSO with provider)
            if (!bHighSpeedMode && mLPBConfigParams.enableLCS)
            {
                MUINT32 const bitDepth = 10; // no use
                MSize anySize; // no use
                MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE; //not necessary here

                MINT32 maxBufNum = 10 + eis_supFrms;

                if ( OK != decideLcsoImage(
                            helper, bitDepth,
                            anySize, usage,
                            2, maxBufNum,
                            pImage_LcsoRaw
                        ))
                {
                    MY_LOGE("No lcso image");
                    return BAD_VALUE;
                }

                sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                pFactory->setImageStreamInfo(pImage_LcsoRaw);
                // Need to set the same PairMetadata, and the PairMetadata need to wait 2 buffer
                pFactory->setPairRule(pFullPair, 2);
                sp<StreamBufferProvider> pProducer = pFactory->create();
                // Need set Selector
                pProducer->setSelector(pVssSelector);

                mpResourceContainer->setConsumer(pImage_LcsoRaw->getStreamId(),pProducer);

                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_LcsoRaw,
                        pProducer,
                        0
                    }
                );
            }
        }
        // YUV preview -> display client
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_DISPLAY);

            setCamClient(
                "Hal:Image:yuvDisp",
                eSTREAMID_IMAGE_PIPE_YUV_00,
                vImageParam,
                clientMode,
                GRALLOC_USAGE_HW_COMPOSER,
                MTRUE
            );
        }
        // YUV record
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_REC_CB);
            clientMode.push_back(IImgBufProvider::eID_PRV_CB);

            setCamClient(
                "Hal:Image:yuvRecord",
                eSTREAMID_IMAGE_PIPE_YUV_01,
                vImageParam,
                clientMode,
                GRALLOC_USAGE_HW_VIDEO_ENCODER
            );
        }

        pBuilder->setDst(vImageParam);
    }

    mpPipeline = pBuilder->create();

    if ( mpPipeline == 0) {
        MY_LOGE("Fail to create Legacy Pipeline.");
        return BAD_VALUE;
    }

    if(!bHighSpeedMode)
    {
        sp<StreamBufferProvider> pTempConsumer = mpResourceContainer->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
        MY_LOGD("provider(%x)", pTempConsumer.get());
        //
        //
        if ( mNeedDumpIMGO )
        {
            pFullRawPool->allocateBuffer(
            pImage_FullRaw->getStreamName(),
            pImage_FullRaw->getMaxBufNum(),
            pImage_FullRaw->getMinInitBufNum()
                    );

            sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
            pProcessor->registerListener(
                            eSTREAMID_META_HAL_DYNAMIC_P2,
                            pFullPair
                            );
        }
        //
        sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
        pProcessor->registerListener(
                        eSTREAMID_META_APP_DYNAMIC_P1,
                        pFullPair
                        );
        pProcessor->registerListener(
                        eSTREAMID_META_HAL_DYNAMIC_P1,
                        pFullPair
                        );
        //
        sp<FrameInfo> pFrameInfo = new FrameInfo();
        mpResourceContainer->setLatestFrameInfo(pFrameInfo);
        pProcessor->registerListener(
                        eSTREAMID_META_APP_DYNAMIC_P1,
                        pFrameInfo
                        );
    }
    //
    if ( mNeedDumpRRZO )
    {
        pResizedRawPool->allocateBuffer(
        pImage_ResizedRaw->getStreamName(),
        pImage_ResizedRaw->getMaxBufNum(),
        pImage_ResizedRaw->getMinInitBufNum()
                );

        sp<StreamBufferProvider> pTempConsumer =
            mpResourceContainer->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_RESIZER );

        if ( pTempConsumer != 0 ) {
            sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
            pProcessor->registerListener(
                            eSTREAMID_META_HAL_DYNAMIC_P2,
                            pResizedPair
                            );
            // Need set Selector
            sp<DumpBufferSelector> pSelector = new DumpBufferSelector();
            pSelector->setDumpConfig( mOpenId );
            pTempConsumer->setSelector(pSelector);
            //
        }
    }

    FUNC_END;

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
constructZsdPreviewPipeline()
{
    FUNC_START;

    CAM_TRACE_NAME("DFC:constructZsdPreviewPipeline");

    mLPBConfigParams.mode = LegacyPipelineMode_T::PipelineMode_Feature_ZsdPreview;

    HwInfoHelper helper(mOpenId);
    if( ! helper.updateInfos() ) {
        MY_LOGE("cannot properly update infos");
        return BAD_VALUE;
    }

    if (helper.getPDAFSupported(SENSOR_SCENARIO_ID_NORMAL_CAPTURE) && !CamManager::getInstance()->isMultiDevice())
    {
        mLPBConfigParams.disableFrontalBinning = MTRUE;
    }

    if (helper.getDualPDAFSupported(SENSOR_SCENARIO_ID_NORMAL_CAPTURE))
    {
        mLPBConfigParams.enableDualPD = MTRUE;
    }

    sp<LegacyPipelineBuilder> pBuilder =
        LegacyPipelineBuilder::createInstance(
                                    mOpenId,
                                    "ZSD",
                                    mLPBConfigParams);
    if ( pBuilder == 0 ) {
        MY_LOGE("Cannot create LegacyPipelineBuilder.");
        return BAD_VALUE;
    }
    //
    mSensorParam.mode = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
    if( ! helper.getSensorSize( mSensorParam.mode, mSensorParam.size) ||
        ! helper.getSensorFps( (MUINT32)mSensorParam.mode, (MINT32&)mSensorParam.fps) ||
        ! helper.queryPixelMode( mSensorParam.mode, mSensorParam.fps, mSensorParam.pixelMode)
      ) {
        MY_LOGE("cannot get params about sensor");
        return BAD_VALUE;
    }

#ifdef FEATURE_MODIFY
    // Sensor
    mSensorParam.vhdrMode = mpParamsManagerV3->getParamsMgr()->getVHdr();
    MY_LOGD("VHDR sensor mode:%d, rawType:%d, size:%dx%d, fps:%d pixel:%d vhdrMode:%d",
        mSensorParam.mode,
        mSensorParam.rawType,
        mSensorParam.size.w, mSensorParam.size.h,
        mSensorParam.fps,
        mSensorParam.pixelMode,
        mSensorParam.vhdrMode);
#else
    // Sensor
    MY_LOGD("sensor mode:%d, rawType:%d, size:%dx%d, fps:%d pixel:%d",
        mSensorParam.mode,
        mSensorParam.rawType,
        mSensorParam.size.w, mSensorParam.size.h,
        mSensorParam.fps,
        mSensorParam.pixelMode);
#endif
    //
    MUINT featureCFG = 0;
    if (mLPBConfigParams.enableDualPD)
    {
        FEATURE_CFG_ENABLE_MASK(featureCFG,IScenarioControl::FEATURE_DUAL_PD);
    }
    //
    pBuilder->setSrc(mSensorParam);
    //

    sp<IScenarioControl> pScenarioCtrl = enterScenarioControl(getScenario(), mSensorParam.size, mSensorParam.fps, featureCFG);
    if( pScenarioCtrl.get() == NULL )
    {
        MY_LOGE("get Scenario Control fail");
        return BAD_VALUE;
    }
    pBuilder->setScenarioControl(pScenarioCtrl);


    // Image
    sp<PairMetadata>  pPair;
    sp<ZsdSelector> pSelector = new ZsdSelector();
    sp<BufferPoolImp> pFullRawPool;
    sp<IImageStreamInfo> pImage_Raw;
    sp<IImageStreamInfo> pImage_ResizedRaw;
    sp<PairMetadata>  pResizedPair;
    sp<BufferPoolImp> pResizedRawPool;

    {
        Vector<PipelineImageParam> vImageParam;
        // RAW
        {
            CAM_TRACE_NAME("allocate RAW");
            MUINT32 const bitDepth = getPreviewRawBitDepth();
            MSize previewsize;
            MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE; //not necessary here
            mpParamsManagerV3->getParamsMgr()->getPreviewSize(&previewsize.w, &previewsize.h);
            //RRZO RAW
            if ( OK != decideRrzoImage(
                            helper, bitDepth,
                            previewsize, usage,
                            0, 6,
                            pImage_ResizedRaw
                        ))
            {
                MY_LOGE("No rrzo image");
                return BAD_VALUE;
            }
            //
             if(mNeedDumpRRZO)
             {
                 MY_LOGD("Open RRZO for dump mNeedDumpRRZO(%d)",mNeedDumpRRZO);
                 pResizedPair = PairMetadata::createInstance(pImage_ResizedRaw->getStreamName());
                 pResizedRawPool = new BufferPoolImp(pImage_ResizedRaw);
                 //
                 sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                 pFactory->setImageStreamInfo(pImage_ResizedRaw);
                 pFactory->setPairRule(pResizedPair, 2);
                 pFactory->setUsersPool(pResizedRawPool);
                 sp<StreamBufferProvider> pProducer = pFactory->create();
                 //
                 mpResourceContainer->setConsumer( pImage_ResizedRaw->getStreamId(), pProducer );
                 //
                 vImageParam.push_back(
                     PipelineImageParam{
                         pImage_ResizedRaw,
                         pProducer,
                         0
                     }
                 );
             }
             else
             {
                 vImageParam.push_back(
                     PipelineImageParam{
                         pImage_ResizedRaw,
                         NULL,
                         0
                     }
                 );
             }

#if 1
            //IMGO RAW
            if ( OK != decideImgoImage(
                            helper, bitDepth,
                            mSensorParam.size, usage,
                            0, 9,
                            pImage_Raw
                        ))
            {
                MY_LOGE("No imgo image");
                return BAD_VALUE;
            }
            pPair = PairMetadata::createInstance(pImage_Raw->getStreamName());
            pFullRawPool = new BufferPoolImp(pImage_Raw);

            sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
            pFactory->setImageStreamInfo(pImage_Raw);
            pFactory->setPairRule(pPair, 2);
            pFactory->setUsersPool(pFullRawPool);
            sp<StreamBufferProvider> pProducer = pFactory->create();
            // Need set Selector
            pProducer->setSelector(pSelector);
            if(mNeedDumpIMGO)
            {
                pSelector->setDumpConfig( mOpenId );
            }
            //
            mpResourceContainer->setConsumer( pImage_Raw->getStreamId(), pProducer);

            vImageParam.push_back(
                PipelineImageParam{
                    pImage_Raw,
                    pProducer,
                    0
                }
            );
#endif
            // RAW (LCSO with provider)
            if (mLPBConfigParams.enableLCS)
            {
                MUINT32 const bitDepth = 10; // no use
                MSize anySize; // no use
                MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE; //not necessary here
                sp<IImageStreamInfo> pImage_Lcso;

                if ( OK != decideLcsoImage(
                            helper, bitDepth,
                            anySize, usage,
                            4, 12,
                            pImage_Lcso
                        ))
                {
                    MY_LOGE("No lcso image");
                    return BAD_VALUE;
                }

                sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                pFactory->setImageStreamInfo(pImage_Lcso);
                // Need to set the same PairMetadata, and the PairMetadata need to wait 2 buffer
                pFactory->setPairRule(pPair, 2);
                sp<StreamBufferProvider> pProducer = pFactory->create();
                // Need set Selector
                pProducer->setSelector(pSelector);

                mpResourceContainer->setConsumer(pImage_Lcso->getStreamId(),pProducer);

                vImageParam.push_back(
                    PipelineImageParam{
                        pImage_Lcso,
                        pProducer,
                        0
                    }
                );
            }
        }
        // YUV preview -> display client
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_DISPLAY);

            setCamClient(
                "Hal:Image:yuvDisp",
                eSTREAMID_IMAGE_PIPE_YUV_00,
                vImageParam,
                clientMode,
                GRALLOC_USAGE_HW_COMPOSER,
                MTRUE
            );
        }
        // YUV preview callback -> preview callback client
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_PRV_CB);
            clientMode.push_back(IImgBufProvider::eID_GENERIC);

            setCamClient(
                "Hal:Image:yuvPrvCB",
                eSTREAMID_IMAGE_PIPE_YUV_01,
                vImageParam,
                clientMode
            );
        }
        // YUV FD & OT
        {
            Vector<MUINT32> clientMode;
            clientMode.push_back(IImgBufProvider::eID_FD);
            clientMode.push_back(IImgBufProvider::eID_OT);

            setCamClient(
                "Hal:Image:yuvFD",
                eSTREAMID_IMAGE_YUV_FD,
                vImageParam,
                clientMode
            );
        }

        pBuilder->setDst(vImageParam);
    }

    mpPipeline = pBuilder->create();

    if ( mpPipeline == 0) {
        MY_LOGE("Fail to create Legacy Pipeline.");
        return BAD_VALUE;
    }
    //
    pFullRawPool->allocateBuffer(
            pImage_Raw->getStreamName(),
            pImage_Raw->getMaxBufNum(),
            pImage_Raw->getMinInitBufNum()
            );

#if 1
    sp<StreamBufferProvider> pTempConsumer =
        mpResourceContainer->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );

    if ( pTempConsumer != 0 ) {
        CAM_TRACE_NAME("DFC:SetSelector");
        sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
        pProcessor->registerListener(
                        eSTREAMID_META_APP_DYNAMIC_P1,
                        pPair
                        );
        pProcessor->registerListener(
                        eSTREAMID_META_HAL_DYNAMIC_P1,
                        pPair
                        );
        //
        sp<FrameInfo> pFrameInfo = new FrameInfo();
        mpResourceContainer->setLatestFrameInfo(pFrameInfo);
        pProcessor->registerListener(
                        eSTREAMID_META_APP_DYNAMIC_P1,
                        pFrameInfo
                        );
    }
#endif

    if ( mNeedDumpRRZO )
    {
        pResizedRawPool->allocateBuffer(
        pImage_ResizedRaw->getStreamName(),
        pImage_ResizedRaw->getMaxBufNum(),
        pImage_ResizedRaw->getMinInitBufNum()
                );

        sp<StreamBufferProvider> pTempConsumer =
            mpResourceContainer->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_RESIZER );

        if ( pTempConsumer != 0 ) {
            sp<ResultProcessor> pProcessor = mpPipeline->getResultProcessor().promote();
            pProcessor->registerListener(
                            eSTREAMID_META_HAL_DYNAMIC_P2,
                            pResizedPair
                            );
            pProcessor->registerListener(
                            eSTREAMID_META_APP_DYNAMIC_P1,
                            pResizedPair
                            );
            // Need set Selector
            sp<DumpBufferSelector> pSelector = new DumpBufferSelector();
            pSelector->setDumpConfig( mOpenId );
            pTempConsumer->setSelector(pSelector);
            //
        }
    }

    FUNC_END;

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
FeatureFlowControl::
getScenario() const
{
    switch(mLPBConfigParams.mode)
    {
        case LegacyPipelineMode_T::PipelineMode_Preview:
        case LegacyPipelineMode_T::PipelineMode_Feature_Preview:
            return IScenarioControl::Scenario_NormalPreivew;
        case LegacyPipelineMode_T::PipelineMode_ZsdPreview:
        case LegacyPipelineMode_T::PipelineMode_Feature_ZsdPreview:
            return IScenarioControl::Scenario_ZsdPreview;
        case LegacyPipelineMode_T::PipelineMode_VideoRecord:
        case LegacyPipelineMode_T::PipelineMode_Feature_VideoRecord:
            return IScenarioControl::Scenario_VideoRecord;
        case LegacyPipelineMode_T::PipelineMode_HighSpeedVideo:
        case LegacyPipelineMode_T::PipelineMode_Feature_HighSpeedVideo:
            return IScenarioControl::Scenario_HighSpeedVideo;
        default:
            MY_LOGW("no mapped scenario for mode %d", mLPBConfigParams.mode);
            break;
    }
    return IScenarioControl::Scenario_None;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FeatureFlowControl::
decideSensorModeAndRrzo(
    MSize&  recordsize,
    MUINT&  sensorMode,
    MBOOL   bHighSpeedMode
)
{
    MSize paramSize, wantedSize, sensorSize;
    MSize previewModeSize = MSize(0,0);
    HwInfoHelper helper(mOpenId);
    bool isEisOn = mpParamsManagerV3->getParamsMgr()->getVideoStabilization();
    //
    if( ! helper.updateInfos() ) {
        MY_LOGE("cannot properly update infos");
        return BAD_VALUE;
    }
    //
    mpParamsManagerV3->getParamsMgr()->getVideoSize(&paramSize.w, &paramSize.h);
    if( paramSize.w*paramSize.h > IMG_1080P_SIZE )
    {
        mb4K2KVideoRecord = MTRUE;
    }
    else
    {
        mb4K2KVideoRecord = MFALSE;
    }
    //
    wantedSize = paramSize;

    if(!bHighSpeedMode)
    {
        if( isEisOn )
        {
            MFLOAT eis_factor = mb4K2KVideoRecord ?
                                EISCustom::getEISFactor() : EISCustom::getEISFHDFactor();

            wantedSize.w *= eis_factor/100.0f;//for EIS
            wantedSize.h *= eis_factor/100.0f;//for EIS
        }
        //
        sensorMode = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
        if( helper.getSensorSize( SENSOR_SCENARIO_ID_NORMAL_PREVIEW, previewModeSize) &&
            wantedSize.w <= previewModeSize.w &&
            wantedSize.h <= previewModeSize.h )
        {
            sensorMode = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
        }
    }
    //
    if( !helper.getSensorSize( sensorMode, sensorSize) )
    {
        MY_LOGE("cannot get params about sensor");
        return BAD_VALUE;
    }
    MY_LOGI("wanted(%dx%d), previewMode(%dx%d), mode(%d), sensorSize(%dx%d)",
            wantedSize.w,
            wantedSize.h,
            previewModeSize.w,
            previewModeSize.h,
            sensorMode,
            sensorSize.w,
            sensorSize.h);
    //
    recordsize.w = wantedSize.w;
    recordsize.h = wantedSize.h;
    if( wantedSize.w < 1920 )
    {
        recordsize.w = 1920;
    }
    if( wantedSize.h < 1080 )
    {
        recordsize.h = 1080;
    }
    MY_LOGI("record(%dx%d)",
                recordsize.w, recordsize.h);
    return OK;
}
