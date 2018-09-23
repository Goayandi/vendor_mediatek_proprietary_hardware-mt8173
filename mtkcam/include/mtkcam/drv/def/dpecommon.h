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

#ifndef _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_DPE_COMMON_H_
#define _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_DPE_COMMON_H_
//
#include <mtkcam/def/common.h>


/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {

enum DPEIMGIFORMAT {
    DPE_IMGI_Y_FMT = 0,
    DPE_IMGI_YC_FMT,
    DPE_IMGI_CY_FMT
};

enum DPEDMAPort {
    DMA_DVE_NONE = 0,
    DMA_DVE_IMGI,
    DMA_DVE_DVI,
    DMA_DVE_MASKI,
    DMA_DVE_DVO,
    DMA_DVE_CONFO,
    DMA_DVE_RESPO,
    DMA_WMFE_IMGI,
    DMA_WMFE_DPI,
    DMA_WMFE_TBLI,
    DMA_WMFE_DPO,
};


enum DVE_VERT_DSMODE {
    DVE_VERT_DS_MODE_DIV8 = 0,
    DVE_VERT_DS_MODE_DIV4
};

enum DVE_HORZ_DSMODE {
    DVE_HORZ_DS_MODE_DIV8 = 0,
    DVE_HORZ_DS_MODE_DIV4
};


struct DPEBufInfo
{
    DPEDMAPort  dmaport;
    MINT32      memID;      //  memory ID
    MUINTPTR    u4BufVA;    //  Vir Address of pool
    MUINTPTR    u4BufPA;    //  Phy Address of pool
    MUINT32     u4BufSize;  //  Per buffer size
    MUINT32     u4Stride;  //  Buffer Stride
        DPEBufInfo()
        : dmaport(DMA_DVE_NONE)
        , memID(0)
        , u4BufVA(0)
        , u4BufPA(0)
        , u4BufSize(0)
        , u4Stride(0)
        {
        }
};


struct DVEBBox
{
        MUINT32 DVE_ORG_BBOX_RIGHT;
        MUINT32 DVE_ORG_BBOX_LEFT;
        MUINT32 DVE_ORG_BBOX_BOTTOM;
        MUINT32 DVE_ORG_BBOX_TOP;
        DVEBBox()
        : DVE_ORG_BBOX_RIGHT(0)
        , DVE_ORG_BBOX_LEFT(0)
        , DVE_ORG_BBOX_BOTTOM(0)
        , DVE_ORG_BBOX_TOP(0)
        {
        }
};

struct DVECandidate
{
        MUINT32 DVE_CAND_SEL;
        MUINT32 DVE_CAND_TYPE;
        DVECandidate()
        : DVE_CAND_SEL(0)
        , DVE_CAND_TYPE(0)
        {
        }
};

struct DVEPenalty
{
    unsigned char DVE_REFINE_PNLTY_SEL;
    unsigned char DVE_GMV_PNLTY_SEL;
    unsigned char DVE_PREV_PNLTY_SEL;
    unsigned char DVE_NBR_PNLTY_SEL;
    unsigned char DVE_RAND_PNLTY_SEL;
    unsigned char DVE_TMPR_PNLTY_SEL;
    unsigned char DVE_SPTL_PNLTY_SEL;
    unsigned char DVE_RAND_COST;
    unsigned char DVE_GMV_COST;
    unsigned char DVE_PREV_COST;
    unsigned char DVE_NBR_COST;
    unsigned char DVE_REFINE_COST;
    unsigned char DVE_TMPR_COST;
    unsigned char DVE_SPTL_COST;
        DVEPenalty()
        : DVE_REFINE_PNLTY_SEL(2)
        , DVE_GMV_PNLTY_SEL(2)
        , DVE_PREV_PNLTY_SEL(2)
        , DVE_NBR_PNLTY_SEL(2)
        , DVE_RAND_PNLTY_SEL(2)
        , DVE_TMPR_PNLTY_SEL(2)
        , DVE_SPTL_PNLTY_SEL(2)
        , DVE_RAND_COST(32)
        , DVE_GMV_COST(3)
        , DVE_PREV_COST(2)
        , DVE_NBR_COST(1)
        , DVE_REFINE_COST(1)
        , DVE_TMPR_COST(3)
        , DVE_SPTL_COST(0)
        {
        }
};


struct  DVEConfig
{
    bool                            Dve_Skp_Pre_Dv;
    bool                            Dve_Mask_En;
    bool                            Dve_l_Bbox_En;
    bool                            Dve_r_Bbox_En;
    MUINT32                         Dve_Horz_Ds_Mode;
    MUINT32                         Dve_Vert_Ds_Mode;
    DPEIMGIFORMAT                   Dve_Imgi_l_Fmt;
    DPEIMGIFORMAT                   Dve_Imgi_r_Fmt;
    DVEBBox                         Dve_Org_l_Bbox;
    DVEBBox                         Dve_Org_r_Bbox;
    MUINT32                         Dve_Org_Width;
    MUINT32                         Dve_Org_Height;
    MUINT32                         Dve_Org_Horz_Sr_0;
    MUINT32                         Dve_Org_Horz_Sr_1;
    MUINT32                         Dve_Org_Vert_Sr_0;
    MUINT32                         Dve_Org_Start_Vert_Sv;
    MUINT32                         Dve_Org_Start_Horz_Sv;
    MUINT32                         Dve_Cand_Num;
    DVECandidate                    Dve_Cand_0;
    DVECandidate                    Dve_Cand_1;
    DVECandidate                    Dve_Cand_2;
    DVECandidate                    Dve_Cand_3;
    DVECandidate                    Dve_Cand_4;
    DVECandidate                    Dve_Cand_5;
    DVECandidate                    Dve_Cand_6;
    DVECandidate                    Dve_Cand_7;
    MUINT32                         Dve_Rand_Lut_0;
    MUINT32                         Dve_Rand_Lut_1;
    MUINT32                         Dve_Rand_Lut_2;
    MUINT32                         Dve_Rand_Lut_3;
    MUINT32                         DVE_VERT_GMV;
    MUINT32                         DVE_HORZ_GMV;
    MUINT32                         Dve_Horz_Dv_Ini;    //DVE Initial Disparity Vector
    MUINT32                         Dve_Coft_Shift;     //Shift factor of block texture
    MUINT32                         Dve_Corner_Th;      //Threshold of block variance.
    MUINT32                         Dve_Smth_Luma_Th_1;
    MUINT32                         Dve_Smth_Luma_Th_0;
    MUINT32                         Dve_Smth_Luma_Ada_Base;
    MUINT32                         Dve_Smth_Luma_Horz_Pnlty_Sel;
    bool                            Dve_Smth_Dv_Mode;
    MUINT32                         Dve_Smth_Dv_Th_1;
    MUINT32                         Dve_Smth_Dv_Th_0;
    MUINT32                         Dve_Smth_Dv_Ada_Base;
    MUINT32                         Dve_Smth_Dv_Vert_Pnlty_Sel;
    MUINT32                         Dve_Smth_Dv_Horz_Pnlty_Sel;
    MUINT32                         Dve_Ord_Pnlty_Sel;
    MUINT32                         Dve_Ord_Coring;
    MUINT32                         Dve_Ord_Th;
    DVEPenalty                      Dve_Type_Penality_Ctrl;
    DPEBufInfo                      Dve_Imgi_l;     //Input Left View Imgi DMA Port
    DPEBufInfo                      Dve_Imgi_r;     //Input Right View Imgi DMA Port
    DPEBufInfo                      Dve_Dvi_l;      //Input Left View DVI DMA Port (last disparity vector)
    DPEBufInfo                      Dve_Dvi_r;      //Input Right View DVI DMA Port (last disparity vector)
    DPEBufInfo                      Dve_Maski_l;
    DPEBufInfo                      Dve_Maski_r;
    DPEBufInfo                      Dve_Dvo_l;
    DPEBufInfo                      Dve_Dvo_r;
    DPEBufInfo                      Dve_Confo_l;
    DPEBufInfo                      Dve_Confo_r;
    DPEBufInfo                      Dve_Respo_l;
    DPEBufInfo                      Dve_Respo_r;
    MUINT32                         Dve_Vert_Sv;  //ReadOnly, DVE Statistic Result 0
    MUINT32                         Dve_Horz_Sv;    //ReadOnly, DVE Statistic Result 0
                                    //
                                    DVEConfig()     //HW Default value
                                        : Dve_Skp_Pre_Dv(false)
                                        , Dve_Mask_En(false)
                                        , Dve_l_Bbox_En(false)
                                        , Dve_r_Bbox_En(false)
                                        , Dve_Horz_Ds_Mode(0)
                                        , Dve_Vert_Ds_Mode(0)
                                        , Dve_Org_Width(0)
                                        , Dve_Org_Height(0)
                                        , Dve_Org_Horz_Sr_0(512)
                                        , Dve_Org_Horz_Sr_1(512)
                                        , Dve_Org_Vert_Sr_0(24)
                                        , Dve_Org_Start_Vert_Sv(0)
                                        , Dve_Org_Start_Horz_Sv(0)
                                        , Dve_Cand_Num(8)
                                        , Dve_Rand_Lut_0(11)
                                        , Dve_Rand_Lut_1(23)
                                        , Dve_Rand_Lut_2(31)
                                        , Dve_Rand_Lut_3(43)
                                        , Dve_Horz_Dv_Ini(0)
                                        , Dve_Coft_Shift(2)
                                        , Dve_Corner_Th(16)
                                        , Dve_Smth_Luma_Th_1(0)
                                        , Dve_Smth_Luma_Th_0(12)
                                        , Dve_Smth_Luma_Ada_Base(0)
                                        , Dve_Smth_Luma_Horz_Pnlty_Sel(1)
                                        , Dve_Smth_Dv_Mode(true)
                                        , Dve_Smth_Dv_Th_1(0)
                                        , Dve_Smth_Dv_Th_0(12)
                                        , Dve_Smth_Dv_Ada_Base(5)
                                        , Dve_Smth_Dv_Vert_Pnlty_Sel(1)
                                        , Dve_Smth_Dv_Horz_Pnlty_Sel(2)
                                        , Dve_Ord_Pnlty_Sel(1)
                                        , Dve_Ord_Coring(4)
                                        , Dve_Ord_Th(256)
                                        , Dve_Vert_Sv(4)
                                        , Dve_Horz_Sv(1)
                                        , DVE_VERT_GMV(0)
                                        , DVE_HORZ_GMV(0)
                                        , Dve_Imgi_l_Fmt(DPE_IMGI_Y_FMT)
                                        , Dve_Imgi_r_Fmt(DPE_IMGI_Y_FMT)
                                    {
                                    }
};




enum WMFEFILTERSIZE {
    WMFE_FILTER_SIZE_1x1 = 0,
    WMFE_FILTER_SIZE_3x3,
    WMFE_FILTER_SIZE_5x5,
    WMFE_FILTER_SIZE_7x7,
};

enum WMFEDPIFORMAT {
    WMFE_DPI_D_FMT = 0,
    WMFE_DPI_DX_FMT,
    WMFE_DPI_XD_FMT
};


struct  WMFECtrl
{
    bool Wmfe_Enable;
    WMFEFILTERSIZE WmfeFilterSize;
    MUINT32 Wmfe_Width;
    MUINT32 Wmfe_Height;
    DPEIMGIFORMAT WmfeImgiFmt;
    WMFEDPIFORMAT WmfeDpiFmt;
    DPEBufInfo Wmfe_Imgi;
    DPEBufInfo Wmfe_Dpi;
    DPEBufInfo Wmfe_Tbli;
    DPEBufInfo Wmfe_Dpo;
};



struct  WMFEConfig
{
    WMFECtrl Wmfe_Ctrl_0;
    WMFECtrl Wmfe_Ctrl_1;
    WMFECtrl Wmfe_Ctrl_2;
};

/******************************************************************************
 *
 ******************************************************************************/
};  //namespace NSIoPipe
};  //namespace NSCam
#endif  //_MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_DPE_COMMON_H_

