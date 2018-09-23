/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

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
*      TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/

/**
* @file eis_hal.cpp
*
* EIS Hal Source File
*
*/

#include <cstdio>
#include <queue>
#include <android/sensor.h>
#include <utils/threads.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include <mtkcam/utils/imgbuf/IIonImageBufferHeap.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/drv/iopipe/CamIO/IHalCamIO.h>

using namespace std;
using namespace android;
using namespace NSCam;
using namespace NSCam::NSIoPipe;
using namespace NSCam::NSIoPipe::NSCamIOPipe;

#include "eis_hal_imp.h"
#include "eis_macro.h"

#include <mtkcam/feature/eis/lmv_hal.h>
#include <mtkcam/utils/sys/SensorListener.h>
#include <mtkcam/aaa/INvBufUtil.h>
#include <camera_custom_nvram.h>
#include <camera_custom_eis.h>

#if 1
#undef  ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_ALWAYS
#define DP_TRACE_CALL()                 ATRACE_CALL()
#define DP_TRACE_BEGIN(name)            ATRACE_BEGIN(name)
#define DP_TRACE_END()                  ATRACE_END()

#else

#define DP_TRACE_CALL()
#define DP_TRACE_BEGIN(name)
#define DP_TRACE_END()

#endif  // CONFIG_FOR_SYSTRACE


/*******************************************************************************
*
********************************************************************************/
#define EIS_HAL_DEBUG

#ifdef EIS_HAL_DEBUG

#undef __func__
#define __func__ __FUNCTION__

#undef  LOG_TAG
#define LOG_TAG "EisHal"
#include <mtkcam/utils/std/Log.h>

#define EIS_LOG(fmt, arg...)    CAM_LOGD("[%s]" fmt, __func__, ##arg)
#define EIS_INF(fmt, arg...)    CAM_LOGI("[%s]" fmt, __func__, ##arg)
#define EIS_WRN(fmt, arg...)    CAM_LOGW("[%s] WRN(%5d):" fmt, __func__, __LINE__, ##arg)
#define EIS_ERR(fmt, arg...)    CAM_LOGE("[%s] %s ERROR(%5d):" fmt, __func__,__FILE__, __LINE__, ##arg)

#else
#define EIS_LOG(a,...)
#define EIS_INF(a,...)
#define EIS_WRN(a,...)
#define EIS_ERR(a,...)
#endif

#define EIS_HAL_NAME "EisHal"
#define intPartShift 8
#define floatPartShift (31 - intPartShift)
#define DEBUG_DUMP_FRAMW_NUM 10
queue<EIS_GyroRecord> emptyQueue;

MAKE_EISHAL_OBJ(0);
MAKE_EISHAL_OBJ(1);
MAKE_EISHAL_OBJ(2);
MAKE_EISHAL_OBJ(3);


const MUINT32 GyroInterval_ms = 10;

MINT32 EisHalImp::mEISInterval = GyroInterval_ms;
MINT32 EisHalImp::mEMEnabled = 0;
MINT32 EisHalImp::mDebugDump = 0;

#define EISO_BUFFER_NUM (30)

/*******************************************************************************
* GYRO Data
********************************************************************************/
static MFLOAT gAccInfo[3] = {0};
static MFLOAT gGyroInfo[3] = {0};


typedef struct EIS_TSRecord_t
{
    MUINT64 id;
    MUINT64 ts;
}EIS_TSRecord;


#define UNREASONABLE_GYRO_VALUE (10.0f)

/*******************************************************************************
* Debug property
********************************************************************************/
#define RECORD_WITHOUT_EIS_ENABLE   (0) //Only used in AIM debug, Must be disable in normal case
#if RECORD_WITHOUT_EIS_ENABLE
static EIS_GyroRecord gGyroRecord[TSRECORD_MAXSIZE];
static EIS_TSRecord gTSRecord[TSRECORD_MAXSIZE];
static EIS_TSRecord gvHDRRecord[TSRECORD_MAXSIZE];
static MUINT32 gvHDRRecordWriteID = 0;
static MUINT32 gTSRecordWriteID = 0;
static MUINT32 gGyroRecordWriteID = 0;
static MINT32 g_AIMDump = 0;
static MINT32 g_GyroOnly = 0;
static MINT32 g_ImageOnly = 0;
static MINT32 g_forecDejello = 1;

#endif

/*******************************************************************************
* User defined
********************************************************************************/

template <typename T>
const T EIS_MAX(const T& a, const T& b)
{
    return a > b ? a : b;
}

template <typename T>
const T EIS_MIN(const T& a, const T& b)
{
    return a < b ? a : b;
}

template <typename T>
const T EIS_FLOOR(const T& x)
{
    return (int)x - ((int)x>x);
}

/*******************************************************************************
*
********************************************************************************/
void subSensorListenser(const MUINT sensorID, ASensorEvent event)
{
    static MUINT32 accCnt = 1;
    static MUINT32 gyroCnt = 1;
    MBOOL bReversed = MFALSE;

    switch(event.type)
    {
        case ASENSOR_TYPE_ACCELEROMETER:
            if (UNLIKELY(EisHalImp::mDebugDump >= 2))
            {
                EIS_LOG("[%u] Acc(%f,%f,%f,%lld)",
                        accCnt++,
                        event.acceleration.x,
                        event.acceleration.y,
                        event.acceleration.z,
                        event.timestamp);
            }

            gAccInfo[0] = event.acceleration.x;
            gAccInfo[1] = event.acceleration.y;
            gAccInfo[2] = event.acceleration.z;

            break;
        case ASENSOR_TYPE_GYROSCOPE:
            if (UNLIKELY(EisHalImp::mDebugDump >= 2))
            {
                EIS_LOG("[%u] Gyro(%f,%f,%f,%lld)",
                        gyroCnt++,
                        event.vector.x,
                        event.vector.y,
                        event.vector.z,
                        event.timestamp);
            }

            gGyroInfo[0] = event.vector.x;
            EISHAL_SwitchIsGyroReverseNotZero(sensorID, bReversed);
            if (UNLIKELY(bReversed))
            {
                gGyroInfo[1] = -event.vector.y;
                gGyroInfo[2] = -event.vector.z;
            }else
            {
                gGyroInfo[1] = event.vector.y;
                gGyroInfo[2] = event.vector.z;
            }

            if ( LIKELY((event.vector.x < UNREASONABLE_GYRO_VALUE) && (event.vector.x > -UNREASONABLE_GYRO_VALUE) &&
                 (event.vector.y < UNREASONABLE_GYRO_VALUE) && (event.vector.y > -UNREASONABLE_GYRO_VALUE) &&
                 (event.vector.z < UNREASONABLE_GYRO_VALUE) && (event.vector.z > -UNREASONABLE_GYRO_VALUE)))
            {
                EIS_GyroRecord tmp;
                tmp.x = event.vector.x;
                if (UNLIKELY(bReversed))
                {
                    tmp.y = -event.vector.y;
                    tmp.z = -event.vector.z;
                }else
                {
                    tmp.y = event.vector.y;
                    tmp.z = event.vector.z;
                }
                tmp.ts = event.timestamp;
                EISHAL_SwitchPushGyroQueue(sensorID,tmp);
                DP_TRACE_CALL();
            }else
            {
                EIS_ERR("Unreasonable gyro data(%f,%f,%f,%lld)", event.vector.x, event.vector.y, event.vector.z, event.timestamp);
                EISHAL_SwitchSetLastGyroTimestampValue(sensorID, event.timestamp);
                EISHAL_SWitchWaitGyroCond_Signal(sensorID);
            }
            break;
        default:
            if (UNLIKELY(EisHalImp::mDebugDump >= 2))
            {
                EIS_WRN("unknown type(%d)",event.type);
            }
    }
}

template<const unsigned int aSensorIdx>
void EisHalObj<aSensorIdx>::EisSensorListener(ASensorEvent event)
{
    subSensorListenser(aSensorIdx,event);
}

/*******************************************************************************
*
********************************************************************************/
EisHal *EisHal::CreateInstance(char const *userName,const MUINT32 &aSensorIdx, MUINT32 MultiSensor)
{
    EIS_LOG("%s",userName);
    return EisHalImp::GetInstance(aSensorIdx);
}

/*******************************************************************************
*
********************************************************************************/
EisHal *EisHalImp::GetInstance(const MUINT32 &aSensorIdx, MUINT32 MultiSensor)
{
    EIS_LOG("sensorIdx(%u)",aSensorIdx);

    switch(aSensorIdx)
    {
        case 0 : return EisHalObj<0>::GetInstance(MultiSensor);
        case 1 : return EisHalObj<1>::GetInstance(MultiSensor);
        case 2 : return EisHalObj<2>::GetInstance(MultiSensor);
        case 3 : return EisHalObj<3>::GetInstance(MultiSensor);
        default :
            EIS_WRN("Current limit is 4 sensors, use 0");
            return EisHalObj<0>::GetInstance(MultiSensor);
    }
}

/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::DestroyInstance(char const *userName)
{
    EIS_LOG("%s",userName);
}

/*******************************************************************************
*
********************************************************************************/
EisHalImp::EisHalImp(const MUINT32 &aSensorIdx, MUINT32 MultiSensor) : EisHal(),mSensorIdx(aSensorIdx), mMultiSensor(MultiSensor)
{
    mUsers = 0;

    //> member variable
    mEisInput_W = 0;
    mEisInput_H = 0;
    mP1Target_W = 0;
    mP1Target_H = 0;
    mSrzOutW = 0;
    mSrzOutH = 0;
    mGpuGridW = 0;
    mGpuGridH = 0;
    mDoEisCount = 0;    //Vent@20140427: Add for EIS GMV Sync Check.
    mCmvX_Int = 0;
    mCmvX_Flt = 0;
    mCmvY_Int = 0;
    mCmvY_Flt = 0;
    mGMV_X = 0;
    mGMV_Y = 0;
    mMVtoCenterX = 0;
    mMVtoCenterY = 0;
    mFrameCnt = 0;
    mIsEisConfig = 0;
    mIsEisPlusConfig = 0;
    mSensorDev = 0;
    mEisSupport = MTRUE;
    mEisPlusCropRatio = 20;
    mGyroEnable = MFALSE;
    mAccEnable  = MFALSE;
    mEisPlusResult.GridX = 0;
    mEisPlusResult.GridY = 0;
    mEisPlusResult.ClipX = 0;
    mEisPlusResult.ClipY = 0;

    memset(&mSensorStaticInfo, 0, sizeof(mSensorStaticInfo));
    memset(&mSensorDynamicInfo, 0, sizeof(mSensorDynamicInfo));

    mTsForAlgoDebug = 0;
    m_pNvram = NULL;
    mGisInputW = 0;
    mGisInputH = 0;
    mSkipWaitGyro = MFALSE;
    mbEMSaveFlag = MFALSE;

    mEISInterval = GyroInterval_ms;
    mEMEnabled = 0;
    mDebugDump = 0;
    m_pLMVHal = NULL;
#if EIS_ALGO_READY
    m_pEisPlusAlg = NULL;
    m_pGisAlg = NULL;

    memset(&mEisAlgoProcData, 0, sizeof(mEisAlgoProcData));
#endif

    mChangedInCalibration = 0;
    mNVRAMRead = MFALSE;
    mSleepTime = 0;
    mtRSTime = 0;
    mbLastCalibration = MTRUE;
    mSensorPixelClock = 0;
    mSensorLinePixel = 0;
    m_pNVRAM_defParameter = NULL;

    m_pEisDbgBuf = NULL;
    m_pEisPlusWorkBuf = NULL;
    m_pGisWorkBuf = NULL;

    // sensor listener
    mpSensorListener = NULL;

    // sensor
    m_pHalSensorList = NULL;
    m_pHalSensor = NULL;


}

