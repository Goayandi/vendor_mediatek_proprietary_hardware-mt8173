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
#define LOG_TAG "MtkCam/BMDeNoiseNode"

// Standard C header file
#include <string>
#include <set>

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
#include <mtkcam/utils/hw/IScenarioControl.h>

// Module header file
#include <mtkcam/pipeline/hwnode/BMDeNoiseNode.h>
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>
#include <mtkcam/feature/stereo/effecthal/BMDeNoiseEffectHal.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>

// Local header file
#include "hwnode_utilities.h"
#include "BaseNode.h"

using namespace std;
using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils::Sync;
using namespace NSCam::NSCamFeature::NSFeaturePipe;
/*******************************************************************************
* Global Define
********************************************************************************/
#define BMDeNoiseThread_NAME       ("Cam@BMDeNoiseNode")
#define BMDeNoiseThread_POLICY     (SCHED_OTHER)
#define BMDeNoiseThread_PRIORITY   (0)

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
class BMDeNoiseNodeImp
    : public BaseNode
    , public BMDeNoiseNode
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                                            Definitions.
    typedef android::sp<IPipelineFrame>                     QueNode_T;
    typedef android::List<QueNode_T>                        Que_T;

private:
    class BMDeNoiseThread
            : public Thread
        {

        public:

                                        BMDeNoiseThread(BMDeNoiseNodeImp* pNodeImp)
                                            : mpNodeImp(pNodeImp)
                                        {}

                                        ~BMDeNoiseThread()
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

                        BMDeNoiseNodeImp*       mpNodeImp = nullptr;

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

            sp<IImageBuffer>            mpImageBuffer_fullraw_1 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_mfbo_final_1 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_mfbo_final_2 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_w_1 = nullptr;
            sp<IImageBuffer>            mpImageBuffer_warpingMatrix = nullptr;
            sp<IImageBuffer>            mpImageBuffer_disparityMap = nullptr;
            sp<IImageBuffer>            mpImageBuffer_outImage = nullptr;
            sp<IImageBuffer>            mpImageBuffer_outImageThumbnail = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_fullraw_1 = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_mfbo_final_1 = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_mfbo_final_2 = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_w_1 = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_warpingMatrix = nullptr;
            sp<IImageStreamBuffer>      mpHalInStreamBuffer_disparityMap = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_outImage = nullptr;
            sp<IImageStreamBuffer>      mpHalOutStreamBuffer_outImageThumbnail = nullptr;
        private:
            NodeId_T                    mNodeId;
            char const*                 mNodeName;
    };

public:     ////                    Operations.

                                    BMDeNoiseNodeImp();

                                    ~BMDeNoiseNodeImp();

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

    static MVOID                    onEffectRequestFinished(MVOID* tag, String8 status, sp<EffectRequest>& request);

    MERROR                          onEffectReqDone(String8 status, sp<EffectRequest>& request);

    MVOID                           releaseMain2HalMeta(sp<EffectRequest> request);

    MINT32                          getDeNoiseEffectType(IMetadata* pHalInMeta);

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
    sp<IImageStreamInfo>            mpInHalImageFullRaw_1 = nullptr;
    sp<IImageStreamInfo>            mpInHalImageMFBO_final_1 = nullptr;
    sp<IImageStreamInfo>            mpInHalImageMFBO_final_2 = nullptr;
    sp<IImageStreamInfo>            mpInHalImageW_1 = nullptr;
    sp<IImageStreamInfo>            mpInHalImageWarpingMatrix = nullptr;
    sp<IImageStreamInfo>            mpInHalImageDisparityMap = nullptr;
    sp<IImageStreamInfo>            mpOutHalImage = nullptr;
    sp<IImageStreamInfo>            mpOutHalImageThumbnail = nullptr;
    // logLevel
    MINT32                          mLogLevel = -1;
    // Node thread
    sp<BMDeNoiseThread>             mpBMDeNoiseThread = nullptr;

    mutable Mutex                   mRequestQueueLock;
    Condition                       mRequestQueueCond;
    Que_T                           mRequestQueue;
    MBOOL                           mbRequestExit  = MFALSE;
    MBOOL                           mbRequestDrained  = MFALSE;
    Condition                       mbRequestDrainedCond;

    mutable Mutex                               mEffectRequestLock;
    KeyedVector< MUINT32, sp<IPipelineFrame> >  mvPipelineFrameMap;
    KeyedVector< MUINT32, NodeProcessFrame* >   mvProcessFrameMap;
    List< sp<EffectRequest> >                   mFinishedEffectReq_pre_process;
    List< sp<EffectRequest> >                   mFinishedEffectReq_warping;

    BMDeNoiseEffectHal*             mpBMDeNoiseEffectHal = nullptr;

    set<MINT32>                     mFrameArrivedQueue;

    sp<IScenarioControl>            mpScenarioCtrl = nullptr;
};

