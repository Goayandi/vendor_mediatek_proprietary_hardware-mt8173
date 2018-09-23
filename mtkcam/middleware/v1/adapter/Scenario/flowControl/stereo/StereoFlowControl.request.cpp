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

#define LOG_TAG "MtkCam/StereoFlowControl_Request"
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

#include <mtkcam/utils/fwk/MtkCamera.h>

#include <bandwidth_control.h>

#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>

#include "StereoFlowControl.h"
using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace android;
using namespace NSCam::v3;

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
 * StereoRequestUpdater
 ******************************************************************************/
StereoFlowControl::StereoRequestUpdater::StereoRequestUpdater(
    sp<StereoFlowControl>  pStereoFlowControl,
    sp<StereoBufferSynchronizer> pStereoBufferSynchronizer,
    sp<IParamsManagerV3> pParamsManagerV3
)
: mpStereoFlowControl(pStereoFlowControl)
, mpStereoBufferSynchronizer(pStereoBufferSynchronizer)
, mpParamsMngrV3(pParamsManagerV3)
{
    MY_LOGD("StereoRequestUpdater ctor");

    char cProperty[PROPERTY_VALUE_MAX];
    ::property_get("debug.camera.log", cProperty, "0");
    mLogLevel = ::atoi(cProperty);
    if ( 0 == mLogLevel ) {
        ::property_get("debug.camera.log.stereoflow", cProperty, "0");
        mLogLevel = ::atoi(cProperty);
    }

    mAppSetting.clear();
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
StereoFlowControl::StereoRequestUpdater::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;

    mSelector_resized = NULL;
    mSelector_full = NULL;
    mSelector_resized_main2 = NULL;
    mSelector_full_main2 = NULL;

    mPool_resized = NULL;
    mPool_full = NULL;
    mPool_resized_main2 = NULL;
    mPool_full_main2 = NULL;

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoRequestUpdater::
setSensorParams(StereoPipelineSensorParam main1, StereoPipelineSensorParam main2){
    mSensorParam        = main1;
    mSensorParam_main2  = main2;

    MY_LOGD("mSensorParam.size=(%dx%d)  mSensorParam_main2.size=(%dx%d)",
        mSensorParam.size.w,
        mSensorParam.size.h,
        mSensorParam_main2.size.w,
        mSensorParam_main2.size.h
    );

    if( mSensorParam.size.w == 0 ||
        mSensorParam.size.h == 0 ||
        mSensorParam_main2.size.w == 0 ||
        mSensorParam_main2.size.h == 0
    ){
        MY_LOGE("some of the mSensorParam is 0, should not have happened");
        return UNKNOWN_ERROR;
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoRequestUpdater::
addSelector(sp<StereoSelector> pSelector, MINT32 streamType)
{
    FUNC_START;

    switch(streamType){
        case StreamType::RESIZED:{
            mSelector_resized = pSelector;
            break;
        }
        case StreamType::FULL:{
            mSelector_full = pSelector;
            break;
        }
        case StreamType::RESIZED_MAIN2:{
            mSelector_resized_main2 = pSelector;
            break;
        }
        case StreamType::FULL_MAIN2:{
            mSelector_full_main2 = pSelector;
            break;
        }
        default:{
            MY_LOGE("adding selector with unknown stream type!");
            return UNKNOWN_ERROR;
        }
    }

    MY_LOGD("mSelector_resized:%p",         mSelector_resized.get());
    MY_LOGD("mSelector_full:%p",            mSelector_full.get());
    MY_LOGD("mSelector_resized_main2:%p",   mSelector_resized_main2.get());
    MY_LOGD("mSelector_full_main2:%p",      mSelector_full_main2.get());

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoRequestUpdater::
addPool(sp<StereoBufferPool> pPool, MINT32 streamType)
{
    FUNC_START;

    switch(streamType){
        case StreamType::RESIZED:{
            mPool_resized = pPool;
            break;
        }
        case StreamType::FULL:{
            mPool_full = pPool;
            break;
        }
        case StreamType::RESIZED_MAIN2:{
            mPool_resized_main2 = pPool;
            break;
        }
        case StreamType::FULL_MAIN2:{
            mPool_full_main2 = pPool;
            break;
        }
        default:{
            MY_LOGE("adding selector with unknown stream type!");
            return UNKNOWN_ERROR;
        }
    }

    MY_LOGD("mPool_resized:%p",         mPool_resized.get());
    MY_LOGD("mPool_full:%p",            mPool_full.get());
    MY_LOGD("mPool_resized_main2:%p",   mPool_resized_main2.get());
    MY_LOGD("mPool_full_main2:%p",      mPool_full_main2.get());

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
StereoFlowControl::StereoRequestUpdater::
setPreviewStreamId(MINT32 streamId)
{
    mPrvStreamId = streamId;
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
StereoFlowControl::StereoRequestUpdater::
setFdStreamId(MINT32 streamId)
{
    mFdStreamId = streamId;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoRequestUpdater::
updateRequestSetting(IMetadata* appSetting, IMetadata* halSetting)
{
    MY_LOGD1("Creating P2 request req(%d) + ", mReqNo);
    RWLock::AutoRLock _l(mLock);

    if(mSelector_resized == NULL || mSelector_resized_main2 == NULL){
        MY_LOGE("updateRequestSetting before required providers are set, sould not have happened!");
    }

    // step 0
    // lock synchronizer
    mpStereoBufferSynchronizer->lock();

    android::sp<IImageBufferHeap> spHeap_resized        = NULL;
    android::sp<IImageBufferHeap> spHeap_full           = NULL;
    android::sp<IImageBufferHeap> spHeap_resized_main2  = NULL;
    android::sp<IImageBufferHeap> spHeap_full_main2     = NULL;
    Vector<ISelector::MetaItemSet> tempMetadata;
    Vector<ISelector::BufferItemSet> rvBufferSet_resized;
    Vector<ISelector::BufferItemSet> rvBufferSet_full;
    Vector<ISelector::BufferItemSet> rvBufferSet_resized_main2;
    Vector<ISelector::BufferItemSet> rvBufferSet_full_main2;
    IMetadata appDynamic;

    // step 1
    // getResult from main1_resized
    // appMeta and halMeta also updated by this Selector
    if(mSelector_resized != NULL)
    {
        MY_LOGD1("getResult from main1_resized  +");
        tempMetadata.clear();

        MINT32 tempReqNo = -1;

#warning "FIXME. [N-Migration]getResult() interface change, just compile pass currently."
        if(mSelector_resized->getResult(tempReqNo, tempMetadata, rvBufferSet_resized) != OK){
            MY_LOGE("mSelector_resized->getResult failed!");
        }

        bool found_appDynamic = false;
        bool found_halDynamic = false;
        for ( int i = 0; i < tempMetadata.size(); ++i ){
            switch(tempMetadata[i].id){
                case eSTREAMID_META_APP_DYNAMIC_P1:
                    appDynamic = tempMetadata[i].meta;
                    found_appDynamic = true;
                    break;
                case eSTREAMID_META_HAL_DYNAMIC_P1:
                    *halSetting = tempMetadata[i].meta;
                    found_halDynamic = true;
                    break;
                default:
                    MY_LOGE("unexpected meta stream from selector:%d", tempMetadata[i].id);
                    break;
            }
        }
        if(!found_appDynamic || !found_appDynamic){
            MY_LOGE("some meta stream from P1_main1 is not correctly set! (%d ,%d )",
                found_appDynamic,
                found_halDynamic
            );
            return UNKNOWN_ERROR;
        }
        MY_LOGD1("getResult from main1_resized  -");
    }

    // step 2
    // getResult from main1_full
    if(mSelector_full != NULL)
    {
        MY_LOGD1("getResult from main1_full  +");
        tempMetadata.clear();

        MINT32 tempReqNo = -1;

#warning "FIXME. [N-Migration]getResult() interface change, just compile pass currently."
        if(mSelector_full->getResult(tempReqNo, tempMetadata, rvBufferSet_full) != OK){
            MY_LOGE("mSelector_full->getResult failed!");
        }
        MY_LOGD1("getResult from main1_full  -");
    }

    // step 3
    // getResult from main2_resized
    if(mSelector_resized_main2 != NULL)
    {
        MY_LOGD1("getResult from main2_resized  +");
        tempMetadata.clear();

        MINT32 tempReqNo = -1;
#warning "FIXME. [N-Migration]getResult() interface change, just compile pass currently."
        if(mSelector_resized_main2->getResult(tempReqNo, tempMetadata, rvBufferSet_resized_main2) != OK){
            MY_LOGE("mSelector_resized_main2->getResult failed!");
        }

        bool found_appDynamic = false;
        bool found_halDynamic = false;
        for ( int i = 0; i < tempMetadata.size(); ++i ){
            switch(tempMetadata[i].id){
                case eSTREAMID_META_APP_DYNAMIC_P1_MAIN2:
                    found_appDynamic = true;
                    break;
                case eSTREAMID_META_HAL_DYNAMIC_P1_MAIN2:{
                    MY_LOGD1("use main2 hal dynamic");
                    IMetadata theMeta = tempMetadata[i].meta;

                    MY_LOGD1("push main2 hal dynamic into hal_meta");
                    IMetadata::IEntry entry_meta(MTK_P1NODE_MAIN2_HAL_META);
                    entry_meta.push_back(theMeta, Type2Type< IMetadata >());
                    halSetting->update(entry_meta.tag(), entry_meta);
                    found_halDynamic = true;
                    break;
                }
                default:
                    MY_LOGE("unexpected meta stream from selector:%d", tempMetadata[i].id);
                    break;
            }
        }
        if(!found_appDynamic || !found_appDynamic){
            MY_LOGE("some meta stream from P1_main1 is not correctly set! (%d ,%d )",
                found_appDynamic,
                found_halDynamic
            );
            return UNKNOWN_ERROR;
        }
        MY_LOGD1("getResult from main2_resized  -");
    }

    // step 4
    // getResult from main2_full
    if(mSelector_full_main2 != NULL)
    {
        MY_LOGD1("getResult from main2_full  +");
        tempMetadata.clear();

        MINT32 tempReqNo = -1;
#warning "FIXME. [N-Migration]getResult() interface change, just compile pass currently."
        if(mSelector_full_main2->getResult(tempReqNo, tempMetadata, rvBufferSet_full_main2) != OK){
            MY_LOGE("mSelector_full_main2->getResult failed!");
        }

        MY_LOGD1("getResult from main2_full  -");
    }

    // step 5
    // unlock synchronizer
    mpStereoBufferSynchronizer->unlock();

    // step 6
    // udpate specific parameters for stereo cam into app control
    {
        // DoF Level
        {
            MINT32 DoFLevel;
            if(!tryGetMetadata<MINT32>(&mAppSetting, MTK_STEREO_FEATURE_DOF_LEVEL, DoFLevel)){
                MY_LOGW("cannot get DoF level! Reset to 15");
                DoFLevel = 15;
            }

            IMetadata::IEntry entry(MTK_STEREO_FEATURE_DOF_LEVEL);
            entry.push_back(DoFLevel, Type2Type< MINT32 >());
            appSetting->update(entry.tag(), entry);
        }
        // add 3DNR flag
        {
            MINT32 mode3DNR = MTK_NR_FEATURE_3DNR_MODE_OFF;
            if(!tryGetMetadata<MINT32>(&mAppSetting, MTK_NR_FEATURE_3DNR_MODE, mode3DNR)){
                MY_LOGW("cannot get 3DNR flag ! Reset to MTK_NR_FEATURE_3DNR_MODE_OFF");
                mode3DNR = MTK_NR_FEATURE_3DNR_MODE_OFF;
            }

            IMetadata::IEntry entry(MTK_NR_FEATURE_3DNR_MODE);
            entry.push_back(mode3DNR, Type2Type< MINT32 >());
            appSetting->update(entry.tag(), entry);
        }
        // add touch info
        {
            IMetadata::IEntry entry = mAppSetting.entryFor(MTK_STEREO_FEATURE_TOUCH_POSITION);
            if(entry.isEmpty()) {
                entry.push_back(0, Type2Type< MINT32 >());
                entry.push_back(0, Type2Type< MINT32 >());
            }
            MY_LOGD1("Set touch info x, y : %d, %d", entry.itemAt(0, Type2Type<MINT32>()), entry.itemAt(1, Type2Type<MINT32>()));
            appSetting->update(entry.tag(), entry);
        }
    }

    // step 7
    // enque halBuffer to P2Pipeline's buffer pool
    {
        MY_LOGD1("setBuffer to pool +");
        mPool_resized->setBuffer("pre spHeap_resized", spHeap_resized);
        mPool_resized_main2->setBuffer("pre spHeap_resized_main2", spHeap_resized_main2);
        if(mPool_full != NULL) {
            mPool_full->setBuffer("pre spHeap_full", spHeap_full);
        }
        if(mPool_full_main2 != NULL) {
            mPool_full_main2->setBuffer("pre spHeap_full_main2", spHeap_full_main2);
        }
        MY_LOGD1("setBuffer to pool -");
    }

    // step 8
    // update scaler crop region
    {
        IMetadata::IEntry entry(MTK_SCALER_CROP_REGION);
        entry.push_back(mpStereoFlowControl->getActiveArrayCrop(), Type2Type<MRect>());
        appSetting->update(MTK_SCALER_CROP_REGION, entry);
    }

    // step 9
    // check if we need to do some callback(ex: distance warning/ too close warning)
    if(mpStereoFlowControl != NULL){
        mpStereoFlowControl->checkNotifyCallback(&appDynamic);
    }

    // step 10
    // send timestamp for those who needs one.
    {
        sp<IResourceContainer> pResourceContainierP2Prv = IResourceContainer::getInstance(mpStereoFlowControl->getOpenId_P2Prv());
        sp<StreamBufferProvider> provider_resizer       = pResourceContainierP2Prv->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_RESIZER);
        sp<StreamBufferProvider> provider_resizer_01    = pResourceContainierP2Prv->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01);
        sp<StreamBufferProvider> provider_opaque        = pResourceContainierP2Prv->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
        sp<StreamBufferProvider> provider_opaque_01     = pResourceContainierP2Prv->queryConsumer(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01);
        sp<StreamBufferProvider> provider_preview_yuv   = pResourceContainierP2Prv->queryConsumer(mPrvStreamId);
        sp<StreamBufferProvider> provider_record_yuv    = pResourceContainierP2Prv->queryConsumer(eSTREAMID_IMAGE_PIPE_BOKEHNODE_RECORDYUV);
        sp<StreamBufferProvider> provider_fd_yuv        = pResourceContainierP2Prv->queryConsumer(mFdStreamId);
        sp<StreamBufferProvider> provider_prvcb_yuv        = pResourceContainierP2Prv->queryConsumer(eSTREAMID_IMAGE_PIPE_YUV_01);
        if( provider_resizer == NULL || provider_resizer_01 == NULL || provider_preview_yuv == NULL)
        {
            MY_LOGE("Cannot get provider. create P2 request faield.");
            MY_LOGE_IF(provider_resizer == NULL, "provider_resizer is NULL");
            MY_LOGE_IF(provider_resizer_01 == NULL, "provider_resizer_01 is NULL");
            MY_LOGE_IF(provider_preview_yuv == NULL, "provider_preview_yuv is NULL");
            return BAD_VALUE;
        }else{
            MINT64 timestamp_P1;
            if(tryGetMetadata<MINT64>(&appDynamic, MTK_SENSOR_TIMESTAMP, timestamp_P1)){
                MY_LOGD1("timestamp_P1:%lld", timestamp_P1);
            }else{
                MY_LOGW("Can't get timestamp from appDynamic, set to 0");
                timestamp_P1 = 0;
            }
            MY_LOGD1("manually do doTimestampCallback mReqNo(%d),errorResult(%d),timestamp_P1(%lld)", mReqNo, MFALSE, timestamp_P1);
            provider_resizer->doTimestampCallback(          mReqNo, MFALSE, timestamp_P1);
            provider_resizer_01->doTimestampCallback(       mReqNo, MFALSE, timestamp_P1);
            provider_preview_yuv->doTimestampCallback(      mReqNo, MFALSE, timestamp_P1);
            //
            if(provider_opaque != NULL) {
                provider_opaque->doTimestampCallback(mReqNo, MFALSE, timestamp_P1);
            }
            if(provider_opaque_01 != NULL) {
                provider_opaque_01->doTimestampCallback(mReqNo, MFALSE, timestamp_P1);
            }
            if(provider_record_yuv != NULL) {
                provider_record_yuv->doTimestampCallback(mReqNo, MFALSE, timestamp_P1);
            }
            if(provider_fd_yuv!= NULL) {
                provider_fd_yuv->doTimestampCallback(mReqNo, MFALSE, timestamp_P1);
            }
            if(provider_prvcb_yuv!= NULL) {
                provider_prvcb_yuv->doTimestampCallback(mReqNo, MFALSE, timestamp_P1);
            }
        }
    }

    MY_LOGD1("Creating P2 request req(%d) - ", mReqNo);
    mReqNo++;
    if(mReqNo > STEREO_FLOW_PREVIEW_REQUSET_NUM_END){
        mReqNo = STEREO_FLOW_PREVIEW_REQUSET_NUM_START;
    };

#if 0
    // Debug Log
    {
        MSize tempSize;
        if( ! tryGetMetadata<MSize>(const_cast<IMetadata*>(halSetting), MTK_HAL_REQUEST_SENSOR_SIZE, tempSize) ){
            MY_LOGE("cannot get MTK_HAL_REQUEST_SENSOR_SIZE after updating request");
        }else{
            MY_LOGD1("MTK_HAL_REQUEST_SENSOR_SIZE:(%dx%d)", tempSize.w, tempSize.h);
        }
        IMetadata tempMetadata;
        if( ! tryGetMetadata<IMetadata>(const_cast<IMetadata*>(halSetting), MTK_P1NODE_MAIN2_HAL_META, tempMetadata) ){
            MY_LOGE("cannot get MTK_P1NODE_MAIN2_HAL_META after updating request");
        }else{
            MY_LOGD1("MTK_P1NODE_MAIN2_HAL_META");
        }
        MINT32 tempLevel;
        if( ! tryGetMetadata<MINT32>(const_cast<IMetadata*>(appSetting), MTK_STEREO_FEATURE_DOF_LEVEL, tempLevel) ){
            MY_LOGE("cannot get MTK_STEREO_FEATURE_DOF_LEVEL after updating request");
        }else{
            MY_LOGD1("MTK_STEREO_FEATURE_DOF_LEVEL:%d", tempLevel);
        }
    }
#endif
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoRequestUpdater::
updateParameters(IMetadata* setting)
{
    sp<IParamsManager> paramsMgr = mpStereoFlowControl->getParamsMgr();
    if(paramsMgr == NULL){
        MY_LOGE("paramsMgr == NULL");
        return UNKNOWN_ERROR;
    }

    RWLock::AutoWLock _l(mLock);

    MINT32 pipelineMode = mpStereoFlowControl->getPipelineMode();
    // EIS Metadata setting : Preview
    if( pipelineMode == PipelineMode_ZSD)
    {
        mpStereoFlowControl->getParamsMgrV3()->updateRequestPreview(setting);
    }
    // Record
    else if(pipelineMode == PipelineMode_RECORDING)
    {
        mpStereoFlowControl->getParamsMgrV3()->updateRequestRecord(setting);
    }

    // DoF Level
    {
        int DoFLevel_UI = paramsMgr->getInt(MtkCameraParameters::KEY_STEREO_DOF_LEVEL);
        int DoFLevel_HAL = DoFLevel_UI*2;
        int DoFMin_HAL = 0;
        int DoFMax_HAL = 30;

        if(DoFLevel_UI<DoFMin_HAL){
            MY_LOGW("DoFLevel_HAL not in illegal range, reset to %d", DoFMin_HAL);
            DoFLevel_HAL = DoFMin_HAL;
        }

        if(DoFLevel_UI>DoFMax_HAL){
            MY_LOGW("DoFLevel_HAL not in illegal range, reset to %d", DoFMax_HAL);
            DoFLevel_HAL = DoFMax_HAL;
        }

        IMetadata::IEntry entry(MTK_STEREO_FEATURE_DOF_LEVEL);
        entry.push_back(DoFLevel_HAL, Type2Type< MINT32 >());
        mAppSetting.update(entry.tag(), entry);
        MY_LOGD1("Set DofLevel UI/HAL (%d/%d)", DoFLevel_UI, DoFLevel_HAL);
    }

    // add 3DNR flag
    {
        MINT32 mode3DNR = MTK_NR_FEATURE_3DNR_MODE_OFF;
        if(::strcmp(paramsMgr->getStr
                    (MtkCameraParameters::KEY_3DNR_MODE),
                    "on") == 0 )
        {
            MY_LOGD1("3DNR on");
            mode3DNR = MTK_NR_FEATURE_3DNR_MODE_ON;
        }
        IMetadata::IEntry entry(MTK_NR_FEATURE_3DNR_MODE);
        entry.push_back(mode3DNR, Type2Type< MINT32 >());
        mAppSetting.update(entry.tag(), entry);
    }
    // add touch info
    {
        String8 sString = paramsMgr->getStr(MtkCameraParameters::KEY_STEREO_TOUCH_POSITION);
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
        mAppSetting.update(entry.tag(), entry);
    }

    return OK;
}
/******************************************************************************
 * StereoMain2RequestUpdater
 ******************************************************************************/
StereoFlowControl::StereoMain2RequestUpdater::StereoMain2RequestUpdater(
    sp<StereoFlowControl>  pStereoFlowControl,
    sp<StereoBufferSynchronizer> pStereoBufferSynchronizer,
    sp<IParamsManagerV3> pParamsManagerV3
)
: StereoRequestUpdater(pStereoFlowControl, pStereoBufferSynchronizer, pParamsManagerV3)
{
    MY_LOGD("StereoMain2RequestUpdater ctor");
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoMain2RequestUpdater::
updateRequestSetting(IMetadata* appSetting, IMetadata* halSetting){
    // update app control
    {
        IMetadata::IEntry entry(MTK_SCALER_CROP_REGION);
        entry.push_back(mpStereoFlowControl->getActiveArrayCrop_Main2(), Type2Type<MRect>());
        appSetting->update(MTK_SCALER_CROP_REGION, entry);
    }

    // update hal control
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
        entry.push_back(mSensorParam_main2.size, Type2Type< MSize >());
        halSetting->update(entry.tag(), entry);

        IMetadata::IEntry entry2(MTK_P1NODE_SENSOR_CROP_REGION);
        entry2.push_back(mpStereoFlowControl->getSensorDomainCrop_Main2(), Type2Type<MRect>());
        halSetting->update(MTK_P1NODE_SENSOR_CROP_REGION, entry2);

        // always request exif since stereo is zsd flow
        IMetadata::IEntry entry3(MTK_HAL_REQUEST_REQUIRE_EXIF);
        entry3.push_back(true, Type2Type< MUINT8 >());
        halSetting->update(entry3.tag(), entry3);

        IMetadata::IEntry entry4(MTK_3A_ISP_PROFILE);
        entry4.push_back(mpStereoFlowControl->getIspProfile(), Type2Type< MUINT8 >());
        halSetting->update(entry4.tag(), entry4);
    }

    mpParamsMngrV3->updateRequestHal(halSetting);

    MY_LOGD1("P1 main2 udpate request sensorSize:(%dx%d)  sensorCrop:(%d,%d,%d,%d)",
        mSensorParam_main2.size.w,
        mSensorParam_main2.size.h,
        mpStereoFlowControl->getSensorDomainCrop_Main2().p.x,
        mpStereoFlowControl->getSensorDomainCrop_Main2().p.y,
        mpStereoFlowControl->getSensorDomainCrop_Main2().s.w,
        mpStereoFlowControl->getSensorDomainCrop_Main2().s.h
    );

    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoFlowControl::StereoMain2RequestUpdater::
updateParameters(IMetadata* setting){
    RWLock::AutoWLock _l(mLock);

    return mpStereoFlowControl->updateParametersCommon(setting, mpStereoFlowControl->getParamsMgrV3(), mSensorParam_main2);
}
