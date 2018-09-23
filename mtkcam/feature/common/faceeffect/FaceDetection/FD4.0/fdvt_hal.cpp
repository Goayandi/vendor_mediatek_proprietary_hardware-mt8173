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
** $Log: fdvt_hal.cpp $
 *
*/
#define LOG_TAG "mHalFDVT"

#define FDVT_DDP_SUPPORT

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>
//#include "MediaHal.h"
//#include <mhal/inc/camera/faces.h>
#include <faces.h>
//#include "MediaLog.h"
//#include "MediaAssert.h"
#include "fdvt_hal.h"
#include <mtkcam/utils/std/Log.h>

#include "camera_custom_fd.h"

#ifdef FDVT_DDP_SUPPORT
#include <DpBlitStream.h>
#endif

#include <mtkcam/def/PriorityDefs.h>
#include <sys/prctl.h>
using namespace android;

#define DUMP_IMAGE (0)


//****************************//
//-------------------------------------------//
//  Global face detection related parameter  //
//-------------------------------------------//

#define USE_SW_FD_TO_DEBUG (0)
#if (MTKCAM_FDFT_USE_HW == '1') && (USE_SW_FD_TO_DEBUG == 0)
#define USE_HW_FD (1)
#else
#define USE_HW_FD (0)
#endif

#if (MTKCAM_FDFT_SUB_VERSION == '1')
#define HW_FD_SUBVERSION (1)
#else
#define HW_FD_SUBVERSION (0)
#endif

#define SINGLE_FACE_STABLE_ENABLE (1)

#define MHAL_NO_ERROR 0
#define MHAL_INPUT_SIZE_ERROR 1
#define MHAL_UNINIT_ERROR 2
#define MHAL_REINIT_ERROR 3

#define MAX_FACE_NUM 15

#define MHAL_FDVT_FTBUF_W (320)
#define MHAL_FDVT_FTBUF_H (240)

// v1 is for SD/FB default mode, v2 is for 320x240 manuel mode, v3 is for 400x300 manuel mode
static MUINT32 image_width_array_v1[FD_SCALES]  = {320, 256, 204, 160, 128, 102, 80, 64, 50, 40, 34, 0, 0, 0};
static MUINT32 image_height_array_v1[FD_SCALES] = {240, 192, 152, 120, 96, 76, 60, 48, 38, 30, 25, 0, 0, 0};
static MUINT32 image_width_array_v2[FD_SCALES]  = {320, 262, 210, 168, 134, 108, 86, 70, 56, 46, 38, 0, 0, 0};
static MUINT32 image_height_array_v2[FD_SCALES] = {240, 196, 157, 125, 100,  80, 64, 52, 41, 33, 27, 0, 0, 0};
static MUINT32 image_width_array_v3[FD_SCALES]  = {400, 328, 262, 210, 168, 134, 108, 86, 70, 56, 46, 38, 0, 0};
static MUINT32 image_height_array_v3[FD_SCALES] = {300, 245, 196, 157, 125, 100,  80, 64, 52, 41, 33, 27, 0, 0};

static const MUINT32 gimage_input_width_vga = 640;
static const MUINT32 gimage_input_height_vga = 480;
static const MUINT32 gimage_input_height_buffer = 640;
static void Create640WidthImage_Y(MUINT8* src_img, MUINT32 src_w, MUINT32 src_h, MUINT8* dst_img, MBOOL input_4_3, MBOOL input_16_9);
static void CreateScaleImages_Y(MUINT8* image_buffer_vga, MUINT8* image_buffer,MUINT32 w, MUINT32 h, MUINT32* ImageWidthArray, MUINT32* ImageHeightArray, MUINT32 sacles);

static Mutex       gLock;
static Mutex       gInitLock;
//****************************//


/******************************************************************************
*
*******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)


/*******************************************************************************
* Utility
********************************************************************************/
typedef struct
{
    MUINT8 *srcAddr;
    MUINT32 srcWidth;
    MUINT32 srcHeight;
    MUINT8 *dstAddr;
    MUINT32 dstWidth;
    MUINT32 dstHeight;
} PIPE_BILINEAR_Y_RESIZER_STRUCT, *P_PIPE_BILINEAR_Y_RESIZER_STRUCT;

#define PIPE_IUL_I_TO_X(i)              ((i) << 16)                       ///< Convert from integer to S15.16 fixed-point
#define PIPE_IUL_X_TO_I(x)              (((x) + (1 << 15)) >> 16)    ///< Convert from S15.16 fixed-point to integer (round)
#define PIPE_IUL_X_TO_I_CHOP(x)     ((x) >> 16)                       ///< Convert from S15.16 fixed-point to integer (chop)
#define PIPE_IUL_X_TO_I_CARRY(x)        (((x) + 0x0000FFFF) >> 16) ///< Convert from S15.16 fixed-point to integer (carry)
#define PIPE_IUL_X_FRACTION(x)      ((x) & 0x0000FFFF)

#define PIPE_LINEAR_INTERPOLATION(val1, val2, weighting2)   \
   PIPE_IUL_X_TO_I((val1) * (PIPE_IUL_I_TO_X(1) - (weighting2)) + (val2) * (weighting2))

