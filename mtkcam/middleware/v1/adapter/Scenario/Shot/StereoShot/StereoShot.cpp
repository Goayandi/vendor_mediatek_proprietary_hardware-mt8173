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

#define LOG_TAG "StereoShot"
//
#include "StereoShot.h"
// shot mode enum.
#include <mtkcam/def/Modes.h>
//
#include <mtkcam/middleware/v1/camshot/_callbacks.h>
// support createRawImageStreamInfo and createImageStreamInfo
#include <mtkcam/middleware/v1/camshot/CamShotUtils.h>
//
#include <mtkcam/drv/IHalSensor.h>
//
#include <mtkcam/utils/fwk/MtkCamera.h>
//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/hw/CamManager.h>

#include <mtkcam/middleware/v1/LegacyPipeline/ILegacyPipeline.h>
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProvider.h>
#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProviderFactory.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineBuilder.h>
// stereo
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/StereoLegacyPipelineBuilder.h>
#include <mtkcam/middleware/v1/LegacyPipeline/NodeId.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/BayerAndBayer/vsdof/StereoPipelineData.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/StereoLegacyPipelineDef.h>
//
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/drv/iopipe/SImager/ISImagerDataTypes.h>
// for debug use
#include <mtkcam/utils/std/Misc.h>
#include <mtkcam/utils/std/Trace.h>
#include <chrono>
#include <string>

//#include <met_tag.h>
using namespace android;
using namespace NSCam::Utils;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace NSCamHW;
using namespace NSShot;
using namespace NSCamShot;
using namespace NS3Av3;

