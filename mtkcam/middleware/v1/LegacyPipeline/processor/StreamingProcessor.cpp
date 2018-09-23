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

#define LOG_TAG "MtkCam/StreamingProcessor"
//
#include <sys/prctl.h>
#include <sys/resource.h>
#include <system/thread_defs.h>
//
#include "MyUtils.h"
#include <mtkcam/middleware/v1/LegacyPipeline/processor/StreamingProcessor.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/fwk/MtkCamera.h>
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>

#if MTKCAM_HAVE_AEE_FEATURE == 1
#include <aee.h>
#define AEE_ASSERT(String) \
    do { \
        CAM_LOGE("ASSERT("#String") fail"); \
        aee_system_exception( \
            #String, \
            NULL, \
            DB_OPT_DEFAULT, \
            String); \
    } while(0)
#else
#define AEE_ASSERT(String) \
    do { \
        CAM_LOGE("ASSERT("#String") fail"); \
    } while(0)

#endif

//
using namespace android;
using namespace NSCam;
using namespace NSCam::v1;

/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%d:%s] " fmt, getOpenId(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
//
#define MY_LOGD1(...)               MY_LOGD_IF((mLogLevel>=1),__VA_ARGS__)
#define MY_LOGD2(...)               MY_LOGD_IF((mLogLevel>=2),__VA_ARGS__)
#define MY_LOGD3(...)               MY_LOGD_IF((mLogLevel>=3),__VA_ARGS__)
//
#define FUNC_START                  MY_LOGD1("+")
#define FUNC_END                    MY_LOGD1("-")
//
/******************************************************************************
 *
 ******************************************************************************/
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata metadata,
    MUINT32 const tag,
    T & rVal
)
{
    IMetadata::IEntry entry = metadata.entryFor(tag);
    if( !entry.isEmpty() ) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    return MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/

class StreamingProcessorImp
    : public StreamingProcessor
    , public IRequestCallback
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IListener Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual void                onResultReceived(
                                    MUINT32         const requestNo,
                                    StreamId_T      const streamId,
                                    MBOOL           const errorResult,
                                    IMetadata       const result
                                );

    virtual void                onFrameEnd(
                                    MUINT32         const /*requestNo*/
                                ) {};

    virtual String8             getUserName();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IDataListener Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    virtual void                onMetaReceived(
                                    MUINT32         const requestNo,
                                    StreamId_T      const streamId,
                                    MBOOL           const errorResult,
                                    IMetadata       const result
                                );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  RefBase Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual void                onLastStrongRef( const void* /*id*/);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  StreamingProcessor Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual status_t            startAutoFocus();

    virtual status_t            cancelAutoFocus();

    virtual status_t            preCapture(int& flashRequired, nsecs_t tTimeout);

    virtual status_t            startSmoothZoom(int value);

    virtual status_t            stopSmoothZoom();

    virtual status_t            sendCommand(
                                    int32_t cmd,
                                    int32_t arg1, int32_t arg2
                                );

    virtual void                dump();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IRequestCallback Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    virtual void                RequestCallback(
                                    uint32_t requestNo,
                                    MINT32   type,
                                    MINTPTR  _ext1,
                                    MINTPTR  _ext2
                                );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.
                                    StreamingProcessorImp(
                                        MINT32                            openId,
                                        sp<INotifyCallback>         const &rpCamMsgCbInfo,
                                        wp< RequestSettingBuilder > const &rpRequestSettingBuilder,
                                        sp< IParamsManagerV3 >      const &rpParamsManagerV3
                                    );

                                    ~StreamingProcessorImp();

            void                    doNotifyCb(
                                        uint32_t requestNo,
                                        int32_t _msgType,
                                        int32_t _ext1,
                                        int32_t _ext2,
                                        int32_t _ext3
                                    );

            bool                    needFocusCallback(StreamId_T const streamId);

            bool                    needZoomCallback(
                                        MUINT32 requestNo,
                                        MINT32  &zoomIndex
                                    );

            bool                    isAfCallback(
                                        MUINT8  afState,
                                        MINT&   msg,
                                        MINT&   msgExt
                                    );

            void                    checkMultiZoneAfWindow(IMetadata const result);

          MINT32                    getOpenId() const       { return mOpenId; }

public:     ////                    Definitions.

    typedef KeyedVector< MUINT32, MUINT32 > Que_T;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Data Members.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    sp<INotifyCallback>             mpCamMsgCbInfo;
    sp< IParamsManagerV3 >          mpParamsManagerV3;
    wp< RequestSettingBuilder >     mpRequestSettingBuilder;

protected: ////                     Logs.
    MINT32                          mLogLevel;
    MBOOL                           isSupportMultiZonAfWindow;
    MINT32                          mOpenId;

protected: ////                     3A information.
    MUINT8                          mAestate;
    MUINT8                          mAfstate;
    MUINT8                          mAfstateLastCallback;
    mutable Mutex                   m3ALock;

protected: ////                     zoom callback information.
    MUINT32                         mZoomTargetIndex;
    Que_T                           mZoomResultQueue;
    mutable Mutex                   mZoomLock;
    MBOOL                           mZoomTriggerd;
    mutable Condition               mWaitZoomLock;

protected: ////                     af callback information.
    MBOOL                           mListenAfResult;
    MBOOL                           mNeedAfCallback;
    MUINT32                         mAfRequest;
    mutable Mutex                   mAutoFocusLock;

protected: ////                     ae information for preCapture.
    MBOOL                           mListenPreCapture;
    MBOOL                           mListenPreCaptureStart;
    MBOOL                           mListenAfResultPreCapture;
    MBOOL                           mListenAeResult;
    mutable Mutex                   mPreCaptureLock;
    Condition                       mPreCaptureCond;
    MBOOL                           mbAeFlashRequired;
    MBOOL                           mbUpdateAf;

    MUINT32                         mAeRequest;
    MBOOL                           mSkipCheckAeRequestNumber;
protected:
    MBOOL                           mUninit;
private:
    MRect                           mActiveArray;
    MINT                            lastAFCBMsg;
    MINT                            lastAFCBExt;

};


/******************************************************************************
 *
 ******************************************************************************/
sp< StreamingProcessor >
StreamingProcessor::
createInstance(
    MINT32                            openId,
    sp<INotifyCallback>         const &rpCamMsgCbInfo,
    wp< RequestSettingBuilder > const &rpRequestSettingBuilder,
    sp< IParamsManagerV3 >      const &rpParamsManagerV3
)
{
    return new StreamingProcessorImp(
                    openId,
                    rpCamMsgCbInfo,
                    rpRequestSettingBuilder,
                    rpParamsManagerV3
               );
}

/******************************************************************************
 *
 ******************************************************************************/
StreamingProcessorImp::
StreamingProcessorImp(
    MINT32                            openId,
    sp<INotifyCallback>         const &rpCamMsgCbInfo,
    wp< RequestSettingBuilder > const &rpRequestSettingBuilder,
    sp< IParamsManagerV3 >      const &rpParamsManagerV3
)
    : mpCamMsgCbInfo(rpCamMsgCbInfo)
    , mpParamsManagerV3(rpParamsManagerV3)
    , mpRequestSettingBuilder(rpRequestSettingBuilder)
    , mOpenId(openId)
    //
    , mAfstate(0)
    , mAfstateLastCallback(0)
    //
    , mZoomTriggerd(MFALSE)
    //
    , mListenAfResultPreCapture(MFALSE)
    , mListenAeResult(MFALSE)
    , mbUpdateAf(MFALSE)
    , mUninit(false)
    //
    , lastAFCBMsg(0)
    , lastAFCBExt(0)
{
    mLogLevel = ::property_get_int32("debug.camera.log", 0);
    if ( mLogLevel == 0 ) {
        mLogLevel = ::property_get_int32("debug.camera.log.SProcessor", 0);
    }
    // get multiAF window support
    MINT32 value;
    value = ::property_get_int32("debug.camera.multizoneaf", -1);
    if ( value != 1 )
    {
        isSupportMultiZonAfWindow = MFALSE;
    }
    else
    {
        isSupportMultiZonAfWindow = MTRUE;
    }
    //
    // get sensor active area size
    sp<IMetadataProvider> pMetadataProvider = NSMetadataProviderManager::valueFor(openId);
    IMetadata static_meta = pMetadataProvider->geMtktStaticCharacteristics();
    IMetadata::IEntry activeA = static_meta.entryFor(MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION);
    if( !activeA.isEmpty() )
    {
        mActiveArray = activeA.itemAt(0, Type2Type<MRect>());
    }
}


/******************************************************************************
 *
 ******************************************************************************/
StreamingProcessorImp::
~StreamingProcessorImp()
{
    MY_LOGI("+");
    mPreCaptureCond.signal();
}

/******************************************************************************
 *
 ******************************************************************************/
void
StreamingProcessorImp::
onLastStrongRef(const void* /*id*/)
{
    MY_LOGI("lastAFCBMsg(%d), lastAFCBExt(%d)", lastAFCBMsg, lastAFCBExt);

    // if last callback is focus moving, make sure send moving stop at last time
    if (lastAFCBMsg == CAMERA_MSG_FOCUS_MOVE && lastAFCBExt == 1)
    {
        MY_LOGI("Send Focus stoped");
        doNotifyCb(
                0,
                CAMERA_MSG_FOCUS_MOVE,
                0,
                0,
                0);
    }

    mUninit = true;
}

/******************************************************************************
 *
 ******************************************************************************/
void
StreamingProcessorImp::
onMetaReceived(
    MUINT32         const requestNo,
    StreamId_T      const /*streamId*/,
    MBOOL           const errorResult,
    IMetadata       const result
)
{
    // precapture AF
    {
        Mutex::Autolock _l(mPreCaptureLock);
        if (mListenPreCapture && mListenPreCaptureStart && mListenAfResultPreCapture) {
            MUINT8 curAfstate = 0xFF;
            if ( !errorResult && !tryGetMetadata< MUINT8 >(result, MTK_CONTROL_AF_STATE, curAfstate) ) {
                //MY_LOGW("AF state not update.");
            }
            MY_LOGD_IF( mLogLevel >= 1, "requestNo:(%d) curAfstate %d", requestNo, curAfstate);;
            if (curAfstate == MTK_CONTROL_AF_STATE_FOCUSED_LOCKED  ||
                curAfstate == MTK_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED)
            {
                mListenAfResultPreCapture = MFALSE;
            }

            if (!mListenAeResult && !mListenAfResultPreCapture) {
                mListenPreCapture = MFALSE;
                mPreCaptureCond.signal();
            }
        }
    }
}

/******************************************************************************
 *
 ******************************************************************************/
void
StreamingProcessorImp::
onResultReceived(
    MUINT32         const requestNo,
    StreamId_T      const streamId,
    MBOOL           const errorResult,
    IMetadata       const result
)
{
    MY_LOGD2("reqNo %d, errRes %d",
            requestNo,
            errorResult);
    // update request number
    {
        Mutex::Autolock _l(mAutoFocusLock);
        mListenAfResult = ( requestNo == mAfRequest ) ? MTRUE : mListenAfResult;
    }
    //
    if ( errorResult )
    {
        if (requestNo == mAeRequest)
        {
            MY_LOGW("mAeRequest(%d) had errorResult", mAeRequest);
            mSkipCheckAeRequestNumber = MTRUE;
        }
        return;
    }
    // update 3A
    {
        Mutex::Autolock _l(m3ALock);
        MUINT8 curAfstate = 0xFF;
        tryGetMetadata< MUINT8 >(result, MTK_CONTROL_AF_STATE, curAfstate);
        tryGetMetadata< MUINT8 >(result, MTK_CONTROL_AE_STATE, mAestate);
        //
        if(curAfstate != 0xFF)
        {
            if( mbUpdateAf==MFALSE)
            {
                mAfstateLastCallback = curAfstate;
            }
            mbUpdateAf = MTRUE;
            mAfstate = curAfstate;
        }
    }
    //
    // update AE/AF state for PreCapture
    {
        Mutex::Autolock _l(mPreCaptureLock);
        if (mListenPreCapture) {
            MY_LOGD1("requestNo:(%d,%d) mAestate: %d, mAfstate %d", requestNo, mAeRequest, mAestate, mAfstate);
            MY_LOGD1("mListenAeResult: %d, mListenAfResultPreCapture %d", mListenAeResult, mListenAfResultPreCapture);

            // check if starting to receive AE and AF results
            if ((mListenAeResult || mListenAfResultPreCapture)
                && (requestNo == mAeRequest || mSkipCheckAeRequestNumber))
            {
                    mListenPreCaptureStart = MTRUE;
            }

            if (mListenPreCaptureStart) {
                if (mListenAeResult) {
                    MY_LOGD1("requestNo:(%d,%d) mAestate: %d", requestNo, mAeRequest, mAestate);
                    if (mAestate == MTK_CONTROL_AE_STATE_CONVERGED || mAestate == MTK_CONTROL_AE_STATE_FLASH_REQUIRED)
                    {
                        mbAeFlashRequired = (mAestate == MTK_CONTROL_AE_STATE_FLASH_REQUIRED ? MTRUE : MFALSE);
                        mListenAeResult = MFALSE;
                    }
                }

                /*if (mListenAfResultPreCapture)
                {
                    MY_LOGD1("requestNo:%d mAfstate: %d", requestNo, mAfstate);
                    if (mAfstate == MTK_CONTROL_AF_STATE_FOCUSED_LOCKED  ||
                        mAfstate == MTK_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED)
                    {
                        mListenAfResultPreCapture = MFALSE;
                    }
                }*/

                if (!mListenAeResult && !mListenAfResultPreCapture) {
                    mListenPreCapture = MFALSE;
                    mPreCaptureCond.signal();
                }
            }
        }
    }

    // check focus callback
    if ( needFocusCallback(streamId) )
    {
        MINT msg = 0, msgExt = 0;
        if(isAfCallback(mAfstate, msg, msgExt))
        {
            // do multi af window callback
            //if(isSupportMultiZonAfWindow)
            {
                checkMultiZoneAfWindow(result);
            }
            //
            doNotifyCb(
                requestNo,
                msg,
                msgExt,
                0,
                0);
            lastAFCBMsg = msg;
            lastAFCBExt = msgExt;
        }
    }
    // check zoom callback
    MINT32 zoomIndex = 0;
    if ( needZoomCallback( requestNo, zoomIndex ) ) {
        doNotifyCb(
            requestNo,
            CAMERA_MSG_ZOOM,
            zoomIndex, 0, 0
        );
    }
}

