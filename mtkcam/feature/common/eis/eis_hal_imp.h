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
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/

/**
* @file eis_hal_imp.h
*
* EIS Hal Implementation Header File
*
*/


#ifndef _EIS_HAL_IMP_H_
#define _EIS_HAL_IMP_H_

#include <Mutex.h>
#include <Condition.h>

#include <mtkcam/feature/eis/eis_hal.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>


#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)

#define MAX_MEMORY_SIZE (40)

typedef enum EIS_HAL_STATE
{
    EIS_HAL_STATE_NONE   = 0x0000,
    EIS_HAL_STATE_ALIVE  = 0x0001,
    EIS_HAL_STATE_UNINIT = 0x0002
}EIS_HAL_STATE_ENUM;


#define TSRECORD_MAXSIZE    (108000)
#define EIS_ACCELEROMETER   (1)
#define EIS_GYROSCOPE          (2)
#define GYRO_DATA_PER_FRAME     (100)

typedef struct EIS_GyroRecord_t
{
    MINT32  id;
    MFLOAT x;
    MFLOAT y;
    MFLOAT z;
    MUINT64 ts;
}EIS_GyroRecord;


typedef struct GyroEISStatistics_t
{
    MINT32  eis_data[EIS_WIN_NUM*4];
    MUINT64 ts;
}GyroEISStatistics;

struct NVRAM_CAMERA_FEATURE_STRUCT_t;
struct NVRAM_CAMERA_FEATURE_GIS_STRUCT_t;

class LMVHal;
class SensorListener;
struct ASensorEvent;

/**
  *@class EisHalImp
  *@brief Implementation of EisHal class
*/
class EisHalImp : public EisHal
{

    template<const unsigned int> friend class EisHalObj;
private:
    EisHalImp(const EisHalImp&);
    EisHalImp &operator = (const EisHalImp&);
    /**
         *@brief EisHalImp constructor
       */
    EisHalImp(const MUINT32 &aSensorIdx, MUINT32 MultiSensor = 0);

    /**
         *@brief EisHalImp destructor
       */
    ~EisHalImp() {}

public:

    /**
         *@brief Create EisHal object
         *@param[in] aSensorIdx : sensor index
         *@return
         *-EisHal object
       */
    static EisHal *GetInstance(const MUINT32 &aSensorIdx, MUINT32 MultiSensor = 0);

    /**
         *@brief Destroy EisHal object
         *@param[in] userName : user name,i.e. who destroy EIS object
       */
    virtual MVOID DestroyInstance(char const *userName);

    /**
         *@brief Initialization function
         *@param[in] aSensorIdx : sensor index
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 Init();

    /**
         *@brief Unitialization function
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 Uninit();

    /**
         *@brief Configure EIS
         *@details Use this API after pass1/pass2 config and before pass1/pass2 start
         *@param[in] aEisPass : indicate pass1 or pass2
         *@param[in] aEisConfig : EIS config data
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 ConfigFFFMEis(const EIS_HAL_CONFIG_DATA &aEisConfig, const MUINT32 eisMode = 0, const MULTISCALE_INFO* apMultiScaleInfo = NULL);

    virtual MINT32 ConfigGis(const EIS_HAL_CONFIG_DATA &aEisConfig);

    virtual MINT32 ConfigCalibration(const LMV_HAL_CONFIG_DATA &aLMVConfig);

    /**
         *@brief Execute EIS
         *@param[in] aEisPass : indicate pass1 or pass2
         *@param[in] apEisConfig : EIS config data, mainly for pass2
         *@param[in] aTimeStamp : time stamp of pass1 image
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 AbortP2Calibration();

    virtual MINT32 ForcedDoEisPass2();

    virtual MINT32 DoGis(EIS_HAL_CONFIG_DATA *apEisConfig = NULL, MINT64 aTimeStamp = -1, MINT64 aExpTime = 0);

    virtual MINT32 DoFEFMEis(EIS_HAL_CONFIG_DATA *apEisConfig = NULL, FEFM_PACKAGE *fefmData = NULL, MINT64 aTimeStamp = -1, MINT64 aExpTime = 0);

    virtual MINT32 DoCalibration(LMV_HAL_CONFIG_DATA *apLMVConfig = NULL, MINT64 aTimeStamp = -1, MINT64 aExpTime = 0);


    /**
         *@brief Get EIS Plus GPU infomation
         *@param[in] aGridX : pointer to array
         *@param[in] aGridY : pointer to array
       */
    virtual MVOID SetEisPlusGpuInfo(MINT32 * const aGridX, MINT32 * const aGridY);

