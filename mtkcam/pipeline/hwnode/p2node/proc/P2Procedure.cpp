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

#define PROCESSOR_NAME_P2   ("P2")
#define DEFINE_OPEN_ID      (muOpenId)

#include "../P2Common.h"
#include "P2Procedure.h"

#include <fstream>
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/hw/HwTransform.h>

using namespace NSCamHW;
using namespace NSIoPipe;
using namespace NSIoPipe::NSPostProc;
using namespace NS3Av3;

#define DEBUG_PORT_IN_IMGO          (0x1)
#define DEBUG_PORT_IN_RRZO          (0x2)
#define DEBUG_PORT_OUT_WDMAO        (0x1)
#define DEBUG_PORT_OUT_WROTO        (0x2)
#define DEBUG_PORT_OUT_IMG2O        (0x4)
#define DEBUG_PORT_OUT_IMG3O        (0x8)
#define DEBUG_PORT_OUT_MFBO        (0x10)
#define DEBUG_DRAWLINE_PORT_WDMAO   (0x1)
#define DEBUG_DRAWLINE_PORT_WROTO   (0x2)
#define DEBUG_DRAWLINE_PORT_IMG2O   (0x4)

#ifdef EIS_FACTOR
#undef EIS_FACTOR
#endif
#define EIS_FACTOR 120


/******************************************************************************
 *
 ******************************************************************************/
P2Procedure::
P2Procedure(CreateParams const &params)
        : mbEnableLog(params.bEnableLog),
          muOpenId(params.uOpenId),
          mCreateParams(params),
          mpPipe(params.pPipe),
          mnStreamTag(-1),
          mp3A(params.p3A),
          mConfigVencStream(MFALSE),
          muRequestCnt(0),
          muEnqueCnt(0),
          muDequeCnt(0),
          mDebugScanLineMask(0),
          mpDebugScanLine(NULL)
{
    mpMultiFrameHandler = new MultiFrameHandler(
            params.pPipe, params.bEnableLog);

    muDumpBuffer = ::property_get_int32("debug.camera.dump.p2", 0);
    muDumpCondIn = ::property_get_int32("debug.camera.dump.p2.cond.in", 0xFF);
    muDumpPortIn = ::property_get_int32("debug.camera.dump.p2.in", 0xFF);
    muDumpPortOut = ::property_get_int32("debug.camera.dump.p2.out", 0xFF);
    muDumpPortImg3o = ::property_get_int32("debug.camera.dump.p2.ext.img3o", 0);
    muDumpPortMfbo = ::property_get_int32("debug.camera.dump.p2.ext.mfbo", 0);
    muSensorFormatOrder = SENSOR_FORMAT_ORDER_NONE;

    if (muDumpBuffer) {
        IHalSensorList *const pIHalSensorList = MAKE_HalSensorList();
        if (pIHalSensorList) {
            MUINT32 sensorDev = (MUINT32) pIHalSensorList->querySensorDevIdx(muOpenId);

            NSCam::SensorStaticInfo sensorStaticInfo;
            memset(&sensorStaticInfo, 0, sizeof(NSCam::SensorStaticInfo));
            pIHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
            muSensorFormatOrder = sensorStaticInfo.sensorFormatOrder;
        }
    }

    mDebugScanLineMask = ::property_get_int32("debug.camera.scanline.p2", 0);
    if (mDebugScanLineMask != 0) {
        mpDebugScanLine = DebugScanLine::createInstance();
    }

    mnStreamTag = (params.type == P2Node::PASS2_TIMESHARING)
                  ? ENormalStreamTag_Vss
                  : ENormalStreamTag_Normal;

}


/******************************************************************************
 *
 ******************************************************************************/