/******************************************************************************
 *
 ******************************************************************************/
void
StreamingProcessorImp::
RequestCallback(
    uint32_t requestNo,
    MINT32   _type,
    MINTPTR  _ext1,
    MINTPTR  _ext2
)
{
    MY_LOGD("requestNo:%d _msgType:%d %d %d", requestNo, _type, _ext1, _ext2);
    switch(_type)
    {
        case IRequestCallback::MSG_START_AUTOFOCUS:
        {
            Mutex::Autolock _l(mAutoFocusLock);
            mAfRequest = requestNo;
            mNeedAfCallback = MTRUE;
        } break;
        case IRequestCallback::MSG_START_ZOOM:
        {
            Mutex::Autolock _l(mZoomLock);
            mZoomResultQueue.add(_ext1, requestNo);
        } break;
        case IRequestCallback::MSG_START_PRECAPTURE:
        {
            Mutex::Autolock _l(mPreCaptureLock);
            MY_LOGD("without af %d", requestNo);

            mAeRequest                = requestNo;
            mListenPreCapture         = MTRUE;
            mListenPreCaptureStart    = MFALSE;
            mListenAeResult           = MTRUE;
            mListenAfResultPreCapture = MFALSE;
            mSkipCheckAeRequestNumber = MFALSE;
        } break;
        case IRequestCallback::MSG_START_PRECAPTURE_WITH_AF:
        {
            Mutex::Autolock _l(mPreCaptureLock);
            MY_LOGD("with af %d", requestNo);

            mAeRequest                = requestNo;
            mListenPreCapture         = MTRUE;
            mListenPreCaptureStart    = MFALSE;
            mListenAeResult           = MTRUE;
            mListenAfResultPreCapture = MTRUE;
            mSkipCheckAeRequestNumber = MFALSE;
        } break;
        default:
            MY_LOGE("Unsupported message type %d", _type);
        break;
    };
}

/******************************************************************************
 *
 ******************************************************************************/
String8
StreamingProcessorImp::
getUserName()
{
    return String8::format("StreamingProcessor");
}

/******************************************************************************
 *
 ******************************************************************************/
bool
StreamingProcessorImp::
needFocusCallback(StreamId_T const streamId)
{
    Mutex::Autolock _l(mAutoFocusLock);
    //
    if (streamId != eSTREAMID_META_HAL_DYNAMIC_P1)
    {
        return false;
    }
    if( ::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FOCUS_MODE), CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO) == 0 ||
        ::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FOCUS_MODE), CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE) == 0)
    {
        return true;
    }
    else
    {
        MY_LOGD2("%d/%d",mNeedAfCallback,mListenAfResult);
        return ( mNeedAfCallback && mListenAfResult );
    }
}

/******************************************************************************
 *
 ******************************************************************************/
bool
StreamingProcessorImp::
needZoomCallback(
    MUINT32 requestNo,
    MINT32  &zoomIndex
)
{
    Mutex::Autolock _l(mZoomLock);
    for ( size_t i = 0; i < mZoomResultQueue.size(); ++i ) {
        if ( mZoomResultQueue.valueAt(i) == requestNo ) {
            zoomIndex = mZoomResultQueue.keyAt(i);
            return true;
        }
    }
    return false;
}
/******************************************************************************
 *
 ******************************************************************************/
