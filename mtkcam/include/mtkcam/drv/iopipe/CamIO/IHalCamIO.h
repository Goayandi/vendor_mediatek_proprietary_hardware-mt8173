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

#ifndef _MTK_HARDWARE_INCLUDE_MTKCAM_HAL_IHALCAMIO_H_
#define _MTK_HARDWARE_INCLUDE_MTKCAM_HAL_IHALCAMIO_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/drv/IHalSensor.h>
#include <vector>
#include <map>
#include "../Port.h"
#include "../PortMap.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

#define IOPIPE_MAX_SENSOR_CNT           (5)
//#define IOPIPE_MAX_ENABLE_SENSOR_CNT    (2)
#define IOPIPE_MAX_NUM_USERS            (16)
#define IOPIPE_MAX_USER_NAME_LEN        (32)

/******************************************************************************
 * @enum EPipeSignal
 * @brief Pipe Signal event
 ******************************************************************************/

typedef enum {
    EPIPE_Sensor_0 = 0,
    EPIPE_Sensor_1,
    EPIPE_Sensor_2,
    EPIPE_Sensor_3,
    EPIPE_Sensor_RSVD,
} EPIPE_IDX_E;

enum EPipeSignal {
    EPipeSignal_NONE    = 0x0000,           /*!< signal None */
    EPipeSignal_SOF     = 0x0001,           /*!< signal Start Of Frame */
    EPipeSignal_EOF     = 0x0002,           /*!< signal End of Frame */
    EPipeSignal_VSYNC   = 0x0003,           // vsync
    EPipeSignal_AFDONE  = 0x0004,           // AF done
    EPipeSignal_TG_INT  = 0x0005,           // TG interrupt
};

enum EPipeSignalClearType {
    EPipeSignal_ClearNone = 0x0,
    EPipeSignal_ClearWait = 0x1
};

typedef enum
{
   EPipe_P1Sel_SGG = 0, //0
   EPipe_P1Sel_SGG_EN,
   EPipe_P1Sel_LCS,
   EPipe_P1Sel_LCS_EN,
   EPipe_P1Sel_IMG,
   EPipe_P1Sel_UFE,     //5
   EPipe_P1Sel_EIS,
   EPipe_P1Sel_W2G,

   //raw-D
   EPipe_P1Sel_SGG_D = 0x100,
   EPipe_P1Sel_SGG_EN_D,
   EPipe_P1Sel_LCS_D,
   EPipe_P1Sel_LCS_EN_D,
   EPipe_P1Sel_IMG_D,
   EPipe_P1Sel_UFE_D,   //5
   EPipe_P1Sel_EIS_D,
   EPipe_P1Sel_W2G_D,

   EPipe_P1SEL_RSVD = 0xFFFFFFFF,

}EPipe_P1SEL;

enum EPipeRawFmt
{
    EPipe_PROCESSED_RAW = 0x0000,           /*!< Processed raw */
    EPipe_PURE_RAW      = 0x0001,           /*!< Pure raw */
};

typedef enum
{
	ESTT_CacheInvalidByRange = 0,
	ESTT_CacheInvalidAll,
	ESTT_CacheBypass,
} ESTT_CACHE_SYNC_POLICY;

//call back when enque request be dropped by drv
//for L-camera
typedef void (*fp_DropCB)(MUINT32 magic, void* cookie);
typedef void (*fp_EventCB)(void *para, void* cookie);

/******************************************************************************
 * @struct ResultMetadata
 *
 * @brief Pipe output metadata for describing given frame
 *
 * @param[out] mCropRect: Input CROP is applied BEFORE transforming and resizing.
 *                        This is based on the pipe's input port (TG)
 *
 * @param[out] mDstSize:  w/h of out image which's on dram
 *
 * @param[out] mTransform: ROTATION CLOCKWISE is applied AFTER FLIP_{H|V}.
 *
 * @param[out] mMagicNum_hal: magic number passed from middleware.
 *                            not sync if it is different from mMagicNum_tuning
 *
 * @param[out] mMagicNum_tuning: magic number passed from isp tuning
 *                               not sync if it is different from mMagicNum_hal
 *
 ******************************************************************************/
struct ResultMetadata {
    MRect                           mCrop_s;    //scaler crop
    MSize                           mDstSize;   //scaler scaledown size
    MRect                           mCrop_d;    //damo   crop
    MUINT32                         mTransform;
    MUINT32                         mMagicNum_hal;
    MUINT32                         mMagicNum_tuning;
    MUINT32                         mRawType;   //0 represents processed raw, 1 represents pure raw
    MINT64                          mTimeStamp;
    MVOID                           *mPrivateData; //set to internal static mem space
    MUINT32                         mPrivateDataSize;

                                    ResultMetadata(
                                        MRect     rCropRect = MRect(MPoint(0,0), MSize(0,0)),
                                        MUINT32   rTranform = 0,
                                        MUINT32   rNum1 = 0,
                                        MUINT32   rNum2 = 0,
                                        MUINT32   rRawType = 0,
                                        MUINT64   TimeStamp = 0,
                                        MVOID*    privateData = 0,
                                        MUINT32   privateDataSize = 0
                                    )
                                        : mCrop_s(rCropRect)
                                        , mCrop_d(rCropRect)
                                        , mTransform(rTranform)
                                        , mMagicNum_hal(rNum1)
                                        , mMagicNum_tuning(rNum2)
                                        , mRawType(rRawType)
                                        , mTimeStamp(TimeStamp)
                                        , mPrivateData(privateData)
                                        , mPrivateDataSize(privateDataSize)
                                    {
                                        mDstSize = MSize(0,0);
                                    }
};

/******************************************************************************
 * @struct SignalTimeInfo
 *
 * @brief The time info after signal wait done
 *
 * @param[out] :
 *
 ******************************************************************************/
struct SignalTimeInfo {
    MUINT32    tLastSig_sec;            /* time stamp of the latest occuring signal*/
    MUINT32    tLastSig_usec;           /* time stamp of the latest occuring signal*/
    MUINT32    tMark2WaitSig_sec;       /* time period from marking a signal to user try to wait and get the signal*/
    MUINT32    tMark2WaitSig_usec;      /* time period from marking a signal to user try to wait and get the signal*/
    MUINT32    tLastSig2GetSig_sec;     /* time period from latest occuring signal to user try to wait and get the signal*/
    MUINT32    tLastSig2GetSig_usec;    /* time period from latest occuring signal to user try to wait and get the signal*/
    MUINT32    passedbySigcnt;          /* the count for the signal passed by  */
};

/******************************************************************************
 * @struct IspP1RegIo
 *
 * @brief The time info after signal wait done
 *
 * @param[out] :
 *
 ******************************************************************************/
typedef struct {
    MUINT32     Addr;
    MUINT32     Data;
} IspP1RegIo;

/******************************************************************************
 * @struct BufInfo
 *
 * @brief Pipe's output port parameter
 * @param[in/out] mPortID: (Nrm/Stt)The output port ID of the pipe.
 * @param[in/out] mBuffer: (Nrm)A pointer to an image buffer.
 * @param[out] mMetaData: (Nrm/Stt)result for
 *              mMagicNum_hal: (Nrm/Stt)
 *              mTimeStamp: (Nrm/Stt)
 *              (others: (Nrm))
 * @param[in] FrameBased: (Nrm)IO/Magic# for current image. supported only in FrameBased mode(camera 3.0)
 * @param[int/out] mSize: (Nrm/Stt)
 * @param[int/out] mVa: (Nrm/Stt)
 * @param[int/out] mPa: (Nrm/Stt)
 *
 ******************************************************************************/
