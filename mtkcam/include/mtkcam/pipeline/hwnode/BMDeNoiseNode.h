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
 * @file BMDeNoiseNode.h
 * @brief This is the HWNode for Bayer+White DeNoise
*/

#ifndef _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_V3_HWNODE_BMDENOISENODE_
#define _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_V3_HWNODE_BMDENOISENODE_

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>

// Module header file

// Local header file


/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace v3 {
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
 * @class BMDeNoiseNode
 * @brief This is the HWNode for Bayer+White DeNoise
 */
class BMDeNoiseNode: virtual public IPipelineNode
{
public:
typedef IPipelineNode::InitParams       InitParams;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
// Constructor
// Copy constructor
// Create instance
static android::sp<BMDeNoiseNode>   createInstance();
// Destr instance


protected:
// Destructor


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief init
     * @param [in] rParams: ConfigParams
     * @return
     * -true indicates success, otherwise indicates fail
     */
    struct  ConfigParams
    {
        /**
         * A pointer to a set of input app meta stream info.
         */
        android::sp<IMetaStreamInfo> pInAppMeta;

        /**
         * A pointer to a set of output app meta stream info.
         */
        android::sp<IMetaStreamInfo> pOutAppMeta;

        /**
         * A pointer to a set of input hal meta stream info.
         */
        android::sp<IMetaStreamInfo> pInHalMeta;

        /**
         * A pointer to a set of output hal meta stream info.
         */
        android::sp<IMetaStreamInfo> pOutHalMeta;

        /**
         * A pointer to a set of input ImageStream Info for main1's fullraw,
         * only used when ISO smaller than alg threshold
         */
        android::sp<IImageStreamInfo> pInFullRaw_1;

        /**
         * A pointer to a set of input ImageStream Info for main1's mfbo applied module rotation.
         */
        android::sp<IImageStreamInfo> pInMFBO_final_1;

        /**
         * A pointer to a set of input ImageStream Info for main2's mfbo applied module rotation.
         */
        android::sp<IImageStreamInfo> pInMFBO_final_2;

        /**
         * A pointer to a set of input ImageStream Info for main1's w applied module rotation.
         */
        android::sp<IImageStreamInfo> pInW_1;

        /**
         * A pointer to a set of input ImageStream Info for warping matirx from N3D alg.
         */
        android::sp<IImageStreamInfo> pInWarpingMatrix;

        /**
         * A pointer to a set of input ImageStream Info for disparity map from depthMap Node.
         */
        android::sp<IImageStreamInfo> pInDisparityMap;

        /**
         * A pointer to a set of output ImageStream Info.
         */
        android::sp<IImageStreamInfo> vOutImage;

        /**
         * A pointer to a set of output thumbnail ImageStream Info.
         */
        android::sp<IImageStreamInfo> vOutImageThumbnail;
    };
    virtual MERROR                  config(ConfigParams const& rParams)     = 0;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  MISC Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief XXX
     * @param [in] XXX
     * @param [out] XXX
     * @return XXX
     */


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  XXX Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  XXX Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief init
     * @param [in] rParams: InitParams
     * @return
     * -OK indicates success, otherwise indicates fail
     */
    virtual MERROR                  init(InitParams const& rParams)         = 0;

    /**
     * @brief uninit
     * @return
     * -OK indicates success, otherwise indicates fail
     */
    virtual MERROR                  uninit()         = 0;

    /**
     * @brief flush
     * @return
     * -OK indicates success, otherwise indicates fail
     */
    virtual MERROR                  flush()         = 0;

    /**
     * @brief queue
     * @return
     * -OK indicates success, otherwise indicates fail
     */
    virtual MERROR                  queue(android::sp<IPipelineFrame> pFrame)         = 0;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  YYY Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  MISC Public Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:




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


#endif
