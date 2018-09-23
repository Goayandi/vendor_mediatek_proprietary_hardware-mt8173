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

#include "StreamingFeatureData.h"
#include "StreamingFeature_Common.h"

#define PIPE_CLASS_TAG "Data"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_DATA
#include <featurePipe/core/include/PipeLog.h>

using NSCam::NSIoPipe::NSPostProc::QParams;
using NSCam::NSIoPipe::NSPostProc::MCropRect;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

StreamingFeatureRequest::StreamingFeatureRequest(const FeaturePipeParam &extParam, MUINT32 requestNo)
    : mExtParam(extParam)
    , mFeatureMask(extParam.mFeatureMask)
    , mRequestNo(requestNo)
    , mDebugDump(0)
    , mVarMap(mExtParam.mVarMap)
    , mQParams(mExtParam.mQParams)
    , mvIn(mQParams.mvIn)
    , mvOut(mQParams.mvOut)
    , mvCropRsInfo(mQParams.mvCropRsInfo)
    , mDisplayFPSCounter(NULL)
    , mFrameFPSCounter(NULL)
    , mResult(MTRUE)
    , mNeedDump(MFALSE)
    , mForceIMG3O(MFALSE)
    , mForceGpuPass(MFALSE)
    , mForceGpuOut(NO_FORCE)
    , mIs4K2K(MFALSE)
{
    mQParams.mDequeSuccess = MFALSE;
    mIs4K2K = mvIn.size() && NSFeaturePipe::is4K2K(mvIn[0].mBuffer->getImgSize());
    MBOOL isEisOn = MFALSE;
    isEisOn = getVar<MBOOL>("3dnr.eis.isEisOn", isEisOn);
    MSize tmpSize = getMaxOutSize();
    // 3DNR + EIS1.2 in 4K2K record mode use CRZ to reduce throughput
    mIsP2ACRZMode = (need3DNR() && isEisOn && isLastNodeP2A() && NSFeaturePipe::is4K2K(tmpSize)) ?
        MTRUE : MFALSE;
    mTimer.start();
}

StreamingFeatureRequest::~StreamingFeatureRequest()
{
    double frameFPS = 0, displayFPS = 0;
    if( mDisplayFPSCounter )
    {
        mDisplayFPSCounter->update(mTimer.getDisplayMark());
        displayFPS = mDisplayFPSCounter->getFPS();
    }
    if( mFrameFPSCounter )
    {
        mFrameFPSCounter->update(mTimer.getFrameMark());
        frameFPS = mFrameFPSCounter->getFPS();
    }
    mTimer.print(mRequestNo, displayFPS, frameFPS);
}

MVOID StreamingFeatureRequest::setDisplayFPSCounter(FPSCounter *counter)
{
    mDisplayFPSCounter = counter;
}

MVOID StreamingFeatureRequest::setFrameFPSCounter(FPSCounter *counter)
{
    mFrameFPSCounter = counter;
}

MBOOL StreamingFeatureRequest::updateResult(MBOOL result)
{
    mResult = (mResult && result);
    mQParams.mDequeSuccess = mResult;
    return mResult;
}

MBOOL StreamingFeatureRequest::doExtCallback(FeaturePipeParam::MSG_TYPE msg)
{
    MBOOL ret = MFALSE;
    if( msg == FeaturePipeParam::MSG_FRAME_DONE )
    {
        mTimer.stop();
    }
    if( mExtParam.mCallback )
    {
        ret = mExtParam.mCallback(msg, mExtParam);
    }
    return ret;
}

MSize StreamingFeatureRequest::getMaxOutSize() const
{
    MSize maxSize = MSize(0, 0);
    MUINT32 max = 0;
    for( unsigned i = 0, n = mQParams.mvOut.size(); i < n; ++i )
    {
        MSize size = mQParams.mvOut[i].mBuffer->getImgSize();
        MUINT32 temp = size.w * size.h;
        if( temp > max )
        {
            maxSize = size;
            max = temp;
        }
    }
    return maxSize;
}

MSize StreamingFeatureRequest::getInputSize() const
{
    MSize inputSize = MSize(0, 0);
    if( mQParams.mvIn.size() > 0 )
    {
        inputSize = mQParams.mvIn[0].mBuffer->getImgSize();
    }
    return inputSize;
}

MCropRect StreamingFeatureRequest::getP2Crop() const
{
  MCropRect crop;
  for( unsigned i = 0, n = mQParams.mvCropRsInfo.size(); i < n; ++i )
  {
      if( mQParams.mvCropRsInfo[i].mGroupID == 2 )
      {
          crop = mQParams.mvCropRsInfo[i].mCropRect;
          break;
      }
      else if( mQParams.mvCropRsInfo[i].mGroupID == 3 )
      {
          crop = mQParams.mvCropRsInfo[i].mCropRect;
      }
  }
  return crop;
}

