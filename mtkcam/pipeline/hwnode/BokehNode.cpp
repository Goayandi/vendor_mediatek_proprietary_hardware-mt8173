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

#define LOG_TAG "MtkCam/BokehNode"
//
#include "hwnode_utilities.h"

#include <mtkcam/pipeline/hwnode/BokehNode.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include "BaseNode.h"
//
#include <utils/RWLock.h>
#include <utils/Thread.h>

// for EffectHal
#include <mtkcam/feature/effectHalBase/IEffectHal.h>
#include <mtkcam/feature/effectHalBase/EffectHalBase.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/feature/stereo/effecthal/BokehEffectHal.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>
// for metadata
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#include <mtkcam/feature/stereo/pipe/vsdof_common.h>
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>
//
#include <string>
#include <list>
#include <algorithm>
//
#include <chrono>
using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils::Sync;
using namespace StereoHAL;

/******************************************************************************
 *
 ******************************************************************************/
#define BOKEHTHREAD_NAME       ("Cam@Bokeh")
#define BOKEHTHREAD_POLICY     (SCHED_OTHER)
#define BOKEHTHREAD_PRIORITY   (0)

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

#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")

/******************************************************************************
 *
 ******************************************************************************/
static inline
MBOOL
isStream(sp<IStreamInfo> pStreamInfo, StreamId_T streamId )
{
    return pStreamInfo.get() && pStreamInfo->getStreamId() == streamId;
}
/******************************************************************************
 *
 ******************************************************************************/
template<typename T>
inline MBOOL GET_ENTRY_ARRAY(const IMetadata& metadata, MINT32 entry_tag, T* array, MUINT32 size)
{
    IMetadata::IEntry entry = metadata.entryFor(entry_tag);
    if (entry.tag() != IMetadata::IEntry::BAD_TAG &&
        entry.count() == size)
    {
        for (MUINT32 i = 0; i < size; i++)
        {
            *array++ = entry.itemAt(i, Type2Type< T >());
        }
        return MTRUE;
    }
    return MFALSE;
}
/******************************************************************************
 *
 ******************************************************************************/
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata *pMetadata,
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
inline MBOOL
is3DNROn(
    IMetadata* const inApp
)
{
    if (inApp == NULL) {
        return false;
    }
    MINT32 e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;
    if(!tryGetMetadata<MINT32>(inApp,
        MTK_NR_FEATURE_3DNR_MODE, e3DnrMode))
    {
        MY_LOGW_IF(1, "no MTK_NR_FEATURE_3DNR_MODE");
    }
    return e3DnrMode;
}
/******************************************************************************
 *
 ******************************************************************************/
    class process_frame
    {
        public:
            process_frame(
                        sp<IPipelineFrame> pFrame,
                        MUINT32 iFrameID,
                        MINT32 logLevel
                        ):
                        mpFrame(pFrame),
                        miFrameId(iFrameID),
                        mLogLevel(logLevel)
            {
            }
            ~process_frame()
            {
            }
            MBOOL                   init();
            MBOOL                   uninit(MBOOL vaild = MTRUE);
            MUINT32                 getScenario() {return scenario;}
            MUINT32                 getFrameID()  {return miFrameId;}
            MVOID                   setScenarioId(MUINT32 id)
                                    {
                                        scenario = id;
                                    }
            MVOID                   setGMVData(MBOOL enable, MINT32 x, MINT32 y)
                                    {
                                        mb3DNREnable = enable;
                                        miGMV_x = x;
                                        miGMV_y = y;
                                    }
            MVOID                   setAFTrigger(MINT8 mode)
                                    {
                                        miAFTrigger = mode;
                                    }
            MVOID                   setAFRegion(MINT32 x, MINT32 y)
                                    {
                                        miAFx = x;
                                        miAFy = y;
                                    }
            MVOID                   setBokehLevel(MINT32 val)
                                    {
                                        miBokehLevel = val;
                                    }
            MVOID                   setConvOffset(MFLOAT val)
                                    {
                                        mfConvOffset = val;
                                    }
            MBOOL                   is3DNREnable() {return mb3DNREnable;}
            MINT32                  getGMV_x()     {return miGMV_x;}
            MINT32                  getGMV_y()     {return miGMV_y;}
            MINT32                  getBokehLevel(){return miBokehLevel;}
            MINT32                  getAFTrigger(){return miAFTrigger;}
            MINT32                  getAFx(){return miAFx;}
            MINT32                  getAFy(){return miAFy;}
            MVOID                   setGSensor(MINT32 rot);
            MINT32                  getGSensor(){return miGSensorAngle;}
            MFLOAT                  getConvOffset(){return mfConvOffset;}
            MVOID                   setMagicNumber(MINT32 number) {miMagicNumber = number;}
            MINT32                  getMagicNumber(){return miMagicNumber;}
            MVOID                   unlockAllStream(const char* nodeName);
        private:
        public:
            sp<IPipelineFrame>          mpFrame                                 = NULL;
            DefaultKeyedVector<BokehEffectRequestBufferType, sp<IImageBuffer> >
                                        mvImgBufContainer;
            DefaultKeyedVector<BokehEffectRequestBufferType, sp<IImageStreamBuffer> >
                                        mvImgStreamBuffer;
            // Input: metadata
            IMetadata*                  mpInAppMetadata                         = NULL;
            sp<IMetaStreamBuffer>       mpInAppMetadataStreamBuffer             = NULL;
            IMetadata*                  mpInHalMetadata                         = NULL;
            sp<IMetaStreamBuffer>       mpInHalMetadataStreamBuffer             = NULL;

            IMetadata*                  mpInHalMetadata_P1                       = NULL;
            sp<IMetaStreamBuffer>       mpInHalMetadataStreamBuffer_P1           = NULL;
            // Output: metadata
            IMetadata*                  mpOutAppMetadata                         = NULL;
            sp<IMetaStreamBuffer>       mpOutAppMetadataStreamBuffer             = NULL;
            IMetadata*                  mpOutHalMetadata                         = NULL;
            sp<IMetaStreamBuffer>       mpOutHalMetadataStreamBuffer             = NULL;
            // store time to get processing time
            std::chrono::time_point<std::chrono::system_clock> start;
            std::chrono::time_point<std::chrono::system_clock> end;
            // StreamId in this pipeline frame
            DefaultKeyedVector<MUINT32, StreamId_T > mvStreamIdList;
        private:
            MUINT32                 scenario        = eSTEREO_SCENARIO_UNKNOWN;
            MBOOL                   mb3DNREnable     = MFALSE;
            MINT32                  miGMV_x         = 0;
            MINT32                  miGMV_y         = 0;
            MUINT32                 miFrameId       = 0;
            MINT32                  miBokehLevel    = 0;
            MINT32                  miAFx           = 0;
            MINT32                  miAFy           = 0;
            MUINT8                  miAFTrigger     = MTK_CONTROL_AF_TRIGGER_IDLE;
            MINT32                  mLogLevel       = -1;
            MINT32                  miGSensorAngle  = 0;
            MINT32                  miMagicNumber   = -1;
            MFLOAT                  mfConvOffset    = 0.0;
    };
/******************************************************************************
 *
 ******************************************************************************/
class BokehNodeImp
    : public BaseNode
    , public BokehNode
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                                            Definitions.
    typedef android::sp<IPipelineFrame>                     QueNode_T;
    typedef android::List<QueNode_T>                        Que_T;
protected:
    class BokehThread
        : public Thread
    {

    public:

                                    BokehThread(BokehNodeImp* pNodeImp)
                                        : mpNodeImp(pNodeImp)
                                    {}

                                    ~BokehThread()
                                    {}

    public:

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Thread Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    public:
                    // Ask this object's thread to exit. This function is asynchronous, when the
                    // function returns the thread might still be running. Of course, this
                    // function can be called from a different thread.
                    virtual void        requestExit();

                    // Good place to do one-time initializations
                    virtual status_t    readyToRun();

    private:
                    // Derived class must implement threadLoop(). The thread starts its life
                    // here. There are two ways of using the Thread object:
                    // 1) loop: if threadLoop() returns true, it will be called again if
                    //          requestExit() wasn't called.
                    // 2) once: if threadLoop() returns false, the thread will exit upon return.
                    virtual bool        threadLoop();

    private:

                    BokehNodeImp*       mpNodeImp          = NULL;

    };


private:
    //
public:     ////                    Operations.

                                    BokehNodeImp();

                                    ~BokehNodeImp();

    virtual MERROR                  config(ConfigParams const& rParams);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.

    virtual MERROR                  init(InitParams const& rParams);

    virtual MERROR                  uninit();

    virtual MERROR                  flush();

    virtual MERROR                  queue(
                                        android::sp<IPipelineFrame> pFrame
                                    );

protected:  ////                    Operations.

    MERROR                          onDequeRequest( //TODO: check frameNo
                                        android::sp<IPipelineFrame>& rpFrame
                                    );

    MVOID                           onProcessFrame(
                                        android::sp<IPipelineFrame> const& pFrame
                                    );

    MVOID                           waitForRequestDrained();


    MERROR                          getImageBufferAndLock(
                                        android::sp<IPipelineFrame> const& pFrame,
                                        StreamId_T const streamId,
                                        sp<IImageStreamBuffer>& rpStreamBuffer,
                                        sp<IImageBuffer>& rpImageBuffer,
                                        MBOOL const isInStream
                                    );

protected:

    MERROR                          threadSetting();
    MBOOL                           isInMetaStream(
                                        StreamId_T const streamId
                                    ) const;
    MVOID                           releaseMetaStream(
                                                      sp<IPipelineFrame> const& pFrame,
                                                      sp<IMetaStreamBuffer> const pStreamBuffer,
                                                      MUINT32 mask)const;
    //MVOID                           releaseImageBuffer(
    //                                                   sp<IImageStreamBuffer> const pStreamBuffer,
    //                                                   sp<IImageBuffer> const pImageBuffer)const;
    MVOID                           releaseImageStream(
                                                      sp<IPipelineFrame> const& pFrame,
                                                      sp<IImageStreamBuffer> const pStreamBuffer,
                                                      MUINT32 mask)const;
    MERROR                          suspendThisFrame(
                                                      sp<IPipelineFrame> const& pFrame,
                                                      process_frame*& data);
private:
    static MVOID                    EffectHalCb(MVOID* tag, String8 status, android::sp<NSCam::EffectRequest>& request);
    MVOID                           AddEffectRequest(process_frame *pFrameData);
    MBOOL                           queryGMVFromMetaStream(process_frame *pFrameData, IMetadata* const metaStream);
    MBOOL                           queryAFFromMetaStream(process_frame *pFrameData, IMetadata* const metaStream);
    MBOOL                           queryConvOffsetFromMetaStream(process_frame *pFrameData, IMetadata* const metaStream);
public:
    //sem_t                           semBokeh;
    MVOID                           onProcessEffectRequestSuccess(process_frame*& frame);
    MVOID                           onProcessEffectRequestFail(process_frame*& frame);
    MVOID                           releaseBuffer(process_frame*& frame, MUINT32 mask);

protected:  ////                    Data Members. (Request Queue)
    mutable RWLock                  mConfigRWLock;
    mutable Mutex                   mResultLock;
    // image
    sp<IImageStreamInfo>            mpInHalImageMain                = NULL;
    sp<IImageStreamInfo>            mpInHalImageMain_Capture        = NULL;
    //sp<IImageStreamInfo>            mpInHalImageMY_S                = NULL;
    sp<IImageStreamInfo>            mpInHalImageDMBG                 = NULL;
    //
    sp<IImageStreamInfo>            mpOutHalImageCleanImage        = NULL;
    sp<IImageStreamInfo>            mpOutHalImageBokehResult        = NULL;
    sp<IImageStreamInfo>            mpOutHalImageBokehThumbnail     = NULL;
    //
    sp<IImageStreamInfo>            mpOutAppImagePostView           = NULL;
    sp<IImageStreamInfo>            mpOutAppImageRecord             = NULL;
    sp<IImageStreamInfo>            mpOutAppImagePreview            = NULL;
    sp<IImageStreamInfo>            mpOutAppPrvCB                   = NULL;
    // metadata
    sp<IMetaStreamInfo>             mpInHalMetadata                 = NULL;
    sp<IMetaStreamInfo>             mpInHalMetadata_P1                 = NULL;
    sp<IMetaStreamInfo>             mpInAppMetadata                 = NULL;
    //
    sp<IMetaStreamInfo>             mpOutAppMetadata                = NULL;
    sp<IMetaStreamInfo>             mpOutHalMetadata                 = NULL;
    //
    mutable Mutex                   mRequestQueueLock;
    Condition                       mRequestQueueCond;
    Que_T                           mRequestQueue;
    MBOOL                           mbRequestDrained                = MFALSE;
    Condition                       mbRequestDrainedCond;
    MBOOL                           mbRequestExit                   = MFALSE;

private:   ////                     Threads
    sp<BokehThread>                 mpBokehThread                   = NULL;
    //
    // EffectHal
    BokehEffectHal                 *mpBokehEffectHal                = NULL;
    //
    // debug level
    MINT32                          mLogLevel                      = -1;
    //
    DefaultKeyedVector<StreamId_T, BokehEffectRequestBufferType>
                                    mvStreamIdMap;
    MUINT32                         miEffectRequestInCount          = 0;
    MUINT32                         miEffectRequestOutCount         = 0;
    //
    //MSize                           miMYSSize;
    MBOOL                           mbFirstRun = MTRUE;
    //
    MBOOL                           mbHasAfTrigger = MFALSE;
    MINT32                          miCacheAFx           = 0;
    MINT32                          miCacheAFy           = 0;
    std::list<MINT32>               mvNeededStream;
};


/******************************************************************************
 *
 ******************************************************************************/
android::sp<BokehNode>
BokehNode::
createInstance()
{
    MY_LOGD("createInstance");
    return new BokehNodeImp();
}


/******************************************************************************
 *
 ******************************************************************************/
BokehNodeImp::
BokehNodeImp()
    : BaseNode()
    , BokehNode()
    , mConfigRWLock()
    , mResultLock()
    , mRequestQueueLock()
{
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.camera.log", cLogLevel, "0");
    mLogLevel = atoi(cLogLevel);
    if ( mLogLevel == 0 ) {
        ::property_get("debug.camera.log.bokeh", cLogLevel, "0");
        mLogLevel = atoi(cLogLevel);
    }
    if ( 0 == mLogLevel ) {
        ::property_get("debug.camera.log.basenode", cLogLevel, "0");
        mLogLevel = ::atoi(cLogLevel);
    }
}


/******************************************************************************
 *
 ******************************************************************************/
