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


// Standard C header file

// Android system/core header file
#include <utils/RefBase.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/IMetadata.h>
// Module header file

// Local header file
#include "DepthMapEffectRequest.h"
#include "DepthMapPipe_Common.h"



namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

/*******************************************************************************
* Global Define
********************************************************************************/

/*******************************************************************************
* External Function
********************************************************************************/

/*******************************************************************************
* Enum Define
********************************************************************************/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

sp<IDepthMapEffectRequest>
IDepthMapEffectRequest::
createInstance(MUINT32 reqID, PFN_CALLBACK_T callback, MVOID* tag)
{
    return new DepthMapEffectRequest(reqID, callback, tag);
}

IDepthMapEffectRequest::
IDepthMapEffectRequest(MUINT32 reqID, PFN_CALLBACK_T callback, MVOID* tag)
: EffectRequest(reqID, callback, tag)
{

}

DepthMapEffectRequest::
DepthMapEffectRequest(MUINT32 reqID, PFN_CALLBACK_T callback, MVOID* tag)
: IDepthMapEffectRequest(reqID, callback, tag)
{

}

DepthMapEffectRequest::
~DepthMapEffectRequest()
{
    mpBufferHandler = NULL;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IDepthMapEffectRequest Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
DepthMapEffectRequest::
pushRequestImageBuffer(
    const NodeBufferSetting& setting,
    sp<IImageBuffer>& pImgBuf
)
{
    MY_LOGD(" bufferID=%d  iotype=%d", setting.bufferID, setting.ioType);
    sp<EffectFrameInfo> pEffectFrame = new EffectFrameInfo(getRequestNo(), setting.bufferID);
    pEffectFrame->setFrameBuffer(pImgBuf);

    if(setting.ioType == eBUFFER_IOTYPE_INPUT)
        this->vInputFrameInfo.add(setting.bufferID, pEffectFrame);
    else
        this->vOutputFrameInfo.add(setting.bufferID, pEffectFrame);

    return MTRUE;
}

MBOOL
DepthMapEffectRequest::
pushRequestMetadata(
    const NodeBufferSetting& setting,
    IMetadata* pMetaBuf
)
{
    sp<EffectFrameInfo> pEffectFrame = new EffectFrameInfo(getRequestNo(), setting.bufferID);
    sp<EffectParameter> pEffParam = new EffectParameter();

    pEffParam->setPtr(DEPTHMAP_META_KEY_STRING, (void*)pMetaBuf);
    pEffectFrame->setFrameParameter(pEffParam);

    if(setting.ioType == eBUFFER_IOTYPE_INPUT)
        this->vInputFrameInfo.add(setting.bufferID, pEffectFrame);
    else
        this->vOutputFrameInfo.add(setting.bufferID, pEffectFrame);

    return MTRUE;
}

MBOOL
DepthMapEffectRequest::
getRequestImageBuffer(
    const NodeBufferSetting& setting,
    IImageBuffer*& rpImgBuf
)
{

    auto vFrameInfo = (setting.ioType == eBUFFER_IOTYPE_INPUT) ? vInputFrameInfo : vOutputFrameInfo;
    if(vFrameInfo.indexOfKey(setting.bufferID)<0)
        return MFALSE;

    sp<EffectFrameInfo> pEffectFrame = vFrameInfo.valueFor(setting.bufferID);
    sp<IImageBuffer> pImgBuf;

    pEffectFrame->getFrameBuffer(pImgBuf);
    rpImgBuf = pImgBuf.get();
    return MTRUE;
}

MBOOL
DepthMapEffectRequest::
getRequestMetadata(
    const NodeBufferSetting& setting,
    IMetadata*& rpMetaBuf
)
{
    auto vFrameInfo = (setting.ioType == eBUFFER_IOTYPE_INPUT) ? vInputFrameInfo : vOutputFrameInfo;
    if(vFrameInfo.indexOfKey(setting.bufferID)<0)
        return MFALSE;

    sp<EffectFrameInfo> pEffectFrame = vFrameInfo.valueFor(setting.bufferID);
    sp<EffectParameter> pEffParam = pEffectFrame->getFrameParameter();
    rpMetaBuf = reinterpret_cast<IMetadata*>(pEffParam->getPtr(DEPTHMAP_META_KEY_STRING));
    return MTRUE;
}

MBOOL
DepthMapEffectRequest::
startTimer()
{
    this->mTimer.start();
    return MTRUE;
}

MBOOL
DepthMapEffectRequest::
stopTimer()
{
    this->mTimer.stop();
    return MTRUE;
}

MUINT32
DepthMapEffectRequest::
getElapsedTime()
{
    return this->mTimer.getElapsed();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapEffectRequest Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

MBOOL
DepthMapEffectRequest::
init(
    sp<BaseBufferHandler> pHandler,
    const EffectRequestAttrs& reqAttrs
)
{
    mpBufferHandler = pHandler;
    mReqAttrs = reqAttrs;
    // config request to the handler
    MBOOL bRet = pHandler->configRequest(this);
    return bRet;
}

MBOOL
DepthMapEffectRequest::
handleProcessDone(eDepthMapPipeNodeID nodeID)
{
    MBOOL bRet = mpBufferHandler->onProcessDone(nodeID);
    return bRet;
}

MBOOL
DepthMapEffectRequest::
checkAllOutputReady()
{
    size_t outBufSize = this->vOutputFrameInfo.size();
    // make sure all output frame are ready
    for(size_t index=0;index<outBufSize;++index)
    {
        if(!this->vOutputFrameInfo[index]->isFrameBufferReady())
        {
            VSDOF_LOGD("req_id=%d Data not ready!! buffer key=%d",
                this->getRequestNo(), this->vOutputFrameInfo.keyAt(index));
            return MFALSE;
        }
    }
    return MTRUE;
}

MBOOL
DepthMapEffectRequest::
setOutputBufferReady(const DepthMapBufferID& bufferID)
{
    ssize_t index = this->vOutputFrameInfo.indexOfKey(bufferID);
    if(index >= 0)
    {
        sp<EffectFrameInfo> pFrame = this->vOutputFrameInfo.valueAt(index);
        pFrame->setFrameReady(true);
        return MTRUE;
    }
    return MFALSE;
}

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam