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

 // Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/drv/iopipe/PostProc/DpeUtility.h>
#include <vsdof/hal/stereo_tuning_provider.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
// Module header file
// Local header file
#include "WMFNode.h"
#include "../DepthMapPipe_Common.h"
#include "../DepthMapPipeUtils.h"
#include "../util/vsdof_util.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"
// Logging header file
#define PIPE_CLASS_TAG "WMFNode"
#include <featurePipe/core/include/PipeLog.h>

using namespace NSCam::NSIoPipe::NSDpe;
using namespace VSDOF::util;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

WMFNode::
WMFNode(
    const char *name,
    DepthMapPipeNodeID nodeID,
    sp<DepthMapFlowOption> pFlowOpt,
    const DepthMapPipeSetting& setting
)
: DepthMapPipeNode(name, nodeID, pFlowOpt, setting)
{
    this->addWaitQueue(&mJobQueue);
    this->addWaitQueue(&mWMFSecQueue);
}

WMFNode::
~WMFNode()
{
    MY_LOGD("[Destructor]");
}

MVOID
WMFNode::
cleanUp()
{
    VSDOF_LOGD("+");
    if(mpDPEStream != NULL)
    {
        mpDPEStream->uninit();
        mpDPEStream = NULL;
    }

    if(mpDpStream != NULL)
    {
        delete mpDpStream;
    }
    // JOB queue clear
    mJobQueue.clear();
    mWMFSecQueue.clear();
    // release Tbli buffer pool
    mpTbliImgBuf = NULL;
    ImageBufferPool::destroy(mpTbliImgBufPool);

    VSDOF_LOGD("-");
}

