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
 * @file BaseBufferPoolMgr.h
 * @brief base classes of BufferPoolMgr and BufferHanlder
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_BUFFERPOOL_HANDLER_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_BUFFERPOOL_HANDLER_H_

// Standard C header file

// Android system/core header file
#include <utils/RefBase.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>

// Module header file
#include <featurePipe/core/include/ImageBufferPool.h>
// Local header file
#include "../DepthMapPipe_Common.h"
#include "BaseBufferPoolMgr.h"


/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

/*******************************************************************************
* Enum Definition
********************************************************************************/

/*******************************************************************************
* Class Definition
********************************************************************************/
class DepthMapEffectRequest;
class EffectRequestAttrs;
/**
 * @class BaseBufferHandler
 * @brief Base class of BufferHanlder
 */
class BaseBufferHandler : public virtual android::VirtualLightRefBase
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    BaseBufferHandler(sp<BaseBufferPoolMgr> pPoopMgr)
                        : mpBufferPoolMgr(pPoopMgr)
                        {}
    virtual ~BaseBufferHandler() {}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BaseBufferPoolMgr Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief request buffer from the buffer handler
     * @param [in] srcNodeID caller's node id
     * @param [in] bufferID buffer id used to request buffer.
     * @return
     * - requested buffer pointer
     */
    virtual IImageBuffer* requestBuffer(
                                DepthMapPipeNodeID srcNodeID,
                                DepthMapBufferID bufferID) {return NULL;}

    /**
     * @brief request metadata from the buffer handler
     * @param [in] srcNodeID caller's node id
     * @param [in] bufferID buffer id used to request buffer.
     * @return
     * - requested metadata pointer
     */
    virtual IMetadata* requestMetadata(
                                DepthMapPipeNodeID srcNodeID,
                                DepthMapBufferID bufferID) {return NULL;}

     /**
     * @brief request tuning buffer from the buffer handler
     * @param [in] srcNodeID caller's node id
     * @param [in] bufferID buffer id used to request buffer.
     * @return
     * - requested tuning buffer pointer
     */
    virtual MVOID* requestTuningBuf(
                            DepthMapPipeNodeID srcNodeID,
                            DepthMapBufferID bufferID
                            ) {return NULL;}
    /**
     * @brief Callback when the correnspoinding node finishes its job.
     * @param [in] nodeID Node ID that finishes its job
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL onProcessDone(DepthMapPipeNodeID nodeID) {return NULL;}

    /**
     * @brief configure the effectRequest and generate its attributes
     * @param [in] pDepthReq EffectRequest pointer
     * @param [out] pOutAttr output effect attribute
     * @return
     * - effect attributes
     */
    virtual MBOOL configRequest(DepthMapEffectRequest* pDepthReq) {return MFALSE;}

    /**
     * @brief config the buffer is for the node's output and do not release it.
     * @param [in] srcNodeID caller's node id
     * @param [in] bufferID buffer id used to config
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL configOutBuffer(
                            DepthMapPipeNodeID srcNodeID,
                            DepthMapBufferID bufferID,
                            DepthMapPipeNodeID outNodeID
                            ) {return MFALSE;}

    /**
     * @brief Get the enqueued/active buffer of this operation
     * @param [in] srcNodeID caller's node id
     * @param [in] bufferID buffer id
     * @param [out] pImgBuf enqued buffer
     * @return
     * - MTRUE indicates success
     * - MFALSE indicates failure
     */
    virtual MBOOL getEnqueBuffer(
                            DepthMapPipeNodeID srcNodeID,
                            DepthMapBufferID bufferID,
                            IImageBuffer*& pImgBuf) {return MFALSE;}

    /**
     * @brief Get the SmartBuffer format of enqued buffer
     * @param [in] srcNodeID caller's node id
     * @param [in] bufferID buffer id
     * @param [out] psmImgBuf enqued smart image buffer
     * @return
     * - MTRUE indicates success
     * - MFALSE indicates failure
     */
    virtual MBOOL getEnquedSmartBuffer(
                            DepthMapPipeNodeID srcNodeID,
                            DepthMapBufferID bufferID,
                            SmartImageBuffer& pSmImgBuf) {return MFALSE;}
    virtual MBOOL getEnquedSmartBuffer(
                            DepthMapPipeNodeID srcNodeID,
                            DepthMapBufferID bufferID,
                            SmartGraphicBuffer& pSmGraBuf) {return MFALSE;}
    virtual MBOOL getEnquedSmartBuffer(
                            DepthMapPipeNodeID srcNodeID,
                            DepthMapBufferID bufferID,
                            SmartTuningBuffer& pSmTuningBuf) {return MFALSE;}

    /**
     * @brief
     */
    sp<BaseBufferPoolMgr> getBufferPoolMgr() {return mpBufferPoolMgr;}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BaseBufferPoolMgr Protected Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    sp<BaseBufferPoolMgr> mpBufferPoolMgr;

};

typedef sp<BaseBufferHandler> BufferPoolHandlerPtr;



}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif