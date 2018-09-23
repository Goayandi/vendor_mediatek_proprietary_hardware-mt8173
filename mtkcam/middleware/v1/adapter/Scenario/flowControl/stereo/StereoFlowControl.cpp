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

#define LOG_TAG "MtkCam/StereoFlowControl"
//
#include "../MyUtils.h"
//
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>

#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>

#include <mtkcam/middleware/v1/camutils/CamInfo.h>
#include <mtkcam/middleware/v1/IParamsManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/request/IRequestController.h>
#include <mtkcam/middleware/v1/LegacyPipeline/ILegacyPipeline.h>

#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>

#include <mtkcam/utils/fwk/MtkCamera.h>

#include <bandwidth_control.h>

#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>

#include "StereoFlowControl.h"
#include <mtkcam/drv/iopipe/SImager/ISImagerDataTypes.h>
#include <mtkcam/middleware/v1/camshot/BufferCallbackHandler.h>
//#include <StereoLegacyPipeline.h>
//
#include <string>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProviderFactory.h>
#include <mtkcam/middleware/v1/camshot/BufferCallbackHandler.h>
#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/BufferPoolImp.h>
using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace android;
using namespace NSCam::v3;
using namespace NSCam::v3::Utils;


/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s] " fmt, __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

#define MY_LOGD1(...)               MY_LOGD_IF(1<=mLogLevel, __VA_ARGS__)

#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")

#define NEW_CAPTURE_FLOW (1)
#define CHECK_OBJECT(x)  do{                                        \
    if (x == nullptr) { MY_LOGE("Null %s Object", #x); return MFALSE;} \
} while(0)
/******************************************************************************
 *
 ******************************************************************************/
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if( pMetadata == NULL ) {
        MY_LOGW("pMetadata == NULL");
        return MFALSE;
    }

    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if( !entry.isEmpty() ) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    return MFALSE;
}
/******************************************************************************
 *
 ******************************************************************************/
StereoFlowControl::
StereoFlowControl(
    char const*                 pcszName,
    MINT32 const                i4OpenId,
    sp<IParamsManagerV3>          pParamsManagerV3,
    sp<ImgBufProvidersManager>  pImgBufProvidersManager,
    sp<INotifyCallback>         pCamMsgCbInfo
)
    : mpParamsManagerV3(pParamsManagerV3)
    , mpImgBufProvidersMgr(pImgBufProvidersManager)
    , mName(const_cast<char*>(pcszName))
    , mpCamMsgCbInfo(pCamMsgCbInfo)
    , mOpenId_P2Prv(28285) // magic number for pipelineP2Prv
{
    if(!StereoSettingProvider::getStereoSensorIndex(mOpenId, mOpenId_main2)){
        MY_LOGE("Cannot get sensor ids from StereoSettingProvider! (%d,%d)", mOpenId, mOpenId_main2);
        return;
    }

    if(getOpenId() != i4OpenId){
        MY_LOGE("mOpenId(%d) != i4OpenId(%d), should not have happened!", getOpenId(), i4OpenId);
        return;
    }

    mpResourceContainier = IResourceContainer::getInstance(getOpenId());
    mpResourceContainierMain2 = IResourceContainer::getInstance(getOpenId_Main2());
    mpResourceContainierP2Prv = IResourceContainer::getInstance(getOpenId_P2Prv()); // Use this magic number to create consumer container for P2 Prv

    mCurrentSensorModuleType = getSensorModuleType();
    mCurrentStereoMode = getStereoMode();
    StereoSettingProvider::setStereoFeatureMode(mCurrentStereoMode);

    MY_LOGD("mCurrentSensorModuleType=%d, mCurrentStereoMode=%d", mCurrentSensorModuleType, mCurrentStereoMode);


    char cProperty[PROPERTY_VALUE_MAX];
    ::property_get("debug.camera.log", cProperty, "0");
    mLogLevel = ::atoi(cProperty);
    if ( 0 == mLogLevel ) {
        ::property_get("debug.camera.log.stereoflow", cProperty, "0");
        mLogLevel = ::atoi(cProperty);
    }

    enterMMDVFSScenario();

    MY_LOGD("StereoFlowControl => mOpenId(%d), mOpenId_main2(%d), mOpenId_P2Prv(%d), mLogLevel(%d)",
        getOpenId(),
        getOpenId_Main2(),
        getOpenId_P2Prv(),
        mLogLevel
    );
}

/******************************************************************************
 *
 ******************************************************************************/
