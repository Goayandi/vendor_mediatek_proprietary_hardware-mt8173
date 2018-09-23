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

#include "GPUNode.h"

#define PIPE_CLASS_TAG "GPUNode"
#define PIPE_TRACE TRACE_GPU_NODE
#include <featurePipe/core/include/PipeLog.h>
#include <mtkcam/feature/eis/eis_ext.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

GPUNode::GPUNode(const char *name)
    : StreamingFeatureNode(name)
    , mCurrentFeature(0)
    , mNumofAllocated(0)
    , mDynamicInit(MTRUE)
{
    TRACE_FUNC_ENTER();
    this->addWaitQueue(&mWarpMapDatas);
    this->addWaitQueue(&mFullImgDatas);
    TRACE_FUNC_EXIT();
}

GPUNode::~GPUNode()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
}

MVOID GPUNode::setInputBufferPool(const android::sp<GraphicBufferPool> &pool)
{
    TRACE_FUNC_ENTER();
    mInputBufferPool = pool;
    TRACE_FUNC_EXIT();
}

MVOID GPUNode::setOutputBufferPool(const android::sp<GraphicBufferPool> &pool)
{
    TRACE_FUNC_ENTER();
    mOutputBufferPool = pool;
    TRACE_FUNC_EXIT();
}

