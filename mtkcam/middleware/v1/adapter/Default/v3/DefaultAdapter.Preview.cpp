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
//
#include <cutils/properties.h>      // [debug] should be remove
//
#include <inc/CamUtils.h>
using namespace android;
using namespace MtkCamUtils;
//
#include <camera_custom_zsd.h>
#include <mtkcam/utils/fwk/MtkCamera.h>
//
#include <inc/ImgBufProvidersManager.h>
//
#include <mtkcam/middleware/v1/IParamsManager.h>
#include <mtkcam/middleware/v1/ICamAdapter.h>
#include <inc/BaseCamAdapter.h>
//
#include "inc/v3/DefaultAdapter.h"
using namespace NSDefaultAdapter;
//
#include <mtkcam/drv/IHalSensor.h>
using namespace NSCam;
//
#include <camera_custom_vt.h>
//
#include <cutils/properties.h>
//



#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#define SUPPORT_3A (1)
//
#if SUPPORT_3A
#include <mtkcam/aaa/IHal3A.h>
using namespace NS3Av3;
#endif
//
/******************************************************************************
*
*******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)(%d)(%s)[%s] " fmt, ::gettid(), getOpenId(), getName(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, arg...)    if (cond) { MY_LOGV(arg); }
#define MY_LOGD_IF(cond, arg...)    if (cond) { MY_LOGD(arg); }
#define MY_LOGI_IF(cond, arg...)    if (cond) { MY_LOGI(arg); }
#define MY_LOGW_IF(cond, arg...)    if (cond) { MY_LOGW(arg); }
#define MY_LOGE_IF(cond, arg...)    if (cond) { MY_LOGE(arg); }

#define MY_LOGD1(...)               MY_LOGD_IF((mLogLevel>=1),__VA_ARGS__)
#define MY_LOGD2(...)               MY_LOGD_IF((mLogLevel>=2),__VA_ARGS__)
#define MY_LOGD3(...)               MY_LOGD_IF((mLogLevel>=3),__VA_ARGS__)

//
#define FUNC_START                  MY_LOGD2("+")
#define FUNC_END                    MY_LOGD2("-")


/******************************************************************************
*
*******************************************************************************/
bool
CamAdapter::
previewEnabled() const
{
    bool ret = false;
    if(isThirdPartyZSD()&&
       mbNeedResumPreview)
    {
        ret = false;
    }
    else
    {
        ret =  (mpStateManager->isState(IState::eState_Preview)||
                mpStateManager->isState(IState::eState_ZSLCapture)||
                recordingEnabled());
    }
    MY_LOGD("ret (%d)", ret);
    return ret;
}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
startPreview()
{
    CAM_TRACE_NAME("Adapter:startPreview");
    return  mpStateManager->getCurrentState()->onStartPreview(this);
}


/******************************************************************************
*
*******************************************************************************/
void
CamAdapter::
stopPreview()
{
    CAM_TRACE_NAME("Adapter:stopPreview");
    mpStateManager->getCurrentState()->onStopPreview(this);
}



/******************************************************************************
*
*******************************************************************************/
int
CamAdapter::
getSensorScenario()
{
    CAM_TRACE_NAME("Adapter:getSensorScenario");
    int scenarioMap[]= {
                        SENSOR_SCENARIO_ID_NORMAL_PREVIEW,    //0
                        SENSOR_SCENARIO_ID_NORMAL_PREVIEW,    //1:Preview Mode
                        SENSOR_SCENARIO_ID_NORMAL_PREVIEW,    //2:Capture Mode
                        SENSOR_SCENARIO_ID_NORMAL_PREVIEW,    //3:JPEG only
                        SENSOR_SCENARIO_ID_NORMAL_VIDEO,      //4:Video Preview
                        SENSOR_SCENARIO_ID_SLIM_VIDEO1,       //5:Slim Video 1
                        SENSOR_SCENARIO_ID_SLIM_VIDEO2,       //6:Slim Video 2
                       };

    // Engineer sensor Scenario
    String8 ms8SaveMode = mpParamsManagerV3->getParamsMgr()->getStr(MtkCameraParameters::KEY_RAW_SAVE_MODE);

    int camera_mode = mpParamsManagerV3->getParamsMgr()->getInt(MtkCameraParameters::KEY_CAMERA_MODE);
    if (camera_mode != 0)
    {
        ms8SaveMode = String8("4"); // force sensor scenario to "normal video" // should remove when em camera app modifed
    }
    const char *strSaveMode = ms8SaveMode.string();
    int mode = atoi(strSaveMode);


    if (mode > (int)(sizeof(scenarioMap)/sizeof(int)) || mode < 0 )
    {
        MY_LOGW("Wrong mode:%d", mode);
        return SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
    }

    // for debug
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    property_get( "debug.cameng.force_sensormode", value, "-1");
    MINT32 val = atoi(value);
    if( val > 0 )
    {
        MY_LOGD1("force use sensor scenario %d", val);
        return val;
    }

    //
    return scenarioMap[mode];
}


