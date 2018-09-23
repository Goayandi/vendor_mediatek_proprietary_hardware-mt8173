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
#define LOG_TAG "MtkCam/BMPreProcessNode"
#define USE_DEFAULT_ISP 0

// Standard C header file
#include <string>

// Android system/core header file
#include <utils/RWLock.h>
#include <utils/Thread.h>
#include <cutils/properties.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <chrono>

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/feature/effectHalBase/IEffectHal.h>
#include <mtkcam/feature/effectHalBase/EffectHalBase.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>

#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <isp_tuning/isp_tuning.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
//#include <met_tag.h>

// Module header file
#include <mtkcam/pipeline/hwnode/BMPreProcessNode.h>
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>
#include <mtkcam/feature/stereo/effecthal/BMDeNoiseEffectHal.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <DpBlitStream.h>

// Local header file
#include "BaseNode.h"
#include "hwnode_utilities.h"

using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils::Sync;
using namespace NSCam::NSCamFeature::NSFeaturePipe;
using namespace NSCam::NSIoPipe::NSPostProc;
/*******************************************************************************
* Global Define
********************************************************************************/
#define BMPreProcessThread_NAME       ("Cam@BMPreProcessNode")
#define BMPreProcessThread_POLICY     (SCHED_OTHER)
#define BMPreProcessThread_PRIORITY   (0)

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

// MET tags
#define DO_P1_PREPROCESS "do P1 pre-process"
#define DO_MFBO_MDP "do MFBO MDP rotation"
/*******************************************************************************
* External Function
********************************************************************************/



/*******************************************************************************
* Enum Define
********************************************************************************/



/*******************************************************************************
* Structure Define
********************************************************************************/



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  MISC Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static inline
MBOOL
isStream(sp<IStreamInfo> pStreamInfo, StreamId_T streamId )
{
    return pStreamInfo.get() && pStreamInfo->getStreamId() == streamId;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class BMPreProcessNodeImp
    : public BaseNode
    , public BMPreProcessNode
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                                            Definitions.
    typedef android::sp<IPipelineFrame>                     QueNode_T;
    typedef android::List<QueNode_T>                        Que_T;

private:
    class BMPreProcessThread
            : public Thread
        {

        public:

                                        BMPreProcessThread(BMPreProcessNodeImp* pNodeImp)
                                            : mpNodeImp(pNodeImp)
                                        {}

                                        ~BMPreProcessThread()
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

                        BMPreProcessNodeImp*       mpNodeImp = nullptr;

        };
private:
    class NodeProcessFrame
    {
        public:
            // store time to get processing time
            std::chrono::time_point<std::chrono::system_clock> start;
            std::chrono::time_point<std::chrono::system_clock> end;
            //
            NodeProcessFrame(
                        sp<IPipelineFrame> const pFrame,
                        NodeId_T nodeId,
                        char const* nodeName
                        ):
                        mpFrame(pFrame),
                        mNodeId(nodeId),
                        mNodeName(nodeName)
            {
            }

            MBOOL                   init();
            MBOOL                   uninit();
        public:
            sp<IPipelineFrame> const    mpFrame = nullptr;

            IMetadata*                  mpAppInMeta = nullptr;
            IMetadata*                  mpAppOutMeta = nullptr;
            IMetadata*                  mpHalInMeta = nullptr;
            IMetadata*                  mpHalOutMeta = nullptr;
            sp<IMetaStreamBuffer>       mpAppInMetaStreamBuffer = nullptr;
            sp<IMetaStreamBuffer>       mpAppOutMetaStreamBuffer = nullptr;
            sp<IMetaStreamBuffer>       mpHalInMetaStreamBuffer = nullptr;
            sp<IMetaStreamBuffer>       mpHalOutMetaStreamBuffer = nullptr;

            sp<IImageBuffer>            mpImageBuffer_fullraw = nullptr;
            sp<IImageBuffer>            mpImageBuffer_fullrawMain2 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_mfbo_1 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_mfbo_2 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_mfbo_final_1 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_mfbo_final_2 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_w1 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_w2 = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_fullraw = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_fullrawMain2 = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_mfbo_1 = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_mfbo_2 = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_mfbo_final_1 = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_mfbo_final_2 = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_w1 = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_w2 = nullptr;
        private:
            NodeId_T                    mNodeId;
            char const*                 mNodeName;
    };
    struct ISPTuningConfig
    {
        IMetadata* pInAppMeta;
        IMetadata* pInHalMeta;
        IHal3A* p3AHAL;
    };
    struct sMDP_Config
    {
        DpBlitStream* pDpStream = nullptr;
        IImageBuffer* pSrcBuffer = nullptr;
        IImageBuffer* pDstBuffer = nullptr;
        MINT32 rotAngle = 0;
        // These 2 size are used for special request that the
        // allocated buffer size is bigger than actual content size
        MSize customizedSrcSize = MSize(0, 0);
        MSize customizedDstSize = MSize(0, 0);
    };

public:     ////                    Operations.

                                    BMPreProcessNodeImp();

                                    ~BMPreProcessNodeImp();

    virtual MERROR                  config(ConfigParams const& rParams);

protected:
    MERROR                          threadSetting();

    MERROR                          onDequeRequest(
                                        android::sp<IPipelineFrame>& rpFrame
                                    );

    MERROR                          onProcessFrame(
                                        android::sp<IPipelineFrame> const& pFrame
                                    );

    MERROR                          suspendThisFrame(
                                        sp<IPipelineFrame> const& pFrame
                                    );

    MBOOL                           isInMetaStream(
                                        StreamId_T const streamId
                                    ) const;

    MBOOL                           isInImageStream(
                                        StreamId_T const streamId
                                    ) const;

    MERROR                          getImageBufferAndLock(
                                        android::sp<IPipelineFrame> const& pFrame,
                                        StreamId_T const streamId,
                                        sp<IImageStreamBuffer>& rpStreamBuffer,
                                        sp<IImageBuffer>& rpImageBuffer
                                    );

    MERROR                          getMetadataAndLock(
                                        android::sp<IPipelineFrame> const& pFrame,
                                        StreamId_T const streamId,
                                        sp<IMetaStreamBuffer>& rpStreamBuffer,
                                        IMetadata*& rpOutMetadataResult
                                    );

    MERROR                          markStatus(NodeProcessFrame* processFrame);

    MVOID                           releaseMain2HalMeta(sp<EffectRequest> request);

    TuningParam                     applyISPTuning(MVOID* pVATuningBuffer, const ISPTuningConfig& ispConfig, MBOOL useDefaultISP);

    MBOOL                           prepareModuleSettings();

    MINT32                          eTransformToDegree(MINT32 eTrans);

    MBOOL                           executeMDPBayer12(sMDP_Config config);

    MBOOL                           handleDispatchFrame(NodeProcessFrame* processFrame, android::sp<IPipelineFrame> const& pFrame);

    MBOOL                           needDoBMDeNoise(IMetadata* pHalInMeta);

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
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Private Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    // meta
    sp<IMetaStreamInfo>             mpInAppMetadata = nullptr;
    sp<IMetaStreamInfo>             mpOutAppMetadata = nullptr;
    sp<IMetaStreamInfo>             mpInHalMetadata = nullptr;
    sp<IMetaStreamInfo>             mpOutHalMetadata = nullptr;
    // image
    sp<IImageStreamInfo>            mpInHalImageFullRaw = nullptr;
    sp<IImageStreamInfo>            mpInHalImageFullRawMain2 = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageMFBO_1 = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageMFBO_2 = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageMFBO_final_1 = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageMFBO_final_2 = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageW_1 = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageW_2 = nullptr;
    // logLevel
    MINT32                          mLogLevel = -1;
    // Node thread
    sp<BMPreProcessThread>          mpBMPreProcessThread = nullptr;

    mutable Mutex                   mRequestQueueLock;
    Condition                       mRequestQueueCond;
    Que_T                           mRequestQueue;
    MBOOL                           mbRequestExit  = MFALSE;
    MBOOL                           mbRequestDrained  = MFALSE;
    Condition                       mbRequestDrainedCond;

    MINT32                          mSensorIdx_Main1 = -1;
    MINT32                          mSensorIdx_Main2 = -1;

    IHal3A*                         mp3AHal_Main1 = nullptr;
    IHal3A*                         mp3AHal_Main2 = nullptr;
    INormalStream*                  mpINormalStream = nullptr;
    DpBlitStream*                   mpDpStream = nullptr;

    MINT32                          mModuleTrans = -1;
};

android::sp<BMPreProcessNode>
BMPreProcessNode::
createInstance()
{
    MY_LOGD("createInstance");
    return new BMPreProcessNodeImp();
}

BMPreProcessNodeImp::
BMPreProcessNodeImp()
    : BaseNode()
{
    mLogLevel = ::property_get_int32("debug.camera.log", 0);
    if(mLogLevel == 0){
        mLogLevel = ::property_get_int32("debug.camera.log.bmdenoise", 0);
    }
    if(mLogLevel == 0){
        mLogLevel = ::property_get_int32("debug.camera.log.basenode", 0);
    }

    MY_LOGD("mLogLevel(%d)", mLogLevel);
}

BMPreProcessNodeImp::
~BMPreProcessNodeImp()
{

}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMPreProcessNode Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MERROR
BMPreProcessNodeImp::
config(ConfigParams const& rParams)
{
    FUNC_START;
    flush();

    {
        // meta
        mpInAppMetadata = rParams.pInAppMeta;
        mpOutAppMetadata = rParams.pOutAppMeta;
        mpInHalMetadata = rParams.pInHalMeta;
        mpOutHalMetadata = rParams.pOutHalMeta;
        if(mpInAppMetadata==nullptr)
        {
            MY_LOGE("mpInAppMetadata is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutAppMetadata==nullptr)
        {
            MY_LOGE("mpOutAppMetadata is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalMetadata==nullptr)
        {
            MY_LOGE("mpInHalMetadata is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalMetadata==nullptr)
        {
            MY_LOGE("mpOutHalMetadata is null");
            return UNKNOWN_ERROR;
        }
        // image
        mpInHalImageFullRaw = rParams.pInFullRaw;
        mpInHalImageFullRawMain2 = rParams.pInFullRawMain2;
        mpOutHalImageMFBO_1 = rParams.vOutImageMFBO_1;
        mpOutHalImageMFBO_2 = rParams.vOutImageMFBO_2;
        mpOutHalImageMFBO_final_1 = rParams.vOutImageMFBO_final_1;
        mpOutHalImageMFBO_final_2 = rParams.vOutImageMFBO_final_2;
        mpOutHalImageW_1 = rParams.vOutImageW_1;
        mpOutHalImageW_2 = rParams.vOutImageW_2;
        if(mpInHalImageFullRaw==nullptr)
        {
            MY_LOGE("mpInHalImageFullRaw is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalImageFullRawMain2==nullptr)
        {
            MY_LOGE("mpInHalImageFullRawMain2 is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImageMFBO_1==nullptr)
        {
            MY_LOGE("mpOutHalImageMFBO_1 is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImageMFBO_2==nullptr)
        {
            MY_LOGE("mpOutHalImageMFBO_2 is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImageW_1==nullptr)
        {
            MY_LOGE("mpOutHalImageW_2 is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImageMFBO_final_1==nullptr)
        {
            MY_LOGE("mpOutHalImageMFBO_final_1 is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImageMFBO_final_2==nullptr)
        {
            MY_LOGE("mpOutHalImageMFBO_final_2 is null");
            return UNKNOWN_ERROR;
        }
    }

    MY_LOGD("mpInHalImageFullRaw:%dx%d  mpInHalImageFullRawMain2:%dx%d",
        mpInHalImageFullRaw->getImgSize().w, mpInHalImageFullRaw->getImgSize().h,
        mpInHalImageFullRawMain2->getImgSize().w, mpInHalImageFullRawMain2->getImgSize().h
    );

    MY_LOGD("mpOutHalImageMFBO_1:%dx%d mpOutHalImageMFBO_2:%dx%d mpOutHalImageMFBO_final_1:%dx%d mpOutHalImageMFBO_final_2:%dx%d",
        mpOutHalImageMFBO_1->getImgSize().w, mpOutHalImageMFBO_1->getImgSize().h,
        mpOutHalImageMFBO_2->getImgSize().w, mpOutHalImageMFBO_2->getImgSize().h,
        mpOutHalImageMFBO_final_1->getImgSize().w, mpOutHalImageMFBO_final_1->getImgSize().h,
        mpOutHalImageMFBO_final_2->getImgSize().w, mpOutHalImageMFBO_final_2->getImgSize().h
    );

    MY_LOGD("mpOutHalImageW_1:%dx%d mpOutHalImageW_1:%dx%d",
        mpOutHalImageW_1->getImgSize().w, mpOutHalImageW_1->getImgSize().h,
        mpOutHalImageW_2->getImgSize().w, mpOutHalImageW_2->getImgSize().h
    );

    FUNC_END;
    return OK;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMPreProcessNode Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MERROR
BMPreProcessNodeImp::
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
            MY_LOGW(
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

MERROR
BMPreProcessNodeImp::
onProcessFrame(
    android::sp<IPipelineFrame> const& pFrame
)
{
    FUNC_START;

    IPipelineFrame::InfoIOMapSet IOMapSet;

    // get frame IO Map
    if(OK != pFrame->queryInfoIOMapSet( getNodeId(), IOMapSet ))
    {
        MY_LOGE("queryInfoIOMap failed, IOMap img/meta: %d/%d",
                IOMapSet.mImageInfoIOMapSet.size(),
                IOMapSet.mMetaInfoIOMapSet.size()
                );
        return UNKNOWN_ERROR;
    }

    // ensure all frame streams are available
    NodeProcessFrame* pProcessFrame = new NodeProcessFrame(pFrame, getNodeId(), getNodeName());
    pProcessFrame->init();
    //met_tag_start(0, DO_P1_PREPROCESS);

    {
        IStreamBufferSet& streamBufferSet = pFrame->getStreamBufferSet();
        // get in app meta
        if(OK!=getMetadataAndLock(
                                pFrame,
                                mpInAppMetadata->getStreamId(),
                                pProcessFrame->mpAppInMetaStreamBuffer,
                                pProcessFrame->mpAppInMeta))
        {
            MY_LOGE("get in app meta fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out app meta
        if(OK!=getMetadataAndLock(
                                pFrame,
                                mpOutAppMetadata->getStreamId(),
                                pProcessFrame->mpAppOutMetaStreamBuffer,
                                pProcessFrame->mpAppOutMeta))
        {
            MY_LOGE("get Out app meta fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in hal meta
        if(OK!=getMetadataAndLock(
                                pFrame,
                                mpInHalMetadata->getStreamId(),
                                pProcessFrame->mpHalInMetaStreamBuffer,
                                pProcessFrame->mpHalInMeta))
        {
            MY_LOGE("get in hal meta fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out hal meta
        if(OK!=getMetadataAndLock(
                                pFrame,
                                mpOutHalMetadata->getStreamId(),
                                pProcessFrame->mpHalOutMetaStreamBuffer,
                                pProcessFrame->mpHalOutMeta))
        {
            MY_LOGE("get out hal meta fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in fullraw image
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageFullRaw->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_fullraw,
                                pProcessFrame->mpImageBuffer_fullraw))
        {
            MY_LOGE("get in fullraw image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in fullrawMain2 image
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageFullRawMain2->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_fullrawMain2,
                                pProcessFrame->mpImageBuffer_fullrawMain2))
        {
            MY_LOGE("get in fullrawMain2 image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out mpOutHalImageMFBO_1
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageMFBO_1->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_mfbo_1,
                                pProcessFrame->mpImageBuffer_mfbo_1))
        {
            MY_LOGE("get out mpOutHalImageMFBO_1 fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out mpOutHalImageMFBO_2
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageMFBO_2->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_mfbo_2,
                                pProcessFrame->mpImageBuffer_mfbo_2))
        {
            MY_LOGE("get out mpOutHalImageMFBO_2 fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out mpOutHalImageW1
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageW_1->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_w1,
                                pProcessFrame->mpImageBuffer_w1))
        {
            MY_LOGE("get out mpOutHalImageW_1 fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out mpOutHalImageW2
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageW_2->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_w2,
                                pProcessFrame->mpImageBuffer_w2))
        {
            MY_LOGE("get out mpOutHalImageW_2 fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out mpOutHalImageMFBO_final_1
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageMFBO_final_1->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_mfbo_final_1,
                                pProcessFrame->mpImageBuffer_mfbo_final_1))
        {
            MY_LOGE("get out mpOutHalImageMFBO_final_1 fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out mpOutHalImageMFBO_final_2
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageMFBO_final_2->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_mfbo_final_2,
                                pProcessFrame->mpImageBuffer_mfbo_final_2))
        {
            MY_LOGE("get out mpOutHalImageMFBO_final_2 fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
    }

    // check if whe need to do BMDeNoise
    if(!needDoBMDeNoise(pProcessFrame->mpHalInMeta)){
        handleDispatchFrame(pProcessFrame, pFrame);
        FUNC_END;
        return OK;
    }

    // prepare enque params
    TuningParam rTuningParam_main1;
    TuningParam rTuningParam_main2;
    const MUINT32 tuningsize = mpINormalStream->getRegTableSize();
    MVOID* tuningBuf_main1 = (MVOID*) malloc(tuningsize);
    MVOID* tuningBuf_main2 = (MVOID*) malloc(tuningsize);
    memset(tuningBuf_main1, 0, tuningsize);
    memset(tuningBuf_main2, 0, tuningsize);

    IMetadata halMeta_Main1 = *(pProcessFrame->mpHalInMeta);
    IMetadata halMeta_Main2;
    if( ! tryGetMetadata<IMetadata>(pProcessFrame->mpHalInMeta, MTK_P1NODE_MAIN2_HAL_META, halMeta_Main2) ){
        MY_LOGE("cannot get MTK_P1NODE_MAIN2_HAL_META from mpHalInMeta");
    }

    {
        trySetMetadata<MUINT8>(&halMeta_Main1, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Denoise_toW);
        ISPTuningConfig ispConfig = {pProcessFrame->mpAppInMeta, &halMeta_Main1, mp3AHal_Main1};
        rTuningParam_main1 = applyISPTuning(tuningBuf_main1, ispConfig, USE_DEFAULT_ISP);
    }

    {
        trySetMetadata<MUINT8>(&halMeta_Main2, MTK_3A_ISP_PROFILE, NSIspTuning::EIspProfile_N3D_Denoise_toW);
        ISPTuningConfig ispConfig = {pProcessFrame->mpAppInMeta, &halMeta_Main2, mp3AHal_Main2};
        rTuningParam_main2 = applyISPTuning(tuningBuf_main2, ispConfig, USE_DEFAULT_ISP);
    }

    //enque to Pass2, use blocking call
    StereoSizeProvider* sizePrvider = StereoSizeProvider::getInstance();
    StereoArea area;

    QParams enqueParams, dequeParams;
    // main1_w & main1_mfbo
    {
        int frameGroup = 0;

        enqueParams.mvStreamTag.push_back(ENormalStreamTag_DeNoise);
        enqueParams.mvSensorIdx.push_back(mSensorIdx_Main1);
        enqueParams.mvTuningData.push_back(tuningBuf_main1);

        Input src;
        src.mPortID = PORT_IMGI;
        src.mPortID.group = frameGroup;
        src.mBuffer = pProcessFrame->mpImageBuffer_fullraw.get();
        enqueParams.mvIn.push_back(src);

        Input LSCIn;
        LSCIn.mPortID = PORT_DEPI;
        LSCIn.mPortID.group = frameGroup;
        LSCIn.mBuffer = static_cast<IImageBuffer*>(rTuningParam_main1.pLsc2Buf);
        enqueParams.mvIn.push_back(LSCIn);

        StereoArea fullraw_crop;
        fullraw_crop = sizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);

        MCrpRsInfo crop1, crop2, crop3;
        // CRZ
        crop1.mGroupID = 1;
        crop1.mFrameGroup = frameGroup;
        crop1.mCropRect.p_fractional.x=0;
        crop1.mCropRect.p_fractional.y=0;
        crop1.mCropRect.p_integral.x=0;
        crop1.mCropRect.p_integral.y=0;
        crop1.mCropRect.s.w=src.mBuffer->getImgSize().w;
        crop1.mCropRect.s.h=src.mBuffer->getImgSize().h;
        crop1.mResizeDst.w=src.mBuffer->getImgSize().w;
        crop1.mResizeDst.h=src.mBuffer->getImgSize().h;
        enqueParams.mvCropRsInfo.push_back(crop1);

        // WROT
        StereoArea area;
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_W_1);
        pProcessFrame->mpImageBuffer_w1->setExtParam(area.contentSize());

        crop3.mGroupID = 3;
        crop3.mFrameGroup = frameGroup;
        crop3.mCropRect.p_fractional.x=0;
        crop3.mCropRect.p_fractional.y=0;
        crop3.mCropRect.p_integral.x=fullraw_crop.startPt.x;
        crop3.mCropRect.p_integral.y=fullraw_crop.startPt.y;
        crop3.mCropRect.s = fullraw_crop.size;
        crop3.mResizeDst = fullraw_crop.size;
        enqueParams.mvCropRsInfo.push_back(crop3);

        Output out;
        out.mPortID = PORT_WROT;
        out.mPortID.group = frameGroup;
        out.mBuffer = pProcessFrame->mpImageBuffer_w1.get();
        out.mTransform = mModuleTrans;
        enqueParams.mvOut.push_back(out);

        // MFBO
        Output out2;
        out2.mPortID = PORT_MFBO;
        out2.mPortID.group = frameGroup;
        out2.mBuffer = pProcessFrame->mpImageBuffer_mfbo_1.get();
        // A trick to make MFBO destination crop
        pProcessFrame->mpImageBuffer_mfbo_1->setExtParam(
            fullraw_crop.size,
            fullraw_crop.size.w*pProcessFrame->mpImageBuffer_mfbo_1->getPlaneBitsPerPixel(0)/8*fullraw_crop.startPt.y
        );
        enqueParams.mvOut.push_back(out2);
    }

    // main2_mfbo
    {
        int frameGroup = 1;

        enqueParams.mvStreamTag.push_back(ENormalStreamTag_DeNoise);
        enqueParams.mvSensorIdx.push_back(mSensorIdx_Main2);
        enqueParams.mvTuningData.push_back(tuningBuf_main2);

        Input src;
        src.mPortID = PORT_IMGI;
        src.mPortID.group = frameGroup;
        src.mBuffer = pProcessFrame->mpImageBuffer_fullrawMain2.get();
        enqueParams.mvIn.push_back(src);

        Input LSCIn;
        LSCIn.mPortID = PORT_DEPI;
        LSCIn.mPortID.group = frameGroup;
        LSCIn.mBuffer = static_cast<IImageBuffer*>(rTuningParam_main2.pLsc2Buf);
        enqueParams.mvIn.push_back(LSCIn);

        StereoArea fullraw_crop;
        fullraw_crop = sizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_2);

        MCrpRsInfo crop1, crop2, crop3;
        // CRZ
        crop1.mGroupID = 1;
        crop1.mFrameGroup = frameGroup;
        crop1.mCropRect.p_fractional.x=0;
        crop1.mCropRect.p_fractional.y=0;
        crop1.mCropRect.p_integral.x=0;
        crop1.mCropRect.p_integral.y=0;
        crop1.mCropRect.s.w=src.mBuffer->getImgSize().w;
        crop1.mCropRect.s.h=src.mBuffer->getImgSize().h;
        crop1.mResizeDst.w=src.mBuffer->getImgSize().w;
        crop1.mResizeDst.h=src.mBuffer->getImgSize().h;
        enqueParams.mvCropRsInfo.push_back(crop1);

        // WROT
        StereoArea area;
        area = sizePrvider->getBufferSize(E_BM_PREPROCESS_W_2);
        pProcessFrame->mpImageBuffer_w2->setExtParam(area.contentSize());

        crop3.mGroupID = 3;
        crop3.mFrameGroup = frameGroup;
        crop3.mCropRect.p_fractional.x=0;
        crop3.mCropRect.p_fractional.y=0;
        crop3.mCropRect.p_integral.x=fullraw_crop.startPt.x;
        crop3.mCropRect.p_integral.y=fullraw_crop.startPt.y;
        crop3.mCropRect.s = fullraw_crop.size;
        crop3.mResizeDst = fullraw_crop.size;
        enqueParams.mvCropRsInfo.push_back(crop3);

        Output out;
        out.mPortID = PORT_WROT;
        out.mPortID.group = frameGroup;
        out.mBuffer = pProcessFrame->mpImageBuffer_w2.get();
        out.mTransform = mModuleTrans;
        enqueParams.mvOut.push_back(out);

        // MFBO
        Output out2;
        out2.mPortID = PORT_MFBO;
        out2.mPortID.group = frameGroup;
        out2.mBuffer = pProcessFrame->mpImageBuffer_mfbo_2.get();
        // A trick to make MFBO destination crop
        pProcessFrame->mpImageBuffer_mfbo_2->setExtParam(
            fullraw_crop.size,
            fullraw_crop.size.w*pProcessFrame->mpImageBuffer_mfbo_2->getPlaneBitsPerPixel(0)/8*fullraw_crop.startPt.y
        );
        enqueParams.mvOut.push_back(out2);
    }

    enqueParams.mpfnCallback = NULL;
    enqueParams.mpfnEnQFailCallback = NULL;
    enqueParams.mpCookie = NULL;

    MY_LOGD("mpINormalStream enque start! reqID=%d", pFrame->getRequestNo());
    CAM_TRACE_BEGIN("BMPreProcessNode::NormalStream");

    if(!mpINormalStream->enque(enqueParams))
    {
        MY_LOGE("mpINormalStream enque failed! reqID=%d", pFrame->getRequestNo());
        return UNKNOWN_ERROR;
    }

    if(!mpINormalStream->deque(dequeParams))
    {
        MY_LOGE("mpINormalStream deque failed! reqID=%d", pFrame->getRequestNo());
        return UNKNOWN_ERROR;
    }
    //met_tag_end(0, DO_P1_PREPROCESS);

    CAM_TRACE_END();
    MY_LOGD("mpINormalStream deque end! reqID=%d", pFrame->getRequestNo());

    MY_LOGD("BMPreProcessNode::MDP start! reqID=%d", pFrame->getRequestNo());
    //do MDP module rotation
    {
        CAM_TRACE_NAME("BMPreProcessNode::MDP");
        //met_tag_start(0, DO_MFBO_MDP);
        // do MDP rotate for main1 mfbo
        {
            StereoArea area_dst;
            area_dst = sizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_1);

            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = pProcessFrame->mpImageBuffer_mfbo_1.get();
            config.pDstBuffer = pProcessFrame->mpImageBuffer_mfbo_final_1.get();
            config.rotAngle = eTransformToDegree(mModuleTrans);
            config.customizedDstSize = area_dst.contentSize();

            if(!executeMDPBayer12(config))
            {
                MY_LOGE("executeMDPBayer12 for main1 mfbo failed.");
            }
        }

        // do MDP rotate for main2 mfbo
        {
            StereoArea area_dst;
            area_dst = sizePrvider->getBufferSize(E_BM_PREPROCESS_MFBO_FINAL_2);

            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = pProcessFrame->mpImageBuffer_mfbo_2.get();
            config.pDstBuffer = pProcessFrame->mpImageBuffer_mfbo_final_2.get();
            config.rotAngle = eTransformToDegree(mModuleTrans);
            config.customizedDstSize = area_dst.contentSize();

            if(!executeMDPBayer12(config))
            {
                MY_LOGE("executeMDPBayer12 for main2 mfbo failed.");
            }
        }
        //met_tag_end(0, DO_MFBO_MDP);
    }

    handleDispatchFrame(pProcessFrame, pFrame);

    FUNC_END;
    return OK;
}

MERROR
BMPreProcessNodeImp::
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
    if (BMPreProcessThread_POLICY == SCHED_OTHER) {
        sched_p.sched_priority = 0;
        ::sched_setscheduler(0, BMPreProcessThread_POLICY, &sched_p);
        ::setpriority(PRIO_PROCESS, 0, BMPreProcessThread_PRIORITY);   //  Note: "priority" is nice value.
    } else {
        sched_p.sched_priority = BMPreProcessThread_PRIORITY;          //  Note: "priority" is real-time priority.
        ::sched_setscheduler(0, BMPreProcessThread_POLICY, &sched_p);
    }

    MY_LOGD("tid(%d) policy(%d) priority(%d)", ::gettid(), BMPreProcessThread_POLICY, BMPreProcessThread_PRIORITY);

    return OK;

}

MERROR
BMPreProcessNodeImp::
suspendThisFrame(
    sp<IPipelineFrame> const& pFrame)
{
    MY_LOGE("Discard request id=%d", pFrame->getRequestNo());

    MERROR err = BaseNode::flush(pFrame);

    return err;
}

MBOOL
BMPreProcessNodeImp::
isInMetaStream(
    StreamId_T const streamId
) const
{
    if( isStream(mpInAppMetadata, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalMetadata, streamId) )
        return MTRUE;
    //
    // MY_LOGD("stream id %#" PRIxPTR " is not output for this node", streamId);
    return MFALSE;
}

MBOOL
BMPreProcessNodeImp::
isInImageStream(
    StreamId_T const streamId
) const
{
    //
    if( isStream(mpInHalImageFullRaw, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalImageFullRawMain2, streamId) )
        return MTRUE;
    //
    // MY_LOGD("stream id %#" PRIxPTR " is not output for this node", streamId);
    return MFALSE;
}

MERROR
BMPreProcessNodeImp::
getMetadataAndLock(
    android::sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    sp<IMetaStreamBuffer>& rpStreamBuffer,
    IMetadata*& rpMetadata
)
{
    IStreamBufferSet&      rStreamBufferSet = pFrame->getStreamBufferSet();
    MY_LOGD("nodeID %d streamID %d",getNodeId(), streamId);
    MERROR const err = ensureMetaBufferAvailable_(
            pFrame->getFrameNo(),
            streamId,
            rStreamBufferSet,
            rpStreamBuffer
            );
    MY_LOGD_IF(rpStreamBuffer==NULL," rpStreamBuffer==NULL");
    //metaInLock = FALSE;

    if( err != OK )
        return err;

    rpMetadata = isInMetaStream(streamId) ?
        rpStreamBuffer->tryReadLock(getNodeName()) :
        rpStreamBuffer->tryWriteLock(getNodeName());
    //metaInLock = TRUE;
    if( rpMetadata == NULL ) {
        MY_LOGE("[frame:%u node:%d][stream buffer:%s] cannot get metadata",
                pFrame->getFrameNo(), getNodeId(), rpStreamBuffer->getName());
        return BAD_VALUE;
    }

    MY_LOGD_IF(1,"stream %#" PRIxPTR ": stream buffer %p, metadata: %p",
        streamId, rpStreamBuffer.get(), rpMetadata);

    return OK;
}

MERROR
BMPreProcessNodeImp::
getImageBufferAndLock(
    android::sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    sp<IImageStreamBuffer>& rpStreamBuffer,
    sp<IImageBuffer>& rpImageBuffer
)
{
    IStreamBufferSet&      rStreamBufferSet = pFrame->getStreamBufferSet();
    sp<IImageBufferHeap>   pImageBufferHeap = NULL;
    MERROR const err = ensureImageBufferAvailable_(
            pFrame->getFrameNo(),
            streamId,
            rStreamBufferSet,
            rpStreamBuffer
            );

    if( err != OK )
        return err;

    //  Query the group usage.
    MUINT const groupUsage = rpStreamBuffer->queryGroupUsage(getNodeId());

    pImageBufferHeap = isInImageStream(streamId) ?
        rpStreamBuffer->tryReadLock(getNodeName()) :
        rpStreamBuffer->tryWriteLock(getNodeName());

    if (pImageBufferHeap == NULL) {
        MY_LOGE("pImageBufferHeap == NULL");
        return BAD_VALUE;
    }

    rpImageBuffer = pImageBufferHeap->createImageBuffer();
    if (rpImageBuffer == NULL) {
        MY_LOGE("rpImageBuffer == NULL");
        return BAD_VALUE;
    }
    rpImageBuffer->lockBuf(getNodeName(), groupUsage);

    MY_LOGD("stream buffer: (%p) %p, heap: %p, buffer: %p, usage: %p",
        streamId, rpStreamBuffer.get(), pImageBufferHeap.get(), rpImageBuffer.get(), groupUsage);

    return OK;
}

MERROR
BMPreProcessNodeImp::
markStatus(NodeProcessFrame* processFrame)
{
    FUNC_START;
    MERROR err = OK;
    IStreamBufferSet& streamBufferSet = processFrame->mpFrame->getStreamBufferSet();

    // mark meta stream
    auto markMetaStream = [](sp<IPipelineFrame> const& pFrame,
                                    sp<IMetaStreamBuffer> const pStreamBuffer,
                                    IMetadata* metadata,
                                    Pipeline_NodeId_T nodeId,
                                    char const* nodeName)
    {
        IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();
        StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();

        pStreamBuffer->unlock(nodeName, metadata);
        //
        pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
        //
        //  Mark this buffer as USED by this user.
        //  Mark this buffer as RELEASE by this user.
        rStreamBufferSet.markUserStatus(
                                        streamId,
                                        nodeId,
                                        IUsersManager::UserStatus::USED |
                                        IUsersManager::UserStatus::RELEASE
                                        );
    };

    markMetaStream(processFrame->mpFrame, processFrame->mpAppInMetaStreamBuffer, processFrame->mpAppInMeta, mNodeId, mNodeName);

    markMetaStream(processFrame->mpFrame, processFrame->mpAppOutMetaStreamBuffer, processFrame->mpAppOutMeta, mNodeId, mNodeName);

    markMetaStream(processFrame->mpFrame, processFrame->mpHalInMetaStreamBuffer, processFrame->mpHalInMeta, mNodeId, mNodeName);

    markMetaStream(processFrame->mpFrame, processFrame->mpHalOutMetaStreamBuffer, processFrame->mpHalOutMeta, mNodeId, mNodeName);

    // release image stream
    auto markImageStream = [](sp<IPipelineFrame> const& pFrame,
                                    sp<IImageStreamBuffer> const pStreamBuffer,
                                    sp<IImageBuffer> imageBuf,
                                    Pipeline_NodeId_T nodeId,
                                    char const* nodeName)
    {
        IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();
        StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();

        imageBuf->unlockBuf(nodeName);
        pStreamBuffer->unlock(nodeName, imageBuf->getImageBufferHeap());
        rStreamBufferSet.markUserStatus(
                    streamId,
                    nodeId,
                    IUsersManager::UserStatus::USED |
                    IUsersManager::UserStatus::RELEASE
                    );
    };


    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_fullraw, processFrame->mpImageBuffer_fullraw, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_fullrawMain2, processFrame->mpImageBuffer_fullrawMain2, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_mfbo_1, processFrame->mpImageBuffer_mfbo_1, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_mfbo_2, processFrame->mpImageBuffer_mfbo_2, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_mfbo_final_1, processFrame->mpImageBuffer_mfbo_final_1, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_mfbo_final_2, processFrame->mpImageBuffer_mfbo_final_2, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_w1, processFrame->mpImageBuffer_w1, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_w2, processFrame->mpImageBuffer_w2, mNodeId, mNodeName);

    FUNC_END;
    return err;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MERROR
BMPreProcessNodeImp::
init(InitParams const& rParams)
{
    FUNC_START;

    mOpenId = rParams.openId;
    mNodeId = rParams.nodeId;
    mNodeName = rParams.nodeName;

    MY_LOGD("OpenId %d, nodeId %d, name %s",
            getOpenId(), getNodeId(), getNodeName() );

    // create node thread
    mpBMPreProcessThread = new BMPreProcessThread(this);
    if( mpBMPreProcessThread->run(LOG_TAG) != OK )
    {
        MY_LOGE("run thread failed.");
        return UNKNOWN_ERROR;
    }

    StereoSettingProvider::getStereoSensorIndex(mSensorIdx_Main1, mSensorIdx_Main2);

    MY_LOGD("getStereoSensorIndex: (%d, %d)", mSensorIdx_Main1, mSensorIdx_Main2);

    MY_LOGD("BMPreProcessNode::onInit=>create_3A_instance senosrIdx:(%d/%d)", mSensorIdx_Main1, mSensorIdx_Main2);
    CAM_TRACE_BEGIN("BMPreProcessNode::onInit=>create_3A_instance");
    mp3AHal_Main1 = MAKE_Hal3A(mSensorIdx_Main1, "BMPREPROCESS_3A_MAIN1");
    mp3AHal_Main2 = MAKE_Hal3A(mSensorIdx_Main2, "BMPREPROCESS_3A_MAIN2");
    MY_LOGD("3A create instance, Main1: %x Main2: %x", mp3AHal_Main1, mp3AHal_Main2);
    CAM_TRACE_END();

    // normal stream
    MY_LOGD("BMPreProcessNode::onInit=>create normalStream");
    CAM_TRACE_BEGIN("BMPreProcessNode::onInit=>create normalStream");
    mpINormalStream = NSCam::NSIoPipe::NSPostProc::INormalStream::createInstance(mSensorIdx_Main1);
    if (mpINormalStream == NULL)
    {
        MY_LOGE("mpINormalStream create instance for BMPreProcessNode Node failed!");
        return MFALSE;
    }
    mpINormalStream->init(LOG_TAG);
    CAM_TRACE_END();

    // MDP
    MY_LOGD("BMPreProcessNode::onInit=>new DpBlitStream");
    CAM_TRACE_BEGIN("BMPreProcessNode::onInit=>new DpBlitStream");
    mpDpStream = new DpBlitStream();
    CAM_TRACE_END();

    prepareModuleSettings();

    FUNC_END;
    return OK;
}

MERROR
BMPreProcessNodeImp::
uninit()
{
    FUNC_START;
    //
    if ( OK != flush() )
        MY_LOGE("flush failed");

    mpBMPreProcessThread->requestExit();
    mpBMPreProcessThread->join();
    mpBMPreProcessThread = NULL;

    if(mp3AHal_Main1)
    {
        mp3AHal_Main1->destroyInstance("BMPREPROCESS_3A_MAIN1");
        mp3AHal_Main1 = NULL;
    }

    if(mp3AHal_Main2)
    {
        mp3AHal_Main2->destroyInstance("BMPREPROCESS_3A_MAIN2");
        mp3AHal_Main2 = NULL;
    }
    if(mpINormalStream != nullptr)
    {
        mpINormalStream->uninit(LOG_TAG);
        mpINormalStream->destroyInstance();
        mpINormalStream = nullptr;
    }
    if(mpDpStream!= nullptr)
        delete mpDpStream;

    FUNC_END;
    return OK;
}

MERROR
BMPreProcessNodeImp::
flush()
{
    FUNC_START;

    FUNC_END;
    return OK;
}

MERROR
BMPreProcessNodeImp::
queue(android::sp<IPipelineFrame> pFrame)
{
    FUNC_START;

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

    FUNC_END;
    return OK;
}

TuningParam
BMPreProcessNodeImp::
applyISPTuning(MVOID* pVATuningBuffer, const ISPTuningConfig& ispConfig, MBOOL useDefaultISP)
{
    CAM_TRACE_NAME("BMPreProcessNode::applyISPTuning");
    MY_LOGD("+");

    TuningParam tuningParam = {NULL, NULL};
    tuningParam.pRegBuf = reinterpret_cast<void*>(pVATuningBuffer);

    MetaSet_T inMetaSet;
    inMetaSet.appMeta = *ispConfig.pInAppMeta;
    inMetaSet.halMeta = *ispConfig.pInHalMeta;

    trySetMetadata<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 1);

    if(useDefaultISP){
        MY_LOGE("Test mode not support!");
    }else{
        MetaSet_T resultMeta;
        ispConfig.p3AHAL->setIsp(0, inMetaSet, &tuningParam, &resultMeta);

        // DO NOT write ISP resultMeta back into input hal Meta since there are other node doing this concurrently
        // (*ispConfig.pInHalMeta) += resultMeta.halMeta;
    }

    MY_LOGD("-");
    return tuningParam;
}

MBOOL
BMPreProcessNodeImp::
prepareModuleSettings()
{
    MY_LOGD("+");

    // module rotation
    ENUM_ROTATION eRot = StereoSettingProvider::getModuleRotation();
    switch(eRot)
    {
        case eRotate_0:
            mModuleTrans = 0;
            break;
        case eRotate_90:
            mModuleTrans = eTransform_ROT_90;
            break;
        case eRotate_180:
            mModuleTrans = eTransform_ROT_180;
            break;
        case eRotate_270:
            mModuleTrans = eTransform_ROT_270;
            break;
        default:
            MY_LOGE("Not support module rotation =%d", eRot);
            return MFALSE;
    }

    MY_LOGD("mModuleTrans=%d", mModuleTrans);

    MY_LOGD("-");
    return MTRUE;
}

MINT32
BMPreProcessNodeImp::
eTransformToDegree(MINT32 eTrans)
{
    switch(mModuleTrans)
    {
        case 0:
            return 0;
        case eTransform_ROT_90:
            return 90;
        case eTransform_ROT_180:
            return 180;
        case eTransform_ROT_270:
            return 270;
        default:
            MY_LOGE("Not support eTransform =%d", eTrans);
            return -1;
    }
}

MBOOL
BMPreProcessNodeImp::
executeMDPBayer12(sMDP_Config config)
{
    if(config.pDpStream == nullptr)
        return MFALSE;
    //***************************src****************************//
    MINTPTR src_addr_list[3] = {0, 0, 0};
    unsigned int src_size_list[3] = {0, 0, 0};
    size_t planeCount = config.pSrcBuffer->getPlaneCount();
    MY_LOGD("src planeCount:%d src srcSize(%dx%d), ImgBitsPerPixel:%d stride@plane[0]:%d",
        planeCount,
        config.pSrcBuffer->getImgSize().w,
        config.pSrcBuffer->getImgSize().h,
        config.pSrcBuffer->getImgBitsPerPixel(),
        config.pSrcBuffer->getBufStridesInBytes(0)
    );


    for(size_t i=0;i<planeCount;++i)
    {
        src_addr_list[i] = config.pSrcBuffer->getBufVA(i);
        src_size_list[i] = config.pSrcBuffer->getBufSizeInBytes(i);
    }

    MINT32 yPitch = config.pSrcBuffer->getBufStridesInBytes(0);
    MINT32 uvPitch = 0;

    config.pDpStream->setSrcBuffer((void **)src_addr_list, src_size_list, planeCount);

    config.pDpStream->setSrcConfig(
                            config.pSrcBuffer->getImgSize().w,
                            config.pSrcBuffer->getImgSize().h,
                            yPitch,
                            uvPitch,
                            DP_COLOR_RGB565_RAW,
                            DP_PROFILE_FULL_BT601,
                            eInterlace_None,
                            0,
                            DP_SECURE_NONE,
                            false);

    MY_LOGD("yPitch:%d, uvPitch:%d", yPitch, uvPitch);
    //***************************dst********************************//
    size_t dst_planeCount = config.pDstBuffer->getPlaneCount();
    MSize dstBufferSize = config.pDstBuffer->getImgSize();

    yPitch = config.customizedDstSize.w*(config.pDstBuffer->getImgBitsPerPixel()/8);
    uvPitch = 0;

    MY_LOGD("dst Buffer Size:%dx%d, ImgBitsPerPixel:%d, planeCount:%d",
        dstBufferSize.w,
        dstBufferSize.h,
        config.pDstBuffer->getImgBitsPerPixel(),
        dst_planeCount
    );

    MY_LOGD("customized dst Buffer Size:%dx%d rotateAngle:%d",
        config.customizedDstSize.w,
        config.customizedDstSize.h,
        config.rotAngle
    );

    MINTPTR dst_addr_list[3] = {0, 0, 0};
    unsigned int dst_size_list[3] = {0, 0, 0};
    for(size_t i=0;i<dst_planeCount;++i)
    {
        dst_addr_list[i] = config.pDstBuffer->getBufVA(i);
        dst_size_list[i] = config.customizedDstSize.w*config.customizedDstSize.h*(config.pDstBuffer->getImgBitsPerPixel()/8);
    }

    config.pDpStream->setDstBuffer((void**)dst_addr_list, dst_size_list, dst_planeCount);

    config.pDpStream->setDstConfig(
                                config.customizedDstSize.w,
                                config.customizedDstSize.h,
                                yPitch,
                                uvPitch,
                                DP_COLOR_RGB565_RAW,
                                DP_PROFILE_FULL_BT601,
                                eInterlace_None,
                                0,
                                DP_SECURE_NONE,
                                false);

    config.pDpStream->setRotate(config.rotAngle);
    //*******************************************************************//
    if (config.pDpStream->invalidate())  //trigger HW
    {
          MY_LOGE("FDstream invalidate failed");
          return MFALSE;
    }
    return MTRUE;
}

MBOOL
BMPreProcessNodeImp::
handleDispatchFrame(NodeProcessFrame* processFrame, android::sp<IPipelineFrame> const& pFrame)
{
    //mark status and release
    markStatus(processFrame);

    processFrame->uninit();
    delete processFrame;

    pFrame->getStreamBufferSet().applyRelease(getNodeId());

    onDispatchFrame(pFrame);

    return MTRUE;
}

MBOOL
BMPreProcessNodeImp::
needDoBMDeNoise(IMetadata* pHalInMeta)
{
    MINT32 ISO = 0;
    MINT32 debugISO = 0;
    IMetadata exifMeta;

    if( tryGetMetadata<IMetadata>(pHalInMeta, MTK_3A_EXIF_METADATA, exifMeta) ) {
        if(tryGetMetadata<MINT32>(&exifMeta, MTK_3A_EXIF_AE_ISO_SPEED, ISO)){
            MY_LOGD("Get ISO:%d", ISO);
        }else{
            MY_LOGE("Get ISO failed, use default value:%d", ISO);
        }
    }
    else {
        MY_LOGE("no tag: MTK_3A_EXIF_METADATA, use default value:%d", ISO);
    }

    debugISO = ::property_get_int32("debug.bmdenoise.iso", -1);

    MY_LOGD("metaISO:%d/debugISO:%d", ISO, debugISO);

    if(debugISO != -1 && debugISO >= 0){
        ISO = debugISO;
    }

    DeNoiseMode denoiseMode = getBMDeNoiseMode(ISO);

    if(denoiseMode == eDeNoiseMode_NONE){
        MY_LOGD("DeNoiseMode:%d, no need to do BM Denoise", denoiseMode);
        return MFALSE;
    }else{
        MY_LOGD("DeNoiseMode:%d, do BM Denoise", denoiseMode);
        return MTRUE;
    }
}

/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
BMPreProcessNodeImp::NodeProcessFrame::
init(){
    start = std::chrono::system_clock::now();
    return MTRUE;
}

MBOOL
BMPreProcessNodeImp::NodeProcessFrame::
uninit(){
    MINT32 reqID = mpFrame->getRequestNo();
    // if(mLogLevel > 0)
    {
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        MY_LOGD("BMDeNoise_Profile: BMPreProcessNode processing time(%lf ms) reqID=%d",
            elapsed_seconds.count() *1000,
            reqID
        );
    }
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MVOID
BMPreProcessNodeImp::
releaseMain2HalMeta(sp<EffectRequest> request)
{
    sp<EffectFrameInfo> pFrame_HalMetaMain2 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
    sp<EffectParameter> pEffParam = pFrame_HalMetaMain2->getFrameParameter();
    IMetadata* pMeta = reinterpret_cast<IMetadata*>(pEffParam->getPtr(BMDENOISE_EFFECT_PARAMS_KEY));
    delete pMeta;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMPreProcessThread Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void
BMPreProcessNodeImp::BMPreProcessThread::
requestExit()
{
    Mutex::Autolock _l(mpNodeImp->mRequestQueueLock);
    mpNodeImp->mbRequestExit = MTRUE;
    mpNodeImp->mRequestQueueCond.signal();
}

status_t
BMPreProcessNodeImp::BMPreProcessThread::
readyToRun()
{
    return mpNodeImp->threadSetting();
}

bool
BMPreProcessNodeImp::BMPreProcessThread::
threadLoop()
{
    sp<IPipelineFrame> pFrame;
    if  (
            !exitPending()
        &&  OK == mpNodeImp->onDequeRequest(pFrame)
        &&  pFrame != 0
        )
    {
        if(mpNodeImp->onProcessFrame(pFrame) != OK){
            MY_LOGE("process frame failed!");
        }
        return true;
    }

    MY_LOGD("exit bmdenoise thread");
    return  false;

}