MBOOL StreamingFeatureRequest::getDisplayOutput(NSCam::NSIoPipe::NSPostProc::Output &output)
{
    TRACE_FUNC_ENTER();
    unsigned count = 0;
    for(unsigned i = 0, size = mQParams.mvOut.size(); i < size; ++i )
    {
        if( isDisplayOutput(mQParams.mvOut[i]) )
        {
            if( ++count == 1 )
            {
                output = mQParams.mvOut[i];
            }
        }
    }
    if( count != 1 )
    {
        TRACE_FUNC("frame %d: suspicious display output number = %d", mRequestNo, count);
    }
    TRACE_FUNC_EXIT();
    return count >= 1;
}

MBOOL StreamingFeatureRequest::getRecordOutput(NSCam::NSIoPipe::NSPostProc::Output &output)
{
    TRACE_FUNC_ENTER();
    unsigned count = 0;
    for(unsigned i = 0, size = mQParams.mvOut.size(); i < size; ++i )
    {
        if( isRecordOutput(mQParams.mvOut[i]) )
        {
            if( ++count == 1 )
            {
                output = mQParams.mvOut[i];
            }
        }
    }
    if( count != 1 )
    {
        TRACE_FUNC("frame %d: suspicious record output number = %d", mRequestNo, count);
    }
    TRACE_FUNC_EXIT();
    return count >= 1;
}

IImageBuffer* StreamingFeatureRequest::getRecordOutputBuffer()
{
    NSCam::NSIoPipe::NSPostProc::Output output;
    IImageBuffer *outputBuffer = NULL;
    if( getRecordOutput(output) )
    {
        outputBuffer = output.mBuffer;
    }
    return outputBuffer;
}

MBOOL StreamingFeatureRequest::getDisplayCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    ret = getCropInfo(NORMAL_CROP_GROUP, NSCam::NSIoPipe::EPortCapbility_Disp, WROTO_CROP_GROUP, crop);
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL StreamingFeatureRequest::getRecordCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    ret = getCropInfo(NORMAL_CROP_GROUP, NSCam::NSIoPipe::EPortCapbility_Rcrd, WDMAO_CROP_GROUP, crop);
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL StreamingFeatureRequest::getEISDisplayCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop, Domain domain, const MSize &maxSize)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    NSCam::NSIoPipe::NSPostProc::MCrpRsInfo eisCropInfo;

    if( getCropInfo(EIS_CROP_GROUP, NSCam::NSIoPipe::EPortCapbility_Disp, EIS_WROTO_CROP_GROUP, eisCropInfo) )
    {
        crop = applyEISCropRatio(eisCropInfo, domain, maxSize);
        ret = MTRUE;

        TRACE_FUNC( "eis display crop: domain=%d, pos=(%d,%d), size=(%dx%d)",
                    domain,
                    crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                    crop.mCropRect.s.w, crop.mCropRect.s.h);
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL StreamingFeatureRequest::getEISRecordCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop, Domain domain, const MSize &maxSize)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    NSCam::NSIoPipe::NSPostProc::MCrpRsInfo eisCropInfo;

    if( getCropInfo(EIS_CROP_GROUP, NSCam::NSIoPipe::EPortCapbility_Rcrd, EIS_WDMAO_CROP_GROUP, eisCropInfo) )
    {
        crop = applyEISCropRatio(eisCropInfo, domain, maxSize);
        ret = MTRUE;

        TRACE_FUNC( "eis record crop: domain=%d, pos=(%d,%d), size=(%dx%d)",
                    domain,
                    crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                    crop.mCropRect.s.w, crop.mCropRect.s.h);
    }

    TRACE_FUNC_EXIT();
    return ret;
}

MVOID StreamingFeatureRequest::setDumpProp(MINT32 start, MINT32 count)
{
    mNeedDump = (start < 0) ||
                ((mRequestNo >= start) && ((mRequestNo - start) < count));
}

MVOID StreamingFeatureRequest::setForceIMG3O(MBOOL forceIMG3O)
{
    mForceIMG3O = forceIMG3O;
}

MVOID StreamingFeatureRequest::setForceGpuPass(MBOOL forceGpuPass)
{
    mForceGpuPass = forceGpuPass;
}

MVOID StreamingFeatureRequest::setForceGpuOut(MUINT32 forceGpuOut)
{
    mForceGpuOut = forceGpuOut;
}

