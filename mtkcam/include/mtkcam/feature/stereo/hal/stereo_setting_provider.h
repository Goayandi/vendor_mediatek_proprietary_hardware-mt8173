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
#ifndef _STEREO_SETTING_PROVIDER_H_
#define _STEREO_SETTING_PROVIDER_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/common.h>                // For property
#include <string>
#include <camera_custom_stereo.h>

using namespace NSCam;
using namespace android;

//NOTICE: property has 31 characters limitation
#define PROPERTY_ENABLE_LOG         STEREO_PROPERTY_PREFIX"log"
#define PERPERTY_PASS1_LOG          PROPERTY_ENABLE_LOG".Pass1"
#define PERPERTY_DEPTHMAP_NODE_LOG  PROPERTY_ENABLE_LOG".DepthMapNode"
#define PERPERTY_BOKEH_NODE_LOG     PROPERTY_ENABLE_LOG".BokehNode"
#define PERPERTY_BMDENOISE_NODE_LOG PROPERTY_ENABLE_LOG".BMDenoiseNode"
#define PERPERTY_DIT_NODE_LOG       PROPERTY_ENABLE_LOG".DITNode"
#define PERPERTY_JENC_NODE_LOG      PROPERTY_ENABLE_LOG".JpgEncNode"
#define PERPERTY_VENC_NODE_LOG      PROPERTY_ENABLE_LOG".VEncNode"
#define PROPERTY_ENABLE_PROFILE_LOG PROPERTY_ENABLE_LOG".Profile"

/**************************************************************************
 *                      D E F I N E S / M A C R O S                       *
 **************************************************************************/

/**************************************************************************
 *     E N U M / S T R U C T / T Y P E D E F    D E C L A R A T I O N     *
 **************************************************************************/

struct STEREO_PARAMS_T
{
    std::string jpsSize;          // stereo picture size
    std::string jpsSizesStr;      // supported stereo picture size
    std::string refocusSize;      // refocus picture size
    std::string refocusSizesStr;  // supported refocus picture size
};

enum ENUM_STEREO_FEATURE_MODE
{
    E_STEREO_FEATURE_CAPTURE      = 1,
    E_STEREO_FEATURE_VSDOF        = 1<<2,
    E_STEREO_FEATURE_DENOISE      = 1<<3,
    E_STEREO_FEATURE_THIRD_PARTY  = 1<<4,
};

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *        P U B L I C    F U N C T I O N    D E C L A R A T I O N         *
 **************************************************************************/

/**************************************************************************
 *                   C L A S S    D E C L A R A T I O N                   *
 **************************************************************************/

/**
 * \brief This class provides static stereo settings
 * \details Make sure you have turned on sensors before using any sensor related API
 *
 */
class StereoSettingProvider
{
public:
    /**
     * \brief Get stereo sensor index
     * \details In most of the time, main1 index is 0, main2 index is 2.
     *
     * \param  main1 sensor index
     * \param  main2 sensor index
     * \param  stereo profile
     *
     * \return true if successfully get both index; otherwise false
     * \see stereoProfile()
     */
    static bool getStereoSensorIndex(int32_t &main1Idx, int32_t &main2Idx, ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());
    /**
     * \brief Get sensor device index
     * \details In most of the time, main1 index is 1, main2 index is 4.
     *
     * \param  main1 Saves the device index of main stereo sensor
     * \param  main2 Saves the device index of another stereo sensor
     * \param  stereo profile
     *
     * \return true if successfully get both index; otherwise false
     * \see stereoProfile()
     */
    static bool getStereoSensorDevIndex(int32_t &main1DevIdx, int32_t &main2DevIdx, ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());

    /**
     * \brief Get image ratio
     *
     * \return Ratio of image
     */
    static ENUM_STEREO_RATIO imageRatio() { return m_imageRatio; }

    /**
     * \brief Get De-Noise setting
     * \return True if running de-noise feature
     */
    static bool isDeNoise() { return (m_stereoFeatureMode & E_STEREO_FEATURE_DENOISE); }

