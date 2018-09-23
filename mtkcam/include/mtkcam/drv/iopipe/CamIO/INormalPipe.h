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

#ifndef _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_CAMIO_INORMALPIPE_H_
#define _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_CAMIO_INORMALPIPE_H_

//#define IOPIPE_SET_MODUL_REG(handle,RegName,Value) HWRWCTL_SET_MODUL_REG(handle,RegName,Value)
//#define IOPIPE_SET_MODUL_REGS(handle, StartRegName, size, ValueArray) HWRWCTL_SET_MODUL_REGS(handle, StartRegName, size, ValueArray)
#include "IHalCamIO.h"
#include <mtkcam/drv/def/ICam_type.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

#define NORMALPIPE_MAX_INST_CNT 3


typedef enum _Normalpipe_PIXMODE{
    _UNKNOWN_PIX_MODE  = 0x0,      // default use 2 pixmode constraint if unknown pix mode being asignned
    _1_PIX_MODE        = 0x1,      // 1 pix mode
    _2_PIX_MODE        = 0x2,      // 2 pix mode
    _4_PIX_MODE        = 0x4,      // 2 pix mode
    _MAX_PIX_MODE      = 0x5
}Normalpipe_PIXMODE;


struct NormalPipe_QueryIn {
    MUINT32             width;      //unit:pix
    Normalpipe_PIXMODE  pixMode;

    NormalPipe_QueryIn(
        MUINT32 _width = 0,
        Normalpipe_PIXMODE _pixMode = _UNKNOWN_PIX_MODE
        )
        :width(_width)
        ,pixMode(_pixMode)
    {}
};

struct NormalPipe_QueryInfo {
    MUINT32 x_pix;                  //horizontal resolution, unit:pix
    MUINT32 stride_pix;             //stride, uint:pix. this is a approximative value under pak mode
    MUINT32 stride_byte;            //stride, uint:byte
    MUINT32 xsize_byte;
    MUINT32 crop_x;                 //crop start point-x , unit:pix
    E_CAM_FORMAT query_fmt;         //query support fmt
    MUINT32 bs_ratio;               //bayer scaler scaling ratio, unit:%
    NormalPipe_QueryInfo (
                MUINT32 _x_pix = 0,
                MUINT32 _stride_pix = 0,
                MUINT32 _stride_byte = 0,
                MUINT32 _xsize_byte = 0,
                MUINT32 _crop_x = 0,
                E_CAM_FORMAT _query_fmt = CAM_UNKNOWN,
                MUINT32 _bs_ratio = 100
            )
        : x_pix(_x_pix)
        , stride_pix(_stride_pix)
        , stride_byte(_stride_byte)
        , xsize_byte(_xsize_byte)
        , crop_x(_crop_x)
        , query_fmt(_query_fmt)
        , bs_ratio(_bs_ratio)
    {}
};

typedef struct {
   MBOOL  mSupported;

} NormalPipe_EIS_Info;


typedef struct {
    MBOOL  mEnabled;
    MSize  size;

} NormalPipe_HBIN_Info;

/**
    enum for current frame status.
    mapping to kernel status
*/
typedef enum {
    _normal_status          = 0,
    _drop_frame_status      = 1,
    _last_working_frame     = 2,
    _1st_enqloop_status     = 3,
} NormalPipe_FRM_STATUS;


enum ENPipeCmd {
    ENPipeCmd_GET_TG_INDEX          = 0x0800,
    ENPipeCmd_GET_BURSTQNUM         = 0x0801,
    ENPipeCmd_SET_STT_SOF_CB        = 0x0802,
    ENPipeCmd_CLR_STT_SOF_CB        = 0x0803,
    ENPipeCmd_GET_LAST_ENQ_SOF      = 0x0804,
    ENPipeCmd_SET_MEM_CTRL          = 0x0805,
    ENPipeCmd_SET_IDLE_HOLD_CB      = 0x0806,
    ENPipeCmd_GET_STT_CUR_BUF       = 0x0812,

