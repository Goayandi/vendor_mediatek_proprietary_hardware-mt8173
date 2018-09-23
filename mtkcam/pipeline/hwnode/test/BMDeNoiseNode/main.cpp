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
#define LOG_TAG "BMDeNoiseNodeTest"
//
#include <gtest/gtest.h>
//
#include <Log.h>
//
#include <stdlib.h>
#include <utils/Errors.h>
#include <utils/List.h>
#include <utils/RefBase.h>
//
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#include <v3/pipeline/IPipelineDAG.h>
#include <v3/pipeline/IPipelineNode.h>
#include <v3/pipeline/IPipelineNodeMapControl.h>
#include <v3/pipeline/IPipelineFrameControl.h>
//
#include <v3/utils/streambuf/StreamBufferPool.h>
#include <v3/utils/streambuf/StreamBuffers.h>
#include <v3/utils/streaminfo/MetaStreamInfo.h>
#include <v3/utils/streaminfo/ImageStreamInfo.h>
//
#include <imagebuf/IIonImageBufferHeap.h>
//
#include <IHalSensor.h>
//
#include <v3/hwnode/BMDeNoiseNode.h>
#include <metadata/IMetadataProvider.h>
//
#include <LegacyPipeline/StreamId.h>
#include <LegacyPipeline/NodeId.h>

#include <mtkcam/feature/stereo/hal/stereo_common.h>
//
using namespace NSCam;
using namespace v3;
using namespace NSCam::v3::Utils;
using namespace android;

/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
//
#define FUNCTION_IN     MY_LOGD_IF(1, "+");
/******************************************************************************
 *
 ******************************************************************************/
void help()
{
    printf("BMDeNoiseNode Node <test>\n ");
}

/******************************************************************************
 *
 ******************************************************************************/

namespace {
    //
    sp<HalImageStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalImageRaw;
    sp<HalImageStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalImageRaw_main2;
    sp<HalImageStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalImageYuv0;
    sp<HalImageStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalImageWarpingMarix;
    sp<HalImageStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalImageDisparityMap;
    //
    sp<HalMetaStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalMetadataRequest;
    sp<HalMetaStreamBuffer::Allocator::StreamBufferPoolT> mpPool_HalMetadataResult;
    //
    typedef NSCam::v3::Utils::IStreamInfoSetControl       IStreamInfoSetControlT;
    android::sp<IStreamInfoSetControlT> mpStreamInfoSet;
    android::sp<IPipelineNodeMapControl>mpPipelineNodeMap;
    android::sp<IPipelineDAG>           mpPipelineDAG;
    android::sp<BMDeNoiseNode>          mpNode1;
    //
    static int gSensorId = 0;
    //
    IHalSensor* mpSensorHalObj;

    IMetadata halRequest;
    IMetadata halRequest_main2;
}; // namespace

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
android::sp<IPipelineNodeMapControl>
getPipelineNodeMapControl()
{
    return mpPipelineNodeMap;
}


/******************************************************************************
 *
 ******************************************************************************/
android::sp<IStreamInfoSet>
getStreamInfoSet()
{
    return mpStreamInfoSet;
}


/******************************************************************************
 *
 ******************************************************************************/
android::sp<IPipelineNodeMap>
getPipelineNodeMap()
{
    return mpPipelineNodeMap;
}


/******************************************************************************
 *
 ******************************************************************************/
android::sp<IPipelineDAG>
getPipelineDAG()
{
    return mpPipelineDAG;
}


/******************************************************************************
 *
 ******************************************************************************/
