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

#define LOG_TAG "MtkCam/VideoRawDumpBufferPoolImp"
//
#include "MyUtils.h"
//
#include <utils/RWLock.h>
#include <utils/Thread.h>
//
#include <sys/prctl.h>
#include <sys/resource.h>
//
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProvider.h>
#include <Format.h>
//
#include "VideoRawBufferPool.h"
//
#include "RawDumpCmdQueThread.h"
//
#include <mtkcam/drv/IHalSensor.h>

using namespace android;
using namespace android::MtkCamUtils;
using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::Utils;

using namespace NSCam::v3::Utils;
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

#if 0
#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")
#else
#define FUNC_START
#define FUNC_END
#endif

/******************************************************************************
 *
 ******************************************************************************/
#define BUFFERPOOL_NAME       ("Cam@v1BufferPool")
#define BUFFERPOOL_POLICY     (SCHED_OTHER)
#define BUFFERPOOL_PRIORITY   (0)
//
#define MAX_VIDEO_DUMPCNT  200
static uint32_t g_u4RawDumpCnt = 0;


class VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp
    : public virtual RefBase
    , public Thread
{
public:
                                        VideoRawDumpBufferPoolImp(
                                            sp<IImageStreamInfo> pStreamInfo,
                                            sp<IParamsManagerV3> pParamsManagerV3,
                                            MINT32 const i4OpenId,
                                            sp<INotifyCallback> pCamMsgCbInfo
                                        )
                                            : mpStreamInfo(pStreamInfo), mpParamsManagerV3(pParamsManagerV3)
                                            ,mOpenId(i4OpenId), mpCamMsgCbInfo(pCamMsgCbInfo)
                                        {
                                            mLogLevel = ::property_get_int32("debug.camera.log", 0);
                                            if ( mLogLevel == 0 ) {
                                                mLogLevel = ::property_get_int32("debug.camera.log.BPImp", 0);
                                            }
                                            //
                                            //eng mode video raw dump
                                            {
                                                int camera_mode = mpParamsManagerV3->getParamsMgr()->getInt(MtkCameraParameters::KEY_CAMERA_MODE);
                                                int tg_out_mode = 0; // NORNAL
                                                MY_LOGD("camera mode = %d", camera_mode);
                                                if (camera_mode == 1)
                                                {
                                                    tg_out_mode = mpParamsManagerV3->getParamsMgr()->getInt(MtkCameraParameters::KEY_PREVIEW_DUMP_RESOLUTION);
                                                    MY_LOGD("tg_out_mode = %d", tg_out_mode);
                                                }

                                                if (camera_mode != 0)
                                                {

                                                    MY_LOGD("create RawDumpThread instance");

                                                    status_t status = NO_ERROR;

                                                    NSCam::IHalSensorList *pSensorHalList = NULL;
                                                    NSCam::SensorStaticInfo sensorStaticInfo;

                                                    pSensorHalList = MAKE_HalSensorList();
                                                    MUINT32 sensorDev = pSensorHalList->querySensorDevIdx(mOpenId);
                                                    pSensorHalList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);

                                                    MUINT32 rawSensorBit = 0;
                                                    switch (sensorStaticInfo.rawSensorBit)
                                                    {
                                                        case RAW_SENSOR_8BIT:
                                                            rawSensorBit = 8;
                                                            break;
                                                        case RAW_SENSOR_10BIT:
                                                            rawSensorBit = 10;
                                                            break;
                                                        case RAW_SENSOR_12BIT:
                                                            rawSensorBit = 12;
                                                            break;
                                                        case RAW_SENSOR_14BIT:
                                                            rawSensorBit = 14;
                                                            break;
                                                    }
                                                    //
                                                    mpRawDumpCmdQueThread = IRawDumpCmdQueThread::createInstance(sensorStaticInfo.sensorFormatOrder, rawSensorBit, mpParamsManagerV3->getParamsMgr());
                                                    if  ( mpRawDumpCmdQueThread  == 0 || OK != (status = mpRawDumpCmdQueThread->run("RawDumpCmdQueThread")) )
                                                    {
                                                        MY_LOGE(
                                                            "Fail to run mpRawDumpCmdQueThread  - mpRawDumpCmdQueThread .get(%p), status[%s(%d)]",
                                                            mpRawDumpCmdQueThread.get(), ::strerror(-status), -status
                                                        );
                                                        g_u4RawDumpCnt = 0;
                                                        // goto lbExit;
                                                    }

                                                    MY_LOGD("mpRawDumpCmdQueThread::setCallbacks is called");
                                                    mpRawDumpCmdQueThread->setCallbacks(mpCamMsgCbInfo);
                                                }
                                            }
                                            char rRawCntValue[PROPERTY_VALUE_MAX] = {'\0'};
                                            property_get("eng.video.dumpcnt", rRawCntValue, "0");
                                            mu4MaxRawDumpCnt = (atoi(rRawCntValue) == 0) ? MAX_VIDEO_DUMPCNT : atoi(rRawCntValue);
                                            MY_LOGD("mu4MaxRawDumpCnt:%d", mu4MaxRawDumpCnt);
                                            //eng mode video raw dump
                                            //
                                        }

                                        ~VideoRawDumpBufferPoolImp() {
                                            // start of Video Raw Dump
                                            int camera_mode = mpParamsManagerV3->getParamsMgr()->getInt(MtkCameraParameters::KEY_CAMERA_MODE);
                                            if (camera_mode != 0)
                                            {
                                                sp<IRawDumpCmdQueThread> pRawDumpCmdQueThread = mpRawDumpCmdQueThread;
                                                if ( pRawDumpCmdQueThread  != 0 ) {
                                                    MY_LOGD(
                                                        "RawDumpCmdQ Thread: (tid, getStrongCount)=(%d, %d)",
                                                        pRawDumpCmdQueThread->getTid(), pRawDumpCmdQueThread->getStrongCount()
                                                    );
                                                    pRawDumpCmdQueThread->requestExit();
                                                }
                                            }
                                            // end of Video Raw Dump
                                            };
