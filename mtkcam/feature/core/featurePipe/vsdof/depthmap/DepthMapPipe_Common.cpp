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

#include "DepthMapPipeUtils.h"
#include "DepthMapPipe_Common.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

ImageBufInfoMap::
ImageBufInfoMap(sp<EffectRequest> ptr)
:mvImgBufData(), mpReqPtr(ptr)
{

}

ssize_t
ImageBufInfoMap::
addImageBuffer(const DepthMapBufferID& key, const SmartImageBuffer& value)
{
    return mvImgBufData.add(key, value);
}

ssize_t
ImageBufInfoMap::
addGraphicBuffer(const DepthMapBufferID& key, const SmartGraphicBuffer& value)
{
    return mvGraImgBufData.add(key, value);
}

ssize_t
ImageBufInfoMap::
delImageBuffer(const DepthMapBufferID& key)
{
    return mvImgBufData.removeItem(key);
}

ssize_t
ImageBufInfoMap::
delGraphicBuffer(const DepthMapBufferID& key)
{
    return mvGraImgBufData.removeItem(key);
}

const SmartImageBuffer&
ImageBufInfoMap::
getImageBuffer(const DepthMapBufferID& key)
{
    return mvImgBufData.valueFor(key);
}

const SmartGraphicBuffer&
ImageBufInfoMap::
getGraphicBuffer(const DepthMapBufferID& key)
{
    return mvGraImgBufData.valueFor(key);
}

DepthNodeOpState getRequestState(EffectRequestPtr &request)
{
    sp<EffectParameter> pReqParam = request->getRequestParameter();
    DepthNodeOpState eState = (DepthNodeOpState) pReqParam->getInt(DEPTHMAP_REQUEST_STATE_KEY);
    return eState;
}

SimpleTimer::
SimpleTimer()
{}

SimpleTimer::
SimpleTimer(bool bStartTimer)
{
    if(bStartTimer)
        start = std::chrono::system_clock::now();
}

MBOOL
SimpleTimer::
startTimer()
{
    start = std::chrono::system_clock::now();
    return MTRUE;
}

float
SimpleTimer::
countTimer()
{
    std::chrono::time_point<std::chrono::system_clock> cur = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = cur-start;
    return elapsed_seconds.count()*1000;
}

} //NSFeaturePipe
} //NSCamFeature
} //NSCam