android::sp<BMDeNoiseNode>
BMDeNoiseNode::
createInstance()
{
    MY_LOGD("createInstance");
    return new BMDeNoiseNodeImp();
}

BMDeNoiseNodeImp::
BMDeNoiseNodeImp()
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

BMDeNoiseNodeImp::
~BMDeNoiseNodeImp()
{

}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDeNoiseNode Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MERROR
BMDeNoiseNodeImp::
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
        mpInHalImageFullRaw_1 = rParams.pInFullRaw_1;
        mpInHalImageMFBO_final_1 = rParams.pInMFBO_final_1;
        mpInHalImageMFBO_final_2 = rParams.pInMFBO_final_2;
        mpInHalImageW_1 = rParams.pInW_1;
        mpInHalImageWarpingMatrix = rParams.pInWarpingMatrix;
        mpInHalImageDisparityMap = rParams.pInDisparityMap;
        mpOutHalImage = rParams.vOutImage;
        mpOutHalImageThumbnail = rParams.vOutImageThumbnail;
        if(mpInHalImageFullRaw_1==nullptr)
        {
            MY_LOGE("mpInHalImageFullRaw_1 is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalImageMFBO_final_1==nullptr)
        {
            MY_LOGE("mpInHalImageMFBO_final_1 is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalImageMFBO_final_2==nullptr)
        {
            MY_LOGE("mpInHalImageMFBO_final_2 is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalImageW_1==nullptr)
        {
            MY_LOGE("mpInHalImageW_1 is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalImageWarpingMatrix==nullptr)
        {
            MY_LOGE("mpInHalImageWarpingMatrix is null");
            return UNKNOWN_ERROR;
        }
        if(mpInHalImageDisparityMap==nullptr)
        {
            MY_LOGE("mpInHalImageDisparityMap is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImage==nullptr)
        {
            MY_LOGE("mpOutHalImage is null");
            return UNKNOWN_ERROR;
        }
        if(mpOutHalImageThumbnail==nullptr)
        {
            MY_LOGE("mpOutHalImageThumbnail is null");
            return UNKNOWN_ERROR;
        }
    }

    MY_LOGD("mpInHalImageFullRaw_1:%dx%d mpInHalImageMFBO_final_1:%dx%d mpInHalImageMFBO_final_2:%dx%d mpInHalImageW_1:%dx%d mpInHalImageWarpingMatrix:%dx%d mpInHalImageDisparityMap:%dx%d",
        mpInHalImageFullRaw_1->getImgSize().w, mpInHalImageFullRaw_1->getImgSize().h,
        mpInHalImageMFBO_final_1->getImgSize().w, mpInHalImageMFBO_final_1->getImgSize().h,
        mpInHalImageMFBO_final_2->getImgSize().w, mpInHalImageMFBO_final_2->getImgSize().h,
        mpInHalImageW_1->getImgSize().w, mpInHalImageW_1->getImgSize().h,
        mpInHalImageWarpingMatrix->getImgSize().w, mpInHalImageWarpingMatrix->getImgSize().h,
        mpInHalImageDisparityMap->getImgSize().w, mpInHalImageDisparityMap->getImgSize().h
        );

    MY_LOGD("mpOutHalImage:%dx%d mpOutHalImageThumbnail:%dx%d",
        mpOutHalImage->getImgSize().w, mpOutHalImage->getImgSize().h,
        mpOutHalImageThumbnail->getImgSize().w, mpOutHalImageThumbnail->getImgSize().h
    );

    mpBMDeNoiseEffectHal->configure();
    mpBMDeNoiseEffectHal->start();

    FUNC_END;
    return OK;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDeNoiseNode Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MERROR
BMDeNoiseNodeImp::
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
BMDeNoiseNodeImp::
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
        // get in mpInHalImageFullRaw_1
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageFullRaw_1->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_fullraw_1,
                                pProcessFrame->mpImageBuffer_fullraw_1))
        {
            MY_LOGE("get in mpInHalImageFullRaw_1 image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in mpInHalImageW_1
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageW_1->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_w_1,
                                pProcessFrame->mpImageBuffer_w_1))
        {
            MY_LOGE("get in mpInHalImageW_1 image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in mpInHalImageMFBO_final_1
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageMFBO_final_1->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_mfbo_final_1,
                                pProcessFrame->mpImageBuffer_mfbo_final_1))
        {
            MY_LOGE("get in mpInHalImageMFBO_final_1 image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in mpInHalImageMFBO_final_2
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageMFBO_final_2->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_mfbo_final_2,
                                pProcessFrame->mpImageBuffer_mfbo_final_2))
        {
            MY_LOGE("get in mpInHalImageMFBO_final_2 image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in warpingMatrix image
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageWarpingMatrix->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_warpingMatrix,
                                pProcessFrame->mpImageBuffer_warpingMatrix))
        {
            MY_LOGE("get in warpingMatrix image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get in disparity map
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpInHalImageDisparityMap->getStreamId(),
                                pProcessFrame->mpHalInStreamBuffer_disparityMap,
                                pProcessFrame->mpImageBuffer_disparityMap))
        {
            MY_LOGE("get in disparityMap fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out image
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImage->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_outImage,
                                pProcessFrame->mpImageBuffer_outImage))
        {
            MY_LOGE("get out image fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
        // get out image thumbnail
        if(OK!=getImageBufferAndLock(
                                pFrame,
                                mpOutHalImageThumbnail->getStreamId(),
                                pProcessFrame->mpHalOutStreamBuffer_outImageThumbnail,
                                pProcessFrame->mpImageBuffer_outImageThumbnail))
        {
            MY_LOGE("get out image thumbnail fail.");
            suspendThisFrame(pFrame);
            return UNKNOWN_ERROR;
        }
    }

    //update effect request: deNoise
    {
        sp<EffectRequest> pEffectReq;
        sp<EffectParameter> pEffectParam;
        sp<EffectFrameInfo> pFrameInfo;
        MUINT32 iReqIdx = pFrame->getRequestNo();
        pEffectReq = new EffectRequest(iReqIdx, onEffectRequestFinished, (void*)this);

        pEffectParam = new EffectParameter();
        pEffectParam->set(BMDENOISE_REQUEST_TYPE_KEY, (int)getDeNoiseEffectType(pProcessFrame->mpHalInMeta));
        pEffectReq->setRequestParameter(pEffectParam);

        // prepare input frame info: FULLRAW_1
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_PRE_PROCESS_IN_FULLRAW_1);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_fullraw_1);
        pEffectReq->vInputFrameInfo.add(BID_PRE_PROCESS_IN_FULLRAW_1, pFrameInfo);

        // prepare input frame info: MFBO_FINAL_1
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_IN_MFBO_FINAL_1);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_mfbo_final_1);
        pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_MFBO_FINAL_1, pFrameInfo);

        // prepare input frame info: MFBO_FINAL_2
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_IN_MFBO_FINAL_2);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_mfbo_final_2);
        pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_MFBO_FINAL_2, pFrameInfo);

        // prepare input frame info: W_1
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_IN_W_1);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_w_1);
        pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_W_1, pFrameInfo);

        // prepare input frame info: disparity map
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_IN_DISPARITY_MAP_1);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_disparityMap);
        pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_DISPARITY_MAP_1, pFrameInfo);

        // prepare input frame info: warping matrix
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_IN_WARPING_MATRIX);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_warpingMatrix);
        pEffectReq->vInputFrameInfo.add(BID_DENOISE_IN_WARPING_MATRIX, pFrameInfo);

        // prepare output frame info: denoise result
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_FINAL_RESULT);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_outImage);
        pEffectReq->vOutputFrameInfo.add(BID_DENOISE_FINAL_RESULT, pFrameInfo);

        // prepare output frame info: denoise result thumbnail
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_DENOISE_FINAL_RESULT_THUMBNAIL);
        pFrameInfo->setFrameBuffer(pProcessFrame->mpImageBuffer_outImageThumbnail);
        pEffectReq->vOutputFrameInfo.add(BID_DENOISE_FINAL_RESULT_THUMBNAIL, pFrameInfo);

        // prepare input metadata: IN_APP
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_META_IN_APP);
        pEffectParam = new EffectParameter();
        pEffectParam->setPtr(BMDENOISE_EFFECT_PARAMS_KEY, (void*)pProcessFrame->mpAppInMeta);
        pFrameInfo->setFrameParameter(pEffectParam);
        pEffectReq->vInputFrameInfo.add(BID_META_IN_APP, pFrameInfo);

        // prepare input metadata: IN_HAL
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_META_IN_HAL);
        pEffectParam = new EffectParameter();
        pEffectParam->setPtr(BMDENOISE_EFFECT_PARAMS_KEY, (void*)pProcessFrame->mpHalInMeta);
        pFrameInfo->setFrameParameter(pEffectParam);
        pEffectReq->vInputFrameInfo.add(BID_META_IN_HAL, pFrameInfo);

        // prepare input metadata: IN_HAL_MAIN2
        pFrameInfo = new EffectFrameInfo(iReqIdx, BID_META_IN_HAL_MAIN2);
        pEffectParam = new EffectParameter();
        IMetadata* pHalMeta_Main2 = new IMetadata();
        if( ! tryGetMetadata<IMetadata>(pProcessFrame->mpHalInMeta, MTK_P1NODE_MAIN2_HAL_META, *pHalMeta_Main2) ){
            MY_LOGE("cannot get MTK_P1NODE_MAIN2_HAL_META from mpHalInMeta");
        }else{
            pEffectParam->setPtr(BMDENOISE_EFFECT_PARAMS_KEY, (void*)pHalMeta_Main2);
            pFrameInfo->setFrameParameter(pEffectParam);
            pEffectReq->vInputFrameInfo.add(BID_META_IN_HAL_MAIN2, pFrameInfo);
        }

        {
            MY_LOGD("scenario control by BMDeNoiseNode: enter capture");
            MUINT featureFlagStereo = 0;
            FEATURE_CFG_ENABLE_MASK(featureFlagStereo, IScenarioControl::FEATURE_BMDENOISE_CAPTURE);

            IScenarioControl::ControlParam controlParam;
            controlParam.scenario = IScenarioControl::Scenario_ZsdPreview;
            controlParam.sensorSize = mpInHalImageFullRaw_1->getImgSize();
            controlParam.sensorFps = 30;
            controlParam.featureFlag = featureFlagStereo;
            controlParam.enableBWCControl = MFALSE;

            mpScenarioCtrl->enterScenario(controlParam);
        }

        mpBMDeNoiseEffectHal->updateEffectRequest(pEffectReq);
    }

    {
        Mutex::Autolock _l(mEffectRequestLock);
        mvPipelineFrameMap.add(pFrame->getRequestNo(), pFrame);
        mvProcessFrameMap.add(pFrame->getRequestNo(), pProcessFrame);
    }

    FUNC_END;
    return OK;
}

MERROR
BMDeNoiseNodeImp::
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
    if (BMDeNoiseThread_POLICY == SCHED_OTHER) {
        sched_p.sched_priority = 0;
        ::sched_setscheduler(0, BMDeNoiseThread_POLICY, &sched_p);
        ::setpriority(PRIO_PROCESS, 0, BMDeNoiseThread_PRIORITY);   //  Note: "priority" is nice value.
    } else {
        sched_p.sched_priority = BMDeNoiseThread_PRIORITY;          //  Note: "priority" is real-time priority.
        ::sched_setscheduler(0, BMDeNoiseThread_POLICY, &sched_p);
    }

    MY_LOGD("tid(%d) policy(%d) priority(%d)", ::gettid(), BMDeNoiseThread_POLICY, BMDeNoiseThread_PRIORITY);

    return OK;

}

MERROR
BMDeNoiseNodeImp::
suspendThisFrame(
    sp<IPipelineFrame> const& pFrame)
{
    MY_LOGE("Discard request id=%d", pFrame->getRequestNo());

    MERROR err = BaseNode::flush(pFrame);

    return err;
}

MBOOL
BMDeNoiseNodeImp::
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
BMDeNoiseNodeImp::
isInImageStream(
    StreamId_T const streamId
) const
{
    //
    if( isStream(mpInHalImageFullRaw_1, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalImageMFBO_final_1, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalImageMFBO_final_2, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalImageW_1, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalImageWarpingMatrix, streamId) )
        return MTRUE;
    //
    if( isStream(mpInHalImageDisparityMap, streamId) )
        return MTRUE;
    //
    // MY_LOGD("stream id %#" PRIxPTR " is not output for this node", streamId);
    return MFALSE;
}

MERROR
BMDeNoiseNodeImp::
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
BMDeNoiseNodeImp::
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
BMDeNoiseNodeImp::
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

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_fullraw_1, processFrame->mpImageBuffer_fullraw_1, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_mfbo_final_1, processFrame->mpImageBuffer_mfbo_final_1, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_mfbo_final_2, processFrame->mpImageBuffer_mfbo_final_2, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_w_1, processFrame->mpImageBuffer_w_1, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_warpingMatrix, processFrame->mpImageBuffer_warpingMatrix, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalInStreamBuffer_disparityMap, processFrame->mpImageBuffer_disparityMap, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_outImage, processFrame->mpImageBuffer_outImage, mNodeId, mNodeName);

    markImageStream(processFrame->mpFrame, processFrame->mpHalOutStreamBuffer_outImageThumbnail, processFrame->mpImageBuffer_outImageThumbnail, mNodeId, mNodeName);

    FUNC_END;
    return err;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MERROR
BMDeNoiseNodeImp::
init(InitParams const& rParams)
{
    FUNC_START;

    MINT32 value;
    value = ::property_get_int32("debug.bmdenoise.threadPriority", BMDeNoiseThread_PRIORITY);
    if(value != BMDeNoiseThread_PRIORITY){
        MY_LOGD("changing bmdenoise thread priority from %d to %d, beware not to lower than ANDROID_PRIORITY_FOREGROUND:%d",
            BMDeNoiseThread_PRIORITY,
            value,
            ANDROID_PRIORITY_FOREGROUND
        );
        #undef BMDeNoiseThread_PRIORITY
        #define BMDeNoiseThread_PRIORITY value
    }

    mOpenId = rParams.openId;
    mNodeId = rParams.nodeId;
    mNodeName = rParams.nodeName;

    MY_LOGD("OpenId %d, nodeId %d, name %s",
            getOpenId(), getNodeId(), getNodeName() );





    // create node thread
    mpBMDeNoiseThread = new BMDeNoiseThread(this);
    if( mpBMDeNoiseThread->run(LOG_TAG) != OK )
    {
        MY_LOGE("run thread failed.");
        return UNKNOWN_ERROR;
    }

    mpBMDeNoiseEffectHal = new BMDeNoiseEffectHal();
    mpBMDeNoiseEffectHal->init();

    int main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);

    MY_LOGD("getStereoSensorIndex: (%d, %d)", main1Idx, main2Idx);

    sp<EffectParameter> effParam = new EffectParameter();
    effParam->set(SENSOR_IDX_MAIN1, main1Idx);
    effParam->set(SENSOR_IDX_MAIN2, main2Idx);
    mpBMDeNoiseEffectHal->setParameters(effParam);

    mpScenarioCtrl = IScenarioControl::create(getOpenId());
    if( mpScenarioCtrl == NULL )
    {
        MY_LOGE("get Scenario Control fail");
        return UNKNOWN_ERROR;
    }

    FUNC_END;
    return OK;
}