char const*
StereoFlowControl::
getName()   const
{
    return mName;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t
StereoFlowControl::
getOpenId() const
{
    return mOpenId;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t
StereoFlowControl::
getOpenId_Main2() const
{
    return mOpenId_main2;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t
StereoFlowControl::
getOpenId_P2Prv() const
{
    return mOpenId_P2Prv;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
startPreview()
{
    FUNC_START;

    mPipelineMode = PipelineMode_ZSD;
    mIspProfile = NSIspTuning::EIspProfile_N3D_Capture;
    MY_LOGD("mPipelineMode=%d, mIspProfile=%d", mPipelineMode, mIspProfile);

    mpResourceContainier->setFeatureFlowControl(this);

    // Scenario Control: CPU Core
    enterPerformanceScenario(ScenarioMode::PREVIEW); // exit current scenario before entering the next one

    // get pass1 active array crop
    if(queryPass1ActiveArrayCrop() != OK)
    {
        return BAD_VALUE;
    }

    // create request controllers
    if(createStereoRequestController() != OK)
    {
        return UNKNOWN_ERROR;
    }

    // create pipelines
    MY_LOGD("create pipelines");
    // get context builder table
    if(buildStereoPipeline(
                        mCurrentStereoMode,
                        mCurrentSensorModuleType,
                        mPipelineMode)!=OK)
    {
        MY_LOGE("get context builder content fail");
        return INVALID_OPERATION;
    }

    if ( mpPipeline_P1 == 0 || mpPipeline_P1_Main2 == 0 || mpPipeline_P2 == 0) {
        MY_LOGE("Cannot get pipeline. start preview fail.");
        MY_LOGE_IF(mpPipeline_P1 == 0, "mpPipeline_P1 == 0");
        MY_LOGE_IF(mpPipeline_P1_Main2 == 0, "mpPipeline_P1_Main2 == 0");
        MY_LOGE_IF(mpPipeline_P2 == 0, "mpPipeline_P2 == 0");
        return BAD_VALUE;
    }

    // create stereoSynchronizer
    if(setAndStartStereoSynchronizer() != OK)
    {
        return UNKNOWN_ERROR;
    }

    // start pipeline
    if(startStereoPipeline(
                        STEREO_FLOW_PREVIEW_REQUSET_NUM_START,
                        STEREO_FLOW_PREVIEW_REQUSET_NUM_END) != OK)
    {
        return UNKNOWN_ERROR;
    }
    // create callback handler
    mpCallbackHandler = new BufferCallbackHandler(0);
    mpCallbackHandler->setImageCallback(mpShot);

    // temperature monitor
    mpTemperatureMonitor = ITemperatureMonitor::create();
    if(mpTemperatureMonitor != nullptr)
    {
        MINT32 dev_main1 = -1, dev_main2 = -1;
        if(!StereoSettingProvider::getStereoSensorDevIndex(dev_main1, dev_main2)){
            MY_LOGE("Cannot get sensor dev ids from StereoSettingProvider! (%d,%d)", dev_main1, dev_main2);
            goto lbExit;
        }
        mpTemperatureMonitor->addToObserve(mOpenId, dev_main1);
        mpTemperatureMonitor->addToObserve(mOpenId_main2, dev_main2);
        mpTemperatureMonitor->init();
        mpTemperatureMonitor->run("TemperatureMonitor");
    }
    FUNC_END;
lbExit:
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
stopPreview()
{
    FUNC_START;
    Mutex::Autolock _l(mCaptureLock);

    while(!mvCaptureRequests.isEmpty()){
        MY_LOGD("wait for capture requests(%d) done...", mvCaptureRequests.size());
        mCondCaptureLock.wait(mCaptureLock);
    }
    //
    uninitPipelineAndRelatedResource();
    //
    mpCallbackHandler.clear();
    mpCallbackHandler = nullptr;
    // uninit temperator monitor
    if(mpTemperatureMonitor!=nullptr)
    {
        mpTemperatureMonitor->requestExit();
        mpTemperatureMonitor->join();
        mpTemperatureMonitor->uninit();
        mpTemperatureMonitor = nullptr;
    }

    exitPerformanceScenario();

    // release member object
    mpPipeline_P2 = nullptr;
    mpPipeline_P1 = nullptr;
    mpPipeline_P1_Main2 = nullptr;
    if(mpImageStreamManager != NULL)
    {
        mpImageStreamManager->destroy();
        mpImageStreamManager=nullptr;
    }
    mpShot = nullptr;
    mpParamsManagerV3 = nullptr;
    mpImgBufProvidersMgr = nullptr;
    mpCamMsgCbInfo = nullptr;
    mpResourceContainier = nullptr;
    mpResourceContainierMain2 = nullptr;
    mpResourceContainierP2Prv = nullptr;
    mpStereoBufferSynchronizer = nullptr;
    IResourceContainer::getInstance(getOpenId())->clearBufferProviders();
    IResourceContainer::getInstance(getOpenId_Main2())->clearBufferProviders();
    IResourceContainer::getInstance(getOpenId_P2Prv())->clearBufferProviders();

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
pausePreview(
    MBOOL stopPipeline
)
{
    MY_LOGE("no implementation!");
    return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
resumePreview()
{
    MY_LOGE("no implementation!");
    return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
startRecording()
{
    FUNC_START;

    mPipelineMode = PipelineMode_RECORDING;
    mIspProfile = NSIspTuning::EIspProfile_N3D_Video;
    MY_LOGD("mPipelineMode=%d, mIspProfile=%d", mPipelineMode, mIspProfile);

    // get pass1 active array crop
    if(queryPass1ActiveArrayCrop() != OK)
    {
        return BAD_VALUE;
    }

    // create request controllers
    if(createStereoRequestController() != OK)
    {
        return UNKNOWN_ERROR;
    }

    setRequsetTypeForAllPipelines(MTK_CONTROL_CAPTURE_INTENT_VIDEO_RECORD);

    // Scenario Control: CPU Core
    enterPerformanceScenario(ScenarioMode::PREVIEW);

    // create pipelines
    MY_LOGD("create pipelines");

    // get context builder table
    if(buildStereoPipeline(
                        mCurrentStereoMode,
                        mCurrentSensorModuleType,
                        mPipelineMode)!=OK)
    {
        MY_LOGE("get context builder content fail");
        return INVALID_OPERATION;
    }

    if ( mpPipeline_P1 == 0 || mpPipeline_P1_Main2 == 0 || mpPipeline_P2 == 0) {
        MY_LOGE("Cannot get pipeline. start recording fail.");
        MY_LOGE_IF(mpPipeline_P1 == 0, "mpPipeline_P1 == 0");
        MY_LOGE_IF(mpPipeline_P1_Main2 == 0, "mpPipeline_P1_Main2 == 0");
        MY_LOGE_IF(mpPipeline_P2 == 0, "mpPipeline_P2 == 0");
        return BAD_VALUE;
    }

    // create stereoSynchronizer
    if(setAndStartStereoSynchronizer() != OK)
    {
        return UNKNOWN_ERROR;
    }

    // start pipeline
    if(startStereoPipeline(
                        STEREO_FLOW_PREVIEW_REQUSET_NUM_START,
                        STEREO_FLOW_PREVIEW_REQUSET_NUM_END) != OK)
    {
        return UNKNOWN_ERROR;
    }

    // temperature monitor
    mpTemperatureMonitor = ITemperatureMonitor::create();
    if(mpTemperatureMonitor != nullptr)
    {
        MINT32 dev_main1 = -1, dev_main2 = -1;
        if(!StereoSettingProvider::getStereoSensorDevIndex(dev_main1, dev_main2)){
            MY_LOGE("Cannot get sensor dev ids from StereoSettingProvider! (%d,%d)", dev_main1, dev_main2);
            goto lbExit;
        }
        mpTemperatureMonitor->addToObserve(mOpenId, dev_main1);
        mpTemperatureMonitor->addToObserve(mOpenId_main2, dev_main2);
        mpTemperatureMonitor->init();
        mpTemperatureMonitor->run("TemperatureMonitor");
    }
lbExit:
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
stopRecording()
{
    FUNC_START;

    // do nothing to reduce stopRecord time

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
autoFocus()
{
    // also send auto focus to P2 pipeline for touch point update
    if(mpRequestController_P2 != 0){
        if(mpRequestController_P2->autoFocus() != OK){
            MY_LOGE("mpRequestController_P2->autoFocus failed!");
        }
    }

    // send auto focus to P1_main2 pipeline
    if(mpRequestController_P1_Main2 != 0){
        if(mpRequestController_P1_Main2->autoFocus() != OK){
            MY_LOGE("mpRequestController_P1_Main2->autoFocus failed!");
        }
    }

    // send auto focus to P1 pipeline
    return (mpRequestController_P1 != 0) ?
        mpRequestController_P1->autoFocus() : OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
cancelAutoFocus()
{
    if(mpRequestController_P2 != 0){
        if(mpRequestController_P2->cancelAutoFocus() != OK){
            MY_LOGE("mpRequestController_P2->cancelAutoFocus failed!");
        }
    }

    if(mpRequestController_P1_Main2 != 0){
        if(mpRequestController_P1_Main2->cancelAutoFocus() != OK){
            MY_LOGE("mpRequestController_P1_Main2->cancelAutoFocus failed!");
        }
    }

    return (mpRequestController_P1 != 0) ?
        mpRequestController_P1->cancelAutoFocus() : OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
precapture(int& flashRequired)
{
    return (mpRequestController_P1 != 0) ?
        mpRequestController_P1->precapture(flashRequired) : OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
takePicture()
{
    FUNC_START;
    MY_LOGE("not implementation");
    FUNC_END;
    return UNKNOWN_ERROR;
}

/******************************************************************************
 * Take Picture with StereoShotParam
 ******************************************************************************/
status_t
StereoFlowControl::
takePicture(StereoShotParam shotParam)
{
    CAM_TRACE_NAME("takePicture");
    FUNC_START;

    MINT32 reqNo = miCapCounter;
    CaptureBufferData bufferData;
    IMetadata appMeta = shotParam.mShotParam.mAppSetting;
    IMetadata halMeta;
    BufferList vDstStreams;
    {
        Mutex::Autolock _l(mCaptureLock);

        if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
           getPipelineMode() == PipelineMode_ZSD)
        {
            // DENOISE Only:
            // stop taking picture if the on-going capture reqeusts still many
            while(mvCaptureRequests.size() >= get_stereo_bmdenoise_capture_buffer_cnt()){
                MY_LOGD("still too many capture request(%d) on-going...waiting", mvCaptureRequests.size());
                mCondCaptureLock.wait(mCaptureLock);
            }
            MY_LOGD("capture request(%d) on-going...can do more request", mvCaptureRequests.size());
        }

        if(isDNGEnable)
        {
            MY_LOGD("Dng is enabled");
        }

        status_t ret = OK;
        //
        SettingSet           settingSet;
        Vector< SettingSet > vSettings;

        MY_LOGD("waitAndLock synchronizer");
        mpStereoBufferSynchronizer->waitAndLockCapture();

        MY_LOGD("pause P2 pipeline");
        vDstStreams.clear();
        mpRequestController_P2->pausePipeline(vDstStreams);

        if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
           getPipelineMode() == PipelineMode_ZSD)
        {
            // DENOISE Only:
            // do not flush p2 pipeline since it may still doing capture requests
            MY_LOGD("do not flush p2 pipeline since it may still doing capture requests");
        }
        else
        {
            if(mpPipeline_P2!=nullptr)
            {
                mpPipeline_P2->flush();
                mpPipeline_P2->waitUntilDrained();
            }

        }

        MY_LOGD_IF(mpStereoBufferPool_RESIZER != nullptr,"[main1 rrz]  after preview drain buffer size (%d/%d)",
            mpStereoBufferPool_RESIZER->getCurrentAvailableBufSize(),
            mpStereoBufferPool_RESIZER->getInUseBufSize()
        );
        MY_LOGD_IF(mpStereoBufferPool_OPAQUE != nullptr,"[main1 imgo] after preview drain available buffer size (%d/%d)",
            mpStereoBufferPool_OPAQUE->getCurrentAvailableBufSize(),
            mpStereoBufferPool_OPAQUE->getInUseBufSize()
        );
        MY_LOGD_IF(mpStereoBufferPool_RESIZER_MAIN2 != nullptr,"[main2 rrz] after preview drain available buffer size (%d/%d)",
            mpStereoBufferPool_RESIZER_MAIN2->getCurrentAvailableBufSize(),
            mpStereoBufferPool_RESIZER_MAIN2->getInUseBufSize()
        );
        MY_LOGD_IF(mpStereoBufferPool_OPAQUE_MAIN2 != nullptr,"[main2 imgo] after preview drain available buffer size (%d/%d)",
            mpStereoBufferPool_OPAQUE_MAIN2->getCurrentAvailableBufSize(),
            mpStereoBufferPool_OPAQUE_MAIN2->getInUseBufSize()
        );
        MY_LOGD("wait preview frame drained done");
        //
        if(STEREO_3RDPARTY == mCurrentStereoMode && getPipelineMode() == PipelineMode_ZSD)
        {
            // 3rd party
            buildCaptureStream_3rdParty(vDstStreams, isDNGEnable);
            setCaptureBufferProvoider_3rdParty(shotParam.mJpegParam, shotParam.mShotParam, shotParam.mbDngMode);
        }
        else if(STEREO_BB_PRV_CAP_REC == (mCurrentStereoMode|mCurrentSensorModuleType) &&
           getPipelineMode() == PipelineMode_ZSD)
        {
            // vsdof
            buildCaptureStream(vDstStreams, isDNGEnable);
            setCaptureBufferProvoider(shotParam.mJpegParam, shotParam.mShotParam, shotParam.mbDngMode);
        }
    #if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
        else if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
           getPipelineMode() == PipelineMode_ZSD)
        {
            // denoise
            buildCaptureStream_DeNoise(vDstStreams, isDNGEnable);
            // check if we need to re-construct providers
            // if we do, wait until all current capture done then substitute the buffer providers
            if(needReAllocBufferProvider(shotParam)){
                MY_LOGD("needReAllocBufferProvider");
                while(!mvCaptureRequests.isEmpty()){
                    MY_LOGD("on-going capture requests not done, wait +");
                    mCondCaptureLock.wait(mCaptureLock);
                    MY_LOGD("on-going capture requests not done, wait -");
                }
                MY_LOGD("capture requests all done, ready to re-allocate buffer providers");
            }
            setCaptureBufferProvoider_DeNoise(
                shotParam.mJpegParam,
                shotParam.mShotParam,
                shotParam.mbDngMode,
                needReAllocBufferProvider(shotParam),
                get_stereo_bmdenoise_capture_buffer_cnt()
            );
            mCurrentStereoShotParam = shotParam;
        }
    #endif
        else
        {
            MY_LOGE("should not happened!");
            return UNKNOWN_ERROR;
        }
        MY_LOGD("build Capture Stream and set Capture buffer provider end");
        //
        MY_LOGD("get selectors");
        // mpStereoBufferSynchronizer->waitAndLockCapture();
        {
            ret &= getSelector(mOpenId, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE, bufferData.mpSelector_full);
            ret &= getSelector(mOpenId, eSTREAMID_IMAGE_PIPE_RAW_RESIZER, bufferData.mpSelector_resized);
            ret &= getSelector(mOpenId_main2, eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01, bufferData.mpSelector_resized_main2);
            if(mpStereoSelector_OPAQUE_MAIN2 != nullptr){
                ret &= getSelector(mOpenId_main2, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01, bufferData.mpSelector_full_main2);
            }
            if(ret!=OK){
                return UNKNOWN_ERROR;
            }
        }
        //
        MY_LOGD("get metadata and image buffer");
        if(prepareMetadataAndImageBuffer(&appMeta, &halMeta, &bufferData)!=OK)
        {
            MY_LOGE("Prepare metadata fail.");
            return false;
        }
        MY_LOGD_IF(mpStereoBufferPool_RESIZER != nullptr,"[main1 rrz]  after prepare done buffer size (%d/%d)",
            mpStereoBufferPool_RESIZER->getCurrentAvailableBufSize(),
            mpStereoBufferPool_RESIZER->getInUseBufSize()
        );
        MY_LOGD_IF(mpStereoBufferPool_OPAQUE != nullptr,"[main1 imgo] after prepare drain done buffer size (%d/%d)",
            mpStereoBufferPool_OPAQUE->getCurrentAvailableBufSize(),
            mpStereoBufferPool_OPAQUE->getInUseBufSize()
        );
        MY_LOGD_IF(mpStereoBufferPool_RESIZER_MAIN2 != nullptr,"[main2 rrz] after prepare drain done buffer size (%d/%d)",
            mpStereoBufferPool_RESIZER_MAIN2->getCurrentAvailableBufSize(),
            mpStereoBufferPool_RESIZER_MAIN2->getInUseBufSize()
        );
        MY_LOGD_IF(mpStereoBufferPool_OPAQUE_MAIN2 != nullptr,"[main2 imgo] after prepare drain done buffer size (%d/%d)",
            mpStereoBufferPool_OPAQUE_MAIN2->getCurrentAvailableBufSize(),
            mpStereoBufferPool_OPAQUE_MAIN2->getInUseBufSize()
        );
        mpStereoBufferSynchronizer->unlockCapture();
        //
        MY_LOGD("build capture managers");
        {
            sp<ResultProcessor> pResultProcessor = mpPipeline_P2->getResultProcessor().promote();
            CHECK_OBJECT(pResultProcessor);
            //partial
            pResultProcessor->registerListener(
                STEREO_FLOW_CAPTURE_REQUSET_NUM_START,
                STEREO_FLOW_CAPTURE_REQUSET_NUM_END,
                true,
                mpShot
            );
            //full
            pResultProcessor->registerListener(
                STEREO_FLOW_CAPTURE_REQUSET_NUM_START,
                STEREO_FLOW_CAPTURE_REQUSET_NUM_END,
                false,
                mpShot
            );
        }
        //
        MY_LOGD("build capture streams");
        settingSet.appSetting = appMeta;
        settingSet.halSetting = halMeta;
        vSettings.push(settingSet);
        // dng buffer
        mpShot->setDngFlag(isDNGEnable);

        // switch to high performance mode for capture
        enterPerformanceScenario(ScenarioMode::CAPTURE);

        MY_LOGD("miCapCounter(%d)", miCapCounter);
        mvCaptureRequests.add(miCapCounter, std::chrono::system_clock::now());

        miCapCounter++;
        if(miCapCounter > STEREO_FLOW_CAPTURE_REQUSET_NUM_END)
        {
            miCapCounter = STEREO_FLOW_CAPTURE_REQUSET_NUM_START;
        }
    }

    // do timestamp callbacks then submit request
    {
        // p1 raws
        sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE, bufferData.timestamp_P1);
        sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_RAW_RESIZER, bufferData.timestamp_P1);
        sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01, bufferData.timestamp_P1);
        if(mpStereoBufferPool_OPAQUE_MAIN2 != nullptr){
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01, bufferData.timestamp_P1);
        }
        // p2 buffers
        if(STEREO_3RDPARTY == mCurrentStereoMode && getPipelineMode() == PipelineMode_ZSD)
        {
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN1YUV, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN2YUV, bufferData.timestamp_P1);
        }
        else if(STEREO_BB_PRV_CAP_REC == (mCurrentStereoMode|mCurrentSensorModuleType) &&
           getPipelineMode() == PipelineMode_ZSD)
        {
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_DEPTHMAPYUV, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_STEREO_DBG, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_JPG_Bokeh, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_JPG_CleanMainImg, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_JPG_JPS, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_BOKEHNODE_RESULTYUV, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_BOKEHNODE_CLEANIMAGEYUV, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_STEREO_DBG_LDC, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL, bufferData.timestamp_P1);
        }
    #if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
        else if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
           getPipelineMode() == PipelineMode_ZSD)
        {
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_DUALYUVNODE_THUMBNAIL, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_STEREO_DBG, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_YUV_JPEG, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL, bufferData.timestamp_P1);
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_JPEG, bufferData.timestamp_P1);
        }
    #endif
        else
        {
            MY_LOGE("should not happened!");
            return UNKNOWN_ERROR;
        }

        if(isDNGEnable)
        {
            sendTimestampToProvider(reqNo, eSTREAMID_IMAGE_PIPE_RAW16, bufferData.timestamp_P1);
        }

        mpPipeline_P2->submitRequest(reqNo, appMeta, halMeta, vDstStreams);
    }

    if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
       getPipelineMode() == PipelineMode_ZSD)
    {
        // DENOISE Only:
        // resume pipeline immediately and no need to wait P2 pipeline done
        // since hardware did not conflict
        mpRequestController_P2->resumePipeline();
        MY_LOGD("DeNoise capture resumePipeline");
    }
    else
    {
        // waitUntilDrained: wait capture done.
        if(mpPipeline_P2!=nullptr)
        {
            mpPipeline_P2->waitUntilDrained();
        }

    }

    if( !(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
       getPipelineMode() == PipelineMode_ZSD) )
    {
        // exclusive denoise case here
        mpRequestController_P2->resumePipeline();
    }

    FUNC_END;
    return OK;
}

/******************************************************************************
 * set callback function
 ******************************************************************************/
