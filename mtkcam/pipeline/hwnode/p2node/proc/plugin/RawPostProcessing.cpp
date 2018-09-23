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


#include "RawPostProcessing.h"
#include "../PluginProcedure.h"
#include <mtkcam/utils/std/DebugScanLine.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

// test mode: 0=> in-place processing, 1=> in-out processing
//  => in-place: do a software NR and draw a thick line in the middle of image
//  => in-out: memcpy the source image and draw a thick line in the middle of image
#define TEST_MODE (0)

// auto-mount raw processing
REGISTER_POSTPROCESSING(Raw, RawPostProcessing);

/******************************************************************************
 *
 ******************************************************************************/
RawPostProcessing::
RawPostProcessing(MUINT32 const openId)
        : PostProcessing(openId)
{
}


/******************************************************************************
 *
 ******************************************************************************/
RawPostProcessing::
~RawPostProcessing() {
}


/******************************************************************************
 *
 ******************************************************************************/
PostProcessing::ProcessingProfile&
RawPostProcessing::
profile() {
#if TEST_MODE == 0
    static PostProcessing::ProcessingProfile profile(
            eIN_PLACE_PROCESSING,
            eIMG_FMT_DEFAULT);
#elif TEST_MODE == 1
    static PostProcessing::ProcessingProfile profile(
            eIN_OUT_PROCESSING,
            eIMG_FMT_DEFAULT,
            eIMG_FMT_DEFAULT);
#else
#error unsupported this test mode
#endif
    return profile;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
RawPostProcessing::
doOrNot(PreConditionParams const& params) {
    MUINT8 flag = 0;
    if (params.bResized || (tryGetMetadata<MUINT8>(params.pInHalMeta, MTK_P2NODE_UT_PLUGIN_FLAG, flag) && !(flag & MTK_P2NODE_UT_PLUGIN_FLAG_RAW)))
        return MFALSE;

    return MTRUE;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
RawPostProcessing::
process(ProcessingParams const& param) {

    IImageBuffer *pInBuf = param.pInBuffer;
    void *pInVa = (void *) (pInBuf->getBufVA(0));
    int nImgWidth = pInBuf->getImgSize().w;
    int nImgHeight = pInBuf->getImgSize().h;
    int nBufSize = pInBuf->getBufSizeInBytes(0);
    int nImgStride = pInBuf->getBufStridesInBytes(0);

    DebugScanLine *mpDebugScanLine = DebugScanLine::createInstance();

#if TEST_MODE == 0
    MY_LOGD("in-place processing. va:0x%x", pInVa);
    for (int i = 0; i < 5; i++)
        mpDebugScanLine->drawScanLine(nImgWidth, nImgHeight, pInVa, nBufSize, nImgStride);
#elif TEST_MODE == 1
    IImageBuffer *pOutBuf = param.pOutBuffer;
    void *pOutVa = (void *) (pOutBuf->getBufVA(0));
    MY_LOGD("in-out processing. va[in]:0x%x, va[out]:0x%x", pInVa, pOutVa);
    MY_LOGD("raw memcpy. width:%d, height:%d, stride:%d, buf_size:%d", nImgWidth, nImgHeight, nImgStride, nBufSize);
    memcpy(pOutVa, pInVa, nBufSize);
    //MY_LOGD("memcmp ret:%d", memcmp(pInVa, pOutVa , nBufSize));
    for (int i = 0; i < 5; i++)
        mpDebugScanLine->drawScanLine(nImgWidth, nImgHeight, pOutVa, nBufSize, nImgStride);
    pOutBuf->syncCache(eCACHECTRL_FLUSH);
#endif

    if (mpDebugScanLine != NULL) {
        mpDebugScanLine->destroyInstance();
        mpDebugScanLine = NULL;
    }

    return MTRUE;
}