static void PipeBilinearResizer(P_PIPE_BILINEAR_Y_RESIZER_STRUCT pUtlRisizerInfo)
{
    if(pUtlRisizerInfo->srcWidth == 0 || pUtlRisizerInfo->srcHeight==0)
        return;
    if(pUtlRisizerInfo->dstWidth == 0 || pUtlRisizerInfo->dstHeight==0)
        return;

    const MUINT32 srcPitch = pUtlRisizerInfo->srcWidth;
    const MUINT32 srcStepX = PIPE_IUL_I_TO_X(pUtlRisizerInfo->srcWidth) /pUtlRisizerInfo->dstWidth;
    const MUINT32 srcStepY = PIPE_IUL_I_TO_X(pUtlRisizerInfo->srcHeight) /pUtlRisizerInfo->dstHeight;
    const MUINT32 img_w = pUtlRisizerInfo->dstWidth;

    MUINT8 *const src_buffer = pUtlRisizerInfo->srcAddr;
    MUINT8 *dstAddr= pUtlRisizerInfo->dstAddr;
    MUINT32 srcCoordY = 0;
    MINT32 h = pUtlRisizerInfo->dstHeight;

    while (--h >= 0)
    {
        MINT32 w = img_w;
        MUINT32 srcCoordX = 0;

        MINT32 srcOffset_1;
        MINT32 srcOffset_2;
        MUINT8 *src_ptr_1;
        MUINT8 *src_ptr_2;

        MINT32 y_carry = PIPE_IUL_X_TO_I_CARRY(srcCoordY);
        MINT32 y_chop  = PIPE_IUL_X_TO_I_CHOP(srcCoordY);

        if(y_carry < 0 || y_carry >= (MINT32)pUtlRisizerInfo->srcHeight)
            return;
        if(y_chop < 0 || y_chop >= (MINT32)pUtlRisizerInfo->srcHeight)
            return;


        srcOffset_1 = y_chop * srcPitch;
        srcOffset_2 = y_carry * srcPitch;
        src_ptr_1 = src_buffer + srcOffset_1;
        src_ptr_2 = src_buffer + srcOffset_2;

        while (--w >= 0)
        {
            MUINT8 pixel_1, pixel_2;
            MINT32 y, y1;

            MINT32 x_carry = PIPE_IUL_X_TO_I_CARRY(srcCoordX);
            MINT32 x_chop = PIPE_IUL_X_TO_I_CHOP(srcCoordX);

            MINT32 weighting2;

            weighting2 = PIPE_IUL_X_FRACTION(srcCoordX);

            /// 1st horizontal interpolation.
            pixel_1 = *(src_ptr_1 + x_chop);
            pixel_2 = *(src_ptr_1 + x_carry);
            y = PIPE_LINEAR_INTERPOLATION(pixel_1, pixel_2, weighting2);

            /// 2nd horizontal interpolation.
            pixel_1 = *(src_ptr_2 + x_chop);
            pixel_2 = *(src_ptr_2 + x_carry);
            y1 = PIPE_LINEAR_INTERPOLATION(pixel_1, pixel_2, weighting2);

            /// Vertical interpolation.
            weighting2 = PIPE_IUL_X_FRACTION(srcCoordY);

            y = PIPE_LINEAR_INTERPOLATION(y, y1, weighting2);

            *dstAddr++ = (MUINT8)y;

            srcCoordX += srcStepX;
        }
        srcCoordY += srcStepY;
    }
}

static void Create640WidthImage_Y(MUINT8* src_img, MUINT32 src_w, MUINT32 src_h, MUINT8* dst_img)
{
    PIPE_BILINEAR_Y_RESIZER_STRUCT UtlRisizerInfo;
    UtlRisizerInfo.srcAddr = src_img;
    UtlRisizerInfo.srcWidth= src_w;
    UtlRisizerInfo.srcHeight= src_h;
    UtlRisizerInfo.dstAddr = dst_img;
    UtlRisizerInfo.dstWidth = gimage_input_width_vga;
    UtlRisizerInfo.dstHeight = gimage_input_width_vga*src_h/src_w;

    PipeBilinearResizer(&UtlRisizerInfo);
}

static void CreateScaleImages_Y(MUINT8* image_buffer_vga, MUINT8* image_buffer,MUINT32 w, MUINT32 h, MUINT32* ImageWidthArray, MUINT32* ImageHeightArray, MUINT32 sacles)
{
    MINT32 current_scale;
    PIPE_BILINEAR_Y_RESIZER_STRUCT UtlRisizerInfo;
    MUINT8* dst_ptr;
    dst_ptr = image_buffer;

    for ( current_scale = 0 ; current_scale < sacles ; current_scale ++ )
    {
        UtlRisizerInfo.srcAddr = image_buffer_vga;
        UtlRisizerInfo.srcWidth= w;
        UtlRisizerInfo.srcHeight= h;
        UtlRisizerInfo.dstAddr = dst_ptr;
        UtlRisizerInfo.dstWidth = ImageWidthArray[current_scale];
        UtlRisizerInfo.dstHeight = ImageHeightArray[current_scale];
        if (UtlRisizerInfo.dstWidth == 0 || UtlRisizerInfo.dstHeight==0)
        {
            MY_LOGD("PipeBilinearResizer break %d", current_scale);
            break;
        }
        PipeBilinearResizer(&UtlRisizerInfo);
        dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];
    }
}

/*******************************************************************************
* Public API
********************************************************************************/
halFDBase*
halFDVT::
getInstance()
{
    Mutex::Autolock _l(gLock);
    halFDBase* pHalFD;

    pHalFD = new halFDVT();

    return  pHalFD;

}

void
halFDVT::
destroyInstance()
{
    Mutex::Autolock _l(gLock);
    delete this;

}

halFDVT::halFDVT()
{
    mpMTKFDVTObj = NULL;

    mFDW = 0;
    mFDH = 0;
    mInited = 0;
    mDoDumpImage = 0;

    mFTStop = 0;
    mFTBufReady = 1;
    sem_init(&mSemFTExec, 0, 0);
    sem_init(&mSemFTLock, 0, 1);
    pthread_create(&mFTThread, NULL, onFTThreadLoop, this);

    #if (USE_HW_FD)
    mpMTKFDVTObj = MTKDetection::createInstance(DRV_FD_OBJ_HW);
    #else
    mpMTKFDVTObj = MTKDetection::createInstance(DRV_FD_OBJ_FDFT_SW);
    #endif
}

halFDVT::~halFDVT()
{
    mFDW = 0;
    mFDH = 0;
    {
        mFTStop = 1;
        mFTBufReady = 0;
        ::sem_post(&mSemFTExec);
        pthread_join(mFTThread, NULL);
        sem_destroy(&mSemFTExec);
    }
    MY_LOGD("[Destroy] mSemFTExec");
    if (mpMTKFDVTObj) {
        mpMTKFDVTObj->destroyInstance();
    }
    MY_LOGD("[Destroy] mpMTKFDVTObj->destroyInstance");
    sem_destroy(&mSemFTLock);
    MY_LOGD("[Destroy] mSemFTLock");
    mpMTKFDVTObj = NULL;
}

