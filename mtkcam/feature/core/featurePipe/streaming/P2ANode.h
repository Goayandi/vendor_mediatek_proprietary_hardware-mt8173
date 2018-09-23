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

#ifndef _MTK_CAMERA_STREAMING_FEATURE_PIPE_P2A_NODE_H_
#define _MTK_CAMERA_STREAMING_FEATURE_PIPE_P2A_NODE_H_

#include "StreamingFeatureNode.h"
#include "QParamsBase.h"
#include "P2A_3DNR.h"

#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <feature/include/common/3dnr/3dnr_hal_base.h>
#include <feature/core/featurePipe/core/include/CamThreadNode.h>
#include "FMHal.h"

#include <mtkcam/aaa/IHal3A.h>

using namespace NS3Av3;

class EisHal;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class P2AEnqueData
{
public:
    RequestPtr mRequest;
    ImgBuffer mDsImg;
    ImgBuffer mFullImg;
    ImgBuffer mPrevFullImg;

    FMResult mFMResult;
    ImgBuffer mFE1Img;
    ImgBuffer mFE2Img;
    ImgBuffer mFE3Img;
};

class P2ANode : public StreamingFeatureNode, public QParamsBase<P2AEnqueData>
{
public:
    P2ANode(const char *name);
    virtual ~P2ANode();

    MVOID setNormalStream(NSCam::NSIoPipe::NSPostProc::INormalStream *stream);

    MVOID setDsImgPool(const android::sp<ImageBufferPool> &pool);
    MVOID setFullImgPool(const android::sp<GraphicBufferPool> &pool, MUINT32 allocate = 0);

public:
    virtual MBOOL onData(DataID id, const RequestPtr &data);

protected:
    virtual MBOOL onInit();
    virtual MBOOL onUninit();
    virtual MBOOL onThreadStart();
    virtual MBOOL onThreadStop();
    virtual MBOOL onThreadLoop();

protected:
    virtual MBOOL onQParamsCB(const QParams &params, const P2AEnqueData &request);

private:
    MBOOL initP2();
    MVOID uninitP2();
    MBOOL processP2A(const RequestPtr &request);
    MBOOL calcSizeInfo(const RequestPtr &request);
    MRect calcPostCropSize(const RequestPtr &request);
    MBOOL prepareQParams(QParams &params, const RequestPtr &request);
    MBOOL prepareCropInfo(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareCRZCrop(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareEarlyDisplayCrop(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MCrpRsInfo calViewAngleCrop(QParams &params, const RequestPtr &request, MRect postCropSize);
    MBOOL prepareStreamTag(QParams &params, const RequestPtr &request);
    MBOOL prepareIO(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareFullImg(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareDsImg(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareVIPI(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareLCEI(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareFEFM(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID prepareEarlyDisplayImg(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MVOID enqueFeatureStream(QParams &params, P2AEnqueData &data);

    MBOOL needDigitalZoomCrop(const RequestPtr &request);
    MVOID printRequestIO(const RequestPtr &request);

private:
    MBOOL init3A();
    MVOID uninit3A();
    MBOOL prepare3A(QParams &params, const RequestPtr &request);

private:
    MBOOL initEIS();
    MVOID uninitEIS();
    MBOOL prepareEIS(QParams &params, const RequestPtr &reqquest, const MCrpRsInfo outCropInfo);

private:
    struct eis_region
    {
        MUINT32 x_int;
        MUINT32 x_float;
        MUINT32 y_int;
        MUINT32 y_float;
        MSize s;
        MINT32 gmvX;
        MINT32 gmvY;
    };

    MBOOL init3DNR();
    MVOID uninit3DNR();
    MBOOL prepare3DNR(QParams &params, const RequestPtr &request);
    MVOID dump_Qparam(QParams& rParams, const char *pSep);
    MVOID dump_vOutImageBuffer(const QParams & params);
    MVOID dump_imgiImageBuffer(const QParams & params);

    MBOOL do3dnrFlow(
        NSCam::NSIoPipe::NSPostProc::QParams &enqueParams,
        const RequestPtr &request,
        MRect &dst_resizer_rect,
        eis_region &eisInfo,
        MINT32 iso,
        MUINT32 requestNo
        );

    MBOOL mb3dnrInitedSuccess;
    MBOOL mb3dnrDemoEnabled;
    NSCam::NSIoPipe::NSPostProc::hal3dnrBase* mp3dnr;
    MINT32 m3dnrLogLevel;
    NSCam::NSIoPipe::NSPostProc::NR3DParam* mpNr3dParam;

    IHal3A* mp3A;

private:
    // VHDR functions, implemented in P2A_VHDR.cpp
    MBOOL initVHDR();
    MVOID uninitVHDR();
    MBOOL prepareVHDR(QParams &params, const RequestPtr &request);
    // VHDR members
    //VHdrHal *mVHDR;

    // EIS: FE/FM functions, implemented in P2A_FMFE.cpp
    MBOOL initFEFM();
    MVOID uninitFEFM();
    MBOOL prepareCropInfo_FE(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MBOOL prepareFE(QParams &params, const RequestPtr &request, P2AEnqueData &data);
    MBOOL prepareFM(QParams &params, const RequestPtr &request, P2AEnqueData &data);

private:
    WaitQueue<RequestPtr> mRequests;

    NSCam::NSIoPipe::NSPostProc::INormalStream *mNormalStream;
    android::Mutex mEnqueMutex;
    EisHal*   mpEisHal;
    FMHal::Config mFM_FE_cfg[3];

    MUINT32 mFullImgPoolAllocateNeed;

    android::sp<ImageBufferPool> mDsImgPool;
    android::sp<GraphicBufferPool> mFullImgPool;
    android::sp<ImageBufferPool> mFE1ImgPool;
    android::sp<ImageBufferPool> mFE2ImgPool;
    android::sp<ImageBufferPool> mFE3ImgPool;
    android::sp<FatImageBufferPool> mFEOutputPool;
    android::sp<FatImageBufferPool> mFEOutputPool_m;
    android::sp<FatImageBufferPool> mFMOutputPool;
    android::sp<FatImageBufferPool> mFMOutputPool_m;
    android::sp<FatImageBufferPool> mFMRegisterPool;
    ImgBuffer mPrevFullImg;

    FEFMGroup mPrevFE;
    dip_x_reg_t tuningDat_FE_H;
    dip_x_reg_t tuningDat_FE_M;
    dip_x_reg_t tuningDat_FE_L;

    dip_x_reg_t tuningDat_FM_P_N_H;
    dip_x_reg_t tuningDat_FM_P_N_M;
    dip_x_reg_t tuningDat_FM_P_N_L;

    dip_x_reg_t tuningDat_FM_N_P_H;
    dip_x_reg_t tuningDat_FM_N_P_M;
    dip_x_reg_t tuningDat_FM_N_P_L;

    _SRZ_SIZE_INFO_ mpsrz1Param;
    _SRZ_SIZE_INFO_ mpsrz1_2Param;
    _SRZ_SIZE_INFO_ mpsrz1_3Param;

    MUINT32   mCropMode;
    MUINT32   mEisMode;


    typedef enum
    {
        CROP_MODE_NONE = 0,
        CROP_MODE_USE_CRZ
    } CROP_MODE_ENUM;

};

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam

#endif // _MTK_CAMERA_STREAMING_FEATURE_PIPE_P2A_NODE_H_
