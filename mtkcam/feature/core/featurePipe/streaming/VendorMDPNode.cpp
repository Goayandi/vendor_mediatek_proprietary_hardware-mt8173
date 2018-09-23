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

#include "VendorMDPNode.h"

#define PIPE_CLASS_TAG "VMDPNode"
#define PIPE_TRACE TRACE_VMDP_NODE
#include <featurePipe/core/include/PipeLog.h>
#include <mtkcam/feature/eis/eis_ext.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

VendorMDPNode::VendorMDPNode(const char *name)
    : StreamingFeatureNode(name)
    , mDpIspStream(DpIspStream::ISP_ZSD_STREAM)
{
    TRACE_FUNC_ENTER();
    this->addWaitQueue(&mData);
    TRACE_FUNC_EXIT();
}

VendorMDPNode::~VendorMDPNode()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
}

MBOOL VendorMDPNode::onData(DataID id, const ImgBufferData &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data.mRequest->mRequestNo, ID2Name(id));
    if( id == ID_VENDOR_TO_VMDP_FULLIMG )
    {
        this->mData.enque(data);
    }
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::onInit()
{
    TRACE_FUNC_ENTER();
    StreamingFeatureNode::onInit();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::onUninit()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::onThreadStart()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::onThreadStop()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::onThreadLoop()
{
    TRACE_FUNC("Waitloop");
    MBOOL isLastNode;
    RequestPtr request;
    ImgBufferData data;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }
    if( !mData.deque(data) )
    {
        MY_LOGE("Data deque out of sync");
        return MFALSE;
    }
    if( data.mRequest == NULL )
    {
        MY_LOGE("Request out of sync");
        return MFALSE;
    }
    TRACE_FUNC_ENTER();

    request = data.mRequest;
    request->mTimer.startVMDP();
    TRACE_FUNC("Frame %d in MDP", request->mRequestNo);
    processMDP(request, data.mData);
    request->mTimer.stopVMDP();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::processMDP(const RequestPtr &request, const ImgBuffer &out)
{
    TRACE_FUNC_ENTER();
    if( request->needVendorMDP() )
    {
        MBOOL result = prepareOut(request, out);
        request->updateResult(result);
    }
    FeaturePipeParam::MSG_TYPE msg = request->needEarlyDisplay() ? FeaturePipeParam::MSG_DISPLAY_DONE : FeaturePipeParam::MSG_FRAME_DONE;
    handleData(ID_VMDP_TO_HELPER, CBMsgData(msg, request));

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorMDPNode::prepareOut(const RequestPtr &request, const ImgBuffer &out)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;

    android::Vector<NSCam::NSIoPipe::NSPostProc::Output> outputs;
    MBOOL doAll = !request->needEarlyDisplay();
    for( unsigned i = 0, n = request->mQParams.mvOut.size(); i < n; ++i )
    {
        if( doAll ||
            isDisplayOutput(request->mQParams.mvOut[i]) )
        {
            outputs.push_back(request->mQParams.mvOut[i]);
        }
    }
    if( outputs.size() <= 0 )
    {
        return MTRUE;
    }

    MCrpRsInfo cropInfo;
    MCropRect cropRect;

    if( request->needEIS() &&
        (request->getEISDisplayCrop(cropInfo, RRZO_DOMAIN) ||
         request->getEISRecordCrop(cropInfo, RRZO_DOMAIN)) )
    {
        cropRect = cropInfo.mCropRect;
    }
    else if( !request->needEIS() &&
             (request->getDisplayCrop(cropInfo) ||
              request->getRecordCrop(cropInfo)) )
    {
        cropRect = cropInfo.mCropRect;
    }
    else
    {
        cropRect.s = out->getImageBuffer()->getImgSize();
    }
    cropRect.p_integral.x &= (~1);
    cropRect.p_integral.y &= (~1);

    ret = convertRGBA8888(out->getImageBufferPtr(), outputs, cropRect);

    if( request->needDump() )
    {
        for( unsigned i = 0; i < outputs.size(); ++i )
        {
            dumpData(request, outputs[i].mBuffer, "vmdp_%d", i);
        }
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL VendorMDPNode::convertRGBA8888(IImageBuffer *src,
                               const android::Vector<NSCam::NSIoPipe::NSPostProc::Output> &mvOut,
                               const MCropRect &crop)
{
    MUINT32 i,n;
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;
    MINT32 result;
    std::vector<IImageBuffer*>::const_iterator it, end;

    if (!src )
    {
        MY_LOGE("Invalid src");
        return MFALSE;
    }
    if( mvOut.size() <= 0 )
    {
        MY_LOGW("skipping process: mvOut.size() = %d", mvOut.size());
        return MTRUE;
    }

    DpColorFormat inFmt = SUPPORT_GPU_YV12 ? DP_COLOR_YV12 : DP_COLOR_RGBA8888;

    if( SUPPORT_GPU_YV12 )
    {
        result = mDpIspStream.setSrcConfig(src->getImgSize().w,
                                           src->getImgSize().h,
                                           src->getBufStridesInBytes(0),
                                           (src->getBufStridesInBytes(0) >> DP_COLOR_GET_H_SUBSAMPLE(inFmt)),
                                           inFmt,
                                           DP_PROFILE_FULL_BT601,
                                           eInterlace_None,
                                           NULL, false);
    }
    else
    {
        result = mDpIspStream.setSrcConfig(inFmt,
                                           src->getImgSize().w,
                                           src->getImgSize().h,
                                           src->getBufStridesInBytes(0),
                                           false);
    }
    if( result < 0 )
    {
        MY_LOGE("DpIspStream->setSrcConfig failed");
        return MFALSE;
    }

    MUINT32 va[3] = {(MUINT32) src->getBufVA(0),0,0};
    MUINT32 mva[3] = {(MUINT32) src->getBufPA(0),0,0};
    MUINT32 size[3] = {(MUINT32) src->getBufStridesInBytes(0) * src->getImgSize().h, 0, 0};
    MUINT32 planeCount = 1;

    if( SUPPORT_GPU_YV12 )
    {
        size[0] = src->getImgSize().w * src->getImgSize().h;
        for( unsigned i = 1; i < 3; ++i )
        {
            va[i] = (MUINT32)src->getBufVA(i);
            mva[i] = (MUINT32)src->getBufPA(i);
            size[i] = size[0] >> 2;
        }
        planeCount = DP_COLOR_GET_PLANE_COUNT(inFmt);
    }

    if (mDpIspStream.queueSrcBuffer((void**)va, mva, size, planeCount) < 0 )
    {
        MY_LOGE("DpIspStream->queueSrcBuffer failed");
        return MFALSE;
    }

    if (mDpIspStream.setSrcCrop(crop.p_integral.x, crop.p_fractional.x, crop.p_integral.y, crop.p_fractional.y, crop.s.w,crop.s.h) < 0 )
    {
        MY_LOGE("DpIspStream->setSrcCrop failed");
        return MFALSE;
    }

    for (i = 0, n = mvOut.size(); i < n; ++i )
    {
        DpColorFormat dpFmt;
        IImageBuffer* buffer = mvOut[i].mBuffer;

        if (!buffer )
        {
            MY_LOGE("Invalid output buffer");
            return MFALSE;
        }

        if (!toDpColorFormat((NSCam::EImageFormat) buffer->getImgFormat(), dpFmt) )
        {
            MY_LOGE("Not supported format");
            return MFALSE;
        }


        MUINT32 u4Rotation;
        MUINT32 u4Flip;
        MUINT32 u4Transform;

        u4Transform = mvOut[i].mTransform;

        switch (u4Transform)
        {
#define TransCase( trans, rot, flip ) \
        case (trans):                 \
            u4Rotation = (rot);       \
            u4Flip = (flip);          \
            break;
        TransCase(0                  , 0   , 0)
        TransCase(eTransform_FLIP_H  , 0   , 1)
        TransCase(eTransform_FLIP_V  , 180 , 1)
        TransCase(eTransform_ROT_90  , 90  , 0)
        TransCase(eTransform_ROT_180 , 180 , 0)
        TransCase(eTransform_FLIP_H|eTransform_ROT_90 , 270 , 1)
        TransCase(eTransform_FLIP_V|eTransform_ROT_90 , 90  , 1)
        TransCase(eTransform_ROT_270 , 270 , 0)
        default:
            MY_LOGE("not supported transform(0x%x)", u4Transform);
            u4Rotation = 0;
            u4Flip = 0;
            break;
#undef TransCase
        }

        MY_LOGD("u4Transform %d, Rotation %d, Flip %d", u4Transform, u4Rotation, u4Flip);

        if(mDpIspStream.setRotation(i, u4Rotation))
        {
            MY_LOGE("DpIspStream->setDstConfig failed");
            return MFALSE;
        }

        if(mDpIspStream.setFlipStatus(i, u4Flip))
        {
            MY_LOGE("DpIspStream->setDstConfig failed");
            return MFALSE;
        }

        DP_PROFILE_ENUM bProfile = DP_PROFILE_FULL_BT601;
        if (mvOut[i].mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_Rcrd)
        {
            bProfile = DP_PROFILE_BT601;
            MY_LOGD("mvOut record port ID: %d", i);
        }

        if (mDpIspStream.setDstConfig(i, //port
                buffer->getImgSize().w,
                buffer->getImgSize().h,
                buffer->getBufStridesInBytes(0),
                (buffer->getPlaneCount() > 1 ? buffer->getBufStridesInBytes(1) : 0),
                dpFmt,
                bProfile,
                eInterlace_None,  //default
                NULL, //default
                false) < 0)
        {
            MY_LOGE("DpIspStream->setDstConfig failed");
            return MFALSE;
        }

        MUINT32 va[3]   = {0,0,0};
        MUINT32 size[3] = {0,0,0};
        MUINT32 pa[3]   = {0,0,0};
        for (unsigned j = 0; j < buffer->getPlaneCount(); ++j )
        {
            va[j]   = buffer->getBufVA(j);
            pa[j]   = buffer->getBufPA(j);
            size[j] = buffer->getBufSizeInBytes(j);
        }

        if (mDpIspStream.queueDstBuffer(i, //port
                (void**)va,
                pa,
                size,
                buffer->getPlaneCount()) < 0)
        {
            MY_LOGE("queueDstBuffer failed");
            return MFALSE;
        }
    }

    if (mDpIspStream.startStream() < 0 )
    {
        MY_LOGE("startStream failed");
        return MFALSE;
    }

    if (mDpIspStream.dequeueSrcBuffer() < 0 )
    {
        MY_LOGE("dequeueSrcBuffer failed");
        return MFALSE;
    }

    for ( i = 0, n = mvOut.size(); i < n; ++i )
    {
        MUINT32 va[3] = {0,0,0};

        if( mDpIspStream.dequeueDstBuffer(i,(void**)va) < 0 )
        {
            MY_LOGE("dequeueDstBuffer failed");
            return MFALSE;
        }
    }

    if (mDpIspStream.stopStream() < 0 )
    {
        MY_LOGE("stopStream failed");
        return MFALSE;
    }

    if (mDpIspStream.dequeueFrameEnd() < 0 )
    {
        MY_LOGE("dequeueFrameEnd failed");
        return MFALSE;
    }

    TRACE_FUNC_EXIT();
    return ret;
}



} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
