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

/**
 * @file IBMDeNoisePipe.h
 * @brief This is the interface of bayer+white de-noise feature pipe
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_IBMDENOISE_PIPE_H_
#define _MTK_CAMERA_FEATURE_PIPE_IBMDENOISE_PIPE_H_

// Standard C header file

// Android system/core header file
#include <utils/RefBase.h>

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>

// Module header file

// Local header file

#define BMDENOISE_EFFECT_PARAMS_KEY "Metadata"
#define BMDENOISE_COMPLETE_KEY "onComplete"
#define BMDENOISE_FLUSH_KEY "onFlush"
#define BMDENOISE_ERROR_KEY "onError"
#define BMDENOISE_REQUEST_TYPE_KEY "ReqType"
#define BMDENOISE_REQUEST_TIMEKEY "TimeStamp"
#define BMDENOISE_G_SENSOR_KEY "G_SENSOR"

using namespace android;

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
/*******************************************************************************
* Enum Define
********************************************************************************/
enum BMDeNoiseBufferID{
    BID_BMDENOISEPIPE_INVALID = 0,
    BID_REQUEST_STATE,

    BID_PRE_PROCESS_IN_FULLRAW_1,
    BID_PRE_PROCESS_IN_FULLRAW_2,

    BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_1,
    BID_PRE_PROCESS_INTERMEDIATE_OUT_MFBO_2,

    BID_PRE_PROCESS_OUT_W_1,
    BID_PRE_PROCESS_OUT_W_2,
    BID_PRE_PROCESS_OUT_MFBO_FINAL_1,
    BID_PRE_PROCESS_OUT_MFBO_FINAL_2,

    BID_DENOISE_IN_MFBO_FINAL_1,
    BID_DENOISE_IN_MFBO_FINAL_2,
    BID_DENOISE_IN_W_1,

    BID_DENOISE_IN_DISPARITY_MAP_1,
    BID_DENOISE_IN_DISPARITY_MAP_2,
    BID_DENOISE_IN_WARPING_MATRIX,

    BID_DENOISE_HAL_OUT,
    BID_DENOISE_HAL_OUT_ROT_BACK,
    BID_DENOISE_FINAL_RESULT,
    BID_DENOISE_FINAL_RESULT_THUMBNAIL,

    // metadata
    BID_META_IN_APP,
    BID_META_IN_HAL,
    BID_META_IN_HAL_MAIN2,
    BID_META_OUT_APP,
    BID_META_OUT_HAL
} ;

enum BMDeNoisePipeRequestType{
    TYPE_NONE = 0,
    TYPE_BM_DENOISE,
    TYPE_MNR,
    TYPE_SWNR,
    TYPE_PREPROCESS,     //deprecated
    TYPE_WARPING_MATRIX  //deprecated
};
/*******************************************************************************
* Structure Define
********************************************************************************/




/*******************************************************************************
* Class Define
********************************************************************************/
/**
 * @class IBMDeNoisePipe
 * @brief This is the interface of bayer+white de-noise feature pipe
 */
class IBMDeNoisePipe : public virtual android::RefBase
{

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
// Constructor
static sp<IBMDeNoisePipe> createInstance(MINT32 iSensorIdx_Main1, MINT32 iSensorIdx_Main2, MUINT32 iFeatureMask=0);
// Copy constructor
// Create instance
// Destr instance
MBOOL destroyInstance();
// destructor
virtual ~IBMDeNoisePipe() {};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief initalization
     * @param [in] szCallerame :  caller name
     * @return
     *-true indicates success, otherwise indicates fail
     */
    virtual MBOOL init() = 0;
    virtual MBOOL uninit() = 0;
    virtual MBOOL enque(android::sp<EffectRequest>& request) = 0;
    virtual MVOID flush() = 0;
    virtual MVOID setFlushOnStop(MBOOL flushOnStop) = 0;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Protected Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Private Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:




};

/*******************************************************************************
* Namespace end.
********************************************************************************/
};
};
};


#endif