/******************************************************************************
*   CamAdapter::startPreview() -> IState::onStartPreview() ->
*   IStateHandler::onHandleStartPreview() -> CamAdapter::onHandleStartPreview()
*******************************************************************************/
status_t
CamAdapter::
onHandleStartPreview()
{
    CAM_TRACE_NAME("Adapter:onHandleStartPreview");
    FUNC_START
    status_t ret = INVALID_OPERATION;
    int templateID = 0;
    //
    int sensorScenario = getSensorScenario();
    //
    if  ( ! BaseCamAdapter::init() )
    {
        goto lbExit;
    }
    //
#warning "FIXME"
#if 0
    mpFlowControl = IFlowControl::createInstance(
                        "DefaultCamAdapter",
                        getOpenId(),
                        IFlowControl::ControlType_T::CONTROL_DEFAULT,
                        getParamsManager(),
                        mpImgBufProvidersMgr,
                        this
                    );
    if ( mpFlowControl == 0 ) goto lbExit;

    if ( mpFlowControl->setParameters() != OK ) goto lbExit;
#endif
    if(isThirdPartyZSD() &&
       mbNeedResumPreview == MTRUE)
    {
        // Default ZSD behavior for 3rd-party APP
        sp<IFeatureFlowControl> spFlowControl = IResourceContainer::getInstance(getOpenId())->queryFeatureFlowControl();
        if( spFlowControl == NULL ) {
            MY_LOGW("failed to queryFeatureFlowControl");
        } else {
            spFlowControl->resumePreview();
        }
        if ( ret != OK )
        {
            MY_LOGE("Error to mpFlowControl->resumePreview()");
            goto lbExit;
        }
        MY_LOGD("mpFlowControl->resumePreview() success");
    }
    else
    {
        // send command of LTM on/off IFlowControl before startPreview
        ret = mpFlowControl->sendCommand(
                IFlowControl::eExtCmd_setLtmEnable,
                isEnabledLTM() ? 1 : 0,
                1 // the arg2 represents that if this command sent from middleware
                  // rather than AP
                );
        if ( ret != OK )
        {
            MY_LOGE("Error to set LTM enable(%d) via " \
                    "sendCommand(IFlowControl::eExtCmd_setLtmEnable)",
                    isEnabledLTM() ? 1 : 0);
        }

        ret = mpFlowControl->startPreview();
        if ( ret != OK )
        {
            MY_LOGE("Error to mpFlowControl->startPreview()");
            goto lbExit;
        }
        MY_LOGD("mpFlowControl->startPreview() success");
    }
    mbNeedResumPreview = MFALSE;
    //
    mpStateManager->transitState(IState::eState_Preview);
   //
lbExit:
    //
    if(ret != OK)
    {
        forceStopAndCleanPreview();
    }
    //
    FUNC_END;
    return ret;
}


