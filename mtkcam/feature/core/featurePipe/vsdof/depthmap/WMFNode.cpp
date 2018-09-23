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

#include "WMFNode.h"
#include "DepthMapPipe_Common.h"
#include "DepthMapPipeUtils.h"
#include "../util/vsdof_util.h"
#include "DataStorage.h"

#include <mtkcam/drv/iopipe/PostProc/DpeUtility.h>
#include <vsdof/hal/stereo_tuning_provider.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>

#define PIPE_CLASS_TAG "WMFNode"
#include <featurePipe/core/include/PipeLog.h>

using namespace NSCam::NSIoPipe::NSDpe;
using namespace VSDOF::util;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

WMFNode::BufferPoolSet::
BufferPoolSet()
{}

WMFNode::BufferPoolSet::
~BufferPoolSet()
{}


MBOOL
WMFNode::BufferPoolSet::
init(DepthFlowType flowType)
{
    MY_LOGD("+");
    // create the TBLI buffer pool
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    MUINT32 iWMFInputWidth = pSizeProvder->getBufferSize(E_MY_S, eSTEREO_SCENARIO_RECORD).size.w;

    MUINT32 iTbliStride = DPEQueryInDMAStride(DMA_WMFE_TBLI, WMFE_DPI_D_FMT, iWMFInputWidth);

    CREATE_IMGBUF_POOL( mpTbliImgBufPool, "TbliBufPool", MSize(iTbliStride, 1),
                        eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);
    ALLOCATE_BUFFER_POOL(mpTbliImgBufPool, 1);


    MSize szDMWSize = pSizeProvder->getBufferSize(E_DMW, eSTEREO_SCENARIO_RECORD).size;
    CREATE_IMGBUF_POOL(mpDMWImgBufPool, "InternalDMWBufPool", szDMWSize,
                        eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);
    // DMW + internal working buffer => 2 set + queued buffer
    if(flowType == eDEPTH_FLOW_TYPE_QUEUED_DEPTH) {
        ALLOCATE_BUFFER_POOL(mpDMWImgBufPool, VSDOF_WORKING_BUF_SET * 2 + DEPTH_FLOW_QUEUE_SIZE);}
    else{
        ALLOCATE_BUFFER_POOL(mpDMWImgBufPool, VSDOF_WORKING_BUF_SET * 2);}


    MY_LOGD("-");
    return MTRUE;
}

MBOOL
WMFNode::BufferPoolSet::
uninit()
{
    ImageBufferPool::destroy(mpTbliImgBufPool);
    ImageBufferPool::destroy(mpDMWImgBufPool);
    return MTRUE;
}


WMFNode::
WMFNode(const char* name, DepthFlowType flowType)
: DepthMapPipeNode(name, flowType)
{
    this->addWaitQueue(&mOCCInfoQueue);
}

WMFNode::
~WMFNode()
{
}

MVOID
WMFNode::
cleanUp()
{
    VSDOF_LOGD("+");
    if(mpDPEStream != nullptr)
    {
        mpDPEStream->uninit();
        mpDPEStream->destroyInstance("VSDOF_WMF");
        mpDPEStream = nullptr;
    }
    // JOB queue clear
    mOCCInfoQueue.clear();
    //
    mpTbliImgBuf = nullptr;
    // buf pool uninit
    mBufPoolSet.uninit();
    VSDOF_LOGD("-");
}

MBOOL
WMFNode::
onInit()
{
    CAM_TRACE_NAME("WMFNode::onInit");
    VSDOF_LOGD("+");
    // init DPEStream
    mpDPEStream = NSCam::NSIoPipe::NSDpe::IDpeStream::createInstance("VSDOF_WMF");
    if(mpDPEStream == NULL)
    {
        MY_LOGE("DPE Stream create instance failed!");
        return MFALSE;
    }
    MBOOL bRet = mpDPEStream->init();
    if(!bRet)
    {
        MY_LOGE("DPEStream init failed!!");
        return MFALSE;
    }
    // init the buffer pool set
    mBufPoolSet.init(mFlowType);
    // get Tbli tuning buffer
    mpTbliImgBuf = mBufPoolSet.mpTbliImgBufPool->request();
    // query tuning mgr to get Tbli
    StereoTuningProvider::getWMFTuningInfo(eWMFFilterSize, reinterpret_cast<void*>(mpTbliImgBuf->mImageBuffer->getBufVA(0)));
    mpTbliImgBuf->mImageBuffer->syncCache(eCACHECTRL_FLUSH);
    VSDOF_LOGD("-");
    return MTRUE;
}

MBOOL
WMFNode::
onUninit()
{
    CAM_TRACE_NAME("WMFNode::onUninit");
    VSDOF_LOGD("+");
    cleanUp();
    VSDOF_LOGD("-");
    return MTRUE;
}

MBOOL
WMFNode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
WMFNode::
onThreadStop()
{
    return MTRUE;
}

MBOOL
WMFNode::
onData(DataID data, ImgInfoMapPtr& pSmImgInfo)
{
    MBOOL ret=MTRUE;
    VSDOF_PRFLOG("+, dataID=%d reqId=%d", data, pSmImgInfo->getRequestPtr()->getRequestNo());

    switch(data)
    {
        case OCC_TO_WMF_DMH_MY_S:
            mOCCInfoQueue.enque(pSmImgInfo);
            break;
        default:
            ret = MFALSE;
            break;
    }

    VSDOF_LOGD("-");
    return ret;
}

MBOOL
WMFNode::
onThreadLoop()
{
    ImgInfoMapPtr pJobImgInfo;
    EffectRequestPtr reqPtr;

    if( !waitAnyQueue() )
    {
        return MFALSE;
    }
    CAM_TRACE_NAME("WMFNode::onThreadLoop");


    if( !mOCCInfoQueue.deque(pJobImgInfo) )
    {
        MY_LOGD("mOCCInfoQueue.deque() failed");
        return MFALSE;
    }
    return threadLoop_WMFFirstPass(pJobImgInfo);
}

const char*
WMFNode::
onDumpBIDToName(DepthMapBufferID BID)
{
#define MAKE_NAME_CASE(name) \
    case name: return #name;
    switch(BID)
    {
        MAKE_NAME_CASE(BID_WMF_OUT_DMW);
        MAKE_NAME_CASE(BID_OCC_OUT_MY_S);
        default:
            MY_LOGW("unknown BID:%d", BID);
            return "unknown";
    }

#undef MAKE_NAME_CASE
}


