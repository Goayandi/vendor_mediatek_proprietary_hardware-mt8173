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

#define LOG_TAG "MtkCam/CamClient/FDClient"
//
#include <cutils/properties.h>
#include "FDClient.h"
#include <mtkcam/utils/fwk/MtkCamera.h>
#include <mtkcam/feature/hdrDetection/Defs.h>
//
using namespace NSCamClient;
using namespace NSFDClient;
//

#if '1'==MTKCAM_HAVE_3A_HAL

//using namespace NS3A;
using namespace NS3Av3;
#endif

/******************************************************************************
*
*******************************************************************************/
#define ENABLE_LOG_PER_FRAME        (1)


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


#define HRD_NOTSUPPORT (0)
#define HRD_READY      (1)
#define HRD_STARTPREV  (2)
#define HRD_STOPPREV   (3)
#define HRD_TAKEPIC    (4)
#define HRD_STARTREC   (5)
#define HRD_STOPREC    (6)
#define HRD_HRDSUPPORT (7)

/******************************************************************************
 *
 ******************************************************************************/
sp<IFDClient>
IFDClient::
createInstance(sp<IParamsManager> pParamsMgr)
{
    return  new FDClient(pParamsMgr);
}


/******************************************************************************
 *
 ******************************************************************************/
FDClient::
FDClient(sp<IParamsManager> pParamsMgr)
    // Camera control objects
    : mpCamMsgCbInfo(new CamMsgCbInfo)
    , mpParamsMgr(pParamsMgr)
    , mpImgBufQueue(NULL)
    , mpImgBufPvdrClient(NULL)
    // Command & Thread control
    , mCmdQue()
    , mCmdQueMtx()
    , mCmdQueCond()
    , mi4ThreadId(0)
    , mModuleMtx()
    , mControlMtx()
    , mCallbackMtx()
    // Face Detection
    , mIsFDStarted(0)
    , mIsDetected_FD(false)
    , mFDBufferWidth(0)
    , mFDBufferPlanes(0)
    , mRotation_Info(0)
    , mAEStable(0)
    , mpFDHalObj(NULL)
    , mpDetectedFaces(NULL)
    , mpDDPBuffer(NULL)
    , mpExtractYBuffer(NULL)
    , mpFDWorkingBuffer(NULL)
    , mFDWorkingBufferSize(0)
    // Smile detection
    , mIsDetected_SD(false)
    , mIsSDenabled(false)
    // Gesture detection
    , mpGDHalObj(NULL)
    , mpDetectedGestures(NULL)
    , mIsDetected_GD(false)
    , mIsGDenabled(false)
    // ASD
    , mpASDClient(NULL)
    // Heart rate detection
    , mIsHRStarted(0)
    , mIsHRPreStarted(0)
    , mHRMode(0)
    , mISHRenabled(0)
    , mHRStarting(0)
    , mpHRObj(NULL)
    , mIsHRSupported(0)
    , mpHeartrateUser(NULL)
#if '1' == MTKCAM_HR_MONITOR_SUPPORT
    , mHeartrateCb(NULL)
#endif
    // main face information
    , mMainFace_X(0)
    , mMainFace_Y(0)
    , mIsMainFaceEn(false)
    // Debug
    , mClientFDDumpOPT(0)
    , mLogLevel(0)
    , mForceDisableFD(0)
    , mSetCBDone(false)
{
    MY_LOGD("+ this(%p)", this);
    mpCBClient = FDCBClient::createInstance();
}


/******************************************************************************
 *
 ******************************************************************************/
