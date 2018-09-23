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


//
#include "BaseNode.h"
//
#include "hwnode_utilities.h"
//
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/hwnode/BMDepthMapNode.h>
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>
#include <mtkcam/feature/stereo/pipe/IDepthMapEffectRequest.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/feature/stereo/effecthal/BMDepthMapEffectHal.h>
//
#include <utils/RWLock.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include <cutils/properties.h>
//
#include <sys/prctl.h>
#include <sys/resource.h>
//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <chrono>
//#include <met_tag.h>

#define LOG_TAG "MtkCam/BMDepthMapNode"

// MET tags
#define DO_DEPTHMAP "do depth map"

using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils::Sync;

using namespace NSCam::NSCamFeature::NSFeaturePipe_DepthMap_BM;
using namespace NSCam::NSCamFeature_BM;

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

class BMDepthMapNodeImp
    : public BaseNode
    , public BMDepthMapNode

{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    BMDepthMapNodeImp();
    virtual ~BMDepthMapNodeImp();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MERROR init(InitParams_Base const& rParams);
    virtual MERROR init(InitParams const& rParams);
    virtual MERROR config(ConfigParams const& rParams);
    virtual MERROR uninit();
    virtual MERROR flush();
    virtual MERROR queue(sp<IPipelineFrame> pFrame);


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDepthMapNodeImp private function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:

    /**
     * @brief callback launched when the request is finished
     * @param [in] tag Cookie pointer
     * @param [in] status Callback result status string
     * @param [in] pRequest Finished Efferct Request
     */
    static MVOID onEffectRequestFinished(
                                    MVOID* tag,
                                    String8 status,
                                    sp<EffectRequest>& pRequest);
    /**
     * @brief internal callback launched when the request is finished
     * @param [in] status Callback result status string
     * @param [in] pRequest Finished Efferct Request
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR onEffectReqDone(
                    String8 status,
                    sp<IDepthMapEffectRequest>& pRequest);
    /**
     * @brief Request success handle functino
     * @param [in] pFrame Pipeline frame
     * @param [in] pRequest Finished Efferct Request
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR onEffectReqSucess(
                    sp<IPipelineFrame> const& pFrame,
                    sp<IDepthMapEffectRequest>& pRequest);
    /**
     * @brief Request failure handle functino
     * @param [in] pFrame Pipeline frame
     * @param [in] pRequest Finished Efferct Request
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR onEffectReqFailed(
                    String8 status,
                    sp<IPipelineFrame> const& pFrame,
                    sp<IDepthMapEffectRequest>& pRequest);
    /**
     * @brief release in/out stream
     * @param [in] pFrame Pipeline frame
     * @param [in] pRequest Finished Efferct Request
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR releaseAllInputStreams(
                        sp<IDepthMapEffectRequest>& pRequest,
                        sp<IPipelineFrame> const& pFrame);
    MERROR releaseAllOutputStreams(
                        sp<IDepthMapEffectRequest>& pRequest,
                        sp<IPipelineFrame> const& pFrame,
                        MBOOL bIsSuccess,
                        SortedVector<StreamId_T>* pExceptStreamId=NULL);
    /**
     * @brief retrieve imagea stream  inside frame
     * @param [in] pFrame Pipeline frame
     * @param [in] streamId Retrieving Stream ID
     * @param [in] eType StreamID IO Type
     * @param [out] rpImageBuffer output imagebuffer
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR retrieveImageBuffer(
                        sp<IPipelineFrame> const& pFrame,
                        StreamId_T const streamId,
                        DepthNodeBufIOType eType,
                        sp<IImageBuffer>& rpImageBuffer);
    /**
     * @brief retrieve metadata inside frame
     * @param [in] pFrame Pipeline frame
     * @param [in] streamId Retrieving Stream ID
     * @param [in] eType StreamID IO Type
     * @param [out] rpMetadata output metadata
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR retrieveMetadata(
                        sp<IPipelineFrame> const& pFrame,
                        StreamId_T const streamId,
                        DepthNodeBufIOType eType,
                        IMetadata*& rpMetadata);
    /**
     * @brief release image/metadata inside frame
     * @param [in] pFrame Pipeline frame
     * @param [in] streamId Retrieving Stream ID
     * @param [in] eType StreamID IO Type
     * @param [in] pImageBuffer buffer to release
     * @param [in] bIsSuccess the request result status
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR releaseImageBuffer(
                    sp<IPipelineFrame> const& pFrame,
                    StreamId_T const streamId,
                    DepthNodeBufIOType eType,
                    sp<IImageBuffer> const& pImageBuffer,
                    MBOOL bIsSuccess);

    MERROR releaseMetadata(
                    sp<IPipelineFrame> const& pFrame,
                    StreamId_T const streamId,
                    DepthNodeBufIOType eType,
                    IMetadata* pMetadata,
                    MBOOL bIsSuccess);
    /**
     * @brief check the stream is allowed to be null
     * @param [in] streamId Retrieving Stream ID
     * @return
     * - MTRUE indicates yes
     * - MFALSE indicates no.
     */
    MBOOL validateTolerantStreamID(StreamId_T const streamId);

    /**
     * @brief Iterate the ioMapSet and add imagebuffer/metadata into request
     * @param [in] pFrame PipelineFrame
     * @param [in] rIOMapSet IOMapSet
     * @param [out] pEffectReq Output EffectRequest
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR addEnqueImageBufferToRequest(
                                sp<IPipelineFrame>& pFrame,
                                IPipelineFrame::ImageInfoIOMapSet const& rIOMapSet,
                                sp<IDepthMapEffectRequest>& pEffectReq);

    MERROR addEnqueMetaBufferToRequest(
                                sp<IPipelineFrame>& pFrame,
                                IPipelineFrame::MetaInfoIOMapSet const& rIOMapSet,
                                sp<IDepthMapEffectRequest>& pEffectReq);
    /**
     * @brief Split Main1 HAL META into main2 Hal meta
     * @param [out] pEffectReq EffectRequest
     * @param [in] pFrame PipelineFrame
     * @param [out] rpHalMeta_Main2 Main2 Hal Meta
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR splitHalMetaFrameInfo(
                            sp<IDepthMapEffectRequest>& pEffectReq,
                            sp<IPipelineFrame> pFrame,
                            IMetadata*& rpHalMeta_Main2);

    /**
     * @brief add imagebuffer/metadata into request
     * @param [in] pFrame PipelineFrame
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR addImageBufferFrameInfo(
                            const sp<IPipelineFrame>& pFrame,
                            const sp<IImageStreamInfo>& pImgStreamInfo,
                            const NodeBufferSetting& setting,
                            sp<IDepthMapEffectRequest>& pEffectReq);

    MERROR addMetaFrameInfo(
                        const sp<IPipelineFrame>& pFrame,
                        const sp<IMetaStreamInfo>& pMetaStreamInfo,
                        const NodeBufferSetting& setting,
                        sp<IDepthMapEffectRequest>& pEffectReq);

    /**
     * @brief Map stream ID to buffer ID
     * @param [in] streamId streamID
     * @param [out] rBufID bufferID
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR mapToBufferID(StreamId_T streamId, DepthMapBufferID &rBufID);
    /**
     * @brief Map buffer id to stream id
     * @param [in] bufID buffer id
     * @param [out] rStreamId streamID
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR mapToStreamID(DepthMapBufferID bufID, StreamId_T& rStreamId);

    /**
     * @brief Suspend frame operation
     * @param [in] pFrame PipelineFrame
     * @param [in] pRequest EffectRequest
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR suspendThisFrame(
                    sp<IPipelineFrame> const& pFrame,
                    sp<IDepthMapEffectRequest> const& pRequest);
    /**
     * @brief unlock all stream and mark output stream error
     * @param [in] pFrame PipelineFrame
     * @param [in] pRequest EffectRequest
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR unlockAndMarkOutStreamError(
                    sp<IPipelineFrame> const& pFrame,
                    sp<IDepthMapEffectRequest> pRequest);
    /**
     * @brief generate bufferid-streamid map
     * @return
     * - OK indicates success.
     * - others indicates failure.
     */
    MERROR generateBIDMaps();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDepthMapNodeImp protected member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    MINT32 mLogLevel;
    MINT32 mOpenId_Main2;
    // meta streaminfo
    sp<IMetaStreamInfo> mpInAppMeta = NULL;
    sp<IMetaStreamInfo> mpInHalMeta = NULL;
    sp<IMetaStreamInfo> mpInHalMeta_Main2 = NULL;
    sp<IMetaStreamInfo> mpOutAppMeta = NULL;
    sp<IMetaStreamInfo> mpOutHalMeta = NULL;
    // image streaminfo - input
    sp<IImageStreamInfo> mpInHalResizeRaw = NULL;
    sp<IImageStreamInfo> mpInHalResizeRaw_Main2 = NULL;
    sp<IImageStreamInfo> mpInHalFullRaw = NULL;
    sp<IImageStreamInfo> mpInHalFullRaw_Main2 = NULL;
    // image streaminfo - output
    sp<IImageStreamInfo> mpOutAppImageExtraData = NULL;
    sp<IImageStreamInfo> mpOutAppImageLDC = NULL;
    sp<IImageStreamInfo> mpOutAppImageDepthMap = NULL;
    sp<IImageStreamInfo> mpOutHalImageMainImage = NULL;
    sp<IImageStreamInfo> mpOutHalImageMainImage_Cap = NULL;
    sp<IImageStreamInfo> mpOutHalImageMY_S = NULL;
    sp<IImageStreamInfo> mpOutHalImageDMW = NULL;
    sp<IImageStreamInfo> mpOutHalImageFD = NULL;
    sp<IImageStreamInfo> mpOutHalImageJPSMain1 = NULL;
    sp<IImageStreamInfo> mpOutHalImageJPSMain2 = NULL;
    sp<IImageStreamInfo> mpOutHalImageDisparity_Left = NULL;
    sp<IImageStreamInfo> mpOutHalImageDisparity_Right = NULL;
    sp<IImageStreamInfo> mpOutHalImageWarpingMatrix = NULL;

    // DepthMap effecthal
    DepthMapEffectHal* mpDepthEffectHal = NULL;
    // Pipeline frame container
    KeyedVector< MUINT32, sp<IPipelineFrame> > mvPipelineFrameMap;
    mutable RWLock                  mFrameMapRWLock;
    // node config lock
    mutable RWLock                  mConfigRWLock;
    // init flag
    bool                            mbInit;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDepthMapNodeImp private member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
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
sp<BMDepthMapNode>
BMDepthMapNode::
createInstance()
{
    return new BMDepthMapNodeImp();
}


/******************************************************************************
 *
 ******************************************************************************/
BMDepthMapNodeImp::
BMDepthMapNodeImp()
    : BaseNode()
    , BMDepthMapNode()
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
BMDepthMapNodeImp::
~BMDepthMapNodeImp()
{
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
init(InitParams_Base const& rParams)
{
    MY_LOGE("Error: need to use the BMDepthMapNode::InitParams!");
    return INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
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
    effParam->set(EFFECTKEY_SENSOR_IDX_MAIN1, mOpenId);
    effParam->set(EFFECTKEY_SENSOR_IDX_MAIN2, mOpenId_Main2);
    effParam->set(EFFECTKEY_SENSOR_STATE, (int)rParams.sensorState);
    // feature mode decision
    if(!rParams.vsdof_mode && !rParams.stereoCapture_mode && rParams.denoise_mode)
        effParam->set(EFFECTKEY_FEATURE_MODE, (int)eDEPTHNODE_MODE_DENOISE);
    else
    {
        MY_LOGE("Cannot support this feature mode: vsdof_mode=%d stereoCapture_mode=%d denoise_mode=%d",
                rParams.vsdof_mode, rParams.stereoCapture_mode , rParams.denoise_mode);
        return BAD_VALUE;
    }
    mpDepthEffectHal->setParameters(effParam);

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
config(ConfigParams const& rParams)
{
    FUNC_START;

    if(!mbInit)
    {
        MY_LOGW("Not perform init operations yet!");
        return INVALID_OPERATION;
    }

    // init stream info
    {
        RWLock::AutoWLock _l(mConfigRWLock);
        // metadata
        mpInAppMeta = rParams.pInAppMeta;
        mpInHalMeta = rParams.pInHalMeta;
        mpInHalMeta_Main2 = rParams.pInHalMeta_Main2;
        mpOutAppMeta = rParams.pOutAppMeta;
        mpOutHalMeta = rParams.pOutHalMeta;
        // hal input image
        mpInHalResizeRaw = rParams.pInResizedRaw;
        mpInHalResizeRaw_Main2 = rParams.pInResizedRaw_Main2;
        mpInHalFullRaw = rParams.pInFullRaw;
        mpInHalFullRaw_Main2 = rParams.pInFullRaw_Main2;
        // hal output image
        mpOutHalImageMainImage = rParams.pHalImageMainImg;
        mpOutHalImageMainImage_Cap = rParams.pHalImageMainImg_Cap;
        mpOutHalImageMY_S = rParams.pHalImageMY_S;
        mpOutHalImageDMW = rParams.pHalImageDMW;
        if(rParams.pHalImageFD != NULL)
        {
            mpOutHalImageFD = rParams.pHalImageFD;
            // FD is tolerant streamID
            mvTolerantStreamId.add(mpOutHalImageFD->getStreamId(), MTRUE);
        }
        mpOutHalImageJPSMain1 = rParams.pHalImageJPSMain1;
        mpOutHalImageJPSMain2 = rParams.pHalImageJPSMain2;
        mpOutHalImageDisparity_Left = rParams.pHalImageDisparityMap_Left;
        mpOutHalImageDisparity_Right = rParams.pHalImageDisparityMap_Right;
        mpOutHalImageWarpingMatrix = rParams.pHalImageWarpingMatrix;
        // app output image buffer
        mpOutAppImageLDC = rParams.pAppImageLDC;
        mpOutAppImageExtraData = rParams.pAppImageExtraData;
        mpOutAppImageDepthMap = rParams.pAppImageDepthMap;
        // config effect hal
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
BMDepthMapNodeImp::
generateBIDMaps()
{
    FUNC_START;
    // hal input image stream
    ADD_BID_STREAMID_MAP(mpInHalResizeRaw, BID_P2AFM_IN_RSRAW1);
    ADD_BID_STREAMID_MAP(mpInHalResizeRaw_Main2, BID_P2AFM_IN_RSRAW2);
    ADD_BID_STREAMID_MAP(mpInHalFullRaw, BID_P2AFM_IN_FSRAW1);
    ADD_BID_STREAMID_MAP(mpInHalFullRaw_Main2, BID_P2AFM_IN_FSRAW2);
    // hal output image
    ADD_BID_STREAMID_MAP(mpOutHalImageMY_S, BID_OCC_OUT_MY_S);
    ADD_BID_STREAMID_MAP(mpOutHalImageDMW, BID_WMF_OUT_DMW);
    ADD_BID_STREAMID_MAP(mpOutHalImageMainImage, BID_P2AFM_OUT_MV_F);
    ADD_BID_STREAMID_MAP(mpOutHalImageMainImage_Cap, BID_P2AFM_OUT_MV_F_CAP);
    ADD_BID_STREAMID_MAP(mpOutHalImageJPSMain1, BID_N3D_OUT_JPS_MAIN1);
    ADD_BID_STREAMID_MAP(mpOutHalImageJPSMain2, BID_N3D_OUT_JPS_MAIN2);
    ADD_BID_STREAMID_MAP(mpOutHalImageFD, BID_P2AFM_OUT_FDIMG);
    ADD_BID_STREAMID_MAP(mpOutHalImageDisparity_Left, BID_DPE_OUT_DMP_L);
    ADD_BID_STREAMID_MAP(mpOutHalImageDisparity_Right, BID_DPE_OUT_DMP_R);
    ADD_BID_STREAMID_MAP(mpOutHalImageWarpingMatrix, BID_N3D_OUT_WARPING_MATRIX);
    // app
    ADD_BID_STREAMID_MAP(mpOutAppImageLDC, BID_N3D_OUT_LDC);
    ADD_BID_STREAMID_MAP(mpOutAppImageDepthMap, BID_WMF_OUT_DEPTHMAP);
    ADD_BID_STREAMID_MAP(mpOutAppImageExtraData, BID_FD_OUT_EXTRADATA);
    // meta stream
    ADD_BID_STREAMID_MAP(mpInAppMeta, BID_META_IN_APP);
    ADD_BID_STREAMID_MAP(mpInHalMeta, BID_META_IN_HAL_MAIN1);
    ADD_BID_STREAMID_MAP(mpOutAppMeta, BID_META_OUT_APP);
    ADD_BID_STREAMID_MAP(mpOutHalMeta, BID_META_OUT_HAL);


    FUNC_END;
    return OK;

}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
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
BMDepthMapNodeImp::
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
BMDepthMapNodeImp::
onEffectRequestFinished(MVOID* tag, String8 status, sp<EffectRequest>& pRequest)
{
    BMDepthMapNodeImp *depthMapNode = (BMDepthMapNodeImp *) tag;
    sp<IDepthMapEffectRequest> pDepthReq = (IDepthMapEffectRequest*) pRequest.get();
    depthMapNode->onEffectReqDone(status, pDepthReq);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
onEffectReqDone(
    String8 status,
    sp<IDepthMapEffectRequest>& pRequest
)
{
    MUINT32 reqID = pRequest->getRequestNo();
    sp<IPipelineFrame> pFrame;
    {
        RWLock::AutoWLock _l(mFrameMapRWLock);
        pFrame = mvPipelineFrameMap.valueFor(reqID);
        mvPipelineFrameMap.removeItem(reqID);
    }

    if(status == DEPTHMAP_COMPLETE_KEY)
    {
        onEffectReqSucess(pFrame, pRequest);
    }
    else if(status == DEPTHMAP_FLUSH_KEY)
    {
        suspendThisFrame(pFrame, pRequest);
    }
    else
    {
        onEffectReqFailed(status, pFrame, pRequest);
    }
    // release the split MAIN2 metadata
    IMetadata *pMetaMain2;
    pRequest->getRequestMetadata({BID_META_IN_HAL_MAIN2, eBUFFER_IOTYPE_INPUT}, pMetaMain2);
    delete pMetaMain2;

    // stop timer adb log
    pRequest->stopTimer();
    MY_LOGD_IF(mLogLevel >= 1, "Request finished! reqID=%d, status=%s, Total cost time=%d ms",
                reqID, status.string(), pRequest->getElapsedTime());
    //met_tag_end(0, DO_DEPTHMAP);



    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
onEffectReqFailed(String8 status, sp<IPipelineFrame> const& pFrame, sp<IDepthMapEffectRequest>& pRequest)
{
    MY_LOGD_IF(mLogLevel >= 1, "+");
    MUINT32 reqID = pRequest->getRequestNo();

    if(status == DEPTHMAP_ERROR_KEY)
    {
        MY_LOGD_IF(mLogLevel >= 1, " status: %s", DEPTHMAP_ERROR_KEY);
        // release all input stream
        releaseAllInputStreams(pRequest, pFrame);
        // mark all output stream failed
        releaseAllOutputStreams(pRequest, pFrame, MFALSE);
        // dispatch
        MY_LOGD_IF(mLogLevel >= 1, "BMDepthMapNode: applyRelease reqID=%d", reqID);
        pFrame->getStreamBufferSet().applyRelease(getNodeId());
        MY_LOGD_IF(mLogLevel >= 1, "BMDepthMapNode: onDispatchFrame reqID=%d", reqID);
        onDispatchFrame(pFrame);
        MY_LOGD_IF(mLogLevel >= 1, "-");
    }
    else
    {
        MY_LOGE("Un-recognized status=%s", status.string());
        suspendThisFrame(pFrame, pRequest);
    }
    return OK;
}

MERROR
BMDepthMapNodeImp::
onEffectReqSucess(
    sp<IPipelineFrame> const& pFrame,
    sp<IDepthMapEffectRequest>& pRequest
)
{
    MUINT32 reqID = pRequest->getRequestNo();
    MY_LOGD_IF(mLogLevel >= 1, "+ , reqId=%d", reqID);

    //release all input stream
    releaseAllInputStreams(pRequest, pFrame);
    // release all output stream
    releaseAllOutputStreams(pRequest, pFrame, MTRUE);

    // dispatch
    MY_LOGD_IF(mLogLevel >= 1, "BMDepthMapNode: applyRelease reqID=%d", reqID);
    pFrame->getStreamBufferSet().applyRelease(getNodeId());
    MY_LOGD_IF(mLogLevel >= 1, "BMDepthMapNode: onDispatchFrame reqID=%d", reqID);
    onDispatchFrame(pFrame);
    MY_LOGD_IF(mLogLevel >= 1, "-");

    return OK;
}

MERROR
BMDepthMapNodeImp::
releaseAllOutputStreams(
    sp<IDepthMapEffectRequest>& pRequest,
    sp<IPipelineFrame> const& pFrame,
    MBOOL bIsSuccess,
    SortedVector<StreamId_T>* pExceptStreamId
)
{
    MY_LOGD("+");
    // get IOMapSet of this frame, which define all the in/out stream inside the frame
    IPipelineFrame::InfoIOMapSet IOMapSet;
    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ) ) {
        MY_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }

    // get the image stream IOMapSet
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = IOMapSet.mImageInfoIOMapSet[0];

    IImageBuffer *pImgBuf;

    // iterate all the output image stream and mark release status
    for(size_t index=0; index<imageIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vOut.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vOut.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

        if (ret != OK)
            return BAD_VALUE;

        if(!pRequest->getRequestImageBuffer({bufID, eBUFFER_IOTYPE_OUTPUT}, pImgBuf))
        {
            // check the missing stream is tolerant
            if(!validateTolerantStreamID(streamId))
                MY_LOGE("Cannot find the output frameInfo, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufID);
            continue;
        }

        // check except stream id
        if(pExceptStreamId == NULL || pExceptStreamId->indexOf(streamId) < 0)
            releaseImageBuffer(pFrame, streamInfo->getStreamId(), eBUFFER_IOTYPE_OUTPUT, pImgBuf, bIsSuccess);
        else
            releaseImageBuffer(pFrame, streamInfo->getStreamId(), eBUFFER_IOTYPE_OUTPUT, pImgBuf, !bIsSuccess);
        MY_LOGD_IF(mLogLevel >= 1, "release imagebuffer, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufID);
    }

    // get the meta stream IOMapSet
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = IOMapSet.mMetaInfoIOMapSet[0];

    // iterate all the output meta stream and mark release status
    for(size_t index=0; index<metaIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vOut.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vOut.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);
        if (ret != OK)
            return BAD_VALUE;

        sp<EffectParameter> effParam = pRequest->vOutputFrameInfo.valueFor(bufID)->getFrameParameter();
        IMetadata* pMetaBuf;

        MBOOL bRet = pRequest->getRequestMetadata({bufID, eBUFFER_IOTYPE_OUTPUT}, pMetaBuf);
        if(!bRet)
        {
            MY_LOGE("Cannot get metadata, bufferID=%d ioType=%d", bufID, eBUFFER_IOTYPE_OUTPUT);
            return BAD_VALUE;
        }
        // check except stream id
        if(pExceptStreamId == NULL || pExceptStreamId->indexOf(streamId) < 0)
            releaseMetadata(pFrame, streamInfo->getStreamId(), eBUFFER_IOTYPE_OUTPUT, pMetaBuf, bIsSuccess);
        else
            releaseMetadata(pFrame, streamInfo->getStreamId(), eBUFFER_IOTYPE_OUTPUT, pMetaBuf, !bIsSuccess);
        MY_LOGD_IF(mLogLevel >= 1, "release metadata, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufID);
    }
    MY_LOGD("-");
    return OK;
}

MERROR
BMDepthMapNodeImp::
releaseAllInputStreams(
    sp<IDepthMapEffectRequest>& pRequest,
    sp<IPipelineFrame> const& pFrame
)
{
    MY_LOGD("+");
    // get IOMapSet of this frame, which define all the in/out stream inside the frame
    IPipelineFrame::InfoIOMapSet IOMapSet;
    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ) ) {
        MY_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }

    // get the image stream IOMapSet
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = IOMapSet.mImageInfoIOMapSet[0];
    MBOOL bRet;
    MY_LOGD_IF(mLogLevel >= 1, "ImageBuffer: vIn size=%d, vOut size=%d", imageIOMap.vIn.size(), imageIOMap.vOut.size());

    IImageBuffer* pImgBuf;
     // iterate all the input image stream and release
    for(size_t index=0; index<imageIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vIn.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vIn.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

        if (ret != OK)
            return BAD_VALUE;

        bRet = pRequest->getRequestImageBuffer({bufID, eBUFFER_IOTYPE_INPUT}, pImgBuf);
        if(!bRet)
        {
            MY_LOGE("Cannot get request imageBuffer!! bufferID=%d", bufID);
            return BAD_VALUE;;
        }
        releaseImageBuffer(pFrame, streamInfo->getStreamId(), eBUFFER_IOTYPE_INPUT, pImgBuf, MTRUE);
        MY_LOGD_IF(mLogLevel >= 1, "release imagebuffer, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufID);
    }

    // get the meta stream IOMapSet
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = IOMapSet.mMetaInfoIOMapSet[0];
    MY_LOGD_IF(mLogLevel >= 1, "Metadata: vIn size=%d, vOut size=%d", metaIOMap.vIn.size(), metaIOMap.vOut.size());

    // input meta stream
    for(size_t index=0; index<metaIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vIn.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vIn.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

        if (ret != OK)
            return BAD_VALUE;

        IMetadata* pMetaBuf;
        bRet = pRequest->getRequestMetadata({bufID, eBUFFER_IOTYPE_INPUT}, pMetaBuf);
        if(!bRet)
        {
            MY_LOGE("Cannot get request metadata buffer!! bufferID=%d", bufID);
            return BAD_VALUE;;
        }
        releaseMetadata(pFrame, streamInfo->getStreamId(), eBUFFER_IOTYPE_INPUT, pMetaBuf, MTRUE);
        MY_LOGD_IF(mLogLevel >= 1, "release metadata, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bufID);
    }
    MY_LOGD("-");
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
unlockAndMarkOutStreamError(sp<IPipelineFrame> const& pFrame, sp<IDepthMapEffectRequest> pRequest)
{
    FUNC_START;
    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();

    MERROR ret;
    StreamId_T streamId;
    sp<IImageBuffer> pImageBuffer;
    IMetadata* pMetaData;

    #define GET_BUFFER_AND_UNLOCK(frameInfo, pImageBuffer) \
        frameInfo->getFrameBuffer(pImageBuffer);\
        pImageBuffer->unlockBuf(getNodeName());\
        pStreamBuffer->unlock(getNodeName(), pImageBuffer->getImageBufferHeap());

    #define GET_META_AND_UNLOCK(frameInfo, pMetaData) \
        pMetaData = reinterpret_cast<IMetadata*>(frameInfo->getFrameParameter()\
                                            ->getPtr(DEPTHMAP_META_KEY_STRING));\
        pMetaStreamBuffer->unlock(getNodeName(), pMetaData);

    // iterate all input frame info, unlock buffer
    for(size_t index=0; index<pRequest->vInputFrameInfo.size();index++)
    {
        DepthMapBufferID bufID = (DepthMapBufferID)pRequest->vInputFrameInfo.keyAt(index);
        // skip the BID_META_IN_HAL_MAIN2, it has no actual streamID
        if(bufID == BID_META_IN_HAL_MAIN2)
            continue;

        sp<EffectFrameInfo> frameInfo = pRequest->vInputFrameInfo.valueAt(index);
        ret = mapToStreamID(bufID, streamId);
        if(ret != OK)
            continue;

        // if image buffer
        sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, getNodeId());
        if (pStreamBuffer != 0)
        {   //unlock buffer
            GET_BUFFER_AND_UNLOCK(frameInfo, pImageBuffer);
            continue;
        }
        //if metadata
        sp<IMetaStreamBuffer> pMetaStreamBuffer = rStreamBufferSet.getMetaBuffer(streamId, getNodeId());
        if (pMetaStreamBuffer != 0)
        {   //unlock meta
            GET_META_AND_UNLOCK(frameInfo, pMetaData);
        }
        else
        {
            MY_LOGW("Stream ID=%#" PRIxPTR " not found, cannot unlock!", streamId);
        }
    }

    // iterate all output frame info, unlock buffer, mark error status
    for(size_t index=0; index<pRequest->vOutputFrameInfo.size();index++)
    {
        DepthMapBufferID bufID = (DepthMapBufferID)pRequest->vOutputFrameInfo.keyAt(index);
        sp<EffectFrameInfo> frameInfo = pRequest->vOutputFrameInfo.valueAt(index);
        ret = mapToStreamID(bufID, streamId);
        if(ret != OK)
            continue;

        // if image buffer
        sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, getNodeId());
        if (pStreamBuffer != 0)
        {   //unlock buffer
            GET_BUFFER_AND_UNLOCK(frameInfo, pImageBuffer);
            //mark error
            pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
            continue;
        }
        //if metadata
        sp<IMetaStreamBuffer> pMetaStreamBuffer = rStreamBufferSet.getMetaBuffer(streamId, getNodeId());
        if (pMetaStreamBuffer != 0)
        {   //unlock meta
            GET_META_AND_UNLOCK(frameInfo, pMetaData);
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
BMDepthMapNodeImp::
suspendThisFrame(sp<IPipelineFrame> const& pFrame, sp<IDepthMapEffectRequest> const& pRequest)
{
    MY_LOGD_IF(mLogLevel >= 1, "Discard request id=%d", pFrame->getRequestNo());
    //unlock the current buffer inside the EffectRequest
    unlockAndMarkOutStreamError(pFrame, pRequest);
    MERROR err = BaseNode::flush(pFrame);
    return err;
}


MERROR
BMDepthMapNodeImp::
addImageBufferFrameInfo(
    const sp<IPipelineFrame>& pFrame,
    const sp<IImageStreamInfo>& pImgStreamInfo,
    const NodeBufferSetting& setting,
    sp<IDepthMapEffectRequest>& pEffectReq
)
{
    DepthMapBufferID bid = setting.bufferID;
    sp<IImageBuffer> pImgBuf;
    StreamId_T streamId = pImgStreamInfo->getStreamId();

    MERROR res = retrieveImageBuffer(pFrame, streamId, setting.ioType, pImgBuf);
    if(res == OK)
    {
        pEffectReq->pushRequestImageBuffer(setting, pImgBuf);
        MY_LOGD_IF(mLogLevel >= 2, "add imagebuffer into request, reqIdx=%d bufID=%d", pFrame->getRequestNo(), bid);
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
BMDepthMapNodeImp::
addMetaFrameInfo(
    const sp<IPipelineFrame>& pFrame,
    const sp<IMetaStreamInfo>& pMetaStreamInfo,
    const NodeBufferSetting& setting,
    sp<IDepthMapEffectRequest>& pEffectReq
)
{
    DepthNodeBufIOType eIOType = setting.ioType;
    DepthMapBufferID bid = setting.bufferID;
    IMetadata* pMetaBuf;
    MERROR res = retrieveMetadata(pFrame, pMetaStreamInfo->getStreamId(), eIOType, pMetaBuf);
    if(res == OK)
    {
        pEffectReq->pushRequestMetadata(setting, pMetaBuf);
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
BMDepthMapNodeImp::
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
    sp<IDepthMapEffectRequest> pEffectReq;
    MUINT32 iReqIdx = pFrame->getRequestNo();
    pEffectReq = IDepthMapEffectRequest::createInstance(iReqIdx, onEffectRequestFinished, (void*)this);
    // start timer
    pEffectReq->startTimer();
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

        // add meta buffer into effect pRequest
        MERROR ret= addEnqueMetaBufferToRequest(pFrame, metaIOMapSet, pEffectReq);
        if(ret != OK)
        {
            suspendThisFrame(pFrame, pEffectReq);
            return BAD_VALUE;
        }
    }

    // add Frame into frame map
    {
        RWLock::AutoWLock _l(mFrameMapRWLock);
        //record the to-be-enque frame
        mvPipelineFrameMap.add(iReqIdx, pFrame);
    }

    // log enque time
    system_clock::time_point after_enque_tp = system_clock::now();
    std::chrono::duration<double> elap = system_clock::now() - bef_enque_tp;
    MY_LOGD_IF(mLogLevel >= 2, "reqID=%d  enque time=%f ms", iReqIdx, elap.count()*1000);
    //met_tag_start(0, DO_DEPTHMAP);

    // enque the effect Request
    mpDepthEffectHal->updateEffectRequest(pEffectReq.get());

    MY_LOGD_IF(mLogLevel >= 2, "- , reqId=%d", iReqIdx);
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
addEnqueImageBufferToRequest(
    sp<IPipelineFrame>& pFrame,
    IPipelineFrame::ImageInfoIOMapSet const& rIOMapSet,
    sp<IDepthMapEffectRequest>& pEffectReq
)
{
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = rIOMapSet[0];
    // Add input image stream to EffectRequest
    for(size_t index=0; index<imageIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vIn.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vIn.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

        if (ret != OK)
            return BAD_VALUE;

        ret = addImageBufferFrameInfo(pFrame, streamInfo, {bufID, eBUFFER_IOTYPE_INPUT}, pEffectReq);
        if (ret != OK)
            return BAD_VALUE;

        MY_LOGD_IF(mLogLevel >= 2, "input imagBuf stream bid=%d",bufID);
    }

    // Add output image stream to EffectRequest
    for(size_t index=0; index<imageIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = imageIOMap.vOut.keyAt(index);
        sp<IImageStreamInfo> streamInfo = imageIOMap.vOut.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

        if (ret != OK)
            return BAD_VALUE;

        ret = addImageBufferFrameInfo(pFrame, streamInfo, {bufID, eBUFFER_IOTYPE_OUTPUT}, pEffectReq);
        if (ret != OK)
            return BAD_VALUE;

       MY_LOGD_IF(mLogLevel >= 2, "output imagBuf stream bid=%d",bufID);
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
addEnqueMetaBufferToRequest(
    sp<IPipelineFrame>& pFrame,
    IPipelineFrame::MetaInfoIOMapSet const& rIOMapSet,
    sp<IDepthMapEffectRequest>& pEffectReq
)
{
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = rIOMapSet[0];

    IMetadata* pSplitHalMeta_Main2 = NULL;

    // Add input meta stream to EffectRequest
    for(size_t index=0; index<metaIOMap.vIn.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vIn.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vIn.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

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
            ret = addMetaFrameInfo(pFrame, streamInfo, {bufID, eBUFFER_IOTYPE_INPUT}, pEffectReq);
            if (ret != OK)
                goto lbExit;
        }
        MY_LOGD_IF(mLogLevel >= 2, "input MetaBuf stream bid=%d",bufID);
    }
    // Add output meta stream to EffectRequest
    for(size_t index=0; index<metaIOMap.vOut.size(); index++)
    {
        StreamId_T streamId = metaIOMap.vOut.keyAt(index);
        sp<IMetaStreamInfo> streamInfo = metaIOMap.vOut.valueAt(index);
        DepthMapBufferID bufID;
        MERROR ret = mapToBufferID(streamId, bufID);

        if (ret != OK)
            goto lbExit;

        ret = addMetaFrameInfo(pFrame, streamInfo, {bufID, eBUFFER_IOTYPE_OUTPUT}, pEffectReq);
        if (ret != OK)
            goto lbExit;

        MY_LOGD_IF(mLogLevel >= 2, "output MetaBuf stream bid=%d",bufID);
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
BMDepthMapNodeImp::
splitHalMetaFrameInfo(
    sp<IDepthMapEffectRequest>& pEffectReq,
    sp<IPipelineFrame> pFrame,
    IMetadata*& rpHalMeta_Main2
)
{
    // Split HalMeta_Main1 into HalMeta_Main1/Main2
    IMetadata* pMetaBuf;
    if(retrieveMetadata(pFrame, mpInHalMeta->getStreamId(), eBUFFER_IOTYPE_INPUT, pMetaBuf) == OK)
    {
        if( ! tryGetMetadata<IMetadata>(const_cast<IMetadata*>(pMetaBuf), MTK_P1NODE_MAIN2_HAL_META, *rpHalMeta_Main2) ){
            MY_LOGE("cannot get MTK_P1NODE_MAIN2_HAL_META after updating request");
            return BAD_VALUE;
        }
        // add main1 hal meta
        pEffectReq->pushRequestMetadata({BID_META_IN_HAL_MAIN1, eBUFFER_IOTYPE_INPUT}, pMetaBuf);
        // add main2 hal meta
        pEffectReq->pushRequestMetadata({BID_META_IN_HAL_MAIN2, eBUFFER_IOTYPE_INPUT}, rpHalMeta_Main2);

        return OK;
    }
    else
        return BAD_VALUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
BMDepthMapNodeImp::
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
BMDepthMapNodeImp::
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
BMDepthMapNodeImp::
retrieveImageBuffer(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId, DepthNodeBufIOType eType, sp<IImageBuffer>& rpImageBuffer)
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
    sp<IImageBufferHeap> pImageBufferHeap = (eType == eBUFFER_IOTYPE_INPUT) ?
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
BMDepthMapNodeImp::
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
BMDepthMapNodeImp::
releaseImageBuffer(
    sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    DepthNodeBufIOType eType,
    sp<IImageBuffer> const& pImageBuffer,
    MBOOL bIsSuccess)
{

    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();
    sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, getNodeId());
    pImageBuffer->unlockBuf(getNodeName());
    pStreamBuffer->unlock(getNodeName(), pImageBuffer->getImageBufferHeap());

    if(eType==eBUFFER_IOTYPE_OUTPUT)
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
BMDepthMapNodeImp::
retrieveMetadata(sp<IPipelineFrame> const& pFrame, StreamId_T const streamId, DepthNodeBufIOType eType, IMetadata*& rpMetadata)
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

    rpMetadata = (eType==eBUFFER_IOTYPE_INPUT) ?
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
BMDepthMapNodeImp::
releaseMetadata(
    sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    DepthNodeBufIOType eType,
    IMetadata* const pMetadata,
    MBOOL bIsSuccess
)
{
    IStreamBufferSet& rStreamBufferSet  = pFrame->getStreamBufferSet();
    sp<IMetaStreamBuffer>  pMetaStreamBuffer = rStreamBufferSet.getMetaBuffer(streamId, getNodeId());

    pMetaStreamBuffer->unlock(getNodeName(), pMetadata);

    if(eType==eBUFFER_IOTYPE_OUTPUT)
    {
        if(bIsSuccess)
            pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
        else
            pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
    }
    rStreamBufferSet.markUserStatus(streamId, getNodeId(), IUsersManager::UserStatus::USED|IUsersManager::UserStatus::RELEASE);

    return OK;
}

