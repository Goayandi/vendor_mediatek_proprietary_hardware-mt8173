/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#ifndef _HDR_H_
#define _HDR_H_

#include <utils/Vector.h>
#include <utils/RefBase.h>
#include <utils/Condition.h>
#include <semaphore.h>

#include <mtkcam/def/common.h>

#include <hal/IHDRHAL.h>
#include <camera_custom_hdr.h>
#include <mtkcam/drv/IHalSensor.h>

#include <mtkcam/utils/imgbuf/IImageBuffer.h>

#include <mtkcam/feature/hdr/HDRDefs.h>

using namespace android;
using namespace NSCam;

// ---------------------------------------------------------------------------

typedef MBOOL (*HDRProcCompleteCallback_t)(MVOID* user, const sp<IImageBuffer>& hdrResult, MBOOL ret);

// ---------------------------------------------------------------------------

namespace NS3Av3 {
    class CaptureParam_T;
};

// ---------------------------------------------------------------------------

namespace NSCam {

class HDR final : public virtual VirtualLightRefBase
{
public:
    HDR(char const * const pszShotName,
            uint32_t const u4ShotMode, int32_t const i4OpenId);
    virtual ~HDR();

    MBOOL setParam(MUINT32 paramId, MUINTPTR iArg1, MUINTPTR iArg2);
    MBOOL setShotParam(void const* pParam);
    MBOOL setJpegParam(void const* pParam);

    MBOOL getParam(MUINT32 paramId, MUINT32& rArg1, MUINT32& rArg2);

    MVOID setCompleteCallback(
            HDRProcCompleteCallback_t completeCB, MVOID* user);

    inline MINT32 getOutputFrameCount() const { return mu4OutputFrameNum; }

    // getCaptureInfo() is used to
    // 1. get 3A capture settings
    // 2. record the number of HDR input frames to mu4OutputFrameNum
    MBOOL getCaptureInfo(Vector<NS3Av3::CaptureParam_T>& vCap3AParam, MINT32& hdrFrameNum);

    MBOOL updateInfo();

    // prepare() is used to init HDR HAL and
    // allocate processing memory asynchronously
    MBOOL prepare();

    // addInputFrame() is used to set HDR input working buffers
    // for HDR post-processing
    // [NOTE] the frame index ordering should be
    // {0, 2, 4, ...} for main YUV and {1, 3, 5, ...} for small Y8 frames
    MBOOL addInputFrame(
            MINT32 frameIndex, const sp<IImageBuffer>& inBuffer);

    // addOutputFrame() is used to set HDR output buffers
    // for HDR post-processing
    // [NOTE] the type should de defined in HDROutputType
    MBOOL addOutputFrame(
            HDROutputType type, sp<IImageBuffer>& outBuffer);

    // process() is used to create a worker thread for HDR post-processing
    MBOOL process();

    // release() is used to
    // 1. clear HDR settings
    // 2. all HDR image buffers
    MBOOL release();

    // waitInputFrame() is used to wait for HDR input buffers
    MVOID waitInputFrame();

    // waitOutputFrame() is used to wait for HDR output buffers
    MVOID waitOutputFrame();

    // notify() is used to notify the user that HDR post-processing is done
    MVOID notify(MBOOL ret) const;

private:
    // number of sensor output frames (i.e. HDR input frames)
    MINT32     mu4OutputFrameNum;

    // YUV buffer size
    MUINT32    mu4W_yuv;
    MUINT32    mu4H_yuv;

    // small image buffer size
    MUINT32    mu4W_small;
    MUINT32    mu4H_small;

    // SW EIS image buffer size
    MUINT32    mu4W_se;
    MUINT32    mu4H_se;

    // down-scaled weighting map
    // NOTE: should be set after obtaining the dimension of OriWeight[0]
    MUINT32    mu4W_dsmap;
    MUINT32    mu4H_dsmap;

    // JPEG size
    MUINT32      mRotPicWidth;
    MUINT32      mRotPicHeight;

    // camera device ID
    MINT32       mOpenID;

    // shot mode
    MUINT32      mHDRShotMode;

    // sensor full size
    MSize        mSensorSize;

    // sensor type
    MUINT32      mSensorType;

    // HDR HAL
    std::unique_ptr<IHDRHAL> mpHdrHal;

    // class Memory is a simple implementation for non-ION memory buffer allocation
    // and cannot be used by hardware or inter-process communication directly
    class Memory final : public virtual VirtualLightRefBase
    {
    public:
        // allocates a new buffer of size bytes
        Memory(const size_t size);

        // frees the memory space pointed to by mAllocatedMemory
        ~Memory();

        // returns the amount of allocated memory in bytes
        size_t getSize() const { return mSize; }

        // returns a pointer to the allocated memory
        void* getAllocatedMemory() const { return mAllocatedMemory; }

    private:
        size_t mSize;
        void* mAllocatedMemory;
    };

    // HDR input buffer
    mutable Mutex       mSourceImgBufLock[HDR_MAX_INPUT_FRAME];
    sem_t               mSourceImgBufSem[HDR_MAX_INPUT_FRAME];
    sp<IImageBuffer>    mpSourceImgBuf[HDR_MAX_INPUT_FRAME];

    // TODO: reused as mWeightingBuf, mpBlurredWeightMapBuf
    mutable Mutex       mSmallImgBufLock[HDR_MAX_INPUT_FRAME];
    sem_t               mSmallImgBufSem[HDR_MAX_INPUT_FRAME];
    sp<IImageBuffer>    mpSmallImgBuf[HDR_MAX_INPUT_FRAME];

