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

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include "FaceBeautyEffectHal.h"
#include <mtkcam/sdk/hal/IFaceBeautySDKHal.h>
#include "Facebeauty.h"

/******************************************************************************
 *
 ******************************************************************************/
#define FUNCTION_LOG_START      ALOGD("[%s] - E.", __FUNCTION__)
#define FUNCTION_LOG_END        ALOGD("[%s] - X.", __FUNCTION__)


using namespace NSCam;
using namespace android;

bool
FaceBeautyEffectHal::
allParameterConfigured() 
{
    FUNCTION_LOG_START;
    //@todo implement this    
    FUNCTION_LOG_END;
    return true;
}


status_t
FaceBeautyEffectHal::
initImpl() 
{
    FUNCTION_LOG_START;
    //@todo implement this
    //mpListener = NULL;
    FUNCTION_LOG_END;
    return OK;
}


status_t
FaceBeautyEffectHal::
uninitImpl() 
{
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}


//non-blocking
status_t           
FaceBeautyEffectHal::
prepareImpl() 
{
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}


status_t
FaceBeautyEffectHal::
releaseImpl() 
{
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}

status_t
FaceBeautyEffectHal::
getNameVersionImpl(EffectHalVersion &nameVersion) const
{
    FUNCTION_LOG_START;
    //@todo implement this
    nameVersion.effectName = "FaceBeauty";
    nameVersion.major = 1;
    nameVersion.minor = 0;
    FUNCTION_LOG_END;
    return OK;
}


status_t           
FaceBeautyEffectHal::
getCaptureRequirementImpl(EffectParameter *inputParam, Vector<EffectCaptureRequirement> &requirements) const 
{   
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}


status_t           
FaceBeautyEffectHal::
setParameterImpl(android::String8 &key, android::String8 &object) 
{
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}

status_t
FaceBeautyEffectHal::
setParametersImpl(android::sp<EffectParameter> parameter)
{
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}


status_t           
FaceBeautyEffectHal::
startImpl(uint64_t *uid) 
{
    FUNCTION_LOG_START;
    //@todo implement this    
    EffectResult result;
    EffectParameter parameter;
    result.set("onCompleted", 1);
    startDone(result, parameter, OK);   //@todo call this after completed
    FUNCTION_LOG_END;
    return OK;
}


status_t           
FaceBeautyEffectHal::
abortImpl(EffectResult &result, EffectParameter const *parameter) 
{
    FUNCTION_LOG_START;
    //@todo implement this
    result.set("onAborted", 1);
    FUNCTION_LOG_END;
    return OK;
}


//non-blocking
status_t           
FaceBeautyEffectHal::
addInputFrameImpl(const sp<IImageBuffer> frame, const sp<EffectParameter>)
{
    FUNCTION_LOG_START;
    //@todo implement this
    //@todo use a map to maintain EffectParameter/IImageBuffer
    //@todo check the live cycle of parameter
    if (frame == NULL || 
            ((frame->getImgFormat() != eImgFmt_I422)/*TODO*/ &&
             (frame->getImgFormat() != eImgFmt_YV12)))
        return BAD_VALUE;
    mpInputFrame = frame.get();
    FUNCTION_LOG_END;
    return OK;
}


//non-blocking
status_t           
FaceBeautyEffectHal::
addOutputFrameImpl(const sp<IImageBuffer> frame, const sp<EffectParameter> parameter)
{
    FUNCTION_LOG_START;
    if (frame == NULL || 
            ((frame->getImgFormat() != eImgFmt_I422)/*TODO*/ &&
             (frame->getImgFormat() != eImgFmt_YV12))             ||
            mpInputFrame->getImgSize().w != frame->getImgSize().w ||
            mpInputFrame->getImgSize().h != frame->getImgSize().h ||
            parameter == NULL)
    {
        ALOGE("[%s] Bad value", __FUNCTION__);
        return BAD_VALUE;
    }
    FACE_BEAUTY_SDK_HAL_PARAMS sdkParams;
    sdkParams.SmoothLevel     = parameter->getInt("fb-smooth-level");
    sdkParams.SkinColor       = parameter->getInt("fb-skin-color");
    sdkParams.EnlargeEyeLevel = parameter->getInt("fb-enlarge-eye");
	sdkParams.SlimFaceLevel   = parameter->getInt("fb-slim-face");
    if (!CaptureFaceBeautySDK_apply(mpInputFrame, frame.get(), &sdkParams, (EImageFormat) frame->getImgFormat()))
        return UNKNOWN_ERROR;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}
