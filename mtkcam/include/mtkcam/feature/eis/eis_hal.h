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
TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/

/**
* @file eis_hal.h
*
* EIS Hal Header File
*
*/

#ifndef _EIS_HAL_H_
#define _EIS_HAL_H_

#include <mtkcam/feature/eis/eis_type.h>
#include <mtkcam/def/UITypes.h>


/**
  *@brief EIS hal class used by scenario
*/
class EisHal
{
public:


    /**
         *@brief Create EisHal object
         *@param[in] userName : user name,i.e. who create EIS HAL object
         *@param[in] aSensorIdx : sensor index
         *@return
         *-EisHal object
       */
    static EisHal *CreateInstance(char const *userName, const MUINT32 &aSensorIdx, MUINT32 MultiSensor = 0);

    /**
         *@brief Destroy EisHal object
         *@param[in] userName : user name,i.e. who destroy EIS HAL object
       */
    virtual MVOID DestroyInstance(char const *userName) = 0;

    /**
         *@brief Initialization function
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 Init() = 0;

    /**
         *@brief Unitialization function
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 Uninit() = 0;

    /**
         *@brief Configure EIS
         *@details Use this API after pass1/pass2 config and before pass1/pass2 start
         *@param[in] aEisPass : indicate pass1 or pass2
         *@param[in] aEisConfig : EIS config data
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 ConfigFFFMEis(const EIS_HAL_CONFIG_DATA &aEisConfig, const MUINT32 eisMode = 0, const MULTISCALE_INFO* apMultiScaleInfo = NULL) = 0;
    virtual MINT32 ConfigGis(const EIS_HAL_CONFIG_DATA &aEisConfig) = 0;

    virtual MINT32 ConfigCalibration(const LMV_HAL_CONFIG_DATA &aLMVConfig) = 0;

    /**
         *@brief Execute EIS
         *@param[in] aEisPass : indicate pass1 or pass2
         *@param[in] apEisConfig : EIS config data, mainly for pass2
         *@param[in] aTimeStamp : time stamp of pass1 image
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 DoGis(EIS_HAL_CONFIG_DATA *apEisConfig = NULL, MINT64 aTimeStamp = -1, MINT64 aExpTime = 0) = 0;

    /**
         *@brief Execute EIS
         *@param[in] aEisPass : pass2 only!!!
         *@param[in] apEisConfig : EIS config data, mainly for pass2
         *@param[in] aTimeStamp : time stamp of pass1 image
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 DoFEFMEis(EIS_HAL_CONFIG_DATA *apEisConfig = NULL, FEFM_PACKAGE *fefmData = NULL, MINT64 aTimeStamp = -1, MINT64 aExpTime = 0) = 0;

    virtual MINT32 AbortP2Calibration() = 0;
    virtual MINT32 ForcedDoEisPass2() = 0;

    /**
         *@brief DoCalibration
         *@param[in] aEisPass : indicate pass1 or pass2
         *@param[in] apEisConfig : EIS config data, mainly for pass2
         *@param[in] aTimeStamp : time stamp of pass1 image
         *@return
         *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
       */
    virtual MINT32 DoCalibration(LMV_HAL_CONFIG_DATA *apLMVConfig = NULL, MINT64 aTimeStamp = -1, MINT64 aExpTime = 0) = 0;

    /**
         *@brief Get EIS Plus GPU infomation
         *@param[in] aGridX : pointer to array
         *@param[in] aGridY : pointer to array
       */
    virtual MVOID SetEisPlusGpuInfo(MINT32 * const aGridX, MINT32 * const aGridY) = 0;

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
                                      MUINT32 eis_fator) = 0;


    /**
         *@brief Get EIS algorithm result (CMV)
         *@param[out] P_EIS_PLUS_RESULT_INFO_STRUCT : EISPlus result
    */
    virtual MVOID GetEisPlusResult(P_EIS_PLUS_RESULT_INFO_STRUCT apEisPlusResult) = 0;

    /**
         *@brief Get EIS plus crop ratio
         *@return
         *-EIS plus crop ratio
    */
    virtual MUINT32 GetEisPlusCropRatio() = 0;

    /**
         *@brief Get EIS Plus GPU infomation
         *@param[in,out] aEisPlusGpu : EIS_PLUS_GPU_INFO
       */
    virtual MVOID GetEisPlusGpuInfo(EIS_PLUS_GPU_INFO *aEisPlusGpu, MUINT32 eisMode = 0) = 0;


protected:

    /**
         *@brief EisHal constructor
       */
    EisHal() {};

    /**
         *@brief EisHal destructor
       */
    virtual ~EisHal() {};
};

#endif //_EIS_HAL_H_

