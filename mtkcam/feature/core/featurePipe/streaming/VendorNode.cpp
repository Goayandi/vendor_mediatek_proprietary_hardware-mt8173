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

#include "VendorNode.h"

#define PIPE_CLASS_TAG "VendorNode"
#define PIPE_TRACE TRACE_VENDOR_NODE
#include <featurePipe/core/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

VendorNode::VendorNode(const char *name)
    : StreamingFeatureNode(name)
    , mOutImgPoolAllocateNeed(0)
{
    TRACE_FUNC_ENTER();
    this->addWaitQueue(&mData);
    TRACE_FUNC_EXIT();
}

VendorNode::~VendorNode()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
}

MVOID VendorNode::setOutImgPool(const android::sp<GraphicBufferPool> &pool, MUINT32 allocate)
{
    TRACE_FUNC_ENTER();
    mOutImgPool = pool;
    mOutImgPoolAllocateNeed = allocate;
    TRACE_FUNC_EXIT();
}

MBOOL VendorNode::onData(DataID id, const ImgBufferData &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data.mRequest->mRequestNo, ID2Name(id));
    MBOOL ret = MFALSE;
    if( id == ID_P2A_TO_VENDOR_FULLIMG )
    {
        mData.enque(data);
        ret = MTRUE;
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL VendorNode::onInit()
{
    TRACE_FUNC_ENTER();
    StreamingFeatureNode::onInit();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorNode::onUninit()
{
    TRACE_FUNC_ENTER();
    mOutImgPool = NULL;
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorNode::onThreadStart()
{
    TRACE_FUNC_ENTER();
    if( mOutImgPoolAllocateNeed && mOutImgPool != NULL )
    {
        Timer timer;
        timer.start();
        mOutImgPool->allocate(mOutImgPoolAllocateNeed);
        timer.stop();
        MY_LOGD("fpipe.vendor %s %d buf in %d ms", STR_ALLOCATE, mOutImgPoolAllocateNeed, timer.getElapsed());
    }
    return MTRUE;
}

MBOOL VendorNode::onThreadStop()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorNode::onThreadLoop()
{
    TRACE_FUNC("Waitloop");
    ImgBufferData data;
    RequestPtr request;
    if( !waitAllQueue() )
    {
        return MFALSE;
    }
    if( !mData.deque(data) )
    {
        MY_LOGE("Request deque out of sync");
        return MFALSE;
    }
    if( data.mRequest == NULL )
    {
        MY_LOGE("Request out of sync");
        return MFALSE;
    }
    TRACE_FUNC_ENTER();
    request = data.mRequest;
    request->mTimer.startVendor();
    TRACE_FUNC("Frame %d in Vendor", request->mRequestNo);
    ImgBuffer out;
    if( request->needVendor() )
    {
        out = mOutImgPool->requestIIBuffer();
        out->getImageBuffer()->setExtParam(data.mData->getImageBuffer()->getImgSize());
        MBOOL result = processVendor(request, data.mData, out);
        request->updateResult(result);
    }
    else
    {
        // bypass, return input as output
        out = data.mData;
    }
    handleData(ID_VENDOR_TO_VMDP_FULLIMG, ImgBufferData(out, request));
    if( mUsageHint.supportGPUNode() )
    {
        handleData(ID_VENDOR_TO_NEXT_FULLIMG, ImgBufferData(out, request));
    }
    request->mTimer.stopVendor();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL VendorNode::processVendor(const RequestPtr &request, const ImgBuffer &in, const ImgBuffer &out)
{
    TRACE_FUNC_ENTER();
    MBOOL result = MTRUE;

    // Add vendor code here
    // and return process result

    TRACE_FUNC_EXIT();
    return result;
}

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