#define CHECK_OBJECT(x)  do{                                        \
    if (x == nullptr) { MY_LOGE("Null %s Object", #x); return MFALSE;} \
} while(0)
/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

#include <mtkcam/utils/std/Log.h> 

#define FUNC_START  MY_LOGD("+")
#define FUNC_END    MY_LOGD("-")
#define FUNC_NAME   MY_LOGD("")
//
// in stereo shot, it needs 8 image buffer.
// Raw*3, Bokeh image, clean image, JPS, depth map, and extra data.
#define RESOURCE_CONTAINER_ID 628
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
extern
sp<IShot>
createInstance_StereoShot(
    char const*const    pszShotName,
    uint32_t const      u4ShotMode,
    int32_t const       i4OpenId
)
{
    MY_LOGE("not implemented!");
    return  NULL;
}
/******************************************************************************
 *
 ******************************************************************************/
StereoShot::
StereoShot(
    char const*const pszShotName,
    uint32_t const u4ShotMode,
    int32_t const i4OpenId,
    sp<NSCam::v1::NSLegacyPipeline::IFlowControl> spIFlowControl
)
    : ImpShot(pszShotName, u4ShotMode, i4OpenId)
    , mpIFlowControl(spIFlowControl)
{
    MY_LOGD("ctor(0x%x)", this);

    MINT32 value = ::property_get_int32("debug.vsdof.dumpcapturedata", 0);
    if(value>0)
    {
        mbEnableDumpCaptureData = MTRUE;
    }

    value = ::property_get_int32("debug.bmdenoise.fasts2s", 0);
    if(value>0)
    {
        mbEnableFastShot2Shot = MTRUE;
    }

    value = ::property_get_int32("bmdenoise.tuning.dump", 0);
    if(value>0)
    {
        mbBMDenoiseTuningDump = MTRUE;
    }

    MY_LOGD("mbEnableDumpCaptureData:%d, mbEnableFastShot2Shot:%d, mbBMDenoiseTuningDump:%d",
        mbEnableDumpCaptureData,
        mbEnableFastShot2Shot,
        mbBMDenoiseTuningDump
    );
}
/******************************************************************************
 *
 ******************************************************************************/
StereoShot::
~StereoShot()
{
    MY_LOGD("dctor(0x%x)", this);
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoShot::
onDestroy()
{
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoShot::
setDstStream(MINT32 reqId, std::list<StreamId_T> dstStreams)
{
    mvCBList.push_back(reqId);
    mvReqCBList.add(reqId, dstStreams);
    //met_tag_start(0, TAKE_PICTURE);
}
/******************************************************************************
 *
 ******************************************************************************/
bool
StereoShot::
sendCommand(
    uint32_t const  cmd,
    MUINTPTR const  arg1,
    uint32_t const  arg2,
    uint32_t const  arg3
)
{
    CAM_TRACE_CALL();
    bool ret = true;
    //
    switch  (cmd)
    {
    //  This command is to reset this class. After captures and then reset,
    //  performing a new capture should work well, no matter whether previous
    //  captures failed or not.
    //
    //  Arguments:
    //          N/A
    /*case eCmd_reset:
        ret = onCmd_reset();
        break;

    //  This command is to perform capture.
    //
    //  Arguments:
    //          N/A
    case eCmd_capture:
        ret = onCmd_capture();
        break;

    //  This command is to perform cancel capture.
    //
    //  Arguments:
    //          N/A
    case eCmd_cancel:
        onCmd_cancel();
        break;
    case eCmd_setDofLevel:
        miBokehLevel = arg2;
	    MY_LOGD("Set bokeh level(%d)", miBokehLevel);
		break;*/
    // temp solution to fix capture does not know af point
    /*case eCmd_setAFRegion:
        maAFRegion = ((MINT32*)arg1);
        MY_LOGD("AF region xmin(%d) ymin(%d) xmax(%d) ymax(%d) weight(%d)",
                maAFRegion[0],
                maAFRegion[1],
                maAFRegion[2],
                maAFRegion[3],
                maAFRegion[4]);
        break;*/
    //
    default:
        ret = ImpShot::sendCommand(cmd, arg1, arg2, arg3);
    }
    //
    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
bool
StereoShot::
onCmd_reset()
{
    CAM_TRACE_CALL();
    bool ret = true;
    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
bool
StereoShot::
onCmd_capture()
{
    FUNC_START;
    bool ret = true;
    /*CAM_TRACE_CALL();
    bool ret = true;
    if(eShotMode_ZsdShot == mu4ShotMode)
    {
        ZsdCapture();
    }
    else
    {
        MY_LOGE("Unsupported shot mode.");
    }*/
    FUNC_END;
    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoShot::
onCmd_cancel()
{
    CAM_TRACE_CALL();
}
/******************************************************************************
 * metadata event handler
 ******************************************************************************/
void
StereoShot::
onResultReceived(
    MUINT32    const requestNo,
    StreamId_T const streamId,
    MBOOL      const errorResult,
    IMetadata  const result)
{
    CAM_TRACE_FMT_BEGIN("onMetaReceived No%d,StreamID  %#" PRIxPTR, requestNo,streamId);
    MY_LOGD("requestNo %d, stream %#" PRIxPTR, requestNo, streamId);

    Mutex::Autolock _l(mMetadataLock);

    if (streamId == eSTREAMID_META_APP_FULL)
    {
        mAppDone = MTRUE;

        ssize_t index = mvAppMetadataSet.indexOfKey(requestNo);
        if(index >= 0)
        {
            IMetadata resultAppMetadata = result;
            resultAppMetadata += mvAppMetadataSet.valueAt(index);
            if (StereoSettingProvider::is3rdParty())
                mpShotCallback->onCB_3rdPartyMetaData((MUINTPTR)&resultAppMetadata);
            else if(mbSupportDng)
                mpShotCallback->onCB_DNGMetaData((MUINTPTR)&resultAppMetadata);
        }
        else
        {
            if(StereoSettingProvider::is3rdParty())
                MY_LOGE("cannot find app metadata in capture 3rd Party mode requestNo(%d)", requestNo);
            else if(mbSupportDng)
                MY_LOGE("cannot find app metadata in capture Dng mode requestNo(%d)", requestNo);
        }

        mvAppMetadataSet.removeItem(requestNo);

    }
    else if (streamId == eSTREAMID_META_HAL_FULL)
    {
        mHalDone = MTRUE;
    }


    {
        if (mbCBShutter && streamId == eSTREAMID_META_APP_DYNAMIC_DEPTH)
        {
            // mbCBShutter = MFALSE;
            mpShotCallback->onCB_Shutter(true, 0);
        }
    }
    CAM_TRACE_FMT_END();
}
/******************************************************************************
 * image event handler
 ******************************************************************************/
MERROR
StereoShot::
onResultReceived(
    MUINT32    const requestNo,
    StreamId_T const streamId,
    MBOOL      const errorBuffer,
    android::sp<IImageBuffer>& pBuffer
)
{
    CAM_TRACE_CALL();
    Mutex::Autolock _l(mImgResultLock);
    MY_LOGD("image request(%d) streamID(%#x)", requestNo, streamId);
    CHECK_OBJECT(pBuffer);
    if (errorBuffer)
    {
        MY_LOGE("the content of buffer may not correct...");
    }
    pBuffer->lockBuf(LOG_TAG, eBUFFER_USAGE_SW_READ_MASK);

    uint8_t const* puBuf = (uint8_t const*)pBuffer->getBufVA(0);
    MUINT32 u4Size = pBuffer->getBitstreamSize();
    bool isLastFrame = false;

    if(eSTREAMID_IMAGE_PIPE_RAW16 != streamId){
        mvReqCBList.editValueFor(requestNo).remove(streamId);
    }

    MY_LOGD("mvReqCBList[%d].size=%d", requestNo, mvReqCBList.editValueFor(requestNo).size());

    if(mvReqCBList.editValueFor(requestNo).empty()){
        mvReqCBList.removeItem(requestNo);
        isLastFrame = true;
    }

    MSize size = pBuffer->getImgSize();
    string streamName = "";
    string fileEXT = "";
    string size_x = "0";
    string size_y = "0";
    const char* extra_data;
    switch (streamId)
    {
        // todo: needs to cb JPS, clean image and depth map.
        case eSTREAMID_IMAGE_PIPE_JPG_Bokeh:    // bokeh result
            streamName = "JPG_BOKEH";
            fileEXT = ".jpg";
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 0, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_COMPRESSED_IMAGE);
            break;
        case eSTREAMID_IMAGE_PIPE_JPG_JPS:      // jps
            streamName = "JPS";
            fileEXT = ".jpg";
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 1, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_JPS);
            // todo: get dbg info from stereo hal
            break;
        case eSTREAMID_IMAGE_PIPE_DEPTHMAPNODE_DEPTHMAPYUV: // depthmap
            streamName = "DEPTHMAP_";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".yuv";
            // for byte format, we need to compute size manually.
            u4Size = pBuffer->getBufSizeInBytes(0);
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 2, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_DEPTHMAP);
            break;
        case eSTREAMID_IMAGE_PIPE_JPG_CleanMainImg:
            streamName = "CLEAN_IMG";
            fileEXT = ".jpg";
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 3, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_STEREO_CLEAR_IMAGE);
            break;
        case eSTREAMID_IMAGE_PIPE_STEREO_DBG:
            streamName = "EXTRA_DBG";
            fileEXT = ".dbg";
            // for byte format, we need to compute size manually.
            extra_data = (const char*)puBuf;
            u4Size = strlen(extra_data)+1;
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 4, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_STEREO_DBG);

            // early notify for shot to shot user experience
            if (StereoSettingProvider::isDeNoise() && mbEnableFastShot2Shot){
                mpShotCallback->onCB_P2done();
            }
            break;
        case eSTREAMID_IMAGE_PIPE_STEREO_DBG_LDC:
            streamName = "ldc";
            fileEXT = ".dbg";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            // for byte format, we need to compute size manually.
            u4Size = pBuffer->getBufSizeInBytes(0);
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 5, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_STEREO_LDC);
            break;
        case eSTREAMID_IMAGE_PIPE_RAW_RESIZER:
            streamName = "RES_";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".raw";
            break;
        case eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01:
            streamName = "RES_01_";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".raw";
            break;
        case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:
            streamName = "OPA_";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".raw";
            break;
        case eSTREAMID_IMAGE_PIPE_RAW16:
            streamName = "raw16";
            fileEXT = ".dbg";
            mpShotCallback->onCB_Raw16Image(pBuffer.get());
            break;
        case eSTREAMID_IMAGE_JPEG:
            streamName = "JPG_DENOISE";
            fileEXT = ".jpg";
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 0, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_COMPRESSED_IMAGE);
            break;
        case eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN1YUV:
            streamName = "MAIN1_YUV_3RDPARTY";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".yuv";
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 0, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_STEREO_MAIN1_YUV);
            break;
        case eSTREAMID_IMAGE_PIPE_DUALYUVNODE_MAIN2YUV:
            streamName = "MAIN2_YUV_3RDPARTY";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".yuv";
            mpShotCallback->onCB_CompressedImage_packed(0, u4Size, puBuf, 0, isLastFrame, MTK_CAMERA_MSG_EXT_DATA_STEREO_MAIN2_YUV);
            break;
        case eSTREAMID_IMAGE_PIPE_DUALYUVNODE_THUMBNAIL:
            streamName = "YUV_THUMBNAIL";
            size_x = std::to_string(size.w)+std::string("x");
            size_y = std::to_string(size.h);
            fileEXT = ".yuv";
            // early notify for fast thumbnail callback
            if (StereoSettingProvider::isDeNoise()){
                mpShotCallback->onCB_PostviewClient(0, pBuffer.get());
            }
            break;
        default:
            MY_LOGW("not supported streamID(%#x)", streamId);
    }
    // dump data
    if(mbEnableDumpCaptureData)
    {
        MSize size = pBuffer->getImgSize();

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        msFilename = std::string("/sdcard/stereo/Cap/")+std::to_string(requestNo)+std::string("/");
        NSCam::Utils::makePath(msFilename.c_str(), 0660);

        std::string saveFileName = msFilename +
                                   streamName+
                                   size_x+
                                   size_y+
                                   std::string("_")+
                                   std::to_string(millis)+
                                   fileEXT;
        pBuffer->saveToFile(saveFileName.c_str());
    }
    if(mbBMDenoiseTuningDump && streamId == eSTREAMID_IMAGE_JPEG){
        MSize size = pBuffer->getImgSize();

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        msFilename = std::string("/sdcard/bmdenoise/")
            +std::to_string(requestNo)+std::string("_")+std::to_string(millis)+std::string("/DeNoise/");
        NSCam::Utils::makePath(msFilename.c_str(), 0660);

        std::string saveFileName = msFilename +
                                   streamName+
                                   size_x+
                                   size_y+
                                   std::string("_")+
                                   std::to_string(millis)+
                                   fileEXT;
        pBuffer->saveToFile(saveFileName.c_str());

        // move other tuning data to time-stamp specific folder
        msCMD = std::string("cp -R /sdcard/bmdenoise/")+std::to_string(requestNo)+std::string("/DeNoise/* ")
            + msFilename;
        MY_LOGD("%s", msCMD.c_str());
        system(msCMD.c_str());

        msCMD = std::string("rm -rf /sdcard/bmdenoise/")+std::to_string(requestNo)+std::string("/DeNoise/");
        MY_LOGD("%s", msCMD.c_str());
        system(msCMD.c_str());
    }
    pBuffer->unlockBuf(LOG_TAG);

    MY_LOGD("mvReqCBList.size=%d", mvReqCBList.size());

    // check if we can notify flowControl that one capture requests have done
    // notice: each capture request calls changeToPreviewStatus() once
    if(isLastFrame){
        mpIFlowControl->changeToPreviewStatus();

        if (StereoSettingProvider::isDeNoise() && !mbEnableFastShot2Shot){
            mpShotCallback->onCB_P2done();
        }

        //met_tag_end(0, TAKE_PICTURE);
    }
    return OK;
}

/******************************************************************************
*
 ******************************************************************************/
/*MUINT32
StereoShot::
getRotation() const
{
    return mShotParam.mu4Transform;
}*/
/******************************************************************************
 *
 ******************************************************************************/
String8
StereoShot::
getUserName()
{
    CAM_TRACE_CALL();
    return String8(LOG_TAG);
}