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
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
//
#include <mtkcam/feature/stereo/pipe/vsdof_common.h>
#include <mtkcam/feature/stereo/pipe/vsdof_data_define.h>

#include "bokeh_common.h"
#include "VSDOF_P2BNode.h"
//
#define NODE_NAME "VSDOF_P2BNode"
//
#define PIPE_MODULE_TAG "BokehPipe"
#define PIPE_CLASS_TAG "VSDOF_P2BNode"
#include <PipeLog.h>
#include <drv/isp_reg.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <common/vsdof/hal/stereo_tuning_provider.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
//
#include "../util/vsdof_util.h"
// for NormalStream
#define FEATURE_PIPE_CALLER_NAME "FeaturePipe_Bokeh_P2B"

#include <string>
//
#include <DpBlitStream.h>

using namespace android;
using namespace std;

using namespace NSCam::NSCamFeature::NSFeaturePipe;
using namespace NSCam::NSIoPipe::NSPostProc;
//************************************************************************
// utility function
//************************************************************************
#define NS_PER_SEC  1000000000
#define NS_PER_MS   1000000
#define NS_PER_US   1000
static void Wait(int ms)
{
  long waitSec;
  long waitNSec;
  waitSec = (ms * NS_PER_MS) / NS_PER_SEC;
  waitNSec = (ms * NS_PER_MS) % NS_PER_SEC;
  struct timespec t;
  t.tv_sec = waitSec;
  t.tv_nsec = waitNSec;
  if( nanosleep(&t, NULL) != 0 )
  {
  }
}
/*******************************************************************************
 *
 ********************************************************************************/
VSDOF_P2BNode::
VSDOF_P2BNode(
    const char *name,
    Graph_T *graph,
    MINT32 openId,
    MINT8 mode)
    : BokehPipeNode(name, graph)
    , miOpenId(openId)
    , miMode(mode)
{
    MY_LOGD("ctor(0x%x)", this);
    this->addWaitQueue(&mRequests);
    /*if(miMode == ADVANCE)
    {
        MY_LOGD("ADVANCE mode");
        this->addWaitQueue(&mDMGBufQueue);
    }
    this->addWaitQueue(&mDMBGBufQueue);*/
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.vsdof.showdepthmap", cLogLevel, "0");
    mShowDepthMap = ::atoi(cLogLevel);
    MY_LOGD("miPipeLogEnable(%d) mode(%d) openid(%x)", miPipeLogEnable, miMode, miOpenId);
}
/*******************************************************************************
 *
 ********************************************************************************/
