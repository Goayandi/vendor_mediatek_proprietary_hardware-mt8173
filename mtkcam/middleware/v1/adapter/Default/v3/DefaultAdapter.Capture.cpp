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

#define LOG_TAG "MtkCam/DefaultAdapter"
//

#include "MyUtils.h"
#include <mtkcam/utils/fwk/MtkCamera.h>
//
#include <inc/CamUtils.h>
using namespace android;
using namespace MtkCamUtils;
//
#include <inc/ImgBufProvidersManager.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProviderFactory.h>
#include <mtkcam/middleware/v1/IParamsManager.h>
#include <mtkcam/middleware/v1/IParamsManagerV3.h>
#include <mtkcam/middleware/v1/ICamAdapter.h>
#include <inc/BaseCamAdapter.h>
#include <buffer/ClientBufferPool.h>
//
#include "inc/v3/DefaultAdapter.h"
using namespace NSDefaultAdapter;
//
#include <hardware/camera3.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

#include <mtkcam/drv/IHalSensor.h>

/* only need when MFB support */
#if MTKCAM_HAVE_MFB_SUPPORT
#include <mtkcam/pipeline/hwnode/MfllNode.h>
#include <mtkcam/feature/mfnr/MfllProperty.h>
using NSCam::v3::MfllNode;
#endif

#include <mtkcam/feature/hdrDetection/Defs.h>

/******************************************************************************
*
*******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
#define FUNC_START                  MY_LOGD("+")
#define FUNC_END                    MY_LOGD("-")


/******************************************************************************
*   Function Prototype.
*******************************************************************************/
//
bool
createShotInstance(
    sp<IShot>&          rpShot,
    uint32_t const      u4ShotMode,
    int32_t const       i4OpenId,
    sp<IParamsManager>  pParamsMgr
);
//

/******************************************************************************
*
*******************************************************************************/
void
CamAdapter::
updateShotMode()
{
    String8 s8ShotMode = getParamsManager()->getShotModeStr();
    uint32_t u4ShotMode = getParamsManager()->getShotMode();
    {
        if (msAppMode == MtkCameraParameters::APP_MODE_NAME_MTK_ZSD)
        {
            if( u4ShotMode == eShotMode_ContinuousShot )
            {
                s8ShotMode = "ZSD CShot";
                u4ShotMode = eShotMode_ContinuousShot;
            }
            else if (u4ShotMode == eShotMode_HdrShot)
            {
                s8ShotMode = "ZSD HDR";
                u4ShotMode = eShotMode_ZsdHdrShot;
            }
            else
            {
                s8ShotMode = "ZSD";
                u4ShotMode = NSCam::eShotMode_ZsdShot;
                //u4ShotMode = NSCam::eShotMode_ZsdVendorShot;
            }
        }
        if (msAppMode == MtkCameraParameters::APP_MODE_NAME_MTK_ENG)
        {
            s8ShotMode = "ENG";
            u4ShotMode = NSCam::eShotMode_EngShot;
        }
    }
    if( mpStateManager->isState(IState::eState_Recording))
    {
        s8ShotMode = "VSS";
        u4ShotMode = eShotMode_VideoSnapShot;
    }
    //
    mShotMode = u4ShotMode;
    MY_LOGI("<shot mode> %#x(%s)", mShotMode, s8ShotMode.string());
    return;
}

/******************************************************************************
*   Before CamAdapter::takePicture() -> IState::onCapture()
*
*   To determine if using Multi Frame Noise Reduction based on the current ISO/EXP,
*   if yes, mShotMode will be updated to
*     1. eShotMode_NormalShot  -->  eShotMode_MfllShot
*     2. eShotMode_ZsdShot     -->  eShotMode_ZsdMfllShot
*******************************************************************************/
void
CamAdapter::
updateShotModeForMultiFrameBlending()
{
#if MTKCAM_HAVE_MFB_SUPPORT
    int mfbMode = 0; // Normal
    int doMfb = 0; // No need
    int forceMfb = 0; // force MFB
    MERROR err = 0;

    // Determine if it's necessary to do Multi-frame Noise Reduction, because
    // MFNR has no scene mode for UI which means, MFNR should be auto truned
    // on while capturing with NormalShot or ZsdShot.
    if (mShotMode == eShotMode_NormalShot
#if MTKCAM_HAVE_ZSDMFB_SUPPORT
        || mShotMode == eShotMode_ZsdShot
#endif
        )
    {
        // check if force MFNR
        forceMfb = mfll::MfllProperty::isForceMfll();

#if MTKCAM_HAVE_MFB_BUILDIN_SUPPORT
        // if makes MFNR default on, do not check option from UI
        mfbMode = MTKCAM_HAVE_MFB_BUILDIN_SUPPORT;
#else
        // 0: No, 1: MFNR, 2: AIS
        // if it's not force MFB, check parameters from UI
        if (!forceMfb) {
            mfbMode = getParamsManager()->getMultFrameBlending();
            if (getParamsManager()->getMultFrameBlending() <= 0)
                return;
        }
#endif

        err = MfllNode::getCaptureInfo(mfbMode, doMfb, getOpenId());
        if (err != OK) {
            MY_LOGE("MfllNode::getCaptureInfo returns code %#x, use normal capture", err);
            doMfb = 0;
        }

        /* force MFB if necessary */
        if (forceMfb)
            doMfb = 1;

        if (doMfb == 0)
            return;

        switch (mShotMode)
        {
            case eShotMode_NormalShot:
                mShotMode = eShotMode_MfllShot;
                MY_LOGD("<shot mode> update shot mode to MfllShot");
                break;
            case eShotMode_ZsdShot:
                mShotMode = eShotMode_ZsdMfllShot;
                MY_LOGD("<shot mode> update shot mode to ZsdMfllShot");
                break;
            default:
                MY_LOGE("Not support eShotMode(%d)", mShotMode);
                break;
        }
    }
    return;
#endif
}

