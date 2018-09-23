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

#ifndef _MTK_CAMERA_STREAMING_FEATURE_PIPE_STREAMING_FEATURE_DATA_H_
#define _MTK_CAMERA_STREAMING_FEATURE_PIPE_STREAMING_FEATURE_DATA_H_

#include "MtkHeader.h"
//#include <mtkcam/feature/featurePipe/IStreamingFeaturePipe.h>
//#include <mtkcam/common/faces.h>
//#include <mtkcam/featureio/eis_type.h>
#include <camera_custom_eis.h>

#include <utils/RefBase.h>
#include <utils/Vector.h>

#include <featurePipe/core/include/WaitQueue.h>
#include <featurePipe/core/include/IIBuffer.h>

#include "StreamingFeatureTimer.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum Domain { RRZO_DOMAIN, WARP_DOMAIN };

class StreamingFeatureRequest : public virtual android::RefBase
{
private:
    // must allocate extParam before everything else
    FeaturePipeParam mExtParam;

public:
    MUINT32 mFeatureMask;
    const MUINT32 mRequestNo;
    MINT32 mDebugDump;
    StreamingFeatureTimer mTimer;

    NSCam::NSIoPipe::NSPostProc::QParams mP2A_QParams;
    MSize mFullImgSize;
    NSCam::NSIoPipe::NSPostProc::MCropRect mFullImgCrop;
    NSCam::NSIoPipe::NSPostProc::MCropRect mMDPCrop;
    // alias members, do not change initialize order
    VarMap &mVarMap;
    NSCam::NSIoPipe::NSPostProc::QParams &mQParams;
    android::Vector<NSCam::NSIoPipe::NSPostProc::Input> &mvIn;
    android::Vector<NSCam::NSIoPipe::NSPostProc::Output> &mvOut;
    android::Vector<NSCam::NSIoPipe::NSPostProc::MCrpRsInfo> &mvCropRsInfo;

public:
    StreamingFeatureRequest(const FeaturePipeParam &extParam, MUINT32 requestNo);
    ~StreamingFeatureRequest();

    MVOID setDisplayFPSCounter(FPSCounter *counter);
    MVOID setFrameFPSCounter(FPSCounter *counter);

    MBOOL updateResult(MBOOL result);
    MBOOL doExtCallback(FeaturePipeParam::MSG_TYPE msg);

    MSize getMaxOutSize() const;
    MSize getInputSize() const;
    NSCam::NSIoPipe::NSPostProc::MCropRect getP2Crop() const;
    MBOOL getDisplayOutput(NSCam::NSIoPipe::NSPostProc::Output &output);
    MBOOL getRecordOutput(NSCam::NSIoPipe::NSPostProc::Output &output);
    IImageBuffer* getRecordOutputBuffer();
    MBOOL getDisplayCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop);
    MBOOL getRecordCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop);
    MBOOL getEISDisplayCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop, Domain domain, const MSize &maxSize = MSize(0, 0));
    MBOOL getEISRecordCrop(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop, Domain domain, const MSize &maxSize = MSize(0, 0));

    DECLARE_VAR_MAP_INTERFACE(mVarMap, setVar, getVar, tryGetVar, clearVar);

    MVOID setDumpProp(MINT32 start, MINT32 count);
    MVOID setForceIMG3O(MBOOL forceIMG3O);
    MVOID setForceGpuPass(MBOOL forceGpuPass);
    MVOID setForceGpuOut(MUINT32 forceGpuOut);

    const char* getFeatureMaskName() const;
    MBOOL need3DNR() const;
    MBOOL needVHDR() const;
    MBOOL needVFB() const;
    MBOOL needExVFB() const;
    MBOOL needEIS() const;
    MBOOL needEIS22() const;
    MBOOL needEIS25() const;
    MBOOL needEIS_Q() const;
    MBOOL needVendor() const;
    MBOOL needVendorMDP() const;
    MBOOL needGPU() const;
    MBOOL needFullImg() const;
    MBOOL needDsImg() const;
    MBOOL needFEFM() const;
    MBOOL needEarlyDisplay() const;
    MBOOL needP2AEarlyDisplay() const;
    MBOOL skipMDPDisplay() const;
    MBOOL needDump() const;
    MBOOL isLastNodeP2A() const;
    MBOOL is4K2K() const;
    MBOOL isP2ACRZMode() const;
    MBOOL useGpuPassThrough() const;
    MBOOL useDirectGpuOut() const;

private:
    enum CropGroup { EIS_CROP_GROUP, NORMAL_CROP_GROUP };

