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

#define LOG_TAG "ShotCallbackProcessor"
//
#include <mtkcam/def/BuiltinTypes.h>
#include <mtkcam/def/Errors.h>
//
#include "../image/IImageShotCallback.h"
#include "../metadata/IMetaShotCallback.h"
#include "ShotCallbackProcessor.h"
/******************************************************************************
 *
 ******************************************************************************/
#include <Misc.h>
#include <chrono>
#include <string>
/******************************************************************************
 *
 ******************************************************************************/
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

#define FUNC_START  MY_LOGD("+")
#define FUNC_END    MY_LOGD("-")
#define FUNC_NAME   MY_LOGD("")
#define WRITE_PERMISSION 0660
/******************************************************************************
 *
 ******************************************************************************/
using namespace NSCam;
using namespace NSCam::v1::NSLegacyPipeline;
/******************************************************************************
 *
 ******************************************************************************/
ShotCallbackProcessor::
ShotCallbackProcessor(
    const char* pszShotName,
    sp<IShotCallback>& shotCallback
)
{
    mShotName = pszShotName;
    mpShotCallback = shotCallback;
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.stereo.dumpcapturedata", cLogLevel, "0");
    MINT32 value = ::atoi(cLogLevel);
    if(value > 0)
    {
        mbDumpResult = MTRUE;
    }
    MY_LOGD("ctor(0x%x)", this);
}
/******************************************************************************
 *
 ******************************************************************************/
ShotCallbackProcessor::
~ShotCallbackProcessor()
{
    MY_LOGD("dctor(0x%x)", this);
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ShotCallbackProcessor::
addCallback(
    sp<IImageShotCallback>& imageCallback
)
{
    if(imageCallback == nullptr)
    {
        return BAD_VALUE;
    }
    mvImageCallbackPool.addCallback(imageCallback);
    if(mbDumpResult)
    {
        imageCallback->setDumpInfo(MTRUE, msFilename);
    }
    if(imageCallback->isNeedImageCount())
        miTotalImgCount++;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ShotCallbackProcessor::
addCallback(
    sp<IMetaShotCallback>& metaCallback
)
{
    if(metaCallback == nullptr)
    {
        return BAD_VALUE;
    }
    mvMetaCallbackPool.addCallback(metaCallback);
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ShotCallbackProcessor::
resetProcessor()
{
    if(mbDumpResult)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        msFilename = std::string("/sdcard/stereo/Cap/")+std::to_string(millis);
        MY_LOGD("filename(%s)", msFilename.c_str());
        NSCam::Utils::makePath(msFilename.c_str(), WRITE_PERMISSION);
    }
    mvImageCallbackPool.clearPool();
    mvMetaCallbackPool.clearPool();
    miImgCount = 0;
    miTotalImgCount = 0;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ShotCallbackProcessor::
destroy()
{
    mpShotCallback = nullptr;
    mvImageCallbackPool.clearPool();
    mvMetaCallbackPool.clearPool();
    return OK;
}
/******************************************************************************
 * metadata event handler
 ******************************************************************************/
void
ShotCallbackProcessor::
onResultReceived(
    MUINT32    const requestNo,
    StreamId_T const streamId,
    MBOOL      const errorResult,
    IMetadata  const result)
{
    Mutex::Autolock _l(mMetadataLock);

    CAM_TRACE_FMT_BEGIN("onMetaReceived No%d,StreamID  %#" PRIxPTR, requestNo,streamId);
    MY_LOGD("requestNo %d, stream %#" PRIxPTR, requestNo, streamId);
    //
    sp<IMetaShotCallback> callback = nullptr;
    if(mvMetaCallbackPool.getCallback(streamId, callback))
    {
        if(callback==nullptr)
        {
            MY_LOGE("callback is null");
            return;
        }
        if(errorResult)
        {
            callback->processFailData(requestNo, streamId, result);
        }
        else
        {
            callback->sendCallback(mpShotCallback, streamId, result);
        }
    }
    CAM_TRACE_FMT_END();
}
/******************************************************************************
 * image event handler
 ******************************************************************************/
MERROR
ShotCallbackProcessor::
onResultReceived(
    MUINT32    const requestNo,
    StreamId_T const streamId,
    MBOOL      const errorBuffer,
    android::sp<IImageBuffer>& pBuffer
)
{
    ATRACE_CALL();
    Mutex::Autolock _l(mImgResultLock);
    MY_LOGD("image request(%d) streamID(%#x)", requestNo, streamId);
    CHECK_OBJECT(pBuffer);
    //
    sp<IImageShotCallback> callback = nullptr;
    if(mvImageCallbackPool.getCallback(streamId, callback))
    {
        if(callback == nullptr)
            return UNKNOWN_ERROR;
        // check need to count image.
        MBOOL isFinalImage = MFALSE;
        if(callback->isNeedImageCount())
        {
            miImgCount++;
            if(miImgCount == miTotalImgCount)
            {
                isFinalImage = MTRUE;
            }
        }
        if(errorBuffer)
        {
            callback->processFailData(requestNo, streamId, pBuffer);
        }
        else
        {
            callback->sendCallback(mpShotCallback, streamId, pBuffer, isFinalImage);
        }
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
String8
ShotCallbackProcessor::
getUserName()
{
    ATRACE_CALL();
    return String8(LOG_TAG);
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ImageCallbackPool::
addCallback(
    sp<IImageShotCallback>& callback
)
{
    ssize_t index = mvImageShotCallbackPool.indexOfKey(callback->getStreamId());
    if(index >= 0)
    {
        mvImageShotCallbackPool.replaceValueAt(index, callback);
    }
    else
    {
        mvImageShotCallbackPool.add(callback->getStreamId(), callback);
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ImageCallbackPool::
clearPool(
)
{
    mvImageShotCallbackPool.clear();
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageCallbackPool::
getCallback(
    MINT32 streamId,
    sp<IImageShotCallback>& callback
)
{
    ssize_t index = mvImageShotCallbackPool.indexOfKey(streamId);
    if(index >= 0)
    {
        callback = mvImageShotCallbackPool.valueAt(index);
        return MTRUE;
    }
    else
    {
        return MFALSE;
    }
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
MetaCallbackPool::
addCallback(
    sp<IMetaShotCallback>& callback
)
{
    ssize_t index = mvMetaShotCallbackPool.indexOfKey(callback->getStreamId());
    if(index >= 0)
    {
        mvMetaShotCallbackPool.replaceValueAt(index, callback);
    }
    else
    {
        mvMetaShotCallbackPool.add(callback->getStreamId(), callback);
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
MetaCallbackPool::
clearPool(
)
{
    mvMetaShotCallbackPool.clear();
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
MetaCallbackPool::
getCallback(
    MINT32 streamId,
    sp<IMetaShotCallback>& callback
)
{
    ssize_t index = mvMetaShotCallbackPool.indexOfKey(streamId);
    if(index >= 0)
    {
        callback = mvMetaShotCallbackPool.valueAt(index);
        return MTRUE;
    }
    else
    {
        return MFALSE;
    }
}