MINT32
halFDVT::halFDInit(
    MUINT32 fdW,
    MUINT32 fdH,
    MUINT8 *WorkingBuffer,
    MUINT32 WorkingBufferSize,
    MBOOL   SWResizerEnable,
    MUINT8  Current_mode  //0:FD, 1:SD, 2:vFB 3:CFB 4:VSDOF
)
{
    Mutex::Autolock _l(gInitLock);
    MUINT32 i;
    MINT32 err = MHAL_NO_ERROR;
    MTKFDFTInitInfo FDFTInitInfo;
    FD_Customize_PARA FDCustomData;

    if(mInited) {
        MY_LOGW("Warning!!! FDVT HAL OBJ is already inited!!!!");
        MY_LOGW("Old Width/Height : %d/%d, Parameter Width/Height : %d/%d", mFDW, mFDH, fdW, fdH);
        return MHAL_REINIT_ERROR;
    }
    {
        char cLogLevel[PROPERTY_VALUE_MAX];
        ::property_get("debug.camera.fd.dumpimage", cLogLevel, "0");
        mDoDumpImage = atoi(cLogLevel);
    }
    // Start initial FD
    mCurrentMode = Current_mode;
    #if (0 == SMILE_DETECT_SUPPORT)
    // If Smile detection is not support, change mode to FD mode
    if (mCurrentMode == HAL_FD_MODE_SD) {
        mCurrentMode = HAL_FD_MODE_FD;
    }
    #endif
    MY_LOGD("[mHalFDInit] Current_mode:%d, SrcW:%d, SrcH:%d, ",Current_mode, fdW, fdH);

    if ( Current_mode == HAL_FD_MODE_FD || Current_mode == HAL_FD_MODE_MANUAL ) {
        for(i=0;i<FD_SCALES;i++)
        {
            mimage_width_array[i] = image_width_array_v3[i];
            mimage_height_array[i] = image_height_array_v3[i];
        }
        mUserScaleNum = 12;
        if(Current_mode == HAL_FD_MODE_MANUAL) {
            mUseUserScale = 1;
        } else {
            mUseUserScale = 0;
        }
    } else {
        for(i=0;i<FD_SCALES;i++)
        {
            mimage_width_array[i] = image_width_array_v1[i];
            mimage_height_array[i] = image_height_array_v1[i];
        }
        mUserScaleNum = 11;
        mUseUserScale = 0;
    }

    get_fd_CustomizeData(&FDCustomData);

    // Set FD/FT buffer resolution
    #if (SINGLE_FACE_STABLE_ENABLE == 1)
    // force enable adaptive scale table
    mUseUserScale = 1;
    #else
    if (Current_mode != HAL_FD_MODE_MANUAL) {
        mUseUserScale = FDCustomData.UseCustomScale;

    }
    #endif
    mFDW = fdW;
    mFDH = fdH;
    mFTWidth =  MHAL_FDVT_FTBUF_W;
    mFTHeight = MHAL_FDVT_FTBUF_W * mFDH/mFDW;
    for(int j=0;j<FD_SCALES;j++)
    {
        mimage_height_array[j] = mimage_width_array[j]*mFDH/mFDW;
        mUserScaleNum = j;
        if(mimage_height_array[j] <= 25 || mimage_width_array[j] <= 25) {
            break;
        }
    }
    FDFTInitInfo.FDBufWidth = mimage_width_array[0];
    FDFTInitInfo.FDBufHeight = mimage_height_array[0];
    FDFTInitInfo.FDTBufWidth = mFTWidth;
    FDFTInitInfo.FDTBufHeight =  mFTHeight;
    FDFTInitInfo.FDSrcWidth = mFDW;
    FDFTInitInfo.FDSrcHeight = mFDH;

    // Set FD/FT initial parameters
    mFDFilterRatio = FDCustomData.FDSizeRatio;
    FDFTInitInfo.WorkingBufAddr = WorkingBuffer;
    FDFTInitInfo.WorkingBufSize = WorkingBufferSize;
    FDFTInitInfo.FDThreadNum = FDCustomData.FDThreadNum;
    #if (USE_SW_FD_TO_DEBUG)
    FDFTInitInfo.FDThreshold = 256;
    #else
    FDFTInitInfo.FDThreshold = FDCustomData.FDThreshold;
    #endif
    FDFTInitInfo.MajorFaceDecision = FDCustomData.MajorFaceDecision;
    FDFTInitInfo.OTRatio = FDCustomData.OTRatio;
    FDFTInitInfo.SmoothLevel = FDCustomData.SmoothLevel;
    FDFTInitInfo.Momentum = FDCustomData.Momentum;
    FDFTInitInfo.MaxTrackCount = FDCustomData.MaxTrackCount;
    if(mCurrentMode == HAL_FD_MODE_VFB)
        FDFTInitInfo.FDSkipStep = 1;   //FB mode
    else
        FDFTInitInfo.FDSkipStep = FDCustomData.FDSkipStep;

    FDFTInitInfo.FDRectify = FDCustomData.FDRectify;

    FDFTInitInfo.OTFlow = FDCustomData.OTFlow;
    if(mCurrentMode == HAL_FD_MODE_VFB) {
        FDFTInitInfo.OTFlow = 1;
        if(FDFTInitInfo.OTFlow==0)
            FDFTInitInfo.FDRefresh = 90;   //FB mode
        else
            FDFTInitInfo.FDRefresh = FDCustomData.FDRefresh;   //FB mode
    }
    else{
        FDFTInitInfo.FDRefresh = FDCustomData.FDRefresh;
    }
    FDFTInitInfo.FDImageArrayNum = 14;
    FDFTInitInfo.FDImageWidthArray = mimage_width_array;
    FDFTInitInfo.FDImageHeightArray = mimage_height_array;
    FDFTInitInfo.FDCurrent_mode = mCurrentMode;
    FDFTInitInfo.FDModel = FDCustomData.FDModel;
    FDFTInitInfo.FDMinFaceLevel = 0;
    FDFTInitInfo.FDMaxFaceLevel = 13;
    FDFTInitInfo.FDImgFmtCH1 = FACEDETECT_IMG_Y_SINGLE;
    FDFTInitInfo.FDImgFmtCH2 = FACEDETECT_IMG_RGB565;
    FDFTInitInfo.SDImgFmtCH1 = FACEDETECT_IMG_Y_SCALES;
    FDFTInitInfo.SDImgFmtCH2 = FACEDETECT_IMG_Y_SINGLE;
    FDFTInitInfo.SDThreshold = FDCustomData.SDThreshold;
    FDFTInitInfo.SDMainFaceMust = FDCustomData.SDMainFaceMust;
    FDFTInitInfo.GSensor = FDCustomData.GSensor;
    FDFTInitInfo.GenScaleImageBySw = 1;
    FDFTInitInfo.ParallelRGB565Conversion = true;
    FDFTInitInfo.LockOtBufferFunc = LockFTBuffer;
    FDFTInitInfo.UnlockOtBufferFunc = UnlockFTBuffer;
    FDFTInitInfo.lockAgent = this;
    FDFTInitInfo.DelayThreshold = 83; // 83 is default value
    FDFTInitInfo.DelayCount = 2; // 2 is default value
    FDFTInitInfo.FDManualMode = mUseUserScale;
    FDFTInitInfo.LandmarkEnableCnt = 1;
    // dump initial info
    dumpFDParam(FDFTInitInfo);
    // Set initial info to FD algo
    mpMTKFDVTObj->FDVTInit(&FDFTInitInfo);

    mEnableSWResizerFlag = SWResizerEnable;
    if(mEnableSWResizerFlag)
    {
        mImageScaleTotalSize = 0;
        for(i=0; i<FD_SCALES;i++)
        {
            mImageScaleTotalSize += mimage_width_array[i]*mimage_height_array[i];
        }
        mpImageScaleBuffer = new unsigned char[mImageScaleTotalSize];
    }

    mpImageVGABuffer = new unsigned char[gimage_input_width_vga*gimage_input_height_buffer];
    memset(mpImageVGABuffer, 0, gimage_input_width_vga*gimage_input_height_buffer);

    MY_LOGD("[%s] End", __FUNCTION__);
    mFrameCount = 0;
    mDetectedFaceNum = 0;
    mInited = 1;
    return err;
}