/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::Init()
{
    DP_TRACE_CALL();

    //====== Check Reference Count ======
    Mutex::Autolock lock(mLock);
    MUINT32 index;
    if(mUsers > 0)
    {
        android_atomic_inc(&mUsers);
        EIS_LOG("snesorIdx(%u) has %d users",mSensorIdx,mUsers);
        return EIS_RETURN_NO_ERROR;
    }

    MINT32 err = EIS_RETURN_NO_ERROR;

    //====== Dynamic Debug ======
    mEISInterval = GyroInterval_ms;
    mEMEnabled = 0;
    mDebugDump = 0;

    mDebugDump = property_get_int32("debug.eis.dump", mDebugDump);
    mEMEnabled = property_get_int32("debug.eis.EMEnabled", mEMEnabled);
    mEISInterval = property_get_int32("eisrecord.setinterval", GyroInterval_ms);

#if RECORD_WITHOUT_EIS_ENABLE
    g_AIMDump = 0;
    g_GyroOnly = 0;
    g_ImageOnly = 0;
    g_forecDejello = 1;

    g_AIMDump = property_get_int32("debug.eis.AIMDump", g_AIMDump);
    g_GyroOnly = property_get_int32("debug.eis.GyroOnly", g_GyroOnly);
    g_ImageOnly = property_get_int32("debug.eis.ImageOnly", g_ImageOnly);
    g_forecDejello = property_get_int32("debug.eis.forceDejello", g_forecDejello);
#endif

    EISHAL_SwitchClearGyroQueue(mSensorIdx);
    EISHAL_SwitchSetGyroCountZero(mSensorIdx);

    m_pLMVHal = NULL;
    //Init GIS member data
    m_pNvram = NULL;
    mChangedInCalibration = 0;
    mNVRAMRead = MFALSE;
    mSleepTime = 0;
    mtRSTime = 0;

    EISHAL_SwitchSetGyroReverseValue(mSensorIdx, 0);
    EISHAL_SwitchSetLastGyroTimestampValue(mSensorIdx, 0);
    mbLastCalibration = MTRUE;
    mSensorPixelClock = 0;
    mSensorLinePixel = 0;
    mbEMSaveFlag = MFALSE;
    m_pNVRAM_defParameter = new NVRAM_CAMERA_FEATURE_GIS_STRUCT;
    memset(m_pNVRAM_defParameter, 0, sizeof(NVRAM_CAMERA_FEATURE_GIS_STRUCT));
    memset(mRecordParameter, 0, sizeof(mRecordParameter));

#if RECORD_WITHOUT_EIS_ENABLE
    gTSRecordWriteID = 0;
    gGyroRecordWriteID = 0;
    gvHDRRecordWriteID = 0;
    memset(&gTSRecord, 0, sizeof(gTSRecord));
    memset(&gvHDRRecord, 0, sizeof(gvHDRRecord));
    memset(&gGyroRecord, 0, sizeof(gTSRecord));
#endif

    EIS_LOG("(%p)  mSensorIdx(%u) init", this, mSensorIdx);

    //====== Create Sensor Object ======

    m_pHalSensorList = MAKE_HalSensorList();
    if(m_pHalSensorList == NULL)
    {
        EIS_ERR("IHalSensorList::get fail");
        goto create_fail_exit;
    }

    if(EIS_RETURN_NO_ERROR != GetSensorInfo())
    {
        EIS_ERR("GetSensorInfo fail");
        goto create_fail_exit;
    }

    //====== Create EIS Algorithm Object ======

#if EIS_ALGO_READY

    EIS_LOG("TG(%d)",mSensorDynamicInfo.TgInfo);

    m_pEisPlusAlg = MTKEisPlus::createInstance();

    if(m_pEisPlusAlg == NULL)
    {
        EIS_ERR("MTKEisPlus::createInstance fail");
        goto create_fail_exit;
    }

    m_pGisAlg = MTKGyro::createInstance();

    if (m_pGisAlg == NULL)
    {
        EIS_ERR("MTKGyro::createInstance fail");
        goto create_fail_exit;
    }

#endif

    //====== Create Sensor Listener Object ======
    DP_TRACE_BEGIN("SensorListener");
    mpSensorListener = SensorListener::createInstance();

    switch(mSensorIdx)
    {
    case 0:
        if (MTRUE != mpSensorListener->setListener(EisHalObj<0>::EisSensorListener))
        {
            EIS_ERR("setListener 0 fail");
        }
        else
        {
            EIS_LOG("setListener 0 success");
        }
    break;
    case 1:
        if (MTRUE != mpSensorListener->setListener(EisHalObj<1>::EisSensorListener))
        {
            EIS_ERR("setListener 1 fail");
        }
        else
        {
            EIS_LOG("setListener 1 success");
        }

    break;
    case 2:
        if (MTRUE != mpSensorListener->setListener(EisHalObj<2>::EisSensorListener))
        {
            EIS_ERR("setListener 2 fail");
        }
        else
        {
            EIS_LOG("setListener 2 success");
        }

    break;
    case 3:
        if (MTRUE != mpSensorListener->setListener(EisHalObj<3>::EisSensorListener))
        {
            EIS_ERR("setListener 3 fail");
        }
        else
        {
            EIS_LOG("setListener 3 success");
        }

    break;

    };

    DP_TRACE_END();

    //====== Get EIS Plus Crop Ratio ======

    EIS_PLUS_Customize_Para_t customSetting;

    EISCustom::getEISPlusData(&customSetting);

    mEisPlusCropRatio = 100 + customSetting.crop_ratio;
    EIS_LOG("mEisPlusCropRatio(%u)",mEisPlusCropRatio);

    //====== Increase User Count ======

    android_atomic_inc(&mUsers);

    EIS_LOG("-");
    return EIS_RETURN_NO_ERROR;

create_fail_exit:

    if (m_pHalSensorList != NULL)
    {
        m_pHalSensorList = NULL;
    }

#if EIS_ALGO_READY
    if (m_pEisPlusAlg != NULL)
    {
        m_pEisPlusAlg->EisPlusReset();
        m_pEisPlusAlg->destroyInstance(m_pEisPlusAlg);
        m_pEisPlusAlg = NULL;
    }

#endif

    EIS_LOG("-");
    return EIS_RETURN_NULL_OBJ;
}

/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::Uninit()
{
    Mutex::Autolock lock(mLock);

    //====== Check Reference Count ======

    if(mUsers <= 0)
    {
        EIS_LOG("mSensorIdx(%u) has 0 user",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }

    //====== Uninitialize ======

    android_atomic_dec(&mUsers);    //decrease referebce count

    if(mUsers == 0)    // there is no user
    {
        MINT32 err = EIS_RETURN_NO_ERROR;

        EIS_LOG("mSensorIdx(%u) uninit", mSensorIdx);

        //======  Release EIS Algo Object ======

#if EIS_ALGO_READY

        if (UNLIKELY(mDebugDump >= 2))
        {
            EIS_LOG("mIsEisPlusConfig(%d)",mIsEisPlusConfig);
            if(mIsEisPlusConfig == 1)
            {
                err = m_pEisPlusAlg->EisPlusFeatureCtrl(EIS_PLUS_FEATURE_SAVE_LOG,(MVOID *)&mTsForAlgoDebug,NULL);
                if(err != S_EIS_PLUS_OK)
                {
                    EIS_ERR("EisPlusFeatureCtrl(EIS_PLUS_FEATURE_SAVE_LOG) fail(0x%x)",err);
                }
            }
        }

#if !RECORD_WITHOUT_EIS_ENABLE
        if (UNLIKELY((mDebugDump >= 2) || (mEMEnabled == 1)))
#endif
        {
            EIS_LOG("mIsEisPlusConfig(%d)",mIsEisPlusConfig);
            if(mIsEisPlusConfig == 1)
            {
                err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SAVE_LOG, (MVOID *)&mTsForAlgoDebug, NULL);
                if(err != S_GYRO_OK)
                {
                    EIS_ERR("GyroFeatureCtrl(GYRO_FEATURE_SAVE_LOG) fail(0x%x)",err);
                }
            }
        }

#if RECORD_WITHOUT_EIS_ENABLE
        if (g_AIMDump == 1)
        {
            if (gTSRecordWriteID != 0)
            {
                MUINT32 LogCount,WriteCnt;
                char LogFileName[100];
                FILE * pLogFp ;

                LogCount = (mTsForAlgoDebug/(MUINT64)1000000000LL);

                EIS_LOG("RecordWriteID, mTsForAlgoDebug (%lld), LogCount(%d)",  mTsForAlgoDebug, LogCount);
                sprintf(LogFileName, "storage/sdcard0/gyro/EIS_Gyro_LOG_%d.bin", LogCount);
                pLogFp = fopen(LogFileName, "wb");
                if (NULL == pLogFp )
                {
                    EIS_ERR("Can't open file to save EIS HAL Log\n");
                }else
                {
                    WriteCnt = fwrite((void*)(&gGyroRecordWriteID),sizeof(gGyroRecordWriteID),1,pLogFp);
                    WriteCnt = fwrite(gGyroRecord,sizeof(gGyroRecord),1,pLogFp);
                    fflush(pLogFp);
                    fclose(pLogFp);
                }
                EIS_LOG("%d, gGyroRecordWriteID(%d)", LogCount, gGyroRecordWriteID);

                sprintf(LogFileName, "storage/sdcard0/gyro/EIS_TS_LOG_%d.bin", LogCount);
                pLogFp = fopen(LogFileName, "wb");
                if (NULL == pLogFp )
                {
                    EIS_ERR("Can't open file to save EIS HAL Log\n");
                }else
                {
                    MUINT64 timewithSleep = elapsedRealtime();
                    MUINT64 timewithoutSleep = uptimeMillis();
                    MUINT64 Diff =  timewithSleep - timewithoutSleep;
                    WriteCnt = fwrite((void*)(&gTSRecordWriteID),sizeof(gTSRecordWriteID),1,pLogFp);
                    WriteCnt = fwrite((void*)(&Diff),sizeof(Diff),1,pLogFp);
                    WriteCnt = fwrite(gTSRecord,sizeof(gTSRecord),1,pLogFp);
                    fflush(pLogFp);
                    fclose(pLogFp);
                }
                EIS_LOG("%d, gTSRecordWriteID(%d)", LogCount, gTSRecordWriteID);
#if 0
                sprintf(LogFileName, "storage/sdcard0/gyro/EIS_vHDR_LOG_%d.bin", LogCount);
                pLogFp = fopen(LogFileName, "wb");
                if (NULL == pLogFp )
                {
                    EIS_ERR("Can't open file to save EIS HAL Log\n");
                }else
                {
                    MUINT32 IsFirstLE;
                    IsFirstLE = mSensorStaticInfo.iHDR_First_IS_LE;
                    WriteCnt = fwrite((void*)(&gvHDRRecordWriteID),sizeof(gvHDRRecordWriteID),1,pLogFp);
                    WriteCnt = fwrite((void*)(&IsFirstLE),sizeof(IsFirstLE),1,pLogFp);
                    WriteCnt = fwrite(gvHDRRecord,sizeof(gvHDRRecord),1,pLogFp);
                    fflush(pLogFp);
                    fclose(pLogFp);
                }
                EIS_LOG("%d, gvHDRRecordWriteID(%d)", LogCount, gvHDRRecordWriteID);
#endif
            }
            gTSRecordWriteID = 0;
            gGyroRecordWriteID = 0;
            gvHDRRecordWriteID = 0;
            memset(&gTSRecord, 0, sizeof(gTSRecord));
            memset(&gvHDRRecord, 0, sizeof(gvHDRRecord));
            memset(&gGyroRecord, 0, sizeof(gGyroRecord));
        }
#endif
        //Writeback to NVRAM
        if (mNVRAMRead)
        {
            MUINT32 sensorDev;

            //Force update the NVRAM tmp buffer
            if (m_pNvram && m_pNVRAM_defParameter)
            {
                memcpy(&(m_pNvram->gis), m_pNVRAM_defParameter, sizeof(NVRAM_CAMERA_FEATURE_GIS_STRUCT));
            }

            sensorDev = m_pHalSensorList->querySensorDevIdx(mSensorIdx);
            err = MAKE_NvBufUtil().write(CAMERA_NVRAM_DATA_FEATURE, sensorDev);
        }

        if (m_pGisAlg != NULL)
        {
            EIS_LOG("m_pGisAlg uninit");
            m_pGisAlg->GyroReset();
            m_pGisAlg->destroyInstance(m_pGisAlg);
            m_pGisAlg = NULL;
        }

        EIS_LOG("TG(%d)",mSensorDynamicInfo.TgInfo);

        if (m_pEisPlusAlg != NULL)
        {
            EIS_LOG("m_pEisPlusAlg uninit");
            m_pEisPlusAlg->EisPlusReset();
            m_pEisPlusAlg->destroyInstance(m_pEisPlusAlg);
            m_pEisPlusAlg = NULL;
        }
#endif

        // Next-Gen EIS
        if (mpSensorListener != NULL)
        {
            EIS_LOG("mpSensorListener uninit");
            mpSensorListener->disableSensor(SensorListener::SensorType_Acc);
            mpSensorListener->disableSensor(SensorListener::SensorType_Gyro);
            mpSensorListener->destroyInstance();
            mpSensorListener = NULL;
        }


        //====== Destroy Sensor Object ======

        if(m_pHalSensorList != NULL)
        {
            m_pHalSensorList = NULL;
        }

        //======  Release Memory and IMem Object ======
        //>  free EIS Plus working buffer
        if (m_pEisPlusWorkBuf != NULL)
        {
            m_pEisPlusWorkBuf->unlockBuf("EISPlusWorkBuf");
            DestroyMemBuf(m_pEisPlusWorkBuf);
        }

        //>  free GIS working buffer

        if (m_pGisWorkBuf != NULL)
        {
            m_pGisWorkBuf->unlockBuf("GisWorkBuf");
            DestroyMemBuf(m_pGisWorkBuf);
        }

        //======  Reset Member Variable ======
        if (m_pLMVHal != NULL)
        {
            m_pLMVHal->DestroyInstance(LOG_TAG);
            m_pLMVHal = NULL;
        }

        mEisInput_W = 0;
        mEisInput_H = 0;
        mP1Target_W = 0;
        mP1Target_H = 0;
        mSrzOutW = 0;
        mSrzOutH = 0;
        mFrameCnt = 0; // first frmae
        mIsEisConfig = 0;
        mIsEisPlusConfig = 0;
        mCmvX_Int = 0;
        mDoEisCount = 0;    //Vent@20140427: Add for EIS GMV Sync Check.
        mCmvX_Flt = 0;
        mCmvY_Int = 0;
        mMVtoCenterX = 0;
        mMVtoCenterY = 0;
        mCmvY_Flt = 0;
        mGMV_X = 0;
        mGMV_Y = 0;
        mGpuGridW = 0;
        mGpuGridH = 0;
        mEisPlusCropRatio = 20;
        mGyroEnable = MFALSE;
        mAccEnable  = MFALSE;

        mChangedInCalibration = 0;
        mGisInputW = 0;
        mGisInputH = 0;
        mNVRAMRead = MFALSE;
        m_pNvram = NULL;
        mSleepTime = 0;
        mtRSTime = 0;
        mbLastCalibration = MTRUE;
        EISHAL_SwitchSetGyroCountZero(mSensorIdx);
        EISHAL_SwitchSetLastGyroTimestampValue(mSensorIdx, 0);

        mSensorPixelClock = 0;
        mSensorLinePixel = 0;
        mbEMSaveFlag = MFALSE;

        delete m_pNVRAM_defParameter;
        m_pNVRAM_defParameter = NULL;
        memset(mRecordParameter, 0, sizeof(mRecordParameter));

        mEISInterval = GyroInterval_ms;
        mEMEnabled = 0;
        mDebugDump = 0;

    }
    else
    {
        EIS_LOG("mSensorIdx(%u) has %d users",mSensorIdx,mUsers);
    }

    EISHAL_SwitchClearGyroQueue(mSensorIdx);

    return EIS_RETURN_NO_ERROR;
}

/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::CreateMemBuf(MUINT32 memSize, android::sp<IImageBuffer>& spImageBuf)
{
    MINT32 err = EIS_RETURN_NO_ERROR;
    IImageBufferAllocator* pImageBufferAlloc = IImageBufferAllocator::getInstance();

    EIS_LOG("Size(%u)",memSize);
    IImageBufferAllocator::ImgParam bufParam((size_t)memSize, 0);
    spImageBuf = pImageBufferAlloc->alloc("EIS_HAL", bufParam);
    EIS_LOG("X");
    return err;
}