MBOOL GPUNode::onData(DataID id, const ImgBufferData &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data.mRequest->mRequestNo, ID2Name(id));
    MBOOL ret = MFALSE;
    if( id == ID_VFB_TO_GPU_WARP ||
        id == ID_EIS_TO_GPU_WARP )
    {
        this->mWarpMapDatas.enque(data);
        ret = MTRUE;
    }
    else if( id == ID_P2A_TO_GPU_FULLIMG ||
             id == ID_VENDOR_TO_NEXT_FULLIMG )
    {
        this->mFullImgDatas.enque(data);
        ret = MTRUE;
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL GPUNode::onInit()
{
    mNumofAllocated = 0;
    TRACE_FUNC_ENTER();
    StreamingFeatureNode::onInit();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL GPUNode::onUninit()
{
    mNumofAllocated = 0;
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL GPUNode::onThreadStart()
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    if( mInputBufferPool != NULL &&
        mOutputBufferPool != NULL )
    {
        MUINT32 inputPoolSize = mUsageHint.getNumGpuInBuffer();
        MUINT32 outputPoolSize = mUsageHint.getNumGpuOutBuffer();
        Timer timer(MTRUE);
        mNumofAllocated = mUsageHint.getNumExtraGpuInBuffer();

        mInputBufferPool->allocate(inputPoolSize);
        mOutputBufferPool->allocate(outputPoolSize);
        mNodeSignal->setSignal(NodeSignal::SIGNAL_GPU_READY);
        MY_LOGD("GPU %s (%d+%d) buf in %d ms", STR_ALLOCATE, inputPoolSize, outputPoolSize, timer.getNow());

        ret = mGpuWarp.init(mDynamicInit ? GpuWarpBase::REGISTER_MODE_RUNTIME : GpuWarpBase::REGISTER_MODE_INIT, mInputBufferPool->getImageSize(), MAX_WARP_SIZE);
        if( ret )
        {
            MUINT32 feature = GpuWarpBase::getDefaultFeature();
            feature = GpuWarpBase::toggleEIS(feature, MTRUE);
            if( configGpuWarp(NULL, NULL, feature) )
            {
                mCurrentFeature = feature;
            }
        }
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL GPUNode::onThreadStop()
{
    TRACE_FUNC_ENTER();
    mGpuWarp.uninit();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL GPUNode::onThreadLoop()
{
    TRACE_FUNC("Waitloop");
    MUINT32 requestNo;
    RequestPtr request;
    ImgBufferData warpMap;
    ImgBufferData inBuffer;

    tryAllocateBuffer();

    if( !waitAllQueueSync(requestNo) )
    {
        return MFALSE;
    }
    if( !mWarpMapDatas.deque(requestNo, warpMap) )
    {
        MY_LOGE("WarpMapData deque out of sync: %d", requestNo);
        return MFALSE;
    }
    else if( !mFullImgDatas.deque(requestNo, inBuffer) )
    {
        MY_LOGE("FullImgData deque out of sync: %d", requestNo);
        return MFALSE;
    }
    if( warpMap.mRequest == NULL ||
        warpMap.mRequest->mRequestNo != requestNo ||
        warpMap.mRequest != inBuffer.mRequest )
    {
        MY_LOGE("Request out of sync");
        return MFALSE;
    }
    TRACE_FUNC_ENTER();
    request = warpMap.mRequest;
    request->mTimer.startGPU();
    TRACE_FUNC("Frame %d in GPU", request->mRequestNo);
    processGPU(request, warpMap.mData, inBuffer.mData);
    request->mTimer.stopGPU();

    warpMap.mData = NULL;
    inBuffer.mData = NULL;
    TRACE_FUNC("FullImgPool (%d/%d)", mInputBufferPool->peakAvailableSize(), mInputBufferPool->peakPoolSize());

    tryAllocateBuffer();

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID GPUNode::tryAllocateBuffer()
{
    TRACE_FUNC_ENTER();
    while( (mNumofAllocated > 0) &&
           (mWarpMapDatas.empty() || mFullImgDatas.empty()) &&
           !mNodeSignal->getStatus(NodeSignal::STATUS_IN_FLUSH) )
    {
        mInputBufferPool->allocate();
        mNumofAllocated--;
        MY_LOGD("Allocation remain = %d", mNumofAllocated);
    }
    TRACE_FUNC_EXIT();
}

MBOOL GPUNode::processGPU(const RequestPtr &request, const ImgBuffer &warpMap, const ImgBuffer &fullImg)
{
    TRACE_FUNC_ENTER();
    ImgBufferData outBuffer;

    if( request->needGPU() )
    {
        sp<GraphicBuffer> *inGBAddr = NULL, *outGBAddr = NULL;
        MSize fullImgSize, inSize, outSize;
        MBOOL directOut = MFALSE;

        fullImgSize = request->mFullImgSize;
        inSize = fullImgSize;
        inGBAddr = fullImg->getGraphicBufferAddr();
        outBuffer.mRequest = request;

        if( request->useDirectGpuOut() )
        {
            IImageBuffer *recordBuffer = request->getRecordOutputBuffer();
            if( recordBuffer )
            {
                outSize = recordBuffer->getImgSize();
                outGBAddr = getGraphicBufferAddr(recordBuffer);
                if( outGBAddr != NULL )
                {
                    directOut = MTRUE;
                    outBuffer.mData = new IIBuffer_IImageBuffer(recordBuffer);
                }
            }
        }
        if( !directOut )
        {

            MUINT32 iCropRatio = request->is4K2K() ?
                                EISCustom::getEISFactor() : EISCustom::getEISFHDFactor();

            outSize.w = fullImgSize.w*100.0f/(iCropRatio);
            outSize.h = fullImgSize.h*100.0f/(iCropRatio);
            outBuffer.mData = mOutputBufferPool->requestIIBuffer();
            outBuffer.mData->getImageBuffer()->setExtParam(outSize);
            outGBAddr = outBuffer.mData->getGraphicBufferAddr();
        }

        MBOOL warpResult = MFALSE;
        if( updateGpuWarpConfig(request, inGBAddr, outGBAddr) )
        {
            IImageBuffer *warpPtr = warpMap->getImageBufferPtr();

            request->mTimer.startWarpGPU();
            warpResult = mGpuWarp.process(inGBAddr, outGBAddr, warpPtr, inSize, outSize);
            request->mTimer.stopWarpGPU();
        }
        if( !warpResult )
        {
            MY_LOGE("Frame %d failed GpuWarp", request->mRequestNo);
        }
        if( request->needDump() )
        {
            dumpData(request, warpMap->getImageBufferPtr(), "warp");
            IImageBuffer *out = directOut ? request->getRecordOutputBuffer() : outBuffer.mData->getImageBufferPtr();
            dumpData(request, out, "gpu");
        }

        request->updateResult(warpResult);
    }

    handleData(ID_GPU_TO_MDP_FULLIMG, outBuffer);
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL GPUNode::configGpuWarp(GpuWarpBase::GB_PTR inAddr, GpuWarpBase::GB_PTR outAddr, MUINT32 feature)
{
    TRACE_FUNC_ENTER();
    MBOOL ret;
    GpuWarpBase::GB_PTR_ARRAY inAddrArray, outAddrArray;
    prepareGBAddrArray(inAddr, outAddr, inAddrArray, outAddrArray);
    Timer timer(MTRUE);
    ret = mGpuWarp.config(inAddrArray, outAddrArray, feature);
    MY_LOGD("reconfig time = %d ms", timer.getNow());
    if( !ret )
    {
        MY_LOGE("GpuWarp config failed");
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL GPUNode::updateGpuWarpConfig(const RequestPtr &request, sp<GraphicBuffer> *inGBAddr, sp<GraphicBuffer> *outGBAddr)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;
    MUINT32 feature = GpuWarpBase::getDefaultFeature();

    if( request->needVFB() )
    {
        feature = GpuWarpBase::toggleVFB(feature, MTRUE);
    }
    else if( request->needEIS() )
    {
        feature = GpuWarpBase::toggleEIS(feature, MTRUE);
    }

    if( !inGBAddr || !outGBAddr )
    {
        MY_LOGE("Invalid GraphicBuffer: in=%p out=%p", inGBAddr, outGBAddr);
        ret = MFALSE;
    }
    else if( feature != mCurrentFeature ||
             !mGpuWarp.isGBRegister(inGBAddr, outGBAddr) )
    {
        ret = configGpuWarp(inGBAddr, outGBAddr, feature);
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MVOID GPUNode::prepareNGBAddrArray(GpuWarpBase::GB_PTR target, GraphicBufferPool::CONTAINER_TYPE &pool, MUINT32 limit, GpuWarpBase::GB_PTR_ARRAY &addrArray)
{
    TRACE_FUNC_ENTER();
    unsigned arraySize, poolSize, index, count;
    arraySize = poolSize = pool.size();
    if( arraySize > limit )
    {
        MY_LOGW("select %d out of %d pool for gpu config", limit, arraySize);
        arraySize = limit;
    }
    index = count = 0;
    if( target && arraySize < poolSize )
    {
        for( unsigned i = 0; i < poolSize; ++i )
        {
            if( target == pool[i]->getGraphicBufferAddr() )
            {
                index = i;
                break;
            }
        }
    }
    addrArray.resize(arraySize);
    for( ; count < arraySize && index < poolSize; ++count, ++index )
    {
        addrArray[count] = pool[index]->getGraphicBufferAddr();
    }
    for( index = 0; count < arraySize; ++count, ++index )
    {
        addrArray[count] = pool[index]->getGraphicBufferAddr();
    }
    TRACE_FUNC_EXIT();
}

MVOID GPUNode::prepareGBAddrArray(GpuWarpBase::GB_PTR inAddr, GpuWarpBase::GB_PTR outAddr, GpuWarpBase::GB_PTR_ARRAY &inAddrArray, GpuWarpBase::GB_PTR_ARRAY &outAddrArray)
{
    TRACE_FUNC_ENTER();
    if( mDynamicInit )
    {
        inAddrArray.clear();
        outAddrArray.clear();
        if( inAddr )
        {
            inAddrArray.resize(1);
            inAddrArray[0] = inAddr;
        }
        if( outAddr )
        {
            outAddrArray.resize(1);
            outAddrArray[0] = outAddr;
        }
    }
    else
    {
        GraphicBufferPool::CONTAINER_TYPE inPool, outPool;
        inPool = mInputBufferPool->getPoolContents();
        outPool = mOutputBufferPool->getPoolContents();
        prepareNGBAddrArray(inAddr, inPool, mGpuWarp.getNumRegisterSlot(), inAddrArray);
        prepareNGBAddrArray(outAddr, outPool, mGpuWarp.getNumRegisterSlot(), outAddrArray);
    }
    TRACE_FUNC_EXIT();
}

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