private:
    MBOOL getCropInfo(CropGroup group, NSCam::NSIoPipe::EPortCapbility cap, MUINT32 defCropGroup, NSCam::NSIoPipe::NSPostProc::MCrpRsInfo &crop);
    NSCam::NSIoPipe::NSPostProc::MCrpRsInfo applyEISCropRatio(NSCam::NSIoPipe::NSPostProc::MCrpRsInfo crop, Domain domain, const MSize &maxSize);

private:
    FPSCounter *mDisplayFPSCounter;
    FPSCounter *mFrameFPSCounter;

private:
    MBOOL mResult;
    MBOOL mNeedDump;
    MBOOL mForceIMG3O;
    MBOOL mForceGpuPass;
    MUINT32 mForceGpuOut;
    MBOOL mIs4K2K;
    MBOOL mIsP2ACRZMode;
};
typedef android::sp<StreamingFeatureRequest> RequestPtr;
typedef android::sp<IIBuffer> ImgBuffer;

template <typename T>
class Data
{
public:
    T mData;
    RequestPtr mRequest;

    // lower value will be process first
    MUINT32 mPriority;

    Data()
        : mPriority(0)
    {}

    virtual ~Data() {}

    Data(const T &data, const RequestPtr &request, MINT32 nice = 0)
        : mData(data)
        , mRequest(request)
      , mPriority(request->mRequestNo)
    {
        if( nice > 0 )
        {
            // TODO: watch out for overflow
            mPriority += nice;
        }
    }

    T& operator->()
    {
        return mData;
    }

    const T& operator->() const
    {
        return mData;
    }

    class IndexConverter
    {
    public:
        IWaitQueue::Index operator()(const Data &data) const
        {
            return IWaitQueue::Index(data.mRequest->mRequestNo,
                                     data.mPriority);
        }
        static unsigned getID(const Data &data)
        {
            return data.mRequest->mRequestNo;
        }
        static unsigned getPriority(const Data &data)
        {
            return data.mPriority;
        }
    };
};


class MyFace
{
public:
    MtkCameraFaceMetadata mMeta;
    MtkCameraFace mFaceBuffer[15];
    MtkFaceInfo mPosBuffer[15];
    MyFace()
    {
        mMeta.faces = mFaceBuffer;
        mMeta.posInfo = mPosBuffer;
        mMeta.number_of_faces = 0;
        mMeta.ImgWidth = 0;
        mMeta.ImgHeight = 0;
    }

    MyFace(const MyFace &src)
    {
        mMeta = src.mMeta;
        mMeta.faces = mFaceBuffer;
        mMeta.posInfo = mPosBuffer;
        for( int i = 0; i < 15; ++i )
        {
            mFaceBuffer[i] = src.mFaceBuffer[i];
            mPosBuffer[i] = src.mPosBuffer[i];
        }
    }

    MyFace operator=(const MyFace &src)
    {
        mMeta = src.mMeta;
        mMeta.faces = mFaceBuffer;
        mMeta.posInfo = mPosBuffer;
        for( int i = 0; i < 15; ++i )
        {
            mFaceBuffer[i] = src.mFaceBuffer[i];
            mPosBuffer[i] = src.mPosBuffer[i];
        }
        return *this;
    }
};

class VFBResult
{
public:
    ImgBuffer mDsImg;
    ImgBuffer mAlphaCL;
    ImgBuffer mAlphaNR;
    ImgBuffer mPCA;

    VFBResult();
    VFBResult(const ImgBuffer &dsImg, const ImgBuffer &alphaCL, const ImgBuffer &alphaNR, const ImgBuffer &pca);
};

class FEFMGroup
{
public:
    ImgBuffer High;
    ImgBuffer Medium;
    ImgBuffer Low;

    ImgBuffer Register_High;
    ImgBuffer Register_Medium;
    ImgBuffer Register_Low;

    MVOID clear();
    MBOOL isValid() const;
};

class FMResult
{
public:
    FEFMGroup FM_A;
    FEFMGroup FM_B;
    FEFMGroup FE;
    FEFMGroup PrevFE;
};

typedef Data<ImgBuffer> ImgBufferData;
typedef Data<EIS_HAL_CONFIG_DATA> EisConfigData;
typedef Data<MyFace> FaceData;
typedef Data<VFBResult> VFBData;
typedef Data<FMResult> FMData;
typedef Data<FeaturePipeParam::MSG_TYPE> CBMsgData;

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam

#endif // _MTK_CAMERA_STREAMING_FEATURE_PIPE_STREAMING_FEATURE_DATA_H_