struct BufInfo
{
    PortID                          mPortID;                //deque: portID.                enque: PortID
    IImageBuffer*                   mBuffer;                //deque: input buf infor.       enque: output buf infor
    ResultMetadata                  mMetaData;              //deuqe: deque result.          enque: bypass
    MINT32                          mBufIdx;                //deque: bypass.                enque: bypass
    MUINT32                         mRawOutFmt;             //deque: bypass.                enque: pure_raw, processed_raw
    struct {
        MUINT32                         mMagicNum_tuning;
        MSize                           mDstSize;           //w/h of out image which's on dram
        MRect                           mCropRect;          //crop image in TG coordinate axis(before scaler)
        MUINT32                         mSOFidx;            //deque: bypass.           enque: sof idx for camera 3.0
    } FrameBased;
    MUINT32                         mSize;
    MUINTPTR                        mVa;
    MUINTPTR                        mPa;
    MUINTPTR                        mPa_offset;
    MUINT32                         mStride;                //unit:byte.

                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MUINT32         size,
                                        MUINTPTR        va,
                                        MUINTPTR        pa
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mSize(size)
                                        , mVa(va)
                                        , mPa(pa)
                                    {
                                        mBufIdx = 0xFFFF;
                                        mRawOutFmt = EPipe_PROCESSED_RAW;
                                        FrameBased.mMagicNum_tuning= 0x40000000;
                                        FrameBased.mDstSize = MSize(0,0);
                                        FrameBased.mCropRect = MRect(MPoint(0,0),MSize(0,0));
                                        FrameBased.mSOFidx = 0xFFFF;
                                        mStride = 0;
                                    }


                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MUINT32         size,
                                        MUINTPTR        va,
                                        MUINTPTR        pa,
                                        ResultMetadata& metaData
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mMetaData(metaData)
                                        , mSize(size)
                                        , mVa(va)
                                        , mPa(pa)
                                    {
                                        mBufIdx = 0xFFFF;
                                        mRawOutFmt = EPipe_PROCESSED_RAW;
                                        FrameBased.mMagicNum_tuning= 0x40000000;
                                        FrameBased.mDstSize = MSize(0,0);
                                        FrameBased.mCropRect = MRect(MPoint(0,0),MSize(0,0));
                                        FrameBased.mSOFidx = 0xFFFF;
                                        mStride = 0;
                                    }

                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MUINT32         size,
                                        MUINTPTR        va,
                                        MUINTPTR        pa,
                                        ResultMetadata& metaData,
                                        MINT32          BufIdx
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mMetaData(metaData)
                                        , mBufIdx(BufIdx)
                                        , mSize(size)
                                        , mVa(va)
                                        , mPa(pa)
                                    {
                                        mRawOutFmt = EPipe_PROCESSED_RAW;
                                        FrameBased.mMagicNum_tuning= 0x40000000;
                                        FrameBased.mDstSize = MSize(0,0);
                                        FrameBased.mCropRect = MRect(MPoint(0,0),MSize(0,0));
                                        FrameBased.mSOFidx = 0xFFFF;
                                        mStride = 0;
                                    }

                                    //ALPS_camera 1.0
                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MINT32          idx = 0xFFFF
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mBufIdx(idx)
                                    {
                                        mStride = 0;
                                    }

                                    //ALPS_camera 3.0
                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MSize           outsize,
                                        MRect           crop,
                                        MUINT32         magic,
                                        MINT32          idx = 0xFFFF
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mBufIdx(idx)
                                    {
                                        mRawOutFmt = EPipe_PROCESSED_RAW;
                                        FrameBased.mDstSize = outsize;
                                        FrameBased.mCropRect = crop;
                                        FrameBased.mMagicNum_tuning = magic;
                                        FrameBased.mSOFidx = 0xFFFF;
                                        mVa = 0;
                                        mPa = 0;
                                        mPa_offset = 0;
                                        mSize = 0;
                                        mStride = 0;
                                    }
                                    //ALPS_camera 3.0, iHDR
                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MSize           outsize,
                                        MRect           crop,
                                        MUINT32         magic,
                                        MUINT32         sofidx,
                                        MINT32          idx = 0xFFFF
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mBufIdx(idx)
                                    {
                                            mRawOutFmt = EPipe_PROCESSED_RAW;
                                            FrameBased.mDstSize = outsize;
                                            FrameBased.mCropRect = crop;
                                            FrameBased.mMagicNum_tuning = magic;
                                            FrameBased.mSOFidx = sofidx;
                                            mVa = 0;
                                            mPa = 0;
                                            mPa_offset = 0;
                                            mSize = 0;
                                            mStride = 0;
                                    }
                                    //ALPS_camera 3.0, iHDR, RawOurFormat
                                    BufInfo(
                                        PortID const&   rPortID,
                                        IImageBuffer*   buffer,
                                        MSize           outsize,
                                        MRect           crop,
                                        MUINT32         magic,
                                        MUINT32         sofidx,
                                        MUINT32         rawFmt,
                                        MINT32          idx = 0xFFFF
                                    )
                                        : mPortID(rPortID)
                                        , mBuffer(buffer)
                                        , mBufIdx(idx)
                                    {
                                            mRawOutFmt = rawFmt;
                                            FrameBased.mDstSize = outsize;
                                            FrameBased.mCropRect = crop;
                                            FrameBased.mMagicNum_tuning = magic;
                                            FrameBased.mSOFidx = sofidx;
                                            mVa = 0;
                                            mPa = 0;
                                            mPa_offset = 0;
                                            mSize = 0;
                                            mStride = 0;
                                    }
                                    BufInfo(){
                                        mRawOutFmt = EPipe_PROCESSED_RAW;
                                        FrameBased.mMagicNum_tuning= 0x40000000;
                                        FrameBased.mDstSize = MSize(0,0);
                                        FrameBased.mCropRect = MRect(MPoint(0,0),MSize(0,0));
                                        FrameBased.mSOFidx = 0xFFFF;
                                        mVa = 0;
                                        mPa = 0;
                                        mPa_offset = 0;
                                        mBuffer = NULL;
                                        mBufIdx = 0;
                                        mSize = 0;
                                        mStride = 0;
                                    }
};


