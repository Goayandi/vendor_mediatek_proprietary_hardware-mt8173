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

#define LOG_TAG "MtkCam/DefaultFlowControl"
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
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
#include <mtkcam/middleware/v1/LegacyPipeline/NodeId.h>
#include "DefaultFlowControl.h"
#include <mtkcam/pipeline/pipeline/PipelineContext.h>

#include <mtkcam/utils/fwk/MtkCamera.h>

#include <future>
#include <vector>

using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace android;
using namespace NSCam::v3;
using namespace NSCam::v3::NSPipelineContext;
using namespace NSCamHW;
using namespace NSCam::Utils;

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
/******************************************************************************
 *
 ******************************************************************************/
static
MVOID
prepare_stream(BufferList& vDstStreams, StreamId id, MBOOL criticalBuffer)
{
    vDstStreams.push_back(
        BufferSet{
            .streamId       = id,
            .criticalBuffer = criticalBuffer,
        }
    );
}

/******************************************************************************
 *
 ******************************************************************************/
DefaultFlowControl::
DefaultFlowControl(
    char const*                 pcszName,
    MINT32 const                i4OpenId,
    sp<IParamsManagerV3>          pParamsManagerV3,
    sp<ImgBufProvidersManager>  pImgBufProvidersManager,
    sp<INotifyCallback>         pCamMsgCbInfo
)
    : mName(const_cast<char*>(pcszName))
    , mOpenId(i4OpenId)
    , mLogLevel(1)
    , mpRequestThreadLoopCnt(1)
    , mpParamsManagerV3(pParamsManagerV3)
    , mpImgBufProvidersMgr(pImgBufProvidersManager)
    , mpCamMsgCbInfo(pCamMsgCbInfo)
    , mb4K2KVideoRecord(MFALSE)
    , mpDeviceHelper(NULL)
    , mRequestType(MTK_CONTROL_CAPTURE_INTENT_PREVIEW)
    , mLastZoomRatio(100)
    , mbIsLastImgoPreview(MFALSE)
    , mbNeedHighQualityZoom(MFALSE)
    , mEnLtm(0)
{
    mLPBConfigParams.mode = 0;
    mLPBConfigParams.enableEIS = MFALSE;
    mLPBConfigParams.enableLCS = MFALSE;
    //
    MY_LOGD("ResourceContainer::getInstance(%d)",mOpenId);
    mpResourceContainer = IResourceContainer::getInstance(mOpenId);
    //Default enable Imgo Preview for ZSD preview
    mbNeedHighQualityZoom = (property_get_int32("debug.feature.enableImgoPrv", 1) > 0) ? MTRUE : MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/
char const*
DefaultFlowControl::
getName()   const
{
    return mName;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t
DefaultFlowControl::
getOpenId() const
{
    return mOpenId;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
startPreview()
{
    FUNC_START;

    MINT32 lcsOpen = mEnLtm;
    mLPBConfigParams.enableLCS = (lcsOpen > 0); // LCS test

    mpRequestController = IRequestController::createInstance(
                                                mName,
                                                mOpenId,
                                                mpCamMsgCbInfo,
                                                mpParamsManagerV3
                                            );
    //
    mpResourceContainer->setFeatureFlowControl(this);
    //
    mpDeviceHelper = new CamManager::UsingDeviceHelper(getOpenId());
    mLPBConfigParams.enableEIS = mpDeviceHelper->isFirstUsingDevice();
    if ( mpParamsManagerV3->getParamsMgr()->getRecordingHint() )
    {
        constructRecordingPipeline();
        mRequestType = MTK_CONTROL_CAPTURE_INTENT_PREVIEW;
    } else if ( PARAMSMANAGER_MAP_INST(eMapAppMode)->stringFor(mpParamsManagerV3->getParamsMgr()->getHalAppMode())
                ==  MtkCameraParameters::APP_MODE_NAME_MTK_ZSD)
    {
        constructZsdPreviewPipeline();
        mRequestType = CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG;
    }
    else
    {
        constructNormalPreviewPipeline();
        mRequestType = MTK_CONTROL_CAPTURE_INTENT_PREVIEW;
    }
    mpDeviceHelper->configDone();
    mpRequestController->setRequestType(mRequestType);
    //
    mpParamsManagerV3->setPreviewMaxFps(
                            (mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) <= mSensorParam.fps) ?
                            mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) : mSensorParam.fps);
    //
    if ( mpPipeline == 0 ) {
        MY_LOGE("Cannot get pipeline. start preview fail.");
        mpDeviceHelper = NULL;
        return BAD_VALUE;
    }
    //

    MERROR ret = mpRequestController->startPipeline(
                                    0,/*start*/
                                    1000, /*end*/
                                    mpPipeline,
                                    this,
                                    mLPBConfigParams.mode,
                                    mpRequestThreadLoopCnt);
    //
    for ( size_t i = 0; i < mFocusCommandList.size(); ++i) {
        switch( mFocusCommandList[i] ) {
            case COMMAND_AUTO_FOCUS:
                mpRequestController->autoFocus();
                MY_LOGE("should not happen.");
            break;
            case COMMAND_CANCEL_AUTO_FOCUS:
                mpRequestController->cancelAutoFocus();
                MY_LOGD("Cancel AF.");
            break;
        };
    }
    mFocusCommandList.clear();
    FUNC_END;
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
stopPreview()
{
    FUNC_START;

    std::vector< std::future<MERROR> > vFutures;
    if( mpRequestController != 0 ) {
        vFutures.push_back(
            std::async(std::launch::async,
                [ this ]() { return mpRequestController->stopPipeline(); }
            )
        );
    }

    if ( mpPipeline != 0 ) {
        mpPipeline->flush();
        mpPipeline->waitUntilDrained();
        mpPipeline = NULL;
        mpDeviceHelper = NULL;
    }

    for( auto &fut : vFutures ) {
        MERROR result = fut.get();
    }
    mpRequestController = NULL;

    mb4K2KVideoRecord = MFALSE;
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
pausePreview(
    MBOOL stopPipeline
)
{
    Vector< BufferSet > vDstStreams;
    if ( !stopPipeline ) {
        vDstStreams.push_back(
        BufferSet{
            .streamId       = eSTREAMID_IMAGE_PIPE_RAW_OPAQUE,
            .criticalBuffer = false
            }
        );
    }
    return ( mpRequestController == nullptr ) ? UNKNOWN_ERROR :
        mpRequestController->pausePipeline(vDstStreams);
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
resumePreview()
{
    return ( mpRequestController == nullptr ) ? UNKNOWN_ERROR :
        mpRequestController->resumePipeline();
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
startRecording()
{
    status_t status;
#if 1
    if( ( mLPBConfigParams.mode != LegacyPipelineMode_T::PipelineMode_VideoRecord &&
          mLPBConfigParams.mode != LegacyPipelineMode_T::PipelineMode_HighSpeedVideo ) ||
        needReconstructRecordingPipe() )
    {
        mpRequestController->stopPipeline();

        if ( mpPipeline != 0 ) {
            mpPipeline->flush();
            mpPipeline->waitUntilDrained();
            mpPipeline = NULL;
            mpDeviceHelper = NULL;
        }
        //
        mpDeviceHelper = new CamManager::UsingDeviceHelper(getOpenId());
        constructRecordingPipeline();
        mpDeviceHelper->configDone();
    }
#endif

    if ( mpPipeline == 0 ) {
        MY_LOGE("Cannot get pipeline. start preview fail.");
        mpDeviceHelper = NULL;
        return BAD_VALUE;
    }
    //
    mRequestType = MTK_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
    mpRequestController->setRequestType(mRequestType);
    //
    status = mpRequestController->startPipeline(
                                    1001,/*start*/
                                    2000, /*end*/
                                    mpPipeline,
                                    this,
                                    mLPBConfigParams.mode,
                                    mpRequestThreadLoopCnt);

    Vector< SettingSet > vSettings;
    BufferList           vDstStreams;
    Vector< MINT32 >     vRequestNo;

    prepareVideoInfo(vSettings,vDstStreams);
    submitRequest( vSettings, vDstStreams, vRequestNo );

    return status;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
stopRecording()
{
    // set capture intent back to preview
    if ( mpRequestController != 0 ) {
        mRequestType = MTK_CONTROL_CAPTURE_INTENT_PREVIEW;
        mpRequestController->setRequestType(mRequestType);
        mpRequestController->setParameters( this );
    } else {
        MY_LOGW("No RequestController.");
    }

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
autoFocus()
{
    if ( mpRequestController != 0 ) {
        return mpRequestController->autoFocus();
    } else {
        mFocusCommandList.push_back(COMMAND_AUTO_FOCUS);
        return OK;
    }
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
cancelAutoFocus()
{
    if ( mpRequestController != 0 ) {
        return mpRequestController->cancelAutoFocus();
    } else {
        mFocusCommandList.push_back(COMMAND_CANCEL_AUTO_FOCUS);
        return OK;
    }
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
precapture(int& flashRequired)
{
    return (mpRequestController != 0) ?
        mpRequestController->precapture(flashRequired) : OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
takePicture()
{
#if 1
    FUNC_START;

    if( mLPBConfigParams.mode == LegacyPipelineMode_T::PipelineMode_VideoRecord )
    {
        Vector< SettingSet > vSettings;
        BufferList           vDstStreams;
        Vector< MINT32 >     vRequestNo;

        prepareVSSInfo(vSettings,vDstStreams);
        submitRequest( vSettings, vDstStreams, vRequestNo );

        FUNC_END;
        return OK;
    }
    else
    {
        MY_LOGW("Not support LegacyPipelineMode %d",mLPBConfigParams.mode);
    }

#endif
    //
    FUNC_END;
    return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DefaultFlowControl::
runHighQualityZoomFlow()
{
    if( mpParamsManagerV3 != NULL &&
        mpParamsManagerV3->getParamsMgr() != NULL &&
        mpPipeline != NULL)
    {
        //For ZSD preview only (because only ZSD preview has IMGO)
        if( mLPBConfigParams.mode == LegacyPipelineMode_T::PipelineMode_ZsdPreview ||
            mLPBConfigParams.mode == LegacyPipelineMode_T::PipelineMode_Feature_ZsdPreview)
        {
            MUINT32 zoomRatio = mpParamsManagerV3->getParamsMgr()->getZoomRatio();
            mpParamsManagerV3->calculateCropRegion(getSensorMode());
            MRect reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion;
            mpParamsManagerV3->getCropRegion(reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion);
            MSize previewsize;
            mpParamsManagerV3->getParamsMgr()->getPreviewSize(&previewsize.w, &previewsize.h);
            //
            MY_LOGD("sensor mode = %d",getSensorMode());
            MY_LOGD("now zoomRatio(%d) last zoomratio(%d)",zoomRatio,mLastZoomRatio);
            MY_LOGD("previewsize(%d,%d)",previewsize.w, previewsize.h);
            MY_LOGD("reqSensorPreviewCropRegion(%d,%d,%d,%d)", reqSensorPreviewCropRegion.p.x,reqSensorPreviewCropRegion.p.y,reqSensorPreviewCropRegion.s.w,reqSensorPreviewCropRegion.s.h);
            //
            //the case is for saving pass2 power & performance
            MBOOL bUseImgoPreview = (reqPreviewCropRegion.s.w < previewsize.w) ? MTRUE : MFALSE;
            MY_LOGD("bUseImgoPreview(%d) mbIsLastImgoPreview(%d)",bUseImgoPreview,mbIsLastImgoPreview);
            //
            if ( zoomRatio != mLastZoomRatio &&
                 bUseImgoPreview != mbIsLastImgoPreview )
            {
                setNewZsdPreviewRequestBuilder(bUseImgoPreview);
                mbIsLastImgoPreview = bUseImgoPreview;
            }
        }
    }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DefaultFlowControl::
setNewZsdPreviewRequestBuilder(MBOOL useImgoPreview)
{
    MY_LOGD("useImgoPreview = %d", useImgoPreview);
    //
    BufferList vDstStreams;
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_RESIZER, false);
    if(mLPBConfigParams.enableLCS)
        prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_LCSO, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_00, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_01, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_YUV_FD, false);
    //
    if( mpPipeline == NULL )
    {
        MY_LOGW("setNewZsdPreviewRequestBuilder Failed (mpPipeline is NULL)");
        return;
    }
    //
    sp<RequestBuilder> pRequestBuilder = new RequestBuilder();
    KeyedVector< NSCam::v3::Pipeline_NodeId_T, NSCam::v3::NSPipelineContext::IOMapSet > vMetaIOMapInfo;
    mpPipeline->getMetaIOMapInfo(vMetaIOMapInfo);
    //P1 node
    IOMap p1_Image_IOMap;
    for ( size_t i = 0; i < vDstStreams.size(); ++i ) {
        switch( vDstStreams[i].streamId ) {
            case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:
            case eSTREAMID_IMAGE_PIPE_RAW_RESIZER:
            case eSTREAMID_IMAGE_PIPE_RAW_LCSO:
                p1_Image_IOMap.addOut(vDstStreams[i].streamId);
                break;
            default:
                break;
        }
    }
    pRequestBuilder->setIOMap(
            eNODEID_P1Node,
            IOMapSet().add(
                p1_Image_IOMap
                ),
            vMetaIOMapInfo.valueFor(eNODEID_P1Node)
            );
    pRequestBuilder->setRootNode(
        NodeSet().add(eNODEID_P1Node)
        );
    //P2 node
    IOMap p2_Image_IOMap;
    for ( size_t i = 0; i < vDstStreams.size(); ++i ) {
        switch( vDstStreams[i].streamId ) {
            case eSTREAMID_IMAGE_PIPE_YUV_00:
            case eSTREAMID_IMAGE_PIPE_YUV_01:
            case eSTREAMID_IMAGE_YUV_FD:
                p2_Image_IOMap.addOut(vDstStreams[i].streamId);
                break;
            case eSTREAMID_IMAGE_PIPE_RAW_RESIZER:
                if(!useImgoPreview)
                {
                    p2_Image_IOMap.addIn(eSTREAMID_IMAGE_PIPE_RAW_RESIZER);
                }
                break;
            case eSTREAMID_IMAGE_PIPE_RAW_LCSO:
                p2_Image_IOMap.addIn(eSTREAMID_IMAGE_PIPE_RAW_LCSO);
            case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:
                if(useImgoPreview)
                {
                    p2_Image_IOMap.addIn(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
                }
                break;
            default:
                break;
        };
    }
    pRequestBuilder->setIOMap(
            eNODEID_P2Node,
            IOMapSet().add(
                p2_Image_IOMap
                ),
            vMetaIOMapInfo.valueFor(eNODEID_P2Node)
            );
    // edge
    pRequestBuilder->setNodeEdges(
        NodeEdgeSet().addEdge(eNODEID_P1Node, eNODEID_P2Node)
        );
    // set RequestBuilder
    mpPipeline->setRequestBuilder(pRequestBuilder);
    // update FrameCallback
    sp<ResultProcessor> pResultProcessor = mpPipeline->getResultProcessor().promote();
    if( pResultProcessor != NULL )
        pRequestBuilder->updateFrameCallback(pResultProcessor);

}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
setParameters()
{
    if( mbNeedHighQualityZoom )
    {
        runHighQualityZoomFlow();
    }
    //
    return (mpRequestController != 0) ?
        mpRequestController->setParameters( this ) : OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
sendCommand(
    int32_t cmd,
    int32_t arg1,
    int32_t arg2
)
{
    // the argument "arg2" is NOT 0 if the command is sent from middleware,
    // we assume that the LTM on/off command should be sent from middleware hence
    // we need to check arg2 here
    if (cmd == IFlowControl::eExtCmd_setLtmEnable && arg2 != 0)
    {
        mEnLtm = arg1;
        MY_LOGD("set LTM enable to %d", mEnLtm);
        return OK; // this command is un-necessary to be sent to mpRequestController
    }

    //
    return (mpRequestController != 0) ?
        mpRequestController->sendCommand( cmd, arg1, arg2 ) : INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DefaultFlowControl::
dump(
    int /*fd*/,
    Vector<String8>const& /*args*/
)
{
#warning "TODO"
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DefaultFlowControl::
updateParameters(
    IMetadata* setting
)
{
    if (mpParamsManagerV3 != 0)
    {
        if( mLPBConfigParams.mode == LegacyPipelineMode_T::PipelineMode_VideoRecord ||
            mLPBConfigParams.mode == LegacyPipelineMode_T::PipelineMode_HighSpeedVideo )
        {
            mpParamsManagerV3->updateRequestRecord(setting);
        }else
        {
            mpParamsManagerV3->updateRequestPreview(setting);
        }
    }

    return ( mpParamsManagerV3 != 0 )
                ? mpParamsManagerV3->updateRequest(setting, mSensorParam.mode)
                : UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DefaultFlowControl::
updateRequestSetting(
    IMetadata* /*appSetting*/,
    IMetadata* halSetting
)
{
    CAM_TRACE_NAME("DFC:updateRequestSetting");
    MBOOL isRepeating = true;
    // update app control
    // update hal control
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
        entry.push_back(mSensorParam.size, Type2Type< MSize >());
        halSetting->update(entry.tag(), entry);
    }
    // update default HAL settings
    mpParamsManagerV3->updateRequestHal(halSetting);

    //
    if( mLPBConfigParams.mode == LegacyPipelineMode_T::PipelineMode_ZsdPreview)
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_REQUIRE_EXIF);
        entry.push_back(true, Type2Type< MUINT8 >());
        halSetting->update(entry.tag(), entry);
    }

    if ( !isRepeating ) {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_REPEAT);
        entry.push_back(isRepeating, Type2Type< MUINT8 >());
        halSetting->update(entry.tag(), entry);
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DefaultFlowControl::
submitRequest(
    Vector< SettingSet > rvSettings,
    BufferList           rvDstStreams,
    Vector< MINT32 >&    rvRequestNo
)
{
    status_t ret = UNKNOWN_ERROR;
    if( mpRequestController == NULL)
    {
        MY_LOGE("mpRequestController is NULL");
        return UNKNOWN_ERROR;
    }
    //
    for ( size_t i = 0; i < rvSettings.size(); ++i ) {
        IMetadata::IEntry entry = rvSettings[i].appSetting.entryFor(MTK_CONTROL_CAPTURE_INTENT);
        if( entry.isEmpty() && !rvSettings[i].appSetting.isEmpty() ) {
            MY_LOGD("Does not contain MTK_CONTROL_CAPTURE_INTENT, overwrite it.");
            IMetadata appSetting;
            mpParamsManagerV3->updateRequest( &appSetting, mSensorParam.mode, mRequestType);
            appSetting += rvSettings[i].appSetting;
            *(const_cast<IMetadata*>(&rvSettings[i].appSetting)) = appSetting;
            updateRequestSetting( const_cast<IMetadata*>(&rvSettings[i].appSetting), const_cast<IMetadata*>(&rvSettings[i].halSetting) );
        }
    }
    //
    Vector< BufferList >  vDstStreams;
    for (size_t i = 0; i < rvSettings.size(); ++i) {
        vDstStreams.push_back(rvDstStreams);
    }
    ret = mpRequestController->submitRequest( rvSettings, vDstStreams, rvRequestNo );

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
DefaultFlowControl::
submitRequest(
    Vector< SettingSet > rvSettings,
    Vector< BufferList > rvDstStreams,
    Vector< MINT32 >&    rvRequestNo
)
{
    status_t ret = UNKNOWN_ERROR;
    if( mpRequestController == NULL)
    {
        MY_LOGE("mpRequestController is NULL");
        return UNKNOWN_ERROR;
    }
    //
    for ( size_t i = 0; i < rvSettings.size(); ++i ) {
        IMetadata::IEntry entry = rvSettings[i].appSetting.entryFor(MTK_CONTROL_CAPTURE_INTENT);
        if( entry.isEmpty() && !rvSettings[i].appSetting.isEmpty() ) {
            MY_LOGD("Does not contain MTK_CONTROL_CAPTURE_INTENT, overwrite it.");
            IMetadata appSetting;
            mpParamsManagerV3->updateRequest( &appSetting, mSensorParam.mode, mRequestType);
            appSetting += rvSettings[i].appSetting;
            *(const_cast<IMetadata*>(&rvSettings[i].appSetting)) = appSetting;
            updateRequestSetting( const_cast<IMetadata*>(&rvSettings[i].appSetting), const_cast<IMetadata*>(&rvSettings[i].halSetting) );
        }
    }
    //
    ret = mpRequestController->submitRequest( rvSettings, rvDstStreams, rvRequestNo );

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
MVOID
DefaultFlowControl::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;
    mpRequestController = NULL;
    if ( mpPipeline != 0 ) {
        mpPipeline->flush();
        mpPipeline->waitUntilDrained();
        mpPipeline = NULL;
        mpDeviceHelper = NULL;
    }
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DefaultFlowControl::
selectHighSpeedSensorScen(
    MUINT   /*fps*/,
    MUINT&  sensorScen)
{
    SensorSlimVideoInfo sensorSlimVideoselect;
    SensorSlimVideoInfo sensorSlimVideo[2];
    //
    HwInfoHelper helper(mOpenId);
    if( ! helper.updateInfos() )
    {
        MY_LOGE("cannot properly update infos");
        return BAD_VALUE;
    }
    //
    sensorSlimVideo[0].scenario = SENSOR_SCENARIO_ID_SLIM_VIDEO1;
    sensorSlimVideo[1].scenario = SENSOR_SCENARIO_ID_SLIM_VIDEO2;
    for(MUINT i = 0; i<2; i++)
    {
        helper.getSensorFps(sensorSlimVideo[i].scenario, (MINT32&)sensorSlimVideo[i].fps);
        MY_LOGD("Slim video(%d) FPS(%d)",
                i,
                sensorSlimVideo[i].fps);
    }
    //
    sensorSlimVideoselect.scenario = 0;
    sensorSlimVideoselect.fps = 0;
    for(MUINT i = 0; i<2; i++)
    {
        if(mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) <= sensorSlimVideo[i].fps)
        {
            if(sensorSlimVideoselect.fps > 0)
            {
                if(sensorSlimVideoselect.fps > sensorSlimVideo[i].fps)
                {
                    sensorSlimVideoselect.scenario = sensorSlimVideo[i].scenario;
                    sensorSlimVideoselect.fps = sensorSlimVideo[i].fps;
                }
            }
            else
            {
                sensorSlimVideoselect.scenario = sensorSlimVideo[i].scenario;
                sensorSlimVideoselect.fps = sensorSlimVideo[i].fps;
            }
        }
    }
    //
    if(sensorSlimVideoselect.fps > 0)
    {
        MY_LOGD("Use sensor scenario(%d) FPS(%d)",
                sensorSlimVideoselect.scenario,
                sensorSlimVideoselect.fps);
        sensorScen = sensorSlimVideoselect.scenario;
    }
    else
    {
        MY_LOGE("No sensor scenario FPS >= %d",
                mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE));
        sensorScen = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
        return UNKNOWN_ERROR;
    }
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
DefaultFlowControl::
needReconstructRecordingPipe()
{
    MBOOL ret = MFALSE;
    MSize paramSize;
    mpParamsManagerV3->getParamsMgr()->getVideoSize(&paramSize.w, &paramSize.h);

    if( (paramSize.w*paramSize.h <= IMG_1080P_SIZE && mb4K2KVideoRecord) ||
        (paramSize.w*paramSize.h > IMG_1080P_SIZE && !mb4K2KVideoRecord) )
    {
        ret = MTRUE;
    }
    MY_LOGD("param(%dx%d), b4K2K(%d), ret(%d)",
             paramSize.w, paramSize.h, mb4K2KVideoRecord, ret);
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DefaultFlowControl::
prepareVSSInfo(
    Vector< SettingSet >& vSettings,
    BufferList&           vDstStreams
)
{
    SettingSet tempSetting;
    IMetadata appSetting;
    IMetadata halSetting;

    //update default seting
    if (mpParamsManagerV3 != 0) {
        mpParamsManagerV3->updateRequestRecord(&appSetting);
        mpParamsManagerV3->updateRequest(&appSetting, mSensorParam.mode, CAMERA3_TEMPLATE_VIDEO_SNAPSHOT);
        updateRequestSetting(&appSetting, &halSetting);
    } else {
        MY_LOGE("Fail to update app setting.");
    }

    //update hal seting
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_REQUIRE_EXIF);
        entry.push_back(true, Type2Type< MUINT8 >());
        halSetting.update(entry.tag(), entry);
    }
    //update crop info
    {
        MRect reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion;
        mpParamsManagerV3->getCropRegion(reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion);
        //
        {
            IMetadata::IEntry entry(MTK_SCALER_CROP_REGION);
            entry.push_back(
                    reqCropRegion,
                    Type2Type<MRect>());
            appSetting.update(entry.tag(), entry);
        }
        {
            IMetadata::IEntry entry(MTK_P1NODE_SENSOR_CROP_REGION);
            entry.push_back(
                    reqSensorPreviewCropRegion,
                    Type2Type<MRect>());
            halSetting.update(entry.tag(), entry);
        }
    }

    tempSetting.appSetting = appSetting;
    tempSetting.halSetting = halSetting;

    vSettings.push(tempSetting);

    // stream
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_RESIZER, false);
     if(mLPBConfigParams.enableLCS)
        prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_LCSO, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_00, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_01, false);

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DefaultFlowControl::
prepareVideoInfo(
    Vector< SettingSet >& vSettings,
    BufferList&           vDstStreams
)
{
    SettingSet tempSetting;
    IMetadata appSetting;
    IMetadata halSetting;

    //update default seting
    if (mpParamsManagerV3 != 0) {
        mpParamsManagerV3->updateRequestRecord(&appSetting);
        mpParamsManagerV3->updateRequest(&appSetting, mSensorParam.mode, CAMERA3_TEMPLATE_VIDEO_RECORD);
        updateRequestSetting(&appSetting, &halSetting);
    } else {
        MY_LOGE("Fail to update app setting.");
    }

    MINT fps = (mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) <= mSensorParam.fps) ?
                mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) : mSensorParam.fps;
    MSize vdoSize;
    mpParamsManagerV3->getParamsMgr()->getVideoSize(&vdoSize.w, &vdoSize.h);

    if(decide_smvr_directlink(vdoSize.w, vdoSize.h, fps))
    {
        MY_LOGD("%dx%d@%d -> direct link",vdoSize.w, vdoSize.h, fps);
        mpParamsManagerV3->getParamsMgr()->set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, MtkCameraParameters::PIXEL_FORMAT_BITSTREAM);
    }
    else
    {
        MY_LOGD("%dx%d@%d -> non direct link",vdoSize.w, vdoSize.h, fps);
        mpParamsManagerV3->getParamsMgr()->set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420P);
        fps = 0;
        vdoSize.w = 0;
        vdoSize.h = 0;
    }

    //update hal seting
    {
        IMetadata::IEntry entry(MTK_P2NODE_HIGH_SPEED_VDO_FPS);
        entry.push_back(
                fps,
                Type2Type< MINT32 >());
        halSetting.update(entry.tag(), entry);
    }
    {
        IMetadata::IEntry entry(MTK_P2NODE_HIGH_SPEED_VDO_SIZE);
        entry.push_back(
                vdoSize,
                Type2Type< MSize >());
        halSetting.update(entry.tag(), entry);
    }
    //update crop info
    {
        MRect reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion;
        mpParamsManagerV3->getCropRegion(reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion);
        //
        {
            IMetadata::IEntry entry(MTK_SCALER_CROP_REGION);
            entry.push_back(
                    reqCropRegion,
                    Type2Type<MRect>());
            appSetting.update(entry.tag(), entry);
        }
        {
            IMetadata::IEntry entry(MTK_P1NODE_SENSOR_CROP_REGION);
            entry.push_back(
                    reqSensorPreviewCropRegion,
                    Type2Type<MRect>());
            halSetting.update(entry.tag(), entry);
        }
    }

    tempSetting.appSetting = appSetting;
    tempSetting.halSetting = halSetting;

    vSettings.push(tempSetting);

    // stream
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_RESIZER, false);
    if(mLPBConfigParams.enableLCS)
        prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_LCSO, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_00, false);
    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_01, false);

    return OK;
}

