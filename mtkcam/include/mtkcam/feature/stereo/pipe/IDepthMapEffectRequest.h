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
 * @file IDepthMapEffectRequest.h
 * @brief Effect Request Interface inside the DepthMapPipe
 */

#ifndef _MTK_CAMERA_FEATURE_INTERFACE_DEPTH_MAP_EFFECT_REQUEST_H_
#define _MTK_CAMERA_FEATURE_INTERFACE_DEPTH_MAP_EFFECT_REQUEST_H_

// Standard C header file

// Android system/core header file
#include <utils/RefBase.h>
#include <utils/String8.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/IMetadata.h>
// Module header file

// Local header file
#include "IBMDepthMapPipe.h"

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using namespace NSCam;
/*******************************************************************************
* Enum Definition
********************************************************************************/
/**
 * @brief buffer IN/OUT type
 */
typedef enum eDepthNodeBufIOType {
    eBUFFER_IOTYPE_INPUT,
    eBUFFER_IOTYPE_OUTPUT
} DepthNodeBufIOType;

/*******************************************************************************
* Structure Definition
********************************************************************************/
struct NodeBufferSetting
{
public:
    DepthMapBufferID bufferID;
    DepthNodeBufIOType ioType;
};

/*******************************************************************************
* Class Definition
********************************************************************************/
/**
 * @class IDepthMapEffectRequest
 * @brief Interface of the effect request inside the DepthMapPipe
 */
class IDepthMapEffectRequest : public EffectRequest
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    //typedef MVOID (*DPE_EFFECR_REQ_CALLBACK)(MVOID* tag, String8 status, sp<IDepthMapEffectRequest> request);
    typedef MVOID (*PFN_CALLBACK_T)(MVOID* tag, String8 status, sp<EffectRequest>& request);
    /**
     * @brief create a new DepthMapEffectRequest
     * @param [in] _reqNo request no.
     * @param [in] _cb callback when job finishes
     * @param [in] _tag cookie pointer
     * @return
     * - new DepthMapEffectRequest pointer
     */
    static sp<IDepthMapEffectRequest> createInstance(
                                                MUINT32 _reqNo = 0,
                                                PFN_CALLBACK_T _cb = NULL,
                                                MVOID* _tag = NULL
                                            );

protected:
    IDepthMapEffectRequest(
                    MUINT32 _reqNo = 0,
                    PFN_CALLBACK_T _cb = NULL,
                    MVOID* _tag = NULL);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IDepthMapEffectRequest Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief push input/output image stream buffer into the EffectRequest
     * @param [in] reqestID request ID
     * @param [in] setting buffer setting
     * @param [in] pImgBuf image buffer pointer
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL pushRequestImageBuffer(
                const NodeBufferSetting& setting,
                sp<IImageBuffer>& pImgBuf
            ) = 0;

    /**
     * @brief push input/output metadata into the EffectRequest
     * @param [in] reqestID request ID
     * @param [in] setting buffer setting
     * @param [in] pMetaBuf Metadata pointer
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL pushRequestMetadata(
                const NodeBufferSetting& setting,
                IMetadata* pMetaBuf
            ) = 0;

     /**
     * @brief get input/output image buffer from the EffectRequest
     * @param [in] ioType input or output
     * @param [in] bufferID image buffer ID
     * @param [out] rpImgBuf image buffer pointer
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL getRequestImageBuffer(
                const NodeBufferSetting& setting,
                IImageBuffer*& rpImgBuf
            ) = 0;

    /**
     * @brief get input/output metadata from the EffectRequest
     * @param [in] ioType input or output
     * @param [in] bufferID meta buffer ID
     * @param [out] rpMetaBuf Metadata pointer
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL getRequestMetadata(
                const NodeBufferSetting& setting,
                IMetadata*& rpMetaBuf
            ) = 0;
    /**
     * @brief start the request timer
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL startTimer() = 0;

    /**
     * @brief stop the request timer
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL stopTimer() = 0;

    /**
     * @brief get elapsed time from timer started
     * @return
     * - elapsed time in milliseconds
     */
    virtual MUINT32 getElapsedTime() = 0;

};


}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam

#endif