MBOOL
StereoFlowControl::
setCallbacks(sp<StereoShot> pShot)
{
    if(pShot!=nullptr)
    {
        mpShot = pShot;
        return MTRUE;
    }
    else
    {
        MY_LOGE("pShot is null");
        return MFALSE;
    }
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
setParameters()
{
    FUNC_START;
    MERROR ret = OK;

    if(mpRequestController_P1 != 0){
        ret = mpRequestController_P1->setParameters( this );
        MY_LOGE_IF(ret != OK, "mpRequestController_P1->setParameters failed!");
    }
    if(mpRequestController_P1_Main2 != 0){
        ret = ret & mpRequestController_P1_Main2->setParameters( mpStereoRequestUpdater_P1_Main2 );
        MY_LOGE_IF(ret != OK, "mpRequestController_P1_Main2->setParameters failed!");
    }
    if(mpRequestController_P2 != 0){
        ret = ret & mpRequestController_P2->setParameters( mpStereoRequestUpdater_P2 );
        MY_LOGE_IF(ret != OK, "mpRequestController_P2->setParameters failed!");
    }

    FUNC_END;
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
sendCommand(
    int32_t cmd,
    int32_t arg1,
    int32_t arg2
)
{
    MERROR ret = INVALID_OPERATION;
    switch(cmd)
    {
        case CONTROL_DNG_FLAG:
            isDNGEnable = arg1;
            break;
        default:
            break;
    }

    if(mpRequestController_P1_Main2 != 0){
        ret = ret & mpRequestController_P1_Main2->sendCommand( cmd, arg1, arg2 );
    }
    if(mpRequestController_P2 != 0){
        ret = ret & mpRequestController_P2->sendCommand( cmd, arg1, arg2 );
    }

    return (mpRequestController_P1 != 0) ?
        mpRequestController_P1->sendCommand( cmd, arg1, arg2 ) : INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
dump(
    int fd,
    Vector<String8>const& args
)
{
#warning "TODO"
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
updateParametersCommon(
    IMetadata* setting,
    sp<IParamsManagerV3> pParamsMgrV3,
    StereoPipelineSensorParam& sensorParam
)
{
    MERROR ret = UNKNOWN_ERROR;

    sp<IParamsManager> paramsMgr = pParamsMgrV3->getParamsMgr();
    if(paramsMgr == NULL){
        MY_LOGE("paramsMgr == NULL");
        return UNKNOWN_ERROR;
    }

    // auto-flicker check
    {
        MtkCameraParameters newParams;

        newParams.unflatten(paramsMgr->flatten());

        const char *abMode = newParams.get(CameraParameters::KEY_ANTIBANDING);

        if(strcmp(abMode, CameraParameters::ANTIBANDING_AUTO) == 0){
            MY_LOGD1("Should not use auto-flicker in stereo mode, force switch to OFF!");
            paramsMgr->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
        }
    }

    // EIS Metadata setting : Preview
    if( mPipelineMode == PipelineMode_ZSD)
    {
        pParamsMgrV3->updateRequestPreview(setting);
    }
    // Record
    else if(mPipelineMode == PipelineMode_RECORDING)
    {
        pParamsMgrV3->updateRequestRecord(setting);
    }

    ret = pParamsMgrV3->updateRequest(setting, sensorParam.mode);

    const char* value = paramsMgr->getStr(MtkCameraParameters::KEY_STEREO_PREVIEW_ENABLE);
    if(::strcmp(value, "") != 0)
    {
        // check if p2 pipeline is enable
        bool isP2Enable = paramsMgr->isEnabled(MtkCameraParameters::KEY_STEREO_PREVIEW_ENABLE);
        if(isP2Enable)
        {
            MY_LOGD("Enable preview");
            if(mpRequestController_P2 != nullptr)
            {
                mpRequestController_P2->resumePipeline();
            }
        }
        else
        {
            MY_LOGD("Disable preview");
            if(mpRequestController_P2 != nullptr)
            {
                BufferList vDstStreams;
                vDstStreams.clear();
                mpRequestController_P2->pausePipeline(vDstStreams);
            }
        }
    }

    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
updateParameters(
    IMetadata* setting
)
{
    MERROR ret = UNKNOWN_ERROR;
    if( mpParamsManagerV3 != 0 )
    {

        ret = updateParametersCommon(setting, getParamsMgrV3(), mSensorParam);

        IMetadata::IEntry entry(MTK_STATISTICS_LENS_SHADING_MAP_MODE);
        entry.push_back(MTK_STATISTICS_LENS_SHADING_MAP_MODE_ON, Type2Type< MUINT8 >());
        setting->update(entry.tag(), entry);
    }
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
updateRequestSetting(
    IMetadata* appSetting,
    IMetadata* halSetting
)
{
    // update app control
    {
        IMetadata::IEntry entry(MTK_SCALER_CROP_REGION);
        entry.push_back(this->getActiveArrayCrop(), Type2Type<MRect>());
        appSetting->update(MTK_SCALER_CROP_REGION, entry);

        // add 3DNR flag
        MINT32 mode3DNR = MTK_NR_FEATURE_3DNR_MODE_OFF;
        if(::strcmp(mpParamsManagerV3->getParamsMgr()->getStr
                    (MtkCameraParameters::KEY_3DNR_MODE),
                    "on") == 0 )
        {
            MY_LOGD1("3DNR on");
            mode3DNR = MTK_NR_FEATURE_3DNR_MODE_ON;
        }
        IMetadata::IEntry entry2(MTK_NR_FEATURE_3DNR_MODE);
        entry2.push_back(mode3DNR, Type2Type< MINT32 >());
        appSetting->update(entry2.tag(), entry2);
    }

    // update hal control
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
        entry.push_back(mSensorParam.size, Type2Type< MSize >());
        halSetting->update(entry.tag(), entry);

        IMetadata::IEntry entry2(MTK_P1NODE_SENSOR_CROP_REGION);
        entry2.push_back(this->getSensorDomainCrop(), Type2Type<MRect>());
        halSetting->update(MTK_P1NODE_SENSOR_CROP_REGION, entry2);

        // Always request exif since stereo is zsd flow
        IMetadata::IEntry entry3(MTK_HAL_REQUEST_REQUIRE_EXIF);
        entry3.push_back(true, Type2Type< MUINT8 >());
        halSetting->update(entry3.tag(), entry3);

        IMetadata::IEntry entry4(MTK_3A_ISP_PROFILE);
        entry4.push_back(this->getIspProfile(), Type2Type< MUINT8 >());
        halSetting->update(entry4.tag(), entry4);
    }

    mpParamsManagerV3->updateRequestHal(halSetting);

    MY_LOGD1("P1 main1 udpate request sensorSize:(%dx%d)  sensorCrop:(%d,%d,%d,%d)",
        mSensorParam.size.w,
        mSensorParam.size.h,
        this->getSensorDomainCrop().p.x,
        this->getSensorDomainCrop().p.y,
        this->getSensorDomainCrop().s.w,
        this->getSensorDomainCrop().s.h
    );

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
submitRequest(
    Vector< SettingSet > rvSettings,
    BufferList           rvDstStreams,
    Vector< MINT32 >&    rvRequestNo
)
{
    status_t ret = UNKNOWN_ERROR;
    if( mpRequestController_P2 == NULL)
    {
        MY_LOGE("mpRequestController_P2 is NULL");
        return UNKNOWN_ERROR;
    }
    //
    Vector< BufferList >  vDstStreams;
    for (size_t i = 0; i < rvSettings.size(); ++i) {
        vDstStreams.push_back(rvDstStreams);
    }
    ret = mpRequestController_P2->submitRequest( rvSettings, vDstStreams, rvRequestNo );

    if( ret != OK)
    {
        MY_LOGE("submitRequest Fail!");
        return UNKNOWN_ERROR;
    }
    //
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
submitRequest(
    Vector< SettingSet > rvSettings,
    Vector< BufferList > rvDstStreams,
    Vector< MINT32 >&    rvRequestNo
)
{
    MY_LOGE("No implementation!");
    return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
StereoFlowControl::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;
    mpRequestController_P1 = NULL;
    if ( mpPipeline_P1 != 0 ) {
        mpPipeline_P1->flush();
        mpPipeline_P1->waitUntilDrained();
        mpPipeline_P1 = NULL;
    }
    mpRequestController_P1_Main2 = NULL;
    if ( mpPipeline_P1_Main2 != 0 ) {
        mpPipeline_P1_Main2->flush();
        mpPipeline_P1_Main2->waitUntilDrained();
        mpPipeline_P1_Main2 = NULL;
    }
    mpRequestController_P2 = NULL;
    if ( mpPipeline_P2 != 0 ) {
        mpPipeline_P2->flush();
        mpPipeline_P2->waitUntilDrained();
        mpPipeline_P2 = NULL;
    }

    exitMMDVFSScenario();

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
checkNotifyCallback(
    IMetadata* meta
)
{
    MY_LOGD1("+");

    // check stereo warning
    {
        MINT32 newResult = -1;
        if(tryGetMetadata<MINT32>(meta, MTK_STEREO_FEATURE_WARNING, newResult)){
            MY_LOGD1("check cb MTK_STEREO_FEATURE_WARNING: %d  ignore:%d", newResult, mbIgnoreStereoWarning);
            if(mCurrentStereoWarning != -1 && newResult != mCurrentStereoWarning){
                // to adapter
                if(mbIgnoreStereoWarning){
                    // do nothing
                }else{
                    MY_LOGD("do cb MTK_STEREO_FEATURE_WARNING: %d", newResult);
                    mpCamMsgCbInfo->doNotifyCallback(
                        MTK_CAMERA_MSG_EXT_NOTIFY,
                        MTK_CAMERA_MSG_EXT_NOTIFY_STEREO_WARNING,
                        newResult
                    );
                }
            }

            mCurrentStereoWarning = newResult;
        }
    }

    MY_LOGD1("-");
    return OK;
}

/******************************************************************************
 * for developing purpose, we can block P2 pipeline by this function
 ******************************************************************************/
MBOOL
StereoFlowControl::
waitP2PrvReady()
{
    MY_LOGD1("+");

    // MY_LOGW("temp block waitP2PrvReady");
    // mbEnableP2Prv = MFALSE;

    Mutex::Autolock _l(mP2PrvLock);
    while(!mbEnableP2Prv){
        MY_LOGD1("wait for mbEnableP2Prv");
        mCondP2PrvLock.wait(mP2PrvLock);
    }

    MY_LOGD1("-");
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
changeToPreviewStatus()
{
    FUNC_START;
    Mutex::Autolock _l(mCaptureLock);

    std::chrono::time_point<std::chrono::system_clock> captureStartTime = mvCaptureRequests.valueAt(0);
    MUINT32 reqId = mvCaptureRequests.keyAt(0);
    mvCaptureRequests.removeItemsAt(0);
    mCondCaptureLock.broadcast();

    std::chrono::duration<double> elapsed_seconds = std::chrono::system_clock::now()-captureStartTime;
    MY_LOGD("Profile: takePicture time(%lf ms) reqID=%d",
        elapsed_seconds.count() *1000,
        reqId
    );

    if(mvCaptureRequests.isEmpty()){
        enterPerformanceScenario(ScenarioMode::PREVIEW);
    }else{
        MY_LOGD("still some capture requests on-going: %d", mvCaptureRequests.size());
        return OK;
    }

    {
        sp<ResultProcessor> pResultProcessor = mpPipeline_P2->getResultProcessor().promote();
        CHECK_OBJECT(pResultProcessor);
        //partial
        pResultProcessor->removeListener(
            STEREO_FLOW_CAPTURE_REQUSET_NUM_START,
            STEREO_FLOW_CAPTURE_REQUSET_NUM_END,
            true,
            mpShot
        );
        //full
        pResultProcessor->removeListener(
            STEREO_FLOW_CAPTURE_REQUSET_NUM_START,
            STEREO_FLOW_CAPTURE_REQUSET_NUM_END,
            false,
            mpShot
        );
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
prepareMetadataAndImageBuffer(
    IMetadata* appSetting,
    IMetadata* halSetting,
    CaptureBufferData* bufferData)
{
    FUNC_START;

    android::sp<IImageBufferHeap> spHeap_full           = nullptr;
    android::sp<IImageBufferHeap> spHeap_resized        = nullptr;
    android::sp<IImageBufferHeap> spHeap_full_main2     = nullptr;
    android::sp<IImageBufferHeap> spHeap_resized_main2  = nullptr;
    Vector<ISelector::MetaItemSet> tempMetadata;
    Vector<ISelector::BufferItemSet> rvBufferSet_resized;
    Vector<ISelector::BufferItemSet> rvBufferSet_full;
    Vector<ISelector::BufferItemSet> rvBufferSet_resized_main2;
    Vector<ISelector::BufferItemSet> rvBufferSet_full_main2;
    IMetadata appDynamic;
    IMetadata appDynamic_main2;

    // step 1
    // getResult from main1_resized
    // appMeta and halMeta also updated by this Selector
    {
        MY_LOGD("getResultCapture from main1_resized  +");
        tempMetadata.clear();
        MINT32 tempReqNo = -1;
#warning "FIXME. [N-Migration]getResultCapture() interface change, just compile pass currently."
        if(bufferData->mpSelector_resized->getResultCapture(tempReqNo, tempMetadata, rvBufferSet_resized) != OK){
            MY_LOGE("mSelector_resized->getResultCapture failed!");
        }
        MBOOL index_appDynamic = MFALSE;
        MBOOL index_halDynamic = MFALSE;
        for ( auto metadata : tempMetadata ){
            switch(metadata.id){
                case eSTREAMID_META_APP_DYNAMIC_P1:
                    appDynamic = metadata.meta;
                    mpShot->addAppMetaData(miCapCounter, appDynamic);
                    index_appDynamic = MTRUE;
                break;
                case eSTREAMID_META_HAL_DYNAMIC_P1:
                    *halSetting = metadata.meta;
                    index_halDynamic = MTRUE;
                break;
                default:
                    MY_LOGE("unexpected meta stream from selector:%d", metadata.id);
                break;
            }
        }
        if(!index_appDynamic || !index_halDynamic)
        {
            MY_LOGE("some meta stream from P1_main1 is not correctly set!"
            );
            return UNKNOWN_ERROR;
        }
        MY_LOGD("getResultCapture from main1_resized  -");
    }
    // step 1.1 get timestamp
    {
        if(tryGetMetadata<MINT64>(&appDynamic, MTK_SENSOR_TIMESTAMP, bufferData->timestamp_P1))
        {
            MY_LOGD("timestamp_P1:%lld", bufferData->timestamp_P1);
        }
        else
        {
            MY_LOGW("Can't get timestamp from mSelectorAppMetadata_main1, set to 0");
            bufferData->timestamp_P1 = 0;
        }
    }
    // step 2
    // getResultCapture from main1_full
    {
        MY_LOGD("getResultCapture from main1_full  +");
        tempMetadata.clear();
        MINT32 tempReqNo = -1;
#warning "FIXME. [N-Migration]getResultCapture() interface change, just compile pass currently."
        if(bufferData->mpSelector_full->getResultCapture(tempReqNo, tempMetadata, rvBufferSet_full) != OK){
            MY_LOGE("mSelector_full->getResultCapture failed!");
        }
        MY_LOGD("getResultCapture from main1_full  -");
    }
    // step 3
    // getResultCapture from main2_resized
    {
        MY_LOGD("getResultCapture from main2_resized  +");
        tempMetadata.clear();
        MINT32 tempReqNo = -1;
#warning "FIXME. [N-Migration]getResultCapture() interface change, just compile pass currently."
        if(bufferData->mpSelector_resized_main2->getResultCapture(tempReqNo, tempMetadata, rvBufferSet_resized_main2) != OK){
            MY_LOGE("mSelector_resized_main2->getResultCapture failed!");
        }
        MBOOL index_appDynamic = MFALSE;
        MBOOL index_halDynamic = MFALSE;
        IMetadata::IEntry entry_meta(MTK_P1NODE_MAIN2_HAL_META);
        for ( auto metadata : tempMetadata ){
            switch(metadata.id){
                case eSTREAMID_META_APP_DYNAMIC_P1_MAIN2:
                    index_appDynamic = MTRUE;
                    appDynamic_main2 = metadata.meta;
                    break;
                case eSTREAMID_META_HAL_DYNAMIC_P1_MAIN2:
                    MY_LOGD("push main2 hal dynamic into hal_meta");
                    entry_meta.push_back(metadata.meta, Type2Type< IMetadata >());
                    halSetting->update(entry_meta.tag(), entry_meta);
                    index_halDynamic = MTRUE;
                    break;
                default:
                    MY_LOGE("unexpected meta stream from selector:%d", metadata.id);
                    break;
            }
        }
        if(!index_appDynamic|| !index_halDynamic)
        {
            MY_LOGE("some meta stream from P1_main2 is not correctly set! (%d ,%d )",
                index_appDynamic,
                index_halDynamic
            );
            return UNKNOWN_ERROR;
        }
        MY_LOGD("getResult from main2_resized  -");
    }

    // step 4
    // getResultCapture from main2_full
    if(bufferData->mpSelector_full_main2 != NULL)
    {
        MY_LOGD1("getResultCapture from mpSelector_full_main2  +");
        tempMetadata.clear();

        MINT32 tempReqNo = -1;
#warning "FIXME. [N-Migration]getResultCapture() interface change, just compile pass currently."
        if(bufferData->mpSelector_full_main2->getResultCapture(tempReqNo, tempMetadata, rvBufferSet_full_main2) != OK){
            MY_LOGE("mpSelector_full_main2->getResultCapture failed!");
        }

        MY_LOGD1("getResultCapture from mpSelector_full_main2  -");
    }

    // step 4
    // get DOF level and push into appSetting
    // temp hard code dof level here
    // will get this value from app
    {
        sp<IParamsManager> mParam = nullptr;
        if (mpParamsManagerV3 != 0) {
            mParam = mpParamsManagerV3->getParamsMgr();
        }
        else
        {
            MY_LOGE("get parameter manager fail");
            return UNKNOWN_ERROR;
        }
        MINT32 bokehLevel = mParam->getInt(MtkCameraParameters::KEY_STEREO_DOF_LEVEL);
        int DoFLevel_HAL = bokehLevel*2;
        int DoFMin_HAL = 0;
        int DoFMax_HAL = 30;

        DoFLevel_HAL = std::max(DoFMin_HAL, std::min(DoFLevel_HAL, DoFMax_HAL));

        MY_LOGD("Bokeh level(%d)", DoFLevel_HAL);
        IMetadata::IEntry entry(MTK_STEREO_FEATURE_DOF_LEVEL);
        entry.push_back(DoFLevel_HAL, Type2Type< MINT32 >());
        appSetting->update(entry.tag(), entry);
    }
    {
        // force using hw to encode all jpeg
        IMetadata::IEntry entry(MTK_JPG_ENCODE_TYPE);
        entry.push_back(NSCam::NSIoPipe::NSSImager::JPEGENC_HW_ONLY, Type2Type< MUINT8 >());
        halSetting->update(entry.tag(), entry);
    }
    {
        // for capture, force turn off 3dnr.
        IMetadata::IEntry entry(MTK_NR_FEATURE_3DNR_MODE);
        entry.push_back(MFALSE, Type2Type< MINT32 >());
        appSetting->update(entry.tag(), entry);
    }
    {
        // add capture intent
        IMetadata::IEntry entry(MTK_CONTROL_CAPTURE_INTENT);
        entry.push_back(MTK_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG, Type2Type< MUINT8 >());
        appSetting->update(entry.tag(), entry);
    }
    {
        sp<IParamsManager> mParam = nullptr;
        if (mpParamsManagerV3 != 0) {
            mParam = mpParamsManagerV3->getParamsMgr();
        }
        else
        {
            MY_LOGE("get parameter manager fail");
            return UNKNOWN_ERROR;
        }
        String8 sString = mParam->getStr(MtkCameraParameters::KEY_STEREO_TOUCH_POSITION);
        // String8 sString = String8("-110,120"); for test
        const char *strPosX = sString.string();
        const char *strPosY = strPosX ? ::strchr(strPosX, ',') : NULL;
        MINT32  PosX = 0, PosY = 0;
        if(strPosX != NULL && strPosY != NULL) {
            PosX = ::atoi(strPosX);
            PosY = ::atoi(strPosY+1);
            if(PosX < -1000 || PosX > 1000 || PosY < -1000 || PosY > 1000) {
                PosX = 0;
                PosY = 0;
            }
        }
        MY_LOGD1("Get touch pos from parameter(x, y): %d, %d", PosX, PosY);
        IMetadata::IEntry entry(MTK_STEREO_FEATURE_TOUCH_POSITION);
        entry.push_back(PosX, Type2Type< MINT32 >());
        entry.push_back(PosY, Type2Type< MINT32 >());
        appSetting->update(entry.tag(), entry);
    }
	// get image buffer
	{
		//
		/*bufferData->mpIMGOBuf_Main1 = spHeap_full->createImageBuffer();
		bufferData->mpRRZOBuf_Main1 = spHeap_resized->createImageBuffer();
		bufferData->mpRRZOBuf_Main2 = spHeap_resized_main2->createImageBuffer();
        CHECK_OBJECT(bufferData->mpIMGOBuf_Main1);
        CHECK_OBJECT(bufferData->mpRRZOBuf_Main1);
        CHECK_OBJECT(bufferData->mpRRZOBuf_Main2);*/
        // if we setBuffer again, then available buffer will increase.
        mpStereoBufferPool_OPAQUE->setBuffer("spHeap_full", spHeap_full);
        mpStereoBufferPool_RESIZER->setBuffer("spHeap_resized", spHeap_resized);
        mpStereoBufferPool_RESIZER_MAIN2->setBuffer("spHeap_resized_main2", spHeap_resized_main2);
        if(mpStereoBufferPool_OPAQUE_MAIN2 != nullptr){
            mpStereoBufferPool_OPAQUE_MAIN2->setBuffer("spHeap_full_main2", spHeap_full_main2);
        }
	}

#if 1
    // Debug Log
    {
        MSize tempSize;
        if( ! tryGetMetadata<MSize>(const_cast<IMetadata*>(halSetting), MTK_HAL_REQUEST_SENSOR_SIZE, tempSize) ){
            MY_LOGE("cannot get MTK_HAL_REQUEST_SENSOR_SIZE after updating request");
        }else{
            MY_LOGD("MTK_HAL_REQUEST_SENSOR_SIZE:(%dx%d)", tempSize.w, tempSize.h);
        }
        IMetadata tempMetadata;
        if( ! tryGetMetadata<IMetadata>(const_cast<IMetadata*>(halSetting), MTK_P1NODE_MAIN2_HAL_META, tempMetadata) ){
            MY_LOGE("cannot get MTK_P1NODE_MAIN2_HAL_META after updating request");
        }else{
            MY_LOGD("MTK_P1NODE_MAIN2_HAL_META");
        }
        MINT32 tempLevel;
        if( ! tryGetMetadata<MINT32>(const_cast<IMetadata*>(appSetting), MTK_STEREO_FEATURE_DOF_LEVEL, tempLevel) ){
            MY_LOGE("cannot get MTK_STEREO_FEATURE_DOF_LEVEL after updating request");
        }else{
            MY_LOGD("MTK_STEREO_FEATURE_DOF_LEVEL:%d", tempLevel);
        }
        MINT32 tempTransform;
        if( ! tryGetMetadata<MINT32>(const_cast<IMetadata*>(appSetting), MTK_JPEG_ORIENTATION, tempTransform) ){
            MY_LOGE("cannot get MTK_JPEG_ORIENTATION after updating request");
        }else{
            MY_LOGD("MTK_JPEG_ORIENTATION:%d", tempTransform);
        }

        MINT64 timestamp_main1 = -1;
        MINT64 timestamp_main2 = -1;
        int timestamp_main1_ms = -1;
        int timestamp_main2_ms = -1;
        int timestamp_diff = -1;
        if( ! tryGetMetadata<MINT64>(&appDynamic, MTK_SENSOR_TIMESTAMP, timestamp_main1)){
            MY_LOGE("cannot get timestamp_main1 after updating request");
        }else{
            // MY_LOGD("capture timestamp_main1:%lld", timestamp_main1);
            timestamp_main1_ms = timestamp_main1/1000000;
        }
        if( ! tryGetMetadata<MINT64>(&appDynamic_main2, MTK_SENSOR_TIMESTAMP, timestamp_main2)){
            MY_LOGE("cannot get timestamp_main2 after updating request");
        }else{
            // MY_LOGD("capture timestamp_main2:%lld", timestamp_main2);
            timestamp_main2_ms = timestamp_main2/1000000;
        }
        timestamp_diff     = timestamp_main1_ms - timestamp_main2_ms;
        MY_LOGD("capture_ts_diff:(main1/main2/diff)(%09d/%09d/%09d)(ms)",
            timestamp_main1_ms,
            timestamp_main2_ms,
            timestamp_diff
        );
    }
#endif
    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
getSelector(
    MINT32 sensorId,
    MINT32 streamId,
    sp<StereoSelector>& pSelector
)
{
    pSelector = mpStereoBufferSynchronizer->querySelector(streamId);

    if(pSelector!=nullptr){
        return OK;
    }else{
        MY_LOGE("pSelector is null");
        return UNKNOWN_ERROR;
    }
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
buildCaptureStream(
BufferList& vDstStreams, MBOOL isDng)
{
    CAM_TRACE_NAME("buildCaptureStream");

    // stream
    auto addDstStreams = [&vDstStreams](MINT32 streamId, MBOOL bNeedProvider)
    {
        vDstStreams.push_back(
            BufferSet{
                .streamId       = streamId,
                .criticalBuffer = bNeedProvider,
            }
        );
    };

    MY_LOGD("stereo capture with vsdof");
    // metadata
    addDstStreams(eSTREAMID_META_APP_DYNAMIC_DEPTH, MFALSE);
    addDstStreams(eSTREAMID_META_HAL_DYNAMIC_DEPTH, MFALSE);
    addDstStreams(eSTREAMID_META_HAL_DYNAMIC_BOKEH, MFALSE);
    addDstStreams(eSTREAMID_META_APP_DYNAMIC_BOKEH, MFALSE);
    addDstStreams(eSTREAMID_META_APP_DYNAMIC_DUALIT, MFALSE);
    addDstStreams(eSTREAMID_META_APP_DYNAMIC_BOKEH_JPG, MFALSE);
    addDstStreams(eSTREAMID_META_APP_DYNAMIC_JPEG, MFALSE);
    addDstStreams(eSTREAMID_META_APP_DYNAMIC_JPS, MFALSE);
    // image
    //addDstStreams(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_MY_SYUV, MFALSE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_DMBGYUV, MFALSE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_MAINIMAGE_CAPYUV, MFALSE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_JPSMAIN1YUV, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_JPSMAIN2YUV, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_DEPTHMAPYUV, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_STEREO_DBG_LDC, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_STEREO_DBG, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_DUALIMAGETRANSFORM_JPSYUV, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_BOKEHNODE_CLEANIMAGEYUV, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_BOKEHNODE_RESULTYUV, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_JPG_Bokeh, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_JPG_CleanMainImg, MTRUE);
    addDstStreams(eSTREAMID_IMAGE_PIPE_JPG_JPS, MTRUE);
    if(isDng)
    {
        addDstStreams(eSTREAMID_IMAGE_PIPE_RAW16, MTRUE);
    }
    MY_LOGD("vDstStreams.size(%d)", vDstStreams.size());
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
MERROR
StereoFlowControl::
buildCaptureStream_DeNoise(BufferList& vDstStreams, MBOOL isDng)
{
    CAM_TRACE_NAME("buildCaptureStream_DeNoise");

    // stream
    auto addDstStreams = [&vDstStreams](MINT32 streamId, MBOOL bNeedProvider)
    {
        vDstStreams.push_back(
            BufferSet{
                .streamId       = streamId,
                .criticalBuffer = bNeedProvider,
            }
        );
    };

    {
        MY_LOGD(" build denoise capture stream");
        // metadata
        addDstStreams(eSTREAMID_META_HAL_DYNAMIC_DEPTH, MFALSE);
        addDstStreams(eSTREAMID_META_APP_DYNAMIC_DEPTH, MFALSE);
        addDstStreams(eSTREAMID_META_HAL_DYNAMIC_BMDENOISE, MFALSE);
        addDstStreams(eSTREAMID_META_APP_DYNAMIC_BMDENOISE, MFALSE);
        addDstStreams(eSTREAMID_META_APP_DYNAMIC_JPEG, MFALSE);
        // image
        addDstStreams(eSTREAMID_IMAGE_PIPE_DUALYUVNODE_THUMBNAIL, MTRUE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_STEREO_DBG, MTRUE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_BMDEPTHMAPNODE_DISPARITY_L, MFALSE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_BMDEPTHMAPNODE_DISPARITY_R, MFALSE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_BMDENOISE_WAPING_MATRIX, MFALSE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_YUV_JPEG, MTRUE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL, MTRUE);
        addDstStreams(eSTREAMID_IMAGE_JPEG, MTRUE);

        if(isDng)
        {
            addDstStreams(eSTREAMID_IMAGE_PIPE_RAW16, MTRUE);
        }
    }

    MY_LOGD("vDstStreams.size(%d)", vDstStreams.size());
    return OK;
}
#endif
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
buildCaptureStream_3rdParty(BufferList& vDstStreams, MBOOL isDng)
{
    CAM_TRACE_NAME("buildCaptureStream_3rdParty");

    // stream
    auto addDstStreams = [&vDstStreams](MINT32 streamId, MBOOL bNeedProvider)
    {
        vDstStreams.push_back(
            BufferSet{
                .streamId       = streamId,
                .criticalBuffer = bNeedProvider,
            }
        );
    };

    {
        MY_LOGD(" build 3rd party capture stream");
        // metadata
        addDstStreams(eSTREAMID_META_HAL_DYNAMIC_DUALYUV, MFALSE);
        addDstStreams(eSTREAMID_META_APP_DYNAMIC_DUALYUV, MFALSE);
        // image
        addDstStreams(eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN1YUV, MTRUE);
        addDstStreams(eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN2YUV, MTRUE);

        if(isDng)
        {
            addDstStreams(eSTREAMID_IMAGE_PIPE_RAW16, MTRUE);
        }
    }

    MY_LOGD("vDstStreams.size(%d)", vDstStreams.size());
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
setCaptureBufferProvoider(JpegParam jpegParam, ShotParam shotParam, MBOOL isDng)
{
    CAM_TRACE_NAME("setCaptureBufferProvoider");

    // for capture scenario, it needs to prepare 5 pools.
    // ExtraDebug, DepthMap, JPS, Bokeh result, Clean image.
    sp<IImageStreamInfo> extraDebugStreamInfo = nullptr;
    sp<IImageStreamInfo> depthStreamInfo = nullptr;
    sp<IImageStreamInfo> jpsStreamInfo = nullptr;
    sp<IImageStreamInfo> bokehResultStreamInfo = nullptr;
    sp<IImageStreamInfo> bokehCleanImageStreamInfo = nullptr;
    sp<IImageStreamInfo> jpgBokehImageStreamInfo = nullptr;
    sp<IImageStreamInfo> jpgCleanImageStreamInfo = nullptr;
    sp<IImageStreamInfo> thumbImageStreamInfo = nullptr;
    sp<IImageStreamInfo> raw16ImageStreamInfo = nullptr;
    sp<IImageStreamInfo> ldcImageStreamInfo = nullptr;
    sp<IImageStreamInfo> jpsMain1ImageStreamInfo = nullptr;
    sp<IImageStreamInfo> jpsMain2ImageStreamInfo = nullptr;
    sp<IImageStreamInfo> jpsOutImageStreamInfo = nullptr;
    //
    StereoSizeProvider* pStereoSizeProvider = StereoSizeProvider::getInstance();
    MSize depthMap = StereoSizeProvider::getInstance()->getBufferSize(
                                    E_DEPTH_MAP,
                                    eSTEREO_SCENARIO_CAPTURE).size;

    MSize finalDepthMapSize = depthMap;
    MSize finalPictureSize = MSize(shotParam.mi4PictureWidth, shotParam.mi4PictureHeight);
    if(shotParam.mu4Transform & eTransform_ROT_90){
        finalDepthMapSize = MSize(depthMap.h, depthMap.w);
        finalPictureSize = MSize(shotParam.mi4PictureHeight, shotParam.mi4PictureWidth);
    }
    // get ldc size
    MSize szLDC = StereoSizeProvider::getInstance()->getBufferSize(E_LDC).size;
    MY_LOGD("final capture size: depthmap%dx%d, picture:%dx%d, LDC:%dx%d",
        finalDepthMapSize.w,
        finalDepthMapSize.h,
        finalPictureSize.w,
        finalPictureSize.h,
        szLDC.w,
        szLDC.h
    );

    MSize szMainIMGO;
    MUINT32 junkStride;
    MRect tgCropRect;
    if(isDng)
    {
        StereoSizeProvider::getInstance()->getPass1Size( eSTEREO_SENSOR_MAIN1,
            eImgFmt_BAYER10,
            EPortIndex_IMGO,
            eSTEREO_SCENARIO_CAPTURE,
            //below are outputs
            tgCropRect,
            szMainIMGO,
            junkStride);
        MY_LOGD("final IMGO raw capture size: %dx%d",
            szMainIMGO.w,
            szMainIMGO.h
        );
    }


    // ExtraDebug
    {
        extraDebugStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:ExtraData")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_STEREO_DBG,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_STA_BYTE,
                                              MSize(StereoSettingProvider::getExtraDataBufferSizeInBytes(), 1),
                                              0);
    }
    // LDC
    {
        ldcImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:LDCData")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_STEREO_DBG_LDC,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_STA_BYTE,
                                              szLDC,
                                              0);
    }
    // JPS
    {
        jpsStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:JpegEnc_JPS")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_JPG_JPS,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_BLOB,
                                              pStereoSizeProvider->getSBSImageSize(),
                                              0);
    }
    // YUV JPS image size
    {
        jpsOutImageStreamInfo = createImageStreamInfo((std::string("Hal:Image:DualIT_JPS")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_DUALIMAGETRANSFORM_JPSYUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE,
                                              eImgFmt_YUY2,
                                              pStereoSizeProvider->getSBSImageSize(),
                                              0);
    }
    // thumbnail
    {
        thumbImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:Bokeh_Thumb")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_YV12,
                                              MSize(jpegParam.mi4JpegThumbWidth, jpegParam.mi4JpegThumbHeight),
                                              0);
    }
    // depthmap
    {
        depthStreamInfo =
                createImageStreamInfo((std::string("Hal:Image:DepthMap")+std::to_string(miCapCounter)).c_str(),
                                      eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_DEPTHMAPYUV,
                                      eSTREAMTYPE_IMAGE_INOUT,
                                      1,
                                      1,
                                      eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE,
                                      eImgFmt_STA_BYTE,
                                      finalDepthMapSize,
                                      0);
    }
    // bokehResult
    {
        bokehResultStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:BokehResult")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_BOKEHNODE_RESULTYUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE,
                                              eImgFmt_NV21,
                                              finalPictureSize,
                                              0);
    }
    // bokehClean
    {
        bokehCleanImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:CleanImage")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_BOKEHNODE_CLEANIMAGEYUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE,
                                              eImgFmt_NV21,
                                              finalPictureSize,
                                              0);
    }
    // jpgBokeh
    {
        jpgBokehImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:JpegEnc_Bokeh")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_JPG_Bokeh,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_BLOB,
                                              finalPictureSize,
                                              0);
    }
    // jpgClean
    {
        jpgCleanImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:JpegEnc_Clean")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_JPG_CleanMainImg,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_BLOB,
                                              finalPictureSize,
                                              0);
    }
    // jpsMain1
    {
        Pass2SizeInfo pass2SizeInfo;
        StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_2, eSTEREO_SCENARIO_CAPTURE, pass2SizeInfo);
        jpsMain1ImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:JPSMain1")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_JPSMAIN1YUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_YV12,
                                              pass2SizeInfo.areaWDMA.size,
                                              0);
    }
    // jpsMain2
    {
        Pass2SizeInfo pass2SizeInfo;
        StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_P_2, eSTEREO_SCENARIO_CAPTURE, pass2SizeInfo);
        jpsMain2ImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:JPSMain2")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_JPSMAIN2YUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_YV12,
                                              pass2SizeInfo.areaWDMA.size,
                                              0);
    }
    // RAW16
    if(isDng){
        raw16ImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:Raw16")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_RAW16,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_RAW16,
                                              MSize(szMainIMGO.w, szMainIMGO.h),
                                              0);
    }

    // create pool
    // need to refactory!
    allocProvider(extraDebugStreamInfo,       mpCallbackHandler);
    allocProvider(jpsStreamInfo,              mpCallbackHandler);
    allocProvider(depthStreamInfo,            mpCallbackHandler);
    allocProvider(jpgBokehImageStreamInfo,    mpCallbackHandler);
    allocProvider(jpgCleanImageStreamInfo,    mpCallbackHandler);
    allocProvider(ldcImageStreamInfo,    mpCallbackHandler);
    allocProvider(thumbImageStreamInfo);
    allocProvider(bokehResultStreamInfo);
    allocProvider(bokehCleanImageStreamInfo);
    allocProvider(jpsMain1ImageStreamInfo);
    allocProvider(jpsMain2ImageStreamInfo);
    allocProvider(jpsOutImageStreamInfo);
    if(isDng)
    {
        allocProvider(raw16ImageStreamInfo,    mpCallbackHandler);
    }
    // set callback set to callback manager
    list<StreamId_T> vDstStreams;
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_STEREO_DBG);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_JPG_JPS);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_DEPTHMAPYUV);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_JPG_Bokeh);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_JPG_CleanMainImg);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_STEREO_DBG_LDC);
    mpShot->setDstStream(miCapCounter, vDstStreams);

    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
setCaptureBufferProvoider_DeNoise(
    JpegParam jpegParam,
    ShotParam shotParam,
    MBOOL isDng,
    MBOOL needReAllocate,
    MINT32 bufferCount)
{
    CAM_TRACE_NAME("setCaptureBufferProvoider_DeNoise");

    sp<IImageStreamInfo> YUVThumbStreamInfo = nullptr;
    sp<IImageStreamInfo> extraDebugStreamInfo = nullptr;
    sp<IImageStreamInfo> DeNoiseResultStreamInfo = nullptr;
    sp<IImageStreamInfo> DeNoiseThumbStreamInfo = nullptr;
    sp<IImageStreamInfo> JpegEncStreamInfo = nullptr;
    sp<IImageStreamInfo> raw16ImageStreamInfo = nullptr;
    //
    StereoSizeProvider* pStereoSizeProvider = StereoSizeProvider::getInstance();

    MSize finalPictureSize = MSize(shotParam.mi4PictureWidth, shotParam.mi4PictureHeight);
    MSize YUVThumbnailSize = MSize(shotParam.mi4PostviewWidth, shotParam.mi4PostviewHeight);
    if(shotParam.mu4Transform & eTransform_ROT_90){
        finalPictureSize = MSize(shotParam.mi4PictureHeight, shotParam.mi4PictureWidth);
        YUVThumbnailSize = MSize(shotParam.mi4PostviewHeight, shotParam.mi4PostviewWidth);
    }

    MSize szMainIMGO;
    MUINT32 junkStride;
    MRect tgCropRect;
    if(isDng)
    {
        StereoSizeProvider::getInstance()->getPass1Size( eSTEREO_SENSOR_MAIN1,
            eImgFmt_BAYER10,
            EPortIndex_IMGO,
            eSTEREO_SCENARIO_CAPTURE,
            //below are outputs
            tgCropRect,
            szMainIMGO,
            junkStride);
        MY_LOGD("final IMGO raw capture size: %dx%d",
            szMainIMGO.w,
            szMainIMGO.h
        );
    }

    // YUVThumbnail
    {
        YUVThumbStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:YUVThumbnail")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_DUALYUVNODE_THUMBNAIL,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              bufferCount,
                                              bufferCount,
                                              eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              shotParam.miPostviewDisplayFormat, //iPostviewThumbFmt,
                                              YUVThumbnailSize,
                                              0);
    }
    // ExtraDebug
    {
        extraDebugStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:ExtraData")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_STEREO_DBG,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              bufferCount,
                                              bufferCount,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_STA_BYTE,
                                              MSize(StereoSettingProvider::getExtraDataBufferSizeInBytes(), 1),
                                              0);
    }
    // DeNoiseResult
    {
        DeNoiseResultStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:DeNoiseResult")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_YUV_JPEG,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              bufferCount,
                                              bufferCount,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE,
                                              eImgFmt_NV21,
                                              finalPictureSize,
                                              0);
    }
    // DeNoiseThumb
    {
        DeNoiseThumbStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:DenoiseThumb")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              bufferCount,
                                              bufferCount,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_YV12,
                                              MSize(jpegParam.mi4JpegThumbWidth, jpegParam.mi4JpegThumbHeight),
                                              0);
    }
    // JpegEnc
    {
        JpegEncStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:JpegEnc")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_JPEG,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              bufferCount,
                                              bufferCount,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_BLOB,
                                              finalPictureSize,
                                              0);
    }

    // RAW16
    if(isDng)
    {
        raw16ImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:Raw16")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_RAW16,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              bufferCount,
                                              bufferCount,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_RAW16,
                                              MSize(szMainIMGO.w, szMainIMGO.h),
                                              0);
    }

    if(needReAllocate){
        MY_LOGD("Re-Allocate buffer providers");
        // create pool
        allocProvider(YUVThumbStreamInfo,         mpCallbackHandler);
        allocProvider(extraDebugStreamInfo,       mpCallbackHandler);
        allocProvider(JpegEncStreamInfo,         mpCallbackHandler);
        allocProvider(DeNoiseResultStreamInfo);
        allocProvider(DeNoiseThumbStreamInfo);
        if(isDng)
        {
            allocProvider(raw16ImageStreamInfo,    mpCallbackHandler);
        }
    }

    // set callback set to callback manager
    list<StreamId_T> vDstStreams;
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_DUALYUVNODE_THUMBNAIL);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_STEREO_DBG);
    vDstStreams.push_back(eSTREAMID_IMAGE_JPEG);
    mpShot->setDstStream(miCapCounter, vDstStreams);
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
setCaptureBufferProvoider_3rdParty(JpegParam jpegParam, ShotParam shotParam, MBOOL isDng)
{
    CAM_TRACE_NAME("setCaptureBufferProvoider_3rdParty");

    sp<IImageStreamInfo> Main1YUVStreamInfo = nullptr;
    sp<IImageStreamInfo> Main2YUVStreamInfo = nullptr;
    sp<IImageStreamInfo> raw16ImageStreamInfo = nullptr;
    //
    StereoSizeProvider* pStereoSizeProvider = StereoSizeProvider::getInstance();

    MSize finalPictureSize = MSize(shotParam.mi4PictureWidth, shotParam.mi4PictureHeight);
    if(shotParam.mu4Transform & eTransform_ROT_90){
        finalPictureSize = MSize(shotParam.mi4PictureHeight, shotParam.mi4PictureWidth);
    }

    MSize szMainIMGO;
    MUINT32 junkStride;
    MRect tgCropRect;
    if(isDng)
    {
        StereoSizeProvider::getInstance()->getPass1Size( eSTEREO_SENSOR_MAIN1,
            eImgFmt_BAYER10,
            EPortIndex_IMGO,
            eSTEREO_SCENARIO_CAPTURE,
            //below are outputs
            tgCropRect,
            szMainIMGO,
            junkStride);
        MY_LOGD("final IMGO raw capture size: %dx%d",
            szMainIMGO.w,
            szMainIMGO.h
        );
    }


    // Main1YUV
    {
        Main1YUVStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:Main1YUV")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN1YUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_NV21,
                                              finalPictureSize,
                                              0);
    }
    // Main2YUV
    {
        Main2YUVStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:Main2YUV")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN2YUV,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_NV21,
                                              finalPictureSize,
                                              0);
    }

    // RAW16
    if(isDng)
    {
        raw16ImageStreamInfo =
                        createImageStreamInfo((std::string("Hal:Image:Raw16")+std::to_string(miCapCounter)).c_str(),
                                              eSTREAMID_IMAGE_PIPE_RAW16,
                                              eSTREAMTYPE_IMAGE_INOUT,
                                              1,
                                              1,
                                              eBUFFER_USAGE_SW_READ_OFTEN |eBUFFER_USAGE_HW_CAMERA_READWRITE| eBUFFER_USAGE_SW_WRITE_OFTEN,
                                              eImgFmt_RAW16,
                                              MSize(szMainIMGO.w, szMainIMGO.h),
                                              0);
    }

    // create pool
    allocProvider(Main1YUVStreamInfo,         mpCallbackHandler);
    allocProvider(Main2YUVStreamInfo,         mpCallbackHandler);
    if(isDng)
    {
        allocProvider(raw16ImageStreamInfo,    mpCallbackHandler);
    }
    // set callback set to callback manager
    list<StreamId_T> vDstStreams;
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN1YUV);
    vDstStreams.push_back(eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN2YUV);
    mpShot->setDstStream(miCapCounter, vDstStreams);
    return OK;
}
/******************************************************************************
 * Enter MMDVFS Scenario
 ******************************************************************************/
