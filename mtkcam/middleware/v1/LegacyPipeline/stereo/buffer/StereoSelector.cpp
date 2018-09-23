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

#define LOG_TAG "MtkCam/StereoSelector"
//
#include "MyUtils.h"
//
#include <utils/RWLock.h>
#include <cutils/properties.h>
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoSelector.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoBufferSynchronizer.h>

using namespace android;
using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace NSCam::Utils;

using namespace NSCam::v3::Utils;
/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

#define MY_LOGD1(...)               MY_LOGD_IF(1<=mLogLevel, __VA_ARGS__)

#define FUNC_START                  MY_LOGD1("+")
#define FUNC_END                    MY_LOGD1("-")

/******************************************************************************
 *
 ******************************************************************************/
StereoSelector::
StereoSelector(char const* szCallerName)
    : mStreamId(-1)
    , mLogLevel(0)
    , mspSyncronizer(NULL)
    , mpPool(NULL)
    , mName(szCallerName)
{
    MY_LOGD("StereoSelector ctor");

    // get log level
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.STEREO.log", cLogLevel, "0");
    mLogLevel = atoi(cLogLevel);
    if ( mLogLevel == 0 ) {
        ::property_get("debug.STEREO.log.bufferpool", cLogLevel, "0");
        mLogLevel = atoi(cLogLevel);
    }
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoSelector::
selectResult(
    MINT32                          rRequestNo,
    Vector<MetaItemSet>             rvResult,
    Vector<BufferItemSet>           rvBuffers,
    MBOOL                           errorResult
)
{
    FUNC_START;

    if(mspSyncronizer.get() == NULL){
        MY_LOGE("mspSyncronizer not set! selectResult will always be false");
        return UNKNOWN_ERROR;
    }

    // Stereo selector always enque buffer and meta to StereoSynchronizer
#warning "FIXME. [N-Migration]should be un-marked"
#if 0
    MERROR ret = mspSyncronizer->enqueBuffer(
        rRequestNo,
        mStreamId,
        rvResult,
        rvBuffers
    );
#else
    MERROR ret = UNKNOWN_ERROR;
#endif
    if(ret != OK){
        MY_LOGE("something goes wrong when doing mspSyncronizer->enqueBuffer!");
    }

    MY_LOGD1("req:%d, buffer:%p, meta:%d, is being selected by StereoSelector", rRequestNo, rvBuffers.size(), rvResult.size());

    // Since StereoSelector uses every single buffer, always return true
    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoSelector::
getResult(
    MINT32&                          rRequestNo,
    Vector<MetaItemSet>&             rvResultMeta,
    Vector<BufferItemSet>&           rvBuffers
)
{
    FUNC_START;
#warning "FIXME. [N-Migration]should be un-marked"
#if 0
    // Stereo selector always deque buffer and meta from StereoSynchronizer
    MERROR ret = mspSyncronizer->dequeBuffer(
        rRequestNo,
        mStreamId,
        rvResultMeta,
        rpHeap
    );
#else
    MERROR ret = UNKNOWN_ERROR;
#endif
    if(ret == NOT_ENOUGH_DATA){
        MY_LOGD("can not deque from mspSyncronizer->dequeBuffer because it is force stopped");
        return BAD_VALUE;
    }

    if(ret != OK){
        MY_LOGE("can not deque from mspSyncronizer->dequeBuffer something went wrong!");
        return BAD_VALUE;
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoSelector::
getResultCapture(
    MINT32&                          rRequestNo,
    Vector<MetaItemSet>&             rvResultMeta,
    Vector<BufferItemSet>&           rvBuffers
)
{
    FUNC_START;
#warning "FIXME. [N-Migration]should be un-marked"
#if 0
    // Stereo selector always deque buffer and meta from StereoSynchronizer
    MERROR ret = mspSyncronizer->dequeBufferCapture(
        rRequestNo,
        mStreamId,
        rvResultMeta,
        rpHeap
    );
#else
    MERROR ret = UNKNOWN_ERROR;
#endif
    if(ret == NOT_ENOUGH_DATA){
        MY_LOGD("can not deque from mspSyncronizer->dequeBufferCapture because it is force stopped");
        return BAD_VALUE;
    }

    if(ret != OK){
        MY_LOGE("can not deque from mspSyncronizer->dequeBufferCapture something went wrong!");
        return BAD_VALUE;
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoSelector::
returnBuffer(
    BufferItemSet&    rBuffer
)
{
    return returnBufferToPool(rBuffer);
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoSelector::
flush()
{
    if(mspSyncronizer != 0){
        mspSyncronizer->flush();
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoSelector::
onLastStrongRef( const void* /*id*/)
{
    flush();
}
/******************************************************************************
 *
 ******************************************************************************/
android::status_t
StereoSelector::
setSynchronizer(
        android::sp<StereoBufferSynchronizer> pSynchronizer, StreamId_T streamId
    )
{
    mspSyncronizer = pSynchronizer;
    mStreamId = streamId;

    MY_LOGD("mspSyncronizer = %p, mStreamId=%d", mspSyncronizer.get(), mStreamId);

    return 0;
}
