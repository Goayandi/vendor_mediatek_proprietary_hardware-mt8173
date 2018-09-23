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

#define LOG_TAG "MtkCam/HwUtils"
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/hw/CamManager.h>
#include "ScenarioControl.h"
#include <bandwidth_control.h>
#include <dlfcn.h>
//
#include <string.h>
#include <cutils/properties.h>
#include <utils/KeyedVector.h>
//
#ifdef ENABLE_PERFSERVICE
#include <perfservicenative/PerfServiceNative.h>
#endif
//
#include <camera_custom_scenario_control.h>
//
#ifdef ENABLE_STEREO_PERFSERVICE
#include <camera_custom_stereo.h>
#endif

using namespace android;
using namespace NSCam;
using namespace NSCam::Utils;

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


/******************************************************************************
 *
 ******************************************************************************/
#define DUMP_SCENARIO_PARAM(_id, _str, _param)                  \
    do{                                                         \
        MY_LOGD_IF(1, "(id:%d) %s: scenario %d: size %dx%d@%d feature 0x%x", \
                _id,                                            \
                _str,                                           \
                _param.scenario,                                \
                _param.sensorSize.w, _param.sensorSize.h,       \
                _param.sensorFps,                               \
                _param.featureFlag                              \
                );                                              \
    } while(0)

#ifdef ENABLE_PERFSERVICE
class PerfService
    : public android::RefBase
{
#define PERF_SERVICE_LIB                "/vendor/lib/libperfservicenative.so"
public:
                                    PerfService()
                                        : mpfRegScn(NULL)
                                        , mpfUnregScn(NULL)
                                        , mpfRegScnConfig(NULL)
                                        , mpfEnable(NULL)
                                        , mpfDisable(NULL)
                                        , mpfGetClusterInfo(NULL)
                                    {
                                        mpPerfServiceHandle = ::dlopen(PERF_SERVICE_LIB, RTLD_NOW);
                                        if( ! mpPerfServiceHandle ) {
                                            char const *err_str = ::dlerror();
                                            MY_LOGW("dlopen: %s error=%s",
                                                    PERF_SERVICE_LIB, (err_str ? err_str : "unknown"));
                                        }
                                    }
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  RefBase Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  ////                    Operations.
    virtual void                    onLastStrongRef(const void* /*id*/) {
                                        if( mpPerfServiceHandle )
                                            ::dlclose(mpPerfServiceHandle);
                                    }
protected:
    typedef int     (*pFn_RegScn)(void);
    typedef void    (*pFn_UnregScn)(int);
    typedef void    (*pFn_RegScnConfig)(int, int, int ,int ,int, int);
    typedef int     (*pFn_GetClusterInfo)(int, int);
    typedef void    (*pFn_Enable)(int);
    typedef void    (*pFn_Disable)(int);
    void*                           mpPerfServiceHandle;
    pFn_RegScn                      mpfRegScn;
    pFn_UnregScn                    mpfUnregScn;
    pFn_RegScnConfig                mpfRegScnConfig;
    pFn_GetClusterInfo              mpfGetClusterInfo;
    pFn_Enable                      mpfEnable;
    pFn_Disable                     mpfDisable;

public:
    void*                           getFunc(const char* symbol) {
                                        if( ! mpPerfServiceHandle )
                                            return NULL;
                                        void* func = dlsym(mpPerfServiceHandle, symbol);
                                        if( func == NULL )
                                            MY_LOGW("dlsym %s failed, %s", symbol, dlerror());
                                        return func;
                                    }
    int                             userRegScn() {
                                        if( ! mpfRegScn )
                                            mpfRegScn = reinterpret_cast<pFn_RegScn>(getFunc("PerfServiceNative_userRegScn"));
                                        if( mpfRegScn == NULL ) return -1;
                                        return mpfRegScn();
                                    }
    void                            userUnregScn(int handle) {
                                        if( ! mpfUnregScn )
                                            mpfUnregScn = reinterpret_cast<pFn_UnregScn>(getFunc("PerfServiceNative_userUnregScn"));
                                        if( mpfUnregScn == NULL ) return;
                                        mpfUnregScn(handle);
                                    }
    void                            userRegScnConfig(int handle, int cmd, int param_1, int param_2, int param_3, int param_4) {
                                        if( ! mpfRegScnConfig)
                                            mpfRegScnConfig = reinterpret_cast<pFn_RegScnConfig>(getFunc("PerfServiceNative_userRegScnConfig"));
                                        if( mpfRegScnConfig == NULL ) return;
                                        mpfRegScnConfig(handle, cmd, param_1, param_2, param_3, param_4);
                                    }
    int                             getClusterInfo(int cmd, int param_1) {
                                        if( ! mpfGetClusterInfo)
                                            mpfGetClusterInfo = reinterpret_cast<pFn_GetClusterInfo>(getFunc("PerfServiceNative_getClusterInfo"));
                                        if( mpfGetClusterInfo == NULL ) return -1;
                                        return mpfGetClusterInfo(cmd, param_1);
                                    }
    void                            userEnable(int handle) {
                                        if( ! mpfEnable )
                                            mpfEnable = reinterpret_cast<pFn_Enable>(getFunc("PerfServiceNative_userEnable"));
                                        if( mpfEnable == NULL ) return;
                                        mpfEnable(handle);
                                    }
    void                            userDisable(int handle) {
                                        if( ! mpfDisable)
                                            mpfDisable = reinterpret_cast<pFn_Disable>(getFunc("PerfServiceNative_userDisable"));
                                        if( mpfDisable == NULL ) return;
                                        mpfDisable(handle);
                                    }
};
#endif

/******************************************************************************
 *
 ******************************************************************************/
BWC_PROFILE_TYPE mapToBWCProfile(MINT32 const scenario)
{
    switch(scenario)
    {
        case IScenarioControl::Scenario_NormalPreivew:
            return BWCPT_CAMERA_PREVIEW;
        case IScenarioControl::Scenario_ZsdPreview:
            return BWCPT_CAMERA_ZSD;
        case IScenarioControl::Scenario_VideoRecord:
            return BWCPT_VIDEO_RECORD_CAMERA;
        case IScenarioControl::Scenario_Capture:
            return BWCPT_CAMERA_CAPTURE;
        case IScenarioControl::Scenario_ContinuousShot:
            return BWCPT_CAMERA_ICFP;
        case IScenarioControl::Scenario_VideoTelephony:
            return BWCPT_VIDEO_TELEPHONY;
        case IScenarioControl::Scenario_HighSpeedVideo:
            return BWCPT_VIDEO_RECORD_SLOWMOTION;
        default:
            MY_LOGE("not supported scenario %d", scenario);
    }
    return BWCPT_NONE;
}


/******************************************************************************
 *
 ******************************************************************************/
Mutex                               gLock;
DefaultKeyedVector<
    MINT32, wp<ScenarioControl>
    >                               gScenarioControlMap;


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
isMultiOpen()
{
    Mutex::Autolock _l(gLock);
    return gScenarioControlMap.size() > 1;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
isInStereoMode(MINT32 featureFlag)
{
    return FEATURE_CFG_IS_ENABLED(featureFlag, IScenarioControl::FEATURE_VSDOF_PREVIEW) ||
           FEATURE_CFG_IS_ENABLED(featureFlag, IScenarioControl::FEATURE_VSDOF_RECORD) ||
           FEATURE_CFG_IS_ENABLED(featureFlag, IScenarioControl::FEATURE_STEREO_CAPTURE) ||
           FEATURE_CFG_IS_ENABLED(featureFlag, IScenarioControl::FEATURE_BMDENOISE_PREVIEW) ||
           FEATURE_CFG_IS_ENABLED(featureFlag, IScenarioControl::FEATURE_BMDENOISE_CAPTURE);
}

/******************************************************************************
 *
 ******************************************************************************/
sp<IScenarioControl>
IScenarioControl::
create(MINT32 const openId)
{
    Mutex::Autolock _l(gLock);
    sp<ScenarioControl> pControl = NULL;
    ssize_t index = gScenarioControlMap.indexOfKey(openId);
    if( index < 0 ) {
        pControl = new ScenarioControl(openId);
        gScenarioControlMap.add(openId, pControl);
    }
    else {
        MY_LOGW("dangerous, already have user with open id %d", openId);
        pControl = gScenarioControlMap.valueAt(index).promote();
    }
    //
    if( ! pControl.get() ) {
        MY_LOGE("cannot create properly");
    }
    //
    return pControl;
}


/******************************************************************************
 *
 ******************************************************************************/
ScenarioControl::
ScenarioControl(MINT32 const openId)
    : mOpenId(openId)
    , mCurPerfHandle(-1)
{
    mCurParam.scenario = Scenario_None;
    mEngMode = -1;
    if (mEngMode == -1)
    {
        char value[PROPERTY_VALUE_MAX];
        property_get("ro.build.type", value, "eng");
        if (0 == strcmp(value, "eng")) {
            mEngMode = 1;
        }
        else
        {
            mEngMode = 0;
        }
    }
}

/******************************************************************************
 *
 ******************************************************************************/
void
ScenarioControl::
onLastStrongRef(const void* /*id*/)
{
    // reset
    if( mCurParam.scenario != Scenario_None ) {
        exitScenario();
    }
    //
    {
        Mutex::Autolock _l(gLock);
        ssize_t index = gScenarioControlMap.indexOfKey(mOpenId);
        if( index >= 0 ) {
            gScenarioControlMap.removeItemsAt(index);
        }
        else {
            MY_LOGW("dangerous, has been removed (open id %d)", mOpenId);
        }
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::
enterScenario(
    MINT32 const scenario
)
{
    ControlParam param;
    param.scenario = scenario;
    param.sensorSize = mCurParam.sensorSize;
    param.sensorFps = mCurParam.sensorFps;
    param.featureFlag = mCurParam.featureFlag;
    param.videoSize = mCurParam.videoSize;

    return enterScenario(param);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::
enterScenario(
    ControlParam const & param
)
{
    if( mCurParam.scenario != Scenario_None ) {
        MY_LOGD("exit previous setting");
        exitScenario();
    }
    DUMP_SCENARIO_PARAM(mOpenId, "enter:", param);
    //
    BWC_PROFILE_TYPE type = mapToBWCProfile(param.scenario);
    if( type == BWCPT_NONE )
        return BAD_VALUE;
    //
#ifdef ENABLE_PERFSERVICE
    {
        sp<PerfService> pPerf = new PerfService();
        int handle = pPerf->userRegScn();
        //
        if( handle != -1 && mEngMode != 1){
            bool useCpuCtrl = false;
            KeyedVector<ScenInfoKey, int> scenInfo;
            Vector<CpuCtrlParam> cpuCtrlParam;
            //
            scenInfo.clear();
            scenInfo.add(ScenInfoKey::KEY_SCENARIO, param.scenario);
            scenInfo.add(ScenInfoKey::KEY_FEATURE_FLAG, param.featureFlag);
            scenInfo.add(ScenInfoKey::KEY_DEVICE_COUNT, CamManager::getInstance()->getDeviceCount());
            {
                size_t i = 0;
                for( i = 0 ; i < scenInfo.size() ; i++ )
                {
                    MY_LOGD("Scenario Info %d/%d, (key,value)=(%d,%d)",
                            i, scenInfo.size(),
                            scenInfo.keyAt(i),
                            scenInfo.valueAt(i));
                }
            }
            useCpuCtrl = get_cpu_ctrl(scenInfo, cpuCtrlParam);
            //
            switch(param.scenario){
                case Scenario_VideoRecord:{
                    if(FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_ADV_EIS)){
                        MY_LOGD("Enable FHD VR GIS thermal_manager");
                        system("/vendor/bin/thermal_manager thermal_policy_02 1");
                    }
                    //For Save Power
                    if( param.videoSize.h <= 1088 ) //For max 1080p
                    {
#define VDEC_CPU_MIN_SAMPLE_TIME 20000
#define VDEC_CPU_ABOVE_HISPEED_DELAY 80000
                        pPerf->userRegScnConfig(handle, CMD_SET_CPUFREQ_MIN_SAMPLE_TIME , VDEC_CPU_MIN_SAMPLE_TIME, 0, 0, 0);         // quick fall - 20ms
                        pPerf->userRegScnConfig(handle, CMD_SET_CPUFREQ_ABOVE_HISPEED_DELAY, VDEC_CPU_ABOVE_HISPEED_DELAY, 0, 0, 0);  // slow rise - 80ms
                        MY_LOGD("Enable CPU quick fall - 20ms & slow rise - 80ms");
#undef VDEC_CPU_MIN_SAMPLE_TIME
#undef VDEC_CPU_ABOVE_HISPEED_DELAY
                    }
                    //
                    break;
                }
                case Scenario_ZsdPreview:{
#ifdef ENABLE_STEREO_PERFSERVICE
                    std::vector<int> cpu_core;
                    std::vector<int> cpu_frequency_max;
                    std::vector<int> cpu_frequency_min;
                    MBOOL isUseStereoSetting = MFALSE;
                    int numOfCluster = pPerf->getClusterInfo(CMD_GET_CLUSTER_NUM, 0);
                    int coreMax = 0;
                    int freqMax = 0;
                    int freqMin = 0;
                    MY_LOGD("Number of cluster: %d", numOfCluster);
                    //
                    if(FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_VSDOF_PREVIEW))
                    {
                        cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoCamVsdof, ENUM_STEREO_CAM_SCENARIO::eStereoCamPreview);
                        MBOOL result = get_stereo_cam_cpu_frequency(ENUM_STEREO_CAM_FEATURE::eStereoCamVsdof, ENUM_STEREO_CAM_SCENARIO::eStereoCamPreview, cpu_frequency_max, cpu_frequency_min);
                        if(!result)
                        {
                            MY_LOGE("get_stereo_cam_cpu_frequency(vsdof preview) fail");
                            return UNKNOWN_ERROR;
                        }
                        if(cpu_core.size() != numOfCluster &&
                           cpu_frequency_max.size() != numOfCluster &&
                           cpu_frequency_min.size() != numOfCluster)
                        {
                            MY_LOGE("cpu_core/cpu_frequency is not equal to cluster(%d)", numOfCluster);
                            return UNKNOWN_ERROR;
                        }
                        MY_LOGD("feature flag vsdof preview");
                        isUseStereoSetting = MTRUE;
                    }
                    if(FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_VSDOF_RECORD))
                    {
                        cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoCamVsdof, ENUM_STEREO_CAM_SCENARIO::eStereoCamRecord);
                        MBOOL result = get_stereo_cam_cpu_frequency(ENUM_STEREO_CAM_FEATURE::eStereoCamVsdof, ENUM_STEREO_CAM_SCENARIO::eStereoCamRecord, cpu_frequency_max, cpu_frequency_min);
                        if(!result)
                        {
                            MY_LOGE("get_vsdof_cpu_frequency fail");
                            return UNKNOWN_ERROR;
                        }
                        if(cpu_core.size() != numOfCluster &&
                           cpu_frequency_max.size() != numOfCluster &&
                           cpu_frequency_min.size() != numOfCluster)
                        {
                            MY_LOGE("cpu_core/cpu_frequency is not equal to cluster(%d)", numOfCluster);
                            return UNKNOWN_ERROR;
                        }
                        MY_LOGD("feature flag vsdof record");
                        isUseStereoSetting = MTRUE;
                    }
                    if(FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_STEREO_CAPTURE))
                    {
                        cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoCamStereo_Capture, ENUM_STEREO_CAM_SCENARIO::eStereoCamCapture);
                        MBOOL result = get_stereo_cam_cpu_frequency(ENUM_STEREO_CAM_FEATURE::eStereoCamStereo_Capture, ENUM_STEREO_CAM_SCENARIO::eStereoCamCapture, cpu_frequency_max, cpu_frequency_min);
                        if(!result)
                        {
                            MY_LOGE("get_vsdof_cpu_frequency fail");
                            return UNKNOWN_ERROR;
                        }
                        if(cpu_core.size() != numOfCluster &&
                           cpu_frequency_max.size() != numOfCluster &&
                           cpu_frequency_min.size() != numOfCluster)
                        {
                            MY_LOGE("cpu_core/cpu_frequency is not equal to cluster(%d)", numOfCluster);
                            return UNKNOWN_ERROR;
                        }
                        MY_LOGD("feature flag vsdof capture");
                        isUseStereoSetting = MTRUE;
                    }
                    if(FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_BMDENOISE_PREVIEW))
                    {
                        cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoBMDnoise, ENUM_STEREO_CAM_SCENARIO::eStereoCamPreview);
                        MBOOL result = get_stereo_cam_cpu_frequency(ENUM_STEREO_CAM_FEATURE::eStereoBMDnoise, ENUM_STEREO_CAM_SCENARIO::eStereoCamPreview, cpu_frequency_max, cpu_frequency_min);
                        if(!result)
                        {
                            MY_LOGE("get_bmdenoise_cpu_frequency fail");
                            return UNKNOWN_ERROR;
                        }
                        if(cpu_core.size() != numOfCluster &&
                           cpu_frequency_max.size() != numOfCluster &&
                           cpu_frequency_min.size() != numOfCluster)
                        {
                            MY_LOGE("cpu_core/cpu_frequency is not equal to cluster(%d)", numOfCluster);
                            return UNKNOWN_ERROR;
                        }
                        MY_LOGD("feature flag bmdenoise preview");
                        isUseStereoSetting = MTRUE;
                    }
                    if(FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_BMDENOISE_CAPTURE))
                    {
                        cpu_core = get_stereo_cam_cpu_num(ENUM_STEREO_CAM_FEATURE::eStereoBMDnoise, ENUM_STEREO_CAM_SCENARIO::eStereoCamCapture);
                        MBOOL result = get_stereo_cam_cpu_frequency(ENUM_STEREO_CAM_FEATURE::eStereoBMDnoise, ENUM_STEREO_CAM_SCENARIO::eStereoCamCapture, cpu_frequency_max, cpu_frequency_min);
                        if(!result)
                        {
                            MY_LOGE("get_bmdenoise_cpu_frequency fail");
                            return UNKNOWN_ERROR;
                        }
                        if(cpu_core.size() != numOfCluster &&
                           cpu_frequency_max.size() != numOfCluster &&
                           cpu_frequency_min.size() != numOfCluster)
                        {
                            MY_LOGE("cpu_core/cpu_frequency is not equal to cluster(%d)", numOfCluster);
                            return UNKNOWN_ERROR;
                        }
                        MY_LOGD("feature flag bmdenoise capture");
                        isUseStereoSetting = MTRUE;
                        // GPU control
                        int GPULevels = pPerf->GetCapability(CMD_GET_GPU_FREQ_LEVEL_COUNT);
                        pPerf->userRegScnConfig(handle, CMD_SET_GPU_FREQ_MIN, GPULevels-1, 0, 0, 0);
                        MY_LOGD("Manually PerfService Control by Stereo Feature GPU min freq:%d", GPULevels);
                    }
                    if(isUseStereoSetting)
                    {
                        for(int cluster = 0 ; cluster < numOfCluster; cluster ++){
                            coreMax = pPerf->getClusterInfo(CMD_GET_CLUSTER_CPU_NUM, cluster);
                            freqMax = pPerf->getClusterInfo(CMD_GET_CLUSTER_CPU_FREQ_MAX, cluster);

                            if(coreMax == -1 || freqMax == -1){
                                MY_LOGE("Unablde to get cluster info! cluster:%d, coreMax:%d, freqMax:%d", cluster, coreMax, freqMax);
                                break;
                            }

                            coreMax = cpu_core[cluster];
                            if(cpu_frequency_min[cluster] != -1 && cpu_frequency_max[cluster] != -1)
                            {
                                freqMin = cpu_frequency_min[cluster];
                                freqMax = cpu_frequency_max[cluster];
                            }
                            else
                            {
                                freqMin = freqMax;
                            }

                            MY_LOGD("Manually PerfService Control by Stereo Feature - cluster:%d, freqMin:%d, freqMax:%d, coreMax:%d ", cluster, freqMin, freqMax, coreMax);
                            pPerf->userRegScnConfig(handle, CMD_SET_CLUSTER_CPU_CORE_MIN, cluster, coreMax, 0, 0);
                            pPerf->userRegScnConfig(handle, CMD_SET_CLUSTER_CPU_CORE_MAX, cluster, coreMax, 0, 0);
                            pPerf->userRegScnConfig(handle, CMD_SET_CLUSTER_CPU_FREQ_MIN, cluster, freqMin, 0, 0);
                            pPerf->userRegScnConfig(handle, CMD_SET_CLUSTER_CPU_FREQ_MAX, cluster, freqMax, 0, 0);
                        }
                    }
                    else
                    {
                        pPerf->userRegScnConfig(handle, CMD_SET_CLUSTER_CPU_CORE_MIN, 0, 4, 0, 0); // min 4xLL
                    }
#else
                    pPerf->userRegScnConfig(handle, CMD_SET_CLUSTER_CPU_CORE_MIN, 0, 4, 0, 0); // min 4xLL
#endif
                    pPerf->userEnable(handle);
                    MY_LOGD("perfService enable Scenario_ZsdPreview");
                    break;
                }
                default:{
                    // Do nothing
                    break;
                }
            }
            if( useCpuCtrl )
            {
                size_t i = 0;
                for( i = 0 ; i < cpuCtrlParam.size() ; i++ )
                {
                    MY_LOGD("Cpu Ctrl %d/%d, cmd:%d, cluster:%d, arg:(%d,%d,%d)",
                            i, cpuCtrlParam.size(),
                            cpuCtrlParam[i].cmd,
                            cpuCtrlParam[i].cluster,
                            cpuCtrlParam[i].arg1,
                            cpuCtrlParam[i].arg2,
                            cpuCtrlParam[i].arg3);
                    pPerf->userRegScnConfig(handle,
                                            cpuCtrlParam[i].cmd,
                                            cpuCtrlParam[i].cluster,
                                            cpuCtrlParam[i].arg1,
                                            cpuCtrlParam[i].arg2,
                                            cpuCtrlParam[i].arg3);
                }
                pPerf->userEnable(handle);
            }
            //
            mCurPerfHandle = handle;
        }
        else
        {
            MY_LOGW("cannot regscn eng(%d)", mEngMode);
        }
    }
