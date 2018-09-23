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
 * @file DepthMapEffectRequest.h
 * @brief Effect Request inside the DepthMapPipe
 */

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_EFFECT_REQUEST_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_EFFECT_REQUEST_H_

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>
#include <mtkcam/feature/stereo/pipe/IDepthMapEffectRequest.h>

// Module header file

// Local header file
#include "DepthMapPipeTimer.h"
#include "DepthMapPipe_Common.h"
#include "./bufferPoolMgr/BaseBufferPoolMgr.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"


/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {
/*******************************************************************************
* Structure Define
********************************************************************************/

/**
 * @struct EffectRequestAttrs
 * @brief EffectRequest attributes
 */
struct EffectRequestAttrs
{
    MBOOL isEISOn;
    DepthMapPipeOpState opState;
    BufferPoolScenario bufferScenario;
};


/*******************************************************************************
* Class Definition
********************************************************************************/
class BaseBufferHandler;
class DepthMapFlowOption;
/**
 * @class DepthMapEffectRequest
 * @brief Effect request used in DepthMapNode/Pipe
 */

class DepthMapEffectRequest
        : public IDepthMapEffectRequest
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    typedef MVOID (*PFN_CALLBACK_T)(MVOID* tag, String8 status, sp<EffectRequest>& request);
    DepthMapEffectRequest(
                        MUINT32 reqNo = 0,
                        PFN_CALLBACK_T callback = NULL,
                        MVOID* tag = NULL);

    virtual ~DepthMapEffectRequest();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IDepthMapEffectRequest Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MBOOL pushRequestImageBuffer(
                const NodeBufferSetting& setting,
                sp<IImageBuffer>& pImgBuf
            );

    virtual MBOOL pushRequestMetadata(
                const NodeBufferSetting& setting,
                IMetadata* pMetaBuf
            ) ;

     virtual MBOOL getRequestImageBuffer(
                const NodeBufferSetting& setting,
                IImageBuffer*& rpImgBuf
            );

    virtual MBOOL getRequestMetadata(
                const NodeBufferSetting& setting,
                IMetadata*& rpMetaBuf
            );

    virtual MBOOL startTimer();

    virtual MBOOL stopTimer();

    virtual MUINT32 getElapsedTime();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapEffectRequest Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
     /**
     * @brief init the effect request
     * @param [in] pHandler current buffer handler address
     * @param [in] reqAttrs request attributes
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL init(
            sp<BaseBufferHandler> pHandler,
            const EffectRequestAttrs& reqAttrs);
    /**
     * @brief get BufferHandler
     * @return
     * - buffer handler pointer
     */
    sp<BaseBufferHandler> getBufferHandler() {return mpBufferHandler;}
    /**
     * @brief notify the node operation done
     * @param [in] nodeID DepthMapPipe node id
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL handleProcessDone(DepthMapPipeNodeID nodeID);

    /**
     * @brief check the output buffer inside the
     *        request is ready or not
     * @return
     * - MTRUE indicates ready
     * - MFALSE indicates not ready
     */
    MBOOL checkAllOutputReady();
    /**
     * @brief set the buffer ready for use
     * @param [in] bufferID buffer id
     * @return
     * - MTRUE indicates success
     * - MFALSE indicates the buffer is not Request buffer.
     */
    MBOOL setOutputBufferReady(const DepthMapBufferID& bufferID);

    const EffectRequestAttrs& getRequestAttr() {return mReqAttrs;}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapEffectRequest Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapEffectRequest Public Data Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    // timer to record the entire process
    DepthMapPipeTimer mTimer;
    // use for any temporay data
    MVOID* mpData = NULL;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapEffectRequest Private Data Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    // buffer handler
    sp<BaseBufferHandler> mpBufferHandler = NULL;
    EffectRequestAttrs mReqAttrs;
};

typedef sp<DepthMapEffectRequest> DepthMapRequestPtr;

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif