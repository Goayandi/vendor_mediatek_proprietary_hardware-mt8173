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

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>
// Module header file


// Local header file
#include "BMDeNoisePipe_Common.h"
#include "BMDeNoisePipe.h"


/*******************************************************************************
* Global Define
********************************************************************************/
#define PIPE_CLASS_TAG "BMDeNoisePipe.h"

/*******************************************************************************
* External Function
********************************************************************************/


namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
/*******************************************************************************
* Enum Define
********************************************************************************/




/*******************************************************************************
* Structure Define
********************************************************************************/






//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
sp<IBMDeNoisePipe>
IBMDeNoisePipe::createInstance(MINT32 iSensorIdx_Main1, MINT32 iSensorIdx_Main2, MUINT32 iFeatureMask)
{
    return new BMDeNoisePipe(iSensorIdx_Main1);
}

BMDeNoisePipe::BMDeNoisePipe(MINT32 openSensorIndex)
  : CamPipe<BMDeNoisePipeNode>("BMDeNoisePipe")
  , miOpenId(openSensorIndex)
  , mDeNoiseNode("DeNoise", &mCamGraph, openSensorIndex)
{
    MY_LOGD("OpenId(%d)", miOpenId);

    mbDebugLog = StereoSettingProvider::isLogEnabled(PERPERTY_BMDENOISE_NODE_LOG);
}

BMDeNoisePipe::~BMDeNoisePipe()
{
    // must call dispose to free CamGraph
    this->dispose();
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
BMDeNoisePipe::
init()
{
    CAM_TRACE_NAME("BMDeNoisePipe::init");
    return PARENT::init();
}

MBOOL
BMDeNoisePipe::
uninit()
{
    CAM_TRACE_NAME("BMDeNoisePipe::uninit");
    return PARENT::uninit();
}

MVOID
BMDeNoisePipe::
flush()
{
    CAM_TRACE_NAME("BMDeNoisePipe::flush");
    // lauch the default flush operations
    CamPipe::flush();

    // relase all the effectRequest
    android::Mutex::Autolock lock(mReqMapLock);
    for(size_t index=0;index<mvRequestPtrMap.size();++index)
    {
        MUINT32 iFlushReqID = mvRequestPtrMap.keyAt(index);
        VSDOF_LOGD("flush ReqID = %d", iFlushReqID);

        EffectRequestPtr pEffectReq = mvRequestPtrMap.valueAt(index);
        // callback to pipeline node with FLUSH KEY
        pEffectReq->mpOnRequestProcessed(pEffectReq->mpTag, String8( BMDENOISE_FLUSH_KEY ), pEffectReq);
    }
    // clear all request map
    mvRequestPtrMap.clear();
}

MBOOL
BMDeNoisePipe::
enque(EffectRequestPtr& request)
{
    CAM_TRACE_NAME("BMDeNoisePipe::enque");
    MUINT32 reqID = request->getRequestNo();
    // autolock for request map
    {
        VSDOF_PRFLOG("request map add reqId=%d ", reqID);
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.add(reqID, request);
    }
    if(mvRequestPtrMap.size()>6)
        MY_LOGW("The size of queued request inside BMDeNoisePipe is over 6.");
    return CamPipe::enque(ROOT_ENQUE, request);
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
BMDeNoisePipe::
onInit()
{
    // node connection
    this->connectData(DENOISE_RESULT_OUT,             DENOISE_RESULT_OUT,               mDeNoiseNode,    this);

    // set root node
    this->setRootNode(&mDeNoiseNode);

    return MTRUE;
}

MVOID
BMDeNoisePipe::
onUninit()
{
}

MBOOL
BMDeNoisePipe::
onData(DataID id, EffectRequestPtr &data)
{
    android::String8 status;
    status = BMDENOISE_COMPLETE_KEY;
    MUINT32 reqID = data->getRequestNo();

    MY_LOGD("recieve DENOISE_RESULT_OUT done reqId:%d DataID:%d mvRequestPtrMap.size:%d +", reqID, id, mvRequestPtrMap.size());

    if(data->mpOnRequestProcessed)
    {
        data->mpOnRequestProcessed(data->mpTag, status, data);
    }
    // remove request
    {
        android::Mutex::Autolock lock(mReqMapLock);
        mvRequestPtrMap.removeItem(reqID);

        MY_LOGD("recieve DENOISE_RESULT_OUT done reqId:%d DataID:%d mvRequestPtrMap.size:%d -", reqID, id, mvRequestPtrMap.size());
    }
    return MTRUE;
}

MBOOL
BMDeNoisePipe::
onData(DataID id, FrameInfoPtr &data)
{
    MY_LOGE("Not implemented.");
    return MTRUE;
}

MBOOL
BMDeNoisePipe::
onData(DataID id, SmartImageBuffer &data)
{
    MY_LOGE("Not implemented.");
    return MTRUE;
}
}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam