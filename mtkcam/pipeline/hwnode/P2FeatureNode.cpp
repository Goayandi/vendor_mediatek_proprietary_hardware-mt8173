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

#define LOG_TAG "MtkCam/P2FeatureNode"
//
#include <mtkcam/utils/std/Log.h>
#include "BaseNode.h"
#include "hwnode_utilities.h"
#include <mtkcam/pipeline/hwnode/P2FeatureNode.h>
//
#include <utils/RWLock.h>
#include <utils/Thread.h>
//
#include <sys/prctl.h>
#include <sys/resource.h>
//
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/drv/iopipe/SImager/IImageTransform.h>
//
#include <vector>
#include <list>
#include <fstream>
//
#include <mtkcam/utils/std/DebugScanLine.h>
//
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#if 0/*[EP_TEMP]*/
#include <featureio/eis_hal.h>
#endif
//
#include <mtkcam/utils/std/Trace.h>
//
#include <cutils/properties.h>
//
using namespace NSCam;
#include <mtkcam/utils/hw/HwTransform.h>
#include "P2_utils.h"
//

#ifdef FEATURE_MODIFY
#include <mtkcam/drv/IHalSensor.h>
#include <debug_exif/cam/dbg_cam_param.h>
#include <mtkcam/utils/exif/DebugExifUtils.h>
#include <camera_custom_3dnr.h>
#include <mtkcam/feature/eis/eis_ext.h>
#include <camera_custom_eis.h>
#include <mtkcam/feature/featurePipe/IStreamingFeaturePipe.h>
using NSCam::NSCamFeature::NSFeaturePipe::IStreamingFeaturePipe;
using NSCam::NSCamFeature::NSFeaturePipe::FeaturePipeParam;
using NSCam::NSCamFeature::VarMap;
using namespace NSCam::NSCamFeature::NSFeaturePipe;
#define VAR_P2_REQUEST "p2request"
#endif // FEATURE_MODIFY

using namespace android;
using namespace NSCam::v3;
using namespace NSCam::Utils::Sync;
using namespace NSCamHW;

using namespace std;
using namespace NSIoPipe;
using namespace NSIoPipe::NSPostProc;
using namespace NS3Av3;

/******************************************************************************
 *
 ******************************************************************************/
#define PROCESSOR_NAME_P2   ("P2_Feature")
#define PROCESSOR_NAME_MDP  ("MDP_P2_Feature")
#define P2THREAD_POLICY     (SCHED_OTHER)
#define P2THREAD_PRIORITY   (-2)
//
#define WAITBUFFER_TIMEOUT (1000000000L)
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
#if 0
#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")
#else
#define FUNC_START
#define FUNC_END
#endif
#define P2_DEQUE_DEBUG (1)
/******************************************************************************
 *
 ******************************************************************************/
#define SUPPORT_3A               (1)
#define SUPPORT_EIS              (1)
#define SUPPORT_EIS_MV           (0)
#define FD_PORT_SUPPORT          (1)
#define FORCE_EIS_ON             (SUPPORT_EIS && (0))
#define FORCE_BURST_ON           (0)
#define DEBUG_LOG                (0)

#ifdef HAL3A_SIMULATOR_IMP
typedef NSCam::v3::IHal3ASimulator IHal3A_T;
#ifdef MAKE_Hal3A
#undef MAKE_Hal3A
#endif
#define MAKE_Hal3A(...) NSCam::v3::Hal3ASimulator::createInstance(__VA_ARGS__)
#else
typedef IHal3A IHal3A_T;
#endif

/******************************************************************************
 *
 ******************************************************************************/
namespace P2Feature {
/******************************************************************************
 *
 ******************************************************************************/


inline
MBOOL isStream(sp<IStreamInfo> pStreamInfo, StreamId_T streamId ) {
    return pStreamInfo.get() && pStreamInfo->getStreamId() == streamId;
}


/******************************************************************************
 *
 ******************************************************************************/
class StreamControl
{
    public:
        typedef enum
        {
            eStreamStatus_NOT_USED = (0x00000000UL),
            eStreamStatus_FILLED   = (0x00000001UL),
            eStreamStatus_ERROR    = (0x00000001UL << 1),
        } eStreamStatus_t;

    public:

        virtual                         ~StreamControl() {};

    public:

        virtual MERROR                  getInfoIOMapSet(
                                            sp<IPipelineFrame> const& pFrame,
                                            IPipelineFrame::InfoIOMapSet& rIOMapSet
                                        ) const                                   = 0;

        // query in/out stream function
        virtual MBOOL                   isInImageStream(
                                            StreamId_T const streamId
                                        ) const                                   = 0;

        virtual MBOOL                   isInMetaStream(
                                            StreamId_T const streamId
                                        ) const                                   = 0;

        // image stream related
        virtual MERROR                  acquireImageStream(
                                            sp<IPipelineFrame> const& pFrame,
                                            StreamId_T const streamId,
                                            sp<IImageStreamBuffer>& rpStreamBuffer
                                        )                                         = 0;

        virtual MVOID                   releaseImageStream(
                                            sp<IPipelineFrame> const& pFrame,
                                            sp<IImageStreamBuffer> const pStreamBuffer,
                                            MUINT32 const status
                                        ) const                                   = 0;

        virtual MERROR                  acquireImageBuffer(
                                            StreamId_T const streamId,
                                            sp<IImageStreamBuffer> const pStreamBuffer,
                                            sp<IImageBuffer>& rpImageBuffer
                                        ) const                                   = 0;

        virtual MVOID                   releaseImageBuffer(
                                            sp<IImageStreamBuffer> const pStreamBuffer,
                                            sp<IImageBuffer> const pImageBuffer
                                        ) const                                   = 0;

        // meta stream related
        virtual MERROR                  acquireMetaStream(
                                            sp<IPipelineFrame> const& pFrame,
                                            StreamId_T const streamId,
                                            sp<IMetaStreamBuffer>& rpStreamBuffer
                                        )                                         = 0;

        virtual MVOID                   releaseMetaStream(
                                            sp<IPipelineFrame> const& pFrame,
                                            sp<IMetaStreamBuffer> const pStreamBuffer,
                                            MUINT32 const status
                                        ) const                                   = 0;

        virtual MERROR                  acquireMetadata(
                                            StreamId_T const streamId,
                                            sp<IMetaStreamBuffer> const pStreamBuffer,
                                            IMetadata*& rpMetadata
                                        ) const                                   = 0;

        virtual MVOID                   releaseMetadata(
                                            sp<IMetaStreamBuffer> const pStreamBuffer,
                                            IMetadata* const pMetadata
                                        ) const                                   = 0;

        // frame control related
        virtual MVOID                   onPartialFrameDone(
                                            sp<IPipelineFrame> const& pFrame
                                        )                                         = 0;

        virtual MVOID                   onFrameDone(
                                            sp<IPipelineFrame> const& pFrame
                                        )                                         = 0;

};


class MetaHandle
    : public VirtualLightRefBase
{
    public:
        typedef enum
        {
            STATE_NOT_USED,
            STATE_READABLE,
            STATE_WRITABLE,
            STATE_WRITE_OK = STATE_READABLE,
            STATE_WRITE_FAIL,
        } BufferState_t;

    public:
        static sp<MetaHandle>           create(
                                            StreamControl* const pCtrl,
                                            sp<IPipelineFrame> const& pFrame,
                                            StreamId_T const streamId
                                        );
                                        ~MetaHandle();
    protected:
                                        MetaHandle(
                                            StreamControl* pCtrl,
                                            sp<IPipelineFrame> const& pFrame,
                                            StreamId_T const streamId,
                                            sp<IMetaStreamBuffer> const pStreamBuffer,
                                            BufferState_t const init_state,
                                            IMetadata * const pMeta
                                        )
                                            : mpStreamCtrl(pCtrl)
                                            , mpFrame(pFrame)
                                            , mStreamId(streamId)
                                            , mpStreamBuffer(pStreamBuffer)
                                            , mpMetadata(pMeta)
                                            , muState(init_state)
                                        {}

    public:
        IMetadata*                      getMetadata() { return mpMetadata; }

#if 0
        virtual MERROR                  waitState(
                                            BufferState_t const state,
                                            nsecs_t const nsTimeout = WAITBUFFER_TIMEOUT
                                        )                                                   = 0;
#endif
        MVOID                           updateState(
                                            BufferState_t const state
                                        );
    private:
        Mutex                           mLock;
        //Condition                       mCond;
        StreamControl* const            mpStreamCtrl;
        sp<IPipelineFrame> const        mpFrame;
        StreamId_T const                mStreamId;
        sp<IMetaStreamBuffer> const     mpStreamBuffer;
        IMetadata* const                mpMetadata;
        MUINT32                         muState;
};


class BufferHandle
    : public VirtualLightRefBase
{
    public:
        typedef enum
        {
            STATE_NOT_USED,
            STATE_READABLE,
            STATE_WRITABLE,
            STATE_WRITE_OK = STATE_READABLE,
            STATE_WRITE_FAIL,
        } BufferState_t;

    public:
        virtual                         ~BufferHandle() {}

    public:
        virtual IImageBuffer*           getBuffer()                                         = 0;

        virtual MERROR                  waitState(
                                            BufferState_t const state,
                                            nsecs_t const nsTimeout = WAITBUFFER_TIMEOUT
                                        )                                                   = 0;
        virtual MVOID                   updateState(
                                            BufferState_t const state
                                        )                                                   = 0;
        virtual MUINT32                 getState()                                          = 0;

        virtual MUINT32                 getUsage() { return 0; };

        virtual MUINT32                 getTransform() { return 0; };

        virtual StreamId_T              getStreamId() { return 0; };

    #ifdef FEATURE_MODIFY
        virtual MBOOL                   isValid() { return MTRUE; }
        virtual MBOOL                   earlyRelease() { return MTRUE; }
    #endif // FEATURE_MODIFY

};


class StreamBufferHandle
    : public BufferHandle
{
    public:
        static sp<BufferHandle>         create(
                                            StreamControl* const pCtrl,
                                            sp<IPipelineFrame> const& pFrame,
                                            StreamId_T const streamId
                                        );
                                        ~StreamBufferHandle();
    protected:
                                        StreamBufferHandle(
                                            StreamControl* pCtrl,
                                            sp<IPipelineFrame> const& pFrame,
                                            StreamId_T const streamId,
                                            sp<IImageStreamBuffer> const pStreamBuffer,
                                            MUINT32 const uTransform,
                                            MUINT32 const uUsage
                                        )
                                            : mpStreamCtrl(pCtrl)
                                            , mpFrame(pFrame)
                                            , mStreamId(streamId)
                                            , mpStreamBuffer(pStreamBuffer)
                                            , muState(STATE_NOT_USED)
                                            , mpImageBuffer(NULL)
                                            , muTransform(uTransform)
                                            , muUsage(uUsage)
                                            #ifdef FEATURE_MODIFY
                                            , mEarlyReleased(MFALSE)
                                            #endif // FEATURE_MODIFY
                                        {}

    public:
        IImageBuffer*                   getBuffer() { return mpImageBuffer.get(); }
        MERROR                          waitState(
                                            BufferState_t const state,
                                            nsecs_t const nsTimeout
                                        );
        MVOID                           updateState(
                                            BufferState_t const state
                                        );

        virtual MUINT32                 getState() { return muState; };

        virtual MUINT32                 getTransform() { return muTransform; };

        virtual MUINT32                 getUsage() { return muUsage; };

        virtual StreamId_T              getStreamId() { return mStreamId; };

    #ifdef FEATURE_MODIFY
        virtual MBOOL                   isValid() {  return !mEarlyReleased; }
        virtual MBOOL                   earlyRelease();
    #endif // FEATURE_MODIFY


    private:
        Mutex                           mLock;
        Condition                       mCond;
        StreamControl* const            mpStreamCtrl;
        sp<IPipelineFrame> const        mpFrame;
        StreamId_T const                mStreamId;
        sp<IImageStreamBuffer> const    mpStreamBuffer;
        MUINT32                         muState;
        MUINT32                         muTransform;
        MUINT32                         muUsage;
        sp<IImageBuffer>                mpImageBuffer;
    #ifdef FEATURE_MODIFY
        MBOOL                           mEarlyReleased;
    #endif // FEATURE_MODIFY
};


class FrameLifeHolder : public VirtualLightRefBase
{
    public:
                                        FrameLifeHolder(
                                            MINT32 const openId,
                                            StreamControl* const pCtrl,
                                            sp<IPipelineFrame> const& pFrame,
                                            MBOOL const enableLog
                                            )
                                            : mbEnableLog(enableLog)
                                            , mOpenId(openId)
                                            , mpStreamControl(pCtrl)
                                            , mpFrame(pFrame)
                                        {
                                            if  ( ATRACE_ENABLED() ) {
                                                mTraceName = String8::format("Cam:%d:IspP2|%d|request:%d frame:%d", mOpenId, mpFrame->getRequestNo(), mpFrame->getRequestNo(), mpFrame->getFrameNo());
                                                CAM_TRACE_ASYNC_BEGIN(mTraceName.string(), 0);
                                            }
                                            CAM_TRACE_ASYNC_BEGIN("P2F:FrameLifeHolder", mpFrame->getFrameNo());
                                            MY_LOGD_IF(mbEnableLog, "frame %zu +", mpFrame->getFrameNo());
                                        }

                                        ~FrameLifeHolder() {
                                            if  ( ! mTraceName.isEmpty() ) {
                                                CAM_TRACE_ASYNC_END(mTraceName.string(), 0);
                                            }
                                            if( mpStreamControl )
                                                mpStreamControl->onFrameDone(mpFrame);
                                            MY_LOGD_IF(mbEnableLog, "frame %zu -", mpFrame->getFrameNo());
                                            CAM_TRACE_ASYNC_END("P2F:FrameLifeHolder", mpFrame->getFrameNo());
                                        }

    public:
        MVOID                           onPartialFrameDone() {
                                            if( mpStreamControl )
                                                mpStreamControl->onPartialFrameDone(mpFrame);
                                        }

    private:
        MBOOL const                     mbEnableLog;
        MINT32 const                    mOpenId;
        String8                         mTraceName;
        StreamControl* const            mpStreamControl; //stream control & dispatch
        sp<IPipelineFrame> const        mpFrame;
};

class Request;
class Processor: virtual public RefBase
{
public:
    virtual MERROR   queueRequest(sp<Request>)           = 0;
    virtual MERROR   handleRequest(sp<Request>)          = 0;
    virtual MVOID    flushRequests()                     = 0;
    virtual MVOID    waitForIdle()                       = 0;
    virtual MVOID    setNextProcessor(wp<Processor>)     = 0;
    virtual MBOOL    isAsyncProcessor()                  = 0;
    virtual MVOID    callbackAsync(sp<Request>, MERROR)  = 0;
    virtual MBOOL    close()                             = 0;
};

class Request
    : public VirtualLightRefBase
{
public:
    struct Context
    {
        MUINT8                      burst_num;
        MBOOL                       resized;
        StreamId_T                  fd_stream_id;
        sp<BufferHandle>            in_buffer;
        sp<BufferHandle>            in_lcso_buffer;
        sp<Cropper::crop_info_t>    crop_info;
        vector<sp<BufferHandle> >   out_buffers;
        sp<BufferHandle>            in_mdp_buffer;
        sp<MetaHandle>              in_app_meta;
#ifdef FEATURE_MODIFY
        MINT32                      iso;
#endif // FEATURE_MODIFY
        sp<MetaHandle>              in_hal_meta;
        sp<MetaHandle>              out_app_meta;
        sp<MetaHandle>              out_hal_meta;
    };

    struct Context context;

    typedef enum
    {
        STATE_WAIT,
        STATE_SUSPEND,
        STATE_RUNNING,
        STATE_DONE,
    } RequestState_t;

    RequestState_t          uState;

    Request(sp<FrameLifeHolder> pFrameLifeHolder)
        : mpFrameLifeHolder(pFrameLifeHolder)
        , uState(STATE_WAIT)
        , index(0)
        , frameNo(0)
    {
    }

    MUINT32                    index;
    MUINT32                    frameNo;

    MVOID setCurrentOwner(wp<Processor> pProcessor)
    {
        mwpProcessor = pProcessor;
    }

    MVOID responseDone(MERROR status)
    {
        sp<Processor> spProcessor = mwpProcessor.promote();
        if(spProcessor.get())
        {
            MY_LOGD_IF(DEBUG_LOG, "perform callback %d[%d]",frameNo ,index);
            spProcessor->callbackAsync(this, status);
        }
    }

    MVOID onPartialRequestDone()
    {
        if(mpFrameLifeHolder.get())
            mpFrameLifeHolder->onPartialFrameDone();
    }

    ~Request()
    {
        if(context.in_buffer.get())
        {
            context.in_buffer.clear();
            MY_LOGW("context.in_buffer not released");
        }
        if(context.in_lcso_buffer.get())
        {
            context.in_lcso_buffer.clear();
            MY_LOGW("context.in_lcso_buffer not released");
        }
        if(context.in_mdp_buffer.get())
        {
            context.in_mdp_buffer.clear();
            MY_LOGW("context.in_mdp_buffer not released");
        }

        vector<sp<BufferHandle> >::iterator iter = context.out_buffers.begin();
        while(iter != context.out_buffers.end())
        {
            if((*iter).get())
            {
                MY_LOGW("context.out_buffers[0x%x] not released", (*iter)->getStreamId());
                (*iter).clear();
            }
            iter++;
        }

        if(context.in_app_meta.get())
        {
            context.in_app_meta.clear();
            MY_LOGW("context.in_app_meta not released");
        }
        if(context.in_hal_meta.get())
        {
            context.in_hal_meta.clear();
            MY_LOGW("context.in_hal_meta not released");
        }
        if(context.out_app_meta.get())
        {
            context.out_app_meta.clear();
            MY_LOGW("context.out_app_meta not released");
        }
        if(context.out_hal_meta.get())
        {
            context.out_hal_meta.clear();
            MY_LOGW("context.out_hal_meta not released");
        }

        onPartialRequestDone();
    }

private:
    sp<FrameLifeHolder>     mpFrameLifeHolder;
    wp<Processor>           mwpProcessor;

};


template<typename TProcedure>
struct ProcedureTraits {
    typedef typename TProcedure::InitParams  TInitParams;
    typedef typename TProcedure::FrameParams TProcParams;

    typedef MERROR (TProcedure::*TOnExtract)(Request*, TProcParams&);
    typedef MERROR (TProcedure::*TOnExecute)(sp<Request>,TProcParams const&);
    typedef MERROR (TProcedure::*TOnFinish)(TProcParams const&, MBOOL const);
    typedef MVOID  (TProcedure::*TOnFlush)();

    static constexpr TOnExtract   fnOnExtract     = &TProcedure::onExtractParams;
    static constexpr TOnExecute   fnOnExecute     = &TProcedure::onExecute;
    static constexpr TOnFinish    fnOnFinish      = &TProcedure::onFinish;
    static constexpr TOnFlush     fnOnFlush       = &TProcedure::onFlush;

    static constexpr MBOOL  isAsync               = TProcedure::isAsync;
};


template<typename TProcedure, typename TProcedureTraits = ProcedureTraits<TProcedure>>
class ProcessorBase : virtual public Processor, virtual public Thread
{
    typedef typename TProcedureTraits::TInitParams          TInitParams;
    typedef typename TProcedureTraits::TProcParams          TProcParams;

    typedef typename TProcedureTraits::TOnExtract           TOnExtract;
    typedef typename TProcedureTraits::TOnExecute           TOnExecute;
    typedef typename TProcedureTraits::TOnFinish            TOnFinish;
    typedef typename TProcedureTraits::TOnFlush             TOnFlush;


protected:
    TProcedure         mProcedure;

public:
    ProcessorBase(TInitParams const& initParams, char const* name)
        : Thread(false)
        , mProcedure(initParams)
        , mbRequestDrained(MTRUE)
        , mbExit(MFALSE)
        , mName(name)
    {
       run(name);
    }
    #define PROC_TAG(str) "[%s] " str, mName.string()
    virtual MBOOL close()
    {
        MY_LOGD_IF(DEBUG_LOG, PROC_TAG("close processor"));

        requestExit();
        join();
        waitForIdle();
        return MTRUE;
    }

    ~ProcessorBase()
    {
        MY_LOGD_IF(DEBUG_LOG, PROC_TAG("destroy processor"));
    }

    virtual MERROR queueRequest(sp<Request> pRequest)
    {
        Mutex::Autolock _l(mRequestLock);
        pRequest->setCurrentOwner(this);
        //  Make sure the request with a smaller frame number has a higher priority.
        vector<sp<Request> >::iterator it = mvPending.end();
        for (; it != mvPending.begin();)
        {
            --it;
            if (0 <= (MINT32)(pRequest->frameNo - (*it)->frameNo))
            {
                ++it; //insert(): insert before the current node
                break;
            }
        }
        mvPending.insert(it, pRequest);
        MY_LOGD_IF(DEBUG_LOG, PROC_TAG("after request[%d-%d] queued, pending:%d, running:%d"),
            pRequest->frameNo, pRequest->index, mvPending.size(), mvRunning.size());

        mRequestQueueCond.signal();
        return OK;
    }


    virtual MERROR handleRequest(sp<Request> pRequest)
    {
        TProcParams params;
        TOnExtract fnOnExtract = TProcedureTraits::fnOnExtract;
        TOnExecute fnOnExecute = TProcedureTraits::fnOnExecute;
        TOnFinish fnOnFinish = TProcedureTraits::fnOnFinish;

        if (OK == (mProcedure.*fnOnExtract)(pRequest.get(), params))
        {
            MERROR ret = (mProcedure.*fnOnExecute)(pRequest, params);
            if(isAsyncProcessor() && ret == OK)
            {
                // do aync processing
                Mutex::Autolock _l(mCallbackLock);
                mvRunning.push_back(make_pair(pRequest,params));
                MY_LOGD_IF(DEBUG_LOG, PROC_TAG("request[%d-%d], pending:%d, running:%d"),
                    pRequest->frameNo, pRequest->index, mvPending.size(), mvRunning.size());
                mCallbackCond.signal();
            }
            else
            {
                (mProcedure.*fnOnFinish)(params, ret == OK);
                // trigger to release buffer
                params = TProcParams();
                pRequest->onPartialRequestDone();

                sp<Processor> spProcessor = mwpNextProcessor.promote();
                if (spProcessor.get())
                {
                    spProcessor->queueRequest(pRequest);
                }
            }
            return ret;
        }
        return OK;
    }

    virtual MVOID callbackAsync(sp<Request> pRequest, MERROR status)
    {
        if (isAsyncProcessor())
        {
            Mutex::Autolock _l1(mAsyncLock);
            Mutex::Autolock _l2(mCallbackLock);
            MY_LOGD_IF(DEBUG_LOG, PROC_TAG("request[%d-%d], pending:%d, running:%d"),
                pRequest->frameNo, pRequest->index, mvPending.size(), mvRunning.size());
            TOnFinish fnOnFinish = &TProcedure::onFinish;

            MBOOL isFound = MFALSE;

            do
            {
                typename vector<pair<sp<Request>,TProcParams> >::iterator iter = mvRunning.begin();
                for(; iter != mvRunning.end(); iter++)
                   if((*iter).first == pRequest)
                   {
                       (mProcedure.*fnOnFinish)((*iter).second, status == OK);
                       mvRunning.erase(iter);
                       isFound = MTRUE;
                       break;
                   }

                if(!isFound)
                {
                     MY_LOGW_IF(1 ,PROC_TAG("request[%d-%d] callback faster than execution finished"),
                        pRequest->frameNo, pRequest->index);
                     mCallbackCond.wait(mCallbackLock);
                }
            } while (!isFound);

            MY_LOGD_IF(DEBUG_LOG, PROC_TAG("request callback async, status:%d"), status);
            pRequest->onPartialRequestDone();

            mAsyncCond.signal();
            sp<Processor> spProcessor = mwpNextProcessor.promote();
            if (spProcessor.get())
            {
                spProcessor->queueRequest(pRequest);
            }
        }
        return;
    }

    virtual MVOID setNextProcessor(wp<Processor> pProcessor)
    {
        mwpNextProcessor = pProcessor;
    }

    virtual MBOOL isAsyncProcessor()
    {
        return TProcedureTraits::isAsync;
    }

public:
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Thread Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // Ask this object's thread to exit. This function is asynchronous, when the
    // function returns the thread might still be running. Of course, this
    // function can be called from a different thread.
    virtual void requestExit()
    {
        Mutex::Autolock _l(mRequestLock);
        mbExit = MTRUE;
        mRequestQueueCond.signal();
    }

    // Good place to do one-time initializations
    virtual status_t readyToRun()
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
        if (P2THREAD_POLICY == SCHED_OTHER) {
            sched_p.sched_priority = 0;
            ::sched_setscheduler(0, P2THREAD_POLICY, &sched_p);
            ::setpriority(PRIO_PROCESS, 0, P2THREAD_PRIORITY);   //  Note: "priority" is nice value.
        } else {
            sched_p.sched_priority = P2THREAD_PRIORITY;          //  Note: "priority" is real-time priority.
            ::sched_setscheduler(0, P2THREAD_POLICY, &sched_p);
        }

        MY_LOGD_IF(1 ,PROC_TAG("tid(%d) policy(%d) priority(%d)"), ::gettid(), P2THREAD_POLICY, P2THREAD_PRIORITY);

        return OK;

    }

private:

    // Derived class must implement threadLoop(). The thread starts its life
    // here. There are two ways of using the Thread object:
    // 1) loop: if threadLoop() returns true, it will be called again if
    //          requestExit() wasn't called.
    // 2) once: if threadLoop() returns false, the thread will exit upon return.
    virtual bool threadLoop()
    {
        while(!exitPending() && OK == onWaitRequest())
        {
            sp<Request> pRequest = NULL;
            {
                Mutex::Autolock _l(mRequestLock);

                if( mvPending.size() == 0 )
                {
                    MY_LOGW_IF(1 ,PROC_TAG("no request"));
                    return true;
                }

                pRequest = mvPending.front();
                mvPending.erase( mvPending.begin());
            }

            MY_LOGW_IF(handleRequest(pRequest) != OK, "request execute failed");

            return true;
        }
        MY_LOGD_IF(DEBUG_LOG, PROC_TAG("exit thread"));
        return  false;
    }

public:

    virtual MVOID flushRequests()
    {
        FUNC_START;

        Mutex::Autolock _l(mRequestLock);

        mvPending.clear();

        if (!mbRequestDrained)
        {
            MY_LOGD_IF(1 ,PROC_TAG("wait for request drained"));
            mRequestDrainedCond.wait(mRequestLock);
        }

        TOnFlush fnOnFlush = TProcedureTraits::fnOnFlush;
        (mProcedure.*fnOnFlush)();

        FUNC_END;
        return;
    }

    virtual MVOID waitForIdle()
    {
        if(isAsyncProcessor())
            return;

        Mutex::Autolock _l(mAsyncLock);
        while(mvRunning.size())
        {
            MY_LOGD_IF(1 ,PROC_TAG("wait request done %d"), mvRunning.size());
            mAsyncCond.wait(mAsyncLock);
        }

        return;
    }

    virtual MERROR onWaitRequest()
    {
        Mutex::Autolock _l(mRequestLock);
        while (!mvPending.size() && !mbExit)
        {
            // set drained flag
            mbRequestDrained = MTRUE;
            mRequestDrainedCond.signal();

            status_t status = mRequestQueueCond.wait(mRequestLock);
            if (OK != status)
            {
                MY_LOGW_IF(1 ,PROC_TAG("wait status:%d:%s, request size:%zu, exit:%d"),
                    status, ::strerror(-status), mvPending.size(), mbExit
                );
            }
        }

        if (mbExit)
        {
            MY_LOGW_IF(mvPending.size(), PROC_TAG("existed mvPending.size:%zu"), mvPending.size());
            return DEAD_OBJECT;
        }

        mbRequestDrained = MFALSE;
        return OK;
    }

protected:
    vector<sp<Request> >     mvPending;
    // for async request
    vector<pair<sp<Request>,TProcParams> >
                                    mvRunning;
    wp<Processor>                   mwpNextProcessor;
    mutable Mutex                   mRequestLock;
    mutable Condition               mRequestQueueCond;
    mutable Condition               mRequestDrainedCond;
    MBOOL                           mbRequestDrained;
    MBOOL                           mbExit;
    mutable Mutex                   mAsyncLock;
    mutable Condition               mAsyncCond;
    mutable Mutex                   mCallbackLock;
    mutable Condition               mCallbackCond;
    String8                         mName;

};


class P2Procedure
{
    protected:
        class MultiFrameHandler
        {
            public:
                MultiFrameHandler(IStreamingFeaturePipe* const pPipe, MBOOL bEableLog)
                    : muMfEnqueCnt(0)
                    , muMfDequeCnt(0)
                    , mbEnableLog(bEableLog)
                    , mpPipe(pPipe)
                {
                }
                MERROR                      collect(sp<Request>, FeaturePipeParam&);
                MVOID                       flush();
                static MVOID                callback(QParams& rParams)
                {
                    MultiFrameHandler* pHandler = reinterpret_cast<MultiFrameHandler*>(rParams.mpCookie);
                    pHandler->handleCallback(rParams);
                }
                MVOID                       handleCallback(QParams& rParams);

            private:
                IStreamingFeaturePipe* const mpPipe;
                mutable Mutex               mLock;
                MBOOL const                 mbEnableLog;
                QParams                     mParamCollecting;
                MUINT32                     muMfEnqueCnt;
                MUINT32                     muMfDequeCnt;
                vector<sp<Request> >        mvReqCollecting;
                vector<vector<sp<Request> > > mvRunning;
        };


    public:
        struct InitParams
        {
            MINT32                          openId;
            P2FeatureNode::ePass2Type       type;
            MRect                           activeArray;
            IHal3A_T*                       p3A;
            IStreamingFeaturePipe*          pPipe;
            MBOOL                           enableLog;
            MBOOL                           enableDumpBuffer;
            #ifdef FEATURE_MODIFY
            P2FeatureNode::UsageHint        usageHint;
            #endif // FEATURE_MODIFY
        };

        struct FrameInput
        {
            PortID                          mPortId;
            sp<BufferHandle>                mHandle;
        };

        struct FrameOutput
        {
            FrameOutput()
            : mUsage(0)
            , mTransform(0)
            {};
            PortID                          mPortId;
            sp<BufferHandle>                mHandle;
            MINT32                          mTransform;
            MUINT32                         mUsage;
        };

        struct FrameParams
        {
            FrameInput           in;
            FrameInput           in_lcso;
            Vector<FrameOutput>  vOut;
            //
            MBOOL                bResized;
            //
            sp<MetaHandle>       inApp;
            sp<MetaHandle>       inHal;
            sp<MetaHandle>       outApp;
            sp<MetaHandle>       outHal;
        };

    private: //private use structures
        struct eis_region
        {
            MUINT32 x_int;
            MUINT32 x_float;
            MUINT32 y_int;
            MUINT32 y_float;
            MSize   s;
#if SUPPORT_EIS_MV
            MUINT32 x_mv_int;
            MUINT32 x_mv_float;
            MUINT32 y_mv_int;
            MUINT32 y_mv_float;
            MUINT32 is_from_zzr;
#endif
#ifdef FEATURE_MODIFY
            // 3dnr vipi: needs x_int/y_int/gmvX/gmvY
            MINT32 gmvX;
            MINT32 gmvY;
#endif // FEATURE_MODIFY
        };

    public:
        static sp<Processor>            createProcessor(InitParams& params);
                                        ~P2Procedure();

                                        P2Procedure(InitParams const& params)
                                            : mInitParams(params)
                                            , mpPipe(params.pPipe)
                                            , mp3A(params.p3A)
                                            , muEnqueCnt(0)
                                            , muDequeCnt(0)
                                            , mDebugScanLineMask(0)
                                            , mpDebugScanLine(NULL)
                                            , mbEnableLog(params.enableLog)
                                            , mbEnableDumpBuffer(params.enableDumpBuffer)
                                            , mOpenId(params.openId)
                                        {
                                            mpMultiFrameHandler = new MultiFrameHandler(
                                                params.pPipe, params.enableLog);

                                            char cProperty[PROPERTY_VALUE_MAX];
                                            ::property_get("debug.camera.scanline.p2", cProperty, "0");
                                            mDebugScanLineMask = ::atoi(cProperty);
                                            if ( mDebugScanLineMask != 0)
                                            {
                                                mpDebugScanLine = DebugScanLine::createInstance();
                                            }

#ifdef FEATURE_MODIFY
                                            //disable/enable LCE
                                            mbIsDisabLCE = ::property_get_int32("isp.lce.disable", 0);
#endif // FEATURE_MODIFY
                                        }

        MERROR                          onExtractParams(Request*, FrameParams&);

    public:
        MERROR                          waitIdle();

    public:                             // used by job
        static const MBOOL              isAsync = MTRUE;

        MERROR                          onExecute(
                                            sp<Request> const pRequest,
                                            FrameParams const& params
                                        );

        MERROR                          onFinish(
                                            FrameParams const& params,
                                            MBOOL const success
                                        );

        MVOID                           onFlush();

    protected:

        MERROR                          mapPortId(
                                            StreamId_T const streamId, // [in]
                                            MUINT32 const transform,   // [in]
                                            MBOOL const isFdStream,    // [in]
                                            MUINT32& rOccupied,        // [in/out]
                                            PortID&  rPortId           // [out]
                                        ) const;

        MERROR                          checkParams(FrameParams const params) const;
#ifdef FEATURE_MODIFY
        MERROR                          getCropInfos_NoEIS(
                                            IMetadata* const inApp,
                                            IMetadata* const inHal,
                                            MBOOL const isResized,
                                            MSize const& dstSize,
                                            MCropRect& result
                                        ) const;
#endif
        MERROR                          getCropInfos(
                                            IMetadata* const inApp,
                                            IMetadata* const inHal,
                                            MBOOL const isResized,
                                            Cropper::crop_info_t& cropInfos
                                        ) const;

        MVOID                           queryCropRegion(
                                            IMetadata* const meta_request,
                                            MBOOL const isEisOn,
                                            MRect& targetCrop
                                        ) const;

        MVOID                           updateCropRegion(
                                            MRect const crop,
                                            IMetadata* meta_result
                                        ) const;

        MBOOL                           isEISOn(
                                            IMetadata* const inApp
                                        ) const;

        MBOOL                           queryEisRegion(
                                            IMetadata* const inHal,
                                            eis_region& region
                                        ) const;

#ifdef FEATURE_MODIFY
        MVOID                           prepareFeaturePipeParam(
                                          FeaturePipeParam &featureEnqueParams,
                                          const sp<Request> &pRequest,
                                          IMetadata *pMeta_InApp,
                                          IMetadata *pMeta_InHal,
                                          IMetadata *pMeta_OutApp,
                                          IMetadata *pMeta_OutHal,
                                          const Cropper::crop_info_t &cropInfos);
        MBOOL                           setP2B3A(FeaturePipeParam &param);
        MVOID                           drawScanLine(const Output &output);
        static MVOID                    partialRelease(FeaturePipeParam &param);
        static MBOOL                    featurePipeCB(FeaturePipeParam::MSG_TYPE msg, FeaturePipeParam &data);

        MVOID prepare3DNR_SL2E_Info(
            IMetadata *pMeta_InApp,
            IMetadata *pMeta_InHal,
            QParams &enqueParams);

        MVOID prepareFeatureData_3DNR(
            FeaturePipeParam &featureEnqueParams,
            MINT32 dstsize_resizer_w, MINT32 dstsize_resizer_h,
            MINT32 iso, IMetadata *pMeta_InApp, IMetadata *pMeta_InHal);

        MBOOL isCRZApplied(IMetadata* const inApp, QParams& rParams);

#endif // FEATURE_MODIFY
        static MVOID                    pass2CbFunc(QParams& rParams);

        MVOID                           handleDeque(QParams& rParams);

    private:
        MultiFrameHandler*            mpMultiFrameHandler;
        MBOOL const                     mbEnableLog;
        MBOOL const                     mbEnableDumpBuffer;
#ifdef FEATURE_MODIFY
        MINT                            mOpenId;
        MBOOL                           mbIsDisabLCE;
#endif // FEATURE_MODIFY

        //
        mutable Mutex                   mLock;
        mutable Condition               mCondJob;
        //
        InitParams const                mInitParams;
        //
        IStreamingFeaturePipe* const    mpPipe;
        IHal3A_T* const                 mp3A;
        //
        MUINT32                         muEnqueCnt;
        MUINT32                         muDequeCnt;
        vector<sp<Request> >            mvRunning;
        //
        #define DRAWLINE_PORT_WDMAO     0x1
        #define DRAWLINE_PORT_WROTO     0x2
        #define DRAWLINE_PORT_IMG2O     0x4
        MUINT32                         mDebugScanLineMask;
        DebugScanLine*                  mpDebugScanLine;

#if P2_DEQUE_DEBUG
        vector<QParams>                 mvParams;
#endif
};


class MDPProcedure
{
    public:
        struct InitParams
        {
            MBOOL                          enableLog;
        };

        struct FrameInput
        {
            sp<BufferHandle>                mHandle;
        };

        struct FrameOutput
        {
            sp<BufferHandle>                mHandle;
            MINT32                          mTransform;
        };

        struct FrameParams
        {
            sp<Cropper::crop_info_t>    pCropInfo;
            FrameInput                  in;
            Vector<FrameOutput>         vOut;
        };
    public:
        static sp<Processor>            createProcessor(InitParams& params);
                                        ~MDPProcedure() {}
                                        MDPProcedure(InitParams const& params)
                                        : mbEnableLog(params.enableLog)
                                        {}

        MERROR                          onExtractParams(Request*, FrameParams&);

    protected:

                                        MDPProcedure(MBOOL const enableLog)
                                        : mbEnableLog(enableLog)
                                        {}

    public:
        static const MBOOL              isAsync = MFALSE;

        MERROR                          waitIdle() { return OK; } // since is synchronous

        MERROR                          onExecute(
                                            sp<Request> const pRequest,

                                            FrameParams const& params
                                        );

        MERROR                          onFinish(
                                            FrameParams const& params,
                                            MBOOL const success
                                        );

        MVOID                           onFlush(){};

    private:
        MBOOL const                     mbEnableLog;

};


/******************************************************************************
 *
 ******************************************************************************/
class P2NodeImp
    : virtual public BaseNode
    , virtual public P2FeatureNode
    , public StreamControl
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                                            Definitions.
    typedef android::sp<IPipelineFrame>                     QueNode_T;
    typedef android::List<QueNode_T>                        Que_T;
    //
public:     ////                    Operations.

                                    P2NodeImp(ePass2Type const type, const UsageHint &usageHint);

    virtual                         ~P2NodeImp();

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
    MVOID                           onProcessFrame(
                                        android::sp<IPipelineFrame> const& pFrame
                                    );
    MERROR                          verifyConfigParams(
                                        ConfigParams const & rParams
                                    ) const;

public:     ////                    StreamControl

    MERROR                          getInfoIOMapSet(
                                        sp<IPipelineFrame> const& pFrame,
                                        IPipelineFrame::InfoIOMapSet& rIOMapSet
                                    ) const;

    MBOOL                           isInImageStream(
                                        StreamId_T const streamId
                                    ) const;

    MBOOL                           isInMetaStream(
                                        StreamId_T const streamId
                                    ) const;

    MERROR                          acquireImageStream(
                                        sp<IPipelineFrame> const& pFrame,
                                        StreamId_T const streamId,
                                        sp<IImageStreamBuffer>& rpStreamBuffer
                                    );

    MVOID                           releaseImageStream(
                                        sp<IPipelineFrame> const& pFrame,
                                        sp<IImageStreamBuffer> const pStreamBuffer,
                                        MUINT32 const status
                                    ) const;

    MERROR                          acquireImageBuffer(
                                        StreamId_T const streamId,
                                        sp<IImageStreamBuffer> const pStreamBuffer,
                                        sp<IImageBuffer>& rpImageBuffer
                                    ) const;

    MVOID                           releaseImageBuffer(
                                        sp<IImageStreamBuffer> const rpStreamBuffer,
                                        sp<IImageBuffer> const pImageBuffer
                                    ) const;

    MERROR                          acquireMetaStream(
                                        sp<IPipelineFrame> const& pFrame,
                                        StreamId_T const streamId,
                                        sp<IMetaStreamBuffer>& rpStreamBuffer
                                    );

    MVOID                           releaseMetaStream(
                                        sp<IPipelineFrame> const& pFrame,
                                        sp<IMetaStreamBuffer> const pStreamBuffer,
                                        MUINT32 const status
                                    ) const;

    MERROR                          acquireMetadata(
                                        StreamId_T const streamId,
                                        sp<IMetaStreamBuffer> const pStreamBuffer,
                                        IMetadata*& rpMetadata
                                    ) const;

    MVOID                           releaseMetadata(
                                        sp<IMetaStreamBuffer> const pStreamBuffer,
                                        IMetadata* const pMetadata
                                    ) const;

    MVOID                           onPartialFrameDone(
                                        sp<IPipelineFrame> const& pFrame
                                    );

    MVOID                           onFrameDone(
                                        sp<IPipelineFrame> const& pFrame
                                    );

public:

    MERROR                          mapToRequests(
                                        android::sp<IPipelineFrame> const& pFrame
                                    );

inline MBOOL                        isFullRawLocked(StreamId_T const streamId) const {
                                        for( size_t i = 0; i < mpvInFullRaw.size(); i++ ) {
                                            if( isStream(mpvInFullRaw[i], streamId) )
                                                return MTRUE;
                                        }
                                        return MFALSE;
                                    }

inline MBOOL                        isResizeRawLocked(StreamId_T const streamId) const {
                                        return isStream(mpInResizedRaw, streamId);
                                    }

inline MBOOL                        isLcsoRawLocked(StreamId_T const streamId) const {
                                        return isStream(mpInLcsoRaw, streamId);
                                    }

protected:  ////                    LOGE & LOGI on/off
    MINT32                          mLogLevel;

