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

#ifndef _MTK_HARDWARE_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_PORT_INDEX_H_
#define _MTK_HARDWARE_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_PORT_INDEX_H_

/******************************************************************************
 *
 ******************************************************************************/


/******************************************************************************
 *
 ******************************************************************************/
namespace NSImageio {
namespace NSIspio {
enum EPortIndex
{
    EPortIndex_TG1I,        // 0
    EPortIndex_TG2I,
    EPortIndex_CAMSV_TG1I,
    EPortIndex_CAMSV_TG2I,
    EPortIndex_LSCI,
    EPortIndex_CQI,         // 5
    EPortIndex_IMGO,
    EPortIndex_UFEO,
    EPortIndex_RRZO,
    EPortIndex_CAMSV_IMGO,
    EPortIndex_CAMSV2_IMGO, // 10
    EPortIndex_LCSO,
    EPortIndex_AAO,
    EPortIndex_AFO,
    EPortIndex_PDO,
    EPortIndex_EISO,        // 15
    EPortIndex_FLKO,
    EPortIndex_RSSO,
    EPortIndex_IMGI,
    EPortIndex_IMGBI,
    EPortIndex_IMGCI,       // 20
    EPortIndex_VIPI,
    EPortIndex_VIP2I,
    EPortIndex_VIP3I,
    EPortIndex_UFDI,
    EPortIndex_LCEI,        // 25
    EPortIndex_DMGI,
    EPortIndex_DEPI,
    EPortIndex_TDRI,
    EPortIndex_IMG2O,
    EPortIndex_IMG2BO,
    EPortIndex_IMG3O,
    EPortIndex_IMG3BO,
    EPortIndex_IMG3CO,
    EPortIndex_MFBO,
    EPortIndex_FEO,
    EPortIndex_WROTO,
    EPortIndex_WDMAO,
    EPortIndex_JPEGO,
    EPortIndex_VENC_STREAMO,
    EPortIndex_RAWI,
    EPortIndex_CAMSV_0_TGI,
    EPortIndex_CAMSV_1_TGI,
    EPortIndex_CAMSV_2_TGI,
    EPortIndex_CAMSV_3_TGI,
    EPortIndex_CAMSV_4_TGI,
    EPortIndex_CAMSV_5_TGI,
    EPortIndex_REGI,
    EPortIndex_UNKNOW       //always put at the bottom of this enum
    //
    //EPortIndex_DISPO,
    //EPortIndex_VIDO,
    //
};


/******************************************************************************
 *
 ******************************************************************************/
};  //namespace
};  //namespace
#endif  //_MTK_HARDWARE_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_PORT_INDEX_H_

