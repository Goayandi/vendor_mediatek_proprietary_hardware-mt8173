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

#define LOG_TAG "MtkCam/LPipelinePrv"

//
#include "MyUtils.h"
#include "PipelineBuilderPreview.h"
#include "LegacyPipelinePreview.h"

#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

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
#define FUNC_START                  MY_LOGD("+")
#define FUNC_END                    MY_LOGD("-")
//
/******************************************************************************
*
*******************************************************************************/
LegacyPipelinePreview::
LegacyPipelinePreview(MINT32 openId)
{
    mOpenId = openId;
}


/******************************************************************************
*
*******************************************************************************/
LegacyPipelinePreview::
~LegacyPipelinePreview()
{}

/******************************************************************************
*
*******************************************************************************/
MERROR
LegacyPipelinePreview::
submitSetting(
    MINT32 const requestNo,
    IMetadata& appMeta,
    IMetadata& halMeta,
    ResultSet* /*pResultSet*/
)
{
    MY_LOGD_IF( mLogLevel >= 1, "submitSetting %d +", requestNo);
    CAM_TRACE_FMT_BEGIN("LP:[Prv]submitSetting %d",requestNo);
    Mutex::Autolock _l(mLock);
    //
    if( mpPipelineContext == NULL || mpRequestBuilder == NULL)
    {
        MY_LOGW("mpPipelineContext or mpRequestBuilder == NULL, skip this time");
        CAM_TRACE_FMT_END();
        return OK;
    }
    //
    {
        MUINT8 test;
        IMetadata::IEntry entry = halMeta.entryFor(MTK_HAL_REQUEST_REPEAT);
        if( !entry.isEmpty() ) {
            test = entry.itemAt(0, Type2Type<MUINT8>());
            MY_LOGD_IF( test == 0, "[%d] not repeat.", requestNo);
        }
    }
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

    sp<IPipelineFrame> pFrame =
        mpRequestBuilder->build(requestNo, mpPipelineContext);

    if( ! pFrame.get() ) {
        MY_LOGE("build request failed");
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }

    mpPipelineContext->queue(pFrame);
    //
    MY_LOGD_IF( mLogLevel >= 1, "submitSetting %d -", requestNo);
    CAM_TRACE_FMT_END();
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
static
MERROR
wait_and_get_buffer(
    MINT32 const             requestNo,
    sp<StreamBufferProvider> pProvider,
    RequestBuilder&          builder
)
{
    if ( pProvider == nullptr ) {
        CAM_LOGE("Null provider.");
        return UNKNOWN_ERROR;
    }
    //
    sp<HalImageStreamBuffer> buffer;
    pProvider->dequeStreamBufferAndWait(
        requestNo,
        pProvider->queryImageStreamInfo(),
        buffer
    );

    if ( buffer == nullptr ) {
        CAM_LOGW("Null HalImageStreamBuffer.");
        return UNKNOWN_ERROR;
    }
    builder.setImageStreamBuffer(
            pProvider->queryImageStreamInfo()->getStreamId(),
            buffer
        );
    //
    return OK;
}

MERROR
LegacyPipelinePreview::
submitRequest(
    MINT32 const        requestNo,
    IMetadata&          appMeta,
    IMetadata&          halMeta,
    Vector<BufferSet>   vDstStreams
)
{
    MY_LOGD_IF( 1, "submitRequest %d vDstStreams:%d +", requestNo, vDstStreams.size());
    CAM_TRACE_FMT_BEGIN("LP:[Prv]submitRequest No%d",requestNo);
    Mutex::Autolock _l(mLock);
    //
    if( mpPipelineContext == NULL )
    {
        MY_LOGW("mpPipelineContext == NULL, skip this time");
        CAM_TRACE_FMT_END();
        return OK;
    }

    RequestBuilder aRequestBuilder;
    sp<HalMetaStreamBuffer> pHalMetaStreamBuffer =
           HalMetaStreamBufferAllocatorT(
                queryMetaStreamInfo(eSTREAMID_META_HAL_CONTROL).get()
           )(halMeta);

    sp<IMetaStreamBuffer> pAppMetaStreamBuffer =
           HalMetaStreamBufferAllocatorT(
                queryMetaStreamInfo(eSTREAMID_META_APP_CONTROL).get()
           )(appMeta);

    // p1 node
    {
        MBOOL aNeedP1 = false;
        IOMap p1_Image_IOMap;
        for ( size_t i = 0; i < vDstStreams.size(); ++i ) {
            switch( vDstStreams[i].streamId ) {
                case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:
                case eSTREAMID_IMAGE_PIPE_RAW_RESIZER:
                case eSTREAMID_IMAGE_PIPE_RAW_LCSO:
                    aNeedP1 = true;
                    p1_Image_IOMap.addOut(vDstStreams[i].streamId);
                    if ( vDstStreams[i].criticalBuffer ) {
                        MY_LOGD_IF(0, "waitAndGetBuffer, requestNo(%d)", requestNo);
                        wait_and_get_buffer( requestNo, queryProvider(vDstStreams[i].streamId), aRequestBuilder );
                    }
                    break;
                default:
                    break;
            };
        }
        if ( aNeedP1 ) {
            aRequestBuilder.setIOMap(
                    eNODEID_P1Node,
                    IOMapSet().add(
                        p1_Image_IOMap
                        ),
                    vIOMapInfo.valueFor(eNODEID_P1Node)
                    );
            aRequestBuilder.setRootNode(
                NodeSet().add(eNODEID_P1Node)
                );
        } else {
            MY_LOGE("request:%d No P1 node. preview pipeline does not support memory in.", requestNo);
            CAM_TRACE_FMT_END();
            return BAD_VALUE;
        }
    }

    // p2 node
    {
        MBOOL aNeedP2       = false;
        MBOOL aHasResizeRaw = false;
        MBOOL aHasLcso      = false;
        MBOOL aHasFullRaw   = false;
        IOMap p2_Image_IOMap;
        for ( size_t i = 0; i < vDstStreams.size(); ++i ) {
            switch( vDstStreams[i].streamId ) {
                case eSTREAMID_IMAGE_PIPE_YUV_00:
                case eSTREAMID_IMAGE_PIPE_YUV_01:
                case eSTREAMID_IMAGE_YUV_FD:
                    aNeedP2 = true;
                    p2_Image_IOMap.addOut(vDstStreams[i].streamId);
                    if ( vDstStreams[i].criticalBuffer ) {
                        wait_and_get_buffer( requestNo, queryProvider(vDstStreams[i].streamId), aRequestBuilder );
                    }
                    break;
                case eSTREAMID_IMAGE_PIPE_RAW_RESIZER:
                    aHasResizeRaw = true;
                    break;
                case eSTREAMID_IMAGE_PIPE_RAW_LCSO:
                    aHasLcso = true;
                    break;
                case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:
                    aHasFullRaw = true;
                    break;
                default:
                    break;
            };
        }
        if ( aNeedP2 ) {
            if ( aHasLcso ) p2_Image_IOMap.addIn(eSTREAMID_IMAGE_PIPE_RAW_LCSO);
            if ( aHasResizeRaw ) p2_Image_IOMap.addIn(eSTREAMID_IMAGE_PIPE_RAW_RESIZER);
            else if ( aHasFullRaw ){
                p2_Image_IOMap.addIn(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
                MY_LOGW("request:%d No resized raw as p2 input, use imgo instead.", requestNo);
            } else {
                MY_LOGW("request:%d No resized raw as p2 input, something wrong ?", requestNo);
            }

            // NSCam::v3::Pipeline_NodeId_T nodeId = ( vIOMapInfo.indexOfKey(eNODEID_P2Node) < 0)
            //                                         ? eNODEID_P2aNode : eNODEID_P2Node;
            aRequestBuilder.setIOMap(
                    eNODEID_P2Node,
                    IOMapSet().add(
                        p2_Image_IOMap
                        ),
                    vIOMapInfo.valueFor(eNODEID_P2Node)
                    );
            aRequestBuilder.setNodeEdges(
                NodeEdgeSet().addEdge(eNODEID_P1Node, eNODEID_P2Node)
                );
        } else {
            MY_LOGD("request:%d No P2 node", requestNo);
        }
    }

    aRequestBuilder.updateFrameCallback(mpResultProcessor);

    sp<IPipelineFrame> pFrame =
        aRequestBuilder
        .setMetaStreamBuffer(
                eSTREAMID_META_APP_CONTROL,
                pAppMetaStreamBuffer
                )
        .setMetaStreamBuffer(
                eSTREAMID_META_HAL_CONTROL,
                pHalMetaStreamBuffer
                )
        .build(requestNo, mpPipelineContext);

    if( ! pFrame.get() ) {
        MY_LOGE("build request failed");
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }

    mpPipelineContext->queue(pFrame);
    //
    MY_LOGD_IF( 1, "submitRequest %d vDstStreams:%d -", requestNo, vDstStreams.size());
    CAM_TRACE_FMT_END();
    return OK;
}