    /**
         *@brief Set GPU warp matrix with DZ
         *@param[in] eis_in_width  : video source width
         *@param[in] eis_in_height : video source height
         *@param[in] target_width  : record target width
         *@param[in] target_height : record target height
         *@param[in] crop_offset_x : crop x offset
         *@param[in] crop_offset_y : crop y offset
         *@param[in] crop_width    : DZ crop width
         *@param[in] crop_height   : DZ crop height
         *@param[in] eis_fator     : eis factor
      */

    virtual MVOID SetEISPlusGpuWarpDZ(MUINT32 eis_in_width,
                                      MUINT32 eis_in_height,
                                      MUINT32 target_width,
                                      MUINT32 target_height,
                                      MUINT32 crop_offset_x,
                                      MUINT32 crop_offset_y,
                                      MUINT32 crop_width,
                                      MUINT32 crop_height,
                                      MUINT32 eis_fator);


    /**
         *@brief Get EIS algorithm result (CMV)
         *@param[out] P_EIS_PLUS_RESULT_INFO_STRUCT : EISPlus result
       */
    virtual MVOID GetEisPlusResult(P_EIS_PLUS_RESULT_INFO_STRUCT apEisPlusResult);

    /**
         *@brief Get EIS Plus GPU infomation
         *@param[in,out] aEisPlusGpu : EIS_PLUS_GPU_INFO
       */
    virtual MVOID GetEisPlusGpuInfo(EIS_PLUS_GPU_INFO *aEisPlusGpu, MUINT32 eisMode = 0);

    /**
         *@brief Get EIS plus crop ratio
         *@return
         *-EIS plus crop ratio
       */
    virtual MUINT32 GetEisPlusCropRatio();

private:

    /**
         *@brief Get sensor info
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    MINT32 GetSensorInfo();

#if EIS_ALGO_READY
    /**
         *@brief Get EIS2.0 customize info
         *@param[out] a_pTuningData : EIS_PLUS_TUNING_PARA_STRUCT
       */
    MVOID GetEisPlusCustomize(EIS_PLUS_TUNING_PARA_STRUCT *a_pTuningData);

    /**
         *@brief Get EIS2.5 customize info
         *@param[out] a_pTuningData : EIS25_TUNING_PARA_STRUCT
       */
    MVOID GetEis25Customize(EIS25_TUNING_PARA_STRUCT *a_pTuningData);

#endif

    /**
         *@brief Create IMem buffer
         *@param[in,out] memSize : memory size, will align to L1 cache
         *@param[in] bufCnt : how many buffer
         *@param[in,out] bufInfo : IMem object
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    MINT32 CreateMemBuf(MUINT32 memSize, android::sp<IImageBuffer>& spImageBuf);
    /**
         *@brief Destroy IMem buffer
         *@param[in] bufCnt : how many buffer
         *@param[in,out] bufInfo : IMem object
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    MINT32 DestroyMemBuf(android::sp<IImageBuffer>& spImageBuf);
    MINT32 EnableSensor();

    MINT32 CreateEISPlusAlgoBuf();
    MINT32 ConfigGyroAlgo(const EIS_HAL_CONFIG_DATA &aEisConfig);
    MINT32 ExecuteGyroAlgo(const EIS_HAL_CONFIG_DATA* apEisConfig,const MINT64 aTimeStamp, const MINT64 aExpTime,GYRO_MV_RESULT_INFO_STRUCT& gyroMVresult);

    /***************************************************************************************/

    volatile MINT32 mUsers;
    mutable Mutex mLock;
    mutable Mutex mP1Lock;
    mutable Mutex mP2Lock;

#if EIS_ALGO_READY

    // EIS algo object
    MTKEis *m_pEisAlg;
    MTKEisPlus *m_pEisPlusAlg;
    EIS_SET_PROC_INFO_STRUCT mEisAlgoProcData;

    MTKGyro* m_pGisAlg;

#endif