    ENPipeCmd_GET_TG_OUT_SIZE       = 0x110F,
    ENPipeCmd_GET_RMX_OUT_SIZE      = 0x1110,//
    ENPipeCmd_GET_HBIN_INFO         = 0x1111,//
    ENPipeCmd_GET_EIS_INFO          = 0x1112,
    ENPipeCmd_GET_UNI_INFO          = 0x1113,
    ENPipeCmd_GET_BIN_INFO          = 0x1114,
    ENPipeCmd_SET_EIS_CBFP          = 0x1117,
    ENPipeCmd_SET_LCS_CBFP          = 0x1118,
    ENPipeCmd_SET_SGG2_CBFP         = 0X1119,
    ENPipeCmd_GET_PMX_INFO          = 0X111B,
    ENPipeCmd_GET_CUR_FRM_STATUS    = 0x111D,
    ENPipeCmd_GET_CUR_SOF_IDX       = 0x111E,
    ENPIPECmd_AE_SMOOTH             = 0x1105,
    ENPipeCmd_HIGHSPEED_AE          = 0x1106,
    ENPipeCmd_SET_TG_INT_LINE       = 0x110E,
    //cmd for p1hwcfg, only isp3.0 support
    ENPipeCmd_SET_MODULE_EN         = 0x1401,
    ENPipeCmd_SET_MODULE_SEL        = 0x1402,
    ENPipeCmd_SET_MODULE_CFG        = 0x1403,
    ENPipeCmd_GET_MODULE_HANDLE     = 0x1404,
    ENPipeCmd_SET_MODULE_CFG_DONE   = 0x1405,
    ENPipeCmd_RELEASE_MODULE_HANDLE = 0x1406,
    ENPipeCmd_SET_MODULE_DBG_DUMP   = 0x1407,

    ENPipeCmd_MAX
};

enum ENPipeQueryCmd {
    ENPipeQueryCmd_X_PIX        = 0x00000001,
    ENPipeQueryCmd_X_BYTE       = 0x00000002,
    ENPipeQueryCmd_CROP_X_PIX   = 0x00000004,
    ENPipeQueryCmd_CROP_X_BYTE  = 0x00000008,
    ENPipeQueryCmd_CROP_START_X = 0x00000010,
    ENPipeQueryCmd_STRIDE_PIX   = 0x00000020,
    ENPipeQueryCmd_STRIDE_BYTE  = 0x00000040,
    ENPipeQueryCmd_QUERY_FMT    = 0x10000000,
    ENPipeQueryCmd_BS_RATIO     = 0x20000000,   //bayer scaler max scaling ratio,unit:%
};

/*****************************************************************************
*
* @class INormalPipe
* @brief CamIO Pipe Interface for Normal pipe in frame based mode.
* @details
* The data path will be Sensor --> ISP-- Mem.
*
******************************************************************************/

class INormalPipe : public IHalCamIO
{
public:     ////                    Instantiation.
    static  INormalPipe     *createInstance(MUINT32 sensorIndex,char const* szCallerName);
    void                    destroyInstance(char const* szCallerName);

public:     ////                    Attributes.
    /**
    * @brief Query pipe capability.
    * @param[in] portIdx:   Refer to PortMap.h, 'PortID::index' field
    * @param[in] eCmd:      width/stride pixel/byte crop constraint to query. Cmds are bitmap, plz refer to enum:ENPipeQueryCmd
    * @param[in] imgFmt:    EImageFormat in hardware/include/mtkcam/ImageFormat.h
    * @param[in] input:      input query information
    * @param[out] queryInfo: result
    * @details
    * @note
    * @return
    * - MTRUE indicates success; MFALSE indicates failure.
    */
    static MBOOL            query(MUINT32 portIdx, MUINT32 eCmd, EImageFormat imgFmt, NormalPipe_QueryIn input,
                                    NormalPipe_QueryInfo &queryInfo);

    /**
    * @brief get the pipe name
    *
    * @details
    *
    * @note
    *
    * @return
    * A null-terminated string indicating the name of the pipe.
    *
    */
    static  char const*             pipeName() { return "CamIO::NormalPipe"; }

    /**
    * @brief get the pipe name
    *
    * @details
    *
    * @note
    *
    * @return
    * A null-terminated string indicating the name of the pipe.
    *
    */
    virtual char const*             getPipeName() const { return pipeName(); }


    virtual MBOOL sendCommand(MINT32 cmd, MINTPTR arg1, MINTPTR arg2, MINTPTR arg3) = 0; //note: cmd use enum, in this layer for swo

};


/******************************************************************************
 *
 ******************************************************************************/
};  //namespace NSCamIO
};  //namespace NSIoPipe
};  //namespace NSCam
#endif  //_MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_POSTPROC_INORMALSTREAM_H_