void clear_global_var()
{
    mpPipelineNodeMap = NULL;
    mpStreamInfoSet = NULL;
    mpPipelineDAG = NULL;

    mpPool_HalImageRaw->uninitPool("Tester");
    mpPool_HalImageRaw = NULL;

    mpPool_HalImageRaw_main2->uninitPool("Tester");
    mpPool_HalImageRaw_main2 = NULL;

    mpPool_HalImageYuv0->uninitPool("Tester");
    mpPool_HalImageYuv0 = NULL;

    mpPool_HalImageWarpingMarix->uninitPool("Tester");
    mpPool_HalImageWarpingMarix = NULL;

    mpPool_HalImageDisparityMap->uninitPool("Tester");
    mpPool_HalImageDisparityMap = NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
void prepareConfig(BMDeNoiseNode::ConfigParams &params)
{
    printf("prepareConfig + \n");
    //
    //
    mpStreamInfoSet = IStreamInfoSetControl::create();
    mpPipelineNodeMap = IPipelineNodeMapControl::create();
    mpPipelineDAG = IPipelineDAG::create();

    //
    sp<IMetaStreamInfo>  pAppMetaResult   = 0;
    sp<IMetaStreamInfo>  pAppMetaRequest  = 0;
    sp<IMetaStreamInfo>  pHalMetaResult  = 0;
    sp<IMetaStreamInfo>  pHalMetaRequest  = 0;
    //
    sp<ImageStreamInfo>  pHalImageInRaw   = 0;
    sp<ImageStreamInfo>  pHalImageInRaw_main2   = 0;
    sp<ImageStreamInfo>  pHalImageOutYuv0 = 0;
    sp<ImageStreamInfo>  pHalImageWarpingMatrix = 0;
    sp<ImageStreamInfo>  pHalImageDisparityMap = 0;

    #define addBufPlane(planes, height, stride)                                 \
        do{                                                                         \
            IImageStreamInfo::BufPlane bufPlane= { (height) * (stride), (stride) }; \
            planes.push_back(bufPlane);                                             \
        }while(0)

    printf("create image buffer\n");
    //Hal:Image: src
    {
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_RAW_OPAQUE;
        MSize const imgSize(4208, 3120);//[++]
        MINT const format = eImgFmt_BAYER10;
        MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN |
                            eBUFFER_USAGE_HW_CAMERA_READWRITE
                            ;
        IImageStreamInfo::BufPlanes_t bufPlanes;
        IImageStreamInfo::BufPlane bufPlane;
        //
        bufPlane.rowStrideInBytes = imgSize.w * 10 / 8;
        bufPlane.sizeInBytes = bufPlane.rowStrideInBytes * imgSize.h;
        bufPlanes.push_back(bufPlane);
        //
        sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
            "Hal:Image:P1_fullraw",
            streamId,
            eSTREAMTYPE_IMAGE_INOUT,
            5, 1,
            usage, format, imgSize, bufPlanes
        );
        pHalImageInRaw = pStreamInfo;
        //
        //
        size_t bufStridesInBytes[3] = {0};
        size_t bufBoundaryInBytes[3]= {0};
        for (size_t i = 0; i < bufPlanes.size(); i++) {
            bufStridesInBytes[i] = bufPlanes[i].rowStrideInBytes;
        }
        IIonImageBufferHeap::AllocImgParam_t const allocImgParam(
            format, imgSize,
            bufStridesInBytes, bufBoundaryInBytes,
            bufPlanes.size()
        );

        mpPool_HalImageRaw = new HalImageStreamBuffer::Allocator::StreamBufferPoolT(
            pStreamInfo->getStreamName(),
            HalImageStreamBuffer::Allocator(pStreamInfo.get(), allocImgParam)
        );
        MERROR err = mpPool_HalImageRaw->initPool("Tester", pStreamInfo->getMaxBufNum(), pStreamInfo->getMinInitBufNum());
        if  ( err ) {
            MY_LOGE("mpPool_HalImageRaw init fail");
        }
    }
    {
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01;
        MSize const imgSize(4208, 3120);//[++]
        MINT const format = eImgFmt_BAYER10;
        MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN |
                            eBUFFER_USAGE_HW_CAMERA_READWRITE
                            ;
        IImageStreamInfo::BufPlanes_t bufPlanes;
        IImageStreamInfo::BufPlane bufPlane;
        //
        bufPlane.rowStrideInBytes = imgSize.w * 10 / 8;
        bufPlane.sizeInBytes = bufPlane.rowStrideInBytes * imgSize.h;
        bufPlanes.push_back(bufPlane);
        //
        sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
            "Hal:Image:P1_fullraw_main2",
            streamId,
            eSTREAMTYPE_IMAGE_INOUT,
            5, 1,
            usage, format, imgSize, bufPlanes
        );
        pHalImageInRaw_main2 = pStreamInfo;
        //
        //
        size_t bufStridesInBytes[3] = {0};
        size_t bufBoundaryInBytes[3]= {0};
        for (size_t i = 0; i < bufPlanes.size(); i++) {
            bufStridesInBytes[i] = bufPlanes[i].rowStrideInBytes;
        }
        IIonImageBufferHeap::AllocImgParam_t const allocImgParam(
            format, imgSize,
            bufStridesInBytes, bufBoundaryInBytes,
            bufPlanes.size()
        );

        mpPool_HalImageRaw_main2 = new HalImageStreamBuffer::Allocator::StreamBufferPoolT(
            pStreamInfo->getStreamName(),
            HalImageStreamBuffer::Allocator(pStreamInfo.get(), allocImgParam)
        );
        MERROR err = mpPool_HalImageRaw_main2->initPool("Tester", pStreamInfo->getMaxBufNum(), pStreamInfo->getMinInitBufNum());
        if  ( err ) {
            MY_LOGE("mpPool_HalImageRaw_main2 init fail");
        }
    }
    //Hal:Image: dst0
    {
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_BMDENOISE_RESULT;
        MSize const imgSize(4208, 3120);
        MINT const format = eImgFmt_YV12;
        MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN |
                            eBUFFER_USAGE_HW_CAMERA_READWRITE
                            ;
        IImageStreamInfo::BufPlanes_t bufPlanes;

        if( format == eImgFmt_YV12 ) {
            addBufPlane(bufPlanes, (unsigned int)imgSize.h, (unsigned int)imgSize.w);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w/2);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w/2);
        }
        else if( format == eImgFmt_NV21 ) {
            addBufPlane(bufPlanes, (unsigned int)imgSize.h, (unsigned int)imgSize.w);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w);
        }
        //
        sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
            "Hal:Image:YUV0",
            streamId,
            eSTREAMTYPE_IMAGE_INOUT,
            5, 1,
            usage, format, imgSize, bufPlanes
        );
        pHalImageOutYuv0 = pStreamInfo;
        //
        //
        size_t bufStridesInBytes[3] = {0};
        size_t bufBoundaryInBytes[3]= {0};
        for (size_t i = 0; i < bufPlanes.size(); i++) {
            bufStridesInBytes[i] = bufPlanes[i].rowStrideInBytes;
        }
        IIonImageBufferHeap::AllocImgParam_t const allocImgParam(
            format, imgSize,
            bufStridesInBytes, bufBoundaryInBytes,
            bufPlanes.size()
        );

        mpPool_HalImageYuv0 = new HalImageStreamBuffer::Allocator::StreamBufferPoolT(
            pStreamInfo->getStreamName(),
            HalImageStreamBuffer::Allocator(pStreamInfo.get(), allocImgParam)
        );
        MERROR err = mpPool_HalImageYuv0->initPool("Tester", pStreamInfo->getMaxBufNum(), pStreamInfo->getMinInitBufNum());
        if  ( err ) {
            MY_LOGE("mpPool_HalImageYuv0 init fail");
        }
    }
    //Hal:Image: warpingMmatrix
    {
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_BMDENOISE_WAPING_MATRIX;
        MSize const imgSize(200, 200);
        MINT const format = eImgFmt_YV12;
        MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN |
                            eBUFFER_USAGE_HW_CAMERA_READWRITE
                            ;
        IImageStreamInfo::BufPlanes_t bufPlanes;

        if( format == eImgFmt_YV12 ) {
            addBufPlane(bufPlanes, (unsigned int)imgSize.h, (unsigned int)imgSize.w);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w/2);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w/2);
        }
        else if( format == eImgFmt_NV21 ) {
            addBufPlane(bufPlanes, (unsigned int)imgSize.h, (unsigned int)imgSize.w);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w);
        }
        //
        sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
            "Hal:Image:WarpingMatrix",
            streamId,
            eSTREAMTYPE_IMAGE_INOUT,
            5, 1,
            usage, format, imgSize, bufPlanes
        );
        pHalImageWarpingMatrix = pStreamInfo;
        //
        //
        size_t bufStridesInBytes[3] = {0};
        size_t bufBoundaryInBytes[3]= {0};
        for (size_t i = 0; i < bufPlanes.size(); i++) {
            bufStridesInBytes[i] = bufPlanes[i].rowStrideInBytes;
        }
        IIonImageBufferHeap::AllocImgParam_t const allocImgParam(
            format, imgSize,
            bufStridesInBytes, bufBoundaryInBytes,
            bufPlanes.size()
        );

        mpPool_HalImageWarpingMarix = new HalImageStreamBuffer::Allocator::StreamBufferPoolT(
            pStreamInfo->getStreamName(),
            HalImageStreamBuffer::Allocator(pStreamInfo.get(), allocImgParam)
        );
        MERROR err = mpPool_HalImageWarpingMarix->initPool("Tester", pStreamInfo->getMaxBufNum(), pStreamInfo->getMinInitBufNum());
        if  ( err ) {
            MY_LOGE("mpPool_HalImageWarpingMarix init fail");
        }
    }
    //Hal:Image: disparity map
    {
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_BMDEPTHMAPNODE_DISPARITY_L;
        MSize const imgSize(240, 136);
        MINT const format = eImgFmt_YV12;
        MUINT const usage = eBUFFER_USAGE_SW_READ_OFTEN |
                            eBUFFER_USAGE_HW_CAMERA_READWRITE
                            ;
        IImageStreamInfo::BufPlanes_t bufPlanes;

        if( format == eImgFmt_YV12 ) {
            addBufPlane(bufPlanes, (unsigned int)imgSize.h, (unsigned int)imgSize.w);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w/2);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w/2);
        }
        else if( format == eImgFmt_NV21 ) {
            addBufPlane(bufPlanes, (unsigned int)imgSize.h, (unsigned int)imgSize.w);
            addBufPlane(bufPlanes, (unsigned int)imgSize.h/2, (unsigned int)imgSize.w);
        }
        //
        sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
            "Hal:Image:DisparityMap",
            streamId,
            eSTREAMTYPE_IMAGE_INOUT,
            5, 1,
            usage, format, imgSize, bufPlanes
        );
        pHalImageDisparityMap = pStreamInfo;
        //
        //
        size_t bufStridesInBytes[3] = {0};
        size_t bufBoundaryInBytes[3]= {0};
        for (size_t i = 0; i < bufPlanes.size(); i++) {
            bufStridesInBytes[i] = bufPlanes[i].rowStrideInBytes;
        }
        IIonImageBufferHeap::AllocImgParam_t const allocImgParam(
            format, imgSize,
            bufStridesInBytes, bufBoundaryInBytes,
            bufPlanes.size()
        );

        mpPool_HalImageDisparityMap = new HalImageStreamBuffer::Allocator::StreamBufferPoolT(
            pStreamInfo->getStreamName(),
            HalImageStreamBuffer::Allocator(pStreamInfo.get(), allocImgParam)
        );
        MERROR err = mpPool_HalImageDisparityMap->initPool("Tester", pStreamInfo->getMaxBufNum(), pStreamInfo->getMinInitBufNum());
        if  ( err ) {
            MY_LOGE("mpPool_HalImageDisparityMap init fail");
        }
    }



    //App:Meta:DeNoise Out
    printf("create DeNoise out app metadata\n");
    {
        sp<IMetaStreamInfo>
        pStreamInfo = new MetaStreamInfo(
            "App DeNoise out Metadata",
            eSTREAMID_META_APP_DYNAMIC_BMDENOISE,
            eSTREAMTYPE_META_OUT,
            5, 1
        );
        pAppMetaResult = pStreamInfo;
    }


    //App:Meta:DeNoise In
    printf("create DeNoise in app metadata\n");
    {
        sp<MetaStreamInfo>
        pStreamInfo = new MetaStreamInfo(
            "App DeNoise in Request",
            eSTREAMID_META_APP_CONTROL,
            eSTREAMTYPE_META_IN,
            5, 5
        );
        pAppMetaRequest = pStreamInfo;
    }

    //Hal:Meta:DeNoise Out
    printf("create DeNoise out hal metadata\n");
    {
        sp<MetaStreamInfo>
        pStreamInfo = new MetaStreamInfo(
            "Hal DeNoise out Request",
            eSTREAMID_META_HAL_DYNAMIC_BMDENOISE,
            eSTREAMTYPE_META_IN,
            5, 1
        );
        pHalMetaResult = pStreamInfo;
    }

    //Hal:Meta:DeNoise In
    printf("create DeNoise in hal metadata\n");
    {
        sp<MetaStreamInfo>
        pStreamInfo = new MetaStreamInfo(
            "Hal DeNoise in Request",
            eSTREAMID_META_HAL_DYNAMIC_P1,
            eSTREAMTYPE_META_IN,
            5, 1
        );
        pHalMetaRequest = pStreamInfo;
    }

    params.pInAppMeta  = pAppMetaRequest;
    params.pInHalMeta  = pHalMetaRequest;
    params.pOutAppMeta = pAppMetaResult;
    params.pOutHalMeta = pHalMetaResult;