    // SW EIS image
    // TODO: reuse buffer of mpDownSizedWeightMapBuf
    mutable Mutex       mSEImgBufLock;
    sem_t               mSEImgBufSem;
    sp<IImageBuffer>    mpSEImgBuf[HDR_MAX_INPUT_FRAME];

    // HDR working buffer
    mutable Mutex       mHdrWorkingBufLock;
    sem_t               mHdrWorkingBufSem;
    sp<Memory>          mpHdrWorkingBuf;

    // TODO: reuse buffer of mpSmallImgBuf
    mutable Mutex       mWeightingBufLock;
    sem_t               mWeightingBufSem;
    sp<IImageBuffer>    mWeightingBuf[HDR_MAX_INPUT_FRAME];
    HDR_PIPE_WEIGHT_TBL_INFO** OriWeight;

    // down-sized weighting map
    sem_t               mDownSizedWeightMapBufAllocSem;
    mutable Mutex       mDownSizedWeightMapBufLock;
    sem_t               mDownSizedWeightMapBufSem;
    sp<IImageBuffer>    mpDownSizedWeightMapBuf[HDR_MAX_INPUT_FRAME];

    // blurred weighting map
    mutable Mutex       mBlurredWeightMapBufLock;
    sem_t               mBlurredWeightMapBufSem;
    sp<IImageBuffer>    mpBlurredWeightMapBuf[HDR_MAX_INPUT_FRAME];
    HDR_PIPE_WEIGHT_TBL_INFO** BlurredWeight;

    // blending buffer
    mutable Mutex       mBlendingBufLock;
    sem_t               mBlendingBufSem;
    sp<IImageBuffer>    mBlendingBuf;

    // HDR result buffer
    HDR_PIPE_HDR_RESULT_STRUCT mrHdrCroppedResult;
    sp<IImageBuffer>    mHdrResult;

    // HDR output buffers
    sem_t               mHDROutputFramesSem[HDR_OUTPUT_NUM];
    sp<IImageBuffer>    mHDROutputFrames[HDR_OUTPUT_NUM];

    HDR_PIPE_BMAP_BUFFER       mHdrSetBmapInfo;

    // exposure setting
    HDRExpSettingInputParam_T  mExposureInputParam;
    HDRExpSettingOutputParam_T mExposureOutputParam;

    // HDRProc parameters
    HDRProc_ShotParam  mShotParam;

    // complete callback
    mutable Mutex mCompleteCBLock;
    HDRProcCompleteCallback_t mCompleteCB;
    MVOID*                    mpCompleteCBUser;

    // capture policy & priority
    int mCapturePolicy;
    int mCapturePriority;

    // indicates whether a cancelling capture event has been sent
    MBOOL mforceBreak;

    // used for debug purpose
    // a serial number for file saving
    static MINT32 sSequenceNumber;

    // option of debug mode
    MINT32 mDebugMode;

    // getCaptureExposureSettings() is used to
    // get HDR output exposure settings from HDR custom API - getHDRExpSetting()
    MBOOL getCaptureExposureSettings(
            HDRExpSettingOutputParam_T& exposureOutputParam);

    // init() is used to create HDR HAL
    MBOOL init();

    // uninit() is used to destroy HDR HAL
    MBOOL uninit();

    // allocateProcessMemory() is used to allocate processing memory asynchronously
    MBOOL allocateProcessMemory();

    // the start routine of HDR post-processing
    static  MVOID hdrProcess(MVOID* arg);

    // the start routine of allocating HDR working buffer
    static  MVOID allocateProcessMemoryTask(MVOID* arg);

    MBOOL   ImageRegistratoin();
    MBOOL   WeightingMapGeneration();
    MBOOL   Blending();
    MBOOL   writeHDROutputFrame();

    MBOOL   createSEImg();

    // do small image normalization
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_Normalization(unsigned int method);

    // do SE to get GMV
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_SE();

    // do Feature Extraction
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_FeatureExtraction();

    // do Alignment
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_Alignment();

    // set the resulting weighting map to HDR HAL
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_SetBmapBuffer();

    // get original Weighting map
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_OriWeightMapGet();

    // do down-scaled weighting map
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_DownScaleWeightMap();

    // do up-scaled weighting map
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_UpScaleWeightMap();

    // do fusion
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_Fusion();

    // get HDR result from HDR HAL
    // return MTRUE if success; otherwise MFALSE is returned
    MBOOL   do_HdrCroppedResultGet();

    MBOOL   releaseSourceImgBufLocked(MUINT32 index);
    MBOOL   releaseSmallImgBufLocked(MUINT32 index);
    MBOOL   releaseSEImgBufLocked();

    MUINT32 getHdrWorkingbufferSize();
    MBOOL   releaseHdrWorkingBufLocked();

    MBOOL   requestOriWeightMapBuf();
    MBOOL   releaseOriWeightMapBufLocked();

    MBOOL   requestBlurredWeightMapBuf();
    MBOOL   releaseBlurredWeightMapBufLocked();

    MBOOL   requestDownSizedWeightMapBuf();
    MBOOL   releaseDownSizedWeightMapBufLocked();

    MBOOL   releaseBlendingBufLocked();

    MBOOL   requestImageBuffer(HDRBufferType type);
    MBOOL   releaseImageBuffer(HDRBufferType type);

    MBOOL CDPResize(
            IImageBuffer* pInputBuf,
            IImageBuffer* pOutputBuf,
            MUINT32 transform = 0);

    MBOOL CDPResize_simple(
            IImageBuffer* pInputBuf,
            IImageBuffer* pOutputBuf,
            MUINT32 transform = 0);

    // used for debug purpose
    static int dumpToFile(
            char const* fileName, unsigned char *vaddr, size_t size);
};

} // namespace NSCam

#endif // _HDR_H_
