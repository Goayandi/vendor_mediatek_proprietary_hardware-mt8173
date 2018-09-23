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
#ifndef _STEREO_COMMON_H_
#define _STEREO_COMMON_H_

#define DUMP_START_CAPTURE  3000
#define WITH16ALIGN true
#define STEREO_PROPERTY_PREFIX  "debug.STEREO."

#include <stdint.h>
#include <math.h>
#include <mtkcam/def/common.h>
#include <cutils/properties.h>
//#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Format.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/drv/IHalSensor.h>
#include "stereo_setting_provider.h"
#include "StereoArea.h"

using namespace NSCam;

namespace StereoHAL {

enum ENUM_STEREO_SCENARIO
{
    eSTEREO_SCENARIO_UNKNOWN,   //sensor scenario: SENSOR_SCENARIO_ID_UNNAMED_START
    eSTEREO_SCENARIO_PREVIEW,   //sensor scenario: SENSOR_SCENARIO_ID_NORMAL_PREVIEW
    eSTEREO_SCENARIO_RECORD,    //sensor scenario: SENSOR_SCENARIO_ID_NORMAL_VIDEO
    eSTEREO_SCENARIO_CAPTURE    //sensor scenario: SENSOR_SCENARIO_ID_NORMAL_CAPTURE
};

/**
 * \brief Get stereo scenario
 * \details Note that record sensor scenario is unsupported
 *
 * \param sensorCenario Sensor scenario
 * \return sensor scenario
 */
inline ENUM_STEREO_SCENARIO getStereoSenario(int sensorCenario)
{
    switch(sensorCenario) {
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
            return eSTEREO_SCENARIO_RECORD;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
            return eSTEREO_SCENARIO_CAPTURE;
            break;
        default:
            break;
    }

    return eSTEREO_SCENARIO_UNKNOWN;
}

/**
 * \brief Get sensor scenario
 * \details Get sensor scenario, note that preview and record both use preview scenario.
 *
 * \param eScenario Stereo scenario
 * \return sensor scenario
 */
inline int getSensorSenario(ENUM_STEREO_SCENARIO eScenario)
{
    switch(eScenario) {
        case eSTEREO_SCENARIO_PREVIEW:
        case eSTEREO_SCENARIO_RECORD:
            return SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
            break;
        // case eSTEREO_SCENARIO_RECORD:
        //     return SENSOR_SCENARIO_ID_NORMAL_VIDEO;
        //     break;
        case eSTEREO_SCENARIO_CAPTURE:
            return SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
            break;
        default:
            break;
    }

    return SENSOR_SCENARIO_ID_UNNAMED_START;
}

enum ENUM_STEREO_SENSOR
{
    eSTEREO_SENSOR_UNKNOWN,
    eSTEREO_SENSOR_MAIN1,
    eSTEREO_SENSOR_MAIN2
};

typedef struct AF_WIN_COORDINATE_STRUCT
{
    MINT af_win_start_x;
    MINT af_win_start_y;
    MINT af_win_end_x;
    MINT af_win_end_y;

    inline  AF_WIN_COORDINATE_STRUCT()
            : af_win_start_x(0)
            , af_win_start_y(0)
            , af_win_end_x(0)
            , af_win_end_y(0)
            {
            }

    inline  AF_WIN_COORDINATE_STRUCT(MINT startX, MINT startY, MINT endX, MINT endY)
            : af_win_start_x(startX)
            , af_win_start_y(startY)
            , af_win_end_x(endX)
            , af_win_end_y(endY)
            {
            }

    inline  MPoint centerPoint()
            {
                return MPoint((af_win_start_x+af_win_end_x)/2, (af_win_start_y+af_win_end_y)/2);
            }

} *P_AF_WIN_COORDINATE_STRUCT;

/**
* \brief Diff two time
* \details Diff two time, sample code:
*           struct timespec t_start, t_end, t_result;
*           clock_gettime(CLOCK_MONOTONIC, &t_start);
*           ...
*           clock_gettime(CLOCK_MONOTONIC, &t_end);
*           t_result = timeDiff(t_start, t_end);
*           ALOGD("Runnning Time: %lu.%.9lu", t_result.tv_sec, t_result.tv_nsec);
*
* \param timespec Start of the time
* \param timespec End of the time
*
* \return Diff of two times
*/
inline struct timespec timeDiff( struct timespec start, struct timespec end)
{
    struct timespec t_result;

    if( ( end.tv_nsec - start.tv_nsec ) < 0) {
        t_result.tv_sec = end.tv_sec - start.tv_sec - 1;
        t_result.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        t_result.tv_sec = end.tv_sec - start.tv_sec;
        t_result.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return t_result;
}

/**
 * \brief Check system property's value, only for number values
 * \details Check system property's value, only for number values
 *
 * \param PROPERTY_NAME The property to query, e.g. "debug.STEREO.enable_verify"
 * \param HAS_DEFAULT If ture, refer to DEFAULT value
 * \param DEFAULT Default value of the property. Default is 0.
 * \return -1: property not been set; otherwise the property value; if HAS_DEFAULT return >= 0
 */
inline int checkStereoProperty(const char *PROPERTY_NAME, const bool HAS_DEFAULT=true, const int DEFAULT=0)
{
    char val[PROPERTY_VALUE_MAX];
    ::memset(val, 0, sizeof(char)*PROPERTY_VALUE_MAX);

    int len = 0;
    if(HAS_DEFAULT) {
        char strDefault[PROPERTY_VALUE_MAX];
        sprintf(strDefault, "%d", DEFAULT);
        len = property_get(PROPERTY_NAME, val, strDefault);
    } else {
        len = property_get(PROPERTY_NAME, val, NULL);
    }

    if(len <= 0) {
        return -1; //property not exist
    }

    //return (!strcmp(val, "1"));
    return atoi(val);
}

/**
 * \brief Set system property
 *
 * \param PROPERTY_NAME Property to set
 * \param val value of the property
 *
 * \return true if success
 */
inline bool setProperty(const char *PROPERTY_NAME, int val)
{
    if(NULL == PROPERTY_NAME) {
        return false;
    }

    char value[PROPERTY_VALUE_MAX];
    sprintf(value, "%d", val);
    int ret = property_set(PROPERTY_NAME, value);

    return (0 == ret);
}

};
#endif