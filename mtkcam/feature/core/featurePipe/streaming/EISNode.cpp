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


#include <system/thread_defs.h>
#include <sys/resource.h>
#include <utils/ThreadDefs.h>


#include "EISNode.h"
#include "GpuWarpBase.h"
#include <mtkcam/drv/iopipe/CamIO/IHalCamIO.h>
#include <mtkcam/feature/eis/eis_hal.h>


#define PIPE_CLASS_TAG "EISNode"
#define PIPE_TRACE TRACE_EIS_NODE
#include <featurePipe/core/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

EISNode::EISNode(const char *name)
    : StreamingFeatureNode(name)
    , mAlgCounter (0)
    , mpEisHal(NULL)
{
    TRACE_FUNC_ENTER();
    mEisPlusGpuInfo.gridW = mEisPlusGpuInfo.gridH = 0;
    this->addWaitQueue(&mRequests);
    this->addWaitQueue(&mConfigDatas);
    TRACE_FUNC_EXIT();
}

EISNode::~EISNode()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
}

MBOOL EISNode::onData(DataID id, const RequestPtr &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data->mRequestNo, ID2Name(id));
    MBOOL ret = MFALSE;
    if( id == ID_P2A_TO_EIS_P2DONE )
    {
        mRequests.enque(FMData(FMResult(), data));
        ret = MTRUE;
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL EISNode::onData(DataID id, const EisConfigData &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data.mRequest->mRequestNo, ID2Name(id));
    MBOOL ret = MFALSE;
    if( id == ID_P2A_TO_EIS_CONFIG )
    {
        mConfigDatas.enque(data);
        ret = MTRUE;
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL EISNode::onData(DataID id, const FMData &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data.mRequest->mRequestNo, ID2Name(id));
    MBOOL ret = MFALSE;
    if( id == ID_P2A_TO_EIS_FM )
    {
        mRequests.enque(data);
        ret = MTRUE;
    }

    if (mpEisHal)
    {
        mpEisHal->ForcedDoEisPass2();
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL EISNode::onInit()
{
    TRACE_FUNC_ENTER();
    StreamingFeatureNode::onInit();

    mpEisHal = EisHal::CreateInstance("FeaturePipe_EisNode", mSensorIndex);
    if (mpEisHal == NULL)
    {
        MY_LOGE("FeaturePipe_EisNode CreateInstance failed");
        return MFALSE;
    }

    if(EIS_RETURN_NO_ERROR != mpEisHal->Init())
    {
        MY_LOGE("mpEisHal init failed");
        return MFALSE;
    }

    mpEisHal->GetEisPlusGpuInfo(&mEisPlusGpuInfo, mUsageHint.mEisMode);
    mAlgCounter = 0;

    int w = mEisPlusGpuInfo.gridW*4, h = mEisPlusGpuInfo.gridH*2;
    mWarpMapBufferPool = ImageBufferPool::create("EISNode", w, h, eImgFmt_BAYER8, ImageBufferPool::USAGE_SW);

    while (!mQueue.empty())
    {
        mQueue.pop();
    }

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL EISNode::onUninit()
{
    TRACE_FUNC_ENTER();

    while (!mQueue.empty())
    {
        mQueue.pop();
    }

    if (mpEisHal)
    {
        mpEisHal->Uninit();
        mpEisHal->DestroyInstance("FeaturePipe_EisNode");
        mpEisHal = NULL;
    }
    mAlgCounter = 0;

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL EISNode::onThreadStart()
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    if( mWarpMapBufferPool != NULL )
    {
        int EIS_WARP_MAP_NUM = 6;
        mWarpMapBufferPool->allocate(EIS_WARP_MAP_NUM);
        ret = MTRUE;
    }

    ::setpriority(PRIO_PROCESS, 0, (ANDROID_PRIORITY_FOREGROUND-6));

    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL EISNode::onThreadStop()
{
    TRACE_FUNC_ENTER();
    ImageBufferPool::destroy(mWarpMapBufferPool);
    mWarpMapBufferPool = NULL;
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL EISNode::onThreadLoop()
{
    TRACE_FUNC("Waitloop");
    RequestPtr request;
    FMData p2aRequest;
    EisConfigData configData;

    if( waitAllQueue() )
    {
        if( !mRequests.deque(p2aRequest) )
        {
            MY_LOGE("P2A done deque out of sync");
            return MFALSE;
        }
        if( !mConfigDatas.deque(configData) )
        {
            MY_LOGE("ConfigData deque out of sync");
            return MFALSE;
        }
        if( p2aRequest.mRequest == NULL ||
            p2aRequest.mRequest != configData.mRequest )
        {
            MY_LOGE("Request out of sync");
            return MFALSE;
        }
        TRACE_FUNC_ENTER();
        request = p2aRequest.mRequest;
        request->mTimer.startEIS();
        TRACE_FUNC("Frame %d in EIS", request->mRequestNo);
        if( request->needEIS25() )
        {
            processEIS25(request, configData.mData, p2aRequest.mData);
        }
        else if( request->needEIS22() )
        {
            processEIS22(request, configData.mData);
        }
        else
        {
            handleWarpData(NULL, request);
        }
        request->mTimer.stopEIS();
    }
    if( mNodeSignal->getStatus(NodeSignal::STATUS_IN_FLUSH) )
    {
        this->flushQueue();
    }
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL EISNode::processEIS22(const RequestPtr &request, const EIS_HAL_CONFIG_DATA &config)
{
    TRACE_FUNC_ENTER();
    EIS_HAL_CONFIG_DATA eisHalCfgData;

    MINT64 expTime = 0;
    MINT64 ts = 0;
    ImgBuffer warpMap;

    warpMap = mWarpMapBufferPool->requestIIBuffer();

    expTime = request->getVar<MINT32>("eis.expTime", expTime);
    ts = request->getVar<MINT64>("eis.timeStamp", ts);
    memcpy(&eisHalCfgData, &config, sizeof(config));

    mpEisHal->SetEisPlusGpuInfo((MINT32 *) (warpMap->getImageBuffer())->getBufVA(0),
                                (MINT32 *) ((warpMap->getImageBuffer())->getBufVA(0) + mEisPlusGpuInfo.gridW*mEisPlusGpuInfo.gridH*4));


    if( request->is4K2K() )
    {
        GpuWarpBase::makePassThroughWarp(warpMap->getImageBuffer(), request->mFullImgSize);
    }
    else
    {
        mpEisHal->DoGis(&eisHalCfgData, ts, expTime);
    }

    handleWarpData(warpMap, request);

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL EISNode::processEIS25(const RequestPtr &request, const EIS_HAL_CONFIG_DATA &config, const FMResult &fm)
{
    TRACE_FUNC_ENTER();

    EIS_HAL_CONFIG_DATA eisHalCfgData;
    unsigned waitSize = 0;
    MINT64 expTime = 0;
    MINT64 ts = 0;
    MINT32 eisMode = 0;
    MINT32 algMode = 0;
    MINT32 algCounter = 0;
    MINT32 eisCounter = 0;
    ImgBuffer warpMap;
    EISStore last;
    FEFM_PACKAGE fefmData;

    if( request->needEIS_Q() )
    {
        waitSize = EISCustom::isEnabledEIS25_Queue() ? EIS_QUEUE_SIZE : 0;
    }

    ts = request->getVar<MINT64>("eis.timeStamp", ts);
    expTime = request->getVar<MINT32>("eis.expTime", expTime);

    eisMode = request->getVar<MINT32>("eis.eisMode", eisMode);
    eisCounter = request->getVar<MINT32>("eis.eisCounter", eisCounter);

    MY_LOGD("eisMode:%d, eisCounter:%d", eisMode, eisCounter);
    warpMap = mWarpMapBufferPool->requestIIBuffer();

    memset(&eisHalCfgData,0,sizeof(eisHalCfgData));

    eisHalCfgData.gmv_X = request->getVar<MINT32>("eis.gmv_x", eisHalCfgData.gmv_X);
    eisHalCfgData.gmv_Y = request->getVar<MINT32>("eis.gmv_y", eisHalCfgData.gmv_Y);
    eisHalCfgData.confX = request->getVar<MINT32>("eis.confX", eisHalCfgData.confX);
    eisHalCfgData.confY = request->getVar<MINT32>("eis.confY", eisHalCfgData.confY);

    IImageBuffer *FMFwHigh_Ptr = 0;
    IImageBuffer *FMFwMedium_Ptr = 0;
    IImageBuffer *FMFwLow_Ptr = 0;

    IImageBuffer *FMBwHigh_Ptr = 0;
    IImageBuffer *FMBwMedium_Ptr = 0;
    IImageBuffer *FMBwLow_Ptr = 0;

    IImageBuffer *FEHigh_Ptr = 0;
    IImageBuffer *FEMedium_Ptr = 0;
    IImageBuffer *FELow_Ptr;

    IImageBuffer *FEPreHigh_Ptr = 0;
    IImageBuffer *FEPreMedium_Ptr = 0;
    IImageBuffer *FEPreLow_Ptr = 0;

    IImageBuffer *FMA_Regiester_wHigh_Ptr = 0;
    IImageBuffer *FMA_Regiester_wMedium_Ptr = 0;
    IImageBuffer *FMA_Regiester_wLow_Ptr = 0;

    IImageBuffer *FMB_Regiester_wHigh_Ptr = 0;
    IImageBuffer *FMB_Regiester_wMedium_Ptr = 0;
    IImageBuffer *FMB_Regiester_wLow_Ptr = 0;

    if (fm.FM_A.isValid())
    {
        //Previous to Current
        FMFwHigh_Ptr = fm.FM_A.High->getImageBufferPtr();
        FMFwMedium_Ptr = fm.FM_A.Medium->getImageBufferPtr();
        FMFwLow_Ptr = fm.FM_A.Low->getImageBufferPtr();

        FMA_Regiester_wHigh_Ptr   = fm.FM_A.Register_High->getImageBufferPtr();
        FMA_Regiester_wMedium_Ptr = fm.FM_A.Register_Medium->getImageBufferPtr();
        FMA_Regiester_wLow_Ptr    = fm.FM_A.Register_Low->getImageBufferPtr();

        FMFwHigh_Ptr->syncCache(eCACHECTRL_INVALID);
        FMFwMedium_Ptr->syncCache(eCACHECTRL_INVALID);
        FMFwLow_Ptr->syncCache(eCACHECTRL_INVALID);

        FMA_Regiester_wHigh_Ptr->syncCache(eCACHECTRL_INVALID);
        FMA_Regiester_wMedium_Ptr->syncCache(eCACHECTRL_INVALID);
        FMA_Regiester_wLow_Ptr->syncCache(eCACHECTRL_INVALID);

        fefmData.ForwardFM[0] = (MUINT16*)FMFwHigh_Ptr->getBufVA(0);
        fefmData.ForwardFM[1] = (MUINT16*)FMFwMedium_Ptr->getBufVA(0);
        fefmData.ForwardFM[2] = (MUINT16*)FMFwLow_Ptr->getBufVA(0);

        fefmData.ForwardFMREG[0] = *((MUINT32*)FMA_Regiester_wHigh_Ptr->getBufVA(0));
        fefmData.ForwardFMREG[1] = *((MUINT32*)FMA_Regiester_wMedium_Ptr->getBufVA(0));
        fefmData.ForwardFMREG[2] = *((MUINT32*)FMA_Regiester_wLow_Ptr->getBufVA(0));
    }


    if (fm.FM_B.isValid())
    {
        //Current to Previous
        FMBwHigh_Ptr = fm.FM_B.High->getImageBufferPtr();
        FMBwMedium_Ptr = fm.FM_B.Medium->getImageBufferPtr();
        FMBwLow_Ptr = fm.FM_B.Low->getImageBufferPtr();

        FMB_Regiester_wHigh_Ptr   = fm.FM_B.Register_High->getImageBufferPtr();
        FMB_Regiester_wMedium_Ptr = fm.FM_B.Register_Medium->getImageBufferPtr();
        FMB_Regiester_wLow_Ptr    = fm.FM_B.Register_Low->getImageBufferPtr();

        FMBwHigh_Ptr->syncCache(eCACHECTRL_INVALID);
        FMBwMedium_Ptr->syncCache(eCACHECTRL_INVALID);
        FMBwLow_Ptr->syncCache(eCACHECTRL_INVALID);

        FMB_Regiester_wHigh_Ptr->syncCache(eCACHECTRL_INVALID);
        FMB_Regiester_wMedium_Ptr->syncCache(eCACHECTRL_INVALID);
        FMB_Regiester_wLow_Ptr->syncCache(eCACHECTRL_INVALID);

        fefmData.BackwardFM[0] = (MUINT16*)FMBwHigh_Ptr->getBufVA(0);
        fefmData.BackwardFM[1] = (MUINT16*)FMBwMedium_Ptr->getBufVA(0);
        fefmData.BackwardFM[2] = (MUINT16*)FMBwLow_Ptr->getBufVA(0);

        fefmData.BackwardFMREG[0] = *((MUINT32*)FMB_Regiester_wHigh_Ptr->getBufVA(0));
        fefmData.BackwardFMREG[1] = *((MUINT32*)FMB_Regiester_wMedium_Ptr->getBufVA(0));
        fefmData.BackwardFMREG[2] = *((MUINT32*)FMB_Regiester_wLow_Ptr->getBufVA(0));
    }

    if (fm.FE.isValid())
    {
        FEHigh_Ptr = fm.FE.High->getImageBufferPtr();
        FEMedium_Ptr = fm.FE.Medium->getImageBufferPtr();
        FELow_Ptr = fm.FE.Low->getImageBufferPtr();

        FEHigh_Ptr->syncCache(eCACHECTRL_INVALID);
        FEMedium_Ptr->syncCache(eCACHECTRL_INVALID);
        FELow_Ptr->syncCache(eCACHECTRL_INVALID);

        fefmData.FE[0] = (MUINT16*)FEHigh_Ptr->getBufVA(0);
        fefmData.FE[1] = (MUINT16*)FEMedium_Ptr->getBufVA(0);
        fefmData.FE[2] = (MUINT16*)FELow_Ptr->getBufVA(0);
    }

    if (fm.PrevFE.isValid())
    {
        FEPreHigh_Ptr = fm.PrevFE.High->getImageBufferPtr();
        FEPreMedium_Ptr = fm.PrevFE.Medium->getImageBufferPtr();
        FEPreLow_Ptr = fm.PrevFE.Low->getImageBufferPtr();

        FEPreHigh_Ptr->syncCache(eCACHECTRL_INVALID);
        FEPreMedium_Ptr->syncCache(eCACHECTRL_INVALID);
        FEPreLow_Ptr->syncCache(eCACHECTRL_INVALID);

        fefmData.LastFE[0] = (MUINT16*)FEPreHigh_Ptr->getBufVA(0);
        fefmData.LastFE[1] = (MUINT16*)FEPreMedium_Ptr->getBufVA(0);
        fefmData.LastFE[2] = (MUINT16*)FEPreLow_Ptr->getBufVA(0);
    }

    if (waitSize == 0)
    {
        algCounter = mQueue.size();

        if (algCounter == 0 && (eisCounter < EISCustom::getEIS25_StartFrame()) )
        {
            algMode = 0;
            memcpy(&eisHalCfgData, &config, sizeof(config));

            mpEisHal->SetEisPlusGpuInfo((MINT32 *) (warpMap->getImageBuffer())->getBufVA(0),
                                        (MINT32 *) ((warpMap->getImageBuffer())->getBufVA(0) + mEisPlusGpuInfo.gridW*mEisPlusGpuInfo.gridH*4));

            eisHalCfgData.process_idx = algCounter;
            eisHalCfgData.process_mode = algMode;
            mAlgCounter = algCounter;

            mpEisHal->DoFEFMEis(&eisHalCfgData, &fefmData, ts, expTime);

            /* Directly do DZ in GPU */
            if( request->useDirectGpuOut() )
            {
                this->applyDZ(request, MSize(config.crzOutW, config.crzOutH));
            }

            handleWarpData(warpMap, request);
            MY_LOGD("Before Start Queue frame: %d, algMode: %d, algCounter: %d", mQueue.size(), algMode, algCounter);

        }else
        {

            mQueue.push(EISStore(request, config));

            /* Do remaining frames all at once */
            //while( mQueue.size() > waitSize )
            {

                ImgBuffer warpQMap = mWarpMapBufferPool->requestIIBuffer();
                last = mQueue.front();
                mQueue.pop();

                algMode = 4;
                /* Do remaing frames one by one */
                algCounter = (mAlgCounter > 0) ? (--mAlgCounter) : 0;
                /* Do remaing frames all at once */
                //algCounter = (mQueue.size() > 0) ? (mQueue.size() - 1) : 0 ;

                mpEisHal->SetEisPlusGpuInfo((MINT32 *) (warpQMap->getImageBuffer())->getBufVA(0),
                                            (MINT32 *) ((warpQMap->getImageBuffer())->getBufVA(0) + mEisPlusGpuInfo.gridW*mEisPlusGpuInfo.gridH*4));

                last.mConfig.process_idx = algCounter;
                last.mConfig.process_mode = algMode;

                mpEisHal->DoFEFMEis(&last.mConfig, &fefmData, ts, expTime);

                /* Directly do DZ in GPU */
                if( last.mRequest->useDirectGpuOut() )
                {
                    this->applyDZ(last.mRequest, MSize(last.mConfig.crzOutW, last.mConfig.crzOutH));
                }

                handleWarpData(warpQMap, last.mRequest);
                warpQMap = NULL;
                MY_LOGI("Flush mQueue size: %d, algMode: %d, algCounter: %d", mQueue.size(), algMode, algCounter);
            }
        }

    }else
    {

        if (eisCounter == EISCustom::getEIS25_StartFrame())
        {
            algMode = 1;
            algCounter = 0;
            memcpy(&eisHalCfgData, &config, sizeof(config));

            mpEisHal->SetEisPlusGpuInfo((MINT32 *) (warpMap->getImageBuffer())->getBufVA(0),
                                        (MINT32 *) ((warpMap->getImageBuffer())->getBufVA(0) + mEisPlusGpuInfo.gridW*mEisPlusGpuInfo.gridH*4));

            eisHalCfgData.process_idx = algCounter;
            eisHalCfgData.process_mode = algMode;
            mAlgCounter = algCounter;

            mpEisHal->DoFEFMEis(&eisHalCfgData, &fefmData, ts, expTime);

            /* Directly do DZ in GPU */
            if( request->useDirectGpuOut() )
            {
                this->applyDZ(request, MSize(config.crzOutW, config.crzOutH));
            }

            handleWarpData(warpMap, request);

            MY_LOGD("Switch frame: %d, algMode: %d, algCounter: %d", mQueue.size(), algMode, algCounter);

        }else
        {

            mQueue.push(EISStore(request, config));

            if (mQueue.size() <= EIS_QUEUE_SIZE)
            {
                algMode = 2;
                algCounter = mQueue.size();
                mAlgCounter = algCounter;

                memcpy(&eisHalCfgData, &config, sizeof(config));
                mpEisHal->SetEisPlusGpuInfo((MINT32 *) (warpMap->getImageBuffer())->getBufVA(0),
                                            (MINT32 *) ((warpMap->getImageBuffer())->getBufVA(0) + mEisPlusGpuInfo.gridW*mEisPlusGpuInfo.gridH*4));

                eisHalCfgData.process_idx = algCounter;
                eisHalCfgData.process_mode = algMode;

                mpEisHal->DoFEFMEis(&eisHalCfgData, &fefmData, ts, expTime);

                //Do NOT call the handleWarpData
                MY_LOGD("Queue frame: %d, algMode: %d, algCounter: %d", mQueue.size(), algMode, algCounter);

            }else
            {

                last = mQueue.front();
                mQueue.pop();

                algMode = 3;
                algCounter = mQueue.size();
                mAlgCounter = algCounter;

                mpEisHal->SetEisPlusGpuInfo((MINT32 *) (warpMap->getImageBuffer())->getBufVA(0),
                                            (MINT32 *) ((warpMap->getImageBuffer())->getBufVA(0) + mEisPlusGpuInfo.gridW*mEisPlusGpuInfo.gridH*4));

                last.mConfig.process_idx = algCounter;
                last.mConfig.process_mode = algMode;

                mpEisHal->DoFEFMEis(&last.mConfig, &fefmData, ts, expTime);

                /* Directly do DZ in GPU */
                if( last.mRequest->useDirectGpuOut() )
                {
                    this->applyDZ(last.mRequest, MSize(last.mConfig.crzOutW, last.mConfig.crzOutH));
                }

                handleWarpData(warpMap, last.mRequest);
                MY_LOGD("Execute frame: %d, algMode: %d, algCounter: %d", mQueue.size(), algMode, algCounter);
            }

        }

    }
    warpMap = NULL;
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID EISNode::applyDZ(const RequestPtr &request, const MSize &srcSize)
{
    MUINT32 factor = request->is4K2K() ?
                     EISCustom::getEISFactor() : EISCustom::getEISFHDFactor();
    double ratio = 100.0 / factor;

    IImageBuffer *recordBuffer = request->getRecordOutputBuffer();
    MSize dstSize = recordBuffer ? recordBuffer->getImgSize() :
                    MSize(request->mFullImgSize.w*ratio, request->mFullImgSize.h*ratio);

    MCrpRsInfo cropDZ;
    if( !request->getEISRecordCrop(cropDZ, RRZO_DOMAIN) &&
        !request->getEISDisplayCrop(cropDZ, RRZO_DOMAIN) )
    {
        cropDZ.mCropRect.s.w = srcSize.w * ratio;
        cropDZ.mCropRect.s.h = srcSize.h * ratio;
        cropDZ.mCropRect.p_integral.x = (srcSize.w - cropDZ.mCropRect.s.w) >> 1;
        cropDZ.mCropRect.p_integral.y = (srcSize.h - cropDZ.mCropRect.s.h) >> 1;
        MY_LOGW("Frame %d: cannot find dz crop, use full crop (x,y)=(%d,%d), (wxh)=%dx%d",
                 request->mRequestNo,
                 cropDZ.mCropRect.p_integral.x, cropDZ.mCropRect.p_integral.y,
                 cropDZ.mCropRect.s.w, cropDZ.mCropRect.s.h);
    }

    /* Apply DZ in GPU warp matrix */
    mpEisHal->SetEISPlusGpuWarpDZ(srcSize.w, srcSize.h,
                                  dstSize.w, dstSize.h,
                                  cropDZ.mCropRect.p_integral.x , cropDZ.mCropRect.p_integral.y,
                                  cropDZ.mCropRect.s.w, cropDZ.mCropRect.s.h,
                                  factor);
}

MBOOL EISNode::handleWarpData(const ImgBuffer &warp, const RequestPtr &request)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    StreamingFeatureDataID next;
    next = request->needVFB() ? ID_EIS_TO_VFB_WARP : ID_EIS_TO_GPU_WARP;
    ret = handleData(next, ImgBufferData(warp, request));
    TRACE_FUNC_EXIT();
    return ret;
}

MVOID EISNode::flushQueue()
{
    TRACE_FUNC_ENTER();
    MSize lastSize(0, 0);
    ImgBuffer warpMap;

    MY_LOGD("Trigger EIS flush queue: size=%d", mQueue.size());
    while( mQueue.size() )
    {
        EISStore last;
        last = mQueue.front();
        mQueue.pop();

        if( warpMap == NULL ||
            last.mRequest->mFullImgSize.w != lastSize.w ||
            last.mRequest->mFullImgSize.h != lastSize.h )
        {
            lastSize = last.mRequest->mFullImgSize;
            warpMap = mWarpMapBufferPool->requestIIBuffer();
            GpuWarpBase::makePassThroughWarp(warpMap->getImageBuffer(), lastSize);
        }
        handleWarpData(warpMap, last.mRequest);
    }
    TRACE_FUNC_EXIT();
}

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