MERROR
StereoFlowControl::
enterMMDVFSScenario()
{
    FUNC_START;

    BWC BwcIns;
    BwcIns.Profile_Change(BWCPT_CAMERA_ZSD,true);

    mmdvfs_set(
        BWCPT_CAMERA_ZSD,
        MMDVFS_CAMERA_MODE_STEREO, 1,
        MMDVFS_PARAMETER_EOF
    );

    // Query mmdvfs level for HWSync configuration
    mmdvfs_step_enum step;
    mbDVFSLevel = MFALSE;
    step = mmdvfs_query(
                        BWCPT_CAMERA_ZSD,
                        MMDVFS_CAMERA_MODE_STEREO, mbDVFSLevel,
                        MMDVFS_PARAMETER_EOF
                        );
    if(step == mmdvfs_step_enum::MMDVFS_STEP_LOW){
        mbDVFSLevel = MFALSE;
    }else
    if(step == mmdvfs_step_enum::MMDVFS_STEP_HIGH ||
       step == mmdvfs_step_enum::MMDVFS_STEP_HIGH2HIGH
    ){
        mbDVFSLevel = MTRUE;
    }else{
        MY_LOGE("unknown dvfs step value:%d", step);
    }

    MY_LOGD("Current DVFS level is %d", mbDVFSLevel);

    FUNC_END;
    return OK;
}
/******************************************************************************
 * Exit MMDVFS Scenario
 ******************************************************************************/
MERROR
StereoFlowControl::
exitMMDVFSScenario()
{
    FUNC_START;

    BWC BwcIns;
    BwcIns.Profile_Change(BWCPT_CAMERA_ZSD,false);

    FUNC_END;
    return OK;
}
/******************************************************************************
 * Enter scenario control with stereo flag on
 ******************************************************************************/