MBOOL
WMFNode::
onInit()
{
    CAM_TRACE_NAME("WMFNode::onInit");
    VSDOF_LOGD("+");
    // init DPEStream
    mpDPEStream = NSCam::NSIoPipe::NSDpe::IDpeStream::createInstance("VSDOF_DPE");
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
    // init DpStream
    mpDpStream = new DpBlitStream();

    // Tbli buffer pool
    StereoSizeProvider * pSizeProvder = StereoSizeProvider::getInstance();
    MUINT32 iWMFInputWidth = pSizeProvder->getBufferSize(E_MY_S, eSTEREO_SCENARIO_RECORD).size.w;
    MUINT32 iTbliStride = DPEQueryInDMAStride(DMA_WMFE_TBLI, WMFE_DPI_D_FMT, iWMFInputWidth);
    mpTbliImgBufPool = ImageBufferPool::create("TbliBufPool", iTbliStride, 1,
                                                eImgFmt_Y8, ImageBufferPool::USAGE_HW, MTRUE);
    // allocate one buffer
    mpTbliImgBufPool->allocate(1);
    // get Tbli tuning buffer
    mpTbliImgBuf = mpTbliImgBufPool->request();
    // query tuning mgr to get Tbli + filter size
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
onData(DataID data, DepthMapRequestPtr& pRequest)
{
    MBOOL ret=MTRUE;
    VSDOF_PRFLOG("+, dataID=%d reqId=%d", data, pRequest->getRequestNo());

    switch(data)
    {
        case OCC_TO_WMF_DMH_MY_S:
            mJobQueue.enque(pRequest);
            break;
        case P2AFM_TO_WMF_MY_SL:
            {
                Mutex::Autolock _l(mMYSLMutex);
                mMYSLReqIDQueue.push(pRequest->getRequestNo());
                break;
            }
        default:
            ret = MFALSE;
            break;
    }

    VSDOF_LOGD("-");
    return ret;
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

MBOOL
WMFNode::
onThreadLoop()
{
    DepthMapRequestPtr pRequest;

    if( !waitAnyQueue() )
    {
        return MFALSE;
    }
    CAM_TRACE_NAME("WMFNode::onThreadLoop");

    bool bExist = false;

    // check the second pass WMF queue first
    bExist = mWMFSecQueue.deque(pRequest);

    if(bExist)
    {
        return threadLoop_WMFSecPass(pRequest);
    }
    else
    {
        if( !mJobQueue.deque(pRequest) )
        {
            MY_LOGD("mJobQueue.deque() failed");
            return MFALSE;
        }
        return threadLoop_WMFFirstPass(pRequest);
    }

}

MVOID
WMFNode::
debugWMFParam(WMFEConfig &config)
{
    if(!DepthPipeLoggingSetup::mbDebugLog)
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
threadLoop_WMFFirstPass(DepthMapRequestPtr& pRequest)
{
    CAM_TRACE_NAME("WMFNode::threadLoop_WMFFirstPass");
    // mark on-going-request start
    this->incExtThreadDependency();

    VSDOF_PRFLOG("threadLoop start: WMF First-Pass, reqID=%d", pRequest->getRequestNo());
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    EnqueCookieContainer *pEnqueCookie = new EnqueCookieContainer(pRequest, this);

    // enque param
    WMFEParams enqueParams;
    WMFEConfig wmfConfig;
    EnqueBIDConfig config = {BID_OCC_OUT_MY_S ,BID_OCC_OUT_DMH, BID_WMF_DMW_INTERNAL, BID_WMF_OUT_DMW};
    // prepare the enque params
    prepareWMFEnqueConfig(pBufferHandler, config, wmfConfig);
    enqueParams.mpfnCallback = onWMFEnqueDone_FirstPass;
    enqueParams.mWMFEConfigVec.push_back(wmfConfig);
    enqueParams.mpCookie = (void*) pEnqueCookie;
    // debug params
    //debugWMFParam(wmfConfig);

    // timer
    pRequest->mTimer.startWMF();
    pRequest->mTimer.startWMFEnque();
    CAM_TRACE_BEGIN("WMFNode::WMFEEnque");
    VSDOF_PRFLOG("WMFE enque start - first pass, reqID=%d", pRequest->getRequestNo());
    MBOOL bRet = mpDPEStream->WMFEenque(enqueParams);
    //
    pRequest->mTimer.stopWMFEnque();
    VSDOF_PRFLOG("WMFE enque start - first pass, reqID=%d, config-time=%d ms",
                    pRequest->getRequestNo(), pRequest->mTimer.getElapsedWMFEnque());
    CAM_TRACE_END();
    if(!bRet)
    {
        MY_LOGE("WMF enque failed!!");
        goto lbExit;
    }

    VSDOF_PRFLOG("threadLoop end: WMF First-Pass, reqID=%d", pRequest->getRequestNo());

    return MTRUE;

lbExit:
    delete pEnqueCookie;
    // mark on-going-request end
    this->decExtThreadDependency();
    return MFALSE;
}

MVOID
WMFNode::
onWMFEnqueDone_FirstPass(WMFEParams& rParams)
{
    EnqueCookieContainer* pEnqueCookie = reinterpret_cast<EnqueCookieContainer*>(rParams.mpCookie);
    WMFNode* pWMFNode = reinterpret_cast<WMFNode*>(pEnqueCookie->mpNode);
    pWMFNode->handleWMFEnqueDone_FirstPass(rParams, pEnqueCookie);
}

MVOID
WMFNode::
handleWMFEnqueDone_FirstPass(
    WMFEParams& rParams,
    EnqueCookieContainer* pEnqueCookie
)
{
    CAM_TRACE_NAME("WMFNode::handleWMFEnqueDone_FirstPass");
    DepthMapRequestPtr pRequest = pEnqueCookie->mRequest;
    // timer
    pRequest->mTimer.stopWMF();
    MUINT32 iReqNo =  pRequest->getRequestNo();
    VSDOF_PRFLOG("+, reqID=%d exec-time=%d msec", iReqNo, pRequest->mTimer.getElapsedWMF());

    // mark MY_S/DMW frameReady and return
    pRequest->setOutputBufferReady(BID_OCC_OUT_MY_S);
    pRequest->setOutputBufferReady(BID_WMF_OUT_DMW);
    //
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    // get buffer and invalidate
    IImageBuffer* pImgBuf_MY_S, *pImgBuf_DMW;
    pBufferHandler->getEnqueBuffer(getNodeId(), BID_OCC_OUT_MY_S, pImgBuf_MY_S);
    pBufferHandler->getEnqueBuffer(getNodeId(), BID_WMF_OUT_DMW, pImgBuf_DMW);
    pImgBuf_MY_S->syncCache(eCACHECTRL_INVALID);
    pImgBuf_DMW->syncCache(eCACHECTRL_INVALID);
    // handle data and dump
    handleDataAndDump(WMF_OUT_DMW_MY_S, pRequest);

    if(pRequest->getRequestAttr().opState == eSTATE_NORMAL)
    {
        // launch onProcessDone
        pBufferHandler->onProcessDone(getNodeId());
    }
    else
    {
        // enque second pass WaitQueue
        mWMFSecQueue.enque(pRequest);
    }


lbExit:
    delete pEnqueCookie;
    VSDOF_PRFLOG("-, reqID=%d", iReqNo);
    // mark on-going-request end
    this->decExtThreadDependency();
}

MBOOL
WMFNode::
threadLoop_WMFSecPass(DepthMapRequestPtr& pRequest)
{
    CAM_TRACE_NAME("WMFNode::threadLoop_WMFSecPass");

    MINT32 iMYSLReadyReqID = 0;

    {
        Mutex::Autolock _l(mMYSLMutex);
        iMYSLReadyReqID = mMYSLReqIDQueue.front();
        mMYSLReqIDQueue.pop();
    }

    MINT32 iReqNo = pRequest->getRequestNo();
    if(iMYSLReadyReqID != iReqNo)
    {
        MY_LOGE("MYSL-ready request id is different to current one, iMYSLReadyReqID=%d iReqNo=%d",
                    iMYSLReadyReqID, iReqNo);
        return MFALSE;
    }

    // mark on-going-request start
    this->incExtThreadDependency();

    VSDOF_PRFLOG("threadLoop start: WMF Sec-Pass, reqID=%d", iReqNo);
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    EnqueCookieContainer *pEnqueCookie = new EnqueCookieContainer(pRequest, this);

    IMetadata* pInAppMeta = pBufferHandler->requestMetadata(getNodeId(), BID_META_IN_APP);
    // get rotation
    MINT32 jpegOrientation = 0;
    if(!tryGetMetadata<MINT32>(pInAppMeta, MTK_JPEG_ORIENTATION, jpegOrientation)) {
        MY_LOGE("Cannot find MTK_JPEG_ORIENTATION meta!");
    }

    DepthMapBufferID outDepthBID;
    // if need rotation, use the rotation-temp depth buffer as output and then rotate it
    if(jpegOrientation == 0)
    {
        // no rotation, use default
        outDepthBID = BID_WMF_OUT_DEPTHMAP;
    }
    else
    {
        // do rotation, use BID_WMF_DEPTHMAP_ROTATION
        outDepthBID = BID_WMF_DEPTHMAP_ROTATION;
    }

    // enque param
    WMFEParams enqueParams;
    WMFEConfig wmfConfig;
    EnqueBIDConfig config = {BID_P2AFM_OUT_MY_SL ,BID_WMF_OUT_DMW, BID_WMF_DEPTHMAP_INTERNAL, outDepthBID};
    // prepare the enque params
    prepareWMFEnqueConfig(pBufferHandler, config, wmfConfig);
    enqueParams.mpfnCallback = onWMFEnqueDone_SecPass;
    enqueParams.mWMFEConfigVec.push_back(wmfConfig);
    enqueParams.mpCookie = (void*) pEnqueCookie;
    // debug params
    debugWMFParam(wmfConfig);
    // timer
    pRequest->mTimer.startWMFSecPass();
    pRequest->mTimer.startWMFSecPassEnque();
    VSDOF_PRFLOG("WMFE enque start - second pass, reqID=%d", pRequest->getRequestNo());
    CAM_TRACE_BEGIN("WMFNode::WMFEEnque_Pass2");
    MBOOL bRet = mpDPEStream->WMFEenque(enqueParams);
    CAM_TRACE_END();
    //
    pRequest->mTimer.stopWMFSecPassEnque();
    VSDOF_PRFLOG("WMFE enque start - second pass, reqID=%d config-time=%d ms",
                    pRequest->getRequestNo(), pRequest->mTimer.getElapsedWMFSecPassEnque());

    if(!bRet)
    {
        MY_LOGE("WMF enque second pass failed!!");
        goto lbExit;
    }

    VSDOF_PRFLOG("threadLoop end: WMF Sec-Pass, reqID=%d", pRequest->getRequestNo());
    return MTRUE;

lbExit:
    delete pEnqueCookie;
    // mark on-going-request end
    this->decExtThreadDependency();
    return MFALSE;
}


MVOID
WMFNode::
onWMFEnqueDone_SecPass(WMFEParams& rParams)
{
    EnqueCookieContainer* pEnqueCookie = reinterpret_cast<EnqueCookieContainer*>(rParams.mpCookie);
    WMFNode* pWMFNode = (WMFNode*)pEnqueCookie->mpNode;
    pWMFNode->handleWMFEnqueDone_SecPass(rParams, pEnqueCookie);
}

MVOID
WMFNode::
handleWMFEnqueDone_SecPass(WMFEParams& rParams, EnqueCookieContainer* pEnqueCookie)
{
    CAM_TRACE_NAME("WMFNode::handleWMFEnqueDone_SecPass");
    DepthMapRequestPtr pRequest = pEnqueCookie->mRequest;
    sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    // timer
    pRequest->mTimer.stopWMFSecPass();
    VSDOF_PRFLOG("+, reqID=%d, exec-time=%d msec",
            pRequest->getRequestNo(), pRequest->mTimer.getElapsedWMFSecPass());

    IMetadata* pInAppMeta = pBufferHandler->requestMetadata(getNodeId(), BID_META_IN_APP);
    // get rotation
    MINT32 jpegOrientation = 0;
    tryGetMetadata<MINT32>(pInAppMeta, MTK_JPEG_ORIENTATION, jpegOrientation);
    //do roataion
    if(jpegOrientation >= 0)
    {
        VSDOF_LOGD("Do g-sensor rotation: %d degree", jpegOrientation);
        // get source DepthMap buffer: rotation temp buffer (BID_WMF_DEPTHMAP_ROTATION)
        IImageBuffer *pImgBuf_SrcDepth;
        MBOOL bRet = pBufferHandler->getEnqueBuffer(getNodeId(), BID_WMF_DEPTHMAP_ROTATION, pImgBuf_SrcDepth);
        // target DepthMap
        IImageBuffer *pImgBuf_TargetDepth;
        pImgBuf_TargetDepth = pBufferHandler->requestBuffer(getNodeId(), BID_WMF_OUT_DEPTHMAP);


        if(!bRet || pImgBuf_TargetDepth==NULL)
            MY_LOGE("Failed to get rotation DepthMap buffer");
        else
        {
            CAM_TRACE_BEGIN("DepthMap MDP rotate");
            sMDP_Config config;
            config.pDpStream = mpDpStream;
            config.pSrcBuffer = pImgBuf_SrcDepth;
            config.pDstBuffer = pImgBuf_TargetDepth;
            config.rotAngle = jpegOrientation;

            if(!excuteMDP(config))
            {
                MY_LOGE("excuteMDP fail: Cannot roatate depth map.");
            }
            CAM_TRACE_END();
        }
    }

    IImageBuffer *pImgBuf_DepthMap;
    pBufferHandler->getEnqueBuffer(getNodeId(), BID_WMF_OUT_DEPTHMAP, pImgBuf_DepthMap);
    // syncCache
    pImgBuf_DepthMap->syncCache(eCACHECTRL_INVALID);
    // mark DepthMap output ready and output
    pRequest->setOutputBufferReady(BID_WMF_OUT_DEPTHMAP);
    // dump first and handle data
    handleDump(WMF_OUT_DEPTHMAP, pRequest);
    handleData(WMF_OUT_DEPTHMAP, pRequest);

    delete pEnqueCookie;
    // launch onProcessDone
    pBufferHandler->onProcessDone(getNodeId());

    VSDOF_PRFLOG("-, reqID=%d", pRequest->getRequestNo());
    // mark on-going-request end
    this->decExtThreadDependency();
}

MVOID
WMFNode::
prepareWMFEnqueConfig(
    sp<BaseBufferHandler> pBufferHandler,
    const EnqueBIDConfig& enqueConfig,
    WMFEConfig &rWMFConfig
)
{
    MBOOL bRet = MTRUE;
    IImageBuffer *pImgBuf_SrcImg, *pImgBuf_SrcDepth, *pImgBuf_OutputDepth;
    // input buffers
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), enqueConfig.srcImgBID, pImgBuf_SrcImg);
    bRet &= pBufferHandler->getEnqueBuffer(getNodeId(), enqueConfig.srcDepthBID, pImgBuf_SrcDepth);
    pImgBuf_SrcImg->syncCache(eCACHECTRL_FLUSH);
    pImgBuf_SrcDepth->syncCache(eCACHECTRL_FLUSH);
    // output buffers
    pImgBuf_OutputDepth = pBufferHandler->requestBuffer(getNodeId(), enqueConfig.outDepthBID);
    // request internal working buffer
    IImageBuffer* pInterBuf = pBufferHandler->requestBuffer(getNodeId(), enqueConfig.interalImgBID);

    // WMF TBLI tuning buffer
    setupDPEBufInfo(DMA_WMFE_TBLI, mpTbliImgBuf->mImageBuffer.get(), rWMFConfig.Wmfe_Ctrl_0.Wmfe_Tbli);
    setupDPEBufInfo(DMA_WMFE_TBLI, mpTbliImgBuf->mImageBuffer.get(), rWMFConfig.Wmfe_Ctrl_1.Wmfe_Tbli);
    // filter size
    rWMFConfig.Wmfe_Ctrl_0.WmfeFilterSize = eWMFFilterSize;
    rWMFConfig.Wmfe_Ctrl_1.WmfeFilterSize = eWMFFilterSize;

    // config WMFEConfig set 0
    rWMFConfig.Wmfe_Ctrl_0.Wmfe_Enable = true;
    // size are all the same, use srcImg(MY_S or MY_SL) size
    rWMFConfig.Wmfe_Ctrl_0.Wmfe_Width = pImgBuf_SrcImg->getImgSize().w;
    rWMFConfig.Wmfe_Ctrl_0.Wmfe_Height = pImgBuf_SrcImg->getImgSize().h;

    // config WMFEConfig set 1
    rWMFConfig.Wmfe_Ctrl_1.Wmfe_Enable = true;
    // interal working buffer size
    rWMFConfig.Wmfe_Ctrl_1.Wmfe_Width = pInterBuf->getImgSize().w;
    rWMFConfig.Wmfe_Ctrl_1.Wmfe_Height = pInterBuf->getImgSize().h;

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
    setupDPEBufInfo(DMA_WMFE_DPO, pInterBuf, rWMFConfig.Wmfe_Ctrl_0.Wmfe_Dpo);

    // CTRL1: input buffer IMGI: MY_S or MY_SL
    setupDPEBufInfo(DMA_WMFE_IMGI, pImgBuf_SrcImg, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Imgi);
    // CTRL1: input buffer DPI: WorkingBuf_Internal
    setupDPEBufInfo(DMA_WMFE_DPI, pInterBuf, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Dpi);
    // CTRL1: output buffer DPO: DWM or DepthMap
    setupDPEBufInfo(DMA_WMFE_DPO, pImgBuf_OutputDepth, rWMFConfig.Wmfe_Ctrl_1.Wmfe_Dpo);

    // disable the set 2
    rWMFConfig.Wmfe_Ctrl_2.Wmfe_Enable = false;

    #define DEBUG_BUFFER_SETUP(buf) \
        MY_LOGD("DPE buf:" # buf);\
        MY_LOGD("Image buffer size=%dx%d:", buf->getImgSize().w, buf->getImgSize().h);\
        MY_LOGD("Image buffer format=%x", buf->getImgFormat());

    // debug section
    if(DepthPipeLoggingSetup::mbDebugLog)
    {
        DEBUG_BUFFER_SETUP(mpTbliImgBuf->mImageBuffer);
        DEBUG_BUFFER_SETUP(pImgBuf_SrcImg);
        DEBUG_BUFFER_SETUP(pImgBuf_SrcDepth);
        DEBUG_BUFFER_SETUP(pInterBuf);
        DEBUG_BUFFER_SETUP(pImgBuf_OutputDepth);
    }

    #undef DEBUG_BUFFER_SETUP
}

MBOOL
WMFNode::
setupDPEBufInfo(DPEDMAPort dmaPort, IImageBuffer* pImgBuf, DPEBufInfo& rBufInfo)
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


MVOID
WMFNode::
onFlush()
{
    while(!mMYSLReqIDQueue.empty())
        mMYSLReqIDQueue.pop();
    DepthMapPipeNode::onFlush();
}

}; //NSFeaturePipe_DepthMap_BM
}; //NSCamFeature
}; //NSCam
