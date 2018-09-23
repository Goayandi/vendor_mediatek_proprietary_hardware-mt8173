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

#define LOG_TAG "mtkcam-vsdof"

#if defined(__func__)
#undef __func__
#endif
#define __func__ __FUNCTION__
//
#define LOG_VRB(fmt, arg...)        CAM_LOGV("[%s] " fmt, __func__, ##arg)
#define LOG_DBG(fmt, arg...)        CAM_LOGD("[%s] " fmt, __func__, ##arg)
#define LOG_INF(fmt, arg...)        CAM_LOGI("[%s] " fmt, __func__, ##arg)
#define LOG_WRN(fmt, arg...)        CAM_LOGW("[%s] " fmt, __func__, ##arg)
#define LOG_ERR(fmt, arg...)        CAM_LOGE("[ERROR][%s] " fmt, __func__, ##arg)
#define LOG_AST(cond, fmt, arg...)  do{ if(!cond) CAM_LOGA("[%s] " fmt, __func__, ##arg); } while(0)
//
#include <mtkcam/def/ImageFormat.h>
#include <DpBlitStream.h>
//
#include "vsdof_util.h"
//
using namespace NSCam;
//
namespace VSDOF
{
namespace util
{
DpColorFormat
getDpColorFormat(MINT32 format)
{
    switch(format)
    {
        case eImgFmt_RGBA8888:
            LOG_DBG("eImgFmt_RGBA8888");
            return DP_COLOR_RGBA8888;
        case eImgFmt_YUY2:
            LOG_DBG("eImgFmt_YUY2");
            return DP_COLOR_YUYV;
        case eImgFmt_NV16:
            LOG_DBG("eImgFmt_NV16");
            return DP_COLOR_NV16;
        case eImgFmt_NV21:
            LOG_DBG("eImgFmt_NV21");
            return DP_COLOR_NV21;
        case eImgFmt_YV12:
            LOG_DBG("eImgFmt_YV12");
            return DP_COLOR_YV12;
        case eImgFmt_STA_BYTE:
        case eImgFmt_Y8:
            LOG_DBG("eImgFmt_Y8 eImgFmt_STA_BYTE");
            return DP_COLOR_GREY;
        case eImgFmt_NV12:
            LOG_DBG("eImgFmt_NV12");
            return DP_COLOR_NV12;
        case eImgFmt_YV16:
            LOG_DBG("eImgFmt_YV16");
            return DP_COLOR_YV16;
        case eImgFmt_RGB48:
            LOG_DBG("eImgFmt_RGB48, return DP_COLOR_RGB565_RAW for special MDP path");
            return DP_COLOR_RGB565_RAW;
        case eImgFmt_BAYER12_UNPAK:
            LOG_DBG("eImgFmt_BAYER12_UNPAK, return DP_COLOR_RGB565_RAW for special MDP path");
            return DP_COLOR_RGB565_RAW;
        default:
            LOG_ERR("unsupported(%x)", format);
    }
    return DP_COLOR_UNKNOWN;
}
//
MBOOL
excuteMDP(sMDP_Config config)
{
    if(config.pDpStream == nullptr)
        return MFALSE;
    //***************************src****************************//
    MINTPTR src_addr_list[3] = {0, 0, 0};
    unsigned int src_size_list[3] = {0, 0, 0};
    size_t planeCount = config.pSrcBuffer->getPlaneCount();
    LOG_DBG("CKH: src planeCount: %d", planeCount);
    for(size_t i=0;i<planeCount;++i)
    {
        src_addr_list[i] = config.pSrcBuffer->getBufVA(i);
        src_size_list[i] = config.pSrcBuffer->getBufSizeInBytes(i);
    }
    MINT32 uvPitch = 0;
    if(planeCount>1)
    {
        uvPitch = config.pSrcBuffer->getBufStridesInBytes(1);
    }
    config.pDpStream->setSrcBuffer((void **)src_addr_list, src_size_list, planeCount);
    config.pDpStream->setSrcConfig(
                            config.pSrcBuffer->getImgSize().w,
                            config.pSrcBuffer->getImgSize().h,
                            config.pSrcBuffer->getBufStridesInBytes(0),
                            uvPitch,
                            getDpColorFormat(config.pSrcBuffer->getImgFormat()),
                            DP_PROFILE_FULL_BT601,
                            eInterlace_None,
                            0,
                            DP_SECURE_NONE,
                            false);
    LOG_DBG("CKH: config.pSrcBuffer->getBufStridesInBytes(0) %d", config.pSrcBuffer->getBufStridesInBytes(0));
    LOG_DBG("CKH: uvPitch %d", uvPitch);
    //***************************dst********************************//
    MINTPTR dst_addr_list[3] = {0, 0, 0};
    unsigned int dst_size_list[3] = {0, 0, 0};
    planeCount = config.pDstBuffer->getPlaneCount();
    LOG_DBG("CKH: dst planeCount: %d", planeCount);
    for(size_t i=0;i<planeCount;++i)
    {
        dst_addr_list[i] = config.pDstBuffer->getBufVA(i);
        dst_size_list[i] = config.pDstBuffer->getBufSizeInBytes(i);
    }
    uvPitch = 0;
    if(planeCount>1)
    {
        uvPitch = config.pDstBuffer->getBufStridesInBytes(1);
    }
    config.pDpStream->setDstBuffer((void**)dst_addr_list, dst_size_list, planeCount);
    config.pDpStream->setDstConfig(config.pDstBuffer->getImgSize().w,
                                    config.pDstBuffer->getImgSize().h,
                                    config.pDstBuffer->getBufStridesInBytes(0),
                                    uvPitch,
                                    getDpColorFormat(config.pDstBuffer->getImgFormat()),
                                    DP_PROFILE_FULL_BT601,
                                    eInterlace_None,
                                    0,
                                    DP_SECURE_NONE,
                                    false);
    config.pDpStream->setRotate(config.rotAngle);
    LOG_DBG("CKH: config.pDstBuffer->getBufStridesInBytes(0) %d", config.pDstBuffer->getBufStridesInBytes(0));
    LOG_DBG("CKH: uvPitch %d", uvPitch);
    LOG_DBG("CKH: config.rotAngle %d", config.rotAngle);
    //*******************************************************************//
    if (config.pDpStream->invalidate())  //trigger HW
    {
          LOG_ERR("FDstream invalidate failed");
          return MFALSE;
    }
    return MTRUE;
}
//
MBOOL
excuteMDPBayer12(sMDP_Config config)
{
    if(config.pDpStream == nullptr)
        return MFALSE;
    //***************************src****************************//
    MINTPTR src_addr_list[3] = {0, 0, 0};
    unsigned int src_size_list[3] = {0, 0, 0};
    size_t planeCount = config.pSrcBuffer->getPlaneCount();
    LOG_DBG("src planeCount:%d", planeCount);
    LOG_DBG("src srcSize(%dx%d), ImgBitsPerPixel:%d stride@plane[0]:%d",
        config.pSrcBuffer->getImgSize().w,
        config.pSrcBuffer->getImgSize().h,
        config.pSrcBuffer->getImgBitsPerPixel(),
        config.pSrcBuffer->getBufStridesInBytes(0)
    );


    for(size_t i=0;i<planeCount;++i)
    {
        src_addr_list[i] = config.pSrcBuffer->getBufVA(i);
        src_size_list[i] = config.pSrcBuffer->getBufSizeInBytes(i);
    }

    MINT32 yPitch = config.pSrcBuffer->getBufStridesInBytes(0);
    MINT32 uvPitch = 0;

    config.pDpStream->setSrcBuffer((void **)src_addr_list, src_size_list, planeCount);
    config.pDpStream->setSrcConfig(
                            config.pSrcBuffer->getImgSize().w,
                            config.pSrcBuffer->getImgSize().h,
                            yPitch,
                            uvPitch,
                            getDpColorFormat(config.pSrcBuffer->getImgFormat()),
                            DP_PROFILE_FULL_BT601,
                            eInterlace_None,
                            0,
                            DP_SECURE_NONE,
                            false);
    LOG_DBG("yPitch:%d, uvPitch:%d", yPitch, uvPitch);
    //***************************dst********************************//
    size_t dst_planeCount = config.pDstBuffer->getPlaneCount();
    MSize dstBufferSize = config.pDstBuffer->getImgSize();

    yPitch = config.customizedDstSize.w*(config.pDstBuffer->getImgBitsPerPixel()/8);
    uvPitch = 0;

    LOG_DBG("dst Buffer Size:%dx%d, ImgBitsPerPixel:%d, planeCount:%d",
        dstBufferSize.w,
        dstBufferSize.h,
        config.pDstBuffer->getImgBitsPerPixel(),
        dst_planeCount
    );

    LOG_DBG("customized dst Buffer Size:%dx%d rotateAngle:%d",
        config.customizedDstSize.w,
        config.customizedDstSize.h,
        config.rotAngle
    );

    MINTPTR dst_addr_list[3] = {0, 0, 0};
    unsigned int dst_size_list[3] = {0, 0, 0};
    for(size_t i=0;i<dst_planeCount;++i)
    {
        dst_addr_list[i] = config.pDstBuffer->getBufVA(i);
        dst_size_list[i] = config.customizedDstSize.w*config.customizedDstSize.h*(config.pDstBuffer->getImgBitsPerPixel()/8);
    }

    config.pDpStream->setDstBuffer((void**)dst_addr_list, dst_size_list, dst_planeCount);
    config.pDpStream->setDstConfig(
                                config.customizedDstSize.w,
                                config.customizedDstSize.h,
                                yPitch,
                                uvPitch,
                                getDpColorFormat(config.pDstBuffer->getImgFormat()),
                                DP_PROFILE_FULL_BT601,
                                eInterlace_None,
                                0,
                                DP_SECURE_NONE,
                                false);
    config.pDpStream->setRotate(config.rotAngle);
    //*******************************************************************//
    if (config.pDpStream->invalidate())  //trigger HW
    {
          LOG_ERR("FDstream invalidate failed");
          return MFALSE;
    }
    return MTRUE;
}
//
MBOOL
excuteMDPSubProcess( sMDP_Config config, sMDP_extConfig extConfig )
{
    if(config.pDpStream == nullptr)
        return MFALSE;

    LOG_DBG("excuteMDPSubProcess: getSrcBufSize:%d getDstBufSize:%d",
        config.pSrcBuffer->getBufSizeInBytes(0),
        config.pDstBuffer->getBufSizeInBytes(0)
    );
    //***************************src****************************//
    MINTPTR src_addr_list[3] = {0, 0, 0};
    unsigned int src_size_list[3] = {0, 0, 0};
    MINT32 uvPitch = 0;
    size_t planeCount = config.pSrcBuffer->getPlaneCount();

    MINT32 srcWidth, srcHeight, srcYPitch, srcBufferSize;
    srcWidth = extConfig.srcSize.w;
    srcHeight = extConfig.srcSize.h;
    srcYPitch = extConfig.srcStride;
    srcBufferSize = extConfig.srcBufSize;

    for(size_t i=0;i<planeCount;++i)
    {
        src_addr_list[i] = config.pSrcBuffer->getBufVA(i) + extConfig.srcOffset;
        src_size_list[i] = srcBufferSize;
    }

     LOG_DBG("excuteMDPSubProcess: src:%p src:%dx%d, srcYPitch:%d, bufSize:%d, planeCount:%d",
        src_addr_list[0],
        srcWidth,
        srcHeight,
        srcYPitch,
        srcBufferSize,
        planeCount
    );

    config.pDpStream->setSrcBuffer((void **)src_addr_list, src_size_list, planeCount);
    config.pDpStream->setSrcConfig(
                            srcWidth,
                            srcHeight,
                            srcYPitch,
                            uvPitch,
                            getDpColorFormat(config.pSrcBuffer->getImgFormat()),
                            DP_PROFILE_FULL_BT601,
                            eInterlace_None,
                            0,
                            DP_SECURE_NONE,
                            false);
    //***************************dst********************************//
    MINTPTR dst_addr_list[3] = {0, 0, 0};
    unsigned int dst_size_list[3] = {0, 0, 0};
    planeCount = config.pDstBuffer->getPlaneCount();

    MINT32 dstWidth, dstHeight, dstYPitch, dstBufferSize;
    dstWidth = extConfig.dstSize.w;
    dstHeight = extConfig.dstSize.h;
    dstYPitch = extConfig.dstStride;
    dstBufferSize = extConfig.dstBufSize;

    for(size_t i=0;i<planeCount;++i)
    {
        dst_addr_list[i] = config.pDstBuffer->getBufVA(i) + extConfig.dstOffset;
        dst_size_list[i] = dstBufferSize;
    }

    LOG_DBG("excuteMDPSubProcess: dst:%p dst:%dx%d, dstYPitch:%d, bufSize:%d, planeCount:%d, config.rotAngle:%d",
        dst_addr_list[0],
        dstWidth,
        dstHeight,
        dstYPitch,
        dstBufferSize,
        planeCount,
        config.rotAngle
    );

    config.pDpStream->setDstBuffer((void**)dst_addr_list, dst_size_list, planeCount);
    config.pDpStream->setDstConfig(
                                    dstWidth,
                                    dstHeight,
                                    dstYPitch,
                                    uvPitch,
                                    getDpColorFormat(config.pDstBuffer->getImgFormat()),
                                    DP_PROFILE_FULL_BT601,
                                    eInterlace_None,
                                    0,
                                    DP_SECURE_NONE,
                                    false);
    config.pDpStream->setRotate(config.rotAngle);
    //*******************************************************************//
    if (config.pDpStream->invalidate())  //trigger HW
    {
          LOG_ERR("FDstream invalidate failed");
          return MFALSE;
    }
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
android::sp<IImageBuffer>
createEmptyImageBuffer(
    const IImageBufferAllocator::ImgParam imgParam,
    const char* name,
    MINT usage)
{
    IImageBufferAllocator *allocator = IImageBufferAllocator::getInstance();
    android::sp<IImageBuffer> pImgBuf = allocator->alloc(name, imgParam);
    pImgBuf->lockBuf(name, usage);
    return pImgBuf;
}
}
}