MINT32
halFDVT::halFDGetVersion(
)
{
    #if (USE_HW_FD)
    #if (HW_FD_SUBVERSION == 1)
    return HAL_FD_VER_HW41;
    #else
    return HAL_FD_VER_HW40;
    #endif
    #else
    return HAL_FD_VER_SW36;
    #endif
}

MINT32
halFDVT::halFDDo(
struct FD_Frame_Parameters &Param
)
{
    Mutex::Autolock _l(gInitLock);
    FdOptions FDOps;
    MUINT8* y_vga;
    MBOOL SDEnable;
    MINT32 StartPos = 0;
    MINT32 ScaleNum = mUserScaleNum;
    FDVT_OPERATION_MODE_ENUM Mode = FDVT_IDLE_MODE;

    SDEnable = Param.SDEnable && (mCurrentMode == HAL_FD_MODE_SD);
    MY_LOGD("SD Status: SDEnable:%d, img_w:%d, img_h:%d, Src_Phy_Addr:0x%x,", mCurrentMode, mFTWidth, mFTHeight, Param.pImageBufferPhyP0);

    if(!mInited) {
        return MHAL_UNINIT_ERROR;
    }

    FACEDETECT_GSENSOR_DIRECTION direction;
    if( Param.Rotation_Info == 0 )
        direction = FACEDETECT_GSENSOR_DIRECTION_0;
    else if( Param.Rotation_Info == 90 )
        direction = FACEDETECT_GSENSOR_DIRECTION_270;
    else if( Param.Rotation_Info == 270 )
        direction = FACEDETECT_GSENSOR_DIRECTION_90;
    else if( Param.Rotation_Info == 180 )
        direction = FACEDETECT_GSENSOR_DIRECTION_180;
    else
        direction = FACEDETECT_GSENSOR_DIRECTION_NO_SENSOR;

    #if (SINGLE_FACE_STABLE_ENABLE == 1)
    // Dynamic scaler
    if (mDetectedFaceNum == 1 && (mFrameCount % 3) != 0) {
        MINT32 i;
        float FaceRatio = ((float)(mFirstFaceResult.rect[2] - mFirstFaceResult.rect[0])) / 2000.0;
        MY_LOGD("WillDBG use all detect : %f", FaceRatio);
        for (i = 0; i < mUserScaleNum; i++) {
            if(FaceRatio <= ((float)24/(float)mimage_width_array[i]))
                break;
        }
        MY_LOGD("WillDBG stop loop : %d", i);
        if(i != 0) {
            float leftdiff, rightdiff;
            leftdiff = FaceRatio - ((float)24/(float)mimage_width_array[i - 1]);
            rightdiff = ((float)24/(float)mimage_width_array[i]) - FaceRatio;
            if(leftdiff < rightdiff) {
                i--;
            }
        }
        if(i == 0) {
            StartPos = 0;
            ScaleNum = 2;
        } else if (i >= (mUserScaleNum - 1)) {
            StartPos = mUserScaleNum - 2;
            ScaleNum = 2;
        } else {
            StartPos = i - 1;
            ScaleNum = 3;
        }
        MY_LOGD("WillDBG Start pos : %d, Scalenum : %d", StartPos, ScaleNum);
    }
    if (mDetectedFaceNum == 1) {
        Mode = FDVT_GFD_MODE;
    }
    mFrameCount++;
    #endif

    // Set FD operation
    FDOps.fd_state = FDVT_GFD_MODE;
    FDOps.direction = direction;
    FDOps.fd_scale_count = ScaleNum;
    FDOps.fd_scale_start_position = StartPos;
    FDOps.gfd_fast_mode = 0;
    FDOps.ae_stable = Param.AEStable;
    FDOps.ForceFDMode = Mode;
    if (Param.pImageBufferPhyP2 != NULL) {
        FDOps.inputPlaneCount = 3;
    } else if (Param.pImageBufferPhyP1 != NULL) {
        FDOps.inputPlaneCount = 2;
    } else {
        FDOps.inputPlaneCount = 1;
    }
    FDOps.ImageBufferPhyPlane1 = Param.pImageBufferPhyP0;
    FDOps.ImageBufferPhyPlane2 = Param.pImageBufferPhyP1;
    FDOps.ImageBufferPhyPlane3 = Param.pImageBufferPhyP2;
    FDOps.ImageScaleBuffer = mpImageScaleBuffer;
    FDOps.ImageBufferRGB565 = Param.pRGB565Image;
    FDOps.ImageBufferSrcVirtual = (MUINT8*)Param.pImageBufferVirtual;

    if(mEnableSWResizerFlag)
    {
        FDVT_OPERATION_MODE_ENUM mode;
        mpMTKFDVTObj->FDVTGetMode(&mode);

        // Prepare buffer for FD algo
        #if (USE_HW_FD)
        if(SDEnable) {
            if(mFDW == 640) // VGA like size
            {
                y_vga = (MUINT8*)Param.pPureYImage;
                CreateScaleImages_Y( (MUINT8*)y_vga, mpImageScaleBuffer, mFDW, mFDH, mimage_width_array, mimage_height_array, 1);
            }
            else
            {
                MY_LOGD("Not VGA like size : %dx%d", mFDW, mFDH);
                Create640WidthImage_Y((MUINT8*)Param.pPureYImage, mFDW, mFDH, mpImageVGABuffer);
                y_vga = mpImageVGABuffer;
                CreateScaleImages_Y( (MUINT8*)Param.pPureYImage, mpImageScaleBuffer, mFDW, mFDH, mimage_width_array, mimage_height_array, 1);
            }
        }
        #else
        if(mFDW == 640) // VGA like size
        {
            y_vga = (MUINT8*)Param.pPureYImage;
            if( mode == FDVT_GFD_MODE || SDEnable)
            {
                CreateScaleImages_Y( (MUINT8*)y_vga, mpImageScaleBuffer, mFDW, mFDH, mimage_width_array, mimage_height_array, 1);
            }
        }
        else
        {
            if(SDEnable || mode == FDVT_GFD_MODE)
            {
                MY_LOGD("Not VGA like size : %dx%d", mFDW, mFDH);
                if(SDEnable) {
                    Create640WidthImage_Y((MUINT8*)Param.pPureYImage, mFDW, mFDH, mpImageVGABuffer);
                    y_vga = mpImageVGABuffer;
                } else {
                    y_vga = ImageBuffer2;
                }
                CreateScaleImages_Y( (MUINT8*)Param.pPureYImage, mpImageScaleBuffer, mFDW, mFDH, mimage_width_array, mimage_height_array, 1);
            }
        }
        #endif // end of #if (USE_HW_FD)

        MY_LOGD("FDVTMain IN : %p", Param.pImageBufferPhyP0);
        mpMTKFDVTObj->FDVTMain(&FDOps);
        // Wait FT Buffer done.
        LockFTBuffer(this);
        UnlockFTBuffer(this);
        MY_LOGD("FDVTMain out");

        result  DetectResult[MAX_FACE_NUM];
        mpMTKFDVTObj->FDVTGetResult( (MUINT8 *)&DetectResult, FACEDETECT_TRACKING_DISPLAY);

        if(SDEnable)
        {
            FDOps.fd_state = FDVT_SD_MODE;
            FDOps.ImageBufferRGB565 = y_vga;
            mpMTKFDVTObj->FDVTMain(&FDOps);
        }
    }
    else
    {
        FDOps.ImageScaleBuffer = Param.pScaleImages;
        mpMTKFDVTObj->FDVTMain(&FDOps);

        if(SDEnable)
        {
            FDOps.fd_state = FDVT_SD_MODE;
            FDOps.ImageBufferRGB565 = Param.pPureYImage;
            mpMTKFDVTObj->FDVTMain(&FDOps);
        }
    }

    return MHAL_NO_ERROR;
}

