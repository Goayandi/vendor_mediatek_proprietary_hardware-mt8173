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

#define LOG_TAG "MtkCam/DepthMapNode"

#include "hwnode_utilities.h"

#include "BaseNode.h"

#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/hwnode/DepthMapNode.h>
#include <mtkcam/feature/stereo/pipe/IDepthMapPipe.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/feature/stereo/effecthal/DepthMapEffectHal.h>
//
#include <utils/RWLock.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>

//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <chrono>

using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils::Sync;
using namespace NSCam::NSCamFeature::NSFeaturePipe;

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

//
#if 1
#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")
#else
#define FUNC_START
#define FUNC_END
#endif

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


class DepthMapNodeImp
    : public BaseNode
    , public DepthMapNode

{
public:

    DepthMapNodeImp();
    virtual ~DepthMapNodeImp();
    virtual MERROR config(ConfigParams const& rParams);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.

    virtual MERROR init(InitParams_Base const& rParams);
    virtual MERROR init(InitParams const& rParams);

    virtual MERROR uninit();

    virtual MERROR flush();

    virtual MERROR queue(sp<IPipelineFrame> pFrame);

protected:  ////                    LOGE & LOGI on/off
    MINT32 mLogLevel;
    MINT32 mOpenId_Main2;

protected:
    // meta
    sp<IMetaStreamInfo> mpInAppMeta = NULL;
    sp<IMetaStreamInfo> mpInHalMeta = NULL;
    sp<IMetaStreamInfo> mpInHalMeta_Main2 = NULL;
    sp<IMetaStreamInfo> mpOutAppMeta = NULL;
    sp<IMetaStreamInfo> mpOutHalMeta = NULL;
    // image
    sp<IImageStreamInfo> mpInHalResizeRaw = NULL;
    sp<IImageStreamInfo> mpInHalResizeRaw_Main2 = NULL;
    sp<IImageStreamInfo> mpInHalFullRaw = NULL;
    sp<IImageStreamInfo> mpInHalFullRaw_Main2 = NULL;
    sp<IImageStreamInfo> mpOutAppImageExtraData = NULL;
    sp<IImageStreamInfo> mpOutAppImageLDC = NULL;
    sp<IImageStreamInfo> mpOutAppImageDepthMap = NULL;
    sp<IImageStreamInfo> mpOutHalImageMainImage = NULL;
    sp<IImageStreamInfo> mpOutHalImageMainImage_Cap = NULL;
    sp<IImageStreamInfo> mpOutHalImageDMBG = NULL;
    sp<IImageStreamInfo> mpOutHalImageFD = NULL;

    sp<IImageStreamInfo> mpOutHalImageJPSMain1 = NULL;
    sp<IImageStreamInfo> mpOutHalImageJPSMain2 = NULL;

    DepthMapEffectHal* mpDepthEffectHal = NULL;

    KeyedVector< MUINT32, sp<IPipelineFrame> > mvPipelineFrameMap;

    mutable RWLock                  mConfigRWLock;
    mutable RWLock                  mQueueRWLock;
    bool                            mbInit;

private:
    typedef enum IOType
    {
        IOTYPE_INPUT,
        IOTYPE_OUTPUT,
    } DataIOType;

    static MVOID onEffectRequestFinished(MVOID* tag, String8 status, sp<EffectRequest>& request);

    MERROR onEffectReqDone(String8 status, sp<EffectRequest>& request);
    MERROR onEffectReqSucess(sp<IPipelineFrame> const& pFrame, sp<EffectRequest>& request);
    MERROR onEffectReqFailed(String8 status, sp<IPipelineFrame> const& pFrame, sp<EffectRequest>& request);

    MERROR releaseAllInputStreams(sp<EffectRequest>& request, sp<IPipelineFrame> const& pFrame);
    MERROR releaseAllOutputStreams(sp<EffectRequest>& request, sp<IPipelineFrame> const& pFrame,
                                MBOOL bIsSuccess, SortedVector<StreamId_T>* pExceptStreamId=NULL);

    MERROR retrieveImageBuffer(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId,
                                DataIOType eType, sp<IImageBuffer>& rpImageBuffer);

    MERROR retrieveMetadata(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId,
                                DataIOType eType, IMetadata*& rpMetadata);

    MERROR releaseImageBuffer(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId,
                                DataIOType eType, sp<IImageBuffer> const& pImageBuffer, MBOOL bIsSuccess);

    MERROR releaseMetadata(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId,
                                DataIOType eType, IMetadata* pMetadata, MBOOL bIsSuccess);

    MBOOL validateTolerantStreamID(StreamId_T const streamId);

    MERROR addImageBufferFrameInfo(sp<EffectRequest>& pEffectReq, DataIOType eIOType,
                                sp<IPipelineFrame>& pFrame, sp<IImageStreamInfo>& pImgStreamInfo, DepthMapBufferID bid);
    MERROR addMetaFrameInfo(sp<EffectRequest>& pEffectReq, DataIOType eIOType,
                                sp<IPipelineFrame>& pFrame, sp<IMetaStreamInfo>& pMetaStreamInfo, DepthMapBufferID bid);

    MERROR addEnqueImageBufferToRequest(sp<IPipelineFrame>& pFrame, IPipelineFrame::ImageInfoIOMapSet const& rIOMapSet, sp<EffectRequest>& pEffectReq);
    MERROR addEnqueMetaBufferToRequest(sp<IPipelineFrame>& pFrame, IPipelineFrame::MetaInfoIOMapSet const& rIOMapSet, sp<EffectRequest>& pEffectReq);

    MERROR splitHalMetaFrameInfo(sp<EffectRequest>& pEffectReq, sp<IPipelineFrame> pFrame, IMetadata*& rMeta);

    MERROR mapToBufferID(StreamId_T streamId, DepthMapBufferID &rBufID);
    MERROR mapToStreamID(DepthMapBufferID bufID, StreamId_T& rStreamId);

    MERROR unlockAndMarkOutStreamError(sp<IPipelineFrame> const& pFrame, sp<EffectRequest> request);
    MERROR suspendThisFrame(sp<IPipelineFrame> const& pFrame, sp<EffectRequest> const& request);

    MERROR generateBIDMaps();

    //BID To StreamID Map
    KeyedVector<DepthMapBufferID, StreamId_T> mDepthBIDToStreamIDMap;
    //StreamID to BID Map
    KeyedVector<StreamId_T, DepthMapBufferID> mStreamIDMapToDepthBID;
    // error-tolerant stream id lsit
    KeyedVector<StreamId_T, MBOOL> mvTolerantStreamId;

};

/******************************************************************************
 *
 ******************************************************************************/
sp<DepthMapNode>
DepthMapNode::
createInstance()
{
    return new DepthMapNodeImp();
}


/******************************************************************************
 *
 ******************************************************************************/
DepthMapNodeImp::
DepthMapNodeImp()
    : BaseNode()
    , DepthMapNode()
    , mbInit(MFALSE)
    , mvPipelineFrameMap()

{
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.camera.log", cLogLevel, "0");
    mLogLevel = atoi(cLogLevel);
    if ( mLogLevel == 0 ) {
        ::property_get("debug.camera.log.depthMap", cLogLevel, "0");
        mLogLevel = atoi(cLogLevel);
    }
}


/******************************************************************************
 *
 ******************************************************************************/
DepthMapNodeImp::
~DepthMapNodeImp()
{
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
init(InitParams_Base const& rParams)
{
    MY_LOGE("Error: need to use the DepthMapNode::InitParams!");
    return INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
init(InitParams const& rParams)
{
    FUNC_START;

    if(mbInit)
    {
        MY_LOGW("Already perform init operations!");
        return INVALID_OPERATION;
    }

    RWLock::AutoWLock _l(mConfigRWLock);

    mbInit = MTRUE;
    mOpenId = rParams.openId;
    mOpenId_Main2 = rParams.openId_main2;
    mNodeId = rParams.nodeId;
    mNodeName = rParams.nodeName;

    //
    MY_LOGD("OpenId %d, nodeId %d, name %s, openID=%d openID_main2=%d",
            getOpenId(), getNodeId(), getNodeName(), mOpenId, mOpenId_Main2);

    mpDepthEffectHal = new DepthMapEffectHal();
    mpDepthEffectHal->init();

    // prepare sensor idx parameters
    sp<EffectParameter> effParam = new EffectParameter();
    effParam->set(SENSOR_IDX_MAIN1, mOpenId);
    effParam->set(SENSOR_IDX_MAIN2, mOpenId_Main2);
    mpDepthEffectHal->setParameters(effParam);

    FUNC_END;
    return OK;
}



/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
config(ConfigParams const& rParams)
{
    FUNC_START;

    if(!mbInit)
    {
        MY_LOGW("Not perform init operations yet!");
        return INVALID_OPERATION;
    }


    {
        RWLock::AutoWLock _l(mConfigRWLock);
        mpInAppMeta = rParams.pInAppMeta;
        mpInHalMeta = rParams.pInHalMeta;
        mpInHalMeta_Main2 = rParams.pInHalMeta_Main2;
        mpOutAppMeta = rParams.pOutAppMeta;
        mpOutHalMeta = rParams.pOutHalMeta;

        mpInHalResizeRaw = rParams.pInResizedRaw;
        mpInHalResizeRaw_Main2 = rParams.pInResizedRaw_Main2;
        mpInHalFullRaw = rParams.pInFullRaw;
        mpInHalFullRaw_Main2 = rParams.pInFullRaw_Main2;

        mpOutAppImageDepthMap = rParams.pAppImageDepthMap;
        mpOutAppImageLDC = rParams.pAppImageLDC;
        mpOutHalImageMainImage = rParams.pHalImageMainImg;
        mpOutHalImageDMBG = rParams.pHalImageDMBG;

        if(rParams.pHalImageFD != NULL)
        {
            mpOutHalImageFD = rParams.pHalImageFD;
            // FD is tolerant streamID
            mvTolerantStreamId.add(mpOutHalImageFD->getStreamId(), MTRUE);
        }

        if(rParams.pHalImageMainImg_Cap != NULL) {
            mpOutHalImageMainImage_Cap = rParams.pHalImageMainImg_Cap;}

        if(rParams.pHalImageJPSMain1 != NULL) {
            mpOutHalImageJPSMain1 = rParams.pHalImageJPSMain1;}

        if(rParams.pHalImageJPSMain2 != NULL) {
            mpOutHalImageJPSMain2 = rParams.pHalImageJPSMain2;}

        if(rParams.pAppImageExtraData != NULL) {
            mpOutAppImageExtraData = rParams.pAppImageExtraData;
        }

        mpDepthEffectHal->configure();
        mpDepthEffectHal->start();
    }

    // Generate the BID to StreamID map
    generateBIDMaps();

    FUNC_END;

    return OK;


}

#define ADD_BID_STREAMID_MAP(streamInfo, depthBID)\
    if(streamInfo.get()) \
    { \
        mDepthBIDToStreamIDMap.add(depthBID, streamInfo->getStreamId()); \
        mStreamIDMapToDepthBID.add(streamInfo->getStreamId(), depthBID); \
        MY_LOGD_IF(mLogLevel >= 1, "===========> BID=%d streamID=%#" PRIxPTR "", depthBID, streamInfo->getStreamId());\
    }


/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
generateBIDMaps()
{
    FUNC_START;
    // image stream
    ADD_BID_STREAMID_MAP(mpInHalResizeRaw, BID_P2AFM_IN_RSRAW1);
    ADD_BID_STREAMID_MAP(mpInHalResizeRaw_Main2, BID_P2AFM_IN_RSRAW2);
    ADD_BID_STREAMID_MAP(mpInHalFullRaw, BID_P2AFM_IN_FSRAW1);
    ADD_BID_STREAMID_MAP(mpInHalFullRaw_Main2, BID_P2AFM_IN_FSRAW2);
    ADD_BID_STREAMID_MAP(mpOutAppImageDepthMap, BID_GF_OUT_DEPTHMAP);
    ADD_BID_STREAMID_MAP(mpOutHalImageDMBG, BID_GF_OUT_DMBG);
    ADD_BID_STREAMID_MAP(mpOutHalImageMainImage, BID_P2AFM_OUT_MV_F);
    ADD_BID_STREAMID_MAP(mpOutHalImageMainImage_Cap, BID_P2AFM_OUT_MV_F_CAP);
    ADD_BID_STREAMID_MAP(mpOutHalImageJPSMain1, BID_N3D_OUT_JPS_MAIN1);
    ADD_BID_STREAMID_MAP(mpOutHalImageJPSMain2, BID_N3D_OUT_JPS_MAIN2);
    ADD_BID_STREAMID_MAP(mpOutHalImageFD, BID_P2AFM_OUT_FDIMG);
    ADD_BID_STREAMID_MAP(mpOutAppImageLDC, BID_N3D_OUT_LDC);
    // meta stream
    ADD_BID_STREAMID_MAP(mpInAppMeta, BID_META_IN_APP);
    ADD_BID_STREAMID_MAP(mpInHalMeta, BID_META_IN_HAL);
    ADD_BID_STREAMID_MAP(mpOutAppMeta, BID_META_OUT_APP);
    ADD_BID_STREAMID_MAP(mpOutHalMeta, BID_META_OUT_HAL);
    ADD_BID_STREAMID_MAP(mpOutAppImageExtraData, BID_FD_OUT_EXTRADATA);

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
uninit()
{
    FUNC_START;

    if(!mbInit)
    {
        MY_LOGW("Not perform init operations yet!");
        return INVALID_OPERATION;
    }

    flush();


    {
        RWLock::AutoWLock _l(mConfigRWLock);
        mpDepthEffectHal->abort();
        mpDepthEffectHal->unconfigure();
        mpDepthEffectHal->uninit();
        delete mpDepthEffectHal;
        mbInit = MFALSE;
    }
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
flush()
{
    FUNC_START;
    MBOOL bRet = mpDepthEffectHal->flush();
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DepthMapNodeImp::
onEffectRequestFinished(MVOID* tag, String8 status, sp<EffectRequest>& request)
{
    DepthMapNodeImp *depthMapNode = (DepthMapNodeImp *) tag;
    depthMapNode->onEffectReqDone(status, request);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
onEffectReqDone(String8 status, sp<EffectRequest>& request)
{
    MUINT32 reqID = request->getRequestNo();
    sp<IPipelineFrame> pFrame;
    {
        MY_LOGD(" onEffectReqDone: reqID=%d", reqID);
        RWLock::AutoWLock _l(mQueueRWLock);
        pFrame = mvPipelineFrameMap.valueFor(reqID);
        mvPipelineFrameMap.removeItem(reqID);
    }

    if(status == DEPTHMAP_COMPLETE_KEY)
    {
        onEffectReqSucess(pFrame, request);
    }
    else if(status == DEPTHMAP_FLUSH_KEY)
    {
        suspendThisFrame(pFrame, request);
    }
    else
    {
        onEffectReqFailed(status, pFrame, request);
    }
    // release the split MAIN2 metadata
    sp<EffectFrameInfo> pFrame_HalMetaMain2 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
    sp<EffectParameter> pEffParam = pFrame_HalMetaMain2->getFrameParameter();
    IMetadata* pMeta = reinterpret_cast<IMetadata*>(pEffParam->getPtr(VSDOF_PARAMS_METADATA_KEY));
    delete pMeta;

    // retrieve and delete time tag
    if(mLogLevel >= 1)
    {
        using std::chrono::system_clock;
        const sp<EffectParameter> pEffectParam = request->getRequestParameter();
        system_clock::time_point* tp = (system_clock::time_point*)pEffectParam->getPtr(DEPTHMAP_REQUEST_TIMEKEY);
        std::chrono::duration<double> elap = system_clock::now() - *tp;
        delete tp;
        MY_LOGD("DepthMapNode: reqID=%d, status=%s, cost time=%f msec", request->getRequestNo(), status.string(), elap.count()*1000);
    }

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
onEffectReqFailed(String8 status, sp<IPipelineFrame> const& pFrame, sp<EffectRequest>& request)
{
    MY_LOGD_IF(mLogLevel >= 1, "+");
    MUINT32 reqID = request->getRequestNo();

    if(status == DEPTHMAP_ERROR_KEY)
    {
        MY_LOGD_IF(mLogLevel >= 1, " status: %s", DEPTHMAP_ERROR_KEY);
        // release all input stream
        releaseAllInputStreams(request, pFrame);
        // mark all output stream failed
        releaseAllOutputStreams(request, pFrame, MFALSE);
        // dispatch
        MY_LOGD_IF(mLogLevel >= 1, "DepthMapNode: applyRelease reqID=%d", reqID);
        pFrame->getStreamBufferSet().applyRelease(getNodeId());
        MY_LOGD_IF(mLogLevel >= 1, "DepthMapNode: onDispatchFrame reqID=%d", reqID);
        onDispatchFrame(pFrame);
        MY_LOGD_IF(mLogLevel >= 1, "-");
    }
    else
    {
        MY_LOGE("Un-recognized status=%s", status.string());
        suspendThisFrame(pFrame, request);
    }
    return OK;
}

MERROR
DepthMapNodeImp::
onEffectReqSucess(sp<IPipelineFrame> const& pFrame, sp<EffectRequest>& request)
{
    MUINT32 reqID = request->getRequestNo();
    MY_LOGD_IF(mLogLevel >= 1, "+ , reqId=%d", reqID);

    // write MAIN2 HALMETA back to the MAIN2 field inside the MAIN1 HalMeta
    sp<EffectFrameInfo> pFrame_HalMetaMain1 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL);
    sp<EffectFrameInfo> pFrame_HalMetaMain2 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
    sp<EffectParameter> pEffParam_Main1 = pFrame_HalMetaMain1->getFrameParameter();
    sp<EffectParameter> pEffParam_Main2 = pFrame_HalMetaMain2->getFrameParameter();
    IMetadata* pMetaMain1 = reinterpret_cast<IMetadata*>(pEffParam_Main1->getPtr(VSDOF_PARAMS_METADATA_KEY));
    IMetadata* pMetaMain2 = reinterpret_cast<IMetadata*>(pEffParam_Main2->getPtr(VSDOF_PARAMS_METADATA_KEY));
    trySetMetadata<IMetadata>(pMetaMain1, MTK_P1NODE_MAIN2_HAL_META, *pMetaMain2);

    //release all input stream
    releaseAllInputStreams(request, pFrame);
    // release all output stream
    releaseAllOutputStreams(request, pFrame, MTRUE);

    // dispatch
    MY_LOGD_IF(mLogLevel >= 1, "DepthMapNode: applyRelease reqID=%d", reqID);
    pFrame->getStreamBufferSet().applyRelease(getNodeId());
    MY_LOGD_IF(mLogLevel >= 1, "DepthMapNode: onDispatchFrame reqID=%d", reqID);
    onDispatchFrame(pFrame);
    MY_LOGD_IF(mLogLevel >= 1, "-");

    return OK;
}

MERROR
DepthMapNodeImp::
releaseAllOutputStreams(sp<EffectRequest>& request, sp<IPipelineFrame> const& pFrame, MBOOL bIsSuccess, SortedVector<StreamId_T>* pExceptStreamId)
{
    // get IOMapSet of this frame, which define all the in/out stream inside the frame
    IPipelineFrame::InfoIOMapSet IOMapSet;
    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ) ) {
        MY_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }

    // get the image stream IOMapSet
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = IOMapSet.mImageInfoIOMapSet[0];

    sp<IImageBuffer> pImgBuf;

    // iterate all the output image stream and mark release status
    for(size_t index=0; index<imageIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vOut.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vOut.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        if(request->vOutputFrameInfo.indexOfKey(bufId) < 0)
        {
            // check the missing stream is tolerant
            if(!validateTolerantStreamID(streamId))
                MY_LOGE("Cannot find the output frameInfo, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufId);
            continue;
        }
        else
            request->vOutputFrameInfo.valueFor(bufId)->getFrameBuffer(pImgBuf);
        // check except stream id
        if(pExceptStreamId == NULL || pExceptStreamId->indexOf(streamId) < 0)
            releaseImageBuffer(pFrame, streamInfo->getStreamId(), IOTYPE_OUTPUT, pImgBuf, bIsSuccess);
        else
            releaseImageBuffer(pFrame, streamInfo->getStreamId(), IOTYPE_OUTPUT, pImgBuf, !bIsSuccess);
        MY_LOGD_IF(mLogLevel >= 1, "release imagebuffer, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufId);
    }

    // get the meta stream IOMapSet
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = IOMapSet.mMetaInfoIOMapSet[0];

    // iterate all the output meta stream and mark release status
    for(size_t index=0; index<metaIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vOut.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vOut.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        sp<EffectParameter> effParam = request->vOutputFrameInfo.valueFor(bufId)->getFrameParameter();
        IMetadata* pMetaBuf = (IMetadata*) effParam->getPtr(VSDOF_PARAMS_METADATA_KEY);
        // check except stream id
        if(pExceptStreamId == NULL || pExceptStreamId->indexOf(streamId) < 0)
            releaseMetadata(pFrame, streamInfo->getStreamId(), IOTYPE_OUTPUT, pMetaBuf, bIsSuccess);
        else
            releaseMetadata(pFrame, streamInfo->getStreamId(), IOTYPE_OUTPUT, pMetaBuf, !bIsSuccess);
        MY_LOGD_IF(mLogLevel >= 1, "release metadata, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufId);
    }
    return OK;
}

MERROR
DepthMapNodeImp::
releaseAllInputStreams(sp<EffectRequest>& request, sp<IPipelineFrame> const& pFrame)
{
    // get IOMapSet of this frame, which define all the in/out stream inside the frame
    IPipelineFrame::InfoIOMapSet IOMapSet;
    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ) ) {
        MY_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }

    // get the image stream IOMapSet
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = IOMapSet.mImageInfoIOMapSet[0];
    MY_LOGD_IF(mLogLevel >= 1, "ImageBuffer: vIn size=%d, vOut size=%d", imageIOMap.vIn.size(), imageIOMap.vOut.size());

    sp<IImageBuffer> pImgBuf;

     // iterate all the input image stream and release
    for(size_t index=0; index<imageIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vIn.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vIn.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        request->vInputFrameInfo.valueFor(bufId)->getFrameBuffer(pImgBuf);
        releaseImageBuffer(pFrame, streamInfo->getStreamId(), IOTYPE_INPUT, pImgBuf, MTRUE);
        MY_LOGD_IF(mLogLevel >= 1, "release imagebuffer, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufId);
    }

    // get the meta stream IOMapSet
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = IOMapSet.mMetaInfoIOMapSet[0];
    MY_LOGD_IF(mLogLevel >= 1, "Metadata: vIn size=%d, vOut size=%d", metaIOMap.vIn.size(), metaIOMap.vOut.size());

    // input meta stream
    for(size_t index=0; index<metaIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vIn.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vIn.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        sp<EffectParameter> effParam = request->vInputFrameInfo.valueFor(bufId)->getFrameParameter();
        IMetadata* pMetaBuf = (IMetadata*) effParam->getPtr(VSDOF_PARAMS_METADATA_KEY);
        releaseMetadata(pFrame, streamInfo->getStreamId(), IOTYPE_INPUT, pMetaBuf, MTRUE);
        MY_LOGD_IF(mLogLevel >= 1, "release metadata, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufId);
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
unlockAndMarkOutStreamError(sp<IPipelineFrame> const& pFrame, sp<EffectRequest> request)
{
    FUNC_START;
    // get IOMapSet of this frame, which define all the in/out stream inside the frame
    IPipelineFrame::InfoIOMapSet IOMapSet;
    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ) ) {
        MY_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }

    sp<IImageBuffer> pImageBuffer;
    IMetadata* pMetaData;

    #define GET_BUFFER_AND_UNLOCK(frameInfo) \
        frameInfo->getFrameBuffer(pImageBuffer);\
        pImageBuffer->unlockBuf(getNodeName());\
        pStreamBuffer->unlock(getNodeName(), pImageBuffer->getImageBufferHeap());

    #define GET_META_AND_UNLOCK(frameInfo) \
        pMetaData = reinterpret_cast<IMetadata*>(frameInfo->getFrameParameter()\
                                            ->getPtr(VSDOF_PARAMS_METADATA_KEY));\
        pMetaStreamBuffer->unlock(getNodeName(), pMetaData);


    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();
    // get the image stream IOMapSet
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = IOMapSet.mImageInfoIOMapSet[0];
    // get the meta stream IOMapSet
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = IOMapSet.mMetaInfoIOMapSet[0];

     // unlokc input buffer
    for(size_t index=0; index<imageIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vIn.keyAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);
        if (ret != OK)
            return BAD_VALUE;
        // get its frame info
        sp<EffectFrameInfo> frameInfo = request->vInputFrameInfo.valueFor(bufId);
        sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, getNodeId());
        if (pStreamBuffer != 0)
        {   //unlock buffer
            GET_BUFFER_AND_UNLOCK(frameInfo);
        }
        else
            MY_LOGW("Stream ID=%#" PRIxPTR " not found, cannot unlock!", streamId);
    }

     // unlock output buffer and mark error
    for(size_t index=0; index<imageIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vOut.keyAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);
        if (ret != OK)
            return BAD_VALUE;
        // get its frame info
        sp<EffectFrameInfo> frameInfo = request->vOutputFrameInfo.valueFor(bufId);
        sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, getNodeId());
        if (pStreamBuffer != 0)
        {
            //unlock buffer
            GET_BUFFER_AND_UNLOCK(frameInfo);
            //mark error
            pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
        }
        else
            MY_LOGW("Stream ID=%#" PRIxPTR " not found, cannot unlock!", streamId);
    }

    // unlokc input meta stream
    for(size_t index=0; index<metaIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vIn.keyAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);
        if (ret != OK)
            return BAD_VALUE;

        // get its frame info
        sp<EffectFrameInfo> frameInfo = request->vInputFrameInfo.valueFor(bufId);
        //if metadata
        sp<IMetaStreamBuffer> pMetaStreamBuffer = rStreamBufferSet.getMetaBuffer(streamId, getNodeId());
        if (pMetaStreamBuffer != 0)
        {   //unlock meta
            GET_META_AND_UNLOCK(frameInfo);
        }
        else
            MY_LOGW("Stream ID=%#" PRIxPTR " not found, cannot unlock!", streamId);
    }

    // unlokc output meta stream and mark error
    for(size_t index=0; index<metaIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vOut.keyAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);
        if (ret != OK)
            return BAD_VALUE;

        // get its frame info
        sp<EffectFrameInfo> frameInfo = request->vOutputFrameInfo.valueFor(bufId);
        //if metadata
        sp<IMetaStreamBuffer> pMetaStreamBuffer = rStreamBufferSet.getMetaBuffer(streamId, getNodeId());
        if (pMetaStreamBuffer != 0)
        {
            //unlock meta
            GET_META_AND_UNLOCK(frameInfo);
            //mark error
            pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
        }
        else
        {
            MY_LOGW("Stream ID=%#" PRIxPTR " not found, cannot unlock!", streamId);
        }
    }
    #undef GET_BUFFER_AND_UNLOCK
    #undef GET_META_AND_UNLOCK

    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
suspendThisFrame(sp<IPipelineFrame> const& pFrame, sp<EffectRequest> const& request)
{
    MY_LOGD_IF(mLogLevel >= 1, "Discard request id=%d", pFrame->getRequestNo());
    //unlock the current buffer inside the EffectRequest
    unlockAndMarkOutStreamError(pFrame, request);
    MERROR err = BaseNode::flush(pFrame);
    return err;
}


MERROR
DepthMapNodeImp::
addImageBufferFrameInfo(sp<EffectRequest>& pEffectReq, DataIOType eIOType,
sp<IPipelineFrame>& pFrame, sp<IImageStreamInfo>& pImgStreamInfo, DepthMapBufferID bid)
{
    sp<IImageBuffer> pImgBuf;
    StreamId_T streamId = pImgStreamInfo->getStreamId();

    MERROR res = retrieveImageBuffer(pFrame, streamId, eIOType, pImgBuf);
    if(res == OK)
    {
        sp<EffectFrameInfo> pEffectFrame = new EffectFrameInfo(pFrame->getRequestNo(), bid);
        pEffectFrame->setFrameBuffer(pImgBuf);
        if(eIOType == IOTYPE_INPUT)
            pEffectReq->vInputFrameInfo.add(bid, pEffectFrame);
        else
            pEffectReq->vOutputFrameInfo.add(bid, pEffectFrame);

        MY_LOGD_IF(mLogLevel >= 2, "add imagebuffer into request, reqIdx=%d bufID=%d buffer size=%dx%d format=%x",
                    pFrame->getRequestNo(), bid, pImgBuf->getImgSize().w, pImgBuf->getImgSize().h, pImgBuf->getImgFormat());
    }else
    {
        MBOOL bValid = validateTolerantStreamID(streamId);
        if(bValid)
        {
            res = OK;
            MY_LOGD_IF(mLogLevel >= 1, "Found missing streamId=%#" PRIxPTR ", can be tolerant! BufID=%d reqIdx=%d",
                        streamId, bid, pFrame->getRequestNo());
            // mark release
            IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();
            rStreamBufferSet.markUserStatus(streamId, getNodeId(),
                            IUsersManager::UserStatus::USED|IUsersManager::UserStatus::RELEASE);
        }
        else
        {
            MY_LOGE("Failed to get ImageBuffer, redID=%d streadmId=%#" PRIxPTR " bufID=%d",
                    pFrame->getRequestNo(), streamId, bid);
        }
    }

    return res;
}


MERROR
DepthMapNodeImp::
addMetaFrameInfo(sp<EffectRequest>& pEffectReq, DataIOType eIOType,
sp<IPipelineFrame>& pFrame, sp<IMetaStreamInfo>& pMetaStreamInfo, DepthMapBufferID bid)
{
    IMetadata* pMetaBuf;
    MERROR res = retrieveMetadata(pFrame, pMetaStreamInfo->getStreamId(), eIOType, pMetaBuf);
    if(res == OK)
    {
        sp<EffectFrameInfo> pEffectFrame = new EffectFrameInfo(pFrame->getRequestNo(), bid);
        sp<EffectParameter> effParam = new EffectParameter();
        effParam->setPtr(VSDOF_PARAMS_METADATA_KEY, (void*)pMetaBuf);
        pEffectFrame->setFrameParameter(effParam);
        if(eIOType == IOTYPE_INPUT)
            pEffectReq->vInputFrameInfo.add(bid, pEffectFrame);
        else
            pEffectReq->vOutputFrameInfo.add(bid, pEffectFrame);
        MY_LOGD_IF(mLogLevel >= 2, "add metadata into request, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bid);
    }else
    {
        MY_LOGE("retrieve metadata failed!! res=%d BufID=%d reqIdx=%d", res, bid, pFrame->getRequestNo());
    }
    return res;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
queue(sp<IPipelineFrame> pFrame)
{
    if( ! pFrame.get() ) {
        MY_LOGE("Null frame");
        return BAD_VALUE;
    }

    IPipelineFrame::InfoIOMapSet IOMapSet;

    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ) ) {
        MY_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }
    // ----- EffectRequest packing ----
    sp<EffectRequest> pEffectReq;
    MUINT32 iReqIdx = pFrame->getRequestNo();
    pEffectReq = new EffectRequest(iReqIdx, onEffectRequestFinished, (void*)this);

    using std::chrono::system_clock;
    system_clock::time_point bef_enque_tp = system_clock::now();

    MY_LOGD_IF(mLogLevel >= 2, "+ , reqId=%d", iReqIdx);

    // add Image Buffer Stream to EffectRequest
    {
        // get image ioMapSet
        IPipelineFrame::ImageInfoIOMapSet& imageIOMapSet = IOMapSet.mImageInfoIOMapSet;

        // imgIOMapSet size should be only 1
        if(imageIOMapSet.size() != 1) {
            MY_LOGE("imageIOMapSet size should be 1. size=%d", imageIOMapSet.size());
            return BAD_VALUE;
        }

        MERROR ret = addEnqueImageBufferToRequest(pFrame, imageIOMapSet, pEffectReq);
        if(ret != OK)
        {
            suspendThisFrame(pFrame, pEffectReq);
            return BAD_VALUE;
        }
    }

    // add Meta Buffer Stream to EffectRequest
    {
        //get meta ioMapSet
        IPipelineFrame::MetaInfoIOMapSet& metaIOMapSet =IOMapSet.mMetaInfoIOMapSet;

        // metaIOMapSet size should be only 1
        if(metaIOMapSet.size() != 1) {
            MY_LOGE("metaIOMapSet size should be 1. size=%d", metaIOMapSet.size());
            return BAD_VALUE;
        }

        // add meta buffer into effect request
        MERROR ret= addEnqueMetaBufferToRequest(pFrame, metaIOMapSet, pEffectReq);
        if(ret != OK)
        {
            suspendThisFrame(pFrame, pEffectReq);
            return BAD_VALUE;
        }
    }

    // add Frame into frame map
    {
        RWLock::AutoWLock _l(mQueueRWLock);
        //record the to-be-enque frame
        mvPipelineFrameMap.add(iReqIdx, pFrame);
    }

    // decide the scenario
    sp<EffectParameter> pEffectParam = new EffectParameter();
    DepthNodeOpState state;

    if(pEffectReq->vOutputFrameInfo.indexOfKey(BID_P2AFM_OUT_MV_F_CAP) >=0)
        state = eSTATE_CAPTURE;
    else
        state = eSTATE_NORMAL;
    // set state to EffectParameter
    pEffectParam->set(DEPTHMAP_REQUEST_STATE_KEY, state);

    // log enque time
    system_clock::time_point after_enque_tp = system_clock::now();
    std::chrono::duration<double> elap = system_clock::now() - bef_enque_tp;
    MY_LOGD_IF(mLogLevel >= 2, "reqID=%d  enque time=%f ms", iReqIdx, elap.count()*1000);

    // add time tag
    if(mLogLevel>=1)
    {
        using std::chrono::system_clock;
        system_clock::time_point* tp = new system_clock::time_point(bef_enque_tp);
        pEffectParam->setPtr(DEPTHMAP_REQUEST_TIMEKEY, tp);
    }

    // set effect parameter to request
    pEffectReq->setRequestParameter(pEffectParam);
    // enque the effect Request
    mpDepthEffectHal->updateEffectRequest(pEffectReq);

    MY_LOGD_IF(mLogLevel >= 2, "- , reqId=%d", iReqIdx);
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
addEnqueImageBufferToRequest(
    sp<IPipelineFrame>& pFrame,
    IPipelineFrame::ImageInfoIOMapSet const& rIOMapSet,
    sp<EffectRequest>& pEffectReq)
{
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = rIOMapSet[0];
    // Add input image stream to EffectRequest
    for(size_t index=0; index<imageIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vIn.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vIn.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        ret = addImageBufferFrameInfo(pEffectReq, IOTYPE_INPUT, pFrame,
                                        streamInfo, bufId);
        if (ret != OK)
            return BAD_VALUE;

        MY_LOGD_IF(mLogLevel >= 2, "input imagBuf stream bid=%d",bufId);
    }

    // Add output image stream to EffectRequest
    for(size_t index=0; index<imageIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vOut.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vOut.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        ret = addImageBufferFrameInfo(pEffectReq, IOTYPE_OUTPUT, pFrame,
                                        streamInfo, bufId);
        if (ret != OK)
            return BAD_VALUE;

       MY_LOGD_IF(mLogLevel >= 2, "output imagBuf stream bid=%d",bufId);
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
addEnqueMetaBufferToRequest(
    sp<IPipelineFrame>& pFrame,
    IPipelineFrame::MetaInfoIOMapSet const& rIOMapSet,
    sp<EffectRequest>& pEffectReq)
{
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = rIOMapSet[0];

    IMetadata* pSplitHalMeta_Main2 = NULL;

    // Add input meta stream to EffectRequest
    for(size_t index=0; index<metaIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vIn.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vIn.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            return BAD_VALUE;

        // Split mpInHalMeta to MAIN1/MAIN2
        if(mpInHalMeta->getStreamId() == streamId)
        {
            pSplitHalMeta_Main2 = new IMetadata();
            MERROR ret = splitHalMetaFrameInfo(pEffectReq, pFrame, pSplitHalMeta_Main2);
            if(ret != OK)
            {
                MY_LOGE("get mpInHalMeta metadata fail.");
                goto lbExit;
            }
        }
        else
        {
            ret = addMetaFrameInfo(pEffectReq, IOTYPE_INPUT, pFrame, streamInfo, bufId);
            if (ret != OK)
                goto lbExit;
        }
        MY_LOGD_IF(mLogLevel >= 2, "input MetaBuf stream bid=%d",bufId);
    }
    // Add output meta stream to EffectRequest
    for(size_t index=0; index<metaIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vOut.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vOut.valueAt(index);
        DepthMapBufferID bufId;
        MERROR ret = mapToBufferID(streamId, bufId);

        if (ret != OK)
            goto lbExit;

        ret = addMetaFrameInfo(pEffectReq, IOTYPE_OUTPUT, pFrame, streamInfo, bufId);
        if (ret != OK)
            goto lbExit;

        MY_LOGD_IF(mLogLevel >= 2, "output MetaBuf stream bid=%d",bufId);
    }

    return OK;

lbExit:
    if(pSplitHalMeta_Main2 != NULL)
        delete pSplitHalMeta_Main2;

    return BAD_VALUE;

}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
splitHalMetaFrameInfo(sp<EffectRequest>& pEffectReq, sp<IPipelineFrame> pFrame, IMetadata*& rpHalMeta_Main2)
{
    // Split HalMeta_Main1 into HalMeta_Main1/Main2
    IMetadata* pMetaBuf;
    if(retrieveMetadata(pFrame, mpInHalMeta->getStreamId(), IOTYPE_INPUT, pMetaBuf) == OK)
    {
        if( ! tryGetMetadata<IMetadata>(const_cast<IMetadata*>(pMetaBuf), MTK_P1NODE_MAIN2_HAL_META, *rpHalMeta_Main2) ){
            MY_LOGE("cannot get MTK_P1NODE_MAIN2_HAL_META after updating request");
            return BAD_VALUE;
        }
        sp<EffectFrameInfo> effectFrame = new EffectFrameInfo(pFrame->getRequestNo(), BID_META_IN_HAL);
        sp<EffectParameter> effParam = new EffectParameter();
        effParam->setPtr(VSDOF_PARAMS_METADATA_KEY, (void*)pMetaBuf);
        effectFrame->setFrameParameter(effParam);
        pEffectReq->vInputFrameInfo.add(BID_META_IN_HAL, effectFrame);
        //
        effectFrame = new EffectFrameInfo(pFrame->getRequestNo(), BID_META_IN_HAL_MAIN2);
        effParam = new EffectParameter();
        effParam->setPtr(VSDOF_PARAMS_METADATA_KEY, (void*)rpHalMeta_Main2);
        effectFrame->setFrameParameter(effParam);
        pEffectReq->vInputFrameInfo.add(BID_META_IN_HAL_MAIN2, effectFrame);

        return OK;
    }
    else
        return BAD_VALUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
mapToStreamID(DepthMapBufferID bufID, StreamId_T& rStreamId)
{
    MY_LOGD_IF(mLogLevel >= 1, "mapToStreamID: bufID=%d", bufID);

    if(mDepthBIDToStreamIDMap.indexOfKey(bufID)>=0)
    {
        rStreamId = mDepthBIDToStreamIDMap.valueFor(bufID);
        return OK;
    }
    else
    {
        MY_LOGE("mapToStreamID Failed!!! bufID=%d", bufID);
        return BAD_VALUE;
    }
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
mapToBufferID(StreamId_T streamId, DepthMapBufferID &rBufID)
{



    if(mStreamIDMapToDepthBID.indexOfKey(streamId)>=0)
    {
        rBufID = mStreamIDMapToDepthBID.valueFor(streamId);
        MY_LOGD_IF(mLogLevel >= 1, "mapToBufferID: streamId=%#" PRIxPTR " BufID=%d", streamId, rBufID);
        return OK;
    }
    else
    {
        MY_LOGE("mapToBufferID Failed!!! streamId=%#" PRIxPTR "", streamId);
        return BAD_VALUE;
    }
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
retrieveImageBuffer(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId, DataIOType eType, sp<IImageBuffer>& rpImageBuffer)
{
    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();

    sp<IImageStreamBuffer>  pImageStreamBuffer = NULL;

    MERROR const err = ensureImageBufferAvailable_(
        pFrame->getFrameNo(),
        streamId,
        rStreamBufferSet,
        pImageStreamBuffer
    );

    if(err != OK)
    {
        MY_LOGW("reqID=%d streamId: %#" PRIxPTR ", err = %d , failed to get pImageStreamBuffer! ",
                pFrame->getRequestNo(), streamId, err);
        return err;
    }

    MUINT groupUsage = pImageStreamBuffer->queryGroupUsage(getNodeId());
    sp<IImageBufferHeap> pImageBufferHeap = (eType == IOTYPE_INPUT) ?
        pImageStreamBuffer->tryReadLock(getNodeName()) :
        pImageStreamBuffer->tryWriteLock(getNodeName());

    if(pImageBufferHeap == NULL)
    {
        MY_LOGW("[node:%#" PRIxPTR "][stream buffer:%s] reqID=%d cannot get ImageBufferHeap",
                getNodeId(), pImageStreamBuffer->getName(), pFrame->getRequestNo());
        return BAD_VALUE;
    }

    rpImageBuffer = pImageBufferHeap->createImageBuffer();
    if (rpImageBuffer == NULL)
    {
        MY_LOGE("[node:%#" PRIxPTR "][stream buffer:%s] reqID=%d cannot create ImageBuffer",
                getNodeId(), pImageStreamBuffer->getName(), pFrame->getRequestNo());
        return BAD_VALUE;
    }
    MBOOL bRet = rpImageBuffer->lockBuf(getNodeName(), groupUsage);
    if(!bRet)
    {
        MY_LOGE("reqID=%d LockBuf failed, stream%#" PRIxPTR "!", pFrame->getRequestNo(), streamId);
        return BAD_VALUE;
    }

    MY_LOGD_IF(mLogLevel >= 1, "stream %#" PRIxPTR ": buffer: %p, usage: %p",
        streamId, rpImageBuffer.get(), groupUsage);

    return OK;
}


MBOOL
DepthMapNodeImp::
validateTolerantStreamID(StreamId_T const streamId)
{
    ssize_t index = mvTolerantStreamId.indexOfKey(streamId);
    if(index<0)
        return MFALSE;
    else
        return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
releaseImageBuffer(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId, DataIOType eType, sp<IImageBuffer> const& pImageBuffer, MBOOL bIsSuccess)
{

    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();
    sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, getNodeId());
    pImageBuffer->unlockBuf(getNodeName());
    pStreamBuffer->unlock(getNodeName(), pImageBuffer->getImageBufferHeap());

    if(eType==IOTYPE_OUTPUT)
    {
        if(bIsSuccess)
            pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
        else
            pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
    }
    //
    //  Mark this buffer as USED by this user.
    //  Mark this buffer as RELEASE by this user.
    rStreamBufferSet.markUserStatus(
            streamId,
            getNodeId(),
            IUsersManager::UserStatus::USED|IUsersManager::UserStatus::RELEASE);
    return OK;
}



/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
retrieveMetadata(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId, DataIOType eType, IMetadata*& rpMetadata)
{
    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();

    sp<IMetaStreamBuffer>  pMetaStreamBuffer = NULL;

    MERROR const err = ensureMetaBufferAvailable_(
        pFrame->getFrameNo(),
        streamId,
        rStreamBufferSet,
        pMetaStreamBuffer
    );

    if(err != OK)
    {
        MY_LOGE("reqID=%d streamId: %#" PRIxPTR " err=%d, failed to get pMetaStreamBuffer!", pFrame->getRequestNo(), streamId, err);
        return BAD_VALUE;
    }

    rpMetadata = (eType==IOTYPE_INPUT) ?
        pMetaStreamBuffer->tryReadLock(getNodeName()) :
        pMetaStreamBuffer->tryWriteLock(getNodeName());


    if(rpMetadata == NULL)
    {
        MY_LOGE("[node:%#" PRIxPTR "][stream buffer:%s] reqID=%d, Cannot get Metadata",
                getNodeId(), pMetaStreamBuffer->getName(), pFrame->getRequestNo());

    }


    return OK;

}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
DepthMapNodeImp::
releaseMetadata(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId, DataIOType eType, IMetadata* const pMetadata, MBOOL bIsSuccess)
{
    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();
    sp<IMetaStreamBuffer>  pMetaStreamBuffer = rStreamBufferSet.getMetaBuffer(streamId, getNodeId());

    pMetaStreamBuffer->unlock(getNodeName(), pMetadata);

    if(eType==IOTYPE_OUTPUT)
    {
        if(bIsSuccess)
            pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
        else
            pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
    }
    rStreamBufferSet.markUserStatus(streamId, getNodeId(), IUsersManager::UserStatus::USED|IUsersManager::UserStatus::RELEASE);

    return OK;
}