const char* StreamingFeatureRequest::getFeatureMaskName() const
{
    switch(mFeatureMask)
    {
    case 0:
        return "NONE";
    case MASK_EIS:
        return "EIS22";
    case MASK_EIS|MASK_EIS_QUEUE:
        return "EIS22Q";
    case MASK_EIS|MASK_EIS_25:
        return "EIS25";
    case MASK_EIS|MASK_EIS_25|MASK_EIS_QUEUE:
        return "EIS25Q";
    case MASK_3DNR:
        return "3DNR";
    case MASK_3DNR|MASK_EIS:
        return "3DNR+EIS22";
    case MASK_3DNR|MASK_EIS|MASK_EIS_QUEUE:
        return "3DNR+EIS22Q";
    case MASK_3DNR|MASK_EIS|MASK_EIS_25:
        return "3DNR+EIS25";
    case MASK_3DNR|MASK_EIS|MASK_EIS_25|MASK_EIS_QUEUE:
        return "3DNR+EIS25Q";
    case MASK_VENDOR|MASK_EIS:
        return "Vendor+EIS22";
    case MASK_VENDOR|MASK_EIS|MASK_EIS_QUEUE:
        return "Vendor+EIS22Q";
    case MASK_VENDOR|MASK_EIS|MASK_EIS_25:
        return "Vendor+EIS25";
    case MASK_VENDOR|MASK_EIS|MASK_EIS_25|MASK_EIS_QUEUE:
        return "Vendor+EIS25Q";
    case MASK_VENDOR|MASK_3DNR:
        return "Vendor+3DNR";
    case MASK_VENDOR|MASK_3DNR|MASK_EIS:
        return "Vendor+3DNR+EIS22";
    case MASK_VENDOR|MASK_3DNR|MASK_EIS|MASK_EIS_QUEUE:
        return "Vendor+3DNR+EIS22Q";
    case MASK_VENDOR|MASK_3DNR|MASK_EIS|MASK_EIS_25:
        return "Vendor+3DNR+EIS25";
    case MASK_VENDOR|MASK_3DNR|MASK_EIS|MASK_EIS_25|MASK_EIS_QUEUE:
        return "Vendor+3DNR+EIS25Q";
    default:
        return "UNKNOWN";
    }
}