/******************************************************************************
 * @struct QBufInfo
 *
 * @brief vector of Pipe output's ports
 *
 * @param[in/out] mvOut: indicate vector of pipe output's ports
 *
 ******************************************************************************/
struct QBufInfo
{
    MVOID*                          mpCookie;
    MUINT64                         mShutterTimeNs;
    std::vector<BufInfo>            mvOut;
                                    QBufInfo()
                                        : mpCookie(NULL)
                                        , mShutterTimeNs(0)
                                        , mvOut()
                                    {
                                    }
};

struct portInfo {
    PortID                          mPortID;
    EImageFormat                    mFmt;
    MSize                           mDstSize;
    MRect                           mCropRect;
    int                             mStride[3];
    MBOOL                           mPureRaw;
    MBOOL                           mPureRawPak;
                                portInfo(PortID _mPortID,
                                          EImageFormat   _mFmt,
                                          MSize  _mDstSize,
                                          MRect  _mCropRect,
                                          int    _mStride0,
                                          int    _mStride1,
                                          int    _mStride2,
                                          MBOOL  _mPureRaw = 0,
                                          MBOOL  _mPureRawPak = 0)
                                    : mPortID(_mPortID)
                                    , mFmt(_mFmt)
                                    , mDstSize(_mDstSize)
                                    , mCropRect(_mCropRect)
                                    , mPureRaw(_mPureRaw)
                                    , mPureRawPak(_mPureRawPak)
                                {
                                    mStride[0]=_mStride0;
                                    mStride[1]=_mStride1;
                                    mStride[2]=_mStride2;
                                }
};

struct statPortInfo {
    PortID                      mPortID;
    MUINT32                     mWidth;
    MUINT32                     mHeight;
                            statPortInfo(PortID _mPortID)
                                : mPortID(_mPortID)
                            {
                                mWidth = mHeight = 0;
                            }

                            statPortInfo(PortID _mPortID,MUINT32 _W, MUINT32 _H)
                                : mPortID(_mPortID)
                                , mWidth(_W)
                                , mHeight(_H)
                            {
                            }
};


/******************************************************************************
 * @struct QPortID
 *
 * @brief Config list of dma ports
 *
 ******************************************************************************/
struct QPortID {
    std::vector<PortID>     mvPortId;
};

/******************************************************************************
 * @enum E_Pattern
 *
 * @input data pattern
 *
 ******************************************************************************/
typedef enum{
    EPipe_Normal    = 0,
    EPipe_Dual_pix  = 1,    //dual pd's pattern
}E_Pattern;

/******************************************************************************
 * @enum E_Pattern
 *
 * @input data pattern
 *
 ******************************************************************************/
typedef enum {
    E_UNI_LINK_AUTO = 0,
    E_UNI_LINK_ON,
    E_UNI_LINK_OFF
} E_UNI_LINK_SEL;

/******************************************************************************
 * @struct QFunc
 *
 * @config cam's extra function
 *
 ******************************************************************************/
typedef union
{
        struct
        {
                MUINT32 DATA_PATTERN     :  3;       //plz reference to E_Pattern
                MUINT32 OFF_BIN          :  1;       //force to off frontal binning
                MUINT32 rsv              : 28;
        } Bits;
        MUINT32 Raw;
}QFunc;

/******************************************************************************
 * @struct QInitParam
 *
 * @brief frame setting only in initialization
 *
 * @param[in] mPureRaw: 1: pure raw; 0: pre-process raw
 *
 * @param[in] mBitdepth: desired bitdepth. It can be different from tg input
 *
 * @note: mDstSize can <= CropRect.size if and only if this port have a build-in resizer.
 *        Otherwise, mDstSize == CropRect.size
 ******************************************************************************/