MERROR
BMDeNoiseNodeImp::
uninit()
{
    FUNC_START;
    //
    if ( OK != flush() )
        MY_LOGE("flush failed");

    mpBMDeNoiseThread->requestExit();
    mpBMDeNoiseThread->join();
    mpBMDeNoiseThread = nullptr;

    mpBMDeNoiseEffectHal->abort();
    mpBMDeNoiseEffectHal->unconfigure();
    mpBMDeNoiseEffectHal->uninit();
    delete mpBMDeNoiseEffectHal;

    mpScenarioCtrl->exitScenario();
    mpScenarioCtrl = nullptr;

    FUNC_END;
    return OK;
}

MERROR
BMDeNoiseNodeImp::
flush()
{
    FUNC_START;

    mpBMDeNoiseEffectHal->flush();

    FUNC_END;
    return OK;
}

MERROR
BMDeNoiseNodeImp::
queue(android::sp<IPipelineFrame> pFrame)
{
    FUNC_START;

    if( ! pFrame.get() ) {
        MY_LOGE("Null frame");
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mRequestQueueLock);

    // only if the frame is queue to this node for the second time triggers
    MBOOL hadArrived = MFALSE;
    MINT32 reqNo = (MINT32)pFrame->getFrameNo();

    if(mFrameArrivedQueue.find(reqNo) != mFrameArrivedQueue.end()){
        MY_LOGD("pFrame->getFrameNo():%d is the second time enqued", reqNo);
        hadArrived = MTRUE;
        mFrameArrivedQueue.erase(reqNo);
    }else{
        MY_LOGD("pFrame->getFrameNo():%d is the first time enqued", reqNo);
        hadArrived = MFALSE;
        mFrameArrivedQueue.insert(reqNo);
    }

    if(hadArrived){
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
    }

    FUNC_END;
    return OK;
}

