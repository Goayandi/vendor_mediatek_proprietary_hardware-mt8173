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
#ifndef _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_GF_NODE_H
#define _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_GF_NODE_H
//
#include <utils/KeyedVector.h>
#include <utils/Mutex.h>
//
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>
#include <common/vsdof/hal/gf_hal.h>
//
#include "DepthMapPipeNode.h"
#include "DepthMapPipe_Common.h"
//
using namespace android;
//
class GF_HAL;
class DpBlitStream;
/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam{
namespace NSCamFeature{
namespace NSFeaturePipe{
class GFNode : public DepthMapPipeNode
{
//************************************************************************
// Public function
//************************************************************************
public:
    //****************************************************************
    // Does not support default construct
    //****************************************************************
    GFNode() = delete;
    GFNode(const char *name, DepthFlowType flowType);
    virtual ~GFNode();
//************************************************************************
// DepthMapPipeNode interface
//************************************************************************
protected:
    virtual MBOOL onInit();
    virtual MBOOL onUninit();
    virtual MBOOL onThreadStart();
    virtual MBOOL onThreadStop();
    virtual MBOOL onThreadLoop();
    virtual const char* onDumpBIDToName(DepthMapBufferID BID);
public:
    virtual MBOOL onData(DataID id, ImgInfoMapPtr& data);

//************************************************************************
// Private definition
//************************************************************************
private:
    typedef enum eGuidedFilterPass
    {
        eGF_PASS_NORMAL,
        eGF_PASS_CAPTURE,
    } GuidedFilterPass;
//************************************************************************
// Private function
//************************************************************************
private:
    MVOID cleanUp();
    //
    MBOOL createBufferPool(
            android::sp<ImageBufferPool>& pPool,
            MUINT32 width,
            MUINT32 height,
            NSCam::EImageFormat format,
            MUINT32 bufCount,
            const char* caller,
            MUINT32 bufUsage = ImageBufferPool::USAGE_HW);
    //
    MVOID setImageBufferValue(
            SmartImageBuffer& buffer,
            MINT32 width,
            MINT32 height,
            MINT32 value);
    //
    MBOOL executeAlgo(
            ImgInfoMapPtr pSrcImgInfoMap);
    //
    MBOOL runNormalPass(
            ImgInfoMapPtr pSrcImgInfoMap,
            DepthNodeOpState eOpState);
    //
    MBOOL runCapturePass(
            ImgInfoMapPtr pSrcImgInfoMap,
            DepthNodeOpState eOpState);
    //
    MBOOL requireAlgoDataFromRequest(
            const ImgInfoMapPtr pSrcImgInfoMap,
            GuidedFilterPass gfPass,
            GF_HAL_IN_DATA& inData,
            GF_HAL_OUT_DATA& outData);

    MBOOL requireInputMetaFromRequest(
            const ImgInfoMapPtr pSrcImgInfoMap,
            GF_HAL_IN_DATA& inData);
    //
    MBOOL jpegRotationOnDepthMap(
            const ImgInfoMapPtr pSrcImgInfoMap);
    //
    MVOID debugGFParams(
            const GF_HAL_IN_DATA& inData,
            const GF_HAL_OUT_DATA& outData);

//************************************************************************
// Private member
//************************************************************************
private:
    WaitQueue<ImgInfoMapPtr>              mJobQueue;
    // store the MYSL buffers, key
    KeyedVector<MUINT32, ImgInfoMapPtr>     mMYSLInfoMap;
    Mutex                                   mMYSLMutex;
    GF_HAL*                                 mpGf_Hal     = nullptr; // GF HAL for PV/VR
    GF_HAL*                                 mpGf_Hal_Cap = nullptr; // GF HAL for CAP
    android::sp<ImageBufferPool>            mpInternalDepthMapImgBufPool;
    DpBlitStream*                           mpDpStream = nullptr;
};

};
};
};
#endif // _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_GF_NODE_H