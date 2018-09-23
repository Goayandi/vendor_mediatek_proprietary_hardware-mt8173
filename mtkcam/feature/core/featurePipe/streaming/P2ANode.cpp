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

#include "P2ANode.h"
#include <mtkcam/drv/iopipe/CamIO/IHalCamIO.h>
#include <mtkcam/feature/eis/eis_hal.h>
//#include "FMHal.h"

#define PIPE_CLASS_TAG "P2ANode"
#define PIPE_TRACE TRACE_P2A_NODE
#include <featurePipe/core/include/PipeLog.h>

using NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Normal;
using NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Vss;
using NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_FM;
using NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_FE;
using NSCam::NSIoPipe::NSPostProc::Input;
using NSCam::NSIoPipe::NSPostProc::Output;
using NSImageio::NSIspio::EPortIndex_LCEI;
using NSImageio::NSIspio::EPortIndex_IMG3O;
using NSImageio::NSIspio::EPortIndex_WDMAO;
using NSImageio::NSIspio::EPortIndex_WROTO;
using NSImageio::NSIspio::EPortIndex_VIPI;
using NSImageio::NSIspio::EPortIndex_IMGI;
using NSImageio::NSIspio::EPortIndex_DEPI;//left
using NSImageio::NSIspio::EPortIndex_DMGI;//right
using NSImageio::NSIspio::EPortIndex_FEO;
using NSImageio::NSIspio::EPortIndex_MFBO;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

// turn on flag to keep crop mode size in mFullImgSize
// must review usage of mFullImgSize before turn on this flag
//#define USE_REQ_REC_FULLIMGSIZE
static MVOID getFullImgSize(MBOOL isCropMode, const RequestPtr &request, MSize &imgSize)
{
    if (isCropMode)
    {
        imgSize = request->mMDPCrop.s;
        MY_LOGD("crop size is %dx%d", imgSize.w, imgSize.h);
    }
    else
    {
        imgSize = request->mFullImgSize;
    }
}

P2ANode::P2ANode(const char *name)
    : StreamingFeatureNode(name)
    , mNormalStream(NULL)
    , mpEisHal(NULL)
    , mFullImgPoolAllocateNeed(0)
    , mp3dnr(NULL)
    , mpNr3dParam(NULL)
    , mCropMode(CROP_MODE_NONE)
    , mEisMode(0)
    , mp3A(NULL)
    , mb3dnrInitedSuccess(MFALSE)
{
    TRACE_FUNC_ENTER();
    this->addWaitQueue(&mRequests);

    m3dnrLogLevel = getPropertyValue("camera.3dnr.log.level", 0);

    memset((void*)&tuningDat_FE_H, 0, sizeof(tuningDat_FE_H));
    memset((void*)&tuningDat_FE_M, 0, sizeof(tuningDat_FE_M));
    memset((void*)&tuningDat_FE_L, 0, sizeof(tuningDat_FE_L));
    memset((void*)&tuningDat_FM_P_N_H, 0, sizeof(tuningDat_FM_P_N_H));
    memset((void*)&tuningDat_FM_P_N_M, 0, sizeof(tuningDat_FM_P_N_M));
    memset((void*)&tuningDat_FM_P_N_L, 0, sizeof(tuningDat_FM_P_N_L));
    memset((void*)&tuningDat_FM_N_P_H, 0, sizeof(tuningDat_FM_N_P_H));
    memset((void*)&tuningDat_FM_N_P_M, 0, sizeof(tuningDat_FM_N_P_M));
    memset((void*)&tuningDat_FM_N_P_L, 0, sizeof(tuningDat_FM_N_P_L));

    TRACE_FUNC_EXIT();
}

