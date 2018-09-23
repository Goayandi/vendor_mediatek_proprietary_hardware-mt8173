/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2015. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#ifndef _IHDRPROC2_H_
#define _IHDRPROC2_H_

#include <utils/RefBase.h>
#include <utils/Vector.h>

#include <mtkcam/feature/hdr/HDRDefs.h>

#include <mtkcam/utils/imgbuf/IImageBuffer.h>

#include <mtkcam/main/core/module.h>

using namespace android;

namespace NSCam {

// ---------------------------------------------------------------------------

typedef MBOOL (*HDRProcCompleteCallback_t)(
        MVOID* user, const sp<IImageBuffer>& hdrResult, MBOOL ret);

class IHDRProc2;

// ---------------------------------------------------------------------------

class HDRProc2Factory
{
public:
    virtual ~HDRProc2Factory() = default;

    template<typename... Args>
    static IHDRProc2& getIHDRProc2(const Args&... /*args*/)
    {
        // Typedefs of function pointer that generate IHDRProc2 instance(s)
        typedef IHDRProc2* (*IHDRProc2_factory_t)();
        return *MAKE_MTKCAM_MODULE(
                MTKCAM_MODULE_ID_HDR_PROC2, IHDRProc2_factory_t/*, args...*/);
    }
};

// ---------------------------------------------------------------------------

class IHDRProc2
{
public:
    virtual ~IHDRProc2() = default;

    static IHDRProc2* loadModule();

    // init the instance
    virtual MBOOL init(MINT32 openID, HDRHandle& handle) = 0;

    // uninit the instance
    virtual MBOOL uninit(const HDRHandle& handle) = 0;

    virtual MBOOL setParam(
            const HDRHandle& handle,
            MUINT32 paramId, MUINTPTR iArg1, MUINTPTR iArg2) = 0;

    virtual MBOOL getParam(
            const HDRHandle& handle,
            MUINT32 paramId, MUINT32 & rArg1, MUINT32 & rArg2) = 0;

    virtual MBOOL setShotParam(
            const HDRHandle& handle,
            MSize& pictureSize, MSize& postviewSize, MRect& cropRegion) = 0;

    virtual MBOOL prepare(const HDRHandle& handle) = 0;

    virtual MBOOL addInputFrame(
            const HDRHandle& handle,
            MINT32 frameIndex, const sp<IImageBuffer>& inBuffer) = 0;
    virtual MBOOL addOutputFrame(
            const HDRHandle& handle,
            HDROutputType type, sp<IImageBuffer>& outBuffer) = 0;

    virtual MBOOL start(const HDRHandle& handle) = 0;
    virtual MBOOL release(const HDRHandle& handle) = 0;

    virtual MBOOL getHDRCapInfo(
            const HDRHandle& handle,
            MINT32& i4FrameNum,
            Vector<MUINT32>& vu4Eposuretime,
            Vector<MUINT32>& vu4SensorGain,
            Vector<MUINT32>& vu4FlareOffset) = 0;

    virtual MVOID setCompleteCallback(
            const HDRHandle& handle,
            HDRProcCompleteCallback_t completeCB,
            MVOID* user) = 0;

};

} // namespace NSCam

#endif // _IHDRPROC2_H_