MBOOL StreamingFeatureRequest::need3DNR() const
{
    return HAS_3DNR(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needVHDR() const
{
    return HAS_VHDR(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needVFB() const
{
    return HAS_VFB(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needExVFB() const
{
    return HAS_VFB_EX(mFeatureMask) && HAS_VFB(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needEIS() const
{
    return HAS_EIS(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needEIS22() const
{
    return HAS_EIS(mFeatureMask) && !HAS_EIS_25(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needEIS25() const
{
    return HAS_EIS(mFeatureMask) && HAS_EIS_25(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needEIS_Q() const
{
    return (HAS_EIS(mFeatureMask) && HAS_EIS_QUEUE(mFeatureMask));
}

MBOOL StreamingFeatureRequest::needVendor() const
{
    return HAS_VENDOR(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needVendorMDP() const
{
    return HAS_VENDOR(mFeatureMask) &&
           (!needEIS() || needEarlyDisplay());
}

MBOOL StreamingFeatureRequest::needGPU() const
{
    return HAS_EIS(mFeatureMask) ||
           (HAS_VFB_EX(mFeatureMask) && HAS_VFB(mFeatureMask));
}

MBOOL StreamingFeatureRequest::needFullImg() const
{
    return mForceIMG3O || HAS_3DNR(mFeatureMask) || HAS_VFB(mFeatureMask) || HAS_EIS(mFeatureMask) || HAS_VENDOR(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needDsImg() const
{
    return HAS_VFB(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needFEFM() const
{
    return needEIS25() && !is4K2K() && EISCustom::isEnabledEIS25_ImageMode();
}

MBOOL StreamingFeatureRequest::needEarlyDisplay() const
{
    // return needEIS25() || needEIS22();
    return HAS_EIS(mFeatureMask);
}

MBOOL StreamingFeatureRequest::needP2AEarlyDisplay() const
{
    return needEarlyDisplay() && !needVendor();
}

MBOOL StreamingFeatureRequest::skipMDPDisplay() const
{
    return needEarlyDisplay();
}

MBOOL StreamingFeatureRequest::needDump() const
{
    return mNeedDump;
}

MBOOL StreamingFeatureRequest::isLastNodeP2A() const
{
    return !HAS_VENDOR(mFeatureMask) &&
           !HAS_EIS(mFeatureMask) &&
           !HAS_VFB(mFeatureMask);
}

MBOOL StreamingFeatureRequest::is4K2K() const
{
    return mIs4K2K;
}

MBOOL StreamingFeatureRequest::isP2ACRZMode() const
{
    return mIsP2ACRZMode;
}

MBOOL StreamingFeatureRequest::useGpuPassThrough() const
{
    return mForceGpuPass;
}

MBOOL StreamingFeatureRequest::useDirectGpuOut() const
{
    MBOOL val = MFALSE;
    if( SUPPORT_GPU_YV12 )
    {
        if( mForceGpuOut )
        {
            val = (mForceGpuOut == FORCE_ON);
        }
        else
        {
            val = this->is4K2K();
        }
    }
    return val;
}

MBOOL StreamingFeatureRequest::getCropInfo(CropGroup group, NSCam::NSIoPipe::EPortCapbility cap, MUINT32 defCropGroup, NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MFALSE;
    unsigned count = 0;
    MUINT32 cropGroup = defCropGroup;
    for( unsigned i = 0, size = mQParams.mvOut.size(); i < size; ++i )
    {
        if( mQParams.mvOut[i].mPortID.capbility == cap )
        {
            switch( mQParams.mvOut[i].mPortID.index )
            {
            case NSImageio::NSIspio::EPortIndex_WDMAO:
                cropGroup = (group == EIS_CROP_GROUP) ? EIS_WDMAO_CROP_GROUP : WDMAO_CROP_GROUP;
                break;
            case NSImageio::NSIspio::EPortIndex_WROTO:
                cropGroup = (group == EIS_CROP_GROUP) ? EIS_WROTO_CROP_GROUP : WROTO_CROP_GROUP;
                break;
            }
        }
    }
    TRACE_FUNC("wanted crop group = %d, found group = %d", defCropGroup, cropGroup);

    for( unsigned i = 0, size = mQParams.mvCropRsInfo.size(); i < size; ++i )
    {
        if( mQParams.mvCropRsInfo[i].mGroupID == cropGroup )
        {
            if( ++count == 1 )
            {
                crop = mQParams.mvCropRsInfo[i];
                TRACE_FUNC("Found crop(%d): %dx%d", crop.mGroupID, crop.mCropRect.s.w, crop.mCropRect.s.h);
            }
        }
    }

    if( count != 1 )
    {
        MY_LOGW("frame %d: suspicious crop(ask/found: %d/%d) number = %d", mRequestNo, defCropGroup, cropGroup, count);
    }
    TRACE_FUNC_EXIT();
    return count >= 1;
}

NSCam::NSIoPipe::NSPostProc::MCrpRsInfo StreamingFeatureRequest::applyEISCropRatio(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo crop, Domain domain, const MSize &maxSize)
{
    double ratio = 100.0 / (mIs4K2K ?
                            EISCustom::getEISFactor() : EISCustom::getEISFHDFactor());
    MSize size = crop.mCropRect.s;
    if( maxSize.w && (maxSize.w < size.w) )
    {
        size.w = maxSize.w;
    }
    if( maxSize.h && (maxSize.h < size.h) )
    {
        size.h = maxSize.h;
    }
    MSize newSize = MSize(size.w*ratio, size.h*ratio);

    if( domain == RRZO_DOMAIN )
    {
        crop.mCropRect.p_integral.x += (size.w - newSize.w) >> 1;
        crop.mCropRect.p_integral.y += (size.h - newSize.h) >> 1;
        crop.mCropRect.s = newSize;
    }
    else // if( domain == WARP_DOMAIN )
    {
        crop.mCropRect.p_integral.x *= ratio;
        crop.mCropRect.p_integral.y *= ratio;
        crop.mCropRect.p_fractional.x = 0;
        crop.mCropRect.p_fractional.y = 0;
        crop.mCropRect.s = newSize;
    }

    return crop;
}

VFBResult::VFBResult()
{
}

VFBResult::VFBResult(const ImgBuffer &dsImg, const ImgBuffer &alphaCL, const ImgBuffer &alphaNR, const ImgBuffer &pca)
    : mDsImg(dsImg)
    , mAlphaCL(alphaCL)
    , mAlphaNR(alphaNR)
    , mPCA(pca)
{
}

MVOID FEFMGroup::clear()
{
    this->High = NULL;
    this->Medium = NULL;
    this->Low = NULL;
}

MBOOL FEFMGroup::isValid() const
{
    return this->High != NULL;
}

} // NSFeaturePipe
} // NSCamFeature
} // NSCam
