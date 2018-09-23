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

#include "FMHal.h"

#define PIPE_MODULE_TAG "FMHal"
#define PIPE_CLASS_TAG "FMHal"
#define PIPE_TRACE TRACE_FM_HAL
#include <featurePipe/core/include/PipeLog.h>


namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

FMHal::Config ConfigTable[2][3] =
{
  // 4Kx2K
  {
    FMHal::Config(MSize(3200,1824), 32),
    FMHal::Config(MSize(1920,1088), 32),
    FMHal::Config(MSize(960,544), 16),
  },
  // FHD
  {
    FMHal::Config(MSize(1600,912), 16),
    FMHal::Config(MSize(960,544), 16),
    FMHal::Config(MSize(480,272), 8),
  },
};

FMHal::Config::Config()
    : mImageSize(0, 0)
    , mBlockSize(1)
    , mFESize(0, 0)
    , mFMSize(0, 0)
{
}

FMHal::Config::Config(const MSize &imageSize, MUINT32 blockSize)
    : mImageSize(imageSize)
    , mBlockSize(blockSize ? blockSize : 1)
    , mFESize(mImageSize.w/mBlockSize*40, mImageSize.h/mBlockSize)
    , mFMSize((mFESize.w/40)*2, mFESize.h)
{
    MUINT32 wMod, hMod;
    wMod = imageSize.w%mBlockSize;
    hMod = imageSize.h%mBlockSize;
    if( wMod || hMod )
    {
        mImageSize.w -= wMod; //? (mBlockSize-wMod) : 0;
        mImageSize.h -= hMod; //? (mBlockSize-hMod) : 0;
        MY_LOGW("Size(%d,%d) not aligned with blocksize(%d), rounding down(%d,%d)", imageSize.w, imageSize.h, mBlockSize, mImageSize.w, mImageSize.h);
        mFESize = MSize(mImageSize.w/mBlockSize*40, mImageSize.h/mBlockSize);
        mFMSize = MSize(mFESize.w/40*2, mFESize.h);
    }
}

MUINT32 FMHal::getConfigCount(FMHal::eTarget target)
{
    MUINT32 count = 0;
    if( target == CFG_4K2K || target == CFG_FHD )
    {
        count = 3;
    }
    return count;
}

MBOOL FMHal::getConfig(const MSize &streamingSize, FMHal::Config config[])
{
    MBOOL ret = MFALSE;
    float ratio = (float)streamingSize.h/streamingSize.w;
    if(ratio < (((3.0/4)+(9.0/16))/2))//16:9
    {
        MY_LOGD("FMHAL 16:9 wxh(%d x %d)",streamingSize.w,streamingSize.h);
        if(streamingSize.w > 1600)
        {
            config[0] = FMHal::Config(MSize(1600,912), 16);
            config[1] = FMHal::Config(MSize(960,544), 16);
            config[2] = FMHal::Config(MSize(480,272), 8);
        }
        else if(streamingSize.w > 960)
        {
            config[0] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 16);
            config[1] = FMHal::Config(MSize(960,544), 16);
            config[2] = FMHal::Config(MSize(480,272), 8);
        }
        else if(streamingSize.w > 480)
        {
            config[0] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 16);
            config[1] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 16);
            config[2] = FMHal::Config(MSize(480,272), 8);
        }
        else
        {
            config[0] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 8);
            config[1] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 8);
            config[2]= FMHal::Config(MSize(streamingSize.w,streamingSize.h), 8);
        }
        ret = MTRUE;
    }
    else
    {
        MY_LOGD("FMHAL 4:3 wxh(%d x %d)",streamingSize.w,streamingSize.h);
        if(streamingSize.w > 1600)
        {
            config[0] = FMHal::Config(MSize(1600,1200), 16);
            config[1] = FMHal::Config(MSize(960,720), 16);
            config[2] = FMHal::Config(MSize(480,360), 8);
        }
        else if(streamingSize.w > 960)
        {
            config[0] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 16);
            config[1] = FMHal::Config(MSize(960,720), 16);
            config[2] = FMHal::Config(MSize(480,360), 8);
        }
        else if(streamingSize.w > 480)
        {
            config[0] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 16);
            config[1]= FMHal::Config(MSize(streamingSize.w,streamingSize.h), 16);
            config[2] = FMHal::Config(MSize(480,360), 8);
        }
        else
        {
            config[0] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 8);
            config[1] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 8);
            config[2] = FMHal::Config(MSize(streamingSize.w,streamingSize.h), 8);
        }
        ret = MTRUE;
    }
    //if( index < FMHal::getConfigCount(target) )
    //{
    //    config = ConfigTable[target][index];
    //    ret = MTRUE;
    //}
    return ret;
}

MBOOL FMHal::configTuning_FE(eTarget target, dip_x_reg_t &tuning, MUINT32 mode)
{
    MBOOL ret = MTRUE;

    tuning.DIP_X_CTL_YUV_EN.Raw = 0;
    tuning.DIP_X_CTL_YUV2_EN.Raw = 0;
    tuning.DIP_X_CTL_RGB_EN.Raw = 0;

    if(mode == FE_MODE_32) // 4K2K
    {
        tuning.DIP_X_FE_CTRL1.Bits.FE_MODE = 0;
        tuning.DIP_X_FE_CTRL1.Bits.FE_PARAM = 4;
        tuning.DIP_X_FE_CTRL1.Bits.FE_FLT_EN = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_TH_G = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_TH_C = 0;
        tuning.DIP_X_FE_CTRL1.Bits.FE_DSCR_SBIT = 3;

        tuning.DIP_X_FE_IDX_CTRL.Bits.FE_XIDX = 0;
        tuning.DIP_X_FE_IDX_CTRL.Bits.FE_YIDX = 0;

        tuning.DIP_X_FE_CROP_CTRL1.Bits.FE_START_X = 0;
        tuning.DIP_X_FE_CROP_CTRL1.Bits.FE_START_Y = 0;

        tuning.DIP_X_FE_CROP_CTRL2.Bits.FE_IN_WD = 0;
        tuning.DIP_X_FE_CROP_CTRL2.Bits.FE_IN_HT = 0;
    }
    else if(mode == FE_MODE_16)
    {
        tuning.DIP_X_FE_CTRL1.Bits.FE_MODE = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_PARAM = 4;
        tuning.DIP_X_FE_CTRL1.Bits.FE_FLT_EN = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_TH_G = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_TH_C = 0;
        tuning.DIP_X_FE_CTRL1.Bits.FE_DSCR_SBIT = 3;

        tuning.DIP_X_FE_IDX_CTRL.Bits.FE_XIDX = 0;
        tuning.DIP_X_FE_IDX_CTRL.Bits.FE_YIDX = 0;

        tuning.DIP_X_FE_CROP_CTRL1.Bits.FE_START_X = 0;
        tuning.DIP_X_FE_CROP_CTRL1.Bits.FE_START_Y = 0;

        tuning.DIP_X_FE_CROP_CTRL2.Bits.FE_IN_WD = 0;
        tuning.DIP_X_FE_CROP_CTRL2.Bits.FE_IN_HT = 0;
    }
    else // mode == FE_MODE_8
    {
        tuning.DIP_X_FE_CTRL1.Bits.FE_MODE = 2;
        tuning.DIP_X_FE_CTRL1.Bits.FE_PARAM = 4;
        tuning.DIP_X_FE_CTRL1.Bits.FE_FLT_EN = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_TH_G = 1;
        tuning.DIP_X_FE_CTRL1.Bits.FE_TH_C = 0;
        tuning.DIP_X_FE_CTRL1.Bits.FE_DSCR_SBIT = 3;

        tuning.DIP_X_FE_IDX_CTRL.Bits.FE_XIDX = 0;
        tuning.DIP_X_FE_IDX_CTRL.Bits.FE_YIDX = 0;

        tuning.DIP_X_FE_CROP_CTRL1.Bits.FE_START_X = 0;
        tuning.DIP_X_FE_CROP_CTRL1.Bits.FE_START_Y = 0;

        tuning.DIP_X_FE_CROP_CTRL2.Bits.FE_IN_WD = 0;
        tuning.DIP_X_FE_CROP_CTRL2.Bits.FE_IN_HT = 0;
    }

    return ret;
}

MBOOL FMHal::configTuning_FM(eTarget target, dip_x_reg_t &tuning, MSize FMSize, MUINT32 mode)
{
    MBOOL ret = MTRUE;
    MUINT32 type = (mode == FE_MODE_8)? 1: 0;
    MUINT32 w = FMSize.w/mode;
    MUINT32 h = FMSize.h/mode;

    //MY_LOGD("mode(%d) size(%dx%d) type(%d) wxh(%dx%d)",mode,FMSize.w,FMSize.h,type,w,h);

    tuning.DIP_X_CTL_YUV_EN.Raw = 0;
    tuning.DIP_X_CTL_YUV2_EN.Raw = 0x00000001;
    tuning.DIP_X_CTL_RGB_EN.Raw = 0;

    tuning.DIP_X_FM_SIZE.Bits.FM_OFFSET_X  = 16;
    tuning.DIP_X_FM_SIZE.Bits.FM_OFFSET_Y  = 16;
    tuning.DIP_X_FM_SIZE.Bits.FM_SR_TYPE   = type;//iblock 16:0 or iblock 8:1

    tuning.DIP_X_FM_TH_CON0.Bits.FM_MIN_RATIO   = 89;
    tuning.DIP_X_FM_TH_CON0.Bits.FM_SAD_TH      = 600;
    tuning.DIP_X_FM_TH_CON0.Bits.FM_RES_TH      = 0;

    tuning.DIP_X_FM_SIZE.Bits.FM_WIDTH = w;//FMSize.w/mode;//data.mFE1Img->getImageBuffer()->getImgSize().w/16;
    tuning.DIP_X_FM_SIZE.Bits.FM_HEIGHT = h;//FMSize.h/mode;//data.mFE1Img->getImageBuffer()->getImgSize().h/16;

    return ret;
}

FMHal::FMHal()
{
}

FMHal::~FMHal()
{
}

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