struct QInitParam
{
    MUINT32                             mRawType;   //ctrl sensor output test pattern or not
    MUINT32                             mBitdepth;
    std::vector<IHalSensor::ConfigParam> mSensorCfg;
    std::vector<portInfo>               mPortInfo;
    fp_DropCB                           m_DropCB;   //call back for drop enque request
    void*                               m_returnCookie;
    MBOOL                               m_DynamicRawType;//1:dynamically switch processed/pure raw
    MBOOL                               m_bOffBin;      // 1. : force to off frontal binning
    MBOOL                               m_bN3D;
    QFunc                               m_Func;
    E_UNI_LINK_SEL                      m_UniLinkSel;
                                    //
                                    QInitParam(
                                               MUINT32 rRawType,
                                               MUINT32 const& rBitdepth,
                                               std::vector<IHalSensor::ConfigParam> const & rSensorCfg,
                                               std::vector<portInfo> const & rPortInfo
                                    )
                                        : mRawType(rRawType)
                                        , mBitdepth(rBitdepth)
                                        , mSensorCfg(rSensorCfg)
                                        , mPortInfo(rPortInfo)
                                    {
                                        m_DropCB = NULL;
                                        m_returnCookie = NULL;
                                        m_DynamicRawType = MTRUE;
                                        m_bOffBin = MFALSE;
                                        m_bN3D = MFALSE;
                                        m_Func.Raw = 0;
                                        m_UniLinkSel = E_UNI_LINK_AUTO;
                                    }
                                    //
                                    QInitParam(
                                               MUINT32 rRawType,
                                               MUINT32 const& rBitdepth,
                                               std::vector<IHalSensor::ConfigParam> const & rSensorCfg,
                                               std::vector<portInfo> const & rPortInfo,
                                               MBOOL dynamicSwt
                                    )
                                        : mRawType(rRawType)
                                        , mBitdepth(rBitdepth)
                                        , mSensorCfg(rSensorCfg)
                                        , mPortInfo(rPortInfo)
                                        , m_DynamicRawType(dynamicSwt)
                                    {
                                        m_DropCB = NULL;
                                        m_returnCookie = NULL;
                                        m_bOffBin = MFALSE;
                                        m_bN3D = MFALSE;
                                        m_Func.Raw = 0;
                                        m_UniLinkSel = E_UNI_LINK_AUTO;
                                    }
                                    QInitParam(
                                               MUINT32 rRawType,
                                               MUINT32 const& rBitdepth,
                                               std::vector<IHalSensor::ConfigParam> const & rSensorCfg,
                                               std::vector<portInfo> const & rPortInfo,
                                               MBOOL dynamicSwt,
                                               MBOOL bN3d
                                    )
                                        : mRawType(rRawType)
                                        , mBitdepth(rBitdepth)
                                        , mSensorCfg(rSensorCfg)
                                        , mPortInfo(rPortInfo)
                                        , m_DynamicRawType(dynamicSwt)
                                        , m_bN3D(bN3d)
                                    {
                                        m_DropCB = NULL;
                                        m_returnCookie = NULL;
                                        m_bOffBin = MFALSE;
                                        m_Func.Raw = 0;
                                        m_UniLinkSel = E_UNI_LINK_AUTO;
                                    }
};

/******************************************************************************
 * @struct QInitStatParam
 *
 * @brief 3A statistic dmao port setting only in initialization
 *
 * @note:
 ******************************************************************************/
struct QInitStatParam
{
    std::vector<statPortInfo>       mStatPortInfo;
                                //
                                QInitStatParam(
                                        std::vector<statPortInfo> const & rStatPortInfo
                                )
                                    : mStatPortInfo(rStatPortInfo)
                                {
                                }
};

/******************************************************************************
 *
 ******************************************************************************/
//class ISignalConsumer
//{
//private:
//    const MINT32    mUserKey;
//    const char*     mName;
//    //
//public:
//    explicit ISignalConsumer(int _mUserKey, const char* _name)
//        : mUserKey(_mUserKey),
//          mName(_name)
//        {}
//    int                 getUserKey()            const   { return mUserKey; }
//    const char*         getName()               const   { return mName; }
//};


/******************************************************************************
 * @class IHalCamIO
 *
 * @brief CamIO base interface
 *
 * @details TG --> CamIO --> MEM
 ******************************************************************************/
class IHalCamIO
{
protected:
    virtual        ~IHalCamIO(){};

public:
    virtual MVOID   destroyInstance(char const* szCallerName) = 0;

    virtual MBOOL   start() = 0;

    virtual MBOOL   stop(MBOOL bNonblocking = MFALSE)  = 0;

    virtual MBOOL   abort() = 0;

    virtual MBOOL   init() = 0;

    virtual MBOOL   uninit()  = 0;

    virtual MBOOL   enque(QBufInfo const& rQBuf)   = 0;

    virtual MBOOL   deque(QPortID& rQPort, QBufInfo& rQBuf, MUINT32 u4TimeoutMs = 0xFFFFFFFF) = 0;

    virtual MBOOL   reset() = 0;

    //one-time conifg
    virtual MBOOL   configPipe(QInitParam const& vInPorts, MINT32 burstQnum = 1) = 0;

    virtual MBOOL   sendCommand(MINT32 cmd, MINTPTR arg1, MINTPTR arg2, MINTPTR arg3) = 0;

public:
    virtual MINT32  attach(const char* UserName) = 0;

    virtual MBOOL   wait(EPipeSignal eSignal, EPipeSignalClearType eClear, const MINT32 mUserKey,
                            MUINT32 TimeoutMs = 0xFFFFFFFF, SignalTimeInfo *pTime = NULL) = 0;

    virtual MBOOL   signal(EPipeSignal eType, const MINT32 mUserKey) = 0;
    virtual MBOOL   abortDma(PortID port, char const* szCallerName) = 0;

    virtual MUINT32 getIspReg(MUINT32 RegAddr, MUINT32 RegCount, MUINT32 RegData[], MBOOL bPhysical = MTRUE) = 0;
    virtual MUINT32 getIspReg(IspP1RegIo *pRegs, MUINT32 RegCount, MBOOL bPhysical = MTRUE) = 0;
    virtual MUINT32 getIspUniReg(IspP1RegIo *pRegs, MUINT32 RegCount, MBOOL bPhysical = MTRUE) = 0;

};

/******************************************************************************
 * @class IHalCamStatIO
 *
 * @brief CamIO base interface
 *
 * @details TG --> CamIO --> MEM
 ******************************************************************************/
class IHalCamStatIO
{
protected:
    virtual        ~IHalCamStatIO(){};

public:
    virtual MVOID   destroyInstance(char const* szCallerName) = 0;

    virtual MBOOL   start() = 0;

    virtual MBOOL   stop()  = 0;

    virtual MBOOL   init() = 0;

    virtual MBOOL   uninit()  = 0;

    virtual MBOOL   enque(QBufInfo const& rQBuf)   = 0;

    virtual MBOOL   deque(PortID port, QBufInfo& rQBuf, ESTT_CACHE_SYNC_POLICY cacheSyncPolicy = ESTT_CacheInvalidByRange, MUINT32 u4TimeoutMs = 0xFFFFFFFF) = 0;

    virtual MBOOL   reset() = 0;

    //one-time conifg
    virtual MBOOL   configPipe(QInitStatParam const& vInPorts, MINT32 burstQnum = 1) = 0;

    virtual MBOOL   sendCommand(MINT32 cmd, MINTPTR arg1, MINTPTR arg2, MINTPTR arg3) = 0;

public:
    virtual MINT32  attach(const char* UserName) = 0;

    virtual MBOOL   wait(EPipeSignal eSignal, EPipeSignalClearType eClear, const MINT32 mUserKey,
                            MUINT32 TimeoutMs = 0xFFFFFFFF, SignalTimeInfo *pTime = NULL) = 0;

    virtual MBOOL   signal(EPipeSignal eSignal, const MINT32 mUserKey) = 0;
    virtual MBOOL   abortDma(PortID port, char const* szCallerName) = 0;

    virtual MUINT32 getIspReg(MUINT32 RegAddr, MUINT32 RegCount, MUINT32 RegData[], MBOOL bPhysical = MTRUE) = 0;
    virtual MUINT32 getIspReg(IspP1RegIo *pRegs, MUINT32 RegCount, MBOOL bPhysical = MTRUE) = 0;
    virtual MUINT32 getIspUniReg(IspP1RegIo *pRegs, MUINT32 RegCount, MBOOL bPhysical = MTRUE) = 0;

};

};
};
};

#endif