MINT32
halFDVT::halFDUninit(
)
{
    //MHAL_LOG("[halFDUninit] IN \n");
    Mutex::Autolock _l(gInitLock);

    if(!mInited) {
        MY_LOGW("FD HAL Object is already uninited...");
        return MHAL_NO_ERROR;
    }

    mpMTKFDVTObj->FDVTReset();

    if (mEnableSWResizerFlag)
    {
         delete[]mpImageScaleBuffer;
    }
    delete[]mpImageVGABuffer;
    mInited = 0;

    return MHAL_NO_ERROR;
}

MINT32
halFDVT::halFDGetFaceInfo(
    MtkCameraFaceMetadata *fd_info_result
)
{
    MUINT8 i;
    MY_LOGD("[GetFaceInfo] NUM_Face:%d,", mFdResult_Num);

    if( (mFdResult_Num < 0) || (mFdResult_Num > 15) )
         mFdResult_Num = 0;

    fd_info_result->number_of_faces =  mFdResult_Num;

    for(i=0;i<mFdResult_Num;i++)
    {
        fd_info_result->faces[i].rect[0] = mFdResult[i].rect[0];
        fd_info_result->faces[i].rect[1] = mFdResult[i].rect[1];
        fd_info_result->faces[i].rect[2] = mFdResult[i].rect[2];
        fd_info_result->faces[i].rect[3] = mFdResult[i].rect[3];
        fd_info_result->faces[i].score = mFdResult[i].score;
        fd_info_result->posInfo[i].rop_dir = mFdResult[i].rop_dir;
        fd_info_result->posInfo[i].rip_dir  = mFdResult[i].rip_dir;
    }

    return MHAL_NO_ERROR;
}

MINT32
halFDVT::halFDGetFaceResult(
    MtkCameraFaceMetadata *fd_result,
    MINT32 ResultMode
)
{

    MINT32 faceCnt = 0;
    result pbuf[MAX_FACE_NUM];
    MUINT8 i;
    MINT8 DrawMode = 0;

    faceCnt = mpMTKFDVTObj->FDVTGetResult((MUINT8 *) pbuf, FACEDETECT_TRACKING_DISPLAY);

    MY_LOGD("[%s]first scale W(%d) H(%d)", __FUNCTION__, mimage_width_array[0], mimage_height_array[0]);
    mpMTKFDVTObj->FDVTGetICSResult((MUINT8 *) fd_result, (MUINT8 *) pbuf, mimage_width_array[0], mimage_height_array[0], 0, 0, 0, DrawMode);

    mDetectedFaceNum = mFdResult_Num = fd_result->number_of_faces;

    if(mDetectedFaceNum > 0) {
        mFirstFaceResult.rect[0] = fd_result->faces[0].rect[0];
        mFirstFaceResult.rect[1] = fd_result->faces[0].rect[1];
        mFirstFaceResult.rect[2] = fd_result->faces[0].rect[2];
        mFirstFaceResult.rect[3] = fd_result->faces[0].rect[3];
    }

    //Facial Size Filter
    for(i=0;i < mFdResult_Num;i++)
    {
        int diff = fd_result->faces[i].rect[3] - fd_result->faces[i].rect[1];
        float ratio = (float)diff / 2000.0;
        if(ratio < mFDFilterRatio) {
            int j;
            for(j = i; j < (mFdResult_Num - 1); j++) {
                fd_result->faces[j].rect[0] = fd_result->faces[j+1].rect[0];
                fd_result->faces[j].rect[1] = fd_result->faces[j+1].rect[1];
                fd_result->faces[j].rect[2] = fd_result->faces[j+1].rect[2];
                fd_result->faces[j].rect[3] = fd_result->faces[j+1].rect[3];
                fd_result->faces[j].score = fd_result->faces[j+1].score;
                fd_result->faces[j].id = fd_result->faces[j+1].id;
                fd_result->posInfo[j].rop_dir = fd_result->posInfo[j+1].rop_dir;
                fd_result->posInfo[j].rip_dir = fd_result->posInfo[j+1].rip_dir;
                fd_result->faces_type[j] = fd_result->faces_type[j+1];
            }
            fd_result->faces[j].rect[0] = 0;
            fd_result->faces[j].rect[1] = 0;
            fd_result->faces[j].rect[2] = 0;
            fd_result->faces[j].rect[3] = 0;
            fd_result->faces[j].score = 0;
            fd_result->posInfo[j].rop_dir = 0;
            fd_result->posInfo[j].rip_dir = 0;
            fd_result->number_of_faces--;
            mFdResult_Num--;
            faceCnt--;
            i--;
        }
    }

    for(i=0;i<mFdResult_Num;i++)
    {
          mFdResult[i].rect[0] = fd_result->faces[i].rect[0];
          mFdResult[i].rect[1] = fd_result->faces[i].rect[1];
          mFdResult[i].rect[2] = fd_result->faces[i].rect[2];
          mFdResult[i].rect[3] = fd_result->faces[i].rect[3];
          mFdResult[i].score = fd_result->faces[i].score;
          mFdResult[i].rop_dir = fd_result->posInfo[i].rop_dir;
          mFdResult[i].rip_dir  = fd_result->posInfo[i].rip_dir;
    }
    for(i=mFdResult_Num;i<MAX_FACE_NUM;i++)
    {
          mFdResult[i].rect[0] = 0;
          mFdResult[i].rect[1] = 0;
          mFdResult[i].rect[2] = 0;
          mFdResult[i].rect[3] = 0;
          mFdResult[i].score = 0;
          mFdResult[i].rop_dir = 0;
          mFdResult[i].rip_dir  = 0;
    }

    return faceCnt;
}

MINT32
halFDVT::halSDGetSmileResult()
{
    MINT32 SmileCnt = 0;
    MUINT32 pbuf1;

    #if (0 == SMILE_DETECT_SUPPORT)
    // If Smile detection is not support, return zero directly.
    return 0;
    #endif

    SmileCnt = mpMTKFDVTObj->FDVTGetSDResult(pbuf1);

    return SmileCnt;
}

MINT32
halFDVT::halFDYUYV2ExtractY(
MUINT8 *dstAddr,
MUINT8 *srcAddr,
MUINT32 src_width,
MUINT32 src_height
)
{
    MY_LOGD("DO Extract Y In");
    int i,j;

    int length = src_width*src_height*2;

    for(i=length;i != 0;i-=2)
    {
      *dstAddr++ = *srcAddr;
      srcAddr+=2;
    }

    MY_LOGD("DO Extract Y Out");

    return MHAL_NO_ERROR;
}

// Create RGB565 QVGA for Tracking directly
MINT32
halFDVT::halFTBufferCreate(
MUINT8 *dstAddr,
MUINT8 *srcAddr,
MUINT8 ucPlane,
MUINT32 src_width,
MUINT32 src_height
)
{
    if((src_width == 640) && (ucPlane ==1))
    {
        MY_LOGD("DO RGB565 (SW) In");
        doRGB565BufferP1_SW(dstAddr, srcAddr, src_width, src_height);
        MY_LOGD("DO RGB565 (SW) Out");
    }
    else if((src_width == 320) && (ucPlane ==1))
    {
        MY_LOGD("DO RGB565 (SW_QVGA) In");
        doRGB565BufferQVGAP1_SW(dstAddr, srcAddr, src_width, src_height);
        MY_LOGD("DO RGB565 (SW_QVGA) Out");
    }
    else
    {
        MY_LOGD("DO RGB565 (DDP) In");
        mFTWidth =  MHAL_FDVT_FTBUF_W;
        mFTHeight = mFTWidth * src_height / src_width;
        doRGB565Buffer_DDP(dstAddr, srcAddr, ucPlane);
        MY_LOGD("DO RGB565 (DDP) Out");
    }

    if(mDoDumpImage)
        dumpFDImages(srcAddr, dstAddr);

    return MHAL_NO_ERROR;
}

// Create RGB565 QVGA for Tracking directly
MINT32
halFDVT::halGetFTBufferSize(
MUINT32 *width,
MUINT32 *height
)
{
    *width = mFTWidth;
    *height = mFTHeight;
    return MHAL_NO_ERROR;
}

// Create RGB565 QVGA for Tracking use another thread
MINT32
halFDVT::halFTBufferCreateAsync(
MUINT8 *dstAddr,
MUINT8 *srcAddr,
MUINT8 ucPlane,
MUINT32 src_width,
MUINT32 src_height
)
{
    mFTParameter.dstAddr = dstAddr;
    mFTParameter.srcAddr = srcAddr;
    mFTParameter.ucPlane = ucPlane;
    mFTParameter.src_width = src_width;
    mFTParameter.src_height = src_height;
    LockFTBuffer(this);
    mFTBufReady = 0;
    ::sem_post(&mSemFTExec);
    return MHAL_NO_ERROR;
}