/******************************************************************************
*
*******************************************************************************/
MINT32 EisHalImp::DestroyMemBuf(android::sp<IImageBuffer>& spImageBuf)
{
    MINT32 err = EIS_RETURN_NO_ERROR;
    IImageBufferAllocator* pImageBufferAlloc = IImageBufferAllocator::getInstance();
    EIS_LOG("DestroyMemBuf");


    if (spImageBuf != NULL)
    {
        pImageBufferAlloc->free(spImageBuf.get());
        spImageBuf = NULL;
    }

    EIS_LOG("X");
    return err;
}

/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::GetSensorInfo()
{
    EIS_LOG("mSensorIdx(%u)",mSensorIdx);

    mSensorDev = m_pHalSensorList->querySensorDevIdx(mSensorIdx);
    m_pHalSensorList->querySensorStaticInfo(mSensorDev,&mSensorStaticInfo);

    m_pHalSensor = m_pHalSensorList->createSensor(EIS_HAL_NAME,1,&mSensorIdx);
    if(m_pHalSensor == NULL)
    {
        EIS_ERR("m_pHalSensorList->createSensor fail");
        return EIS_RETURN_API_FAIL;
    }

    if(MFALSE == m_pHalSensor->querySensorDynamicInfo(mSensorDev,&mSensorDynamicInfo))
    {
        EIS_ERR("querySensorDynamicInfo fail");
        return EIS_RETURN_API_FAIL;
    }

    m_pHalSensor->destroyInstance(EIS_HAL_NAME);
    m_pHalSensor = NULL;

    return EIS_RETURN_NO_ERROR;
}