#warning "need fix"
    params.pInMFBO_final_1 = pHalImageInRaw;
    params.pInMFBO_final_2 = pHalImageInRaw_main2;
    params.pInDisparityMap = pHalImageDisparityMap;
    params.pInWarpingMatrix = pHalImageWarpingMatrix;
    params.vOutImage = pHalImageOutYuv0;

    ////////////////////////////////////////////////////////////////////////////
    //
    /////////////////////////////////////////////////////////////////////////////
    printf("add stream to streamInfoset\n");

    mpStreamInfoSet->editHalImage().addStream(pHalImageInRaw);
    mpStreamInfoSet->editHalImage().addStream(pHalImageInRaw_main2);
    mpStreamInfoSet->editHalImage().addStream(pHalImageOutYuv0);
    mpStreamInfoSet->editHalImage().addStream(pHalImageWarpingMatrix);
    mpStreamInfoSet->editHalImage().addStream(pHalImageDisparityMap);

    mpStreamInfoSet->editAppMeta().addStream(pAppMetaResult);
    mpStreamInfoSet->editAppMeta().addStream(pAppMetaRequest);
    mpStreamInfoSet->editHalMeta().addStream(pHalMetaResult);
    mpStreamInfoSet->editHalMeta().addStream(pHalMetaRequest);

    ////////////////////////////////////////////////////////////////////////////
    //
    /////////////////////////////////////////////////////////////////////////////
    //
    //
    printf("add stream to pipelineNodeMap\n");
    mpPipelineNodeMap->setCapacity(1);
    //
    {
        mpPipelineDAG->addNode(eNODEID_BMDeNoiseNode);
        ssize_t const tmpNodeIndex = mpPipelineNodeMap->add(eNODEID_BMDeNoiseNode, mpNode1);
        //
        sp<IStreamInfoSetControl> const&
        rpInpStreams = mpPipelineNodeMap->getNodeAt(tmpNodeIndex)->editInStreams();
        sp<IStreamInfoSetControl> const&
        rpOutStreams = mpPipelineNodeMap->getNodeAt(tmpNodeIndex)->editOutStreams();

        // IN streams
        rpInpStreams->editAppMeta().addStream(pAppMetaRequest);
        rpInpStreams->editHalMeta().addStream(pHalMetaRequest);
        rpInpStreams->editHalImage().addStream(pHalImageInRaw);
        rpInpStreams->editHalImage().addStream(pHalImageInRaw_main2);
        rpInpStreams->editHalImage().addStream(pHalImageWarpingMatrix);
        rpInpStreams->editHalImage().addStream(pHalImageDisparityMap);
        // OUT streams
        rpOutStreams->editAppMeta().addStream(pAppMetaResult);
        rpOutStreams->editAppMeta().addStream(pHalMetaResult);
        rpOutStreams->editHalImage().addStream(pHalImageOutYuv0);
    }
    //
    mpPipelineDAG->setRootNode(mpNode1->getNodeId());
    for (size_t i = 0; i < mpPipelineNodeMap->size(); i++)
    {
        mpPipelineDAG->setNodeValue(mpPipelineNodeMap->nodeAt(i)->getNodeId(), i);
    }

    printf("prepareConfig - \n");
}


