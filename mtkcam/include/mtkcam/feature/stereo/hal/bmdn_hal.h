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
 * @file bwdn_hal.h
 * @brief This is the bayer+white de-noise algorithm wrapper
*/

#ifndef _MTK_BMDN_HAL_H_
#define _MTK_BMDN_HAL_H_

// Standard C header file

// Android system/core header file
#include <utils/RefBase.h>

// mtkcam custom header file
#include "stereo_common.h"

// mtkcam global header file

// Module header file

// Local header file

using namespace StereoHAL;

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NS_BMDN_HAL
{



/*******************************************************************************
* Enum Define
********************************************************************************/




/*******************************************************************************
* Structure Define
********************************************************************************/
struct BMDN_HAL_CONFIGURE_DATA
{
    int initWidth = -1;
    int initHeight = -1;
    int initPaddedWidth = -1;
    int initPaddedHeight = -1;
};

struct BMDN_HAL_IN_DATA
{
    int isRotate = -1;
    int bayerOrder = -1;
    int RA = -1;

    //ISP Info
    int OBOffsetBayer[4] = {-1, -1, -1, -1};
    int OBOffsetMono[4] = {-1, -1, -1, -1};
    int sensorGainBayer = -1;
    int sensorGainMono = -1;
    int ispGainBayer = -1;
    int ispGainMono= -1;
    int preGainBayer[3] = {-1, -1, -1};
    int preGainMono[3] = {-1 , -1, -1};

    //ratio crop offset
    int OffsetX = -1;
    int OffsetY = -1;

    //ISO dependent
    int BW_SingleRange = -1;
    int BW_OccRange = -1;
    int BW_Range = -1;
    int BW_Kernel = -1;
    int B_Range = -1;
    int B_Kernel = -1;
    int W_Range = -1;
    int W_Kernel = -1;
    int VSL = -1;
    int VOFT = -1;
    int VGAIN = -1;

    //information from N3D, TODO - add LDC info
    float Trans[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int dPadding[2] = {-1 ,-1};

    //unpadded width, height
    int width = -1;
    int height = -1;

    //padded width, height
    int paddedWidth = -1;
    int paddedHeight = -1;

    // Input Data
    // (16 + Width + 16) x (16 + Height + 16) 12-bit Mono
    unsigned short* MonoProcessedRaw = nullptr;
    // (16 + Width + 16) x (16 + Height + 16) 12-bit RGB-Bayer
    unsigned short* BayerProcessedRaw = nullptr;
    // (16 + Width + 16) x (16 + Height + 16) 12-bit BayerW
    unsigned short* BayerW = nullptr;

    short* depthL = nullptr;
    short* depthR = nullptr;

    int dwidth = -1;
    int dheight = -1;
    int dPitch = -1;
    int dsH = -1;
    int dsV = -1;

    int FPREPROC = -1;
    int FSSTEP = -1;

    // Full Shading Gain
    float* BayerGain = nullptr;
    float* MonoGain = nullptr;

    // RGB denoise output after BMDN
    // Width x Height x 3 RGB 12-bit image
    unsigned short* output = nullptr;
};

/*******************************************************************************
* Class Define
********************************************************************************/
/**
 * @class BMDN_HAL
 * @brief This is the bayer+white de-noise algorithm wrapper
 */
class BMDN_HAL
{

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    // Constructor
    // Copy constructor
    // Create instance
    static BMDN_HAL* getInstance();
    // Destr instance
    static bool destroyInstance();

    protected:
    // Destructor
    virtual ~BMDN_HAL(){};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief To init Bayer+White de-noise algorithm
     * @return [true, false]
     */
    virtual bool BMDNHalInit() = 0;
    /**
     * @brief To configure Bayer+White de-noise algorithm
     * @param [in] BMDN_HAL_CONFIGURE_DATA
     * @return [true, false]
     */
    virtual bool BMDNHalConfigure(BMDN_HAL_CONFIGURE_DATA& configureData) = 0;
    /**
     * @brief To run Bayer+White de-noise algorithm
     * @param [in] BMDN_HAL_IN_DATA
     * @param [out] BMDN_HAL_OUT_DATA
     * @return [true, false]
     */
    virtual bool BMDNHalRun(BMDN_HAL_IN_DATA& inData) = 0;
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
    static BMDN_HAL* instance;
    static Mutex mLock;
};

/*******************************************************************************
* Namespace end.
********************************************************************************/

};


#endif