MERROR
StereoFlowControl::
enterPerformanceScenario(ScenarioMode newScenario)
{
    FUNC_START;

    if(mpScenarioCtrl != 0){
        if(mCurrentScenarioMode == newScenario){
            MY_LOGD("enterPerformanceScenario twice with same scenario, ignored");
            FUNC_END;
            return OK;
        }else{
            MY_LOGD("enterPerformanceScenario newScenario scenario, exitPerformanceScenario first");
            exitPerformanceScenario();
        }
    }

    if(newScenario == NONE){
        MY_LOGE("not supproting this scenarioMode:%d", newScenario);
        FUNC_END;
        return OK;
    }

    StereoPipelineSensorParam sensorParam_ScenarioControl;
    prepareSensor(getOpenId(), sensorParam_ScenarioControl);

    MUINT featureFlagStereo = 0;

    if (mCurrentStereoMode == E_STEREO_FEATURE_DENOISE){
        if(newScenario == ScenarioMode::CAPTURE){
            MY_LOGD("denoise mode in capture, just change mCurrentScenarioMode to newScenario, the real CPU scenario is controlled by BMDeNoiseNode");
            mCurrentScenarioMode = newScenario;
            FUNC_END;
            return OK;
        }else{
            MY_LOGD("denoise mode");
            FEATURE_CFG_ENABLE_MASK(featureFlagStereo, IScenarioControl::FEATURE_BMDENOISE_PREVIEW);
        }
    }else{
        if(newScenario == ScenarioMode::CAPTURE){
            MY_LOGD("vsdof mode + high perf");
            FEATURE_CFG_ENABLE_MASK(featureFlagStereo, IScenarioControl::FEATURE_STEREO_CAPTURE);
        }else if(newScenario == ScenarioMode::PREVIEW){
            MY_LOGD("vsdof/3rd party preview mode");
            FEATURE_CFG_ENABLE_MASK(featureFlagStereo, IScenarioControl::FEATURE_VSDOF_PREVIEW);
        }
        else{
            MY_LOGD("vsdof/3rd party record mode");
            FEATURE_CFG_ENABLE_MASK(featureFlagStereo, IScenarioControl::FEATURE_VSDOF_RECORD);
        }
    }

    IScenarioControl::ControlParam controlParam;
    controlParam.scenario = getScenario();
    controlParam.sensorSize = sensorParam_ScenarioControl.size;
    controlParam.sensorFps = sensorParam_ScenarioControl.fps;
    controlParam.featureFlag = featureFlagStereo;
    controlParam.enableBWCControl = MFALSE;

    mpScenarioCtrl = IScenarioControl::create(getOpenId());
    if( mpScenarioCtrl == NULL )
    {
        MY_LOGE("get Scenario Control fail");
        return UNKNOWN_ERROR;
    }

    mpScenarioCtrl->enterScenario(controlParam);

    mCurrentScenarioMode = newScenario;

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
exitPerformanceScenario()
{
    FUNC_START;

    if(mpScenarioCtrl != 0){
        mpScenarioCtrl->exitScenario();
        mpScenarioCtrl = NULL;
    }else{
        MY_LOGE("Can't get scenario control when exitPerformanceScenario? should not have happened!");
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::
setBufferPoolsReturnMode(MINT32 returnMode)
{
    MY_LOGD("returnMode:%d", returnMode);
    if(mpStereoBufferPool_RESIZER != nullptr){
        mpStereoBufferPool_RESIZER->setReturnMode(returnMode);
    }
    if(mpStereoBufferPool_RESIZER_MAIN2 != nullptr){
        mpStereoBufferPool_RESIZER_MAIN2->setReturnMode(returnMode);
    }
    if(mpStereoBufferPool_OPAQUE != nullptr){
        mpStereoBufferPool_OPAQUE->setReturnMode(returnMode);
    }
    if(mpStereoBufferPool_OPAQUE_MAIN2 != nullptr){
        mpStereoBufferPool_OPAQUE_MAIN2->setReturnMode(returnMode);
    }
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MERROR
StereoFlowControl::
allocProvider(
    sp<IImageStreamInfo> pImgStreamInfo,
    sp<BufferCallbackHandler> pCallbackHandler)
{
    MY_LOGD("name(%s) streamId(0x%x) size(%dx%d)", pImgStreamInfo->getStreamName(), pImgStreamInfo->getStreamId(), pImgStreamInfo->getImgSize().w, pImgStreamInfo->getImgSize().h);
    if(pImgStreamInfo!=nullptr)
    {
        sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
        sp<CallbackBufferPool> pPool = new CallbackBufferPool(pImgStreamInfo);
        pPool->allocateBuffer(
                pImgStreamInfo->getStreamName(),
                pImgStreamInfo->getMaxBufNum(),
                pImgStreamInfo->getMinInitBufNum());
        pCallbackHandler->setBufferPool(pPool);
        pFactory->setImageStreamInfo(pImgStreamInfo);
        pFactory->setUsersPool(pPool);
        //
        sp<StreamBufferProvider> provider = pFactory->create(mOpenId_P2Prv, MTRUE);
        mpImageStreamManager->updateBufProvider(
                                        pImgStreamInfo->getStreamId(),
                                        provider,
                                        Vector<StreamId_T>());
    }
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MERROR
StereoFlowControl::
allocProvider(
    sp<IImageStreamInfo> pImgStreamInfo)
{
    MY_LOGD("name(%s) streamId(0x%x) size(%dx%d)", pImgStreamInfo->getStreamName(), pImgStreamInfo->getStreamId(), pImgStreamInfo->getImgSize().w, pImgStreamInfo->getImgSize().h);

    if(pImgStreamInfo!=nullptr){
        sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
        sp< BufferPoolImp > pPool = new BufferPoolImp(pImgStreamInfo);
        pPool->allocateBuffer(
            pImgStreamInfo->getStreamName(),
            pImgStreamInfo->getMaxBufNum(),
            pImgStreamInfo->getMinInitBufNum());
        pFactory->setImageStreamInfo(pImgStreamInfo);
        pFactory->setUsersPool(pPool);
        //
        sp<StreamBufferProvider> provider = pFactory->create(mOpenId_P2Prv, MTRUE);
        mpImageStreamManager->updateBufProvider(
                                        pImgStreamInfo->getStreamId(),
                                        provider,
                                        Vector<StreamId_T>());
    }
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
StereoFlowControl::
sendTimestampToProvider(
    MUINT32 requestNo,
    StreamId_T id,
    MINT64 timestamp)
{
    sp<IResourceContainer> pResourceContainierCap = IResourceContainer::getInstance(mOpenId_P2Prv);
    sp<StreamBufferProvider> provider =
                        pResourceContainierCap->queryConsumer(id);
    if(provider==nullptr)
    {
        MY_LOGE("get provider fail. streamId(0x%x)", id);
        return MFALSE;
    }
    provider->doTimestampCallback(requestNo, MFALSE, timestamp);
    return MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MERROR
StereoFlowControl::
uninitPipelineAndRelatedResource()
{
    if(mbUsingThreadInit)
    {
        pthread_join(mPipelineInitThreadHandle, NULL);
    }
    // stop P2's pipeline
    if( mpRequestController_P2 != 0 ){
        mpRequestController_P2->stopPipeline();
        mpRequestController_P2 = NULL;
    }

    // stop P1's pipeline
    if( mpRequestController_P1 != 0 ){
        mpRequestController_P1->stopPipeline();
        mpRequestController_P1 = NULL;
    }

    // stop P1_main2's pipeline
    if( mpRequestController_P1_Main2 != 0 ){
        mpRequestController_P1_Main2->stopPipeline();
        mpRequestController_P1_Main2 = NULL;
    }

    {
        if(mpPipeline_P2!=nullptr){
            mpPipeline_P2->flush();
            mpPipeline_P2->waitUntilDrained();
        }
        if ( mpPipeline_P1 != 0 ){
            mpPipeline_P1->flush();
            mpPipeline_P1->waitUntilDrained();
        }
        if ( mpPipeline_P1_Main2 != 0 ){
            mpPipeline_P1_Main2->flush();
            mpPipeline_P1_Main2->waitUntilDrained();
        }

        if(mpStereoBufferSynchronizer != 0)
        {
            mpStereoBufferSynchronizer->flush();
        }

        if ( mpPipeline_P2 != 0 ){
            mpPipeline_P2 = NULL;
            // no one will use these buffer pool anymore
            mpStereoBufferPool_RESIZER = NULL;
            mpStereoBufferPool_RESIZER_MAIN2 = NULL;
            mpStereoBufferPool_OPAQUE = NULL;
            mpStereoBufferPool_OPAQUE_MAIN2 = NULL;

            mpStereoRequestUpdater_P2 = NULL;
        }
        if ( mpPipeline_P1 != 0 ){
            mpPipeline_P1 = NULL;

            mpStereoSelector_RESIZER = NULL;
            mpStereoSelector_OPAQUE = NULL;
        }
        if ( mpPipeline_P1_Main2 != 0 ){
            mpPipeline_P1_Main2 = NULL;

            mpStereoSelector_RESIZER_MAIN2 = NULL;
            mpStereoSelector_OPAQUE_MAIN2 = NULL;

            mpStereoRequestUpdater_P1_Main2 = NULL;
        }
    }

#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    if(mpBMDN_HAL){
        mpBMDN_HAL->destroyInstance();
    }
#endif

    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MERROR
StereoFlowControl::
doSynchronizerBinding()
{
    FUNC_START;
    if(mpStereoSelector_RESIZER != nullptr){
        MY_LOGD("bind stream/selector : eSTREAMID_IMAGE_PIPE_RAW_RESIZER");
        sp<StreamBufferProvider> provider = mpResourceContainier->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_RESIZER);

        if(provider == nullptr){
            MY_LOGE("provider is nullptr!");
            return BAD_VALUE;
        }

        provider->setSelector(mpStereoSelector_RESIZER);
        mpStereoBufferSynchronizer->addStream(eSTREAMID_IMAGE_PIPE_RAW_RESIZER,     mpStereoSelector_RESIZER);
        mpStereoRequestUpdater_P2->addSelector(mpStereoSelector_RESIZER,            StereoRequestUpdater::StreamType::RESIZED);
        mpStereoRequestUpdater_P2->addPool(mpStereoBufferPool_RESIZER,              StereoRequestUpdater::StreamType::RESIZED);
        mpStereoBufferPool_RESIZER->setSynchronizer(mpStereoBufferSynchronizer);
    }

    if(mpStereoSelector_OPAQUE != nullptr){
        MY_LOGD("bind stream/selector : eSTREAMID_IMAGE_PIPE_RAW_OPAQUE");
        sp<StreamBufferProvider> provider = mpResourceContainier->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);

        if(provider == nullptr){
            MY_LOGE("provider is nullptr!");
            return BAD_VALUE;
        }

        provider->setSelector(mpStereoSelector_OPAQUE);
        mpStereoBufferSynchronizer->addStream(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE,      mpStereoSelector_OPAQUE);
        mpStereoRequestUpdater_P2->addSelector(mpStereoSelector_OPAQUE,             StereoRequestUpdater::StreamType::FULL);
        mpStereoRequestUpdater_P2->addPool(mpStereoBufferPool_OPAQUE,               StereoRequestUpdater::StreamType::FULL);
        mpStereoBufferPool_OPAQUE->setSynchronizer(mpStereoBufferSynchronizer);
    }

    if(mpStereoSelector_RESIZER_MAIN2 != nullptr){
        MY_LOGD("bind stream/selector : eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01");
        sp<StreamBufferProvider> provider        = mpResourceContainierMain2->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01);

        if(provider == nullptr){
            MY_LOGE("provider is nullptr!");
            return BAD_VALUE;
        }

        provider->setSelector(mpStereoSelector_RESIZER_MAIN2);
        mpStereoBufferSynchronizer->addStream(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01,      mpStereoSelector_RESIZER_MAIN2);
        mpStereoRequestUpdater_P2->addSelector(mpStereoSelector_RESIZER_MAIN2,          StereoRequestUpdater::StreamType::RESIZED_MAIN2);
        mpStereoRequestUpdater_P2->addPool(mpStereoBufferPool_RESIZER_MAIN2,            StereoRequestUpdater::StreamType::RESIZED_MAIN2);
        mpStereoBufferPool_RESIZER_MAIN2->setSynchronizer(mpStereoBufferSynchronizer);
    }

    if(mpStereoSelector_OPAQUE_MAIN2 != nullptr){
        MY_LOGD("bind stream/selector : eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01");
        sp<StreamBufferProvider> provider        = mpResourceContainierMain2->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01);

        if(provider == nullptr){
            MY_LOGE("provider is nullptr!");
            return BAD_VALUE;
        }

        provider->setSelector(mpStereoSelector_OPAQUE_MAIN2);
        mpStereoBufferSynchronizer->addStream(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01,    mpStereoSelector_OPAQUE_MAIN2);
        mpStereoRequestUpdater_P2->addSelector(mpStereoSelector_OPAQUE_MAIN2,        StereoRequestUpdater::StreamType::FULL_MAIN2);
        mpStereoRequestUpdater_P2->addPool(mpStereoBufferPool_OPAQUE_MAIN2,            StereoRequestUpdater::StreamType::FULL_MAIN2);
        mpStereoBufferPool_OPAQUE_MAIN2->setSynchronizer(mpStereoBufferSynchronizer);
    }

    FUNC_END;
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
StereoFlowControl::
setRequsetTypeForAllPipelines(MINT32 type)
{
    MY_LOGD("new requestType:%d", type);

    if( mpRequestController_P2 != 0 ){
        mpRequestController_P2->setRequestType(type);
    }

    if( mpRequestController_P1 != 0 ){
        mpRequestController_P1->setRequestType(type);
    }

    if( mpRequestController_P1_Main2 != 0 ){
        mpRequestController_P1_Main2->setRequestType(type);
    }
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
StereoFlowControl::
needReAllocBufferProvider(StereoShotParam shotParam)
{
    // bool equalily = true;
    // equalily = equalily && (mCurrentStereoShotParam.mbDngMode == shotParam.mbDngMode);
    // MY_LOGD("1:%d", equalily);
    // equalily = equalily && (mCurrentStereoShotParam.miDOFLevel == shotParam.miDOFLevel);
    // MY_LOGD("2:%d", equalily);
    // equalily = equalily && (mCurrentStereoShotParam.mShotParam.mi4PictureWidth == shotParam.mShotParam.mi4PictureWidth);
    // MY_LOGD("3:%d", equalily);
    // equalily = equalily && (mCurrentStereoShotParam.mShotParam.mi4PictureHeight == shotParam.mShotParam.mi4PictureHeight);
    // MY_LOGD("4:%d", equalily);
    // equalily = equalily && (mCurrentStereoShotParam.mShotParam.mu4Transform == shotParam.mShotParam.mu4Transform);
    // MY_LOGD("5:%d", equalily);
    // equalily = equalily && (mCurrentStereoShotParam.mJpegParam.mi4JpegThumbWidth == shotParam.mJpegParam.mi4JpegThumbWidth);
    // MY_LOGD("6:%d", equalily);
    // equalily = equalily && (mCurrentStereoShotParam.mJpegParam.mi4JpegThumbHeight == shotParam.mJpegParam.mi4JpegThumbHeight);
    // MY_LOGD("7:%d", equalily);
    return !(mCurrentStereoShotParam == shotParam);
}

/******************************************************************************
 *
 ******************************************************************************/
sp<ImageStreamInfo>
StereoFlowControl::
createImageStreamInfo(
    char const*         streamName,
    StreamId_T          streamId,
    MUINT32             streamType,
    size_t              maxBufNum,
    size_t              minInitBufNum,
    MUINT               usageForAllocator,
    MINT                imgFormat,
    MSize const&        imgSize,
    MUINT32             transform
)
{
        IImageStreamInfo::BufPlanes_t bufPlanes;
    #define addBufPlane(planes, height, stride)                                      \
            do{                                                                      \
                size_t _height = (size_t)(height);                                   \
                size_t _stride = (size_t)(stride);                                   \
                IImageStreamInfo::BufPlane bufPlane= { _height * _stride, _stride }; \
                planes.push_back(bufPlane);                                          \
            }while(0)
        switch( imgFormat ) {
            case eImgFmt_YV12:
                addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
                addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w >> 1);
                addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w >> 1);
                break;
            case eImgFmt_NV21:
                addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
                addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w);
                break;
            case eImgFmt_RAW16:
            case eImgFmt_YUY2:
                addBufPlane(bufPlanes , imgSize.h      , imgSize.w << 1);
                break;
            case eImgFmt_Y8:
                addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
            case eImgFmt_STA_BYTE:
                addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
                break;
            case eImgFmt_RGBA8888:
                addBufPlane(bufPlanes , imgSize.h      , imgSize.w<<2);
                break;
            case eImgFmt_BLOB:
                        /*
                        add 328448 for image size
                        standard exif: 1280 bytes
                        4 APPn for debug exif: 0xFF80*4 = 65408*4 bytes
                        max thumbnail size: 64K bytes
                        */
                addBufPlane(bufPlanes , 1              , (imgSize.w * imgSize.h * 12 / 10) + 328448); //328448 = 64K+1280+65408*4
                break;
            default:
                MY_LOGE("format not support yet %p", imgFormat);
                break;
        }
    #undef  addBufPlane

        sp<ImageStreamInfo>
            pStreamInfo = new ImageStreamInfo(
                    streamName,
                    streamId,
                    streamType,
                    maxBufNum, minInitBufNum,
                    usageForAllocator, imgFormat, imgSize, bufPlanes, transform
                    );

        if( pStreamInfo == NULL ) {
            MY_LOGE("create ImageStream failed, %s, %#" PRIxPTR ,
                    streamName, streamId);
        }

        return pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
StereoFlowControl::
getSensorModuleType(
)
{
    MINT32 value = 0;
    // get main1 sensor format
    MUINT sensorRawFmtType_Main1 = StereoSettingProvider::getSensorRawFormat(getOpenId());
    // get main2 sensor format
    MUINT sensorRawFmtType_Main2 = StereoSettingProvider::getSensorRawFormat(getOpenId_Main2());

    if(sensorRawFmtType_Main1 == SENSOR_RAW_Bayer &&
       sensorRawFmtType_Main2 == SENSOR_RAW_MONO)
    {
        MY_LOGD("Sensor module is B+M");
        value = BAYER_AND_MONO;
    }
    else if(sensorRawFmtType_Main1 == SENSOR_RAW_Bayer &&
       sensorRawFmtType_Main2 == SENSOR_RAW_Bayer)
    {
        MY_LOGD("Sensor module is B+B");
        value = BAYER_AND_BAYER;
    }
    else
    {
        MY_LOGE("unspport sensor module type");
    }
    return value;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
StereoFlowControl::
getStereoMode(
)
{
    MINT32 value = 0;
    if(::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(
            MtkCameraParameters::KEY_STEREO_REFOCUS_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable stereo capture");
        value |= E_STEREO_FEATURE_CAPTURE;
    }
    if(::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(
            MtkCameraParameters::KEY_STEREO_VSDOF_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable vsdof");
        value |= E_STEREO_FEATURE_VSDOF;
    }
    if(::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(
            MtkCameraParameters::KEY_STEREO_DENOISE_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable denoise");
        value |= E_STEREO_FEATURE_DENOISE;
    }
    if(::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(
            MtkCameraParameters::KEY_STEREO_3RDPARTY_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable 3party");
        value |= E_STEREO_FEATURE_THIRD_PARTY;
    }
    MY_LOGD("Stereo mode(%d)", value);
    return value;
}
/******************************************************************************
 *
 ******************************************************************************/
void*
StereoFlowControl::
doThreadInit(void* arg)
{
    StereoFlowControl* pSelf = reinterpret_cast<StereoFlowControl*>(arg);
    pSelf->buildStereoP2Pipeline();
    pSelf->buildStereoP1Pipeline_Main2();
    return NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
buildStereoPipeline(
    MINT32 stereoMode,
    MINT32 sensorModuleType,
    MINT32 pipelineMode
)
{
    status_t ret = UNKNOWN_ERROR;
    if(mCurrentSensorModuleType == UNSUPPORTED_SENSOR_MODULE)
    {
        MY_LOGE("invalid operation.");
        goto lbExit;
    }
    if(mPipelineMode == PipelineMode_RECORDING)
    {
        // main1
        mpStereoSelector_RESIZER            = new StereoSelector("RESIZER");
        // main2
        if(mCurrentSensorModuleType == BAYER_AND_MONO){
            mpStereoSelector_OPAQUE_MAIN2 = new StereoSelector("OPAQUE_01");
        }else{
            mpStereoSelector_RESIZER_MAIN2 = new StereoSelector("RESIZER_01");
        }
    }
    else
    {
        // main1
        mpStereoSelector_RESIZER            = new StereoSelector("RESIZER");
        mpStereoSelector_OPAQUE             = new StereoSelector("OPAQUE");
        // main2
        mpStereoSelector_RESIZER_MAIN2 = new StereoSelector("RESIZER_01");

        if(mCurrentSensorModuleType == BAYER_AND_MONO){
            mpStereoSelector_OPAQUE_MAIN2 = new StereoSelector("OPAQUE_01");
        }
    }
    //
    if(STEREO_3RDPARTY == (stereoMode) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGD("STEREO_3RDPARTY: get 3rd party prv_cap table");
        ret = get_3rdParty_Content(mCurrentPipelineContent);
    }
    else if(STEREO_BB_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGD("get bb prv_cap table");
        ret = get_BB_Prv_Cap_Content(mCurrentPipelineContent);
    }
    else if(STEREO_BB_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_RECORDING)
    {
        MY_LOGD("get bb rec table");
        ret = get_BB_Rec_Content(mCurrentPipelineContent);
    }
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    else if(STEREO_BM_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGE("unsupport mode: STEREO_BM_PRV_CAP_REC, PipelineMode_ZSD");
        MY_LOGW("temp let it go, get bm prv_cap table");
        ret = get_BM_Denoise_Content(mCurrentPipelineContent);
    }
    else if(STEREO_BM_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_RECORDING)
    {
        MY_LOGE("unsupport mode: STEREO_BM_PRV_CAP_REC, PipelineMode_RECORDING");
    }
    else if(STEREO_BM_DENOISE == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGD("STEREO_BM_DENOISE: get bm prv_cap table");
        ret = get_BM_Denoise_Content(mCurrentPipelineContent);
    }
#endif
    else
    {
        MY_LOGE("should not happened!");
        goto lbExit;
    }
    // after get table finish, init flow
    if( pthread_create(&mPipelineInitThreadHandle, NULL, doThreadInit, this) != 0 )
    {
        MY_LOGE("pthread create failed, using original method to init");
        mbUsingThreadInit = MFALSE;
        buildStereoP2Pipeline();
        buildStereoP1Pipeline_Main2();
    }
    else
    {
        mbUsingThreadInit = MTRUE;
    }
    //MY_LOGE("P1 +");
    buildStereoP1Pipeline_Main1();
    //MY_LOGE("P1 -");
    // wait p2 & main1_p1 pipeline construct finished.
    if(mbUsingThreadInit)
        pthread_join(mPipelineInitThreadHandle, NULL);
lbExit:
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
get_BB_Prv_Cap_Content(
    StereoModeContextBuilderContent &content
)
{
    // pass1 main1 content
    content.mP1Main1Content.metaTable = StereoPipelineMgrData::p1::zsd::gStereoMetaTbl_ZsdP1;
    content.mP1Main1Content.nodeConfigData = StereoPipelineMgrData::p1::zsd::gStereoConnectSetting_ZsdP1;
    content.mP1Main1Content.imageTable = StereoPipelineMgrData::p1::zsd::gStereoImgStreamTbl_ZsdP1;

    // pass1 main2 content
    content.mP1Main2Content.metaTable = StereoPipelineMgrData::p1::zsd::gStereoMetaTbl_ZsdP1Main2;
    content.mP1Main2Content.nodeConfigData = StereoPipelineMgrData::p1::zsd::gStereoConnectSetting_ZsdP1Main2;
    content.mP1Main2Content.imageTable = StereoPipelineMgrData::p1::zsd::gStereoImgStreamTbl_ZsdP1Main2_RRZO;

    // pass2 contents
    content.mFullContent.metaTable = StereoPipelineMgrData::p2::PrvCap::gStereoMetaTbl_P2PrvCap;
    content.mFullContent.nodeConfigData = StereoPipelineMgrData::p2::PrvCap::gStereoP2ConnectSetting_P2PrvCap;
    content.mFullContent.imageTable = StereoPipelineMgrData::p2::PrvCap::gStereoImgStreamTbl_P2PrvCap;

    content.mPrvContent.metaTable = StereoPipelineMgrData::p2::prv::gStereoMetaTbl_P2Prv;
    content.mPrvContent.nodeConfigData = StereoPipelineMgrData::p2::prv::gStereoP2ConnectSetting_P2Prv;
    content.mPrvContent.imageTable = StereoPipelineMgrData::p2::prv::gStereoImgStreamTbl_P2Prv_Main2_RRZO;

    content.mCapContent.metaTable = StereoPipelineMgrData::p2::cap::gStereoMetaTbl_P2Cap;
    content.mCapContent.nodeConfigData = StereoPipelineMgrData::p2::cap::gStereoP2ConnectSetting_P2Cap;
    content.mCapContent.imageTable = StereoPipelineMgrData::p2::cap::gStereoImgStreamTbl_P2Cap_Main2_RRZO;

    content.mDngCapContent.metaTable = StereoPipelineMgrData::p2::dngCap::gStereoMetaTbl_P2DNGCap;
    content.mDngCapContent.nodeConfigData = StereoPipelineMgrData::p2::dngCap::gStereoP2ConnectSetting_P2DNGCap;
    content.mDngCapContent.imageTable = StereoPipelineMgrData::p2::dngCap::gStereoImgStreamTbl_P2DNGCap;

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
get_BB_Rec_Content(
    StereoModeContextBuilderContent &content
)
{
    content.mP1Main1Content.metaTable = StereoPipelineMgrData::p1::VdoRec::gStereoMetaTbl_VdoP1;
    content.mP1Main1Content.nodeConfigData = StereoPipelineMgrData::p1::VdoRec::gStereoConnectSetting_VdoP1;
    content.mP1Main1Content.imageTable = StereoPipelineMgrData::p1::VdoRec::gStereoImgStreamTbl_VdoP1;

    content.mP1Main2Content.metaTable = StereoPipelineMgrData::p1::VdoRec::gStereoMetaTbl_VdoP1Main2;
    content.mP1Main2Content.nodeConfigData = StereoPipelineMgrData::p1::VdoRec::gStereoConnectSetting_VdoP1Main2;
    content.mP1Main2Content.imageTable = StereoPipelineMgrData::p1::VdoRec::gStereoImgStreamTbl_VdoP1Main2_RRZO;

    content.mFullContent.metaTable = StereoPipelineMgrData::p2::VdoRec::gStereoMetaTbl_P2VdoRec;
    content.mFullContent.nodeConfigData = StereoPipelineMgrData::p2::VdoRec::gStereoP2ConnectSetting_P2VdoRec;
    content.mFullContent.imageTable = StereoPipelineMgrData::p2::VdoRec::gStereoImgStreamTbl_P2VdoRec;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
status_t
StereoFlowControl::
get_BM_Denoise_Content(
    StereoModeContextBuilderContent &content
)
{
    // pass1 main1 content
    content.mP1Main1Content.metaTable = StereoPipelineMgrData::p1::BM_ZSD_Denoise::gStereoMetaTbl_ZsdP1;
    content.mP1Main1Content.nodeConfigData = StereoPipelineMgrData::p1::BM_ZSD_Denoise::gStereoConnectSetting_ZsdP1;
    content.mP1Main1Content.imageTable = StereoPipelineMgrData::p1::BM_ZSD_Denoise::gStereoImgStreamTbl_ZsdP1;

    // pass1 main2 content
    content.mP1Main2Content.metaTable = StereoPipelineMgrData::p1::BM_ZSD_Denoise::gStereoMetaTbl_ZsdP1Main2;
    content.mP1Main2Content.nodeConfigData = StereoPipelineMgrData::p1::BM_ZSD_Denoise::gStereoConnectSetting_ZsdP1Main2;
    content.mP1Main2Content.imageTable = StereoPipelineMgrData::p1::BM_ZSD_Denoise::gStereoImgStreamTbl_ZsdP1Main2_BOTH;

    // pass2 contents
    content.mFullContent.metaTable = StereoPipelineMgrData::p2::BM_PrvCap_Denoise::gStereoMetaTbl_P2PrvCap;
    content.mFullContent.nodeConfigData = StereoPipelineMgrData::p2::BM_PrvCap_Denoise::gStereoP2ConnectSetting_P2PrvCap;
    content.mFullContent.imageTable = StereoPipelineMgrData::p2::BM_PrvCap_Denoise::gStereoImgStreamTbl_P2PrvCap;

    content.mPrvContent.metaTable = StereoPipelineMgrData::p2::BM_Prv_Denoise::gStereoMetaTbl_P2Prv;
    content.mPrvContent.nodeConfigData = StereoPipelineMgrData::p2::BM_Prv_Denoise::gStereoP2ConnectSetting_P2Prv;
    content.mPrvContent.imageTable = StereoPipelineMgrData::p2::BM_Prv_Denoise::gStereoImgStreamTbl_P2Prv;

    content.mCapContent.metaTable = StereoPipelineMgrData::p2::BM_Cap_Denoise::gStereoMetaTbl_P2Cap;
    content.mCapContent.nodeConfigData = StereoPipelineMgrData::p2::BM_Cap_Denoise::gStereoP2ConnectSetting_P2Cap;
    content.mCapContent.imageTable = StereoPipelineMgrData::p2::BM_Cap_Denoise::gStereoImgStreamTbl_P2Cap;

    content.mDngCapContent.metaTable = StereoPipelineMgrData::p2::BM_DngCap_Denoise::gStereoMetaTbl_P2DNGCap;
    content.mDngCapContent.nodeConfigData = StereoPipelineMgrData::p2::BM_DngCap_Denoise::gStereoP2ConnectSetting_P2DNGCap;
    content.mDngCapContent.imageTable = StereoPipelineMgrData::p2::BM_DngCap_Denoise::gStereoImgStreamTbl_P2DNGCap;
    return OK;
}

#endif
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
get_3rdParty_Content(
    StereoModeContextBuilderContent &content
)
{
    // pass1 main1 content
    content.mP1Main1Content.metaTable = StereoPipelineMgrData::p1::ZSD_3rdParty::gStereoMetaTbl_ZsdP1;
    content.mP1Main1Content.nodeConfigData = StereoPipelineMgrData::p1::ZSD_3rdParty::gStereoConnectSetting_ZsdP1;
    content.mP1Main1Content.imageTable = StereoPipelineMgrData::p1::ZSD_3rdParty::gStereoImgStreamTbl_ZsdP1;

    // pass1 main2 content
    content.mP1Main2Content.metaTable = StereoPipelineMgrData::p1::ZSD_3rdParty::gStereoMetaTbl_ZsdP1Main2;
    content.mP1Main2Content.nodeConfigData = StereoPipelineMgrData::p1::ZSD_3rdParty::gStereoConnectSetting_ZsdP1Main2;
    content.mP1Main2Content.imageTable = StereoPipelineMgrData::p1::ZSD_3rdParty::gStereoImgStreamTbl_ZsdP1Main2_BOTH;

    // pass2 contents
    content.mFullContent.metaTable = StereoPipelineMgrData::p2::PrvCap_3rdParty::gStereoMetaTbl_P2PrvCap;
    content.mFullContent.nodeConfigData = StereoPipelineMgrData::p2::PrvCap_3rdParty::gStereoP2ConnectSetting_P2PrvCap;
    content.mFullContent.imageTable = StereoPipelineMgrData::p2::PrvCap_3rdParty::gStereoImgStreamTbl_P2PrvCap;

    content.mPrvContent.metaTable = StereoPipelineMgrData::p2::Prv_3rdParty::gStereoMetaTbl_P2Prv;
    content.mPrvContent.nodeConfigData = StereoPipelineMgrData::p2::Prv_3rdParty::gStereoP2ConnectSetting_P2Prv;
    content.mPrvContent.imageTable = StereoPipelineMgrData::p2::Prv_3rdParty::gStereoImgStreamTbl_P2Prv;

    content.mCapContent.metaTable = StereoPipelineMgrData::p2::Cap_3rdParty::gStereoMetaTbl_P2Cap;
    content.mCapContent.nodeConfigData = StereoPipelineMgrData::p2::Cap_3rdParty::gStereoP2ConnectSetting_P2Cap;
    content.mCapContent.imageTable = StereoPipelineMgrData::p2::Cap_3rdParty::gStereoImgStreamTbl_P2Cap;

    content.mDngCapContent.metaTable = StereoPipelineMgrData::p2::DngCap_3rdParty::gStereoMetaTbl_P2DNGCap;
    content.mDngCapContent.nodeConfigData = StereoPipelineMgrData::p2::DngCap_3rdParty::gStereoP2ConnectSetting_P2DNGCap;
    content.mDngCapContent.imageTable = StereoPipelineMgrData::p2::DngCap_3rdParty::gStereoImgStreamTbl_P2DNGCap;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
queryPass1ActiveArrayCrop()
{
    if(!StereoSizeProvider::getInstance()->getPass1ActiveArrayCrop(StereoHAL::eSTEREO_SENSOR_MAIN1, mActiveArrayCrop)){
        MY_LOGE("can't get active arrayCrop from StereoSizeProvider for eSTEREO_SENSOR_MAIN1");
        return BAD_VALUE;
    }
    if(!StereoSizeProvider::getInstance()->getPass1ActiveArrayCrop(StereoHAL::eSTEREO_SENSOR_MAIN2, mActiveArrayCrop_Main2)){
        MY_LOGE("can't get active arrayCrop from StereoSizeProvider for eSTEREO_SENSOR_MAIN2");
        return BAD_VALUE;
    }

    MY_LOGD("StereoSizeProvider => active array crop main1(%d,%d,%dx%d), main2(%d,%d,%dx%d)",
        mActiveArrayCrop.p.x,
        mActiveArrayCrop.p.y,
        mActiveArrayCrop.s.w,
        mActiveArrayCrop.s.h,
        mActiveArrayCrop_Main2.p.x,
        mActiveArrayCrop_Main2.p.y,
        mActiveArrayCrop_Main2.s.w,
        mActiveArrayCrop_Main2.s.h
    );
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
createStereoRequestController()
{
    FUNC_START;
    mpRequestController_P1 = IRequestController::createInstance(
                                                "mpRequestController_P1",
                                                getOpenId(),
                                                mpCamMsgCbInfo,
                                                mpParamsManagerV3
                                            );
    if(mpRequestController_P1 == nullptr)
    {
        MY_LOGE("mpRequestController_P1 create fail.");
        goto lbExit;
    }
    mpRequestController_P1_Main2 = IRequestController::createInstance(
                                            "mpRequestController_P1_Main2",
                                            getOpenId_Main2(),
                                            mpCamMsgCbInfo,
                                            mpParamsManagerV3
                                        );
    if(mpRequestController_P1_Main2 == nullptr)
    {
        MY_LOGE("mpRequestController_P1_Main2 create fail.");
        goto lbExit;
    }
    mpRequestController_P2 = IRequestController::createInstance(
                                            "mpRequestController",
                                            getOpenId(), // dont use getOpenId_P2Prv() or RequestSettingBuilder will go wrong
                                            mpCamMsgCbInfo,
                                            mpParamsManagerV3
                                        );
    if(mpRequestController_P2 == nullptr)
    {
        MY_LOGE("mpRequestController_P2 create fail.");
        goto lbExit;
    }
    MY_LOGD("mpRequestController_P1=%p", mpRequestController_P1.get());
    MY_LOGD("mpRequestController_P1_Main2=%p", mpRequestController_P1_Main2.get());
    MY_LOGD("mpRequestController_P2=%p", mpRequestController_P2.get());
    FUNC_END;
    return OK;
lbExit:
    mpRequestController_P1 = nullptr;
    mpRequestController_P1_Main2 = nullptr;
    mpRequestController_P2 = nullptr;
    return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
setAndStartStereoSynchronizer()
{
    FUNC_START;

    if(getPipelineMode() == PipelineMode_ZSD){
        if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType)){
            mpStereoBufferSynchronizer = StereoBufferSynchronizer::createInstance("Synchronizer", get_stereo_bmdenoise_zsd_cap_stored_frame_cnt());
        }else{
            mpStereoBufferSynchronizer = StereoBufferSynchronizer::createInstance("Synchronizer", get_stereo_zsd_cap_stored_frame_cnt());
        }
    }else{
        mpStereoBufferSynchronizer = StereoBufferSynchronizer::createInstance("Synchronizer", 0);
    }

    // create stereoRequestUpdater
    MY_LOGD("create stereoRequestUpdater");
    mpStereoRequestUpdater_P2 = new StereoRequestUpdater(this, mpStereoBufferSynchronizer, mpParamsManagerV3);
    mpStereoRequestUpdater_P1_Main2 = new StereoMain2RequestUpdater(this, mpStereoBufferSynchronizer, mpParamsManagerV3);

    mpStereoRequestUpdater_P2->setSensorParams(mSensorParam, mSensorParam_main2);
    if(STEREO_3RDPARTY == mCurrentStereoMode && getPipelineMode() == PipelineMode_ZSD)
    {
        mpStereoRequestUpdater_P2->setPreviewStreamId(eSTREAMID_IMAGE_PIPE_YUV_00);
        mpStereoRequestUpdater_P2->setFdStreamId(eSTREAMID_IMAGE_YUV_FD);
    }
    else if(STEREO_BM_DENOISE == (mCurrentStereoMode|mCurrentSensorModuleType) &&
       getPipelineMode() == PipelineMode_ZSD)
    {
        mpStereoRequestUpdater_P2->setPreviewStreamId(eSTREAMID_IMAGE_PIPE_YUV_00);
        mpStereoRequestUpdater_P2->setFdStreamId(eSTREAMID_IMAGE_YUV_FD);
    }
    else
    {
        mpStereoRequestUpdater_P2->setPreviewStreamId(eSTREAMID_IMAGE_PIPE_BOKEHNODE_PREVIEWYUV);
        mpStereoRequestUpdater_P2->setFdStreamId(eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_FDYUV);
    }
    mpStereoRequestUpdater_P1_Main2->setSensorParams(mSensorParam, mSensorParam_main2);

    // bind stream/selector pair to synchronizer
    if(doSynchronizerBinding() != OK){
        MY_LOGE("Cannot doSynchronizerBinding. start preview fail.");
        return BAD_VALUE;
    }

    // set stereo buffer pool return mode
    setBufferPoolsReturnMode(StereoBufferPool::RETURN_MODE::TO_SYNCHRONIZER);

    mpStereoBufferSynchronizer->start();
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
startStereoPipeline(
    MINT32 request_num_start,
    MINT32 request_num_end
)
{
    FUNC_START;
    MERROR startPipelineResult = OK;
    {
        MERROR ret = mpRequestController_P1->startPipeline(
                                    request_num_start,
                                    request_num_end,
                                    mpPipeline_P1,
                                    this
                                );
        if(ret != OK){
            MY_LOGE("startPipeline mpRequestController_P1 failed!");
            startPipelineResult = UNKNOWN_ERROR;
        }
    }
    {
        MERROR ret = mpRequestController_P1_Main2->startPipeline(
                                    request_num_start,
                                    request_num_end,
                                    mpPipeline_P1_Main2,
                                    mpStereoRequestUpdater_P1_Main2
                                );
        if(ret != OK){
            MY_LOGE("startPipeline mpRequestController_P1_Main2 failed!");
            startPipelineResult = UNKNOWN_ERROR;
        }
    }
    {
        MERROR ret = mpRequestController_P2->startPipeline(
                                    request_num_start,
                                    request_num_end,
                                    mpPipeline_P2,
                                    mpStereoRequestUpdater_P2,
                                    LegacyPipelineMode_T::PipelineMode_Feature_StereoZSD
                                );
        if(ret != OK){
            MY_LOGE("startPipeline mpRequestController_P2 failed!");
            startPipelineResult = UNKNOWN_ERROR;
        }
    }
    FUNC_END;
    return startPipelineResult;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
buildStereoP1Pipeline(
    ContextBuilderContent p1Main1Content,
    ContextBuilderContent p1Main2Content,
    MBOOL bRecording,
    MBOOL bMain2MONO
)
{
    if(bRecording)
    {
        // main1
        mpStereoSelector_RESIZER            = new StereoSelector("RESIZER");
        // main2
        if(bMain2MONO){
            mpStereoSelector_OPAQUE_MAIN2 = new StereoSelector("OPAQUE_01");
        }else{
            mpStereoSelector_RESIZER_MAIN2 = new StereoSelector("RESIZER_01");
        }
    }
    else
    {
        // main1
        mpStereoSelector_RESIZER            = new StereoSelector("RESIZER");
        mpStereoSelector_OPAQUE             = new StereoSelector("OPAQUE");
        // main2
        mpStereoSelector_RESIZER_MAIN2 = new StereoSelector("RESIZER_01");

        if(bMain2MONO){
            mpStereoSelector_OPAQUE_MAIN2 = new StereoSelector("OPAQUE_01");
        }
    }

    mpPipeline_P1 = constructP1Pipeline_Main1(
        p1Main1Content
    );

    mpPipeline_P1_Main2 = constructP1Pipeline_Main2(
        p1Main2Content
    );
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
buildStereoP1Pipeline_Main1(
)
{
    mpPipeline_P1 = constructP1Pipeline_Main1(
        mCurrentPipelineContent.mP1Main1Content
    );
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
buildStereoP1Pipeline_Main2(
)
{
    mpPipeline_P1_Main2 = constructP1Pipeline_Main2(
        mCurrentPipelineContent.mP1Main2Content
    );
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoFlowControl::
buildStereoP2Pipeline()
{
    status_t ret = UNKNOWN_ERROR;
    MUINT32 stereoMode = mCurrentStereoMode;
    MUINT32 pipelineMode = mPipelineMode;
    MUINT32 sensorModuleType = mCurrentSensorModuleType;
    //
    if(STEREO_3RDPARTY == (stereoMode) &&
       pipelineMode == PipelineMode_ZSD)
    {
        mpPipeline_P2 = constructP2Pipeline_PrvCap_3rdParty(
            mCurrentPipelineContent,
            mpImageStreamManager
        );
    }
    else if(STEREO_BB_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGD("get bb prv_cap table");
        mpPipeline_P2 = constructP2Pipeline_PrvCap_BB(
                            mCurrentPipelineContent,
                            mpImageStreamManager
                     );
    }
    else if(STEREO_BB_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_RECORDING)
    {
        MY_LOGD("get bb rec table");
        mpPipeline_P2 = constructP2Pipeline_Rec_BB(
                            mCurrentPipelineContent
                     );
    }
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    else if(STEREO_BM_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGE("unsupport mode: STEREO_BM_PRV_CAP_REC, PipelineMode_ZSD");
        MY_LOGW("temp let it go, get bm prv_cap table");
        mpPipeline_P2 = constructP2Pipeline_PrvCap_BM(
            mCurrentPipelineContent,
            mpImageStreamManager
        );
    }
    else if(STEREO_BM_PRV_CAP_REC == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_RECORDING)
    {
        MY_LOGE("unsupport mode: STEREO_BM_PRV_CAP_REC, PipelineMode_RECORDING");
    }
    else if(STEREO_BM_DENOISE == (stereoMode|sensorModuleType) &&
       pipelineMode == PipelineMode_ZSD)
    {
        MY_LOGD("STEREO_BM_DENOISE: get bm prv_cap table");

        if( pthread_create(&mThreadHandle, NULL, doParrallelInit, this) != 0 )
        {
            MY_LOGE("pthread create failed");
            goto lbExit;
        }
        mpPipeline_P2 = constructP2Pipeline_PrvCap_BM(
            mCurrentPipelineContent,
            mpImageStreamManager
        );

        MY_LOGD("wait thread init done +");
        if( pthread_join(mThreadHandle, NULL) != 0 ){
            MY_LOGE("pthread join failed!");
            goto lbExit;
        }
        MY_LOGD("wait thread init done -");
    }
#endif
    else
    {
        MY_LOGE("should not happened!");
        goto lbExit;
    }

    ret = OK;
lbExit:
    return ret;
}

#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
/******************************************************************************
 *
 ******************************************************************************/
MVOID
StereoFlowControl::
doBMDNHALInit(){
    mpBMDN_HAL = NS_BMDN_HAL::BMDN_HAL::getInstance();
    mpBMDN_HAL->BMDNHalInit();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID*
StereoFlowControl::
doParrallelInit(void* arg)
{
    ::prctl(PR_SET_NAME,"initBMDN_HAL", 0, 0, 0);
    StereoFlowControl* pSelf = reinterpret_cast<StereoFlowControl*>(arg);
    pSelf->doBMDNHALInit();
    pthread_exit(NULL);
    return NULL;
}
#endif