#endif
    //

    if(param.enableBWCControl){
        BWC BwcIns;
        BwcIns.Profile_Change(type,true);
        //
        MUINT32 multiple = 1;
        if (FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_DUAL_PD)) {
            multiple = 2;
        }
        //
        mmdvfs_set(
                type,
                MMDVFS_SENSOR_SIZE,         param.sensorSize.size() * multiple,//param.sensorSize.size(),
                MMDVFS_SENSOR_FPS,          param.sensorFps,
                MMDVFS_CAMERA_MODE_PIP,     isMultiOpen(),
                MMDVFS_CAMERA_MODE_VFB,     FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_VFB),
                MMDVFS_CAMERA_MODE_EIS_2_0, FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_ADV_EIS),
                MMDVFS_CAMERA_MODE_IVHDR,   FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_VHDR),
                MMDVFS_CAMERA_MODE_STEREO,  isInStereoMode(param.featureFlag),
                MMDVFS_PARAMETER_EOF);
    }

    // keep param
    mCurParam = param;
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::
exitScenario()
{
    if( mCurParam.scenario == Scenario_None ) {
        MY_LOGD("already exit");
        return OK;
    }
    DUMP_SCENARIO_PARAM(mOpenId, "exit:", mCurParam);
    BWC_PROFILE_TYPE type = mapToBWCProfile(mCurParam.scenario);
    if( type == BWCPT_NONE )
        return BAD_VALUE;
    //
    if(mCurParam.enableBWCControl){
        BWC BwcIns;
        BwcIns.Profile_Change(type,false);
    }
    //
#ifdef ENABLE_PERFSERVICE
    if( mCurPerfHandle != -1 )
    {
        sp<PerfService> pPerf = new PerfService();
        //
        pPerf->userDisable(mCurPerfHandle);
        pPerf->userUnregScn(mCurPerfHandle);
        //
        mCurPerfHandle = -1;
        MY_LOGD("perfService disable");

        if(FEATURE_CFG_IS_ENABLED(mCurParam.featureFlag, FEATURE_ADV_EIS))
        {
            MY_LOGD("Disable FHD VR GIS thermal_manager");
            system("/vendor/bin/thermal_manager thermal_policy_02 0");
        }
    }
#endif
    //
    // reset param
    mCurParam.scenario = Scenario_None;
    //
    return OK;
}
