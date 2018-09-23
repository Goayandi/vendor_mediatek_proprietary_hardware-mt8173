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
 * @file bwdn_hal_imp.h
 * @brief This is the implementation of bayer+white de-noise algorithm wrapper
*/

#ifndef _MTK_BMDN_HAL_IMP_H_
#define _MTK_BMDN_HAL_IMP_H_

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file

// Module header file
#include <mtkcam/feature/stereo/hal/bmdn_hal.h>
#include <libbwdn/MTKBWDN.h>

// Local header file

using namespace NS_BMDN_HAL;

/*******************************************************************************
* Enum Define
********************************************************************************/




/*******************************************************************************
* Structure Define
********************************************************************************/




/*******************************************************************************
* Class Define
********************************************************************************/
/**
 * @class BMDN_HAL_IMP
 * @brief This is the implementation of bayer+white de-noise algorithm wrapper
 */
class BMDN_HAL_IMP : public BMDN_HAL
{

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
// Constructor
BMDN_HAL_IMP(char const* szCallerName );
// Copy constructor
// Create instance
// Destr instance
virtual ~BMDN_HAL_IMP();


protected:
// Destructor


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  bwdn_hal Public interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief To init Bayer+White de-noise algorithm
     * @return [true, false]
     */
    virtual bool BMDNHalInit();
    /**
     * @brief To configure Bayer+White de-noise algorithm
     * @param [in] BMDN_HAL_CONFIGURE_DATA
     * @return [true, false]
     */
    virtual bool BMDNHalConfigure(BMDN_HAL_CONFIGURE_DATA& configureData);
    /**
     * @brief To run Bayer+White de-noise algorithm
     * @param [in] BMDN_HAL_IN_DATA
     * @param [out] BMDN_HAL_OUT_DATA
     * @return [true, false]
     */
    virtual bool BMDNHalRun(BMDN_HAL_IN_DATA& inData);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  bwdn_hal_imp Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    /**
     * @brief To setup alg dirver
     * @return [true, false]
     */
    bool _halConfigure(BMDN_HAL_CONFIGURE_DATA& configureData);
    /**
     * @brief To check params before calling algorithm driver
     * @return [true, false]
     */
    bool _checkParams(BMDN_HAL_IN_DATA& inData);

    /**
     * @brief To run algorithm driver
     * @return [true, false]
     */
    bool _doAlg(BMDN_HAL_IN_DATA& inData);

    /**
     * @brief To clear buffer allocated for algorithm driver
     * @return [true, false]
     */
    bool _clearBWDNData();

    /**
     * @brief To setup affinity control for alorithm driver
     * @return [true, false]
     */
    bool _affinityControl();

    /**
     * @brief To clear affinity control for alorithm driver
     * @return [true, false]
     */
    bool _clearAffinityControl();

    /**
     * @brief To get caller name
     * @return caller name
     */
    char const*                         getUserName() {return mName;};
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
    const char*     mName;
    const bool      LOG_ENABLED;

    Mutex           mLock;

    MTKBWDN*        bwdn = nullptr;
    int             worksize = 0;
    char*           workbuf = nullptr;

    MSize           mCurrentSrcImgSize = MSize(-1,-1);
    MSize           mCurrentSrcImgPaddedSize = MSize(-1,-1);

    BWDN_INITPARAMS mCurrentInit_params;

    CPUAffinity*    mCPUAffinity = nullptr;
};
#endif