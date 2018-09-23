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
#define LOG_TAG "MtkCam/StereoSynchronizer"
//
#include "MyUtils.h"
//
#include <sys/prctl.h>
#include <sys/resource.h>
#include <system/thread_defs.h>
#include <cutils/properties.h>
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Time.h>
//
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
//
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProvider.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoSelector.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoBufferSynchronizer.h>
// STL
#include <map>
#include <list>

using namespace std;
using namespace android;
using namespace NSCam;
using namespace NSCam::v1;

#define THREAD_NAME       ("Cam@StereoSynchronizer")
#define THREAD_POLICY     (SCHED_OTHER)
#define THREAD_PRIORITY   (0)


/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s][%s] " fmt, getUserName(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

#define MY_LOGW_NO_USERNAME(fmt, arg...)        CAM_LOGW("[%s][%s] " fmt, __FUNCTION__, ##arg)

#define MY_LOGD1(...)               MY_LOGD_IF(1<=mLogLevel, __VA_ARGS__)
#define MY_LOGD2(...)               MY_LOGD_IF(2<=mLogLevel, __VA_ARGS__)

#define FUNC_START                  MY_LOGD2("+")
#define FUNC_END                    MY_LOGD2("-")

#define AVA_QUE_KEEP_COUNT              1
#define PEN_QUE_KEEP_COUNT              2
#define SYNC_THRESHOLD_MS               1
#define PAIR_THRESHOLD_MS               33
#define SYNC_FAIL_WARNING_COUNT         10
#define SYNC_FAIL_RESET_COUNT           20

