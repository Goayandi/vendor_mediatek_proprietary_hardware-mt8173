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
#define LOG_TAG "MtkCam/CamClient/PREFEATUREBASE"
//
#include "PreviewFeatureBase.h"
using namespace NSCamClient;
using namespace NSPreviewFeatureBase;
//
#include <sys/prctl.h>
#include <sys/resource.h>
//

/******************************************************************************
*
*******************************************************************************/
#define ENABLE_LOG_PER_FRAME        (0)
static int g_cmd_get_count = 0;
static int g_cmd_post_count = 0;

/******************************************************************************
*
*******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

/******************************************************************************
 *
 ******************************************************************************/
//#define debug
#ifdef debug
#include <fcntl.h>
#include <sys/stat.h>
bool
savePreviewToFile(char const*const fname, MUINT8 *const buf, MUINT32 const size)
{
    int nw, cnt = 0;
    uint32_t written = 0;

    CAM_LOGD("(name, buf, size) = (%s, %x, %d)", fname, buf, size);
    CAM_LOGD("opening file [%s]\n", fname);
    int fd = ::open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (fd < 0) {
        CAM_LOGE("failed to create file [%s]: %s", fname, ::strerror(errno));
        return false;
    }

    CAM_LOGD("writing %d bytes to file [%s]\n", size, fname);
    while (written < size) {
        nw = ::write(fd,
                     buf + written,
                     size - written);
        if (nw < 0) {
            CAM_LOGE("failed to write to file [%s]: %s", fname, ::strerror(errno));
            break;
        }
        written += nw;
        cnt++;
    }
    CAM_LOGD("done writing %d bytes to file [%s] in %d passes\n", size, fname, cnt);
    ::close(fd);
    return true;
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
void PreviewFeatureBase::requestStop()
{
    MY_LOGD("+");
    //
    //postCommand(Command::eID_STOP);
    //
    MY_LOGD("-");
}

/******************************************************************************
 *
 ******************************************************************************/
// Ask this object's thread to exit. This function is asynchronous, when the
// function returns the thread might still be running. Of course, this
// function can be called from a different thread.
void PreviewFeatureBase::requestExit()
{
    MY_LOGD("+");
    Thread::requestExit();

    CMD_Q_T cmdQ;
    cmdQ.mvX = 0;
    cmdQ.mvY = 0;
    cmdQ.mStitchDir = 0;
    cmdQ.isShot = 0;
    cmdQ.isSound = 0;
    postCommand(cmdQ);

    MY_LOGD("-");
}

/******************************************************************************
 *
 ******************************************************************************/
// Good place to do one-time initializations
status_t PreviewFeatureBase::readyToRun()
{
    ::prctl(PR_SET_NAME,"PreviewFeatureBase@Preview", 0, 0, 0);
    //
    mi4ThreadId = ::gettid();

    //  thread policy & priority
    //  Notes:
    //      Even if pthread_create() with SCHED_OTHER policy, a newly-created thread
    //      may inherit the non-SCHED_OTHER policy & priority of the thread creator.
    //      And thus, we must set the expected policy & priority after a thread creation.
    int const policy    = SCHED_OTHER;
    int const priority  = 0;
    //
    //
    struct sched_param sched_p;
    ::sched_getparam(0, &sched_p);
    sched_p.sched_priority = priority;  //  Note: "priority" is nice value
    sched_setscheduler(0, policy, &sched_p);
    setpriority(PRIO_PROCESS, 0, priority);
    //
    //
    MY_LOGD(
        "policy:(expect, result)=(%d, %d), priority:(expect, result)=(%d, %d)"
        , policy, ::sched_getscheduler(0)
        , priority, getpriority(PRIO_PROCESS, 0)
    );

    g_cmd_get_count = 0;
    g_cmd_post_count = 0;

    return NO_ERROR;
}


/******************************************************************************
 *
 ******************************************************************************/
void PreviewFeatureBase::postCommand(CMD_Q_T &cmdQ)
{
    g_cmd_post_count++;
    Mutex::Autolock _lock(mCmdQueMtx);
    //
    if  ( ! mCmdQue.empty() )
    {
        //Command::EID const& rBegCmd = *mCmdQue.begin();
        MY_LOGI("que size:%d > 0", mCmdQue.size());
    }
    //
    mCmdQue.push_back(cmdQ);
    mCmdQueCond.broadcast();
    //
    //MY_LOGD("command+(%d)", cmd_count);
}


/******************************************************************************
 *
 ******************************************************************************/
bool PreviewFeatureBase::getCommand(CMD_Q_T &cmdQ)
{
    bool ret = false;
    //
    Mutex::Autolock _lock(mCmdQueMtx);
    //
    MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "+ que size(%d)", mCmdQue.size());
    //
    //  Wait until the queue is not empty or this thread will exit.
    while   ( mCmdQue.empty() && ! exitPending() )
    {
        status_t status = mCmdQueCond.wait(mCmdQueMtx);
        if  ( NO_ERROR != status )
        {
            MY_LOGW("wait status(%d), que size(%d), exitPending(%d)", status, mCmdQue.size(), exitPending());
        }
    }
    //
    if  ( ! mCmdQue.empty() && ! exitPending() )
    {
        g_cmd_get_count++;
        //  If the queue is not empty, take the first command from the queue.
        ret = true;
        cmdQ = *mCmdQue.begin();
        mCmdQue.erase(mCmdQue.begin());
        MY_LOGD("command p(%d) g(%d)", g_cmd_get_count, g_cmd_post_count);
    }
    //
    MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "- que size(%d), ret(%d)", mCmdQue.size(), ret);
    return  ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool PreviewFeatureBase::threadLoop()
{
    CMD_Q_T cmdQ;
    if  ( getCommand(cmdQ) )
    {
        performCallback(cmdQ.mvX, cmdQ.mvY, cmdQ.mStitchDir, cmdQ.isShot, cmdQ.isSound);
    }
    //
    //MY_LOGD("-");
    return  true;
}
/******************************************************************************
 *
 ******************************************************************************/