/******************************************************************************
*
*******************************************************************************/
bool
CamAdapter::
updateShotInstance()
{
    CAM_TRACE_CALL();
    MY_LOGI("<shot mode> %#x", mShotMode);
    bool bRet = true;

    /* check if the shot mode should be managed by IResourceContainer */
    if ([this]()->bool{
                switch (mShotMode) {
                case NSCam::eShotMode_ZsdShot:
                case NSCam::eShotMode_ZsdMfllShot:
                case NSCam::eShotMode_ZsdHdrShot:
                    return true;
                default:
                    return false;
                }
            }()
            &&
        msAppMode != MtkCameraParameters::APP_MODE_NAME_MTK_ENG
        )
    {
        MY_LOGD("Try to get last ZsdShot");
        sp<IPipelineResource> pPipelineReource = IResourceContainer::getInstance(getOpenId())->queryPipelineResource(mShotMode);
        if( pPipelineReource != 0 )
        {
            mpShot = pPipelineReource->getShot();
        }
        else
        {
            sp<PipelineResource> pPplRes = new PipelineResource();
            IResourceContainer::getInstance(getOpenId())->setPipelineResource(mShotMode, pPplRes);
            pPipelineReource = IResourceContainer::getInstance(getOpenId())->queryPipelineResource(mShotMode);
        }
        //
        if( mpShot == 0
                || mShotMode == NSCam::eShotMode_ZsdMfllShot
                || mShotMode == NSCam::eShotMode_ZsdHdrShot)
        {
            MY_LOGD("Need to create ZsdShot");

            // if the shot instance exists, remove it and re-create one
            if (mpShot.get()) {
                MY_LOGD("%s: clear the previous shot instance", __FUNCTION__);
                mpShot = NULL;
            }
            bRet = createShotInstance(mpShot,
                                        (msAppMode == MtkCameraParameters::APP_MODE_NAME_MTK_ENG)?(NSCam::eShotMode_EngShot):mShotMode,
                                        getOpenId(), getParamsManager());
            //
            pPipelineReource->setShot(mpShot);
        }
    }
    else
    {
        IResourceContainer::getInstance(getOpenId())->clearPipelineResource();
        //
        bRet = createShotInstance(mpShot,
                                    (msAppMode == MtkCameraParameters::APP_MODE_NAME_MTK_ENG)?(NSCam::eShotMode_EngShot):mShotMode,
                                    getOpenId(), getParamsManager());
    }
    return bRet;
}

/******************************************************************************
*
*******************************************************************************/
bool
CamAdapter::
isTakingPicture() const
{
    FUNC_START;
    bool ret =  mpStateManager->isState(IState::eState_PreCapture) ||
                mpStateManager->isState(IState::eState_NormalCapture) ||
                mpStateManager->isState(IState::eState_VideoSnapshot);
    if  ( ret )
    {
        MY_LOGD("isTakingPicture(1):%s", mpStateManager->getCurrentState()->getName());
    }
    //
    FUNC_END;
    return  ret;
}

/******************************************************************************
*
*******************************************************************************/
bool
CamAdapter::
isSupportVideoSnapshot()
{
    FUNC_START;
    if( ::strcmp(mpParamsManagerV3->getParamsMgr()->getStr(MtkCameraParameters::KEY_VIDEO_FRAME_FORMAT), MtkCameraParameters::PIXEL_FORMAT_BITSTREAM) == 0 ||
            mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_PREVIEW_FRAME_RATE) > 30)
    {
        return false;
    }
    else
    {
        return true;
    }
    FUNC_END;
}

/******************************************************************************
* Normal Capture: Preview -> PreCapture->Idle
* ZSD Capture: Preview -> Capture
* Recording capture: Record -> Capture
*******************************************************************************/
status_t
CamAdapter::
takePicture()
{
    FUNC_START;
    status_t status = OK;
    mbFlashOn = MFALSE;
    //
    mbInTakePicture = MTRUE;
    //
    if (isTakingPicture())
    {
        MY_LOGW("already taking picture...");
        goto lbExit;
    }
    //
    updateShotMode();
    //
    // State:Recording->VideoSnapShot
    if(mpStateManager->isState(IState::eState_Recording))
    {
        if (isSupportVideoSnapshot())
        {
            status = mpStateManager->getCurrentState()->onCapture(this);
            if (OK != status)
            {
                MY_LOGE("onCapture Fail!");
                goto lbExit;
            }
        }
        else
        {
            MY_LOGE("Not support VSS");
            status = INVALID_OPERATION;
            goto lbExit;
        }
    }
    else
    {
        //  ZSD Capture, direct capture Preview -> TakePicture
        if (msAppMode == MtkCameraParameters::APP_MODE_NAME_MTK_ZSD)
        {
            status = onHandlePreCapture();
            if (OK != status)
            {
                MY_LOGE("onPreCapture Fail!");
                goto lbExit;
            }
            //
            if( !mbFlashOn )
            {
                CAM_TRACE_BEGIN("updateShotModeForMultiFrameBlending");
                // MFNR is only applied when no flash
                updateShotModeForMultiFrameBlending();
                CAM_TRACE_END();
            }
            //
            status = mpStateManager->getCurrentState()->onCapture(this);
            if(OK != status)
            {
                goto lbExit;
            }
        }
        else // Normal Capture, Preview -> preCapture- > Idle
        {
            status = mpStateManager->getCurrentState()->onPreCapture(this);
            if (OK != status)
            {
                goto lbExit;
            }
            //
            status = mpStateManager->getCurrentState()->onStopPreview(this);
            if (OK != status)
            {
                goto lbExit;
            }
            // MFNR is only applied when no flash
            if (!mbFlashOn)
                updateShotModeForMultiFrameBlending();
            //
            status = mpStateManager->getCurrentState()->onCapture(this);
            if (OK != status)
            {
                goto lbExit;
            }
        }
    }
    //
lbExit:
    mbInTakePicture = MFALSE;
    FUNC_END;
    return status;
}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
cancelPicture()
{
    FUNC_START;
    CAM_TRACE_NAME("Adapter:cancelPicture");
    if( mpStateManager->isState(IState::eState_NormalCapture) ||
        mpStateManager->isState(IState::eState_ZSLCapture) ||
        mpStateManager->isState(IState::eState_VideoSnapshot))
    {
        mpStateManager->getCurrentState()->onCancelCapture(this);
    }
    FUNC_END;
    return OK;
}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
setCShotSpeed(int32_t i4CShotSpeed)
{
    FUNC_START;
    if(i4CShotSpeed <= 0 )
    {
        MY_LOGE("cannot set continuous shot speed as %d fps)", i4CShotSpeed);
        return BAD_VALUE;
    }

    sp<IShot> pShot = mpShot;
    if( pShot != 0 )
    {
        pShot->sendCommand(eCmd_setCShotSpeed, i4CShotSpeed, 0);
    }
    FUNC_END;
    return OK;
}


/******************************************************************************
*   CamAdapter::takePicture() -> IState::onCapture() ->
*   (Idle)IStateHandler::onHandleNormalCapture() -> CamAdapter::onHandleNormalCapture()
*******************************************************************************/
status_t
CamAdapter::
onHandleNormalCapture()
{
    FUNC_START;
    CAM_TRACE_NAME("Adapter:onHandleNormalCapture");
    status_t status = DEAD_OBJECT;
    sp<ZipImageCallbackThread> pCallbackThread = mpZipCallbackThread.promote();
    sp<ICaptureCmdQueThread> pCaptureCmdQueThread = mpCaptureCmdQueThread;
    //
    if( !pCallbackThread.get() )
    {
        MY_LOGE("no callback thread");
        goto lbExit;
    }
    //
    pCallbackThread->setShotMode(mShotMode, IState::eState_Idle, msAppMode);
    pCallbackThread = 0;
    //
    if  ( pCaptureCmdQueThread != 0 ) {
        status = pCaptureCmdQueThread->onCapture();
    }
    //
lbExit:
    FUNC_END;
    return  status;
}

/******************************************************************************
*
*******************************************************************************/
int
CamAdapter::
getFlashQuickCalibrationResult()
{
    MY_LOGD("+");
    //
    #if '1'==MTKCAM_HAVE_3A_HAL
    //
    IHal3A* pHal3a = MAKE_Hal3A(getOpenId(), getName());

    if ( ! pHal3a )
    {
        MY_LOGE("pHal3a == NULL");
        return 1;
    }

    int result = 0;
    pHal3a->send3ACtrl(NS3Av3::E3ACtrl_GetQuickCalibration, (MINTPTR)&result, 0);

    pHal3a->destroyInstance(getName());
    #endif
    //
    MY_LOGD("-");
    return result;
}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
onHandleCaptureDone()
{
    CAM_TRACE_NAME("Adapter:onHandleCaptureDone");
    //
    FUNC_START;
    if (mpStateManager->isState(IState::eState_NormalCapture))
    {
        return onHandleNormalCaptureDone();
    }
    else if (mpStateManager->isState(IState::eState_ZSLCapture))
    {
        return onHandleZSLCaptureDone();
    }
    else if (mpStateManager->isState(IState::eState_VideoSnapshot))
    {
        return onHandleVideoSnapshotDone();
    }

    FUNC_END;
    return  OK;
}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
onHandleNormalCaptureDone()
{
    CAM_TRACE_NAME("Adapter:onHandleNormalCaptureDone");
    //
    FUNC_START;
    mpStateManager->transitState(IState::eState_Idle);
    FUNC_END;
    return OK ;
}


/******************************************************************************
*   CamAdapter::cancelPicture() -> IState::onCancelCapture() ->
*   IStateHandler::onHandleCancelCapture() -> CamAdapter::onHandleCancelCapture()
*******************************************************************************/
status_t
CamAdapter::
onHandleCancelCapture()
{
    CAM_TRACE_NAME("Adapter:onHandleCancelCapture");
    FUNC_START;
    sp<IShot> pShot = mpShot;
    if  ( pShot != 0 )
    {
        pShot->sendCommand(eCmd_cancel);
    }
    else
    {
        MY_LOGI("pShot is NULL");
    }
    //
    if (mpStateManager->isState(IState::eState_ZSLCapture))
    {
        onHandleCancelZSLCapture();
    }
    else if (mpStateManager->isState(IState::eState_NormalCapture))
    {
        onHandleCancelNormalCapture();
    }
    else if (mpStateManager->isState(IState::eState_VideoSnapshot))
    {
        onHandleCancelVideoSnapshot();
    }
    FUNC_END;
    return  OK;
}


/******************************************************************************
*   CamAdapter::takePicture() -> IState::onCapture() ->
*   (Preview)IStateHandler::onHandleZSLCapture() -> CamAdapter::onHandleZSLCapture()
*******************************************************************************/
status_t
CamAdapter::
onHandleZSLCapture()
{
    FUNC_START;
    CAM_TRACE_BEGIN("Adapter:onHandleZSLCapture_before_onCapture");
    status_t status = DEAD_OBJECT;
    sp<ZipImageCallbackThread> pCallbackThread = mpZipCallbackThread.promote();
    sp<ICaptureCmdQueThread> pCaptureCmdQueThread = mpCaptureCmdQueThread;
    //
    if( !pCallbackThread.get() )
    {
        MY_LOGE("no callback thread");
        goto lbExit;
    }
    //
    pCallbackThread->setShotMode(mShotMode, IState::eState_Preview, msAppMode);
    pCallbackThread = 0;
    CAM_TRACE_END();
    //
    if  ( pCaptureCmdQueThread != 0 ) {
        status = pCaptureCmdQueThread->onCapture();
    }
    else
    {
        MY_LOGE("pCaptureCmdQueThread == NULL");
    }
    //
lbExit:
    FUNC_END;
    return  status;

}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
onHandleZSLCaptureDone()
{
    //
    FUNC_START;
    CAM_TRACE_NAME("Adapter:onHandleZSLCaptureDone");
    if (mpStateManager->isState(IState::eState_ZSLCapture))
    {
        MY_LOGD("mbFlashOn(%d), mbCancelAF(%d), getCancelAF(%d)",
                mbFlashOn, mbCancelAF, mpParamsManagerV3->getCancelAF());
        if(getParamsManager()->getShotMode() == eShotMode_ContinuousShot)
        {
            if( mpFlowControl.get() != NULL )
            {
                if( mbFlashOn )
                {
                    mpParamsManagerV3->getParamsMgr()->set(CameraParameters::KEY_FLASH_MODE, mOldFlashSetting);
                    mpFlowControl->setParameters();
                }
                mpFlowControl->changeToPreviewStatus();
                //
                if( (mbFlashOn && mbCancelAF) || (!mbFlashOn && mpParamsManagerV3->getCancelAF()) )
                {
                    mpParamsManagerV3->setCancelAF(MFALSE);//reset
                    mpFlowControl->cancelAutoFocus();
                }
            }
            else
            {
                MY_LOGE("get Flow Control fail");
            }
            mbCancelAF = MFALSE;
        }
        else if( mpParamsManagerV3->getCancelAF() )
        {
            if( mpFlowControl.get() != NULL && mpParamsManagerV3.get() != NULL )
            {
                mpParamsManagerV3->setCancelAF(MFALSE);//reset
                mpFlowControl->cancelAutoFocus();
            }
            else
            {
                MY_LOGW("mpFlowControl or mpParamsManagerV3 is NULL, cancelAutoFocus is skiped");
            }
        }
        mpStateManager->transitState(IState::eState_Preview);
    }
    else
    {
        MY_LOGW("Not in IState::ZSLCapture !!");
    }
    FUNC_END;
    return  OK;
}


/******************************************************************************
*   CamAdapter::cancelPicture() -> IState::onCancelCapture() ->
*   IStateHandler::onHandleCancelCapture() -> CamAdapter::onHandleCancelCapture()
*******************************************************************************/
status_t
CamAdapter::
onHandleCancelPreviewCapture()
{
    return  onHandleCancelCapture();
}


/******************************************************************************
*
*
*******************************************************************************/
status_t
CamAdapter::
onHandleVideoSnapshot()
{
    FUNC_START;
    CAM_TRACE_NAME("Adapter:onHandleVideoSnapshot");
    status_t status = DEAD_OBJECT;
    sp<ZipImageCallbackThread> pCallbackThread = mpZipCallbackThread.promote();
    sp<ICaptureCmdQueThread> pCaptureCmdQueThread = mpCaptureCmdQueThread;
    //
    if( !pCallbackThread.get() )
    {
        MY_LOGE("no callback thread");
        goto lbExit;
    }
    //
    pCallbackThread->setShotMode(mShotMode, IState::eState_Recording, msAppMode);
    pCallbackThread = 0;
    //
    mpFlowControl->takePicture();

    if  ( pCaptureCmdQueThread != 0 ) {
        status = pCaptureCmdQueThread->onCapture();
    }
    //
lbExit:
    FUNC_END;
    return  status;

}


/******************************************************************************
*
*
*******************************************************************************/
status_t
CamAdapter::
onHandleVideoSnapshotDone()
{
    FUNC_START;
    if(mpStateManager->isState(IState::eState_VideoSnapshot))
    {
        mpStateManager->transitState(IState::eState_Recording);
    }
    else
    {
        MY_LOGW("Not in IState::eState_VideoSnapshot !!");
    }
    FUNC_END;
    return OK ;
}


/******************************************************************************
*
*
*******************************************************************************/
status_t
CamAdapter::
onHandleCancelVideoSnapshot()
{
    FUNC_START;
    FUNC_END;
    return  OK;
}

/******************************************************************************
*
*
*******************************************************************************/
status_t
CamAdapter::
onHandleCancelNormalCapture()
{
    FUNC_START;
    FUNC_END;
    return  OK;
}
/******************************************************************************
*
*
*******************************************************************************/
status_t
CamAdapter::
onHandleCancelZSLCapture()
{
    FUNC_START;
    FUNC_END;
    return  OK;
}

/******************************************************************************
 *
 ******************************************************************************/
template <typename T>
inline MVOID
updateEntry(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T const& val
)
{
    if( pMetadata == NULL ) {
        //MY_LOGE("pMetadata == NULL");
        return;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
}

/******************************************************************************
*
*******************************************************************************/
bool
CamAdapter::
onCaptureThreadLoop()
{
    FUNC_START;
    CAM_TRACE_NAME("Adapter:onCaptureThreadLoop");
    bool ret = false;
    MUINT32 transform = 0;
    //
    //  [1] transit to "Capture" state.
    camera3_request_template_t templateID = CAMERA3_TEMPLATE_STILL_CAPTURE;
    if(mpStateManager->isState(IState::eState_Idle))
    {
        mpStateManager->transitState(IState::eState_NormalCapture);
        templateID = CAMERA3_TEMPLATE_STILL_CAPTURE;
    }
    else
    if(mpStateManager->isState(IState::eState_Preview))
    {
        mpStateManager->transitState(IState::eState_ZSLCapture);
        templateID = CAMERA3_TEMPLATE_STILL_CAPTURE;
    }
    else
    if(mpStateManager->isState(IState::eState_Recording))
    {
        mpStateManager->transitState(IState::eState_VideoSnapshot);
        templateID = CAMERA3_TEMPLATE_VIDEO_SNAPSHOT;
    }
    //
    if( mpStateManager->isState(IState::eState_VideoSnapshot))
    {
        MY_LOGD("In VSS");
    }
    //else
    {
        //
        //  [2.1] update mpShot instance.
        ret = updateShotInstance();
        sp<IShot> pShot = mpShot;
        //
        //  [2.2] return if no shot instance.
        if  ( ! ret || pShot == 0 )
        {
            #warning "[TODO] perform a dummy compressed-image callback or CAMERA_MSG_ERROR to inform app of end of capture?"
            MY_LOGE("updateShotInstance(%d), pShot.get(%p)", ret, pShot.get());
            goto lbExit;
        }
        else
        {
            CAM_TRACE_BEGIN("prepare parameters");
            //  [3.1] prepare parameters
            sp<IParamsManager> pParamsMgr = getParamsManager();
            //
            int iPictureWidth = 0, iPictureHeight = 0;
            if(mpStateManager->isState(IState::eState_VideoSnapshot))
            {
                if(pParamsMgr->getVHdr() != SENSOR_VHDR_MODE_NONE) // VHDR VSS, run feature pipeline
                {
                    pShot->sendCommand(eCmd_setPipelineMode, NULL,ePipelineMode_Feature);
                }

                pParamsMgr->getVideoSize(&iPictureWidth, &iPictureHeight);
            }
            else
            {
                pParamsMgr->getPictureSize(&iPictureWidth, &iPictureHeight);
            }
            //
            int iPreviewWidth = 0, iPreviewHeight = 0;
            pParamsMgr->getPreviewSize(&iPreviewWidth, &iPreviewHeight);
            String8 s8DisplayFormat = mpImgBufProvidersMgr->queryFormat(IImgBufProvider::eID_DISPLAY);
            if  ( String8::empty() == s8DisplayFormat ) {
                MY_LOGW("Display Format is empty");
            }
            //
            if(mName == MtkCameraParameters::APP_MODE_NAME_MTK_ZSD ||       //ZSD preview
                    mpStateManager->isState(IState::eState_VideoSnapshot) ) //VSS shot
            {
            }
            // convert rotation to transform
            MINT32 rotation = pParamsMgr->getInt(CameraParameters::KEY_ROTATION);
            //
            switch(rotation)
            {
                case 0:
                    transform = 0;
                    break;
                case 90:
                    transform = eTransform_ROT_90;
                    break;
                case 180:
                    transform = eTransform_ROT_180;
                    break;
                case 270:
                    transform = eTransform_ROT_270;
                    break;
                default:
                    break;
            }

            //  [3.2] prepare parameters: ShotParam
            ShotParam shotParam;
            shotParam.miPictureFormat           = MtkCameraParameters::queryImageFormat(pParamsMgr->getStr(CameraParameters::KEY_PICTURE_FORMAT));
            if  ( 0 != iPictureWidth && 0 != iPictureHeight )
            {
                shotParam.mi4PictureWidth       = iPictureWidth;
                shotParam.mi4PictureHeight      = iPictureHeight;
            }
            else
            {   //  When 3rd-party apk sets "picture-size=0x0", replace it with "preview-size".
                shotParam.mi4PictureWidth       = iPreviewWidth;
                shotParam.mi4PictureHeight      = iPreviewHeight;
            }
            MY_LOGD("shotPicWxH(%dx%d)", shotParam.mi4PictureWidth, shotParam.mi4PictureHeight);
            shotParam.miPostviewDisplayFormat   = MtkCameraParameters::queryImageFormat(s8DisplayFormat.string());
            shotParam.miPostviewClientFormat    = MtkCameraParameters::queryImageFormat(pParamsMgr->getStr(MtkCameraParameters::KEY_POST_VIEW_FMT));
            // if postview YUV format is unknown, set NV21 as default
            if (shotParam.miPostviewClientFormat == eImgFmt_UNKNOWN)
                shotParam.miPostviewClientFormat = eImgFmt_NV21;

            shotParam.mi4PostviewWidth          = iPreviewWidth;
            shotParam.mi4PostviewHeight         = iPreviewHeight;
            shotParam.ms8ShotFileName           = pParamsMgr->getStr(MtkCameraParameters::KEY_CAPTURE_PATH);
            shotParam.mu4ZoomRatio              = pParamsMgr->getZoomRatio();
            shotParam.muSensorGain              = pParamsMgr->getInt(MtkCameraParameters::KEY_ENG_MANUAL_SENSOR_GAIN);
            shotParam.muSensorSpeed             = pParamsMgr->getInt(MtkCameraParameters::KEY_ENG_MANUAL_SHUTTER_SPEED);
            {
                MINT32 sensorGain, sensorSpeed;
                sensorGain = property_get_int32("debug.camera.sensor.gain", -1);
                if( sensorGain > 0 )
                {
                    shotParam.muSensorGain = sensorGain;
                }
                sensorSpeed = property_get_int32("debug.camera.sensor.speed", -1);
                if( sensorSpeed > 0 )
                {
                    shotParam.muSensorSpeed = sensorSpeed;
                }
                MY_LOGD("prop (gain,speed)=(%d,%d), param (gain,speed)=(%d,%d)",
                        sensorGain, sensorSpeed, shotParam.muSensorGain, shotParam.muSensorSpeed);
            }
            shotParam.mu4ShotCount              = pParamsMgr->getInt(MtkCameraParameters::KEY_BURST_SHOT_NUM);
            shotParam.mu4Transform              = transform;
            shotParam.mu4MultiFrameBlending     = pParamsMgr->getMultFrameBlending();

            // LTM module on/off information
            shotParam.mbEnableLtm               = isEnabledLTM() ? MTRUE : MFALSE;
            CAM_TRACE_END();
            //
            {
                CAM_TRACE_BEGIN("update request");
                if( mpParamsManagerV3 != 0 ) {
                    ITemplateRequest* obj = NSTemplateRequestManager::valueFor(getOpenId());
                    if(obj == NULL) {
                        MY_LOGE("cannot get template request!");
                    }
                    else {
                        //template id is defined in camera3.h
                        shotParam.mAppSetting = obj->getMtkData(templateID);
                        mpParamsManagerV3->updateRequestJpeg(&shotParam.mAppSetting);
                        if( mShotMode == eShotMode_VideoSnapShot )
                        {
                            MINT32 sensorMode = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
                            sp<IFeatureFlowControl> spFlowControl = IResourceContainer::getInstance(getOpenId())->queryFeatureFlowControl();
                            if( spFlowControl == NULL )
                            {
                                MY_LOGW("failed to queryFeatureFlowControl");
                            }
                            else
                            {
                                sensorMode = spFlowControl->getSensorMode();
                            }
                            mpParamsManagerV3->updateRequest(&shotParam.mAppSetting, sensorMode);
                            mpParamsManagerV3->updateRequestVSS(&shotParam.mAppSetting);
                        }
                        else if( mShotMode == eShotMode_EngShot)
                        {
                            MUINT sensormode = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
                            String8 ms8SaveMode = pParamsMgr->getStr(MtkCameraParameters::KEY_RAW_SAVE_MODE);
                            const char *strSaveMode = ms8SaveMode.string();
                            switch (atoi(strSaveMode))
                            {
                                case 1:
                                    sensormode = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
                                    break;
                                case 2:
                                    sensormode = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
                                    break;
                                case 4:
                                    sensormode = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
                                    break;
                                case 5:
                                    sensormode = SENSOR_SCENARIO_ID_SLIM_VIDEO1;
                                    break;
                                case 6:
                                    sensormode = SENSOR_SCENARIO_ID_SLIM_VIDEO2;
                                    break;
                                default:
                                    if (atoi(strSaveMode) > 6)
                                    {
                                         sensormode = atoi(strSaveMode) - 2;
                                    }
                                    break;
                            }
                            //
                            mpParamsManagerV3->updateRequest(&shotParam.mAppSetting, sensormode);
                            //
                        }
                        else
                        {
                            mpParamsManagerV3->updateRequest(&shotParam.mAppSetting, SENSOR_SCENARIO_ID_NORMAL_CAPTURE);
                        }
                        MRect reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion;
                        mpParamsManagerV3->getCropRegion(reqCropRegion, reqSensorCropRegion, reqPreviewCropRegion, reqSensorPreviewCropRegion);
                        //
                        {
                            IMetadata::IEntry entry1(MTK_SCALER_CROP_REGION);
                            MRect regionCapCropRegion;
                            if( reqCropRegion.s.w * iPictureHeight > reqCropRegion.s.h * iPictureWidth ) { // pillarbox
                                regionCapCropRegion.s.w = reqCropRegion.s.h * iPictureWidth / iPictureHeight;
                                regionCapCropRegion.s.h = reqCropRegion.s.h;
                                regionCapCropRegion.p.x = reqCropRegion.p.x + ((reqCropRegion.s.w - regionCapCropRegion.s.w) >> 1);
                                regionCapCropRegion.p.y = reqCropRegion.p.y;
                            }
                            else { // letterbox
                                regionCapCropRegion.s.w = reqCropRegion.s.w;
                                regionCapCropRegion.s.h = reqCropRegion.s.w * iPictureHeight / iPictureWidth;
                                regionCapCropRegion.p.x = reqCropRegion.p.x;
                                regionCapCropRegion.p.y = reqCropRegion.p.y + ((reqCropRegion.s.h - regionCapCropRegion.s.h) >> 1);
                            }
                            entry1.push_back(regionCapCropRegion, Type2Type<MRect>());
                            shotParam.mAppSetting.update(MTK_SCALER_CROP_REGION, entry1);
                            MY_LOGD("zoom crop(%d,%d,%dx%d), cap crop(%d,%d,%dx%d)"
                                    , reqCropRegion.p.x, reqCropRegion.p.y, reqCropRegion.s.w, reqCropRegion.s.h
                                    , regionCapCropRegion.p.x, regionCapCropRegion.p.y, regionCapCropRegion.s.w, regionCapCropRegion.s.h);
                        }
                        {
                            IMetadata::IEntry entry1(MTK_P1NODE_SENSOR_CROP_REGION);
                            if( mpStateManager->isState(IState::eState_VideoSnapshot) )
                            {
                                entry1.push_back(reqSensorPreviewCropRegion, Type2Type<MRect>());
                            }
                            else
                            {
                                entry1.push_back(reqSensorCropRegion, Type2Type<MRect>());
                            }
                            shotParam.mHalSetting.update(MTK_P1NODE_SENSOR_CROP_REGION, entry1);
                        }

                        // update default HAL settings
                        mpParamsManagerV3->updateRequestHal(&shotParam.mHalSetting);

                        {
                            IMetadata::IEntry entry(MTK_HAL_REQUEST_REQUIRE_EXIF);
                            entry.push_back(true, Type2Type< MUINT8 >());
                            shotParam.mHalSetting.update(entry.tag(), entry);
                            MY_LOGD("set HalSetting REQUIRE_EXIF (true)");
                        }
                        if( mIsRaw16CBEnable )
                        {
                            IMetadata::IEntry entry(MTK_STATISTICS_LENS_SHADING_MAP_MODE);
                            entry.push_back(MTK_STATISTICS_LENS_SHADING_MAP_MODE_ON, Type2Type< MUINT8 >());
                            shotParam.mAppSetting.update(entry.tag(), entry);
                            MY_LOGD("DNG set MTK_STATISTICS_LENS_SHADING_MAP_MODE (ON)");
                        }

                        // update HDR mode to 3A
                        {
                            NSCam::HDRMode kHDRMode =
                                mpParamsManagerV3->getParamsMgr()->getHDRMode();
                            IMetadata::IEntry entry(MTK_3A_HDR_MODE);
                            entry.push_back(
                                    static_cast<MUINT8>(kHDRMode), Type2Type< MUINT8 >());
                            shotParam.mHalSetting.update(entry.tag(), entry);
                        }
                    }
                }
                else {
                    MY_LOGW("cannot create paramsmgr v3");
                }
                CAM_TRACE_END();
                // T.B.D.
                // shotParam.mHalSetting
            }

            //for continuous shot
            PreviewMetadata previewMetadata;
            sp<IResourceContainer> spContainer = NULL;
            {
                if( (mShotMode == eShotMode_ContinuousShot) && mpStateManager->isState(IState::eState_NormalCapture) )
                {
                    MY_LOGD("normal CShot, get metadata and streambufferprovider");
                    //get preview metadata
                    if( mpParamsManagerV3 != 0 )
                    {
                        ITemplateRequest* obj = NSTemplateRequestManager::valueFor(getOpenId());
                        if(obj == NULL)
                        {
                            MY_LOGE("cannot get template request!");
                        }
                        else
                        {
                            //template id is defined in camera3.h
                            previewMetadata.mPreviewAppSetting = obj->getMtkData(CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG);
                            mpParamsManagerV3->updateRequest(&previewMetadata.mPreviewAppSetting, SENSOR_SCENARIO_ID_NORMAL_CAPTURE);
                            // update default HAL settings
                            mpParamsManagerV3->updateRequestHal(&previewMetadata.mPreviewHalSetting);
                            //
                            IMetadata::IEntry entry(MTK_HAL_REQUEST_REQUIRE_EXIF);
                            entry.push_back(true, Type2Type< MUINT8 >());
                            previewMetadata.mPreviewHalSetting.update(entry.tag(), entry);
                        }
                    }
                    else
                    {
                        MY_LOGE("cannot get mpParamsManagerV3");
                    }
                    //
                    if( mbFlashOn )
                    {
                        updateEntry<MUINT8>(&previewMetadata.mPreviewAppSetting , MTK_FLASH_MODE , MTK_FLASH_MODE_TORCH);
                        updateEntry<MUINT8>(&previewMetadata.mPreviewAppSetting , MTK_CONTROL_AE_MODE , MTK_CONTROL_AE_MODE_ON);
                    }

                    //create StreamBufferProvider and store in consumer container
                    sp<ClientBufferPool> pClient = new ClientBufferPool(getOpenId(), MTRUE);
                    if( pClient.get() == NULL )
                    {
                        MY_LOGE("pClient.get() == NULL");
                        goto lbExit;
                    }
                    pClient->setCamClient(
                                    "MShot:Preview:Image:yuvDisp",
                                    mpImgBufProvidersMgr,
                                    IImgBufProvider::eID_DISPLAY
                                    );
                    //
                    sp<StreamBufferProviderFactory> pFactory = StreamBufferProviderFactory::createInstance();
                    if( pFactory.get() == NULL )
                    {
                        MY_LOGE("pFactory.get() == NULL");
                        goto lbExit;
                    }
                    pFactory->setUsersPool(pClient);
                    //
                    sp<StreamBufferProvider> spProvider = pFactory->create();
                    if( spProvider.get() == NULL )
                    {
                        MY_LOGE("spProvider.get() == NULL");
                        goto lbExit;
                    }
                    //
                    spContainer = IResourceContainer::getInstance(getOpenId());
                    if( spContainer.get() == NULL )
                    {
                        MY_LOGE("spContainer.get() == NULL");
                        goto lbExit;
                    }
                    spContainer->setConsumer( eSTREAMID_IMAGE_PIPE_YUV_00, spProvider);
                }
            }
            //
            //  [3.3] prepare parameters: JpegParam
            JpegParam jpegParam;
            jpegParam.mu4JpegQuality            = pParamsMgr->getInt(CameraParameters::KEY_JPEG_QUALITY);
            jpegParam.mu4JpegThumbQuality       = pParamsMgr->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
            jpegParam.mi4JpegThumbWidth         = pParamsMgr->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
            jpegParam.mi4JpegThumbHeight        = pParamsMgr->getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
            jpegParam.ms8GpsLatitude            = pParamsMgr->getStr(CameraParameters::KEY_GPS_LATITUDE);
            jpegParam.ms8GpsLongitude           = pParamsMgr->getStr(CameraParameters::KEY_GPS_LONGITUDE);
            jpegParam.ms8GpsAltitude            = pParamsMgr->getStr(CameraParameters::KEY_GPS_ALTITUDE);
            jpegParam.ms8GpsTimestamp           = pParamsMgr->getStr(CameraParameters::KEY_GPS_TIMESTAMP);
            jpegParam.ms8GpsMethod              = pParamsMgr->getStr(CameraParameters::KEY_GPS_PROCESSING_METHOD);
            //
            //check if raw16 CB is enable
            if( mIsRaw16CBEnable )//CAMERA_CMD_ENABLE_RAW16_CALLBACK
            {
                MY_LOGD("adapter.capture: mIsRaw16CBEnable == 1 to enableDataMsg(NSShot::EIShot_DATA_MSG_RAW)");
                pShot->enableDataMsg(NSShot::EIShot_DATA_MSG_RAW);
            }
            //
            // Default ZSD behavior for 3rd-party APP
            if(isThirdPartyZSD())
            {
                sp<IFeatureFlowControl> spFlowControl = IResourceContainer::getInstance(getOpenId())->queryFeatureFlowControl();
                if( spFlowControl == NULL ) {
                    MY_LOGW("failed to queryFeatureFlowControl");
                } else {
                    spFlowControl->pausePreview(false);
                }
                mbNeedResumPreview = MTRUE;
                MY_LOGD("mpFlowControl->suspendPreview() success");
            }
            //
            //  [4.1] perform Shot operations.
            {
                CAM_TRACE_BEGIN("SendCommand to Shot");
                ret = pShot->sendCommand(eCmd_reset);
                ret = ret && pShot->setCallback(this);
                ret = ret && pShot->sendCommand(eCmd_setShotParam, (MUINTPTR)&shotParam, sizeof(ShotParam));
                ret = ret && pShot->sendCommand(eCmd_setJpegParam, (MUINTPTR)&jpegParam, sizeof(JpegParam));
                ret = ret && ( ( (mShotMode == eShotMode_ContinuousShot) && mpStateManager->isState(IState::eState_NormalCapture))  ?
                        pShot->sendCommand(eCmd_setPreviewMetadata, (MUINTPTR)&previewMetadata, sizeof(PreviewMetadata)) : MTRUE );
                CAM_TRACE_END();
                CAM_TRACE_BEGIN("SendCommand eCmd_capture");
                ret = ret && pShot->sendCommand(eCmd_capture);
                CAM_TRACE_END();
            }
            if  ( ! ret )
            {
                MY_LOGE("fail to perform shot operations");
            }
            //
            spContainer = NULL;
        }
        //
        //
        lbExit:
        //
        //  [5.1] uninit shot instance.
        MY_LOGD("free shot instance: (mpShot/pShot)=(%p/%p)", mpShot.get(), pShot.get());
        CAM_TRACE_BEGIN("uninit");
        //
        mpShot = NULL;
        pShot  = NULL;
        //
        //  [5.11] update focus steps.
        //
        {
            sp<IParamsManager> pParamsMgr = getParamsManager();
            pParamsMgr->updateBestFocusStep();
        }
        //
        //  [5.2] notify capture done.
        CAM_TRACE_END();
    }
    // update flash calibration result.
    int flashCaliEn = getParamsManager()->getInt(MtkCameraParameters::KEY_ENG_FLASH_CALIBRATION);
    if (flashCaliEn == 1)
    {
        if (getFlashQuickCalibrationResult() != 0)
        {
            onCB_Error(CAMERA_ERROR_CALI_FLASH, 0);
        }
    }
    //
    mpStateManager->getCurrentState()->onCaptureDone(this);
    //
    //
    FUNC_END;
    return  true;
}

