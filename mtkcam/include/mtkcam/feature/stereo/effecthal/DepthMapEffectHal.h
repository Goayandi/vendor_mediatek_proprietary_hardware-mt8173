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
#ifndef _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_DEPTHMAP_EFFECT_HAL_H_
#define _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_DEPTHMAP_EFFECT_HAL_H_

#include <utils/Mutex.h>
#include <mtkcam/feature/effectHalBase/EffectHalBase.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/feature/stereo/pipe/IDepthMapPipe.h>

#define SENSOR_IDX_MAIN1 "sensorIdx_main1"
#define SENSOR_IDX_MAIN2 "sensorIdx_main2"

namespace NSCam {

using namespace NSCam::NSCamFeature::NSFeaturePipe;

class DepthMapEffectHal : public EffectHalBase
{
public:
    DepthMapEffectHal();
    virtual ~DepthMapEffectHal();

    // EffectHalBase
    virtual android::status_t   initImpl();
    virtual android::status_t   uninitImpl();
    virtual android::status_t   prepareImpl();
    virtual android::status_t   releaseImpl();
    virtual android::status_t   getNameVersionImpl(EffectHalVersion &nameVersion) const;
    virtual android::status_t   setParameterImpl(android::String8 &key, android::String8 &object);
    virtual android::status_t   setParametersImpl(android::sp<EffectParameter> parameter);
    virtual android::status_t   startImpl(uint64_t *uid=NULL);
    virtual android::status_t   abortImpl(EffectResult &result, EffectParameter const *parameter=NULL);
    virtual android::status_t   updateEffectRequestImpl(android::sp<EffectRequest> request);

    // no-use
    virtual bool allParameterConfigured();
    virtual android::status_t   getCaptureRequirementImpl(EffectParameter *inputParam, Vector<EffectCaptureRequirement> &requirements) const;

    MBOOL flush();
private:
    MINT32 miSensorIdx_Main1;
    MINT32 miSensorIdx_Main2;
    DepthFlowType mFlowType;
    IDepthMapPipe *mpDepthMapPipe;
    MBOOL mbReadyToPush;
    Mutex mOpLock;
    MINT32 miLogLevel;
};




};





 #endif