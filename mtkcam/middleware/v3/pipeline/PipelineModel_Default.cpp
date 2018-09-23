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

#define LOG_TAG "MtkCam/HwPipeline"
//
#include "MyUtils.h"
//
#include "PipelineModel_Default.h"
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/pipeline/pipeline/PipelineContext.h>
//
#include <mtkcam/drv/IHalSensor.h>
//
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/hwnode/P2Node.h>
#include <mtkcam/pipeline/hwnode/FDNode.h>
#include <mtkcam/pipeline/hwnode/JpegNode.h>
#include <mtkcam/pipeline/hwnode/RAW16Node.h>
//#include <mtkcam/pipeline/pipeline/PipelineContext.h>
//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/hw/CamManagerV3.h>
#include <mtkcam/utils/hw/IScenarioControl.h>
#include <mtkcam/utils/hw/HwTransform.h>
#define HIGH_SPEED_BURST_NUM                (4)

using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils;
using NSCam::v3::Utils::MetaStreamInfo;
using NSCam::v3::Utils::ImageStreamInfo;
using namespace NSCam::v3::NSPipelineContext;
using NSCam::v3::Utils::HalMetaStreamBuffer;
using NSCam::v3::Utils::HalImageStreamBuffer;

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

#if 1
#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")
#else
#define FUNC_START
#define FUNC_END
#endif

#define CHECK_ERROR(_err_)                                \
    do {                                                  \
        MERROR const err = (_err_);                       \
        if( err != OK ) {                                 \
            MY_LOGE("err:%d(%s)", err, ::strerror(-err)); \
            return err;                                   \
        }                                                 \
    } while(0)

/******************************************************************************
 *
 ******************************************************************************/
typedef HalMetaStreamBuffer::       Allocator
                                    HalMetaStreamBufferAllocatorT;
/******************************************************************************
 *
 ******************************************************************************/
#define SUPPORT_RECONFIGURE         (0)

static inline
MBOOL
isStream(sp<IStreamInfo> pStreamInfo, StreamId_T streamId )
{
    return pStreamInfo.get() && pStreamInfo->getStreamId() == streamId;
}

// utility functions
static
sp<ImageStreamInfo>     createImageStreamInfo(
                            char const*         streamName,
                            StreamId_T          streamId,
                            MUINT32             streamType,
                            size_t              maxBufNum,
                            size_t              minInitBufNum,
                            MUINT               usageForAllocator,
                            MINT                imgFormat,
                            MSize const&        imgSize,
                            MUINT32             transform = 0
                        );


MBOOL                   getSensorOutputFmt(
                            SensorStaticInfo const& sensorInfo,
                            MUINT32 bitDepth,
                            MBOOL isFull,
                            MINT* pFmt
                        );

/******************************************************************************
 *
 ******************************************************************************/


/******************************************************************************
 *
 ******************************************************************************/
namespace {
class PipelineDefaultImp
    : public PipelineModel_Default
    , public IPipelineBufferSetFrameControl::IAppCallback
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Definitions.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////

    struct MyConfigParams
    {
        PipeConfigParams            configParams;
        //
        // internal setting
        MBOOL                       skipJpeg;
    };
    //
    class MyProcessedParams
    {
    public:
        MINT32 const                mOpenId;
        /*  input params  */
        PipeConfigParams            mConfigParams;
        // internal setting
        MBOOL                       mSkipJpeg;
        //
        /* static info  */
        MBOOL                       mbSensorInfoQueried;
        SensorStaticInfo            mSensorStaticInfo;
        //
        /*  processed params  */
        // Stream infos
        MBOOL                       mbHasRecording;
        MBOOL                       mb4KRecording;
        MBOOL                       mbHasRaw;
        MBOOL                       mbHasOpaque;
        MBOOL                       mbHasJpeg;
        MSize                       mMaxStreamSize;
        //
        // constrained high speed video related
        MUINT32                     mOperation_mode;
        MUINT32                     mDefaultBusrstNum;
        //
        // node related
        MBOOL                       mbUseP1Node;
        MBOOL                       mbUseP2Node;
        MBOOL                       mbUseP2VSSNode;
        MBOOL                       mbUseFDNode;
        MBOOL                       mbUseJpegNode;
        MBOOL                       mbUseRaw16Node;
        //
        // sensor related
        MUINT                       mSensorMode;
        MSize                       mSensorSize;
        MUINT                       mSensorFps;
        //
        // p1 related
        MUINT32                     mPixelMode;
        MINT                        mFullrawFormat;
        MSize                       mFullrawSize;
        size_t                      mFullrawStride;
        MINT                        mResizedrawFormat;
        MSize                       mResizedrawSize;
        size_t                      mResizedrawStride;
        //
    public:
                                    MyProcessedParams(MINT32 const openId);
        MERROR                      update(
                                        PipeConfigParams const& rParams,
                                        MBOOL skipJpeg
                                        )
                                    {
                                        // update config params
                                        mConfigParams = rParams;
                                        mSkipJpeg     = skipJpeg; //TODO: use this?
                                        //
                                        MERROR err;
                                        if( OK != (err = querySensorStatics())  ||
                                            OK != (err = preprocess())          ||
                                            OK != (err = decideSensor())        ||
                                            OK != (err = decideP1())
                                          )
                                            return err;
                                        return OK;
                                    }
        //
    protected:
        MERROR                      querySensorStatics();
        MERROR                      preprocess();
        MERROR                      decideSensor();
        MERROR                      decideP1();
    };

    struct parsedAppRequest
    {
        // original AppRequest
        AppRequest* const           pRequest;
        //
        IMetadata*                  pAppMetaControl;
        // in
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vIImageInfos_Raw;
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vIImageInfos_Opaque;
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vIImageInfos_Yuv;
        // out
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vOImageInfos_Raw;
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vOImageInfos_Opaque;
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vOImageInfos_Yuv;
        KeyedVector< StreamId_T, sp<IImageStreamInfo> >
                                    vOImageInfos_Jpeg;
        //
                                    parsedAppRequest(AppRequest* pRequest)
                                        : pRequest(pRequest)
                                        , pAppMetaControl()
                                    {}
                                    ~parsedAppRequest() {
                                        if( pAppMetaControl )
                                            pRequest->vIMetaBuffers[0]->unlock(
                                                    "parsedAppRequest", pAppMetaControl
                                                    );
                                    }
        //
        MERROR                      parse();
    };

    struct pass1Resource
    {
        enum StreamStatus
        {
            eStatus_Uninit  = 0,
            eStatus_Inited,
            eStatus_Reuse,
            eStatus_NoNeed,
            eNUM_Status
        };
        //
        KeyedVector< StreamId_T, StreamStatus>
                                    vStreamStatus;

        MBOOL                       bStreamReused;

                                    pass1Resource()
                                        : vStreamStatus()
                                        , bStreamReused()
                                    {}

                                    ~pass1Resource()
                                    {}

        MINT32                      setCapacity(size_t size) { return vStreamStatus.setCapacity(size); }

        MVOID                       setReuseFlag(MBOOL flag) { bStreamReused = flag; }

        MBOOL                       getReuseFlag() { return bStreamReused; }

        MVOID                       updateStreamStatus(
                                        StreamId_T const streamId,
                                        StreamStatus eStatus
                                    )
                                    {
                                        ssize_t const index = vStreamStatus.indexOfKey(streamId);
                                        if ( index < 0 )
                                            vStreamStatus.add(streamId, eStatus);
                                        else
                                            vStreamStatus.replaceValueFor(streamId, eStatus);
                                    }

        MBOOL                       checkStreamsReusable()
                                    {
                                        MBOOL bRet = MTRUE;
                                        for ( size_t i=0; i<vStreamStatus.size(); i++) {
                                            if ( vStreamStatus.valueAt(i) < StreamStatus::eStatus_Reuse )
                                                return MFALSE;
                                        }
                                        return MTRUE;
                                    }
    };

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  ////                    Data Members.
    MINT32 const                    mOpenId;
    MINT32                          mLogLevel;
    android::String8 const          mName;
    android::wp<IPipelineModelMgr::IAppCallback>const
                                    mpAppCallback;
    //
    MyProcessedParams               mParams;
    // meta: hal
    android::sp<IMetaStreamInfo>    mpHalMeta_Control;
    android::sp<IMetaStreamInfo>    mpHalMeta_DynamicP1;
    android::sp<IMetaStreamInfo>    mpHalMeta_DynamicP2;
    // meta: app
    android::sp<IMetaStreamInfo>    mpAppMeta_DynamicP1;
    android::sp<IMetaStreamInfo>    mpAppMeta_DynamicP2;
    android::sp<IMetaStreamInfo>    mpAppMeta_DynamicFD;
    android::sp<IMetaStreamInfo>    mpAppMeta_DynamicJpeg;
    android::sp<IMetaStreamInfo>    mpAppMeta_Control;
    // image: hal
    android::sp<IImageStreamInfo>   mpHalImage_P1_Raw;
    android::sp<IImageStreamInfo>   mpHalImage_P1_ResizerRaw;
    android::sp<IImageStreamInfo>   mpHalImage_FD_YUV;
    android::sp<IImageStreamInfo>   mpHalImage_Jpeg_YUV;
    android::sp<IImageStreamInfo>   mpHalImage_Thumbnail_YUV;

    // image: app
    android::sp<IImageStreamInfo>   mpAppImage_Yuv_In;
    android::sp<IImageStreamInfo>   mpAppImage_Opaque_In;
    android::sp<IImageStreamInfo>   mpAppImage_Opaque_Out;
    android::KeyedVector <
            StreamId_T, android::sp<IImageStreamInfo>
                    >               mvAppYuvImage;
    android::sp<IImageStreamInfo>   mpAppImage_Jpeg;
    android::sp<IImageStreamInfo>   mpAppImage_RAW16;

    //// raw/yuv stream mapping
    StreamSet                       mvYuvStreams_Fullraw;
    StreamSet                       mvYuvStreams_Resizedraw;
    //android::Vector<StreamId_T>     mvYuvStreams_Fullraw;
    //android::Vector<StreamId_T>     mvYuvStreams_Resizedraw;

    android::KeyedVector < StreamId_T, MINT64 >
                                    mvStreamDurations;
    MBOOL                           mJpegRotationEnable;
protected:
    android::RWLock                 mRWLock;
    //
    sp<PipelineContext>             mpPipelineContext;

    MBOOL                           mPrevFDEn;

    sp<PipelineContext>             mpOldCtx;

    MBOOL                           mP1ImageStreamReuse;

    pass1Resource                   mPass1Resource;

    CamManagerV3*                   mpCamMgrV3;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineModel Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.
    virtual char const*             getName() const { return mName.string(); }
    virtual MINT32                  getOpenId() const { return mOpenId; }

    virtual MERROR                  submitRequest(AppRequest& request);

    virtual MERROR                  beginFlush();

    virtual MVOID                   endFlush();

    virtual MVOID                   endRequesting() {}

    virtual MVOID                   waitDrained();

    virtual sp<PipelineContext>     getContext();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  PipelineModel_Default Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.
    virtual MERROR                  configure(
                                        PipeConfigParams const& rConfigParams,
                                        android::sp<IPipelineModel> pOldPipeline
                                    );
    virtual void                    onLastStrongRef(const void* id);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Instantiation.
                                    ~PipelineDefaultImp();
                                    PipelineDefaultImp(
                                        MINT32 const openId,
                                        android::String8 const& name,
                                        wp<IPipelineModelMgr::IAppCallback> pAppCallback
                                    );

private:    ////                    Operations.

    MVOID                           evaluatePreviewSize(
                                        PipeConfigParams const& rConfigParams,
                                        MSize &rSize
                                    );

    MERROR                          setupAppStreamsLocked(
                                        PipeConfigParams const& rConfigParams
                                    );

    MERROR                          setupHalStreamsLocked(
                                        PipeConfigParams const& rConfigParams
                                    );