/******************************************************************************
*   CamAdapter::stopPreview() -> IState::onStopPreview() ->
*   IStateHandler::onHandleStopPreview() -> CamAdapter::onHandleStopPreview()
*******************************************************************************/
status_t
CamAdapter::
onHandleStopPreview()
{
    CAM_TRACE_NAME("Adapter:onHandleStopPreview");
    //
    IResourceContainer::getInstance(getOpenId())->clearPipelineResource();
    //
    return forceStopAndCleanPreview();
}


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
        //MY_LOGE("pMetadata == NULL");
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
status_t
CamAdapter::
getAeStateAndMode(MUINT8& AeState, MUINT8& AeMode)
{
    sp<IFrameInfo> pFrameInfo = IResourceContainer::getInstance(getOpenId())->queryLatestFrameInfo();
    if(pFrameInfo==NULL)
    {
        MY_LOGW("failed to queryLatestFrameInfo");
        return UNKNOWN_ERROR;
    }

    IMetadata meta;
    pFrameInfo->getFrameMetadata(meta);
    tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_MODE, AeMode);
    if ( !tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_STATE, AeState) ) {
        MY_LOGW("NO AE State information");
        return UNKNOWN_ERROR;
    }

    // AE mode & flash mode check
    const char *flashMode = (mpParamsManagerV3->getParamsMgr()->getStr(CameraParameters::KEY_FLASH_MODE)).string();
    AeMode =
        !strcmp(flashMode, CameraParameters::FLASH_MODE_OFF) ?
            MTK_CONTROL_AE_MODE_ON :
        !strcmp(flashMode, CameraParameters::FLASH_MODE_AUTO) ?
            MTK_CONTROL_AE_MODE_ON_AUTO_FLASH :
        !strcmp(flashMode, CameraParameters::FLASH_MODE_ON) ?
            MTK_CONTROL_AE_MODE_ON_ALWAYS_FLASH :
        !strcmp(flashMode, CameraParameters::FLASH_MODE_RED_EYE) ?
            MTK_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE :
        !strcmp(flashMode, CameraParameters::FLASH_MODE_TORCH) ?
            MTK_CONTROL_AE_MODE_ON:
        MTK_CONTROL_AE_MODE_ON;

    MY_LOGD("AE Mode(%d) AE State(%d)", AeMode, AeState);

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
CamAdapter::
triggerFlashCshotPrecapture()
{
    //ZSD CShot with flash need 2nd precapture
    MY_LOGD("ZSD CShot with flash, set flash torch mode and trigger precapture");
    mpParamsManagerV3->getParamsMgr()->set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_TORCH);
    if( mpFlowControl != NULL)
    {
        mpFlowControl->setParameters();
        int flashResult = 0;
        sp<IResourceContainer> spResourceContainer = IResourceContainer::getInstance(getOpenId());
        if( spResourceContainer == NULL )
        {
            MY_LOGW("failed to get IResourceContainer");
            return OK;
        }
        int frameCount = 15;
        while( frameCount > 0 )
        {
            MUINT8 flashMode = 0,  controlAeMode = 0;
            sp<IFrameInfo> pFrameInfo = spResourceContainer->queryLatestFrameInfo();
            IMetadata meta;
            pFrameInfo->getFrameMetadata(meta);
            tryGetMetadata< MUINT8 >(&meta, MTK_FLASH_MODE, flashMode);
            tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_MODE, controlAeMode);
            MY_LOGD("flashMode(%d) controlAeMode(%d) frameCount(%d)", flashMode, controlAeMode, frameCount);
            if( flashMode == MTK_FLASH_MODE_TORCH && controlAeMode == MTK_CONTROL_AE_MODE_ON )
            {
                break;
            }
            frameCount--;
#warning "[Todo] remove usleep..."
            usleep(33*1000);
        }
        status_t ret = mpFlowControl->precapture(flashResult);
    }
    else
    {
        MY_LOGW("mpFlowControl == NULL, torch mode not set");
    }
    return OK;
}


/******************************************************************************
*   CamAdapter::takePicture() -> IState::onPreCapture() ->
*   IStateHandler::onHandlePreCapture() -> CamAdapter::onHandlePreCapture()
*******************************************************************************/
status_t
CamAdapter::
onHandlePreCapture()
{
    CAM_TRACE_NAME("Adapter:onHandlePreCapture");
    FUNC_START;
    status_t ret = OK;
    int flashRequired = 0;

    CAM_TRACE_BEGIN("get flash info");
    MBOOL bAeStateSearching = MFALSE;
    mbFlashOn = MFALSE;
    {
        //get flash info
        MUINT8 AeState = 0, AeMode = 0;
        if ( OK != getAeStateAndMode(AeState, AeMode) ) {
            mbFlashOn = MTRUE;
        }
        if( AeMode == MTK_CONTROL_AE_MODE_ON_ALWAYS_FLASH ||
            (AeMode == MTK_CONTROL_AE_MODE_ON_AUTO_FLASH && AeState == MTK_CONTROL_AE_STATE_FLASH_REQUIRED) )
        {
            mbFlashOn = MTRUE;
        }
        else if( AeMode == MTK_CONTROL_AE_MODE_ON_AUTO_FLASH && (AeState == MTK_CONTROL_AE_STATE_SEARCHING || AeState == MTK_CONTROL_AE_STATE_INACTIVE) )
        {
            bAeStateSearching = MTRUE;
        }
        MY_LOGD("AE Mode(%d) AE State(%d), mbFlashOn(%d), bAeStateSearching(%d)",
                AeMode, AeState, mbFlashOn, bAeStateSearching);
    }
    CAM_TRACE_END();

    if(mpStateManager->isState(IState::eState_Recording))
    {
        MY_LOGD("Skip PreCapture in VSS");
    }
    else if(msAppMode == MtkCameraParameters::APP_MODE_NAME_MTK_ZSD)
    {
        if( mbFlashOn || bAeStateSearching )
        {
            MY_LOGD("Need to PreCapture in ZSD");
            if( mpStateManager->isState(IState::eState_ZSLCapture) )
            {
                // Previous ZSD capture not done, need to wait capture done for flash flow,
                // or the state machine can't run onPreCapture() in eState_ZSLCapture
                MY_LOGD("Need to wait eState_ZSLCapture to eState_Preview");
                IStateManager::StateObserver stateWaiter(mpStateManager);
                mpStateManager->registerOneShotObserver(&stateWaiter);
                ret = stateWaiter.waitState(IState::eState_Preview);
                if (OK != ret)
                {
                    MY_LOGE("wait State_Preview Fail!");
                    goto lbExit;
                }
                MY_LOGD("wait State_Preview done");
            }
            // flash enable
            MUINT32 flashCaliEn = getParamsManager()->getInt(MtkCameraParameters::KEY_ENG_FLASH_CALIBRATION);
            if (flashCaliEn == 1)
            {
                enableFlashCalibration(flashCaliEn);
            }
            ret = mpFlowControl->precapture(flashRequired);
            mOldFlashSetting = mpParamsManagerV3->getParamsMgr()->getInt(CameraParameters::KEY_FLASH_MODE);
            if( mShotMode == eShotMode_ContinuousShot &&
                (mbFlashOn || (bAeStateSearching && flashRequired > 0)) )
            {
                mbCancelAF = mpParamsManagerV3->getCancelAF();
                ret = triggerFlashCshotPrecapture();
            }
        }
    }
    else
    {
        if( (!mbFlashOn) && (!bAeStateSearching) && (mShotMode == eShotMode_ContinuousShot) )
        {
            MY_LOGD("Skip PreCapture in normal");
        }
        else
        {
            // flash enable
            MUINT32 flashCaliEn = getParamsManager()->getInt(MtkCameraParameters::KEY_ENG_FLASH_CALIBRATION);
            if (flashCaliEn == 1)
            {
                enableFlashCalibration(flashCaliEn);
            }
            ret = mpFlowControl->precapture(flashRequired);
        }
    }
    //
    if (ret != OK)
    {
        MY_LOGE("mpFlowControl->precapture Fail!");
        goto lbExit;
    }
    //
    if(msAppMode != MtkCameraParameters::APP_MODE_NAME_MTK_ZSD)
    {
        mpStateManager->transitState(IState::eState_PreCapture);
    }
    //
    if( bAeStateSearching && flashRequired > 0 )
    {
        mbFlashOn = MTRUE;
    }
lbExit:

    MY_LOGD("mbFlashOn(%d)", mbFlashOn);
    FUNC_END;
    return ret;
}


/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
forceStopAndCleanPreview()
{
    FUNC_START;
    status_t ret = INVALID_OPERATION;
    //
    ret = mpFlowControl->stopPreview();
    if (ret != OK)
    {
        MY_LOGD("mpFlowControl->stopPreview() fail");
    }
    //
#warning "FIXME"
#if 0
    mpFlowControl = NULL;
#endif
    MY_LOGD1("transitState->eState_Idle");
    mpStateManager->transitState(IState::eState_Idle);
    //
    FUNC_END;
    //
    return ret;
}



/******************************************************************************
*
*******************************************************************************/
status_t
CamAdapter::
enableFlashCalibration(int enable)
{
    CAM_TRACE_NAME("Adapter:enableFlashCalibration");
    FUNC_START;
    //
    status_t ret = OK;
    //
    #if '1'==MTKCAM_HAVE_3A_HAL
    //
    IHal3A* pHal3a = MAKE_Hal3A(getOpenId(), getName());

    if ( ! pHal3a )
    {
        MY_LOGE("pHal3a == NULL");
        return INVALID_OPERATION;
    }

    if ( ! pHal3a->send3ACtrl(NS3Av3::E3ACtrl_EnableFlashQuickCalibration,enable, 0) )
    {
        MY_LOGE("enableFlashQuickCalibration fail");
        ret = INVALID_OPERATION;
        goto lbExit;
    }
    #endif

lbExit:
    #if '1'==MTKCAM_HAVE_3A_HAL
    pHal3a->destroyInstance(getName());
    #endif
    //
    FUNC_END;
    return ret;
}
