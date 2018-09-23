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

#include "StreamingFeaturePipe.h"
#include "DebugControl.h"
#include <mtkcam/feature/eis/eis_ext.h>
#include <camera_custom_eis.h>

using NSCam::NSIoPipe::NSPostProc::QParams;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

IStreamingFeaturePipe* IStreamingFeaturePipe::createInstance(MUINT32 sensorIndex, const UsageHint &usageHint)
{
    return new StreamingFeaturePipe(sensorIndex, usageHint);
}

MBOOL IStreamingFeaturePipe::destroyInstance(const char *name)
{
    (void)name;
    delete this;
    return MTRUE;
}

IStreamingFeaturePipe::UsageHint::UsageHint()
    : mMode(USAGE_FULL)
    , mStreamingSize(0, 0)
    , mEisMode(0)
    , mVendorMode(0)
    , m3DNRMode(0)
{
}

IStreamingFeaturePipe::UsageHint::UsageHint(eUsageMode mode, const MSize &streamingSize)
    : mMode(mode)
    , mStreamingSize(streamingSize)
    , mEisMode(0)
    , mVendorMode(0)
    , m3DNRMode(0)
{
}

MBOOL IStreamingFeaturePipe::UsageHint::isTimeSharing() const
{
    return (mMode == IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH_TIME_SAHRING);
}

MBOOL IStreamingFeaturePipe::UsageHint::support4K2K() const
{
    return is4K2K(mStreamingSize);
}

MBOOL IStreamingFeaturePipe::UsageHint::supportP2AFeature() const
{
    return (mMode != IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH
            && mMode != IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH_TIME_SAHRING);
}

MBOOL IStreamingFeaturePipe::UsageHint::support3DNR() const
{
    MBOOL ret = MFALSE;
    // could use property to enalbe 3DNR
    // assign value at getPipeUsageHint
    if (this->is3DNRModeMaskEnable(E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) ||
            this->is3DNRModeMaskEnable(E3DNR_MODE_MASK_UI_SUPPORT))
    {
        ret = this->supportP2AFeature();
    }

#if 0
    // TODO: remove

    #ifdef SUPPORT_3DNR
    ret = this->supportP2AFeature();
    #endif
#endif
    return ret;
}

MBOOL IStreamingFeaturePipe::UsageHint::supportVFB() const
{
    MBOOL ret = MFALSE;
    #if SUPPORT_VFB
    ret = (mMode == IStreamingFeaturePipe::USAGE_FULL);
    #endif // SUPPORT_VFB
    return ret;
}

MBOOL IStreamingFeaturePipe::UsageHint::supportEIS_22() const
{
    return EIS_MODE_IS_EIS_22_ENABLED(mEisMode) &&
           (mMode == IStreamingFeaturePipe::USAGE_FULL);
}

MBOOL IStreamingFeaturePipe::UsageHint::supportEIS_25() const
{
    return EIS_MODE_IS_EIS_25_ENABLED(mEisMode) &&
           (mMode == IStreamingFeaturePipe::USAGE_FULL);
}

MBOOL IStreamingFeaturePipe::UsageHint::supportEIS_Q() const
{
    return supportEIS_25() && EISCustom::isEnabledEIS25_Queue();
}

MBOOL IStreamingFeaturePipe::UsageHint::supportVendor() const
{
    return mVendorMode &&
           (mMode != IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH &&
            mMode != IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH_TIME_SAHRING);
}

MBOOL IStreamingFeaturePipe::UsageHint::supportGPUNode() const
{
    return supportEIS_25() || supportEIS_22();
}

MBOOL IStreamingFeaturePipe::UsageHint::supportMDPNode() const
{
    return supportEIS_25() || supportEIS_22();
}

MUINT32 IStreamingFeaturePipe::UsageHint::getNumP2ABuffer() const
{
    MUINT32 num = 0;
    if( !supportGPUNode() )
    {
        num = max(num, get3DNRBufferNum().mBasic);
        num = max(num, getVendorBufferNum().mBasic);
    }
    return num;
}

MUINT32 IStreamingFeaturePipe::UsageHint::getNumGpuInBuffer() const
{
    MUINT32 num = 0;
    num = max(num, getEISBufferNum().mBasic);
    num += getVendorBufferNum().mBasic;
    return num;
}

MUINT32 IStreamingFeaturePipe::UsageHint::getNumExtraGpuInBuffer() const
{
    MUINT32 num = 0;
    num = max(num, getEISBufferNum().mExtra);
    num += getVendorBufferNum().mExtra;
    return num;
}

MUINT32 IStreamingFeaturePipe::UsageHint::getNumGpuOutBuffer() const
{
    return supportGPUNode() ? 3 : 0;
}

MUINT32 IStreamingFeaturePipe::UsageHint::getNumVendorInBuffer() const
{
    MUINT32 num = 0;
    num = max(num,  getVendorBufferNum().mBasic);
    return num;
}

MUINT32 IStreamingFeaturePipe::UsageHint::getNumVendorOutBuffer() const
{
    return supportGPUNode() ? 0 : 3;
}

IStreamingFeaturePipe::BufferNumInfo IStreamingFeaturePipe::UsageHint::get3DNRBufferNum() const
{
    return support3DNR() ? BufferNumInfo(3) : BufferNumInfo(0);
}

IStreamingFeaturePipe::BufferNumInfo IStreamingFeaturePipe::UsageHint::getEISBufferNum() const
{
    MUINT32 basic = 0, extra = 0;
    if( supportEIS_25() || supportEIS_22() )
    {
        basic = 5;
        if( supportEIS_Q() )
        {
            extra = EIS_QUEUE_SIZE;
        }
    }
    return BufferNumInfo(basic, extra);
}

IStreamingFeaturePipe::BufferNumInfo IStreamingFeaturePipe::UsageHint::getVendorBufferNum() const
{
    return supportVendor() ? BufferNumInfo(3) : BufferNumInfo(0);
}

}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam
