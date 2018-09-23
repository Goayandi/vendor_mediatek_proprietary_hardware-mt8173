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

#define LOG_TAG "MtkCam/Cam1Device"
//
#include "MyUtils.h"
#include "StereoCam1Device.h"
//
using namespace android;
//
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/hw/CamManager.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
using namespace NSCam;
//
#include <sys/prctl.h>


/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("(%d)(%s:%d)[%s] " fmt, ::gettid(), getDevName(), getOpenId(), __FUNCTION__, ##arg)
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
extern "C"
NSCam::Cam1Device*
createCam1Device_MtkStereo(
    String8 const&          rDevName,
    int32_t const           i4OpenId
)
{
    return new StereoCam1Device(rDevName, i4OpenId);
}


/******************************************************************************
 *
 ******************************************************************************/
StereoCam1Device::
StereoCam1Device(
    String8 const&          rDevName,
    int32_t const           i4OpenId
)
    : Cam1DeviceBase(rDevName, i4OpenId)
    //
    , mpHalSensor(NULL)
    //
#if '1'==MTKCAM_HAVE_3A_HAL
    , mpHal3a_Main(NULL)
    , mpHal3a_Main2(NULL)
    , mpSync3AMgr(NULL)
#endif
    , mbThreadRunning(MFALSE)
    //
    , mSensorId_Main(-1)
    , mSensorId_Main2(-1)
{
}


/******************************************************************************
 *
 ******************************************************************************/
StereoCam1Device::
~StereoCam1Device()
{
}


/******************************************************************************
 *
 ******************************************************************************/
bool
StereoCam1Device::
onInit()
{
    MY_LOGD("+");
    {
        CAM_TRACE_NAME("init(profile)");
    }
    Utils::CamProfile  profile(__FUNCTION__, "StereoCam1Device");
    //
    bool    ret = false;
    int     err = 0, i4DeviceNum = 0;
    //

    IHalSensorList* const pHalSensorList = MAKE_HalSensorList();
    if(!pHalSensorList)
    {
        MY_LOGE("pHalSensorList == NULL");
        return 0;
    }
    //
    pHalSensorList->searchSensors();
    i4DeviceNum = pHalSensorList->queryNumberOfSensors();
    if(i4DeviceNum < 2)
    {
        MY_LOGE("i4DeviceNum(%d) < 2", i4DeviceNum);
        return 0;
    }
    else if(i4DeviceNum > 4)
    {
        MY_LOGW("i4DeviceNum(%d) > 4, it is weired to turn on stereo cam in such situation!", i4DeviceNum);
    }else{
        MY_LOGD("i4DeviceNum(%d)", i4DeviceNum);
    }

    uint32_t curDevIdx = pHalSensorList->querySensorDevIdx(mi4OpenId);
    switch(curDevIdx){
        case SENSOR_DEV_MAIN:
            MY_LOGD("setup rear+rear sensor pair to StereoSettingProvider");
            StereoSettingProvider::setStereoProfile(STEREO_SENSOR_PROFILE_REAR_REAR);
            break;
        case SENSOR_DEV_SUB:
            MY_LOGD("setup front+front sensor pair to StereoSettingProvider");
            StereoSettingProvider::setStereoProfile(STEREO_SENSOR_PROFILE_FRONT_FRONT);
            break;
        default:
            MY_LOGE("Unrecognized opendId(%d)", mi4OpenId);
            return 0;
    }

    if(i4DeviceNum == 2){
        MY_LOGW("There is only 2 sensors, use rear+front setting!");
        StereoSettingProvider::setStereoProfile(STEREO_SENSOR_PROFILE_REAR_FRONT);
    }

    if(!StereoSettingProvider::getStereoSensorIndex(mSensorId_Main, mSensorId_Main2)){
        MY_LOGE("Cannot get sensor ids from StereoSettingProvider! (%d,%d)", mSensorId_Main, mSensorId_Main2);
        goto lbExit;
    }else{
        MY_LOGD("main1 openId:%d, main2 openId:%d", getOpenId_Main(), getOpenId_Main2());
    }
    //--------------------------------------------------------------------------
    {
        CAM_TRACE_NAME("init(sensor)");
        //  (0) power on sensor
        if( pthread_create(&mThreadHandle, NULL, doThreadInit, this) != 0 )
        {
            MY_LOGE("pthread create failed");
            goto lbExit;
        }
        mbThreadRunning = MTRUE;
        //
        // workaround: yuv sensor, 3A depends on sensor power-on
        if( pHalSensorList->queryType( getOpenId_Main() ) == NSCam::NSSensorType::eYUV ||
            pHalSensorList->queryType( getOpenId_Main2() ) == NSCam::NSSensorType::eYUV )
        {
            if( !waitThreadInitDone() )
            {
                MY_LOGE("init in thread failed");
                goto lbExit;
            }
        }
    }
    //--------------------------------------------------------------------------
        //  (1) Open 3A
#if '1'==MTKCAM_HAVE_3A_HAL
    {
        CAM_TRACE_NAME("init(3A)");

        mpSync3AMgr = MAKE_Sync3AMgr();
        if  ( ! mpSync3AMgr ) {
            MY_LOGE("bad mpSync3AMgr");
            goto lbExit;
        }
        if  ( ! mpSync3AMgr->init(0, getOpenId_Main(), getOpenId_Main2()) ) {
            MY_LOGE("mpSync3AMgr->init fail");
            goto lbExit;
        }
        mpHal3a_Main = MAKE_Hal3A(
                getOpenId_Main(),
                LOG_TAG);
        if  ( ! mpHal3a_Main ) {
            MY_LOGE("bad mpHal3a_Main");
            goto lbExit;
        }
        mpHal3a_Main2 = MAKE_Hal3A(
                getOpenId_Main2(),
                LOG_TAG);
        if  ( ! mpHal3a_Main2 ) {
            MY_LOGE("bad mpHal3a_Main2");
            goto lbExit;
        }

        MY_LOGD("notifyPwrOn");
        if( !waitThreadInitDone() )
        {
            MY_LOGE("init in thread failed");
            goto lbExit;
        }
        if (mpHal3a_Main != NULL)
        {
            mpHal3a_Main->notifyPwrOn();
        }
        if (mpHal3a_Main2 != NULL)
        {
            mpHal3a_Main2->notifyPwrOn();
        }

        profile.print("3A Hal -");
    }
#endif  //MTKCAM_HAVE_3A_HAL
    //--------------------------------------------------------------------------
    {
        CAM_TRACE_NAME("init(base)");
        //  (2) Init Base.
        if  ( ! Cam1DeviceBase::onInit() )
        {
            goto lbExit;
        }
    }
    //
    //--------------------------------------------------------------------------
    ret = true;
lbExit:
    profile.print("");
    MY_LOGD("- ret(%d) sensorId(%d,%d)", ret, getOpenId_Main(), getOpenId_Main2());
    return  ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
StereoCam1Device::
onUninit()
{
    MY_LOGD("+");
    Utils::CamProfile  profile(__FUNCTION__, "StereoCam1Device");
    //
    if( !waitThreadInitDone() )
    {
        MY_LOGE("init in thread failed");
    }
    //--------------------------------------------------------------------------
    //  (1) Uninit Base
    {
        CAM_TRACE_NAME("uninit(base)");
        Cam1DeviceBase::onUninit();
        profile.print("Cam1DeviceBase::onUninit() -");
    }
    //--------------------------------------------------------------------------
    //  (2) Close 3A
#if '1'==MTKCAM_HAVE_3A_HAL
    {
        CAM_TRACE_NAME("uninit(3A)");
        if  ( mpSync3AMgr )
        {
            mpSync3AMgr->uninit();
            mpSync3AMgr = NULL;
        }
        if  ( mpHal3a_Main )
        {
            mpHal3a_Main->notifyPwrOff();
            mpHal3a_Main->destroyInstance(LOG_TAG);
            mpHal3a_Main = NULL;
        }
        if  ( mpHal3a_Main2 )
        {
            mpHal3a_Main2->notifyPwrOff();
            mpHal3a_Main2->destroyInstance(LOG_TAG);
            mpHal3a_Main2 = NULL;
        }
        profile.print("3A Hal -");
    }
#endif  //MTKCAM_HAVE_3A_HAL
    //--------------------------------------------------------------------------
    //  (4) Close Sensor
#if '1'==MTKCAM_HAVE_SENSOR_HAL
    {
        CAM_TRACE_NAME("uninit(sensor)");
        //
        if(mpHalSensor)
        {
            MY_LOGD("powerOff dual sensors 1-by-1");

            MUINT pIndex_main[1] = { (MUINT)getOpenId_Main()};
            mpHalSensor->powerOff(USER_NAME, 1, pIndex_main);

            MUINT pIndex_main2[1] = { (MUINT)getOpenId_Main2()};
            mpHalSensor->powerOff(USER_NAME, 1, pIndex_main2);

            mpHalSensor->destroyInstance(USER_NAME);
            mpHalSensor = NULL;
        }
        MY_LOGD("SensorHal::destroyInstance()");
        profile.print("Sensor Hal -");
    }
#endif  //MTKCAM_HAVE_SENSOR_HAL
    //--------------------------------------------------------------------------
    profile.print("");
    MY_LOGD("-");
    return  true;
}


/******************************************************************************
 * [Template method] Called by startPreview().
 ******************************************************************************/
bool
StereoCam1Device::
onStartPreview()
{
    bool ret = false;
    Utils::CamManager* pCamMgr = Utils::CamManager::getInstance();
    //
    //  (0) wait for thread
    if( !waitThreadInitDone() )
    {
        MY_LOGE("init in thread failed");
        goto lbExit;
    }
    //
    //  (1) Check Permission.
    if ( ! pCamMgr->getPermission() )
    {
        MY_LOGE("Cannot start preview ... Permission denied");
        goto lbExit;
    }

    //  (3) Initialize Camera Adapter.
    if  ( ! initCameraAdapter() )
    {
        MY_LOGE("NULL Camera Adapter");
        goto lbExit;
    }

    // (4) Enter thermal policy.
    enterThermalPolicy();

    ret = true;
lbExit:
    return ret;
}


/******************************************************************************
 *  [Template method] Called by stopPreview().
 ******************************************************************************/
void
StereoCam1Device::
onStopPreview()
{
    if  ( mpCamAdapter != 0 )
    {
        mpCamAdapter->cancelPicture();
        mpCamAdapter->uninit();
        mpCamAdapter.clear();
    }

    exitThermalPolicy();
}


/******************************************************************************
 * Create a thread to hide some initial steps to speed up launch time
 ******************************************************************************/
bool
StereoCam1Device::
powerOnSensor()
{
    MY_LOGD("+");
    bool    ret = false;
    //  (1) Open Sensor
#if '1'==MTKCAM_HAVE_SENSOR_HAL
    CAM_TRACE_CALL();
    Utils::CamProfile  profile(__FUNCTION__, "StereoCam1Device");
    //
    NSCam::IHalSensorList* pHalSensorList = MAKE_HalSensorList();
    MUINT pIndex[2] = { (MUINT)getOpenId_Main(), (MUINT)getOpenId_Main2() };
    MUINT const main1Index = getOpenId_Main();
    MUINT const main2Index = getOpenId_Main2();
    if(!pHalSensorList)
    {
        MY_LOGE("pHalSensorList == NULL");
        goto lbExit;
    }
    //
    mpHalSensor = pHalSensorList->createSensor(
                                        USER_NAME,
                                        2,
                                        pIndex);
    if(mpHalSensor == NULL)
    {
       MY_LOGE("mpHalSensor is NULL");
       goto lbExit;
    }

    MY_LOGD("powerOn dual sensors 1-by-1");
    if( !mpHalSensor->powerOn(USER_NAME, 1, &main1Index) )
    {
        MY_LOGE("sensor power on failed: %d", pIndex[0]);
        goto lbExit;
    }
    if( !mpHalSensor->powerOn(USER_NAME, 1, &main2Index) )
    {
        MY_LOGE("sensor power on failed: %d", pIndex[1]);
        goto lbExit;
    }
    //
    profile.print("Sensor Hal -");
#endif  //MTKCAM_HAVE_SENSOR_HAL
    ret = true;
lbExit:
    MY_LOGD("-");
    return ret;
}


/******************************************************************************
 * the init function to be called in the thread
 ******************************************************************************/
void*
StereoCam1Device::
doThreadInit(void* arg)
{
    ::prctl(PR_SET_NAME,"initCamdevice", 0, 0, 0);
    StereoCam1Device* pSelf = reinterpret_cast<StereoCam1Device*>(arg);
    pSelf->mRet = pSelf->powerOnSensor();
    pthread_exit(NULL);
    return NULL;
}


/******************************************************************************
 * Wait for initializations by thread are done.
 ******************************************************************************/
bool
StereoCam1Device::
waitThreadInitDone()
{
    bool ret = false;
    if( mbThreadRunning )
    {
        MY_LOGD("wait init done +");
        int s = pthread_join(mThreadHandle, NULL);
        MY_LOGD("wait init done -");
        mbThreadRunning = MFALSE;
        if( s != 0 )
        {
            MY_LOGE("pthread join error: %d", s);
            goto lbExit;
        }

        if( !mRet )
        {
            MY_LOGE("init in thread failed");
            goto lbExit;
        }
    }

    ret = true;
lbExit:
    return ret;
}


/******************************************************************************
 *  Set the camera parameters. This returns BAD_VALUE if any parameter is
 *  invalid or not supported.
 ******************************************************************************/
status_t
StereoCam1Device::
setParameters(const char* params)
{
    CAM_TRACE_CALL();
    status_t status = OK;
    //
    //  (1) Update params to mpParamsMgr.
    status = mpParamsMgr->setParameters(String8(params));
    if  ( OK != status ) {
        goto lbExit;
    }

    //  Here (1) succeeded.
    //  (2) If CamAdapter exists, apply mpParamsMgr to CamAdapter;
    //      otherwise it will be applied when CamAdapter is created.
    {
        sp<ICamAdapter> pCamAdapter = mpCamAdapter;
        if  ( pCamAdapter != 0 ) {
            status = pCamAdapter->setParameters();
        }
    }

lbExit:
    return  status;
}


/******************************************************************************
 *  get stereo mode
 *  currently used for thermal policy decision
 ******************************************************************************/
int32_t
StereoCam1Device::
getStereoMode(
)
{
    int32_t value = 0;
    if(::strcmp(mpParamsMgr->getStr(
            MtkCameraParameters::KEY_STEREO_REFOCUS_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable stereo capture");
        value |= E_STEREO_FEATURE_CAPTURE;
    }
    if(::strcmp(mpParamsMgr->getStr(
            MtkCameraParameters::KEY_STEREO_VSDOF_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable vsdof");
        value |= E_STEREO_FEATURE_VSDOF;
    }
    if(::strcmp(mpParamsMgr->getStr(
            MtkCameraParameters::KEY_STEREO_DENOISE_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable denoise");
        value |= E_STEREO_FEATURE_DENOISE;
    }
    if(::strcmp(mpParamsMgr->getStr(
            MtkCameraParameters::KEY_STEREO_3RDPARTY_MODE), MtkCameraParameters::ON) == 0)
    {
        MY_LOGD("enable 3party");
        value |= E_STEREO_FEATURE_THIRD_PARTY;
    }
    MY_LOGD("Stereo mode(%d)", value);
    return value;
}

/******************************************************************************
 *  enter thermal policy
 *
 ******************************************************************************/
status_t
StereoCam1Device::
enterThermalPolicy(
)
{
    switch(getStereoMode()){
        case E_STEREO_FEATURE_CAPTURE | E_STEREO_FEATURE_VSDOF:
        case E_STEREO_FEATURE_THIRD_PARTY:
            MY_LOGD("enable thermal policy 03");
            system("/system/bin/thermal_manager thermal_policy_03 1");
            break;
        case E_STEREO_FEATURE_DENOISE:
            MY_LOGD("enable thermal policy 04");
            system("/system/bin/thermal_manager thermal_policy_04 1");
            break;
        default:
            MY_LOGW("feature combination not supported:%d, use default policy", getStereoMode());
    }
    return OK;
}

/******************************************************************************
 *  exit thermal policy
 *
 ******************************************************************************/
status_t
StereoCam1Device::
exitThermalPolicy(
)
{
    MY_LOGD("disable thermal policies");
    system("/system/bin/thermal_manager thermal_policy_03 0");
    system("/system/bin/thermal_manager thermal_policy_04 0");
    return OK;
}