void
StreamingProcessorImp::
doNotifyCb(
    uint32_t requestNo,
    int32_t _msgType,
    int32_t _ext1,
    int32_t _ext2,
    int32_t /*_ext3*/
)
{
    int32_t msg1 = _ext1;
    int32_t msg2 = _ext2;
    //
    switch (_msgType)
    {
        case CAMERA_MSG_FOCUS:
        {
            Mutex::Autolock _l(mAutoFocusLock);
            //
            MY_LOGD("CAMERA_MSG_FOCUS requestNo:%d _msgType:%d msg:%d,%d",
                    requestNo,
                    _msgType,
                    msg1,
                    msg2);
            mNeedAfCallback = MFALSE;
            mListenAfResult = MFALSE;
        } break;
        case CAMERA_MSG_FOCUS_MOVE:
        {
            MY_LOGD("CAMERA_MSG_FOCUS_MOVE requestNo:%d _msgType:%d msg:%d,%d",
                    requestNo,
                    _msgType,
                    msg1,
                    msg2);
        } break;
        case CAMERA_MSG_ZOOM:
        {
            Mutex::Autolock _l(mZoomLock);
            //
            mZoomResultQueue.removeItem(msg1);
            msg2 = (mUninit) || (msg1 == (MINT32)mZoomTargetIndex) || (!mZoomTriggerd);
            MY_LOGD("smoothZoom requestNo:%d (%d, %d) target:%d uninit:%d",
                requestNo, msg1, msg2, mZoomTargetIndex, mUninit
            );
            if (msg2 == 1) //last zoom
            {
                mZoomTriggerd = MFALSE;
            }
            //
            mpParamsManagerV3->getParamsMgr()->set(
                CameraParameters::KEY_ZOOM,
                msg1
            );
            mWaitZoomLock.signal();
        } break;
        default:
            MY_LOGE("Unsupported message type %d", _msgType);
            return;
        break;
    };
    //
    mpCamMsgCbInfo->doNotifyCallback(
        _msgType,
        msg1,
        msg2
    );
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StreamingProcessorImp::
startAutoFocus()
{
    FUNC_START;
    //
    MUINT8 afState = 0;
    MUINT8 afMode  = 0;
    String8 const s = mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FOCUS_MODE);
    if  ( !s.isEmpty() ) {
        afMode = PARAMSMANAGER_MAP_INST(IParamsManager::eMapFocusMode)->valueFor(s);
    } else {
        MY_LOGW("cannot get focus mode from parameter.");
    }

    MY_LOGD1("afMode %d", afMode );

    {
        Mutex::Autolock _l(m3ALock);
        afState = mAfstate;
    }
    //
    {
        IParamsManager::IMap const* focusMap = IParamsManager::getMapInst(IParamsManager::int2type<IParamsManager::eMapFocusMode>());
        /**
          * If the camera does not support auto-focus, it is a no-op and
          * onAutoFocus(boolean, Camera) callback will be called immediately
          * with a fake value of success set to true.
          *
          * Similarly, if focus mode is set to INFINITY, there's no reason to
          * bother the HAL.
          */
        if ( focusMap->valueFor( String8(CameraParameters::FOCUS_MODE_FIXED) ) == afMode ||
             focusMap->valueFor( String8(CameraParameters::FOCUS_MODE_INFINITY) ) == afMode) {
            if ( mpCamMsgCbInfo->msgTypeEnabled(CAMERA_MSG_FOCUS) ) {
                mpCamMsgCbInfo->doNotifyCallback(CAMERA_MSG_FOCUS, 1, 0);
                //
                MY_LOGD1("afMode %d", afMode);
                //
                FUNC_END;
                return OK;
            }
        }

        /**
         * If we're in CAF mode, and AF has already been locked, just fire back
         * the callback right away; the HAL would not send a notification since
         * no state change would happen on a AF trigger.
         */
        if ( ( afMode == focusMap->valueFor( String8(CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE) ) ||
               afMode == focusMap->valueFor( String8(CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO) ) ) &&
               afState == ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED ) {
            if ( mpCamMsgCbInfo->msgTypeEnabled(CAMERA_MSG_FOCUS) ) {
                mpCamMsgCbInfo->doNotifyCallback(CAMERA_MSG_FOCUS, 1, 0);
                //
                MY_LOGD1("afMode %d", afMode);
                //
                FUNC_END;
                return OK;
            }
        }
    }

    sp< RequestSettingBuilder > builder = mpRequestSettingBuilder.promote();
    if ( builder != 0 ) {
        builder->triggerAutofocus(static_cast< IRequestCallback* >(this));
    } else {
        MY_LOGW("builder cannot be promoted.");
    }

    FUNC_END;
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StreamingProcessorImp::
cancelAutoFocus()
{
    FUNC_START;

    {
        Mutex::Autolock _l(mAutoFocusLock);
        mNeedAfCallback = MFALSE;
        mListenAfResult = MFALSE;
    }
    //
    MUINT8 afMode = 0;
    String8 const s = mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FOCUS_MODE);
    if  ( !s.isEmpty() ) {
        afMode = PARAMSMANAGER_MAP_INST(IParamsManager::eMapFocusMode)->valueFor(s);
    }
    // Canceling does nothing in FIXED or INFINITY modes
    IParamsManager::IMap const* focusMap = IParamsManager::getMapInst(IParamsManager::int2type<IParamsManager::eMapFocusMode>());
    if ( focusMap->valueFor( String8(CameraParameters::FOCUS_MODE_FIXED) ) == afMode ||
         focusMap->valueFor( String8(CameraParameters::FOCUS_MODE_INFINITY) ) == afMode) {
        //
        MY_LOGD1("afMode %d", afMode);
        //
        FUNC_END;
        return OK;
    }

    sp< RequestSettingBuilder > builder = mpRequestSettingBuilder.promote();
    if ( builder != 0 ) {
        builder->triggerCancelAutofocus();
    } else {
        MY_LOGW("builder cannot be promoted.");
    }

    FUNC_END;
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StreamingProcessorImp::
preCapture(int& flashRequired, nsecs_t tTimeout)
{
    CAM_TRACE_CALL();

    FUNC_START;

    sp< RequestSettingBuilder > builder = mpRequestSettingBuilder.promote();
    if ( builder == 0 )
    {
        MY_LOGW("builder cannot be promoted.");
        return OK;
    }

    {
        MERROR res;
        String8 pFocusMode = String8(mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FOCUS_MODE));
        MY_LOGD("FocusMode = %s", pFocusMode.string());

        MBOOL isSupportAF = mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS) == 0 ? MFALSE :
                           !strcmp(pFocusMode, String8(CameraParameters::FOCUS_MODE_INFINITY)) ? MFALSE :
                           !strcmp(pFocusMode, String8(CameraParameters::FOCUS_MODE_FIXED)) ? MFALSE :
                           !strcmp(pFocusMode, String8("manual")) ? MFALSE : MTRUE;

        const char *pFlashMode = mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FLASH_MODE);
        #if 0
        MBOOL needAf = isSupportAF & !(!strcmp(pFlashMode, CameraParameters::FLASH_MODE_ON) && mAestate == MTK_CONTROL_AE_STATE_CONVERGED);
        #else
        MBOOL needAf = isSupportAF;
        #endif
        MY_LOGD("FlashMode = %s aeState = %d isSupportAF = %d needAf = %d", pFlashMode, mAestate, isSupportAF, needAf);

        builder->triggerPrecaptureMetering(static_cast< IRequestCallback* >(this), needAf);
        Mutex::Autolock _l(mPreCaptureLock);
        MY_LOGI("preCapture tTimeout(%lld)",tTimeout);
        if ( tTimeout != 0LL)
        {
            res = mPreCaptureCond.waitRelative(mPreCaptureLock, tTimeout);
            if (OK != res)
            {
                dump();
                AEE_ASSERT("CRDISPATCH_KEY:MtkCam: precapture timeout");
            }
        }
        else
        {
            mPreCaptureCond.wait(mPreCaptureLock);
        }
        MY_LOGD1("AE is already in good state, mbAeFlashRequired %d", mbAeFlashRequired);
        flashRequired = (mbAeFlashRequired == MTRUE);
        mpParamsManagerV3->setCancelAF(!mpParamsManagerV3->getAfTriggered());
    }

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StreamingProcessorImp::
startSmoothZoom(int value)
{
    FUNC_START;
    //
    MY_LOGD_IF( 1, "startSmoothZoom %d", value );
    //
    int currentIndex = 0;
    int inc = 0;
    {
        Mutex::Autolock _l(mZoomLock);
        mZoomTargetIndex = value;
        currentIndex = mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_ZOOM);
        if( value < 0 || value > mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_MAX_ZOOM) )
        {
            MY_LOGE("return fail: smooth zoom(%d)", value);
            return BAD_VALUE;
        }

        if( value == currentIndex )
        {
            MY_LOGE("smooth zoom(%d) equals to current", value);
            return OK;
        }

        inc = ( (MINT32)mZoomTargetIndex > currentIndex ) ? 1 : -1;
    }

    sp< RequestSettingBuilder > builder = mpRequestSettingBuilder.promote();
    if ( builder != 0 ) {
        for ( currentIndex += inc ; currentIndex != value + inc; currentIndex += inc ) {
            MY_LOGD1("triggerTriggerZoom %d/%d", currentIndex, value );
            builder->triggerTriggerZoom( currentIndex, static_cast< IRequestCallback* >(this) );
            mZoomTriggerd = MTRUE;
        }
    } else {
        MY_LOGW("builder cannot be promoted.");
    }
    //
    FUNC_END;
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
StreamingProcessorImp::
stopSmoothZoom()
{
    FUNC_START;
    //
    if (mZoomTriggerd)
    {
        mZoomTriggerd = MFALSE;
        MY_LOGD("wait zoom callback stop+");
        Mutex::Autolock _l(mZoomLock);
        mWaitZoomLock.wait(mZoomLock);
        MY_LOGD("wait zoom callback stop -");
    }
    sp< RequestSettingBuilder > builder = mpRequestSettingBuilder.promote();
    if ( builder != 0 ) {
        builder->triggerCancelZoom();
    } else {
        MY_LOGW("builder cannot be promoted.");
    }
    //
    {
        Mutex::Autolock _l(mZoomLock);
        if(mZoomResultQueue.size()>0)
        {
            mZoomTargetIndex = mZoomResultQueue.valueAt(mZoomResultQueue.size() - 1);
        }
    }
    //
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
StreamingProcessorImp::
isAfCallback(
    MUINT8  afState,
    MINT&   msg,
    MINT&   msgExt)
{
    bool bAfCallback = false;
    msgExt = 0;
    //
    if(!mbUpdateAf)
    {
        MY_LOGD1("No AF update");
        return bAfCallback;
    }
    //
    switch(afState)
    {
        case MTK_CONTROL_AF_STATE_INACTIVE:
        case MTK_CONTROL_AF_STATE_ACTIVE_SCAN:
        {
            //No need to callback
            break;
        }
        case MTK_CONTROL_AF_STATE_FOCUSED_LOCKED:
        case MTK_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED:
        {
            if( afState == MTK_CONTROL_AF_STATE_FOCUSED_LOCKED )
            {
                msgExt = 1;
            }
            bAfCallback = true;
            msg = CAMERA_MSG_FOCUS;
            break;
        }
        case MTK_CONTROL_AF_STATE_PASSIVE_SCAN:
        case MTK_CONTROL_AF_STATE_PASSIVE_FOCUSED:
        case MTK_CONTROL_AF_STATE_PASSIVE_UNFOCUSED:
        {
            MBOOL doCb = MTRUE;
            if( afState == MTK_CONTROL_AF_STATE_PASSIVE_SCAN )
            {
                msgExt = 1;
            }
            // if current status is passive focused/unfocused,
            // and last status is not passive scan,
            // no need to do callback
            else if(mAfstateLastCallback != MTK_CONTROL_AF_STATE_PASSIVE_SCAN)
            {
                doCb = MFALSE;
            }
            //
            if ( doCb && mpCamMsgCbInfo->msgTypeEnabled( CAMERA_MSG_FOCUS_MOVE ) )
            {
                bAfCallback = true;
                msg = CAMERA_MSG_FOCUS_MOVE;
            }
            else
            {
                MY_LOGD2("CAMERA_MSG_FOCUS_MOVE has been disable or doCb(%d)", doCb);
            }
            break;
        }

        default:
        {
            MY_LOGW("Un-process af state %d",afState);
            break;
        }
    }
    //
    if(mAfstateLastCallback != afState)
    {
        MY_LOGI("AFstate(%d -> %d), msg(%d), msgExt(%d), AfCb(%d)",
                mAfstateLastCallback,
                afState,
                msg,
                msgExt,
                bAfCallback);
        //
        mAfstateLastCallback = afState;
    }
    else
    {
        bAfCallback = false;
    }
    //
    return bAfCallback;
}

status_t
StreamingProcessorImp::
sendCommand(
    int32_t /*cmd*/,
    int32_t /*arg1*/,
    int32_t /*arg2*/
)
{
    return INVALID_OPERATION;
}

void
StreamingProcessorImp::
checkMultiZoneAfWindow(IMetadata const result)
{
    /*
    * MTK_FOCUS_AREA_POSITION
    * MINT32
    * X1, Y1, X2, Y2 ...
    *
    * MTK_FOCUS_AREA_SIZE
    * MSize
    * (w, h)
    *
    * MTK_FOCUS_AREA_RESULT
    * MUINT8
    * res1, res2, ...
    */

    /**************************************************************
    * Get informaiton from metadata
    ***************************************************************/
    IMetadata::IEntry entryPosition = result.entryFor(MTK_FOCUS_AREA_POSITION);
    IMetadata::IEntry entrySize     = result.entryFor(MTK_FOCUS_AREA_SIZE);
    IMetadata::IEntry entryResult   = result.entryFor(MTK_FOCUS_AREA_RESULT);
    // how many af window
    MUINT windowCount = 0;
    // no update in multi-zone in meta, meta will be empty
    if( entryPosition.isEmpty() || entrySize.isEmpty() || entryResult.isEmpty() )
    {
        MY_LOGD2("No multi-zone AF update");
        return;
    }
    // there are X and Y in each af window position, so divide by 2 to get amount of windows
    windowCount = entryPosition.count()/2;
    // check if entryResult have same window number
    if ( entryResult.count() != windowCount )
    {
        MY_LOGW("Multi-AF Position#(%d)!=result#(%d)", windowCount, entryResult.count());
        return;
    }
    //
    MINT32 activePisitoinX[windowCount];
    MINT32 activePisitoinY[windowCount];
    MSize  focusAreaSize;
    MUINT8 focusResults[windowCount];
    // get position
    for (MUINT i = 0; i < entryPosition.count(); i++)
    {
        MINT32 val = entryPosition.itemAt(i, Type2Type<MINT32>());
        if (i % 2 ==0)
        {
            activePisitoinX[i/2] = val;
            MY_LOGD2("Position X%d: %d", i/2, val);
        }
        else
        {
            activePisitoinY[i/2] = val;
            MY_LOGD2("Position Y%d: %d", i/2, val);
        }
    }
    // get area size
    tryGetMetadata< MSize >(result, MTK_FOCUS_AREA_SIZE, focusAreaSize);
    MY_LOGD2("Focus area (%dx%d)",focusAreaSize.w,focusAreaSize.h);

    // get focus results
    for (MUINT i = 0; i < entryResult.count(); i++)
    {
        focusResults[i] = entryResult.itemAt(i, Type2Type<MUINT8>());
        MY_LOGD2("Focus result %d (%d)",i, focusResults[i]);
    }

    /**************************************************************
    * Transform informaiton from active window to app window
    *
    *       ---------------------------------
    *       |      Active window            |
    *       |  --------------------------   |
    *       |  |                        |   |
    *       |  |   Crop for display     |   |
    *       |  |                        |   |
    *       |  |                        |   |
    *       |  --------------------------   |
    *       ---------------------------------
    * and APP need -1000~+1000 for x and y
    ***************************************************************/
    MRect reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion;
    mpParamsManagerV3->getCropRegion(reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion);
    MINT32 appPisitoinX[windowCount];
    MINT32 appPisitoinY[windowCount];

    // convert window location
    for (MUINT i = 0; i < windowCount; i++)
    {
        appPisitoinX[i] = (((activePisitoinX[i]-reqPreviewCropRegion.p.x)*2000)/reqPreviewCropRegion.s.w)-1000;
        appPisitoinY[i] = (((activePisitoinY[i]-reqPreviewCropRegion.p.y)*2000)/reqPreviewCropRegion.s.h)-1000;
        MY_LOGD2("APP_XY_%d(%d,%d)",i,appPisitoinX[i],appPisitoinY[i]);
    }
    // convert window size
    MSize  appfocusAreaSize;
    appfocusAreaSize.w = (focusAreaSize.w * 2000) / reqPreviewCropRegion.s.w;
    appfocusAreaSize.h = (focusAreaSize.h * 2000) / reqPreviewCropRegion.s.h;
    MY_LOGD2("APP window size (%d,%d)",appfocusAreaSize.w, appfocusAreaSize.h);
    /**************************************************************
    * Callback information to APP
    ***************************************************************/
    //camera_memory* mem = mspCamMsgCbInfo->mRequestMemory(-1, sizeof(int32_t), 1, NULL);
    MINT32 arraySize = 3 + (3 * windowCount);
    MINT32 MultiAFInfo[arraySize];

    MultiAFInfo[0] = (MINT32)windowCount;
    MultiAFInfo[1] = (MINT32)appfocusAreaSize.w;
    MultiAFInfo[2] = (MINT32)appfocusAreaSize.h;

    for (int i = 3 ; i < arraySize ; )
    {
        MultiAFInfo[i]   =  appPisitoinX[(i/3)-1];
        MultiAFInfo[i+1] =  appPisitoinY[(i/3)-1];
        MultiAFInfo[i+2] =  focusResults[(i/3)-1];
        i = i+3;
    }

    mpCamMsgCbInfo->doDataCallback(MTK_CAMERA_MSG_EXT_DATA_AF,MultiAFInfo,arraySize);
}

void
StreamingProcessorImp::
dump()
{
    MY_LOGI("cameraId:%d logLevel:%d multiZoom:%d uninit:%d"
        , mOpenId, mLogLevel, isSupportMultiZonAfWindow, mUninit);
    //
    String8 str = String8::format("Zoom info targetIdx:%d trigger:%d"
        , mZoomTargetIndex, mZoomTriggerd);
    if ( !mZoomResultQueue.isEmpty() ) {
        str += String8::format(" req(%zd): ", mZoomResultQueue.size());
        for (size_t i = 0; i < mZoomResultQueue.size(); ++i)
            str += String8::format(" %d, ", mZoomResultQueue.valueAt(i));
    }
    MY_LOGI("%s", str.string());
    //
    MY_LOGI("[%zd]Focus info cbAFstate:%d AFres:%d needAF:%d"
        , mAfRequest, mAfstateLastCallback, mListenAfResult, mNeedAfCallback);
    MY_LOGI("[%zd]precapture info AE:%d AF:%d withAE:%d withAF:%d check:%d start:%d"
        , mAeRequest, mAestate, mAfstate
        , mListenAeResult, mListenAfResultPreCapture
        , mSkipCheckAeRequestNumber
        , mListenPreCaptureStart);
    //
    MY_LOGI("multi zone af info (%d, %d, %d, %d) msg:%d ext:%d"
        , mActiveArray.p.x, mActiveArray.p.y, mActiveArray.s.w, mActiveArray.s.h
        , lastAFCBMsg, lastAFCBExt);
    // mpParamsManagerV3
    // mpRequestSettingBuilder->dump();
}