BokehNodeImp::
~BokehNodeImp()
{

}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
init(InitParams const& rParams)
{
    FUNC_START;
    //
    mOpenId = rParams.openId;
    mNodeId = rParams.nodeId;
    mNodeName = rParams.nodeName;
    //
    MY_LOGD("OpenId %d, nodeId %d, name %s",
            getOpenId(), getNodeId(), getNodeName() );
    //
    mpBokehThread = new BokehThread(this);
    if( mpBokehThread->run(BOKEHTHREAD_NAME) != OK ) {
        return UNKNOWN_ERROR;
    }

    // create bokeh EffectHal
    mpBokehEffectHal = new BokehEffectHal();
    mpBokehEffectHal->init();
    sp<EffectParameter> effectParams = new EffectParameter();
    effectParams->set(BOKEH_EFFECT_REQUEST_OPEN_ID, getOpenId());
    effectParams->set(BOKEH_EFFECT_REQUEST_RUN_PATH, GENERAL);
    effectParams->set(VSDOF_FEAME_STOP_MODE, FLUSH);//VSDOF_STOP_MODE::FLUSH);
    mpBokehEffectHal->setParameters(effectParams);
    mpBokehEffectHal->configure();
    mpBokehEffectHal->prepare();
    //
    mvNeededStream.clear();
    //
    MY_LOGD("mLogLevel(%d)", mLogLevel);
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
config(ConfigParams const& rParams)
{
    FUNC_START;
    //
    flush();
    if(mvStreamIdMap.size()!=0)
        mvStreamIdMap.clear();
    //
    {
        // metadata: input
        mpInAppMetadata = rParams.pInAppMeta;
        mpInHalMetadata = rParams.pInHalMeta;
        mpInHalMetadata_P1 = rParams.pInHalMeta_P1;
        // metadata: output
        mpOutAppMetadata = rParams.pOutAppMeta;
        mpOutHalMetadata = rParams.pOutHalMeta;
        // imagebuffer stream: input
        mpInHalImageMain = rParams.pInHalImageMainImage;
        mpInHalImageMain_Capture = rParams.pInHalImageMainImage_Capture;
        //mpInHalImageMY_S = rParams.pInHalImageMYS;
        mpInHalImageDMBG = rParams.pInHalImageDMBG;
        // imagebuffer stream: output
        mpOutAppImagePostView = rParams.pOutAppImagePostView;
        mpOutHalImageCleanImage = rParams.pOutHalImageCleanImage;
        mpOutAppImageRecord = rParams.pOutAppImageRecord;
        mpOutAppImagePreview = rParams.pOutAppImagePreview;
        mpOutHalImageBokehResult = rParams.pOutHalImageBokehResult;
        mpOutHalImageBokehThumbnail = rParams.pOutHalImageBokehThumbnail;
        mpOutAppPrvCB = rParams.pOutAppPrvCB;
        /*if(mpInHalImageMY_S!=nullptr)
        {
            miMYSSize = mpInHalImageMY_S->getImgSize();
        }
        else
        {
            MY_LOGE("mpInHalImageMY_S is null!!!!");
        }*/
        // build StreamId map for effect request
#define BUILD_STREAMID_AND_REQUESTID_MAP(STREAMINFO, TYPE)\
        do{\
            if(STREAMINFO!=nullptr)\
            {\
                mvStreamIdMap.add(\
                        STREAMINFO->getStreamId(), TYPE);\
                MY_LOGD("key "#STREAMINFO" 0x%x value "#TYPE" %x", STREAMINFO->getStreamId(), TYPE);\
            }\
            else\
            {\
                MY_LOGD(#STREAMINFO " is null");\
            }\
        }while(0);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpInHalImageMain, BOKEH_ER_BUF_MAIN1);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpInHalImageMain_Capture, BOKEH_ER_BUF_MAIN1);
        //BUILD_STREAMID_AND_REQUESTID_MAP(mpInHalImageMY_S, BOKEH_ER_BUF_MYS);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpInHalImageDMBG, BOKEH_ER_BUF_DMBG);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpOutHalImageCleanImage, BOKEH_ER_BUF_CLEAN_IMG);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpOutAppImageRecord, BOKEH_ER_BUF_RECORD);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpOutAppImagePreview, BOKEH_ER_BUF_DISPLAY);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpOutHalImageBokehResult, BOKEH_ER_BUF_VSDOF_IMG);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpOutHalImageBokehThumbnail, BOKEH_ER_BUF_THUMBNAIL);
        BUILD_STREAMID_AND_REQUESTID_MAP(mpOutAppPrvCB, BOKEH_ER_BUF_PRVCB);
#undef BUILD_STREAMID_AND_REQUESTID_MAP
    }

/******************************************************************************
 *
 ******************************************************************************/
auto dumpStreamInfo = [](const sp<IImageStreamInfo>& info)
    {
    if(info == nullptr)
    {
        return;
    }
    MY_LOGD("streamInfo(%#08x) name(%s) format(%#08x) size(%dx%d)",
            info->getStreamId(), info->getStreamName(),
            info->getImgFormat(), info->getImgSize().w, info->getImgSize().h);
};
    //
    dumpStreamInfo(mpInHalImageMain);
    //dumpStreamInfo(mpInHalImageMY_S);
    dumpStreamInfo(mpInHalImageDMBG);
    dumpStreamInfo(mpOutAppImagePostView);
    dumpStreamInfo(mpOutHalImageCleanImage);
    dumpStreamInfo(mpOutAppImageRecord);
    dumpStreamInfo(mpOutAppImagePreview);
    dumpStreamInfo(mpOutHalImageBokehResult);
    dumpStreamInfo(mpInHalImageMain_Capture);
    dumpStreamInfo(mpOutHalImageBokehThumbnail);
    dumpStreamInfo(mpOutAppPrvCB);
    //
    mpBokehEffectHal->start();
    //
    {
        auto addToNeedStream =[this](sp<IImageStreamInfo> imgBuffer)
        {
            if(imgBuffer!=0)
            {
                mvNeededStream.push_back(imgBuffer->getStreamId());
            }
            return;
        };
        addToNeedStream(mpInHalImageMain);
        //addToNeedStream(mpInHalImageMY_S);
        addToNeedStream(mpInHalImageDMBG);
        addToNeedStream(mpOutAppImagePostView);
        addToNeedStream(mpOutHalImageCleanImage);
        addToNeedStream(mpOutAppImageRecord);
        addToNeedStream(mpOutAppImagePreview);
        addToNeedStream(mpOutHalImageBokehResult);
        addToNeedStream(mpInHalImageMain_Capture);
        addToNeedStream(mpOutHalImageBokehThumbnail);
    }
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
uninit()
{
    FUNC_START;
    //
    if ( OK != flush() )
        MY_LOGE("flush failed");
    //
    // exit threads
    mpBokehThread->requestExit();
    // join
    mpBokehThread->join();
    //
    mpBokehThread = NULL;
    //
    //
    // release effectHal
    if(NULL != mpBokehEffectHal)
    {
        mpBokehEffectHal->abort();
        mpBokehEffectHal->release();
        mpBokehEffectHal->unconfigure();
        mpBokehEffectHal->uninit();
        delete mpBokehEffectHal;
        mpBokehEffectHal = NULL;
        MY_LOGD("Destroy bokeh effectHal.");
    }
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
flush()
{
    FUNC_START;
    //
    // 1. clear requests
    {
        Mutex::Autolock _l(mRequestQueueLock);
        //
        Que_T::iterator it = mRequestQueue.begin();
        while ( it != mRequestQueue.end() ) {
            BaseNode::flush(*it);
            it = mRequestQueue.erase(it);
        }
    }
    //
    // 2. clear effect hal request
    if(mpBokehEffectHal != nullptr)
    {
        mpBokehEffectHal->flush();
    }
    //
    // 3. wait enque thread
    waitForRequestDrained();

    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
queue(android::sp<IPipelineFrame> pFrame)
{
    FUNC_START;
    //
    if( ! pFrame.get() ) {
        MY_LOGE("Null frame");
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mRequestQueueLock);

    //  Make sure the request with a smaller frame number has a higher priority.
    Que_T::iterator it = mRequestQueue.end();
    for (; it != mRequestQueue.begin(); ) {
        --it;
        if  ( 0 <= (MINT32)(pFrame->getFrameNo() - (*it)->getFrameNo()) ) {
            ++it;   //insert(): insert before the current node
            break;
        }
    }

    mRequestQueue.insert(it, pFrame);

    mRequestQueueCond.signal();
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
onDequeRequest(
    android::sp<IPipelineFrame>& rpFrame
)
{
    FUNC_START;
    //
    Mutex::Autolock _l(mRequestQueueLock);
    //
    //  Wait until the queue is not empty or not going exit
    while ( mRequestQueue.empty() && ! mbRequestExit )
    {
        // set dained flag
        mbRequestDrained = MTRUE;
        mbRequestDrainedCond.signal();
        //
        status_t status = mRequestQueueCond.wait(mRequestQueueLock);
        if  ( OK != status ) {
            MY_LOGW_IF(mLogLevel,
                "wait status:%d:%s, mRequestQueue.size:%zu",
                status, ::strerror(-status), mRequestQueue.size()
            );
        }
    }
    //
    if  ( mbRequestExit ) {
        MY_LOGW_IF(!mRequestQueue.empty(), "[flush] mRequestQueue.size:%zu", mRequestQueue.size());
        return DEAD_OBJECT;
    }
    //
    //  Here the queue is not empty, take the first request from the queue.
    mbRequestDrained = MFALSE;
    rpFrame = *mRequestQueue.begin();
    mRequestQueue.erase(mRequestQueue.begin());
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
onProcessFrame(
    android::sp<IPipelineFrame> const& pFrame
)
{
    //FUNC_START;
    //
    IPipelineFrame::InfoIOMapSet IOMapSet;

    IStreamBufferSet& streamBufferSet = pFrame->getStreamBufferSet();
    //
    process_frame* pProcessFrame = NULL;

    if(OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet )) {
        MY_LOGE("Get IOMapSet failed.");
        return;
    }
    //
    pProcessFrame = new process_frame(pFrame, pFrame->getRequestNo(), mLogLevel);
    pProcessFrame->init();
    // Process InHalMeta and InHalMeta.
    {
        // Get GMV data and AF info
        // 1. Check if NR3D is on.
        // 2. Get GMV data from Hal metadata.
        // 3. Get AF data from Hal metadata.
        MBOOL b3DNR = MFALSE;
        // Step 1. -----------------------------------------------------------------------------------
        if(OK==ensureMetaBufferAvailable_(
                                        pFrame->getFrameNo(),
                                        mpInAppMetadata->getStreamId(),
                                        streamBufferSet,
                                        pProcessFrame->mpInAppMetadataStreamBuffer))
        {
            pProcessFrame->mpInAppMetadata = pProcessFrame->mpInAppMetadataStreamBuffer->tryReadLock(getNodeName());
            // get bokeh level
            MINT32 tempBokehLevel = 0;
            if(!tryGetMetadata<MINT32>(pProcessFrame->mpInAppMetadata, MTK_STEREO_FEATURE_DOF_LEVEL, tempBokehLevel))
            {
                MY_LOGE("Get bokeh level fail.");
            }
            pProcessFrame->setBokehLevel(tempBokehLevel);
            //
            b3DNR = is3DNROn(pProcessFrame->mpInAppMetadata);
            //MY_LOGD("b3DNR(%d)", b3DNR);
        }
        else
        {
            MY_LOGE("(InAppMetadata) ensureMetaBufferAvailable_");
            suspendThisFrame(pFrame, pProcessFrame);
            return;
        }
        if(OK==ensureMetaBufferAvailable_(pFrame->getFrameNo(),
                                          mpInHalMetadata->getStreamId(),
                                          streamBufferSet,
                                          pProcessFrame->mpInHalMetadataStreamBuffer))
        {
            pProcessFrame->mpInHalMetadata = isInMetaStream(mpInHalMetadata->getStreamId()) ?
                                      pProcessFrame->mpInHalMetadataStreamBuffer->tryReadLock(getNodeName()) :
                                      pProcessFrame->mpInHalMetadataStreamBuffer->tryWriteLock(getNodeName());
        }
        else
        {
            MY_LOGE("(InHalMetadata) ensureMetaBufferAvailable_");
            suspendThisFrame(pFrame, pProcessFrame);
            return;
        }
        if(OK==ensureMetaBufferAvailable_(pFrame->getFrameNo(),
                                          mpInHalMetadata_P1->getStreamId(),
                                          streamBufferSet,
                                          pProcessFrame->mpInHalMetadataStreamBuffer_P1))
        {
            pProcessFrame->mpInHalMetadata_P1 = isInMetaStream(mpInHalMetadata_P1->getStreamId()) ?
                                      pProcessFrame->mpInHalMetadataStreamBuffer_P1->tryReadLock(getNodeName()) :
                                      pProcessFrame->mpInHalMetadataStreamBuffer_P1->tryWriteLock(getNodeName());
        }
        else
        {
            MY_LOGE("(InHalMetadata) ensureMetaBufferAvailable_");
            suspendThisFrame(pFrame, pProcessFrame);
            return;
        }
        // Step 2. -----------------------------------------------------------------------------------
        if(b3DNR)
        {
            queryGMVFromMetaStream(pProcessFrame, pProcessFrame->mpInHalMetadata_P1);
        }
        // Step 3. -----------------------------------------------------------------------------------
        //queryAFFromMetaStream(pProcessFrame, pProcessFrame->mpInAppMetadata);
        // get magic number
        MINT32 tempMagicNumber = 0;
        if(!tryGetMetadata<MINT32>(pProcessFrame->mpInHalMetadata_P1, MTK_P1NODE_PROCESSOR_MAGICNUM, tempMagicNumber))
        {
            MY_LOGE("Get magic number fail.");
        }
        pProcessFrame->setMagicNumber(tempMagicNumber);

        // get Convergence offset
        queryConvOffsetFromMetaStream(pProcessFrame, pProcessFrame->mpInHalMetadata);
        // -------------------------------------------------------------------------------------------
    }
    // Handle output metadata
    {
        if(OK != ensureMetaBufferAvailable_(
                                    pFrame->getFrameNo(),
                                    mpOutAppMetadata->getStreamId(),
                                    streamBufferSet,
                                    pProcessFrame->mpOutAppMetadataStreamBuffer))
        {
            MY_LOGE("(OutAppMetadata) ensureMetaBufferAvailable_");
            suspendThisFrame(pFrame, pProcessFrame);
            return;
        }
        pProcessFrame->mpOutAppMetadata = pProcessFrame->mpOutAppMetadataStreamBuffer->tryWriteLock(getNodeName());
        if(OK != ensureMetaBufferAvailable_(
                                    pFrame->getFrameNo(),
                                    mpOutHalMetadata->getStreamId(),
                                    streamBufferSet,
                                    pProcessFrame->mpOutHalMetadataStreamBuffer))
        {
            MY_LOGE("(OutHalMetadata) ensureMetaBufferAvailable_");
            suspendThisFrame(pFrame, pProcessFrame);
            return;
        }
        pProcessFrame->mpOutHalMetadata = pProcessFrame->mpOutHalMetadataStreamBuffer->tryWriteLock(getNodeName());
    }
    //
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = IOMapSet.mImageInfoIOMapSet[0];
    {
        StreamId_T streamId = 0;
        MINT8 scenario = eSTEREO_SCENARIO_UNKNOWN;
        sp<IImageStreamBuffer> pStreamBuffer = nullptr;
        sp<IImageBuffer> pImgBuffer = nullptr;
        auto checkIsNeededStream=[this](MINT32 streamid)
        {
            return (std::find(mvNeededStream.begin(), mvNeededStream.end(), streamid) != mvNeededStream.end());
        };

        for( size_t i=0; i<imageIOMap.vOut.size() ; ++i )
        {
            streamId = imageIOMap.vOut.keyAt(i);
            if(isStream(this->mpOutHalImageCleanImage, streamId))
            {
                MY_LOGD("capture frame(%d)", pFrame->getRequestNo());
                scenario = eSTEREO_SCENARIO_CAPTURE;
                // get g sensor
                MINT32 tempGsensor = 0;
                if(!tryGetMetadata<MINT32>(pProcessFrame->mpInAppMetadata, MTK_JPEG_ORIENTATION, tempGsensor))
                {
                    MY_LOGE("Get g sensor fail.");
                }
                pProcessFrame->setGSensor(tempGsensor);
            }
            else if(isStream(this->mpOutAppImageRecord, streamId))
            {
                scenario = eSTEREO_SCENARIO_RECORD;
            }
            // get output buffer
            auto index = mvStreamIdMap.indexOfKey(streamId);
            auto bufIndex = mvStreamIdMap.valueAt(index);
            MY_LOGD_IF(mLogLevel>0, "out streamid(0x%x) bufIndex(%x)", streamId, bufIndex);
            if(index >= 0 && bufIndex >= 0)
            {
                MERROR const err1 = getImageBufferAndLock(
                            pFrame,
                            streamId,
                            pStreamBuffer,//pProcessFrame->mvImgStreamBuffer[it->second],
                            pImgBuffer, //pProcessFrame->mvImgBufContainer[it->second],
                            MFALSE
                            );

                if( err1 != OK && checkIsNeededStream(streamId)) {
                    MY_LOGE("getImageBufferAndLock err = %d StreamId(%d)", err1, streamId);
                    suspendThisFrame(pFrame, pProcessFrame);
                    return;
                }
                auto bufKey = pProcessFrame->mvImgStreamBuffer.indexOfKey(bufIndex);
                pProcessFrame->mvImgStreamBuffer.replaceValueAt(bufKey, pStreamBuffer);
                pProcessFrame->mvImgBufContainer.replaceValueAt(bufKey, pImgBuffer);
                if(mLogLevel>0)
                {
                    MY_LOGD("Lock out buffer streamId(%d) imgBuf(0x%x) streamBuf(0x%x)",
                            streamId,
                            pProcessFrame->mvImgStreamBuffer.valueAt(bufKey).get(),
                            pProcessFrame->mvImgBufContainer.valueAt(bufKey).get());
                }
            }
            else
            {
                MY_LOGE("cannot find stream in StreamIdMap(%d)", streamId);
            }
        }
        //
        if(scenario == eSTEREO_SCENARIO_UNKNOWN)
        {
            scenario = eSTEREO_SCENARIO_PREVIEW;
        }
        pProcessFrame->setScenarioId(scenario);
        //
        for( size_t i=0; i<imageIOMap.vIn.size() ; ++i )
        {
            streamId = imageIOMap.vIn.keyAt(i);
            auto index = mvStreamIdMap.indexOfKey(streamId);
            auto bufIndex = mvStreamIdMap.valueAt(index);
            MY_LOGD_IF(mLogLevel>0, "in streamid(0x%x) bufIndex(%x)", streamId, bufIndex);
            if(index >= 0 && bufIndex >= 0)
            {
                MERROR const err1 = getImageBufferAndLock(
                            pFrame,
                            streamId,
                            pStreamBuffer,//pProcessFrame->mvImgStreamBuffer[it->second],
                            pImgBuffer, //pProcessFrame->mvImgBufContainer[it->second],
                            MFALSE
                            );

                if( err1 != OK && checkIsNeededStream(streamId)) {
                    MY_LOGE("getImageBufferAndLock err = %d StreamId(%d)", err1, streamId);
                    suspendThisFrame(pFrame, pProcessFrame);
                    return;
                }
                auto bufKey = pProcessFrame->mvImgStreamBuffer.indexOfKey(bufIndex);
                pProcessFrame->mvImgStreamBuffer.replaceValueAt(bufKey, pStreamBuffer);
                pProcessFrame->mvImgBufContainer.replaceValueAt(bufKey, pImgBuffer);
                if(mLogLevel>0)
                {
                    MY_LOGD("Lock in buffer streamId(%d) imgBuf(0x%x) streamBuf(0x%x)",
                        streamId,
                        pProcessFrame->mvImgStreamBuffer.valueAt(bufKey).get(),
                        pProcessFrame->mvImgBufContainer.valueAt(bufKey).get());
                }
            }
            else
            {
                MY_LOGE("cannot find stream in StreamIdMap(%d)", streamId);
            }
        }
        if(eSTEREO_SCENARIO_CAPTURE==scenario)
        {
            // replace APP out metadata
            if(pProcessFrame->mpInAppMetadata!=nullptr &&
                pProcessFrame->mpOutAppMetadata!=nullptr)
            {
                *pProcessFrame->mpOutAppMetadata = *pProcessFrame->mpInAppMetadata;
                // JPS always does not rotate.
                IMetadata::IEntry entry(MTK_JPEG_ORIENTATION);
                entry.push_back(0, Type2Type<MINT32>());
                pProcessFrame->mpOutAppMetadata->update(MTK_JPEG_ORIENTATION, entry);
                //
            }
        }
    }
    // process af cache
    {
        if(mbHasAfTrigger)
        {
            pProcessFrame->setAFTrigger(MTRUE);
            pProcessFrame->setAFRegion(miCacheAFx, miCacheAFy);
            mbHasAfTrigger = MFALSE;
        }
    }
    AddEffectRequest(pProcessFrame);
    //sem_wait(&semBokeh);

    //FUNC_END;
    return;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
waitForRequestDrained()
{
    FUNC_START;
    //
    Mutex::Autolock _l(mRequestQueueLock);
    if( !mbRequestDrained ) {
        MY_LOGD("wait for request drained");
        MY_LOGD("mbRequestDrained(%d), miEffectRequestInCount(%d) miEffectRequestOutCount(%d)",
                mbRequestDrained,
                miEffectRequestInCount,
                miEffectRequestOutCount);
        mbRequestDrainedCond.wait(mRequestQueueLock);
    }
    //
    FUNC_END;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
getImageBufferAndLock(
    android::sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    sp<IImageStreamBuffer>& rpStreamBuffer,
    sp<IImageBuffer>& rpImageBuffer,
    MBOOL const isInStream
)
{
    IStreamBufferSet&      rStreamBufferSet = pFrame->getStreamBufferSet();
    sp<IImageBufferHeap>   pImageBufferHeap = NULL;
    MY_LOGD_IF(mLogLevel>0, "CKH: RequestNo(%d) StreamId(%d)", pFrame->getRequestNo(), streamId);
    if(streamId == 0)
    {
        MY_LOGE("StreamId is 0");
        return BAD_VALUE;
    }
    MERROR const err = ensureImageBufferAvailable_(
            pFrame->getFrameNo(),
            streamId,
            rStreamBufferSet,
            rpStreamBuffer
            );

    if( err != OK )
        return err;
    //
    //  Query the group usage.
    MUINT const groupUsage = rpStreamBuffer->queryGroupUsage(getNodeId());

    if(isInStream){
        pImageBufferHeap = rpStreamBuffer->tryReadLock(getNodeName());
    }
    else{
        pImageBufferHeap = rpStreamBuffer->tryWriteLock(getNodeName());
    }

    if (pImageBufferHeap == NULL) {
        MY_LOGE("pImageBufferHeap == NULL");
        return BAD_VALUE;
    }
    MY_LOGD_IF(mLogLevel>0, "@pImageBufferHeap->getBufSizeInBytes(0) = %d", pImageBufferHeap->getBufSizeInBytes(0));
    rpImageBuffer = pImageBufferHeap->createImageBuffer();

    if (rpImageBuffer == NULL) {
        MY_LOGE("rpImageBuffer == NULL");
        return BAD_VALUE;
    }
    rpImageBuffer->lockBuf(getNodeName(), groupUsage);

    MY_LOGD_IF(mLogLevel>0, "stream buffer: (%p) %p, heap: %p, buffer: %p, usage: %p",
        streamId, rpStreamBuffer.get(), pImageBufferHeap.get(), rpImageBuffer.get(), groupUsage);

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
threadSetting()
{
    //
    //  thread policy & priority
    //  Notes:
    //      Even if pthread_create() with SCHED_OTHER policy, a newly-created thread
    //      may inherit the non-SCHED_OTHER policy & priority of the thread creator.
    //      And thus, we must set the expected policy & priority after a thread creation.
    MINT tid;
    struct sched_param sched_p;
    ::sched_getparam(0, &sched_p);
    if (BOKEHTHREAD_POLICY == SCHED_OTHER) {
        sched_p.sched_priority = 0;
        ::sched_setscheduler(0, BOKEHTHREAD_POLICY, &sched_p);
        ::setpriority(PRIO_PROCESS, 0, BOKEHTHREAD_PRIORITY);   //  Note: "priority" is nice value.
    } else {
        sched_p.sched_priority = BOKEHTHREAD_PRIORITY;          //  Note: "priority" is real-time priority.
        ::sched_setscheduler(0, BOKEHTHREAD_POLICY, &sched_p);
    }

    MY_LOGD("tid(%d) policy(%d) priority(%d)", ::gettid(), BOKEHTHREAD_POLICY, BOKEHTHREAD_PRIORITY);

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
EffectHalCb(MVOID* tag, String8 status, sp<NSCam::EffectRequest>& request)
{
    BokehNodeImp *_BokehNode = reinterpret_cast<BokehNodeImp*>(tag);
    //
    const sp<EffectParameter> params = request->getRequestParameter();
    process_frame *frame = reinterpret_cast<process_frame*>(params->getPtr(VSDOF_FRAME_USER_DATA));
    //
    if(status == String8("Done"))
    {
        _BokehNode->onProcessEffectRequestSuccess(frame);
    }
    else
    {
        MY_LOGD("status(%s)", status.string());
        _BokehNode->onProcessEffectRequestFail(frame);
    }
    //sem_post(&_BokehNode->semBokeh);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
onProcessEffectRequestFail(process_frame*& frame)
{
    if(frame!=nullptr)
    {
        MY_LOGD_IF(mLogLevel>0, "+ status(fail) requestId(%d)", frame->getFrameID());
        //
        // add metadata for test
        if(frame->mpOutAppMetadata!=nullptr)
        {
            IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
            entry.push_back(MSize(2560, 1440), Type2Type< MSize >());
            frame->mpOutAppMetadata->update(entry.tag(), entry);
        }
        frame->unlockAllStream(getNodeName());
        // release image buffer/ metadata buffer
        releaseBuffer(frame, STREAM_BUFFER_STATUS::WRITE_ERROR);
        // release
        sp<IPipelineFrame> const pFrame = frame->mpFrame;
        if(frame->mpFrame == 0)
        {
            MY_LOGE("mpFrame is null");
        }
        IStreamBufferSet& streamBufferSet = pFrame->getStreamBufferSet();
        streamBufferSet.applyRelease(getNodeId());
        //
        frame->uninit();
        delete frame;
        frame = nullptr;
        onDispatchFrame(pFrame);
        miEffectRequestOutCount++;
        if(mbRequestDrained && (miEffectRequestInCount == miEffectRequestOutCount))
        {
            mbRequestDrainedCond.signal();
        }
    }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
onProcessEffectRequestSuccess(process_frame*& frame)
{
    if(mLogLevel==28285)
    {
        MY_LOGD("Dump image +");
        NSCam::Utils::makePath("/sdcard/bokeh/",0660);
        std::string filename = "/sdcard/bokeh/bokeh_"+std::to_string(frame->getFrameID())+".yuv";
        sp<IImageBuffer> pBuf = nullptr;
        for(ssize_t i=0;i<frame->mvImgBufContainer.size();++i)
        {
            pBuf = frame->mvImgBufContainer.valueAt(i);
            if(pBuf!=nullptr)
            {
                pBuf->saveToFile(filename.c_str());
            }
        }
        MY_LOGD("Dump image -");
    }
    // get process_frame
    if(frame!=nullptr)
    {
        MY_LOGD_IF(mLogLevel>0, "+ status(done) requestId(%d)", frame->getFrameID());
        //
        // add metadata for test
        if(frame->mpOutAppMetadata!=nullptr)
        {
            IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
            entry.push_back(MSize(2560, 1440), Type2Type< MSize >());
            frame->mpOutAppMetadata->update(entry.tag(), entry);
        }
        frame->unlockAllStream(getNodeName());
        // release image buffer/ metadata buffer
        releaseBuffer(frame, STREAM_BUFFER_STATUS::WRITE_OK);
        // release
        sp<IPipelineFrame> const pFrame = frame->mpFrame;
        if(frame->mpFrame == 0)
        {
            MY_LOGE("mpFrame is null");
        }
        IStreamBufferSet& streamBufferSet = pFrame->getStreamBufferSet();
        streamBufferSet.applyRelease(getNodeId());
        //
        frame->uninit();
        delete frame;
        frame = nullptr;
        onDispatchFrame(pFrame);
        miEffectRequestOutCount++;
        if(mbRequestDrained && (miEffectRequestInCount == miEffectRequestOutCount))
        {
            mbRequestDrainedCond.signal();
        }
    }
    else
    {
        MY_LOGE_IF(frame==nullptr, "frame is NULL.");
    }
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
releaseBuffer(
    process_frame*& frame,
    MUINT32 mask
)
{
        // release metadata
        if(frame->mpInHalMetadataStreamBuffer!=nullptr)
        {
            releaseMetaStream(
                                frame->mpFrame,
                                frame->mpInHalMetadataStreamBuffer,
                                mask
                        );
        }
        if(frame->mpInHalMetadataStreamBuffer_P1!=nullptr)
        {
            releaseMetaStream(
                                frame->mpFrame,
                                frame->mpInHalMetadataStreamBuffer_P1,
                                mask
                        );
        }
        //
        if(frame->mpInAppMetadataStreamBuffer!=nullptr)
        {
            releaseMetaStream(
                                frame->mpFrame,
                                frame->mpInAppMetadataStreamBuffer,
                                mask
                        );
        }
        //
        if(frame->mpOutHalMetadataStreamBuffer!=nullptr)
        {
            releaseMetaStream(
                                frame->mpFrame,
                                frame->mpOutHalMetadataStreamBuffer,
                                mask
                        );
        }
        //
        if(frame->mpOutAppMetadataStreamBuffer!=nullptr)
        {
            releaseMetaStream(
                                frame->mpFrame,
                                frame->mpOutAppMetadataStreamBuffer,
                                mask
                        );
        }
        // release image buffer
        if(frame->mvImgBufContainer.size() !=
           frame->mvImgStreamBuffer.size())
        {
            MY_LOGE("mvImgBufContainer.size()!=mvImgStreamBuffer.size()");
            return;
        }
        MINT32 index = -1;
        sp<IImageBuffer> imgBuffer;
        sp<IImageStreamBuffer> imgStreamBuffer;
        for(ssize_t i=0; i<frame->mvImgBufContainer.size();++i)
        {
            imgBuffer = frame->mvImgBufContainer.editValueAt(i);
            imgStreamBuffer = frame->mvImgStreamBuffer.editValueAt(i);
            if(imgBuffer != nullptr && imgStreamBuffer != nullptr)
            {
                if(frame->mpFrame == 0)
                {
                    MY_LOGE("mpFrame is null");
                }
                //releaseImageBuffer(imgStreamBuffer,
                //                   imgBuffer);
                releaseImageStream(frame->mpFrame,
                                   imgStreamBuffer,
                                   mask);
                MY_LOGD_IF(mLogLevel>0, "Release buf(0x%x)", frame->mvImgStreamBuffer.keyAt(i));
            }
        }
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
BokehNodeImp::
isInMetaStream(
    StreamId_T const streamId
) const
{
    RWLock::AutoRLock _l(mConfigRWLock);
    return isStream(mpInHalMetadata, streamId) ||
           isStream(mpInHalMetadata_P1, streamId) ||
           isStream(mpInAppMetadata, streamId);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
releaseMetaStream(
                  sp<IPipelineFrame> const& pFrame,
                  sp<IMetaStreamBuffer> const pStreamBuffer,
                  MUINT32 mask) const
{
    IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();
    StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();
    //
    if( pStreamBuffer.get() == NULL ) {
        MY_LOGE("StreamId %d: pStreamBuffer == NULL",
                streamId);
        return;
    }
    //
    if( !isInMetaStream(streamId) ) {
        pStreamBuffer->markStatus(mask);
    }
    //
    //  Mark this buffer as USED by this user.
    //  Mark this buffer as RELEASE by this user.
    rStreamBufferSet.markUserStatus(
            streamId, getNodeId(),
            IUsersManager::UserStatus::USED |
            IUsersManager::UserStatus::RELEASE
            );
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
releaseImageStream(
                  sp<IPipelineFrame> const& pFrame,
                  sp<IImageStreamBuffer> const pStreamBuffer,
                  MUINT32 mask) const
{
    IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();
    StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();
    //
    if( pStreamBuffer.get() == NULL ) {
        MY_LOGE("StreamId %d: pStreamBuffer == NULL",
                streamId);
        return;
    }
    //
    if( !isInMetaStream(streamId) ) {
        pStreamBuffer->markStatus(mask);
    }
    //
    //  Mark this buffer as USED by this user.
    //  Mark this buffer as RELEASE by this user.
    rStreamBufferSet.markUserStatus(
            streamId, getNodeId(),
            IUsersManager::UserStatus::USED |
            IUsersManager::UserStatus::RELEASE
            );
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
BokehNodeImp::
suspendThisFrame(
    sp<IPipelineFrame> const& pFrame,
    process_frame*& data)
{
    MY_LOGE("Discard request id=%d", pFrame->getRequestNo());

    data->unlockAllStream(getNodeName());
    IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();

    auto markStreamBufferStatusToError = [&rStreamBufferSet](MUINT32 streamId, MUINT32 nodeId)
    {
        sp<IImageStreamBuffer> pStreamBuffer = rStreamBufferSet.getImageBuffer(streamId, nodeId);
        if(pStreamBuffer!=nullptr)
        {
            pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::ERROR);
        }
        /*sp<IImageBuffer> imgBuffer;
        sp<IImageStreamBuffer> imgStreamBuffer;
        ssize_t index = data->mvImgBufContainer.indexOfKey(frameId);
        if(index >= 0)
        {
            imgBuffer = data->mvImgBufContainer.editValueAt(index);
            imgStreamBuffer = data->mvImgStreamBuffer.editValueAt(index);
            if(imgStreamBuffer != nullptr)
            {
                imgStreamBuffer->markStatus(STREAM_BUFFER_STATUS::ERROR);
            }
        }*/
    };
    // mark preview or record to ERROR to avoid empty frame(green screen)
    if(mpOutAppImagePreview!=nullptr)
    {
        markStreamBufferStatusToError(mpOutAppImagePreview->getStreamId(), getNodeId());
    }
    if(mpOutAppImageRecord!=nullptr)
    {
        markStreamBufferStatusToError(mpOutAppImageRecord->getStreamId(), getNodeId());
    }
    data->uninit(MFALSE);
    delete data;
    data = nullptr;

    // mark input/output buffer to release
    MERROR err = BaseNode::flush(pFrame);
    return err;
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
BokehNodeImp::
AddEffectRequest(process_frame *pFrameData)
{
    if(pFrameData == nullptr)
    {
        MY_LOGE("pFrameData is NULL.");
        return;
    }
    android::sp<NSCam::EffectRequest> request = new NSCam::EffectRequest(pFrameData->getFrameID(), EffectHalCb, this);
    //
    MUINT32 scenarioId = pFrameData->getScenario();
    // set request params.
    sp<EffectParameter> reqst_para = new EffectParameter();
    reqst_para->set(VSDOF_FRAME_SCENARIO, scenarioId);
    // to do: if 3dnr is off, does not set value.
    reqst_para->set(VSDOF_FRAME_3DNR_FLAG, pFrameData->is3DNREnable());
    reqst_para->set(VSDOF_FRAME_GMV_X, pFrameData->getGMV_x());
    reqst_para->set(VSDOF_FRAME_GMV_Y, pFrameData->getGMV_y());
    reqst_para->setPtr(VSDOF_FRAME_USER_DATA, pFrameData);
    reqst_para->set(VSDOF_FRAME_BOKEH_LEVEL, pFrameData->getBokehLevel());
    reqst_para->set(VSDOF_FRAME_AF_TRIGGER_FLAG, pFrameData->getAFTrigger());
    reqst_para->set(VSDOF_FRAME_AF_POINT_X, pFrameData->getAFx());
    reqst_para->set(VSDOF_FRAME_AF_POINT_Y, pFrameData->getAFy());
    reqst_para->set(VSDOF_FEAME_MAGIC_NUMBER, pFrameData->getMagicNumber());
    reqst_para->setFloat(VSDOF_FRAME_CONV_OFFSET, pFrameData->getConvOffset());
    if(scenarioId == eSTEREO_SCENARIO_CAPTURE)
    {
        reqst_para->set(VSDOF_FRAME_G_SENSOR_ORIENTATION, pFrameData->getGSensor());
        MY_LOGD("G sensor(%d)", pFrameData->getGSensor());
    }
    else
    {
        reqst_para->set(VSDOF_FRAME_G_SENSOR_ORIENTATION, 0);
    }
    // add metadata
    //
    request->setRequestParameter(reqst_para);
    //
    sp<EffectFrameInfo>  frame = nullptr;
    // set input frame
    // main1 image buffer
#define SET_EFFECT_FRAME(FRAMEID, TYPE)\
    do{\
        MINT32 frameIndex = pFrameData->mvImgBufContainer.indexOfKey(FRAMEID);\
        if(frameIndex>=0)\
        {\
            sp<IImageBuffer> imgBuf = pFrameData->mvImgBufContainer.valueAt(frameIndex);\
            if(imgBuf!=0)\
            {\
                frame = new EffectFrameInfo(pFrameData->getFrameID(), FRAMEID, nullptr, this);\
                frame->setFrameBuffer(pFrameData->mvImgBufContainer.valueAt(frameIndex));\
                request->v##TYPE##FrameInfo.add(FRAMEID, frame);\
                if(mLogLevel>0) MY_LOGD("add "#TYPE" "#FRAMEID);\
            }\
        }\
    }while(0);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_MAIN1, Input);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_DMBG, Input);
    //SET_EFFECT_FRAME(BOKEH_ER_BUF_MYS, Input);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_DISPLAY, Output);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_CLEAN_IMG, Output);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_RECORD, Output);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_VSDOF_IMG, Output);
    SET_EFFECT_FRAME(BOKEH_ER_BUF_THUMBNAIL, Output);
#undef SET_EFFECT_FRAME
    //
    // Dump request debug info
    if(mLogLevel>0)
    {
        MY_LOGD("Dump BokehRequest +");
        MY_LOGD("FrameNo(%d)", pFrameData->getFrameID());
        MY_LOGD("ScenarioId(%d)", scenarioId);
        MY_LOGD("Bokeh level(%d)", pFrameData->getBokehLevel());
        MY_LOGD("3dnr(%d)", pFrameData->is3DNREnable());
        MY_LOGD("gmv_x(%d)", pFrameData->getGMV_x());
        MY_LOGD("gmv_y(%d)", pFrameData->getGMV_y());
        MY_LOGD("userdata(0x%x)", pFrameData);
        MY_LOGD("AF trigger(%d)", pFrameData->getAFTrigger());
        MY_LOGD("AFx(%d)", pFrameData->getAFx());
        MY_LOGD("AFy(%d)", pFrameData->getAFy());
        MY_LOGD("In buffer size(%d)", request->vInputFrameInfo.size());
        MY_LOGD("Out buffer size(%d)", request->vOutputFrameInfo.size());
        MY_LOGD("G sensor(%d)", pFrameData->getGSensor());
        MY_LOGD("Magic number(%d)", pFrameData->getMagicNumber());
        MY_LOGD("Dump BokehRequest -");
    }
    //
    mpBokehEffectHal->updateEffectRequest(request);
    miEffectRequestInCount++;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
BokehNodeImp::
queryGMVFromMetaStream(process_frame *pFrameData, IMetadata* const metaStream)
{
    // EIS information needs sync with pass1 hal metadata.
    // For vsdof, EIS just use GMV
    IMetadata::IEntry entry = metaStream->entryFor(MTK_EIS_REGION);
    MINT32 gmv_x, gmv_y;
    if(entry.count() != 16)
    {
        MY_LOGE("Incorrect EIS data. Please check P1Node.");
        return MFALSE;
    }
    gmv_x = entry.itemAt(9, Type2Type<MINT32>());
    gmv_y = entry.itemAt(10, Type2Type<MINT32>());
    //
    pFrameData->setGMVData(MTRUE, gmv_x, gmv_y);
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
BokehNodeImp::
queryAFFromMetaStream(process_frame *pFrameData, IMetadata* const metaStream)
{
    MBOOL ret = MTRUE;
    // get af trigger
    MUINT8 eAFTrigger = MTK_CONTROL_AF_TRIGGER_IDLE;
    if(!tryGetMetadata<MUINT8>(metaStream,
            MTK_CONTROL_AF_TRIGGER, eAFTrigger))
    {
        MY_LOGW_IF(mLogLevel, "no MTK_CONTROL_AF_TRIGGER");
        ret = MFALSE;
    }
    if(eAFTrigger == MTK_CONTROL_AF_TRIGGER_START)
    {
        mbHasAfTrigger = MTRUE;
        pFrameData->setAFTrigger(MTRUE);
        // get scaler region
        MRect scalerRegion;
        if(!tryGetMetadata<MRect>(metaStream,
                MTK_SCALER_CROP_REGION, scalerRegion))
        {
            MY_LOGW_IF(mLogLevel, "no MTK_SCALER_CROP_REGION");
            ret = MFALSE;
        }
        MY_LOGD("scaler region: %d %d %d %d", scalerRegion.p.x, scalerRegion.p.y,
        scalerRegion.s.w, scalerRegion.s.h);
        // get af region
        MINT32 afRegion[5]{0, 0, 0, 0, 0};
        if (!GET_ENTRY_ARRAY(*metaStream, MTK_CONTROL_AF_REGIONS, afRegion, 5))
        {
            MY_LOGW_IF(mLogLevel, "no MTK_CONTROL_AF_REGIONS");
            ret = MFALSE;
        }
        MINT32 afx = (afRegion[0]+afRegion[2]) >> 1;
        MINT32 afy = (afRegion[1]+afRegion[3]) >> 1;
        MY_LOGD_IF(mLogLevel>0, "AF region %d %d %d %d %d",
                    afRegion[0],
                    afRegion[1],
                    afRegion[2],
                    afRegion[3],
                    afRegion[4]);
        // compute touch point
        /*MINT32 transformScaleCord_x = afx - scalerRegion.p.x;
        MINT32 transformScaleCord_y = afy - scalerRegion.p.y;
        MINT32 result_x = miMYSSize.w - ((scalerRegion.s.w-transformScaleCord_x)*(miMYSSize.w)/scalerRegion.s.w);
        MINT32 result_y = miMYSSize.h - ((scalerRegion.s.h-transformScaleCord_y)*(miMYSSize.h)/scalerRegion.s.h);
        pFrameData->setAFRegion(result_x, result_y);
        miCacheAFx = result_x;
        miCacheAFy = result_y;*/
    }
    else
    {
        if(mbFirstRun)
        {
            mbHasAfTrigger = MTRUE;
            //miCacheAFx = miMYSSize.w*.5f;
            //miCacheAFy = miMYSSize.h*.5f;
            pFrameData->setAFTrigger(MTRUE);
            //pFrameData->setAFRegion(miMYSSize.w*.5f, miMYSSize.h*.5f);
            mbFirstRun = MFALSE;
        }
        pFrameData->setAFTrigger(MFALSE);
    }

    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
BokehNodeImp::
queryConvOffsetFromMetaStream(process_frame *pFrameData, IMetadata* const metaStream)
{
    MFLOAT convOffset;
    if(!tryGetMetadata<MFLOAT>(metaStream,
                MTK_CONVERGENCE_DEPTH_OFFSET, convOffset))
    {
        MY_LOGE("no MTK_CONVERGENCE_DEPTH_OFFSET meta");
        return MFALSE;
    }
    pFrameData->setConvOffset(convOffset);
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
void
BokehNodeImp::BokehThread::
requestExit()
{
    //TODO: refine this
    Mutex::Autolock _l(mpNodeImp->mRequestQueueLock);
    mpNodeImp->mbRequestExit = MTRUE;
    mpNodeImp->mRequestQueueCond.signal();
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
BokehNodeImp::BokehThread::
readyToRun()
{
    return mpNodeImp->threadSetting();
}


/******************************************************************************
 *
 ******************************************************************************/
bool
BokehNodeImp::BokehThread::
threadLoop()
{
    sp<IPipelineFrame> pFrame;
    if  (
            !exitPending()
        &&  OK == mpNodeImp->onDequeRequest(pFrame)
        &&  pFrame != 0
        )
    {
        mpNodeImp->onProcessFrame(pFrame);

        return true;
    }

    MY_LOGD("exit unpack thread");
    return  false;

}
/******************************************************************************
 * process_frame
 ******************************************************************************/
MBOOL
process_frame::
init()
{
    start = std::chrono::system_clock::now();
    //mvImgBufContainer.add(BOKEH_ER_BUF_MYS, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_DMBG, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_MAIN1, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_RECORD, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_DISPLAY, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_VSDOF_IMG, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_CLEAN_IMG, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_THUMBNAIL, nullptr);
    mvImgBufContainer.add(BOKEH_ER_BUF_PRVCB, nullptr);
    //
    //mvImgStreamBuffer.add(BOKEH_ER_BUF_MYS, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_DMBG, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_MAIN1, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_RECORD, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_DISPLAY, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_VSDOF_IMG, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_CLEAN_IMG, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_THUMBNAIL, nullptr);
    mvImgStreamBuffer.add(BOKEH_ER_BUF_PRVCB, nullptr);
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
process_frame::
uninit(MBOOL vaild)
{
    MINT32 reqID = mpFrame->getRequestNo();
    // relase stream
    mpFrame = nullptr;
    mpInAppMetadata = nullptr;
    mpInAppMetadataStreamBuffer = nullptr;
    mpInHalMetadata = nullptr;
    mpInHalMetadata_P1 = nullptr;
    mpInHalMetadataStreamBuffer = nullptr;
    mpInHalMetadataStreamBuffer_P1 = nullptr;
    mpOutAppMetadata = nullptr;
    mpOutAppMetadataStreamBuffer = nullptr;
    mpOutHalMetadata = nullptr;
    mpOutHalMetadataStreamBuffer = nullptr;
    mvImgBufContainer.clear();
    mvImgStreamBuffer.clear();
    if(vaild == MTRUE && mLogLevel > 0)
    {
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        MY_LOGE("VSDOF_Profile: processing time(%lf ms) reqID=%d BokehLevel(%d)",
                                    elapsed_seconds.count() *1000,
                                    reqID,
                                    getBokehLevel());
    }
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
process_frame::
unlockAllStream(const char* nodeName)
{
    // unlock metadata
    {
        if(mpInHalMetadataStreamBuffer!=nullptr)
        {
            mpInHalMetadataStreamBuffer->unlock(nodeName, mpInHalMetadata);
        }
        if(mpInHalMetadataStreamBuffer_P1!=nullptr)
        {
            mpInHalMetadataStreamBuffer_P1->unlock(nodeName, mpInHalMetadata_P1);
        }
        //
        if(mpInAppMetadataStreamBuffer!=nullptr)
        {
            mpInAppMetadataStreamBuffer->unlock(nodeName, mpInAppMetadata);
        }
        //
        if(mpOutHalMetadataStreamBuffer!=nullptr)
        {
            mpOutHalMetadataStreamBuffer->unlock(nodeName, mpOutHalMetadata);
        }
        //
        if(mpOutAppMetadataStreamBuffer!=nullptr)
        {
            mpOutAppMetadataStreamBuffer->unlock(nodeName, mpOutAppMetadata);
        }
    }
    // unlock image stream
    {
        if(mvImgBufContainer.size() !=
           mvImgStreamBuffer.size())
        {
            MY_LOGE("mvImgBufContainer.size()!=mvImgStreamBuffer.size()");
            return;
        }
        MINT32 index = -1;
        sp<IImageBuffer> imgBuffer;
        sp<IImageStreamBuffer> imgStreamBuffer;
        for(ssize_t i=0; i<mvImgBufContainer.size();++i)
        {
            imgBuffer = mvImgBufContainer.editValueAt(i);
            imgStreamBuffer = mvImgStreamBuffer.editValueAt(i);
            if(imgBuffer != nullptr && imgStreamBuffer != nullptr)
            {
                imgBuffer->unlockBuf(nodeName);
                imgStreamBuffer->unlock(nodeName, imgBuffer->getImageBufferHeap());
            }
        }
    }
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
process_frame::
setGSensor(MINT32 rot)
{
    miGSensorAngle = rot;
}