namespace NSCam {
namespace v1 {
namespace NSLegacyPipeline{

typedef enum _SYNC_RESULT_
{
    SYNC_RESULT_PAIR_OK       = 0,
    SYNC_RESULT_MAIN1_TOO_OLD = 1,
    SYNC_RESULT_MAIN2_TOO_OLD = 2,
    SYNC_RESULT_PAIR_NOT_SYNC = 3
} SYNC_RESULT;

typedef enum _PAIR_RESULT_
{
    PAIR_RESULT_PAIR_OK         = 0,
    PAIR_RESULT_FULL_TOO_OLD    = 1,
    PAIR_RESULT_RESIZED_TOO_OLD = 2,
    PAIR_RESULT_PAIR_INVALID    = 3
}PAIR_RESULT;

class StereoBufferSynchronizerImp
    : public StereoBufferSynchronizer
    , public Thread
{
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  StereoBufferSynchronizer Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual     MERROR                  addStream(
                                                StreamId_T                      streamId,
                                                sp<StereoSelector>              pSelector
                                            );

    virtual     MERROR                  removeStream(
                                                StreamId_T                      streamId
                                            );

    virtual     MERROR                  enqueBuffer(
                                            MINT32                          reqNo,
                                            StreamId_T                      streamId,
                                            Vector<ISelector::MetaItemSet>  vResultMeta,
                                            sp<IImageBufferHeap>            pHeap
                                        );

    virtual     MERROR                  dequeBuffer(
                                            MINT32&                         rRequestNo,
                                            StreamId_T                      streamId,
                                            Vector<ISelector::MetaItemSet>& rMeta,
                                            sp<IImageBufferHeap>&           rpHeap
                                        );

    virtual     MERROR                  dequeBufferCapture(
                                            MINT32&                         rRequestNo,
                                            StreamId_T                      streamId,
                                            Vector<ISelector::MetaItemSet>& rMeta,
                                            android::sp<IImageBufferHeap>&  rpHeap
                                        );

    virtual     MERROR                  returnBuffer(
                                            android::sp<IImageBufferHeap>&  rpHeap,
                                            StreamId_T                      streamId
                                        );

    virtual     MERROR                  lock(MBOOL stopAvailableQue = MFALSE);

    virtual     MERROR                  unlock();

    virtual     MERROR                  waitAndLockCapture();

    virtual     MERROR                  unlockCapture();

    virtual     MERROR                  start();

    virtual     MERROR                  flush();

    virtual     sp<StereoSelector>      querySelector(StreamId_T streamId);

    virtual     void                    setDebugMode(MINT32 debug);

    virtual     void                    setZSLDelayCount(MINT32 delayCount);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  RefBase Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    void                                onLastStrongRef(const void* /*id*/);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Thread Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    public:    //// thread interface
    // Ask this object's thread to exit. This function is asynchronous, when the
    // function returns the thread might still be running. Of course, this
    // function can be called from a different thread.
    //virtual void        requestExit();

    // Good place to do one-time initializations
    virtual status_t    readyToRun();

private:
    // Derived class must implement threadLoop(). The thread starts its life
    // here. There are two ways of using the Thread object:
    // 1) loop: if threadLoop() returns true, it will be called again if
    //          requestExit() wasn't called.
    // 2) once: if threadLoop() returns false, the thread will exit upon return.
    virtual bool        threadLoop();
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Customized Data Types.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    typedef struct
    {
        StreamId_T                          streamId;
        MINT32                              reqNo;
        sp<IImageBufferHeap>                heap          = NULL;
        Vector<ISelector::MetaItemSet>      metadata;
        MBOOL                               isReturned;
        MBOOL                               isAFStable    = MFALSE;
    }BUFFER_SET;

    typedef struct
    {
        Vector<BUFFER_SET>                  syncBufferSet;
        MBOOL                               isAllBufferReturned;
    }ZSL_BUFFER_SET;
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Inner Classes
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    class DurationProfile {
public:
                        DurationProfile(char const*const szSubjectName, MINT32 nsWarning)
                            : msSubjectName(szSubjectName)
                            , mi4Count(0)
                            , mnsStart(0)
                            , mnsEnd(0)
                            , mnsTotal(0)
                            , mnsWarning(nsWarning)
                            , mLogLevel(0)
                            {}

    virtual            ~DurationProfile(){}

    virtual void        reset(int logLevel) { mi4Count = mnsStart = mnsEnd = mnsTotal = 0; mLogLevel = logLevel;}

    virtual void        pulse_up() {mnsStart = ::systemTime();}
    virtual void        pulse_down()
                        {
                            mnsEnd = ::systemTime();

                            MY_LOGD2("pulse_down:%lld / %lld", mnsStart, mnsEnd);

                            if (mnsStart != 0)
                            {
                                nsecs_t duration = mnsEnd - mnsStart;
                                mnsTotal += duration;
                                mi4Count++;
                                mnsStart = 0;
                            }

                            if(mi4Count != 0){
                                // MY_LOGD2("AvgDuration:%ld", ::ns2ms(mnsTotal)/mi4Count);
                                MY_LOGD2("AvgDuration:%f(us)", (((float)mnsTotal)/((float)mi4Count))/1000LL);
                            }
                            if(mnsTotal != 0){
                                MY_LOGD2("AvgFPS:%f", ((float)mi4Count/mnsTotal)*1000000000LL);
                            }
                        }

    char const*         getUserName() {return msSubjectName;};

protected:
    virtual void        print_overtime(MINT32 duration) const
                        {
                             if (duration > mnsWarning) {
                                 CAM_LOGW("[%s] duration(%f) > (%f)", msSubjectName, duration, mnsWarning);
                             }
                        }

protected:
    char const*         msSubjectName;
    int32_t             mi4Count;
    nsecs_t             mnsStart;
    nsecs_t             mnsEnd;
    nsecs_t             mnsTotal;
    nsecs_t             mnsWarning;
    int                 mLogLevel;
};
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Implementations.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
                                        StereoBufferSynchronizerImp(char const* szCallerName, MINT32 delayCount);

                                        ~StereoBufferSynchronizerImp() {};

    char const*                         getUserName() {return mUserName;};
private:
    MBOOL                               isReadyToSyncCheck();

    MBOOL                               isReadyToLock();

    MBOOL                               isZSLBufferReady();

    void                                syncRoutine(
                                            BUFFER_SET* bufInfoResized,
                                            BUFFER_SET* bufInfoResized_main2,
                                            BUFFER_SET* bufInfoFull,
                                            BUFFER_SET* bufInfoFull_main2
                                        );

    SYNC_RESULT                         isTimeSync(
                                            BUFFER_SET* bufInfoMain1,
                                            BUFFER_SET* bufInfoMain2
                                        );

    void                                returnOldestBufferToSelector(StreamId_T streamId, map< StreamId_T, list<BUFFER_SET>* >& theMap);

    void                                checkPendingQueueSize(StreamId_T streamId);

    void                                checkAllPendingQueueSize();

    void                                checkAvailableQueueSize(StreamId_T streamId);

    void                                checkAllAvailableQueueSize();

    void                                checkZSLAvailableQueueSize(MINT32 keepSize);

    void                                keepZSLOldestBuffer();

    void                                clearQueAndReturnBufferToSelector();

    void                                showPreviewPendingSizes();

    void                                showPreviewAvailableSizes();

    void                                showPreviewUsingSizes();

    void                                showCaptureAvailableSizes();

    void                                showCaptureUsingSizes();

    void                                returnBufferToSelector(BUFFER_SET* bufInfo);

    void                                returnBufferToSelector(StreamId_T streamId, android::sp<IImageBufferHeap>& theHeap);

    void                                returnBufferToPendingQue(BUFFER_SET* bufInfo);

    void                                enqueBufferToAvailableQue(BUFFER_SET* bufInfo, MINT32 assignedReqNo);

    MBOOL                                pairCheck(BUFFER_SET* bufInfoResized, BUFFER_SET* bufInfoFull);

    PAIR_RESULT                         isPairSync(BUFFER_SET* bufInfoResized, BUFFER_SET* bufInfoFull);

    MBOOL                                isInQue(list<BUFFER_SET>* theQue, android::sp<IImageBufferHeap>& theHeap);

    BUFFER_SET                          removeFromQueue(list<BUFFER_SET>* theQue, android::sp<IImageBufferHeap>& theHeap);

    MBOOL                                isCaptureBufferReady();

    MBOOL                                isPreviewUsingQueEmpty();

    MBOOL                                isCaptureUsingQueEmpty();

    MBOOL                                isAFStable(
                                            BUFFER_SET* bufInfoMain1,
                                            BUFFER_SET* bufInfoMain2
                                        );

    void                                markAFState(BUFFER_SET* bufInfoMain1, MBOOL isAFStable);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Data members.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    char                                mUserName[64];
    int                                 mLogLevel                   = 0;
    MINT32                              mDebugMode                  = 0;
    MINT32                              mZSLDelayCount              = 3;
    MBOOL                               mbIsPreviewStopped          = MFALSE;
    MBOOL                               mbExitThread                = MFALSE;
    MBOOL                               mIsLocked                   = MFALSE;
    MBOOL                               mIsCaptureLocked            = MFALSE;
    MBOOL                               mIsAvailableQueStopped      = MFALSE;

    mutable Mutex                       mLock;
    mutable Mutex                       mPreviewPendingQueLock;
    mutable Mutex                       mPreviewAvailableQueLock;
    mutable Mutex                       mPreviewUsingQueLock;

    mutable Mutex                       mCaptureAvailableQueLock;
    mutable Mutex                       mCaptureUsingQueLock;

    mutable Condition                   mCondPushPreviewPending;
    mutable Condition                   mCondPushPreviewAvailable;
    mutable Condition                   mCondLockPreviewAvailable;
    mutable Condition                   mCondPushCaptureAvailable;
    mutable Condition                   mCondPreviewUsingReturned;
    mutable Condition                   mCondCaptureUsingReturned;

    // preview buffer queues
    list<BUFFER_SET>                    previewPendingResizeRaw;
    list<BUFFER_SET>                    previewPendingFullRaw;
    list<BUFFER_SET>                    previewPendingResizeRaw_main2;
    list<BUFFER_SET>                    previewPendingFullRaw_main2;

    list<BUFFER_SET>                    previewAvailableResizeRaw;
    list<BUFFER_SET>                    previewAvailableFullRaw;
    list<BUFFER_SET>                    previewAvailableResizeRaw_main2;
    list<BUFFER_SET>                    previewAvailableFullRaw_main2;

    list<BUFFER_SET>                    previewUsingResizeRaw;
    list<BUFFER_SET>                    previewUsingFullRaw;
    list<BUFFER_SET>                    previewUsingResizeRaw_main2;
    list<BUFFER_SET>                    previewUsingFullRaw_main2;

    // capture buffer queues
    list<BUFFER_SET>                    captureAvailableResizeRaw;
    list<BUFFER_SET>                    captureAvailableFullRaw;
    list<BUFFER_SET>                    captureAvailableResizeRaw_main2;
    list<BUFFER_SET>                    captureAvailableFullRaw_main2;

    list<BUFFER_SET>                    captureUsingResizeRaw;
    list<BUFFER_SET>                    captureUsingFullRaw;
    list<BUFFER_SET>                    captureUsingResizeRaw_main2;
    list<BUFFER_SET>                    captureUsingFullRaw_main2;

    map< StreamId_T, list<BUFFER_SET>* > streamToPreviewPendingQue;
    map< StreamId_T, list<BUFFER_SET>* > streamToPreviewAvailableQue;
    map< StreamId_T, list<BUFFER_SET>* > streamToPreviewUsingQue;
    map< StreamId_T, list<BUFFER_SET>* > streamToCaptureAvailableQue;
    map< StreamId_T, list<BUFFER_SET>* > streamToCaptureUsingQue;
    typedef pair<StreamId_T, list<BUFFER_SET>* > PairStreamToBufferQue;

    map< StreamId_T, sp<StereoSelector> > streamToSelector;
    typedef pair< StreamId_T, sp<StereoSelector> > PairStreamToSelector;

    int                                 failedCounter               = 0;
    MINT32                              mTimestamp                  = -1;

    DurationProfile                     mSyncThreadProfile;
    DurationProfile                     mEnqueThreadProfile;
};

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
        MY_LOGW_NO_USERNAME("pMetadata == NULL");
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
sp<StereoBufferSynchronizer>
StereoBufferSynchronizer::
createInstance(
    char const* szCallerName, MINT32 delayCount
)
{
    return new StereoBufferSynchronizerImp( szCallerName, delayCount);
}
/******************************************************************************
 *
 ******************************************************************************/
StereoBufferSynchronizerImp::
StereoBufferSynchronizerImp(char const* szCallerName, MINT32 delayCount)
    : mSyncThreadProfile("synchronizerProfile::sync", 6000000LL)
    , mEnqueThreadProfile("synchronizerProfile::enque", 6000000LL)
    , mZSLDelayCount(delayCount)
{
    strncpy(mUserName, szCallerName, 64);
    MY_LOGD("StereoBufferSynchronizerImp ctor");

    previewPendingResizeRaw.clear();
    previewPendingFullRaw.clear();
    previewPendingResizeRaw_main2.clear();
    previewPendingFullRaw_main2.clear();

    previewAvailableResizeRaw.clear();
    previewAvailableFullRaw.clear();
    previewAvailableResizeRaw_main2.clear();
    previewAvailableFullRaw_main2.clear();

    streamToPreviewPendingQue.clear();
    streamToPreviewAvailableQue.clear();
    streamToSelector.clear();

    // get log level
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.STEREO.log", cLogLevel, "0");
    mLogLevel = atoi(cLogLevel);
    if ( mLogLevel == 0 ) {
        ::property_get("debug.STEREO.log.stereosync", cLogLevel, "0");
        mLogLevel = atoi(cLogLevel);
    }

    // get debug mode
    char cDebugMode[PROPERTY_VALUE_MAX];
    ::property_get("debug.STEREO.stereosync.dbg", cDebugMode, "0");

    switch(atoi(cDebugMode)){
        case 1:
            setDebugMode(DEBUG_MODE::SKIP_TIMESTAMP_CHECK);
            break;
        case 2:
            setDebugMode(DEBUG_MODE::SKIP_METADATA_CHECK);
            break;
        default:
            break;
    }

    // get debug ZSL count
    char cZSLCount[PROPERTY_VALUE_MAX];
    ::property_get("debug.stereosync.zsl", cZSLCount, "0");
    if(atoi(cZSLCount) != 0){
        mZSLDelayCount = atoi(cZSLCount);
    }


    MY_LOGD("PAIR_THRESHOLD_MS:%d SYNC_THRESHOLD_MS:%d PEN_QUE_KEEP_COUNT:%d AVA_QUE_KEEP_COUNT:%d LogLevel:%d debugMode:%d ZSLDelayCount:%d",
        PAIR_THRESHOLD_MS,
        SYNC_THRESHOLD_MS,
        PEN_QUE_KEEP_COUNT,
        AVA_QUE_KEEP_COUNT,
        mLogLevel,
        atoi(cDebugMode),
        mZSLDelayCount
    );

    mSyncThreadProfile.reset(mLogLevel);
    mEnqueThreadProfile.reset(mLogLevel);
}

/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
onLastStrongRef(const void* /*id*/)
{
    flush();
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
addStream(StreamId_T  streamId, sp<StereoSelector>  pSelector)
{
    FUNC_START;
    Mutex::Autolock _l(mLock);

    list<BUFFER_SET>* thePreviewPendingQue = NULL;
    list<BUFFER_SET>* thePreviewAvailableQue = NULL;
    list<BUFFER_SET>* thePreviewUsingQue = NULL;

    list<BUFFER_SET>* theCaptureAvailableQue = NULL;
    list<BUFFER_SET>* theCaptureUsingQue = NULL;

    switch(streamId){
        case eSTREAMID_IMAGE_PIPE_RAW_RESIZER:{
            thePreviewPendingQue = &previewPendingResizeRaw;
            thePreviewAvailableQue = &previewAvailableResizeRaw;
            thePreviewUsingQue = &previewUsingResizeRaw;

            theCaptureAvailableQue = &captureAvailableResizeRaw;
            theCaptureUsingQue = &captureUsingResizeRaw;
            break;
        }
        case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:{
            thePreviewPendingQue = &previewPendingFullRaw;
            thePreviewAvailableQue = &previewAvailableFullRaw;
            thePreviewUsingQue = &previewUsingFullRaw;

            theCaptureAvailableQue = &captureAvailableFullRaw;
            theCaptureUsingQue = &captureUsingFullRaw;
            break;
        }
        case eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01:{
            thePreviewPendingQue = &previewPendingResizeRaw_main2;
            thePreviewAvailableQue = &previewAvailableResizeRaw_main2;
            thePreviewUsingQue = &previewUsingResizeRaw_main2;

            theCaptureAvailableQue = &captureAvailableResizeRaw_main2;
            theCaptureUsingQue = &captureUsingResizeRaw_main2;
            break;
        }
        case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01:{
            thePreviewPendingQue = &previewPendingFullRaw_main2;
            thePreviewAvailableQue = &previewAvailableFullRaw_main2;
            thePreviewUsingQue = &previewUsingFullRaw_main2;

            theCaptureAvailableQue = &captureAvailableFullRaw_main2;
            theCaptureUsingQue = &captureUsingFullRaw_main2;
            break;
        }
        default:{
            MY_LOGE("Unknown stream %#" PRIxPTR " . Should not have happended!", streamId);
            return UNKNOWN_ERROR;
        }
    }

    if(thePreviewPendingQue == NULL){
        MY_LOGE("thePreviewPendingQue == NULL for stream %#" PRIxPTR " . Should not have happended!", streamId);
        return UNKNOWN_ERROR;
    }

    if(thePreviewAvailableQue == NULL){
        MY_LOGE("thePreviewAvailableQue == NULL for stream %#" PRIxPTR " . Should not have happended!", streamId);
        return UNKNOWN_ERROR;
    }

    if(thePreviewUsingQue == NULL){
        MY_LOGE("thePreviewUsingQue == NULL for stream %#" PRIxPTR " . Should not have happended!", streamId);
        return UNKNOWN_ERROR;
    }

    if(theCaptureAvailableQue == NULL){
        MY_LOGE("theCaptureAvailableQue == NULL for stream %#" PRIxPTR " . Should not have happended!", streamId);
        return UNKNOWN_ERROR;
    }

    if(theCaptureUsingQue == NULL){
        MY_LOGE("theCaptureUsingQue == NULL for stream %#" PRIxPTR " . Should not have happended!", streamId);
        return UNKNOWN_ERROR;
    }

    if(pSelector == NULL){
        MY_LOGE("pSelector == NULL for stream %#" PRIxPTR " . Should not have happended!", streamId);
        return UNKNOWN_ERROR;
    }

    streamToPreviewPendingQue.insert(PairStreamToBufferQue(streamId, thePreviewPendingQue));
    streamToPreviewAvailableQue.insert(PairStreamToBufferQue(streamId, thePreviewAvailableQue));
    streamToPreviewUsingQue.insert(PairStreamToBufferQue(streamId, thePreviewUsingQue));

    streamToCaptureAvailableQue.insert(PairStreamToBufferQue(streamId, theCaptureAvailableQue));
    streamToCaptureUsingQue.insert(PairStreamToBufferQue(streamId, theCaptureUsingQue));

    streamToSelector.insert(PairStreamToSelector(streamId, pSelector));

    pSelector->setSynchronizer(this, streamId);

    MY_LOGD("new stream %#" PRIxPTR " added", streamId);

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
removeStream(StreamId_T  streamId)
{
    MY_LOGE("Hey, this function is not implemented yet!");
    return UNKNOWN_ERROR;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
dequeBuffer(
                MINT32&                             rRequestNo,
                StreamId_T                          streamId,
                Vector<ISelector::MetaItemSet>&     rMeta,
                android::sp<IImageBufferHeap>&      rpHeap
            )
{
    list<BUFFER_SET>* theAvailableQue = NULL;

    MY_LOGD1("dequeBuffer %#" PRIxPTR " + ", streamId);

    // find the preview available queue
    if(streamToPreviewAvailableQue.count(streamId)){
        theAvailableQue = streamToPreviewAvailableQue[streamId];
    }else{
        MY_LOGE("undefined streamId, should not have happended!");
        return NAME_NOT_FOUND;
    }

    // deque from preview available queue
    BUFFER_SET dequedSet;
    {
        Mutex::Autolock _l(mPreviewAvailableQueLock);
        if(theAvailableQue->empty()){
            MY_LOGE("try to deque but the size is 0, should not have happended! Did you call synchronizer->lock() before doing this?");
            return UNKNOWN_ERROR;
        }
        dequedSet = theAvailableQue->front();
        theAvailableQue->pop_front();
        rRequestNo = dequedSet.reqNo;
        rMeta = dequedSet.metadata;
        rpHeap = dequedSet.heap;
        dequedSet.isReturned = MFALSE;
    }

    // push to preview using queue
    {
        Mutex::Autolock _l(mPreviewUsingQueLock);
        list<BUFFER_SET>* thePreviewUsingQue = streamToPreviewUsingQue[streamId];
        thePreviewUsingQue->push_back(dequedSet);

        showPreviewUsingSizes();
    }

    MY_LOGD1("dequeBuffer %#" PRIxPTR " - , rpHeap=%p, reqNo:%d", streamId, rpHeap.get(), rRequestNo);
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
dequeBufferCapture(
                MINT32&                         rRequestNo,
                StreamId_T                      streamId,
                Vector<ISelector::MetaItemSet>& rMeta,
                android::sp<IImageBufferHeap>&  rpHeap
               )
{
    list<BUFFER_SET>* theCaptureAvailableQue = NULL;

    MY_LOGD1("dequeBufferCapture %#" PRIxPTR " + ", streamId);

    // find the capture available queue
    if(streamToCaptureAvailableQue.count(streamId)){
        theCaptureAvailableQue = streamToCaptureAvailableQue[streamId];
    }else{
        MY_LOGE("undefined streamId, should not have happended!");
        return NAME_NOT_FOUND;
    }

    // deque from capture available queue
    BUFFER_SET dequedSet;
    {
        Mutex::Autolock _l(mCaptureAvailableQueLock);
        if(theCaptureAvailableQue->empty()){
            MY_LOGE("try to deque but the size is 0, should not have happended! Did you call synchronizer->waitAndLockCapture() before doing this?");
            return UNKNOWN_ERROR;
        }
        dequedSet = theCaptureAvailableQue->front();
        theCaptureAvailableQue->pop_front();
        rRequestNo = dequedSet.reqNo;
        rMeta = dequedSet.metadata;
        rpHeap = dequedSet.heap;
        dequedSet.isReturned = MFALSE;
    }

    // push to capture using queue
    {
        Mutex::Autolock _l(mCaptureUsingQueLock);
        list<BUFFER_SET>* theCaptureUsingQue = streamToCaptureUsingQue[streamId];
        theCaptureUsingQue->push_back(dequedSet);

        showCaptureUsingSizes();
    }

    MY_LOGD1("dequeBufferCapture %#" PRIxPTR " - , rpHeap=%p, reqNo:%d", streamId, rpHeap.get(), rRequestNo);
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
checkZSLAvailableQueueSize(MINT32 keepSize){
    MY_LOGE("not implemented");
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
keepZSLOldestBuffer(){
    MY_LOGE("not implemented");
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
returnBuffer(
                android::sp<IImageBufferHeap>&  rpHeap,
                StreamId_T                      streamId
            )
{
    MY_LOGD1("returnBuffer %#" PRIxPTR  ", buf:%p + ", streamId, rpHeap.get());
    BUFFER_SET dequedSet;
    // check what kind of this buffer is
    {
        // if this is a preview using buffer, move it to capture available queue
        MBOOL moveToCaptureQue = MFALSE;
        {
            Mutex::Autolock _l(mPreviewUsingQueLock);
            list<BUFFER_SET>* thePreviewUsingQue = streamToPreviewUsingQue[streamId];
            if(isInQue(thePreviewUsingQue, rpHeap)){
                dequedSet = removeFromQueue(thePreviewUsingQue, rpHeap);
                // check this buffer_set's af state, if not stable, dont push to capture queue
                if(dequedSet.isAFStable){
                    moveToCaptureQue = MTRUE;
                }else{
                    // preview used but AF not stable, return to selector immediately
                    if(!dequedSet.isAFStable){
                        MY_LOGD1("buffer not AF stable, dont push to capture queue");
                        returnBufferToSelector(streamId, dequedSet.heap);
                        mCondPreviewUsingReturned.signal();
                        return OK;
                    }
                }
                mCondPreviewUsingReturned.signal();
            }
        }

        // preview used and AF stable, push to capture queue
        if(moveToCaptureQue){
            Mutex::Autolock _l(mCaptureAvailableQueLock);

            if(mIsCaptureLocked){
                returnBufferToSelector(streamId, dequedSet.heap);
            }else{
                list<BUFFER_SET>* theCaptureAvailableQue = streamToCaptureAvailableQue[streamId];
                theCaptureAvailableQue->push_back(dequedSet);

                //check capture available queue size
                if(theCaptureAvailableQue->size() > mZSLDelayCount){
                    MY_LOGD2("theCaptureAvailableQue->size():%d > mZSLDelayCount:%d. return oldest buffer to selector",
                        theCaptureAvailableQue->size(),
                        mZSLDelayCount
                    );
                    returnOldestBufferToSelector(streamId, streamToCaptureAvailableQue);
                }

                showCaptureAvailableSizes();

                mCondPushCaptureAvailable.signal();
            }
            return OK;
        }
    }
    {
        // if this is a capture using buffer, return it to selector immediately
        Mutex::Autolock _l(mCaptureUsingQueLock);
        list<BUFFER_SET>* theCaptureUsingQue = streamToCaptureUsingQue[streamId];
        if(isInQue(theCaptureUsingQue, rpHeap)){
            dequedSet = removeFromQueue(theCaptureUsingQue, rpHeap);
            returnBufferToSelector(streamId, rpHeap);
            mCondCaptureUsingReturned.signal();
            return OK;
        }
    }

    MY_LOGE("cant find this buffer:%p in any of the queue, should not have happended!", rpHeap.get());
    return UNKNOWN_ERROR;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
enqueBuffer(
                MINT32                              rRequestNo,
                StreamId_T                          streamId,
                Vector<ISelector::MetaItemSet>      vMeta,
                android::sp<IImageBufferHeap>       pHeap
            )
{
    list<BUFFER_SET>* thePendingQue = NULL;

    MY_LOGD1("req:%d, heap:%p, meta:%d stream:%#" PRIxPTR " , is enqued to StereoSynchronizer ",
        rRequestNo,
        pHeap.get(),
        vMeta.size(),
        streamId
    );

    {
        Mutex::Autolock _l(mPreviewPendingQueLock);
        if(mbIsPreviewStopped){
            MY_LOGD("preview stopped, return buffer directly");
            returnBufferToSelector(streamId, pHeap);
            return OK;
        }
    }

    if(streamToPreviewPendingQue.count(streamId)){
        thePendingQue = streamToPreviewPendingQue[streamId];
    }else{
        MY_LOGE("undefined streamId, should not have happended!");
        return NAME_NOT_FOUND;
    }

    {
        // Push new buffer into pendingQueue
        Mutex::Autolock _l(mPreviewPendingQueLock);

        mEnqueThreadProfile.pulse_up();

        BUFFER_SET newBuffer;
        newBuffer.reqNo = rRequestNo;
        newBuffer.heap = pHeap;
        newBuffer.metadata = vMeta;
        newBuffer.streamId = streamId;
        newBuffer.isReturned = MFALSE;
        thePendingQue->push_back(newBuffer);

        mCondPushPreviewPending.signal();

        mEnqueThreadProfile.pulse_down();
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isReadyToSyncCheck()
{
    MBOOL bIsReady = MTRUE;

    // make sure there is no too much buffer pending here
    checkAllPendingQueueSize();

    showPreviewPendingSizes();

    // check if there is anyone empty
    for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToPreviewPendingQue.begin(); it!=streamToPreviewPendingQue.end(); ++it){
        if(it->second->empty()){
         MY_LOGD1("streamId(%d) pending Queue is empty, buffer not ready", it->first);
         return MFALSE;
        }
    }

    return bIsReady;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
checkPendingQueueSize(StreamId_T streamId)
{
    FUNC_START;

    list<BUFFER_SET>* thePendingQue = NULL;

    // get front buffer from pending queue
    if(streamToPreviewPendingQue.count(streamId)){
        thePendingQue = streamToPreviewPendingQue[streamId];
    }else{
        FUNC_END;
        return;
    }

    while(thePendingQue->size() > PEN_QUE_KEEP_COUNT){
        MY_LOGW("streamId%#" PRIxPTR " thePendingQue.size=%d > PEN_QUE_KEEP_COUNT=%d return 1 buffer to selector",
            streamId,
            thePendingQue->size(),
            PEN_QUE_KEEP_COUNT
        );
        returnOldestBufferToSelector(streamId, streamToPreviewPendingQue);
    }

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
checkAllPendingQueueSize(){
    for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToPreviewPendingQue.begin(); it!=streamToPreviewPendingQue.end(); ++it){
        checkPendingQueueSize(it->first);
    }
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
checkAvailableQueueSize(StreamId_T streamId)
{
    FUNC_START;

    list<BUFFER_SET>* theAvailableQue = NULL;

    // get front buffer from pending queue
    if(streamToPreviewAvailableQue.count(streamId)){
        theAvailableQue = streamToPreviewAvailableQue[streamId];
    }else{
        FUNC_END;
        return;
    }

    while(theAvailableQue->size() > AVA_QUE_KEEP_COUNT){
        MY_LOGW("theAvailableQue.size=%d > AVA_QUE_KEEP_COUNT=%d return 1 buffer to selector",
            theAvailableQue->size(),
            AVA_QUE_KEEP_COUNT
        );
        returnOldestBufferToSelector(streamId, streamToPreviewAvailableQue);
    }

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
checkAllAvailableQueueSize(){
    for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToPreviewAvailableQue.begin(); it!=streamToPreviewAvailableQue.end(); ++it){
        checkAvailableQueueSize(it->first);
    }
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isPreviewUsingQueEmpty(){
    for(auto e : streamToPreviewAvailableQue ){
        if(!e.second->empty()){
            return MFALSE;
        }
    }
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isCaptureUsingQueEmpty(){
    for(auto e : streamToCaptureUsingQue ){
        if(!e.second->empty()){
            return MFALSE;
        }
    }
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
returnOldestBufferToSelector(StreamId_T streamId, map< StreamId_T, list<BUFFER_SET>* >& theMap){
    FUNC_START;

    list<BUFFER_SET>* theQue = NULL;

    // get front buffer from pending queue
    if(theMap.count(streamId)){
        theQue = theMap[streamId];
    }else{
        return;
    }

    BUFFER_SET dequedSet = theQue->front();
    theQue->pop_front();

    // return to selector
    returnBufferToSelector(streamId, dequedSet.heap);

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
returnBufferToSelector(BUFFER_SET* bufInfo){
    if(bufInfo == NULL){
        return;
    }

    FUNC_START;

    // return to selector
    sp<StereoSelector> theSelector = streamToSelector[bufInfo->streamId];

#warning "FIXME. [N-Migration]should be un-marked"
    //theSelector->returnBuffer(bufInfo->heap);

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
returnBufferToSelector(StreamId_T streamId, android::sp<IImageBufferHeap>& theHeap){
    sp<StereoSelector> theSelector = streamToSelector[streamId];

#warning "FIXME. [N-Migration]should be un-marked"
    //theSelector->returnBuffer(theHeap);
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
returnBufferToPendingQue(BUFFER_SET* bufInfo){
    if(bufInfo == NULL){
        return;
    }

    FUNC_START;

    list<BUFFER_SET>* thePendingQue = NULL;

    // get front buffer from pending queue
    if(streamToPreviewPendingQue.count(bufInfo->streamId)){
        thePendingQue = streamToPreviewPendingQue[bufInfo->streamId];
    }else{
        return;
    }

    {
        Mutex::Autolock _l(mPreviewPendingQueLock);
        thePendingQue->push_front((*bufInfo));
    }

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
clearQueAndReturnBufferToSelector(){
    FUNC_START;

    //clear preview pendingQue
    {
        Mutex::Autolock _l(mPreviewPendingQueLock);
        for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToPreviewPendingQue.begin(); it!=streamToPreviewPendingQue.end(); ++it){
            StreamId_T theStreamId = it->first;
            list<BUFFER_SET>* theQue = it->second;
            sp<StereoSelector> theSelector = streamToSelector[theStreamId];

            while(!theQue->empty()){
                BUFFER_SET dequedSet = theQue->front();
                theQue->pop_front();

                // return to selector
#warning "FIXME. [N-Migration]should be un-marked"
                //theSelector->returnBuffer(dequedSet.heap);
            }
        }
    }

    //clear preview availableQue
    {
        Mutex::Autolock _l(mPreviewAvailableQueLock);
        for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToPreviewAvailableQue.begin(); it!=streamToPreviewAvailableQue.end(); ++it){
            StreamId_T theStreamId = it->first;
            list<BUFFER_SET>* theQue = it->second;
            sp<StereoSelector> theSelector = streamToSelector[theStreamId];

            while(!theQue->empty()){
                BUFFER_SET dequedSet = theQue->front();
                theQue->pop_front();

                // return to selector
#warning "FIXME. [N-Migration]should be un-marked"
                //theSelector->returnBuffer(dequedSet.heap);
            }
        }
    }

    //clear capture availableQue
    {
        Mutex::Autolock _l(mCaptureAvailableQueLock);
        for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToCaptureAvailableQue.begin(); it!=streamToCaptureAvailableQue.end(); ++it){
            StreamId_T theStreamId = it->first;
            list<BUFFER_SET>* theQue = it->second;
            sp<StereoSelector> theSelector = streamToSelector[theStreamId];

            while(!theQue->empty()){
                BUFFER_SET dequedSet = theQue->front();
                theQue->pop_front();

                // return to selector
#warning "FIXME. [N-Migration]should be un-marked"
                //theSelector->returnBuffer(dequedSet.heap);
            }
        }
    }

    //wait while preview/capture usingqueue is empty
    {
        Mutex::Autolock _l(mPreviewUsingQueLock);
        while(!isPreviewUsingQueEmpty()){
            MY_LOGD("preview using not empty, wait +");
            mCondPreviewUsingReturned.wait(mPreviewUsingQueLock);
            MY_LOGD("preview using not empty, wait -");
        }
        MY_LOGD("preview using is empty now");
    }

    {
        Mutex::Autolock _l(mCaptureUsingQueLock);
        while(!isCaptureUsingQueEmpty()){
            MY_LOGD("capture using not empty, wait +");
            mCondCaptureUsingReturned.wait(mCaptureUsingQueLock);
            MY_LOGD("capture using not empty, wait -");
        }
        MY_LOGD("capture using is empty now");
    }

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
enqueBufferToAvailableQue(BUFFER_SET* bufInfo, MINT32 assignedReqNo){
    if(bufInfo == NULL){
        return;
    }
    FUNC_START;

    bufInfo->reqNo = assignedReqNo;

    list<BUFFER_SET>* theAvailableQue = NULL;
    if(streamToPreviewPendingQue.count(bufInfo->streamId)){
        theAvailableQue = streamToPreviewAvailableQue[bufInfo->streamId];
        theAvailableQue->push_back((*bufInfo));
    }else{
        MY_LOGE("pendingQue exist but availableQue not exist? should not have happended");
    }

    FUNC_END;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
showPreviewPendingSizes()
{
    MY_LOGD2("\n(rez/ful/rez_main2/ful_main2)\npendingQueue:  (%d/%d/%d/%d)",
        (streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER)         > 0 ? previewPendingResizeRaw.size()              : -1),
        (streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)          > 0 ? previewPendingFullRaw.size()                : -1),
        (streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)      > 0 ? previewPendingResizeRaw_main2.size()        : -1),
        (streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)       > 0 ? previewPendingFullRaw_main2.size()          : -1)
    );
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
showPreviewAvailableSizes()
{
    MY_LOGD2("\n(rez/ful/rez_main2/ful_main2)\npreviewAvailable:  (%d/%d/%d/%d)",
        (streamToPreviewAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER)       > 0 ? previewAvailableResizeRaw.size()            : -1),
        (streamToPreviewAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)        > 0 ? previewAvailableFullRaw.size()              : -1),
        (streamToPreviewAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)    > 0 ? previewAvailableResizeRaw_main2.size()      : -1),
        (streamToPreviewAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)     > 0 ? previewAvailableFullRaw_main2.size()        : -1)
    );
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
showPreviewUsingSizes()
{
    MY_LOGD2("\n(rez/ful/rez_main2/ful_main2)\npreviewUsing:  (%d/%d/%d/%d)",
        (streamToPreviewUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER)       > 0 ? previewUsingResizeRaw.size()            : -1),
        (streamToPreviewUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)        > 0 ? previewUsingFullRaw.size()              : -1),
        (streamToPreviewUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)    > 0 ? previewUsingResizeRaw_main2.size()      : -1),
        (streamToPreviewUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)     > 0 ? previewUsingFullRaw_main2.size()        : -1)
    );
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
showCaptureAvailableSizes()
{
    MY_LOGD2("\n(rez/ful/rez_main2/ful_main2)\ncaptureAvailable:  (%d/%d/%d/%d)",
        (streamToCaptureAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER)       > 0 ? captureAvailableResizeRaw.size()            : -1),
        (streamToCaptureAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)        > 0 ? captureAvailableFullRaw.size()              : -1),
        (streamToCaptureAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)    > 0 ? captureAvailableResizeRaw_main2.size()      : -1),
        (streamToCaptureAvailableQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)     > 0 ? captureAvailableFullRaw_main2.size()        : -1)
    );
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
showCaptureUsingSizes()
{
    MY_LOGD2("\n(rez/ful/rez_main2/ful_main2)\ncaptureUsing:  (%d/%d/%d/%d)",
        (streamToCaptureUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER)       > 0 ? captureUsingResizeRaw.size()            : -1),
        (streamToCaptureUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)        > 0 ? captureUsingFullRaw.size()              : -1),
        (streamToCaptureUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)    > 0 ? captureUsingResizeRaw_main2.size()      : -1),
        (streamToCaptureUsingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)     > 0 ? captureUsingFullRaw_main2.size()        : -1)
    );
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
syncRoutine(
    BUFFER_SET* bufInfoResized,
    BUFFER_SET* bufInfoResized_main2,
    BUFFER_SET* bufInfoFull,
    BUFFER_SET* bufInfoFull_main2
){
    FUNC_START;

    SYNC_RESULT sync_result = SYNC_RESULT_PAIR_NOT_SYNC;
    MBOOL AFStable = MFALSE;

    // chose a suitable to-be-compared set
    if(bufInfoResized && bufInfoResized_main2){
        sync_result = isTimeSync(bufInfoResized, bufInfoResized_main2);
        AFStable = isAFStable(bufInfoResized, bufInfoResized_main2);
    }
    else
    if(bufInfoResized && bufInfoFull_main2){
        sync_result = isTimeSync(bufInfoResized, bufInfoFull_main2);
        AFStable = isAFStable(bufInfoResized, bufInfoFull_main2);
    }else
    if(bufInfoFull && bufInfoResized_main2){
        sync_result = isTimeSync(bufInfoFull, bufInfoResized_main2);
        AFStable = isAFStable(bufInfoFull, bufInfoResized_main2);
    }else
    if(bufInfoFull && bufInfoFull_main2){
        sync_result = isTimeSync(bufInfoFull, bufInfoFull_main2);
        AFStable = isAFStable(bufInfoFull, bufInfoFull_main2);
    }
    else{
        MY_LOGE("Can not find a suitable to-be-compared set! ");
        sync_result = SYNC_RESULT_PAIR_NOT_SYNC;
    }

    // mark all the BUFFER_SET with AF state
    markAFState(bufInfoResized,         AFStable);
    markAFState(bufInfoResized_main2,   AFStable);
    markAFState(bufInfoFull,            AFStable);
    markAFState(bufInfoFull_main2,      AFStable);

    switch(sync_result){
        case SYNC_RESULT_MAIN1_TOO_OLD:
            // Main1 too old
            // return main1 buffer to selector
            returnBufferToSelector(bufInfoResized);
            returnBufferToSelector(bufInfoFull);
            // return main2 buffer back to pending queue
            returnBufferToPendingQue(bufInfoResized_main2);
            returnBufferToPendingQue(bufInfoFull_main2);
            break;
        case SYNC_RESULT_MAIN2_TOO_OLD:
            // Main2 too old
            // return main2 buffer to selector
            returnBufferToSelector(bufInfoResized_main2);
            returnBufferToSelector(bufInfoFull_main2);
            // return main1 buffer back to pending queue
            returnBufferToPendingQue(bufInfoResized);
            returnBufferToPendingQue(bufInfoFull);
            break;
        case SYNC_RESULT_PAIR_NOT_SYNC:
            // This buffet pair is not sync.
            // return both to selector
            returnBufferToSelector(bufInfoResized);
            returnBufferToSelector(bufInfoFull);
            returnBufferToSelector(bufInfoResized_main2);
            returnBufferToSelector(bufInfoFull_main2);
            break;
        default:
            // SYNC_RESULT_OK
            // This buffet pair is synchronized.
            {
                Mutex::Autolock _l(mPreviewAvailableQueLock);

                if(mIsAvailableQueStopped){
                    MY_LOGD1("mIsAvailableQueStopped, return buffers instead of push to availableQue");
                    returnBufferToSelector(bufInfoResized);
                    returnBufferToSelector(bufInfoFull);
                    returnBufferToSelector(bufInfoResized_main2);
                    returnBufferToSelector(bufInfoFull_main2);
                    FUNC_END;
                    return;
                }

                while(mIsLocked){
                    mCondLockPreviewAvailable.wait(mPreviewAvailableQueLock);
                }

                MINT32 assignedReqNo = bufInfoResized->reqNo;
                enqueBufferToAvailableQue(bufInfoResized, assignedReqNo);
                enqueBufferToAvailableQue(bufInfoFull, assignedReqNo);
                enqueBufferToAvailableQue(bufInfoResized_main2, assignedReqNo);
                enqueBufferToAvailableQue(bufInfoFull_main2, assignedReqNo);

                showPreviewAvailableSizes();

                checkAllAvailableQueueSize();

                mCondPushPreviewAvailable.signal();
            }
            break;
    }

    FUNC_END;
}
/*******************************************************************************
*
********************************************************************************/
SYNC_RESULT
StereoBufferSynchronizerImp::
isTimeSync(
    BUFFER_SET* bufInfoMain1,
    BUFFER_SET* bufInfoMain2
){
    SYNC_RESULT ret = SYNC_RESULT_PAIR_OK;

    MINT64 timestamp_main1 = -1;
    MINT64 timestamp_main2 = -1;

    // fetch the metadata with timestamp
    for ( size_t i = 0; i < bufInfoMain1->metadata.size(); ++i) {
        // MY_LOGD1("tryGetMetadata bufInfoMain1->metadata %d", i);
        if ( tryGetMetadata<MINT64>(const_cast<IMetadata*>(&bufInfoMain1->metadata[i].meta), MTK_SENSOR_TIMESTAMP, timestamp_main1) ) {
            // MY_LOGD1("tryGetMetadata bufInfoMain1->metadata %d, suceess!", i);
            break;
        }
    }
    for ( size_t i = 0; i < bufInfoMain2->metadata.size(); ++i) {
        // MY_LOGD1("tryGetMetadata bufInfoMain2->metadata %d", i);
        if ( tryGetMetadata<MINT64>(const_cast<IMetadata*>(&bufInfoMain2->metadata[i].meta), MTK_SENSOR_TIMESTAMP, timestamp_main2) ) {
            // MY_LOGD1("tryGetMetadata bufInfoMain2->metadata %d, suceess!", i);
            break;
        }
    }

if(mDebugMode == DEBUG_MODE::SKIP_METADATA_CHECK){
    MY_LOGD("Sync check always return OK in DEBUG_MODE_SKIP_METADATA_CHECK");
    return SYNC_RESULT_PAIR_OK;
}

    int timestamp_main1_ms;
    int timestamp_main2_ms;
    int timestamp_diff;

    if(timestamp_main1 != -1 && timestamp_main2 != -1)
    {
        timestamp_main1_ms = timestamp_main1/1000000;
        timestamp_main2_ms = timestamp_main2/1000000;
        timestamp_diff     = timestamp_main1_ms - timestamp_main2_ms;
    }else{
        MY_LOGE("can not get timestamp meta");
        MY_LOGD_IF(timestamp_main1 == -1, "timestamp_main1 is -1");
        MY_LOGD_IF(timestamp_main2 == -1, "timestamp_main2 is -1");
        MY_LOGD1("SYNC_RESULT_PAIR_NOT_SYNC");
        return SYNC_RESULT_PAIR_NOT_SYNC;
    }

    char result_msg[32];
    if(abs(timestamp_diff) < PAIR_THRESHOLD_MS){
        if(abs(timestamp_diff) <= SYNC_THRESHOLD_MS){
            ret = SYNC_RESULT_PAIR_OK;
            snprintf(result_msg,32,"SYNC_RESULT_PAIR_OK");
        }else{
            ret = SYNC_RESULT_PAIR_NOT_SYNC;
            snprintf(result_msg,32,"SYNC_RESULT_PAIR_NOT_SYNC");
        }
    }else{
        if(timestamp_main1 > timestamp_main2){
            ret = SYNC_RESULT_MAIN2_TOO_OLD;
            snprintf(result_msg,32,"SYNC_RESULT_MAIN2_TOO_OLD");
        }else{
            ret = SYNC_RESULT_MAIN1_TOO_OLD;
            snprintf(result_msg,32,"SYNC_RESULT_MAIN1_TOO_OLD");
        }
    }

    MINT32 currentTimestamp = (NSCam::Utils::TimeTool::getReadableTime()) % 100000;
    MY_LOGD1("TS_diff:(main1/main2/diff)(%09d/%09d/%09d)(ms), %25s, machineTimeDelta:%05d(ssmmm)",
        timestamp_main1_ms,
        timestamp_main2_ms,
        timestamp_diff,
        result_msg,
        currentTimestamp - mTimestamp
    );

    mTimestamp = currentTimestamp;

    if(ret != SYNC_RESULT_PAIR_OK){
        failedCounter ++;
        if(failedCounter > SYNC_FAIL_WARNING_COUNT){
            MY_LOGW("consecutive failed count:%d > %d! Please check frame sync modules", failedCounter, SYNC_FAIL_WARNING_COUNT);
        }
    }else{
        failedCounter = 0;
    }

if(mDebugMode == DEBUG_MODE::SKIP_TIMESTAMP_CHECK){
    MY_LOGD("Sync check always return OK in debugMode=DEBUG_MODE_SKIP_TIMESTAMP_CHECK");
    ret = SYNC_RESULT_PAIR_OK;
}
    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isReadyToLock()
{
    showPreviewAvailableSizes();

    for(map<StreamId_T, list<BUFFER_SET>*>::iterator it=streamToPreviewAvailableQue.begin(); it!=streamToPreviewAvailableQue.end(); ++it){
        if( it->second->empty()){
            return MFALSE;
        }
    }
    MY_LOGD1("availablePools are ready");
    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isZSLBufferReady()
{
    MY_LOGE("not implemented");
    return UNKNOWN_ERROR;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
lock(MBOOL stopAvailableQue){
    FUNC_START;
    Mutex::Autolock _l(mPreviewAvailableQueLock);

    if(mIsLocked){
        MY_LOGE("try to lock synchronizer when it is already locked!");
        return UNKNOWN_ERROR;
    }

    if(stopAvailableQue)
    {
        mCondPushPreviewAvailable.broadcast();
    }

    MY_LOGD1("stopAvailableQue(%d)", stopAvailableQue);
    while(MTRUE){
        // break the waiting loop or keep waiting fot availableQue
        if(isReadyToLock()){
            mIsLocked = MTRUE;
            mIsAvailableQueStopped = stopAvailableQue;
            MY_LOGD1("mIsLocked:%d, mIsAvailableQueStopped:%d", mIsLocked, mIsAvailableQueStopped);
            break;
        }else{
            MY_LOGD1("not ready to lock wait +");
            mCondPushPreviewAvailable.wait(mPreviewAvailableQueLock);
            MY_LOGD1("not ready to lock wait -");
        }
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
unlock(){
    FUNC_START;
    Mutex::Autolock _l(mPreviewAvailableQueLock);

    if(!mIsLocked){
        MY_LOGE("try to unlock synchronizer when it is not locked!");
        return UNKNOWN_ERROR;
    }else{
        mIsLocked = MFALSE;
        mIsAvailableQueStopped = MFALSE;
        mCondLockPreviewAvailable.signal();
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isCaptureBufferReady(){
    list<BUFFER_SET> theLatestBuffersFromEachQueue;

    for(auto e : streamToCaptureAvailableQue){
        if(e.second->size() < mZSLDelayCount){
            MY_LOGD1("streamId(%d) capture available queue is %d < ZSLDelayCount:%d. Buffer not ready",
                e.first,
                e.second->size(),
                mZSLDelayCount
            );
            return MFALSE;
        }
        theLatestBuffersFromEachQueue.push_back(e.second->front());
    }

    // Check whether the to-be-used buffer are in the same sync pair
    MY_LOGD1("reqNo check start");
    MINT32 reqNo = theLatestBuffersFromEachQueue.front().reqNo;
    for(auto e : theLatestBuffersFromEachQueue){
        MY_LOGD1("reqNo:%d", e.reqNo);
        if(e.reqNo != reqNo){
            MY_LOGE("reqNo check failed!");
            return MFALSE;
        }
    }

    return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
waitAndLockCapture(){
    FUNC_START;

    Mutex::Autolock _l(mCaptureAvailableQueLock);

    if(mIsCaptureLocked){
        MY_LOGE("try to waitAndLockCapture synchronizer when it is already capture locked!");
        return UNKNOWN_ERROR;
    }

    while(MTRUE){
        if(isCaptureBufferReady()){
            mIsCaptureLocked = MTRUE;
            MY_LOGD1("mIsCaptureLocked:%d", mIsCaptureLocked);
            break;
        }else{
            MY_LOGD1("capture buffer not ready wait +");
            mCondPushCaptureAvailable.wait(mCaptureAvailableQueLock);
            MY_LOGD1("capture buffer not ready wait -");
        }
    }

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
unlockCapture(){
    FUNC_START;
    Mutex::Autolock _l(mCaptureAvailableQueLock);

    if(!mIsCaptureLocked){
        MY_LOGE("try to unlockCapture synchronizer when it is not capture locked!");
        return UNKNOWN_ERROR;
    }else{
        mIsCaptureLocked = MFALSE;
    }

    FUNC_END;
    return OK;

}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
setDebugMode(MINT32 debug)
{
    mDebugMode = debug;
    MY_LOGD("mDebugMode=%d", mDebugMode);
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
setZSLDelayCount(MINT32 delayCount)
{
    mZSLDelayCount = delayCount;
    MY_LOGD("mZSLDelayCount=%d", mZSLDelayCount);
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
flush(){
    FUNC_START;

    {
        Mutex::Autolock _l(mPreviewPendingQueLock);
        MY_LOGD("stop pushing to previewPendingQue");
        mbIsPreviewStopped = MTRUE;
    }

    MY_LOGD("return all pending/available buffers +");

    clearQueAndReturnBufferToSelector();

    MY_LOGD("return all pending/available buffers -");

    Thread::requestExit();
    mbExitThread = MTRUE;

    showPreviewPendingSizes();
    showPreviewAvailableSizes();
    showPreviewUsingSizes();
    showCaptureAvailableSizes();
    showCaptureUsingSizes();

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
sp<StereoSelector>
StereoBufferSynchronizerImp::
querySelector(StreamId_T streamId){
    if(streamToSelector.count(streamId)){
        return streamToSelector[streamId];
    }else{
        MY_LOGE("cant find stereo selector for stream %#" PRIxPTR ".", streamId);
        return nullptr;
    }
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StereoBufferSynchronizerImp::
start(){
    MY_LOGD("synchronizer start to work");
    return run(THREAD_NAME);
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
StereoBufferSynchronizerImp::
readyToRun()
{
    // set name
    ::prctl(PR_SET_NAME, (unsigned long)THREAD_NAME, 0, 0, 0);

    // set normal
    struct sched_param sched_p;
    sched_p.sched_priority = 0;
    ::sched_setscheduler(0, THREAD_POLICY, &sched_p);
    ::setpriority(PRIO_PROCESS, 0, THREAD_PRIORITY);
    //
    ::sched_getparam(0, &sched_p);

    MY_LOGD(
        "tid(%d) policy(%d) priority(%d)"
        , ::gettid(), ::sched_getscheduler(0)
        , sched_p.sched_priority
    );

    //
    return OK;
}
/*******************************************************************************
*
********************************************************************************/
PAIR_RESULT
StereoBufferSynchronizerImp::
isPairSync(
    BUFFER_SET* bufInfo_resized,
    BUFFER_SET* bufInfo_full
){
    PAIR_RESULT ret = PAIR_RESULT_PAIR_OK;

    MINT64 timestamp_resized = -1;
    MINT64 timestamp_full = -1;

    // fetch the metadata with timestamp
    for ( size_t i = 0; i < bufInfo_resized->metadata.size(); ++i) {
        if ( tryGetMetadata<MINT64>(const_cast<IMetadata*>(&bufInfo_resized->metadata[i].meta), MTK_SENSOR_TIMESTAMP, timestamp_resized) ) {
            // MY_LOGD1("tryGetMetadata bufInfo_resized->metadata %d, suceess!", i);
            break;
        }
    }
    for ( size_t i = 0; i < bufInfo_full->metadata.size(); ++i) {
        if ( tryGetMetadata<MINT64>(const_cast<IMetadata*>(&bufInfo_full->metadata[i].meta), MTK_SENSOR_TIMESTAMP, timestamp_full) ) {
            // MY_LOGD1("tryGetMetadata bufInfo_full->metadata %d, suceess!", i);
            break;
        }
    }

if(mDebugMode == DEBUG_MODE::SKIP_METADATA_CHECK){
    MY_LOGD("Sync check always return OK in DEBUG_MODE_SKIP_METADATA_CHECK");
    return PAIR_RESULT_PAIR_OK;
}

    int timestamp_resized_ms;
    int timestamp_full_ms;
    int timestamp_diff;

    if(timestamp_resized != -1 && timestamp_full != -1)
    {
        timestamp_resized_ms = timestamp_resized/1000000;
        timestamp_full_ms = timestamp_full/1000000;
        timestamp_diff     = timestamp_resized_ms - timestamp_full_ms;
    }else{
        MY_LOGE("can not get timestamp meta");
        MY_LOGD_IF(timestamp_resized == -1, "timestamp_resized is -1");
        MY_LOGD_IF(timestamp_full == -1, "timestamp_full is -1");
        MY_LOGD1("PAIR_RESULT_PAIR_INVALID");
        return PAIR_RESULT_PAIR_INVALID;
    }

    char result_msg[32];
    if(timestamp_diff > 0){
        ret = PAIR_RESULT_FULL_TOO_OLD;
        snprintf(result_msg,32,"PAIR_RESULT_FULL_TOO_OLD");
    }else if(timestamp_diff < 0){
        ret = PAIR_RESULT_RESIZED_TOO_OLD;
        snprintf(result_msg,32,"PAIR_RESULT_RESIZED_TOO_OLD");
    }else{
        snprintf(result_msg,32,"PAIR_RESULT_PAIR_OK");
        ret = PAIR_RESULT_PAIR_OK;
    }

    MY_LOGD1("TS_diff:(resized/full/diff)(%09d/%09d/%09d)(ms), %25s",
        timestamp_resized_ms,
        timestamp_full_ms,
        timestamp_diff,
        result_msg
    );

    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
pairCheck(
    BUFFER_SET* bufInfoResized,
    BUFFER_SET* bufInfoFull
){
    PAIR_RESULT pair_result = isPairSync(bufInfoResized, bufInfoFull);
    MBOOL ret = MFALSE;

    switch(pair_result){
        case PAIR_RESULT_FULL_TOO_OLD:
            returnBufferToSelector(bufInfoFull);
            returnBufferToPendingQue(bufInfoResized);
            ret = MFALSE;
            break;
        case PAIR_RESULT_RESIZED_TOO_OLD:
            returnBufferToSelector(bufInfoResized);
            returnBufferToPendingQue(bufInfoFull);
            ret = MFALSE;
            break;
        case PAIR_RESULT_PAIR_INVALID:
            returnBufferToSelector(bufInfoFull);
            returnBufferToSelector(bufInfoResized);
            ret = MFALSE;
            break;
        default:
            ret = MTRUE;
            break;
    }

    if(!ret){
        bufInfoResized->isReturned = MTRUE;
        bufInfoFull->isReturned = MTRUE;
    }

    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isInQue(
    list<BUFFER_SET>* theQue, android::sp<IImageBufferHeap>& theHeap
){
    for(auto e : (*theQue)){
        if(e.heap.get() == theHeap.get()){
            return MTRUE;
        }
    }
    return MFALSE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
StereoBufferSynchronizerImp::
isAFStable(
    BUFFER_SET* bufInfoMain1,
    BUFFER_SET* bufInfoMain2
){

    MUINT8 AFstate_main1 = -1;
    MUINT8 AFstate_main2 = -1;

    MUINT8 lensState_main1 = -1;
    MUINT8 lensState_main2 = -1;

    MBOOL ret = MFALSE;

    // fetch the metadata with af state
    for ( size_t i = 0; i < bufInfoMain1->metadata.size(); ++i) {
        if (tryGetMetadata<MUINT8>(const_cast<IMetadata*>(&bufInfoMain1->metadata[i].meta), MTK_CONTROL_AF_STATE, AFstate_main1) &&
            tryGetMetadata<MUINT8>(const_cast<IMetadata*>(&bufInfoMain1->metadata[i].meta), MTK_LENS_STATE, lensState_main1)
            )
        {
            break;
        }
    }
    for ( size_t i = 0; i < bufInfoMain2->metadata.size(); ++i) {
        if (tryGetMetadata<MUINT8>(const_cast<IMetadata*>(&bufInfoMain2->metadata[i].meta), MTK_CONTROL_AF_STATE, AFstate_main2) &&
            tryGetMetadata<MUINT8>(const_cast<IMetadata*>(&bufInfoMain2->metadata[i].meta), MTK_LENS_STATE, lensState_main2))
        {
            break;
        }
    }

    if( AFstate_main1 == MTK_CONTROL_AF_STATE_PASSIVE_SCAN ||
        AFstate_main1 == MTK_CONTROL_AF_STATE_ACTIVE_SCAN ||
        lensState_main1 == MTK_LENS_STATE_MOVING ||
        AFstate_main2 == MTK_CONTROL_AF_STATE_PASSIVE_SCAN ||
        AFstate_main2 == MTK_CONTROL_AF_STATE_ACTIVE_SCAN ||
        lensState_main2 == MTK_LENS_STATE_MOVING)
    {
        ret = MFALSE;
    }else{
        ret = MTRUE;
    }

    MY_LOGD1("AF state %d/%d  lens state:%d/%d,  return:%d", AFstate_main1, AFstate_main2, lensState_main1, lensState_main2, ret);
    return ret;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StereoBufferSynchronizerImp::
markAFState(
    BUFFER_SET* bufInfo,
    MBOOL isAFStable
){
    if(bufInfo == NULL){
        return;
    }
    bufInfo->isAFStable = isAFStable;
}
/******************************************************************************
 *
 ******************************************************************************/
StereoBufferSynchronizerImp::BUFFER_SET
StereoBufferSynchronizerImp::
removeFromQueue(
    list<BUFFER_SET>* theQue, android::sp<IImageBufferHeap>& theHeap
){
    BUFFER_SET retSet;
    for (std::list<BUFFER_SET>::iterator itr = theQue->begin(); itr != theQue->end();){
        if ((*itr).heap.get() == theHeap.get()){
            retSet = (*itr);
            itr = theQue->erase(itr);
        }
        else{
            ++itr;
        }
    }
    return retSet;
}
/*****************************************************************************
 *
 ******************************************************************************/
bool
StereoBufferSynchronizerImp::
threadLoop()
{
    FUNC_START;

    BUFFER_SET bufInfoResized;
    BUFFER_SET bufInfoResized_main2;
    BUFFER_SET bufInfoFull;
    BUFFER_SET bufInfoFull_main2;

    // try to grab the to-be-checked buffer set
    {
        Mutex::Autolock _l(mPreviewPendingQueLock);

        MY_LOGD1("StereoSynchronizer threadLoop wait +");
        while(!isReadyToSyncCheck()){
            mCondPushPreviewPending.wait(mPreviewPendingQueLock);
        }
        MY_LOGD1("StereoSynchronizer threadLoop wait -");
        mSyncThreadProfile.pulse_up();

        // resized raw from each sensor
        if(streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER)){
            bufInfoResized = previewPendingResizeRaw.front();
            previewPendingResizeRaw.pop_front();
        }
        if(streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)){
            bufInfoResized_main2 = previewPendingResizeRaw_main2.front();
            previewPendingResizeRaw_main2.pop_front();
        }
        // full raw from each sensor
        if(streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)){
            bufInfoFull = previewPendingFullRaw.front();
            previewPendingFullRaw.pop_front();
        }
        if(streamToPreviewPendingQue.count(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)){
            bufInfoFull_main2 = previewPendingFullRaw_main2.front();
            previewPendingFullRaw_main2.pop_front();
        }
    }

    // pair check: check whether imgo & rrzo are the same pair
    MBOOL pairCheck_result_main1 = MTRUE;
    MBOOL pairCheck_result_main2 = MTRUE;
    if(bufInfoResized.heap.get() && bufInfoFull.heap.get()){
        pairCheck_result_main1 = pairCheck(&bufInfoResized, &bufInfoFull);

        if(!pairCheck_result_main1){
            MY_LOGD1("pair check failed for main1!");
        }
    }
    if(bufInfoResized_main2.heap.get() && bufInfoFull_main2.heap.get()){
        pairCheck_result_main2 = pairCheck(&bufInfoResized_main2, &bufInfoFull_main2);

        if(!pairCheck_result_main2){
            MY_LOGD1("pair check failed for main2!");
        }
    }
    if( !pairCheck_result_main1 || !pairCheck_result_main2 ){
        if(bufInfoResized.heap.get() && (!bufInfoResized.isReturned)){
            returnBufferToPendingQue(&bufInfoResized);
        }
        if(bufInfoFull.heap.get() && (!bufInfoFull.isReturned)){
            returnBufferToPendingQue(&bufInfoFull);
        }
        if(bufInfoResized_main2.heap.get() && (!bufInfoResized_main2.isReturned)){
            returnBufferToPendingQue(&bufInfoResized_main2);
        }
        if(bufInfoFull_main2.heap.get() && (!bufInfoFull_main2.isReturned)){
            returnBufferToPendingQue(&bufInfoFull_main2);
        }
        return true;
    }

    // synchcronization routine
    syncRoutine(
        (bufInfoResized.heap.get())       ? &bufInfoResized       : NULL,
        (bufInfoResized_main2.heap.get()) ? &bufInfoResized_main2 : NULL,
        (bufInfoFull.heap.get())          ? &bufInfoFull          : NULL,
        (bufInfoFull_main2.heap.get())    ? &bufInfoFull_main2    : NULL
    );

    mSyncThreadProfile.pulse_down();

    if(mbExitThread)
    {
        MY_LOGD("exit thread loop");
        FUNC_END;
        return false;
    }

    return  true;
}

/******************************************************************************
 *
 ******************************************************************************/
}; //namespace NSLegacyPipeline
}; //namespace v1
}; //namespace NSCam