protected:  ////                    Data Members. (Config)
    ePass2Type const                mType;
    mutable RWLock                  mConfigRWLock;
    // meta
    sp<IMetaStreamInfo>             mpInAppMeta_Request;
    sp<IMetaStreamInfo>             mpInAppRetMeta_Request;
    sp<IMetaStreamInfo>             mpInHalMeta_P1;
    sp<IMetaStreamInfo>             mpOutAppMeta_Result;
    sp<IMetaStreamInfo>             mpOutHalMeta_Result;

    // image
    Vector<sp<IImageStreamInfo> >   mpvInFullRaw;
    sp<IImageStreamInfo>            mpInResizedRaw;
    sp<IImageStreamInfo>            mpInLcsoRaw;
    ImageStreamInfoSetT             mvOutImages;
    sp<IImageStreamInfo>            mpOutFd;
    // feature
    MUINT8 mBurstNum;
#ifdef FEATURE_MODIFY
    UsageHint                       mUsageHint;
#endif // FEATURE_MODIFY

protected:  ////                    Data Members. (Operation)
    mutable Mutex                   mOperationLock;

private:
    sp<Processor>                   mpP2Processor;
    sp<Processor>                   mpMdpProcessor;
};

/******************************************************************************
 *
 ******************************************************************************/
#ifdef FEATURE_MODIFY
MBOOL isAPEnabled_3DNR(MINT32 force3DNR, IMetadata *appInMeta)
{
    MINT32 e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;

    if( appInMeta == NULL ||
        !tryGetMetadata<MINT32>(appInMeta, MTK_NR_FEATURE_3DNR_MODE, e3DnrMode) )
    {
        MY_LOGW("no MTK_NR_FEATURE_3DNR_MODE: appInMeta: %p", appInMeta);
    }

    if (force3DNR)
    {
        char EnableOption[PROPERTY_VALUE_MAX] = {'\0'};
        property_get("camera.3dnr.enable", EnableOption, "n");
        if (EnableOption[0] == '1')
        {
            e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_ON;
        }
        else if (EnableOption[0] == '0')
        {
            e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;
        }
    }
    return (e3DnrMode == MTK_NR_FEATURE_3DNR_MODE_ON);
}

MBOOL isAPEnabled_VHDR(IMetadata *halInMeta)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("isp.lce.disable", value, "0"); // 0: enable, 1: disable
    MBOOL bDisable = atoi(value);

    if(bDisable) {
        return MFALSE;
    } else {
        return MTRUE;
    }
#if 0
    // Because LCEI module will be always on, so this always returns true except VFB on.
    if( isVFBOn)
        return MFALSE;

    IMetadata::IEntry entry = halInMeta->entryFor(MTK_VHDR_LCEI_DATA);
    return ! entry.isEmpty();
#endif
}

MBOOL isHALenabled_VHDR(IMetadata *halInMeta)
{
    MBOOL enabledVHDR = MFALSE;
    MINT32 vhdrMode = SENSOR_VHDR_MODE_NONE;
    tryGetMetadata<MINT32>(halInMeta, MTK_P1NODE_SENSOR_VHDR_MODE, vhdrMode);

    if(vhdrMode != SENSOR_VHDR_MODE_NONE)
    {
        enabledVHDR = MTRUE;
    }
    return enabledVHDR;
}

MBOOL isAPEnabled_VFB(IMetadata *appInMeta)
{
    return MFALSE;
}

MBOOL isAPEnabled_VFB_EX(IMetadata *appInMeta)
{
    return MFALSE;
}

MBOOL isAPEnabled_EIS(IMetadata *appInMeta)
{
    MBOOL enabledStreamingEIS = MFALSE;
    MUINT8 eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF;

    if (!tryGetMetadata<MUINT8>(appInMeta, MTK_CONTROL_VIDEO_STABILIZATION_MODE, eisMode))
    {
        MY_LOGW("no MTK_CONTROL_VIDEO_STABILIZATION_MODE");
        return MFALSE;

    }

    enabledStreamingEIS = (eisMode == MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON) ? MTRUE : MFALSE;

    return enabledStreamingEIS;
    //return MFALSE;
}


#ifdef FEATURE_MODIFY
MVOID P2Procedure::prepare3DNR_SL2E_Info(
    IMetadata *pMeta_InApp,
    IMetadata *pMeta_InHal,
    QParams &enqueParams)
{
    if (pMeta_InApp == NULL || pMeta_InHal == NULL)
    {
        MY_LOGE("sensor(%d) no meta inApp: %p or no meta inHal: %p",
            mOpenId, pMeta_InApp, pMeta_InHal);
        return;
    }

    if (isAPEnabled_3DNR((mInitParams.usageHint.m3DNRMode &
        P2FeatureNode::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT), pMeta_InApp))
    {
        if( pMeta_InHal != NULL && enqueParams.mvIn.size() > 0)
        {
            if (isCRZApplied(pMeta_InApp, enqueParams))
            {
                MY_LOGD_IF(mbEnableLog, "isCRZApplied == TRUE");
                // 1. imgi w/h
                MSize sl2eOriSize;
                sl2eOriSize.w = (enqueParams.mvIn[0].mBuffer)->getImgSize().w;
                sl2eOriSize.h = (enqueParams.mvIn[0].mBuffer)->getImgSize().h;
                // updateEntry<MSize>(request , MTK_ISP_P2_ORIGINAL_SIZE ,  (params.mvIn[0].mBuffer)->getImgSize() );
                trySetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_ORIGINAL_SIZE, sl2eOriSize);
        // verify
                MSize vv_sl2eOriSize;
                tryGetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_ORIGINAL_SIZE, vv_sl2eOriSize);
                MY_LOGD_IF(mbEnableLog, "sensor(%d) vv_3dnr.sl2e.imgi (w,h)=(%d,%d)", mOpenId, vv_sl2eOriSize.w, vv_sl2eOriSize.h);

                // 2. crop info: x, y, w, h
                MRect sl2eCropInfo;
                sl2eCropInfo.p.x = enqueParams.mvCropRsInfo[0].mCropRect.p_integral.x;
                sl2eCropInfo.p.y = enqueParams.mvCropRsInfo[0].mCropRect.p_integral.y;
                sl2eCropInfo.s.w = enqueParams.mvCropRsInfo[0].mCropRect.s.w;
                sl2eCropInfo.s.h = enqueParams.mvCropRsInfo[0].mCropRect.s.h;
                // updateEntry<MRect>(request , MTK_ISP_P2_CROP_REGION ,   request->mFullImgSize.w );
                trySetMetadata<MRect>(pMeta_InHal, MTK_ISP_P2_CROP_REGION, sl2eCropInfo);
        // verify
                MRect vv_sl2eCropInfo;
                tryGetMetadata<MRect>(pMeta_InHal, MTK_ISP_P2_CROP_REGION, vv_sl2eCropInfo);
                MY_LOGD_IF(mbEnableLog, "sensor(%d) vv_3dnr.sl2e.crop (x,y,w,h)=(%d,%d,%d,%d)",
                    mOpenId, vv_sl2eCropInfo.p.x,  vv_sl2eCropInfo.p.y, vv_sl2eCropInfo.s.w, vv_sl2eCropInfo.s.h);

                // 3. img3o w/h
                MSize sl2eRrzSize;
                sl2eRrzSize.w = enqueParams.mvCropRsInfo[0].mResizeDst.w;
                sl2eRrzSize.h = enqueParams.mvCropRsInfo[0].mResizeDst.h;
                // updateEntry<MSize>(request , MTK_ISP_P2_RESIZER_SIZE ,  request->mFullImgSize);
                trySetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_RESIZER_SIZE, sl2eRrzSize);
        // verify
                MSize vv_sl2eRrzSize;
                tryGetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_RESIZER_SIZE, vv_sl2eRrzSize);
                MY_LOGD_IF(mbEnableLog, "sensor(%d) vv_3dnr.sl2e.rrz (w,h)=(%d,%d)", mOpenId, vv_sl2eRrzSize.w, vv_sl2eRrzSize.h);
            }
            else /* 1:1 */
            {
                MY_LOGD_IF(mbEnableLog, "isCRZApplied == FALSE");

                // 1. imgi w/h
                MSize sl2eOriSize;
                sl2eOriSize.w = (enqueParams.mvIn[0].mBuffer)->getImgSize().w;
                sl2eOriSize.h = (enqueParams.mvIn[0].mBuffer)->getImgSize().h;
                // updateEntry<MSize>(request , MTK_ISP_P2_ORIGINAL_SIZE ,  (params.mvIn[0].mBuffer)->getImgSize() );
                trySetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_ORIGINAL_SIZE, sl2eOriSize);

    // verify
                MSize vv_sl2eOriSize;
                tryGetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_ORIGINAL_SIZE, vv_sl2eOriSize);
                MY_LOGD_IF(mbEnableLog, "sensor(%d) vv_3dnr.sl2e.imgi (w,h)=(%d,%d)", mOpenId, vv_sl2eOriSize.w, vv_sl2eOriSize.h);


                // 2. crop info: x, y, w, h
                MRect sl2eCropInfo;
                sl2eCropInfo.p.x = 0;
                sl2eCropInfo.p.y = 0;
                sl2eCropInfo.s.w = (enqueParams.mvIn[0].mBuffer)->getImgSize().w;
                sl2eCropInfo.s.h = (enqueParams.mvIn[0].mBuffer)->getImgSize().h;
                // updateEntry<MRect>(request , MTK_ISP_P2_CROP_REGION ,   request->mFullImgSize.w );
                trySetMetadata<MRect>(pMeta_InHal, MTK_ISP_P2_CROP_REGION, sl2eCropInfo);
    // verify
                MRect vv_sl2eCropInfo;
                tryGetMetadata<MRect>(pMeta_InHal, MTK_ISP_P2_CROP_REGION, vv_sl2eCropInfo);
                MY_LOGD_IF(mbEnableLog, "sensor(%d) vv_3dnr.sl2e.crop (x,y,w,h)=(%d,%d,%d,%d)",
                    mOpenId, vv_sl2eCropInfo.p.x,  vv_sl2eCropInfo.p.y, vv_sl2eCropInfo.s.w, vv_sl2eCropInfo.s.h);

                // 3. img3o w/h
                MSize sl2eRrzSize;
                sl2eRrzSize.w = (enqueParams.mvIn[0].mBuffer)->getImgSize().w;
                sl2eRrzSize.h = (enqueParams.mvIn[0].mBuffer)->getImgSize().h;
                // updateEntry<MSize>(request , MTK_ISP_P2_RESIZER_SIZE ,  request->mFullImgSize);
                trySetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_RESIZER_SIZE, sl2eRrzSize);
    // verify
                MSize vv_sl2eRrzSize;
                tryGetMetadata<MSize>(pMeta_InHal, MTK_ISP_P2_RESIZER_SIZE, vv_sl2eRrzSize);
                MY_LOGD_IF(mbEnableLog, "sensor(%d) vv_3dnr.sl2e.rrz (w,h)=(%d,%d)", mOpenId, vv_sl2eRrzSize.w, vv_sl2eRrzSize.h);
            }
        }
    }

    return;
}
#endif // FEATURE_MODIFY


MVOID P2Procedure::prepareFeatureData_3DNR(
    FeaturePipeParam &featureEnqueParams,
    MINT32 dstsize_resizer_w, MINT32 dstsize_resizer_h,
    MINT32 iso, IMetadata *pMeta_InApp, IMetadata *pMeta_InHal
    )
{
#define INVALID_ISO_VALUE (-1)

    MY_LOGD_IF(mbEnableLog, "sensor(%d) HAS_3DNR: 0x%x", mOpenId, HAS_3DNR(featureEnqueParams.mFeatureMask) );

    if (HAS_3DNR(featureEnqueParams.mFeatureMask) )
    {
        MBOOL tmp_isEISOn = isEISOn(pMeta_InApp);
        eis_region eisInfo;
        memset(&eisInfo, 0, sizeof(eis_region));
        queryEisRegion(pMeta_InHal, eisInfo);

        MINT32 force3DNR = (mInitParams.usageHint.m3DNRMode &
                            P2FeatureNode::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT);
        MINT32 i4IsoThreshold = get_3dnr_off_iso_threshold(force3DNR);
        MINT32 i4GmvThreshold = get_3dnr_gmv_threshold(force3DNR);

        if (iso < i4IsoThreshold || abs(eisInfo.gmvX)/256 > i4GmvThreshold || abs(eisInfo.gmvY)/256 > i4GmvThreshold)
        {
            MY_LOGD_IF(mbEnableLog, "DISABLE 3DNR: due to iso(%d) < %d or |gmvX(%d/256)|=%d >= %d or |gmvY(%d/256)|=%d >= %d)",
                iso, i4IsoThreshold,
                eisInfo.gmvX, abs(eisInfo.gmvX)/256, i4GmvThreshold,
                eisInfo.gmvY, abs(eisInfo.gmvY)/256, i4GmvThreshold);
            featureEnqueParams.setFeatureMask(MASK_3DNR, 0);
            return;
        }

        // cropInfo
        featureEnqueParams.setVar<MUINT32>("3dnr.dstsize_resizer.w", dstsize_resizer_w);
        featureEnqueParams.setVar<MUINT32>("3dnr.dstsize_resizer.h", dstsize_resizer_h);

        #if 0 // debug usage
        {
            MUINT32 www = dstsize_resizer_w;
            MUINT32 hhh = dstsize_resizer_h;
            featureEnqueParams.setVar<MUINT32>("3dnr.dstsize_resizer.w", www);
            featureEnqueParams.setVar<MUINT32>("3dnr.dstsize_resizer.h", hhh);
            MY_LOGE("DEBUG_bbbb_cropInfo_test: w=%d, h=%d", dstsize_resizer_w, dstsize_resizer_h);

            MUINT32 aaaa_www = 0;
            MUINT32 aaaa_hhh = 0;
            aaaa_www = featureEnqueParams.getVar<MUINT32>("3dnr.dstsize_resizer.w", aaaa_www);
            aaaa_hhh = featureEnqueParams.getVar<MUINT32>("3dnr.dstsize_resizer.h", aaaa_hhh);
            MY_LOGE("P2FeatureNode_aaaa_cropInfo_test: w=%d, h=%d",
            aaaa_www, aaaa_hhh);
        }
        #endif


        featureEnqueParams.setVar<MBOOL>("3dnr.eis.isEisOn", tmp_isEISOn);
        featureEnqueParams.setVar<MUINT32>("3dnr.eis.x_int", eisInfo.x_int);
        featureEnqueParams.setVar<MUINT32>("3dnr.eis.y_int", eisInfo.y_int);
        featureEnqueParams.setVar<MUINT32>("3dnr.eis.gmvX", eisInfo.gmvX);
        featureEnqueParams.setVar<MUINT32>("3dnr.eis.gmvY", eisInfo.gmvY);

        MY_LOGD_IF(mbEnableLog, "sensor(%d) 3dnr.eis: isEisOn=%d, x_int=%d, y_int=%d, gmvX=%d, gmvY=%d, dstsize_resizer:w,h(%d,%d), iso(%d)",
            mOpenId,
            tmp_isEISOn,
            eisInfo.x_int,
            eisInfo.y_int,
            eisInfo.gmvX,
            eisInfo.gmvY,
            dstsize_resizer_w, dstsize_resizer_h,
            iso
            );

        // === ISO value ===
        if (iso != INVALID_ISO_VALUE)
        {
            featureEnqueParams.setVar<MUINT32>("3dnr.iso", iso);
        }
    }
}

MVOID prepareFeatureData_VHDR(FeaturePipeParam &pipeParam, IMetadata *pMeta_InHal)
{
}

MVOID prepareFeatureData_VFB(FeaturePipeParam &pipeParam)
{
    if( HAS_VFB(pipeParam.mFeatureMask) )
    {
    }
}

MVOID prepareFeatureData_EIS(FeaturePipeParam &pipeParam, IMetadata *pMeta_InHal)
{
    if( HAS_EIS(pipeParam.mFeatureMask) )
    {
        MINT32 GMV_X,GMV_Y,ConfX,ConfY;
        MINT32 ExpTime = 0,ihwTS = 0,ilwTS = 0;
        MINT32 eisMode = 0,eisCounter = 0;
        MINT64 ts = 0,tmp;
        GMV_X = GMV_Y = ConfX = ConfY = 0;

        IMetadata::IEntry entryRegion = pMeta_InHal->entryFor(MTK_EIS_REGION);
        if (entryRegion.count() > 14)
        {
            GMV_X = entryRegion.itemAt(9, Type2Type<MINT32>());
            GMV_Y = entryRegion.itemAt(10, Type2Type<MINT32>());
            ConfX = entryRegion.itemAt(11, Type2Type<MINT32>());
            ConfY = entryRegion.itemAt(12, Type2Type<MINT32>());
            ExpTime = entryRegion.itemAt(13, Type2Type<MINT32>());
            ihwTS = entryRegion.itemAt(14, Type2Type<MINT32>());
            ilwTS = entryRegion.itemAt(15, Type2Type<MINT32>());
            tmp = (MINT64)(ihwTS&0xFFFFFFFF);
            ts = (tmp<<32) + ((MINT64)ilwTS&0xFFFFFFFF);
            //MY_LOGD("EisHal TS hw: (%x)  lw: (%x) %lld", ihwTS,ilwTS,ts);

        }

        pipeParam.setVar<MINT32>("eis.gmv_x", GMV_X);
        pipeParam.setVar<MINT32>("eis.gmv_y", GMV_Y);
        pipeParam.setVar<MUINT32>("eis.confX", ConfX);
        pipeParam.setVar<MUINT32>("eis.confY", ConfY);
        pipeParam.setVar<MINT32>("eis.expTime", ExpTime);
        pipeParam.setVar<MINT64>("eis.timeStamp", ts);


        IMetadata::IEntry entryMode = pMeta_InHal->entryFor(MTK_EIS_MODE);
        if (entryMode.count() > 0) {
            eisMode = entryMode.itemAt(0, Type2Type<MINT32>());
            eisCounter = entryMode.itemAt(1, Type2Type<MINT32>());
        }
        MY_LOGD("EisHal ExpTime: %d, TS: %lld", ExpTime, ts);
        pipeParam.setVar<MINT32>("eis.eisMode", eisMode);
        pipeParam.setVar<MINT32>("eis.eisCounter", eisCounter);
        MY_LOGD("EisHal EisMode:%d EisCounter:%d", eisMode, eisCounter);

    }
}

MVOID makeDebugExif( const IMetadata *pMeta_InHal, IMetadata *pMeta_OutHal)
{
    MUINT8 isRequestExif = 0;
    tryGetMetadata<MUINT8>(pMeta_InHal, MTK_HAL_REQUEST_REQUIRE_EXIF, isRequestExif);

    if(isRequestExif)
    {
        MINT32 vhdrMode = SENSOR_VHDR_MODE_NONE;
        tryGetMetadata<MINT32>(pMeta_InHal, MTK_P1NODE_SENSOR_VHDR_MODE, vhdrMode);

        if(vhdrMode != SENSOR_VHDR_MODE_NONE)
        {
            // add HDR image flag
            std::map<MUINT32, MUINT32> debugInfoList;
            debugInfoList[MF_TAG_IMAGE_HDR] = 1;

            // get debug Exif metadata
            IMetadata exifMetadata;
            tryGetMetadata<IMetadata>(pMeta_OutHal, MTK_3A_EXIF_METADATA, exifMetadata);

            if (DebugExifUtils::setDebugExif(
                DebugExifUtils::DebugExifType::DEBUG_EXIF_MF,
                static_cast<MUINT32>(MTK_MF_EXIF_DBGINFO_MF_KEY),
                static_cast<MUINT32>(MTK_MF_EXIF_DBGINFO_MF_DATA),
                debugInfoList,
                &exifMetadata) != nullptr)
            {
                trySetMetadata<IMetadata>(pMeta_OutHal, MTK_3A_EXIF_METADATA, exifMetadata);
            }
        } // set HDR image flag end
    }
}

IStreamingFeaturePipe::UsageHint getPipeUsageHint(const P2FeatureNode::UsageHint &usage)
{
    IStreamingFeaturePipe::UsageHint pipeUsage;
    switch( usage.mUsageMode )
    {
    case P2FeatureNode::USAGE_PREVIEW:
        pipeUsage.mMode = IStreamingFeaturePipe::USAGE_P2A_FEATURE;
        break;

    case P2FeatureNode::USAGE_CAPTURE:
        pipeUsage.mMode = IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH;
        break;
    case P2FeatureNode::USAGE_TIMESHARE_CAPTURE:
        pipeUsage.mMode = IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH_TIME_SAHRING;
        break;

    case P2FeatureNode::USAGE_RECORD:
    default:
        pipeUsage.mMode = IStreamingFeaturePipe::USAGE_FULL;
        break;
    }
    pipeUsage.mStreamingSize = usage.mStreamingSize;
    pipeUsage.mEisMode = usage.mEisMode;
    if (usage.m3DNRMode & P2FeatureNode::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT)
        pipeUsage.enable3DNRModeMask(IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT);
    if (usage.m3DNRMode & P2FeatureNode::E3DNR_MODE_MASK_UI_SUPPORT)
        pipeUsage.enable3DNRModeMask(IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_UI_SUPPORT);
    if (usage.m3DNRMode & P2FeatureNode::E3DNR_MODE_MASK_SL2E_EN)
        pipeUsage.enable3DNRModeMask(IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_SL2E_EN);


    return pipeUsage;
}

#endif // FEATURE_MODIFY