/*******************************************************************************
* Private function
********************************************************************************/
// Create RGB565 QVGA for Tracking, use another thread
void* halFDVT::onFTThreadLoop(void* arg)
{
    halFDVT *_this = reinterpret_cast<halFDVT*>(arg);
    while(1) {
        ::sem_wait(&(_this->mSemFTExec));
        if(_this->mFTStop) {
            break;
        }
        {
            _this->halFTBufferCreate(_this->mFTParameter.dstAddr,
                                     _this->mFTParameter.srcAddr,
                                     _this->mFTParameter.ucPlane,
                                     _this->mFTParameter.src_width,
                                     _this->mFTParameter.src_height);
            _this->mFTBufReady = 1;
        }
        _this->UnlockFTBuffer(_this);
    }
    return NULL;
}
void halFDVT::LockFTBuffer(void* arg)
{
    halFDVT *_this = reinterpret_cast<halFDVT*>(arg);
    ::sem_wait(&(_this->mSemFTLock)); // lock FT Buffer
}
void halFDVT::UnlockFTBuffer(void* arg)
{
    halFDVT *_this = reinterpret_cast<halFDVT*>(arg);
    ::sem_post(&(_this->mSemFTLock)); // lock FT Buffer
}
// Create RGB565 buffer
bool halFDVT::doRGB565BufferP1_SW(MUINT8 *a_dstAddr, MUINT8 *a_srcAddr, MUINT32 SrcWidth, MUINT32 SrcHeight)
{
     bool ret = true;

    unsigned char *src_yp = (unsigned char *)a_srcAddr;
    unsigned char *dst_rgb = (unsigned char *)a_dstAddr;

    unsigned char* pucYBuf;
    unsigned char* pucUVBuf;
    unsigned int i, j, k;
    int Y[4], U, V, R[2], G[2], B[2], r, g, b;
    unsigned int dImgW = SrcWidth;
    unsigned int dImgH = SrcHeight;
    //int dImgW = g_FDW;
    //int dImgH = g_FDH;

    pucYBuf = src_yp;

    for(i=0;i<dImgH;i=i+2)
    {
        for(j=0;j<dImgW*2;j=j+4)
        {
            Y[0] = *(pucYBuf + ((i+0) * dImgW*2) + j);
            Y[1] = *(pucYBuf + ((i+0) * dImgW*2) + j+2) ;
            Y[2] = *(pucYBuf + ((i+1) * dImgW*2) + j);
            Y[3] = *(pucYBuf + ((i+1) * dImgW*2) + j+2) ;

            Y [0]= (Y[0]+Y[1]+Y[2]+Y[3]) >> 2;
            U  =  (*(pucYBuf + ((i+0) * dImgW*2) + j+1) + *(pucYBuf + ((i+1) * dImgW*2) + j+1) ) >> 1;
            V  =  (*(pucYBuf + ((i+0) * dImgW*2) + j+3) + *(pucYBuf + ((i+1) * dImgW*2) + j+3) ) >> 1;

            for(k=0;k<1;k++)
            {
                r = (32 * Y[k] + 45 * (V-128) + 16) / 32;
                g = (32 * Y[k] - 11 * (U-128) - 23 * (V-128) + 16) / 32;
                b = (32 * Y[k] + 57 * (U-128) + 16) / 32;

                R[k] = (r<0) ? 0: (r>255) ? 255 : r;
                G[k]= (g<0) ? 0: (g>255) ? 255 : g;
                B[k] = (b<0) ? 0: (b>255) ? 255 : b;
            }

            *(dst_rgb + ((i>>1)  * dImgW ) + (j>>1)+0) = ((G[0] & 0x1C) <<3 ) + ((B[0] & 0xF8) >>3 );
            *(dst_rgb + ((i>>1)  * dImgW ) + (j>>1)+1) = ((G[0] & 0xE0) >>5 ) + ((R[0] & 0xF8));
        }
    }

        return ret;
}
bool halFDVT::doRGB565BufferQVGAP1_SW(MUINT8 *a_dstAddr, MUINT8 *a_srcAddr, MUINT32 SrcWidth1, MUINT32 SrcHeight1)
{
     bool ret = true;

    unsigned char *src_yp = (unsigned char *)a_srcAddr;
    unsigned char *dst_rgb = (unsigned char *)a_dstAddr;

    unsigned char* pucYBuf;
    unsigned char* pucUVBuf;
    unsigned int i, j, k;
    int Y[4], U, V, R[2], G[2], B[2], r, g, b;
    unsigned int dImgW = SrcWidth1;
    unsigned int dImgH = SrcHeight1;
    //int dImgW = g_FDW;
    //int dImgH = g_FDH;

    pucYBuf = src_yp;

    for(i=0;i<dImgH;i++)
    {
        for(j=0;j<dImgW*2;j=j+4)
        {
            Y[0] = *(pucYBuf + (i * dImgW*2) + j+0);
            Y[1] = *(pucYBuf + (i * dImgW*2) + j+2);

            U  =  *(pucYBuf + (i * dImgW*2) + j+1);
            V  =  *(pucYBuf + (i * dImgW*2) + j+3);

            for(k=0;k<2;k++)
            {
                r = (32 * Y[k] + 45 * (V-128) + 16) / 32;
                g = (32 * Y[k] - 11 * (U-128) - 23 * (V-128) + 16) / 32;
                b = (32 * Y[k] + 57 * (U-128) + 16) / 32;

                R[k] = (r<0) ? 0: (r>255) ? 255 : r;
                G[k] = (g<0) ? 0: (g>255) ? 255 : g;
                B[k] = (b<0) ? 0: (b>255) ? 255 : b;
            }

            *(dst_rgb + ((i<<1)  * dImgW ) + j+0) = ((G[0] & 0x1C) <<3 ) + ((B[0] & 0xF8) >>3 );
            *(dst_rgb + ((i<<1)  * dImgW ) + j+1) = ((G[0] & 0xE0) >>5 ) + ((R[0] & 0xF8));

            *(dst_rgb + ((i<<1)  * dImgW ) + j+2) = ((G[1] & 0x1C) <<3 ) + ((B[1] & 0xF8) >>3 );
            *(dst_rgb + ((i<<1)  * dImgW ) + j+3) = ((G[1] & 0xE0) >>5 ) + ((R[1] & 0xF8));

        }
    }

        return ret;
}
bool halFDVT::doRGB565Buffer_DDP(MUINT8 *a_dstAddr, MUINT8 *a_srcAddr, MUINT32 planes)
{
    bool ret = true;

#ifdef FDVT_DDP_SUPPORT
    DpBlitStream FDstream;
    unsigned char *src_yp = (unsigned char *)a_srcAddr;
    unsigned char *dst_rgb = (unsigned char *)a_dstAddr;
    void* src_addr_list[3];
    unsigned int src_size_list[3];
    void* dst_addr_list[3];
    unsigned int dst_size_list[3];

    int src_ysize = mFDW * mFDH;
    int src_usize, src_vsize;
    src_usize = src_vsize = src_ysize / 4;

    //*****************************************************************************//
    //*******************src  YUY2/NV21/YV12 **************************************//
    //*****************************************************************************//
    if(planes ==1)
    {
        FDstream.setSrcBuffer((void *)src_yp, mFDW*mFDH*2);
        FDstream.setSrcConfig(mFDW, mFDH, DP_COLOR_YUYV);
    }

    else if(planes ==2)
    {

        src_addr_list[0] = (void *)src_yp;
        src_addr_list[1] = (void *)(src_yp + src_ysize);
        src_size_list[0] = src_ysize;
        src_size_list[1] = src_usize + src_vsize;
        FDstream.setSrcBuffer((void**)src_addr_list, src_size_list, planes);
        FDstream.setSrcConfig(mFDW, mFDH, DP_COLOR_NV21); //82&72
    }

    else if(planes ==3)
    {
        src_addr_list[0] = (void *)src_yp;
        src_addr_list[1] = (void *)(src_yp + src_ysize);
        src_addr_list[2] = (void *)(src_yp + src_ysize * 5/4);

        src_size_list[0] = src_ysize;
        src_size_list[1] = src_vsize;
        src_size_list[2] = src_usize;
        FDstream.setSrcBuffer((void**)src_addr_list, src_size_list, planes);
        FDstream.setSrcConfig(mFDW, mFDH, DP_COLOR_YV12); //82&72
    }

    //***************************dst RGB565********************************//
    FDstream.setDstBuffer((void *)dst_rgb, mFTWidth*mFTHeight*2);
    FDstream.setDstConfig(mFTWidth, mFTHeight, eRGB565);
    FDstream.setRotate(0);

    //***********************************************************************//
    //Adjust FD thread priority to RR
    //int const policy    = SCHED_OTHER;
#if MTKCAM_HAVE_RR_PRIORITY
    int policy    = SCHED_RR;
    int priority  = PRIO_RT_FD_THREAD;
    //
    //
    struct sched_param sched_p;
    ::sched_getparam(0, &sched_p);
    sched_p.sched_priority = priority;  //  Note: "priority" is nice value
    sched_setscheduler(0, policy, &sched_p);
    setpriority(PRIO_PROCESS, 0, priority);
#endif
    //***********************************************************************//

    // set & add pipe to stream
    if (0>FDstream.invalidate())  //trigger HW
    {
          MY_LOGD("FDstream invalidate failed");
          //***********************************************************************//
          //Adjust FD thread priority to Normal and return false
          //***********************************************************************//
#if MTKCAM_HAVE_RR_PRIORITY
          policy    = SCHED_OTHER;
          priority  = 0;
          ::sched_getparam(0, &sched_p);
          sched_p.sched_priority = priority;  //  Note: "priority" is nice value
          sched_setscheduler(0, policy, &sched_p);
          setpriority(PRIO_PROCESS, 0, priority);
#endif
          return false;
    }

    //***********************************************************************//
    //Adjust FD thread priority to Normal
#if MTKCAM_HAVE_RR_PRIORITY
     policy    = SCHED_OTHER;
     priority  = 0;
    ::sched_getparam(0, &sched_p);
    sched_p.sched_priority = priority;  //  Note: "priority" is nice value
    sched_setscheduler(0, policy, &sched_p);
    setpriority(PRIO_PROCESS, 0, priority);
#endif
    //***********************************************************************//

#endif
     return ret;
}
// Dump FD and FT image
void halFDVT::dumpFDImages(MUINT8* pFDImgBuffer, MUINT8* pFTImgBuffer)
{
    char szFileName[100]={'\0'};
    char szFileName1[100]={'\0'};
    FILE * pRawFp;
    FILE * pRawFp1;
    int i4WriteCnt;
    static int count = 0;

    if (mDoDumpImage > 1) {
        sprintf(szFileName1, "/sdcard/FDTest/src_%04dx%04d_%04d_YUV.raw", mFDW, mFDH, count);
        sprintf(szFileName, "/sdcard/FDTest/dst_%04dx%04d_%04d_RGB.raw", mFTWidth, mFTHeight, count);
        count++;
    } else {
        sprintf(szFileName1, "/sdcard/src_%04d_%04d_YUV.raw", mFDW, mFDH);
        sprintf(szFileName, "/sdcard/dst_%04d_%04d_RGB.raw", mFTWidth, mFTHeight);
    }
    pRawFp1 = fopen(szFileName1, "wb");
    if (NULL == pRawFp1 )
    {
        MY_LOGD("Can't open file to save RAW Image\n");
        while(1);
    }
    i4WriteCnt = fwrite((void *)pFDImgBuffer,1, (mFDW * mFDH * 2),pRawFp1);
    fflush(pRawFp1);
    fclose(pRawFp1);

    pRawFp = fopen(szFileName, "wb");
    if (NULL == pRawFp )
    {
        MY_LOGD("Can't open file to save RAW Image\n");
        while(1);
    }
    i4WriteCnt = fwrite((void *)pFTImgBuffer,1, (mFTWidth * mFTHeight * 2),pRawFp);
    fflush(pRawFp);
    fclose(pRawFp);
}
// Dump FD information
void halFDVT::dumpFDParam(MTKFDFTInitInfo &FDFTInitInfo)
{
    MY_LOGD("WorkingBufAddr = %d", FDFTInitInfo.WorkingBufAddr);
    MY_LOGD("WorkingBufSize = %d", FDFTInitInfo.WorkingBufSize);
    MY_LOGD("FDThreadNum = %d", FDFTInitInfo.FDThreadNum);
    MY_LOGD("FDThreshold = %d", FDFTInitInfo.FDThreshold);
    MY_LOGD("MajorFaceDecision = %d", FDFTInitInfo.MajorFaceDecision);
    MY_LOGD("OTRatio = %d", FDFTInitInfo.OTRatio);
    MY_LOGD("SmoothLevel = %d", FDFTInitInfo.SmoothLevel);
    MY_LOGD("FDSkipStep = %d", FDFTInitInfo.FDSkipStep);
    MY_LOGD("FDRectify = %d", FDFTInitInfo.FDRectify);
    MY_LOGD("FDRefresh = %d", FDFTInitInfo.FDRefresh);
    MY_LOGD("FDBufWidth = %d", FDFTInitInfo.FDBufWidth);
    MY_LOGD("FDBufHeight = %d", FDFTInitInfo.FDBufHeight);
    MY_LOGD("FDTBufWidth = %d", FDFTInitInfo.FDTBufWidth);
    MY_LOGD("FDTBufHeight = %d", FDFTInitInfo.FDTBufHeight);
    MY_LOGD("FDImageArrayNum = %d", FDFTInitInfo.FDImageArrayNum);
    MY_LOGD("FDImageWidthArray = ");
    for (int i = 0; i < FD_SCALES; i++)
    {
        MY_LOGD("%d, ",FDFTInitInfo.FDImageWidthArray[i]);
    }
    MY_LOGD("\n");
    MY_LOGD("FDImageHeightArray = ");
    for (int i = 0; i < FD_SCALES; i++)
    {
        MY_LOGD("%d, ",FDFTInitInfo.FDImageHeightArray[i]);
    }
    MY_LOGD("\n");
    MY_LOGD("FDMinFaceLevel = %d", FDFTInitInfo.FDMinFaceLevel);
    MY_LOGD("FDMaxFaceLevel = %d", FDFTInitInfo.FDMaxFaceLevel);
    MY_LOGD("FDImgFmtCH1 = %d", FDFTInitInfo.FDImgFmtCH1);
    MY_LOGD("FDImgFmtCH2 = %d", FDFTInitInfo.FDImgFmtCH2);
    MY_LOGD("SDImgFmtCH1 = %d", FDFTInitInfo.SDImgFmtCH1);
    MY_LOGD("SDImgFmtCH2 = %d", FDFTInitInfo.SDImgFmtCH2);
    MY_LOGD("SDThreshold = %d", FDFTInitInfo.SDThreshold);
    MY_LOGD("SDMainFaceMust = %d", FDFTInitInfo.SDMainFaceMust);
    MY_LOGD("GSensor = %d", FDFTInitInfo.GSensor);
    MY_LOGD("GenScaleImageBySw = %d", FDFTInitInfo.GenScaleImageBySw);
    MY_LOGD("FDManualMode = %d", FDFTInitInfo.FDManualMode);
    MY_LOGD("mUserScaleNum = %d", mUserScaleNum);
}