    /**
     * \brief Get 3rd-Party setting
     * \return True if running de-noise feature
     */
    static bool is3rdParty() { return (m_stereoFeatureMode & E_STEREO_FEATURE_THIRD_PARTY); }

    /**
     * \brief Get stereo profile
     * \details There could be more than one set of stereo camera on the device,
     *          different profile has different sensor id, baseline, FOV, sizes, etc.
     * \return Stereo profile
     */
    static ENUM_STEREO_SENSOR_PROFILE stereoProfile() { return m_stereoProfile; }

    /**
     * \brief Check if the device has hardware feature extraction component
     * \return true if the device has hardware feature extraction component
     */
    static bool hasHWFE();

    /**
     * \brief Get FE/FM block size
     * \details Block sizes are different in each FE/FM stage
     *
     * \param FE_MODE Block size of mode 0: 32, mode 1: 16, mode 2: 8
     * \return Block size
     */
    static MUINT32 fefmBlockSize(const int FE_MODE);

    /**
     * \brief Get FOV(field of view) of stereo sensors
     *
     * \param mainFOV FOV of main1 sensor
     * \param main2FOV FOV of main2 sensor
     * \param profile Profile of stereo camera, can be rear-rear or front-front
     *
     * \return true if success
     * \see getStereoFOV
     */
    static bool getStereoCameraFOV(SensorFOV &mainFOV, SensorFOV &main2FOV, ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());

    /**
     * \brief Get target FOV(field of view) of stereo sensors.
     * \details Target FOV is the FOV for real output, usally they are the same as Camera FOV.
     *          If we need to pre-crop main1 image, we can change target FOV.
     *
     * \param mainFOV Target FOV of main1 sensor
     * \param main2FOV Target FOV of main2 sensor
     * \param profile Profile of stereo camera, can be rear-rear or front-front
     *
     * \return true if success
     * \see getStereoFOV
     */
    static bool getStereoCameraTargetFOV(SensorFOV &mainFOV, SensorFOV &main2FOV, ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());

    /**
     * \brief Get FOV ratio of main1/main2
     * \details The ratio is decided by the difference of two sensors:
     *          diff >= 20, ratio = 1.4
     *          15 <= diff < 20, ratio = 1.3
     *          10 <= diff < 15, ratio = 1.2
     *          5 <= diff < 10, ratio = 1.1
     *          diff < 5, ratio = 1.0
     *
     * \param stereoProfile Profile of stereo camera, can be rear-rear or front-front
     *
     * \return FOV ratio
     */
    static float getStereoCameraFOVRatio(ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());  //main2_fov / main1_fov

    /**
     * \brief Get module rotation of current stereo profile
     * \details User needs to assign stereo profile first
     *
     * \param profile Profile of stereo camera, can be rear-rear or front-front
     *
     * \return 0, 90, 180, 270 clockwise degree
     * \see stereoProfile()
     */
    static ENUM_ROTATION getModuleRotation(ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());

    /**
     * \brief Get sensor relative position
     * \return 0: main-main2 (main in L)
     *         1: main2-main (main in R)
     */
    static ENUM_STEREO_SENSOR_RELATIVE_POSITION getSensorRelativePosition(ENUM_STEREO_SENSOR_PROFILE profile=stereoProfile());

    /**
     * \brief Query if the sensor is AF or FF
     *
     * \param SENSOR_INDEX sensor index
     * \return true if the sensor is AF
     */
    static bool isSensorAF(const int SENSOR_INDEX);

    /**
     * \brief Enable stereo log, each node can decide to log or not
     * \details This equals setprop debug.STEREO.log 1
     * \return true if success
     * \see disableLog()
     */
    static bool enableLog();

    /**
     * \brief Enable log by a property
     * \details This equals setprop LOG_PROPERTY_NAME 1
     *
     * \param LOG_PROPERTY_NAME Log property to enable
     * \return true if success
     */
    static bool enableLog(const char *LOG_PROPERTY_NAME);

    /**
     * \brief Disable stereo log, each node can decide to log or not
     * \details This equals setprop debug.STEREO.log 1
     * \return true if success
     * \see enableLog()
     */
    static bool disableLog();   //Globally disable log

    /**
     * \brief Check if global log is enabled
     * \details This equals getprop debug.STEREO.log
     * \return true if log is enabled
     */
    static bool isLogEnabled();

    /**
     * \brief Check if a log property is enabled or not
     * \details This equals getprop LOG_PROPERTY_NAME
     *
     * \param LOG_PROPERTY_NAME The log property to check
     * \return true if log is enabled
     */
    static bool isLogEnabled(const char *LOG_PROPERTY_NAME);   //Check log status of each node, refers to global log switch

    /**
     * \brief Check is profile log is enabled or not
     * \details This equals getprop debug.STEREO.Profile
     * \return true if profile log is enabled
     */
    static bool isProfileLogEnabled(); //Check if global profile log is enabled

    /**
     * \brief Get maximum extra data buffer size
     * \details We provides this API for AP to pass extra data buffer to middleware,
     *          NOTICE: if we need to add new data to extra data, or change current data size,
     *                  we need to review the returned size again.
     * \return Estimated size of extra data
     */
    static MUINT32 getExtraDataBufferSizeInBytes();

    /**
     * \brief Get MAX size of warping matrix output by N3D HAL
     * \details This size is for each sensor
     * \return Maxium warping matrix in bytes
     */
    static MUINT32 getMaxWarpingMatrixBufferSizeInBytes();

    /**
     * \brief Provides stereo picture size
     * \details This API provides picture size for AP to save image.
     *          The size is encoded in string.
     *
     * \param param Output parameters
     * \return true if success
     */
    static bool getStereoParams(STEREO_PARAMS_T &param);

    /**
     * \brief Get sensor output format in raw domain
     * \details User should power on the sensor before using this API
     * \param SENSOR_INDEX Sensor index, 0 for main sensor
     * \return enum
     *         {
     *             SENSOR_RAW_Bayer = 0x0,
     *             SENSOR_RAW_MONO,
     *             SENSOR_RAW_RWB,
     *             SENSOR_RAW_FMT_NONE = 0xFF,
     *         };
     * \see getStereoSensorIndex
     */
    static MUINT getSensorRawFormat(const int SENSOR_INDEX);

    /**
     * \brief Check if stereo camera is Bayer+Mono
     * \details Mono sensor will be main2, therefore we don't need to pass sensor index
     * \return true if stereo camera is Bayer+Mono
     */
    static bool isBayerPlusMono();

    /**
     * \brief Set stereo profile
     * \details There could be more than one set of stereo camera on the device,
     *          different profile has different sensor id, baseline, FOV, sizes, etc.
     *
     * \param profile Stereo profile
     */
    static void setStereoProfile(ENUM_STEREO_SENSOR_PROFILE profile);

    /**
     * \brief Set image ratio
     *
     * \param ratio Image ratio
     */
    static void setImageRatio(ENUM_STEREO_RATIO ratio);

    /**
     * \brief Set stereo feature mode
     * \details Stereo feature mode can be combination of ENUM_STEREO_FEATURE_MODE, default is -1
     */
    static void setStereoFeatureMode(MINT32 stereoMode);

    /**
     * \brief Get crop ratio if sensor FOV is different to target FOV
     * \return Crop ratio, default is 1.0f
     */
    static float getFOVCropRatio();

    /**
     * \brief Get current shot mode
     * \return Shot mode, currenly only eShotMode_ZsdShot is returned
     */
    static EShotMode getShotMode();

protected:

private:
    static void _updateImageSettings();

private:
    static ENUM_STEREO_SENSOR_PROFILE   m_stereoProfile;
    static ENUM_STEREO_RATIO            m_imageRatio;
    static MINT32                       m_stereoFeatureMode;
};

#endif  // _STEREO_SETTING_PROVIDER_H_