MVOID
WMFNode::
debugWMFParam(WMFEConfig &config)
{
    if(!mbDebugLog)
        return;

    #define LOG_BUFFER_INFO(buff_info)\
        MY_LOGD(#buff_info ".dmaport=%d", buff_info.dmaport);\
        MY_LOGD(#buff_info ".memID=%d", buff_info.memID);\
        MY_LOGD(#buff_info ".u4BufVA=%x", buff_info.u4BufVA);\
        MY_LOGD(#buff_info ".u4BufPA=%x", buff_info.u4BufPA);\
        MY_LOGD(#buff_info ".u4BufSize=%d", buff_info.u4BufSize);\
        MY_LOGD(#buff_info ".u4Stride=%d", buff_info.u4Stride);


    WMFECtrl ctrl = config.Wmfe_Ctrl_0;
    MY_LOGD("========== Wmfe_Ctrl_0 section==============");
    MY_LOGD("Wmfe_Enable=%d", ctrl.Wmfe_Enable);
    MY_LOGD("WmfeFilterSize=%d", ctrl.WmfeFilterSize);
    MY_LOGD("Wmfe_Width=%d", ctrl.Wmfe_Width);
    MY_LOGD("Wmfe_Height=%d", ctrl.Wmfe_Height);
    MY_LOGD("WmfeImgiFmt=%d", ctrl.WmfeImgiFmt);
    MY_LOGD("WmfeDpiFmt=%d", ctrl.WmfeDpiFmt);

    LOG_BUFFER_INFO(ctrl.Wmfe_Imgi);
    LOG_BUFFER_INFO(ctrl.Wmfe_Dpi);
    LOG_BUFFER_INFO(ctrl.Wmfe_Tbli);
    LOG_BUFFER_INFO(ctrl.Wmfe_Dpo);

    MY_LOGD("Tbli: ");
    short *addr = (short*) ctrl.Wmfe_Tbli.u4BufVA;
    int index = 0;
    while(index < 128)
    {
      MY_LOGD("%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \n",
                 addr[index], addr[index+1], addr[index+2], addr[index+3],
                 addr[index+4], addr[index+5], addr[index+6], addr[index+7]);
      index +=8;
    }

    ctrl = config.Wmfe_Ctrl_1;
    MY_LOGD("========== Wmfe_Ctrl_1 section==============");
    MY_LOGD("Wmfe_Enable=%d", ctrl.Wmfe_Enable);
    MY_LOGD("WmfeFilterSize=%d", ctrl.WmfeFilterSize);
    MY_LOGD("Wmfe_Width=%d", ctrl.Wmfe_Width);
    MY_LOGD("Wmfe_Height=%d", ctrl.Wmfe_Height);
    MY_LOGD("WmfeImgiFmt=%d", ctrl.WmfeImgiFmt);
    MY_LOGD("WmfeDpiFmt=%d", ctrl.WmfeDpiFmt);

    LOG_BUFFER_INFO(ctrl.Wmfe_Imgi);
    LOG_BUFFER_INFO(ctrl.Wmfe_Dpi);
    LOG_BUFFER_INFO(ctrl.Wmfe_Tbli);
    LOG_BUFFER_INFO(ctrl.Wmfe_Dpo);

    MY_LOGD("Tbli: ");
    addr = (short*) ctrl.Wmfe_Tbli.u4BufVA;
    index = 0;
    while(index < 128)
    {
      MY_LOGD("%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \n",
                 addr[index], addr[index+1], addr[index+2], addr[index+3],
                 addr[index+4], addr[index+5], addr[index+6], addr[index+7]);
      index +=8;
    }


}

MBOOL
WMFNode::
threadLoop_WMFFirstPass(ImgInfoMapPtr& rpOCCImgInfo)
{
    CAM_TRACE_NAME("WMFNode::threadLoop_WMFFirstPass");
    // mark on-going-request start
    this->incExtThreadDependency();

    EffectRequestPtr reqPtr = rpOCCImgInfo->getRequestPtr();

    VSDOF_PRFLOG("threadLoop start: WMF First-Pass, reqID=%d", reqPtr->getRequestNo());

    EnquedBufPool *pEnqueBufPool = new EnquedBufPool(rpOCCImgInfo, this);
    // input MY_S/DMH
    SmartImageBuffer smImg_MY_S = rpOCCImgInfo->getImageBuffer(BID_OCC_OUT_MY_S);
    SmartImageBuffer smImg_DMH = rpOCCImgInfo->getImageBuffer(BID_OCC_OUT_DMH);
    // prepare the enque params
    WMFEConfig wmfConfig;
    prepareWMFEnqueConfig(smImg_MY_S->mImageBuffer, smImg_DMH->mImageBuffer,
                            wmfConfig, pEnqueBufPool);
    //
    WMFEParams enqueParams;
    enqueParams.mpfnCallback = onWMFEnqueDone_FirstPass;
    enqueParams.mWMFEConfigVec.push_back(wmfConfig);
    enqueParams.mpCookie = (void*) pEnqueBufPool;
    // debug
    debugWMFParam(wmfConfig);

    CAM_TRACE_BEGIN("WMFNode::WMFEEnque");
    VSDOF_PRFLOG("WMFE enque start - first pass, reqID=%d t=%d", reqPtr->getRequestNo(), pEnqueBufPool->startTimer());
    MBOOL bRet = mpDPEStream->WMFEenque(enqueParams);
    VSDOF_PRFLOG("WMFE enque start - first pass, reqID=%d, config-time=%f ms", reqPtr->getRequestNo(), pEnqueBufPool->countTimer());
    CAM_TRACE_END();
    if(!bRet)
    {
        MY_LOGE("WMF enque failed!!");
        goto lbExit;
    }

    VSDOF_PRFLOG("threadLoop end: WMF First-Pass, reqID=%d", reqPtr->getRequestNo());

    return MTRUE;

lbExit:
    delete pEnqueBufPool;
    // mark on-going-request end
    this->decExtThreadDependency();
    return MFALSE;
}

MVOID
WMFNode::
onWMFEnqueDone_FirstPass(WMFEParams& rParams)
{
    EnquedBufPool* pEnquePool = reinterpret_cast<EnquedBufPool*>(rParams.mpCookie);
    WMFNode* pWMFNode = pEnquePool->mpNode;
    pWMFNode->handleWMFEnqueDone_FirstPass(rParams, pEnquePool);
}

MVOID
WMFNode::
handleWMFEnqueDone_FirstPass(WMFEParams& rParams, EnquedBufPool* pEnqueBufPool)
{
    CAM_TRACE_NAME("WMFNode::handleWMFEnqueDone_FirstPass");
    ImgInfoMapPtr pOCCImgInfo = pEnqueBufPool->mpImgBufInfo;
    EffectRequestPtr pEffectReq = pOCCImgInfo->getRequestPtr();
    MUINT32 iReqNo =  pEffectReq->getRequestNo();

    VSDOF_PRFLOG("+, reqID=%d exec-time=%f msec", iReqNo, pEnqueBufPool->countTimer());
    // if queued flow type + PV/VR scenario
    if(this->isQueuedDepthRequest(pEffectReq))
    {
        VSDOF_LOGD("reqID=%d, Store depth info to data storage!.", iReqNo);
        DepthBufferInfo depthInfo;
        // store DMH to buffer storage
        pEnqueBufPool->getBuffData(BID_WMF_OUT_DMW, depthInfo.mpDepthBuffer);
        depthInfo.mpDepthBuffer->mImageBuffer->syncCache(eCACHECTRL_INVALID);
        // get convergence offset
        tryGetMetadataInFrame<MFLOAT>(pEffectReq, FRAME_OUTPUT,
                        BID_META_OUT_HAL_QUEUED, MTK_CONVERGENCE_DEPTH_OFFSET, depthInfo.mfConvOffset);
        // push into storage
        mpDepthStorage->push_back(depthInfo);
        // notify queued flow type finished
        handleData(QUEUED_FLOW_DONE, pEffectReq);
        // dump buffer
        ImgInfoMapPtr toGFImgInfo = new ImageBufInfoMap(pEffectReq);
        toGFImgInfo->addImageBuffer(BID_WMF_OUT_DMW, depthInfo.mpDepthBuffer);
        handleDump(WMF_TO_GF_DMW_MY_S, toGFImgInfo, "BID_WMF_OUT_DMW_INTO_QUEUE");
    }
    else
    {
        ImgInfoMapPtr toGFImgInfo = new ImageBufInfoMap(pEffectReq);
        // pass MY_S to GF
        SmartImageBuffer smImg_MY_S = pOCCImgInfo->getImageBuffer(BID_OCC_OUT_MY_S);
        toGFImgInfo->addImageBuffer(BID_OCC_OUT_MY_S, smImg_MY_S);
        // pass output DMW to GF
        SmartImageBuffer smImg_DMW;
        pEnqueBufPool->getBuffData(BID_WMF_OUT_DMW, smImg_DMW);
        smImg_DMW->mImageBuffer->syncCache(eCACHECTRL_INVALID);
        toGFImgInfo->addImageBuffer(BID_WMF_OUT_DMW, smImg_DMW);
        // handle data
        handleDataAndDump(WMF_TO_GF_DMW_MY_S, toGFImgInfo);
    }

lbExit:
    delete pEnqueBufPool;
    VSDOF_PRFLOG("-, reqID=%d", iReqNo);
    // mark on-going-request end
    this->decExtThreadDependency();
}


MVOID
WMFNode::
prepareWMFEnqueConfig(
    sp<IImageBuffer> pImgBuf_SrcImg,
    sp<IImageBuffer> pImgBuf_SrcDepth,
    WMFEConfig &rWMFConfig,
    EnquedBufPool* pEnqueBufPool
)
{
    // WMF TBLI tuning buffer
    setupDPEBufInfo(DMA_WMFE_TBLI, mpTbliImgBuf->mImageBuffer, rWMFConfig.Wmfe_Ctrl_0.Wmfe_Tbli);
    setupDPEBufInfo(DMA_WMFE_TBLI, mpTbliImgBuf->mImageBuffer, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Tbli);

    // flush
    pImgBuf_SrcImg->syncCache(eCACHECTRL_FLUSH);
    pImgBuf_SrcDepth->syncCache(eCACHECTRL_FLUSH);

    // filter size
    rWMFConfig.Wmfe_Ctrl_0.WmfeFilterSize = eWMFFilterSize;
    rWMFConfig.Wmfe_Ctrl_1.WmfeFilterSize = eWMFFilterSize;
    // config WMFEConfig set 0
    rWMFConfig.Wmfe_Ctrl_0.Wmfe_Enable = true;
    rWMFConfig.Wmfe_Ctrl_1.Wmfe_Enable = true;
    // size are all the same, use MY_S size
    rWMFConfig.Wmfe_Ctrl_0.Wmfe_Width = pImgBuf_SrcImg->getImgSize().w;
    rWMFConfig.Wmfe_Ctrl_0.Wmfe_Height = pImgBuf_SrcImg->getImgSize().h;

    // request DMW_internal
    SmartImageBuffer smDMWInternal = mBufPoolSet.mpDMWImgBufPool->request();
    pEnqueBufPool->addBuffData(BID_WMF_DMW_INTERNAL, smDMWInternal);
    rWMFConfig.Wmfe_Ctrl_1.Wmfe_Width = smDMWInternal->mImageBuffer->getImgSize().w;
    rWMFConfig.Wmfe_Ctrl_1.Wmfe_Height = smDMWInternal->mImageBuffer->getImgSize().h;

    // request output DMW
    SmartImageBuffer smDMW = mBufPoolSet.mpDMWImgBufPool->request();
    pEnqueBufPool->addBuffData(BID_WMF_OUT_DMW, smDMWInternal);

    // IMGI FMT setting
    if(pImgBuf_SrcImg->getImgFormat() == eImgFmt_YV12 || pImgBuf_SrcImg->getImgFormat() == eImgFmt_Y8)
    {
        rWMFConfig.Wmfe_Ctrl_0.WmfeImgiFmt = DPE_IMGI_Y_FMT;
        rWMFConfig.Wmfe_Ctrl_1.WmfeImgiFmt = DPE_IMGI_Y_FMT;
    }
    else if(pImgBuf_SrcImg->getImgFormat() == eImgFmt_YUY2)
    {
        rWMFConfig.Wmfe_Ctrl_0.WmfeImgiFmt = DPE_IMGI_YC_FMT;
        rWMFConfig.Wmfe_Ctrl_1.WmfeImgiFmt = DPE_IMGI_YC_FMT;
    }
    else
    {
        MY_LOGE("WMF CTRL0:IMGI Format not-suporrted!! ImageFormat=%x", pImgBuf_SrcImg->getImgFormat());
        return;
    }

    rWMFConfig.Wmfe_Ctrl_0.WmfeDpiFmt = WMFE_DPI_D_FMT;
    rWMFConfig.Wmfe_Ctrl_1.WmfeDpiFmt = WMFE_DPI_D_FMT;
    // CTRL0: input buffer IMGI pImgBuf_SrcImg: MY_S or MY_SL
    setupDPEBufInfo(DMA_WMFE_IMGI, pImgBuf_SrcImg, rWMFConfig.Wmfe_Ctrl_0.Wmfe_Imgi);
    // CTRL0: input buffer DPI pImgBuf_SrcDepth:  DMH or DMW
    setupDPEBufInfo(DMA_WMFE_DPI, pImgBuf_SrcDepth, rWMFConfig.Wmfe_Ctrl_0.Wmfe_Dpi);
    // CTRL0: output buffer DPO: WorkingBuf_Internal
    setupDPEBufInfo(DMA_WMFE_DPO, smDMWInternal->mImageBuffer, rWMFConfig.Wmfe_Ctrl_0.Wmfe_Dpo);

    // CTRL1: input buffer IMGI: MY_S or MY_SL
    setupDPEBufInfo(DMA_WMFE_IMGI, pImgBuf_SrcImg, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Imgi);
    // CTRL1: input buffer DPI: WorkingBuf_Internal
    setupDPEBufInfo(DMA_WMFE_DPI, smDMWInternal->mImageBuffer, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Dpi);
    // CTRL1: output buffer DPO: DWM or DepthMap
    setupDPEBufInfo(DMA_WMFE_DPO, smDMW->mImageBuffer, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Dpo);

    // disable the set 2
    rWMFConfig.Wmfe_Ctrl_2.Wmfe_Enable = false;
}

MBOOL
WMFNode::
setupDPEBufInfo(DPEDMAPort dmaPort, android::sp<IImageBuffer> &pImgBuf, DPEBufInfo& rBufInfo)
{
    // plane 0 address
    rBufInfo.memID = pImgBuf->getFD(0);
    rBufInfo.dmaport = dmaPort;
    rBufInfo.u4BufVA = pImgBuf->getBufVA(0);
    rBufInfo.u4BufPA = pImgBuf->getBufPA(0);
    rBufInfo.u4BufSize = pImgBuf->getBufSizeInBytes(0);
    rBufInfo.u4Stride = pImgBuf->getBufStridesInBytes(0);

    return MTRUE;
}

MBOOL
WMFNode::
setDepthBufStorage(sp<DepthInfoStorage> pDepthStorage)
{
    mpDepthStorage = pDepthStorage;
    return MTRUE;
}

}; //NSFeaturePipe
}; //NSCamFeature
}; //NSCam