/******************************************************************************
 *
 ******************************************************************************/
void
prepareRequest(android::sp<IPipelineFrameControl> &pFrame)
{
    printf("prepare request\n");

    pFrame = IPipelineFrameControl::create(0);
    pFrame->setPipelineNodeMap(getPipelineNodeMapControl());
    pFrame->setPipelineDAG(getPipelineDAG());
    pFrame->setStreamInfoSet(getStreamInfoSet());

    //to-be-tested Node
    {
        IPipelineNode::NodeId_T const nodeId = eNODEID_BMDeNoiseNode;
        //
        IPipelineFrame::InfoIOMapSet aInfoIOMapSet;
        IPipelineFrame::ImageInfoIOMapSet& rImageInfoIOMapSet = aInfoIOMapSet.mImageInfoIOMapSet;
        IPipelineFrame::MetaInfoIOMapSet&  rMetaInfoIOMapSet  = aInfoIOMapSet.mMetaInfoIOMapSet;
        //
        //
        sp<IPipelineNodeMapControl::INode> pNodeExt = getPipelineNodeMapControl()->getNodeFor(nodeId);
        sp<IStreamInfoSet const> pInStream = pNodeExt->getInStreams();
        sp<IStreamInfoSet const> pOutStream= pNodeExt->getOutStreams();
        //
        //  Image
        {
            IPipelineFrame::ImageInfoIOMap& rMap =
            rImageInfoIOMapSet.editItemAt(rImageInfoIOMapSet.add());
            //
            //Input
            for (size_t i = 0; i < pInStream->getImageInfoNum(); i++)
            {
                sp<IImageStreamInfo> p = pInStream->getImageInfoAt(i);
                rMap.vIn.add(p->getStreamId(), p);
            }
            //
            //Output
            for (size_t i = 0; i < pOutStream->getImageInfoNum(); i++)
            {
                sp<IImageStreamInfo> p = pOutStream->getImageInfoAt(i);
                rMap.vOut.add(p->getStreamId(), p);
            }
        }
        //
        //  Meta
        {
            IPipelineFrame::MetaInfoIOMap& rMap =
            rMetaInfoIOMapSet.editItemAt(rMetaInfoIOMapSet.add());
            //
            //Input
            for (size_t i = 0; i < pInStream->getMetaInfoNum(); i++)
            {
                sp<IMetaStreamInfo> p = pInStream->getMetaInfoAt(i);
                rMap.vIn.add(p->getStreamId(), p);
            }
            //
            //Output
            for (size_t i = 0; i < pOutStream->getMetaInfoNum(); i++)
            {
                sp<IMetaStreamInfo> p = pOutStream->getMetaInfoAt(i);
                rMap.vOut.add(p->getStreamId(), p);
            }
        }
        //
        //
        pFrame->addInfoIOMapSet(nodeId, aInfoIOMapSet);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  pFrame->setStreamBufferSet(...);
    //
    ////////////////////////////////////////////////////////////////////////////

    //IAppPipeline::AppCallbackParams aAppCallbackParams;
    //aAppCallbackParams.mpBuffersCallback = pAppSimulator;
    sp<IStreamBufferSetControl> pBufferSetControl = IStreamBufferSetControl::create(
        0, NULL
    );
    //
    //
    {
        //
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_RAW_OPAQUE;
        //
        sp<IImageStreamInfo> pStreamInfo = getStreamInfoSet()->getImageInfoFor(streamId);
        sp<HalImageStreamBuffer> pStreamBuffer;
        //
        //acquireFromPool
        MY_LOGD("[acquireFromPool] + %s ", pStreamInfo->getStreamName());
        MERROR err = mpPool_HalImageRaw->acquireFromPool(
            "Tester", pStreamBuffer, ::s2ns(30)
        );
        MY_LOGD("[acquireFromPool] - %s %p err:%d", pStreamInfo->getStreamName(), pStreamBuffer.get(), err);
        MY_LOGE_IF(OK!=err || pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::CONSUMER;
            user.mUsage = pStreamInfo->getUsageForAllocator();
            pUserGraph->addUser(user);
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalImage()->add(pStreamBuffer);
    }
    {
        //
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01;
        //
        sp<IImageStreamInfo> pStreamInfo = getStreamInfoSet()->getImageInfoFor(streamId);
        sp<HalImageStreamBuffer> pStreamBuffer;
        //
        //acquireFromPool
        MY_LOGD("[acquireFromPool] + %s ", pStreamInfo->getStreamName());
        MERROR err = mpPool_HalImageRaw_main2->acquireFromPool(
            "Tester_raw_main2", pStreamBuffer, ::s2ns(30)
        );
        MY_LOGD("[acquireFromPool] - %s %p err:%d", pStreamInfo->getStreamName(), pStreamBuffer.get(), err);
        MY_LOGE_IF(OK!=err || pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::CONSUMER;
            user.mUsage = pStreamInfo->getUsageForAllocator();
            pUserGraph->addUser(user);
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalImage()->add(pStreamBuffer);
    }
    //
    {
        //
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_BMDENOISE_RESULT;
        //
        sp<IImageStreamInfo> pStreamInfo = getStreamInfoSet()->getImageInfoFor(streamId);
        sp<HalImageStreamBuffer> pStreamBuffer;
        //
        //acquireFromPool
        MY_LOGD("[acquireFromPool] + %s ", pStreamInfo->getStreamName());
        MERROR err = mpPool_HalImageYuv0->acquireFromPool(
            "Tester", pStreamBuffer, ::s2ns(30)
        );
        MY_LOGD("[acquireFromPool] - %s %p err:%d", pStreamInfo->getStreamName(), pStreamBuffer.get(), err);
        MY_LOGE_IF(OK!=err || pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::PRODUCER;
            user.mUsage = pStreamInfo->getUsageForAllocator();
            pUserGraph->addUser(user);
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalImage()->add(pStreamBuffer);
    }
    {
        //
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_BMDENOISE_WAPING_MATRIX;
        //
        sp<IImageStreamInfo> pStreamInfo = getStreamInfoSet()->getImageInfoFor(streamId);
        sp<HalImageStreamBuffer> pStreamBuffer;
        //
        //acquireFromPool
        MY_LOGD("[acquireFromPool] + %s ", pStreamInfo->getStreamName());
        MERROR err = mpPool_HalImageWarpingMarix->acquireFromPool(
            "Tester warpingMatrix", pStreamBuffer, ::s2ns(30)
        );
        MY_LOGD("[acquireFromPool] - %s %p err:%d", pStreamInfo->getStreamName(), pStreamBuffer.get(), err);
        MY_LOGE_IF(OK!=err || pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::CONSUMER;
            user.mUsage = pStreamInfo->getUsageForAllocator();
            pUserGraph->addUser(user);
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalImage()->add(pStreamBuffer);
    }
    {
        //
        StreamId_T const streamId = eSTREAMID_IMAGE_PIPE_BMDEPTHMAPNODE_DISPARITY_L;
        //
        sp<IImageStreamInfo> pStreamInfo = getStreamInfoSet()->getImageInfoFor(streamId);
        sp<HalImageStreamBuffer> pStreamBuffer;
        //
        //acquireFromPool
        MY_LOGD("[acquireFromPool] + %s ", pStreamInfo->getStreamName());
        MERROR err = mpPool_HalImageDisparityMap->acquireFromPool(
            "Tester disparity", pStreamBuffer, ::s2ns(30)
        );
        MY_LOGD("[acquireFromPool] - %s %p err:%d", pStreamInfo->getStreamName(), pStreamBuffer.get(), err);
        MY_LOGE_IF(OK!=err || pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::CONSUMER;
            user.mUsage = pStreamInfo->getUsageForAllocator();
            pUserGraph->addUser(user);
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalImage()->add(pStreamBuffer);
    }
    //
    {
        //App:Meta:Result
        StreamId_T const streamId = eSTREAMID_META_APP_DYNAMIC_BMDENOISE;
        //
        sp<IMetaStreamInfo> pStreamInfo = getStreamInfoSet()->getMetaInfoFor(streamId);
        sp<IMetaStreamBuffer> pStreamBuffer;
        //
        //alloc without default value
        typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator StreamBufferAllocatorT;
        pStreamBuffer = StreamBufferAllocatorT(pStreamInfo.get())();
        MY_LOGE_IF(pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::PRODUCER;
            pUserGraph->addUser(user);
            //
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_AppMeta()->add(pStreamBuffer);
    }
    //
    {
        //APP/Meta/Request
        StreamId_T const streamId = eSTREAMID_META_APP_CONTROL;
        //
        sp<IMetaStreamInfo> pStreamInfo = getStreamInfoSet()->getMetaInfoFor(streamId);
        sp<IMetaStreamBuffer> pStreamBuffer;
        //
        IMetadata appRequest;
        {
            IMetadata::IEntry entry1(MTK_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
            entry1.push_back(16, Type2Type< MFLOAT >());
            appRequest.update(MTK_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, entry1);
        }

        typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator StreamBufferAllocatorT;
        pStreamBuffer = StreamBufferAllocatorT(pStreamInfo.get())(appRequest);
        MY_LOGE_IF(pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::CONSUMER;
            pUserGraph->addUser(user);
            //
            //
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_AppMeta()->add(pStreamBuffer);
    }
    //
    {
        //HAL/Meta/Request
        StreamId_T const streamId = eSTREAMID_META_HAL_DYNAMIC_P1;
        //
        sp<IMetaStreamInfo> pStreamInfo = getStreamInfoSet()->getMetaInfoFor(streamId);
        sp<HalMetaStreamBuffer> pStreamBuffer;
        //
        {
            MY_LOGD("push main2 hal dynamic into hal_meta");
            IMetadata::IEntry entry(MTK_P1NODE_MAIN2_HAL_META);
            entry.push_back(halRequest_main2, Type2Type< IMetadata >());
            halRequest.update(entry.tag(), entry);
        }
        typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator StreamBufferAllocatorT;
        pStreamBuffer = StreamBufferAllocatorT(pStreamInfo.get())(halRequest);
        MY_LOGE_IF(pStreamBuffer==0, "pStreamBuffer==0");
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::PRODUCER;
            pUserGraph->addUser(user);
            //
            //
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalMeta()->add(pStreamBuffer);
    }
    //
    {
        //HAL/Meta/Result
        StreamId_T const streamId = eSTREAMID_META_HAL_DYNAMIC_BMDENOISE;
        //
        sp<IMetaStreamInfo> pStreamInfo = getStreamInfoSet()->getMetaInfoFor(streamId);
        sp<HalMetaStreamBuffer> pStreamBuffer;
        //
        typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator StreamBufferAllocatorT;
        pStreamBuffer = StreamBufferAllocatorT(pStreamInfo.get())();
        MY_LOGE_IF(pStreamBuffer==0, "pStreamBuffer==0");
        //
        ssize_t userGroupIndex = 0;
        //User Group1
        {
            sp<IUsersManager::IUserGraph> pUserGraph = pStreamBuffer->createGraph();
            IUsersManager::User user;
            //
            user.mUserId = eNODEID_BMDeNoiseNode;
            user.mCategory = IUsersManager::Category::PRODUCER;
            pUserGraph->addUser(user);
            //
            //
            userGroupIndex = pStreamBuffer->enqueUserGraph(pUserGraph.get());
            pStreamBuffer->finishUserSetup();
        }
        //
        pBufferSetControl->editMap_HalMeta()->add(pStreamBuffer);
    }
    //
    pFrame->setStreamBufferSet(pBufferSetControl);
}

/******************************************************************************
 *
 ******************************************************************************/
void prepareSensor()
{
    IHalSensorList* const pHalSensorList = MAKE_HalSensorList();
    pHalSensorList->searchSensors();
    mpSensorHalObj = pHalSensorList->createSensor("tester", gSensorId);
    MUINT32    sensorArray[1] = {(MUINT32)gSensorId};
    mpSensorHalObj->powerOn("tester", 1, &sensorArray[0]);
}

/******************************************************************************
 *
 ******************************************************************************/
void closeSensor()
{
    MUINT32    sensorArray[1] = {(MUINT32)gSensorId};
    mpSensorHalObj->powerOff("tester", 1, &sensorArray[0]);
    mpSensorHalObj->destroyInstance("tester");
    mpSensorHalObj = NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
TEST(MNDeNoiseNodeTest, BasicTest)
{
    MY_LOGD("start test");

    prepareSensor();

    MY_LOGD("StereoSettingProvider::setStereoProfile(STEREO_SENSOR_PROFILE_REAR_FRONT)...");
    StereoSettingProvider::setStereoProfile(STEREO_SENSOR_PROFILE_REAR_FRONT);

    mpNode1 = BMDeNoiseNode::createInstance();
    //
    MUINT32 frameNo = 0;
    //
    // static metadata
    sp<IMetadataProvider> pMetadataProvider = IMetadataProvider::create(gSensorId);
    NSMetadataProviderManager::add(gSensorId, pMetadataProvider.get());

    //init
    {
        BMDeNoiseNode::InitParams params;
        params.openId = gSensorId;
        params.nodeName = "BMDeNoiseNodeTester";
        params.nodeId = eNODEID_BMDeNoiseNode;
        MERROR ret = mpNode1->init(params);

        EXPECT_EQ(OK, ret) << "mpNode1->init(params);";
    };

    //config
    {
       BMDeNoiseNode::ConfigParams params;
       prepareConfig(params);
       MERROR ret = mpNode1->config(params);

       EXPECT_EQ(OK, ret) << "mpNode1->config(params);";
    }

    //request
    {
        android::sp<IPipelineFrameControl> pFrame;
        prepareRequest(pFrame);

        printf("queue request\n");
        MERROR ret = mpNode1->queue(pFrame);

        EXPECT_EQ(OK, ret) << "mpNode1->queue(pFrame);";
    }

    int secs = 6;

    MY_LOGD("wait %d secs + ", secs);
    printf("wait %d secs +\n", secs);

    usleep(secs*1000*1000);

    MY_LOGD("wait %d secs - ", secs);
    printf("wait %d secs -\n", secs);

    //flush
    printf("flush\n");
    mpNode1->flush();

    //uninit
    printf("uninit\n");
    mpNode1->uninit();

    closeSensor();

    clear_global_var();

    MY_LOGD("end of test");
}

/******************************************************************************
 *
 ******************************************************************************/
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}