public:
    MERROR                              getBufferFromPool(
                                            char const*           szCallerName,
                                            sp<IImageBufferHeap>& rpBuffer,
                                            MUINT32&              rTransform
                                        );

    MERROR                              returnBufferToPool(
                                            char const* szCallerName,
                                            sp<IImageBufferHeap> rpBuffer
                                        );

    MERROR                              allocateBuffer(
                                            char const* szCallerName
                                        );

    MERROR                              addBuffer(
                                             android::sp<IImageBuffer>& pBuffer
                                        );

    MERROR                              removeBufferFromPool(
                                             char const* szCallerName,
                                             android::sp<IImageBufferHeap>& pBuffer
                                        );

    MERROR                              do_construct(
                                            sp<IImageBufferHeap>& pImageBufferHeap
                                        );

    android::sp<IImageStreamInfo>       getStreamInfo();

    MVOID                               flush();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  RefBase Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    void                                onLastStrongRef(const void* /*id*/);

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

public:
    mutable Mutex                       mPoolLock;
    List< sp<IImageBufferHeap> >        mAvailableBuf;
    List< sp<IImageBufferHeap> >        mInUseBuf;
    mutable Condition                   mCondWaitBuf;

public:
    MINT32                              mLogLevel;
    MINT32                              mMaxBuffer;
    MINT32                              mMinBuffer;

public:
    sp<IImageStreamInfo>                mpStreamInfo;
    sp<IParamsManagerV3>                mpParamsManagerV3;
    MINT32                              mOpenId;
    sp<INotifyCallback>                 mpCamMsgCbInfo;

private:
    MUINT32 mu4MaxRawDumpCnt;
    sp<IRawDumpCmdQueThread>        mpRawDumpCmdQueThread;

};


/******************************************************************************
 *
 ******************************************************************************/
VideoRawDumpBufferPool::
~VideoRawDumpBufferPool()
{}

/******************************************************************************
 *
 ******************************************************************************/
