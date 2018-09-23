/********************************************************************************************
 *     LEGAL DISCLAIMER
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION,
TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/
/*
** $Log: fd_hal_base.h $
 *
*/

#ifndef _FDVT_HAL_H_
#define _FDVT_HAL_H_

#include <mtkcam/feature/FaceDetection/fd_hal_base.h>
#include <pthread.h>
#include <semaphore.h>
#include <utils/Mutex.h>
#include "MTKDetection.h"

#define FD_SCALES 14

class MTKDetection;
/*******************************************************************************
*
********************************************************************************/
class halFDVT: public halFDBase
{
public:
    //
    static halFDBase* getInstance();
    virtual void destroyInstance();
    //
    /////////////////////////////////////////////////////////////////////////
    //
    // halFDBase () -
    //! \brief FD Hal constructor
    //
    /////////////////////////////////////////////////////////////////////////
    halFDVT();

    /////////////////////////////////////////////////////////////////////////
    //
    // ~mhalCamBase () -
    //! \brief mhal cam base descontrustor
    //
    /////////////////////////////////////////////////////////////////////////
    virtual ~halFDVT();

    /////////////////////////////////////////////////////////////////////////
    //
    // mHalFDInit () -
    //! \brief init face detection
    //
    /////////////////////////////////////////////////////////////////////////
    //virtual MINT32 halFDInit(MUINT32 fdW, MUINT32 fdH, MUINT32 WorkingBuffer, MUINT32 WorkingBufferSize, MBOOL   SWResizerEnable);
    virtual MINT32 halFDInit(MUINT32 fdW, MUINT32 fdH, MUINT8 *WorkingBuffer, MUINT32 WorkingBufferSize, MBOOL SWResizerEnable, MUINT8 Current_mode);

    /////////////////////////////////////////////////////////////////////////
    //
    // halFDGetVersion () -
    //! \brief get FD version
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFDGetVersion();

    /////////////////////////////////////////////////////////////////////////
    //
    // mHalFDVTDo () -
    //! \brief process face detection
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFDDo(struct FD_Frame_Parameters &Param);

    /////////////////////////////////////////////////////////////////////////
    //
    // mHalFDVTUninit () -
    //! \brief FDVT uninit
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFDUninit();

    /////////////////////////////////////////////////////////////////////////
    //
    // mHalFDVTGetFaceInfo () -
    //! \brief get face detection result
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFDGetFaceInfo(MtkCameraFaceMetadata *fd_info_result);

    /////////////////////////////////////////////////////////////////////////
    //
    // mHalFDVTGetFaceResult () -
    //! \brief get face detection result
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFDGetFaceResult(MtkCameraFaceMetadata * fd_result, MINT32 ResultMode = 1);

    /////////////////////////////////////////////////////////////////////////
    //
    // mHalSDGetSmileResult () -
    //! \brief get smile detection result
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halSDGetSmileResult( ) ;

    /////////////////////////////////////////////////////////////////////////
    //
    // halFDYUYV2ExtractY () -
    //! \brief create Y Channel
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFDYUYV2ExtractY(MUINT8 *dstAddr, MUINT8 *srcAddr, MUINT32 src_width, MUINT32 src_height);

    /////////////////////////////////////////////////////////////////////////
    //
    // halFTBufferCreate () -
    //! \brief create face tracking buffer
    //
    /////////////////////////////////////////////////////////////////////////
    virtual MINT32 halFTBufferCreate(MUINT8 *dstAddr, MUINT8 *srcAddr, MUINT8  ucPlane, MUINT32 src_width, MUINT32 src_height);

    virtual MINT32 halGetFTBufferSize(MUINT32 *width, MUINT32 *height);

    virtual MINT32 halFTBufferCreateAsync(MUINT8 *dstAddr, MUINT8 *srcAddr, MUINT8  ucPlane, MUINT32 src_width, MUINT32 src_height);

private:
    static void* onFTThreadLoop(void*);
    static void LockFTBuffer(void* arg);
    static void UnlockFTBuffer(void* arg);

    bool doRGB565BufferP1_SW(MUINT8 *a_dstAddr, MUINT8 *a_srcAddr, MUINT32 SrcWidth, MUINT32 SrcHeight);
    bool doRGB565BufferQVGAP1_SW(MUINT8 *a_dstAddr, MUINT8 *a_srcAddr, MUINT32 SrcWidth1, MUINT32 SrcHeight1);
    bool doRGB565Buffer_DDP(MUINT8 *a_dstAddr, MUINT8 *a_srcAddr, MUINT32 planes);

    void dumpFDImages(MUINT8* pFDImgBuffer, MUINT8* pFTImgBuffer);
    void dumpFDParam(MTKFDFTInitInfo &FDFTInitInfo);

protected:

    MTKDetection* mpMTKFDVTObj;

    MUINT32 mFDW;
    MUINT32 mFDH;
    MUINT32 mBuffCount;
    MBOOL   mInited;

    pthread_t   mFTThread;
    sem_t       mSemFTExec;
    sem_t       mSemFTLock;
    MINT32      mFTStop;
    MINT32      mFTBufReady;
    struct FTParam {
        MUINT8 *dstAddr;
        MUINT8 *srcAddr;
        MUINT8 ucPlane;
        MUINT32 src_width;
        MUINT32 src_height;
    };
    struct FTParam mFTParameter;

    MUINT32 mimage_width_array[FD_SCALES];
    MUINT32 mimage_height_array[FD_SCALES];
    MUINT32 mImageScaleTotalSize;
    MUINT8  *mpImageScaleBuffer;
    MBOOL   mEnableSWResizerFlag;
    MUINT8  *mpImageVGABuffer;
    MINT32  mFdResult_Num;
    FD_RESULT mFdResult[15];
    MUINT32 mFTWidth;
    MUINT32 mFTHeight;
    MBOOL   mCurrentMode;
    MUINT32 mDoDumpImage;
    float   mFDFilterRatio;

    MUINT32 mUseUserScale;
    MUINT32 mUserScaleNum;
    MUINT32 mFrameCount;
    MUINT32 mDetectedFaceNum;
    FD_RESULT mFirstFaceResult;
};

#endif