MINT32 EisHalImp::EnableSensor()
{
    MINT32 err = EIS_RETURN_NO_ERROR;

    //====== Turn on Eis Configure One Time Flag ======
    m_pHalSensor = m_pHalSensorList->createSensor(EIS_HAL_NAME,1,&mSensorIdx);

    if (LIKELY(m_pHalSensor != NULL))
    {
        err = m_pHalSensor->sendCommand(mSensorDev,SENSOR_CMD_GET_PIXEL_CLOCK_FREQ, (MUINTPTR)&mSensorPixelClock, 0, 0);
        if (err != EIS_RETURN_NO_ERROR)
        {
            EIS_ERR("SENSOR_CMD_GET_PIXEL_CLOCK_FREQ is fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }

        err = m_pHalSensor->sendCommand(mSensorDev,SENSOR_CMD_GET_FRAME_SYNC_PIXEL_LINE_NUM, (MUINTPTR)&mSensorLinePixel, 0, 0);
        if (err != EIS_RETURN_NO_ERROR)
        {
            EIS_ERR("SENSOR_CMD_GET_PIXEL_CLOCK_FREQ is fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }

        m_pHalSensor->destroyInstance(EIS_HAL_NAME);
        m_pHalSensor = NULL;

    }else
    {
        EIS_ERR("m_pHalSensorList->createSensor fail, m_pHalSensor == NULL");
    }

    EIS_LOG("mSensorDev(%u), pixelclock (%d), pixelline(%x)",mSensorDev,mSensorPixelClock,mSensorLinePixel);

    if (mSensorDev == SENSOR_DEV_SUB)
    {
        EISHAL_SwitchSetGyroReverseValue(mSensorIdx, 1);
        EIS_LOG("mSensorDev(%u), GYRO data reversed",mSensorDev);
    }else
    {
        EISHAL_SwitchSetGyroReverseValue(mSensorIdx, 0);
        EIS_LOG("mSensorDev(%u), GYRO data normal",mSensorDev);
    }

    // No use for accelerameter
    //mAccEnable  = mpSensorListener->enableSensor(SensorListener::SensorType_Acc,mEISInterval);
    mAccEnable  = MFALSE;
    mGyroEnable = mpSensorListener->enableSensor(SensorListener::SensorType_Gyro, mEISInterval);
    EIS_LOG("EN:(Acc,Gyro)=(%d,%d)",mAccEnable,mGyroEnable);

    if (m_pLMVHal == NULL)
    {
        m_pLMVHal = LMVHal::CreateInstance(LOG_TAG, mSensorIdx);
    }

    return EIS_RETURN_NO_ERROR;
}

MINT32 EisHalImp::ConfigGyroAlgo(const EIS_HAL_CONFIG_DATA &aEisConfig)
{
    MINT32 err = EIS_RETURN_NO_ERROR;

    GYRO_INIT_INFO_STRUCT  gyroAlgoInitData;
    GYRO_GET_PROC_INFO_STRUCT gyroGetProcData;
    GYRO_SET_WORKING_BUFFER_STRUCT gyroSetworkingbuffer;


    memset(&gyroAlgoInitData, 0, sizeof(gyroAlgoInitData));
    MUINT64 timewithSleep = elapsedRealtime();
    MUINT64 timewithoutSleep = uptimeMillis();

    if (m_pLMVHal->GetLMVStatus())
    {
        m_pLMVHal->GetRegSetting(&gyroAlgoInitData);
    }else
    { //Need pass from metadata!! TBD

    }

    //====== Read NVRAM calibration data ======
    MUINT32 sensorDev;
    sensorDev = m_pHalSensorList->querySensorDevIdx(mSensorIdx);
    err = MAKE_NvBufUtil().getBufAndRead(CAMERA_NVRAM_DATA_FEATURE, sensorDev, (void*&)m_pNvram);
    if (m_pNVRAM_defParameter && m_pNvram)
    {
        memcpy(m_pNVRAM_defParameter, &(m_pNvram->gis), sizeof(NVRAM_CAMERA_FEATURE_GIS_STRUCT));
    }else
    {
        EIS_ERR("m_pNVRAM_defParameter OR m_pNVRAM_defParameter is NULL\n");
        return  EIS_RETURN_NULL_OBJ;
    }
    mNVRAMRead = MFALSE; //No write back

    mRecordParameter[0] = m_pNVRAM_defParameter->gis_defParameter3[0];
    mRecordParameter[1] = m_pNVRAM_defParameter->gis_defParameter3[1];
    mRecordParameter[2] = m_pNVRAM_defParameter->gis_defParameter3[2];
    mRecordParameter[3] = m_pNVRAM_defParameter->gis_defParameter3[3];
    mRecordParameter[4] = m_pNVRAM_defParameter->gis_defParameter3[4];
    mRecordParameter[5] = m_pNVRAM_defParameter->gis_defParameter3[5];

    //> prepare eisPlusAlgoInitData
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        double tRS = 0.0f,numLine;
        numLine = mSensorLinePixel&0xFFFF;
        if (mSensorPixelClock != 0)
        {
            tRS = numLine / mSensorPixelClock;
            tRS = tRS * (float)(gyroAlgoInitData.param_Height-1);
        }else
        {
            EIS_WRN("mSensorPixelClock is %d, so can NOT get tRS",mSensorPixelClock);
        }

        EIS_LOG("tRS in table: %f, calculated tRS: %f", m_pNVRAM_defParameter->gis_deftRS[0], tRS);

        //Replace the tRS from the table by current sensor mode
        mRecordParameter[0] = tRS;

        tRS+= m_pNVRAM_defParameter->gis_defParameter1[5];
        //Check 30 fps maxmum
        if (tRS > 0.042f)
        {
            EIS_WRN("30 fps tRS+tOffset: %f should be small than 0.042 ms", tRS);
        }
        mtRSTime = (long long)((double)tRS*1000000.0f);
        EIS_LOG("waiting gyro time: %lld", mtRSTime);

    }

    gyroAlgoInitData.param_Width = m_pNVRAM_defParameter->gis_defWidth;
    gyroAlgoInitData.param_Height= m_pNVRAM_defParameter->gis_defHeight;
    gyroAlgoInitData.param_crop_Y= m_pNVRAM_defParameter->gis_defCrop;
    gyroAlgoInitData.ProcMode = GYRO_PROC_MODE_MV;
    gyroAlgoInitData.param = mRecordParameter;
    gyroAlgoInitData.sleep_t =  timewithSleep - timewithoutSleep;
    mSleepTime = gyroAlgoInitData.sleep_t;

    EIS_LOG("def data Rec: %f    %f    %f    %f    %f    %f", mRecordParameter[0], mRecordParameter[1], mRecordParameter[2],
                                                              mRecordParameter[3], mRecordParameter[4], mRecordParameter[5]);

    gyroAlgoInitData.crz_crop_X = aEisConfig.cropX;
    gyroAlgoInitData.crz_crop_Y = aEisConfig.cropY;

    gyroAlgoInitData.crz_crop_Width = aEisConfig.crzOutW;
    gyroAlgoInitData.crz_crop_Height = aEisConfig.crzOutH;

    if ((aEisConfig.srzOutW <= D1_WIDTH) && (aEisConfig.srzOutH <= D1_HEIGHT))
    {
        gyroAlgoInitData.block_size = 8;
    }
    else if ((aEisConfig.srzOutW <= EIS_FE_MAX_INPUT_W) && (aEisConfig.srzOutH <= EIS_FE_MAX_INPUT_H))
    {
        gyroAlgoInitData.block_size = 16;
    }
    else
    {
        gyroAlgoInitData.block_size = 32;
    }

#if !RECORD_WITHOUT_EIS_ENABLE
    if (UNLIKELY(mDebugDump >= 2))
#endif
    {
        gyroAlgoInitData.debug = MTRUE;
    }
    EIS_LOG("sleep_t is (%lld)", gyroAlgoInitData.sleep_t);
    EIS_LOG("aEisConfig  IMG w(%d),h(%d)", aEisConfig.imgiW, aEisConfig.imgiH);

    EIS_LOG("crz offset x(%d),y(%d)", gyroAlgoInitData.crz_crop_X, gyroAlgoInitData.crz_crop_Y);
    EIS_LOG("crzOut w(%d),h(%d)", gyroAlgoInitData.crz_crop_Width, gyroAlgoInitData.crz_crop_Height);

    err = m_pGisAlg->GyroInit(&gyroAlgoInitData);
    if (err != S_GYRO_OK)
    {
        EIS_ERR("GyroInit fail(0x%x)",err);
        return EIS_RETURN_API_FAIL;
    }

    err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_GET_PROC_INFO, NULL, &gyroGetProcData);
    if (err != S_GYRO_OK)
    {
        EIS_ERR("get Gyro proc info fail(0x%x)",err);
        return EIS_RETURN_API_FAIL;
    }

    CreateMemBuf(gyroGetProcData.ext_mem_size, m_pGisWorkBuf);
    m_pGisWorkBuf->lockBuf("GisWorkBuf", eBUFFER_USAGE_SW_MASK);
    if (!m_pGisWorkBuf->getBufVA(0))
    {
        EIS_ERR("m_pGisWorkBuf create ImageBuffer fail");
        return EIS_RETURN_MEMORY_ERROR;
    }

    gyroSetworkingbuffer.extMemStartAddr = (MVOID *)m_pGisWorkBuf->getBufVA(0);
    gyroSetworkingbuffer.extMemSize = gyroGetProcData.ext_mem_size;

    err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SET_WORK_BUF_INFO, &gyroSetworkingbuffer, NULL);
    if(err != S_GYRO_OK)
    {

        EIS_ERR("mGisWorkBuf create IMem fail");
        return EIS_RETURN_API_FAIL;
    }

    return EIS_RETURN_NO_ERROR;

}

MINT32 EisHalImp::CreateEISPlusAlgoBuf()
{
    MINT32 err = EIS_RETURN_NO_ERROR;
    EIS_PLUS_GET_PROC_INFO_STRUCT eisPlusGetProcData;
    EIS_PLUS_SET_WORKING_BUFFER_STRUCT eisPlusWorkBufData;

    eisPlusGetProcData.ext_mem_size = 0;
    eisPlusGetProcData.Grid_H = 0;
    eisPlusGetProcData.Grid_W = 0;

    eisPlusWorkBufData.extMemSize = 0;
    eisPlusWorkBufData.extMemStartAddr = NULL;

    //> Preapre EIS Plus Working Buffer

    err = m_pEisPlusAlg->EisPlusFeatureCtrl(EIS_PLUS_FEATURE_GET_PROC_INFO, NULL, &eisPlusGetProcData);
    if (err != S_EIS_PLUS_OK)
    {
        EIS_ERR("EisPlus: EIS_PLUS_FEATURE_GET_PROC_INFO fail(0x%x)",err);
        return EIS_RETURN_API_FAIL;
    }

    EIS_LOG("ext_mem_size(%u)",eisPlusGetProcData.ext_mem_size);

    CreateMemBuf(eisPlusGetProcData.ext_mem_size, m_pEisPlusWorkBuf);
    m_pEisPlusWorkBuf->lockBuf("EISPlusWorkBuf", eBUFFER_USAGE_SW_MASK);
    if (!m_pEisPlusWorkBuf->getBufVA(0))
    {
        EIS_ERR("m_pEisPlusWorkBuf create ImageBuffer fail");
        return EIS_RETURN_MEMORY_ERROR;
    }

    EIS_LOG("m_pEisPlusWorkBuf : size(%u),virAdd(0x%p)", eisPlusGetProcData.ext_mem_size, m_pEisPlusWorkBuf->getBufVA(0));

    eisPlusWorkBufData.extMemSize = eisPlusGetProcData.ext_mem_size;
    eisPlusWorkBufData.extMemStartAddr = (MVOID *)m_pEisPlusWorkBuf->getBufVA(0);

    err = m_pEisPlusAlg->EisPlusFeatureCtrl(EIS_PLUS_FEATURE_SET_WORK_BUF_INFO, &eisPlusWorkBufData, NULL);
    if (err != S_EIS_PLUS_OK)
    {
        EIS_ERR("EisPlus: EIS_PLUS_FEATURE_SET_WORK_BUF_INFO fail(0x%x)",err);
        return EIS_RETURN_API_FAIL;
    }
    return EIS_RETURN_NO_ERROR;

}


/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::ConfigGis(const EIS_HAL_CONFIG_DATA &aEisConfig)
{
    if(mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }

    MINT32 err = EIS_RETURN_NO_ERROR;

#if EIS_ALGO_READY
    static EIS_SET_ENV_INFO_STRUCT eisAlgoInitData;
#endif

#if EIS_ALGO_READY
    if (mIsEisPlusConfig == 0)
    {
        EnableSensor();
        EIS_LOG("EIS Plus first config");

        EIS_PLUS_SET_ENV_INFO_STRUCT eisPlusAlgoInitData;
        //> prepare eisPlusAlgoInitData
        memset(&eisPlusAlgoInitData, 0, sizeof(eisPlusAlgoInitData));

        // get EIS Plus customized data
        GetEisPlusCustomize(&eisPlusAlgoInitData.eis_plus_tuning_data);

        if (UNLIKELY(mDebugDump >= 2))
        {
            eisPlusAlgoInitData.debug = MTRUE;
            EIS_LOG("eisPlus debug(%d)",eisPlusAlgoInitData.debug);
        }

        EIS_LOG("EIS Plus tuning_data");
        EIS_LOG("warping_mode(%d),effort(%d)",eisPlusAlgoInitData.eis_plus_tuning_data.warping_mode,eisPlusAlgoInitData.eis_plus_tuning_data.effort);
        EIS_LOG("search_range(%d,%d)",eisPlusAlgoInitData.eis_plus_tuning_data.search_range_x,eisPlusAlgoInitData.eis_plus_tuning_data.search_range_y);
        EIS_LOG("crop_ratio(%d),stabilization_strength(%f)",eisPlusAlgoInitData.eis_plus_tuning_data.crop_ratio,eisPlusAlgoInitData.eis_plus_tuning_data.stabilization_strength);

        //> Init Eis plus

        eisPlusAlgoInitData.GyroValid= mGyroEnable;
        eisPlusAlgoInitData.Gvalid= mAccEnable;
        eisPlusAlgoInitData.EIS_mode = 2;

        err = m_pEisPlusAlg->EisPlusInit(&eisPlusAlgoInitData);
        if(err != S_EIS_PLUS_OK)
        {
            EIS_ERR("EisPlusInit fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }

        CreateEISPlusAlgoBuf();
        ConfigGyroAlgo(aEisConfig);
        mIsEisPlusConfig = 1;
    }
#endif
    return EIS_RETURN_NO_ERROR;
}

/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::ConfigFFFMEis(const EIS_HAL_CONFIG_DATA & aEisConfig, const MUINT32 eisMode, const MULTISCALE_INFO* apMultiScaleInfo)
{
    if(mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }

    MINT32 err = EIS_RETURN_NO_ERROR;

#if EIS_ALGO_READY
    static EIS_SET_ENV_INFO_STRUCT eisAlgoInitData;
#endif

#if EIS_ALGO_READY
    if (mIsEisPlusConfig == 0)
    {
        EnableSensor();
        EIS_LOG("EIS Plus first config");

        EIS_PLUS_SET_ENV_INFO_STRUCT eisPlusAlgoInitData;

        //> prepare eisPlusAlgoInitData
        memset(&eisPlusAlgoInitData, 0, sizeof(eisPlusAlgoInitData));

        // get EIS Plus customized data
        GetEisPlusCustomize(&eisPlusAlgoInitData.eis_plus_tuning_data);

        if (UNLIKELY(mDebugDump >= 2))
        {
            eisPlusAlgoInitData.debug = MTRUE;
            EIS_LOG("eisPlus debug(%d)",eisPlusAlgoInitData.debug);
        }

        EIS_LOG("EIS Plus tuning_data");
        EIS_LOG("warping_mode(%d),effort(%d)",eisPlusAlgoInitData.eis_plus_tuning_data.warping_mode,eisPlusAlgoInitData.eis_plus_tuning_data.effort);
        EIS_LOG("search_range(%d,%d)",eisPlusAlgoInitData.eis_plus_tuning_data.search_range_x,eisPlusAlgoInitData.eis_plus_tuning_data.search_range_y);
        EIS_LOG("crop_ratio(%d),stabilization_strength(%f)",eisPlusAlgoInitData.eis_plus_tuning_data.crop_ratio,eisPlusAlgoInitData.eis_plus_tuning_data.stabilization_strength);

        //> Init Eis plus

        //EIS 2.5
        GetEis25Customize(&eisPlusAlgoInitData.eis25_tuning_data);
#if RECORD_WITHOUT_EIS_ENABLE
        if (UNLIKELY(eisPlusAlgoInitData.eis25_tuning_data.en_dejello != g_forecDejello))
        {
            eisPlusAlgoInitData.eis25_tuning_data.en_dejello = g_forecDejello;
            EIS_LOG("Force dejello: %d", eisPlusAlgoInitData.eis25_tuning_data.en_dejello);
        }
#endif

        eisPlusAlgoInitData.Gvalid= mAccEnable;
        eisPlusAlgoInitData.GyroValid = EISCustom::isEnabledEIS25_GyroMode() ?
                                        mGyroEnable : MFALSE;

        /* Control EIS 2.5 version for algo
         */
        switch (eisMode)
        {
        case EIS_ALG_MODE_EIS25_FUSION:
            eisPlusAlgoInitData.EIS_mode = 3;
            eisPlusAlgoInitData.en_gyro_fusion = 1;
            break;
        case EIS_ALG_MODE_EIS25_GYRO_ONLY:
            eisPlusAlgoInitData.EIS_mode = 4;
            eisPlusAlgoInitData.en_gyro_fusion = 1;
            break;
        case EIS_ALG_MODE_EIS25_IMAGE_ONLY:
            eisPlusAlgoInitData.EIS_mode = 3;
            eisPlusAlgoInitData.en_gyro_fusion = 0;
            eisPlusAlgoInitData.eis25_tuning_data.en_dejello = 0;
            break;
        default:
            /* fallthrough */
            eisPlusAlgoInitData.EIS_mode = 0;
            eisPlusAlgoInitData.en_gyro_fusion = 0;
            EIS_ERR("Unknown EIS 2.5 Mode!");
            break;
        }

#if RECORD_WITHOUT_EIS_ENABLE
        if (UNLIKELY(g_GyroOnly == 1))
        {
            eisPlusAlgoInitData.EIS_mode = 4;
            EIS_LOG("Force Gyro-only mode: %d",eisPlusAlgoInitData.EIS_mode);
        }
#endif

#if RECORD_WITHOUT_EIS_ENABLE
        if (UNLIKELY(g_ImageOnly == 1))
        {
            g_GyroOnly = 0;
            eisPlusAlgoInitData.EIS_mode = 3;
            eisPlusAlgoInitData.en_gyro_fusion = 0; //DEBUG MODE: Image-only
            EIS_LOG("Force Image-only mode: %d ,en_gyro_fusion: %d", eisPlusAlgoInitData.EIS_mode, eisPlusAlgoInitData.en_gyro_fusion);
        }
#endif

        for (MUINT32 i = 0; (i<MULTISCALE_FEFM) && apMultiScaleInfo; i++)
        {
            eisPlusAlgoInitData.MultiScale_width[i] = apMultiScaleInfo->MultiScale_width[i];
            eisPlusAlgoInitData.MultiScale_height[i] = apMultiScaleInfo->MultiScale_height[i];
            eisPlusAlgoInitData.MultiScale_blocksize[i] = apMultiScaleInfo->MultiScale_blocksize[i];

            EIS_LOG("FE level(%d) w/h (%d/%d) , block (%d)", i, apMultiScaleInfo->MultiScale_width[i],
                                                                apMultiScaleInfo->MultiScale_height[i],
                                                                apMultiScaleInfo->MultiScale_blocksize[i]);
        }

        /*
                                                 EIS_MODE     GYRO_VALID  en_gyro_fusion
        EIS 2.5 Image-based Only                     3          0           N/A            No gyroscope exist
        EIS 2.5 Gyro-based Only                      4          1            1             4Kx2K
        EIS 2.5 Fusion                               3          1            1             FHD
        EIS 2.5 Image-based Only + Gyro still        3          1            0             vHDR
        EIS 1.2                                      0          0            0             4Kx2K with vHDR
        */
        if (UNLIKELY(mDebugDump > 0))
        {
            EIS_LOG("EIS_MODE=%d, GYRO_VALID=%d, en_gyro_fusion=%d",
                    eisPlusAlgoInitData.EIS_mode,
                    eisPlusAlgoInitData.GyroValid,
                    eisPlusAlgoInitData.en_gyro_fusion);
        }

        err = m_pEisPlusAlg->EisPlusInit(&eisPlusAlgoInitData);
        if(err != S_EIS_PLUS_OK)
        {
            EIS_ERR("EisPlusInit fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }

        CreateEISPlusAlgoBuf();
        ConfigGyroAlgo(aEisConfig);

        mIsEisPlusConfig = 1;
    }
#endif

    return EIS_RETURN_NO_ERROR;
}


/*******************************************************************************
*
********************************************************************************/
MINT32 EisHalImp::ConfigCalibration(const LMV_HAL_CONFIG_DATA &aLMVConfig)
{
    if(mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }

    MINT32 err = EIS_RETURN_NO_ERROR;

 #if EIS_ALGO_READY
    if(mIsEisPlusConfig == 0)
    {
        EnableSensor();
        EIS_LOG("GIS first config for calibration");

        GYRO_INIT_INFO_STRUCT  gyroAlgoInitData;
        GYRO_GET_PROC_INFO_STRUCT gyroGetProcData;
        GYRO_SET_WORKING_BUFFER_STRUCT gyroSetworkingbuffer;

        //====== Read NVRAM calibration data ======
        MUINT32 sensorDev;
        sensorDev = m_pHalSensorList->querySensorDevIdx(mSensorIdx);
        err = MAKE_NvBufUtil().getBufAndRead(CAMERA_NVRAM_DATA_FEATURE, sensorDev, (void*&)m_pNvram);
        if (m_pNVRAM_defParameter && m_pNvram)
        {
            memcpy(m_pNVRAM_defParameter, &(m_pNvram->gis), sizeof(NVRAM_CAMERA_FEATURE_GIS_STRUCT));
        }else
        {
            EIS_ERR("m_pNVRAM_defParameter OR m_pNVRAM_defParameter is NULL\n");
            return  EIS_RETURN_NULL_OBJ;
        }
        mNVRAMRead = MTRUE;

        //> Init GIS
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        //> prepare eisPlusAlgoInitData
        memset(&gyroAlgoInitData, 0, sizeof(gyroAlgoInitData));
        MUINT64 timewithSleep = elapsedRealtime();
        MUINT64 timewithoutSleep = uptimeMillis();

        if (m_pLMVHal->GetLMVStatus())
        {
            m_pLMVHal->GetRegSetting(&gyroAlgoInitData);
            m_pLMVHal->GetLMVInputSize(&mGisInputW, &mGisInputH);
        }else
        { //Need pass from metadata!! TBD

        }

        //Get tRS
        {
            double tRS = 0.0f,numLine;
            numLine = mSensorLinePixel&0xFFFF;
            if (mSensorPixelClock != 0)
            {
                tRS = numLine / (float)mSensorPixelClock;
                EIS_LOG("Small tRS: %f ",tRS);
                tRS = tRS * (float)(gyroAlgoInitData.param_Height-1);
                EIS_LOG("Big tRS: %f ",tRS);
            }else
            {
                EIS_WRN("mSensorPixelClock is %d, so can NOT get tRS",mSensorPixelClock);
            }

            EIS_LOG("tRS in table: %f, calculated tRS: %f", m_pNVRAM_defParameter->gis_deftRS[0], tRS);

            m_pNVRAM_defParameter->gis_defParameter1[0] = tRS;
            m_pNVRAM_defParameter->gis_defParameter2[0] = tRS;
            m_pNVRAM_defParameter->gis_defParameter3[0] = tRS;
        }


        EIS_LOG("def data 0: %f    %f    %f    %f    %f    %f", m_pNVRAM_defParameter->gis_defParameter1[0], m_pNVRAM_defParameter->gis_defParameter1[1],
                                                                m_pNVRAM_defParameter->gis_defParameter1[2], m_pNVRAM_defParameter->gis_defParameter1[3],
                                                                m_pNVRAM_defParameter->gis_defParameter1[4], m_pNVRAM_defParameter->gis_defParameter1[5]);

        EIS_LOG("def data 1: %f    %f    %f    %f    %f    %f", m_pNVRAM_defParameter->gis_defParameter2[0], m_pNVRAM_defParameter->gis_defParameter2[1],
                                                                m_pNVRAM_defParameter->gis_defParameter2[2], m_pNVRAM_defParameter->gis_defParameter2[3],
                                                                m_pNVRAM_defParameter->gis_defParameter2[4], m_pNVRAM_defParameter->gis_defParameter2[5]);

        EIS_LOG("def data 2: %f    %f    %f    %f    %f    %f", m_pNVRAM_defParameter->gis_defParameter3[0], m_pNVRAM_defParameter->gis_defParameter3[1],
                                                                m_pNVRAM_defParameter->gis_defParameter3[2], m_pNVRAM_defParameter->gis_defParameter3[3],
                                                                m_pNVRAM_defParameter->gis_defParameter3[4], m_pNVRAM_defParameter->gis_defParameter3[5]);

        mRecordParameter[0] = m_pNVRAM_defParameter->gis_defParameter3[0];
        mRecordParameter[1] = m_pNVRAM_defParameter->gis_defParameter3[1];
        mRecordParameter[2] = m_pNVRAM_defParameter->gis_defParameter3[2];
        mRecordParameter[3] = m_pNVRAM_defParameter->gis_defParameter3[3];
        mRecordParameter[4] = m_pNVRAM_defParameter->gis_defParameter3[4];
        mRecordParameter[5] = m_pNVRAM_defParameter->gis_defParameter3[5];

        gyroAlgoInitData.param_Width = m_pNVRAM_defParameter->gis_defWidth;
        gyroAlgoInitData.param_Height= m_pNVRAM_defParameter->gis_defHeight;
        gyroAlgoInitData.param_crop_Y= m_pNVRAM_defParameter->gis_defCrop;

        //Deprecated. No use now for calibration
        gyroAlgoInitData.crz_crop_X = 0;
        gyroAlgoInitData.crz_crop_Y = 0;
        gyroAlgoInitData.crz_crop_Width = 0;
        gyroAlgoInitData.crz_crop_Height = 0;

        gyroAlgoInitData.ProcMode = GYRO_PROC_MODE_CAL;
        gyroAlgoInitData.param = m_pNVRAM_defParameter->gis_defParameter1;
        gyroAlgoInitData.sleep_t =  timewithSleep - timewithoutSleep;
        EIS_LOG("sleep_t is (%lld)", gyroAlgoInitData.sleep_t);
        EIS_LOG("rrz crop w(%d),h(%d)", gyroAlgoInitData.rrz_crop_Width, gyroAlgoInitData.rrz_crop_Height);
        EIS_LOG("rrzOut w(%d),h(%d)", gyroAlgoInitData.rrz_scale_Width, gyroAlgoInitData.rrz_scale_Height);
        EIS_LOG("crz offset x(%d),y(%d)", gyroAlgoInitData.crz_crop_X, gyroAlgoInitData.crz_crop_Y);
        EIS_LOG("crzOut w(%d),h(%d)", gyroAlgoInitData.crz_crop_Width, gyroAlgoInitData.crz_crop_Height);
        EIS_LOG("aEisConfig  PIXEL_MODE(%d),op h(%d), opv(%d), rp h(%d), rp v(%d)", gyroAlgoInitData.GyroCalInfo.PIXEL_MODE,
                                                                        gyroAlgoInitData.GyroCalInfo.EIS_OP_H_step, gyroAlgoInitData.GyroCalInfo.EIS_OP_V_step,
                                                                        gyroAlgoInitData.GyroCalInfo.EIS_RP_H_num, gyroAlgoInitData.GyroCalInfo.EIS_RP_V_num);
        if (UNLIKELY(mEMEnabled == 1))
        {
            gyroAlgoInitData.debug = MTRUE;
            gyroAlgoInitData.EMmode = MTRUE;
            mbEMSaveFlag = MFALSE;
        }

#if !RECORD_WITHOUT_EIS_ENABLE
        if (UNLIKELY(mDebugDump >= 2))
#endif
        {
            gyroAlgoInitData.debug = MTRUE;
        }

        err = m_pGisAlg->GyroInit(&gyroAlgoInitData);
        if(err != S_GYRO_OK)
        {
            EIS_ERR("GyroInit fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }

        err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_GET_PROC_INFO, NULL, &gyroGetProcData);
        if(err != S_GYRO_OK)
        {
            EIS_ERR("get Gyro proc info fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }

        CreateMemBuf(gyroGetProcData.ext_mem_size, m_pGisWorkBuf);
        m_pGisWorkBuf->lockBuf("GisWorkBuf", eBUFFER_USAGE_SW_MASK);
        if (!m_pGisWorkBuf->getBufVA(0))
        {
            EIS_ERR("m_pGisWorkBuf create ImageBuffer fail");
            return EIS_RETURN_MEMORY_ERROR;
        }

        gyroSetworkingbuffer.extMemStartAddr = (MVOID *)m_pGisWorkBuf->getBufVA(0);
        gyroSetworkingbuffer.extMemSize = gyroGetProcData.ext_mem_size;

        err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SET_WORK_BUF_INFO, &gyroSetworkingbuffer, NULL);
        if(err != S_GYRO_OK)
        {

            EIS_ERR("mGisWorkBuf GYRO_FEATURE_SET_WORK_BUF_INFO fail");
            return EIS_RETURN_API_FAIL;
        }
        mIsEisPlusConfig = 1;
    }
#endif
    return EIS_RETURN_NO_ERROR;
}


MINT32 EisHalImp::ForcedDoEisPass2()
{
    DP_TRACE_CALL();
    EISHAL_SwitchGyroQueueLock(mSensorIdx);
    mSkipWaitGyro = MTRUE;
    EISHAL_SWitchWaitGyroCond_Signal(mSensorIdx);
    EISHAL_SwitchGyroQueueUnlock(mSensorIdx);
    return EIS_RETURN_NO_ERROR;
}


MINT32 EisHalImp::AbortP2Calibration()
{
    MINT32 err = S_GYRO_OK;

#if EIS_ALGO_READY
    if (m_pGisAlg != NULL)
    {
        err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SET_CAL_NONACTIVE, NULL, NULL);
        if(err != S_GYRO_OK)
        {
            EIS_ERR("abort Calibration fail(0x%x)",err);
            return EIS_RETURN_API_FAIL;
        }
    }
#endif
    return EIS_RETURN_NO_ERROR;
}

MINT32 EisHalImp::ExecuteGyroAlgo(const EIS_HAL_CONFIG_DATA* apEisConfig,const MINT64 aTimeStamp, const MINT64 aExpTime,GYRO_MV_RESULT_INFO_STRUCT& gyroMVresult)
{
    MINT32 err = EIS_RETURN_NO_ERROR;
    GYRO_INIT_INFO_STRUCT  gyroAlgoInitData;
    GYRO_SET_PROC_INFO_STRUCT gyroSetProcData;
    memset(&gyroSetProcData,0,sizeof(gyroSetProcData));
    memset(&gyroAlgoInitData, 0, sizeof(gyroAlgoInitData));

    if (m_pLMVHal->GetLMVStatus())
    {
        m_pLMVHal->GetRegSetting(&gyroAlgoInitData);
    }else
    { //Need pass from metadata!! TBD

    }

    MUINT64 gyro_t_frame_array[GYRO_DATA_PER_FRAME];
    double gyro_xyz_frame_array[GYRO_DATA_PER_FRAME*3];

    MBOOL bGyrodataExist = MTRUE;
    EISHAL_SwitchIsGyroCountNotZero(mSensorIdx, bGyrodataExist);
    if ((aTimeStamp != 0) && bGyrodataExist)
    {
        MUINT32 waitTime = 0;
        EIS_GyroRecord lastGyro;
        lastGyro.ts = 0;
        const MUINT64 currentTarget = aTimeStamp + (mSleepTime*1000L) + (mtRSTime*1000L);
        do
        {
            MBOOL bGTTarget = MFALSE;
            EISHAL_SwitchGyroQueueLock(mSensorIdx);
            mSkipWaitGyro = MFALSE;
            EISHAL_SwitchGyroQueueWait(mSensorIdx, lastGyro, currentTarget);
            EISHAL_SwitchIsLastGyroTimestampGT(mSensorIdx, currentTarget, bGTTarget);

            if (bGTTarget)
            {
                EIS_LOG("video (%lld) < global Gyro timestamp => go GIS ", currentTarget);
                EISHAL_SwitchGyroQueueUnlock(mSensorIdx);
                break;
            }

            if (mSkipWaitGyro == MTRUE)
            {
                EIS_LOG("skip wait Gyro: %d by next video trigger",waitTime);
                EISHAL_SwitchGyroQueueUnlock(mSensorIdx);
                break;
            }

            EISHAL_SwitchGyroQueueUnlock(mSensorIdx);
            waitTime++;

            //if (UNLIKELY(mDebugDump >= 1))
            {
                if (UNLIKELY(waitTime > 1))
                {
                    EIS_LOG("wait Gyro time: %d",waitTime);
                }
            }
        }while( lastGyro.ts < currentTarget);
    }

#if RECORD_WITHOUT_EIS_ENABLE
    if (g_AIMDump == 1)
    {
#if 0
        if (UNLIKELY(mDebugDump >= 1))
        {
            EIS_LOG("vHDR LE(%d), SE(%d)",OutputParam.u4videoHDRLE_us, OutputParam.u4videoHDRSE_us);
        }

        if(gvHDRRecordWriteID >= TSRECORD_MAXSIZE)
        {
                gvHDRRecordWriteID = 0;
        }
        gvHDRRecord[gvHDRRecordWriteID].id = 5;
        gvHDRRecord[gvHDRRecordWriteID].ts = ((MUINT64)OutputParam.u4videoHDRLE_us<<32);
        gvHDRRecord[gvHDRRecordWriteID].ts += (MUINT64)OutputParam.u4videoHDRSE_us;
        EIS_LOG("gTSRecord[gvHDRRecordWriteID].ts(%d)",gvHDRRecord[gvHDRRecordWriteID].ts);
        gvHDRRecordWriteID++;
#endif
        if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
        {
            gTSRecordWriteID = 0;
        }
        gTSRecord[gTSRecordWriteID].id = 3;
        gTSRecord[gTSRecordWriteID].ts = aExpTime;
        gTSRecordWriteID++;

        if ( gGyroRecordWriteID > 1)
        {
            if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
            {
                gTSRecordWriteID = 0;
            }
            gTSRecord[gTSRecordWriteID].id = 2;
            gTSRecord[gTSRecordWriteID].ts = 0; //No finding gyro timestamp

            for (int i=gGyroRecordWriteID-1;i>0;i--)
            {
                if (gGyroRecord[i].id == EIS_GYROSCOPE)
                {
                    gTSRecord[gTSRecordWriteID].ts = gGyroRecord[i].ts;
                    break;
                }
            }
            gTSRecordWriteID++;
        }else
        {
            if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
            {
                gTSRecordWriteID = 0;
            }

            gTSRecord[gTSRecordWriteID].id = 2;
            gTSRecord[gTSRecordWriteID].ts = 0; //No gyro data exist
            gTSRecordWriteID++;
        }

        if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
        {
            gTSRecordWriteID = 0;
        }
        gTSRecord[gTSRecordWriteID].id = 1;
        gTSRecord[gTSRecordWriteID].ts = aTimeStamp;
        gTSRecordWriteID++;

    }
#endif


    EISHAL_SwitchGyroQueueLock(mSensorIdx);
    EISHAL_SwitchPopGyroQueue(mSensorIdx, gyro_t_frame_array, gyro_xyz_frame_array, gyroSetProcData.gyro_num);
    EISHAL_SwitchGyroQueueUnlock(mSensorIdx);

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("Gyro data num: %d, video ts: %lld", gyroSetProcData.gyro_num, aTimeStamp);
    }

    gyroSetProcData.frame_t = aTimeStamp;
    gyroSetProcData.frame_AE = aExpTime;

    gyroSetProcData.gyro_t_frame = gyro_t_frame_array;
    gyroSetProcData.gyro_xyz_frame = gyro_xyz_frame_array;

    gyroSetProcData.rrz_crop_X = gyroAlgoInitData.rrz_crop_X;
    gyroSetProcData.rrz_crop_Y = gyroAlgoInitData.rrz_crop_Y;

    gyroSetProcData.rrz_crop_Width = gyroAlgoInitData.rrz_crop_Width;
    gyroSetProcData.rrz_crop_Height = gyroAlgoInitData.rrz_crop_Height;

    gyroSetProcData.rrz_scale_Width = gyroAlgoInitData.rrz_scale_Width;
    gyroSetProcData.rrz_scale_Height = gyroAlgoInitData.rrz_scale_Height;

    gyroSetProcData.crz_crop_X = apEisConfig->cropX;
    gyroSetProcData.crz_crop_Y = apEisConfig->cropY;

    gyroSetProcData.crz_crop_Width = apEisConfig->crzOutW;
    gyroSetProcData.crz_crop_Height = apEisConfig->crzOutH;

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("crz offset x(%d),y(%d)", gyroSetProcData.crz_crop_X, gyroSetProcData.crz_crop_Y);
        EIS_LOG("crzOut w(%d),h(%d)", gyroSetProcData.crz_crop_Width, gyroSetProcData.crz_crop_Height);
    }
    //====== GIS Algorithm ======

    err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SET_PROC_INFO,&gyroSetProcData, NULL);
    if(err != S_GYRO_OK)
    {
            EIS_ERR("GIS:GYRO_FEATURE_SET_PROC_INFO fail(0x%x)",err);
            err = EIS_RETURN_API_FAIL;
            return err;
    }

    err = m_pGisAlg->GyroMain();
    if(err != S_GYRO_OK)
    {
            EIS_ERR("GIS:GyroMain fail(0x%x)",err);
            err = EIS_RETURN_API_FAIL;
            return err;
    }


    err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_GET_MV_RESULT_INFO, NULL, &gyroMVresult);
    if(err != S_GYRO_OK)
    {
        EIS_ERR("GIS:GYRO_FEATURE_SET_PROC_INFO fail(0x%x)",err);
        err = EIS_RETURN_API_FAIL;
        return err;
    }

    return EIS_RETURN_NO_ERROR;
}


MINT32 EisHalImp::DoGis(EIS_HAL_CONFIG_DATA *apEisConfig, MINT64 aTimeStamp, MINT64 aExpTime)
{
    MINT32 err = EIS_RETURN_NO_ERROR;

    if(mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }
    DP_TRACE_CALL();

    MUINT32 i;

#if EIS_ALGO_READY
    //====== Check Config Data ======
    if(apEisConfig == NULL)
    {
        EIS_ERR("apEisConfig is NULL");
        err = EIS_RETURN_NULL_OBJ;
        return err;
    }

    //====== Setting EIS Plus Algo Process Data ======

    EIS_PLUS_SET_PROC_INFO_STRUCT eisPlusProcData;
    memset(&eisPlusProcData, 0, sizeof(eisPlusProcData));

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("GMVx(%d)", apEisConfig->gmv_X);
        EIS_LOG("GMVy(%d)", apEisConfig->gmv_Y);
        EIS_LOG("ConfX(%d)", apEisConfig->confX);
        EIS_LOG("ConfY(%d)", apEisConfig->confY);
    }

    //> Set EisPlusProcData
    if (m_pLMVHal->GetLMVStatus())
    {
        eisPlusProcData.eis_info.eis_gmv_conf[0] = apEisConfig->confX;
        eisPlusProcData.eis_info.eis_gmv_conf[1] = apEisConfig->confY;
        eisPlusProcData.eis_info.eis_gmv[0]      = apEisConfig->gmv_X;
        eisPlusProcData.eis_info.eis_gmv[1]      = apEisConfig->gmv_Y;
    }
    else
    {
        //Set EIS algo to NEVER use GMV
    }

    //> get FE block number

    MUINT32 feBlockNum = 0;

    if(apEisConfig->srzOutW <= D1_WIDTH && apEisConfig->srzOutH <= D1_HEIGHT)
    {
        feBlockNum = 8;
    }
    else if(apEisConfig->srzOutW <= EIS_FE_MAX_INPUT_W && apEisConfig->srzOutH <= EIS_FE_MAX_INPUT_H)
    {
        feBlockNum = 16;
    }
    else
    {
        feBlockNum = 32;
    }

    eisPlusProcData.block_size   = feBlockNum;
    eisPlusProcData.imgiWidth    = apEisConfig->imgiW;
    eisPlusProcData.imgiHeight   = apEisConfig->imgiH;
    eisPlusProcData.CRZoWidth    = apEisConfig->crzOutW;
    eisPlusProcData.CRZoHeight   = apEisConfig->crzOutH;
    eisPlusProcData.SRZoWidth    = apEisConfig->srzOutW;
    eisPlusProcData.SRZoHeight   = apEisConfig->srzOutH;
    eisPlusProcData.oWidth       = apEisConfig->feTargetW;
    eisPlusProcData.oHeight      = apEisConfig->feTargetH;
    eisPlusProcData.TargetWidth  = apEisConfig->gpuTargetW;
    eisPlusProcData.TargetHeight = apEisConfig->gpuTargetH;
    eisPlusProcData.cropX        = apEisConfig->cropX;
    eisPlusProcData.cropY        = apEisConfig->cropY;

    //> config EIS Plus data

    mSrzOutW = apEisConfig->srzOutW;
    mSrzOutH = apEisConfig->srzOutH;

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("mImgi(%u,%u)",eisPlusProcData.imgiWidth,eisPlusProcData.imgiHeight);
        EIS_LOG("CrzOut(%u,%u)",eisPlusProcData.CRZoWidth,eisPlusProcData.CRZoHeight);
        EIS_LOG("SrzOut(%u,%u)",eisPlusProcData.SRZoWidth,eisPlusProcData.SRZoHeight);
        EIS_LOG("FeTarget(%u,%u)",eisPlusProcData.oWidth,eisPlusProcData.oHeight);
        EIS_LOG("GpuTarget(%u,%u)",eisPlusProcData.TargetWidth,eisPlusProcData.TargetHeight);
        EIS_LOG("mCrop(%u,%u)",eisPlusProcData.cropX,eisPlusProcData.cropY);
        EIS_LOG("feBlockNum(%u)",feBlockNum);
    }

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("eisPlusProcData");
        EIS_LOG("eis_gmv_conf[0](%d)",eisPlusProcData.eis_info.eis_gmv_conf[0]);
        EIS_LOG("eis_gmv_conf[1](%d)",eisPlusProcData.eis_info.eis_gmv_conf[1]);
        EIS_LOG("eis_gmv[0](%f)",eisPlusProcData.eis_info.eis_gmv[0]);
        EIS_LOG("eis_gmv[1](%f)",eisPlusProcData.eis_info.eis_gmv[1]);
        EIS_LOG("block_size(%d)",eisPlusProcData.block_size);
        EIS_LOG("imgi(%d,%d)",eisPlusProcData.imgiWidth,eisPlusProcData.imgiHeight);
        EIS_LOG("CRZ(%d,%d)",eisPlusProcData.CRZoWidth,eisPlusProcData.CRZoHeight);
        EIS_LOG("SRZ(%d,%d)",eisPlusProcData.SRZoWidth,eisPlusProcData.SRZoHeight);
        EIS_LOG("target(%d,%d)",eisPlusProcData.TargetWidth,eisPlusProcData.TargetHeight);
        EIS_LOG("crop(%d,%d)",eisPlusProcData.cropX,eisPlusProcData.cropY);
    }

    GYRO_MV_RESULT_INFO_STRUCT gyroMVresult;
    ExecuteGyroAlgo(apEisConfig, aTimeStamp, aExpTime,gyroMVresult);
/////////////////////////////////////////////////////////////////////////////
    for(i = 0; i < 3; i++)
    {
        eisPlusProcData.sensor_info.AcceInfo[i] = gAccInfo[i];
        eisPlusProcData.sensor_info.GyroInfo[i] = gGyroInfo[i];
    }

    eisPlusProcData.sensor_info.gyro_in_mv = gyroMVresult.mv;
    eisPlusProcData.sensor_info.valid_gyro_num  = gyroMVresult.valid_gyro_num;

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("EN:(Acc,Gyro)=(%d,%d)",mAccEnable,mGyroEnable);
        EIS_LOG("EISPlus:Acc(%f,%f,%f)",eisPlusProcData.sensor_info.AcceInfo[0],eisPlusProcData.sensor_info.AcceInfo[1],eisPlusProcData.sensor_info.AcceInfo[2]);
        EIS_LOG("EISPlus:Gyro(%f,%f,%f)",eisPlusProcData.sensor_info.GyroInfo[0],eisPlusProcData.sensor_info.GyroInfo[1],eisPlusProcData.sensor_info.GyroInfo[2]);
    }

    //====== EIS Plus Algorithm ======

    err = m_pEisPlusAlg->EisPlusFeatureCtrl(EIS_PLUS_FEATURE_SET_PROC_INFO,&eisPlusProcData, NULL);
    if(err != S_EIS_PLUS_OK)
    {
        EIS_ERR("EisPlus:EIS_PLUS_FEATURE_SET_PROC_INFO fail(0x%x)",err);
        err = EIS_RETURN_API_FAIL;
        return err;
    }

    err = m_pEisPlusAlg->EisPlusMain(&mEisPlusResult);
    if(err != S_EIS_PLUS_OK)
    {
        EIS_ERR("EisPlus:EisMain fail(0x%x)",err);
        err = EIS_RETURN_API_FAIL;
        return err;
    }

    //if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("EisPlusMain- X: %d  Y: %d\n", mEisPlusResult.ClipX, mEisPlusResult.ClipY);
    }

    //====== Dynamic Debug ======