sp<Processor>
P2Procedure::
createProcessor(CreateParams &params) {
    INormalStream *pPipe = NULL;
    IHal3A_T *p3A = NULL;
    if (params.type != P2Node::PASS2_STREAM &&
        params.type != P2Node::PASS2_TIMESHARING) {
        MY_LOGE("not supported type %d", params.type);
        goto lbExit;
    }

    CAM_TRACE_BEGIN("P2:NormalStream:create");
    pPipe = INormalStream::createInstance(params.uOpenId);
    CAM_TRACE_END();
    if (pPipe == NULL) {
        MY_LOGE("create pipe failed");
        goto lbExit;
    }

    CAM_TRACE_BEGIN("P2:NormalStream:init");
    if (!pPipe->init(LOG_TAG)) {
        CAM_TRACE_END();
        MY_LOGE("pipe init failed");
        goto lbExit;
    }
    CAM_TRACE_END();

#if SUPPORT_3A
    CAM_TRACE_BEGIN("P2:3A:create");
    p3A = MAKE_Hal3A(params.uOpenId, LOG_TAG);
    CAM_TRACE_END();
#endif
    if (p3A == NULL) {
        MY_LOGE("create 3A failed");
        goto lbExit;
    }
    MY_LOGD("create processor type %d: pipe %p, 3A %p",
            params.type, pPipe, p3A);

    lbExit:
    if (!pPipe || !p3A) {
        if (pPipe) {
            pPipe->uninit(LOG_TAG);
            pPipe->destroyInstance();
            pPipe = NULL;
        }
        if (p3A) {
            p3A->destroyInstance(LOG_TAG);
            p3A = NULL;
        }
    }

    params.pPipe = pPipe;
    params.p3A = p3A;
    return pPipe ? new ProcessorBase<P2Procedure>(
            params.uOpenId, params, PROCESSOR_NAME_P2) : NULL;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
mapPortId(
        StreamId_T const streamId,  // [in]
        MUINT32 const transform,    // [in]
        MBOOL const isFdStream,     // [in]
        MUINT8 &rOccupied,          // [in/out]
        PortID &rPortId             // [out]
) const
{
    MERROR ret = OK;
#define PORT_WDMAO_USED  (0x1)
#define PORT_WROTO_USED  (0x2)
#define PORT_IMG2O_USED  (0x4)
    if (transform != 0) {
        if (!(rOccupied & PORT_WROTO_USED)) {
            rPortId = PORT_WROTO;
            rOccupied |= PORT_WROTO_USED;
        }
        else
            ret = INVALID_OPERATION;
    }
    else {
#if SUPPORT_FD_PORT
        if (SUPPORT_FD_PORT && isFdStream) {
            if (rOccupied & PORT_IMG2O_USED) {
                MY_LOGWO("should not be occupied");
                ret = INVALID_OPERATION;
            } else {
                rOccupied |= PORT_IMG2O_USED;
                rPortId = PORT_IMG2O;
            }
        } else
#endif
        if (!(rOccupied & PORT_WDMAO_USED)) {
            rOccupied |= PORT_WDMAO_USED;
            rPortId = PORT_WDMAO;
        } else if (!(rOccupied & PORT_WROTO_USED)) {
            rOccupied |= PORT_WROTO_USED;
            rPortId = PORT_WROTO;
        } else
            ret = INVALID_OPERATION;
    }
    MY_LOGDO_IF(0, "stream id %#" PRIxPTR ", occupied %u",
               streamId, rOccupied);
    return ret;
#undef PORT_WDMAO_USED
#undef PORT_WROTO_USED
#undef PORT_IMG2O_USED
}


/******************************************************************************
 *
 ******************************************************************************/
P2Procedure::
~P2Procedure() {
    MY_LOGDO_IF(mbEnableLog, "destroy processor %d: %p", mCreateParams.type, mpPipe);

    if (mpPipe) {
        if (mConfigVencStream) {
            if (mpPipe->sendCommand(ESDCmd_RELEASE_VENC_DIRLK)) {
                MY_LOGEO("release venc stream failed");
            }
        }
        if (!mpPipe->uninit(LOG_TAG)) {
            MY_LOGEO("pipe uninit failed");
        }
        mpPipe->destroyInstance();
    }

    if (mp3A) {
        mp3A->destroyInstance(LOG_TAG);
    }

    if (mpMultiFrameHandler) {
        delete mpMultiFrameHandler;
    }

    if (mpDebugScanLine != NULL) {
        mpDebugScanLine->destroyInstance();
        mpDebugScanLine = NULL;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
onP2Execute(
        sp<Request> const pRequest,
        FrameParams const &params)
{
    CAM_TRACE_NAME("P2:onP2Execute");
    FUNC_START;

    MERROR ret = OK;
    if (OK != (ret = checkParams(params)))
        return ret;
    // prepare metadata
    IMetadata *pMeta_InApp = params.inApp->getMetadata();
    IMetadata *pMeta_InHal = params.inHal->getMetadata();
    IMetadata *pMeta_OutApp = params.outApp.get() ? params.outApp->getMetadata() : NULL;
    IMetadata *pMeta_OutHal = params.outHal.get() ? params.outHal->getMetadata() : NULL;

    if (pMeta_InApp == NULL || (!params.bYuvReproc && pMeta_InHal == NULL)) {
        MY_LOGEO("meta: in app %p, in hal %p", pMeta_InApp, pMeta_InHal);
        return BAD_VALUE;
    }

    sp<Cropper::CropInfo> pCropInfo = new Cropper::CropInfo;
    if (OK != (ret = getCropInfo(pMeta_InApp, pMeta_InHal, params.bResized, *pCropInfo))) {
        MY_LOGEO("getCropInfo failed");
        return ret;
    }
    pRequest->context.crop_info = pCropInfo;

    String8 strEnqueLog;
    QParams enqueParams;
    //frame tag
    enqueParams.mvStreamTag.push_back(mnStreamTag);

    // input
    {
        if (OK != (ret = params.in.mHandle->waitState(BufferHandle::STATE_READABLE))) {
            MY_LOGWO("src buffer err = %d", ret);
            return BAD_VALUE;
        }
        IImageBuffer *pSrc = params.in.mHandle->getBuffer();

        Input src;
        src.mPortID = params.in.mPortId;
        src.mPortID.group = 0;
        src.mBuffer = pSrc;
        // update src size
#if SUPPORT_MNR || SUPPORT_SWNR || SUPPORT_PLUGIN
        if (params.bRunWorkBuffer) {
            // do nothing if run on working buffer
        }
        else
#endif
        if (params.bResized)
            pSrc->setExtParam(pCropInfo->dstsize_resizer);

        enqueParams.mvIn.push_back(src);
        strEnqueLog += String8::format("EnQ: Src Port(%d) Fmt(0x%x) Size(%dx%d) => ",
                   src.mPortID.index, src.mBuffer->getImgFormat(),
                   src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
    }

    // input LCEI
    if(params.in_lcso.mHandle != NULL){
        if( OK != (ret = params.in_lcso.mHandle->waitState(BufferHandle::STATE_READABLE)) ) {
            MY_LOGW(" (%d) Lcso handle not null but waitState failed! ", pRequest->getFrameNo());
            return BAD_VALUE;
        }else {
            IImageBuffer* pSrc = params.in_lcso.mHandle->getBuffer();
            //
            Input src;
            src.mPortID       = params.in_lcso.mPortId;
            src.mPortID.group = 0;
            src.mBuffer       = pSrc;

            //
            enqueParams.mvIn.push_back(src);
            MY_LOGD_IF(mbEnableLog, "EnQ Src mPortID.index(%d) Fmt(0x%x) "
                "Size(%dx%d)", src.mPortID.index, src.mBuffer->getImgFormat(),
                src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
        }
        //

    }
    // output
    for (size_t i = 0; i < params.vOut.size(); i++) {
        if (params.vOut[i].mHandle == NULL ||
            OK != (ret = params.vOut[i].mHandle->waitState(BufferHandle::STATE_WRITABLE))) {
            MY_LOGWO("dst buffer err = %d", ret);
            continue;
        }
        IImageBuffer *pDst = params.vOut[i].mHandle->getBuffer();

        Output dst;
        dst.mPortID = params.vOut[i].mPortId;
        dst.mPortID.group = 0;
        MUINT32 const uUsage = params.vOut[i].mUsage;
        dst.mPortID.capbility = (NSIoPipe::EPortCapbility)(
                (uUsage & GRALLOC_USAGE_HW_COMPOSER) ? EPortCapbility_Disp :
                (uUsage & GRALLOC_USAGE_HW_VIDEO_ENCODER) ? EPortCapbility_Rcrd :
                EPortCapbility_None);
        dst.mBuffer = pDst;
        dst.mTransform = params.vOut[i].mTransform;

        enqueParams.mvOut.push_back(dst);
    }

    if (enqueParams.mvOut.size() == 0) {
        //MY_LOGWO("no dst buffer");
        return BAD_VALUE;
    }

    {
        TuningParam rTuningParam = {NULL, NULL, NULL};
        void *pTuning = NULL;
        unsigned int tuningsize = (mpPipe != NULL) ? mpPipe->getRegTableSize() : 0;//sizeof(dip_x_reg_t);
        if (tuningsize == 0) {
            MY_LOGW("getRegTableSize is 0 (%p)", mpPipe);
        }
        pTuning = ::malloc(tuningsize);
        if (pTuning == NULL) {
            MY_LOGEO("alloc tuning buffer fail");
            return NO_MEMORY;
        }
        rTuningParam.pRegBuf = pTuning;
        if(params.in_lcso.mHandle != NULL) {
            rTuningParam.pLcsBuf = params.in_lcso.mHandle->getBuffer();
        }
        MY_LOGDO_IF(mbEnableLog, "pass2 setIsp malloc %p : %d, LCSO exist(%d)", pTuning, tuningsize, (rTuningParam.pLcsBuf != NULL));

        MetaSet_T inMetaSet;
        MetaSet_T outMetaSet;

        inMetaSet.appMeta = *pMeta_InApp;
        inMetaSet.halMeta = *pMeta_InHal;

        MBOOL const bGetResult = (pMeta_OutApp || pMeta_OutHal);

        if (params.bResized) {
            trySetMetadata<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 0);
        } else {
            trySetMetadata<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 1);
        }
        if (pMeta_OutHal) {
            // FIX ME: getDebugInfo() @ setIsp() should be modified
            outMetaSet.halMeta = *pMeta_InHal;
        }

        if (mp3A) {
            trySetMetadata<MINT32>(&inMetaSet.halMeta, MTK_PIPELINE_FRAME_NUMBER, pRequest->getFrameNo());
            trySetMetadata<MINT32>(&inMetaSet.halMeta, MTK_PIPELINE_REQUEST_NUMBER, pRequest->getRequestNo());

#if SUPPORT_MNR
            if (pMeta_InHal && params.bRunMnr) {
                trySetMetadata<MUINT8>(&inMetaSet.halMeta, MTK_3A_ISP_PROFILE, EIspProfile_Capture_MultiPass_HWNR);
            } else
#endif
            if (params.bYuvReproc) {
                trySetMetadata<MUINT8>(&inMetaSet.halMeta, MTK_3A_ISP_PROFILE, EIspProfile_YUV_Reprocess);
            }

            MY_LOGDO_IF(mbEnableLog, "P2 setIsp %p : %d", pTuning, tuningsize);
            if (0 > mp3A->setIsp(0, inMetaSet, &rTuningParam,
                                 (bGetResult ? &outMetaSet : NULL))) {
                MY_LOGWO("P2 setIsp - skip tuning pushing");
                if (pTuning != NULL) {
                    MY_LOGDO_IF(mbEnableLog, "P2 setIsp free %p : %d", pTuning, tuningsize);
                    ::free(pTuning);
                }
            } else {
                enqueParams.mvTuningData.push_back(pTuning);
#if SUPPORT_IMG3O_PORT
                // dump tuning data
                if (((!params.bResized && muDumpCondIn & DEBUG_PORT_IN_IMGO) ||
                     (params.bResized && muDumpCondIn & DEBUG_PORT_IN_RRZO)) &&
                    muDumpPortImg3o && muDumpPortOut & DEBUG_PORT_OUT_IMG3O) {
                    char filename[100];
                    sprintf(filename, P2_DEBUG_DUMP_PATH "/p2-%09d-%04d-%04d-tuning.data",
                            params.uUniqueKey, params.uFrameNo, params.uRequestNo);
                    std::ofstream out(filename);
                    out.write(reinterpret_cast<char *>(pTuning), tuningsize);
                }
#endif

                IImageBuffer *pSrc = static_cast<IImageBuffer *>(rTuningParam.pLsc2Buf);
                if (pSrc != NULL) {
                    Input src;
                    src.mPortID = PORT_DEPI;
                    src.mPortID.group = 0;
                    src.mBuffer = pSrc;

                    enqueParams.mvIn.push_back(src);
                    MY_LOGDO_IF(mbEnableLog, "EnQ Src mPortID.index(%d) Fmt(0x%x) "
                            "Size(%dx%d)", src.mPortID.index, src.mBuffer->getImgFormat(),
                               src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
                }
            }
        } else {
            MY_LOGDO_IF(mbEnableLog, "P2 setIsp clear tuning %p : %d", pTuning, tuningsize);
            ::memset((unsigned char *) (pTuning), 0, tuningsize);
        }

        if (pMeta_OutApp) {
            if (params.bYuvReproc) {
                *pMeta_OutApp = *pMeta_InApp;
                *pMeta_OutApp += outMetaSet.appMeta;
                pMeta_OutApp->remove(MTK_SENSOR_TIMESTAMP);
                pMeta_OutApp->remove(MTK_JPEG_THUMBNAIL_SIZE);
                pMeta_OutApp->remove(MTK_JPEG_ORIENTATION);
            }
            else {
                *pMeta_OutApp = outMetaSet.appMeta;
                // workaround (for YUV Reprocessing)
                pMeta_OutApp->remove(MTK_EDGE_MODE);
                pMeta_OutApp->remove(MTK_NOISE_REDUCTION_MODE);
                pMeta_OutApp->remove(MTK_JPEG_THUMBNAIL_SIZE);
                pMeta_OutApp->remove(MTK_JPEG_ORIENTATION);
            }
            MRect cropRegion = pCropInfo->crop_a;
            if (pCropInfo->isEisEabled) {
                cropRegion.p.x += pCropInfo->eis_mv_a.p.x;
                cropRegion.p.y += pCropInfo->eis_mv_a.p.y;
            }
            updateCropRegion(cropRegion, pMeta_OutApp);
        }

        if (pMeta_OutHal) {
            if (!pMeta_OutHal->count())
                *pMeta_OutHal = *pMeta_InHal;
            *pMeta_OutHal += outMetaSet.halMeta;
        }
    }
    // for output group crop
    {
        Vector<Output>::const_iterator iter = enqueParams.mvOut.begin();
        while (iter != enqueParams.mvOut.end()) {
            MCrpRsInfo crop;

            MUINT32 const uPortIndex = iter->mPortID.index;
            if (uPortIndex == PORT_WDMAO.index) {
                crop.mGroupID = 2;
#if SUPPORT_MNR || SUPPORT_SWNR || SUPPORT_PLUGIN
                if (params.bRunWorkBuffer) {
                    crop.mCropRect.p_fractional = {0, 0};
                    crop.mCropRect.p_integral = {0, 0};
                    crop.mCropRect.s = iter->mBuffer->getImgSize();
                }
                else
#endif
                    Cropper::calcViewAngle(mbEnableLog, *pCropInfo, iter->mBuffer->getImgSize(), crop.mCropRect);
            } else if (uPortIndex == PORT_WROTO.index) {
                crop.mGroupID = 3;
                IImageBuffer *pBuf = iter->mBuffer;
                MINT32 const transform = iter->mTransform;
                MSize dstSize = (transform & eTransform_ROT_90)
                                ? MSize(pBuf->getImgSize().h, pBuf->getImgSize().w)
                                : pBuf->getImgSize();
                Cropper::calcViewAngle(mbEnableLog, *pCropInfo, dstSize, crop.mCropRect);
            } else if (uPortIndex == PORT_VENC_STREAMO.index) {
                crop.mGroupID = 2;
                Cropper::calcViewAngle(mbEnableLog, *pCropInfo, iter->mBuffer->getImgSize(), crop.mCropRect);
#if SUPPORT_FD_PORT
            } else if (uPortIndex == PORT_IMG2O.index) {
                crop.mGroupID = 1;
                Cropper::calcViewAngle(mbEnableLog, *pCropInfo, iter->mBuffer->getImgSize(), crop.mCropRect);
#endif
#if SUPPORT_IMG3O_PORT
            } else if (uPortIndex == PORT_IMG3O.index) {
                crop.mGroupID = 4;
                crop.mCropRect.p_fractional = {0, 0};
                crop.mCropRect.p_integral = {0, 0};
                crop.mCropRect.s = iter->mBuffer->getImgSize();
#endif
#if SUPPORT_MFBO_PORT
            } else if (uPortIndex == PORT_MFBO.index) {
                //crop.mGroupID = 4;
                crop.mCropRect.p_fractional = {0, 0};
                crop.mCropRect.p_integral = {0, 0};
                crop.mCropRect.s = iter->mBuffer->getImgSize();
#endif

            } else {
                MY_LOGEO("not supported output port %d", iter->mPortID.index);
                return BAD_VALUE;
            }
            crop.mResizeDst = iter->mBuffer->getImgSize();

            if(iter != enqueParams.mvOut.begin())
                strEnqueLog += ", ";
            strEnqueLog += String8::format("Dst Grp(%d) Rot(%d) Crop(%d,%d)(%dx%d) Size(%dx%d)",
                    crop.mGroupID, iter->mTransform,
                    crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                    crop.mCropRect.s.w, crop.mCropRect.s.h,
                    crop.mResizeDst.w, crop.mResizeDst.h);

            enqueParams.mvCropRsInfo.push_back(crop);
            iter++;
        }
    }

    MY_LOGDO("%s", strEnqueLog.string());

    if (pRequest->context.burst_num > 1) {
        if (mpMultiFrameHandler)
            return mpMultiFrameHandler->collect(pRequest, enqueParams, params.bBurstEnque);
        else
            MY_LOGWO_IF(mbEnableLog, "no burst handler");
    }
    // callback
    enqueParams.mpfnCallback = pass2CbFunc;
    enqueParams.mpCookie = this;

#if 0
    // FIXME: need this?
    enqueParams.mvPrivaData.push_back(NULL);

    // for crop
    enqueParams.mvP1SrcCrop.push_back(pCropInfo->crop_p1_sensor);
    enqueParams.mvP1Dst.push_back(pCropInfo->dstsize_resizer);
    enqueParams.mvP1DstCrop.push_back(pCropInfo->crop_dma);
    #endif

    MY_LOGDO("p2 enque count:%d, size[in/out]:%zu/%zu",
               muEnqueCnt, enqueParams.mvIn.size(), enqueParams.mvOut.size());
    // add request to queue
    {
        Mutex::Autolock _l(mLock);
        mvRunning.push_back(pRequest);
#if P2_DEBUG_DEQUE
        mvParams.push_back(enqueParams);
#endif
        muEnqueCnt++;
    }

    {
        MY_LOGDO_IF(mbEnableLog, "p2 enque +");
        CAM_TRACE_NAME("P2:Driver:enque");
        if (!mpPipe->enque(enqueParams)) {
            MY_LOGEO("p2 enque failed");
            // remove job from queue
            {
                Mutex::Autolock _l(mLock);
                vector<sp<Request>>::iterator iter = mvRunning.end();
                while (iter != mvRunning.begin()) {
                    iter--;
                    if (*iter == pRequest) {
                        mvRunning.erase(iter);
                        break;
                    }
                }

                MY_LOGEO("p2 deque count:%d, enque failed", muDequeCnt);
                muDequeCnt++;
                AEE_ASSERT("\nCRDISPATCH_KEY:MtkCam/P2Node:ISP pass2 deque fail");
            }
            return UNKNOWN_ERROR;
        }
        MY_LOGDO_IF(mbEnableLog, "p2 enque -");
    }

    FUNC_END;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
onP2Finish(
        FrameParams const &params,
        MBOOL const success)
{
    CAM_TRACE_NAME("P2:onP2Finish");
    //params.in.mHandle->updateState(BufferHandle::Buffer_ReadDone);
    for (size_t i = 0; i < params.vOut.size(); i++)
        if (params.vOut[i].mHandle.get())
            params.vOut[i].mHandle->updateState(
                    success ? BufferHandle::STATE_WRITE_OK : BufferHandle::STATE_WRITE_FAIL
            );

    if (muDumpBuffer) {
        MY_LOGDO("[YUV] frame(%d) size(%zu)", params.uFrameNo, params.vOut.size());
        sp<IImageBuffer> pImgBuf = NULL;
        char filename[256] = {0};
        char portname[10] = {0};
        char formatname[10] = {0};
        MUINT32 strides[3] = {0};
        if (!NSCam::Utils::makePath(P2_DEBUG_DUMP_PATH, 0660))
            MY_LOGWO("makePath[%s] fails", P2_DEBUG_DUMP_PATH);

        // dump condition
        MBOOL cond = (!params.bResized && muDumpCondIn & DEBUG_PORT_IN_IMGO) ||
                     (params.bResized && muDumpCondIn & DEBUG_PORT_IN_RRZO);

        // select output buffer if condition matched
        if (cond) {
#define IMAGE_FORMAT_TO_STRING(e)                         \
            (e == eImgFmt_BAYER10) ? "bayer10" :        \
            (e == eImgFmt_FG_BAYER10) ? "fg_bayer10" :  \
            (e == eImgFmt_YV12) ? "yv12" :              \
            (e == eImgFmt_NV21) ? "nv21" :              \
            (e == eImgFmt_YUY2) ? "yuy2" :              \
            (e == eImgFmt_I420) ? "i420" :              \
            "undef"

#define SENSOR_FORMAT_TO_STRING(e)                        \
            (e == SENSOR_FORMAT_ORDER_RAW_B)  ? "b" :   \
            (e == SENSOR_FORMAT_ORDER_RAW_Gb) ? "gb" :  \
            (e == SENSOR_FORMAT_ORDER_RAW_Gr) ? "gr" :  \
            (e == SENSOR_FORMAT_ORDER_RAW_R)  ? "r" :   \
            (e == eImgFmt_YUY2) ? "undef" :             \
            "undef"

            //input
#if SUPPORT_MNR
            // Don't dump the input of mnr tuning. It's dumped in previous run.
            if (!params.bRunMnr)
#endif
            if ((!params.bResized && muDumpPortIn & DEBUG_PORT_IN_IMGO) ||
                (params.bResized && muDumpPortIn & DEBUG_PORT_IN_RRZO)) {
                sprintf(portname, "%s", params.bResized ? "rrzo" : "imgo");
                pImgBuf = params.in.mHandle->getBuffer();
                for (size_t i = 0; i < 3; i++)
                    strides[i] = pImgBuf->getPlaneCount() > i ? pImgBuf->getBufStridesInBytes(i) : 0;
                {
                    sprintf(filename, P2_DEBUG_DUMP_PATH "/p2-%09d-%04d-%04d-%s-%dx%d-%d_%d_%d-%s-%s.raw",
                            params.uUniqueKey, params.uFrameNo, params.uRequestNo,
                            portname,
                            pImgBuf->getImgSize().w,
                            pImgBuf->getImgSize().h,
                            strides[0], strides[1], strides[2],
                            IMAGE_FORMAT_TO_STRING(pImgBuf->getImgFormat()),
                            SENSOR_FORMAT_TO_STRING(muSensorFormatOrder));

                    MY_LOGDO("[raw][in] frame(%d) port(%s) (%dx%d) fmt(0x%x) stride(%d, %d, %d)",
                            params.uFrameNo, portname,
                            pImgBuf->getImgSize().w, pImgBuf->getImgSize().h,
                            pImgBuf->getImgFormat(),
                            strides[0], strides[1], strides[2]);

                    pImgBuf->saveToFile(filename);
                }
            }
#undef  SENSOR_FORMAT_TO_STRING
            sprintf(formatname, "%s", "yuv");
            //output
            for (size_t i = 0; i < params.vOut.size(); i++) {
                if (muDumpPortOut & DEBUG_PORT_OUT_WDMAO &&
                    params.vOut[i].mPortId == PORT_WDMAO)
                    sprintf(portname, "%s", "wdmao");
                else if (muDumpPortOut & DEBUG_PORT_OUT_WROTO &&
                         params.vOut[i].mPortId == PORT_WROTO)
                    sprintf(portname, "%s", "wroto");
                else if (muDumpPortOut & DEBUG_PORT_OUT_IMG2O &&
                         params.vOut[i].mPortId == PORT_IMG2O)
                    sprintf(portname, "%s", "img2o");
#if SUPPORT_IMG3O_PORT
                else if (muDumpPortOut & DEBUG_PORT_OUT_IMG3O &&
                         params.vOut[i].mPortId == PORT_IMG3O)
                    sprintf(portname, "%s", "img3o");
#endif
#if SUPPORT_MFBO_PORT
                else if (muDumpPortOut & DEBUG_PORT_OUT_MFBO &&
                         params.vOut[i].mPortId == PORT_MFBO)
                {
                    sprintf(portname, "%s", "mfbo");
                    sprintf(formatname, "%s", "raw");
                }
#endif

                else
                    continue;

                pImgBuf = params.vOut[i].mHandle->getBuffer();
                for (size_t i = 0; i < 3; i++)
                    strides[i] = pImgBuf->getPlaneCount() > i ? pImgBuf->getBufStridesInBytes(i) : 0;
                {
                    sprintf(filename, P2_DEBUG_DUMP_PATH "/p2-%09d-%04d-%04d-%s-%dx%d-%d_%d_%d-%s.%s",
                            params.uUniqueKey, params.uFrameNo, params.uRequestNo,
                            portname,
                            pImgBuf->getImgSize().w,
                            pImgBuf->getImgSize().h,
                            strides[0], strides[1], strides[2],
                            IMAGE_FORMAT_TO_STRING(pImgBuf->getImgFormat()),
                            formatname);

                    MY_LOGDO("[yuv][out] frame(%d) port(%s) size(%dx%d) fmt(0x%x) strd(%d, %d, %d)",
                            params.uFrameNo, portname,
                            pImgBuf->getImgSize().w, pImgBuf->getImgSize().h,
                            pImgBuf->getImgFormat(),
                            strides[0], strides[1], strides[2]);

                    pImgBuf->saveToFile(filename);
                }
            }
#undef  IMAGE_FORMAT_TO_STRING
        }
    }

    if (params.outApp.get())
        params.outApp->updateState(success ? MetaHandle::STATE_WRITE_OK : MetaHandle::STATE_WRITE_FAIL);
    if (params.outHal.get())
        params.outHal->updateState(success ? MetaHandle::STATE_WRITE_OK : MetaHandle::STATE_WRITE_FAIL);

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
onP2Flush() {
    CAM_TRACE_NAME("P2:onP2Flush");
    if (mpMultiFrameHandler) {
        mpMultiFrameHandler->flush();
    }
    return;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
onP2Notify(
        MUINT32 const event,
        MINTPTR const arg1,
        MINTPTR const arg2,
        MINTPTR const arg3)
{
    switch (event) {
        case eP2_START_VENC_STREAM:
            if (mpPipe->sendCommand(
                    ESDCmd_CONFIG_VENC_DIRLK,
                    arg1, arg2, arg3))
                mConfigVencStream = MTRUE;
            else
                MY_LOGEO("Start venc stream failed");
            break;
        case eP2_STOP_VENC_STREAM:
            if (mpPipe->sendCommand(
                    ESDCmd_RELEASE_VENC_DIRLK))
                mConfigVencStream = MFALSE;
            else
                MY_LOGEO("Stop venc stream failed");
        break;
    }
    return;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
pass2CbFunc(QParams &rParams) {
    //MY_LOGD_IF(mbEnableLog, "pass2CbFunc +++");
    P2Procedure *pProcedure = reinterpret_cast<P2Procedure *>(rParams.mpCookie);
    pProcedure->handleDeque(rParams);
    //MY_LOGD_IF(mbEnableLog, "pass2CbFunc ---");
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
handleDeque(QParams &rParams) {
    CAM_TRACE_NAME("P2:handleDeque");
    Mutex::Autolock _l(mLock);
    sp<Request> pRequest = NULL;
    {
        MY_LOGDO("p2 deque count:%d, result:%d", muDequeCnt, rParams.mDequeSuccess);
        pRequest = mvRunning.front();
        mvRunning.erase(mvRunning.begin());
        muDequeCnt++;

        if (mDebugScanLineMask != 0 && mpDebugScanLine != NULL) {
            for (size_t i = 0; i < rParams.mvOut.size(); i++) {
                if ((rParams.mvOut[i].mPortID.index == PORT_WDMAO.index &&
                     mDebugScanLineMask & DEBUG_DRAWLINE_PORT_WDMAO) ||
                    (rParams.mvOut[i].mPortID.index == PORT_WROTO.index &&
                     mDebugScanLineMask & DEBUG_DRAWLINE_PORT_WROTO) ||
                    (rParams.mvOut[i].mPortID.index == PORT_IMG2O.index &&
                     mDebugScanLineMask & DEBUG_DRAWLINE_PORT_IMG2O))
                {
                    mpDebugScanLine->drawScanLine(
                            rParams.mvOut[i].mBuffer->getImgSize().w,
                            rParams.mvOut[i].mBuffer->getImgSize().h,
                            (void *) (rParams.mvOut[i].mBuffer->getBufVA(0)),
                            rParams.mvOut[i].mBuffer->getBufSizeInBytes(0),
                            rParams.mvOut[i].mBuffer->getBufStridesInBytes(0));
                }
            }
        }

#if P2_DEBUG_DEQUE
        if (mvParams.size()) {
            QParams checkParam;
            checkParam = mvParams.front();
            mvParams.erase(mvParams.begin());

            // make sure params are correct
#define ERROR_IF_NOT_MATCHED(item, i, expected, actual) do{             \
                if( expected != actual)                                             \
                    MY_LOGEO("%s %d: expected %p != %p", item, i, expected, actual); \
            } while(0)

            for (size_t i = 0; i < checkParam.mvIn.size(); i++) {
                if (i > rParams.mvIn.size()) {
                    MY_LOGEO("no src in dequed Params");
                    break;
                }

                ERROR_IF_NOT_MATCHED("src pa of in", i,
                                     checkParam.mvIn[i].mBuffer->getBufPA(0),
                                     rParams.mvIn[i].mBuffer->getBufPA(0)
                );
                ERROR_IF_NOT_MATCHED("src va of in", i,
                                     checkParam.mvIn[i].mBuffer->getBufVA(0),
                                     rParams.mvIn[i].mBuffer->getBufVA(0)
                );
            }

            for (size_t i = 0; i < checkParam.mvOut.size(); i++) {
                if (i > rParams.mvOut.size()) {
                    MY_LOGEO("no enough dst in dequed Params, %d", i);
                    break;
                }

                ERROR_IF_NOT_MATCHED("dst pa of out", i,
                                     checkParam.mvOut[i].mBuffer->getBufPA(0),
                                     rParams.mvOut[i].mBuffer->getBufPA(0)
                );
                ERROR_IF_NOT_MATCHED("dst va of out", i,
                                     checkParam.mvOut[i].mBuffer->getBufVA(0),
                                     rParams.mvOut[i].mBuffer->getBufVA(0)
                );
            }

#undef ERROR_IF_NOT_MATCHED
        }
        else {
            MY_LOGWO("params size not matched");
        }
#endif
    }

    if (rParams.mvTuningData.size() > 0) {
        void *pTuning = rParams.mvTuningData[0];
        if (pTuning) {
            free(pTuning);
        }
    }

    pRequest->responseDone(rParams.mDequeSuccess ? OK : UNKNOWN_ERROR);

    mCondJob.signal();
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
checkParams(FrameParams const params) const {
#define CHECK(val, fail_ret, ...) \
    do{                           \
        if( !(val) )              \
        {                         \
            MY_LOGEO(__VA_ARGS__); \
            return fail_ret;      \
        }                         \
    } while(0)

    CHECK(params.in.mHandle.get(), BAD_VALUE, "no src handle");
    CHECK(params.vOut.size(), BAD_VALUE, "no dst");
    CHECK(params.inApp.get(), BAD_VALUE, "no in app meta");
    CHECK(params.inHal.get(), BAD_VALUE, "no in hal meta");
#undef CHECK
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
getCropInfo(
        IMetadata *const inApp,
        IMetadata *const inHal,
        MBOOL const isResized,
        Cropper::CropInfo &cropInfo) const
{
    if (!tryGetMetadata<MSize>(inHal, MTK_HAL_REQUEST_SENSOR_SIZE, cropInfo.sensor_size)) {
        MY_LOGEO("cannot get MTK_HAL_REQUEST_SENSOR_SIZE");
        return BAD_VALUE;
    }

    MSize const sensor = cropInfo.sensor_size;
    MSize const active = mCreateParams.activeArray.s;

    cropInfo.isResized = isResized;
    // get current p1 buffer crop status
    if (!(tryGetMetadata<MRect>(inHal, MTK_P1NODE_SCALAR_CROP_REGION, cropInfo.crop_p1_sensor) &&
          tryGetMetadata<MSize>(inHal, MTK_P1NODE_RESIZER_SIZE, cropInfo.dstsize_resizer) &&
          tryGetMetadata<MRect>(inHal, MTK_P1NODE_DMA_CROP_REGION, cropInfo.crop_dma)))
    {
        MY_LOGWO_IF(1, "[FIXME] should sync with p1 for rrz setting");

        cropInfo.crop_p1_sensor = MRect(MPoint(0, 0), sensor);
        cropInfo.dstsize_resizer = sensor;
        cropInfo.crop_dma = MRect(MPoint(0, 0), sensor);
    }

    MY_LOGDO_IF(P2_DEBUG_LOG, "SCALAR_CROP_REGION:(%d,%d)(%dx%d) RESIZER_SIZE:(%dx%d) DMA_CROP_REGION:(%d,%d)(%dx%d)",
               cropInfo.crop_p1_sensor.p.x, cropInfo.crop_p1_sensor.p.y,
               cropInfo.crop_p1_sensor.s.w, cropInfo.crop_p1_sensor.s.h,
               cropInfo.dstsize_resizer.w, cropInfo.dstsize_resizer.h,
               cropInfo.crop_dma.p.x, cropInfo.crop_dma.p.y,
               cropInfo.crop_dma.s.w, cropInfo.crop_dma.s.h);

    MINT32 sensorMode;
    if (!tryGetMetadata<MINT32>(inHal, MTK_P1NODE_SENSOR_MODE, sensorMode)) {
        MY_LOGEO("cannot get MTK_P1NODE_SENSOR_MODE");
        return BAD_VALUE;
    }

    HwTransHelper hwTransHelper(muOpenId);
    HwMatrix matToActive;
    if (!hwTransHelper.getMatrixToActive(sensorMode, cropInfo.matSensor2Active) ||
        !hwTransHelper.getMatrixFromActive(sensorMode, cropInfo.matActive2Sensor)) {
        MY_LOGEO("get matrix fail");
        return UNKNOWN_ERROR;
    }

    cropInfo.tranSensor2Resized = simpleTransform(
            cropInfo.crop_p1_sensor.p,
            cropInfo.crop_p1_sensor.s,
            cropInfo.dstsize_resizer
    );

    MBOOL const isEisOn = isEISOn(inApp);

    MRect cropRegion; //active array domain
    queryCropRegion(inApp, isEisOn, cropRegion);
    cropInfo.crop_a = cropRegion;

    // query EIS result
    {
        eis_region eisInfo;
        if (isEisOn && queryEisRegion(inHal, eisInfo)) {
            cropInfo.isEisEabled = MTRUE;
            // calculate mv
            vector_f *pMv_s = &cropInfo.eis_mv_s;
            vector_f *pMv_r = &cropInfo.eis_mv_r;
            MBOOL isResizedDomain = MTRUE;
#if 0
            //eis in sensor domain
            isResizedDomain = MFALSE;
            pMv_s->p.x  = eisInfo.x_int - (sensor.w * (EIS_FACTOR-100)/2/EIS_FACTOR);
            pMv_s->pf.x = eisInfo.x_float;
            pMv_s->p.y  = eisInfo.y_int - (sensor.h * (EIS_FACTOR-100)/2/EIS_FACTOR);
            pMv_s->pf.y = eisInfo.y_float;

            cropInfo.eis_mv_r = transform(cropInfo.tranSensor2Resized, cropInfo.eis_mv_s);
#else
            MSize const resizer = cropInfo.dstsize_resizer;

#if SUPPORT_EIS_MV
            if (eisInfo.is_from_zzr)
            {
                pMv_r->p.x  = eisInfo.x_mv_int;
                pMv_r->pf.x = eisInfo.x_mv_float;
                pMv_r->p.y  = eisInfo.y_mv_int;
                pMv_r->pf.y = eisInfo.y_mv_float;
                cropInfo.eis_mv_s = inv_transform(cropInfo.tranSensor2Resized, cropInfo.eis_mv_r);
            }
            else
            {
                isResizedDomain = MFALSE;
                pMv_s->p.x  = eisInfo.x_mv_int;
                pMv_s->pf.x = eisInfo.x_mv_float;
                pMv_s->p.y  = eisInfo.y_mv_int;
                pMv_s->pf.y = eisInfo.y_mv_float;
                cropInfo.eis_mv_r = transform(cropInfo.tranSensor2Resized, cropInfo.eis_mv_s);
            }
#else
            //eis in resized domain
            pMv_r->p.x = eisInfo.x_int - (resizer.w * (EIS_FACTOR - 100) / 2 / EIS_FACTOR);
            pMv_r->pf.x = eisInfo.x_float;
            pMv_r->p.y = eisInfo.y_int - (resizer.h * (EIS_FACTOR - 100) / 2 / EIS_FACTOR);
            pMv_r->pf.y = eisInfo.y_float;
            cropInfo.eis_mv_s = inv_transform(cropInfo.tranSensor2Resized, cropInfo.eis_mv_r);
#endif
            MY_LOGDO_IF(P2_DEBUG_LOG, "mv (%s): (%d, %d, %d, %d) -> (%d, %d, %d, %d)",
                       isResizedDomain ? "r->s" : "s->r",
                       pMv_r->p.x,
                       pMv_r->pf.x,
                       pMv_r->p.y,
                       pMv_r->pf.y,
                       pMv_s->p.x,
                       pMv_s->pf.x,
                       pMv_s->p.y,
                       pMv_s->pf.y
            );
#endif
            // cropInfo.eis_mv_a = inv_transform(cropInfo.tranActive2Sensor, cropInfo.eis_mv_s);
            cropInfo.matSensor2Active.transform(cropInfo.eis_mv_s.p, cropInfo.eis_mv_a.p);
            // FIXME: float
            //cropInfo.matSensor2Active.transform(cropInfo.eis_mv_s.pf,cropInfo.eis_mv_a.pf);

            MY_LOGDO_IF(P2_DEBUG_LOG, "mv in active %d/%d, %d/%d",
                       cropInfo.eis_mv_a.p.x,
                       cropInfo.eis_mv_a.pf.x,
                       cropInfo.eis_mv_a.p.y,
                       cropInfo.eis_mv_a.pf.y
            );
        }
        else {
            cropInfo.isEisEabled = MFALSE;
            // no need to set 0
            //memset(&cropInfo.eis_mv_a, 0, sizeof(vector_f));
            //memset(&cropInfo.eis_mv_s, 0, sizeof(vector_f));
            //memset(&cropInfo.eis_mv_r, 0, sizeof(vector_f));
        }
    }

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
queryCropRegion(
        IMetadata *const meta_request,
        MBOOL const isEisOn,
        MRect &cropRegion) const
{
    if (!tryGetMetadata<MRect>(meta_request, MTK_SCALER_CROP_REGION, cropRegion)) {
        cropRegion.p = MPoint(0, 0);
        cropRegion.s = mCreateParams.activeArray.s;
        MY_LOGWO_IF(mbEnableLog, "no MTK_SCALER_CROP_REGION, crop full size %dx%d",
                   cropRegion.s.w, cropRegion.s.h);
    }
    MY_LOGDO_IF(mbEnableLog, "control: cropRegion(%d, %d, %dx%d)",
               cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h);

#if SUPPORT_EIS
    if (isEisOn) {
        cropRegion.p.x += (cropRegion.s.w * (EIS_FACTOR - 100) / 2 / EIS_FACTOR);
        cropRegion.p.y += (cropRegion.s.h * (EIS_FACTOR - 100) / 2 / EIS_FACTOR);
        cropRegion.s = cropRegion.s * 100 / EIS_FACTOR;
        MY_LOGDO_IF(mbEnableLog, "EIS: factor %d, cropRegion(%d, %d, %dx%d)",
                   EIS_FACTOR, cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h);
    }
#endif
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::
updateCropRegion(
        MRect const crop,
        IMetadata *meta_result) const
{
    trySetMetadata<MRect>(meta_result, MTK_SCALER_CROP_REGION, crop);
    MY_LOGDO_IF(P2_DEBUG_LOG && mbEnableLog, "result: cropRegion (%d, %d, %dx%d)",
               crop.p.x, crop.p.y, crop.s.w, crop.s.h);
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2Procedure::
isEISOn(IMetadata *const inApp) const {
    MUINT8 eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    if (!tryGetMetadata<MUINT8>(inApp, MTK_CONTROL_VIDEO_STABILIZATION_MODE, eisMode)) {
        MY_LOGWO_IF(mbEnableLog, "no MTK_CONTROL_VIDEO_STABILIZATION_MODE");
    }
#if FORCE_EIS_ON
    eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
#endif
    return eisMode == MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P2Procedure::
queryEisRegion(
        IMetadata *const inHal,
        eis_region &region
) const {
    IMetadata::IEntry entry = inHal->entryFor(MTK_EIS_REGION);

#if SUPPORT_EIS_MV
    // get EIS's motion vector
    if (entry.count() > 8)
    {
        MINT32 x_mv         = entry.itemAt(6, Type2Type<MINT32>());
        MINT32 y_mv         = entry.itemAt(7, Type2Type<MINT32>());
        region.is_from_zzr  = entry.itemAt(8, Type2Type<MINT32>());
        MBOOL x_mv_negative = x_mv >> 31;
        MBOOL y_mv_negative = y_mv >> 31;
        // convert to positive for getting parts of int and float if negative
        if (x_mv_negative) x_mv = ~x_mv + 1;
        if (y_mv_negative) y_mv = ~y_mv + 1;

        region.x_mv_int   = (x_mv & (~0xFF)) >> 8;
        region.x_mv_float = (x_mv & (0xFF)) << 31;
        if(x_mv_negative){
            region.x_mv_int   = ~region.x_mv_int + 1;
            region.x_mv_float = ~region.x_mv_float + 1;
        }

        region.y_mv_int   = (y_mv& (~0xFF)) >> 8;
        region.y_mv_float = (y_mv& (0xFF)) << 31;
        if(y_mv_negative){
            region.y_mv_int   = ~region.y_mv_int + 1;
            region.y_mv_float = ~region.x_mv_float + 1;
        }

        MY_LOGDO_IF(mbEnableLog, "EIS MV:%d, %d, %d",
                        region.s.w,
                        region.s.h,
                        region.is_from_zzr);
     }
#endif

    // get EIS's region
    if (entry.count() > 5) {
        region.x_int = entry.itemAt(0, Type2Type<MINT32>());
        region.x_float = entry.itemAt(1, Type2Type<MINT32>());
        region.y_int = entry.itemAt(2, Type2Type<MINT32>());
        region.y_float = entry.itemAt(3, Type2Type<MINT32>());
        region.s.w = entry.itemAt(4, Type2Type<MINT32>());
        region.s.h = entry.itemAt(5, Type2Type<MINT32>());

        MY_LOGDO_IF(mbEnableLog, "EIS Region: %d, %d, %d, %d, %dx%d",
                   region.x_int,
                   region.x_float,
                   region.y_int,
                   region.y_float,
                   region.s.w,
                   region.s.h);
        return MTRUE;
    }

    MY_LOGWO("wrong eis region count %d", entry.count());
    return MFALSE;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::
onP2PullParams(
        sp<Request> pRequest,
        FrameParams &param_p2)
{
    param_p2.bBurstEnque = MFALSE;
    if (pRequest->context.burst_num > 1) {
        muRequestCnt++;
        if (pRequest->context.burst_num > 1 && muRequestCnt % pRequest->context.burst_num == 0) {
            param_p2.bBurstEnque = MTRUE;
            MY_LOGD("[burst] counter: %d - meet the condition of enque package", muRequestCnt);
        } else {
            MY_LOGD("[burst] counter: %d", muRequestCnt);
        }
    }
#if SUPPORT_MNR || SUPPORT_SWNR || SUPPORT_PLUGIN
    MBOOL bOutWorkingBuffer = MFALSE;
#endif

    param_p2.uUniqueKey = pRequest->getUniqueKey();
    param_p2.uRequestNo = pRequest->getRequestNo();
    param_p2.uFrameNo = pRequest->getFrameNo();

    // input buffer
    param_p2.in.mPortId = PORT_IMGI;
    param_p2.in_lcso.mPortId = PORT_LCEI;
    param_p2.in_lcso.mHandle = pRequest->context.in_lcso_buffer;
    pRequest->context.in_lcso_buffer.clear();

    param_p2.bResized = pRequest->context.resized;
    param_p2.bYuvReproc = pRequest->context.is_yuv_reproc;
#if SUPPORT_SWNR
    bOutWorkingBuffer |= pRequest->context.nr_type == Request::NR_TYPE_SWNR;
#endif
#if SUPPORT_PLUGIN
    bOutWorkingBuffer |= pRequest->context.run_plugin_yuv;
#endif
#if SUPPORT_MNR
    // input buffer(select the source)
    MBOOL bMnrRequest = pRequest->context.nr_type == Request::NR_TYPE_MNR;
    MBOOL bMnrSecondRun = bMnrRequest && pRequest->context.work_buffer.get();
    param_p2.bRunMnr = bMnrSecondRun;
    // output to a working buffer, and set two-run pass2
    if (bMnrRequest && !bMnrSecondRun) {
        pRequest->setReentry(MTRUE);
        bOutWorkingBuffer = MTRUE;
    }
    // set the privous working buffer as input buffer
    if (bMnrSecondRun) {
        MY_LOGDO_IF(mbEnableLog, "input from working buffer");
        param_p2.in.mHandle = pRequest->context.work_buffer;
        pRequest->context.work_buffer.clear();
    }
    else
#endif
    {
        param_p2.in.mHandle = pRequest->context.in_buffer;
        pRequest->context.in_buffer.clear();
    }

    // output buffer
    MUINT8 occupied = 0;
    MBOOL remains = MFALSE;

#if SUPPORT_MNR || SUPPORT_SWNR || SUPPORT_PLUGIN
    param_p2.bRunWorkBuffer = bOutWorkingBuffer;

    if (bOutWorkingBuffer) {
        MY_LOGDO_IF(mbEnableLog, "output to working buffer");
        MERROR ret = OK;
        if (OK != (ret = param_p2.in.mHandle->waitState(BufferHandle::STATE_READABLE))) {
            MY_LOGWO("input buffer err = %d", ret);
            return ret;
        }

        IImageBuffer *pInImageBuffer = param_p2.in.mHandle->getBuffer();
        if (pInImageBuffer == NULL) {
            MY_LOGEO("no input buffer");
            return UNKNOWN_ERROR;
        }
        sp<BufferHandle> pBufferHandle = NULL;
        if (pRequest->context.work_buffer_format == 0)
            pBufferHandle = WorkingBufferHandle::create(
                    (pRequest->context.nr_type == Request::NR_TYPE_SWNR) ? "SWNR" : "MNR",
                    (pRequest->context.nr_type == Request::NR_TYPE_SWNR) ? eImgFmt_I420 : eImgFmt_YUY2,
                    pInImageBuffer->getImgSize());
        else
            pBufferHandle = WorkingBufferHandle::create(
                    "PG",
                    pRequest->context.work_buffer_format,
                    pInImageBuffer->getImgSize());

        if (pBufferHandle.get()) {
            // occupied WDMA0 and WROTO, except IMGO2
            occupied |= 0x3;

            pRequest->context.work_buffer = pBufferHandle;
            pRequest->context.in_mdp_crop = MTRUE;
            P2Procedure::FrameOutput out;
            out.mPortId = PORT_WDMAO;
            out.mHandle = pBufferHandle;
            out.mTransform = 0;
            param_p2.vOut.push_back(out);
        }
    }
#endif

    sp<BufferHandle> pCopyCandidate = NULL;
    vector<sp<BufferHandle>>::iterator iter = pRequest->context.out_buffers.begin();
    for (; iter != pRequest->context.out_buffers.end(); iter++) {
        sp<BufferHandle> pOutBuffer = *iter;
        if (!pOutBuffer.get())
            continue;

        StreamId_T const streamId = pOutBuffer->getStreamId();
        MUINT32 const transform = pOutBuffer->getTransform();
        MUINT32 const usage = pOutBuffer->getUsage();

        PortID port_p2;
        MBOOL isFdStream = streamId == pRequest->context.fd_stream_id;
        if (OK == mapPortId(streamId, transform, isFdStream, occupied, port_p2)) {
            P2Procedure::FrameOutput out;
            // assign to port of VENC direct link
            if (pRequest->context.enable_venc_stream &&
                usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)
                out.mPortId = PORT_VENC_STREAMO;
            else
                out.mPortId = port_p2;

            if (pCopyCandidate == NULL && transform == 0) {
                pCopyCandidate = pOutBuffer;
            }
            out.mHandle = pOutBuffer;
            out.mTransform = transform;
            out.mUsage = usage;

            param_p2.vOut.push_back(out);
            (*iter).clear();
        }
        else
            remains = MTRUE;
    }

    if (param_p2.vOut.size() == 0) {
        if (param_p2.bBurstEnque) {
            mpMultiFrameHandler->enque();
            MY_LOGD("no-buffer frame triggers multi-frame enque");
        }
        return UNKNOWN_ERROR;
    }
    else if (pRequest->isReentry()) {
        // skip to determine mdp's input buffer if need to re-entry
    }
    else if (remains) {
        if (pCopyCandidate == NULL) {
            MY_LOGW("no candidate buffer for copying");
            pRequest->context.in_mdp_buffer = param_p2.vOut[param_p2.vOut.size() - 1].mHandle;
        } else {
            pRequest->context.in_mdp_buffer = pCopyCandidate;
        }

    }

#if SUPPORT_IMG3O_PORT
    if (muDumpPortImg3o) {
        MY_LOGDO("output img3o");
        MERROR ret = OK;
        if (OK != (ret = param_p2.in.mHandle->waitState(BufferHandle::STATE_READABLE))) {
            MY_LOGWO("input buffer err = %d", ret);
            return ret;
        }

        IImageBuffer *pInImageBuffer = param_p2.in.mHandle->getBuffer();
        if (pInImageBuffer == NULL) {
            MY_LOGEO("no input buffer");
            return UNKNOWN_ERROR;
        }

        // format: YUY2
        MUINT32 stridesInBytes[3] = {static_cast<MUINT32>(pInImageBuffer->getImgSize().w << 1), 0, 0};

        sp<BufferHandle> pBufferHandle = WorkingBufferHandle::create(
                "IMG3O_WB",
                eImgFmt_YUY2,
                pInImageBuffer->getImgSize());

        if (pBufferHandle.get()) {
            P2Procedure::FrameOutput out;
            out.mPortId = PORT_IMG3O;
            out.mHandle = pBufferHandle;
            out.mTransform = 0;
            param_p2.vOut.push_back(out);
        }
    }
#endif
#if SUPPORT_MFBO_PORT
    if (muDumpPortMfbo) {
        MY_LOGDO("output mfbo");
        MERROR ret = OK;
        if (OK != (ret = param_p2.in.mHandle->waitState(BufferHandle::STATE_READABLE))) {
            MY_LOGWO("input buffer err = %d", ret);
            return ret;
        }

        IImageBuffer *pInImageBuffer = param_p2.in.mHandle->getBuffer();
        if (pInImageBuffer == NULL) {
            MY_LOGEO("no input buffer");
            return UNKNOWN_ERROR;
        }

        sp<BufferHandle> pBufferHandle = WorkingBufferHandle::create(
                "MFBO_WB",
                pInImageBuffer->getImgFormat(),
                pInImageBuffer->getImgSize());

        if (pBufferHandle.get()) {
            P2Procedure::FrameOutput out;
            out.mPortId = PORT_MFBO;
            out.mHandle = pBufferHandle;
            out.mTransform = 0;
            param_p2.vOut.push_back(out);
        }
    }
#endif

    param_p2.inApp = pRequest->context.in_app_meta;
    param_p2.inHal = pRequest->context.in_hal_meta;
    param_p2.outApp = pRequest->context.out_app_meta;
    param_p2.outHal = pRequest->context.out_hal_meta;

    // pull meta buffers if not re-entry
    if (!pRequest->isReentry()) {
        // let FrameLifeHolder release the buffer
        //pRequest->context.in_app_meta.clear();
        //pRequest->context.in_hal_meta.clear();
        pRequest->context.out_app_meta.clear();
#if SUPPORT_SWNR
        // postpone to release output hal metadata
        if (pRequest->context.nr_type != Request::NR_TYPE_SWNR)
#endif
        pRequest->context.out_hal_meta.clear();
    }

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::MultiFrameHandler::
collect(sp<Request> pRequest, QParams &params, MBOOL bForceEnque) {
    MINT32 index = mvReqCollecting.size();
    // tag
    for (size_t i = 0; i < params.mvStreamTag.size(); i++) {
        mParamCollecting.mvStreamTag.push_back(params.mvStreamTag[i]);
    }
    // truning
    for (size_t i = 0; i < params.mvTuningData.size(); i++) {
        mParamCollecting.mvTuningData.push_back(params.mvTuningData[i]);
    }
    // input
    for (size_t i = 0; i < params.mvIn.size(); i++) {
        params.mvIn.editItemAt(i).mPortID.group = index;
        mParamCollecting.mvIn.push_back(params.mvIn[i]);
    }
    // output
    for (size_t i = 0; i < params.mvOut.size(); i++) {
        params.mvOut.editItemAt(i).mPortID.group = index;
        mParamCollecting.mvOut.push_back(params.mvOut[i]);
    }
    // crop
    for (size_t i = 0; i < params.mvCropRsInfo.size(); i++) {
        params.mvCropRsInfo.editItemAt(i).mFrameGroup = index;
        mParamCollecting.mvCropRsInfo.push_back(params.mvCropRsInfo[i]);
    }
    // module
    for (size_t i = 0; i < params.mvModuleData.size(); i++) {
        params.mvModuleData.editItemAt(i).frameGroup = index;
        mParamCollecting.mvModuleData.push_back(params.mvModuleData[i]);
    }

    mvReqCollecting.push_back(pRequest);

    if (bForceEnque || mvReqCollecting.size() >= pRequest->context.burst_num) {
        enque();
    }
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
P2Procedure::MultiFrameHandler::
enque() {
    if (!mvReqCollecting.size()) {
        return OK;
    }

    QParams enqueParams = mParamCollecting;
    // callback
    enqueParams.mpfnCallback = callback;
    enqueParams.mpCookie = this;

    {
        // push the collected requests
        {
            Mutex::Autolock _l(mLock);
            mvRunning.push_back(mvReqCollecting);
        }
        MY_LOGD("[burst] p2 enque + count:%d, size[I/O]:%zu/%zu",
               muMfEnqueCnt, enqueParams.mvIn.size(), enqueParams.mvOut.size());
        muMfEnqueCnt++;
        CAM_TRACE_NAME("P2:burst:enque");
        if (!mpPipe->enque(enqueParams)) {
            MY_LOGE("[burst] p2 enque failed");
            // remove the collected requests from queue
            {
                Mutex::Autolock _l(mLock);
                mvRunning.erase(mvRunning.end() -1);
            }
            // response error
            vector<sp<Request>>::iterator iter = mvReqCollecting.begin();
            while (iter != mvReqCollecting.end()) {
                (*iter)->responseDone(UNKNOWN_ERROR);
                iter++;
            }

            MY_LOGE("[burst] p2 deque count:%d, enque failed", muMfDequeCnt);
            muMfDequeCnt++;
            AEE_ASSERT("\nCRDISPATCH_KEY:MtkCam/P2Node:ISP pass2 deque fail");
        }
        MY_LOGD("[burst] p2 enque -");
    }
    // clear the collected request
    mParamCollecting.mvStreamTag.clear();
    mParamCollecting.mvTuningData.clear();
    mParamCollecting.mvIn.clear();
    mParamCollecting.mvOut.clear();
    mParamCollecting.mvCropRsInfo.clear();
    mParamCollecting.mvModuleData.clear();
    mvReqCollecting.clear();
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::MultiFrameHandler::
deque(QParams &rParams) {
    CAM_TRACE_NAME("P2:Burst:deque");
    vector<sp<Request>> vpRequest;
    {
        Mutex::Autolock _l(mLock);
        MY_LOGD("[burst] p2 deque count:%d, result:%d", muMfDequeCnt, rParams.mDequeSuccess);
        if (mvRunning.size() == 0) {
            MY_LOGE("[burst] there is no running request");
            AEE_ASSERT("\nCRDISPATCH_KEY:MtkCam/P2Node:ISP pass2 enque/deque unmatched");
        }
        vpRequest = mvRunning.front();
        mvRunning.erase(mvRunning.begin());
        muMfDequeCnt++;
    }

    for (size_t i = 0; i < rParams.mvTuningData.size(); i++) {
        void *pTuning = rParams.mvTuningData[i];
        if (pTuning) {
            free(pTuning);
        }
    }

    vector<sp<Request>>::iterator iter = vpRequest.begin();
    while (iter != vpRequest.end()) {
        (*iter)->responseDone(rParams.mDequeSuccess ? OK : UNKNOWN_ERROR);
        (*iter).clear();
        iter++;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P2Procedure::MultiFrameHandler::
flush() {
    CAM_TRACE_NAME("P2:Burst:flush");
    FUNC_START;

    mParamCollecting = QParams();
    //mvReqCollecting.clear();
    vector<sp<Request>>::iterator iter = mvReqCollecting.begin();
    while (iter != mvReqCollecting.end()) {
        (*iter)->responseDone(UNKNOWN_ERROR);
        iter = mvReqCollecting.erase(iter);
    }

    FUNC_END;
    return;
}