    // IMEM
    //IMemDrv *m_pIMemDrv;
    android::sp<IImageBuffer> m_pEisDbgBuf;
    android::sp<IImageBuffer> m_pEisPlusWorkBuf;
    android::sp<IImageBuffer> m_pGisWorkBuf;

    // EIS member variable
    MUINT32 mEisInput_W;
    MUINT32 mEisInput_H;
    MUINT32 mP1Target_W;
    MUINT32 mP1Target_H;

    // EIS Plus Member Variable
    MUINT32 mSrzOutW;
    MUINT32 mSrzOutH;
    MUINT32 mGpuGridW;
    MUINT32 mGpuGridH;

    // EIS result
    MUINT32 mDoEisCount;    //Vent@20140427: Add for EIS GMV Sync Check.
    MUINT32 mCmvX_Int;
    MUINT32 mCmvX_Flt;
    MUINT32 mCmvY_Int;
    MUINT32 mCmvY_Flt;
    MINT32  mMVtoCenterX;
    MINT32  mMVtoCenterY;

    MINT32 mGMV_X;
    MINT32 mGMV_Y;
    EIS_GET_PLUS_INFO_STRUCT mEisLastData2EisPlus;

    // EISPlus result
    EIS_PLUS_RESULT_INFO_STRUCT mEisPlusResult;

    // member variable
    MUINT32 mFrameCnt;
    MUINT32 mIsEisConfig;
    MUINT32 mIsEisPlusConfig;
    MUINT32 mEisPlusCropRatio;
    MBOOL   mEisSupport;

    // sensor listener
    SensorListener *mpSensorListener;

    // sensor
    IHalSensorList *m_pHalSensorList;
    IHalSensor *m_pHalSensor;

    const MUINT32 mSensorIdx;
    const MUINT32 mMultiSensor;
    MUINT32 mSensorDev;
    SensorStaticInfo mSensorStaticInfo;
    SensorDynamicInfo mSensorDynamicInfo;
    MBOOL mGyroEnable;
    MBOOL mAccEnable;
    MUINT64 mTsForAlgoDebug;
    NVRAM_CAMERA_FEATURE_STRUCT_t* m_pNvram;
    MUINT32 mChangedInCalibration;
    MUINT32 mGisInputW;
    MUINT32 mGisInputH;
    MBOOL   mNVRAMRead;
    MUINT64 mSleepTime;
    MUINT64 mtRSTime;
    MBOOL   mSkipWaitGyro;
    MBOOL   mbLastCalibration;
    MUINT32 mSensorPixelClock;
    MUINT32 mSensorLinePixel;
    MBOOL   mbEMSaveFlag;
    double  mRecordParameter[6];
    NVRAM_CAMERA_FEATURE_GIS_STRUCT_t* m_pNVRAM_defParameter;

    LMVHal *m_pLMVHal;

public:
    static MINT32 mEISInterval;
    static MINT32 mEMEnabled;
    static MINT32 mDebugDump;
};

/**
  *@class EisHalObj
  *@brief singleton object for each EisHal which is seperated by sensor index
*/

template<const unsigned int aSensorIdx>
class EisHalObj : public EisHalImp
{
private:
    static EisHalObj<aSensorIdx>* spInstance;
    static Mutex    s_instMutex;

    EisHalObj(MUINT32 MultiSensor= 0) : EisHalImp(aSensorIdx, MultiSensor) {}
    ~EisHalObj() {}
public:
    static EisHal *GetInstance(MUINT32 MultiSensor = 0)
    {
        if (0 == spInstance)
        {
            AutoMutex lock(s_instMutex);
            if (0 == spInstance)
            {
                spInstance = new EisHalObj(MultiSensor);
                atexit(destroyInstance);
            }

        }
        return spInstance;
    }

    static void destroyInstance(void)
    {
        AutoMutex lock(s_instMutex);
        if (0 != spInstance)
        {
            delete spInstance;
            spInstance = 0;
        }
    }

    static void EisSensorListener(ASensorEvent event);
    static std::queue<EIS_GyroRecord> gGyroDataQueue;
    static Mutex gGyroQueueLock;
    static Condition gWaitGyroCond;
    static MUINT32 gGyroCount;
    static MUINT32 gGyroReverse;
    static MUINT64 gLastGyroTimestamp;

};


#endif

