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

#define LOG_TAG "MtkCam/LPipelineCap"

//
#include "MyUtils.h"
#include "PipelineBuilderCapture.h"
#include "LegacyPipelineCapture.h"

#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/hwnode/P2Node.h>
#include <mtkcam/pipeline/hwnode/JpegNode.h>

using namespace android;
using namespace NSCam;
using namespace NSCam::Utils;
using namespace NSCam::v3;
using namespace NSCam::v3::NSPipelineContext;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;

typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator  HalMetaStreamBufferAllocatorT;

/******************************************************************************
*
*******************************************************************************/
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
*******************************************************************************/
LegacyPipelineCapture::
LegacyPipelineCapture(MINT32 openId)
{
    mLogLevel = 1;
    mOpenId = openId;
}


/******************************************************************************
*
*******************************************************************************/
LegacyPipelineCapture::
~LegacyPipelineCapture()
{}


/******************************************************************************
*
*******************************************************************************/
MERROR
LegacyPipelineCapture::
submitSetting(
    MINT32    const requestNo,
    IMetadata& appMeta,
    IMetadata& halMeta,
    ResultSet* pResultSet
)
{
    CAM_TRACE_FMT_BEGIN("CaptureSubmitSetting No%d",requestNo);
    Mutex::Autolock _l(mLock);
    //
    if( mpPipelineContext == NULL || mpRequestBuilder == NULL)
    {
        MY_LOGW("mpPipelineContext or mpRequestBuilder == NULL, skip this time");
        CAM_TRACE_FMT_END();
        return OK;
    }
    //
    if( !trySetMetadata<MINT32>(halMeta, MTK_PIPELINE_UNIQUE_KEY, mTimestamp) )
        MY_LOGE("set unique key failed");

    sp<HalMetaStreamBuffer> pHalMetaStreamBuffer =
           HalMetaStreamBufferAllocatorT(
                queryMetaStreamInfo(eSTREAMID_META_HAL_CONTROL).get()
           )(halMeta);

    sp<IMetaStreamBuffer> pAppMetaStreamBuffer =
           HalMetaStreamBufferAllocatorT(
                queryMetaStreamInfo(eSTREAMID_META_APP_CONTROL).get()
           )(appMeta);

    mpRequestBuilder->setMetaStreamBuffer(
                eSTREAMID_META_APP_CONTROL,
                pAppMetaStreamBuffer
                );
    mpRequestBuilder->setMetaStreamBuffer(
                eSTREAMID_META_HAL_CONTROL,
                pHalMetaStreamBuffer
                );

    if( pResultSet )
    {
        for( auto i = 0; i < pResultSet->size(); i++ )
        {
            StreamId_T const streamId = pResultSet->keyAt(i);
            sp<IMetaStreamBuffer> pMetaStreamBuffer =
                HalMetaStreamBufferAllocatorT(
                        queryMetaStreamInfo(streamId).get()
                        )(pResultSet->valueAt(i));
            //
            mpRequestBuilder->setMetaStreamBuffer(
                    streamId, pMetaStreamBuffer
                    );
        }
    }

    sp<IPipelineFrame> pFrame =
        mpRequestBuilder->build(requestNo, mpPipelineContext);

    if( ! pFrame.get() ) {
        MY_LOGE("build request failed");
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }

    mpPipelineContext->queue(pFrame);

    CAM_TRACE_FMT_END();
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MERROR
LegacyPipelineCapture::
submitRequest(
    MINT32    const   /*requestNo*/,
    IMetadata&        /*appMeta*/,
    IMetadata&        /*halMeta*/,
    Vector<BufferSet> /*vDstStreams*/
)
{
    MY_LOGE("not supported yet");
    return UNKNOWN_ERROR;
}


/******************************************************************************
*
*******************************************************************************/
MVOID
LegacyPipelineCapture::
setConfiguration(
    Configuration const& config
)
{
    CAM_TRACE_NAME("CaptureSetConfig");
    mConfiguration = config;
    //
    struct update_stream
    {
        MVOID   operator()(LegacyPipelineCapture* lp, sp<IMetaStreamInfo> pInfo) {
                    if( pInfo != 0 ) { lp->setMetaStreamInfo(pInfo); }
                }
        MVOID   operator()(LegacyPipelineCapture* lp, sp<IImageStreamInfo> pInfo) {
                    if( pInfo != 0 ) { lp->setImageStreamInfo(pInfo); }
                }
    };
    // in
    update_stream()(this, config.pControlMeta_App);
    update_stream()(this, config.pControlMeta_Hal);
    update_stream()(this, config.pImage_ImgoRawSrc);

    // out
    update_stream()(this, config.pResultMeta_P1_App);
    update_stream()(this, config.pResultMeta_P1_Hal);
    update_stream()(this, config.pResultMeta_P2_App);
    update_stream()(this, config.pResultMeta_P2_Hal);
    update_stream()(this, config.pResultMeta_Jpeg_App);

    update_stream()(this, config.pImage_RrzoRaw);
    update_stream()(this, config.pImage_ImgoRaw);
    update_stream()(this, config.pImage_LcsoRaw);
    update_stream()(this, config.pImage_Yuv00);
    update_stream()(this, config.pImage_YuvJpeg);
    update_stream()(this, config.pImage_YuvThumbnail);
    update_stream()(this, config.pImage_Jpeg);
}

/******************************************************************************
*
*******************************************************************************/
MERROR
LegacyPipelineCapture::
waitUntilDrainedAndFlush()
{
    MERROR err = OK;
    if( !mpPipelineContext.get() )
    {
        MY_LOGW("get mpPipelineContext fail");
        return UNKNOWN_ERROR;
    }

    //P1
    {
        android::sp< NodeActor<NSCam::v3::P1Node> > nodeActor = NULL;
        err = waitUntilNodeDrainedAndFlush(eNODEID_P1Node, nodeActor);
        if( err != OK )
        {
            MY_LOGW("get wait until node(%d) drained and flush fail", eNODEID_P1Node);
            return err;
        }
    }
    //P2
    {
        android::sp< NodeActor<NSCam::v3::P2Node> > nodeActor = NULL;
        err = waitUntilNodeDrainedAndFlush(eNODEID_P2Node, nodeActor);
        if( err != OK )
        {
            MY_LOGW("get wait until node(%d) drained and flush fail", eNODEID_P2Node);
            return err;
        }
    }
    //Jpeg
    {
        android::sp< NodeActor<NSCam::v3::JpegNode> > nodeActor = NULL;
        err = waitUntilNodeDrainedAndFlush(eNODEID_JpegNode, nodeActor);
        if( err != OK )
        {
            MY_LOGW("get wait until node(%d) drained and flush fail", eNODEID_JpegNode);
            return err;
        }
    }

    return err;
}

/******************************************************************************
*
*******************************************************************************/
template <typename _Node_>
MERROR
LegacyPipelineCapture::
waitUntilNodeDrainedAndFlush(
    NodeId_T const nodeId,
    android::sp< NodeActor<_Node_> >& pNodeActor)
{
    MERROR err = mpPipelineContext->queryNodeActor(nodeId, pNodeActor);
    if( err != NAME_NOT_FOUND )
    {
        if( err != OK || pNodeActor == NULL )
        {
            MY_LOGW("get NodeActor(%d) fail", nodeId);
            return err;
        }
        //
        err = mpPipelineContext->waitUntilNodeDrained(nodeId);
        if( err != OK )
        {
            MY_LOGW("wait until node(%d) drained fail", nodeId);
            return err;
        }
        //
        IPipelineNode* node = pNodeActor->getNode();
        if( node == NULL )
        {
            MY_LOGW("get node(%d) fail", nodeId);
            return UNKNOWN_ERROR;
        }
        //
        err = node->flush();
        if( err != OK )
        {
            MY_LOGW("flush node(%d) fail", nodeId);
            return err;
        }
    }
    return OK;
}