/******************************************************************************
 *
 ******************************************************************************/
} // namespace P2Feature

android::sp<P2FeatureNode>
P2FeatureNode::
createInstance(ePass2Type const type, const UsageHint usageHint)
{
    if( type < 0 ||
        type >= PASS2_TYPE_TOTAL )
    {
        MY_LOGE("not supported p2 type %d", type);
        return NULL;
    }
    //
    return new P2Feature::P2NodeImp(type, usageHint);
}

namespace P2Feature {

/******************************************************************************
 *
 ******************************************************************************/
P2NodeImp::
P2NodeImp(ePass2Type const type, const UsageHint &usageHint)
    : BaseNode()
    , P2FeatureNode()
    //
    , mType(type)
    , mConfigRWLock()
    //
    , mpInAppMeta_Request()
    , mpInHalMeta_P1()
    , mpInAppRetMeta_Request()
    , mpOutAppMeta_Result()
    , mpOutHalMeta_Result()
    //
    , mpvInFullRaw()
    , mpInResizedRaw()
    , mvOutImages()
    , mpOutFd()
    //
    , mOperationLock()
    //
    , mpP2Processor(NULL)
    , mpMdpProcessor(NULL)
    , mBurstNum(0)
    , mUsageHint(usageHint)
{
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.camera.log", cLogLevel, "0");
    mLogLevel = atoi(cLogLevel);
    if ( mLogLevel == 0 ) {
        ::property_get("debug.camera.log.P2FeatureNode", cLogLevel, "0");
        mLogLevel = atoi(cLogLevel);
    }
#if 0 /*[EP_TEMP]*/ //[FIXME] TempTestOnly
    #warning "[FIXME] force enable P2FeatureNode log"
    if (mLogLevel < 2) {
        mLogLevel = 2;
    }
#endif
}


/******************************************************************************
 *
 ******************************************************************************/
P2NodeImp::
~P2NodeImp()
{
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
init(InitParams const& rParams)
{
    CAM_TRACE_NAME("P2F:init");
    FUNC_START;
    //
    mOpenId = rParams.openId;
    mNodeId = rParams.nodeId;
    mNodeName = rParams.nodeName;

    MY_LOGD("OpenId %d, nodeId %d, name %s",
            getOpenId(), getNodeId(), getNodeName() );
    //
    MRect activeArray;
    {
        sp<IMetadataProvider> pMetadataProvider =
            NSMetadataProviderManager::valueFor(getOpenId());
        if( ! pMetadataProvider.get() ) {
            MY_LOGE("sensor(%d) ! pMetadataProvider.get() ", mOpenId);
            return DEAD_OBJECT;
        }
        IMetadata static_meta =
            pMetadataProvider->geMtktStaticCharacteristics();
        if( tryGetMetadata<MRect>(&static_meta,
            MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION, activeArray) ) {
            MY_LOGD_IF(1,"sensor(%d) active array(%d, %d, %dx%d)",
                    mOpenId,
                    activeArray.p.x, activeArray.p.y,
                    activeArray.s.w, activeArray.s.h);
        } else {
            MY_LOGE("sensor(%d) no static info: MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION", mOpenId);
            #ifdef USING_MTK_LDVT /*[EP_TEMP]*/ //[FIXME] TempTestOnly
            activeArray = MRect(1600, 1200);// hard-code sensor size
            MY_LOGD("sensor(%d) set sensor size to active array(%d, %d, %dx%d)",
                mOpenId,
                activeArray.p.x, activeArray.p.y,
                activeArray.s.w, activeArray.s.h);
            #else
            return UNKNOWN_ERROR;
            #endif
        }
        //
        {
            mpP2Processor  = NULL;
            //
            P2Procedure::InitParams param;
            param.openId      = mOpenId;
            param.type        = mType;
            param.activeArray = activeArray;
            param.enableLog   = mLogLevel >= 1;
            param.enableDumpBuffer = mLogLevel >= 3;
            #ifdef FEATURE_MODIFY
            param.usageHint = mUsageHint;
            #endif // FEATURE_MODIFY
            //
            mpP2Processor = P2Procedure::createProcessor(param);
            if( mpP2Processor == NULL )
                return BAD_VALUE;
        }
        //
        {
            mpMdpProcessor = NULL;
            //
            MDPProcedure::InitParams param;
            param.enableLog   = mLogLevel >= 1;
            //
            mpMdpProcessor = MDPProcedure::createProcessor(param);
            if( mpMdpProcessor == NULL )
                return BAD_VALUE;
        }
        mpP2Processor->setNextProcessor(mpMdpProcessor);
    }
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
config(ConfigParams const& rParams)
{
    CAM_TRACE_NAME("P2F:config");
    //
    {
        MERROR const err = verifyConfigParams(rParams);
        if( err != OK ) {
            MY_LOGE("sensor(%d) verifyConfigParams failed, err = %d", mOpenId, err);
            return err;
        }
    }
    //
    flush();
    //
    {
        RWLock::AutoWLock _l(mConfigRWLock);
        // meta
        mpInAppMeta_Request  = rParams.pInAppMeta;
        mpInHalMeta_P1       = rParams.pInHalMeta;
        mpInAppRetMeta_Request = rParams.pInAppRetMeta;
        mpOutAppMeta_Result  = rParams.pOutAppMeta;
        mpOutHalMeta_Result  = rParams.pOutHalMeta;
        // image
        mpvInFullRaw         = rParams.pvInFullRaw;
        mpInResizedRaw       = rParams.pInResizedRaw;
        mpInLcsoRaw          = rParams.pInLcsoRaw;
        mvOutImages          = rParams.vOutImage;
        mpOutFd              = rParams.pOutFDImage;
        // property
        mBurstNum            = rParams.burstNum;
    }
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
uninit()
{
    CAM_TRACE_NAME("P2F:uninit");
    FUNC_START;
    //
    if ( OK != flush() )
        MY_LOGE("sensor(%d) flush failed", mOpenId);
    //
    if( mpP2Processor.get() ) {
        mpP2Processor->waitForIdle();
        mpP2Processor->close();
        //MY_LOGD("[P2] reference count:%d",mpP2Processor->getStrongCount());
        mpP2Processor = NULL;
    }
    //
    if( mpMdpProcessor.get() ) {
        mpMdpProcessor->waitForIdle();
        mpMdpProcessor->close();
        //MY_LOGD("MDP] reference count:%d",mpMdpProcessor->getStrongCount());
        mpMdpProcessor = NULL;
    }
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
flush()
{
    CAM_TRACE_NAME("P2F:flush");
    FUNC_START;
    //
    Mutex::Autolock _l(mOperationLock);
    // 1. wait for P2 thread
    mpP2Processor->flushRequests();
    // 2. wait for MDP thread
    mpMdpProcessor->flushRequests();
    //
    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
queue(android::sp<IPipelineFrame> pFrame)
{
    FUNC_START;
    //
    if( ! pFrame.get() ) {
        MY_LOGE("sensor(%d) Null frame", mOpenId);
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mOperationLock);
    MY_LOGD_IF(mLogLevel >= 2, "sensor(%d) queue pass2 @ frame(%d)", mOpenId, pFrame->getFrameNo());

    onProcessFrame(pFrame);

    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
onProcessFrame(
    android::sp<IPipelineFrame> const& pFrame
)
{
    //FUNC_START;
    if( mpP2Processor == NULL ) {
        MY_LOGW("sensor(%d) may not configured yet", mOpenId);
        BaseNode::flush(pFrame);
        return;
    }
    //
    // map IPipelineFrame to requests
    if( OK != mapToRequests(pFrame) ) {
        MY_LOGW("sensor(%d) map to jobs failed", mOpenId);
        BaseNode::flush(pFrame);
        return;
    }
    //
    //FUNC_END;
    return;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
verifyConfigParams(
    ConfigParams const & rParams
) const
{
    if  ( ! rParams.pInAppMeta.get() ) {
        MY_LOGE("sensor(%d) no in app meta", mOpenId);
        return BAD_VALUE;
    }
    if  ( ! rParams.pInHalMeta.get() ) {
        MY_LOGE("sensor(%d) no in hal meta", mOpenId);
        return BAD_VALUE;
    }
    //if  ( ! rParams.pOutAppMeta.get() ) {
    //    return BAD_VALUE;
    //}
    //if  ( ! rParams.pOutHalMeta.get() ) {
    //    return BAD_VALUE;
    //}
    if  (  rParams.pvInFullRaw.size() == 0
            && ! rParams.pInResizedRaw.get() ) {
        MY_LOGE("sensor(%d) no in image fullraw or resized raw", mOpenId);
        return BAD_VALUE;
    }
    if  (  0 == rParams.vOutImage.size() && !rParams.pOutFDImage.get() ) {
        MY_LOGE("sensor(%d) no out yuv image", mOpenId);
        return BAD_VALUE;
    }
    //
#define dumpStreamIfExist(str, stream)                         \
    do {                                                       \
        MY_LOGD_IF(stream.get(), "%s: id %#" PRIxPTR ", %s",   \
                str,                                           \
                stream->getStreamId(), stream->getStreamName() \
               );                                              \
    } while(0)
    //
    dumpStreamIfExist("[meta] in app", rParams.pInAppMeta);
    dumpStreamIfExist("[meta] in hal", rParams.pInHalMeta);
    dumpStreamIfExist("[meta] in appRet", rParams.pInAppRetMeta);
    dumpStreamIfExist("[meta] out app", rParams.pOutAppMeta);
    dumpStreamIfExist("[meta] out hal", rParams.pOutHalMeta);
    for( size_t i = 0; i < rParams.pvInFullRaw.size(); i++ ) {
        dumpStreamIfExist("[img] in full", rParams.pvInFullRaw[i]);
    }
    dumpStreamIfExist("[img] in resized", rParams.pInResizedRaw);
    for( size_t i = 0; i < rParams.vOutImage.size(); i++ ) {
        dumpStreamIfExist("[img] out yuv", rParams.vOutImage[i]);
    }
    dumpStreamIfExist("[img] out fd", rParams.pOutFDImage);
#undef dumpStreamIfExist
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
getInfoIOMapSet(
    sp<IPipelineFrame> const& pFrame,
    IPipelineFrame::InfoIOMapSet& rIOMapSet
) const
{
    if( OK != pFrame->queryInfoIOMapSet( getNodeId(), rIOMapSet ) ) {
        MY_LOGE("sensor(%d) queryInfoIOMap failed", mOpenId);
        return NAME_NOT_FOUND;
    }
    //
    // do some check
    IPipelineFrame::ImageInfoIOMapSet& imageIOMapSet = rIOMapSet.mImageInfoIOMapSet;
    if( ! imageIOMapSet.size() ) {
        MY_LOGW("sensor(%d) no imageIOMap in frame", mOpenId);
        return BAD_VALUE;
    }
    //
    for( size_t i = 0; i < imageIOMapSet.size(); i++ ) {
        IPipelineFrame::ImageInfoIOMap const& imageIOMap = imageIOMapSet[i];
        if( imageIOMap.vIn.size() < 0 || imageIOMap.vIn.size() > 2 || imageIOMap.vOut.size() == 0) {
            MY_LOGE("sensor(%d) [img] #%d wrong size vIn %d, vOut %d",
                    mOpenId, i, imageIOMap.vIn.size(), imageIOMap.vOut.size());
            return BAD_VALUE;
        }
        MY_LOGD_IF(mLogLevel, "sensor(%d) frame %zu:[img] #%zu, in %d, out %d",
                mOpenId, pFrame->getFrameNo(), i, imageIOMap.vIn.size(), imageIOMap.vOut.size());
    }
    //
    IPipelineFrame::MetaInfoIOMapSet& metaIOMapSet = rIOMapSet.mMetaInfoIOMapSet;
    if( ! metaIOMapSet.size() ) {
        MY_LOGW("sensor(%d) no metaIOMap in frame", mOpenId);
        return BAD_VALUE;
    }
    //
    for( size_t i = 0; i < metaIOMapSet.size(); i++ ) {
        IPipelineFrame::MetaInfoIOMap const& metaIOMap = metaIOMapSet[i];
        if( !mpInAppMeta_Request.get() ||
                0 > metaIOMap.vIn.indexOfKey(mpInAppMeta_Request->getStreamId()) ) {
            MY_LOGE("sensor(%d) [meta] no in app", mOpenId);
            return BAD_VALUE;
        }
        if( !mpInHalMeta_P1.get() ||
                0 > metaIOMap.vIn.indexOfKey(mpInHalMeta_P1->getStreamId()) ) {
            MY_LOGE("sensor(%d) [meta] no in hal", mOpenId);
            return BAD_VALUE;
        }
        MY_LOGD_IF(mLogLevel, "sensor(%d) frame %zu:[meta] #%zu: in %d, out %d",
                mOpenId, pFrame->getFrameNo(), i, metaIOMap.vIn.size(), metaIOMap.vOut.size());
    }
    //
    MY_LOGD("sensor(%d) frame %zu: [image(%d)] in/out %d/%d, [meta(%d)] in/out %d/%d",
        mOpenId, pFrame->getFrameNo(), imageIOMapSet.size(), imageIOMapSet[0].vIn.size(), imageIOMapSet[0].vOut.size(), metaIOMapSet.size(), metaIOMapSet[0].vIn.size(), metaIOMapSet[0].vOut.size());
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2NodeImp::
isInImageStream(
    StreamId_T const streamId
) const
{
    RWLock::AutoRLock _l(mConfigRWLock);
    //
    if( isFullRawLocked(streamId) || isResizeRawLocked(streamId) || isLcsoRawLocked(streamId))
        return MTRUE;
    //
    MY_LOGD_IF(0, "sensor(%d) stream id %p is not in-stream", mOpenId, streamId);
    return MFALSE;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2NodeImp::
isInMetaStream(
    StreamId_T const streamId
) const
{
    RWLock::AutoRLock _l(mConfigRWLock);
    return isStream(mpInAppMeta_Request, streamId) ||
           isStream(mpInHalMeta_P1, streamId) ||
           isStream(mpInAppRetMeta_Request, streamId);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
acquireImageStream(
    android::sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    sp<IImageStreamBuffer>& rpStreamBuffer
)
{
    return ensureImageBufferAvailable_(
            pFrame->getFrameNo(),
            streamId,
            pFrame->getStreamBufferSet(),
            rpStreamBuffer
            );
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
releaseImageStream(
    sp<IPipelineFrame> const& pFrame,
    sp<IImageStreamBuffer> const pStreamBuffer,
    MUINT32 const status
) const
{
    IStreamBufferSet& streamBufferSet = pFrame->getStreamBufferSet();
    StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();
    //
    if( pStreamBuffer == NULL ) {
        MY_LOGE("sensor(%d) pStreamBuffer == NULL", mOpenId);
        return;
    }
    //
    if( ! isInImageStream(streamId) ) {
        pStreamBuffer->markStatus(
                (status != eStreamStatus_FILLED) ?
                STREAM_BUFFER_STATUS::WRITE_ERROR :
                STREAM_BUFFER_STATUS::WRITE_OK
                );
    }
    //
    //  Mark this buffer as USED by this user.
    //  Mark this buffer as RELEASE by this user.
    streamBufferSet.markUserStatus(
            streamId,
            getNodeId(),
            ((status != eStreamStatus_NOT_USED) ? IUsersManager::UserStatus::USED : 0) |
            IUsersManager::UserStatus::RELEASE
            );
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
acquireImageBuffer(
    StreamId_T const streamId,
    sp<IImageStreamBuffer> const pStreamBuffer,
    sp<IImageBuffer>& rpImageBuffer
) const
{
    if( pStreamBuffer == NULL ) {
        MY_LOGE("sensor(%d) pStreamBuffer == NULL", mOpenId);
        return BAD_VALUE;
    }
    //  Query the group usage.
    MUINT const groupUsage = pStreamBuffer->queryGroupUsage(getNodeId());
    sp<IImageBufferHeap>   pImageBufferHeap =
        isInImageStream(streamId) ?
        pStreamBuffer->tryReadLock(getNodeName()) :
        pStreamBuffer->tryWriteLock(getNodeName());

    if (pImageBufferHeap == NULL) {
        MY_LOGE("sensor(%d) [node:%d][stream buffer:%s] cannot get ImageBufferHeap",
                mOpenId, getNodeId(), pStreamBuffer->getName());
        return BAD_VALUE;
    }

    rpImageBuffer = pImageBufferHeap->createImageBuffer();
    if (rpImageBuffer == NULL) {
        MY_LOGE("sensor(%d) [node:%d][stream buffer:%s] cannot create ImageBuffer",
                mOpenId, getNodeId(), pStreamBuffer->getName());
        return BAD_VALUE;
    }
    rpImageBuffer->lockBuf(getNodeName(), groupUsage);

    MY_LOGD_IF(mLogLevel >= 1, "sensor(%d) stream %#" PRIxPTR ": buffer: %p, usage: %p",
        mOpenId, streamId, rpImageBuffer.get(), groupUsage);

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
releaseImageBuffer(
    sp<IImageStreamBuffer> const pStreamBuffer,
    sp<IImageBuffer> const pImageBuffer
) const
{
    if( pStreamBuffer == NULL || pImageBuffer == NULL ) {
        MY_LOGE("sensor(%d) pStreamBuffer && pImageBuffer should not be NULL", mOpenId);
        return;
    }
    //
    pImageBuffer->unlockBuf(getNodeName());
    pStreamBuffer->unlock(getNodeName(), pImageBuffer->getImageBufferHeap());
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
acquireMetaStream(
    android::sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    sp<IMetaStreamBuffer>& rpStreamBuffer
)
{
    return ensureMetaBufferAvailable_(
            pFrame->getFrameNo(),
            streamId,
            pFrame->getStreamBufferSet(),
            rpStreamBuffer
            );
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
releaseMetaStream(
    android::sp<IPipelineFrame> const& pFrame,
    sp<IMetaStreamBuffer> const pStreamBuffer,
    MUINT32 const status
) const
{
    IStreamBufferSet&     rStreamBufferSet = pFrame->getStreamBufferSet();
    StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();
    //
    if( pStreamBuffer.get() == NULL ) {
        MY_LOGE("sensor(%d) StreamId %d: pStreamBuffer == NULL",
                mOpenId, streamId);
        return;
    }
    //
    //Buffer Producer must set this status.
    if( ! isInMetaStream(streamId) ) {
        pStreamBuffer->markStatus(
                (status != eStreamStatus_FILLED) ?
                STREAM_BUFFER_STATUS::WRITE_ERROR :
                STREAM_BUFFER_STATUS::WRITE_OK
                );
    }
    //
    //  Mark this buffer as USED by this user.
    //  Mark this buffer as RELEASE by this user.
    rStreamBufferSet.markUserStatus(
            streamId, getNodeId(),
            ((status != eStreamStatus_NOT_USED) ? IUsersManager::UserStatus::USED : 0) |
            IUsersManager::UserStatus::RELEASE
            );
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
acquireMetadata(
    StreamId_T const streamId,
    sp<IMetaStreamBuffer> const pStreamBuffer,
    IMetadata*& rpMetadata
) const
{
    rpMetadata = isInMetaStream(streamId) ?
        pStreamBuffer->tryReadLock(getNodeName()) :
        pStreamBuffer->tryWriteLock(getNodeName());

    if( rpMetadata == NULL ) {
        MY_LOGE("sensor(%d) [node:%d][stream buffer:%s] cannot get metadata",
                mOpenId, getNodeId(), pStreamBuffer->getName());
        return BAD_VALUE;
    }

    MY_LOGD_IF(0,"sensor(%d) stream %#" PRIxPTR ": stream buffer %p, metadata: %p",
        mOpenId, streamId, pStreamBuffer.get(), rpMetadata);

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
releaseMetadata(
    sp<IMetaStreamBuffer> const pStreamBuffer,
    IMetadata* const pMetadata
) const
{
    if( pMetadata == NULL ) {
        MY_LOGW("sensor(%d) pMetadata == NULL", mOpenId);
        return;
    }
    pStreamBuffer->unlock(getNodeName(), pMetadata);
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
onPartialFrameDone(
    sp<IPipelineFrame> const& pFrame
)
{
    CAM_TRACE_NAME("P2F:PartialFrameDone");
    //FUNC_START;
    MY_LOGD_IF(1, "sensor(%d) frame %d applyRelease+", mOpenId, pFrame->getFrameNo());
    IStreamBufferSet&     rStreamBufferSet = pFrame->getStreamBufferSet();
    rStreamBufferSet.applyRelease(getNodeId());
    MY_LOGD_IF(1, "sensor(%d) frame %d applyRelease-", mOpenId, pFrame->getFrameNo());
    //FUNC_END;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2NodeImp::
onFrameDone(
    sp<IPipelineFrame> const& pFrame
)
{
    CAM_TRACE_NAME("P2F:FrameDone");
    //MY_LOGD("frame %u done", pFrame->getFrameNo());
    onDispatchFrame(pFrame);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2NodeImp::
mapToRequests(
    android::sp<IPipelineFrame> const& pFrame
)
{
    //
    // 1. get IOMap
    IPipelineFrame::InfoIOMapSet IOMapSet;
    if( OK != getInfoIOMapSet(pFrame, IOMapSet) ) {
        MY_LOGE("sensor(%d) queryInfoIOMap failed", mOpenId);
        return BAD_VALUE;
    }
    //
    // 2. create metadata handle (based on IOMap)
    sp<MetaHandle> pMeta_InApp  = mpInAppMeta_Request.get() ?
        MetaHandle::create(this, pFrame, mpInAppMeta_Request->getStreamId()) : NULL;
    sp<MetaHandle> pMeta_InHal  = mpInHalMeta_P1.get() ?
        MetaHandle::create(this, pFrame, mpInHalMeta_P1->getStreamId()) : NULL;

    if( pMeta_InApp  == NULL ||
        pMeta_InHal  == NULL )
    {
        MY_LOGW("sensor(%d) meta check failed: pMeta_InApp == NULL | pMeta_InHal == NULL", mOpenId);
        return BAD_VALUE;
    }

#ifdef FEATURE_MODIFY
    MINT32 iso = -1;
    {
        // get iso from inAppRet
        sp<MetaHandle> pMeta_InAppRet  = mpInAppRetMeta_Request.get() ?
            MetaHandle::create(this, pFrame, mpInAppRetMeta_Request->getStreamId()) : NULL;

        if (pMeta_InAppRet.get() &&
            !tryGetMetadata<MINT32>(pMeta_InAppRet->getMetadata(), MTK_SENSOR_SENSITIVITY, iso))
        {
            MY_LOGW("no MTK_SENSOR_SENSITIVITY from AppRetMeta");
        }
        if (iso == -1)
        {
            IMetadata *inApp = pMeta_InApp->getMetadata();
            if (inApp != NULL && !tryGetMetadata<MINT32>(inApp, MTK_SENSOR_SENSITIVITY, iso))
                MY_LOGW("no MTK_SENSOR_SENSITIVITY from AppMeta");
        }
        // explicitly destruct MetaHandle
        pMeta_InAppRet = NULL;
    }
#endif // FEATURE_MODIFY
    //
   // 3. create FrameLifeHolder
    sp<FrameLifeHolder> pFrameLife = new FrameLifeHolder(mOpenId, this, pFrame, mLogLevel >= 1);
    // 4. process image IO
    IPipelineFrame::ImageInfoIOMapSet& imageIOMapSet = IOMapSet.mImageInfoIOMapSet;
    for( size_t run_idx = 0 ; run_idx < imageIOMapSet.size(); run_idx++ )
    {
        IPipelineFrame::ImageInfoIOMap const& imageIOMap = imageIOMapSet[run_idx];
        sp<Request> pRequest = new Request(pFrameLife);
        pRequest->frameNo = pFrame->getFrameNo();
        pRequest->index = run_idx;
        pRequest->context.fd_stream_id = mpOutFd.get() ? mpOutFd->getStreamId() : 0;

        pRequest->context.iso = iso;
        if (mLogLevel >= 2) {
            MY_LOGD("sensor(%d) [StreamID] run_idx(%d) vIn.size(%d) +++",
                mOpenId, run_idx, imageIOMap.vIn.size());
            for (int i = 0; i < imageIOMap.vIn.size(); i++) {
                StreamId_T const sId = imageIOMap.vIn.keyAt(i);
                MY_LOGD("sensor(%d) [StreamID] In(%d) sId(0x%x) F(%d) R(%d)",
                    mOpenId, i, sId, isFullRawLocked(sId), isResizeRawLocked(sId));
            }
            MY_LOGD("sensor(%d) [StreamID] run_idx(%d) vIn.size(%d) ---",
                mOpenId, run_idx, imageIOMap.vIn.size());
        }
        // source
        for(size_t i = 0; i < imageIOMap.vIn.size() ; i++)
        {
            StreamId_T const sId = imageIOMap.vIn.keyAt(i);
            if(isFullRawLocked(sId) || isResizeRawLocked(sId)){
                pRequest->context.in_buffer = StreamBufferHandle::create(this, pFrame, sId);
            } else if(isLcsoRawLocked(sId)){
                pRequest->context.in_lcso_buffer = StreamBufferHandle::create(this, pFrame, sId);
            }
        }
        if( pRequest->context.in_buffer == NULL )
        {
            MY_LOGW("sensor(%d) get input buffer failed", mOpenId);
            return BAD_VALUE;
        }
        StreamId_T const streamId = pRequest->context.in_buffer->getStreamId();
        //
        {
            RWLock::AutoRLock _l(mConfigRWLock);
            pRequest->context.resized = isResizeRawLocked(streamId);
        }
        // determine whether burst or not
#if FORCE_BURST_ON
        pRequest->context.burst_num = pRequest->context.resized ? 4 : 0;
#else
        pRequest->context.burst_num = pRequest->context.resized ? mBurstNum : 0;
#endif
        // destination
        for( size_t i = 0; i < imageIOMap.vOut.size(); i++ )
        {
            StreamId_T const streamId = imageIOMap.vOut.keyAt(i);
            //MUINT32 const transform = imageIOMap.vOut.valueAt(i)->getTransform();
            pRequest->context.out_buffers.push_back(StreamBufferHandle::create(this, pFrame, streamId));
        }
        //
        pRequest->context.in_app_meta = pMeta_InApp;
        pRequest->context.in_hal_meta = pMeta_InHal;

        if (run_idx == 0)
        {
            pRequest->context.out_app_meta = mpOutAppMeta_Result.get() ?
                MetaHandle::create(this, pFrame, mpOutAppMeta_Result->getStreamId()) : NULL;
            pRequest->context.out_hal_meta= mpOutHalMeta_Result.get() ?
                MetaHandle::create(this, pFrame, mpOutHalMeta_Result->getStreamId()) : NULL;
        }
        // prevent HAL1 that a frame have no buffer from dispatching faster than this scope
        if (run_idx == imageIOMapSet.size() - 1)
        {
            pMeta_InApp.clear();
            pMeta_InHal.clear();
        }
        mpP2Processor->queueRequest(pRequest);
        //
    }
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
mapPortId(
    StreamId_T const streamId, // [in]
    MUINT32 const transform,   // [in]
    MBOOL const isFdStream,    // [in]
    MUINT32& rOccupied,        // [in/out]
    PortID&  rPortId           // [out]
) const
{
    MERROR ret = OK;
#define PORT_WDMAO_USED  (0x1)
#define PORT_WROTO_USED  (0x2)
#define PORT_IMG2O_USED  (0x4)
    if( transform != 0 ) {
        if( !(rOccupied & PORT_WROTO_USED) ) {
            rPortId = PORT_WROTO;
            rOccupied |= PORT_WROTO_USED;
        }
        else
            ret = INVALID_OPERATION;
    }
    else {
#if FD_PORT_SUPPORT
        if( FD_PORT_SUPPORT && isFdStream ) {
            if( rOccupied & PORT_IMG2O_USED ) {
                MY_LOGW("sensor(%d) should not be occupied", mOpenId);
                ret = INVALID_OPERATION;
            } else {
                rOccupied |= PORT_IMG2O_USED;
                rPortId = PORT_IMG2O;
            }
        } else
#endif
        if( !(rOccupied & PORT_WDMAO_USED) ) {
            rOccupied |= PORT_WDMAO_USED;
            rPortId = PORT_WDMAO;
        } else if( !(rOccupied & PORT_WROTO_USED) ) {
            rOccupied |= PORT_WROTO_USED;
            rPortId = PORT_WROTO;
        } else
            ret = INVALID_OPERATION;
    }
    MY_LOGD_IF(0, "sensor(%d) stream id %#" PRIxPTR ", occupied %p",
            mOpenId, streamId, rOccupied);
    return ret;
#undef PORT_WDMAO_USED
#undef PORT_WROTO_USED
#undef PORT_IMG2O_USED
}

/******************************************************************************
 *
 ******************************************************************************/
sp<MetaHandle>
MetaHandle::
create(
    StreamControl* const pCtrl,
    sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId
)
{
    // check StreamBuffer here
    sp<IMetaStreamBuffer> pStreamBuffer = NULL;
    if( pCtrl && OK == pCtrl->acquireMetaStream(
                pFrame,
                streamId,
                pStreamBuffer) )
    {
        IMetadata* pMeta = NULL;
        if( OK == pCtrl->acquireMetadata(
                    streamId,
                    pStreamBuffer,
                    pMeta
                    ) )
        {
            BufferState_t const init_state =
                pCtrl->isInMetaStream(streamId) ? STATE_READABLE : STATE_WRITABLE;
            return new MetaHandle(
                    pCtrl,
                    pFrame,
                    streamId,
                    pStreamBuffer,
                    init_state,
                    pMeta
                    );
        }
        else {
            pCtrl->releaseMetaStream(pFrame, pStreamBuffer, StreamControl::eStreamStatus_NOT_USED);
        }
    }
    //
    return NULL;
}


/******************************************************************************
 *
 ******************************************************************************/
MetaHandle::
~MetaHandle()
{
    if( muState != STATE_NOT_USED )
    {
        const MUINT32 status = (muState != STATE_WRITE_FAIL) ?
            StreamControl::eStreamStatus_FILLED : StreamControl::eStreamStatus_ERROR;
        //
        mpStreamCtrl->releaseMetadata(mpStreamBuffer, mpMetadata);
        mpStreamCtrl->releaseMetaStream(mpFrame, mpStreamBuffer, status);
    }
    else
    {
        mpStreamCtrl->releaseMetaStream(mpFrame, mpStreamBuffer, StreamControl::eStreamStatus_NOT_USED);
    }
    //MY_LOGD("release meta[%d] streamId[0x%x]",mpFrame->getFrameNo() ,mStreamId);
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
MetaHandle::
updateState(BufferState_t const state)
{
    Mutex::Autolock _l(mLock);
    if( muState == STATE_NOT_USED ) {
        MY_LOGW("streamId %#" PRIxPTR " state %d -> %d",
            mStreamId, muState, state);
    }
    else {
        MY_LOGW_IF(state == STATE_WRITE_FAIL, "streamId %#" PRIxPTR " set fail, state %d -> %d",
                mStreamId, muState, state);
        muState = state;
    }
    //mCond.broadcast();
}


/******************************************************************************
 *
 ******************************************************************************/
sp<BufferHandle>
StreamBufferHandle::
create(
    StreamControl* const pCtrl,
    sp<IPipelineFrame> const& pFrame,
    StreamId_T const streamId
)
{
    // check StreamBuffer here
    sp<IImageStreamBuffer> pStreamBuffer = NULL;
    if( OK == pCtrl->acquireImageStream(
                pFrame,
                streamId,
                pStreamBuffer) )
    {
        MUINT32 uTransform = pStreamBuffer->getStreamInfo()->getTransform();
        MUINT32 uUsage = pStreamBuffer->getStreamInfo()->getUsageForAllocator();
        MY_LOGD_IF(0, "create buffer handler, stream:0x%x, transform:%d, usage:%d",
            streamId, uTransform, uUsage);
        return new StreamBufferHandle(
                pCtrl, pFrame,
                streamId, pStreamBuffer,
                uTransform, uUsage);
    }
    //
    return NULL;
}


/******************************************************************************
 *
 ******************************************************************************/
StreamBufferHandle::
~StreamBufferHandle()
{
    #ifdef FEATURE_MODIFY
    if( mEarlyReleased )
    {
        return;
    }
    #endif // FEATURE_MODIFY
    if( muState != STATE_NOT_USED )
    {
        const MUINT32 status = (muState != STATE_WRITE_FAIL) ?
            StreamControl::eStreamStatus_FILLED : StreamControl::eStreamStatus_ERROR;
        //
        mpStreamCtrl->releaseImageBuffer(mpStreamBuffer, mpImageBuffer);
        mpStreamCtrl->releaseImageStream(mpFrame, mpStreamBuffer, status);
    }
    else
    {
        mpStreamCtrl->releaseImageStream(mpFrame, mpStreamBuffer, StreamControl::eStreamStatus_NOT_USED);
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
StreamBufferHandle::
waitState(
    BufferState_t const state,
    nsecs_t const nsTimeout
)
{
    Mutex::Autolock _l(mLock);
    if( mpImageBuffer == NULL ) {
        // get buffer from streambuffer
        const MERROR ret = mpStreamCtrl->acquireImageBuffer(mStreamId, mpStreamBuffer, mpImageBuffer);
        // update initial state
        if( ret == OK )
            muState = mpStreamCtrl->isInImageStream(mStreamId) ? STATE_READABLE : STATE_WRITABLE;
        //return ret;
    }
    //
    if( muState != state ) {
        mCond.waitRelative(mLock, nsTimeout);
    }
    return (muState == state) ? OK : TIMED_OUT;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
StreamBufferHandle::
updateState(BufferState_t const state)
{
    Mutex::Autolock _l(mLock);
    if( muState == STATE_NOT_USED ) {
        MY_LOGW("streamId %#" PRIxPTR " state %d -> %d",
            mStreamId, muState, state);
    }
    else {
        MY_LOGW_IF(state == STATE_WRITE_FAIL, "streamId %#" PRIxPTR " set fail: state %d -> %d",
                mStreamId, muState, state);
        muState = state;
    }
    mCond.broadcast();
}


/******************************************************************************
 *
 ******************************************************************************/
#ifdef FEATURE_MODIFY
MBOOL
StreamBufferHandle::
earlyRelease()
{
    if( mEarlyReleased )
    {
        return MFALSE;
    }
    mEarlyReleased = MTRUE;
    if( muState != STATE_NOT_USED )
    {
        MUINT32 status = (muState != STATE_WRITE_FAIL) ?
            StreamControl::eStreamStatus_FILLED : StreamControl::eStreamStatus_ERROR;
        mpStreamCtrl->releaseImageBuffer(mpStreamBuffer, mpImageBuffer);
        mpStreamCtrl->releaseImageStream(mpFrame, mpStreamBuffer, status);
    }
    else
    {
        mpStreamCtrl->releaseImageStream(mpFrame, mpStreamBuffer, StreamControl::eStreamStatus_NOT_USED);
    }
    return MTRUE;
}
#endif // FEATURE_MODIFY


/******************************************************************************
 *
 ******************************************************************************/
sp<Processor>
P2Procedure::
createProcessor(InitParams& params)
{
    CAM_TRACE_NAME("P2F:[Proc]createProcessor");
    IStreamingFeaturePipe* pPipe = NULL;
    IStreamingFeaturePipe::UsageHint usageHint = getPipeUsageHint(params.usageHint);
    IHal3A_T* p3A = NULL;
    ENormalStreamTag streamtag = ENormalStreamTag_Prv;
    if( params.type == P2FeatureNode::PASS2_STREAM ) {
        streamtag = ENormalStreamTag_Prv;
    }
    else if ( params.type == P2FeatureNode::PASS2_TIMESHARING ) {
        streamtag = ENormalStreamTag_Vss;
    }
    else {
        MY_LOGE("sensor(%d) not supported type %d", params.openId, params.type);
        goto lbExit;
    }
    //
    CAM_TRACE_BEGIN("P2F:[Proc]NormalStream create");
    pPipe = IStreamingFeaturePipe::createInstance(params.openId, usageHint);
    //
    if( pPipe == NULL ) {
        MY_LOGE("sensor(%d) create pipe failed", params.openId);
        CAM_TRACE_END();
        goto lbExit;
    }
    //
    CAM_TRACE_BEGIN("P2F:[Proc]NormalStream init");
    if( ! pPipe->init(LOG_TAG) )
    {
        CAM_TRACE_END();
        MY_LOGE("sensor(%d) pipe init failed", params.openId);
        goto lbExit;
    }
    CAM_TRACE_END();
    //
    #if SUPPORT_3A
    CAM_TRACE_BEGIN("P2F:[Proc]3A create");
    p3A = MAKE_Hal3A(params.openId, LOG_TAG);
    CAM_TRACE_END();
    #endif
    if( p3A == NULL ) {
        MY_LOGE("sensor(%d) create 3A failed", params.openId);
        goto lbExit;
    }
    MY_LOGD("sensor(%d) create processor type %d: pipe %p, 3A %p",
            params.openId, params.type, pPipe, p3A);
    //
lbExit:
    if( ! pPipe || !p3A ) {
        if( pPipe ) {
            pPipe->uninit(LOG_TAG);
            pPipe->destroyInstance();
            pPipe = NULL;
        }
        if( p3A ) {
            p3A->destroyInstance(LOG_TAG);
            p3A = NULL;
        }
    }

    params.pPipe        = pPipe;
    params.p3A          = p3A;
    return pPipe ? new ProcessorBase<P2Procedure>(params, PROCESSOR_NAME_P2) : NULL;
}


/******************************************************************************
 *
 ******************************************************************************/
P2Procedure::
~P2Procedure()
{
    MY_LOGD("sensor(%d) destroy processor %d: %p",
            mOpenId, mInitParams.type, mpPipe);
    //
    if( mpPipe ) {
        if( ! mpPipe->uninit(LOG_TAG) ) {
            MY_LOGE("sensor(%d) pipe uninit failed", mOpenId);
        }
        mpPipe->destroyInstance();
    }

    if( mp3A ) {
        mp3A->destroyInstance(LOG_TAG);
    }

    if( mpMultiFrameHandler ) {
        delete mpMultiFrameHandler;
    }

    if( mpDebugScanLine != NULL )
    {
        mpDebugScanLine->destroyInstance();
        mpDebugScanLine = NULL;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
#ifdef FEATURE_MODIFY
static sp<Request> getP2Request(FeaturePipeParam &param)
{
    return param.getVar<sp<Request> >(VAR_P2_REQUEST, NULL);
}

static P2Procedure* getP2Procedure(FeaturePipeParam &param)
{
    P2Procedure* proc = reinterpret_cast<P2Procedure*>(param.mQParams.mpCookie);
    return proc;
}

static MVOID releaseP2ObjectReference(FeaturePipeParam &param)
{
    // release life cycle
    param.clearVar<sp<Request> >(VAR_P2_REQUEST);
}

MVOID P2Procedure::prepareFeaturePipeParam(FeaturePipeParam &featureEnqueParams,
              const sp<Request> &pRequest,
              IMetadata *pMeta_InApp,
              IMetadata *pMeta_InHal,
              IMetadata *pMeta_OutApp,
              IMetadata *pMeta_OutHal,
              const Cropper::crop_info_t &cropInfos)
{
    featureEnqueParams.setVar<sp<Request> >(VAR_P2_REQUEST, pRequest);

    featureEnqueParams.setFeatureMask(MASK_3DNR, isAPEnabled_3DNR((mInitParams.usageHint.m3DNRMode &
        P2FeatureNode::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT), pMeta_InApp));
    featureEnqueParams.setFeatureMask(MASK_VHDR, isHALenabled_VHDR(pMeta_InHal));
    featureEnqueParams.setFeatureMask(MASK_EIS, isAPEnabled_EIS(pMeta_InApp));
    featureEnqueParams.setFeatureMask(MASK_VFB, isAPEnabled_VFB(pMeta_InApp));
    featureEnqueParams.setFeatureMask(MASK_VFB_EX, isAPEnabled_VFB_EX(pMeta_InApp));

    if (HAS_EIS(featureEnqueParams.mFeatureMask))
    {
        MINT32 eisMode = 0;
        MINT32 eisCounter = 0;
        IMetadata::IEntry entry = pMeta_InHal->entryFor(MTK_EIS_MODE);
        if (entry.count() > 0)
        {
            eisMode = entry.itemAt(0, Type2Type<MINT32>());
            eisCounter = entry.itemAt(1, Type2Type<MINT32>());
            MY_LOGD("sensor(%d) eisMode: %d, eisCounter: %d", mOpenId, eisMode, eisCounter);

            if (EIS_MODE_IS_EIS_25_ENABLED(eisMode))
            {
                featureEnqueParams.setFeatureMask(MASK_EIS_25,MTRUE);
                if (EIS_MODE_IS_EIS_25_STOP_ENABLED(eisMode))
                {
                    featureEnqueParams.setFeatureMask(MASK_EIS_QUEUE,MFALSE);
                }else if (eisCounter >= EISCustom::getEIS25_StartFrame())//Magic number
                {
                    //Start EIS Queue
                    featureEnqueParams.setFeatureMask(MASK_EIS_QUEUE,MTRUE);
                }
            }else if (EIS_MODE_IS_EIS_22_ENABLED(eisMode))
            {
                //GIS enabled
            }else
            {
                //Turn on EIS 1.2 only
                DISABLE_EIS(featureEnqueParams.mFeatureMask);
            }
        }else
        {
            //Turn on EIS 1.2 only
            DISABLE_EIS(featureEnqueParams.mFeatureMask);
        }
    }

    if (HAS_VHDR(featureEnqueParams.mFeatureMask))
    {
        if (HAS_VFB(featureEnqueParams.mFeatureMask) || HAS_VFB_EX(featureEnqueParams.mFeatureMask))
        {
            MY_LOGE("sensor(%d) z/i vHDR could NOT be with vFB&vFB_EX", mOpenId);
            DISABLE_VFB(featureEnqueParams.mFeatureMask);
            DISABLE_VFB_EX(featureEnqueParams.mFeatureMask);
        }
    }

    if (HAS_VFB(featureEnqueParams.mFeatureMask) || HAS_VFB_EX(featureEnqueParams.mFeatureMask))
    {
        //If vFBx is enabled, turn on EIS 1.2 only
        DISABLE_EIS(featureEnqueParams.mFeatureMask);
    }


    prepareFeatureData_3DNR(
        featureEnqueParams,
        cropInfos.dstsize_resizer.w, cropInfos.dstsize_resizer.h,
        pRequest->context.iso, pMeta_InApp, pMeta_InHal
        );    prepareFeatureData_VHDR(featureEnqueParams, pMeta_InHal);
    prepareFeatureData_VFB(featureEnqueParams);
    prepareFeatureData_EIS(featureEnqueParams, pMeta_InHal);
}
MBOOL P2Procedure::setP2B3A(FeaturePipeParam &param)
{
    MBOOL ret = MFALSE;
    void *pTuning;
    if( param.tryGetVar<void*>("p2b_3a_tuning", pTuning) )
    {
        sp<Request> request;
        request = getP2Request(param);
        if( request != NULL )
        {
            IMetadata *appInMeta = request->context.in_app_meta->getMetadata();
            IMetadata *halInMeta = request->context.in_hal_meta->getMetadata();
            if( halInMeta && appInMeta )
            {
                TuningParam rTuningParam = {pTuning, NULL, NULL};
                MetaSet_T inMetaSet;
                inMetaSet.appMeta = *halInMeta;
                inMetaSet.halMeta = *appInMeta;
                // trySetMetadata<MUINT8>(halInMeta, MTK_3A_ISP_PROFILE, EIspProfile_VFB_PostProc);
                mp3A->setIsp(0, inMetaSet, &rTuningParam, NULL/*outMetaSet*/);
                ret = MTRUE;
            }
        }
    }
    return ret;
}

#define EARLY_RELEASE 1

MVOID P2Procedure::partialRelease(FeaturePipeParam &param)
{
    P2Procedure* p2Procedure = getP2Procedure(param);
    sp<Request> p2Request = getP2Request(param);

    if( p2Procedure == NULL || p2Request == NULL )
    {
        MY_LOGE("Cannot retrieve p2Procedure(%p)/p2Request(%p)", p2Procedure, p2Request.get());
        return;
    }

    if( param.mQParams.mvIn.size() > 0 )
    {
        for(size_t i = 0; i < param.mQParams.mvIn.size() ; i++)
            param.mQParams.mvIn.editItemAt(i).mBuffer = NULL;
    }
    for( unsigned i = 0, size = param.mQParams.mvOut.size(); i < size; ++i )
    {
        if( param.mQParams.mvOut[i].mPortID.capbility == EPortCapbility_Disp )
        {
            p2Procedure->drawScanLine(param.mQParams.mvOut[i]);
            param.mQParams.mvOut.editItemAt(i).mBuffer = NULL;
        }
    }

    // release p1 buffer
    if( p2Request->context.in_buffer != NULL )
    {
        #if EARLY_RELEASE
        p2Request->context.in_buffer->earlyRelease();
        #endif // endif EARLY_RELEASE
        p2Request->context.in_buffer = NULL;
    }
    if( p2Request->context.in_lcso_buffer != NULL )
    {
        #if EARLY_RELEASE
        p2Request->context.in_lcso_buffer->earlyRelease();
        #endif // endif EARLY_RELEASE
        p2Request->context.in_lcso_buffer = NULL;
    }
    // release display buffer
    vector<sp<BufferHandle> >::iterator it = p2Request->context.out_buffers.begin();
    vector<sp<BufferHandle> >::iterator end = p2Request->context.out_buffers.end();
    MUINT32 displayCount = 0;
    for( ; it != end; ++it )
    {
        sp<BufferHandle> handle = *it;
        if( handle != NULL )
        {
            MUINT32 usage = handle->getUsage();
            if( usage & GRALLOC_USAGE_HW_COMPOSER )
            {
                ++displayCount;
                #if EARLY_RELEASE
                handle->updateState(BufferHandle::STATE_WRITE_OK);
                handle->earlyRelease();
                #endif
            }
        }
        (*it) = NULL;
    }
    if( displayCount != 1 )
    {
        MY_LOGD("Found unexpected display count = %d", displayCount);
    }
    #if EARLY_RELEASE
    p2Request->onPartialRequestDone();
    #endif // EARLY_RELEASE
}

MBOOL P2Procedure::featurePipeCB(FeaturePipeParam::MSG_TYPE msg, FeaturePipeParam &param)
{
    MBOOL ret = MFALSE;

    if( msg == FeaturePipeParam::MSG_FRAME_DONE )
    {
        partialRelease(param);
        releaseP2ObjectReference(param);
        param.mQParams.mpfnCallback(param.mQParams);
        ret = MTRUE;
    }
    else if( msg == FeaturePipeParam::MSG_DISPLAY_DONE )
    {
        P2Procedure* p2proc = getP2Procedure(param);
        MY_LOGD("sensor(%d) parital release :%d", p2proc->mOpenId, p2proc->muDequeCnt);
        partialRelease(param);
        ret = MTRUE;
    }
    else if( msg == FeaturePipeParam::MSG_FD_DONE )
    {
        // FD
    }
    else if( msg == FeaturePipeParam::MSG_P2B_SET_3A )
    {
        P2Procedure *proc = getP2Procedure(param);
        if( proc )
        {
            ret = proc->setP2B3A(param);
        }
    }

    return ret;
}

MVOID P2Procedure::drawScanLine(const Output &output)
{
    if( mDebugScanLineMask != 0 &&
        mpDebugScanLine != NULL &&
        output.mBuffer != NULL )
    {
        if( (output.mPortID.index == PORT_WDMAO.index &&
             mDebugScanLineMask & DRAWLINE_PORT_WDMAO) ||
            (output.mPortID.index == PORT_WROTO.index &&
             mDebugScanLineMask & DRAWLINE_PORT_WROTO) ||
            (output.mPortID.index == PORT_IMG2O.index &&
             mDebugScanLineMask & DRAWLINE_PORT_IMG2O) )
        {
            mpDebugScanLine->drawScanLine(
                                    output.mBuffer->getImgSize().w,
                                    output.mBuffer->getImgSize().h,
                                    (void*)(output.mBuffer->getBufVA(0)),
                                    output.mBuffer->getBufSizeInBytes(0),
                                    output.mBuffer->getBufStridesInBytes(0));
        }
    }
}
#endif // FEATURE_MODIFY


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
onExecute(
    sp<Request> const pRequest,
    FrameParams const& params
)
{
    CAM_TRACE_NAME("P2F:[Proc]exe");
    //
    MERROR ret = OK;
    //
    if ( OK != (ret = checkParams(params)) )
    #ifdef FEATURE_MODIFY
    {
        pRequest->context.in_buffer.clear();
        pRequest->context.in_lcso_buffer.clear();
        vector<sp<BufferHandle> >::iterator it, end;
        it = pRequest->context.out_buffers.begin();
        end = pRequest->context.out_buffers.end();
        for( ; it != end; ++it )
        {
            (*it).clear();
        }
        return ret;
    }
    #endif // FEATURE_MODIFY
    // prepare metadata
    IMetadata* pMeta_InApp  = params.inApp->getMetadata();
    IMetadata* pMeta_InHal  = params.inHal->getMetadata();
    IMetadata* pMeta_OutApp = params.outApp.get() ? params.outApp->getMetadata() : NULL;
    IMetadata* pMeta_OutHal = params.outHal.get() ? params.outHal->getMetadata() : NULL;
    //
    if( pMeta_InApp == NULL || pMeta_InHal == NULL ) {
        MY_LOGE("sensor(%d) meta: in app %p, in hal %p", mOpenId, pMeta_InApp, pMeta_InHal);
        return BAD_VALUE;
    }
    //
    sp<Cropper::crop_info_t> pCropInfo = new Cropper::crop_info_t;
    if( OK != (ret = getCropInfos(pMeta_InApp, pMeta_InHal, params.bResized, *pCropInfo)) ) {
        MY_LOGE("sensor(%d) getCropInfos failed", mOpenId);
        return ret;
    }
    pRequest->context.crop_info = pCropInfo;
    //
    QParams enqueParams;
    //frame tag
    enqueParams.mvStreamTag.push_back(
                        NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Normal);
    //
    // input
    {
        if( OK != (ret = params.in.mHandle->waitState(BufferHandle::STATE_READABLE)) ) {
            MY_LOGW("sensor(%d) src buffer err = %d", mOpenId, ret);
            return BAD_VALUE;
        }
        IImageBuffer* pSrc = params.in.mHandle->getBuffer();
        //
        Input src;
        src.mPortID       = params.in.mPortId;
        src.mPortID.group = 0;
        src.mBuffer       = pSrc;
        // update src size
        if( params.bResized )
            pSrc->setExtParam(pCropInfo->dstsize_resizer);
        //
        enqueParams.mvIn.push_back(src);
        MY_LOGD_IF(mbEnableLog, "sensor(%d) P2FeatureNode EnQ Src mPortID.index(%d) Fmt(0x%x) "
            "Size(%dx%d)", mOpenId, src.mPortID.index, src.mBuffer->getImgFormat(),
            src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
    }
    // input LCEI
    if(params.in_lcso.mHandle != NULL){
        if( OK != (ret = params.in_lcso.mHandle->waitState(BufferHandle::STATE_READABLE)) ) {
            MY_LOGW(" (%d) Lcso handle not null but waitState failed! ", pRequest->frameNo);
            return BAD_VALUE;
        }else {
            IImageBuffer* pSrc = params.in_lcso.mHandle->getBuffer();
            //
            Input src;
            src.mPortID       = params.in_lcso.mPortId;
            src.mPortID.group = 0;
            src.mBuffer       = pSrc;

            //
            enqueParams.mvIn.push_back(src);
            MY_LOGD_IF(mbEnableLog, "sensor(%d) P2FeatureNode EnQ Src mPortID.index(%d) Fmt(0x%x) "
                "Size(%dx%d)", mOpenId, src.mPortID.index, src.mBuffer->getImgFormat(),
                src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
        }
        //
    }
    //
    // output
    for( size_t i = 0; i < params.vOut.size(); i++ )
    {
        if( params.vOut[i].mHandle == NULL ||
            OK != (ret = params.vOut[i].mHandle->waitState(BufferHandle::STATE_WRITABLE)) ) {
            MY_LOGW("sensor(%d) dst buffer err = %d", mOpenId, ret);
            continue;
        }
        IImageBuffer* pDst = params.vOut[i].mHandle->getBuffer();
        //
        Output dst;
        dst.mPortID       = params.vOut[i].mPortId;
        dst.mPortID.group = 0;
        MUINT32 const uUsage = params.vOut[i].mUsage;
        dst.mPortID.capbility   = (NSIoPipe::EPortCapbility)(
            (uUsage & GRALLOC_USAGE_HW_COMPOSER) ? EPortCapbility_Disp :
            (uUsage & GRALLOC_USAGE_HW_VIDEO_ENCODER) ? EPortCapbility_Rcrd :
            EPortCapbility_None);
        dst.mBuffer       = pDst;
        dst.mTransform    = params.vOut[i].mTransform;
        //
        enqueParams.mvOut.push_back(dst);
    }

    if( enqueParams.mvOut.size() == 0 ) {
        //MY_LOGW("sensor(%d) no dst buffer", mOpenId);
        return BAD_VALUE;
    }

#ifdef FEATURE_MODIFY
    FeaturePipeParam featureEnqueParams(featurePipeCB);
    prepareFeaturePipeParam(featureEnqueParams, pRequest,
                            pMeta_InApp, pMeta_InHal,
                            pMeta_OutApp, pMeta_OutHal,
                            *pCropInfo);
    featureEnqueParams.setQParams(enqueParams);
#endif // FEATURE_MODIFY

    // for output group crop
    {
        Vector<Output>::const_iterator iter = enqueParams.mvOut.begin();
        while( iter != enqueParams.mvOut.end() ) {
            MCrpRsInfo crop;
            MUINT32 const uPortIndex = iter->mPortID.index;
#ifdef FEATURE_MODIFY
            MCrpRsInfo crop_NoEIS;
#endif
            if( uPortIndex == PORT_WDMAO.index ) {
                crop.mGroupID = 2;
                Cropper::calcViewAngle(mbEnableLog, *pCropInfo, iter->mBuffer->getImgSize(), crop.mCropRect);
#ifdef FEATURE_MODIFY
                if (HAS_EIS(featureEnqueParams.mFeatureMask))
                {
                    crop_NoEIS.mGroupID = 12;
                    getCropInfos_NoEIS(pMeta_InApp, pMeta_InHal, params.bResized, iter->mBuffer->getImgSize(), crop_NoEIS.mCropRect);
                    crop_NoEIS.mResizeDst = iter->mBuffer->getImgSize();
                    MY_LOGD_IF(mbEnableLog, "sensor(%d) P2FeatureNode EnQ out NoEIS Crop G(%d) S(%d,%d)(%d,%d)(%dx%d) D(%dx%d)",
                             mOpenId,
                             crop_NoEIS.mGroupID,
                             crop_NoEIS.mCropRect.p_integral.x, crop_NoEIS.mCropRect.p_integral.y,
                             crop_NoEIS.mCropRect.p_fractional.x, crop_NoEIS.mCropRect.p_fractional.y,
                             crop_NoEIS.mCropRect.s.w, crop_NoEIS.mCropRect.s.h,
                             crop_NoEIS.mResizeDst.w, crop_NoEIS.mResizeDst.h);
                }
#endif
            } else if ( uPortIndex == PORT_WROTO.index ) {
                crop.mGroupID = 3;
                IImageBuffer* pBuf      = iter->mBuffer;
                MINT32 const transform  = iter->mTransform;
                MSize dstSize = ( transform & eTransform_ROT_90 )
                                ? MSize(pBuf->getImgSize().h, pBuf->getImgSize().w)
                                : pBuf->getImgSize();
                Cropper::calcViewAngle(mbEnableLog, *pCropInfo, dstSize, crop.mCropRect);
#ifdef FEATURE_MODIFY
                if (HAS_EIS(featureEnqueParams.mFeatureMask))
                {
                    crop_NoEIS.mGroupID = 13;
                    getCropInfos_NoEIS(pMeta_InApp, pMeta_InHal, params.bResized, dstSize, crop_NoEIS.mCropRect);
                    crop_NoEIS.mResizeDst = iter->mBuffer->getImgSize();
                    MY_LOGD_IF(mbEnableLog, "sensor(%d) P2FeatureNode EnQ out NoEIS Crop G(%d) S(%d,%d)(%d,%d)(%dx%d) D(%dx%d)",
                             mOpenId,
                             crop_NoEIS.mGroupID,
                             crop_NoEIS.mCropRect.p_integral.x, crop_NoEIS.mCropRect.p_integral.y,
                             crop_NoEIS.mCropRect.p_fractional.x, crop_NoEIS.mCropRect.p_fractional.y,
                             crop_NoEIS.mCropRect.s.w, crop_NoEIS.mCropRect.s.h,
                             crop_NoEIS.mResizeDst.w, crop_NoEIS.mResizeDst.h);
                }
#endif
#if FD_PORT_SUPPORT
            } else if ( uPortIndex == PORT_IMG2O.index ) {
                crop.mGroupID = 1;
                Cropper::calcViewAngle(mbEnableLog, *pCropInfo, iter->mBuffer->getImgSize(), crop.mCropRect);
#endif
            } else {
                MY_LOGE("sensor(%d) not supported output port %p", mOpenId, iter->mPortID);
                return BAD_VALUE;
            }
            crop.mResizeDst = iter->mBuffer->getImgSize();
            MY_LOGD_IF(mbEnableLog, "sensor(%d) P2FeatureNode EnQ out 00 Crop G(%d) S(%d,%d)(%d,%d)(%dx%d) D(%dx%d)",
                mOpenId,
                crop.mGroupID,
                crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                crop.mCropRect.p_fractional.x, crop.mCropRect.p_fractional.y,
                crop.mCropRect.s.w, crop.mCropRect.s.h,
                crop.mResizeDst.w, crop.mResizeDst.h
                );
            enqueParams.mvCropRsInfo.push_back(crop);
#ifdef FEATURE_MODIFY
            if (crop_NoEIS.mGroupID != 0)
            {
                enqueParams.mvCropRsInfo.push_back(crop_NoEIS);
            }
#endif
            iter++;
        }
    }
    if (pRequest->context.burst_num > 1)
    {
        if(mpMultiFrameHandler)
            return mpMultiFrameHandler->collect(pRequest, featureEnqueParams);
        else
            MY_LOGW_IF(mbEnableLog, "sensor(%d) no burst handler", mOpenId);
    }

    {
        TuningParam rTuningParam = {NULL, NULL, NULL};
        void* pTuning = NULL;
        unsigned int tuningsize = mpPipe->getRegTableSize();
        pTuning = ::malloc(tuningsize);
        if (pTuning == NULL) {
            MY_LOGE("sensor(%d) alloc tuning buffer fail", mOpenId);
            return NO_MEMORY;
        }
        rTuningParam.pRegBuf = pTuning;
        if(params.in_lcso.mHandle != NULL) {
            rTuningParam.pLcsBuf = params.in_lcso.mHandle->getBuffer();
        }
        MY_LOGD_IF(mbEnableLog, "sensor(%d) pass2 setIsp malloc %p : %d, LCSO exist(%d)", mOpenId, pTuning, tuningsize, (rTuningParam.pLcsBuf != NULL));
        //
        MetaSet_T inMetaSet;
        MetaSet_T outMetaSet;
        //
        inMetaSet.appMeta = *pMeta_InApp;
        inMetaSet.halMeta = *pMeta_InHal;
        //
        MBOOL const bGetResult = (pMeta_OutApp || pMeta_OutHal);
        //
        if( params.bResized ) {
            trySetMetadata<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 0);
        } else {
            trySetMetadata<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 1);
        }
        if( pMeta_OutHal ) {
            // FIX ME: getDebugInfo() @ setIsp() should be modified
            //outMetaSet.halMeta = *pMeta_InHal;
        }
        //
        if (mp3A) {

#ifdef FEATURE_MODIFY
            if (mInitParams.usageHint.m3DNRMode & P2FeatureNode::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT)
            {
                prepare3DNR_SL2E_Info(&(inMetaSet.appMeta), &(inMetaSet.halMeta), enqueParams);
            }
#endif // FEATURE_MODIFY

            MY_LOGD_IF(mbEnableLog, "sensor(%d) P2 setIsp %p : %d", mOpenId, pTuning, tuningsize);
            if (0 > mp3A->setIsp(0, inMetaSet, &rTuningParam,
                                    (bGetResult ? &outMetaSet : NULL))) {
                MY_LOGW("sensor(%d) P2 setIsp - skip tuning pushing", mOpenId);
                if (pTuning != NULL) {
                    MY_LOGD_IF(mbEnableLog, "sensor(%d) P2 setIsp free %p : %d", mOpenId, pTuning, tuningsize);
                    ::free(pTuning);
                }
            } else {
                enqueParams.mvTuningData.push_back(pTuning);
                //
                IImageBuffer* pSrc = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);
                if (pSrc != NULL) {
                    Input src;
                    src.mPortID       = PORT_DEPI;
                    src.mPortID.group = 0;
                    src.mBuffer       = pSrc;
                    //
                    enqueParams.mvIn.push_back(src);
                    MY_LOGD_IF(mbEnableLog, "sensor(%d) P2Node EnQ Src mPortID.index(%d) Fmt(0x%x) "
                        "Size(%dx%d)", mOpenId, src.mPortID.index, src.mBuffer->getImgFormat(),
                        src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
                }
            }
        } else {
            MY_LOGD_IF(mbEnableLog, "sensor(%d) P2 setIsp clear tuning %p : %d", mOpenId, pTuning, tuningsize);
            ::memset((unsigned char*)(pTuning), 0, tuningsize);
        }
        //
        if( pMeta_OutApp ) {
            *pMeta_OutApp = outMetaSet.appMeta;
            //
            MRect cropRegion = pCropInfo->crop_a;
            if( pCropInfo->isEisEabled ) {
                cropRegion.p.x += pCropInfo->eis_mv_a.p.x;
                cropRegion.p.y += pCropInfo->eis_mv_a.p.y;
            }
            //
            updateCropRegion(cropRegion, pMeta_OutApp);
        }
        //
        if( pMeta_OutHal ) {
            *pMeta_OutHal = *pMeta_InHal;
            *pMeta_OutHal += outMetaSet.halMeta;
#ifdef FEATURE_MODIFY
            makeDebugExif(pMeta_InHal , pMeta_OutHal);
#endif
        }
    }

    // callback
    enqueParams.mpfnCallback = pass2CbFunc;
    enqueParams.mpCookie     = this;

    //
    #if 0
    // FIXME: need this?
    enqueParams.mvPrivaData.push_back(NULL);

    // for crop
    enqueParams.mvP1SrcCrop.push_back(pCropInfo->crop_p1_sensor);
    enqueParams.mvP1Dst.push_back(pCropInfo->dstsize_resizer);
    enqueParams.mvP1DstCrop.push_back(pCropInfo->crop_dma);
    #endif
    //
    MY_LOGD("sensor(%d) cnt %d, in %d, out %d",
            mOpenId, muEnqueCnt, enqueParams.mvIn.size(), enqueParams.mvOut.size() );
    //
    { // add request to queue
        Mutex::Autolock _l(mLock);
        mvRunning.push_back(pRequest);
#if P2_DEQUE_DEBUG
        mvParams.push_back(enqueParams);
#endif
        muEnqueCnt++;
    }
    //
    {
        MY_LOGD_IF(mbEnableLog, "sensor(%d) enque pass2 ...", mOpenId);
        CAM_TRACE_NAME("P2F:[Proc]drv enq");
#ifdef FEATURE_MODIFY
        featureEnqueParams.setQParams(enqueParams);
#endif // FEATURE_MODIFY

        if( !mpPipe->enque(featureEnqueParams) )
        {
            MY_LOGE("sensor(%d) enque pass2 failed", mOpenId);
            //
            { // remove job from queue
                Mutex::Autolock _l(mLock);
                vector<sp<Request> >::iterator iter = mvRunning.end();
                while( iter != mvRunning.begin() ) {
                    iter--;
                    if( *iter == pRequest ) {
                        mvRunning.erase(iter);
                        break;
                    }
                }

                MY_LOGW("sensor(%d) cnt %d execute failed", mOpenId, muDequeCnt);
                muDequeCnt++;
            }
            return UNKNOWN_ERROR;
        }
        MY_LOGD_IF(mbEnableLog, "sensor(%d) enque pass2 success", mOpenId);
    }
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
onFinish(
    FrameParams const& params,
    MBOOL const success
)
{
    CAM_TRACE_NAME("P2F:[Proc]Finish");
    //params.in.mHandle->updateState(BufferHandle::Buffer_ReadDone);
    for( size_t i = 0; i < params.vOut.size(); i++ )
    #ifdef FEATURE_MODIFY
        if( params.vOut[i].mHandle != NULL && params.vOut[i].mHandle->isValid() )
    #else
        if (params.vOut[i].mHandle.get())
    #endif // FEATURE_MODIFY
            params.vOut[i].mHandle->updateState(
                    success ? BufferHandle::STATE_WRITE_OK : BufferHandle::STATE_WRITE_FAIL
                    );
    #if 1
    if (mbEnableDumpBuffer)
    {
        MY_LOGD("sensor(%d) [YUV] DequeCnt(%d) size(%d)", mOpenId, muDequeCnt, params.vOut.size());
        sp<IImageBuffer> pImgBuf = NULL;
        for( size_t i = 0; i < params.vOut.size(); i++ )
        {
            #ifdef FEATURE_MODIFY
            if( params.vOut[i].mHandle == NULL || !params.vOut[i].mHandle->isValid() )
            {
                continue;
            }
            #endif // FEATURE_MODIFY
            pImgBuf = params.vOut[i].mHandle->getBuffer();
            MY_LOGD("sensor(%d) [YUV] [%d] (%dx%d) Fmt(0x%x)", mOpenId, i,
                pImgBuf->getImgSize().w, pImgBuf->getImgSize().h,
                pImgBuf->getImgFormat()
            );
            {
                char filename[256] = {0};
                sprintf(filename, "/sdcard/yuv/p2-%04d-%dx%d.yuv",
                    muDequeCnt,
                    pImgBuf->getImgSize().w,
                    pImgBuf->getImgSize().h);
                MY_LOGD("sensor(%d) [YUV] save %s", mOpenId, filename);
                pImgBuf->saveToFile(filename);
            }
        }
    }
    #endif
    if( params.outApp.get() )
        params.outApp->updateState(success ? MetaHandle::STATE_WRITE_OK : MetaHandle::STATE_WRITE_FAIL);
    if( params.outHal.get() )
        params.outHal->updateState(success ? MetaHandle::STATE_WRITE_OK : MetaHandle::STATE_WRITE_FAIL);
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
pass2CbFunc(QParams& rParams)
{
    //MY_LOGD_IF(mbEnableLog, "pass2CbFunc +++");
    P2Procedure* pProcedure = reinterpret_cast<P2Procedure*>(rParams.mpCookie);
    pProcedure->handleDeque(rParams);
    //MY_LOGD_IF(mbEnableLog, "pass2CbFunc ---");
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
handleDeque(QParams& rParams)
{
    CAM_TRACE_NAME("P2F:[Proc]deque");
    Mutex::Autolock _l(mLock);
    sp<Request> pRequest = NULL;
    {
        MY_LOGD_IF(mbEnableLog, "sensor(%d) p2 done %d, success %d", mOpenId, muDequeCnt, rParams.mDequeSuccess);
        pRequest = mvRunning.front();
        mvRunning.erase(mvRunning.begin());
        muDequeCnt++;
        MY_LOGD("sensor(%d) p2 done muDequeCnt:%d", mOpenId, muDequeCnt);

        if( mDebugScanLineMask != 0 &&
            mpDebugScanLine != NULL)
        {
            for(size_t i = 0; i < rParams.mvOut.size(); i++)
            {
                #ifdef FEATURE_MODIFY
                if( rParams.mvOut[i].mBuffer == NULL )
                {
                    continue;
                }
                #endif // FEATURE_MODIFY

                if( (   rParams.mvOut[i].mPortID.index == PORT_WDMAO.index &&
                        mDebugScanLineMask & DRAWLINE_PORT_WDMAO) ||
                    (   rParams.mvOut[i].mPortID.index == PORT_WROTO.index &&
                        mDebugScanLineMask & DRAWLINE_PORT_WROTO) ||
                    (   rParams.mvOut[i].mPortID.index == PORT_IMG2O.index &&
                        mDebugScanLineMask & DRAWLINE_PORT_IMG2O)   )
                {
                    mpDebugScanLine->drawScanLine(
                                        rParams.mvOut[i].mBuffer->getImgSize().w,
                                        rParams.mvOut[i].mBuffer->getImgSize().h,
                                        (void*)(rParams.mvOut[i].mBuffer->getBufVA(0)),
                                        rParams.mvOut[i].mBuffer->getBufSizeInBytes(0),
                                        rParams.mvOut[i].mBuffer->getBufStridesInBytes(0));
                }
            }
        }

#if P2_DEQUE_DEBUG
        if( mvParams.size() )
        {
            QParams checkParam;
            checkParam = mvParams.front();
            mvParams.erase(mvParams.begin());
            //
            // make sure params are correct
            //
            #define ERROR_IF_NOT_MATCHED(item, i, expected, actual) do{             \
                if( expected != actual)                                             \
                    MY_LOGE("sensor(%d) %s %d: expected %p != %p", mOpenId, item, i, expected, actual); \
            } while(0)
            //
            for( size_t i = 0 ; i < checkParam.mvIn.size() ; i++ )
            {
                if( i > rParams.mvIn.size() ) {
                    MY_LOGE("sensor(%d) no src in dequed Params", mOpenId);
                    break;
                }
                #ifdef FEATURE_MODIFY
                if( rParams.mvIn[i].mBuffer == NULL )
                {
                    continue;
                }
                #endif // FEATURE_MODIFY
                //
                ERROR_IF_NOT_MATCHED("src pa of in", i,
                        checkParam.mvIn[i].mBuffer->getBufPA(0),
                        rParams.mvIn[i].mBuffer->getBufPA(0)
                        );
                ERROR_IF_NOT_MATCHED("src va of in", i,
                        checkParam.mvIn[i].mBuffer->getBufVA(0),
                        rParams.mvIn[i].mBuffer->getBufVA(0)
                        );
            }
            //
            for( size_t i = 0 ; i < checkParam.mvOut.size() ; i++ )
            {
                if( i > rParams.mvOut.size() ) {
                    MY_LOGE("sensor(%d) no enough dst in dequed Params, %d", mOpenId, i);
                    break;
                }
                #ifdef FEATURE_MODIFY
                if( rParams.mvOut[i].mBuffer == NULL )
                {
                    continue;
                }
                #endif // FEATURE_MODIFY
                //
                ERROR_IF_NOT_MATCHED("dst pa of out", i,
                        checkParam.mvOut[i].mBuffer->getBufPA(0),
                        rParams.mvOut[i].mBuffer->getBufPA(0)
                        );
                ERROR_IF_NOT_MATCHED("dst va of out", i,
                        checkParam.mvOut[i].mBuffer->getBufVA(0),
                        rParams.mvOut[i].mBuffer->getBufVA(0)
                        );
            }
            //
            #undef ERROR_IF_NOT_MATCHED
        }
        else {
            MY_LOGW("sensor(%d) params size not matched", mOpenId);
        }
#endif
    }
    //
    if( rParams.mvTuningData.size() > 0 ) {
        void* pTuning = rParams.mvTuningData[0];
        if( pTuning ) {
            free(pTuning);
        }
    }
    //
    pRequest->responseDone(rParams.mDequeSuccess ? OK : UNKNOWN_ERROR);
    //
    mCondJob.signal();
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
checkParams(FrameParams const params) const
{
#define CHECK(lv, val, fail_ret, ...) \
    do{                               \
        if( !(val) )                  \
        {                             \
            MY_LOG##lv(__VA_ARGS__);  \
            return fail_ret;          \
        }                             \
    } while(0)
    //
    CHECK( E, params.in.mHandle.get() , BAD_VALUE , "no src handle" );
    CHECK( D, params.vOut.size()      , BAD_VALUE , "no dst" );
    CHECK( E, params.inApp.get()      , BAD_VALUE , "no in app meta" );
    CHECK( E, params.inHal.get()      , BAD_VALUE , "no in hal meta" );
    //
#undef CHECK
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
#ifdef FEATURE_MODIFY
MERROR
P2Procedure::
getCropInfos_NoEIS(
    IMetadata* const inApp,
    IMetadata* const inHal,
    MBOOL const isResized,
    MSize const& dstSize,
    MCropRect& result
) const
{
    Cropper::crop_info_t cropInfos;

    if( ! tryGetMetadata<MSize>(inHal, MTK_HAL_REQUEST_SENSOR_SIZE, cropInfos.sensor_size) ) {
        MY_LOGE("sensor(%d) cannot get MTK_HAL_REQUEST_SENSOR_SIZE", mOpenId);
        return BAD_VALUE;
    }
    //
    MSize const sensor = cropInfos.sensor_size;
    MSize const active = mInitParams.activeArray.s;
    //
    cropInfos.isResized = isResized;
    // get current p1 buffer crop status
    if(
            !( tryGetMetadata<MRect>(inHal, MTK_P1NODE_SCALAR_CROP_REGION, cropInfos.crop_p1_sensor) &&
               tryGetMetadata<MSize>(inHal, MTK_P1NODE_RESIZER_SIZE      , cropInfos.dstsize_resizer) &&
               tryGetMetadata<MRect>(inHal, MTK_P1NODE_DMA_CROP_REGION   , cropInfos.crop_dma)
             )
      ) {
        MY_LOGW_IF(1, "sensor(%d) [FIXME] should sync with p1 for rrz setting", mOpenId);
        //
        cropInfos.crop_p1_sensor  = MRect( MPoint(0,0), sensor );
        cropInfos.dstsize_resizer = sensor;
        cropInfos.crop_dma        = MRect( MPoint(0,0), sensor );
    }

    MY_LOGD_IF(mbEnableLog, "sensor(%d) SCALAR_CROP_REGION:(%d,%d)(%dx%d) RESIZER_SIZE:(%dx%d) DMA_CROP_REGION:(%d,%d)(%dx%d)",
        mOpenId,
        cropInfos.crop_p1_sensor.p.x, cropInfos.crop_p1_sensor.p.y,
        cropInfos.crop_p1_sensor.s.w, cropInfos.crop_p1_sensor.s.h,
        cropInfos.dstsize_resizer.w, cropInfos.dstsize_resizer.h,
        cropInfos.crop_dma.p.x, cropInfos.crop_dma.p.y,
        cropInfos.crop_dma.s.w, cropInfos.crop_dma.s.h);

    //
    // setup transform
    MINT32 sensorMode;
    if(!tryGetMetadata<MINT32>(inHal, MTK_P1NODE_SENSOR_MODE, sensorMode)) {
        MY_LOGE("sensor(%d) cannot get MTK_P1NODE_SENSOR_MODE", mOpenId);
        return BAD_VALUE;
    }
    //
    HwTransHelper hwTransHelper(mInitParams.openId);
    HwMatrix matToActive;
    if (!hwTransHelper.getMatrixToActive(sensorMode, cropInfos.matSensor2Active) ||
        !hwTransHelper.getMatrixFromActive(sensorMode, cropInfos.matActive2Sensor)) {
       MY_LOGE("sensor(%d) get matrix fail", mOpenId);
       return UNKNOWN_ERROR;
    }

    cropInfos.tranSensor2Resized = simpleTransform(
                cropInfos.crop_p1_sensor.p,
                cropInfos.crop_p1_sensor.s,
                cropInfos.dstsize_resizer
            );
    //
    MBOOL const isEisOn = MFALSE;
    //
    MRect cropRegion; //active array domain
    queryCropRegion(inApp, isEisOn, cropRegion);
    cropInfos.crop_a = cropRegion;
    cropInfos.isEisEabled = MFALSE;


    // coordinates: s_: sensor
    MRect s_crop;
    cropInfos.matActive2Sensor.transform(cropInfos.crop_a, s_crop);
    MRect s_viewcrop;
    //
    if( s_crop.s.w * dstSize.h > s_crop.s.h * dstSize.w ) { // pillarbox
        s_viewcrop.s.w = div_round(s_crop.s.h * dstSize.w, dstSize.h);
        s_viewcrop.s.h = s_crop.s.h;
        s_viewcrop.p.x = s_crop.p.x + ((s_crop.s.w - s_viewcrop.s.w) >> 1);
        s_viewcrop.p.y = s_crop.p.y;
    }
    else { // letterbox
        s_viewcrop.s.w = s_crop.s.w;
        s_viewcrop.s.h = div_round(s_crop.s.w * dstSize.h, dstSize.w);
        s_viewcrop.p.x = s_crop.p.x;
        s_viewcrop.p.y = s_crop.p.y + ((s_crop.s.h - s_viewcrop.s.h) >> 1);
    }
    MY_LOGD_IF(mbEnableLog, "sensor(%d) s_cropRegion(%d, %d, %dx%d), dst %dx%d, view crop(%d, %d, %dx%d)",
            mOpenId,
            s_crop.p.x     , s_crop.p.y     ,
            s_crop.s.w     , s_crop.s.h     ,
            dstSize.w      , dstSize.h      ,
            s_viewcrop.p.x , s_viewcrop.p.y ,
            s_viewcrop.s.w , s_viewcrop.s.h
           );
    //
    if( isResized ) {
        MRect r_viewcrop = transform(cropInfos.tranSensor2Resized, s_viewcrop);

        result.s            = r_viewcrop.s;
        result.p_integral   = r_viewcrop.p;
        result.p_fractional = MPoint(0,0);

        // make sure hw limitation
        result.s.w &= ~(0x1);
        result.s.h &= ~(0x1);

        // check boundary
        if( Cropper::refineBoundary(cropInfos.dstsize_resizer, result) ) {
            MY_LOGE("sensor(%d) [FIXME] need to check crop!", mOpenId);
            Cropper::dump(cropInfos);
        }
    }
    else {
        result.s            = s_viewcrop.s;
        result.p_integral   = s_viewcrop.p;
        result.p_fractional = MPoint(0,0);

        // make sure hw limitation
        result.s.w &= ~(0x1);
        result.s.h &= ~(0x1);

        // check boundary
        if( Cropper::refineBoundary(cropInfos.sensor_size, result) ) {
            MY_LOGE("sensor(%d) [FIXME] need to check crop!", mOpenId);
            Cropper::dump(cropInfos);
        }
    }
    //
    MY_LOGD_IF(mbEnableLog, "sensor(%d) resized %d, crop %d/%d, %d/%d, %dx%d",
            mOpenId,
            isResized,
            result.p_integral.x,
            result.p_integral.y,
            result.p_fractional.x,
            result.p_fractional.y,
            result.s.w,
            result.s.h
            );

    return OK;
}
#endif
/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
getCropInfos(
    IMetadata* const inApp,
    IMetadata* const inHal,
    MBOOL const isResized,
    Cropper::crop_info_t& cropInfos
) const
{
    if( ! tryGetMetadata<MSize>(inHal, MTK_HAL_REQUEST_SENSOR_SIZE, cropInfos.sensor_size) ) {
        MY_LOGE("sensor(%d) cannot get MTK_HAL_REQUEST_SENSOR_SIZE", mOpenId);
        return BAD_VALUE;
    }
    //
    MSize const sensor = cropInfos.sensor_size;
    MSize const active = mInitParams.activeArray.s;
    //
    cropInfos.isResized = isResized;
    // get current p1 buffer crop status
    if(
            !( tryGetMetadata<MRect>(inHal, MTK_P1NODE_SCALAR_CROP_REGION, cropInfos.crop_p1_sensor) &&
               tryGetMetadata<MSize>(inHal, MTK_P1NODE_RESIZER_SIZE      , cropInfos.dstsize_resizer) &&
               tryGetMetadata<MRect>(inHal, MTK_P1NODE_DMA_CROP_REGION   , cropInfos.crop_dma)
             )
      ) {
        MY_LOGW_IF(1, "sensor(%d) [FIXME] should sync with p1 for rrz setting", mOpenId);
        //
        cropInfos.crop_p1_sensor  = MRect( MPoint(0,0), sensor );
        cropInfos.dstsize_resizer = sensor;
        cropInfos.crop_dma        = MRect( MPoint(0,0), sensor );
    }

    MY_LOGD_IF(mbEnableLog, "sensor(%d) SCALAR_CROP_REGION:(%d,%d)(%dx%d) RESIZER_SIZE:(%dx%d) DMA_CROP_REGION:(%d,%d)(%dx%d)",
        mOpenId,
        cropInfos.crop_p1_sensor.p.x, cropInfos.crop_p1_sensor.p.y,
        cropInfos.crop_p1_sensor.s.w, cropInfos.crop_p1_sensor.s.h,
        cropInfos.dstsize_resizer.w, cropInfos.dstsize_resizer.h,
        cropInfos.crop_dma.p.x, cropInfos.crop_dma.p.y,
        cropInfos.crop_dma.s.w, cropInfos.crop_dma.s.h);

    //
    // setup transform
    MINT32 sensorMode;
    if(!tryGetMetadata<MINT32>(inHal, MTK_P1NODE_SENSOR_MODE, sensorMode)) {
        MY_LOGE("sensor(%d) cannot get MTK_P1NODE_SENSOR_MODE", mOpenId);
        return BAD_VALUE;
    }
    //
    HwTransHelper hwTransHelper(mInitParams.openId);
    HwMatrix matToActive;
    if (!hwTransHelper.getMatrixToActive(sensorMode, cropInfos.matSensor2Active) ||
        !hwTransHelper.getMatrixFromActive(sensorMode, cropInfos.matActive2Sensor)) {
       MY_LOGE("sensor(%d) get matrix fail", mOpenId);
       return UNKNOWN_ERROR;
    }

    cropInfos.tranSensor2Resized = simpleTransform(
                cropInfos.crop_p1_sensor.p,
                cropInfos.crop_p1_sensor.s,
                cropInfos.dstsize_resizer
            );
    //
    MBOOL const isEisOn = isEISOn(inApp);
    //
    MRect cropRegion; //active array domain
    queryCropRegion(inApp, isEisOn, cropRegion);
    cropInfos.crop_a = cropRegion;
    //
    // query EIS result
    {
        eis_region eisInfo;
        if( isEisOn && queryEisRegion(inHal, eisInfo)) {
            cropInfos.isEisEabled = MTRUE;
            // calculate mv
            vector_f* pMv_s = &cropInfos.eis_mv_s;
            vector_f* pMv_r = &cropInfos.eis_mv_r;
            MBOOL isResizedDomain = MTRUE;

            MUINT32 eis_factor = EISCustom::getEISFactor();
#if 0
            //eis in sensor domain
            isResizedDomain = MFALSE;
            pMv_s->p.x  = eisInfo.x_int - (sensor.w * (eis_factor-100)/2/eis_factor);
            pMv_s->pf.x = eisInfo.x_float;
            pMv_s->p.y  = eisInfo.y_int - (sensor.h * (eis_factor-100)/2/eis_factor);
            pMv_s->pf.y = eisInfo.y_float;
            //
            cropInfos.eis_mv_r = transform(cropInfos.tranSensor2Resized, cropInfos.eis_mv_s);
#else
            MSize const resizer = cropInfos.dstsize_resizer;

#if SUPPORT_EIS_MV

            if (eisInfo.is_from_zzr)
            {
                pMv_r->p.x  = eisInfo.x_mv_int;
                pMv_r->pf.x = eisInfo.x_mv_float;
                pMv_r->p.y  = eisInfo.y_mv_int;
                pMv_r->pf.y = eisInfo.y_mv_float;
                cropInfos.eis_mv_s = inv_transform(cropInfos.tranSensor2Resized, cropInfos.eis_mv_r);
            }
            else
            {
                isResizedDomain = MFALSE;
                pMv_s->p.x  = eisInfo.x_mv_int;
                pMv_s->pf.x = eisInfo.x_mv_float;
                pMv_s->p.y  = eisInfo.y_mv_int;
                pMv_s->pf.y = eisInfo.y_mv_float;
                cropInfos.eis_mv_r = transform(cropInfos.tranSensor2Resized, cropInfos.eis_mv_s);
            }
#else
            //eis in resized domain
            pMv_r->p.x  = eisInfo.x_int - (resizer.w * (eis_factor-100)/2/eis_factor);
            pMv_r->pf.x = eisInfo.x_float;
            pMv_r->p.y  = eisInfo.y_int - (resizer.h * (eis_factor-100)/2/eis_factor);
            pMv_r->pf.y = eisInfo.y_float;
            //
            cropInfos.eis_mv_s = inv_transform(cropInfos.tranSensor2Resized, cropInfos.eis_mv_r);
#endif
            //
            MY_LOGD_IF(mbEnableLog, "sensor(%d) mv (%s): (%d, %d, %d, %d) -> (%d, %d, %d, %d)",
                    mOpenId,
                    isResizedDomain ? "r->s" : "s->r",
                    pMv_r->p.x,
                    pMv_r->pf.x,
                    pMv_r->p.y,
                    pMv_r->pf.y,
                    pMv_s->p.x,
                    pMv_s->pf.x,
                    pMv_s->p.y,
                    pMv_s->pf.y
                    );
#endif
            // cropInfos.eis_mv_a = inv_transform(cropInfos.tranActive2Sensor, cropInfos.eis_mv_s);
            cropInfos.matSensor2Active.transform(cropInfos.eis_mv_s.p,cropInfos.eis_mv_a.p);
            // FIXME: float
            //cropInfos.matSensor2Active.transform(cropInfos.eis_mv_s.pf,cropInfos.eis_mv_a.pf);

            MY_LOGD_IF(mbEnableLog, "sensor(%d) mv in active %d/%d, %d/%d",
                    mOpenId,
                    cropInfos.eis_mv_a.p.x,
                    cropInfos.eis_mv_a.pf.x,
                    cropInfos.eis_mv_a.p.y,
                    cropInfos.eis_mv_a.pf.y
                    );
        }
        else {
            cropInfos.isEisEabled = MFALSE;
            //
            // no need to set 0
            //memset(&cropInfos.eis_mv_a, 0, sizeof(vector_f));
            //memset(&cropInfos.eis_mv_s, 0, sizeof(vector_f));
            //memset(&cropInfos.eis_mv_r, 0, sizeof(vector_f));
        }
    }
    // debug
    //cropInfos.dump();
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
queryCropRegion(
    IMetadata* const meta_request,
    MBOOL const isEisOn,
    MRect& cropRegion
) const
{
    if( !tryGetMetadata<MRect>(meta_request, MTK_SCALER_CROP_REGION, cropRegion) ) {
        cropRegion.p = MPoint(0,0);
        cropRegion.s = mInitParams.activeArray.s;
        MY_LOGW_IF(mbEnableLog, "sensor(%d) no MTK_SCALER_CROP_REGION, crop full size %dx%d",
                mOpenId, cropRegion.s.w, cropRegion.s.h);
    }
    MY_LOGD_IF(mbEnableLog, "sensor(%d) control: cropRegion(%d, %d, %dx%d)",
            mOpenId, cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h);
    //
    #if SUPPORT_EIS
    if( isEisOn ) {
        MUINT32 eis_factor = EISCustom::getEISFactor();
        cropRegion.p.x += (cropRegion.s.w * (eis_factor-100)/2/eis_factor);
        cropRegion.p.y += (cropRegion.s.h * (eis_factor-100)/2/eis_factor);
        cropRegion.s   = cropRegion.s * 100 / eis_factor;
        MY_LOGD_IF(mbEnableLog, "sensor(%d) EIS: factor %d, cropRegion(%d, %d, %dx%d)",
                mOpenId, eis_factor,
                cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h);
    }
    #endif
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
updateCropRegion(
    MRect const crop,
    IMetadata* meta_result
) const
{
    trySetMetadata<MRect>(meta_result, MTK_SCALER_CROP_REGION, crop);
    //
    MY_LOGD_IF( DEBUG_LOG && mbEnableLog, "mSensor(%d) result: cropRegion (%d, %d, %dx%d)",
            mOpenId, crop.p.x, crop.p.y, crop.s.w, crop.s.h);
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2Procedure::
isEISOn(
    IMetadata* const inApp
) const
{
    MUINT8 eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    if( !tryGetMetadata<MUINT8>(inApp, MTK_CONTROL_VIDEO_STABILIZATION_MODE, eisMode) ) {
        MY_LOGW_IF(mbEnableLog, "sensor(%d) no MTK_CONTROL_VIDEO_STABILIZATION_MODE", mOpenId);
    }
#if FORCE_EIS_ON
    eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
#endif
    return eisMode == MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2Procedure::
queryEisRegion(
    IMetadata* const inHal,
    eis_region& region
) const
{
    IMetadata::IEntry entry = inHal->entryFor(MTK_EIS_REGION);

#if SUPPORT_EIS_MV
    // get EIS's motion vector
    if (entry.count() > 8)
    {
        MINT32 x_mv         = entry.itemAt(6, Type2Type<MINT32>());
        MINT32 y_mv         = entry.itemAt(7, Type2Type<MINT32>());
        region.is_from_zzr  = entry.itemAt(8, Type2Type<MINT32>());
        MBOOL x_mv_negative = x_mv >> 31;
        MBOOL y_mv_negative = y_mv >> 31;
        // convert to positive for getting parts of int and float if negative
        if (x_mv_negative) x_mv = ~x_mv + 1;
        if (y_mv_negative) y_mv = ~y_mv + 1;
        //
        region.x_mv_int   = (x_mv & (~0xFF)) >> 8;
        region.x_mv_float = (x_mv & (0xFF)) << 31;
        if(x_mv_negative){
            region.x_mv_int   = ~region.x_mv_int + 1;
            region.x_mv_float = ~region.x_mv_float + 1;
        }
        region.y_mv_int   = (y_mv& (~0xFF)) >> 8;
        region.y_mv_float = (y_mv& (0xFF)) << 31;
        if(y_mv_negative){
            region.y_mv_int   = ~region.y_mv_int + 1;
            region.y_mv_float = ~region.x_mv_float + 1;
        }
        //
        MY_LOGD_IF(mbEnableLog, "sensor(%d) EIS MV:%d, %d, %d",
                        mOpenId
                        region.s.w,
                        region.s.h,
                        region.is_from_zzr);
    }
#endif

#ifdef FEATURE_MODIFY
    if (entry.count() > 10)
    {
        region.gmvX = entry.itemAt(9, Type2Type<MINT32>());
        region.gmvY = entry.itemAt(10, Type2Type<MINT32>());
        MY_LOGD_IF(mbEnableLog, "sensor(%d) EIS gmvX:%d, gmvY: %d", mOpenId, region.gmvX, region.gmvY);
    }
#endif // FEATURE_MODIFY

    // get EIS's region
    if (entry.count() > 5)
    {
        region.x_int        = entry.itemAt(0, Type2Type<MINT32>());
        region.x_float      = entry.itemAt(1, Type2Type<MINT32>());
        region.y_int        = entry.itemAt(2, Type2Type<MINT32>());
        region.y_float      = entry.itemAt(3, Type2Type<MINT32>());
        region.s.w          = entry.itemAt(4, Type2Type<MINT32>());
        region.s.h          = entry.itemAt(5, Type2Type<MINT32>());
        //
        MY_LOGD_IF(mbEnableLog, "sensor(%d) EIS Region: %d, %d, %d, %d, %dx%d",
                        mOpenId,
                        region.x_int,
                        region.x_float,
                        region.y_int,
                        region.y_float,
                        region.s.w,
                        region.s.h);
       return MTRUE;
    }
    //
    MY_LOGW("sensor(%d) wrong eis region count %zu", mOpenId, entry.count());
    return MFALSE;
}


/******************************************************************************
 *
 ******************************************************************************/
sp<Processor>
MDPProcedure::
createProcessor(InitParams& params)
{
    return new ProcessorBase<MDPProcedure>(params, PROCESSOR_NAME_MDP);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
MDPProcedure::
onExecute(
    sp<Request> const pRequest,
    FrameParams const& params
)
{
    CAM_TRACE_NAME("P2F:[MDP]exe");
    FUNC_START;
    //
    MERROR ret = OK;
    //
    IImageBuffer* pSrc = NULL;
    vector<IImageBuffer*> vDst;
    vector<MUINT32> vTransform;
    vector<MRect> vCrop;

    // input
    if( params.in.mHandle.get() )
    {
        if( OK != (ret = params.in.mHandle->waitState(BufferHandle::STATE_READABLE)) ) {
            MY_LOGW("src buffer err = %d", ret);
            return ret;
        }
        pSrc = params.in.mHandle->getBuffer();
    }
    else {
        MY_LOGW("no src");
        return BAD_VALUE;
    }
    //
    // output
    for (size_t i = 0; i < params.vOut.size(); i++)
    {
        if (params.vOut[i].mHandle == NULL ||
            OK != (ret = params.vOut[i].mHandle->waitState(BufferHandle::STATE_WRITABLE))) {
            MY_LOGW("dst buffer err = %d", ret);
            continue;
        }
        IImageBuffer* pDst = params.vOut[i].mHandle->getBuffer();
        //
        if (pDst != NULL)
        {
            MCropRect cropRect;
#if 0
            MY_LOGD("dump crop info");
            Cropper::dump(*params.pCropInfo);
#endif
            MINT32 const transform  = params.vOut[i].mTransform;
            MSize dstSize = ( transform & eTransform_ROT_90 )
                            ? MSize(pDst->getImgSize().h, pDst->getImgSize().w)
                            : pDst->getImgSize();

            Cropper::calcViewAngle(mbEnableLog, *params.pCropInfo, dstSize, cropRect);
            MRect crop(cropRect.p_integral, cropRect.s);

            vCrop.push_back(crop);
            vDst.push_back(pDst);
            vTransform.push_back(params.vOut[i].mHandle->getTransform());

            MY_LOGD_IF(DEBUG_LOG, "mdp req:%d out:%d/%d size:%dx%d crop (%d,%d) %dx%d",
                pRequest->frameNo, i, params.vOut.size(),
                pDst->getImgSize().w, pDst->getImgSize().h,
                crop.p.x, crop.p.y, crop.s.w, crop.s.h);
        }
        else
            MY_LOGW("mdp req:%d empty buffer", pRequest->frameNo);
    }
    //
    if (pSrc == NULL || vDst.size() == 0) {
        MY_LOGE("wrong mdp in/out: src %p, dst count %d", pSrc, vDst.size());
        return BAD_VALUE;
    }
    //

    MBOOL success = MFALSE;
    {
        //
        #ifdef USING_MTK_LDVT
        success = MTRUE;
        #else
        NSSImager::IImageTransform* pTrans = NSSImager::IImageTransform::createInstance();
        if( !pTrans ) {
            MY_LOGE("!pTrans");
            return UNKNOWN_ERROR;
        }
        //
        success =
            pTrans->execute(
                pSrc,
                vDst[0],
                (vDst.size() > 1 )? vDst[1] : NULL,
                vCrop[0],
                (vCrop.size() > 1 )? vCrop[1] : NULL,
                vTransform[0],
                (vTransform.size() > 1 )? vTransform[1] : NULL,
                0xFFFFFFFF
                );

        pTrans->destroyInstance();
        pTrans = NULL;
        #endif
    }
    //
    FUNC_END;
    return success ? OK : UNKNOWN_ERROR;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
MDPProcedure::
onFinish(
    FrameParams const& params,
    MBOOL const success
)
{
    CAM_TRACE_NAME("P2F:[MDP]Finish");
    //params.in.mHandle->updateState(BufferHandle::Buffer_ReadDone);
    for( size_t i = 0; i < params.vOut.size(); i++ )
        if (params.vOut[i].mHandle.get())
            params.vOut[i].mHandle->updateState(
                    success ? BufferHandle::STATE_WRITE_OK : BufferHandle::STATE_WRITE_FAIL
                    );
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
onExtractParams(Request* pRequest, FrameParams& param_p2)
{
    // input buffer
    {
        StreamId_T const streamId = pRequest->context.in_buffer->getStreamId();
        param_p2.in.mPortId = PORT_IMGI;
        param_p2.in.mHandle = pRequest->context.in_buffer;
        param_p2.in_lcso.mPortId = PORT_LCEI;
        param_p2.in_lcso.mHandle = pRequest->context.in_lcso_buffer;
        param_p2.bResized = pRequest->context.resized;
        #ifndef FEATURE_MODIFY
        pRequest->context.in_buffer.clear();
        pRequest->context.in_lcso_buffer.clear();
        #endif // FEATURE_MODIFY
    }

    // output buffer
    MUINT32 occupied = 0;
    MBOOL remains = MFALSE;
    vector<sp<BufferHandle> >::iterator iter = pRequest->context.out_buffers.begin();
    for (; iter !=  pRequest->context.out_buffers.end(); iter++)
    {
        sp<BufferHandle> pOutBuffer = *iter;
        if(!pOutBuffer.get())
            continue;

        StreamId_T const streamId = pOutBuffer->getStreamId();
        MUINT32 const transform = pOutBuffer->getTransform();
        MUINT32 const usage = pOutBuffer->getUsage();

        PortID port_p2;
        MBOOL isFdStream = streamId == pRequest->context.fd_stream_id;
        if (OK == mapPortId(streamId, transform, isFdStream, occupied, port_p2))
        {
            P2Procedure::FrameOutput out;
            out.mPortId = port_p2;
            out.mHandle = pOutBuffer;
            out.mTransform = transform;
            out.mUsage = usage;

            param_p2.vOut.push_back(out);
            #ifndef FEATURE_MODIFY
            (*iter).clear();
            #endif // FEATURE_MODIFY
        }
        else
            remains = MTRUE;
    }

    if (remains && param_p2.vOut.size() > 0)
    {
        pRequest->context.in_mdp_buffer = param_p2.vOut[param_p2.vOut.size() - 1].mHandle;
    }

    param_p2.inApp = pRequest->context.in_app_meta;
    param_p2.inHal = pRequest->context.in_hal_meta;
    param_p2.outApp = pRequest->context.out_app_meta;
    param_p2.outHal = pRequest->context.out_hal_meta;

    pRequest->context.in_app_meta.clear();
    pRequest->context.in_hal_meta.clear();
    pRequest->context.out_app_meta.clear();
    pRequest->context.out_hal_meta.clear();

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
MDPProcedure::
onExtractParams(Request* pRequest, FrameParams& param_mdp)
{
    if (!pRequest->context.in_mdp_buffer.get())
    {
        return NOT_ENOUGH_DATA;
    }
    param_mdp.in.mHandle = pRequest->context.in_mdp_buffer;
    pRequest->context.in_mdp_buffer.clear();

    // input&output buffer
    vector<sp<BufferHandle> >::iterator iter = pRequest->context.out_buffers.begin();
    while(iter !=  pRequest->context.out_buffers.end())
    {
        sp<BufferHandle> pOutBuffer = *iter;
        if (pOutBuffer.get() && pOutBuffer->getState() == BufferHandle::STATE_NOT_USED)
        {
            MDPProcedure::FrameOutput out;
            out.mHandle = pOutBuffer;
            out.mTransform = pOutBuffer->getTransform();
            param_mdp.vOut.push_back(out);
            (*iter).clear();
        }
        iter++;
    }
    return (param_mdp.vOut.size() > 0) ? OK : UNKNOWN_ERROR;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::MultiFrameHandler::
collect(sp<Request> pRequest, FeaturePipeParam& featureParams)
{
#ifdef FEATURE_MODIFY
    QParams params = featureParams.getQParams();
#endif
    MINT32 index = mvReqCollecting.size();
    // tag
    for( size_t i = 0; i < params.mvStreamTag.size(); i++ )
    {
        mParamCollecting.mvStreamTag.push_back(params.mvStreamTag[i]);
    }
    // truning
    for( size_t i = 0; i < params.mvTuningData.size(); i++ )
    {
        mParamCollecting.mvTuningData.push_back(params.mvTuningData[i]);
    }
    // input
    for( size_t i = 0; i < params.mvIn.size(); i++ )
    {
        params.mvIn.editItemAt(i).mPortID.group = index;
        mParamCollecting.mvIn.push_back(params.mvIn[i]);
    }
    // output
    for( size_t i = 0; i < params.mvOut.size(); i++ )
    {
        params.mvOut.editItemAt(i).mPortID.group = index;
        mParamCollecting.mvOut.push_back(params.mvOut[i]);
    }
    // crop
    for( size_t i = 0; i < params.mvCropRsInfo.size(); i++ )
    {
        params.mvCropRsInfo.editItemAt(i).mFrameGroup = index;
        mParamCollecting.mvCropRsInfo.push_back(params.mvCropRsInfo[i]);
    }
    // module
    for( size_t i = 0; i < params.mvModuleData.size(); i++ )
    {
        params.mvModuleData.editItemAt(i).frameGroup = index;
        mParamCollecting.mvModuleData.push_back(params.mvModuleData[i]);
    }

    mvReqCollecting.push_back(pRequest);
    if(mvReqCollecting.size() >= pRequest->context.burst_num)
    {
        QParams enqueParams = mParamCollecting;
        // callback
        enqueParams.mpfnCallback = callback;
        enqueParams.mpCookie     = this;

        MY_LOGD_IF(mbEnableLog, "[burst] cnt %d, in %d, out %d",
                muMfEnqueCnt, enqueParams.mvIn.size(), enqueParams.mvOut.size() );
        muMfEnqueCnt++;
        {
#ifdef FEATURE_MODIFY
            featureParams.setQParams(enqueParams);
#endif // FEATURE_MODIFY
            MY_LOGD("[burst] Pass2 enque");
            CAM_TRACE_NAME("[burst] drv_enq");
            if (mpPipe->enque(featureParams))
            {
                Mutex::Autolock _l(mLock);
                MY_LOGD("[burst] Pass2 enque success");
                mvRunning.push_back(mvReqCollecting);
            }
            else
            {
                MY_LOGE("[burst] Pass2 enque failed");
                Mutex::Autolock _l(mLock);
                vector<sp<Request> >::iterator iter = mvReqCollecting.begin();
                while (iter != mvReqCollecting.end())
                {
                    (*iter)->responseDone(UNKNOWN_ERROR);
                    iter++;
                }

                MY_LOGW("[burst] cnt %d execute failed", muMfDequeCnt);
                muMfDequeCnt++;
            }
        }
        // clear the collected request
        mParamCollecting.mvStreamTag.clear();
        mParamCollecting.mvTuningData.clear();
        mParamCollecting.mvIn.clear();
        mParamCollecting.mvOut.clear();
        mParamCollecting.mvCropRsInfo.clear();
        mParamCollecting.mvModuleData.clear();
        mvReqCollecting.clear();
    }
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::MultiFrameHandler::
handleCallback(QParams& rParams)
{
    CAM_TRACE_NAME("[burst] P2 deque");
    Mutex::Autolock _l(mLock);
    vector<sp<Request> > vpRequest;
    {
        MY_LOGD_IF(mbEnableLog, "[burst] p2 done %d, success %d", muMfDequeCnt, rParams.mDequeSuccess);
        vpRequest = mvRunning.front();
        mvRunning.erase(mvRunning.begin());
        muMfDequeCnt++;
        MY_LOGD_IF(mbEnableLog, "[burst] p2 done muDequeCnt:%d", muMfDequeCnt);
    }
    //
    for (size_t i = 0; i < rParams.mvTuningData.size(); i++ )
    {
        void* pTuning = rParams.mvTuningData[i];
        if( pTuning ) {
            free(pTuning);
        }
    }
    //
    vector<sp<Request> >::iterator iter = vpRequest.begin();
    while (iter != vpRequest.end())
    {
        (*iter)->responseDone(rParams.mDequeSuccess ? OK : UNKNOWN_ERROR);
        iter++;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::MultiFrameHandler::
flush()
{
    FUNC_START;

    mParamCollecting = QParams();
    //mvReqCollecting.clear();
    vector<sp<Request> >::iterator iter = mvReqCollecting.begin();
    while (iter != mvReqCollecting.end())
    {
        (*iter)->responseDone(UNKNOWN_ERROR);
        iter = mvReqCollecting.erase(iter);
    }

    FUNC_END;
    return;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
onFlush()
{
    CAM_TRACE_NAME("P2F:[Proc]Flush");
#ifdef FEATURE_MODIFY
    if(mpPipe)
    {
        mpPipe->flush();
    }
#endif // FEATURE_MODIFY
    if(mpMultiFrameHandler)
    {
        mpMultiFrameHandler->flush();
    }
    return;
}

#ifdef FEATURE_MODIFY

#define UHD_VR_WIDTH  (3840)
#define UHD_VR_HEIGHT (2160)
MBOOL P2Procedure::isCRZApplied(IMetadata* const inApp, QParams& rParams)
{
    MSize maxSize = MSize(0, 0);
    MUINT32 max = 0;
    for( unsigned i = 0, n = rParams.mvOut.size(); i < n; ++i )
    {
        MSize size = rParams.mvOut[i].mBuffer->getImgSize();
        MUINT32 temp = size.w * size.h;
        if( temp > max )
        {
            maxSize = size;
            max = temp;
        }
    }
    if ( (maxSize.w >= UHD_VR_WIDTH) &&
         (maxSize.h >= UHD_VR_HEIGHT) && isEISOn(inApp))
    {
        return MTRUE;
    }
    return MFALSE;

}
#endif // FEATURE_MODIFY

} // namespace P2Feature