#if RECORD_WITHOUT_EIS_ENABLE
    if (g_AIMDump == 1)
    {
        mEisPlusResult.GridX[0] = 0;
        mEisPlusResult.GridX[1] = (apEisConfig->gpuTargetW-1)*16;
        mEisPlusResult.GridX[2] = 0;
        mEisPlusResult.GridX[3] = (apEisConfig->gpuTargetW-1)*16;

        mEisPlusResult.GridY[0] = 0;
        mEisPlusResult.GridY[1] = 0;
        mEisPlusResult.GridY[2] = (apEisConfig->gpuTargetH-1)*16;
        mEisPlusResult.GridY[3] = (apEisConfig->gpuTargetH-1)*16;
    }
#endif


    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_INF("EIS GPU WARP MAP");
        for(MUINT32  i = 0; i < mGpuGridW*mGpuGridH; ++i)
        {
            EIS_LOG("X[%u]=%d",i,mEisPlusResult.GridX[i]);
            EIS_LOG("Y[%u]=%d",i,mEisPlusResult.GridY[i]);
        }
    }
 #endif

    if(mDebugDump >= 1)
    {
        EIS_LOG("-");
    }

    return EIS_RETURN_NO_ERROR;
}


MINT32 EisHalImp::DoFEFMEis(EIS_HAL_CONFIG_DATA *apEisConfig, FEFM_PACKAGE *fefmData, MINT64 aTimeStamp, MINT64 aExpTime)
{
    MINT32 err = EIS_RETURN_NO_ERROR;
    MUINT32 i;

    if(mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }

    DP_TRACE_CALL();

#if EIS_ALGO_READY
    //====== Check Config Data ======
    if(apEisConfig == NULL)
    {
        EIS_ERR("apEisConfig is NULL");
        err = EIS_RETURN_NULL_OBJ;
        return err;
    }

    //====== Setting EIS Plus Algo Process Data ======

    EIS_PLUS_SET_PROC_INFO_STRUCT eisPlusProcData;
    memset(&eisPlusProcData, 0, sizeof(eisPlusProcData));

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("GMVx(%d)", apEisConfig->gmv_X);
        EIS_LOG("GMVy(%d)", apEisConfig->gmv_Y);
        EIS_LOG("ConfX(%d)", apEisConfig->confX);
        EIS_LOG("ConfY(%d)", apEisConfig->confY);
    }

    //> Set EisPlusProcData
    if (m_pLMVHal->GetLMVStatus())
    {
        eisPlusProcData.eis_info.eis_gmv_conf[0] = apEisConfig->confX;
        eisPlusProcData.eis_info.eis_gmv_conf[1] = apEisConfig->confY;
        eisPlusProcData.eis_info.eis_gmv[0]      = apEisConfig->gmv_X;
        eisPlusProcData.eis_info.eis_gmv[1]      = apEisConfig->gmv_Y;
    }
    else
    {
        //Set EIS algo to NEVER use GMV
    }

    //> get FE block number

    MUINT32 feBlockNum = 0;

    if(apEisConfig->srzOutW <= D1_WIDTH && apEisConfig->srzOutH <= D1_HEIGHT)
    {
        feBlockNum = 8;
    }
    else if(apEisConfig->srzOutW <= EIS_FE_MAX_INPUT_W && apEisConfig->srzOutH <= EIS_FE_MAX_INPUT_H)
    {
        feBlockNum = 16;
    }
    else
    {
        feBlockNum = 32;
    }

    eisPlusProcData.block_size   = feBlockNum;
    eisPlusProcData.imgiWidth    = apEisConfig->imgiW;
    eisPlusProcData.imgiHeight   = apEisConfig->imgiH;
    eisPlusProcData.CRZoWidth    = apEisConfig->crzOutW;
    eisPlusProcData.CRZoHeight   = apEisConfig->crzOutH;
    eisPlusProcData.SRZoWidth    = apEisConfig->srzOutW;
    eisPlusProcData.SRZoHeight   = apEisConfig->srzOutH;
    eisPlusProcData.oWidth       = apEisConfig->feTargetW;
    eisPlusProcData.oHeight      = apEisConfig->feTargetH;
    eisPlusProcData.TargetWidth  = apEisConfig->gpuTargetW;
    eisPlusProcData.TargetHeight = apEisConfig->gpuTargetH;
    eisPlusProcData.cropX        = apEisConfig->cropX;
    eisPlusProcData.cropY        = apEisConfig->cropY;

    //> config EIS Plus data

    mSrzOutW = apEisConfig->srzOutW;
    mSrzOutH = apEisConfig->srzOutH;

    //> set FE block number to driver

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("mImgi(%u,%u)",eisPlusProcData.imgiWidth,eisPlusProcData.imgiHeight);
        EIS_LOG("CrzOut(%u,%u)",eisPlusProcData.CRZoWidth,eisPlusProcData.CRZoHeight);
        EIS_LOG("SrzOut(%u,%u)",eisPlusProcData.SRZoWidth,eisPlusProcData.SRZoHeight);
        EIS_LOG("FeTarget(%u,%u)",eisPlusProcData.oWidth,eisPlusProcData.oHeight);
        EIS_LOG("GpuTarget(%u,%u)",eisPlusProcData.TargetWidth,eisPlusProcData.TargetHeight);
        EIS_LOG("mCrop(%u,%u)",eisPlusProcData.cropX,eisPlusProcData.cropY);
        EIS_LOG("feBlockNum(%u)",feBlockNum);
    }

    eisPlusProcData.frame_t = (MUINT64) aTimeStamp;
    eisPlusProcData.process_idx = apEisConfig->process_idx;
    eisPlusProcData.process_mode = apEisConfig->process_mode;
    //> get FEO statistic
    eisPlusProcData.fe_result.fe_cur_info[0] = fefmData->FE[0];
    eisPlusProcData.fe_result.fe_cur_info[1] = fefmData->FE[1];
    eisPlusProcData.fe_result.fe_cur_info[2] = fefmData->FE[2];

    eisPlusProcData.fe_result.fe_pre_info[0] = fefmData->LastFE[0];
    eisPlusProcData.fe_result.fe_pre_info[1] = fefmData->LastFE[1];
    eisPlusProcData.fe_result.fe_pre_info[2] = fefmData->LastFE[2];


    eisPlusProcData.fm_result.fm_fw_idx[0] = fefmData->ForwardFM[0];
    eisPlusProcData.fm_result.fm_fw_idx[1] = fefmData->ForwardFM[1];
    eisPlusProcData.fm_result.fm_fw_idx[2] = fefmData->ForwardFM[2];

    eisPlusProcData.fm_result.fm_bw_idx[0] = fefmData->BackwardFM[0];
    eisPlusProcData.fm_result.fm_bw_idx[1] = fefmData->BackwardFM[1];
    eisPlusProcData.fm_result.fm_bw_idx[2] = fefmData->BackwardFM[2];

    //int pre_res = FM_Core ( pre_frame, cur_frame, fm_fw_result );
    //int cur_res = FM_Core ( cur_frame, pre_frame, fm_bw_result );

    eisPlusProcData.fm_result.fm_pre_res[0] = fefmData->ForwardFMREG[0];
    eisPlusProcData.fm_result.fm_pre_res[1] = fefmData->ForwardFMREG[1];
    eisPlusProcData.fm_result.fm_pre_res[2] = fefmData->ForwardFMREG[2];

    eisPlusProcData.fm_result.fm_cur_res[0] = fefmData->BackwardFMREG[0];
    eisPlusProcData.fm_result.fm_cur_res[1] = fefmData->BackwardFMREG[1];
    eisPlusProcData.fm_result.fm_cur_res[2] = fefmData->BackwardFMREG[2];


    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("eisPlusProcData");
        EIS_LOG("eis_gmv_conf[0](%d)",eisPlusProcData.eis_info.eis_gmv_conf[0]);
        EIS_LOG("eis_gmv_conf[1](%d)",eisPlusProcData.eis_info.eis_gmv_conf[1]);
        EIS_LOG("eis_gmv[0](%f)",eisPlusProcData.eis_info.eis_gmv[0]);
        EIS_LOG("eis_gmv[1](%f)",eisPlusProcData.eis_info.eis_gmv[1]);
        EIS_LOG("block_size(%d)",eisPlusProcData.block_size);
        EIS_LOG("imgi(%d,%d)",eisPlusProcData.imgiWidth,eisPlusProcData.imgiHeight);
        EIS_LOG("CRZ(%d,%d)",eisPlusProcData.CRZoWidth,eisPlusProcData.CRZoHeight);
        EIS_LOG("SRZ(%d,%d)",eisPlusProcData.SRZoWidth,eisPlusProcData.SRZoHeight);
        EIS_LOG("target(%d,%d)",eisPlusProcData.TargetWidth,eisPlusProcData.TargetHeight);
        EIS_LOG("crop(%d,%d)",eisPlusProcData.cropX,eisPlusProcData.cropY);
    }

    GYRO_MV_RESULT_INFO_STRUCT gyroMVresult;
    ExecuteGyroAlgo(apEisConfig, aTimeStamp, aExpTime,gyroMVresult);

    for(i = 0; i < 3; i++)
    {
        eisPlusProcData.sensor_info.AcceInfo[i] = gAccInfo[i];
        eisPlusProcData.sensor_info.GyroInfo[i] = gGyroInfo[i];
    }

    eisPlusProcData.sensor_info.gyro_in_mv = gyroMVresult.mv;
    eisPlusProcData.sensor_info.valid_gyro_num  = gyroMVresult.valid_gyro_num;

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("EN:(Acc,Gyro)=(%d,%d)",mAccEnable,mGyroEnable);
        EIS_LOG("EISPlus:Acc(%f,%f,%f)",eisPlusProcData.sensor_info.AcceInfo[0],eisPlusProcData.sensor_info.AcceInfo[1],eisPlusProcData.sensor_info.AcceInfo[2]);
        EIS_LOG("EISPlus:Gyro(%f,%f,%f)",eisPlusProcData.sensor_info.GyroInfo[0],eisPlusProcData.sensor_info.GyroInfo[1],eisPlusProcData.sensor_info.GyroInfo[2]);
    }

    //====== EIS Plus Algorithm ======

    err = m_pEisPlusAlg->EisPlusFeatureCtrl(EIS_PLUS_FEATURE_SET_PROC_INFO,&eisPlusProcData, NULL);
    if(err != S_EIS_PLUS_OK)
    {
        EIS_ERR("EisPlus:EIS_PLUS_FEATURE_SET_PROC_INFO fail(0x%x)",err);
        err = EIS_RETURN_API_FAIL;
        return err;
    }

    err = m_pEisPlusAlg->EisPlusMain(&mEisPlusResult);
    if(err != S_EIS_PLUS_OK)
    {
        EIS_ERR("EisPlus:EisMain fail(0x%x)",err);
        err = EIS_RETURN_API_FAIL;
        return err;
    }

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("EisPlusMain- X: %d  Y: %d\n", mEisPlusResult.ClipX, mEisPlusResult.ClipY);
    }

    //====== Dynamic Debug ======