P2ANode::~P2ANode()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::setNormalStream(NSCam::NSIoPipe::NSPostProc::INormalStream *stream)
{
    TRACE_FUNC_ENTER();
    mNormalStream = stream;
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::setDsImgPool(const android::sp<ImageBufferPool> &pool)
{
    TRACE_FUNC_ENTER();
    mDsImgPool = pool;
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::setFullImgPool(const android::sp<GraphicBufferPool> &pool, MUINT32 allocate)
{
    TRACE_FUNC_ENTER();
    mFullImgPool = pool;
    mFullImgPoolAllocateNeed = allocate;
    TRACE_FUNC_EXIT();
}

MBOOL P2ANode::onInit()
{
    TRACE_FUNC_ENTER();
    StreamingFeatureNode::onInit();
    if( mUsageHint.supportEIS_25() )
    {
        mEisMode = EIS_MODE_OFF;
        FMHal::getConfig(mUsageHint.mStreamingSize,mFM_FE_cfg);
        MY_LOGD("EIS2.5: FE0(%dx%d) FE1(%dx%d) FE2(%dx%d) feo_h(%dx%d) feo_m(%dx%d) feo_l(%dx%d) fmo_h(%dx%d) fmo_m(%dx%d) fmo_l(%dx%d)",
               mFM_FE_cfg[0].mImageSize.w,mFM_FE_cfg[0].mImageSize.h,
               mFM_FE_cfg[1].mImageSize.w,mFM_FE_cfg[1].mImageSize.h,
               mFM_FE_cfg[2].mImageSize.w,mFM_FE_cfg[2].mImageSize.h,
               mFM_FE_cfg[0].mFESize.w,mFM_FE_cfg[0].mFESize.h,
               mFM_FE_cfg[1].mFESize.w,mFM_FE_cfg[1].mFESize.h,
               mFM_FE_cfg[2].mFESize.w,mFM_FE_cfg[2].mFESize.h,
               mFM_FE_cfg[0].mFMSize.w,mFM_FE_cfg[0].mFMSize.h,
               mFM_FE_cfg[1].mFMSize.w,mFM_FE_cfg[1].mFMSize.h,
               mFM_FE_cfg[2].mFMSize.w,mFM_FE_cfg[2].mFMSize.h);
        mFE1ImgPool = ImageBufferPool::create("fpipe.fe1Img", mFM_FE_cfg[1].mImageSize, eImgFmt_YV12, ImageBufferPool::USAGE_HW);
        mFE2ImgPool = ImageBufferPool::create("fpipe.fe2Img", mFM_FE_cfg[2].mImageSize, eImgFmt_YV12, ImageBufferPool::USAGE_HW);
        mFE3ImgPool = ImageBufferPool::create("fpipe.fe3Img", mFM_FE_cfg[2].mImageSize, eImgFmt_YV12, ImageBufferPool::USAGE_HW);
        mFEOutputPool = FatImageBufferPool::create("fpipe.feo", mFM_FE_cfg[0].mFESize, eImgFmt_STA_BYTE, FatImageBufferPool::USAGE_HW);
        mFEOutputPool_m = FatImageBufferPool::create("fpipe.feo_m", mFM_FE_cfg[1].mFESize, eImgFmt_STA_BYTE, FatImageBufferPool::USAGE_HW);
        mFMOutputPool = FatImageBufferPool::create("fpipe.fmo", mFM_FE_cfg[0].mFMSize, eImgFmt_STA_BYTE, FatImageBufferPool::USAGE_HW);
        mFMOutputPool_m = FatImageBufferPool::create("fpipe.fmo_m", mFM_FE_cfg[1].mFMSize, eImgFmt_STA_BYTE, FatImageBufferPool::USAGE_HW);
        mFMRegisterPool = FatImageBufferPool::create("fpipe.fm_readregister", MSize(1,4), eImgFmt_STA_BYTE, FatImageBufferPool::USAGE_HW);
    }
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::onUninit()
{
    TRACE_FUNC_ENTER();
    ImageBufferPool::destroy(mFE1ImgPool);
    ImageBufferPool::destroy(mFE2ImgPool);
    ImageBufferPool::destroy(mFE3ImgPool);
    FatImageBufferPool::destroy(mFEOutputPool);
    FatImageBufferPool::destroy(mFEOutputPool_m);
    FatImageBufferPool::destroy(mFMOutputPool);
    FatImageBufferPool::destroy(mFMOutputPool_m);
    FatImageBufferPool::destroy(mFMRegisterPool);
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::onThreadStart()
{
    TRACE_FUNC_ENTER();
    MUINT32 numBasicBuffer = mUsageHint.getNumP2ABuffer();

    if( mUsageHint.supportVFB() && mDsImgPool != NULL )
    {
        mDsImgPool->allocate(numBasicBuffer);
    }
    if( mFullImgPoolAllocateNeed && mFullImgPool != NULL )
    {
        Timer timer;
        timer.start();
        mFullImgPool->allocate(mFullImgPoolAllocateNeed);
        timer.stop();
        MY_LOGD("mFullImg %s %d buf in %d ms", STR_ALLOCATE, mFullImgPoolAllocateNeed, timer.getElapsed());
    }
    if( mUsageHint.supportEIS_25() && !mUsageHint.support4K2K() &&
        mFE1ImgPool != NULL && mFE2ImgPool != NULL && mFE3ImgPool != NULL &&
        mFEOutputPool != NULL && mFEOutputPool_m != NULL &&
        mFMOutputPool != NULL && mFMOutputPool_m != NULL && mFMRegisterPool != NULL)
    {
        Timer timer;
        timer.start();
        mFE1ImgPool->allocate(1*3);
        mFE2ImgPool->allocate(1*3);
        mFE3ImgPool->allocate(1*3);
        mFEOutputPool->allocate(3*4);
        mFEOutputPool_m->allocate(3*4);
        mFMOutputPool->allocate(6*3);
        mFMOutputPool_m->allocate(6*3);
        mFMRegisterPool->allocate(6*4);
        timer.stop();
        MY_LOGD("FE FM %s in %d ms", STR_ALLOCATE, timer.getElapsed());
    }

    init3A();
    init3DNR();
    initVHDR();
    initFEFM();
    initP2();

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::onThreadStop()
{
    TRACE_FUNC_ENTER();
    waitEnqueQParamsDone();
    uninit3DNR();
    uninitVHDR();
    uninitFEFM();
    uninitP2();
    uninit3A();

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::onData(DataID id, const RequestPtr &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d: %s arrived", data->mRequestNo, ID2Name(id));
    MBOOL ret = MFALSE;

    switch( id )
    {
    case ID_ROOT_ENQUE:
        mRequests.enque(data);
        ret = MTRUE;
        break;
    default:
        ret = MFALSE;
        break;
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL P2ANode::onThreadLoop()
{
    TRACE_FUNC("Waitloop");
    RequestPtr request;

    if( !waitAllQueue() )
    {
        return MFALSE;
    }
    if( !mRequests.deque(request) )
    {
        MY_LOGE("Request deque out of sync");
        return MFALSE;
    }
    else if( request == NULL )
    {
        MY_LOGE("Request out of sync");
        return MFALSE;
    }
    TRACE_FUNC_ENTER();

    request->mTimer.startP2A();
    MY_LOGD("sensor(%d) Frame %d in P2A, feature=0x%04x(%s), in/out=(%d/%d)", mSensorIndex, request->mRequestNo, request->mFeatureMask, request->getFeatureMaskName(), request->mQParams.mvIn.size(), request->mQParams.mvOut.size());
    processP2A(request);
    request->mTimer.stopP2A();

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::processP2A(const RequestPtr &request)
{
    TRACE_FUNC_ENTER();
    P2AEnqueData data;
    QParams param;
    MRect postCropSize;
    MCrpRsInfo P2AOutCropInfo;

    data.mRequest = request;

#if 0
    // move inside request
    if (request->need3DNR())
    {
        MSize tmpSize = request->getMaxOutSize();
        MBOOL isEisOn;
        isEisOn = request->getVar<MBOOL>("3dnr.eis.isEisOn", isEisOn);


        if( isEisOn && request->isLastNodeP2A() && is4K2K(tmpSize) )
        {
            MY_LOGD("Use ENormalStreamTag_FE: (%dx%d)", tmpSize.w, tmpSize.h);
            mCropMode = CROP_MODE_USE_CRZ; //CRZ crop
        }
        else
        {
            mCropMode = CROP_MODE_NONE; //CRZ no crop
        }
    }
#endif
    calcSizeInfo(request);

    prepareQParams(param, request);

    prepare3A(param, request);

    if( request->need3DNR() )
    {
        prepare3DNR(param, request);
    }
    if( request->needEIS() )
    {
        prepareEIS(param, request, P2AOutCropInfo);
    }

    prepareIO(param, request, data);
    prepareCropInfo(param, request, data);

    enqueFeatureStream(param, data);

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::calcSizeInfo(const RequestPtr &request)
{
    TRACE_FUNC_ENTER();

#ifdef USE_REQ_REC_FULLIMGSIZE
    request->mMDPCrop = request->getP2Crop();
    if (request->isP2ACRZMode())
    {
        // 3DNR + EIS1.2 in 4K2K record mode use CRZ to reduce throughput
        request->mFullImgSize = request->mMDPCrop.s;
        request->mFullImgCrop = request->mMDPCrop;
        MY_LOGD("crop size is %dx%d", request->mFullImgSize.w, request->mFullImgSize.h);
    }
    else
    {
        request->mFullImgSize = request->getInputSize();
        request->mFullImgCrop = MCropRect(MPoint(0, 0), request->mFullImgSize);
    }
#else
    request->mFullImgSize = request->getInputSize();
    request->mFullImgCrop = MCropRect(MPoint(0, 0), request->mFullImgSize);
    request->mMDPCrop = request->getP2Crop();
#endif
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::onQParamsCB(const QParams &params, const P2AEnqueData &data)
{
    // This function is not thread safe,
    // avoid accessing P2ANode class members
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;

    RequestPtr request = data.mRequest;
    if( request == NULL )
    {
        MY_LOGE("Missing request");
        ret = MFALSE;
    }
    else
    {
        request->mTimer.stopEnqueP2A();
        MY_LOGD("sensor(%d) Frame %d enque done in %d ms, result = %d", mSensorIndex, request->mRequestNo, request->mTimer.getElapsedEnqueP2A(), params.mDequeSuccess);

        if( !params.mDequeSuccess )
        {
            MY_LOGW("Frame %d enque result failed", request->mRequestNo);
        }

        request->mP2A_QParams = params;
        request->updateResult(params.mDequeSuccess);
        if( mUsageHint.supportVendor() )
        {
            handleData(ID_P2A_TO_VENDOR_FULLIMG, ImgBufferData(data.mFullImg, request));
        }
        else if( mUsageHint.supportGPUNode() )
        {
            handleData(ID_P2A_TO_GPU_FULLIMG, ImgBufferData(data.mFullImg, request));
        }
        else
        {
            handleData(ID_P2A_TO_HELPER, CBMsgData(FeaturePipeParam::MSG_FRAME_DONE, request));
        }
        if( request->needP2AEarlyDisplay() )
        {
            handleData(ID_P2A_TO_HELPER, CBMsgData(FeaturePipeParam::MSG_DISPLAY_DONE, request));
        }

        if( request->needVFB() )
        {
            handleData(ID_P2A_TO_P2B_FULLIMG, ImgBufferData(data.mFullImg, request));
            handleData(ID_P2A_TO_FD_DSIMG, ImgBufferData(data.mDsImg, request));
            handleData(ID_P2A_TO_VFB_DSIMG, ImgBufferData(data.mDsImg, request));
        }

        if( mUsageHint.supportEIS_25() )
        {
            handleData(ID_P2A_TO_EIS_FM, FMData(data.mFMResult, request));
        }
        else if( mUsageHint.supportEIS_22() )
        {
            handleData(ID_P2A_TO_EIS_P2DONE, request);
        }

        if( request->needDump() )
        {
            if( data.mFullImg != NULL )
            {
                data.mFullImg->getImageBuffer()->syncCache(eCACHECTRL_INVALID);
                dumpData(data.mRequest, data.mFullImg->getImageBufferPtr(), "full");
            }
            if( data.mDsImg != NULL )
            {
                data.mDsImg->getImageBuffer()->syncCache(eCACHECTRL_INVALID);
                dumpData(data.mRequest, data.mDsImg->getImageBufferPtr(), "ds");
            }
            if( data.mFMResult.FM_B.Register_Medium != NULL )
            {
                dumpData(data.mRequest, data.mFMResult.FM_B.Register_Medium->getImageBufferPtr(), "fm_reg_m");
            }
        }
        request->mTimer.stopP2A();
    }

    this->decExtThreadDependency();
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL P2ANode::initP2()
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    if( mNormalStream != NULL )
    {
        ret = MTRUE;
    }

    mpEisHal = EisHal::CreateInstance("FeaturePipe_P2A", mSensorIndex);
    if (mpEisHal == NULL)
    {
        MY_LOGE("FeaturePipe_P2A CreateInstance failed");
        ret = MFALSE;
    }

    if(EIS_RETURN_NO_ERROR != mpEisHal->Init())
    {
        MY_LOGE("mpEisHal init failed");
        return MFALSE;
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MVOID P2ANode::uninitP2()
{
    TRACE_FUNC_ENTER();
    if (mpEisHal)
    {
        mpEisHal->Uninit();
        mpEisHal->DestroyInstance("FeaturePipe_P2A");
        mpEisHal = NULL;
    }
    mNormalStream = NULL;
    mPrevFullImg = NULL;
    mPrevFE.clear();

    TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepareQParams(QParams &params, const RequestPtr &request)
{
    TRACE_FUNC_ENTER();
    params = request->mQParams;
    prepareStreamTag(params, request);
    TRACE_FUNC_EXIT();
    return MFALSE;
}

MCrpRsInfo P2ANode::calViewAngleCrop(QParams &params, const RequestPtr &request, MRect postCropSize)
{
    TRACE_FUNC_ENTER();

    MUINT32 PostProcOutWidth = 0, PostProcOutHeight = 0;
    MCrpRsInfo cropInfo;
    MUINT32 eisCrop2Idx = 0,i;

    for( i = 0; i < request->mvCropRsInfo.size(); ++i )
    {
        if( request->mvCropRsInfo[i].mGroupID == 2 )
        {
            eisCrop2Idx = i;
        }
    }

    PostProcOutWidth  = postCropSize.s.w;
    PostProcOutHeight = postCropSize.s.h;

    cropInfo = request->mvCropRsInfo[eisCrop2Idx];

    cropInfo.mGroupID = 1;
    cropInfo.mCropRect.p_integral.x = postCropSize.p.x;
    cropInfo.mCropRect.p_integral.y = postCropSize.p.y;
    cropInfo.mCropRect.s.w = PostProcOutWidth;
    cropInfo.mCropRect.s.h = PostProcOutHeight;
    cropInfo.mResizeDst.w  = PostProcOutWidth;
    cropInfo.mResizeDst.h  = PostProcOutHeight;

    TRACE_FUNC_EXIT();
    return cropInfo;
}

MBOOL P2ANode::needDigitalZoomCrop(const RequestPtr &request)
{
    return !request->needEIS();
}

MBOOL P2ANode::prepareCropInfo(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();

    if( !request->isLastNodeP2A() )
    {
        params.mvCropRsInfo.clear();
    }
    if( request->needFullImg() )
    {
        if (request->isP2ACRZMode())
        {
            prepareCRZCrop(params, request, data);
        }
    }
    if( request->needFEFM() )
    {
        prepareCropInfo_FE(params,request,data);
    }
    else if( request->needP2AEarlyDisplay() )
    {
        prepareEarlyDisplayCrop(params, request, data);
    }
    if( request->needDsImg() )
    {
        MCrpRsInfo crop;
        crop.mGroupID = 2;
        crop.mCropRect = MCropRect(MPoint(0, 0), request->mFullImgSize);
        crop.mResizeDst = data.mDsImg->getImageBuffer()->getImgSize();
        params.mvCropRsInfo.push_back(crop);
    }

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID P2ANode::prepareCRZCrop(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    if( request->mvCropRsInfo.size() > 0 )
    {
        MCrpRsInfo crop;

        params.mvCropRsInfo.clear();

        crop = request->mvCropRsInfo[0];
        crop.mCropRect.p_integral.x = 0;
        crop.mCropRect.p_integral.y = 0;
        crop.mCropRect.p_fractional.x = 0;
        crop.mCropRect.p_fractional.y = 0;
        crop.mCropRect.s.w &= ~1;
        crop.mCropRect.s.h &= ~1;
        params.mvCropRsInfo.push_back(crop);

        if( request->mvCropRsInfo.size() > 1 )
        {
            crop = request->mvCropRsInfo[1];
            crop.mCropRect.p_integral.x = 0;
            crop.mCropRect.p_integral.y = 0;
            crop.mCropRect.p_fractional.x = 0;
            crop.mCropRect.p_fractional.y = 0;
            crop.mCropRect.s.w &= ~1;
            crop.mCropRect.s.h &= ~1;
            params.mvCropRsInfo.push_back(crop);
        }

        crop = request->mvCropRsInfo[0];
        crop.mCropRect.s.w &= ~1;
        crop.mCropRect.s.h &= ~1;
        crop.mResizeDst = crop.mCropRect.s;
        crop.mGroupID = 1; //Use for CRZ
        params.mvCropRsInfo.push_back(crop);
    }
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareEarlyDisplayCrop(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    Output output;
    if( request->getDisplayOutput(output) )
    {
        MCrpRsInfo crop;
        MCrpRsInfo eis_displaycrop;
        if( request->getEISDisplayCrop(eis_displaycrop, RRZO_DOMAIN) )
        {
             crop.mCropRect = eis_displaycrop.mCropRect;
             crop.mResizeDst = eis_displaycrop.mResizeDst;
        } else {
             MY_LOGD("default display  crop");
             crop.mCropRect = MCropRect(MPoint(0, 0), request->mFullImgSize);
             crop.mResizeDst = request->mFullImgSize;
        }
        crop.mGroupID = 3;//wrot
        crop.mFrameGroup = 0;
        params.mvCropRsInfo.push_back(crop);
    }
    TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepareStreamTag(QParams &params, const RequestPtr &request)
{
    TRACE_FUNC_ENTER();
    if( request->need3DNR() && request->isP2ACRZMode() )
    {
        params.mvStreamTag.clear();
        params.mvStreamTag.push_back(ENormalStreamTag_FE);
    }
    else if( request->need3DNR() || !request->isLastNodeP2A() )
    {
        params.mvStreamTag.clear();
        params.mvStreamTag.push_back(ENormalStreamTag_Normal);
    }
    else if( mUsageHint.isTimeSharing() )
    {
        params.mvStreamTag.clear();
        params.mvStreamTag.push_back(ENormalStreamTag_Vss);
    }
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::prepareIO(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;

    {
        NSCam::NSIoPipe::NSPostProc::Output output;
        if( !request->getDisplayOutput(output) ||
            (request->needEIS() && !request->getRecordOutput(output)) )
        {
            printRequestIO(request);
        }
    }

    params.mvIn = request->mQParams.mvIn;
    params.mvOut.clear();

    if( request->isLastNodeP2A() )
    {
        params.mvOut = request->mQParams.mvOut;
    }
    if( request->needFullImg() )
    {
        prepareFullImg(params, request, data);
    }
    if( request->needDsImg() )
    {
        prepareDsImg(params, request, data);
    }
    if( request->need3DNR() && mPrevFullImg != NULL)
    {
        prepareVIPI(params, request, data);
        //handleVipiNr3dOffset(params, request, data);
    }
    if( request->needVHDR() )
    {
        prepareLCEI(params, request, data);
    }
    if( request->needFEFM() )
    {
        prepareFEFM(params, request, data);
    }
    else if( request->needP2AEarlyDisplay() )
    {
        prepareEarlyDisplayImg(params, request, data);
    }
    mPrevFE = data.mFMResult.FE;
    mPrevFullImg = request->need3DNR() ? data.mFullImg : NULL;

    TRACE_FUNC_EXIT();
    return ret;
}

MVOID P2ANode::prepareFullImg(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC("Frame %d FullImgPool=(%d/%d)", request->mRequestNo, mFullImgPool->peakAvailableSize(), mFullImgPool->peakPoolSize());
    data.mFullImg = mFullImgPool->requestIIBuffer();

    MSize tmpSize;
#ifdef USE_REQ_REC_FULLIMGSIZE
    tmpSize = request->mFullImgSize;
#else
    getFullImgSize(request->isP2ACRZMode(), request, tmpSize);
#endif
    data.mFullImg->getImageBuffer()->setExtParam(tmpSize);

    Output output;
    output.mPortID = PortID(EPortType_Memory, EPortIndex_IMG3O, PORTID_OUT);
    output.mBuffer = data.mFullImg->getImageBufferPtr();
    output.mBuffer->setTimestamp(request->mQParams.mvIn[0].mBuffer->getTimestamp());
    params.mvOut.push_back(output);
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareDsImg(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    MSize dsSize;
    dsSize = calcDsImgSize(request->mFullImgSize);
    MY_LOGD("DsImg size from %dx%d to %dx%d", MSize_ARG(request->mFullImgSize), MSize_ARG(dsSize));
    data.mDsImg = mDsImgPool->requestIIBuffer();
    data.mDsImg->getImageBuffer()->setExtParam(dsSize);
    Output output;
    output.mPortID = PortID(EPortType_Memory, EPortIndex_WDMAO, PORTID_OUT);
    output.mBuffer = data.mDsImg->getImageBufferPtr();
    params.mvOut.push_back(output);
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareVIPI(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    data.mPrevFullImg = mPrevFullImg;
    Input input;
    input.mPortID = PortID(EPortType_Memory, EPortIndex_VIPI, PORTID_IN);
    input.mBuffer = data.mPrevFullImg->getImageBufferPtr();

    // mpNr3dParam is prepared at prepare3DNR
    input.mBuffer->setExtParam(
        MSize(mpNr3dParam->vipi_readW, mpNr3dParam->vipi_readH),
        mpNr3dParam->vipi_offst
    );

    params.mvIn.push_back(input);

    TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareLCEI(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    prepareVHDR(params, request);
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareFEFM(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();

    data.mFE1Img = mFE1ImgPool->requestIIBuffer();
    data.mFE2Img = mFE2ImgPool->requestIIBuffer();
    data.mFE3Img = mFE3ImgPool->requestIIBuffer();
    data.mFMResult.FE.High = mFEOutputPool->requestIIBuffer();
    data.mFMResult.FE.Medium = mFEOutputPool_m->requestIIBuffer();
    data.mFMResult.FE.Low = mFEOutputPool_m->requestIIBuffer();//mFEOutputPool_l->requestIIBuffer();

    prepareFE(params, request, data);

    if( mPrevFE.isValid() )
    {
        data.mFMResult.PrevFE = mPrevFE;

        //Previous to Current
        data.mFMResult.FM_A.High = mFMOutputPool->requestIIBuffer();
        data.mFMResult.FM_A.Medium = mFMOutputPool_m->requestIIBuffer();
        data.mFMResult.FM_A.Low = mFMOutputPool_m->requestIIBuffer();//mFMOutputPool_l->requestIIBuffer();

        data.mFMResult.FM_A.Register_High = mFMRegisterPool->requestIIBuffer();
        data.mFMResult.FM_A.Register_Medium = mFMRegisterPool->requestIIBuffer();
        data.mFMResult.FM_A.Register_Low = mFMRegisterPool->requestIIBuffer();

        //Current to Previous
        data.mFMResult.FM_B.High = mFMOutputPool->requestIIBuffer();
        data.mFMResult.FM_B.Medium = mFMOutputPool_m->requestIIBuffer();
        data.mFMResult.FM_B.Low = mFMOutputPool_m->requestIIBuffer();//mFMOutputPool_l->requestIIBuffer();

        data.mFMResult.FM_B.Register_High = mFMRegisterPool->requestIIBuffer();
        data.mFMResult.FM_B.Register_Medium = mFMRegisterPool->requestIIBuffer();
        data.mFMResult.FM_B.Register_Low = mFMRegisterPool->requestIIBuffer();

        prepareFM(params, request, data);
    }
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareEarlyDisplayImg(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    Output output;
    if( request->getDisplayOutput(output) )
    {
        output.mPortID = PortID(EPortType_Memory, EPortIndex_WROTO, PORTID_OUT);
        params.mvOut.push_back(output);
    }
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::enqueFeatureStream(NSCam::NSIoPipe::NSPostProc::QParams &params, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    MBOOL ret;
    MY_LOGD("sensor(%d) Frame %d enque start", mSensorIndex, data.mRequest->mRequestNo);
    data.mRequest->mTimer.startEnqueP2A();
    this->incExtThreadDependency();
    ret = this->enqueQParams(mNormalStream, params, data);
    if( !ret )
    {
        MY_LOGE("Frame %d enque failed", data.mRequest->mRequestNo);
    }
    TRACE_FUNC_EXIT();
}

MVOID P2ANode::printRequestIO(const RequestPtr &request)
{
  for( unsigned i = 0, n = request->mQParams.mvIn.size(); i < n; ++i )
  {
      unsigned index = request->mQParams.mvIn[i].mPortID.index;
      MSize size = request->mQParams.mvIn[i].mBuffer->getImgSize();
      MY_LOGD("sensor(%d) Frame %d mvIn[%d] idx=%d size=(%d,%d)", mSensorIndex, request->mRequestNo, i, index, size.w, size.h);
  }
  for( unsigned i = 0, n = request->mQParams.mvOut.size(); i < n; ++i )
  {
      unsigned index = request->mQParams.mvOut[i].mPortID.index;
      MSize size = request->mQParams.mvOut[i].mBuffer->getImgSize();
      MBOOL isGraphic = (getGraphicBufferAddr(request->mQParams.mvOut[i].mBuffer) != NULL);
      MINT fmt = request->mQParams.mvOut[i].mBuffer->getImgFormat();
      MUINT32 cap = request->mQParams.mvOut[i].mPortID.capbility;
      MY_LOGD("sensor(%d) Frame %d mvOut[%d] idx=%d size=(%d,%d) fmt=%d, cap=%02x, isGraphic=%d", mSensorIndex, request->mRequestNo, i, index, size.w, size.h, fmt, cap, isGraphic);
  }
  for( unsigned i = 0, n = request->mQParams.mvCropRsInfo.size(); i < n; ++i )
  {
      MCrpRsInfo crop = request->mQParams.mvCropRsInfo[i];
      MY_LOGD("sensor(%d) Frame %d crop[%d] " MCrpRsInfo_STR, mSensorIndex, request->mRequestNo, i,
          MCrpRsInfo_ARG(crop));
  }
}

MBOOL P2ANode::init3A()
{
    TRACE_FUNC_ENTER();

    if (mp3A == NULL)
    {
        mp3A = MAKE_Hal3A(mSensorIndex, PIPE_CLASS_TAG);
    }

    if (EIS_MODE_IS_EIS_22_ENABLED(mUsageHint.mEisMode) ||
        EIS_MODE_IS_EIS_25_ENABLED(mUsageHint.mEisMode))
    {
        //Disable OIS
        MY_LOGD("mEisMode:%d => Disable OIS \n", mUsageHint.mEisMode);
        mp3A ->send3ACtrl(E3ACtrl_SetEnableOIS, 0, 0);
        mp3A ->send3ACtrl(E3ACtrl_SetAEEISRecording, 1, 0);
        mEisMode = mUsageHint.mEisMode;
    }

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID P2ANode::uninit3A()
{
    TRACE_FUNC_ENTER();

    if (mp3A)
    {
        //Enable OIS
        if (EIS_MODE_IS_EIS_22_ENABLED(mEisMode) ||
            EIS_MODE_IS_EIS_25_ENABLED(mEisMode))
        {
            MY_LOGD("mEisMode:%d => Enable OIS \n", mEisMode);
            mp3A ->send3ACtrl(E3ACtrl_SetAEEISRecording, 0, 0);
            mp3A ->send3ACtrl(E3ACtrl_SetEnableOIS, 1, 0);
            mEisMode = EIS_MODE_OFF;
        }

        // turn OFF 'pull up ISO value to gain FPS'
        AE_Pline_Limitation_T params;
        params. bEnable = MFALSE; // disable
        params. bEquivalent= MTRUE;
        params. u4IncreaseISO_x100= 100;
        params. u4IncreaseShutter_x100= 100;
        mp3A ->send3ACtrl(E3ACtrl_SetAEPlineLimitation, (MINTPTR)&params, 0);

        // destroy the instance
        mp3A->destroyInstance(PIPE_CLASS_TAG);
        mp3A = NULL;
    }

    TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepare3A(NSCam::NSIoPipe::NSPostProc::QParams &params, const RequestPtr &request)
{
    TRACE_FUNC_ENTER();
    (void)params;
    (void)request;
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::initEIS()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID P2ANode::uninitEIS()
{
}

MBOOL P2ANode::prepareEIS(NSCam::NSIoPipe::NSPostProc::QParams &params, const RequestPtr &request, const MCrpRsInfo outCropInfo)
{
    TRACE_FUNC_ENTER();
    MUINT32 PostProcOutWidth = 0, PostProcOutHeight = 0;
    MUINT32 i;
    EIS_HAL_CONFIG_DATA eisHalConfigData;


    PostProcOutWidth  = request->mFullImgSize.w;
    PostProcOutHeight = request->mFullImgSize.h;
    MUINT32 ratio;

    ratio = mpEisHal->GetEisPlusCropRatio();

    eisHalConfigData.gpuTargetW = PostProcOutWidth*100.0/ratio;
    eisHalConfigData.gpuTargetH = PostProcOutHeight*100.0/ratio;

#define ROUND_TO_2X(x) ((x) & (~0x1))

    eisHalConfigData.gpuTargetW = ROUND_TO_2X(eisHalConfigData.gpuTargetW);
    eisHalConfigData.gpuTargetH = ROUND_TO_2X(eisHalConfigData.gpuTargetH);

#undef ROUND_TO_2X

#if 1
    eisHalConfigData.crzOutW = request->mFullImgSize.w;
    eisHalConfigData.crzOutH = request->mFullImgSize.h;
    eisHalConfigData.srzOutW = request->mFullImgSize.w; //TODO : need to modified for stereo
    eisHalConfigData.srzOutH = request->mFullImgSize.h; //TODO : need to modified for stereo
    eisHalConfigData.feTargetW = request->mFullImgSize.w; //TODO : need to modified for stereo
    eisHalConfigData.feTargetH = request->mFullImgSize.h; //TODO : need to modified for stereo

    eisHalConfigData.cropX   = request->mFullImgCrop.p_integral.x;
    eisHalConfigData.cropY   = request->mFullImgCrop.p_integral.y;


    eisHalConfigData.imgiW = request->mvIn[0].mBuffer->getImgSize().w;
    eisHalConfigData.imgiH = request->mvIn[0].mBuffer->getImgSize().h;

    MY_LOGD("[prepareEIS] imgi(%u,%u),crz(%u,%u),srz(%u,%u),feT(%u,%u),gpuT(%u,%u),crop(%u,%u)",eisHalConfigData.imgiW,
                                                                                                eisHalConfigData.imgiH,
                                                                                                eisHalConfigData.crzOutW,
                                                                                                eisHalConfigData.crzOutH,
                                                                                                eisHalConfigData.srzOutW,
                                                                                                eisHalConfigData.srzOutH,
                                                                                                eisHalConfigData.feTargetW,
                                                                                                eisHalConfigData.feTargetH,
                                                                                                eisHalConfigData.gpuTargetW,
                                                                                                eisHalConfigData.gpuTargetH,
                                                                                                eisHalConfigData.cropX,
                                                                                                eisHalConfigData.cropY);

#endif
    if (request->needEIS25())
    {
        /* Control EIS 2.5 algo mode
         */
        EIS_ALG_MODE algMode = EIS_ALG_MODE_EIS25_FUSION;

        if( request->is4K2K() || // 4k2k only happens as Gyro-based enabled
            (EISCustom::isEnabledEIS25_GyroMode() && !EISCustom::isEnabledEIS25_ImageMode()) )
        {
            /* 4k2k only support EIS 2.5 Gyro-based */
            algMode = EIS_ALG_MODE_EIS25_GYRO_ONLY;
            if( EIS_RETURN_NO_ERROR != mpEisHal->ConfigFFFMEis(eisHalConfigData, algMode) )
            {
                MY_LOGE("P2A ConfigFFFMEis fail");
                return MFALSE;
            }
        }
        else //4k2k with vHDR never happens
        {
            if( request->needVHDR() || // vHDR only happens as Image-based enabled
                (EISCustom::isEnabledEIS25_ImageMode() && !EISCustom::isEnabledEIS25_GyroMode()))
            {
                /* VHDR only support EIS 2.5 Image-based */
                algMode = EIS_ALG_MODE_EIS25_IMAGE_ONLY;
            }

            /* EIS 2.5 Fusion and Image-based version needs config FEFM */
            MULTISCALE_INFO FEFM_MultiScale_info;
            for( MUINT32 i = 0; i < MULTISCALE_FEFM; ++i )
            {
                FEFM_MultiScale_info.MultiScale_width[i] = mFM_FE_cfg[i].mImageSize.w;
                FEFM_MultiScale_info.MultiScale_height[i] = mFM_FE_cfg[i].mImageSize.h;
                FEFM_MultiScale_info.MultiScale_blocksize[i] = mFM_FE_cfg[i].mBlockSize;
            }

            if( EIS_RETURN_NO_ERROR != mpEisHal->ConfigFFFMEis(eisHalConfigData, algMode, &FEFM_MultiScale_info) )
            {
                MY_LOGE("P2A ConfigFFFMEis fail");
                return MFALSE;
            }
        }
    }
    else
    {
        if( EIS_RETURN_NO_ERROR != mpEisHal->ConfigGis(eisHalConfigData) )
        {
            MY_LOGE("P2A ConfigGis fail");
            return MFALSE;
        }
    }


    handleData(ID_P2A_TO_EIS_CONFIG, EisConfigData(eisHalConfigData, request));
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::init3DNR()
{
    TRACE_FUNC_ENTER();

    mp3dnr = NSCam::NSIoPipe::NSPostProc::hal3dnrBase::createInstance();
    if (mp3dnr)
    {
        mp3dnr->init(mUsageHint.is3DNRModeMaskEnable(
            IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT));
        mb3dnrInitedSuccess = MTRUE;
    }

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID P2ANode::uninit3DNR()
{
    TRACE_FUNC_ENTER();
    mPrevFullImg = NULL;

    if(mp3dnr)
    {
        mp3dnr->destroyInstance();
        mp3dnr = NULL;
    }
    TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepare3DNR(NSCam::NSIoPipe::NSPostProc::QParams &params, const RequestPtr &request)
{
    TRACE_FUNC_ENTER();

    // === prepare cropInfo ===

    MBOOL result = MTRUE;
    // 1. prepare all info
    // MBOOL prepare3DNR(
    //     NSCam::NSIoPipe::NSPostProc::QParams &params,
    //      crop_infos const& cropInfo, // TODO: try
    //      MINT32 iso,  // TODO: try 200, 800,
    //      IImageBuffer const *pIMGIBuffer,
    //      IImageBuffer *pIMG3OBuffer // TODO
    // );
    // [markInfo]: TODO: Crop, ISO are needed from upper layer

    // === cropInfo ===
    MRect postCropSize;
//    postCropSize.s.w = postCropSize.s.h = 0;
//    postCropSize = calcPostCropSize(request);
#ifdef USE_REQ_REC_FULLIMGSIZE
    postCropSize.s = request->mFullImgSize;
#else
    getFullImgSize(request->isP2ACRZMode(), request, postCropSize.s);
#endif
    MY_LOGD_IF(m3dnrLogLevel >= 2, "aaaa_cropInfo_test: w=%d, h=%d", postCropSize.s.w, postCropSize.s.h);

    // === prepare eis info ===
    MBOOL tmp_isEisOn = 0;
    MUINT32 tmp_gmvX = 0;
    MUINT32 tmp_gmvY = 0;
    MUINT32 tmp_x_int = 0;
    MUINT32 tmp_y_int = 0;

    eis_region tmpEisRegion;
    tmp_isEisOn = request->getVar<MBOOL>("3dnr.eis.isEisOn", tmp_isEisOn);
    tmp_gmvX = request->getVar<MUINT32>("3dnr.eis.gmvX", tmp_gmvX);
    tmp_gmvY = request->getVar<MUINT32>("3dnr.eis.gmvY", tmp_gmvY);
    tmp_x_int = request->getVar<MUINT32>("3dnr.eis.x_int", tmp_x_int);
    tmp_y_int = request->getVar<MUINT32>("3dnr.eis.y_int", tmp_y_int);

    tmpEisRegion.gmvX = tmp_gmvX;
    tmpEisRegion.gmvY = tmp_gmvY;
    tmpEisRegion.x_int = tmp_x_int;
    tmpEisRegion.y_int = tmp_y_int;

    MY_LOGD_IF(m3dnrLogLevel >= 2, "aaaa_eis_test: isEisOn=%d, gmvX=%d, gmvY=%d, x_int=%d, y_int=%d",
    tmp_isEisOn, tmp_gmvX, tmp_gmvY, tmp_x_int, tmp_y_int);

    // prepare iso
    MINT32 iso = 200;
    iso = request->getVar<MUINT32>("3dnr.iso", iso);
    MY_LOGD_IF(m3dnrLogLevel >= 2, "aaaa_3dnr.iso: %d", iso);

    MBOOL res = do3dnrFlow(params, request, postCropSize, tmpEisRegion, iso, request->mRequestNo);

    TRACE_FUNC_EXIT();
    return MTRUE;
}

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