MVOID
BMDeNoiseNodeImp::
onEffectRequestFinished(MVOID* tag, String8 status, sp<EffectRequest>& request)
{
    BMDeNoiseNodeImp *pNode = (BMDeNoiseNodeImp *) tag;
    pNode->onEffectReqDone(status, request);
}

MERROR
BMDeNoiseNodeImp::
onEffectReqDone(String8 status, sp<EffectRequest>& request)
{
    Mutex::Autolock _l(mEffectRequestLock);

    MUINT32 reqNo = request->getRequestNo();

    MY_LOGD("onEffectReqDone %d:reqNo +", reqNo);

    sp<IPipelineFrame> pFrame = mvPipelineFrameMap.valueFor(request->getRequestNo());
    mvPipelineFrameMap.removeItem(request->getRequestNo());

    NodeProcessFrame* pProcessFrame = mvProcessFrameMap.valueFor(request->getRequestNo());
    mvProcessFrameMap.removeItem(request->getRequestNo());

    MY_LOGD("markStatus and dispatch frame +");
    // replace jpeg orientation to 0, we do rotation in CameraHAL, not in Gallery
    {
        IMetadata::IEntry entry(MTK_JPEG_ORIENTATION);
        entry.push_back(0, Type2Type<MINT32>());
        pProcessFrame->mpAppInMeta->update(MTK_JPEG_ORIENTATION, entry);
    }

    // mark after-use stream status
    markStatus(pProcessFrame);

    // delete IMetadata
    releaseMain2HalMeta(request);

    pProcessFrame->uninit();
    delete pProcessFrame;

    MY_LOGD("BMDeNoiseNode: applyRelease reqNo=%d", pFrame->getRequestNo());
    pFrame->getStreamBufferSet().applyRelease(getNodeId());

    onDispatchFrame(pFrame);

    MY_LOGD("markStatus and dispatch frame -");

    MY_LOGD("onEffectReqDone %d:reqNo -", reqNo);
    return OK;
}

MINT32
BMDeNoiseNodeImp::
getDeNoiseEffectType(IMetadata* pHalInMeta)
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
    MINT32 effectType = 0;

    switch(denoiseMode){
        case eDeNoiseMode_NONE:
            effectType = TYPE_NONE;
            break;
        case eDeNoiseMode_BM:
            effectType = TYPE_BM_DENOISE;
            break;
        default:
            MY_LOGE("denoise mode:%d not supported!, use default");
            effectType = TYPE_BM_DENOISE;
    }

    MY_LOGD("DeNoiseMode:%d, effectType:%d", denoiseMode, effectType);
    return effectType;
}

MVOID
BMDeNoiseNodeImp::
releaseMain2HalMeta(sp<EffectRequest> request)
{
    sp<EffectFrameInfo> pFrame_HalMetaMain2 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
    sp<EffectParameter> pEffParam = pFrame_HalMetaMain2->getFrameParameter();
    IMetadata* pMeta = reinterpret_cast<IMetadata*>(pEffParam->getPtr(BMDENOISE_EFFECT_PARAMS_KEY));
    delete pMeta;
}
//************************************************************************
// NodeProcessFrame
//************************************************************************
MBOOL
BMDeNoiseNodeImp::NodeProcessFrame::
init(){
    start = std::chrono::system_clock::now();
    return MTRUE;
}

MBOOL
BMDeNoiseNodeImp::NodeProcessFrame::
uninit(){
    MINT32 reqID = mpFrame->getRequestNo();
    // if(mLogLevel > 0)
    {
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        MY_LOGD("BMDeNoise_Profile: BMDeNoiseNode processing time(%lf ms) reqID=%d",
            elapsed_seconds.count() *1000,
            reqID
        );
    }
    return MTRUE;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDeNoiseThread Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void
BMDeNoiseNodeImp::BMDeNoiseThread::
requestExit()
{
    Mutex::Autolock _l(mpNodeImp->mRequestQueueLock);
    mpNodeImp->mbRequestExit = MTRUE;
    mpNodeImp->mRequestQueueCond.signal();
}

status_t
BMDeNoiseNodeImp::BMDeNoiseThread::
readyToRun()
{
    return mpNodeImp->threadSetting();
}

bool
BMDeNoiseNodeImp::BMDeNoiseThread::
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