#if RECORD_WITHOUT_EIS_ENABLE
    if (g_AIMDump == 1)
    {
        mEisPlusResult.GridX[0] = 0;
        mEisPlusResult.GridX[1] = (apEisConfig->gpuTargetW-1)*16;
        mEisPlusResult.GridX[2] = 0;
        mEisPlusResult.GridX[3] = (apEisConfig->gpuTargetW-1)*16;

        mEisPlusResult.GridY[0] = 0;
        mEisPlusResult.GridY[1] = 0;
        mEisPlusResult.GridY[2] = (apEisConfig->gpuTargetH-1)*16;
        mEisPlusResult.GridY[3] = (apEisConfig->gpuTargetH-1)*16;

        if(g_forecDejello == 1)
        {
            MUINT32 t,s;
            MUINT32 oWidth = apEisConfig->gpuTargetW;
            MUINT32 oHeight = apEisConfig->gpuTargetH;


            for(t=0;t<mGpuGridH;t++)
            {
                for(s=0;s<mGpuGridW;s++)
                {
                    float indx_x = (float)(oWidth-1)*s/(mGpuGridW-1);
                    float indx_y = (float)(oHeight-1)*t/(mGpuGridH-1);
                    mEisPlusResult.GridX[t*mGpuGridW+s] = (int)(indx_x*16);
                    mEisPlusResult.GridY[t*mGpuGridW+s] = (int)(indx_y*16);
                }
            }
        }

    }
#endif


    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_INF("EIS GPU WARP MAP");
        for(MUINT32  i = 0; i < mGpuGridW*mGpuGridH; ++i)
        {
            EIS_LOG("X[%u]=%d",i,mEisPlusResult.GridX[i]);
            EIS_LOG("Y[%u]=%d",i,mEisPlusResult.GridY[i]);
        }
    }
 #endif


    if(mDebugDump >= 1)
    {
        EIS_LOG("-");
    }

    return EIS_RETURN_NO_ERROR;
}



MINT32 EisHalImp::DoCalibration(LMV_HAL_CONFIG_DATA *apLMVConfig, MINT64 aTimeStamp, MINT64 aExpTime)
{
    MINT32 err = EIS_RETURN_NO_ERROR;
    MUINT32 i;
    MUINT32 aWidth = 0,aHeight = 0;

    if (mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return EIS_RETURN_NO_ERROR;
    }

#if RECORD_WITHOUT_EIS_ENABLE

    if (g_AIMDump == 1)
    {
#if 0
        EIS_LOG("vHDR LE(%d), SE(%d)",OutputParam.u4videoHDRLE_us, OutputParam.u4videoHDRSE_us);
        if(gvHDRRecordWriteID >= TSRECORD_MAXSIZE)
        {
                gvHDRRecordWriteID = 0;
        }
        gvHDRRecord[gvHDRRecordWriteID].id = 5;
        gvHDRRecord[gvHDRRecordWriteID].ts = ((MUINT64)OutputParam.u4videoHDRLE_us<<32);
        gvHDRRecord[gvHDRRecordWriteID].ts += (MUINT64)OutputParam.u4videoHDRSE_us;
        EIS_LOG("gTSRecord[gvHDRRecordWriteID].ts(%d)",gvHDRRecord[gvHDRRecordWriteID].ts);
        gvHDRRecordWriteID++;
#endif
        if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
        {
                gTSRecordWriteID = 0;
        }
        gTSRecord[gTSRecordWriteID].id = 3;
        gTSRecord[gTSRecordWriteID].ts = aExpTime;
        gTSRecordWriteID++;

        if ( gGyroRecordWriteID > 1)
        {
                if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
                {
                    gTSRecordWriteID = 0;
                }
                gTSRecord[gTSRecordWriteID].id = 2;
                gTSRecord[gTSRecordWriteID].ts = 0; //No finding gyro timestamp

                for (int i=gGyroRecordWriteID-1;i>0;i--)
                {
                    if (gGyroRecord[i].id == EIS_GYROSCOPE)
                    {
                        gTSRecord[gTSRecordWriteID].ts = gGyroRecord[i].ts;
                        break;
                    }
                }
                gTSRecordWriteID++;
        }else
        {
            if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
            {
                    gTSRecordWriteID = 0;
            }

            gTSRecord[gTSRecordWriteID].id = 2;
            gTSRecord[gTSRecordWriteID].ts = 0; //No gyro data exist
            gTSRecordWriteID++;
        }

        if(gTSRecordWriteID >= TSRECORD_MAXSIZE)
        {
                gTSRecordWriteID = 0;
        }
        gTSRecord[gTSRecordWriteID].id = 1;
        gTSRecord[gTSRecordWriteID].ts = aTimeStamp;
        gTSRecordWriteID++;

    }
#endif

    //====== Check Config Data ======

    if(apLMVConfig == NULL)
    {
        EIS_ERR("apEisConfig is NULL");
        err = EIS_RETURN_NULL_OBJ;
        return err;
    }

    GYRO_INIT_INFO_STRUCT  gyroAlgoInitData;
    memset(&gyroAlgoInitData, 0, sizeof(gyroAlgoInitData));

    //Pass from P1Node maybe better
    if (m_pLMVHal->GetLMVStatus())
    {
        m_pLMVHal->GetRegSetting(&gyroAlgoInitData);
        m_pLMVHal->GetLMVInputSize(&aWidth, &aHeight);
    }else
    { //Need pass from metadata!! TBD

    }

    if ((aWidth != mGisInputW) || (aHeight != mGisInputH))
    {
        mChangedInCalibration++;
    }


    GYRO_SET_PROC_INFO_STRUCT gyroSetProcData;
    memset(&gyroSetProcData,0,sizeof(gyroSetProcData));

    gyroSetProcData.frame_t= aTimeStamp;
    gyroSetProcData.frame_AE = (MINT32)(aExpTime / 1000);

    gyroSetProcData.rrz_crop_X = gyroAlgoInitData.rrz_crop_X;
    gyroSetProcData.rrz_crop_Y = gyroAlgoInitData.rrz_crop_Y;

    gyroSetProcData.rrz_crop_Width = gyroAlgoInitData.rrz_crop_Width;
    gyroSetProcData.rrz_crop_Height = gyroAlgoInitData.rrz_crop_Height;

    gyroSetProcData.rrz_scale_Width = gyroAlgoInitData.rrz_scale_Width;
    gyroSetProcData.rrz_scale_Height = gyroAlgoInitData.rrz_scale_Height;

    //CRZ relative information is deprecated later!!!
    gyroSetProcData.crz_crop_X = 0;
    gyroSetProcData.crz_crop_Y = 0;

    gyroSetProcData.crz_crop_Width = 0;
    gyroSetProcData.crz_crop_Height = 0;

    //EIS_LOG("crz offset x(%d),y(%d)", gyroSetProcData.crz_crop_X, gyroSetProcData.crz_crop_Y);
    //EIS_LOG("crzOut w(%d),h(%d)", gyroSetProcData.crz_crop_Width, gyroSetProcData.crz_crop_Height);

    EIS_LOG("frame_AE(%d), rrz_crop_X(%d), rrz_crop_Y(%d), rrz_crop_Width(%d), rrz_crop_Height(%d), rrz_out_Width(%d), rrz_out_Height(%d)",
             gyroSetProcData.frame_AE, gyroSetProcData.rrz_crop_X, gyroSetProcData.rrz_crop_Y,
             gyroSetProcData.rrz_crop_Width, gyroSetProcData.rrz_crop_Height,
             gyroSetProcData.rrz_scale_Width, gyroSetProcData.rrz_scale_Height);


    MBOOL bDoCalibration = MTRUE;
    MUINT64 gyro_t_frame_array[GYRO_DATA_PER_FRAME];
    double gyro_xyz_frame_array[GYRO_DATA_PER_FRAME*3];
    GyroEISStatistics currentEisResult;//TBD- Holmes 2016/0620 - Pass from P1Node

    EISHAL_SwitchGyroQueueLock(mSensorIdx);
    EISHAL_SwitchPopGyroQueue(mSensorIdx, gyro_t_frame_array, gyro_xyz_frame_array, gyroSetProcData.gyro_num);
    EISHAL_SwitchGyroQueueUnlock(mSensorIdx);

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("Gyro data num: %d, video ts: %lld", gyroSetProcData.gyro_num, aTimeStamp);
    }

    gyroSetProcData.gyro_t_frame = gyro_t_frame_array;
    gyroSetProcData.gyro_xyz_frame = gyro_xyz_frame_array;

    //EIS statistics
    for (MINT32 i=0; i<EIS_WIN_NUM; i++)
    {
        currentEisResult.eis_data[4*i + 0] = apLMVConfig->lmvData.i4LMV_X[i];
        currentEisResult.eis_data[4*i + 1] = apLMVConfig->lmvData.i4LMV_Y[i];
        currentEisResult.eis_data[4*i + 2] = apLMVConfig->lmvData.NewTrust_X[i];
        currentEisResult.eis_data[4*i + 3] = apLMVConfig->lmvData.NewTrust_Y[i];
    }

