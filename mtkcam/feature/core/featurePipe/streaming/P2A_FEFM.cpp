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
#include "FMHal.h"

#define PIPE_CLASS_TAG "P2A_FEFM"
#define PIPE_TRACE TRACE_P2A_FEFM
#include <featurePipe/core/include/PipeLog.h>

using NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Normal;
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
using NSImageio::NSIspio::EPortIndex_REGI;


namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

/* NOTE : this file just implements some FEFM function in P2ANode.h
* Some object might declare in P2ANode
*/

MBOOL P2ANode::initFEFM()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MVOID P2ANode::uninitFEFM()
{
    TRACE_FUNC_ENTER();
    TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepareCropInfo_FE(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();

    //----------- round 1------------------------------------------
    MCrpRsInfo crop;
    crop.mGroupID = 2;//wdma
    crop.mFrameGroup = 0;
    crop.mCropRect = MCropRect(MPoint(0, 0), request->mFullImgSize);
    crop.mResizeDst = data.mFE1Img->getImageBuffer()->getImgSize();
    params.mvCropRsInfo.push_back(crop);

    if( request->needP2AEarlyDisplay() )
    {
        MCrpRsInfo eis_displaycrop;
        if( request->getEISDisplayCrop(eis_displaycrop, RRZO_DOMAIN) ||
            request->getEISRecordCrop(eis_displaycrop, RRZO_DOMAIN) )
        {
            crop.mCropRect = eis_displaycrop.mCropRect;
            crop.mResizeDst = eis_displaycrop.mResizeDst;
        }
        else
        {
            MY_LOGD("default display crop");
            crop.mCropRect = MCropRect(MPoint(0, 0), request->mFullImgSize);
            crop.mResizeDst = request->mFullImgSize;
        }
        crop.mGroupID = 3;//wrot
        crop.mFrameGroup = 0;
        params.mvCropRsInfo.push_back(crop);
    }

    //srz config
    params.mvModuleData.clear();
    NSCam::NSIoPipe::NSPostProc::ModuleInfo module;
    module.moduleTag = EDipModule_SRZ1;
    module.frameGroup = 0;
    //_SRZ_SIZE_INFO_ *mpsrz1Param = new _SRZ_SIZE_INFO_;
    mpsrz1Param.in_w = request->getInputSize().w;
    mpsrz1Param.in_h = request->getInputSize().h;
    mpsrz1Param.crop_floatX = 0;
    mpsrz1Param.crop_floatY = 0;
    mpsrz1Param.crop_x = 0;
    mpsrz1Param.crop_y = 0;
    mpsrz1Param.crop_w = request->getInputSize().w;
    mpsrz1Param.crop_h = request->getInputSize().h;
    mpsrz1Param.out_w = mFM_FE_cfg[0].mImageSize.w;
    mpsrz1Param.out_h = mFM_FE_cfg[0].mImageSize.h;
    module.moduleStruct = reinterpret_cast<MVOID*> (&mpsrz1Param);
    params.mvModuleData.push_back(module);
    //----------- round 1------------------------------------------

    //----------- round 2------------------------------------------
    crop.mGroupID = 2;//wdma
    crop.mFrameGroup = 1;
    crop.mCropRect = MCropRect(MPoint(0, 0), data.mFE1Img->getImageBuffer()->getImgSize());
    crop.mResizeDst = data.mFE2Img->getImageBuffer()->getImgSize();
    params.mvCropRsInfo.push_back(crop);

    //srz config
    NSCam::NSIoPipe::NSPostProc::ModuleInfo module_2;
    module_2.moduleTag = EDipModule_SRZ1;
    module_2.frameGroup = 1;
    //_SRZ_SIZE_INFO_ *mpsrz1_2Param = new _SRZ_SIZE_INFO_;
    mpsrz1_2Param.in_w = data.mFE1Img->getImageBuffer()->getImgSize().w;
    mpsrz1_2Param.in_h = data.mFE1Img->getImageBuffer()->getImgSize().h;
    mpsrz1_2Param.crop_floatX = 0;
    mpsrz1_2Param.crop_floatY = 0;
    mpsrz1_2Param.crop_x = 0;
    mpsrz1_2Param.crop_y = 0;
    mpsrz1_2Param.crop_w = data.mFE1Img->getImageBuffer()->getImgSize().w;
    mpsrz1_2Param.crop_h = data.mFE1Img->getImageBuffer()->getImgSize().h;
    mpsrz1_2Param.out_w = mFM_FE_cfg[1].mImageSize.w;//960;//data.mFE2Img->getImageBuffer()->getImgSize().w;
    mpsrz1_2Param.out_h = mFM_FE_cfg[1].mImageSize.h;//544;//data.mFE2Img->getImageBuffer()->getImgSize().h;
    module_2.moduleStruct = reinterpret_cast<MVOID*> (&mpsrz1_2Param);
    params.mvModuleData.push_back(module_2);
    //----------- round 2------------------------------------------

    //----------- round 3------------------------------------------
    crop.mGroupID = 2;//wdma, need to remove later
    crop.mFrameGroup = 2;
    crop.mCropRect = MCropRect(MPoint(0, 0), data.mFE2Img->getImageBuffer()->getImgSize());
    crop.mResizeDst = data.mFE2Img->getImageBuffer()->getImgSize();
    params.mvCropRsInfo.push_back(crop);

    //srz config
    NSCam::NSIoPipe::NSPostProc::ModuleInfo module_3;
    module_3.moduleTag = EDipModule_SRZ1;
    module_3.frameGroup = 2;
    //_SRZ_SIZE_INFO_ *mpsrz1_3Param = new _SRZ_SIZE_INFO_;
    mpsrz1_3Param.in_w = data.mFE2Img->getImageBuffer()->getImgSize().w;
    mpsrz1_3Param.in_h = data.mFE2Img->getImageBuffer()->getImgSize().h;
    mpsrz1_3Param.crop_floatX = 0;
    mpsrz1_3Param.crop_floatY = 0;
    mpsrz1_3Param.crop_x = 0;
    mpsrz1_3Param.crop_y = 0;
    mpsrz1_3Param.crop_w = data.mFE2Img->getImageBuffer()->getImgSize().w;
    mpsrz1_3Param.crop_h = data.mFE2Img->getImageBuffer()->getImgSize().h;
    mpsrz1_3Param.out_w = data.mFE2Img->getImageBuffer()->getImgSize().w;
    mpsrz1_3Param.out_h = data.mFE2Img->getImageBuffer()->getImgSize().h;
    module_3.moduleStruct = reinterpret_cast<MVOID*> (&mpsrz1_3Param);
    params.mvModuleData.push_back(module_3);
    //----------- round 3------------------------------------------

    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::prepareFE(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    Input input;
    Output output;
    dip_x_reg_t *isp_reg = NULL;

    if( params.mvTuningData.size() > 0 )
    {
        isp_reg = (dip_x_reg_t*) params.mvTuningData[0];
    }
    if ( isp_reg == NULL)
    {
        isp_reg = &tuningDat_FE_H;
        params.mvTuningData.push_back((MVOID*)isp_reg);
        MY_LOGW("No tuning data! Use dummy FE tuning data as fallback.");
    }

    //----------- round 1------------------------------------------
    output.mPortID = PortID(EPortType_Memory, EPortIndex_FEO, PORTID_OUT);
    output.mPortID.group = 0;
    output.mBuffer = data.mFMResult.FE.High->getImageBufferPtr();
    params.mvOut.push_back(output);

    if( request->needP2AEarlyDisplay() )
    {
        Output display_output;
        if( request->getDisplayOutput(display_output) )
        {
            display_output.mPortID = PortID(EPortType_Memory, EPortIndex_WROTO, PORTID_OUT);
            display_output.mPortID.group = 0;
            params.mvOut.push_back(display_output);
        }
    }

    output.mPortID = PortID(EPortType_Memory, EPortIndex_WDMAO, PORTID_OUT);
    output.mPortID.group = 0;
    output.mBuffer = data.mFE1Img->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 16)
    isp_reg->DIP_X_FE_CTRL1.Bits.FE_MODE = 1;
    isp_reg->DIP_X_FE_CTRL1.Bits.FE_PARAM = 4;
    isp_reg->DIP_X_FE_CTRL1.Bits.FE_FLT_EN = 1;
    isp_reg->DIP_X_FE_CTRL1.Bits.FE_TH_G = 1;
    isp_reg->DIP_X_FE_CTRL1.Bits.FE_TH_C = 0;
    isp_reg->DIP_X_FE_CTRL1.Bits.FE_DSCR_SBIT = 3;

    isp_reg->DIP_X_FE_IDX_CTRL.Bits.FE_XIDX = 0;
    isp_reg->DIP_X_FE_IDX_CTRL.Bits.FE_YIDX = 0;

    isp_reg->DIP_X_FE_CROP_CTRL1.Bits.FE_START_X = 0;
    isp_reg->DIP_X_FE_CROP_CTRL1.Bits.FE_START_Y = 0;

    isp_reg->DIP_X_FE_CROP_CTRL2.Bits.FE_IN_WD = 0;
    isp_reg->DIP_X_FE_CROP_CTRL2.Bits.FE_IN_HT = 0;
    //----------- round 1------------------------------------------

    //----------- round 2------------------------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_IMGI, PORTID_IN);
    input.mPortID.group = 1;
    input.mBuffer = data.mFE1Img->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_WDMAO, PORTID_OUT);
    output.mPortID.group = 1;
    output.mBuffer = data.mFE2Img->getImageBufferPtr();
    params.mvOut.push_back(output);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_FEO, PORTID_OUT);
    output.mPortID.group = 1;
    output.mBuffer = data.mFMResult.FE.Medium->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 16)
    FMHal::configTuning_FE(FMHal::CFG_FHD, tuningDat_FE_M, FE_MODE_16);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FE_M);
    params.mvStreamTag.push_back(ENormalStreamTag_Normal);
    //----------- round 2------------------------------------------

    //----------- round 3------------------------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_IMGI, PORTID_IN);
    input.mPortID.group = 2;
    input.mBuffer = data.mFE2Img->getImageBufferPtr();
    params.mvIn.push_back(input);

    //need to add output bufer for MDP
    output.mPortID = PortID(EPortType_Memory, EPortIndex_WDMAO, PORTID_OUT);
    output.mPortID.group = 2;
    output.mBuffer = data.mFE3Img->getImageBufferPtr();
    params.mvOut.push_back(output);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_FEO, PORTID_OUT);
    output.mPortID.group = 2;
    output.mBuffer = data.mFMResult.FE.Low->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 8)
    FMHal::configTuning_FE(FMHal::CFG_FHD, tuningDat_FE_L, FE_MODE_8);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FE_L);
    params.mvStreamTag.push_back(ENormalStreamTag_Normal);
    //----------- round 3------------------------------------------
    TRACE_FUNC_EXIT();
    return MTRUE;
}

MBOOL P2ANode::prepareFM(QParams &params, const RequestPtr &request, P2AEnqueData &data)
{
    TRACE_FUNC_ENTER();
    Input input;
    Output output;
    MSize FMSize = mFM_FE_cfg[1].mImageSize;



    //-----------------------Previous to Current----------------------------
    //----------------------high-------------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_REGI, PORTID_IN);
    input.mPortID.group = 3;
    input.mBuffer = data.mFMResult.FM_A.Register_High->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DEPI, PORTID_IN);
    input.mPortID.group = 3;
    input.mBuffer = data.mFMResult.PrevFE.High->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DMGI, PORTID_IN);
    input.mPortID.group = 3;
    input.mBuffer = data.mFMResult.FE.High->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_MFBO, PORTID_OUT);
    output.mPortID.group = 3;
    output.mBuffer =  data.mFMResult.FM_A.High->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 16)
    FMHal::configTuning_FM(FMHal::CFG_FHD, tuningDat_FM_P_N_H, mFM_FE_cfg[0].mImageSize, FE_MODE_16);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FM_P_N_H);
    params.mvStreamTag.push_back(ENormalStreamTag_FM);
    //----------------------high-------------------------------

    //----------------------Medium----------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_REGI, PORTID_IN);
    input.mPortID.group = 4;
    input.mBuffer = data.mFMResult.FM_A.Register_Medium->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DEPI, PORTID_IN);
    input.mPortID.group = 4;
    input.mBuffer = data.mFMResult.PrevFE.Medium->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DMGI, PORTID_IN);
    input.mPortID.group = 4;
    input.mBuffer = data.mFMResult.FE.Medium->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_MFBO, PORTID_OUT);
    output.mPortID.group = 4;
    output.mBuffer =  data.mFMResult.FM_A.Medium->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 16)
    FMHal::configTuning_FM(FMHal::CFG_FHD, tuningDat_FM_P_N_M, FMSize, FE_MODE_16);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FM_P_N_M);
    params.mvStreamTag.push_back(ENormalStreamTag_FM);
    //----------------------Medium----------------------------

    //----------------------Low-------------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_REGI, PORTID_IN);
    input.mPortID.group = 5;
    input.mBuffer = data.mFMResult.FM_A.Register_Low->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DEPI, PORTID_IN);
    input.mPortID.group = 5;
    input.mBuffer = data.mFMResult.PrevFE.Low->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DMGI, PORTID_IN);
    input.mPortID.group = 5;
    input.mBuffer = data.mFMResult.FE.Low->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_MFBO, PORTID_OUT);
    output.mPortID.group = 5;
    output.mBuffer =  data.mFMResult.FM_A.Low->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 8)
    FMHal::configTuning_FM(FMHal::CFG_FHD, tuningDat_FM_P_N_L, data.mFE2Img->getImageBuffer()->getImgSize(), FE_MODE_8);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FM_P_N_L);
    params.mvStreamTag.push_back(ENormalStreamTag_FM);
    //----------------------Low-------------------------------
    //-----------------------Previous to Current----------------------------


    //-----------------------Current to Previous----------------------------
    //----------------------high-------------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_REGI, PORTID_IN);
    input.mPortID.group = 6;
    input.mBuffer = data.mFMResult.FM_B.Register_High->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DEPI, PORTID_IN);
    input.mPortID.group = 6;
    input.mBuffer = data.mFMResult.FE.High->getImageBufferPtr();
    //input.mBuffer = data.mFMResult.PrevFE.High->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DMGI, PORTID_IN);
    input.mPortID.group = 6;
    input.mBuffer = data.mFMResult.PrevFE.High->getImageBufferPtr();
    //input.mBuffer = data.mFMResult.FE.High->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_MFBO, PORTID_OUT);
    output.mPortID.group = 6;
    output.mBuffer =  data.mFMResult.FM_B.High->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 16)
    FMHal::configTuning_FM(FMHal::CFG_FHD, tuningDat_FM_N_P_H,  mFM_FE_cfg[0].mImageSize, FE_MODE_16);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FM_N_P_H);
    params.mvStreamTag.push_back(ENormalStreamTag_FM);
    //----------------------high-------------------------------

    //----------------------Medium----------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_REGI, PORTID_IN);
    input.mPortID.group = 7;
    input.mBuffer = data.mFMResult.FM_B.Register_Medium->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DEPI, PORTID_IN);
    input.mPortID.group = 7;
    input.mBuffer = data.mFMResult.FE.Medium->getImageBufferPtr();
    //input.mBuffer = data.mFMResult.PrevFE.Medium->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DMGI, PORTID_IN);
    input.mPortID.group = 7;
    input.mBuffer = data.mFMResult.PrevFE.Medium->getImageBufferPtr();
    //input.mBuffer = data.mFMResult.FE.Medium->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_MFBO, PORTID_OUT);
    output.mPortID.group = 7;
    output.mBuffer =  data.mFMResult.FM_B.Medium->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 16)
    FMHal::configTuning_FM(FMHal::CFG_FHD, tuningDat_FM_N_P_M, FMSize, FE_MODE_16);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FM_N_P_M);
    params.mvStreamTag.push_back(ENormalStreamTag_FM);
    //----------------------Medium----------------------------

    //----------------------Low-------------------------------
    input.mPortID = PortID(EPortType_Memory, EPortIndex_REGI, PORTID_IN);
    input.mPortID.group = 8;
    input.mBuffer = data.mFMResult.FM_B.Register_Low->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DEPI, PORTID_IN);
    input.mPortID.group = 8;
    input.mBuffer = data.mFMResult.FE.Low->getImageBufferPtr();
    //input.mBuffer = data.mFMResult.PrevFE.Low->getImageBufferPtr();
    params.mvIn.push_back(input);

    input.mPortID = PortID(EPortType_Memory, EPortIndex_DMGI, PORTID_IN);
    input.mPortID.group = 8;
    input.mBuffer = data.mFMResult.PrevFE.Low->getImageBufferPtr();
    //input.mBuffer = data.mFMResult.FE.Low->getImageBufferPtr();
    params.mvIn.push_back(input);

    output.mPortID = PortID(EPortType_Memory, EPortIndex_MFBO, PORTID_OUT);
    output.mPortID.group = 8;
    output.mBuffer =  data.mFMResult.FM_B.Low->getImageBufferPtr();
    params.mvOut.push_back(output);

    //tuning config(FE MODE 8)
    FMHal::configTuning_FM(FMHal::CFG_FHD, tuningDat_FM_N_P_L, data.mFE2Img->getImageBuffer()->getImgSize(), FE_MODE_8);
    params.mvTuningData.push_back((MVOID*)&tuningDat_FM_N_P_L);
    params.mvStreamTag.push_back(ENormalStreamTag_FM);
    //----------------------Low-------------------------------
    //-----------------------Current to Previous----------------------------

    TRACE_FUNC_EXIT();
    return MTRUE;
}



} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