VSDOF_P2BNode::
~VSDOF_P2BNode()
{
    MY_LOGD("dctor(0x%x)", this);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
onInit()
{
    CAM_TRACE_NAME("VSDOF_P2BNode::onInit");
    BokehPipeNode::onInit();
    //
    TRACE_FUNC_ENTER();
    FUNC_START;
    MBOOL ret = MTRUE;
    const NSCam::EImageFormat imgFmt = eImgFmt_YV12;
    const MINT32 usage = ImageBufferPool::USAGE_HW;
    CAM_TRACE_BEGIN("VSDOF_P2BNode::NormalStream::createInstance+init");
    // Create NormalStream
    mpINormalStream = INormalStream::createInstance(miOpenId);
    if(nullptr == mpINormalStream)
    {
        MY_LOGE("INormalStream: create instance for FeaturePipe_P2 failed!");
        return MFALSE;
    }
    mpINormalStream->init(getName());
    CAM_TRACE_END();
    // Create tuning buffer pool
    unsigned int tuningsize = sizeof(dip_x_reg_t);
    mpP2BTuningPool = TuningBufferPool::create(
                            "BOKEH_P2B_TUNING_BUF",
                            tuningsize);
    if(nullptr == mpP2BTuningPool.get())
    {
        MY_LOGE("Create mpP2BTuningPool failed.");
        cleanUp();
        return MFALSE;
    }
    for(int i=0;i<BOKEH_PIPELINE_FRAME_TUNING_BUFFER_SIZE;++i)
    {
        if(!mpP2BTuningPool->allocate())
        {
            MY_LOGE("Allocate mpP2BTuningPool working buffer failed.");
            cleanUp();
            return MFALSE;
        }
    }
    //
    mbIsFirstRun3dnr = MTRUE;
    CAM_TRACE_BEGIN("VSDOF_P2BNode::hal3dnr::createInstance+init");
    // 3dnr init
    mp3dnr = hal3dnrBase::createInstance();
    mp3dnr->init();
    CAM_TRACE_END();

    // Create 3dnr working buffer
    StereoArea sOutput3DNRBufferArea =
            StereoSizeProvider::getInstance()->getBufferSize(E_BOKEH_3DNR, eSTEREO_SCENARIO_PREVIEW);
    ret = createBufferPool(
                mp3DNRBufPool,
                sOutput3DNRBufferArea.size.w,
                sOutput3DNRBufferArea.size.h,
                imgFmt,
                BOKEH_PIPELINE_FRAME_3DNR_BUFFER_SIZE,
                "BOKEH_P2B_3DNR_BUF",
                ImageBufferPool::USAGE_HW,
                MFALSE);
    if(!ret)
    {
        MY_LOGE("Create mp3DNRBufPool failed.");
        cleanUp();
        return MFALSE;
    }
    // get input/output buffer for 3dnr.
    mpInputFrame = mp3DNRBufPool->request();
    mpOutputFrame = mp3DNRBufPool->request();
    MY_LOGD("3DNR buffer depth(%d) fmt(%d) size(%dx%d) usage(%d)",
                    BOKEH_PIPELINE_FRAME_3DNR_BUFFER_SIZE,
                    imgFmt,
                    sOutput3DNRBufferArea.size.w,
                    sOutput3DNRBufferArea.size.h,
                    usage);
    //
    /*StereoArea sOutputWDMABufferArea =
            StereoSizeProvider::getInstance()->getBufferSize(
                        E_BOKEH_WDMA,
                        eSTEREO_SCENARIO_CAPTURE);
    ret = createBufferPool(
                mpWDMABufPool,
                sOutputWDMABufferArea.size.w,
                sOutputWDMABufferArea.size.h,
                imgFmt,
                BOKEH_PIPELINE_FRAME_WDMA_BUFFER_SIZE,
                "BOKEH_P2B_WDMA_BUF",
                usage,
                MFALSE);
    if(!ret)
    {
        MY_LOGE("Create mpWDMABufPool failed.");
        cleanUp();
        return MFALSE;
    }
    MY_LOGD("WDMA buffer depth(%d) fmt(%d) size(%dx%d) usage(%d)",
                    BOKEH_PIPELINE_FRAME_WDMA_BUFFER_SIZE,
                    imgFmt,
                    sOutputWDMABufferArea.size.w,
                    sOutputWDMABufferArea.size.h,
                    usage);*/
    // set dmgi & depi size
    StereoArea sDmgArea =
            StereoSizeProvider::getInstance()->getBufferSize(E_DMG, eSTEREO_SCENARIO_PREVIEW);
    miDmgi = sDmgArea.size;
    StereoArea sDmbgArea =
            StereoSizeProvider::getInstance()->getBufferSize(E_DMBG, eSTEREO_SCENARIO_PREVIEW);
    miDepi = sDmbgArea.size;
    MY_LOGD("DMGI size(%dx%d) DEPI size(%dx%d)",
                    miDmgi.w,
                    miDmgi.h,
                    miDepi.w,
                    miDepi.h);
    //
    if(mShowDepthMap)
    {
        createBufferPool(
                mpDepthMapBufPool,
                720,
                408,
                eImgFmt_Y8,
                2,
                "BOKEH_P2B_DEPTHMAP_BUF",
                usage,
                MTRUE);
        mpDepthMapBufPool->allocate(2);
        mpDpStream = new DpBlitStream();
    }
    //
    //isRunning = MTRUE;
    FUNC_END;
    TRACE_FUNC_EXIT();
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
createBufferPool(
    android::sp<ImageBufferPool> &pPool,
    MUINT32 width,
    MUINT32 height,
    NSCam::EImageFormat format,
    MUINT32 bufCount,
    const char* caller,
    MUINT32 bufUsage,
    MBOOL continuesBuffer)
{
    FUNC_START;
    MBOOL ret = MFALSE;
    pPool = ImageBufferPool::create(caller, width, height, format, bufUsage, continuesBuffer);
    if(pPool == nullptr)
    {
        ret = MFALSE;
        MY_LOGE("Create [%s] failed.", caller);
        goto lbExit;
    }
    for(MUINT32 i=0;i<bufCount;++i)
    {
        if(!pPool->allocate())
        {
            ret = MFALSE;
            MY_LOGE("Allocate [%s] working buffer failed.", caller);
            goto lbExit;
        }
    }
    ret = MTRUE;
    FUNC_END;
lbExit:
    return ret;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
onUninit()
{
    CAM_TRACE_NAME("VSDOF_P2BNode::onUninit");
    cleanUp();
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
VSDOF_P2BNode::
cleanUp()
{
    FUNC_START;
    if(nullptr != mpINormalStream)
    {
        MY_LOGD("release mpINormalStream +");
        mpINormalStream->uninit(getName());
        mpINormalStream->destroyInstance();
        MY_LOGD("release mpINormalStream -");
    }
    mpINormalStream = nullptr;
    // destroy 3dnr
    if(mp3dnr){
        MY_LOGD("release 3dnr +");
        mp3dnr->uninit();
        mp3dnr->destroyInstance();
        mp3dnr = nullptr;
        MY_LOGD("release 3dnr -");
    }
    // release input/output buffer.
    mpInputFrame = nullptr;
    mpOutputFrame = nullptr;
    //
    MY_LOGD("release 3dnr buffer +");
    ImageBufferPool::destroy(mp3DNRBufPool);
    MY_LOGD("release 3dnr buffer -");
    //
    // destroy mpP2BTuningPool
    TuningBufferPool::destroy(mpP2BTuningPool);
    // destroy mpWDMABufPool
    //ImageBufferPool::destroy(mpWDMABufPool);
    // dump all queue size
    MY_LOGD("mRequests.size(%d)", mRequests.size());
    //MY_LOGD("mDMGBufQueue.size(%d)", mDMGBufQueue.size());
    //MY_LOGD("mDMBGBufQueue.size(%d)", mDMBGBufQueue.size());
    //MY_LOGD("mDMGFrameInfo.size(%d)", mDMGFrameInfo.size());
    //MY_LOGD("mDMBGFrameInfo.size(%d)", mDMBGFrameInfo.size());
    if(mpDpStream!= nullptr)
        delete mpDpStream;
    FUNC_END;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
onThreadStart()
{
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
onThreadStop()
{
    FUNC_START;
    FUNC_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
onData(
    DataID id,
    EffectRequestPtr &request)
{
    TRACE_FUNC_ENTER();

    MBOOL ret;
    switch(id)
    {
        case ID_ROOT_ENQUE:
            mRequests.enque(request);
            ret = MTRUE;
            break;
        default:
            ret = MFALSE;
            break;
    }

    TRACE_FUNC_EXIT();
    return ret;
}
/*******************************************************************************
 *
 ********************************************************************************/
/*MBOOL
VSDOF_P2BNode::
onData(
    DataID id,
    SmartImageBuffer &data)
{
    TRACE_FUNC_ENTER();
    MBOOL ret;
    switch(id)
    {
        case GF_BOKEH_DMG:
            mDMGBufQueue.enque(data);
            ret = MTRUE;
            break;
        case GF_BOKEH_DMBG:
            mDMBGBufQueue.enque(data);
            ret = MTRUE;
            break;
        default:
            ret = MFALSE;
            break;
    }
    TRACE_FUNC_EXIT();
    return ret;
}*/
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
onThreadLoop()
{
    EffectRequestPtr request = nullptr;
    //SmartImageBuffer dmgBuffer = nullptr;
    //SmartImageBuffer dmbgBuffer = nullptr;

    if( !waitAllQueue() )
    {
        // mJobs.abort() called
        return MFALSE;
    }
    if( !mRequests.deque(request) )
    {
        MY_LOGD("mRequests.deque() failed");
        return MFALSE;
    }
    CAM_TRACE_NAME("VSDOF_P2BNode::onThreadLoop");
    /*if(miMode == ADVANCE)
    {
        if( !mDMGBufQueue.deque(dmgBuffer) )
        {
            MY_LOGD("mDMGBufQueue.deque() failed");
            return MFALSE;
        }
    }
    if( !mDMBGBufQueue.deque(dmbgBuffer) )
    {
        MY_LOGD("mDMBGBufQueue.deque() failed");
        return MFALSE;
    }*/

    if(!enqueP2B(request/*, dmgBuffer, dmbgBuffer*/))
    {
        MY_LOGE("enqueP2B fail.");
        return MFALSE;
    }
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
enqueP2B(
    EffectRequestPtr request/*,
    SmartImageBuffer dmgBuf,
    SmartImageBuffer dmbgBuf*/)
{
    CAM_TRACE_NAME("VSDOF_P2BNode::enqueP2B");
    MY_LOGD_IF(miPipeLogEnable, "enqueP2B + %d", request->getRequestNo());
    // prepare enque parameter.
    QParams enqueParams;
    enqueData *data = nullptr;

    SmartTuningBuffer pTuningBuffer = nullptr;
    SmartImageBuffer pWDMABuffer = nullptr;
    StereoArea sImgiSize;
    MSize outputImageSize;
    //
    MBOOL ret = MFALSE;
    //
    MSize imgiSize;
    // get scenario id.
    const sp<EffectParameter> params = request->getRequestParameter();
    MINT32 scenarioId = params->getInt(VSDOF_FRAME_SCENARIO);
    MINT32 nr3dFlag = params->getInt(VSDOF_FRAME_3DNR_FLAG);
    auto convertAngleToEnum = [](MINT32 angle)->MINT32
    {
        switch(angle)
        {
            case 90:
                return eTransform_ROT_90;
            case 180:
                return eTransform_ROT_180;
            case 270:
                return eTransform_ROT_270;
            default:
                return 0;
        }
        return 0;
    };
    MINT32 gsensorOri = convertAngleToEnum(params->getInt(VSDOF_FRAME_G_SENSOR_ORIENTATION));
    //
    unpackInputBufferFromRequest(request, enqueParams);
    unpackOutputBufferFromRequest(request, enqueParams, gsensorOri);
    // DMG setting
    /*if(miMode == ADVANCE)
    {
        dmgBuf->mImageBuffer->syncCache(eCACHECTRL_FLUSH);
        setP2BInputPort(enqueParams, dmgBuf, BOKEH_ER_BUF_DMBG );
    }*/
    // DMBG setting
    /*dmbgBuf->mImageBuffer->syncCache(eCACHECTRL_FLUSH);
    setP2BInputPort(enqueParams, dmbgBuf, BOKEH_ER_BUF_DMG);*/
    /*if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_CAPTURE)
    {
        pWDMABuffer = mpWDMABufPool->request();
        setP2BOutputPort(enqueParams, pWDMABuffer, BOKEH_ER_BUF_WDMAIMG);
    }*/
    //
    // set p2b tuning parameters.
    {
        enqueParams.mvStreamTag.push_back(ENormalStreamTag_Bokeh);
        pTuningBuffer = mpP2BTuningPool->request();
        unsigned int tuningsize = sizeof(dip_x_reg_t);
        MVOID* pTuning = (MVOID*)pTuningBuffer->mpVA;
        dip_x_reg_t* pReg = (dip_x_reg_t*) pTuning;
        // clear all regs.
        memset(pTuning, 0, tuningsize);
        if(!StereoTuningProvider::getBokehTuningInfo(pTuning))
        {
            MY_LOGE("set tuning parameter fail.");
            goto lbExit;
        }
        enqueParams.mvTuningData.push_back(pTuning);
        // get pass2 output size
        //Pass2SizeInfo pass2SizeInfo;
        //getP2AOutputSize(scenarioId, pass2SizeInfo);
    }
    // get imgi size
    for(auto data : enqueParams.mvIn)
    {
        if(data.mPortID==PORT_IMGI)
        {
            imgiSize = data.mBuffer->getImgSize();
            break;
        }
    }
    // set mdp crop info
    for(auto data : enqueParams.mvOut)
    {
        // Both MDMA and WROT need to set CRZ crop info.
        MCrpRsInfo mdp_crop;
        MCrpRsInfo crz_crop;
        if(data.mPortID==PORT_WDMA)
        {
            // MDP
            mdp_crop.mGroupID = 2;
            MSize const& imgSize = data.mBuffer->getImgSize();
            mdp_crop.mCropRect = MCropRect(imgSize.w, imgSize.h);
            // CRZ
            crz_crop.mGroupID = 1;
            crz_crop.mCropRect = MCropRect(imgiSize.w, imgiSize.h);
            crz_crop.mResizeDst = imgiSize;
            if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_PREVIEW)
                outputImageSize = imgSize;
        }
        else if(data.mPortID==PORT_WROT)
        {
            // MDP
            mdp_crop.mGroupID = 3;
            MSize const& imgSize = data.mBuffer->getImgSize();
            MSize dstSize = ( data.mTransform == eTransform_ROT_90 || data.mTransform == eTransform_ROT_270 )
                                ? MSize(imgSize.h, imgSize.w)
                                : imgSize;
            if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_RECORD||
            scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_CAPTURE)
                outputImageSize = dstSize;
            mdp_crop.mCropRect = MCropRect(dstSize.w, dstSize.h);
            // CRZ
            crz_crop.mGroupID = 1;
            crz_crop.mCropRect = MCropRect(imgiSize.w, imgiSize.h);
            crz_crop.mResizeDst = imgiSize;
        }
        if(miPipeLogEnable)
        {
            MY_LOGD("IMGI size(%dx%d)",
                    imgiSize.w,
                    imgiSize.h);
            MY_LOGD("P2B MDP setting: GroupId(%d) CropRect(%dx%d)",
                    mdp_crop.mGroupID,
                    mdp_crop.mCropRect.s.w,
                    mdp_crop.mCropRect.s.h);
            MY_LOGD("P2B CRZ setting: GroupId(%d) CropRect(%dx%d)",
                    crz_crop.mGroupID,
                    crz_crop.mCropRect.s.w,
                    crz_crop.mCropRect.s.h);
        }
        enqueParams.mvCropRsInfo.push_back(mdp_crop);
        enqueParams.mvCropRsInfo.push_back(crz_crop);
    }
    {
        // set SRZ 3
        setSRZInfo(enqueParams, EDipModule_SRZ3, miDmgi, outputImageSize);
        // set SRZ 4
        if(miMode == ADVANCE)
        {
            setSRZInfo(enqueParams, EDipModule_SRZ4, miDepi, outputImageSize);
        }
    }
    //
    {
        data = new enqueData();
        //
        data->mpTuningBuffer = pTuningBuffer;
        //data->mEnquedSmartImgBufMap.add(BOKEH_ER_BUF_DMG, dmgBuf);
        //data->mEnquedSmartImgBufMap.add(BOKEH_ER_BUF_DMBG, dmbgBuf);
        if(pWDMABuffer!=nullptr)
        {
            data->mEnquedSmartImgBufMap.add(BOKEH_ER_BUF_WDMAIMG, pWDMABuffer);
        }
        if(nr3dFlag && scenarioId != ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_CAPTURE)
        {
            MY_LOGD_IF(miPipeLogEnable, "3DNR enable");
            data->is3DNROn = MTRUE;
            MINT32 nr3dGMVx = params->getInt(VSDOF_FRAME_GMV_X);
            MINT32 nr3dGMVy = params->getInt(VSDOF_FRAME_GMV_Y);
            MINT32 nr3dCMVx = params->getInt(VSDOF_FRAME_CMV_X);
            MINT32 nr3dCMVy = params->getInt(VSDOF_FRAME_CMV_Y);
            MINT32 nr3dISO = params->getInt(VSDOF_FRAME_ISO);
            if(mbIsFirstRun3dnr)
            {
                data->isFrist3NDRFrame = MTRUE;
            }
            nr3dConfigData config = {request->getRequestNo(), nr3dISO, nr3dGMVx, nr3dGMVy, nr3dCMVx, nr3dCMVy};
            setting3DNR(config, enqueParams);
            mbIsFirstRun3dnr = MFALSE;
            data->request = request;
            data->scenarioId = scenarioId;
        }
        else
        {
            data->request = request;
            data->scenarioId = scenarioId;
        }
    }
	data->cookies = this;
    enqueParams.mpfnCallback = onP2Callback;
    enqueParams.mpfnEnQFailCallback = onP2FailCallback;
    enqueParams.mpCookie = data;
    CAM_TRACE_BEGIN("VSDOF_P2BNode::NormalStream::enque");
    data->start = std::chrono::system_clock::now();
    ret = mpINormalStream->enque(enqueParams);
    CAM_TRACE_END();
    if(!ret)
    {
        MY_LOGE("mpINormalStream enque failed");
        goto lbExit;
    }
    {
        android::Mutex::Autolock lock(mP2DoneLock);
        //inputCount++;
        this->incExtThreadDependency();
        //mvEnqueData.push(data);
    }
    // dump debug information
    if(miPipeLogEnable)
    {
        MY_LOGD("scenarioId(%d) nr3dFlag(%d) gsensorOri(%d)",
                scenarioId,
                nr3dFlag,
                gsensorOri);
    }
lbExit:
    if(!ret)
    {
        if(pTuningBuffer!= nullptr)
            pTuningBuffer = nullptr;
        if(data != nullptr)
            delete data;
    }
    return ret;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
getP2AOutputSize(
    MINT32 scenarioId,
    Pass2SizeInfo& outSize)
{
    if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_PREVIEW)
    {
        StereoSizeProvider::getInstance()->getPass2SizeInfo(
                                            PASS2A,
                                            eSTEREO_SCENARIO_PREVIEW,
                                            outSize);
    }
    else if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_CAPTURE)
    {
        StereoSizeProvider::getInstance()->getPass2SizeInfo(
                                            PASS2A,
                                            eSTEREO_SCENARIO_CAPTURE,
                                            outSize);
    }
    else if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_RECORD)
    {
        StereoSizeProvider::getInstance()->getPass2SizeInfo(
                                            PASS2A,
                                            eSTEREO_SCENARIO_RECORD,
                                            outSize);
    }
    else
    {
        MY_LOGE("Unsupported scenario(%d)", scenarioId);
        return false;
    }
    return true;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setSRZInfo(
    QParams& param,
    MINT32 modulTag,
    MSize inputSize,
    MSize outputSize)
{
    CAM_TRACE_NAME("VSDOF_P2BNode::setSRZInfo");
    // set SRZ 3
    _SRZ_SIZE_INFO_ *srzInfo = (_SRZ_SIZE_INFO_ *)malloc(sizeof(_SRZ_SIZE_INFO_));
    srzInfo->in_w = inputSize.w;
    srzInfo->in_h = inputSize.h;
    srzInfo->crop_w = inputSize.w;
    srzInfo->crop_h = inputSize.h;
    srzInfo->crop_x = 0;
    srzInfo->crop_y = 0;
    srzInfo->crop_floatX = 0;
    srzInfo->crop_floatY = 0;
    srzInfo->out_w = outputSize.w;
    srzInfo->out_h = outputSize.h;
    //
    ModuleInfo moduleInfo;
    moduleInfo.moduleTag = modulTag;
    moduleInfo.frameGroup = 0;
    moduleInfo.moduleStruct = srzInfo;
    //
    if(miPipeLogEnable)
    {
        MY_LOGD("srz moduleTag (%d) in(%dx%d) out(%dx%d)",
            modulTag,
            srzInfo->in_w,
            srzInfo->in_h,
            srzInfo->out_w,
            srzInfo->out_h);
    }
    param.mvModuleData.push_back(moduleInfo);
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
unpackInputBufferFromRequest(
    EffectRequestPtr request,
    QParams& param)
{
    // unpack input buffer
    MY_LOGD_IF(miPipeLogEnable,
            "Input buffer size(%d)",
            request->vInputFrameInfo.size());
    // set input QParams and use frameNo to decide which port to set.
    sp<EffectFrameInfo> pFrameInfo = nullptr;
    for(MINT32 i = 0;i<request->vInputFrameInfo.size();++i)
    {
        pFrameInfo = request->vInputFrameInfo.valueAt(i);
        setP2BInputPort(param, pFrameInfo, pFrameInfo->getFrameNo());
    }
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
unpackOutputBufferFromRequest(
    EffectRequestPtr request,
    QParams& param,
    MINT32 rot)
{
    MY_LOGD_IF(miPipeLogEnable,
            "Output buffer size(%d)",
            request->vOutputFrameInfo.size());
    //
    sp<EffectFrameInfo> pFrameInfo = nullptr;
    for(MINT32 i = 0;i<request->vOutputFrameInfo.size();++i)
    {
        pFrameInfo = request->vOutputFrameInfo.valueAt(i);
        setP2BOutputPort(param, pFrameInfo, pFrameInfo->getFrameNo(), rot);
    }
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setP2BInputPort(
    QParams &param,
    FrameInfoPtr frameInfo,
    MUINT32 bufferType)
{
    if(frameInfo.get() == nullptr)
    {
        MY_LOGD("setP2BInputPort failed: null buffer bufferType(%d)", bufferType);
        return MFALSE;
    }
    //
    sp<IImageBuffer> frame = nullptr;
    frameInfo->getFrameBuffer(frame);
    return setP2BInputPort(param, frame, bufferType);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setP2BInputPort(
    QParams &param,
    SmartImageBuffer imgBuffer,
    MUINT32 bufferType)
{
    if(imgBuffer == nullptr)
    {
        MY_LOGD("setP2BInputPort failed: null buffer bufferType(%d)", bufferType);
        return MFALSE;
    }
    return setP2BInputPort(param, imgBuffer->mImageBuffer, bufferType);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setP2BInputPort(
    QParams &param,
    sp<IImageBuffer> imgBuffer,
    MUINT32 bufferType)
{
    if(imgBuffer == nullptr)
    {
        MY_LOGD("setP2BInputPort failed: null buffer bufferType(%d)", bufferType);
        return MFALSE;
    }
    // parse portID first.
    NSCam::NSIoPipe::PortID portId = mapToPortID(bufferType);
    if(portId.index == EPortIndex_UNKNOW)
    {
        // ignore unknow port.
        MY_LOGD("unmap buffer type(%d)", bufferType);
        return MTRUE;
    }
    Input src;
    //
    // set IMGI
    src.mPortID = portId;
    src.mBuffer = imgBuffer.get();
    //
    MY_LOGD_IF(miPipeLogEnable,
                "BufType(%d) PortID: %d Buf: 0x%x fmt(%x)",
                bufferType,
                src.mPortID.index,
                src.mBuffer,
                imgBuffer->getImgFormat());
    param.mvIn.push_back(src);
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setP2BOutputPort(
    QParams &param,
    FrameInfoPtr frameInfo,
    MUINT32 bufferType,
    MINT32 transform)
{
    if(frameInfo.get() == nullptr)
    {
        MY_LOGD("setP2BOutputPort failed: null buffer bufferType(%d)", bufferType);
        return MFALSE;
    }
    //
    sp<IImageBuffer> frame = nullptr;
    frameInfo->getFrameBuffer(frame);
    return setP2BOutputPort(param, frame, bufferType, transform);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setP2BOutputPort(
    QParams &param,
    SmartImageBuffer imgBuffer,
    MUINT32 bufferType,
    MINT32 transform)
{
    if(imgBuffer == nullptr)
    {
        MY_LOGD("setP2BOutputePort failed: null buffer bufferType(%d)", bufferType);
        return MFALSE;
    }
    return setP2BOutputPort(param, imgBuffer->mImageBuffer, bufferType, transform);
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
setP2BOutputPort(
    QParams &param,
    sp<IImageBuffer> imgBuffer,
    MUINT32 bufferType,
    MINT32 transform)
{
    if(imgBuffer == nullptr)
    {
        MY_LOGD("setP2BOutputPort failed: null buffer bufferType(%d)", bufferType);
        return MFALSE;
    }
    // parse portID first.
    NSCam::NSIoPipe::PortID portId = mapToPortID(bufferType);
    if(portId.index == EPortIndex_UNKNOW)
    {
        // ignore unknow port.
        MY_LOGD("unmap buffer type(%d)", bufferType);
        return MTRUE;
    }
    Output src;
    //
    // set IMGI
    src.mPortID = portId;
    src.mBuffer = imgBuffer.get();
    if(portId == PORT_WROT)
    {
        src.mTransform = transform;
    }
    else
        src.mTransform = 0;
    //
    MY_LOGD_IF(miPipeLogEnable,
                "BufType(%d) PortID: %d Buf: 0x%x fmt(%x)",
                bufferType,
                src.mPortID.index,
                src.mBuffer,
                imgBuffer->getImgFormat());
    param.mvOut.push_back(src);
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
VSDOF_P2BNode::
onP2Callback(
    QParams& rParams)
{
    enqueData* pEnqueData = (enqueData*) (rParams.mpCookie);
	if(nullptr!=pEnqueData->cookies)
	{
		VSDOF_P2BNode* pP2B = (VSDOF_P2BNode*)pEnqueData->cookies;
		pP2B->handleP2Done(rParams);
	}
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
VSDOF_P2BNode::
handleP2Done(
    QParams& rParams)
{
    CAM_TRACE_NAME("VSDOF_P2BNode::handleP2Done");
    MY_LOGD_IF(miPipeLogEnable, "+");
    //
    MUINT32 scenarioId = eSTEREO_SCENARIO_UNKNOWN;
    enqueData *data = (enqueData*)rParams.mpCookie;
    EffectRequestPtr request = nullptr;
    if(nullptr == data)
    {
        MY_LOGE("enqueData is null");
        return;
    }
    {
        data->end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = data->end-data->start;
        VSDOF_PRFLOG("VSDOF_Profile: reqID=%d p2b processing time(%lf ms)", data->request->getRequestNo(), elapsed_seconds.count()*1000);
    }
    {
        scenarioId = data->scenarioId;
        request = data->request;
    }
    MY_LOGD_IF(miPipeLogEnable,
            "RequestId(%d) in(%d) out(%d)",
            request->getRequestNo(),
            rParams.mvIn.size(),
            rParams.mvOut.size());
    //
    if(data->is3DNROn)
    {
        for(auto outParam : rParams.mvOut)
        {
            NSCam::NSIoPipe::PortID portId = outParam.mPortID;
            portId.group = 0;
            MUINT32 bufDataType = mapToBufferID( portId,  scenarioId);
            //
            if(bufDataType == BOKEH_ER_BUF_3DNR_OUTPUT)
            {
                // swap buffer
                SmartImageBuffer temp = mpOutputFrame;
                //sp<IImageBuffer> temp = mpOutputFrame;
                mpOutputFrame = mpInputFrame;
                mpInputFrame = temp;
                temp = nullptr;
            }
        }
    }
    if(mShowDepthMap > 0)
    {
        IImageBuffer* input = nullptr;
        IImageBuffer* output = nullptr;
        for(auto inParam : rParams.mvIn)
        {
            NSCam::NSIoPipe::PortID portId = inParam.mPortID;
            MUINT32 bufDataType = mapToBufferID( portId,  scenarioId);
            if(bufDataType == BOKEH_ER_BUF_DMG)
            {
                input = inParam.mBuffer;
                break;
            }
        }
        for(auto outParam : rParams.mvOut)
        {
            NSCam::NSIoPipe::PortID portId = outParam.mPortID;
            MUINT32 bufDataType = mapToBufferID( portId,  scenarioId);
            if(bufDataType == BOKEH_ER_BUF_DISPLAY)
            {
                output = outParam.mBuffer;
                break;
            }
        }
        // mark cache to invalid, ensure get buffer from memory.
        input->syncCache(eCACHECTRL_INVALID);
        shiftDepthMapValue(input, 2);
        input->syncCache(eCACHECTRL_FLUSH);
        outputDepthMap(request->getRequestNo(), input, output);
    }
    // dump out port
    if(mbDumpImageBuffer)
    {
        MY_LOGD("Dump image(%d)+", request->getRequestNo());
        makePath("/sdcard/vsdof/bokeh/result/p2b/", 0660);
        std::string saveFileName = "";
        for(auto outParam : rParams.mvOut)
        {
            NSCam::NSIoPipe::PortID portId = outParam.mPortID;
            IImageBuffer* buf = outParam.mBuffer;
            portId.group = 0;
            MUINT32 bufDataType = mapToBufferID( portId,  scenarioId);
            if(portId == PORT_WDMA)
            {
                saveFileName = std::string("/sdcard/vsdof/bokeh/result/p2b/WDMA_")+
                               std::to_string(scenarioId)+ std::string("_")+
                               std::to_string(request->getRequestNo())+
                               std::string(".yuv");
            }
            else if(portId == PORT_WROT)
            {
                saveFileName = std::string("/sdcard/vsdof/bokeh/result/p2b/WROT_")+
                               std::to_string(scenarioId)+ std::string("_")+
                               std::to_string(request->getRequestNo())+
                               std::string(".yuv");
            }
            buf->saveToFile(saveFileName.c_str());
        }
        MY_LOGD("Dump image-");
    }
    //
    {
        //android::Mutex::Autolock lock(mP2DoneLock);
        //MY_LOGD("isRunning(%d)", isRunning);
        //if(isRunning)
        {
            if(scenarioId == ENUM_STEREO_SCENARIO::eSTEREO_SCENARIO_CAPTURE)
            {
                handleData(P2B_MDP_REQUEST, request);
                MY_LOGD_IF(miPipeLogEnable, "Capture output");
                /*ssize_t index = data->mEnquedSmartImgBufMap.indexOfKey(BOKEH_ER_BUF_WDMAIMG);
                if(index>=0)
                {
                    SmartImageBuffer buffer = data->mEnquedSmartImgBufMap.valueAt(index);
                    handleData(P2B_MDP_WDMA, buffer);
                }
                else
                {
                    MY_LOGE("It does not contain WDMA buffer in capture scenario.");
                }*/
            }
            else
            {
                handleData(P2B_OUT, request);
                MY_LOGD_IF(miPipeLogEnable, "Preview/Record");
            }
        }
    }
    // release smartBuffer
    // sb contain tuning buffer, so clear mEnquedSmartImgBufMap can return tuning
    // buffer to sbPool.
    data->mEnquedSmartImgBufMap.clear();
    data->mpTuningBuffer = nullptr;
    delete data;
    // release malloc buffer
    for(auto moduleInfo : rParams.mvModuleData)
    {
        if(moduleInfo.moduleTag == EDipModule_SRZ3 ||
           moduleInfo.moduleTag == EDipModule_SRZ4)
        {
            free(moduleInfo.moduleStruct);
        }
    }
    this->decExtThreadDependency();
    MY_LOGD_IF(miPipeLogEnable, "-");
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
VSDOF_P2BNode::
onP2FailCallback(
    QParams& rParams)
{
    CAM_TRACE_NAME("VSDOF_P2BNode::onP2FailCallback");
    MY_LOGE("Process p2 fail.");
    enqueData* pEnqueData = (enqueData*) (rParams.mpCookie);
	if(nullptr!=pEnqueData->cookies)
	{
		VSDOF_P2BNode* pP2B = (VSDOF_P2BNode*)pEnqueData->cookies;
		pP2B->handleP2Fail(rParams);
	}
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
VSDOF_P2BNode::
handleP2Fail(
    QParams& rParams)
{
    //
    MUINT32 scenarioId = eSTEREO_SCENARIO_UNKNOWN;
    enqueData *data = (enqueData*)rParams.mpCookie;
    EffectRequestPtr request = nullptr;
	if(nullptr == data)
    {
		MY_LOGE("enqueData is null");
		return;
	}
        {
            scenarioId = data->scenarioId;
            request = data->request;
    }
    // release smartBuffer
    // sb contain tuning buffer, so clear mEnquedSmartImgBufMap can return tuning
    // buffer to sbPool.
    data->mEnquedSmartImgBufMap.clear();
    data->mpTuningBuffer = nullptr;
    this->decExtThreadDependency();
    delete data;
}
/*******************************************************************************
 *
 ********************************************************************************/
NSCam::NSIoPipe::PortID
VSDOF_P2BNode::
mapToPortID(
    const MUINT32 bufDataType)
{
    switch(bufDataType)
    {
        case BOKEH_ER_BUF_MAIN1:
            return PORT_IMGI;
            break;
        case BOKEH_ER_BUF_DMG:
            return PORT_DEPI;
            break;
        case BOKEH_ER_BUF_DMBG:
            return PORT_DMGI;
            break;
        case BOKEH_ER_BUF_3DNR_OUTPUT:
            return PORT_IMG3O;
            break;
        case BOKEH_ER_BUF_VSDOF_IMG:
        case BOKEH_ER_BUF_RECORD:
            return PORT_WROT;
            break;
        case BOKEH_ER_BUF_DISPLAY:
        case BOKEH_ER_BUF_WDMAIMG:
            return PORT_WDMA;
            break;
        case BOKEH_ER_BUF_3DNR_INPUT:
            return PORT_VIPI;
            break;
        /*case BOKEH_ER_BUF_CLEAN_IMG:
            break;*/
        default:
            //MY_LOGE("mapToPortID: not exist bufDataType=%d", bufDataType);
            break;
    }
    NSCam::NSIoPipe::PortID unsupportPort;
    unsupportPort.index = EPortIndex_UNKNOW;
    return unsupportPort;
}
/*******************************************************************************
 *
 ********************************************************************************/
MUINT32
VSDOF_P2BNode::
mapToBufferID(
    NSCam::NSIoPipe::PortID const portId,
    const MUINT32 scenarioID)
{
    if(portId == PORT_IMGI)
    {
        return BOKEH_ER_BUF_MAIN1;
    }
    if(portId == PORT_DMGI)
    {
        return BOKEH_ER_BUF_DMG;
    }
    if(portId == PORT_DEPI)
    {
        return BOKEH_ER_BUF_DMBG;
    }
    if(portId == PORT_IMG3O)
    {
        return BOKEH_ER_BUF_3DNR_OUTPUT;
    }
    if(portId == PORT_WROT)
    {
        if(scenarioID == eSTEREO_SCENARIO_CAPTURE)
            return BOKEH_ER_BUF_VSDOF_IMG;
        else if(scenarioID == eSTEREO_SCENARIO_RECORD)
            return BOKEH_ER_BUF_RECORD;
    }
    if(portId == PORT_WDMA)
    {
        if(scenarioID == eSTEREO_SCENARIO_CAPTURE)
            return BOKEH_ER_BUF_WDMAIMG;
        if(scenarioID == eSTEREO_SCENARIO_RECORD || scenarioID == eSTEREO_SCENARIO_PREVIEW)
            return BOKEH_ER_BUF_DISPLAY;
    }
    return BOKEH_ER_BUF_NONE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
getFrameInfoFromRequest(
    EffectRequestPtr request,
    FrameInfoPtr& frame,
    MINT32 bufType,
    MBOOL isInputPort)
{
    ssize_t keyIndex = -1;
    if(isInputPort)
    {
        keyIndex = request->vInputFrameInfo.indexOfKey(bufType);
        if(keyIndex>=0)
        {
            frame = request->vInputFrameInfo.valueAt(keyIndex);
            return MTRUE;
        }
        else
        {
            return MFALSE;
        }
    }
    else
    {
        keyIndex = request->vOutputFrameInfo.indexOfKey(bufType);
        if(keyIndex>=0)
        {
            frame = request->vOutputFrameInfo.valueAt(keyIndex);
            return MTRUE;
        }
        else
        {
            return MFALSE;
        }
    }
    return MFALSE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
VSDOF_P2BNode::
setting3DNR(
    nr3dConfigData config,
    QParams& rParams)
{
    CAM_TRACE_NAME("VSDOF_P2BNode::setting3DNR");
    FUNC_START;
    ModuleInfo moduleinfo;
    NR3DParam* pNr3dParam;
    sp<IImageBuffer> nr3dInputBuffer = mpInputFrame->mImageBuffer.get();
    //sp<IImageBuffer> nr3dInputBuffer = mpInputFrame;
    MUINT32 nr3dInputBuffer_w = nr3dInputBuffer->getImgSize().w;
    MUINT32 nr3dInputBuffer_h = nr3dInputBuffer->getImgSize().h;
    MY_LOGD_IF(miPipeLogEnable, "setting3DNR - %d", config.frameId);
    if (MTRUE != mp3dnr->prepare(config.frameId, config.ISO))
    {
        MY_LOGE("3dnr prepare err");
    }
    if (MTRUE != mp3dnr->setGMV(config.frameId,
                                config.GMV_x,
                                config.GMV_y,
                                config.CMV_x,
                                config.CMV_y))
    {
        MY_LOGE("3dnr setGMV err");
    }
    if (MTRUE != mp3dnr->checkIMG3OSize(config.frameId,
                                        nr3dInputBuffer_w,
                                        nr3dInputBuffer_h))
    {
        MY_LOGE("3dnr checkIMG3OSize err");
    }
    if(mbIsFirstRun3dnr)
    {
        if (MTRUE != mp3dnr->setVipiParams(MFALSE, 0, 0, 0, 0))
        {
            MY_LOGW("3dnr setVipiParams err");
        }
    }
    else
    {
        if(!mp3dnr->setVipiParams(nr3dInputBuffer!=NULL,
                           nr3dInputBuffer_w, nr3dInputBuffer_h,
                           mpInputFrame->mImageBuffer.get()->getImgFormat(), mpInputFrame->mImageBuffer.get()->getBufStridesInBytes(0)))
        /*if(!mp3dnr->setVipiParams(nr3dInputBuffer!=NULL,
                           nr3dInputBuffer_w, nr3dInputBuffer_h,
                           mpInputFrame->getImgFormat(), mpInputFrame->getBufStridesInBytes(0)))*/
        {
            /*NSCam::NSIoPipe::PortID portId = mapToPortID(BOKEH_ER_BUF_3DNR_INPUT);
            if(removeInPortId(rParams, portId))
            {
                MY_LOGE("remove input port fail.");
            }*/
            MY_LOGD("skip setVipiParams flow");
        }
        else
        {
            setP2BInputPort(rParams, mpInputFrame, BOKEH_ER_BUF_3DNR_INPUT);
            MY_LOGD("configVipi: address:%p, W/H(%d,%d)", nr3dInputBuffer.get(),
            nr3dInputBuffer->getImgSize().w, nr3dInputBuffer->getImgSize().h);
        }
    }
    //set for nr3d module
    if (MTRUE != mp3dnr->get3dnrParams(config.frameId,
                                         nr3dInputBuffer_w, nr3dInputBuffer_h, pNr3dParam))
    {
        MY_LOGD("skip get3dnrParams flow");
    }
    else
    {
        //set for nr3d module
        moduleinfo.moduleTag = 0;
        moduleinfo.moduleStruct   = reinterpret_cast<MVOID*> (pNr3dParam);
        rParams.mvModuleData.push_back(moduleinfo);
    }
    if (MTRUE != mp3dnr->checkStateMachine(NR3D_STATE_WORKING))
    {
        /*NSCam::NSIoPipe::PortID portId = mapToPortID(BOKEH_ER_BUF_3DNR_OUTPUT);
        if(removeOutPortId(rParams, portId))
        {
            MY_LOGE("remove output port fail.");
        }*/
    }
    else
    {
        setP2BOutputPort(rParams, mpOutputFrame, BOKEH_ER_BUF_3DNR_OUTPUT);
    }
    FUNC_END;
}

/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
removeInPortId(QParams& rParams, NSCam::NSIoPipe::PortID portId)
{
    int index = -1;
    MBOOL ret = MFALSE;
    for(auto lPortId: rParams.mvIn)
    {
        index++;
        if(lPortId.mPortID.index == portId.index)
        {
            ret = MTRUE;
            break;
        }
    }
    if(index != -1)
        rParams.mvIn.erase(rParams.mvIn.begin() + index);
    return ret;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
VSDOF_P2BNode::
removeOutPortId(QParams& rParams, NSCam::NSIoPipe::PortID portId)
{
    int index = -1;
    MBOOL ret = MFALSE;
    for(auto lPortId: rParams.mvOut)
    {
        index++;
        if(lPortId.mPortID.index == portId.index)
        {
            ret = MTRUE;
            break;
        }
    }
    if(index != -1)
        rParams.mvOut.erase(rParams.mvOut.begin() + index);
    return ret;
}
//************************************************************************
//
//************************************************************************
sp<IImageBuffer>
VSDOF_P2BNode::
createEmptyImageBuffer(
    const IImageBufferAllocator::ImgParam imgParam,
    const char* name,
    MINT usage)
{
    IImageBufferAllocator *allocator = IImageBufferAllocator::getInstance();
    sp<IImageBuffer> pImgBuf = allocator->alloc(name, imgParam);
    pImgBuf->lockBuf(name, usage);
    return pImgBuf;
}
//************************************************************************
//
//************************************************************************
MVOID
VSDOF_P2BNode::
shiftDepthMapValue(
    IImageBuffer* depthMap,
    MUINT8 shiftValue
)
{
    // offset value
    MUINT8* data = (MUINT8*)depthMap->getBufVA(0);
    MSize const size = depthMap->getImgSize();
    for(int i=0;i<size.w*size.h;++i)
    {
        *data = *data << shiftValue;
        *data = (MUINT8)std::max(0, std::min((int)*data, 255));
        data++;
    }
}
//************************************************************************
//
//************************************************************************
MVOID
VSDOF_P2BNode::
outputDepthMap(MINT32 id,
    IImageBuffer* depthMap,
    IImageBuffer* displayResult)
{
    if(displayResult == nullptr || displayResult->getPlaneCount()!=3)
        return;
    if(mpDpStream == nullptr)
        return;
    //
    SmartImageBuffer mdpBuffer = mpDepthMapBufPool->request();
    //
    VSDOF::util::sMDP_Config config;
    config.pDpStream = mpDpStream;
    config.pSrcBuffer = depthMap;
    config.pDstBuffer = mdpBuffer->mImageBuffer.get();
    config.rotAngle = 0;
    if(!excuteMDP(config))
    {
        MY_LOGE("excuteMDP fail.");
        return;
    }
    //
    MSize outImgSize = displayResult->getImgSize();
    MSize inImgSize = mdpBuffer->mImageBuffer->getImgSize();
    char* outAddr0 = (char*)displayResult->getBufVA(0);
    char* outAddr1 = (char*)displayResult->getBufVA(1);
    char* outAddr2 = (char*)displayResult->getBufVA(2);
    char* inAddr = (char*)mdpBuffer->mImageBuffer->getBufVA(0);
    MINT32 halfInWidth = inImgSize.w >> 1;
    MINT32 halfInHeight = inImgSize.h >> 1;
    MINT32 halfOutWidth = outImgSize.w  >> 1;
    //
    for(int i=0;i<inImgSize.h;++i)
    {
        memcpy(outAddr0, inAddr, inImgSize.w);
        outAddr0 += outImgSize.w;
        inAddr += inImgSize.w;
    }
    //
    for(int i=0;i<halfInHeight;++i)
    {
        memset(outAddr1, 128, halfInWidth);
        memset(outAddr2, 128, halfInWidth);
        outAddr1 += halfOutWidth;
        outAddr2 += halfOutWidth;
    }
    mdpBuffer = nullptr;
    return;
}
//************************************************************************
//
//************************************************************************
MVOID
VSDOF_P2BNode::
outputDepthMapAndAddDebugInfo(MINT32 id,
    IImageBuffer* depthMap,
    IImageBuffer* displayResult)
{
    if(displayResult == nullptr || displayResult->getPlaneCount()!=3)
        return;
    if(mpDpStream == nullptr)
        return;
    // get center four point
    {
        MUINT8 values[4];
        MINT32 width = depthMap->getImgSize().w;
        MINT32 height = depthMap->getImgSize().h;
        MINT32 halfWidth = width>>1;
        MINT32 halfHight = height>>1;
        values[0] = ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-2) + (halfWidth-1)];
        values[1] = ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-2) + (halfWidth)];
        values[2] = ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-1) + (halfWidth-1)];
        values[3] = ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-1) + (halfWidth)];
        MY_LOGD("lu(%d) ru(%d) ld(%d) rd(%d)", values[0], values[1], values[2], values[3]);
        ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-2) + (halfWidth-1)] = 0;
        ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-2) + (halfWidth)] = 0;
        ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-1) + (halfWidth-1)] = 0;
        ((MUINT8*)depthMap->getBufVA(0))[width*(halfHight-1) + (halfWidth)] = 0;
    }
    //
    SmartImageBuffer mdpBuffer = mpDepthMapBufPool->request();
    //
    VSDOF::util::sMDP_Config config;
    config.pDpStream = mpDpStream;
    config.pSrcBuffer = depthMap;
    config.pDstBuffer = mdpBuffer->mImageBuffer.get();
    config.rotAngle = 0;
    if(!excuteMDP(config))
    {
        MY_LOGE("excuteMDP fail.");
        return;
    }
    //
    MSize outImgSize = displayResult->getImgSize();
    MSize inImgSize = mdpBuffer->mImageBuffer->getImgSize();
    char* outAddr0 = (char*)displayResult->getBufVA(0);
    char* outAddr1 = (char*)displayResult->getBufVA(1);
    char* outAddr2 = (char*)displayResult->getBufVA(2);
    char* inAddr = (char*)mdpBuffer->mImageBuffer->getBufVA(0);
    MINT32 halfInWidth = inImgSize.w >> 1;
    MINT32 halfInHeight = inImgSize.h >> 1;
    MINT32 halfOutWidth = outImgSize.w  >> 1;
    //
    for(int i=0;i<inImgSize.h;++i)
    {
        memcpy(outAddr0, inAddr, inImgSize.w);
        outAddr0 += outImgSize.w;
        inAddr += inImgSize.w;
    }
    //
    for(int i=0;i<halfInHeight;++i)
    {
        memset(outAddr1, 128, halfInWidth);
        memset(outAddr2, 128, halfInWidth);
        outAddr1 += halfOutWidth;
        outAddr2 += halfOutWidth;
    }
    mdpBuffer = nullptr;
    return;
}