void* PreviewFeatureBase::onClientThreadLoopFunc(void *arg)
{
    MY_LOGI("+");
    PreviewFeatureBase* pBase = (PreviewFeatureBase*)arg;
    ::prctl(PR_SET_NAME,"onClientThreadLoopFunc", 0, 0, 0);
    pBase->onClientThreadLoop();
    MY_LOGI("-");
    return NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
void PreviewFeatureBase::onClientThreadLoop()
{
    MY_LOGD("+");

    //(0) pre-check
    sp<IImgBufQueue> pBufQueue = NULL;
    {
        Mutex::Autolock _l(mModuleMtx);
        //
        pBufQueue = mpImgBufQueue;
        if ( pBufQueue == 0 || ! isEnabledState())
        {
            MY_LOGE("pBufMgr(%p), isEnabledState(%d)", pBufQueue.get(), isEnabledState());
            return;
        }
    }
    pBufQueue->pauseProcessor();
    pBufQueue->flushProcessor(); // clear "TODO"
    pBufQueue->stopProcessor();  // clear "DONE"

    //(1) prepare all TODO buffers
    if ( ! initBuffers(pBufQueue) )
    {
        MY_LOGE("initBuffers failed");
        return;
    }

    //(2) start
    if  ( !pBufQueue->startProcessor() )
    {
        MY_LOGE("startProcessor failed");
        return;
    }
    int framecun=0;
    //(3) Do until stopFD has been called
    //    either by sendCommand() or by stopPreview()
    while ( isEnabledState() )
    {
        // (3.1) deque from processor
        ImgBufQueNode rQueNode;
        if ( ! waitAndHandleReturnBuffers(pBufQueue, rQueNode) )
        {
            MY_LOGD("No available deque-ed buffer; to be leaving");
            continue;
        }

        if ( rQueNode.getImgBuf() == 0 )
        {
            MY_LOGE("rQueNode.getImgBuf() == 0");
            continue;
        }

        //  Get Time duration.
        nsecs_t const _timestamp = rQueNode.getImgBuf()->getTimestamp();

        // (3.2) do Feature algorithm
        int32_t mvX = 0;
        int32_t mvY = 0;
        int32_t dir = (int32_t) (_timestamp / 1000000);
        MBOOL isShot;
        MY_LOGV("getImgWidth %d getImgHeight %d",rQueNode.getImgBuf()->getImgWidth(),rQueNode.getImgBuf()->getImgHeight());
        #ifdef debug
            // for test
            char sourceFiles[80];
            sprintf(sourceFiles, "%s%d_%dx%d.yuv420", "/sdcard/getpreview", framecun,bufWidth,bufHeight);
            savePreviewToFile((char *) sourceFiles, (MUINT8*)rQueNode.getImgBuf()->getVirAddr() , rQueNode.getImgBuf()->getBufSize());

        #endif

        if ( !FeatureClient->mHalCamFeatureProc(rQueNode.getImgBuf()->getVirAddr() , mvX, mvY, dir, isShot) )
        {
            MY_LOGE("Do mHalCamFeatureProc fail!! leave while loop");
            break;
        }

        if(isShot)
        {
            MY_LOGD("isShot %d, framecun %d, mShotNum %d ", isShot, framecun, mShotNum);
            framecun++;
        }

        if(framecun>=mShotNum)
        {
            isDoMerge = 1;
            ::android_atomic_write(0, &mIsFeatureStarted);
            onStateChanged();
        }
        // (3.3)
        CMD_Q_T cmdQ;
        cmdQ.mvX = mvX;
        cmdQ.mvY = mvY;
        cmdQ.mStitchDir = dir;
        cmdQ.isShot = isShot;
        cmdQ.isSound = 1;
        postCommand(cmdQ);
        //performCallback(mvX, mvY, dir, isShot, 1);

        // (3.4)
        // enque back to processor
        handleReturnBuffers(pBufQueue, rQueNode);
    }

    //(4) stop.
    pBufQueue->pauseProcessor();
    pBufQueue->flushProcessor(); // clear "TODO"
    pBufQueue->stopProcessor();  // clear "DONE"

    MY_LOGD("-");
}