#if EIS_ALGO_READY
    //====== GIS Algorithm ======
    if (bDoCalibration)
    {
        gyroSetProcData.val_LMV = mbLastCalibration;
        mbLastCalibration = MTRUE;
        gyroSetProcData.EIS_LMV = currentEisResult.eis_data;
        err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SET_PROC_INFO,&gyroSetProcData, NULL);
        if(err != S_GYRO_OK)
        {
            EIS_ERR("GIS:GYRO_FEATURE_SET_PROC_INFO fail(0x%x)",err);
            err = EIS_RETURN_API_FAIL;
            return err;
        }

        err = m_pGisAlg->GyroMain();
        if(err != S_GYRO_OK)
        {
            EIS_ERR("GIS:GyroMain fail(0x%x)",err);
            err = EIS_RETURN_API_FAIL;
            return err;
        }
    }else
    {
        mbLastCalibration = MFALSE;
        EIS_LOG("Bypass calibration");
    }

    GYRO_CAL_RESULT_INFO_STRUCT gyroCal;
    memset(&gyroCal,0,sizeof(gyroCal));
    err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_GET_CAL_RESULT_INFO, NULL,&gyroCal);
    if(err != S_GYRO_OK)
    {
        EIS_ERR("GIS:GYRO_FEATURE_SET_PROC_INFO fail(0x%x)",err);
        err = EIS_RETURN_API_FAIL;
        return err;
    }

    {
        EIS_LOG("Cal:frame(%d) valid (%d) , %f    %f    %f    %f    %f    %f", mDoEisCount, gyroCal.dataValid, gyroCal.paramFinal[0], gyroCal.paramFinal[1], gyroCal.paramFinal[2],
                                                                                                       gyroCal.paramFinal[3], gyroCal.paramFinal[4], gyroCal.paramFinal[5] );
    }

    if (mChangedInCalibration > 0)
    {
        gyroCal.dataValid = 3; //Means size is changed in the calibration
    }

    if (mChangedInCalibration == 1)
    {
        EIS_LOG("Cal:frame(%d), Initial W/H (%d/%d) , Current W/H    (%d/%d)", mDoEisCount, mGisInputW, mGisInputH, aWidth, aHeight);
    }

    if (gyroCal.dataValid == 0) //Calibration pass and write to the nvram
    {
        m_pNVRAM_defParameter->gis_defParameter3[0] = gyroCal.paramFinal[0];
        m_pNVRAM_defParameter->gis_defParameter3[1] = gyroCal.paramFinal[1];
        m_pNVRAM_defParameter->gis_defParameter3[2] = gyroCal.paramFinal[2];
        m_pNVRAM_defParameter->gis_defParameter3[3] = gyroCal.paramFinal[3];
        m_pNVRAM_defParameter->gis_defParameter3[4] = gyroCal.paramFinal[4];
        m_pNVRAM_defParameter->gis_defParameter3[5] = gyroCal.paramFinal[5];

        mRecordParameter[0] = gyroCal.paramFinal[0];
        mRecordParameter[1] = gyroCal.paramFinal[1];
        mRecordParameter[2] = gyroCal.paramFinal[2];
        mRecordParameter[3] = gyroCal.paramFinal[3];
        mRecordParameter[4] = gyroCal.paramFinal[4];
        mRecordParameter[5] = gyroCal.paramFinal[5];

    }else if ( (gyroCal.dataValid == 1) || (gyroCal.dataValid == 2) || (gyroCal.dataValid == 4)) //Calibration failed and only used in recording
    {
        mRecordParameter[0] = gyroCal.paramFinal[0];
        mRecordParameter[1] = gyroCal.paramFinal[1];
        mRecordParameter[2] = gyroCal.paramFinal[2];
        mRecordParameter[3] = gyroCal.paramFinal[3];
        mRecordParameter[4] = gyroCal.paramFinal[4];
        mRecordParameter[5] = gyroCal.paramFinal[5];
    }else
    {
        mRecordParameter[0] = m_pNVRAM_defParameter->gis_defParameter3[0];
        mRecordParameter[1] = m_pNVRAM_defParameter->gis_defParameter3[1];
        mRecordParameter[2] = m_pNVRAM_defParameter->gis_defParameter3[2];
        mRecordParameter[3] = m_pNVRAM_defParameter->gis_defParameter3[3];
        mRecordParameter[4] = m_pNVRAM_defParameter->gis_defParameter3[4];
        mRecordParameter[5] = m_pNVRAM_defParameter->gis_defParameter3[5];
    }

    if (UNLIKELY(mEMEnabled == 1))
    {
        if ((gyroCal.dataValid == 0) || (gyroCal.dataValid == 2) || (gyroCal.dataValid == 5))
        {
            if (mbEMSaveFlag == MFALSE)
            {
                err = m_pGisAlg->GyroFeatureCtrl(GYRO_FEATURE_SAVE_EM_INFO, (MVOID *)&mTsForAlgoDebug, NULL);
                if(err != S_GYRO_OK)
                {
                    EIS_ERR("GyroFeatureCtrl(GYRO_FEATURE_SAVE_EM_INFO) fail(0x%x)",err);
                }
                mbEMSaveFlag = MTRUE;
            }
        }
    }

#endif

    if (UNLIKELY(mDebugDump >= 1))
    {
        EIS_LOG("-");
    }

    return EIS_RETURN_NO_ERROR;
}



/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::SetEisPlusGpuInfo(MINT32 * const aGridX, MINT32 * const aGridY)
{
    mEisPlusResult.GridX = aGridX;
    mEisPlusResult.GridY = aGridY;

    if(mDebugDump >= 1)
    {
        EIS_LOG("[IN]grid VA(0x%p,0x%p)", (MVOID*)aGridX, (MVOID*)aGridY);
        EIS_LOG("[MEMBER]grid VA(0x%p,0x%p)",(MVOID*)mEisPlusResult.GridX, (MVOID*)mEisPlusResult.GridY);
    }
}

/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::SetEISPlusGpuWarpDZ(MUINT32 eis_in_width,
                                     MUINT32 eis_in_height,
                                     MUINT32 target_width,
                                     MUINT32 target_height,
                                     MUINT32 crop_offset_x,
                                     MUINT32 crop_offset_y,
                                     MUINT32 crop_width,
                                     MUINT32 crop_height,
                                     MUINT32 eis_fator)
{
    /*                  eis_in_width
     *      ---------------------------------------
     *     |              src_width                |
     *     |    -------------------------------    |
     *     |   |                               |   |
     *     |   |                               |   |
     *     |   | crop_offset    crop_width     |   |
     *     |<------------------->-----------   |   |
     *     |   |             |  | offset    |  |   |
     * eis_|  src_    crop_  |--*           |  |   |
     * in_ |  height  height |      DZ      |  |   |
     * height  |             |              |  |   |
     *     |   |              --------------   |   |
     *     |   |                               |   |
     *     |    -------------------------------    |
     *     |                                       |
     *      ---------------------------------------
     */

    /* Calculate matrix parameters */
    MUINT32 cropped_src_w = eis_in_width*100.0f/eis_fator;
    MUINT32 cropped_src_h = eis_in_height*100.0f/eis_fator;

    MUINT32 src2crop_x = (eis_in_width - cropped_src_w) >> 1;
    MUINT32 src2crop_y = (eis_in_height - cropped_src_h) >> 1;

    MUINT32 src_width  = eis_in_width*100.0f/eis_fator;
    MUINT32 src_height = eis_in_height*100.0f/eis_fator;

    MUINT32 offset_x = crop_offset_x - src2crop_x;
    MUINT32 offset_y = crop_offset_y - src2crop_y;

    /* If corpped source < target size, we have to scale up */
    if (cropped_src_w<target_width)
    {
        crop_width *= (target_width/cropped_src_w);
    }

    if (cropped_src_h<target_height)
    {
        crop_height *= (target_height/cropped_src_h);
    }

    // Simple checking
    if ( eis_fator<100 )
    {
        EIS_WRN("eis_fator=%d. EIS fator should not be smaller than 100!",
                eis_fator);
    }
    if ( offset_x>crop_width || offset_y>crop_height )
    {
        EIS_WRN("offset_x=%d, offset_y=%d. GPU warp matrix offset is invalid!",
                offset_x, offset_y);
    }
    if( mDebugDump > 0 )
    {
        EIS_LOG("src_w=%d, src_h=%d, offset_x=%d, offset_y=%d, crop_w=%d, crop_h=%d",
                src_width, src_height,
                offset_x, offset_y,
                crop_width, crop_height);
    }

    /* Re-calculate warp matrix with DZ */
    MFLOAT dz_new_x[mGpuGridH*mGpuGridW];
    MFLOAT dz_new_y[mGpuGridH*mGpuGridW];
    memset(&dz_new_x, 0, sizeof(dz_new_x));
    memset(&dz_new_y, 0, sizeof(dz_new_y));

    float dz_x_step = (float)(crop_width-1)/EIS_MAX((mGpuGridW-1), (MUINT32)1);
    float dz_y_step = (float)(crop_height-1)/EIS_MAX((mGpuGridH-1), (MUINT32)1);
    float src_x_step = (float)(src_width-1)/EIS_MAX((mGpuGridW-1), (MUINT32)1);
    float src_y_step = (float)(src_height-1)/EIS_MAX((mGpuGridH-1), (MUINT32)1);

    for( unsigned y = 0; y < mGpuGridH; ++y )
    {
        MUINT32 img_y          = offset_y + y*dz_y_step;
        MUINT32 src_idx_y      = EIS_FLOOR(img_y/src_y_step);
        float y_alpha          = img_y - src_idx_y*src_y_step;
        float y_alpha_norm     = y_alpha / src_y_step;
        float y_alpha_inv_norm = (src_y_step - y_alpha) / src_y_step;
        MUINT32 grid_y         = src_idx_y*mGpuGridW;
        MUINT32 grid_y_next    = EIS_MIN((src_idx_y+1), (mGpuGridH-1))*mGpuGridW;
        MUINT32 new_grid_y     = y*mGpuGridW;

        /* Interpolate warp matrix from source matrix */
        for( unsigned x = 0; x < mGpuGridW; ++x )
        {
            MUINT32 img_x          = offset_x + x*dz_x_step;
            MUINT32 src_idx_x      = EIS_FLOOR(img_x/src_x_step) ;
            MUINT32 src_idx_x_next = EIS_MIN(src_idx_x + 1, mGpuGridW-1);
            float x_alpha          = img_x - src_idx_x*src_x_step;
            float x_alpha_norm     = x_alpha / src_x_step;
            float x_alpha_inv_norm = (src_x_step - x_alpha) / src_x_step;

            float dz_tmp0_x = mEisPlusResult.GridX[grid_y + src_idx_x] * x_alpha_inv_norm +
                              mEisPlusResult.GridX[grid_y + src_idx_x_next] * x_alpha_norm;
            float dz_tmp0_y = mEisPlusResult.GridY[grid_y + src_idx_x] * x_alpha_inv_norm +
                              mEisPlusResult.GridY[grid_y + src_idx_x_next] * x_alpha_norm;
            float dz_tmp1_x = mEisPlusResult.GridX[grid_y_next + src_idx_x] * x_alpha_inv_norm +
                              mEisPlusResult.GridX[grid_y_next + src_idx_x_next] * x_alpha_norm;
            float dz_tmp1_y = mEisPlusResult.GridY[grid_y_next + src_idx_x] * x_alpha_inv_norm +
                              mEisPlusResult.GridY[grid_y_next + src_idx_x_next] * x_alpha_norm;

            dz_new_x[new_grid_y+x] = dz_tmp0_x * y_alpha_inv_norm + dz_tmp1_x * y_alpha_norm;
            dz_new_y[new_grid_y+x] = dz_tmp0_y * y_alpha_inv_norm + dz_tmp1_y * y_alpha_norm;
        }
    }

    /* Update to GPU warp matrix */
    for (MUINT32 i = 0; i<mGpuGridH*mGpuGridW; ++i)
    {
        mEisPlusResult.GridX[i] = dz_new_x[i];
        mEisPlusResult.GridY[i] = dz_new_y[i];
        if( mDebugDump > 0 )
        {
            EIS_LOG("new X[%u]=%d", i, mEisPlusResult.GridX[i]);
            EIS_LOG("new Y[%u]=%d", i, mEisPlusResult.GridY[i]);
        }
    }
}


/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::GetEisPlusResult(P_EIS_PLUS_RESULT_INFO_STRUCT apEisPlusResult)
{
    if(mEisSupport == MFALSE)
    {
        EIS_LOG("mSensorIdx(%u) not support EIS",mSensorIdx);
        return;
    }

    apEisPlusResult->ClipX = mEisPlusResult.ClipX;
    apEisPlusResult->ClipY = mEisPlusResult.ClipY;

    if(mDebugDump >= 1)
    {
        EIS_LOG("Clip(%u,%u)",apEisPlusResult->ClipX,apEisPlusResult->ClipY);
    }
}

/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::GetEisPlusGpuInfo(EIS_PLUS_GPU_INFO *aEisPlusGpu, MUINT32 eisMode)
{
    EIS25_Customize_Tuning_Para_t customSetting;
    EISCustom::getEIS25Data(&customSetting);

    MBOOL enDejello = EIS_MODE_IS_EIS_25_IMAGE_ONLY_ENABLED(eisMode) ? 0 : customSetting.en_dejello;

#if RECORD_WITHOUT_EIS_ENABLE
    if (UNLIKELY(enDejello != g_forecDejello))
    {
        enDejello = g_forecDejello;
        EIS_LOG("Force dejello: %d", enDejello);
    }
#endif

    if (EISCustom::isEnabledEIS25_GyroMode())
    {
        if (enDejello)
        {
            mGpuGridW = 31;
            mGpuGridH = 18;
        }else
        {
            mGpuGridW = 2;
            mGpuGridH = 2;
        }
    }
    else
    {
        mGpuGridW = 2;
        mGpuGridH = 2;
    }
    aEisPlusGpu->gridW = mGpuGridW;
    aEisPlusGpu->gridH = mGpuGridH;

    EIS_LOG("W(%u),H(%u)",aEisPlusGpu->gridW,aEisPlusGpu->gridH);
}


/*******************************************************************************
*
********************************************************************************/
MUINT32 EisHalImp::GetEisPlusCropRatio()
{
#if RECORD_WITHOUT_EIS_ENABLE
    if (g_AIMDump == 1)
    {
        return EIS_NONE_FACTOR;
    }
#endif
    if(mDebugDump >= 1)
    {
        EIS_LOG("mEisPlusCropRatio(%d)",mEisPlusCropRatio);
    }
    return mEisPlusCropRatio;
}



#if EIS_ALGO_READY
/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::GetEisPlusCustomize(EIS_PLUS_TUNING_PARA_STRUCT *a_pTuningData)
{
    if (mDebugDump >= 1)
    {
        EIS_LOG("+");
    }

    EIS_PLUS_Customize_Para_t customSetting;

    EISCustom::getEISPlusData(&customSetting);

    a_pTuningData->warping_mode           = static_cast<MINT32>(customSetting.warping_mode);
    a_pTuningData->effort                 = 2;  // limit to 400 points
    a_pTuningData->search_range_x         = customSetting.search_range_x;
    a_pTuningData->search_range_y         = customSetting.search_range_y;
    a_pTuningData->crop_ratio             = customSetting.crop_ratio;
    a_pTuningData->gyro_still_time_th = customSetting.gyro_still_time_th;
    a_pTuningData->gyro_max_time_th = customSetting.gyro_max_time_th;
    a_pTuningData->gyro_similar_th = customSetting.gyro_similar_th;
    a_pTuningData->stabilization_strength = customSetting.stabilization_strength;

    if (mDebugDump >= 1)
    {
        EIS_LOG("-");
    }
}

/*******************************************************************************
*
********************************************************************************/
MVOID EisHalImp::GetEis25Customize(EIS25_TUNING_PARA_STRUCT *a_pTuningData)
{
    if (mDebugDump >= 1)
    {
        EIS_LOG("+");
    }

    EIS25_Customize_Tuning_Para_t customSetting;

    EISCustom::getEIS25Data(&customSetting);

    a_pTuningData->en_dejello             = customSetting.en_dejello;
    a_pTuningData->stabilization_strength = customSetting.stabilization_strength;
    a_pTuningData->stabilization_level    = customSetting.stabilization_level;
    a_pTuningData->gyro_still_mv_th       = customSetting.gyro_still_mv_th;
    a_pTuningData->gyro_still_mv_diff_th  = customSetting.gyro_still_mv_diff_th;

    if (mDebugDump >= 1)
    {
        EIS_LOG("-");
    }
}
#endif

