/********************************************************************************************
 *     LEGAL DISCLAIMER
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION,
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "StereoSettingProvider"

#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>

#include <sstream>  //For ostringstream

#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/def/ImageFormat.h>
#include <camera_custom_stereo.h>       // For CUST_STEREO_* definitions.

#include <cutils/properties.h>

#if defined(__func__)
#undef __func__
#endif
#define __func__ __FUNCTION__

#ifndef GTEST
#define MY_LOGD(fmt, arg...)    CAM_LOGD("[%s]" fmt, __func__, ##arg)
#define MY_LOGI(fmt, arg...)    CAM_LOGI("[%s]" fmt, __func__, ##arg)
#define MY_LOGW(fmt, arg...)    CAM_LOGW("[%s] WRN(%5d):" fmt, __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    CAM_LOGE("[%s] %s ERROR(%5d):" fmt, __func__,__FILE__, __LINE__, ##arg)
#else
#define MY_LOGD(fmt, arg...)    printf("[D][%s]" fmt"\n", __func__, ##arg)
#define MY_LOGI(fmt, arg...)    printf("[I][%s]" fmt"\n", __func__, ##arg)
#define MY_LOGW(fmt, arg...)    printf("[W][%s] WRN(%5d):" fmt"\n", __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    printf("[E][%s] %s ERROR(%5d):" fmt"\n", __func__,__FILE__, __LINE__, ##arg)
#endif

#define MY_LOGV_IF(cond, arg...)    if (cond) { MY_LOGV(arg); }
#define MY_LOGD_IF(cond, arg...)    if (cond) { MY_LOGD(arg); }
#define MY_LOGI_IF(cond, arg...)    if (cond) { MY_LOGI(arg); }
#define MY_LOGW_IF(cond, arg...)    if (cond) { MY_LOGW(arg); }
#define MY_LOGE_IF(cond, arg...)    if (cond) { MY_LOGE(arg); }

#define FUNC_START          MY_LOGD_IF(LOG_ENABLED, "+")
#define FUNC_END            MY_LOGD_IF(LOG_ENABLED, "-")

#include <mtkcam/utils/std/Log.h>

using namespace StereoHAL;

ENUM_STEREO_SENSOR_PROFILE StereoSettingProvider::m_stereoProfile = STEREO_SENSOR_PROFILE_REAR_REAR;
ENUM_STEREO_RATIO StereoSettingProvider::m_imageRatio = eRatio_Default;
MINT32 StereoSettingProvider::m_stereoFeatureMode = -1;

bool
StereoSettingProvider::getStereoSensorIndex(int32_t &main1Idx, int32_t &main2Idx, ENUM_STEREO_SENSOR_PROFILE profile)
{
    const int SENSOR_COUNT = MAKE_HalSensorList()->queryNumberOfSensors();
    if(2 == SENSOR_COUNT) {
        // MY_LOGW("Only two sensors were found on the device");
        main1Idx = 0;
        main2Idx = 1;
        return true;
    }

    return getStereoSensorID(profile, main1Idx, main2Idx);
}

bool
StereoSettingProvider::getStereoSensorDevIndex(int32_t &main1DevIdx, int32_t &main2DevIdx, ENUM_STEREO_SENSOR_PROFILE profile)
{
    int32_t main1Idx = 0;
    int32_t main2Idx = 0;
    if (!getStereoSensorIndex(main1Idx, main2Idx, profile)) {
        return false;
    }
    IHalSensorList *sensorList = MAKE_HalSensorList();
    if (NULL == sensorList) {
        return false;
    }

    main1DevIdx = sensorList->querySensorDevIdx(main1Idx);
    main2DevIdx = sensorList->querySensorDevIdx(main2Idx);
//    MY_LOGD_IF(isLogEnabled(), "Main sensor DEV idx %d, Main2 sensor DEV idx %d", main1DevIdx, main2DevIdx);

    return true;
}

void
StereoSettingProvider::setStereoProfile(ENUM_STEREO_SENSOR_PROFILE profile)
{
    m_stereoProfile = profile;
    _updateImageSettings();
}

void
StereoSettingProvider::setImageRatio(ENUM_STEREO_RATIO ratio)
{
    m_imageRatio = ratio;
    _updateImageSettings();
}

void
StereoSettingProvider::setStereoFeatureMode(MINT32 stereoMode)
{
    m_stereoFeatureMode = stereoMode;
    _updateImageSettings();
}

bool
StereoSettingProvider::hasHWFE()
{
    static bool _hasHWFE = true;
    return _hasHWFE;
}

MUINT32
StereoSettingProvider::fefmBlockSize(const int FE_MODE)
{
    switch(FE_MODE)
    {
        case 0:
           return 32;
            break;
        case 1:
           return 16;
            break;
        case 2:
           return 8;
            break;
        default:
            break;
    }

    return 0;
}

bool
StereoSettingProvider::getStereoCameraFOV(SensorFOV &mainFOV, SensorFOV &main2FOV, ENUM_STEREO_SENSOR_PROFILE profile)
{
    int main1Idx, main2Idx;
    getStereoSensorIndex(main1Idx, main2Idx, profile);
    mainFOV  = FOV_LIST[main1Idx];
    main2FOV = FOV_LIST[main2Idx];
    MY_LOGD_IF(isLogEnabled(), "FOV: %.1f,%.1f / %.1f,%.1f",
                                mainFOV.fov_horizontal, mainFOV.fov_vertical,
                                main2FOV.fov_horizontal, main2FOV.fov_vertical);
    return true;
}

bool
StereoSettingProvider::getStereoCameraTargetFOV(SensorFOV &mainFOV, SensorFOV &main2FOV, ENUM_STEREO_SENSOR_PROFILE profile)
{
    int main1Idx, main2Idx;
    getStereoSensorIndex(main1Idx, main2Idx, profile);
    mainFOV  = TARGET_FOV_LIST[main1Idx];
    main2FOV = TARGET_FOV_LIST[main2Idx];
    MY_LOGD_IF(isLogEnabled(), "FOV: %.1f,%.1f / %.1f,%.1f",
                                mainFOV.fov_horizontal, mainFOV.fov_vertical,
                                main2FOV.fov_horizontal, main2FOV.fov_vertical);
    return true;
}

float
StereoSettingProvider::getStereoCameraFOVRatio(ENUM_STEREO_SENSOR_PROFILE profile)
{
    static float fovRatio = 0.0f;
    if(0.0f == fovRatio) {
        SensorFOV main1TargetFOV, main2TargetFOV;
        getStereoCameraTargetFOV(main1TargetFOV, main2TargetFOV, profile);

        const int FOV_DIFF = (int)fabs(main2TargetFOV.fov_horizontal - main1TargetFOV.fov_horizontal);
        if(FOV_DIFF >= 20) {
            fovRatio = 1.4f;
        } else if(FOV_DIFF >= 15) {
            fovRatio = 1.3f;
        } else if(FOV_DIFF >= 10) {
            fovRatio = 1.2f;
        } else if(FOV_DIFF >= 5) {
            fovRatio = 1.1f;
        } else {
            fovRatio = 1.0f;
        }
    }

    return fovRatio;
}

ENUM_ROTATION
StereoSettingProvider::getModuleRotation(ENUM_STEREO_SENSOR_PROFILE profile)
{
    int main1Index, main2Index;
    StereoSettingProvider::getStereoSensorIndex(main1Index, main2Index, profile);

    return MODULE_ROTATION[main1Index];
}

ENUM_STEREO_SENSOR_RELATIVE_POSITION
StereoSettingProvider::getSensorRelativePosition(ENUM_STEREO_SENSOR_PROFILE profile)
{
    return getSensorRelation(profile);
}

bool
StereoSettingProvider::isSensorAF(const int SENSOR_INDEX)
{
    //TODO: query from sensor instead of hard-coding
    return SENSOR_AF[SENSOR_INDEX];
}

bool
StereoSettingProvider::enableLog()
{
    return setProperty(PROPERTY_ENABLE_LOG, 1);
}

bool
StereoSettingProvider::enableLog(const char *LOG_PROPERTY_NAME)
{
    return setProperty(PROPERTY_ENABLE_LOG, 1) &&
           setProperty(LOG_PROPERTY_NAME, 1);
}

bool
StereoSettingProvider::disableLog()
{
    return setProperty(PROPERTY_ENABLE_LOG, 0);
}

bool
StereoSettingProvider::isLogEnabled()
{
    return (checkStereoProperty(PROPERTY_ENABLE_LOG, true, 0) == 1);
}

bool
StereoSettingProvider::isLogEnabled(const char *LOG_PROPERTY_NAME)
{
    return isLogEnabled() && (checkStereoProperty(LOG_PROPERTY_NAME, true, 0) == 1);
}

bool
StereoSettingProvider::isProfileLogEnabled()
{
    return isLogEnabled() || (checkStereoProperty(PROPERTY_ENABLE_PROFILE_LOG, true, 0) == 1);
}

MUINT32
StereoSettingProvider::getExtraDataBufferSizeInBytes()
{
    return 32768;
}

MUINT32
StereoSettingProvider::getMaxWarpingMatrixBufferSizeInBytes()
{
    return 100 * sizeof(MFLOAT);
}

bool
StereoSettingProvider::getStereoParams(STEREO_PARAMS_T &OutData)
{
    std::ostringstream stringStream;
    MSize szOutput = StereoSizeProvider::getInstance()->getBufferSize(E_MV_Y_LARGE);
    stringStream << szOutput.w << "x" << szOutput.h;
    OutData.jpsSize     = stringStream.str();
    OutData.jpsSizesStr = stringStream.str();

    std::ostringstream oss;
    MSize szCap = StereoSizeProvider::getInstance()->captureImageSize();
    oss << szCap.w << "x" << szCap.h;
    OutData.refocusSize = oss.str();

    //All sizes must be 16-aligned
    if( isDeNoise() ) {
        CAPTURE_SIZE_T::const_iterator it = CAPTURE_SIZES.find( StereoSettingProvider::imageRatio() );
        if(it != CAPTURE_SIZES.end() &&
           it->second.size() > 1)
        {
            SIZE_LIST_T::const_iterator sizeIt = it->second.begin();
            for(++sizeIt; sizeIt != it->second.end(); ++sizeIt) {
                oss << "," << sizeIt->w << "x" << sizeIt->h;
            }
        }
    }

    OutData.refocusSizesStr = oss.str();

    return true;
}

MUINT
StereoSettingProvider::getSensorRawFormat(const int SENSOR_INDEX)
{
    IHalSensorList *sensorList = MAKE_HalSensorList();
    if (NULL == sensorList) {
        MY_LOGE("Cannot get sensor list");
        return SENSOR_RAW_FMT_NONE;
    }

    int32_t sensorCount = sensorList->queryNumberOfSensors();
    if(SENSOR_INDEX >= sensorCount) {
        MY_LOGD("Sensor index should be <= %d", sensorCount-1);
        return SENSOR_RAW_FMT_NONE;
    }

    SensorStaticInfo sensorStaticInfo;
    memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
    int sendorDevIndex = sensorList->querySensorDevIdx(SENSOR_INDEX);
    sensorList->querySensorStaticInfo(sendorDevIndex, &sensorStaticInfo);

    // for dev
    char cForceMain2Type[PROPERTY_VALUE_MAX];
    ::property_get("debug.STEREO.debug.main2Mono", cForceMain2Type, "0");
    if( (atoi(cForceMain2Type) == 1) && (SENSOR_INDEX == 2)){
        MY_LOGD("force main2 to be MONO sensor");
        sensorStaticInfo.rawFmtType = SENSOR_RAW_MONO;
    }

    return sensorStaticInfo.rawFmtType;
}

bool
StereoSettingProvider::isBayerPlusMono()
{
    int32_t main1Idx, main2Idx;
    getStereoSensorIndex(main1Idx, main2Idx, m_stereoProfile);
    return (SENSOR_RAW_MONO == getSensorRawFormat(main2Idx));
}

void
StereoSettingProvider::_updateImageSettings()
{
    if( isDeNoise() ) {
        StereoSizeProvider::getInstance()->__updateBMDeNoiseSizes();
    }

    StereoSizeProvider::getInstance()->__updateCaptureImageSize();
}

float
StereoSettingProvider::getFOVCropRatio()
{
    float ratio = 1.0f;
    SensorFOV main1FOV, main2FOV;
    StereoSettingProvider::getStereoCameraFOV(main1FOV, main2FOV);

    SensorFOV main1TargetFOV, main2TargetFOV;
    StereoSettingProvider::getStereoCameraTargetFOV(main1TargetFOV, main2TargetFOV);   //Get original FOV from custom setting

    if(main1FOV.fov_horizontal - main1TargetFOV.fov_horizontal > 0.001f) {
        ratio = tan(main1TargetFOV.fov_horizontal /2.0f * M_PI / 180.0f) /
                tan(main1FOV.fov_horizontal       /2.0f * M_PI / 180.0f);
    }

    return ratio;
}

EShotMode
StereoSettingProvider::getShotMode()
{
    return eShotMode_ZsdShot;
}