    MERROR                          configContextLocked_Streams(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_Nodes(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_Pipeline(sp<PipelineContext> pContext);
    //
    MERROR                          configContextLocked_P1Node(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_P2Node(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_P2VSSNode(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_FdNode(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_JpegNode(sp<PipelineContext> pContext);
    MERROR                          configContextLocked_Raw16Node(sp<PipelineContext> pContext);
    //
    MERROR                          configRequestRulesLocked();
    //
    MERROR                          compareParamsLocked_P1Node(
                                        P1Node::InitParams const& initParam1,  P1Node::InitParams const& initParam2,
                                        P1Node::ConfigParams const& cfgParam1, P1Node::ConfigParams const& cfgParam2
                                    ) const;
    struct evaluateRequestResult
    {
        DefaultKeyedVector<
            StreamId_T,
            sp<IImageStreamInfo>
                >                               vUpdatedImageInfos;
        //
        NodeSet                                 roots;
        NodeEdgeSet                             edges;
        //
        DefaultKeyedVector<NodeId_T, IOMapSet>  nodeIOMapImage;
        DefaultKeyedVector<NodeId_T, IOMapSet>  nodeIOMapMeta;
        //
        DefaultKeyedVector<StreamId_T, sp<IImageStreamBuffer> >
                                                vAppImageBuffers;
        DefaultKeyedVector<StreamId_T, sp<HalImageStreamBuffer> >
                                                vHalImageBuffers;
        DefaultKeyedVector<StreamId_T, sp<IMetaStreamBuffer> >
                                                vAppMetaBuffers;
        DefaultKeyedVector<StreamId_T, sp<HalMetaStreamBuffer> >
                                                vHalMetaBuffers;
    };

    MERROR                          evaluateRequestLocked(
                                        parsedAppRequest const& request,
                                        evaluateRequestResult& result
                                    );

    MERROR                          refineRequestMetaStreamBuffersLocked(
                                        evaluateRequestResult& result
                                    );

    MERROR                          createStreamInfoLocked_Thumbnail_YUV(
                                        IMetadata const* pMetadata,
                                        android::sp<IImageStreamInfo>& rpStreamInfo
                                    ) const;

    MERROR                          createStreamInfoLocked_Jpeg_YUV(
                                        IMetadata const* pMetadata,
                                        android::sp<IImageStreamInfo>& rpStreamInfo
                                    ) const;

    MSize                           calcThumbnailYuvSize(
                                        MSize const rPicSize,
                                        MSize const rThumbnailsize
                                    ) const;

    MBOOL                           isFdEnable(
                                        IMetadata const* pMetadata
                                    );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineBufferSetFrameControl::IAppCallback Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.
    virtual MVOID                   updateFrame(
                                        MUINT32 const frameNo,
                                        MINTPTR const userId,
                                        Result const& result
                                    );

private:
    // utility functions
    sp<ImageStreamInfo>             createRawImageStreamInfo(
                                        char const*         streamName,
                                        StreamId_T          streamId,
                                        MUINT32             streamType,
                                        size_t              maxBufNum,
                                        size_t              minInitBufNum,
                                        MUINT               usageForAllocator,
                                        MINT                imgFormat,
                                        MSize const&        imgSize,
                                        size_t const        stride
                                    );

    MBOOL                           skipStream(
                                        MBOOL skipJpeg,
                                        IImageStreamInfo* pStreamInfo
                                    ) const;
};
};  //namespace


/******************************************************************************
 *
 ******************************************************************************/
PipelineModel_Default*
PipelineModel_Default::
create(MINT32 const openId, wp<IPipelineModelMgr::IAppCallback> pAppCallback)
{
    String8 const name = String8::format("%s:%d", magicName(), openId);
    PipelineModel_Default* pPipelineModel = new PipelineDefaultImp(openId, name, pAppCallback);
    if  ( ! pPipelineModel ) {
        MY_LOGE("fail to new an instance");
        return NULL;
    }
    //
    return pPipelineModel;
}


/******************************************************************************
 *
 ******************************************************************************/
PipelineDefaultImp::MyProcessedParams::
MyProcessedParams(MINT32 const openId)
    : mOpenId(openId)
    //
    , mConfigParams()
    , mSkipJpeg(MFALSE)
    //
    , mbSensorInfoQueried(MFALSE)
    , mSensorStaticInfo()
    //
    , mbHasRecording()
    , mbHasRaw()
    , mbHasOpaque()
    , mbHasJpeg()
    , mMaxStreamSize()
    //
    , mbUseP1Node()
    , mbUseP2Node()
    , mbUseP2VSSNode()
    , mbUseFDNode()
    , mbUseJpegNode()
    , mbUseRaw16Node()
    //
    , mSensorMode()
    , mSensorSize()
    , mSensorFps()
    //
    , mPixelMode()
    , mFullrawFormat()
    , mFullrawSize()
    , mFullrawStride()
    , mResizedrawFormat()
    , mResizedrawSize()
    , mResizedrawStride()
{
    memset(&mSensorStaticInfo, 0, sizeof(SensorStaticInfo));
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::MyProcessedParams::
querySensorStatics()
{
    if( mbSensorInfoQueried ) return OK;
    //
    IHalSensorList* pSensorList = MAKE_HalSensorList();
    if( pSensorList == NULL ) {
        MY_LOGE("pSensorList == NULL");
        return DEAD_OBJECT;
    }
    //
    MUINT32 sensorDev = pSensorList->querySensorDevIdx(mOpenId);
    pSensorList->querySensorStaticInfo(sensorDev, &mSensorStaticInfo);
    //
    mbSensorInfoQueried = MTRUE;
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::MyProcessedParams::
preprocess()
{
#define hasUsage(flag, usage) ((flag & usage) == usage)
    MBOOL hasVRConsumer = MFALSE;
    MBOOL has4KVR = MFALSE;
    for (size_t i = 0; i < mConfigParams.vImage_Yuv_NonStall.size(); i++) {
        if  ( hasUsage(
                    mConfigParams.vImage_Yuv_NonStall[i]->getUsageForConsumer(),
                    GRALLOC_USAGE_HW_VIDEO_ENCODER
                    ) )
        {
            hasVRConsumer = MTRUE;
            MSize const streamSize = mConfigParams.vImage_Yuv_NonStall[i]->getImgSize();
            has4KVR = ( streamSize.w*streamSize.h > 8000000 )? MTRUE : MFALSE;
            break;
        }
    }
#undef hasUsage
    //
    MSize maxStreamSize;
    {
        struct Log
        {
            static String8
                skippedStream(IImageStreamInfo* pStreamInfo)
                {
                    return String8::format(
                            "skipped stream - format:0x%x type:%x size:%dx%d",
                            pStreamInfo->getImgFormat(), pStreamInfo->getStreamType(),
                            pStreamInfo->getImgSize().w, pStreamInfo->getImgSize().h
                            );
                }

            static String8
                candidateStream(IImageStreamInfo* pStreamInfo)
                {
                    return String8::format(
                            "candidate stream - format:0x%x type:%x size:%dx%d",
                            pStreamInfo->getImgFormat(), pStreamInfo->getStreamType(),
                            pStreamInfo->getImgSize().w, pStreamInfo->getImgSize().h
                            );
                }
        };
        //
        if  ( IImageStreamInfo* pStreamInfo = mConfigParams.pImage_Raw.get() ) {
            if  ( pStreamInfo->getStreamType() == eSTREAMTYPE_IMAGE_IN ) {
                MY_LOGD("%s", Log::skippedStream(pStreamInfo).string());
            }
            else {
                MY_LOGD("%s", Log::candidateStream(pStreamInfo).string());
                maxStreamSize = pStreamInfo->getImgSize();
            }
        }
        //
        if  ( IImageStreamInfo* pStreamInfo = mConfigParams.pImage_Jpeg_Stall.get() ) {
            MY_LOGD("%s", Log::candidateStream(pStreamInfo).string());
            if  ( maxStreamSize.size() <= pStreamInfo->getImgSize().size() ) {
                maxStreamSize = pStreamInfo->getImgSize();
            }
        }
        //
        for (size_t i = 0; i < mConfigParams.vImage_Yuv_NonStall.size(); i++) {
            if  ( IImageStreamInfo* pStreamInfo = mConfigParams.vImage_Yuv_NonStall[i].get()) {
                MY_LOGD("%s", Log::candidateStream(pStreamInfo).string());
                if  ( maxStreamSize.size() <= pStreamInfo->getImgSize().size()) {
                    maxStreamSize = pStreamInfo->getImgSize();
                }
            }
        }
    }
    //
    // update processed params
    mbHasRecording = hasVRConsumer;
    mb4KRecording  = has4KVR;
    mbHasRaw       = mConfigParams.pImage_Raw.get() ? MTRUE : MFALSE;
    mbHasOpaque    = mConfigParams.pImage_Opaque_Out.get() ? MTRUE : MFALSE;
    mbHasJpeg      = mConfigParams.pImage_Jpeg_Stall.get() ? MTRUE : MFALSE;
    mMaxStreamSize = maxStreamSize;
    // constrained high speed video
// #warning "FIX ME: Does the flow of constrained high speed video record w/o operation_mode hint exists?"
    mOperation_mode   = mConfigParams.mOperation_mode;
// #warning "FIX ME: maybe query static metadata to decide burst_num, but actually we have no idea in AOSP config flow"
    mDefaultBusrstNum = (mOperation_mode==0)? 1 : HIGH_SPEED_BURST_NUM;
    MY_LOGD("max stream(%d, %d), raw(%d), jpeg(%d), hasRecording(%d), operation_mode(%d):burst(%d)",
             mMaxStreamSize.w, mMaxStreamSize.h, mbHasRaw, mbHasJpeg,
             mbHasRecording, mOperation_mode, mDefaultBusrstNum );
    //
    mbUseP1Node    = MTRUE;
    mbUseP2Node    = MTRUE;
    mbUseP2VSSNode = hasVRConsumer && mbHasJpeg;
    mbUseFDNode    = MTRUE;
    mbUseJpegNode  = mbHasJpeg;
    mbUseRaw16Node = mbHasRaw && (eImgFmt_RAW16 == mConfigParams.pImage_Raw->getImgFormat());
    //
    return OK;
};


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::MyProcessedParams::
decideSensor()
{
    struct sensormodeHelper
    {
        // use enum to select sensor mode if have preferred sensor mode.
        enum
        {
            eNORMAL_PREVIEW = 0,
            eNORMAL_VIDEO,
            eNORMAL_CAPTURE,
            eSLIM_VIDEO1,
            eSLIM_VIDEO2,
            eNUM_SENSOR_MODE,
        };
        //
                    sensormodeHelper(
                            SensorStaticInfo const& staticInfo,
                            MyProcessedParams* rpParams
                            )
                        : selectIdx(-1)
                        , pParams(rpParams)
                    {
                    #define addMode(idx, _scenarioId_, _key_)                                           \
                        do {                                                                            \
                            sensorSize[idx] = MSize(staticInfo._key_##Width, staticInfo._key_##Height); \
                            sensorFps [idx] = staticInfo._key_##FrameRate/10;                           \
                            sensorMode[idx] = _scenarioId_;                                             \
                            MY_LOGD("candidate mode %d, size(%d, %d)@%d", idx, staticInfo._key_##Width, \
                            staticInfo._key_##Height, staticInfo._key_##FrameRate/10);                  \
                        } while(0)
                        addMode(eNORMAL_PREVIEW, SENSOR_SCENARIO_ID_NORMAL_PREVIEW, preview);
                        addMode(eNORMAL_VIDEO  , SENSOR_SCENARIO_ID_NORMAL_VIDEO  , video);
                        addMode(eNORMAL_CAPTURE, SENSOR_SCENARIO_ID_NORMAL_CAPTURE, capture);
                        addMode(eSLIM_VIDEO1,    SENSOR_SCENARIO_ID_SLIM_VIDEO1, video1);
                        addMode(eSLIM_VIDEO2,    SENSOR_SCENARIO_ID_SLIM_VIDEO2, video2);
                    #undef addMode
                    //
                    #if 1 // preview/video mode, accept by FOV verification
                        NSCamHW::HwTransHelper helper(rpParams->mOpenId);
                        MBOOL acceptPrv = verifyFov(helper, eNORMAL_PREVIEW);
                        // force video mode for 4K recording
                        //MBOOL acceptVdo = verifyFov(helper, eNORMAL_VIDEO) | rpParams->mb4KRecording;
                         MBOOL acceptVdo = verifyFov(helper, eNORMAL_VIDEO);
                    #define altMode(src, dst)                               \
                        do {                                                \
                            sensorSize[src] = sensorSize[dst];              \
                            sensorFps [src] = sensorFps [dst];              \
                            sensorMode[src] = sensorMode[dst];              \
                            MY_LOGD("alt candidate mode %d=>%d", src, dst); \
                        } while(0)
                        if (!acceptPrv) {
                            if (!acceptVdo) {
                                altMode(eNORMAL_VIDEO  , eNORMAL_CAPTURE);
                                altMode(eNORMAL_PREVIEW, eNORMAL_CAPTURE);
                            } else {
                                altMode(eNORMAL_PREVIEW, eNORMAL_VIDEO  );
                            }
                        } else if (!acceptVdo) {
                            altMode(eNORMAL_VIDEO,
                                (sensorFps[eNORMAL_CAPTURE] >= 30 ?
                                eNORMAL_CAPTURE : eNORMAL_PREVIEW));
                        }
                    #undef altMode
                    #endif
                    };
                    ~sensormodeHelper()
                    {
                        if( selectIdx != -1 ) {
                            pParams->mSensorMode = sensorMode[selectIdx];
                            pParams->mSensorSize = sensorSize[selectIdx];
                            pParams->mSensorFps  = sensorFps [selectIdx];
                            //
                            refineFps_MultiOpen(pParams->mSensorMode, pParams->mSensorFps);
                            //
                            MY_LOGD("select mode %d, size(%d, %d)@%d",
                                    pParams->mSensorMode,
                                    pParams->mSensorSize.w, pParams->mSensorSize.h,
                                    pParams->mSensorFps
                                   );
                        } else {
                            MY_LOGW("sensor mode is not selected!");
                            for( int i = 0; i < eNUM_SENSOR_MODE; i++ ) {
                                MY_LOGD("mode %d, size(%d, %d)@%d",
                                        sensorMode[i],
                                        sensorSize[i].w, sensorSize[i].h,
                                        sensorFps[i]
                                       );
                            }
                        }
                    }
        MBOOL       verifyFov(NSCamHW::HwTransHelper helper, MUINT const mode)
                    {
                        #define FOV_DIFF_TOLERANCE (0.002)
                        float dX = 0.0f;
                        float dY = 0.0f;
                        return (helper.calculateFovDifference(
                            sensorMode[mode], &dX, &dY) &&
                            dX < FOV_DIFF_TOLERANCE && dY < FOV_DIFF_TOLERANCE)
                            ? MTRUE : MFALSE;
                    };
        MVOID       refineFps_MultiOpen(MUINT const /*mode*/, MUINT& /*fps*/)
                    {}
                    //
        MSize                       sensorSize[eNUM_SENSOR_MODE];
        MUINT                       sensorFps [eNUM_SENSOR_MODE];
        MUINT                       sensorMode[eNUM_SENSOR_MODE];
        //
        int                         selectIdx;
        MyProcessedParams* const    pParams;
    } aHelper(mSensorStaticInfo, this);
    //
    // 1. Raw stream configured: find sensor mode with raw size.
    if  ( IImageStreamInfo* pStreamInfo = mConfigParams.pImage_Raw.get() ) {
        bool hit = false;
        for (int i = 0; i < sensormodeHelper::eNUM_SENSOR_MODE; i++) {
            if  (pStreamInfo->getImgSize() == aHelper.sensorSize[i]) {
                aHelper.selectIdx = i;
                hit = true;
                break;
            }
        }
        if( !hit ) {
            MY_LOGE("Can't find sesnor size that equals to raw size");
            return UNKNOWN_ERROR;
        }
    } else
    // 2. if has VR consumer: sensor video mode is preferred
    // if ( 1 ) {
    if ( mOperation_mode==CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE &&
         mbHasRecording ) {
        aHelper.selectIdx = sensormodeHelper::eSLIM_VIDEO1;
    }
    else if ( mbHasRecording ) {
        aHelper.selectIdx = sensormodeHelper::eNORMAL_VIDEO;
    }
    else {
        //policy:
        //    find the smallest size that is "larger" than max of stream size
        //    (not the smallest difference)
        bool hit = false;
        for (int i = 0; i < sensormodeHelper::eNUM_SENSOR_MODE; i++) {
            if  ( mMaxStreamSize.w <= aHelper.sensorSize[i].w &&
                  mMaxStreamSize.h <= aHelper.sensorSize[i].h )
            {
                aHelper.selectIdx = i;
                hit = true;
                break;
            }
        }
        if( !hit ) {
            // pick largest one
            MY_LOGW("select capture mode");
            aHelper.selectIdx = sensormodeHelper::eNORMAL_CAPTURE;
        }
    }
    //
    return OK;
};


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::MyProcessedParams::
decideP1()
{
    struct refine
    {
        static
            MVOID       not_larger_than(MSize& size, MSize const& limit) {
                if( size.w > limit.w ) size.w = limit.w;
                if( size.h > limit.h ) size.h = limit.h;
            }
        static
            MVOID       not_smaller_than(MSize& size, MSize const& limit) {
                if( size.w < limit.w ) size.w = limit.w;
                if( size.h < limit.h ) size.h = limit.h;
            }
        static
            MSize       align_2(MSize const& size) {
#define align2(x)  ((x+1) & (~1))
                return MSize(align2(size.w), align2(size.h));
#undef align2
            }
        static
            MSize       scale_roundup(MSize const& size, int mul, int div) {
                return MSize((size.w * mul + div - 1) / div, (size.h * mul + div - 1) / div);
            }
    };
    //
    struct update_pixel_mode
    {
        MERROR  operator() (MyProcessedParams* pParams) {
            MINT32 const openId    = pParams->mOpenId;
            MUINT const sensorMode = pParams->mSensorMode;
            MUINT const sensorFps  = pParams->mSensorFps;
            MUINT32 pixelMode;
            //
            IHalSensor* pSensorHalObj = NULL;
            IHalSensorList* const pHalSensorList = MAKE_HalSensorList();
            //
            if( !pHalSensorList ) { MY_LOGE("pHalSensorList == NULL"); return DEAD_OBJECT; }

            pSensorHalObj = pHalSensorList->createSensor(LOG_TAG, openId);
            if( pSensorHalObj == NULL ) { MY_LOGE("pSensorHalObj is NULL"); return DEAD_OBJECT; }

            pSensorHalObj->sendCommand(
                    pHalSensorList->querySensorDevIdx(openId),
                    SENSOR_CMD_GET_SENSOR_PIXELMODE,
                    (MUINTPTR)(&sensorMode),
                    (MUINTPTR)(&sensorFps),
                    (MUINTPTR)(&pixelMode));

            pSensorHalObj->destroyInstance(LOG_TAG);

            if( pixelMode != 0 && pixelMode != 1 ) {
                MY_LOGE("Un-supported pixel mode %d", pixelMode);
                return BAD_VALUE;
            }
            //
            pParams->mPixelMode = pixelMode;
            return OK;
        }
    };
    //
    if( OK != update_pixel_mode()(this) )
        return UNKNOWN_ERROR;
    //
    NSCamHW::HwInfoHelper infoHelper(mOpenId);
    MSize const sensorSize = mSensorSize;
    //
#define MIN_RRZO_EIS_W          (1280)
#define EIS_RATIO_10X           (12)  // src/dst = 1.2
#define RRZO_SCALE_RATIO_10X    (4)
#if 0
#define MAX_PREVIEW_W           (2560)
#else
#define MAX_PREVIEW_W           (1920)
#endif
#define MAX_RRZO_HW_W           (MAX_PREVIEW_W*EIS_RATIO_10X/10)
#define CHECK_TARGET_SIZE(_msg_, _size_) \
        MY_LOGD_IF(0, "%s: target size(%dx%d)", _msg_, _size_.w, _size_.h);
    // estimate preview yuv max size
    MSize const max_preview_size = refine::align_2(
            MSize(MAX_PREVIEW_W, MAX_PREVIEW_W * sensorSize.h / sensorSize.w));
    //
    MSize maxYuvStreamSize;
    MSize largeYuvStreamSize;
    for (size_t i = 0; i < mConfigParams.vImage_Yuv_NonStall.size(); i++ )
    {
        MSize const streamSize = mConfigParams.vImage_Yuv_NonStall[i]->getImgSize();
        // if stream's size is suitable to use rrzo
        if( streamSize.w <= max_preview_size.w && streamSize.h <= max_preview_size.h )
            refine::not_smaller_than(maxYuvStreamSize, streamSize);
        else
            refine::not_smaller_than(largeYuvStreamSize, streamSize);
    }
    MY_LOGD_IF( !!maxYuvStreamSize, "max yuv stream size(%dx%d)",
            maxYuvStreamSize.w, maxYuvStreamSize.h);
    MY_LOGD_IF( !!largeYuvStreamSize, "large yuv stream size(%dx%d), burst capture",
                largeYuvStreamSize.w, largeYuvStreamSize.h );
    //
    MBOOL bEis = ( mbHasRecording && !mb4KRecording )?
                   CamManagerV3::getInstance()->getEisOwner(mOpenId) :
                   MFALSE;
    MBOOL bDual =  CamManagerV3::getInstance()->isMultiDevice();
    // use resized raw if
    // 1. raw sensor
    // 2. some streams need this
    if( mSensorStaticInfo.sensorType == SENSOR_TYPE_RAW )
    {
        //
        // currently, should always enable resized raw due to some reasons...
        //
        // initial value
        MSize target_rrzo_size = ( !bDual && mb4KRecording )?
                                 largeYuvStreamSize : maxYuvStreamSize;
        CHECK_TARGET_SIZE("max yuv stream", target_rrzo_size);
        // apply limitations
        //  1. lower bounds
        {
            // get eis ownership and apply eis hw limitation
            if ( bEis ) {
                MSize const min_rrzo_eis_size = refine::align_2(
                        MSize(MIN_RRZO_EIS_W, MIN_RRZO_EIS_W * sensorSize.h / sensorSize.w));
                refine::not_smaller_than(target_rrzo_size, min_rrzo_eis_size);
                target_rrzo_size = refine::align_2(
                        refine::scale_roundup(target_rrzo_size, EIS_RATIO_10X, 10)
                        );
               CHECK_TARGET_SIZE("eis lower bound limitation", target_rrzo_size);
            }
            // sensor rrz hw limitation
            MSize const min_rrzo_sensor_scale = refine::align_2(
                    refine::scale_roundup(sensorSize, RRZO_SCALE_RATIO_10X, 10)
                    );
            refine::not_smaller_than(target_rrzo_size, min_rrzo_sensor_scale);
            CHECK_TARGET_SIZE("sensor rrz lower bound limitation", target_rrzo_size);
        }
        //  2. upper bounds
        {
            // MSize const max_rrzo_hw_size = refine::align_2(
            //         MSize(MAX_RRZO_HW_W, MAX_RRZO_HW_W * sensorSize.h / sensorSize.w));
            //
            // refine::not_larger_than(target_rrzo_size, max_rrzo_hw_size);
            if ( !mb4KRecording )
            {
                refine::not_larger_than(target_rrzo_size, max_preview_size);
                CHECK_TARGET_SIZE("preview upper bound limitation", target_rrzo_size);
            }
            refine::not_larger_than(target_rrzo_size, sensorSize);
            CHECK_TARGET_SIZE("sensor size upper bound limitation", target_rrzo_size);
        }
        //
        if ( mPixelMode == 1 && bDual )
        {
            MY_LOGD_IF( 1, "2-pixel mode(%d) sensor(%dx%d)",
                        mPixelMode, sensorSize.w, sensorSize.h);
#define GET_MAX_RRZO_W(_x)      ( (_x)/2 - 32 )
            MSize const max_2pixel_bin_rrzo_size = refine::align_2(
                    MSize( GET_MAX_RRZO_W(sensorSize.w),
                           GET_MAX_RRZO_W(sensorSize.w)*sensorSize.h/sensorSize.w )
                    );
#undef GET_MAX_RRZO_W
            refine::not_larger_than(target_rrzo_size, max_2pixel_bin_rrzo_size);
            CHECK_TARGET_SIZE("2-pixel bin upper bound limitation", target_rrzo_size);
        }
        MY_LOGD_IF(1, "rrzo size(%dx%d)", target_rrzo_size.w, target_rrzo_size.h);
        //
        mResizedrawSize = target_rrzo_size;
        getSensorOutputFmt(mSensorStaticInfo, 10, MFALSE, &mResizedrawFormat);
        // check hw limitation with pixel mode & stride
        if( MTRUE != infoHelper.alignPass1HwLimitation(
                    mPixelMode,
                    mResizedrawFormat,
                    MFALSE,
                    mResizedrawSize,
                    mResizedrawStride
                    )
          )
        {
            MY_LOGE("cannot align to hw limitation: resize");
            return BAD_VALUE;
        }
        MY_LOGD_IF(1, "rrzo size(%dx%d) stride %d",
                mResizedrawSize.w, mResizedrawSize.h, mResizedrawStride);
    }
#undef CHECK_TARGET_SIZE
    //
    // use full raw, if
    // 1. jpeg stream (&& not met BW limit)
    // 2. raw stream
    // 3. opaque stream
    // 4. or stream's size is beyond rrzo's limit
    MBOOL useImgo =
        (mbHasJpeg && ! mbHasRecording) ||
        mbHasRaw ||
        mbHasOpaque ||
        !!largeYuvStreamSize;
    if( useImgo )
    {
        mFullrawSize = sensorSize;
        getSensorOutputFmt(mSensorStaticInfo, 10, MTRUE, &mFullrawFormat);
        // check hw limitation with pixel mode & stride
        if( MTRUE != infoHelper.alignPass1HwLimitation(
                    mPixelMode,
                    mFullrawFormat,
                    MTRUE,
                    mFullrawSize,
                    mFullrawStride
                    )
          )
        {
            MY_LOGE("cannot align to hw limitation: full");
            return BAD_VALUE;
        }
        MY_LOGD_IF(1, "imgo size(%dx%d) stride %d",
                mFullrawSize.w, mFullrawSize.h, mFullrawStride);
    }
    else
    {
        mFullrawSize = MSize(0,0);
    }
    return OK;
};


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::parsedAppRequest::
parse()
{
    struct categorize_img_stream
    {
        typedef KeyedVector< StreamId_T, sp<IImageStreamBuffer> >       IImageSBMapT;
        typedef KeyedVector< StreamId_T, sp<IImageStreamInfo> >         IImageInfoMapT;
        MERROR      operator()(
                IImageSBMapT& map,
                IImageInfoMapT* pMapRaw,
                IImageInfoMapT* pMapOpaque,
                IImageInfoMapT* pMapYuv,
                IImageInfoMapT* pMapJpeg
                )
        {
            for(size_t i = 0; i < map.size(); i++ )
            {
                sp<IImageStreamBuffer> buf = map.valueAt(i);
                if ( IImageStreamInfo const* pStreamInfo = buf->getStreamInfo() )
                {
                    IImageInfoMapT* pTargetMap = NULL;
                    switch( pStreamInfo->getImgFormat() )
                    {
                        //case eImgFmt_BAYER10: //TODO: not supported yet
                        //case eImgFmt_BAYER12:
                        //case eImgFmt_BAYER14:
                        case eImgFmt_RAW16:
                            pTargetMap = pMapRaw;
                            break;
                        case eImgFmt_CAMERA_OPAQUE:
                            pTargetMap = pMapOpaque;
                            break;
                            //
                        case eImgFmt_BLOB:
                            pTargetMap = pMapJpeg;
                            break;
                            //
                        case eImgFmt_YV12:
                        case eImgFmt_NV21:
                        case eImgFmt_YUY2:
                        case eImgFmt_Y8:
                        case eImgFmt_Y16:
                            pTargetMap = pMapYuv;
                            break;
                            //
                        default:
                            MY_LOGE("Unsupported format:0x%x", pStreamInfo->getImgFormat());
                            break;
                    }
                    if( pTargetMap == NULL ) {
                        MY_LOGE("cannot get target map");
                        return UNKNOWN_ERROR;
                    }
                    //
                    pTargetMap->add(
                            pStreamInfo->getStreamId(),
                            const_cast<IImageStreamInfo*>(pStreamInfo)
                            );
                }
            }
            return OK;
        }
    };
    //
    pAppMetaControl = pRequest->vIMetaBuffers[0]->tryReadLock("parsedAppRequest");
    if( ! pAppMetaControl ) {
        MY_LOGE("cannot get control meta");
        return UNKNOWN_ERROR;
    }
    //
    CHECK_ERROR(
            categorize_img_stream() (
                pRequest->vIImageBuffers,
                &vIImageInfos_Raw, &vIImageInfos_Opaque, &vIImageInfos_Yuv, NULL
                )
            );
    CHECK_ERROR(
            categorize_img_stream() (
                pRequest->vOImageBuffers,
                &vOImageInfos_Raw, &vOImageInfos_Opaque, &vOImageInfos_Yuv, &vOImageInfos_Jpeg
                )
            );
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
PipelineDefaultImp::
PipelineDefaultImp(
    MINT32 const openId,
    android::String8 const& name,
    wp<IPipelineModelMgr::IAppCallback> pAppCallback
)
    : mOpenId(openId)
	, mLogLevel(0)
    , mName(name)
    , mpAppCallback(pAppCallback)
    //
    , mParams(openId)
    //
    , mpHalMeta_DynamicP1()
    , mpAppMeta_DynamicP1()
    , mpAppMeta_DynamicP2()
    , mpAppMeta_DynamicFD()
    , mpAppMeta_DynamicJpeg()
    , mpAppMeta_Control()
    //
    , mpHalImage_P1_Raw()
    , mpHalImage_P1_ResizerRaw()
    , mpHalImage_FD_YUV()
    , mpHalImage_Jpeg_YUV()
    , mpHalImage_Thumbnail_YUV()
    //
    , mpAppImage_Yuv_In()
    , mpAppImage_Opaque_In()
    , mpAppImage_Opaque_Out()
    , mvAppYuvImage()
    , mpAppImage_Jpeg()
    , mpAppImage_RAW16()
    //
    , mJpegRotationEnable(MFALSE)
    , mPrevFDEn(MFALSE)
    , mP1ImageStreamReuse()
    //
    , mpCamMgrV3(CamManagerV3::getInstance())
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("jpeg.rotation.enable", value, "1");
    int32_t enable = atoi(value);
    mJpegRotationEnable = (enable & 0x1)? MTRUE : MFALSE;
    MY_LOGD("Jpeg Rotation enable: %d", mJpegRotationEnable);
    //
    mpCamMgrV3->incDevice(getOpenId(), getName());
    //
    mLogLevel = ::property_get_int32("debug.camera.log", 0);
    if ( mLogLevel == 0 ) {
        mLogLevel = ::property_get_int32("debug.camera.log.hwpipeline", 0);
    }
}


/******************************************************************************
 *
 ******************************************************************************/
PipelineDefaultImp::
~PipelineDefaultImp()
{
    FUNC_START;
    // might be fixed because of p1 reuse flow
    if ( mpCamMgrV3->isEisOwner(getOpenId()) )
        mpCamMgrV3->releaseEisOwner(getOpenId());
    mpCamMgrV3->decDevice(getOpenId(), getName());
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
void
PipelineDefaultImp::
onLastStrongRef(const void* /*id*/)
{
    FUNC_START;
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
/**
 * Given:
 *      App input meta streams
 *      App in/out image streams
 *
 * Action:
 *      Determine CONFIG stream set
 *      Determine I/O streams of each node
 *      Prepare Hal stream pools
 *      Configure each node (with their streams)
 *
 */
MERROR
PipelineDefaultImp::
configure(
    PipeConfigParams const& rConfigParams,
    android::sp<IPipelineModel> pOldPipeline
)
{
    CAM_TRACE_NAME("PipelineDefaultImp::configure");
    FUNC_START;
    RWLock::AutoWLock _l(mRWLock);
    //
    mParams.update(rConfigParams, MFALSE);
    //
    MY_LOGD_IF(mpPipelineContext.get(), "strong count %d", mpPipelineContext->getStrongCount());
    mpPipelineContext = PipelineContext::create("DefaultPipeline");
    //
    if ( pOldPipeline.get() )
        mpOldCtx = pOldPipeline->getContext();
    MY_LOGD_IF(  mpOldCtx.get(), "old strong count %d", mpOldCtx->getStrongCount());
    //
    if ( mpOldCtx.get() )
    {
#if 1
        CHECK_ERROR(mpOldCtx->waitUntilNodeDrained(0x01));
#else
        CHECK_ERROR(mpOldCtx->waitUntilDrained());
#endif
    }
    CHECK_ERROR(mpPipelineContext->beginConfigure(mpOldCtx));
    //
    {
        CAM_TRACE_NAME("PipelineDefaultImp::BWC");
        sp<IScenarioControl> pScenarioCtrl = IScenarioControl::create(getOpenId());
        IScenarioControl::ControlParam param;
        param.scenario   =
		    (mParams.mOperation_mode!=0) ? IScenarioControl::Scenario_HighSpeedVideo :
            mParams.mb4KRecording? IScenarioControl::Scenario_VideoRecord :
            mParams.mbHasJpeg?     IScenarioControl::Scenario_StreamingWithJpeg :
            IScenarioControl::Scenario_NormalPreivew;
        param.sensorSize = mParams.mSensorSize;
        param.sensorFps  = mParams.mSensorFps;
        if ( mParams.mb4KRecording && CamManagerV3::getInstance()->isEisOwner(mOpenId) )
            param.featureFlag= (1<<IScenarioControl::FEATURE_ADV_EIS);

        CHECK_ERROR(pScenarioCtrl->exitScenario());
        CHECK_ERROR(pScenarioCtrl->enterScenario(param));
        CHECK_ERROR(mpPipelineContext->setScenarioControl(pScenarioCtrl));
    }
    // create IStreamInfos
    CHECK_ERROR(setupAppStreamsLocked(rConfigParams));
    CHECK_ERROR(setupHalStreamsLocked(rConfigParams));
    //
    // config stream
    CHECK_ERROR(configContextLocked_Streams(mpPipelineContext));
    // config node
    CHECK_ERROR(configContextLocked_Nodes(mpPipelineContext));
    // config pipeline
    CHECK_ERROR(configContextLocked_Pipeline(mpPipelineContext));
    //
    CHECK_ERROR(mpPipelineContext->endConfigure(true));
    mpOldCtx = NULL;
    //
    CHECK_ERROR(configRequestRulesLocked());
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
submitRequest(AppRequest& request)
{
    parsedAppRequest aParsedRequest(&request);
    CHECK_ERROR( aParsedRequest.parse() );
    //
    RWLock::AutoRLock _l(mRWLock);
    //
    RequestBuilder builder;
    //
    evaluateRequestResult evaluateResult;
    //
    CHECK_ERROR( evaluateRequestLocked(aParsedRequest, evaluateResult) );
    //
    CHECK_ERROR( refineRequestMetaStreamBuffersLocked(evaluateResult) );
    //
    builder.setRootNode( evaluateResult.roots );
    builder.setNodeEdges( evaluateResult.edges );
    //
    for( size_t i = 0; i < evaluateResult.vUpdatedImageInfos.size(); i++ )
    {
        builder.replaceStreamInfo(
                evaluateResult.vUpdatedImageInfos.keyAt(i),
                evaluateResult.vUpdatedImageInfos.valueAt(i)
                );
    }
    //
#define try_setIOMap(_nodeId_)                                                        \
    do {                                                                              \
        ssize_t idx_image = evaluateResult.nodeIOMapImage.indexOfKey(_nodeId_);       \
        ssize_t idx_meta  = evaluateResult.nodeIOMapMeta.indexOfKey(_nodeId_);        \
        builder.setIOMap(                                                             \
                _nodeId_,                                                             \
                (0 <= idx_image ) ?                                                   \
                evaluateResult.nodeIOMapImage.valueAt(idx_image) : IOMapSet::empty(), \
                (0 <= idx_meta ) ?                                                    \
                evaluateResult.nodeIOMapMeta.valueAt(idx_meta) : IOMapSet::empty()    \
                );                                                                    \
    } while(0)
    //
    try_setIOMap(eNODEID_P1Node);
    try_setIOMap(eNODEID_P2Node);
    try_setIOMap(eNODEID_P2Node_VSS);
    try_setIOMap(eNODEID_RAW16Out);
    try_setIOMap(eNODEID_FDNode);
    try_setIOMap(eNODEID_JpegNode);
    //
#undef try_setIOMap
    //
#define setStreamBuffers(_sb_type_, _type_, _vStreamBuffer_, _builder_)    \
    do {                                                                   \
        for (size_t i = 0; i < _vStreamBuffer_.size(); i++ )               \
        {                                                                  \
            StreamId_T streamId                = _vStreamBuffer_.keyAt(i); \
            sp<_sb_type_> buffer = _vStreamBuffer_.valueAt(i);             \
            _builder_.set##_type_##StreamBuffer(streamId, buffer);         \
        }                                                                  \
    } while(0)
    //
    setStreamBuffers(IImageStreamBuffer  , Image, evaluateResult.vAppImageBuffers, builder);
    setStreamBuffers(HalImageStreamBuffer, Image, evaluateResult.vHalImageBuffers, builder);
    setStreamBuffers(IMetaStreamBuffer   , Meta , evaluateResult.vAppMetaBuffers , builder);
    setStreamBuffers(HalMetaStreamBuffer , Meta , evaluateResult.vHalMetaBuffers , builder);
#undef setStreamBuffers
    //
    //
    sp<IPipelineFrame> pFrame = builder
        .updateFrameCallback(this)
        .build(request.requestNo, mpPipelineContext);
    if( ! pFrame.get() )
        return UNKNOWN_ERROR;
    //
    return mpPipelineContext->queue(pFrame);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
beginFlush()
{
    FUNC_START;
    //
    if( mpPipelineContext.get() )
        mpPipelineContext->flush();
    else
        MY_LOGW("no context");
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineDefaultImp::
endFlush()
{
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineDefaultImp::
waitDrained()
{
    FUNC_START;
    if( mpPipelineContext.get() )
        mpPipelineContext->waitUntilDrained();
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
sp<PipelineContext>
PipelineDefaultImp::
getContext()
{
    FUNC_START;
    if( mpPipelineContext.get() )
        return mpPipelineContext;
    FUNC_END;
    return NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
PipelineDefaultImp::
skipStream(
    MBOOL skipJpeg,
    IImageStreamInfo* pStreamInfo
) const
{
    if  (
            skipJpeg
        &&  pStreamInfo->getImgFormat() == HAL_PIXEL_FORMAT_BLOB
        &&  pStreamInfo->getImgSize().size() >= 1920*1080
        )
    {
 //&& limited mode
        return MTRUE;
    }

    return MFALSE;
}


/******************************************************************************
 *
 ******************************************************************************/
sp<ImageStreamInfo>
createImageStreamInfo(
    char const*         streamName,
    StreamId_T          streamId,
    MUINT32             streamType,
    size_t              maxBufNum,
    size_t              minInitBufNum,
    MUINT               usageForAllocator,
    MINT                imgFormat,
    MSize const&        imgSize,
    MUINT32             transform
)
{
    IImageStreamInfo::BufPlanes_t bufPlanes;
#define addBufPlane(planes, height, stride)                                      \
        do{                                                                      \
            size_t _height = (size_t)(height);                                   \
            size_t _stride = (size_t)(stride);                                   \
            IImageStreamInfo::BufPlane bufPlane= { _height * _stride, _stride }; \
            planes.push_back(bufPlane);                                          \
        }while(0)
    switch( imgFormat ) {
        case eImgFmt_YV12:
            addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
            addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w >> 1);
            addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w >> 1);
            break;
        case eImgFmt_NV21:
            addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
            addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w);
            break;
        case eImgFmt_YUY2:
            addBufPlane(bufPlanes , imgSize.h      , imgSize.w << 1);
            break;
        default:
            MY_LOGE("format not support yet %p", imgFormat);
            break;
    }
#undef  addBufPlane

    sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
                streamName,
                streamId,
                streamType,
                maxBufNum, minInitBufNum,
                usageForAllocator, imgFormat, imgSize, bufPlanes, transform
                );

    if( pStreamInfo == NULL ) {
        MY_LOGE("create ImageStream failed, %s, %#" PRIxPTR,
                streamName, streamId);
    }

    return pStreamInfo;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineDefaultImp::
evaluatePreviewSize(
    PipeConfigParams const& rConfigParams,
    MSize &rSize
)
{
    sp<IImageStreamInfo> pStreamInfo;
    int consumer_usage = 0;
    int allocate_usage = 0;
    int maxheight = rSize.h;
    int prevwidth = 0;
    int prevheight = 0;
    for (size_t i = 0; i < rConfigParams.vImage_Yuv_NonStall.size(); i++) {
        if  ( (pStreamInfo = rConfigParams.vImage_Yuv_NonStall[i]) != 0 ) {
            consumer_usage = pStreamInfo->getUsageForConsumer();
            allocate_usage = pStreamInfo->getUsageForAllocator();
            MY_LOGD("consumer : %X, allocate : %X", consumer_usage, allocate_usage);
            if(consumer_usage & GRALLOC_USAGE_HW_TEXTURE) {
                prevwidth = pStreamInfo->getImgSize().w;
                prevheight = pStreamInfo->getImgSize().h;
                break;
            }
            if(consumer_usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
                continue;
            }
            prevwidth = pStreamInfo->getImgSize().w;
            prevheight = pStreamInfo->getImgSize().h;
        }
    }
    if(prevwidth == 0 || prevheight == 0)
        return ;
    rSize.h = prevheight * rSize.w / prevwidth;
    if(maxheight < rSize.h) {
        MY_LOGW("Warning!!,  scaled preview height(%d) is larger than max height(%d)", rSize.h, maxheight);
        rSize.h = maxheight;
    }
    MY_LOGD("evaluate preview size : %dx%d", prevwidth, prevheight);
    MY_LOGD("FD buffer size : %dx%d", rSize.w, rSize.h);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
setupAppStreamsLocked(
    PipeConfigParams const& rConfigParams
)
{
    mvStreamDurations.clear();

    MUINT32 iBurstNum = mParams.mDefaultBusrstNum;
    //App:Meta:Control
    {
        sp<IMetaStreamInfo> pStreamInfo;
        if  ( (pStreamInfo = rConfigParams.pMeta_Control) != 0 )
        {
            mpAppMeta_Control = pStreamInfo;
            pStreamInfo->setMaxBufNum(10*iBurstNum);
        }
    }

    //App:dynamic
    if( mParams.mbUseP1Node )
    {
        //App:Meta:01
        //   pass1 result meta
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "App:Meta:DynamicP1",
                    eSTREAMID_META_APP_DYNAMIC_01,
                    eSTREAMTYPE_META_OUT,
                    10*iBurstNum, 1
                    );
        mpAppMeta_DynamicP1 = pStreamInfo;
    }

    if( mParams.mbUseP2Node )
    {
        //App:Meta:P2
        //   pass2 result meta
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "App:Meta:DynamicP2",
                    eSTREAMID_META_APP_DYNAMIC_02,
                    eSTREAMTYPE_META_OUT,
                    10*iBurstNum, 1
                    );
        mpAppMeta_DynamicP2 = pStreamInfo;
    }


    if( mParams.mbUseFDNode )
    {
        //App:Meta:FD
        //   FD result meta
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "App:Meta:FD",
                    eSTREAMID_META_APP_DYNAMIC_FD,
                    eSTREAMTYPE_META_OUT,
                    10, 1
                    );
        mpAppMeta_DynamicFD = pStreamInfo;
    }

    if( mParams.mbUseJpegNode )
    {
        //App:Meta:Jpeg
        //   Jpeg result meta
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "App:Meta:Jpeg",
                    eSTREAMID_META_APP_DYNAMIC_JPEG,
                    eSTREAMTYPE_META_OUT,
                    10, 1
                    );
        mpAppMeta_DynamicJpeg = pStreamInfo;
    }


    //App:Image
    {
        sp<IImageStreamInfo> pStreamInfo;
        //
        //App:Image:Raw
        if  ( (pStreamInfo = rConfigParams.pImage_Raw) != 0
                 &&  eImgFmt_RAW16 == pStreamInfo->getImgFormat()
        ) {
            // RAW16 = rConfigParams.pImage_Raw set this stream
            mpAppImage_RAW16 = pStreamInfo;
            pStreamInfo->setMaxBufNum(1);
            //
            mvStreamDurations.add(
                    pStreamInfo->getStreamId(),
                    rConfigParams.mImage_Raw_Duration.minDuration
                    );
        }
        //
        //App:Image:Jpeg:Stall
        if  ( (pStreamInfo = rConfigParams.pImage_Jpeg_Stall) != 0 ) {
            mpAppImage_Jpeg = pStreamInfo;
            pStreamInfo->setMaxBufNum(1);
            //
            mvStreamDurations.add(
                    pStreamInfo->getStreamId(),
                    rConfigParams.mImage_Jpeg_Duration.minDuration
                    );
        }
        //App:Image:Yuv:In
        if  ( (pStreamInfo = rConfigParams.pImage_Yuv_In) != 0 ) {
            mpAppImage_Yuv_In = pStreamInfo;
            pStreamInfo->setMaxBufNum(2);
            //
            mvStreamDurations.add(
                    pStreamInfo->getStreamId(),
                    rConfigParams.mImage_Yuv_In_Duration.minDuration
                    );
        }
        //App:Image:Opaque:In
        if  ( (pStreamInfo = rConfigParams.pImage_Opaque_In) != 0 ) {
            mpAppImage_Opaque_In = pStreamInfo;
            pStreamInfo->setMaxBufNum(2);
            //
            mvStreamDurations.add(
                    pStreamInfo->getStreamId(),
                    rConfigParams.mImage_Opaque_In_Duration.minDuration
                    );
        }
        //App:Image:Opaque:Out
        if  ( (pStreamInfo = rConfigParams.pImage_Opaque_Out) != 0 ) {
            mpAppImage_Opaque_Out = pStreamInfo;
            pStreamInfo->setMaxBufNum(6);
            //
            mvStreamDurations.add(
                    pStreamInfo->getStreamId(),
                    rConfigParams.mImage_Opaque_Out_Duration.minDuration
                    );
        }
        //
        //App:Image:Yuv:NotStall
        for (size_t i = 0; i < rConfigParams.vImage_Yuv_NonStall.size(); i++) {
            if  ( (pStreamInfo = rConfigParams.vImage_Yuv_NonStall[i]) != 0 ) {
                mvAppYuvImage.add(pStreamInfo->getStreamId(), pStreamInfo);
                if ( mParams.mOperation_mode )
                    pStreamInfo->setMaxBufNum(8*(iBurstNum+2));
                else
                    pStreamInfo->setMaxBufNum(8);
                //
                if( i >= rConfigParams.vImage_Yuv_Duration.size() ) {
                    MY_LOGE("not enough yuv duration for streams");
                    continue;
                }
                mvStreamDurations.add(
                        pStreamInfo->getStreamId(),
                        rConfigParams.vImage_Yuv_Duration[i].minDuration
                        );
            }
        }
        //
        // dump durations
        String8 durations = String8("durations:");
        for( size_t i = 0; i < mvStreamDurations.size(); i++) {
            durations += String8::format("(stream %#" PRIxPTR ": %lld) ",
                    mvStreamDurations.keyAt(i), (long long int)mvStreamDurations.valueAt(i));
        }
        MY_LOGD("%s", durations.string());
    }

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
setupHalStreamsLocked(
    PipeConfigParams const& rConfigParams
)
{
    MUINT32 iBurstNum = mParams.mDefaultBusrstNum;
    //Hal:Meta
    //
    if( 1 )
    {
        //Hal:Meta:Control
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "Hal:Meta:Control",
                    eSTREAMID_META_PIPE_CONTROL,
                    eSTREAMTYPE_META_IN,
                    10*iBurstNum, 1
                    );
        mpHalMeta_Control = pStreamInfo;
    }
    //
    if( mParams.mbUseP1Node )
    {
        //Hal:Meta:01
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "Hal:Meta:P1:Dynamic",
                    eSTREAMID_META_PIPE_DYNAMIC_01,
                    eSTREAMTYPE_META_INOUT,
                    10*iBurstNum, 1
                    );
        mpHalMeta_DynamicP1 = pStreamInfo;
    }
    //
    if( mParams.mbUseP2Node )
    {
        //Hal:Meta:01
        sp<IMetaStreamInfo> pStreamInfo =
            new MetaStreamInfo(
                    "Hal:Meta:P2:Dynamic",
                    eSTREAMID_META_PIPE_DYNAMIC_02,
                    eSTREAMTYPE_META_INOUT,
                    10*iBurstNum, 1
                    );
        mpHalMeta_DynamicP2 = pStreamInfo;
    }

    // p1node image port: imgo/rrzo
    mPass1Resource.setCapacity(2);
    //Hal:Image
    if ( mParams.mbUseP1Node &&
            !! mParams.mFullrawSize.size() )
    {
        sp<IImageStreamInfo> pStreamInfo;
        // p1: fullsize
        MSize const& size = mParams.mFullrawSize;
        MINT const format = mParams.mFullrawFormat;
        size_t const stride = mParams.mFullrawStride;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE;
        //
        MBOOL bDone = MFALSE;
        if ( mpOldCtx.get() &&
             OK == mpOldCtx->queryStream(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00, pStreamInfo) &&
             pStreamInfo.get() )
        {
            if ( pStreamInfo->getImgSize().w == size.w &&
                 pStreamInfo->getImgSize().h == size.h &&
                 pStreamInfo->getImgFormat() == format &&
                 pStreamInfo->getBufPlanes().itemAt(0).rowStrideInBytes == stride &&
                 pStreamInfo->getUsageForAllocator() == usage )
            {
                MY_LOGD_IF( 1, "stream could be reused:%#" PRIxPTR , eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00);
                //mvReuseStream.add(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00);
                mPass1Resource.updateStreamStatus( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00,
                                                   pass1Resource::StreamStatus::eStatus_Reuse);
                bDone = MTRUE;
            }
        }
        //
        if ( !bDone )
        {
            pStreamInfo = createRawImageStreamInfo(
                    "Hal:Image:P1:Fullraw",
                    eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00,
                    eSTREAMTYPE_IMAGE_INOUT,
                    (6+2)*iBurstNum, 0,
                    usage, format, size, stride
            );
            //
            if( pStreamInfo == NULL ) {
                return BAD_VALUE;
            }
            mPass1Resource.updateStreamStatus( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00,
                                               pass1Resource::StreamStatus::eStatus_Inited);
        }
        //
        MY_LOGD_IF( 1, "setup streamid(%#" PRIxPTR "):%p",
                    eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00, pStreamInfo.get() );
        mpHalImage_P1_Raw = pStreamInfo;
    } else if ( mParams.mbUseP1Node &&
                ! mParams.mFullrawSize.size() )
    {
        sp<IImageStreamInfo> pStreamInfo;
        if ( mpOldCtx.get() &&
             OK == mpOldCtx->queryStream(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00, pStreamInfo) &&
             ! pStreamInfo.get() )
        {
            MY_LOGD_IF( 1, "case of previous and current pipeline have no streamid:%#" PRIxPTR ,
                        eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00);
            //mvReuseStream.add(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00);
            mPass1Resource.updateStreamStatus( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00,
                                               pass1Resource::StreamStatus::eStatus_NoNeed);
            mpHalImage_P1_Raw = pStreamInfo;
        }
    }

    //if (
    //        mConfigProfile.mbConfigP1 &&
    //        !! mConfigProfile.mResizedSize
    if ( mParams.mbUseP1Node &&
            !! mParams.mResizedrawSize.size() )
    {
        sp<IImageStreamInfo> pStreamInfo;
        // p1: resize
        MSize const& size = mParams.mResizedrawSize;
        MINT const format = mParams.mResizedrawFormat;
        size_t const stride = mParams.mResizedrawStride;
        MUINT const usage = 0;
        //
        MBOOL bDone = MFALSE;
        if ( mpOldCtx.get() &&
             OK == mpOldCtx->queryStream(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00, pStreamInfo) &&
             pStreamInfo.get() )
        {
            if ( pStreamInfo->getImgSize().w == size.w &&
                 pStreamInfo->getImgSize().h == size.h &&
                 pStreamInfo->getImgFormat() == format &&
                 pStreamInfo->getBufPlanes().itemAt(0).rowStrideInBytes == stride &&
                 pStreamInfo->getUsageForAllocator() == usage )
            {
                MY_LOGD_IF( 1, "stream could be reused:%#" PRIxPTR , eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00);
                //mvReuseStream.add(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00);
                mPass1Resource.updateStreamStatus( eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00,
                                                   pass1Resource::StreamStatus::eStatus_Reuse);
                bDone = MTRUE;
            }
        }
        //
        if ( !bDone )
        {
            pStreamInfo = createRawImageStreamInfo(
                    "Hal:Image:P1:Resizeraw",
                    eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00,
                    eSTREAMTYPE_IMAGE_INOUT,
                    (6+2)*iBurstNum, 0,
                    usage, format, size, stride
            );
            //
            if( pStreamInfo == NULL ) {
                return BAD_VALUE;
            }
            mPass1Resource.updateStreamStatus( eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00,
                                               pass1Resource::StreamStatus::eStatus_Inited);
            }
        //
        MY_LOGD_IF( 1, "setup streamid(%#" PRIxPTR "):%p",
                    eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00, pStreamInfo.get() );
        mpHalImage_P1_ResizerRaw = pStreamInfo;
    } else if ( mParams.mbUseP1Node &&
                ! mParams.mResizedrawSize.size() )
    {
        sp<IImageStreamInfo> pStreamInfo;
        if ( mpOldCtx.get() &&
             OK == mpOldCtx->queryStream(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00, pStreamInfo) &&
             ! pStreamInfo.get() )
        {
            MY_LOGD_IF( 1, "case of previous and current pipeline have no streamid:%#" PRIxPTR ,
                        eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00);
            //mvReuseStream.add(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00);
            mPass1Resource.updateStreamStatus( eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00,
                                               pass1Resource::StreamStatus::eStatus_NoNeed);
            mpHalImage_P1_ResizerRaw = pStreamInfo;
        }
    }

    //Hal:Image:FD
    if ( mParams.mbUseFDNode )
    {
        //MSize const size(640, 480); //FIXME: hard-code here?
        MSize size(640, 480);
        // evaluate preview size
        evaluatePreviewSize(rConfigParams, size);

        mPrevFDEn = MFALSE;

        MY_LOGD("evaluate FD buffer size : %dx%d", size.w, size.h);

        MINT const format = eImgFmt_YUY2;//eImgFmt_YV12;
        MUINT const usage = 0;

        sp<ImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                "Hal:Image:FD",
                eSTREAMID_IMAGE_FD,
                eSTREAMTYPE_IMAGE_INOUT,
                5, 1,
                usage, format, size
            );
        if( pStreamInfo == NULL ) {
            return BAD_VALUE;
        }
        //
        mpHalImage_FD_YUV = pStreamInfo;
    }

    //Hal:Image:YUY2 for jpeg & thumbnail
    if ( mParams.mbUseJpegNode )
    {
        //Hal:Image:YUY2 for jpeg
        {
            MSize const& size = rConfigParams.pImage_Jpeg_Stall->getImgSize();
            MINT const format = eImgFmt_YUY2;
            MUINT const usage = 0;
            sp<ImageStreamInfo>
                pStreamInfo = createImageStreamInfo(
                    "Hal:Image:YuvJpeg",
                    eSTREAMID_IMAGE_PIPE_YUV_JPEG_00,
                    eSTREAMTYPE_IMAGE_INOUT,
                    1, 0,
                    usage, format, size
                );
            if( pStreamInfo == NULL ) {
                return BAD_VALUE;
            }
            //
            mpHalImage_Jpeg_YUV = pStreamInfo;
        }
        //
        //Hal:Image:YUY2 for thumbnail
        {
            MSize const size(-1L, -1L); //unknown now
            MINT const format = eImgFmt_YUY2;
            MUINT const usage = 0;
            sp<ImageStreamInfo>
                pStreamInfo = createImageStreamInfo(
                    "Hal:Image:YuvThumbnail",
                    eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL_00,
                    eSTREAMTYPE_IMAGE_INOUT,
                    1, 0,
                    usage, format, size
                );
            if( pStreamInfo == NULL ) {
                return BAD_VALUE;
            }
            //
            mpHalImage_Thumbnail_YUV = pStreamInfo;
            MY_LOGD("streamId:%#" PRIxPTR " %s %p", pStreamInfo->getStreamId(), pStreamInfo->getStreamName(), pStreamInfo.get());
        }
    }

    return OK;

}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_Streams(sp<PipelineContext> pContext)
{
    CAM_TRACE_CALL();
#define BuildStream(_type_, _IStreamInfo_)                                     \
    do {                                                                       \
        if( _IStreamInfo_.get() ) {                                            \
            MERROR err;                                                        \
            if ( OK != (err = StreamBuilder(_type_, _IStreamInfo_)             \
                    .build(pContext)) )                                        \
            {                                                                  \
                MY_LOGE("StreamBuilder fail stream %# " PRIxPTR " of type %d", \
                    #_type_, _IStreamInfo_->getStreamId());                    \
                return err;                                                    \
            }                                                                  \
        }                                                                      \
    } while(0)
    BuildStream(eStreamType_META_HAL, mpHalMeta_Control);
    BuildStream(eStreamType_META_HAL, mpHalMeta_DynamicP1);
    BuildStream(eStreamType_META_HAL, mpHalMeta_DynamicP2);
    //
    BuildStream(eStreamType_META_APP, mpAppMeta_DynamicP1);
    BuildStream(eStreamType_META_APP, mpAppMeta_DynamicP2);
    BuildStream(eStreamType_META_APP, mpAppMeta_DynamicFD);
    BuildStream(eStreamType_META_APP, mpAppMeta_DynamicJpeg);
    BuildStream(eStreamType_META_APP, mpAppMeta_Control);
    //
    BuildStream(eStreamType_IMG_HAL_POOL   , mpHalImage_FD_YUV);
    //
#if 1
    if ( mPass1Resource.checkStreamsReusable() )
    {
        if ( mpHalImage_P1_Raw.get() )
            CHECK_ERROR( pContext->reuseStream(mpHalImage_P1_Raw) );
        if ( mpHalImage_P1_ResizerRaw.get() )
            CHECK_ERROR( pContext->reuseStream(mpHalImage_P1_ResizerRaw) );
        //
        MY_LOGD_IF( 1, "Reuse:  p1 full raw(%p); resized raw(%p)",
                    mpHalImage_P1_Raw.get(), mpHalImage_P1_ResizerRaw.get());
        mPass1Resource.setReuseFlag(MTRUE);
    }
    else
    {
        BuildStream(eStreamType_IMG_HAL_POOL   , mpHalImage_P1_Raw);
        BuildStream(eStreamType_IMG_HAL_POOL   , mpHalImage_P1_ResizerRaw);
        MY_LOGD_IF( 1, "New: p1 full raw(%p); resized raw(%p)",
                    mpHalImage_P1_Raw.get(), mpHalImage_P1_ResizerRaw.get());
        mPass1Resource.setReuseFlag(MFALSE);
    }
#else
        BuildStream(eStreamType_IMG_HAL_POOL   , mpHalImage_P1_Raw);
        BuildStream(eStreamType_IMG_HAL_POOL   , mpHalImage_P1_ResizerRaw);
#endif
    if ( mpAppImage_Yuv_In.get() )
        BuildStream(eStreamType_IMG_APP, mpAppImage_Yuv_In);
    if ( mpAppImage_Opaque_In.get() )
        BuildStream(eStreamType_IMG_APP, mpAppImage_Opaque_In);
    if ( mpAppImage_Opaque_Out.get() )
        BuildStream(eStreamType_IMG_APP, mpAppImage_Opaque_Out);
    //
    if ( !mJpegRotationEnable )
        BuildStream(eStreamType_IMG_HAL_POOL   , mpHalImage_Jpeg_YUV);
    else
        BuildStream(eStreamType_IMG_HAL_RUNTIME   , mpHalImage_Jpeg_YUV);
    BuildStream(eStreamType_IMG_HAL_RUNTIME, mpHalImage_Thumbnail_YUV);
    //
    for (size_t i = 0; i < mvAppYuvImage.size(); i++ )
    {
        BuildStream(eStreamType_IMG_APP, mvAppYuvImage[i]);
    }
    BuildStream(eStreamType_IMG_APP, mpAppImage_Jpeg);
    BuildStream(eStreamType_IMG_APP, mpAppImage_RAW16);
#undef BuildStream
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_Nodes(sp<PipelineContext> pContext)
{
    if( mParams.mbUseP1Node )
        CHECK_ERROR( configContextLocked_P1Node(pContext) );
    if( mParams.mbUseP2Node )
        CHECK_ERROR( configContextLocked_P2Node(pContext) );
    if( mParams.mbUseP2VSSNode )
        CHECK_ERROR( configContextLocked_P2VSSNode(pContext) );
    if( mParams.mbUseFDNode )
        CHECK_ERROR( configContextLocked_FdNode(pContext) );
    if( mParams.mbUseJpegNode )
        CHECK_ERROR( configContextLocked_JpegNode(pContext) );
    if( mParams.mbUseRaw16Node )
        CHECK_ERROR( configContextLocked_Raw16Node(pContext) );
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_Pipeline(sp<PipelineContext> pContext)
{
    NodeSet roots;
    {
        roots.add(eNODEID_P1Node);
    }
    NodeEdgeSet edges;
    {
        if( mParams.mbUseP1Node && mParams.mbUseP2Node )
            edges.addEdge(eNODEID_P1Node, eNODEID_P2Node);
        if( mParams.mbUseP1Node && mParams.mbUseP2VSSNode )
            edges.addEdge(eNODEID_P1Node, eNODEID_P2Node_VSS);
        if( mParams.mbUseP2Node && mParams.mbUseFDNode )
            edges.addEdge(eNODEID_P2Node, eNODEID_FDNode);
        if( ! mParams.mbUseP2VSSNode ) {
            if( mParams.mbUseP2Node && mParams.mbUseJpegNode )
                edges.addEdge(eNODEID_P2Node, eNODEID_JpegNode);
        }
        else {
            if( mParams.mbUseP2VSSNode && mParams.mbUseJpegNode )
                edges.addEdge(eNODEID_P2Node_VSS, eNODEID_JpegNode);
        }
        if( mParams.mbUseP1Node && mParams.mbUseRaw16Node )
            edges.addEdge(eNODEID_P1Node, eNODEID_RAW16Out);
    }
    //
    CHECK_ERROR(
            PipelineBuilder()
            .setRootNode(roots)
            .setNodeEdges(edges)
            .build(pContext)
            );
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
#define add_stream_to_set(_set_, _IStreamInfo_)                                \
    do {                                                                       \
        if( _IStreamInfo_.get() ) { _set_.add(_IStreamInfo_->getStreamId()); } \
    } while(0)
//
#define setImageUsage( _IStreamInfo_, _usg_ )                                   \
    do {                                                                        \
        if( _IStreamInfo_.get() ) {                                             \
            builder.setImageStreamUsage( _IStreamInfo_->getStreamId(), _usg_ ); \
        }                                                                       \
    } while(0)
/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_P1Node(sp<PipelineContext> pContext)
{
    typedef P1Node                  NodeT;
    typedef NodeActor< NodeT >      NodeActorT;
    //
    NodeId_T const nodeId = eNODEID_P1Node;
    //
    NodeT::InitParams initParam;
    {
        initParam.openId = getOpenId();
        initParam.nodeId = nodeId;
        initParam.nodeName = "P1Node";
    }
    NodeT::ConfigParams cfgParam;
    {
        NodeT::SensorParams sensorParam(
        /*mode     : */mParams.mSensorMode,
        /*size     : */mParams.mSensorSize,
        /*fps      : */mParams.mSensorFps,
        /*pixelMode: */mParams.mPixelMode
        );
        //
        cfgParam.pInAppMeta        = mpAppMeta_Control;
        cfgParam.pInHalMeta        = mpHalMeta_Control;
        cfgParam.pOutAppMeta       = mpAppMeta_DynamicP1;
        cfgParam.pOutHalMeta       = mpHalMeta_DynamicP1;
        cfgParam.pOutImage_resizer = mpHalImage_P1_ResizerRaw;
        if( mpHalImage_P1_Raw.get() )
            cfgParam.pvOutImage_full.push_back(mpHalImage_P1_Raw);
        cfgParam.sensorParams        = sensorParam;
        cfgParam.pStreamPool_resizer = NULL;
        cfgParam.pStreamPool_full    = NULL;
        // cfgParam.pStreamPool_resizer = mpHalImage_P1_ResizerRaw.get() ?
        //    pContext->queryImageStreamPool(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00) : NULL;
        // cfgParam.pStreamPool_full = mpHalImage_P1_Raw.get() ?
        //    pContext->queryImageStreamPool(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00) : NULL;
        if( mpAppImage_Yuv_In.get() )
            cfgParam.pInImage_yuv = mpAppImage_Yuv_In;
        if( mpAppImage_Opaque_In.get() )
            cfgParam.pInImage_opaque = mpAppImage_Opaque_In;
        if( mpAppImage_Opaque_Out.get() )
            cfgParam.pOutImage_opaque = mpAppImage_Opaque_Out;
        cfgParam.sensorParams      = sensorParam;
        cfgParam.pStreamPool_resizer = mpHalImage_P1_ResizerRaw.get() ?
            pContext->queryImageStreamPool(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00) : NULL;
        cfgParam.pStreamPool_full = mpHalImage_P1_Raw.get() ?
            pContext->queryImageStreamPool(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00) : NULL;
        cfgParam.enableEIS = mpCamMgrV3->isEisOwner(getOpenId());
        //        
		if ( mParams.mb4KRecording )
            cfgParam.receiveMode = NodeT::REV_MODE::REV_MODE_CONSERVATIVE;
        //
        if ( mParams.mOperation_mode == CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE )
            cfgParam.burstNum      = mParams.mDefaultBusrstNum;
    }
    //
    sp<NodeActorT> pNode;
    sp<NodeActorT> pOldNode;
    if ( MTRUE == mPass1Resource.getReuseFlag() && mpOldCtx.get() &&
         OK == mpOldCtx->queryNodeActor(nodeId, pOldNode) )
    {
        NodeT::InitParams oldInitParam;
        pOldNode->getInitParam(oldInitParam);
        NodeT::ConfigParams oldCfgParam;
        pOldNode->getConfigParam(oldCfgParam);
        //
        if ( compareParamsLocked_P1Node(initParam, oldInitParam, cfgParam, oldCfgParam) )
        {
            MERROR err = pContext->reuseNode(nodeId);
            MY_LOGD_IF( 1, "check p1 state Old[%d] New[%d]",
                        mpOldCtx->queryINodeActor(nodeId)->getStatus(),
                        pContext->queryINodeActor(nodeId)->getStatus() );
            return err;
        }
    }
    //
    pNode = new NodeActorT( NodeT::createInstance() );
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    StreamSet inStreamSet;
    StreamSet outStreamSet;
    //
    add_stream_to_set(inStreamSet, mpAppMeta_Control);
    add_stream_to_set(inStreamSet, mpHalMeta_Control);
    add_stream_to_set(inStreamSet, mpAppImage_Yuv_In);
    add_stream_to_set(inStreamSet, mpAppImage_Opaque_In);
    //
    add_stream_to_set(outStreamSet, mpHalImage_P1_Raw);
    add_stream_to_set(outStreamSet, mpHalImage_P1_ResizerRaw);
    add_stream_to_set(outStreamSet, mpAppMeta_DynamicP1);
    add_stream_to_set(outStreamSet, mpHalMeta_DynamicP1);
    add_stream_to_set(outStreamSet, mpAppImage_Opaque_Out);
    //
    NodeBuilder builder(nodeId, pNode);
    builder
        .addStream(NodeBuilder::eDirection_IN, inStreamSet)
        .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
    //
    setImageUsage(mpAppImage_Yuv_In        , eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpAppImage_Opaque_In     , eBUFFER_USAGE_SW_READ_OFTEN);
    //
    setImageUsage(mpHalImage_P1_Raw        , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    setImageUsage(mpHalImage_P1_ResizerRaw , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    setImageUsage(mpAppImage_Opaque_Out    , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    //
    MERROR err = builder.build(pContext);
    if( err != OK )
        MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
    return err;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
compareParamsLocked_P1Node(
    P1Node::InitParams const& initParam1,  P1Node::InitParams const& initParam2,
    P1Node::ConfigParams const& cfgParam1, P1Node::ConfigParams const& cfgParam2
) const
{
    FUNC_START;
    if ( initParam1.openId != initParam2.openId ||
         initParam1.nodeId != initParam2.nodeId ||
         strcmp(initParam1.nodeName, initParam2.nodeName) )
        return MFALSE;
    //
    if ( cfgParam1.sensorParams.mode         != cfgParam2.sensorParams.mode ||
         cfgParam1.sensorParams.size         != cfgParam2.sensorParams.size ||
         cfgParam1.sensorParams.fps          != cfgParam2.sensorParams.fps ||
         cfgParam1.sensorParams.pixelMode    != cfgParam2.sensorParams.pixelMode )
        return MFALSE;
    //
    if ( ! cfgParam1.pInAppMeta.get()  || ! cfgParam2.pInAppMeta.get() ||
         ! cfgParam1.pOutAppMeta.get() || ! cfgParam2.pOutAppMeta.get() ||
         ! cfgParam1.pOutHalMeta.get() || ! cfgParam2.pOutHalMeta.get() ||
         cfgParam1.pInAppMeta->getStreamId()  != cfgParam2.pInAppMeta->getStreamId() ||
         cfgParam1.pOutAppMeta->getStreamId() != cfgParam2.pOutAppMeta->getStreamId() ||
         cfgParam1.pOutHalMeta->getStreamId() != cfgParam2.pOutHalMeta->getStreamId() )
        return MFALSE;
    //
    if ( ! cfgParam1.pOutImage_resizer.get() || ! cfgParam2.pOutImage_resizer.get() ||
        cfgParam1.pOutImage_resizer->getStreamId() != cfgParam2.pOutImage_resizer->getStreamId() )
        return MFALSE;
    //
    if ( cfgParam1.pvOutImage_full.size() != cfgParam2.pvOutImage_full.size() )
        return MFALSE;
    //
    for ( size_t i=0; i<cfgParam1.pvOutImage_full.size(); i++ ) {
        MBOOL bMatch = MFALSE;
        for ( size_t j=0; j<cfgParam2.pvOutImage_full.size(); j++ ) {
            if ( cfgParam1.pvOutImage_full.itemAt(i)->getStreamId() == cfgParam2.pvOutImage_full.itemAt(i)->getStreamId() )
            {
                bMatch = MTRUE;
                break;
            }
        }
        if ( !bMatch )
            return MFALSE;
    }
    //
    FUNC_END;
    return MTRUE;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_P2Node(sp<PipelineContext> pContext)
{
    typedef P2Node                  NodeT;
    typedef NodeActor< NodeT >      NodeActorT;
    //
    NodeId_T const nodeId = eNODEID_P2Node;
    //
    NodeT::InitParams initParam;
    {
        initParam.openId = getOpenId();
        initParam.nodeId = nodeId;
        initParam.nodeName = "P2Node";
    }
    NodeT::ConfigParams cfgParam;
    {
        cfgParam.pInAppMeta    = mpAppMeta_Control;
        cfgParam.pInHalMeta    = mpHalMeta_DynamicP1;
        cfgParam.pOutAppMeta   = mpAppMeta_DynamicP2;
        cfgParam.pOutHalMeta   = (!mParams.mbUseP2VSSNode) ? mpHalMeta_DynamicP2 : NULL;
        //
        if( mpHalImage_P1_Raw.get() )
            cfgParam.pvInFullRaw.push_back(mpHalImage_P1_Raw);
        //
        cfgParam.pInResizedRaw = mpHalImage_P1_ResizerRaw;
        //
        if( mpAppImage_Yuv_In.get() )
            cfgParam.pInYuvImage = mpAppImage_Yuv_In;
        //
        if( mpAppImage_Opaque_In.get() )
            cfgParam.pvInOpaque.push_back(mpAppImage_Opaque_In);
        //
        if( mpAppImage_Opaque_Out.get() )
            cfgParam.pvInOpaque.push_back(mpAppImage_Opaque_Out);
        //
        for (size_t i = 0; i < mvAppYuvImage.size(); i++)
            cfgParam.vOutImage.push_back(mvAppYuvImage[i]);
        //
        if( !mParams.mbUseP2VSSNode ) {
            if( mpHalImage_Jpeg_YUV.get() )
                cfgParam.vOutImage.push_back(mpHalImage_Jpeg_YUV);
            if( mpHalImage_Thumbnail_YUV.get() )
                cfgParam.vOutImage.push_back(mpHalImage_Thumbnail_YUV);
        }
        //
        cfgParam.pOutFDImage = mpHalImage_FD_YUV;
        //
        if ( mParams.mOperation_mode == CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE )
            cfgParam.burstNum = mParams.mDefaultBusrstNum;
    }
    //
    sp<NodeActorT> pNode = new NodeActorT( NodeT::createInstance(P2Node::PASS2_STREAM) );
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    StreamSet inStreamSet;
    StreamSet outStreamSet;
    //
    add_stream_to_set(inStreamSet, mpAppMeta_Control);
    add_stream_to_set(inStreamSet, mpHalMeta_DynamicP1);
    add_stream_to_set(inStreamSet, mpHalImage_P1_Raw);
    add_stream_to_set(inStreamSet, mpHalImage_P1_ResizerRaw);
    add_stream_to_set(inStreamSet, mpAppImage_Yuv_In);
    add_stream_to_set(inStreamSet, mpAppImage_Opaque_In);
    add_stream_to_set(inStreamSet, mpAppImage_Opaque_Out);
    //
    add_stream_to_set(outStreamSet, mpAppMeta_DynamicP2);
    if( !mParams.mbUseP2VSSNode ) {
        add_stream_to_set(outStreamSet, mpHalMeta_DynamicP2);
        add_stream_to_set(outStreamSet, mpHalImage_Jpeg_YUV);
        add_stream_to_set(outStreamSet, mpHalImage_Thumbnail_YUV);
    }
    //
    for (size_t i = 0; i < mvAppYuvImage.size(); i++)
        add_stream_to_set(outStreamSet, mvAppYuvImage[i]);
    //
    add_stream_to_set(outStreamSet, mpHalImage_FD_YUV);
    //
    NodeBuilder builder(nodeId, pNode);
    builder
        .addStream(NodeBuilder::eDirection_IN, inStreamSet)
        .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
    //
    setImageUsage(mpAppImage_Yuv_In        , eBUFFER_USAGE_HW_CAMERA_READ | eBUFFER_USAGE_SW_READ_OFTEN);
    setImageUsage(mpAppImage_Opaque_In     , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpAppImage_Opaque_Out    , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpHalImage_P1_Raw        , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpHalImage_P1_ResizerRaw , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    //
    for (size_t i = 0; i < mvAppYuvImage.size(); i++)
        setImageUsage(mvAppYuvImage[i], eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    //
    setImageUsage(mpHalImage_Jpeg_YUV      , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    setImageUsage(mpHalImage_Thumbnail_YUV , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    setImageUsage(mpHalImage_FD_YUV        , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    //
    MERROR err = builder.build(pContext);
    if( err != OK )
        MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
    return err;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_P2VSSNode(sp<PipelineContext> pContext)
{
    typedef P2Node                  NodeT;
    typedef NodeActor< NodeT >      NodeActorT;
    //
    NodeId_T const nodeId = eNODEID_P2Node_VSS;
    //
    NodeT::InitParams initParam;
    {
        initParam.openId = getOpenId();
        initParam.nodeId = nodeId;
        initParam.nodeName = "P2Node_VSS";
    }
    NodeT::ConfigParams cfgParam;
    {
        cfgParam.pInAppMeta    = mpAppMeta_Control;
        cfgParam.pInHalMeta    = mpHalMeta_DynamicP1;
        cfgParam.pOutAppMeta   = NULL;
        cfgParam.pOutHalMeta   = mpHalMeta_DynamicP2;
        //
        if( mpHalImage_P1_Raw.get() ) {
            cfgParam.pvInFullRaw.push_back(mpHalImage_P1_Raw);
            cfgParam.pInResizedRaw = NULL;
        }
        else
            cfgParam.pInResizedRaw = mpHalImage_P1_ResizerRaw;
        //
        if( mParams.mbUseP2VSSNode ) {
            if( mpHalImage_Jpeg_YUV.get() )
                cfgParam.vOutImage.push_back(mpHalImage_Jpeg_YUV);
            if( mpHalImage_Thumbnail_YUV.get() )
                cfgParam.vOutImage.push_back(mpHalImage_Thumbnail_YUV);
        }
    }
    //
    sp<NodeActorT> pNode = new NodeActorT( NodeT::createInstance(P2Node::PASS2_TIMESHARING) );
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    StreamSet inStreamSet;
    StreamSet outStreamSet;
    //
    add_stream_to_set(inStreamSet, mpAppMeta_Control);
    add_stream_to_set(inStreamSet, mpHalMeta_DynamicP1);
    add_stream_to_set(inStreamSet, mpHalImage_P1_Raw);
    add_stream_to_set(inStreamSet, mpHalImage_P1_ResizerRaw);
    //
    add_stream_to_set(outStreamSet, mpHalMeta_DynamicP2);
    add_stream_to_set(outStreamSet, mpHalImage_Jpeg_YUV);
    add_stream_to_set(outStreamSet, mpHalImage_Thumbnail_YUV);
    //
    //
    NodeBuilder builder(nodeId, pNode);
    builder
        .addStream(NodeBuilder::eDirection_IN, inStreamSet)
        .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
    //
    setImageUsage(mpHalImage_P1_Raw        , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpHalImage_P1_ResizerRaw , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    //
    setImageUsage(mpHalImage_Jpeg_YUV      , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    setImageUsage(mpHalImage_Thumbnail_YUV , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    //
    MERROR err = builder.build(pContext);
    if( err != OK )
        MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
    return err;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_FdNode(sp<PipelineContext> pContext)
{
    typedef FdNode                  NodeT;
    typedef NodeActor< NodeT >      NodeActorT;
    //
    NodeId_T const nodeId = eNODEID_FDNode;
    //
    NodeT::InitParams initParam;
    {
        initParam.openId = getOpenId();
        initParam.nodeId = nodeId;
        initParam.nodeName = "FDNode";
    }
    NodeT::ConfigParams cfgParam;
    {
        cfgParam.pInAppMeta    = mpAppMeta_Control;
        cfgParam.pOutAppMeta   = mpAppMeta_DynamicFD;
        cfgParam.vInImage      = mpHalImage_FD_YUV;
    }
    //
    sp<NodeActorT> pNode = new NodeActorT( NodeT::createInstance() );
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    StreamSet inStreamSet;
    StreamSet outStreamSet;
    //
    add_stream_to_set(inStreamSet, mpAppMeta_Control);
    add_stream_to_set(inStreamSet, mpHalImage_FD_YUV);
    //
    add_stream_to_set(outStreamSet, mpAppMeta_DynamicFD);
    //
    NodeBuilder builder(nodeId, pNode);
    builder
        .addStream(NodeBuilder::eDirection_IN, inStreamSet)
        .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
    //
    setImageUsage(mpHalImage_FD_YUV , eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    //
    MERROR err = builder.build(pContext);
    if( err != OK )
        MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
    return err;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_JpegNode(sp<PipelineContext> pContext)
{
    typedef JpegNode                NodeT;
    typedef NodeActor< NodeT >      NodeActorT;
    //
    NodeId_T const nodeId = eNODEID_JpegNode;
    //
    NodeT::InitParams initParam;
    {
        initParam.openId = getOpenId();
        initParam.nodeId = nodeId;
        initParam.nodeName = "JpegNode";
    }
    NodeT::ConfigParams cfgParam;
    {
        cfgParam.pInAppMeta        = mpAppMeta_Control;
        cfgParam.pInHalMeta        = mpHalMeta_DynamicP2;
        cfgParam.pOutAppMeta       = mpAppMeta_DynamicJpeg;
        cfgParam.pInYuv_Main       = mpHalImage_Jpeg_YUV;
        cfgParam.pInYuv_Thumbnail  = mpHalImage_Thumbnail_YUV;
        cfgParam.pOutJpeg          = mpAppImage_Jpeg;
    }
    //
    sp<NodeActorT> pNode = new NodeActorT( NodeT::createInstance() );
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    StreamSet inStreamSet;
    StreamSet outStreamSet;
    //
    add_stream_to_set(inStreamSet, mpAppMeta_Control);
    add_stream_to_set(inStreamSet, mpHalMeta_DynamicP2);
    add_stream_to_set(inStreamSet, mpHalImage_Jpeg_YUV);
    add_stream_to_set(inStreamSet, mpHalImage_Thumbnail_YUV);
    //
    add_stream_to_set(outStreamSet, mpAppMeta_DynamicJpeg);
    add_stream_to_set(outStreamSet, mpAppImage_Jpeg);
    //
    NodeBuilder builder(nodeId, pNode);
    builder
        .addStream(NodeBuilder::eDirection_IN, inStreamSet)
        .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
    //
    setImageUsage(mpHalImage_Jpeg_YUV, eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpHalImage_Thumbnail_YUV, eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(mpAppImage_Jpeg, eBUFFER_USAGE_SW_WRITE_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
    //
    MERROR err = builder.build(pContext);
    if( err != OK )
        MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
    return err;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configContextLocked_Raw16Node(sp<PipelineContext> pContext)
{
    typedef RAW16Node               NodeT;
    typedef NodeActor< NodeT >      NodeActorT;
    //
    NodeId_T const nodeId = eNODEID_RAW16Out;
    //
    NodeT::InitParams initParam;
    {
        initParam.openId = getOpenId();
        initParam.nodeId = nodeId;
        initParam.nodeName = "Raw16Node";
    }
    NodeT::ConfigParams cfgParam;
    {
    }
    //
    sp<NodeActorT> pNode = new NodeActorT( NodeT::createInstance() );
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    StreamSet inStreamSet;
    StreamSet outStreamSet;
    //
    add_stream_to_set(inStreamSet, mpHalImage_P1_Raw);
    //
    add_stream_to_set(outStreamSet, mpAppImage_RAW16);
    //
    NodeBuilder builder(nodeId, pNode);
    builder
        .addStream(NodeBuilder::eDirection_IN, inStreamSet)
        .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
    //
    setImageUsage(mpHalImage_P1_Raw, eBUFFER_USAGE_SW_READ_OFTEN);
    setImageUsage(mpAppImage_RAW16, eBUFFER_USAGE_SW_WRITE_OFTEN);
    //
    MERROR err = builder.build(pContext);
    if( err != OK )
        MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
    return err;

}
/******************************************************************************
 *
 ******************************************************************************/
#undef add_stream_to_set
#undef setImageUsage


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
configRequestRulesLocked()
{
    struct categorize_yuv_stream
    {
        MVOID   operator() (
                    sp<const IImageStreamInfo> const pInfo,
                    MSize const& thres,
                    StreamSet& vLarge, StreamSet& vSmall
                )
                {
                    if( ! pInfo.get() ) return;
                    //
                    MSize const size = pInfo->getImgSize();
                    if ( size.w > thres.w || size.h > thres.h )
                        vLarge.add(pInfo->getStreamId());
                    else
                        vSmall.add(pInfo->getStreamId());
                }
    };
    //
    mvYuvStreams_Fullraw.clear();
    mvYuvStreams_Resizedraw.clear();
    //
    if( ! mpHalImage_P1_ResizerRaw.get() && ! mpHalImage_P1_Raw.get() &&
        ! mpAppImage_Opaque_In.get() && ! mpAppImage_Yuv_In.get() ) {
        MY_LOGE("no available raw stream");
        return UNKNOWN_ERROR;
    }
    //
    MSize const threshold =
        mpHalImage_P1_ResizerRaw.get() ? mpHalImage_P1_ResizerRaw->getImgSize() : MSize(0,0);
    //
    StreamSet& vLarge =
        mpHalImage_P1_Raw.get() || mpAppImage_Opaque_Out.get() ? mvYuvStreams_Fullraw : mvYuvStreams_Resizedraw;
    StreamSet& vSmall =
        mpHalImage_P1_ResizerRaw.get() ? mvYuvStreams_Resizedraw : mvYuvStreams_Fullraw;
    //
    //bool haveFullraw = mpHalImage_P1_Raw.get();
    //
    for( size_t i = 0; i < mvAppYuvImage.size(); i++ ) {
        sp<const IImageStreamInfo> pStreamInfo = mvAppYuvImage.valueAt(i);
        if( (pStreamInfo->getUsageForConsumer() & GRALLOC_USAGE_HW_VIDEO_ENCODER) &&
             mParams.mb4KRecording )
        {
            // 4K recording only
            // Avoid to use IMGO & RRZO in the same time
            // for reaching 30 fps performance
            categorize_yuv_stream()(pStreamInfo, threshold, vSmall, vSmall);
        }
        else
        {
            categorize_yuv_stream()(pStreamInfo, threshold, vLarge, vSmall);
        }
    }
    categorize_yuv_stream()(mpHalImage_FD_YUV, threshold, vLarge, vSmall);
    //
    categorize_yuv_stream()(mpHalImage_Jpeg_YUV, MSize(0,0), vLarge, vSmall);
    categorize_yuv_stream()(mpHalImage_Thumbnail_YUV, MSize(0,0), vLarge, vSmall);
    //
#if 1
    // dump raw stream dispatch rule
    StreamId_T fullStream = mpHalImage_P1_Raw.get() ? mpHalImage_P1_Raw->getStreamId() :
                            mpAppImage_Opaque_Out.get() ? mpAppImage_Opaque_Out->getStreamId() :
                            NULL;
    for( size_t i = 0; i < mvYuvStreams_Fullraw.size(); i++ ) {
        MY_LOGD("full raw streamId:%#" PRIxPTR " -> yuv streamId:%#" PRIxPTR,
                fullStream, mvYuvStreams_Fullraw[i]);
    }
    for( size_t i = 0; i < mvYuvStreams_Resizedraw.size(); i++ ) {
        MY_LOGD("resized raw streamId:%#" PRIxPTR " -> yuv streamId:%#" PRIxPTR,
                mpHalImage_P1_ResizerRaw->getStreamId(), mvYuvStreams_Resizedraw[i]);
    }
#endif
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
evaluateRequestLocked(parsedAppRequest const& request, evaluateRequestResult& result)
{
    enum ePath
    {
        eImagePathP1,
        eImagePathP2Resized,
        eImagePathP2Full,
        eImagePathP2VSS,
        eImagePathRaw16,
        eImagePathJpeg,
        eImagePathFD,
        //
        eMetaPathP1,
        eMetaPathP2,
        eMetaPathP2VSS,
        eMetaPathRaw16,
        eMetaPathJpeg,
        eMetaPathFD,
        //
        ePathCount,
    };
    //
    struct
    {
        IOMap                       maps[ePathCount];
        DefaultKeyedVector<
            StreamId_T,
            sp<IImageStreamInfo>
                >                   vUpdatedImageInfos;
        //
        IOMap&                      editIOMap(ePath path) { return maps[path]; }
        MVOID                       addIn(ePath path, sp<IStreamInfo> pInfo) {
                                        editIOMap(path).addIn(pInfo->getStreamId());
                                    }
        MVOID                       addOut(ePath path, sp<IStreamInfo> pInfo) {
                                        editIOMap(path).addOut(pInfo->getStreamId());
                                    }
        MBOOL                       isConfigured(ePath path) {
                                        return editIOMap(path).vIn.size() ||
                                            editIOMap(path).vOut.size();
                                    }
        MVOID                       updateStreamInfo(sp<IImageStreamInfo> pInfo) {
                                        vUpdatedImageInfos.add(pInfo->getStreamId(), pInfo);
                                    }
    } aCollector;
    NodeEdgeSet& aEdges = result.edges;
    NodeSet& aRoot      = result.roots;

#define FUNC_ASSERT(exp, msg) \
    do{ if(!(exp)) { MY_LOGE("%s", msg); return INVALID_OPERATION; } } while(0)
    //
    FUNC_ASSERT( request.vIImageInfos_Raw.size() == 0, "[TODO] not supported yet!" );
    //FUNC_ASSERT( request.vIImageInfos_Yuv.size() == 0, "[TODO] not supported yet!" );
    FUNC_ASSERT( request.vOImageInfos_Raw.size() <= 1, "[TODO] not supported yet!" );
    FUNC_ASSERT( request.vOImageInfos_Jpeg.size() <= 1, "[TODO] not supported yet!" );
    //
    // set root node
    aRoot.add(eNODEID_P1Node);
    //
    if ( request.vOImageInfos_Raw.size() )
    {
        IImageStreamInfo const* pStreamInfo = request.vOImageInfos_Raw[0].get();
        if ( isStream( mpAppImage_RAW16, pStreamInfo->getStreamId() ) )
        {
            // Raw16: full-size raw -> raw16
            //
            FUNC_ASSERT(
                    mParams.mbUseRaw16Node && mpHalImage_P1_Raw.get() && mpAppImage_RAW16.get(),
                    "not properly configured");
            //
            aCollector.addOut(eImagePathP1, mpHalImage_P1_Raw);
            aCollector.addIn(eImagePathRaw16, mpHalImage_P1_Raw);
            aCollector.addOut(eImagePathRaw16, mpAppImage_RAW16);
            aEdges.addEdge(eNODEID_P1Node, eNODEID_RAW16Out);
            MY_LOGD("evaluateRequestLocked add RAW16");
        }
        else
        {
            MY_LOGE("not supported raw output stream %#" PRIxPTR ,
                    pStreamInfo->getStreamId());
            return INVALID_OPERATION;
        }
    }
    bool useYuvIn = false;
    if ( request.vIImageInfos_Yuv.size() )
    {
        IImageStreamInfo const* pStreamInfo = request.vIImageInfos_Yuv[0].get();
        FUNC_ASSERT(
                mpAppImage_Yuv_In.get(),
                "wrong yuv in config");
        //
        aCollector.addIn(eImagePathP1, mpAppImage_Yuv_In);
        aCollector.addIn(eImagePathP2Full, mpAppImage_Yuv_In);
        useYuvIn = true;
    }
    bool useOpaqueIn = false;
    if ( request.vIImageInfos_Opaque.size() )
    {
        IImageStreamInfo const* pStreamInfo = request.vIImageInfos_Opaque[0].get();
        FUNC_ASSERT(
                mpAppImage_Opaque_In.get(),
                "wrong opaque in config");
        //
        aCollector.addIn(eImagePathP1, mpAppImage_Opaque_In);
        aCollector.addIn(eImagePathP2Full, mpAppImage_Opaque_In);
        useOpaqueIn = true;
    }
    bool useOpaqueOut = false;
    if ( request.vOImageInfos_Opaque.size() )
    {
        IImageStreamInfo const* pStreamInfo = request.vOImageInfos_Opaque[0].get();
        FUNC_ASSERT(
                mpAppImage_Opaque_Out.get(),
                "wrong opaque out config");
        //
        aCollector.addOut(eImagePathP1, mpAppImage_Opaque_Out);
        useOpaqueOut = true;
    }
    //
    if( request.vOImageInfos_Yuv.size() ) {
        //
        FUNC_ASSERT(
                mvYuvStreams_Fullraw.size() == 0 || mpHalImage_P1_Raw.get() ||
                useOpaqueIn || useYuvIn,
                "wrong fullraw config");
        FUNC_ASSERT(
                mvYuvStreams_Resizedraw.size() == 0 || mpHalImage_P1_ResizerRaw.get(),
                "wrong resizedraw config");
        //
        bool useFull = false;
        bool useResized = false;
        for( size_t i = 0; i < request.vOImageInfos_Yuv.size(); i++ )
        {
            sp<IImageStreamInfo> pInfo = request.vOImageInfos_Yuv.valueAt(i);
            //
            StreamId_T const streamId = pInfo->getStreamId();
            if( 0 <= mvYuvStreams_Fullraw.indexOf(streamId) || useOpaqueIn || useYuvIn )
            {
                aCollector.addOut(eImagePathP2Full, pInfo);
                useFull = MTRUE;
            } else if( 0 <= mvYuvStreams_Resizedraw.indexOf(streamId) )
            {
                aCollector.addOut(eImagePathP2Resized, pInfo);
                useResized = MTRUE;
            }
            else
            {
                MY_LOGE("cannot find propery raw for stream %s(%#" PRIxPTR ")",
                        pInfo->getStreamName(),streamId);
                return UNKNOWN_ERROR;
            }
        }
        //
        if ( useFull ) {
            sp<IImageStreamInfo> pImageP2FullIn = NULL;
            if ( useOpaqueOut ) {
                aCollector.addOut(eImagePathP1, mpAppImage_Opaque_Out);
                pImageP2FullIn = mpAppImage_Opaque_Out;
            } else if ( useOpaqueIn ) {
                aCollector.addIn(eImagePathP1, mpAppImage_Opaque_In);
                pImageP2FullIn = mpAppImage_Opaque_In;
            } else if ( useYuvIn ) {
                aCollector.addIn(eImagePathP1, mpAppImage_Yuv_In);
                pImageP2FullIn = mpAppImage_Yuv_In;
            } else {
                aCollector.addOut(eImagePathP1, mpHalImage_P1_Raw);
                pImageP2FullIn = mpHalImage_P1_Raw;
            }
            if( request.vOImageInfos_Yuv.size() )
                aCollector.addIn(eImagePathP2Full, pImageP2FullIn);
        }
        if ( useResized && !useOpaqueIn && !useYuvIn ) {
            aCollector.addOut(eImagePathP1, mpHalImage_P1_ResizerRaw);
            //
            aCollector.addIn(eImagePathP2Resized, mpHalImage_P1_ResizerRaw);
        }
        //
        aEdges.addEdge(eNODEID_P1Node, eNODEID_P2Node);
    }
    //
    if( request.vOImageInfos_Jpeg.size() ) {
        //
        sp<IImageStreamInfo> pHalImage_Thumbnail_YUV;
        createStreamInfoLocked_Thumbnail_YUV(request.pAppMetaControl, pHalImage_Thumbnail_YUV);
        //
        if( pHalImage_Thumbnail_YUV.get() )
            aCollector.updateStreamInfo(pHalImage_Thumbnail_YUV);
        //
        sp<IImageStreamInfo> pSourceRaw =
            ( 0 > mvYuvStreams_Fullraw.indexOf(mpHalImage_Jpeg_YUV->getStreamId())) ? mpHalImage_P1_ResizerRaw :
            (useOpaqueOut) ? mpAppImage_Opaque_Out :
            (useOpaqueIn) ? mpAppImage_Opaque_In :
            (useYuvIn) ? mpAppImage_Yuv_In :
            (mpHalImage_P1_Raw.get()) ? mpHalImage_P1_Raw : NULL;
        FUNC_ASSERT( pSourceRaw.get(), "null source raw" );

        bool const isVss = mParams.mbUseP2VSSNode;
        // p2
        if( ! isVss ) {
            if( ! useYuvIn && ! useOpaqueIn )
                aCollector.addOut(eImagePathP1, pSourceRaw);
            //
            aCollector.addIn(eImagePathP2Full, pSourceRaw);
            aCollector.addOut(eImagePathP2Full, mpHalImage_Jpeg_YUV);
            if( pHalImage_Thumbnail_YUV.get() )
                aCollector.addOut(eImagePathP2Full, pHalImage_Thumbnail_YUV);
            //
            aEdges.addEdge(eNODEID_P1Node, eNODEID_P2Node);
            aEdges.addEdge(eNODEID_P2Node, eNODEID_JpegNode);
        }
        else {
            if( ! useYuvIn && ! useOpaqueIn )
                aCollector.addOut(eImagePathP1, pSourceRaw);
            //
            aCollector.addIn(eImagePathP2VSS, pSourceRaw);
            aCollector.addOut(eImagePathP2VSS, mpHalImage_Jpeg_YUV);
            if( pHalImage_Thumbnail_YUV.get() )
                aCollector.addOut(eImagePathP2VSS, pHalImage_Thumbnail_YUV);
            //
            aEdges.addEdge(eNODEID_P1Node, eNODEID_P2Node_VSS);
            aEdges.addEdge(eNODEID_P2Node_VSS, eNODEID_JpegNode);
        }
        // jpeg
        sp<IImageStreamInfo> pHalImage_Jpeg_YUV;
        if ( !mJpegRotationEnable ) {
        aCollector.addIn(eImagePathJpeg, mpHalImage_Jpeg_YUV);
            }
        else {
               createStreamInfoLocked_Jpeg_YUV(request.pAppMetaControl, pHalImage_Jpeg_YUV);
               if ( pHalImage_Jpeg_YUV.get() ) {
                   aCollector.addIn(eImagePathJpeg, pHalImage_Jpeg_YUV);
                   //request.vIHalImage.add(pHalImage_Jpeg_YUV->getStreamId(), pHalImage_Jpeg_YUV);
                   MY_LOGD_IF( 1, "Add new Jpeg_Yuv to map, StreamName:%s StreamId:%#" PRIxPTR " ImageSize:%dx%d Transform:%d",
                               pHalImage_Jpeg_YUV->getStreamName(), pHalImage_Jpeg_YUV->getStreamId(),
                               pHalImage_Jpeg_YUV->getImgSize().w,  pHalImage_Jpeg_YUV->getImgSize().h,
                               pHalImage_Jpeg_YUV->getTransform() );
                   aCollector.updateStreamInfo(pHalImage_Jpeg_YUV);
               } else {
                   aCollector.addIn(eImagePathJpeg, mpHalImage_Jpeg_YUV);
                   MY_LOGD_IF( 1, "Add default Jpeg_Yuv to map, StreamName:%s StreamId:%#" PRIxPTR " ImageSize:%dx%d Transform:%d",
                               mpHalImage_Jpeg_YUV->getStreamName(), mpHalImage_Jpeg_YUV->getStreamId(),
                               mpHalImage_Jpeg_YUV->getImgSize().w,  mpHalImage_Jpeg_YUV->getImgSize().h,
                               mpHalImage_Jpeg_YUV->getTransform() );
                   aCollector.updateStreamInfo(mpHalImage_Jpeg_YUV);
               }

            }

        if( pHalImage_Thumbnail_YUV.get() )
            aCollector.addIn(eImagePathJpeg, pHalImage_Thumbnail_YUV);
         MY_LOGD_IF( 1, "Add Thumb_Yuv to map, StreamName:%s StreamId:%#" PRIxPTR " ImageSize:%dx%d Transform:%d",
                    mpHalImage_Thumbnail_YUV->getStreamName(), mpHalImage_Thumbnail_YUV->getStreamId(),
                    mpHalImage_Thumbnail_YUV->getImgSize().w,  mpHalImage_Thumbnail_YUV->getImgSize().h,
                    mpHalImage_Thumbnail_YUV->getTransform() );
        aCollector.addOut(eImagePathJpeg, mpAppImage_Jpeg);
        //
    }
    //
    {
        // workaround: if p1node is used, config both raw if exists
        if( aCollector.isConfigured(eImagePathP1) ) {
            //if( mpHalImage_P1_Raw.get() )
            //    aCollector.addOut(eImagePathP1, mpHalImage_P1_Raw);
            // there is no resizer raw if do opaque reprocessing
            if( mpHalImage_P1_ResizerRaw.get() && !useOpaqueIn && !useYuvIn )
                aCollector.addOut(eImagePathP1, mpHalImage_P1_ResizerRaw);
        }
    }
    //
    if( isFdEnable(request.pAppMetaControl) && !useOpaqueIn && !useYuvIn )
    {
        FUNC_ASSERT(
                mpHalImage_FD_YUV.get(),
                "wrong fd yuv config");
        if( 0 <= mvYuvStreams_Resizedraw.indexOf(mpHalImage_FD_YUV->getStreamId()) || useOpaqueOut )
        {
            aCollector.addOut(eImagePathP1, mpHalImage_P1_ResizerRaw);
            aCollector.addIn(eImagePathP2Resized, mpHalImage_P1_ResizerRaw);
            aCollector.addOut(eImagePathP2Resized, mpHalImage_FD_YUV);
        }
        else
        {
            aCollector.addOut(eImagePathP1, mpHalImage_P1_Raw);
            aCollector.addIn(eImagePathP2Full, mpHalImage_P1_Raw);
            aCollector.addOut(eImagePathP2Full, mpHalImage_FD_YUV);
        }
        //
        aCollector.addIn(eImagePathFD, mpHalImage_FD_YUV);
        //
        aEdges.addEdge(eNODEID_P1Node, eNODEID_P2Node);
        aEdges.addEdge(eNODEID_P2Node, eNODEID_FDNode);
    }
    //
    // update meta

    if( aCollector.isConfigured(eImagePathP1) )
    {
        aCollector.addIn(eMetaPathP1, mpAppMeta_Control);
        aCollector.addIn(eMetaPathP1, mpHalMeta_Control);
        aCollector.addOut(eMetaPathP1, mpAppMeta_DynamicP1);
        aCollector.addOut(eMetaPathP1, mpHalMeta_DynamicP1);
    }
    if( aCollector.isConfigured(eImagePathP2Full) ||
        aCollector.isConfigured(eImagePathP2Resized))
    {
        aCollector.addIn(eMetaPathP2, mpAppMeta_Control);
        aCollector.addIn(eMetaPathP2, mpHalMeta_DynamicP1);
        aCollector.addOut(eMetaPathP2, mpAppMeta_DynamicP2);
        if( ! mParams.mbUseP2VSSNode /*aCollector.isConfigured(eImagePathP2VSS)*/ )
            aCollector.addOut(eMetaPathP2, mpHalMeta_DynamicP2);
        //
    }
    //
    if( aCollector.isConfigured(eImagePathFD) )
    {
        aCollector.addIn(eMetaPathFD, mpAppMeta_Control);
        aCollector.addOut(eMetaPathFD, mpAppMeta_DynamicFD);
    }
    //
    if( aCollector.isConfigured(eImagePathP2VSS) )
    {
        aCollector.addIn(eMetaPathP2VSS, mpAppMeta_Control);
        aCollector.addIn(eMetaPathP2VSS, mpHalMeta_DynamicP1);
        aCollector.addOut(eMetaPathP2VSS, mpHalMeta_DynamicP2);
    }
    //
    if( aCollector.isConfigured(eImagePathJpeg) )
    {
        aCollector.addIn(eMetaPathJpeg, mpAppMeta_Control);
        aCollector.addIn(eMetaPathJpeg, mpHalMeta_DynamicP2);
        aCollector.addOut(eMetaPathJpeg, mpAppMeta_DynamicJpeg);
    }
    //
    // update to result
    for( size_t i = 0 ; i < aCollector.vUpdatedImageInfos.size(); i++ )
    {
        result.vUpdatedImageInfos.add(
                aCollector.vUpdatedImageInfos.keyAt(i),
                aCollector.vUpdatedImageInfos.valueAt(i)
                );
    }
    //
#define updateIOMap(_type_, _nodeId_, _path_ )                          \
    do{                                                                 \
        if( aCollector.isConfigured(_path_) ) {                    \
            result.nodeIOMap##_type_.add(                               \
                    _nodeId_,                                           \
                    IOMapSet().add(aCollector.editIOMap(_path_))); \
        }                                                               \
    } while(0)

    updateIOMap(Image, eNODEID_P1Node, eImagePathP1);
    {
        IOMapSet iomaps;
        if( aCollector.isConfigured(eImagePathP2Full) )
            iomaps.add(aCollector.editIOMap(eImagePathP2Full));
        if( aCollector.isConfigured(eImagePathP2Resized) )
            iomaps.add(aCollector.editIOMap(eImagePathP2Resized));
        result.nodeIOMapImage.add(eNODEID_P2Node, iomaps);
    }
    updateIOMap(Image, eNODEID_P2Node_VSS, eImagePathP2VSS);
    updateIOMap(Image, eNODEID_RAW16Out  , eImagePathRaw16);
    updateIOMap(Image, eNODEID_FDNode    , eImagePathFD);
    updateIOMap(Image, eNODEID_JpegNode  , eImagePathJpeg);
    //
    updateIOMap(Meta , eNODEID_P1Node    , eMetaPathP1);
    updateIOMap(Meta , eNODEID_P2Node    , eMetaPathP2);
    updateIOMap(Meta , eNODEID_P2Node_VSS, eMetaPathP2VSS);
    updateIOMap(Meta , eNODEID_RAW16Out  , eMetaPathRaw16);
    updateIOMap(Meta , eNODEID_FDNode    , eMetaPathFD);
    updateIOMap(Meta , eNODEID_JpegNode  , eMetaPathJpeg);
    //
#undef updateIOMap
    //
    { // app image
        result.vAppImageBuffers.setCapacity(
                request.pRequest->vIImageBuffers.size() +
                request.pRequest->vOImageBuffers.size()
                );
        for( size_t i = 0; i < request.pRequest->vIImageBuffers.size(); i++ )
            result.vAppImageBuffers.add(
                    request.pRequest->vIImageBuffers.keyAt(i),
                    request.pRequest->vIImageBuffers.valueAt(i)
                    );
        for( size_t i = 0; i < request.pRequest->vOImageBuffers.size(); i++ )
            result.vAppImageBuffers.add(
                    request.pRequest->vOImageBuffers.keyAt(i),
                    request.pRequest->vOImageBuffers.valueAt(i)
                    );
    }
    //
    { // hal image
        result.vHalImageBuffers.clear();
    }
    //
    { // app meta
        result.vAppMetaBuffers.setCapacity(request.pRequest->vIMetaBuffers.size());
        for( size_t i = 0; i < request.pRequest->vIMetaBuffers.size(); i++ )
            result.vAppMetaBuffers.add(
                    request.pRequest->vIMetaBuffers.keyAt(i),
                    request.pRequest->vIMetaBuffers.valueAt(i)
                    );
    }
    //
    { // hal meta
        result.vHalMetaBuffers.setCapacity(1);
        sp<HalMetaStreamBuffer> pBuffer =
            HalMetaStreamBufferAllocatorT(mpHalMeta_Control.get())();
        result.vHalMetaBuffers.add(mpHalMeta_Control->getStreamId(), pBuffer);
    }
    //
    return OK;
#undef FUNC_ASSERT
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
refineRequestMetaStreamBuffersLocked(evaluateRequestResult& result)
{
    if( ! mpHalMeta_Control.get() ) {
        MY_LOGE("should config hal control meta");
        return UNKNOWN_ERROR;
    }
    //
    {
        sp<IMetaStreamBuffer> pBuf = result.vHalMetaBuffers.valueFor(mpHalMeta_Control->getStreamId());
        if( pBuf.get() )
        {
            IMetadata* pMetadata = pBuf->tryWriteLock(LOG_TAG);

            // update sensor size
            {
                IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
                entry.push_back(mParams.mSensorSize, Type2Type< MSize >());
                pMetadata->update(entry.tag(), entry);
            }

            if ( (mpAppImage_Jpeg.get() &&
                    0 <= result.vAppImageBuffers.indexOfKey(mpAppImage_Jpeg->getStreamId())) ||
                 (mpAppImage_Opaque_Out.get() &&
                    0 <= result.vAppImageBuffers.indexOfKey(mpAppImage_Opaque_Out->getStreamId())) )
            {
                MY_LOGD_IF(1, "set MTK_HAL_REQUEST_REQUIRE_EXIF = 1");
                IMetadata::IEntry entry(MTK_HAL_REQUEST_REQUIRE_EXIF);
                entry.push_back(1, Type2Type<MUINT8>());
                pMetadata->update(entry.tag(), entry);
            }

            // set "the largest frame duration of streams" as "minimum frame duration"
            {
                MINT64 iMinFrmDuration = 0;
                for ( size_t i=0; i<result.vAppImageBuffers.size(); i++ ) {
                    StreamId_T const streamId = result.vAppImageBuffers.keyAt(i);
                    if( mvStreamDurations.indexOfKey(streamId) < 0 ) {
                        MY_LOGE("Request App stream %#" PRIxPTR "have not configured yet", streamId);
                        continue;
                    }
                    iMinFrmDuration = ( mvStreamDurations.valueFor(streamId) > iMinFrmDuration)?
                        mvStreamDurations.valueFor(streamId) : iMinFrmDuration;
                }
                MY_LOGD_IF( mLogLevel>=2, "The min frame duration is %lld", iMinFrmDuration);
                IMetadata::IEntry entry(MTK_P1NODE_MIN_FRM_DURATION);
                entry.push_back(iMinFrmDuration, Type2Type<MINT64>());
                pMetadata->update(entry.tag(), entry);
            }
            //
            for ( size_t i=0; i<result.vAppMetaBuffers.size(); i++ )
            {
                if ( mpAppMeta_Control.get() &&
                     mpAppMeta_Control->getStreamId() ==
                     result.vAppMetaBuffers[i]->getStreamInfo()->getStreamId() )
                {
                    MUINT8 bRepeating = (MUINT8) result.vAppMetaBuffers[i]->isRepeating();
                    IMetadata::IEntry entry(MTK_HAL_REQUEST_REPEAT);
                    entry.push_back(bRepeating, Type2Type< MUINT8 >());
                    pMetadata->update(entry.tag(), entry);
                    MY_LOGD_IF( mLogLevel>=2, "Control AppMetadata is repeating(%d)", bRepeating);
                    break;
                }
            }
            //
            pBuf->unlock(LOG_TAG, pMetadata);
        }
        else
        {
            MY_LOGE("cannot get hal control meta sb.");
            return UNKNOWN_ERROR;
        }
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
#define max(a,b)  ((a) < (b) ? (b) : (a))
#define min(a,b)  ((a) < (b) ? (a) : (b))
MERROR
PipelineDefaultImp::
createStreamInfoLocked_Jpeg_YUV(
    IMetadata const* pMetadata,
    android::sp<IImageStreamInfo>& rpStreamInfo
) const
{
    if ( mpHalImage_Jpeg_YUV == 0 ) {
        MY_LOGW("No config stream: Jpeg_YUV");
        return NO_INIT;
    }
    IMetadata::IEntry const& entryJpegOrientation = pMetadata->entryFor(MTK_JPEG_ORIENTATION);
    if  ( entryJpegOrientation.isEmpty() ) {
        MY_LOGW("No tag: MTK_JPEG_ORIENTATION");
        return NAME_NOT_FOUND;
    }
    //
    MINT32 const jpegOrientation = entryJpegOrientation.itemAt(0, Type2Type<MINT32>());
    MUINT32      jpegTransform   = 0;
    if ( 0==jpegOrientation )
        jpegTransform = 0;
    else if ( 90==jpegOrientation )
        jpegTransform = eTransform_ROT_90;
    else if ( 180==jpegOrientation )
        jpegTransform = eTransform_ROT_180;
    else if ( 270==jpegOrientation )
        jpegTransform = eTransform_ROT_270;
    else
         MY_LOGW("Invalid Jpeg Orientation value: %d", jpegOrientation);
    //
    MUINT32 const imgTransform   = mpHalImage_Jpeg_YUV->getTransform();
    MY_LOGD_IF( 1, "Jpeg orientation from metadata:%d transform current(%d) & previous(%d)",
                jpegOrientation, jpegTransform, imgTransform);
    if ( imgTransform == jpegTransform ) {
        rpStreamInfo = NULL;
        return OK;
    }
    MSize size;
    if ( jpegTransform&eTransform_ROT_90 ) { // pillarbox
        size.w = min(mpHalImage_Jpeg_YUV->getImgSize().w, mpHalImage_Jpeg_YUV->getImgSize().h);
        size.h = max(mpHalImage_Jpeg_YUV->getImgSize().w, mpHalImage_Jpeg_YUV->getImgSize().h);
    } else { // letterbox
        size.w = max(mpHalImage_Jpeg_YUV->getImgSize().w, mpHalImage_Jpeg_YUV->getImgSize().h);
        size.h = min(mpHalImage_Jpeg_YUV->getImgSize().w, mpHalImage_Jpeg_YUV->getImgSize().h);
    }
    MINT const format = mpHalImage_Jpeg_YUV->getImgFormat();
    MUINT const usage = mpHalImage_Jpeg_YUV->getUsageForAllocator();
    sp<ImageStreamInfo>
        pStreamInfo = createImageStreamInfo(
            mpHalImage_Jpeg_YUV->getStreamName(),
            mpHalImage_Jpeg_YUV->getStreamId(),
            mpHalImage_Jpeg_YUV->getStreamType(),
            mpHalImage_Jpeg_YUV->getMaxBufNum(),
            mpHalImage_Jpeg_YUV->getMinInitBufNum(),
            mpHalImage_Jpeg_YUV->getUsageForAllocator(),
            mpHalImage_Jpeg_YUV->getImgFormat(),
            size, jpegTransform
        );
    if( pStreamInfo == NULL ) {
        MY_LOGE(
            "fail to new ImageStreamInfo: %s %#" PRIxPTR,
            mpHalImage_Jpeg_YUV->getStreamName(),
            mpHalImage_Jpeg_YUV->getStreamId()
        );
        return NO_MEMORY;
    }
    rpStreamInfo = pStreamInfo;
    MY_LOGD_IF(
        1,
        "streamId:%#" PRIxPTR " %s %p (%p) yuvsize:%dx%d jpegOrientation:%d",
        rpStreamInfo->getStreamId(),
        rpStreamInfo->getStreamName(),
        rpStreamInfo.get(),
        mpHalImage_Jpeg_YUV.get(),
        rpStreamInfo->getImgSize().w, rpStreamInfo->getImgSize().h, jpegOrientation
    );
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDefaultImp::
createStreamInfoLocked_Thumbnail_YUV(
    IMetadata const* pMetadata,
    android::sp<IImageStreamInfo>& rpStreamInfo
) const
{
    if  ( mpHalImage_Thumbnail_YUV == 0 ) {
        MY_LOGW("No config stream: Thumbnail_YUV");
        return NO_INIT;
    }
    //
    IMetadata::IEntry const& entryThumbnailSize = pMetadata->entryFor(MTK_JPEG_THUMBNAIL_SIZE);
    if  ( entryThumbnailSize.isEmpty() ) {
        MY_LOGW("No tag: MTK_JPEG_THUMBNAIL_SIZE");
        return NAME_NOT_FOUND;
    }
    MSize const& thumbnailSize = entryThumbnailSize.itemAt(0, Type2Type<MSize>());
    if  ( ! thumbnailSize ) {
        MY_LOGW("Bad thumbnail size: %dx%d", thumbnailSize.w, thumbnailSize.h);
        return NOT_ENOUGH_DATA;
    }
    MY_LOGD_IF( 1, "thumbnail size from metadata: %dx%d", thumbnailSize.w, thumbnailSize.h);
    //
    //
    IMetadata::IEntry const& entryJpegOrientation = pMetadata->entryFor(MTK_JPEG_ORIENTATION);
    if  ( entryJpegOrientation.isEmpty() ) {
        MY_LOGW("No tag: MTK_JPEG_ORIENTATION");
        return NAME_NOT_FOUND;
    }
    //
    MSize const yuvthumbnailsize = calcThumbnailYuvSize(
                                        mpHalImage_Jpeg_YUV->getImgSize(),
                                        thumbnailSize
                                        );
    //
    MINT32  jpegOrientation = 0;
    MUINT32 jpegTransform   = 0;
    MSize   thunmbSize      = yuvthumbnailsize; // default thumbnail size
    //
    MINT const format = mpHalImage_Thumbnail_YUV->getImgFormat();
    IImageStreamInfo::BufPlanes_t bufPlanes;
    switch (format)
    {
    case eImgFmt_YUY2:{
        IImageStreamInfo::BufPlane bufPlane;
        bufPlane.rowStrideInBytes = (yuvthumbnailsize.w << 1);
        bufPlane.sizeInBytes = bufPlane.rowStrideInBytes * yuvthumbnailsize.h;
        bufPlanes.push_back(bufPlane);
        }break;
    default:
        MY_LOGE("not supported format: %#x", format);
        break;
    }
    //
    rpStreamInfo = new ImageStreamInfo(
        mpHalImage_Thumbnail_YUV->getStreamName(),
        mpHalImage_Thumbnail_YUV->getStreamId(),
        mpHalImage_Thumbnail_YUV->getStreamType(),
        mpHalImage_Thumbnail_YUV->getMaxBufNum(),
        mpHalImage_Thumbnail_YUV->getMinInitBufNum(),
        mpHalImage_Thumbnail_YUV->getUsageForAllocator(),
        format,
        thunmbSize,
        bufPlanes,
        jpegTransform
    );
    if  ( rpStreamInfo == 0 ) {
        MY_LOGE(
            "fail to new ImageStreamInfo: %s %#" PRIxPTR,
            mpHalImage_Thumbnail_YUV->getStreamName(),
            mpHalImage_Thumbnail_YUV->getStreamId()
        );
        return NO_MEMORY;
    }
    //
    MY_LOGD_IF(
        1,
        "streamId:%#" PRIxPTR " %s %p %p yuvthumbnailsize:%dx%d jpegOrientation:%d",
        rpStreamInfo->getStreamId(),
        rpStreamInfo->getStreamName(),
        rpStreamInfo.get(),
        mpHalImage_Thumbnail_YUV.get(),
        thunmbSize.w, thunmbSize.h, jpegOrientation
    );
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MSize
PipelineDefaultImp::
calcThumbnailYuvSize(
    MSize const rPicSize,
    MSize const rThumbnailsize
) const
{
#define align2(x) (((x) + 1) & (~0x1))
    MSize size;
    MUINT32 const val0 = rPicSize.w * rThumbnailsize.h;
    MUINT32 const val1 = rPicSize.h * rThumbnailsize.w;
    if( val0 > val1 ) {
        size.w = align2(val0/rPicSize.h);
        size.h = rThumbnailsize.h;
    }
    else if( val0 < val1 ) {
        size.w = rThumbnailsize.w;
        size.h = align2(val1/rPicSize.w);
    }
    else {
        size = rThumbnailsize;
    }
#undef align2
    MY_LOGD_IF(1, "thumb %dx%d, pic %dx%d -> yuv for thumb %dx%d",
            rThumbnailsize.w, rThumbnailsize.h,
            rPicSize.w, rPicSize.h,
            size.w, size.h
            );
    return size;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
PipelineDefaultImp::
isFdEnable(
    IMetadata const* pMetadata
)
{
    //  If Face detection is not OFF or scene mode is face priority,
    //  add App:Meta:FD_result stream to Output App Meta Streams.
    //return 0;

    IMetadata::IEntry const& entryFdMode = pMetadata->entryFor(MTK_STATISTICS_FACE_DETECT_MODE);
    IMetadata::IEntry const& entryfaceScene = pMetadata->entryFor(MTK_CONTROL_SCENE_MODE);
    IMetadata::IEntry const& entryGdMode = pMetadata->entryFor(MTK_FACE_FEATURE_GESTURE_MODE);
    IMetadata::IEntry const& entrySdMode = pMetadata->entryFor(MTK_FACE_FEATURE_SMILE_DETECT_MODE);
    IMetadata::IEntry const& entryAsdMode = pMetadata->entryFor(MTK_FACE_FEATURE_ASD_MODE);

    MBOOL FDMetaEn, FDEnable;
    //
    FDMetaEn =   //(0 != mDebugFdMode) ||
             ( !entryFdMode.isEmpty() && MTK_STATISTICS_FACE_DETECT_MODE_OFF != entryFdMode.itemAt(0, Type2Type<MUINT8>())) ||
             ( !entryfaceScene.isEmpty() && MTK_CONTROL_SCENE_MODE_FACE_PRIORITY == entryfaceScene.itemAt(0, Type2Type<MUINT8>())) ||
             ( !entryGdMode.isEmpty() && MTK_FACE_FEATURE_GESTURE_MODE_OFF != entryGdMode.itemAt(0, Type2Type<MINT32>())) ||
             ( !entrySdMode.isEmpty() && MTK_FACE_FEATURE_SMILE_DETECT_MODE_OFF != entrySdMode.itemAt(0, Type2Type<MINT32>())) ||
             ( !entryAsdMode.isEmpty() && MTK_FACE_FEATURE_ASD_MODE_OFF != entryAsdMode.itemAt(0, Type2Type<MINT32>()));
    FDEnable = mPrevFDEn || FDMetaEn;
    mPrevFDEn = FDMetaEn;
    return FDEnable;

}


/******************************************************************************
 *  IPipelineBufferSetFrameControl::IAppCallback Interfaces.
 ******************************************************************************/
MVOID
PipelineDefaultImp::
updateFrame(
    MUINT32 const frameNo,
    MINTPTR const userId,
    Result const& result
)
{
    if ( result.bFrameEnd ) return;

    MY_LOGD("frameNo %d, user %#" PRIxPTR ", AppLeft %d, appMeta %d, HalLeft %d, halMeta %d",
            frameNo, userId,
            result.nAppOutMetaLeft, result.vAppOutMeta.size(),
            result.nHalOutMetaLeft, result.vHalOutMeta.size()
           );
    sp<IPipelineModelMgr::IAppCallback> pAppCallback;
    pAppCallback = mpAppCallback.promote();
    if ( ! pAppCallback.get() ) {
        MY_LOGE("Have not set callback to device");
        FUNC_END;
        return;
    }
    pAppCallback->updateFrame(frameNo, userId, result.nAppOutMetaLeft, result.vAppOutMeta);
}


/******************************************************************************
 *
 ******************************************************************************/
sp<ImageStreamInfo>
PipelineDefaultImp::
createRawImageStreamInfo(
    char const*         streamName,
    StreamId_T          streamId,
    MUINT32             streamType,
    size_t              maxBufNum,
    size_t              minInitBufNum,
    MUINT               usageForAllocator,
    MINT                imgFormat,
    MSize const&        imgSize,
    size_t const        stride
)
{
    IImageStreamInfo::BufPlanes_t bufPlanes;
    //
#define addBufPlane(planes, height, stride)                                      \
        do{                                                                      \
            size_t _height = (size_t)(height);                                   \
            size_t _stride = (size_t)(stride);                                   \
            IImageStreamInfo::BufPlane bufPlane= { _height * _stride, _stride }; \
            planes.push_back(bufPlane);                                          \
        }while(0)
    switch( imgFormat ) {
        case eImgFmt_BAYER10:
        case eImgFmt_FG_BAYER10:
            addBufPlane(bufPlanes , imgSize.h, stride);
            break;
        default:
            MY_LOGE("format not support yet %p", imgFormat);
            break;
    }
#undef  addBufPlane

    sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
                streamName,
                streamId,
                streamType,
                maxBufNum, minInitBufNum,
                usageForAllocator, imgFormat, imgSize, bufPlanes
                );

    if( pStreamInfo == NULL ) {
        MY_LOGE("create ImageStream failed, %s, %#" PRIxPTR,
                streamName, streamId);
    }

    return pStreamInfo;
}


/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
getSensorOutputFmt(
    SensorStaticInfo const& sensorInfo,
    MUINT32 bitDepth,
    MBOOL isFull,
    MINT* pFmt
)
{
    MBOOL ret = MFALSE;
    // sensor fmt
#define case_Format( order_bit, mappedfmt, pFmt) \
        case order_bit:                          \
            (*(pFmt)) = mappedfmt;               \
        break;

    if( sensorInfo.sensorType == SENSOR_TYPE_YUV )
    {
        switch( sensorInfo.sensorFormatOrder )
        {
            case_Format( SENSOR_FORMAT_ORDER_UYVY, eImgFmt_UYVY, pFmt);
            case_Format( SENSOR_FORMAT_ORDER_VYUY, eImgFmt_VYUY, pFmt);
            case_Format( SENSOR_FORMAT_ORDER_YUYV, eImgFmt_YUY2, pFmt);
            case_Format( SENSOR_FORMAT_ORDER_YVYU, eImgFmt_YVYU, pFmt);
            default:
            MY_LOGE("formatOrder not supported, 0x%x", sensorInfo.sensorFormatOrder);
            goto lbExit;
            break;
        }
        MY_LOGD("sensortype:(0x%x), fmt(0x%x)", sensorInfo.sensorType, *pFmt);
    }
    else if( sensorInfo.sensorType == SENSOR_TYPE_RAW )
    {
        if( isFull ) //imgo
        {
            switch( bitDepth )
            {
                case_Format(  8, eImgFmt_BAYER8 , pFmt);
                case_Format( 10, eImgFmt_BAYER10, pFmt);
                case_Format( 12, eImgFmt_BAYER12, pFmt);
                case_Format( 14, eImgFmt_BAYER14, pFmt);
                default:
                MY_LOGE("bitdepth not supported, 0x%x", bitDepth);
                goto lbExit;
                break;
            }
        }
        else // rrzo
        {
            switch( bitDepth )
            {
                case_Format(  8, eImgFmt_FG_BAYER8 , pFmt);
                case_Format( 10, eImgFmt_FG_BAYER10, pFmt);
                case_Format( 12, eImgFmt_FG_BAYER12, pFmt);
                case_Format( 14, eImgFmt_FG_BAYER14, pFmt);
                default:
                MY_LOGE("bitdepth not supported, 0x%x", bitDepth);
                goto lbExit;
                break;
            }
        }
        MY_LOGD("sensortype: 0x%x, full(%d), fmt(0x%x), order(%d)",
                sensorInfo.sensorType, isFull, *pFmt, sensorInfo.sensorFormatOrder);
    }
    else
    {
        MY_LOGE("sensorType not supported yet(0x%x)", sensorInfo.sensorType);
        goto lbExit;
    }
    ret = MTRUE;
#undef case_Format

lbExit:
    return ret;
}