VideoRawDumpBufferPool::
VideoRawDumpBufferPool(
    sp<IImageStreamInfo> pStreamInfo,
    sp<IParamsManagerV3> pParamsManagerV3,
    MINT32 const i4OpenId,
    sp<INotifyCallback> pCamMsgCbInfo
)
    : mImp( new VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp(pStreamInfo, pParamsManagerV3, i4OpenId, pCamMsgCbInfo) )
    , mbNoNeedReturnBuffer( false )
{}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::
acquireFromPool(
    char const*           szCallerName,
    MINT32                rRequestNo,
    sp<IImageBufferHeap>& rpBuffer,
    MUINT32&              rTransform
)
{
    MY_LOGD_IF( 0 , "%s", szCallerName);

    if ( mImp->getBufferFromPool(
              szCallerName,
              rpBuffer,
              rTransform ) == OK )
    {
        return OK;
    }

    return UNKNOWN_ERROR;

}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
getBufferFromPool(
    char const*           szCallerName,
    sp<IImageBufferHeap>& rpBuffer,
    MUINT32&              rTransform
)
{
    FUNC_START;

    Mutex::Autolock _l(mPoolLock);

    MY_LOGD_IF( mLogLevel >= 1, "[%s] mAvailableBuf:%d mInUseBuf:%d",
            szCallerName, mAvailableBuf.size(), mInUseBuf.size());

    if(mAvailableBuf.empty())
    {
#define WAIT_BUFFER_TIMEOUT (1000000000) //ns
        MY_LOGD("No mAvailableBuf, start waiting %lld ns", WAIT_BUFFER_TIMEOUT);
        status_t status = mCondWaitBuf.waitRelative(mPoolLock, WAIT_BUFFER_TIMEOUT);
        if( status != OK )
        {
            MY_LOGW("waiting timeout");
        }
#undef WAIT_BUFFER_TIMEOUT
    }

    if( !mAvailableBuf.empty() )
    {
        typename List< sp<IImageBufferHeap> >::iterator iter = mAvailableBuf.begin();
        mInUseBuf.push_back(*iter);
        rpBuffer = *iter;
        rTransform = mpStreamInfo->getTransform();
        mAvailableBuf.erase(iter);
        //
        FUNC_END;
        return OK;
    }
    MY_LOGW("mAvailableBuf is empty");

    FUNC_END;
    return TIMED_OUT;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::
releaseToPool(
    char const*          szCallerName,
    MINT32               rRequestNo,
    sp<IImageBufferHeap> rpBuffer,
    MUINT64              rTimeStamp,
    bool                 rErrorResult
)
{
    MY_LOGD_IF( 0, "%s", szCallerName);

    FUNC_START;
    MBOOL ret = MFALSE;

    MY_LOGD_IF( 1 , "Ready to onBufferReturned");

    if( mbNoNeedReturnBuffer )
    {
        ret = mImp->removeBufferFromPool(
                      szCallerName,
                      rpBuffer
                      );
    }
    else
    {
        ret = returnBufferToPool(
                      szCallerName,
                      rpBuffer
                      );
    }
    FUNC_END;
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::
returnBufferToPool(
    char const* szCallerName,
    sp<IImageBufferHeap> rpBuffer
)
{
    return mImp->returnBufferToPool(
                  szCallerName,
                  rpBuffer
                  );
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
returnBufferToPool(
    char const* szCallerName,
    sp<IImageBufferHeap> rpBuffer
)
{
//
 { // start of Video Raw Dump

        static bool mEnableRawDump = false;
        //(4) enable RawDump
        {
            String8 const s = mpParamsManagerV3->getParamsMgr()->getStr(MtkCameraParameters::KEY_RAW_DUMP_FLAG);
#undef TRUE
#undef FALSE
            bool RawDumpFlag = ( ! s.isEmpty() && s == CameraParameters::TRUE ) ? 1 : 0;

            if ( mEnableRawDump != RawDumpFlag )
            {
                MY_LOGD("RawDump flag changed to %d ", RawDumpFlag);
                mEnableRawDump = RawDumpFlag;
                 g_u4RawDumpCnt = 0;
            }
        }

        if (mpRawDumpCmdQueThread != 0 && (g_u4RawDumpCnt < mu4MaxRawDumpCnt) && mEnableRawDump == true )
        {
            MY_LOGD("u4RawDumpCnt, Max = (%d, %d)", g_u4RawDumpCnt, mu4MaxRawDumpCnt); // debug
            //
            sp<IImageBuffer> buf = rpBuffer->createImageBuffer();
            mpRawDumpCmdQueThread->postCommand(buf.get());
            g_u4RawDumpCnt++;
        }
        else if (g_u4RawDumpCnt == mu4MaxRawDumpCnt)
        {
            MY_LOGD("Stop");
            mpParamsManagerV3->getParamsMgr()->set(MtkCameraParameters::KEY_RAW_DUMP_FLAG, CameraParameters::FALSE);
            mpRawDumpCmdQueThread->postCommand(NULL);
            mEnableRawDump=false;
            g_u4RawDumpCnt=0;
        }
    } // end of Video Raw Dump

//
    FUNC_START;
    Mutex::Autolock _l(mPoolLock);

    typename List< sp<IImageBufferHeap> >::iterator iter = mInUseBuf.begin();
    while( iter != mInUseBuf.end() ) {
        if ( rpBuffer == (*iter) ) {
            mAvailableBuf.push_back(*iter);
            mInUseBuf.erase(iter);

            MY_LOGD_IF(
                mLogLevel >= 1,
                "[%s] mAvailableBuf:%d mInUseBuf:%d", szCallerName, mAvailableBuf.size(), mInUseBuf.size()
            );

            mCondWaitBuf.signal();

            FUNC_END;
            return OK;
        }
        iter++;
    }

    MY_LOGE("[%s] Cannot find buffer %p.", szCallerName, rpBuffer.get());

    mCondWaitBuf.signal();
    FUNC_END;

    return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
char const*
VideoRawDumpBufferPool::
poolName() const
{
    return "VideoRawDumpBufferPool";
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
VideoRawDumpBufferPool::
dumpPool() const
{
#warning "TODO"
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::
allocateBuffer(
    char const* szCallerName,
    size_t /*maxNumberOfBuffers*/,
    size_t /*minNumberOfInitialCommittedBuffers*/
)
{
    mbNoNeedReturnBuffer = false;
    return mImp->allocateBuffer(szCallerName);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
allocateBuffer(
    char const* szCallerName
)
{
    FUNC_START;

    if ( mpStreamInfo == 0 ) {
        MY_LOGE("No ImageStreamInfo.");
        return UNKNOWN_ERROR;
    }

    mMaxBuffer = mpStreamInfo->getMaxBufNum();
    mMinBuffer = mpStreamInfo->getMinInitBufNum();

    if ( mMinBuffer > mMaxBuffer) {
        MY_LOGE("mMinBuffer:%d > mMaxBuffer:%d", mMinBuffer, mMaxBuffer);
        return UNKNOWN_ERROR;
    }

    for (size_t i = 0; i < mMinBuffer; ++i ) {
        sp<IImageBufferHeap> pHeap;
        if( do_construct(pHeap) == NO_MEMORY ) {
            MY_LOGE("do_construct allocate buffer failed");
            continue;
        }
        {
            Mutex::Autolock _l(mPoolLock);
            mAvailableBuf.push_back(pHeap);
        }
    }

    status_t status = run(BUFFERPOOL_NAME);
    if  ( OK != status ) {
        MY_LOGE("Fail to run the thread - status:%d(%s)", status, ::strerror(-status));
        return UNKNOWN_ERROR;
    }

    MY_LOGD("(min,max):(%d,%d)", mMinBuffer, mMaxBuffer);

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::
addBuffer(
    android::sp<IImageBuffer>& pBuffer
)
{
    mbNoNeedReturnBuffer = true;
    return mImp->addBuffer(pBuffer);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
addBuffer(
    android::sp<IImageBuffer>& pBuffer
)
{
    FUNC_START;
    IImageBufferHeap *pBuf = pBuffer->getImageBufferHeap();
    MY_LOGD("streamID %d, add buf 0x%x", mpStreamInfo->getStreamId(), pBuf);

    if(pBuffer.get() == NULL)
    {
        MY_LOGE("Input Buffer is NULL!");
        return INVALID_OPERATION;
    }

    Mutex::Autolock _l(mPoolLock);
    mAvailableBuf.push_back(pBuf);

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
android::sp<IImageStreamInfo>
VideoRawDumpBufferPool::
getStreamInfo()
{
    return mImp->getStreamInfo();
}


/******************************************************************************
 *
 ******************************************************************************/
android::sp<IImageStreamInfo>
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
getStreamInfo()
{
    return mpStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
VideoRawDumpBufferPool::
flush()
{
    return mImp->flush();
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
flush()
{
    FUNC_START;
    mAvailableBuf.clear();
    mInUseBuf.clear();
    FUNC_END;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
removeBufferFromPool(
    char const* szCallerName,
    android::sp<IImageBufferHeap>& pBuffer
)
{
    FUNC_START;
    Mutex::Autolock _l(mPoolLock);

    typename List< sp<IImageBufferHeap> >::iterator iter = mInUseBuf.begin();
    while( iter != mInUseBuf.end() ) {
        if ( pBuffer == (*iter) ) {
            mInUseBuf.erase(iter);

            MY_LOGD_IF(
                mLogLevel >= 1,
                "[%s] mAvailableBuf:%d mInUseBuf:%d", szCallerName, mAvailableBuf.size(), mInUseBuf.size()
            );
            FUNC_END;
            return OK;
        }
        iter++;
    }
    iter = mAvailableBuf.begin();
    while( iter != mAvailableBuf.end() ) {
        if ( pBuffer == (*iter) ) {
            mAvailableBuf.erase(iter);

            MY_LOGD_IF(
                mLogLevel >= 1,
                "[%s] mAvailableBuf:%d mInUseBuf:%d", szCallerName, mAvailableBuf.size(), mInUseBuf.size()
            );
            FUNC_END;
            return OK;
        }
        iter++;
    }

    MY_LOGE("[%s] Cannot find buffer %p.", szCallerName, pBuffer.get());

    FUNC_END;
    return UNKNOWN_ERROR;
}



/******************************************************************************
 *
 ******************************************************************************/
void
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
onLastStrongRef(const void* /*id*/)
{
    CAM_TRACE_NAME("VideoRawDumpBufferPool:onLastStrongRef");
    FUNC_START;
    requestExit();
    //
    mAvailableBuf.clear();

    if ( mInUseBuf.size() > 0 ) {
        typename List< sp<IImageBufferHeap> >::iterator iter = mInUseBuf.begin();
        while( iter != mInUseBuf.end() ) {
            MY_LOGW("[%s] buffer %p not return to pool.", "AIAP"/*poolName()*/, (*iter).get());
            iter++;
        }
    }

    mInUseBuf.clear();
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
void
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
requestExit()
{
    //let allocate thread back
    Thread::requestExitAndWait();
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
readyToRun()
{
    // set name
    ::prctl(PR_SET_NAME, (unsigned long)BUFFERPOOL_NAME, 0, 0, 0);

    // set normal
    struct sched_param sched_p;
    sched_p.sched_priority = 0;
    ::sched_setscheduler(0, BUFFERPOOL_POLICY, &sched_p);
    ::setpriority(PRIO_PROCESS, 0, BUFFERPOOL_PRIORITY);
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

/******************************************************************************
 *
 ******************************************************************************/
MERROR
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
do_construct(
    sp<IImageBufferHeap>& pImageBufferHeap
)
{
    IImageStreamInfo::BufPlanes_t const& bufPlanes = mpStreamInfo->getBufPlanes();
    size_t bufStridesInBytes[3] = {0};
    size_t bufBoundaryInBytes[3]= {0};
    for (size_t i = 0; i < bufPlanes.size(); i++) {
        bufStridesInBytes[i] = bufPlanes[i].rowStrideInBytes;
    }

    if ( eImgFmt_JPEG == mpStreamInfo->getImgFormat() ||
         eImgFmt_BLOB == mpStreamInfo->getImgFormat() )
    {
        IImageBufferAllocator::ImgParam imgParam(
                mpStreamInfo->getImgSize(),
                (*bufStridesInBytes),
                (*bufBoundaryInBytes));
        imgParam.imgFormat = eImgFmt_BLOB;
        MY_LOGD("eImgFmt_JPEG -> eImgFmt_BLOB");
        pImageBufferHeap = IIonImageBufferHeap::create(
                                mpStreamInfo->getStreamName(),
                                imgParam,
                                IIonImageBufferHeap::AllocExtraParam(),
                                MFALSE
                            );
    }
    else
    {
        IImageBufferAllocator::ImgParam imgParam(
            mpStreamInfo->getImgFormat(),
            mpStreamInfo->getImgSize(),
            bufStridesInBytes, bufBoundaryInBytes,
            bufPlanes.size()
            );
        MY_LOGD("format:%x, size:(%d,%d), stride:%d, boundary:%d, planes:%d",
            mpStreamInfo->getImgFormat(),
            mpStreamInfo->getImgSize().w, mpStreamInfo->getImgSize().h,
            bufStridesInBytes[0], bufBoundaryInBytes[0], bufPlanes.size()
            );
        pImageBufferHeap = IIonImageBufferHeap::create(
                                mpStreamInfo->getStreamName(),
                                imgParam,
                                IIonImageBufferHeap::AllocExtraParam(),
                                MFALSE
                            );
    }

    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
VideoRawDumpBufferPool::VideoRawDumpBufferPoolImp::
threadLoop()
{
    bool next = false;
    {
        Mutex::Autolock _l(mPoolLock);
        if ( (mAvailableBuf.size() + mInUseBuf.size() ) >= mMaxBuffer)
            return false;
    }
    sp<IImageBufferHeap> pHeap;
    if( do_construct(pHeap) == NO_MEMORY ) {
        MY_LOGE("do_construct allocate buffer failed");
        return true;
    }

    {
        Mutex::Autolock _l(mPoolLock);
        mAvailableBuf.push_back(pHeap);

        next = (mAvailableBuf.size() + mInUseBuf.size() ) < mMaxBuffer;
    }
    return next;
}
