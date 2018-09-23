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

#ifndef _MTK_CAMERA_FEATURE_PIPE_I_STREAMING_FEATURE_PIPE_H_
#define _MTK_CAMERA_FEATURE_PIPE_I_STREAMING_FEATURE_PIPE_H_

//#include <effectHal/EffectRequest.h>
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>

#include <utils/RefBase.h>

#include <mtkcam/feature/featurePipe/util/VarMap.h>

#define VAR_EXTREME_VFB "vfb.ext"

#ifdef FEATURE_MASK
#error FEATURE_MASK macro redefine
#endif

#define FEATURE_MASK(name) (1 << OFFSET_##name)

#define MAKE_FEATURE_MASK_FUNC(name)                  \
    const MUINT32 MASK_##name = (1 << OFFSET_##name); \
    inline MBOOL HAS_##name(MUINT32 feature)          \
    {                                                 \
        return (feature & FEATURE_MASK(name));        \
    }                                                 \
    inline MVOID ENABLE_##name(MUINT32 &feature)      \
    {                                                 \
        feature |= FEATURE_MASK(name);                \
    }                                                 \
    inline MVOID DISABLE_##name(MUINT32 &feature)     \
    {                                                 \
        feature &= ~FEATURE_MASK(name);               \
    }

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum FEATURE_MASK_OFFSET
{
    OFFSET_EIS,
    OFFSET_EIS_25,
    OFFSET_VFB,
    OFFSET_VFB_EX,
    OFFSET_VHDR,
    OFFSET_3DNR,
    OFFSET_EIS_QUEUE,
    OFFSET_VENDOR,
};

MAKE_FEATURE_MASK_FUNC(EIS);
MAKE_FEATURE_MASK_FUNC(EIS_25);
MAKE_FEATURE_MASK_FUNC(EIS_QUEUE);
MAKE_FEATURE_MASK_FUNC(VFB);
MAKE_FEATURE_MASK_FUNC(VFB_EX);
MAKE_FEATURE_MASK_FUNC(VHDR);
MAKE_FEATURE_MASK_FUNC(3DNR);
MAKE_FEATURE_MASK_FUNC(VENDOR);

class FeaturePipeParam
{
public:
    enum MSG_TYPE { MSG_FRAME_DONE, MSG_DISPLAY_DONE, MSG_FD_DONE, MSG_P2B_SET_3A };
    typedef MBOOL (*CALLBACK_T)(MSG_TYPE, FeaturePipeParam&);

    VarMap mVarMap;
    MUINT32 mFeatureMask;
    CALLBACK_T mCallback;
    NSCam::NSIoPipe::NSPostProc::QParams mQParams;

    FeaturePipeParam()
        : mFeatureMask(0)
        , mCallback(NULL)
    {
    }

    FeaturePipeParam(CALLBACK_T callback)
        : mFeatureMask(0)
        , mCallback(callback)
    {
    }

    MVOID setFeatureMask(MUINT32 mask, MBOOL enable)
    {
        if( enable )
        {
            mFeatureMask |= mask;
        }
        else
        {
            mFeatureMask &= (~mask);
        }
    }

    MVOID setQParams(NSCam::NSIoPipe::NSPostProc::QParams &qparams)
    {
        mQParams = qparams;
    }

    NSCam::NSIoPipe::NSPostProc::QParams getQParams() const
    {
        return mQParams;
    }

    DECLARE_VAR_MAP_INTERFACE(mVarMap, setVar, getVar, tryGetVar, clearVar);
};

class IStreamingFeaturePipe : public virtual android::RefBase
{
public:
    enum eUsageMode
    {
        USAGE_DEFAULT,
        USAGE_P2A_PASS_THROUGH,
        USAGE_P2A_PASS_THROUGH_TIME_SAHRING,
        USAGE_P2A_FEATURE,
        USAGE_FULL,
    };

    class BufferNumInfo
    {
    public:
        BufferNumInfo()
            : mBasic(0)
            , mExtra(0)
        {
        }

        BufferNumInfo(MUINT32 basic, MUINT32 extra=0)
            : mBasic(basic)
            , mExtra(extra)
        {
        }

    public:
        MUINT32 mBasic;
        MUINT32 mExtra;
    };

    class UsageHint
    {
    public:
        eUsageMode mMode;
        MSize mStreamingSize;
        MUINT32 mEisMode;
        MUINT32 mVendorMode;

        enum E3DNR_MODE_MASK
        {
            E3DNR_MODE_MASK_HAL_FORCE_SUPPORT     = 1 << 0,  // hal force support 3DNR
            E3DNR_MODE_MASK_UI_SUPPORT            = 1 << 1,  // feature option on/off
            E3DNR_MODE_MASK_SL2E_EN               = 1 << 6   // enable SL2E
        };

        UsageHint();
        UsageHint(eUsageMode mode, const MSize &streamingSize);

        MBOOL isTimeSharing() const;
        MBOOL support4K2K() const;
        MBOOL supportP2AFeature() const;
        MBOOL support3DNR() const;
        MBOOL supportVFB() const;
        MBOOL supportEIS_22() const;
        MBOOL supportEIS_25() const;
        MBOOL supportEIS_Q() const;
        MBOOL supportVendor() const;
        MBOOL supportGPUNode() const;
        MBOOL supportMDPNode() const;

        MUINT32 getNumP2ABuffer() const;
        MUINT32 getNumGpuInBuffer() const;
        MUINT32 getNumExtraGpuInBuffer() const;
        MUINT32 getNumGpuOutBuffer() const;
        MUINT32 getNumVendorInBuffer() const;
        MUINT32 getNumVendorOutBuffer() const;
        MVOID enable3DNRModeMask(E3DNR_MODE_MASK mask)
        {
            m3DNRMode |= mask;
        }
        MBOOL is3DNRModeMaskEnable(E3DNR_MODE_MASK mask) const
        {
            return (m3DNRMode & mask);
        }

    private:
        BufferNumInfo get3DNRBufferNum() const;
        BufferNumInfo getEISBufferNum() const;
        BufferNumInfo getVendorBufferNum() const;
        MINT32 m3DNRMode;// 3DNR flag
    };

public:
    virtual ~IStreamingFeaturePipe() {}

public:
    // interface for PipelineNode
    static IStreamingFeaturePipe* createInstance(MUINT32 openSensorIndex, const UsageHint &usageHint);
    MBOOL destroyInstance(const char *name=NULL);

    virtual MBOOL init(const char *name=NULL) = 0;
    virtual MBOOL uninit(const char *name=NULL) = 0;
    virtual MBOOL enque(const FeaturePipeParam &param) = 0;
    virtual MBOOL flush() = 0;
    virtual MBOOL setJpegParam(NSCam::NSIoPipe::NSPostProc::EJpgCmd cmd, int arg1, int arg2) = 0;
    virtual MBOOL setFps(MINT32 fps) = 0;
    virtual MUINT32 getRegTableSize() = 0;
    virtual MBOOL sendCommand(NSCam::NSIoPipe::NSPostProc::ESDCmd cmd, MINTPTR arg1=0, MINTPTR arg2=0, MINTPTR arg3=0) = 0;

protected:
    IStreamingFeaturePipe() {}
};

}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam

#undef MAKE_FEATURE_MASK_FUNC

#endif // _MTK_CAMERA_FEATURE_PIPE_I_STREAMING_FEATURE_PIPE_H_