FDClient::
~FDClient()
{
    MY_LOGD("-");
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
init()
{
    MY_LOGD("+");
    bool ret = true;
    Mutex::Autolock _l(mControlMtx);
    mHDRDetected = -1;
    #if '1'==MTKCAM_HAVE_3A_HAL
    if(mpParamsMgr->getHDRDetectionSupported()) {
        NS3Av3::IHal3A* p3AHal = MAKE_Hal3A(mpParamsMgr->getOpenId(), LOG_TAG);
        if (p3AHal)
        {
            p3AHal->attachCb(IHal3ACb::eID_NOTIFY_HDRD_RESULT, this);
            p3AHal->destroyInstance(LOG_TAG);
        }
    }
    #endif

    mpCBClient->run("FDCBClient@Preview");

    {
        // Get user log level
        mLogLevel = ::property_get_int32("debug.camera.log.fd_hrd", 0);
    }
    {
        mForceDisableFD = ::property_get_int32("debug.camera.fd.forcedisable", 0);
    }
    {
        mGestureRatio = ::property_get_int32("debug.camera.fd.gsratio", 0);
    }
    {
        mFakeFaceEn = ::property_get_int32("debug.camera.fd.fakefaceen", 0);
    }
    //
lbExit:
    MY_LOGD("-");
    return  ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
uninit()
{
    MY_LOGD("+");
    Mutex::Autolock _l(mControlMtx);

    mSetCBDone = false;

    #if '1'==MTKCAM_HAVE_3A_HAL
    if(mpParamsMgr->getHDRDetectionSupported()) {
        NS3Av3::IHal3A* p3AHal = MAKE_Hal3A(mpParamsMgr->getOpenId(), LOG_TAG);
        if (p3AHal)
        {
            p3AHal->detachCb(IHal3ACb::eID_NOTIFY_HDRD_RESULT, this);
            p3AHal->destroyInstance(LOG_TAG);
        }
    }
    #endif
    {
        MY_LOGD("getThreadId(%d), getStrongCount(%d), this(%p)", getThreadId(), getStrongCount(), this);
        //  Notes:
        //  requestExitAndWait() in ICS has bugs. Use requestExit()/join() instead.
        stopFaceDetection();
        stopHeartrateDetection();
        requestExit();
        mpCBClient->requestExit();
        status_t status = join();
        if  ( OK != status )
        {
            MY_LOGW("Not to wait thread(tid:%d), status[%s(%d)]", getThreadId(), ::strerror(-status), -status);
        }
        MY_LOGD("join() exit");
    }
    mHDRDetected = -1;

    //
    if  ( mpImgBufQueue != NULL )
    {
        mpImgBufQueue->stopProcessor();
        mpImgBufQueue = NULL;
    }

    MY_LOGD("-");
    return  true;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
setImgBufProviderClient(sp<IImgBufProviderClient>const& rpClient)
{
    bool ret = false;

    Mutex::Autolock _l(mControlMtx);

    //
    MY_LOGD("+ ImgBufProviderClient(%p)", rpClient.get());
    //
    //
    if  ( rpClient == 0 )
    {
        MY_LOGE("NULL ImgBufProviderClient");
        goto lbExit;
    }
    //
    mpImgBufPvdrClient = rpClient;

#if '1' == MTKCAM_HR_MONITOR_SUPPORT
    {
        performHRCallback(NULL, HRD_READY);
    }
#endif

    //
    //
    ret = true;
lbExit:
    MY_LOGD("-");
    return  ret;
}

/******************************************************************************
 * do 3A callback.
 ******************************************************************************/
void
FDClient::
doNotifyCb(MINT32  _msgType, MINTPTR _ext1, MINTPTR _ext2, MINTPTR _ext3)
{
    switch(_msgType)
    {
        case IHal3ACb::eID_NOTIFY_HDRD_RESULT:
            if (mSetCBDone) {
                MINT32 data = *(MINT32 *)_ext1;
                if (data != mHDRDetected) {
                    CMD_Q_T cmd;
                    MINT32 ret = 0;
                    cmd.isFDCB = 0;
                    cmd.isSDCB = 0;
                    cmd.isGSCB = 0;
                    cmd.isASDCB = true;
                    cmd.isStopCmd = 0;
                    cmd.ASDScene = data == 1? HDR_DETECTOR_HDR : HDR_DETECTOR_NORMAL;
                    ret = mpCBClient->postCommand(cmd);
                    MY_LOGD("Post command result : %d", ret);
                    if( ret || data == -1) {
                        mHDRDetected = data;
                    }
                }
            }
            break;
        default:
            break;
    }
}


/******************************************************************************
 * Set camera message-callback information.
 ******************************************************************************/
void
FDClient::
setCallbacks(sp<CamMsgCbInfo> const& rpCamMsgCbInfo)
{
    Mutex::Autolock _l(mModuleMtx);
    //
    //  value copy
    *mpCamMsgCbInfo = *rpCamMsgCbInfo;
    mpCBClient->setCallbacks(rpCamMsgCbInfo);
    mSetCBDone = true;
}


/******************************************************************************
 *
 ******************************************************************************/
void
FDClient::
enableMsgType(int32_t msgType)
{
    ::android_atomic_or(msgType, &mpCamMsgCbInfo->mMsgEnabled);
}


/******************************************************************************
 *
 ******************************************************************************/
void
FDClient::
disableMsgType(int32_t msgType)
{
    ::android_atomic_and(~msgType, &mpCamMsgCbInfo->mMsgEnabled);
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
isMsgEnabled()
{
    if(mForceDisableFD) {
        return false;
    } else {
        return  CAMERA_MSG_PREVIEW_METADATA == (CAMERA_MSG_PREVIEW_METADATA & ::android_atomic_release_load(&mpCamMsgCbInfo->mMsgEnabled));
    }
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
onStateChanged()
{
    bool ret = true;
    //
    MY_LOGD("isEnabledState(%d) +", isEnabledState());
    //
    if  ( isEnabledState() )
    {
        if(mpImgBufQueue == NULL)
            mpImgBufQueue = new ImgBufQueue(IImgBufProvider::eID_FD, "FDBuf@ImgBufQue");
        if  ( mpImgBufQueue == NULL )
        {
            MY_LOGE("Fail to new ImgBufQueue");
            ret = false;
            goto lbExit;
        }
        if  ( mpImgBufPvdrClient == NULL || ! mpImgBufPvdrClient->onImgBufProviderCreated(mpImgBufQueue) )
        {
            MY_LOGE("onImgBufProviderCreated failed");
            ret = false;
            goto lbExit;
        }

        status_t status = run("FDClient@Preview");
        if ( INVALID_OPERATION == status)
        {
            MY_LOGW("run(): FD is running");
        }
        else if ( OK != status )
        {
            MY_LOGE("Fail to run thread, status[%s(%d)]", ::strerror(-status), -status);
            ret = false;
            goto lbExit;
        }
        //
        postCommand(Command::eID_WAKEUP);
    }
    else
    {
        MUINT32 LoopCnt = 0;
        if  ( mpImgBufPvdrClient != 0 )
        {
            mpImgBufPvdrClient->onImgBufProviderDestroyed(mpImgBufQueue->getProviderId());
        }
        if  ( mpImgBufQueue != NULL )
        {
            mpImgBufQueue->pauseProcessor();
        }

        requestExit();
        status_t status = join();
        if  ( OK != status )
        {
            MY_LOGW("Stop FD Client thread:Not to wait thread(tid:%d), status[%s(%d)]", getThreadId(), ::strerror(-status), -status);
        }
        MY_LOGD("Stop FD Client thread:join() exit");
        mCmdQue.clear();
        mpImgBufQueue = NULL;
    }
    //
lbExit:
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
FDClient::
sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    bool ret = false;

    Mutex::Autolock _l(mControlMtx);

    MY_LOGD("cmd(0x%x) +", cmd);

    switch  (cmd)
    {
    //
    case CAMERA_CMD_START_FACE_DETECTION:
        {
        ret = startFaceDetection();

        if (ret) {
        #if '1'==MTKCAM_HAVE_3A_HAL
        NS3Av3::IHal3A* p3AHal = MAKE_Hal3A(mpParamsMgr->getOpenId(), LOG_TAG);
        if (p3AHal)
        {
            p3AHal->setFDEnable(true);
            p3AHal->destroyInstance(LOG_TAG);
        }
        #endif
        }
        ret = true;
        }
        break;

    case CAMERA_CMD_STOP_FACE_DETECTION:
        {
        #if '1'==MTKCAM_HAVE_3A_HAL
        NS3Av3::IHal3A* p3AHal = MAKE_Hal3A(mpParamsMgr->getOpenId(), LOG_TAG);
        if (p3AHal)
        {
            /*if(mpDetectedFaces != 0)
            {
                mpDetectedFaces->number_of_faces = 0;
                p3AHal->setFDInfo(mpDetectedFaces);
            }*/
            {
                MtkCameraFaceMetadata TempFaceMeta;
                MtkCameraFace TempFace;
                MtkFaceInfo TempInfo;
                TempFaceMeta.number_of_faces = 0;
                memset(&TempFace, 0, sizeof(MtkCameraFace));
                memset(&TempInfo, 0, sizeof(MtkFaceInfo));
                TempFaceMeta.faces = &TempFace;
                TempFaceMeta.posInfo = &TempInfo;
                MY_LOGD("set zero face to 3A");
                p3AHal->setFDInfo(&TempFaceMeta);
            }
            p3AHal->setFDEnable(false);
            p3AHal->destroyInstance(LOG_TAG);
        }
        #endif
        }
        ret = stopFaceDetection();
        ret = true;
        break;

    case CAMERA_CMD_START_SD_PREVIEW:
        mIsSDenabled = true;
        ret = true;
        break;

    case CAMERA_CMD_CANCEL_SD_PREVIEW:
        mIsSDenabled = false;
        ret = true;
        break;

    case CAMERA_CMD_START_GD_PREVIEW:
        //MY_LOGD("StartGD");
        mIsGDenabled = true;
        ret = true;
        break;

    case CAMERA_CMD_CANCEL_GD_PREVIEW:
        MY_LOGD("StopGD");
        mIsGDenabled = false;
        ret = true;
        break;
    // For main face information
    case CAMERA_CMD_SET_MAIN_FACE_COORDINATE:
        MY_LOGD("Set main face : (%d, %d)", arg1, arg2);
        mMainFace_X = arg1;
        mMainFace_Y = arg2;
        mIsMainFaceEn = true;
        ret = true;
        break;
    case CAMERA_CMD_CANCEL_MAIN_FACE:
        MY_LOGD("Cancel main face");
        mIsMainFaceEn = false;
        ret = true;
        break;

    // For HR detection
#if '1' == MTKCAM_HR_MONITOR_SUPPORT
    case CAMERA_CMD_CHECKPARA_HR_PREVIEW:
        {
            int const i4CamMode = mpParamsMgr->getInt(MtkCameraParameters::KEY_CAMERA_MODE);
            if  ( i4CamMode != MtkCameraParameters::CAMERA_MODE_NORMAL ) {
                #undef FALSE
                #undef TRUE
                String8 const CameraHRDSupported = mpParamsMgr->getStr(MtkCameraParameters::KEY_HEARTBEAT_MONITOR);
                //MY_LOGD("Camera app Could do HRD? %s", CameraHRDSupported.c_str());
                //if(!CameraHRDSupported) {
                if(CameraHRDSupported.isEmpty()) {
                } else if(CameraHRDSupported == CameraParameters::FALSE) {
                    if(false != mIsHRSupported) {
                        mIsHRSupported = false;
                        MY_LOGD("callback notsupport HRD");
                        //stopHeartrateDetection();
                        //MY_LOGD("WillDBG callback notsupport HRD 1");
                        performHRCallback(NULL, HRD_NOTSUPPORT);
                    }
                } else if(CameraHRDSupported == CameraParameters::TRUE) {
                    if(true != mIsHRSupported) {
                        mIsHRSupported = true;
                        MY_LOGD("callback support HRD");
                        performHRCallback(NULL, HRD_HRDSUPPORT);
                    }
                }
            }
        }
        ret = true;
        break;

    case CAMERA_CMD_START_HR_PREVIEW:
        MY_LOGD("StartHR");
        {
            int const i4CamMode = mpParamsMgr->getInt(MtkCameraParameters::KEY_CAMERA_MODE);
            if  ( i4CamMode != MtkCameraParameters::CAMERA_MODE_NORMAL ) {
                #undef FALSE
                String8 const CameraHRDSupported = mpParamsMgr->getStr(MtkCameraParameters::KEY_HEARTBEAT_MONITOR);
                //MY_LOGD("Camera app Could do HRD? %s", CameraHRDSupported.c_str());
                //if(!CameraHRDSupported) {
                if(CameraHRDSupported.isEmpty() || CameraHRDSupported == CameraParameters::FALSE) {
                    ret = false;
                    break;
                }
            }
        }
        ret = startHeartrateDetection(1);
        mHRStarting = false;
        break;

    case CAMERA_CMD_STOP_HR_PREVIEW:
        MY_LOGD("StopHR");
        ret = stopHeartrateDetection();
        break;

    case CAMERA_CMD_SETCB_HR_PREVIEW:
    #if '1' == MTKCAM_HR_MONITOR_SUPPORT
        {
            MUINTPTR ptr;
            ptr = (MUINTPTR)(((MUINTPTR)((MUINT32)arg1)) + (((MUINTPTR)((MUINT32)arg2)) << 32));
            MY_LOGD("Set HR Callback : %X", ptr);
            MY_LOGD("Set HR Callback 1: %p", (HeartrateCallback_t)ptr);
            mHeartrateCb = (HeartrateCallback_t)ptr;
        }
    #endif
        break;

    case CAMERA_CMD_SETUSER_HR_PREVIEW:
        {
            MUINTPTR ptr;
            ptr = (MUINTPTR)(((MUINTPTR)((MUINT32)arg1)) + (((MUINTPTR)((MUINT32)arg2)) << 32));
            MY_LOGD("Set HR user : %X", ptr);
            mpHeartrateUser = (void *)ptr;
        }
        break;
    case CAMERA_CMD_SETMODE_HR_PREVIEW:
        mHRMode = arg1;
        MY_LOGD("Set HR mode : %d", mHRMode);
        ret = true;
        break;
#endif
    default:
        break;
    }

    MY_LOGD("-");

    return ret? OK : INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
startHeartrateDetection(int enable)
{
    MY_LOGD("+");
    bool ret = true;
#if '1' == MTKCAM_HR_MONITOR_SUPPORT
    //
    Mutex::Autolock _l(mModuleMtx);
    //
    MY_LOGD("isEnabledState(%d) +", isEnabledState());

    if(!enable)
        goto lbExit;

    //
    if ( !isEnabledState() )
    {
        if(mpImgBufPvdrClient == NULL) {
            MY_LOGE("Preview is not start!!, ");
            //::android_atomic_write(1, &mIsHRPreStarted);
            ret = false;
            goto lbExit;
        }
        ::android_atomic_write(1, &mIsHRStarted);
        ret = onStateChanged();
    }
    else
    {
        if(!isEnabledHR()) {
            ::android_atomic_write(1, &mIsHRStarted);
            ret = true;
        } else {
            MY_LOGW("HR is running");
            ret = false;
        }
    }
lbExit:
    //
#endif
    MY_LOGD("-");
    //
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
stopHeartrateDetection()
{
    MY_LOGD("+");
    bool ret = true;
#if '1' == MTKCAM_HR_MONITOR_SUPPORT
    //
    Mutex::Autolock _l(mModuleMtx);

    //
    MY_LOGD("isEnabledState(%d) +", isEnabledState());
    //

    //::android_atomic_write(0, &mIsHRPreStarted);

    if ( isEnabledHR() )
    {
        ::android_atomic_write(0, &mIsHRStarted);
        if(!isEnabledState()) {
            ret = onStateChanged();
        }
    }
    else
    {
        MY_LOGW("HR is not running");
        ret = false;
    }
    //
#endif
    MY_LOGD("-");
    //
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
startFaceDetection()
{
    MY_LOGD("+");
    bool ret = true;
    //
    Mutex::Autolock _l(mModuleMtx);
    //
    MY_LOGD("isEnabledState(%d) +", isEnabledState());

    //
    if ( !isEnabledState() )
    {
        ::android_atomic_write(1, &mIsFDStarted);
        ret = onStateChanged();
    }
    else
    {
        if(!isEnabledFD()) {
            ::android_atomic_write(1, &mIsFDStarted);
            ret = true;
        } else {
            MY_LOGW("FD is running");
            ret = false;
        }
    }
    //
    MY_LOGD("-");
    //
lbExit:
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
stopFaceDetection()
{
    MY_LOGD("+");
    bool ret = true;
    //
    Mutex::Autolock _l(mModuleMtx);

    //
    MY_LOGD("isEnabledState(%d) +", isEnabledState());
    //
    //
    if ( isEnabledFD() )
    {
        ::android_atomic_write(0, &mIsFDStarted);
        if(!isEnabledState()) {
            ret = onStateChanged();
        }
    }
    else
    {
        MY_LOGW("FD was not running");
        ret = false;
    }
    //
    MY_LOGD("-");
    //
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
stopPreview()
{
    //stopHeartrateDetection();
    performHRCallback(NULL, HRD_STOPPREV);
    return stopFaceDetection();
}


/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::takePicture()
{
    //stopHeartrateDetection();
    performHRCallback(NULL, HRD_TAKEPIC);
    //return stopFaceDetection();
    return true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
startPreview()
{
    performHRCallback(NULL, HRD_STARTPREV);
    return true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
startRecording()
{
    performHRCallback(NULL, HRD_STARTREC);
    return true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
stopRecording()
{
    performHRCallback(NULL, HRD_STOPREC);
    return true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
isEnabledState()
{
    return  (0 != ::android_atomic_release_load(&mIsFDStarted))||(0 != ::android_atomic_release_load(&mIsHRStarted));
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
isEnabledFD()
{
    return  (0 != ::android_atomic_release_load(&mIsFDStarted));
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
isEnabledHR()
{
    return  (0 != ::android_atomic_release_load(&mIsHRStarted));
}

/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
performHRCallback(void *HR_result, int type)
{
    bool ret = true;
#if '1' == MTKCAM_HR_MONITOR_SUPPORT
    if(mHeartrateCb && mpHeartrateUser) {
        HR_detection_result_t result;
        HR_RESULT* AlgoResult = (HR_RESULT *)HR_result;

        if(HR_result == NULL) {
            switch (type) {
            case HRD_NOTSUPPORT:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_NOTSUPPORTMODE;
                break;
            case HRD_READY:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_HRDREADY;
                break;
            case HRD_STARTPREV:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_STARTPREVIEW;
                break;
            case HRD_STOPPREV:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_STOPPREVIEW;
                break;
            case HRD_TAKEPIC:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_TAKEPICTURE;
                break;
            case HRD_STARTREC:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_STARTRECORD;
                break;
            case HRD_STOPREC:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_STOPRECORD;
                break;
            case HRD_HRDSUPPORT:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_HRDSUPPORTMODE;
                break;
            default:
                result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_NOTSUPPORTMODE;
                break;
            }
            result.waveform = NULL;
            mHeartrateCb(result, mpHeartrateUser);
            return ret;
        }

        result.rect = NSCam::MRect(NSCam::MPoint(AlgoResult->x1, AlgoResult->y1),
                                   NSCam::MPoint(AlgoResult->x2, AlgoResult->y2));

        result.confidence = AlgoResult->quality;
        result.facenum = mpDetectedFaces->number_of_faces;
        result.heartbeats = AlgoResult->value;
        result.isvalid = AlgoResult->isvalid;
        result.percentage = AlgoResult->percentage;
        result.stoptype = AlgoResult->stoptype;
        result.waveform = AlgoResult->aiWaveform;
        result.prev_w = AlgoResult->prev_w;
        result.prev_h = AlgoResult->prev_h;
        result.facing = AlgoResult->facing;

        result.ReplyType = HEARTRATE_DEVICE_REPLYTYPE_RESULT;
        mHeartrateCb(result, mpHeartrateUser);
    }
#endif

    return ret;
}




/******************************************************************************
 *
 ******************************************************************************/
bool
FDClient::
//performCallback(bool isDetected_FD, bool isDetected_SD)
performCallback(bool isDetected_FD, bool isDetected_SD, bool isDetected_GD, int ASDScene)
{
    bool ret = true;
    bool performCB = false;
    CMD_Q_T cmd;

    cmd.isFDCB = 0;
    cmd.isSDCB = 0;
    cmd.isGSCB = 0;
    cmd.isASDCB = 0;
    cmd.isStopCmd = 0;
    // (1) FD callback:
    //     (-) always do whenever face is detected
    //     (-) do when face is not detected, but last time was detected
    //     (-) otherwise, do no-op
    bool performFDcb = isDetected_FD ? true : mIsDetected_FD ? true : false;

    if (performFDcb && isMsgEnabled())
    {
        cmd.FrameMeta.number_of_faces = mpDetectedFaces->number_of_faces;
        memcpy(cmd.Faces, mpDetectedFaces->faces, sizeof(camera_face_t)*cmd.FrameMeta.number_of_faces);
        cmd.isFDCB = true;
        performCB = true;
    }
    else
    {
        ret = false;
        MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "No FD CB: isDetected_FD(%d), mIsDetected_FD(%d), isMsgEnabled(%d)", isDetected_FD, mIsDetected_FD, isMsgEnabled());
    }

    // (2) SD callback:

     mIsDetected_SD = isDetected_SD;

    bool performSDcb = mIsDetected_SD && mIsSDenabled;
    if (performSDcb && isMsgEnabled())
    {
        cmd.isSDCB = true;
        performCB = true;
    }
    else
    {
        ret = false;
        MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "No SD CB: isDetected_SD(%d), mIsSDenabled(%d), isMsgEnabled(%d)", mIsDetected_SD, mIsSDenabled, isMsgEnabled());
    }

    // (3) GD callback:

    mIsDetected_GD = isDetected_GD;

    bool performGDcb = mIsDetected_GD && mIsGDenabled;
    if (performGDcb && isMsgEnabled())
    {
        MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "GD CB: isDetected_GD(%d), mIsGDenabled(%d), isMsgEnabled(%d)", mIsDetected_GD, mIsGDenabled, isMsgEnabled());
        cmd.isGSCB = true;
        performCB = true;
    }
    else
    {
        ret = false;
        MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "No GD CB: isDetected_GD(%d), mIsGDenabled(%d), isMsgEnabled(%d)", mIsDetected_GD, mIsGDenabled, isMsgEnabled());
    }

    if((ASDScene != -1) && isMsgEnabled()) {
        MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "ASD CB: Scene(%d)", ASDScene);
        cmd.isASDCB = true;
        cmd.ASDScene = ASDScene;
        performCB = true;
    } else {
        ret = false;
        MY_LOGD_IF(ENABLE_LOG_PER_FRAME, "No ASD CB: Scene(%d), isMsgEnabled(%d)", ASDScene, isMsgEnabled());
    }

    if (isEnabledFD() && performCB) {
        int isPost = false;
        isPost = mpCBClient->postCommand(cmd);
        if(isPost) {
            mIsDetected_FD = isDetected_FD;
        }
    }

    return ret;
}

