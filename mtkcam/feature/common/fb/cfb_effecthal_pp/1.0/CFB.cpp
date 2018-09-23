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
#define LOG_TAG "MtkCam/featureio_CFBShot"
#include <mtkcam/utils/std/Log.h>
//
#include "CFB.h"
#include "../NormalShot/NormalShot.h"
#include <mtkcam/feature/FaceDetection/fd_hal_base.h>
#include <mtkcam/utils/exif/IBaseCamExif.h>
#include <mtkcam/utils/exif/StdExif.h>
#include <sys/stat.h>
#include <mtkcam/utils/fwk/MtkCamera.h>
#include "camera_custom_fb.h"
#include <linux/mt_sched.h>
#include <cutils/properties.h>  // For property_get().
#include <utils/threads.h>
#include <queue>
////#include <camnode/ICamGraphNode.h>
#include <hardware/camera3.h>

#include <feature/common/utils/BufAllocUtil.h>
//

//----vfb------------
//#include "MTKFeatureSet.h"

#define SCHED_POLICY       (SCHED_OTHER)
#define SCHED_PRIORITY     (1)
#define FULL_IMAGE_MAX_WIDTH            3616
#define FULL_IMAGE_MAX_HEIGHT           2160
#define VFB_DS_IMAGE_MAX_WIDTH      640//320
#define VFB_DS_IMAGE_MAX_HEIGHT     480//180

//----vfb------------

//using namespace NS3A;

using namespace NSCam;
using namespace NSIoPipe;

using namespace std;

#define SmallFaceWidthThreshold 40
#define BigFaceWidthThreshold 60
#define FD_WORKING_BUF_SIZE 5242880 //5M: 1024*1024*5
Mhal_facebeauty*     mpFbObj;
#define ENABLE_DEBUG_INFO     (0)

#define NAME "Facebeauty"

//#define Debug_Mode

//Thread
static MVOID* FBCapture(void *arg);
static MVOID* FBUtility(void *arg);
static MVOID* CaptureFaceBeautySDK_capture(void *arg);
static pthread_t threadFB;
static pthread_t threadUtility;
static pthread_t threadFBSDK;
static sem_t semFBthread;
static sem_t semFBthread2;
static sem_t semFBSDKthread;
static sem_t semFBSDKFBDONE;
static sem_t semMemoryDone;
static sem_t semUtilitythread;
static queue<int> qUtilityStatus;
static sem_t semJPGDone;

FACE_BEAUTY_SDK_HAL_PARAMS sdkParams;
SDK_FACE_BEAUTY_POS mSDKFacePos[15] = {0};
int facenumber = 0;
MBOOL isSDKfbdone = MFALSE;
IImageBuffer const*  TempSrcImgBuffer = NULL;
IImageBuffer const*  TempDstImgBuffer = NULL;
FaceBeautyEffectHal* SDKeffecthalobject = NULL;

MBOOL SaveJpg();

typedef struct FACE_BEAUTY_SHOT_INFO {
    int32_t iSmoothLevel;
    int32_t iSkinColor;
    int32_t iEnlargeEyeLevel;
    int32_t iSlimFaceLevel;
    int32_t iExtremeBeauty;
    Vector<Mhal_facebeauty::FACE_BEAUTY_POS> iFacePos;
};

MVOID* CaptureFaceBeautySDK_capture(void *arg)
{
    CAM_LOGD("[%s] +", __FUNCTION__);

    sem_wait(&semFBSDKthread);

    if(TempSrcImgBuffer == NULL || TempDstImgBuffer == NULL)
    {
        CAM_LOGD("[%s]TempSrcImgBuffer is Null",__FUNCTION__);
        return NULL;
    }

    int LS = 0;
    int LC = 0;
    int LE = 0;
    int LF = 0;
    sp<Mhal_facebeauty>  pImpShot = NULL;
    CAM_LOGD("[CaptureFaceBeautySDK_capture] new FBShot");
    pImpShot = new Mhal_facebeauty(TempSrcImgBuffer, TempDstImgBuffer);
    if  ( pImpShot == 0 ) {
        CAM_LOGE("[%s] new FBShot \n", __FUNCTION__);
        goto lbExit;
    }

    //
    //  (1.2) initialize Implementator if needed.
    {
        MtkCameraFaceMetadata FaceInfo;
        Vector<Mhal_facebeauty::FACE_BEAUTY_POS> FacePos;
        FaceInfo.number_of_faces = 0;
        if  ( ! pImpShot->onCreate(&FaceInfo, FacePos) ) {
            CAM_LOGE("[%s] FBShot onCreate() \n", __FUNCTION__);
            goto lbExit;
        }
    }

    //
    //(3)  tuning parameter
    {
        FB_Customize_PARA_STRUCT custData;
        get_FB_CustomizeData(&custData);

        LS = sdkParams.SmoothLevel+13;
        LC = sdkParams.SkinColor+13;
        LE = sdkParams.EnlargeEyeLevel+13;
        LF = sdkParams.SlimFaceLevel+13;
        pImpShot->mSmoothLevel = LS;
        pImpShot->mBrightLevel = LC;
        pImpShot->mRuddyLevel = custData.ruddy_level;
        pImpShot->mEnlargeEyeLevel = LE;
        pImpShot->mSlimFaceLevel = LF;
        pImpShot->mContrastLevel = 5;
        pImpShot->mExtremeBeauty = false;
    }

    //
    if  ( ! pImpShot->apply() ) {
        CAM_LOGE("[%s] FBShot apply() \n", __FUNCTION__);
        goto lbExit;
    }

    isSDKfbdone = MTRUE;
    sem_post(&semFBSDKFBDONE);


lbExit:

    pImpShot->onDestroy();

    SDKeffecthalobject->FBSDKCbFunc();

    CAM_LOGD("[%s] -", __FUNCTION__);

    return NULL;
}

void
SDK_facebeauty::
init(void)
{
    CAM_LOGD("[%s] +", __FUNCTION__);
    isSDKfbdone = MFALSE;
    memset(&sdkParams, 0, sizeof(FACE_BEAUTY_SDK_HAL_PARAMS));
    sem_init(&semFBSDKthread, 0, 0);
    sem_init(&semFBSDKFBDONE, 0, 0);
    pthread_create(&threadFBSDK, NULL, CaptureFaceBeautySDK_capture, NULL);
    pthread_setname_np(threadFBSDK, "FBSDK");
    CAM_LOGD("[%s] -", __FUNCTION__);
}

void
SDK_facebeauty::
uninit(void)
{
    CAM_LOGD("[%s] +", __FUNCTION__);

    if(isSDKfbdone)
    {
        CAM_LOGD("[%s] CFB done", __FUNCTION__);
        isSDKfbdone = MFALSE;
    } else {
        CAM_LOGD("[%s] wait for CFB done", __FUNCTION__);
        sem_wait(&semFBSDKFBDONE);
    }

    sem_post(&semFBSDKthread);
    pthread_join(threadFBSDK, NULL);

    TempSrcImgBuffer = NULL;
    TempDstImgBuffer = NULL;
    SDKeffecthalobject = NULL;
    CAM_LOGD("[%s] -", __FUNCTION__);
}

void
SDK_facebeauty::
SetCallBack(FaceBeautyEffectHal* effecthalobject)
{
    CAM_LOGD("[%s] +", __FUNCTION__);
    SDKeffecthalobject = effecthalobject;
    CAM_LOGD("[%s] -", __FUNCTION__);
}


/*******************************************************************************
*
*******************************************************************************/
MBOOL
SDK_facebeauty::
CaptureFaceBeauty_apply(
        IImageBuffer const* SrcImgBuffer,
        IImageBuffer const* DstImgBuffer,
        FACE_BEAUTY_SDK_HAL_PARAMS const* pParam,
        Vector<SDK_FACE_BEAUTY_POS> const& FacePos,
        EImageFormat const ImageFormat)
{
    CAM_LOGD("[%s] + ls(%d) lc(%d) le(%d) lf(%d) rotation(%d) JPGQuality(%d), face number = %d", __FUNCTION__,pParam->SmoothLevel,pParam->SkinColor
              ,pParam->EnlargeEyeLevel,pParam->SlimFaceLevel,pParam->Rotation,pParam->JPGquality,FacePos.size());

    sdkParams.SmoothLevel      = pParam->SmoothLevel;
    sdkParams.SkinColor        = pParam->SkinColor;
    sdkParams.EnlargeEyeLevel  = pParam->EnlargeEyeLevel;
    sdkParams.SlimFaceLevel    = pParam->SlimFaceLevel;
    sdkParams.Rotation         = pParam->Rotation;
    sdkParams.JPGquality       = pParam->JPGquality;

    TempSrcImgBuffer = SrcImgBuffer;
    TempDstImgBuffer = DstImgBuffer;

    if(FacePos.size() != 0)
    {
        int i=0;
        facenumber = FacePos.size();
        for (Vector<SDK_FACE_BEAUTY_POS>::const_iterator it = FacePos.begin();
            it != FacePos.end();
            it++)
        {
            CAM_LOGD("[%s] face position @[%d,%d,%d,%d]", __FUNCTION__, it->left,it->top,it->right,it->button);
            mSDKFacePos[i].left   = it->left;
            mSDKFacePos[i].top    = it->top;
            mSDKFacePos[i].right  = it->right;
            mSDKFacePos[i].button = it->button;
            i++;
        }
    }

    sem_post(&semFBSDKthread);

    CAM_LOGD("[%s] -", __FUNCTION__);

    return MTRUE;

}

/*******************************************************************************
*
*******************************************************************************/
Mhal_facebeauty::
Mhal_facebeauty(IImageBuffer const* SrcImgBuffer, IImageBuffer const* DstImgBuffer)
    : mSrcImgBuffer(SrcImgBuffer),
      mDstImgBuffer(DstImgBuffer),
      mSDKMode(MTRUE),
      mCancel(MFALSE)
{
}

/*******************************************************************************
*
*******************************************************************************/
bool
Mhal_facebeauty::
onCreate(MtkCameraFaceMetadata* FaceInfo, Vector<FACE_BEAUTY_POS> const& FacePos)
{
    MBOOL   ret = MFALSE;
    MINT32  ec = 0;
    CAM_LOGD("[facebeauty init] FBFaceInfo adr 0x%p FBFaceInfo num %d \n",FaceInfo->faces,FaceInfo->number_of_faces);
    FBmetadata.faces=(MtkCameraFace *)FBFaceInfo;
    FBmetadata.posInfo=(MtkFaceInfo *)MTKPoseInfo;
    FBmetadata.number_of_faces = FaceInfo->number_of_faces;

    for(int i=0;i<FaceInfo->number_of_faces;i++)
    {
        FBmetadata.faces[i].rect[0] = FaceInfo->faces[i].rect[0];
        FBmetadata.faces[i].rect[1] = FaceInfo->faces[i].rect[1];
        FBmetadata.faces[i].rect[2] = FaceInfo->faces[i].rect[2];
        FBmetadata.faces[i].rect[3] = FaceInfo->faces[i].rect[3];
        FBmetadata.posInfo[i].rop_dir = FaceInfo->posInfo[i].rop_dir;
        FBmetadata.posInfo[i].rip_dir = FaceInfo->posInfo[i].rip_dir;
    }

    mvfb_FBmetadata.faces=(MtkCameraFace *)mvfb_FDFaceInfo;
    mvfb_FBmetadata.posInfo=(MtkFaceInfo *)mvfb_FDPoseInfo;
    mvfb_FBmetadata.number_of_faces = FBmetadata.number_of_faces;

    for (Vector<FACE_BEAUTY_POS>::const_iterator it = FacePos.begin();
            it != FacePos.end();
            it++)
    {
        CAM_LOGD("[facebeauty onCreate] beautified face @[%d,%d]", it->x, it->y);
        mBeautifiedFacePos.push_back(*it);
    }

#if 0
    mpIMemDrv =  IMemDrv::createInstance();
    if (mpIMemDrv == NULL)
    {
        CAM_LOGE("g_pIMemDrv is NULL \n");
        return 0;
    }

    mpIImageBufAllocator =  IImageBufferAllocator::getInstance();
    if (mpIImageBufAllocator == NULL)
    {
        CAM_LOGE("mpIImageBufAllocator is NULL \n");
        return 0;
    }

    mpIMemDrv->init(NAME);  //check this, see fd
    //mpIMemDrv->init();    //check this, see fd
    CAM_LOGD("[facebeauty onCreate] init MemDrv - check FD---- \n");
#endif

    mpFb = halFACEBEAUTIFYBase::createInstance(HAL_FACEBEAUTY_OBJ_SW);

    if  ( ! mpFb )
    {
        CAM_LOGE("[init] NULL mpFb \n");
        goto lbExit;
    }
    mpFbObj = this;
    mpFb->CANCEL = MFALSE;
    ret = MTRUE;

    //
    mDebugInfo = IDbgInfoContainer::createInstance();

lbExit:
    if  ( ! ret )
    {
        onDestroy();
    }
    CAM_LOGD("[init] rc(%d) \n", ret);
    return  ret;
}


/*******************************************************************************
*
*******************************************************************************/
void
Mhal_facebeauty::
onDestroy()
{
    CAM_LOGD("[uninit] in");

    Mutex::Autolock autoLock(mUninitMtx);

    if  (mpFb)
    {
        mpFb->mHalFacebeautifyUninit();
        mpFb->destroyInstance();
        mpFb = NULL;
    }
    mu4W_yuv = 0;
    mu4H_yuv = 0;
#if 0
    if (mvImgBufMap.size() != 0)
    {
        CAM_LOGE("ImageBuffer leakage here!");
        for (Vector<ImageBufferMap>::iterator it = mvImgBufMap.begin();
                it != mvImgBufMap.end();
                it++)
        {
            CAM_LOGE("Freeing memID(0x%x),virtAddr(0x%x)!", it->memBuf.memID, it->memBuf.virtAddr);
            if (mpIMemDrv->unmapPhyAddr(&it->memBuf)) {
                CAM_LOGE("mpIMemDrv->unmapPhyAddr() error");
            }
            if (mpIMemDrv->freeVirtBuf(NAME, &it->memBuf)) {
                CAM_LOGE("mpIMemDrv->freeVirtBuf() error");
            }
            //mpIMemDrv->freeVirtBuf(&it->memBuf);
        }
    }
    if  (mpIMemDrv)
    {
        mpIMemDrv->uninit(NAME);
        //mpIMemDrv->uninit();
        mpIMemDrv->destroyInstance();
        mpIMemDrv = NULL;
    }
#endif

    //
    if(mDebugInfo != NULL) {
        mDebugInfo->destroyInstance();
    }

    CAM_LOGD("[uninit] out");
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
onCmd_capture()
{
    Mutex::Autolock autoLock(mUninitMtx);

    MBOOL   ret = MFALSE;

#if 0
    //get sensor info
    {
        mSensorType = 0;
        NSCam::SensorStaticInfo rSensorInfo;
        querySensorInfo(rSensorInfo);
        mSensorType = rSensorInfo.sensorType;
        CAM_LOGD("RAW sensor(CFB): sensor type = %d",mSensorType);// raw->1; yuv->2
    }
#endif

    sem_init(&semMemoryDone, 0, 0);
    sem_init(&semFBthread, 0, 0);
    sem_init(&semFBthread2, 0, 0);

    sem_init(&semUtilitythread, 0, 0);
    pthread_create(&threadUtility, NULL, FBUtility, NULL);
    pthread_setname_np(threadUtility, "FBUtility");

    pthread_attr_t attr = {0, NULL, 1024 * 1024, 4096, SCHED_OTHER, ANDROID_PRIORITY_FOREGROUND};
    pthread_create(&threadFB, &attr, FBCapture, NULL);
    pthread_setname_np(threadFB, "FBCapture");

    sem_wait(&semFBthread);
    sem_wait(&semFBthread2);

#ifndef Debug_Mode
    char EnableOption[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("camera.facebeauty.dump", EnableOption, "0");
    if (EnableOption[0] == '1')
#endif
    {
        char szFileName[100];
        ::sprintf(szFileName, "/sdcard/fbimg_%dx%d.yuv", mpSource->getImgSize().w, mpSource->getImgSize().h);
        mpSource->saveToFile(szFileName);
    }

    //------------------ Sava test ----------------//
    #ifdef BanchMark
    char szFileName[100];
    MUINT32 FDInfo[100]={0};
    MUINT32* htable=(MUINT32*)msFaceBeautyResultInfo.PCAHTable;
    int i=0;
    for(i=0;i<FBmetadata.number_of_faces;i++)
    {
       FDInfo[i*4]   = FBmetadata.faces[i].rect[0];
       FDInfo[i*4+1] = FBmetadata.faces[i].rect[1];
       FDInfo[i*4+2] = FBmetadata.faces[i].rect[2]-FBmetadata.faces[i].rect[0];
       FDInfo[i*4+3] = MTKPoseInfo[i].rip_dir;
       CAM_LOGI("[FACEINFO] x %d y %d w %d",FBmetadata.faces[i].rect[0],FBmetadata.faces[i].rect[1],FBmetadata.faces[i].rect[2]);
    }
    ::sprintf(szFileName, "/sdcard/DCIM/Camera/%s_H_%d_%d.txt", "FDinfo", *htable,capturecount);
    saveBufToFile(szFileName, (MUINT8*)&FDInfo, 100 * 4);
    CAM_LOGI("[FACEINFO] Save File done");
    #endif
    //------------------ Sava test ----------------//

    //  Force to handle done even if there is any error before.
    //to do handleCaptureDone();

    ret = MTRUE;
lbExit:
    releaseBufs();
    pthread_join(threadFB, NULL);
    qUtilityStatus.push(0);
    sem_post(&semUtilitythread);
    pthread_join(threadUtility, NULL);

#if (FB_PROFILE_CAPTURE)
    DbgTmr.print("FBProfiling:: Done");
#endif
    return  ret;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
apply()
{
    MBOOL ret = MFALSE;

    sem_init(&semMemoryDone, 0, 0);
    sem_init(&semFBthread, 0, 0);
    sem_init(&semFBthread2, 0, 0);

    sem_init(&semUtilitythread, 0, 0);
    pthread_create(&threadUtility, NULL, FBUtility, NULL);
    pthread_setname_np(threadUtility, "FBSDKUtility");

    pthread_create(&threadFB, NULL, FBCapture, NULL);
    pthread_setname_np(threadFB, "FBSDKCapture");

    sem_wait(&semFBthread);
    sem_wait(&semFBthread2);

    MUINT32 transform = 0;//(mSrcImgBuffer->getImgSize().w > mSrcImgBuffer->getImgSize().h)? 0: NSCam::eTransform_ROT_90;
    ImgProcess(mpSource, 0, 0, eImgFmt_I422, mDstImgBuffer, 0, 0, eImgFmt_YV12, transform);

    ret = MTRUE;
lbExit:
    releaseBufs();
    pthread_join(threadFB, NULL);
    qUtilityStatus.push(0);
    sem_post(&semUtilitythread);
    pthread_join(threadUtility, NULL);
#if (FB_PROFILE_CAPTURE)
    DbgTmr.print("FBProfiling:: Done");
#endif
    return  ret;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
onCmd_reset()
{
    CAM_LOGD("[onCmd_reset] in");
    MBOOL   ret = MTRUE;
    mpFb->CANCEL = MFALSE;
    mCancel = MFALSE;
    //ret = releaseBufs();
    CAM_LOGD("[onCmd_reset] out");
    return  ret;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
onCmd_cancel()
{
    CAM_LOGD("[onCmd_cancel] in");
    MBOOL   ret = MFALSE;
    //mpFb->CANCEL = MTRUE;
    mCancel = MTRUE;
    CancelAllSteps();
    CAM_LOGD("[onCmd_cancel] out");
    return  ret;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
Mhal_facebeauty::
sendCommand(
    uint32_t const  cmd,
    MUINTPTR const  arg1,
    uint32_t const  arg2,
    uint32_t const  arg3
)
{
    bool ret = true;
    //
#if 0
    switch  (cmd)
    {
    //  This command is to reset this class. After captures and then reset,
    //  performing a new capture should work well, no matter whether previous
    //  captures failed or not.
    //
    //  Arguments:
    //          N/A
    case eCmd_reset:
        ret = onCmd_reset();
        break;

    //  This command is to perform capture.
    //
    //  Arguments:
    //          N/A
    case eCmd_capture:
        ret = onCmd_capture();
        break;

    //  This command is to perform cancel capture.
    //
    //  Arguments:
    //          N/A
    case eCmd_cancel:
        onCmd_cancel();
        break;
    //
    default:
        ret = ImpShot::sendCommand(cmd, arg1, arg2, arg3);
    }
    //
#endif
    return ret;
}


/******************************************************************************
* save the buffer to the file
*******************************************************************************/
static bool
saveBufToFile(char const*const fname, MUINT8 *const buf, MUINT32 const size)
{
    int nw, cnt = 0;
    uint32_t written = 0;

    CAM_LOGD("(name, buf, size) = (%s, %x, %d)", fname, buf, size);
    CAM_LOGD("opening file [%s]\n", fname);
    int fd = ::open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (fd < 0) {
        CAM_LOGE("failed to create file [%s]: %s", fname, ::strerror(errno));
        return false;
    }

    CAM_LOGD("writing %d bytes to file [%s]\n", size, fname);
    while (written < size) {
        nw = ::write(fd,
                     buf + written,
                     size - written);
        if (nw < 0) {
            CAM_LOGE("failed to write to file [%s]: %s", fname, ::strerror(errno));
            break;
        }
        written += nw;
        cnt++;
    }
    CAM_LOGD("done writing %d bytes to file [%s] in %d passes\n", size, fname, cnt);
    ::close(fd);
    return true;
}
#ifdef Debug_Mode
/******************************************************************************
*   read the file to the buffer
*******************************************************************************/
static uint32_t
loadFileToBuf(char const*const fname, uint8_t*const buf, uint32_t size)
{
    int nr, cnt = 0;
    uint32_t readCnt = 0;

    CAM_LOGD("opening file [%s] adr 0x%x\n", fname,buf);
    int fd = ::open(fname, O_RDONLY);
    if (fd < 0) {
        CAM_LOGE("failed to create file [%s]: %s", fname, strerror(errno));
        return readCnt;
    }
    //
    if (size == 0) {
        size = ::lseek(fd, 0, SEEK_END);
        ::lseek(fd, 0, SEEK_SET);
    }
    //
    CAM_LOGD("read %d bytes from file [%s]\n", size, fname);
    while (readCnt < size) {
        nr = ::read(fd,
                    buf + readCnt,
                    size - readCnt);
        if (nr < 0) {
            CAM_LOGE("failed to read from file [%s]: %s",
                        fname, strerror(errno));
            break;
        }
        readCnt += nr;
        cnt++;
    }
    CAM_LOGD("done reading %d bytes to file [%s] in %d passes\n", size, fname, cnt);
    ::close(fd);

    return readCnt;
}
#endif

/******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
fgCamShotNotifyCb(MVOID* user, CamShotNotifyInfo const msg)
{
#if 0
    CAM_LOGD("[fgCamShotNotifyCb] + ");
    Mhal_facebeauty *pFBlShot = reinterpret_cast <Mhal_facebeauty *>(user);
    if (NULL != pFBlShot)
    {
        CAM_LOGD("[fgCamShotNotifyCb] call back type %d",msg.msgType);
        if (NSCamShot::ECamShot_NOTIFY_MSG_EOF == msg.msgType)
        {
            pFBlShot->mpShotCallback->onCB_Shutter(true, 0);
            CAM_LOGD("[fgCamShotNotifyCb] call back done");
        }
    }
    CAM_LOGD("[fgCamShotNotifyCb] -");
#endif
    return MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
handleYuvDataCallback(MUINT8* const puBuf, MUINT32 const u4Size)
{
    CAM_LOGD("[handleYuvDataCallback] + (puBuf, size) = (%p, %d)", puBuf, u4Size);

    #ifdef Debug_Mode
    saveBufToFile("/sdcard/yuv.yuv", puBuf, u4Size);
    #endif

    return 0;
}


/******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
handlePostViewData(MUINT8* const puBuf, MUINT32 const u4Size)
{
#if 0
    CAM_LOGD("[handlePostViewData] + (puBuf, size) = (%p, %d)", puBuf, u4Size);
    mpShotCallback->onCB_PostviewDisplay(0,
                                         u4Size,
                                         reinterpret_cast<uint8_t const*>(puBuf)
                                        );

    CAM_LOGD("[handlePostViewData] -");
#endif
    return  MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
handleJpegData(MUINT8* const puJpegBuf, MUINT32 const u4JpegSize, MUINT8* const puThumbBuf, MUINT32 const u4ThumbSize, MUINT32 const Mode)
{
#if 0
    CAM_LOGD("[handleJpegData] + (puJpgBuf, jpgSize, puThumbBuf, thumbSize, mode ) = (%p, %d, %p, %d, %d)", puJpegBuf, u4JpegSize, puThumbBuf, u4ThumbSize, Mode);

    MUINT8 *puExifHeaderBuf = new MUINT8[DBG_EXIF_SIZE];
    MUINT32 u4ExifHeaderSize = 0;
    makeExifHeader(eAppMode_PhotoMode, puThumbBuf, u4ThumbSize, puExifHeaderBuf, u4ExifHeaderSize, mDebugInfo);
    CAM_LOGD("[handleJpegData] (thumbbuf, size, exifHeaderBuf, size) = (%p, %d, %p, %d)",
                      puThumbBuf, u4ThumbSize, puExifHeaderBuf, u4ExifHeaderSize);
    // Jpeg callback
    if(Mode)
    {
        mpShotCallback->onCB_CompressedImage(0,
                                         u4JpegSize,
                                         reinterpret_cast<uint8_t const*>(puJpegBuf),
                                         u4ExifHeaderSize,          //header size
                                         puExifHeaderBuf,           //header buf
                                         0,                         //callback index
#ifndef MTK_CAM_VIDEO_FACEBEAUTY_SUPPORT
                                         false,                     //final image
#else
                                         true,                      //final image
#endif
                                         MTK_CAMERA_MSG_EXT_DATA_FACEBEAUTY
                                         );
    }
    else
    {
        mpShotCallback->onCB_CompressedImage(0,
                                         u4JpegSize,
                                         reinterpret_cast<uint8_t const*>(puJpegBuf),
                                         u4ExifHeaderSize,                       //header size
                                         puExifHeaderBuf,                    //header buf
                                         0,                       //callback index
                                         true                     //final image
                                         );
    }
    CAM_LOGD("[handleJpegData] -");

    delete [] puExifHeaderBuf;
#endif
    return MTRUE;

}


/******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
fgCamShotDataCb(MVOID* user, CamShotDataInfo const msg)
{
    Mhal_facebeauty *pFBlShot = reinterpret_cast<Mhal_facebeauty *>(user);
    CAM_LOGD("[fgCamShotDataCb] type %d +" ,msg.msgType);
    if (NULL != pFBlShot)
    {
        if (NSCamShot::ECamShot_DATA_MSG_POSTVIEW == msg.msgType)
        {
            pFBlShot->handlePostViewData( (MUINT8*) msg.pBuffer->getBufVA(0), msg.pBuffer->getBufSizeInBytes(0));
        }
        else if (NSCamShot::ECamShot_DATA_MSG_JPEG == msg.msgType)
        {
            pFBlShot->handleJpegData((MUINT8*) msg.pBuffer->getBufVA(0), msg.pBuffer->getBufSizeInBytes(0), reinterpret_cast<MUINT8*>(msg.ext1), msg.ext2,1);
        }
        else if (NSCamShot::ECamShot_DATA_MSG_YUV == msg.msgType)
        {
            pFBlShot->handleYuvDataCallback((MUINT8*) msg.pBuffer->getBufVA(0), msg.pBuffer->getBufSizeInBytes(0));//FIXME
            {   //dbginfo
                IDbgInfoContainer* pDbgInfo = reinterpret_cast<IDbgInfoContainer*>(msg.ext2);
                Mhal_facebeauty *self = reinterpret_cast<Mhal_facebeauty *>(user);
                if (self == NULL)
                {
                    CAM_LOGE("[fgCamShotDataCb] user is NULL");
                    return MFALSE;
                }
                pDbgInfo->copyTo(self->mDebugInfo);
            }
        }
    }
    CAM_LOGD("[fgCamShotDataCb] -" );
    return MTRUE;
}

MBOOL
Mhal_facebeauty::
postionFacesInImage(int u4PostviewWidth, int u4PostviewHeight)
{
    MINT32  g_BufWidth;
    MINT32  g_BufHeight;
    if((u4PostviewWidth*3) == (u4PostviewHeight*4))
    {
        g_BufWidth = 320;
        g_BufHeight = 240;
    }
    else if((u4PostviewWidth*9) == (u4PostviewHeight*16))
    {
        g_BufWidth = 320;
        g_BufHeight = 180;
    }
    else if((u4PostviewWidth*3) == (u4PostviewHeight*5))
    {
        g_BufWidth = 320;
        g_BufHeight = 192;
    }
    else
    {
        g_BufWidth = 320;
        if(u4PostviewWidth != 0)
          g_BufHeight = 320 * u4PostviewHeight/u4PostviewWidth;
        else
          g_BufHeight = 180;
    }

    if(FDWidth != u4PostviewWidth && FDHeight != u4PostviewHeight){
        g_BufWidth  = FDWidth;
        g_BufHeight = FDHeight;
    }

    CAM_LOGD("[doFaceDetection] Postview %dx%d -> Buf %dx%d\n",u4PostviewWidth, u4PostviewHeight, g_BufWidth, g_BufHeight);

    for(int i=0;i<FBmetadata.number_of_faces;i++)
    {
        FBmetadata.faces[i].rect[0] = ((FBmetadata.faces[i].rect[0] + 1000) * g_BufWidth) / 2000;
        FBmetadata.faces[i].rect[1] = ((FBmetadata.faces[i].rect[1] + 1000) * g_BufHeight) / 2000;
        FBmetadata.faces[i].rect[2] = ((FBmetadata.faces[i].rect[2] + 1000) * g_BufWidth) / 2000;
        FBmetadata.faces[i].rect[3] = ((FBmetadata.faces[i].rect[3] + 1000) * g_BufHeight) / 2000;

        int face_size = FBmetadata.faces[i].rect[2] - FBmetadata.faces[i].rect[0];
        if(face_size >= 30)
        {
              int zoom_size;
            zoom_size = face_size/15;

            if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                  (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                  (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                  (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
            {
                 zoom_size = face_size/12;
                     if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                       (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                       (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                       (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                   {
                        zoom_size = face_size/10;
                          if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                            (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                            (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                            (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                       {
                            zoom_size = face_size/8;
                              if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                                (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                                (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                                (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                            {
                                   zoom_size = face_size/7;
                                   if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                                     (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                                     (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                                     (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                                 {
                                       ;
                                 }
                                 else
                                 {
                                         zoom_size = face_size/8;
                                 }
                            }
                            else
                            {
                                    zoom_size = face_size/10;
                            }
                       }
                       else
                       {
                          zoom_size = face_size/12;
                       }
                   }
                   else
                   {
                       zoom_size = face_size/15;
                   }
                 FBmetadata.faces[i].rect[0] -= zoom_size;
                 FBmetadata.faces[i].rect[1] -= zoom_size;
                 FBmetadata.faces[i].rect[2] += zoom_size;
                 FBmetadata.faces[i].rect[3] += zoom_size;
            }



        }
        mvfb_FBmetadata.faces[i].rect[0] = FBmetadata.faces[i].rect[0];
        mvfb_FBmetadata.faces[i].rect[1] = FBmetadata.faces[i].rect[1];
        mvfb_FBmetadata.faces[i].rect[2] = FBmetadata.faces[i].rect[2];
        mvfb_FBmetadata.faces[i].rect[3] = FBmetadata.faces[i].rect[3];
        mvfb_FBmetadata.posInfo[i].rop_dir = FBmetadata.posInfo[i].rop_dir;
        mvfb_FBmetadata.posInfo[i].rip_dir = FBmetadata.posInfo[i].rip_dir;
        mvfb_FBmetadata.number_of_faces = FBmetadata.number_of_faces;

        CAM_LOGI("ZSD: [doFaceDetection] After FBFaceInfo num %d left %d top %d right %d button %d pose %d \n",i,FBmetadata.faces[i].rect[0],FBmetadata.faces[i].rect[1],FBmetadata.faces[i].rect[2],FBmetadata.faces[i].rect[3],MTKPoseInfo[i].rip_dir);
    }

    return MTRUE;

}


MBOOL
Mhal_facebeauty::
doFaceDetection(IImageBuffer* Srcbufinfo, int u4SrcWidth, int u4SrcHeight, int u4PostviewWidth, int u4PostviewHeight, MBOOL bSkipDetect)
{
    int error;
    if(mSDKMode)
        bSkipDetect = 0;

    if (!bSkipDetect)
    {
        //Srcbufinfo->saveToFile("/sdcard/cfb_fd_in.yuv");
        CAM_LOGE("[doFaceDetection] u4SrcWidth = %d u4SrcHeight = %d",u4SrcWidth,u4SrcHeight);

        halFDBase*   mpFDHalObj = halFDBase::createInstance(HAL_FD_OBJ_HW);
        if (mpFDHalObj == NULL)
        {
            CAM_LOGE("[doFaceDetection] can't get halFDBase instance.");
            return MFALSE;
        }
        error = mpFDHalObj->halFDInit(u4SrcWidth, u4SrcHeight, mpFDWorkingBuffer, FD_WORKING_BUF_SIZE, 1, 3); //3:enhanced for CFB
        MUINT8* qVGABuffer = new unsigned char[640*480*2];
        MINT32 numFace;
        MUINT8 count = 0;
        MtkCameraFace doFDFaceInfo[15];
        MtkFaceInfo doFDPoseInfo[15];
        MtkCameraFaceMetadata  doFDFaceMetadata;
        doFDFaceMetadata.faces=(MtkCameraFace *)doFDFaceInfo;
        doFDFaceMetadata.posInfo=(MtkFaceInfo *)doFDPoseInfo;
        MINT32 rotation = (mTransform == 0)? 0:
                                ((mTransform == NSCam::eTransform_ROT_90)? 90:
                                ((mTransform == NSCam::eTransform_ROT_180)? 180: 270));
        if(mSDKMode)
        {
            rotation = sdkParams.Rotation;
            CAM_LOGE("[doFaceDetection]SDK: rotaton = %d",rotation);
        }
        //Srcbufinfo->syncCache(eCACHECTRL_FLUSH);
        do {
            CAM_LOGI("[doFaceDetection] Face detection try %d", count + 1);

            if(error == FD_INIT_NO_ERROR)
            {
                CAM_LOGE("[doFaceDetection] do FD rotation = %d",rotation);
                struct FD_Frame_Parameters Param;
                Param.pScaleImages = NULL;
                Param.pRGB565Image = (MUINT8 *)qVGABuffer;
                Param.pPureYImage  = (MUINT8 *)Srcbufinfo->getBufVA(0);
                Param.pImageBufferPhyP0 = (MUINT8 *)Srcbufinfo->getBufPA(0);
                Param.pImageBufferPhyP1 = NULL;
                Param.pImageBufferPhyP2 = NULL;
                Param.Rotation_Info = rotation;
                Param.SDEnable = false;
                Param.AEStable = 0;
                mpFDHalObj->halFDDo(Param);
            }

            numFace = mpFDHalObj->halFDGetFaceResult(&doFDFaceMetadata);
        } while ((numFace == 0) && (++count <= 12));

        delete qVGABuffer;

        if(error == FD_INIT_NO_ERROR)
        {
            CAM_LOGE("[doFaceDetection] do FD uninit");
        mpFDHalObj->halFDUninit();
        }

        mpFDHalObj->destroyInstance();

        if (numFace == 0)
        {
            CAM_LOGE("[doFaceDetection] No face is detected on captured image, use preview FD info");
        }
        else
        {
            if(mSDKMode)
            {
                CAM_LOGD("[doFaceDetection] SDK: %d faces are detected on captured image", facenumber);
                FBmetadata.number_of_faces = facenumber;
                for(int i=0;i<facenumber;i++)
                {
                    CAM_LOGD("[doFaceDetection] SDK: FDgetresult Found face index %d left %d top %d right %d button %d",(i+1),
                        doFDFaceMetadata.faces[i].rect[0],doFDFaceMetadata.faces[i].rect[1],doFDFaceMetadata.faces[i].rect[2],doFDFaceMetadata.faces[i].rect[3]);

                    FBmetadata.faces[i].rect[0] = mSDKFacePos[i].left;
                    FBmetadata.faces[i].rect[1] = mSDKFacePos[i].top;
                    FBmetadata.faces[i].rect[2] = mSDKFacePos[i].right;
                    FBmetadata.faces[i].rect[3] = mSDKFacePos[i].button;
                    FBmetadata.posInfo[i].rop_dir = doFDFaceMetadata.posInfo[i].rop_dir;
                    FBmetadata.posInfo[i].rip_dir = doFDFaceMetadata.posInfo[i].rip_dir;
                }
            } else {
            CAM_LOGI("[doFaceDetection] %d faces are detected on captured image", doFDFaceMetadata.number_of_faces);
            FBmetadata.number_of_faces = numFace;
            for(int i=0;i<numFace;i++)
            {
                    CAM_LOGI("[doFaceDetection] FDgetresult : Found face index %d left %d top %d right %d button %d",(i+1),
                        doFDFaceMetadata.faces[i].rect[0],doFDFaceMetadata.faces[i].rect[1],doFDFaceMetadata.faces[i].rect[2],doFDFaceMetadata.faces[i].rect[3]);

                FBmetadata.faces[i].rect[0] = doFDFaceMetadata.faces[i].rect[0];
                FBmetadata.faces[i].rect[1] = doFDFaceMetadata.faces[i].rect[1];
                FBmetadata.faces[i].rect[2] = doFDFaceMetadata.faces[i].rect[2];
                FBmetadata.faces[i].rect[3] = doFDFaceMetadata.faces[i].rect[3];
                FBmetadata.posInfo[i].rop_dir = doFDFaceMetadata.posInfo[i].rop_dir;
                FBmetadata.posInfo[i].rip_dir = doFDFaceMetadata.posInfo[i].rip_dir;
            }
        }
    }
    }

    MINT32  g_BufWidth;
    MINT32  g_BufHeight;
    if((u4PostviewWidth*3) == (u4PostviewHeight*4))
    {
        g_BufWidth = 320;
        g_BufHeight = 240;
    }
    else if((u4PostviewWidth*9) == (u4PostviewHeight*16))
    {
        g_BufWidth = 320;
        g_BufHeight = 180;
    }
    else if((u4PostviewWidth*3) == (u4PostviewHeight*5))
    {
        g_BufWidth = 320;
        g_BufHeight = 192;
    }
    else
    {
        g_BufWidth = 320;
        if(u4PostviewWidth != 0)
          g_BufHeight = 320 * u4PostviewHeight/u4PostviewWidth;
        else
          g_BufHeight = 180;
    }

    if(FDWidth != u4PostviewWidth && FDHeight != u4PostviewHeight){
        g_BufWidth  = FDWidth;
        g_BufHeight = FDHeight;
    }

    CAM_LOGD("[doFaceDetection] Postview %dx%d -> Buf %dx%d\n",u4PostviewWidth, u4PostviewHeight, g_BufWidth, g_BufHeight);

    for(int i=0;i<FBmetadata.number_of_faces;i++)
    {
        FBmetadata.faces[i].rect[0] = ((FBmetadata.faces[i].rect[0] + 1000) * g_BufWidth) / 2000;
        FBmetadata.faces[i].rect[1] = ((FBmetadata.faces[i].rect[1] + 1000) * g_BufHeight) / 2000;
        FBmetadata.faces[i].rect[2] = ((FBmetadata.faces[i].rect[2] + 1000) * g_BufWidth) / 2000;
        FBmetadata.faces[i].rect[3] = ((FBmetadata.faces[i].rect[3] + 1000) * g_BufHeight) / 2000;

        int face_size = FBmetadata.faces[i].rect[2] - FBmetadata.faces[i].rect[0];
        if(face_size >= 30)
        {
              int zoom_size;
            zoom_size = face_size/15;

            if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                  (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                  (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                  (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
            {
                 zoom_size = face_size/12;
                     if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                       (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                       (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                       (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                   {
                        zoom_size = face_size/10;
                          if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                            (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                            (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                            (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                       {
                            zoom_size = face_size/8;
                              if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                                (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                                (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                                (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                            {
                                   zoom_size = face_size/7;
                                   if( (FBmetadata.faces[i].rect[0] - zoom_size >= 0) &&
                                     (FBmetadata.faces[i].rect[1] - zoom_size >= 0) &&
                                     (FBmetadata.faces[i].rect[2] + zoom_size <= g_BufWidth -1) &&
                                     (FBmetadata.faces[i].rect[3] + zoom_size <= g_BufHeight-1))
                                 {
                                       ;
                                 }
                                 else
                                 {
                                         zoom_size = face_size/8;
                                 }
                            }
                            else
                            {
                                    zoom_size = face_size/10;
                            }
                       }
                       else
                       {
                          zoom_size = face_size/12;
                       }
                   }
                   else
                   {
                       zoom_size = face_size/15;
                   }
                 FBmetadata.faces[i].rect[0] -= zoom_size;
                 FBmetadata.faces[i].rect[1] -= zoom_size;
                 FBmetadata.faces[i].rect[2] += zoom_size;
                 FBmetadata.faces[i].rect[3] += zoom_size;
            }



        }
        mvfb_FBmetadata.faces[i].rect[0] = FBmetadata.faces[i].rect[0];
        mvfb_FBmetadata.faces[i].rect[1] = FBmetadata.faces[i].rect[1];
        mvfb_FBmetadata.faces[i].rect[2] = FBmetadata.faces[i].rect[2];
        mvfb_FBmetadata.faces[i].rect[3] = FBmetadata.faces[i].rect[3];
        mvfb_FBmetadata.posInfo[i].rop_dir = FBmetadata.posInfo[i].rop_dir;
        mvfb_FBmetadata.posInfo[i].rip_dir = FBmetadata.posInfo[i].rip_dir;
        mvfb_FBmetadata.number_of_faces = FBmetadata.number_of_faces;

        CAM_LOGI("[doFaceDetection] After FBFaceInfo num %d left %d top %d right %d button %d pose %d \n",i,FBmetadata.faces[i].rect[0],FBmetadata.faces[i].rect[1],FBmetadata.faces[i].rect[2],FBmetadata.faces[i].rect[3],MTKPoseInfo[i].rip_dir);
    }

    return MTRUE;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
createFullFrame(IImageBuffer* Srcbufinfo)
{
    /* SDK mode: copy source image to continuous memory */
    MUINT32 transform = 0; //(mSrcImgBuffer->getImgSize().w > mSrcImgBuffer->getImgSize().h)? 0: NSCam::eTransform_ROT_270;
    mTransform = sdkParams.Rotation;//(mSrcImgBuffer->getImgSize().w > mSrcImgBuffer->getImgSize().h)? 0: NSCam::eTransform_ROT_90;
    ImgProcess(mSrcImgBuffer, 0, 0, eImgFmt_YV12, Srcbufinfo, 0, 0, eImgFmt_YV12, transform);
    sem_wait(&semMemoryDone);

    return true;
#if 0
//#define _WORKAROUND_MULTI_PLANE
    MBOOL  ret = MTRUE;
#ifdef MTK_GMO_RAM_OPTIMIZE
    MBOOL  IsGMO = MFALSE;
#endif
    EImageFormat srctype;


    //CAM_TRACE_BEGIN("createFullFrame");

#ifdef MTK_GMO_RAM_OPTIMIZE
    IsGMO = MTRUE;
#endif

    if (!mSDKMode)
    {
        CAM_LOGD("[createFullFrame] + \n");
        MBOOL isMfbShot = MFALSE;
        NSCamShot::ICamShot *pSingleShot;
#ifdef MTK_GMO_RAM_OPTIMIZE
        if (mu4ShotMode != NSCam::eShotMode_FaceBeautyShotCc && IsGMO != MTRUE)
#else
        if (mu4ShotMode != NSCam::eShotMode_FaceBeautyShotCc)
#endif
        //if (mu4ShotMode != NSCam::eShotMode_FaceBeautyShotCc)
        {
            CAM_LOGD("[createFullFrame] use MFB shot \n");
            pSingleShot = NSCamShot::ISmartShot::createInstance(eShotMode_FaceBeautyShot
                                                                , "FaceBeautyshot"
                                                                , getOpenId()
                                                                , mShotParam.mu4MultiFrameBlending
                                                                , &isMfbShot
                                                                );
        }
        else
        {
            CAM_LOGD("[createFullFrame] use single shot \n");
            pSingleShot = NSCamShot::ISingleShot::createInstance(eShotMode_FaceBeautyShot
                                                                , "FaceBeautyshot");
        }

        MUINT32 nrtype = queryCapNRType( getCaptureIso(), isMfbShot);
        //
        pSingleShot->init();
        EImageFormat ePostViewFmt = static_cast<EImageFormat>(mShotParam.miPostviewDisplayFormat);
        CAM_LOGD("[createFullFrame] Postview format: (0x%x)\n", ePostViewFmt);

#ifndef _WORKAROUND_MULTI_PLANE
        pSingleShot->registerImageBuffer(ECamShot_BUF_TYPE_YUV, Srcbufinfo);
#else
        IImageBuffer* mptmp = allocMem(eImgFmt_YUY2, mShotParam.mi4PictureWidth, mShotParam.mi4PictureHeight);
        pSingleShot->registerImageBuffer(ECamShot_BUF_TYPE_YUV, mptmp);
#endif

        //

        pSingleShot->enableDataMsg(ECamShot_DATA_MSG_YUV
                //| ECamShot_DATA_MSG_JPEG
                );
        pSingleShot->enableNotifyMsg(NSCamShot::ECamShot_NOTIFY_MSG_EOF);

        if(mSensorType == SENSOR_TYPE_RAW && mSDKMode != MTRUE)
            srctype = eImgFmt_YV12;
        else
            srctype = eImgFmt_I422;

        // shot param
#ifndef _WORKAROUND_MULTI_PLANE
        NSCamShot::ShotParam rShotParam(srctype,         //yuv format
#else
        NSCamShot::ShotParam rShotParam(eImgFmt_YUY2,         //yuv format
#endif
                    mShotParam.mi4PictureWidth,      //picutre width
                    mShotParam.mi4PictureHeight,     //picture height
                    0,                               //transform
                    ePostViewFmt,
                    mShotParam.mi4PostviewWidth,      //postview width
                    mShotParam.mi4PostviewHeight,     //postview height
                    0,                                //transform
                    mShotParam.mu4ZoomRatio           //zoom
                    );

                // jpeg param
        NSCamShot::JpegParam rJpegParam(mJpegParam.mu4JpegQuality,       //Quality
                    MTRUE                            //isSOI
                    );


                // sensor param
        NSCamShot::SensorParam rSensorParam(
                getOpenId(),                             //sensor idx
                SENSOR_SCENARIO_ID_NORMAL_CAPTURE,       //Scenaio
                getRawBitDepth(),                        //bit depth
                MFALSE,                                  //bypass delay
                MFALSE                                   //bypass scenario
                );
        //
        pSingleShot->setCallbacks(fgCamShotNotifyCb, fgCamShotDataCb, this);
        //
        pSingleShot->setShotParam(rShotParam);

        //
        pSingleShot->setJpegParam(rJpegParam);

        //
        if (mu4ShotMode == NSCam::eShotMode_FaceBeautyShotCc)
        {
            CAM_LOGD("[createFullFrame] ZSD Shot \n");
            pSingleShot->sendCommand( NSCamShot::ECamShot_CMD_SET_CAPTURE_STYLE,
                                      NSCamShot::ECamShot_CMD_STYLE_NORMAL, 0, 0 );
        }

        //
        pSingleShot->sendCommand( NSCamShot::ECamShot_CMD_SET_NRTYPE, nrtype, 0, 0 );

        //
        //CAM_TRACE_BEGIN("startOne");
        if (mu4ShotMode == NSCam::eShotMode_FaceBeautyShotCc)
        {
            IImageBuffer* pImageBuffer;
            mpCapBufMgr->dequeBuf(pImageBuffer);
            pSingleShot->startOne(rSensorParam, pImageBuffer);
            mpCapBufMgr->enqueBuf(pImageBuffer);
        }
        else
        {
            pSingleShot->startOne(rSensorParam);
        }
        //CAM_TRACE_END();

        //
        pSingleShot->uninit();

        //
        pSingleShot->destroyInstance();

#ifdef _WORKAROUND_MULTI_PLANE
        {
            char szFileName[100];
            ::sprintf(szFileName, "/sdcard/srcimg_yuy2_%d_%d.yuv", mShotParam.mi4PictureWidth, mShotParam.mi4PictureHeight);
            mptmp->saveToFile(szFileName);
        }
        ImgProcess(mptmp, 0, 0, eImgFmt_YUY2, Srcbufinfo, 0, 0, eImgFmt_I422);
#if 0
        MUINT32 w = mShotParam.mi4PictureWidth;
        MUINT32 h = mShotParam.mi4PictureHeight;
        for (int i=0;i<w*h*2/4;i++)
        {
            ((MUINT8*)Srcbufinfo->getBufVA(0))[2*i]=((MUINT8*)mptmp->getBufVA(0))[4*i];
            ((MUINT8*)Srcbufinfo->getBufVA(1))[i]=((MUINT8*)mptmp->getBufVA(0))[4*i+1];
            ((MUINT8*)Srcbufinfo->getBufVA(0))[2*i+1]=((MUINT8*)mptmp->getBufVA(0))[4*i+2];
            ((MUINT8*)Srcbufinfo->getBufVA(2))[i]=((MUINT8*)mptmp->getBufVA(0))[4*i+3];
        }
#endif
        deallocMem(mptmp);
        char szFileName[100];
        ::sprintf(szFileName, "/sdcard/srcimg_%dx%d.yuv", mShotParam.mi4PictureWidth, mShotParam.mi4PictureHeight);
        Srcbufinfo->saveToFile(szFileName);
#endif

        mTransform = mShotParam.mu4Transform;

#ifndef MTK_CAM_VIDEO_FACEBEAUTY_SUPPORT
        if(mShotParam.ms8ShotFileName.string()!=NULL) {
            ret = createFBJpegImg(Srcbufinfo,mu4W_yuv,mu4H_yuv,1);
            if  ( ! ret )
            {
                goto lbExit;
            }
        }
#endif //MTK_CAM_VIDEO_FACEBEAUTY_SUPPORT
    }
    else
    {
        /* SDK mode: copy source image to continuous memory */
        MUINT32 transform = 0; //(mSrcImgBuffer->getImgSize().w > mSrcImgBuffer->getImgSize().h)? 0: NSCam::eTransform_ROT_270;
        mTransform = sdkParams.Rotation;//(mSrcImgBuffer->getImgSize().w > mSrcImgBuffer->getImgSize().h)? 0: NSCam::eTransform_ROT_90;
        ImgProcess(mSrcImgBuffer, 0, 0, eImgFmt_YV12, Srcbufinfo, 0, 0, eImgFmt_YV12, transform);
    }

    {
#ifndef Debug_Mode
        char EnableOption[PROPERTY_VALUE_MAX] = {'\0'};
        property_get("camera.facebeauty.dump", EnableOption, "0");
        if (EnableOption[0] == '1')
#endif
        {
            char szFileName[100];
            ::sprintf(szFileName, "/sdcard/srcimg_%dx%d.yuv", Srcbufinfo->getImgSize().w, Srcbufinfo->getImgSize().h);
            Srcbufinfo->saveToFile("/sdcard/srcimg.yuv");
        }
    }
    //CAM_TRACE_END();

        //-----vfb-----
        if(mSensorType == SENSOR_TYPE_RAW && mSDKMode != MTRUE)
        {
            CAM_LOGD("RAW sensor: DO FB by HW path +");
            sem_wait(&semMemoryDone);

            //CAM_TRACE_BEGIN("Resize");

            //ImgProcess(Srcbufinfo, 0, 0, eImgFmt_I422, buffer_DS_Input, 0, 0, eImgFmt_YUY2, 0);

            //CAM_TRACE_END();

            if (mu4ShotMode != NSCam::eShotMode_FaceBeautyShotCc)//When in non ZSD mode, need to do FD
            {
                //CAM_TRACE_BEGIN("DO FD");

                CAM_LOGD("RAW sensor:  Get FD infomation on non-ZSD mode");

                qUtilityStatus.push(5);//resize
                sem_post(&semUtilitythread);

                IImageBuffer* buffer_FD = allocMem(eImgFmt_I422,
                                          mDSWidth,//VFB_DS_IMAGE_MAX_WIDTH,
                                          mDSHeight);//VFB_DS_IMAGE_MAX_HEIGHT);
                ImgProcess(Srcbufinfo, 0, 0, eImgFmt_YV12, buffer_FD, 0, 0, eImgFmt_I422, 0);

                //mvfb_FBmetadata.faces=(MtkCameraFace *)mvfb_FDFaceInfo;
                //mvfb_FBmetadata.posInfo=(MtkFaceInfo *)mvfb_FDPoseInfo;
                doFaceDetection(buffer_FD,
                                buffer_FD->getImgSize().w,
                                buffer_FD->getImgSize().h,
                                mSDKMode? mu4W_yuv: mShotParam.mi4PostviewWidth,
                                mSDKMode? mu4H_yuv: mShotParam.mi4PostviewHeight,
                                (mu4ShotMode == NSCam::eShotMode_FaceBeautyShotCc));
                deallocMem(buffer_FD);
                //CAM_TRACE_END();
            } else {
                CAM_LOGD("RAW sensor:  Get FD infomation on ZSD mode");
                ImgProcess(Srcbufinfo, 0, 0, eImgFmt_I422, buffer_DS_Input, 0, 0, eImgFmt_YUY2, 0);

                postionFacesInImage(mSDKMode? mu4W_yuv: mShotParam.mi4PostviewWidth,
                                  mSDKMode? mu4H_yuv: mShotParam.mi4PostviewHeight);
            }

            CAM_LOGD("RAW sensor:  FDgetresult Found face index %d left %d top %d right %d button %d",mvfb_FBmetadata.number_of_faces,
                mvfb_FBmetadata.faces[0].rect[0],mvfb_FBmetadata.faces[0].rect[1],mvfb_FBmetadata.faces[0].rect[2],mvfb_FBmetadata.faces[0].rect[3]);

            //CAM_TRACE_BEGIN("DO vFB");
            do_VFB();
            //CAM_TRACE_END();

            //CAM_TRACE_BEGIN("DO p2b");
            doP2b();
    //CAM_TRACE_END();

            CAM_LOGD("RAW sensor: DO FB by HW path -");

            return  ret;
        }
        //-------------

    sem_wait(&semMemoryDone);

    #if 0//def Debug_Mode
    Srcbufinfo->loadFromFile("/data/FBSOURCE.yuv");
    Srcbufinfo->saveToFile("/sdcard/img.yuv");
    #endif
    CAM_LOGD("[createFullFrame] - \n");
lbExit:
    return  ret;
#endif
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
createJpegImg(IImageBuffer const * rSrcImgBufInfo
      , NSCamShot::JpegParam const & rJpgParm
      , MUINT32 const u4Transform
      , IImageBuffer const * rJpgImgBufInfo
      , MUINT32 & u4JpegSize)
{
    MBOOL ret = MTRUE;
    // (0). debug
    CAM_LOGD("[createJpegImg] - E.");
    CAM_LOGD("[createJpegImg] - rSrcImgBufInfo.eImgFmt=%d", rSrcImgBufInfo->getImgFormat());
    CAM_LOGD("[createJpegImg] - u4Transform=%d", u4Transform);
    //
    // (1). Create Instance
    NSCam::NSIoPipe::NSSImager::ISImager *pISImager = NSCam::NSIoPipe::NSSImager::ISImager::createInstance(rSrcImgBufInfo);
    if(!pISImager) {
    CAM_LOGE("HdrShot::createJpegImg can't get ISImager instance.");
    return MFALSE;
    }

    // init setting
    pISImager->setTargetImgBuffer(rJpgImgBufInfo);
    // crop to keep aspect ratio
    MRect crop;
    MSize const srcSize(rSrcImgBufInfo->getImgSize());
    MSize const dstSize =
        u4Transform & eTransform_ROT_90 ?
        MSize( rJpgImgBufInfo->getImgSize().h, rJpgImgBufInfo->getImgSize().w ) :
        rJpgImgBufInfo->getImgSize();
#define align2(x)   (((x) + 1) & (~1))
    if( srcSize.w * dstSize.h > srcSize.h * dstSize.w ) {
        crop.s.w = align2(dstSize.w * srcSize.h / dstSize.h);
        crop.s.h = align2(srcSize.h);
        crop.p.x = (srcSize.w - crop.s.w) / 2;
        crop.p.y = 0;
    } else if( srcSize.w * dstSize.h < srcSize.h * dstSize.w ) {
        crop.s.w = align2(srcSize.w);
        crop.s.h = align2(dstSize.h * srcSize.w / dstSize.w);
        crop.p.x = 0;
        crop.p.y = (srcSize.h - crop.s.h) / 2;
    }
    pISImager->setCropROI(crop);
#undef align2
    //
    pISImager->setTransform(u4Transform);
    //
    //pISImager->setFlip(u4Flip);
    //
    //pISImager->setResize(rJpgImgBufInfo.u4ImgWidth, rJpgImgBufInfo.u4ImgHeight);
    //
    pISImager->setEncodeParam(rJpgParm.fgIsSOI, rJpgParm.u4Quality);
    //
    //pISImager->setROI(Rect(0, 0, rSrcImgBufInfo->getImgSize().w, rSrcImgBufInfo->getImgSize().h));
    //
    pISImager->execute();
    //
    u4JpegSize = rJpgImgBufInfo->getBitstreamSize();

    pISImager->destroyInstance();

    CAM_LOGD("[init] - X. ret: %d.", ret);
    return ret;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
createJpegImgWithThumbnail(IImageBuffer const *rYuvImgBufInfo, IImageBuffer const *rPostViewBufInfo, MUINT32 const Mode)
{
    return true;
#if 0
    MBOOL ret = MTRUE;
    MUINT32 stride[3];
    //CAM_TRACE_BEGIN("JpegEnc 3");
    CAM_LOGD("[createJpegImgWithThumbnail] in");
    MBOOL bVertical = ( mTransform == NSCam::eTransform_ROT_90 ||
            mTransform == NSCam::eTransform_ROT_270 );

    IImageBuffer* jpegBuf = allocMem(eImgFmt_JPEG,
                                     bVertical? mu4H_yuv: mu4W_yuv,
                                     bVertical? mu4W_yuv: mu4H_yuv);
    if(jpegBuf == NULL)
    {
        CAM_LOGE("[createJpegImgWithThumbnail] jpegBuf alloc fail");
        ret = MFALSE;
        return ret;
    }
#if 0
    //rThumbImgBufInfo
    IImageBuffer* thumbBuf = allocMem(eImgFmt_JPEG,
                                      bVertical? mJpegParam.mi4JpegThumbHeight: mJpegParam.mi4JpegThumbWidth,
                                      bVertical? mJpegParam.mi4JpegThumbWidth: mJpegParam.mi4JpegThumbHeight);
    if(thumbBuf == NULL)
    {
        CAM_LOGE("[createJpegImgWithThumbnail] thumbBuf alloc fail");
        ret = MFALSE;
        return ret;
    }
#endif
    //do_main_Jpeg();

    //main_Jpeg_encode();

    MUINT32 u4JpegSize = 0;
    MUINT32 u4ThumbSize = 0;

    NSCamShot::JpegParam yuvJpegParam(mJpegParam.mu4JpegQuality, MFALSE);
    ret = ret && createJpegImg(rYuvImgBufInfo, yuvJpegParam, mTransform, jpegBuf, u4JpegSize);
    //CAM_TRACE_END();
    //CAM_TRACE_BEGIN("JpegEnc 4");
    sem_wait(&semJPGDone);

    // (3.1) create thumbnail
    // If postview is enable, use postview buffer,
    // else use yuv buffer to do thumbnail
    //if (0 != mJpegParam.mi4JpegThumbWidth && 0 != mJpegParam.mi4JpegThumbHeight)
    //{
    //    NSCamShot::JpegParam rParam(mJpegParam.mu4JpegThumbQuality, MTRUE);
    //    ret = ret && createJpegImg(rPostViewBufInfo, rParam, mTransform, thumbBuf, u4ThumbSize);
    //}

    #ifdef Debug_Mode // Save Img for debug.
    {
        char szFileName[100];

        saveBufToFile("/sdcard/Result.jpg", (uint8_t*)jpegBuf->getBufVA(0), u4JpegSize);
        CAM_LOGD("[createJpegImgWithThumbnail] Save %s done.", szFileName);

        saveBufToFile("/sdcard/ThumbImg.jpg", (uint8_t*)thumbBuf->getBufVA(0), u4ThumbSize);
        CAM_LOGD("[createJpegImgWithThumbnail] Save %s done.", szFileName);
    }
    #endif  // Debug_Mode

    jpegBuf->syncCache(eCACHECTRL_INVALID);
    thumbBuf->syncCache(eCACHECTRL_INVALID);

    // Jpeg callback, it contains thumbnail in ext1, ext2.
    handleJpegData((MUINT8*)jpegBuf->getBufVA(0), u4JpegSize, (MUINT8*)thumbBuf->getBufVA(0), mthmSize, Mode);

    //CAM_TRACE_END();

    deallocMem(jpegBuf);
    deallocMem(thumbBuf);
    CAM_LOGD("[createJpegImgWithThumbnail] out");
    return ret;
#endif
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
do_thumbnail()
{
    CAM_LOGD(" do_thumbnail  +");

    sem_init(&semJPGDone, 0, 0);
    qUtilityStatus.push(6);
    sem_post(&semUtilitythread);

    CAM_LOGD(" do_thumbnail  -");
    return  MTRUE;
}

MBOOL
Mhal_facebeauty::
thumbnail_encode()
{
#if 0
    MBOOL ret = MTRUE;

    CAM_LOGD(" thumbnail_encode  +");

    MBOOL bVertical = ( mTransform == NSCam::eTransform_ROT_90 ||
            mTransform == NSCam::eTransform_ROT_270 );

    ImgProcess(mpOutputbuffer, 0, 0, eImgFmt_I422, mpPostviewImgBuf, 0, 0, static_cast<EImageFormat>(mShotParam.miPostviewDisplayFormat));

    thumbBuf = allocMem(eImgFmt_JPEG,
                                      bVertical? mJpegParam.mi4JpegThumbHeight: mJpegParam.mi4JpegThumbWidth,
                                      bVertical? mJpegParam.mi4JpegThumbWidth: mJpegParam.mi4JpegThumbHeight);
    if(thumbBuf == NULL)
    {
        CAM_LOGE("[thumbnail_encode] thumbBuf alloc fail");
        ret = MFALSE;
        return ret;
    }

    // (3.1) create thumbnail
    // If postview is enable, use postview buffer,
    // else use yuv buffer to do thumbnail
     MUINT32 u4ThumbSize = 0;
    if (0 != mJpegParam.mi4JpegThumbWidth && 0 != mJpegParam.mi4JpegThumbHeight)
    {
        NSCamShot::JpegParam rParam(mJpegParam.mu4JpegThumbQuality, MTRUE);
        ret = ret && createJpegImg(mpPostviewImgBuf, rParam, mTransform, thumbBuf, u4ThumbSize);
    }
    mthmSize = u4ThumbSize;

    CAM_LOGD(" thumbnail_encode  - mthmSize = %d",mthmSize);

    sem_post(&semJPGDone);
#endif
    return  MTRUE;
}


MBOOL
Mhal_facebeauty::
createFBJpegImg(IImageBuffer* Srcbufinfo, int u4SrcWidth, int u4SrcHeight, MUINT32 const Mode)
{
    return true;
#if 0
    CAM_LOGD("[createFBJpegImg] in");
    MBOOL ret = MTRUE;
    //CAM_TRACE_BEGIN("JpegEnc");
    //CAM_TRACE_BEGIN("JpegEnc 1");

    mPostviewWidth = mShotParam.mi4PostviewWidth;
    mPostviewHeight = mShotParam.mi4PostviewHeight;
    EImageFormat mPostviewFormat = static_cast<EImageFormat>(mShotParam.miPostviewDisplayFormat);
    mpPostviewImgBuf = allocMem(mPostviewFormat, mPostviewWidth, mPostviewHeight);
    if(mpPostviewImgBuf == NULL)
    {
        CAM_LOGE("[STEP1] mpPostviewImgBuf alloc fail");
        ret = MFALSE;
        return ret;
    }
    //CAM_TRACE_END();
    //CAM_TRACE_BEGIN("JpegEnc 2");

    do_thumbnail();

    //ImgProcess(Srcbufinfo, u4SrcWidth, u4SrcHeight, eImgFmt_I422, mpPostviewImgBuf, mPostviewWidth, mPostviewHeight, mPostviewFormat);

    //do_main_Jpeg();

    if(!Mode)
        handlePostViewData((MUINT8*)mpPostviewImgBuf->getBufVA(0), mpPostviewImgBuf->getBufSizeInBytes(0));
    //CAM_TRACE_END();

    ret = createJpegImgWithThumbnail(Srcbufinfo, mpPostviewImgBuf, Mode);

    deallocMem(mpPostviewImgBuf);

    //CAM_TRACE_END();
    CAM_LOGD("[createFBJpegImg] out");
    return ret;
#endif
}

MBOOL
startStep2_3_5(void)
{
    qUtilityStatus.push(4);
    return (sem_post(&semUtilitythread) == 0);
}

MBOOL SDKJPG_encode(my_encode_params& rParams)
{
    //
    MBOOL ret = MTRUE;
    //
    CAM_LOGD("[%s] +", __FUNCTION__);
    NSSImager::ISImager* pSImager = NSSImager::ISImager::createInstance(rParams.pSrc);
    if( pSImager == NULL ) {
        CAM_LOGD("[%s] create SImage failed",__FUNCTION__);
        return MFALSE;
    }

    ret = pSImager->setTargetImgBuffer(rParams.pDst)

        && pSImager->setTransform(rParams.transform)

        && pSImager->setCropROI(rParams.crop)

        && pSImager->setEncodeParam(
                rParams.isSOI,
                rParams.quality,
                rParams.codecType
                )
        && pSImager->execute();

    pSImager->destroyInstance();
    pSImager = NULL;
    //
    if( !ret ) {
        CAM_LOGD("[%s] encode failed",__FUNCTION__);
        return MFALSE;
    }
    //
    CAM_LOGD("[%s] -", __FUNCTION__);
    return OK;
}

MBOOL
Mhal_facebeauty::
SDKcreateFBJpegImg()
{
    //CAM_TRACE_BEGIN("SDKcreateFBJpegImg");

    MBOOL ret = MFALSE;

    my_encode_params params;
    IImageBuffer* jpegBuf = allocMem(eImgFmt_JPEG,
                                    mpSource->getImgSize().w,
                                    mpSource->getImgSize().h);
    params.pSrc = mpSource;
    params.pDst = jpegBuf;
    params.transform = 0; //TODO
    params.crop =MRect(MPoint(0,0), mpSource->getImgSize());
    params.isSOI = 0;
    params.quality = 80;
    params.codecType = NSSImager::JPEGENC_HW_FIRST;//NSSImager::JPEGENC_SW;//NSSImager::JPEGENC_HW_FIRST;//JPEGENC_SW JPEGENC_HW_FIRST

    CAM_LOGD("[%s] +", __FUNCTION__);

    ret = SDKJPG_encode(params);
    if(ret != OK) {
        CAM_LOGE("[%s] SDKJPG main JPG is fail \n",__FUNCTION__);
    }

    mpPostviewImgBuf = allocMem(eImgFmt_YV12, 160, 128);

    ImgProcess(mpSource, mpSource->getImgSize().w, mpSource->getImgSize().h, eImgFmt_YV12, mpPostviewImgBuf, mpPostviewImgBuf->getImgSize().w, mpPostviewImgBuf->getImgSize().h, eImgFmt_YV12);

    IImageBuffer* thumbBuf = allocMem(eImgFmt_JPEG,
                                    mpPostviewImgBuf->getImgSize().w,
                                    mpPostviewImgBuf->getImgSize().h);
    params.pSrc = mpPostviewImgBuf;
    params.pDst = thumbBuf;
    params.transform = 0; //TODO
    params.crop =MRect(MPoint(0,0), mpPostviewImgBuf->getImgSize());
    params.isSOI = 1;
    params.quality = 80;
    params.codecType = NSSImager::JPEGENC_SW;//NSSImager::JPEGENC_HW_FIRST;//JPEGENC_SW JPEGENC_HW_FIRST

    ret = SDKJPG_encode(params);
    if(ret != OK) {
        CAM_LOGE("[%s] SDKJPG thumbnail is fail \n",__FUNCTION__);
    }

#ifdef Debug_Mode
    jpegBuf->saveToFile("/sdcard/MainResult.jpg");
    CAM_LOGD("[SDKcreateFBJpegImg] Main JPG: w = %d h = %d BitstreamSize = %d",  mpSource->getImgSize().w, mpSource->getImgSize().h,jpegBuf->getBitstreamSize());

    thumbBuf->saveToFile("/sdcard/ThumbResult.jpg");
    CAM_LOGD("[SDKcreateFBJpegImg] Thumbnail JPG: w = %d h = %d BitstreamSize = %d",  mpPostviewImgBuf->getImgSize().w, mpPostviewImgBuf->getImgSize().h,thumbBuf->getBitstreamSize());

    jpegBuf->syncCache(eCACHECTRL_INVALID);
    thumbBuf->syncCache(eCACHECTRL_INVALID);
#endif

    CAM_LOGD("[%s] create exif +",__FUNCTION__);

    MINT8 *pExifBuf = new MINT8[DBG_EXIF_SIZE];
    memset(pExifBuf, 0, DBG_EXIF_SIZE*sizeof(MINT8));

    StdExif exif;//[++]name
    size_t exifSize  = exif.getHeaderSize();//EXIF_HEADER_SIZE
    size_t thumbnailMaxSize = 0;
    ExifParams stdParams;
    memset(&stdParams, 0, sizeof(ExifParams));

    thumbnailMaxSize = thumbBuf->getBitstreamSize();//(mpPostviewImgBuf->getImgSize().w) * (mpPostviewImgBuf->getImgSize().h) * 18 / 10;

    stdParams.u4ImageWidth = mpSource->getImgSize().w;       // Image width
    stdParams.u4ImageHeight = mpSource->getImgSize().h;      // Image height
    stdParams.u4FNumber = 28;// Format: F2.8 = 28
    stdParams.u4FocalLength =  350;// Format: FL 3.5 = 350
    stdParams.u4Facing  = 0;//(muFacing == MTK_LENS_FACING_BACK) ? 0 : 1;
    stdParams.u4Orientation  = sdkParams.Rotation;

    exif.init(stdParams, ENABLE_DEBUG_INFO);

    exif.setMaxThumbnail(thumbnailMaxSize);

    //exif.setOutputBuffer(pExifBuf, exifSize);

    //exif.make(pExifBuf,exifSize);

    exif.uninit();

    CAM_LOGD("[%s] create exif - : exifSize = %d thumbnailMaxSize = %d",__FUNCTION__,exifSize,thumbnailMaxSize);

    //sp<IImageBuffer>     pOutImageBuffer     = NULL;
    //sp<IImageBufferHeap>  mpOutImgBufferHeap;//
    int outputsize = jpegBuf->getBitstreamSize() + thumbBuf->getBitstreamSize() + exifSize;
    //MINT8 *pJpegBuf = new MINT8[outputsize];
#if 0
    IImageBuffer *tempJpegBuf = allocMem(eImgFmt_BLOB,
                                    outputsize,
                                    1);
#endif
    MINT8 *pJpegBuf = (MINT8 *)mDstImgBuffer->getBufVA(0);//(MINT8 *)tempJpegBuf->getBufVA(0);

    // out buffer
    size_t const jpegBufSize = jpegBuf->getBitstreamSize()+ exifSize;
    size_t jpegsize_final    = 0;

    CAM_LOGD("[%s] integrate JPG +",__FUNCTION__);
    // main jpeg
    void* pMainJpegAddr = NULL;
    size_t mainJpegSize = 0;

    //thumb
    void* pThumbJpegAddr = NULL;
    size_t ThumbJpegSize = 0;

    pMainJpegAddr = reinterpret_cast<void*>(jpegBuf->getBufVA(0));
    mainJpegSize = jpegBuf->getBitstreamSize();

    pThumbJpegAddr = reinterpret_cast<void*>(thumbBuf->getBufVA(0));
    ThumbJpegSize = thumbBuf->getBitstreamSize();

    //1. copy header
    memcpy(pJpegBuf, pExifBuf, exifSize);
    jpegsize_final += exifSize;

    //2. copy thumb
    memcpy(pJpegBuf + jpegsize_final, pThumbJpegAddr, ThumbJpegSize );
    jpegsize_final += ThumbJpegSize;

    //3.copy jpeg
    memcpy(pJpegBuf + jpegsize_final, pMainJpegAddr, mainJpegSize );
    jpegsize_final += mainJpegSize;

    //4.add end information
    camera3_jpeg_blob jpeg_end;
    size_t tempjpegBufSize = mDstImgBuffer->getImgSize().w;
    jpeg_end.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
    jpeg_end.jpeg_size    = jpegsize_final;
    //
    memcpy(pJpegBuf + tempjpegBufSize - sizeof(camera3_jpeg_blob),
            &jpeg_end,
            sizeof(camera3_jpeg_blob)
          );


    CAM_LOGD("[%s] integrate JPG - : jpegsize_final = %d %d",__FUNCTION__,jpegsize_final,mDstImgBuffer->getImgSize().w);

#ifdef Debug_Mode
    //saveBufToFile("/sdcard/test.jpg", (uint8_t*)pJpegBuf, jpegsize_final);
    saveBufToFile("/sdcard/sdkfb.jpg", (uint8_t*)mDstImgBuffer->getBufVA(0), jpegsize_final);
#endif

    //MUINT32 transform = (mSrcImgBuffer->getImgSize().w > mSrcImgBuffer->getImgSize().h)? 0: NSCam::eTransform_ROT_90;
    //ImgProcess(jpegBuf, 0, 0, eImgFmt_JPEG, mDstImgBuffer, 0, 0, eImgFmt_JPEG, transform);

    delete [] pExifBuf;
    deallocMem(jpegBuf);
    deallocMem(thumbBuf);
    //deallocMem(tempJpegBuf);
    //CAM_TRACE_END();

    CAM_LOGD("[%s] -", __FUNCTION__);

    return ret;
}

MBOOL
Mhal_facebeauty::
doCapture()
{
    MBOOL ret = MFALSE;
    //MINT8 TargetColor = NSCamCustom::get_FB_ColorTarget();
    //MINT8 BlurLevel = NSCamCustom::get_FB_BlurLevel();
    MINT8 TargetColor =0;
    MINT8 BlurLevel =4;

    CAM_LOGD("YUV sensor: docapture +");
    if (requestBufs()
        &&  createFullFrame(mpSource)
        &&  InitialAlgorithm(mu4W_yuv, mu4H_yuv, BlurLevel, TargetColor)
        &&  startStep2_3_5()
        &&  (!mCancel && STEP1(mpSource, mu4W_yuv, mu4H_yuv, mpBlurImg, mpAmap, (void*) &msFaceBeautyResultInfo))
        //&&  SaveJpg()
        //&&  STEP2(mpSource, mu4W_yuv, mu4H_yuv,mpAmap, &FBmetadata,(void*) &msFaceBeautyResultInfo)
        //&&  STEP3(mpAmap,(void*) &msFaceBeautyResultInfo)
        //&&  WaitSaveDone()
        &&  (!mCancel && STEP4(mpSource, mu4W_yuv, mu4H_yuv, mpBlurImg, mpAmap, (void*) &msFaceBeautyResultInfo))
        //&&  STEP5(mpSource, mu4W_yuv, mu4H_yuv, mpAmap, (void*) &msFaceBeautyResultInfo)
        &&  (!mCancel && STEP6(mpSource, mu4W_yuv, mu4H_yuv, mpBlurImg, (void*) &msFaceBeautyResultInfo))
        &&  (!mCancel && (mSDKMode || createFBJpegImg(mpSource,mu4W_yuv,mu4H_yuv,
#ifndef MTK_CAM_VIDEO_FACEBEAUTY_SUPPORT
            0)))
#else
            1)))
#endif //MTK_CAM_VIDEO_FACEBEAUTY_SUPPORT
        )
    {
        ret = MTRUE;
    }
    else
    {
        CAM_LOGE("[FBCapture] Capture fail \n");
    }

    //if(mSDKMode)
    //    ret = SDKcreateFBJpegImg();

    CAM_LOGD("YUV sensor: docapture -");

    sem_post(&semFBthread);

    return ret;

}

MVOID* FBCapture(void *arg)
{
    cpu_set_t cpuset, cpuold, mt_cpuset;

#ifdef USING_MTK_AFFINITY
    CPU_ZERO(&cpuset);
    for(int cpu_no = 4; cpu_no < 8; cpu_no++)
        CPU_SET(cpu_no, &cpuset);
    int s = mt_sched_getaffinity(gettid(), sizeof(cpu_set_t), &cpuold, &mt_cpuset);
    if (s == 0)
    {
        CAM_LOGD("mt_sched_getaffinity fail\n");
    }
    else
    {
        CAM_LOGD("Thread %d Origin CPU Affinity %08lx\n", gettid(), cpuold);
    }
    s = mt_sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpuset);
    if (s != 0)
    {
        CAM_LOGD("mt_sched_setaffinity fail\n");
    }
    else
    {
        s = mt_sched_getaffinity(gettid(), sizeof(cpu_set_t), &cpuset, &mt_cpuset);
        CAM_LOGD("Thread %d New CPU Affinity %08lx\n", gettid(), cpuset);
    }
#endif //USING_MTK_AFFINITY

    mpFbObj->doCapture();

#ifdef USING_MTK_AFFINITY
    mt_sched_exitaffinity(0);
#endif //USING_MTK_AFFINITY
    return NULL;
}

MBOOL
Mhal_facebeauty::
doStep2And3And5()
{
    MBOOL ret = MFALSE;

    if ((!mCancel && STEP2(mpSource, mu4W_yuv, mu4H_yuv,mpAmap, &FBmetadata,(void*) &msFaceBeautyResultInfo)) &&
        (!mCancel && STEP3(mpAmap,(void*) &msFaceBeautyResultInfo)) &&
        (!mCancel && STEP5(mpSource, mu4W_yuv, mu4H_yuv, mpAmap, (void*) &msFaceBeautyResultInfo)))
    {
        ret = MTRUE;
    }
    else
    {
        CAM_LOGE("[FBCapture] Capture fail \n");
    }
    sem_post(&semFBthread2);
    return ret;
}
MVOID* FBUtility(void *arg)
{
    cpu_set_t cpuset, cpuold, mt_cpuset;

#ifdef USING_MTK_AFFINITY
    CPU_ZERO(&cpuset);
    for(int cpu_no = 4; cpu_no < 8; cpu_no++)
        CPU_SET(cpu_no, &cpuset);
    int s = mt_sched_getaffinity(gettid(), sizeof(cpu_set_t), &cpuold, &mt_cpuset);
    if (s == 0)
    {
        CAM_LOGD("mt_sched_getaffinity fail\n");
    }
    else
    {
        CAM_LOGD("Thread %d Origin CPU Affinity %08lx\n", gettid(), cpuold);
    }
    s = mt_sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpuset);
    if (s != 0)
    {
        CAM_LOGD("mt_sched_setaffinity fail\n");
    }
    else
    {
        s = mt_sched_getaffinity(gettid(), sizeof(cpu_set_t), &cpuset, &mt_cpuset);
        CAM_LOGD("Thread %d New CPU Affinity %08lx\n", gettid(), cpuset);
    }
#endif //USING_MTK_AFFINITY

    MBOOL ret = MFALSE;
    int UtilityStatus = 1;
    while(UtilityStatus)
    {
        CAM_LOGD("[FBUtility] Wait in UtilityStatus %d",UtilityStatus);
        sem_wait(&semUtilitythread);
        if(qUtilityStatus.empty())
        {
            CAM_LOGE("[FBUtility] Empty queue");
            continue;
        }
        UtilityStatus = qUtilityStatus.front();
        qUtilityStatus.pop();
        CAM_LOGD("[FBUtility] get command UtilityStatus %d",UtilityStatus);
        switch(UtilityStatus)
        {
            case 3: // memory allocate
                 //CAM_TRACE_BEGIN("allocMem buffer");
                 CAM_LOGD("[FBUtility] memory allocate");
                mpFbObj->FBWorkingBufferSize = mpFbObj->mpFb->getWorkingBuffSize(mpFbObj->mu4W_yuv,mpFbObj->mu4H_yuv,mpFbObj->mDSWidth,mpFbObj->mDSHeight,((mpFbObj->mu4W_yuv >> 1) & 0xFFFFFFF0),((mpFbObj->mu4H_yuv >> 1) & 0xFFFFFFF0));
                CAM_LOGD("[requestBufs] FBWorkingBufferSize %d",mpFbObj->FBWorkingBufferSize);
                if(!(mpFbObj->mpWorkingBuferr = malloc(mpFbObj->FBWorkingBufferSize)))
                {
                    CAM_LOGE("[requestBufs] mpWorkingBuferr alloc fail");
                }

                mpFbObj->mpBlurImg = mpFbObj->allocMem(eImgFmt_I422, mpFbObj->mpSource->getImgSize().w, mpFbObj->mpSource->getImgSize().h);
                if(!mpFbObj->mpBlurImg)
                {
                    CAM_LOGE("[requestBufs] mpBlurImg alloc fail");
                }
                mpFbObj->mpAmap = mpFbObj->allocMem(eImgFmt_I422, mpFbObj->mpSource->getImgSize().w, mpFbObj->mpSource->getImgSize().h);
                if(!mpFbObj->mpAmap)
                {
                    CAM_LOGE("[requestBufs] mpAmap alloc fail");
                }
                /* HW FD size has to be 4:3 */
                do {
                    MUINT32 w = mpFbObj->mDSWidth, h = mpFbObj->mDSHeight;
                    if (w * 3 > h * 4)
                    {
                        h = w * 3 / 4;
                    }
                    else if (w * 3 < h * 4)
                    {
                        w = h * 4 / 3;
                    }
                    mpFbObj->mpDSImg = mpFbObj->allocMem(eImgFmt_I422, w, h);
                    if(!mpFbObj->mpDSImg)
                    {
                        CAM_LOGE("[requestBufs] mpDSImg alloc fail");
                    }
                    //mpFbObj->mpDSImg = mpFbObj->resizeMem(mpFbObj->mpDSImg, mpFbObj->mDSWidth, mpFbObj->mDSHeight);
                } while(0);

                mpFbObj->mpFDWorkingBuffer = new unsigned char[FD_WORKING_BUF_SIZE];

                    //mpFbObj->mpAmapForStep5 = mpFbObj->allocMem(eImgFmt_I422, mpFbObj->mpSource->getImgSize().w, mpFbObj->mpSource->getImgSize().h);
                    //if(!mpFbObj->mpAmapForStep5)
                    //{
                    //    CAM_LOGE("[requestBufs] mpAmapForStep5 alloc fail");
                    //}
                sem_post(&semMemoryDone);
                break;
            case 2: // jpg encode
                CAM_LOGD("[FBUtility] jpg encode ");
                ret = mpFbObj->createFBJpegImg(mpFbObj->mpSource,mpFbObj->mu4W_yuv,mpFbObj->mu4H_yuv,1);
                if  ( ! ret )
                {
                    CAM_LOGD("[FBUtility] jpg encode fail");
                }
                sem_post(&semJPGDone);
                break;
            case 4:
                mpFbObj->doStep2And3And5();
                break;
            case 5:
                //CAM_TRACE_BEGIN("Resize");
                mpFbObj->ImgProcess(mpFbObj->mpSource, 0, 0, eImgFmt_I422, mpFbObj->buffer_DS_Input, 0, 0, eImgFmt_YUY2, 0);
                //CAM_TRACE_END();
                break;
            case 6:
                //CAM_TRACE_BEGIN("thumbnail endcode");
                mpFbObj->thumbnail_encode();
                //CAM_TRACE_END();
                break;
            case 0:
            default:
                break;
        }
    }
    CAM_LOGD("[FBUtility] out");

#ifdef USING_MTK_AFFINITY
    mt_sched_exitaffinity(0);
#endif //USING_MTK_AFFINITY
    return NULL;
}
/******************************************************************************
*
*******************************************************************************/
IImageBuffer*
Mhal_facebeauty::
allocMem(MUINT32 fmt, MUINT32 w, MUINT32 h)
{

    return BufAllocUtil::getInstance().allocMem(LOG_TAG, fmt, w, h);
#if 0
    IImageBuffer* pBuf;

    if( fmt != eImgFmt_JPEG )
    {
        /* To avoid non-continuous multi-plane memory, allocate ION memory and map it to ImageBuffer */
        MUINT32 plane = NSCam::Utils::Format::queryPlaneCount(fmt);
        ImageBufferMap bufMap;

        bufMap.memBuf.size = 0;
        for (int i = 0; i < plane; i++)
        {
            bufMap.memBuf.size += (NSCam::Utils::Format::queryPlaneWidthInPixels(fmt,i, w) * NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt,i) / 8) * NSCam::Utils::Format::queryPlaneHeightInPixels(fmt, i, h);
        }

        if (mpIMemDrv->allocVirtBuf(NAME, &bufMap.memBuf)) {
        //if (mpIMemDrv->allocVirtBuf(&bufMap.memBuf)) {
            CAM_LOGE("g_pIMemDrv->allocVirtBuf() error \n");
            return NULL;
        }
        //memset((void*)bufMap.memBuf.virtAddr, 0 , bufMap.memBuf.size);
        if (mpIMemDrv->mapPhyAddr(&bufMap.memBuf)) {
            CAM_LOGE("mpIMemDrv->mapPhyAddr() error \n");
            return NULL;
        }

        size_t bufBoundaryInBytes[3] = {0, 0, 0};
        size_t bufStridesInBytes[3] = {0};

        for (MUINT32 i = 0; i < plane; i++)
        {
            bufStridesInBytes[i] = NSCam::Utils::Format::queryPlaneWidthInPixels(fmt,i, w) * NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt,i) / 8;
        }
        IImageBufferAllocator::ImgParam imgParam(
                fmt,
                MSize(w,h),
                bufStridesInBytes,
                bufBoundaryInBytes,
                plane
                );

        PortBufInfo_v1 portBufInfo = PortBufInfo_v1(
                                        bufMap.memBuf.memID,
                                        bufMap.memBuf.virtAddr,
                                        bufMap.memBuf.useNoncache,
                                        bufMap.memBuf.bufSecu,
                                        bufMap.memBuf.bufCohe);

        sp<ImageBufferHeap> pHeap = ImageBufferHeap::create(
                                                        LOG_TAG,
                                                        imgParam,
                                                        portBufInfo);
        if(pHeap == 0)
        {
            CAM_LOGE("pHeap is NULL");
            return NULL;
        }
        //
        pBuf = pHeap->createImageBuffer();
        pBuf->incStrong(pBuf);

        bufMap.pImgBuf = pBuf;
        mvImgBufMap.push_back(bufMap);
    }
    else
    {
        MINT32 bufBoundaryInBytes = 0;
        IImageBufferAllocator::ImgParam imgParam(
                MSize(w,h),
                w * h * 6 / 5,  //FIXME
                bufBoundaryInBytes
                );

        pBuf = mpIImageBufAllocator->alloc_ion(LOG_TAG, imgParam);
    }
    if (!pBuf || !pBuf->lockBuf( LOG_TAG, eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN ) )
    {
        CAM_LOGE("Null allocated or lock Buffer failed\n");
        return  NULL;
    }

    pBuf->syncCache(eCACHECTRL_INVALID);

    return pBuf;

#endif
}

/******************************************************************************
*
*******************************************************************************/
void
Mhal_facebeauty::
deallocMem(IImageBuffer *pBuf)
{
    BufAllocUtil::getInstance().deAllocMem(LOG_TAG, pBuf);
#if 0
    pBuf->unlockBuf(LOG_TAG);
    if (pBuf->getImgFormat() == eImgFmt_JPEG)
    {
        mpIImageBufAllocator->free(pBuf);
    }
    else
    {
        pBuf->decStrong(pBuf);
        for (Vector<ImageBufferMap>::iterator it = mvImgBufMap.begin();
                it != mvImgBufMap.end();
                it++)
        {
            if (it->pImgBuf == pBuf)
            {
                if (mpIMemDrv->unmapPhyAddr(&it->memBuf)) {
                    CAM_LOGE("mpIMemDrv->unmapPhyAddr() error");
                }
                if (mpIMemDrv->freeVirtBuf(NAME, &it->memBuf))
                //if (mpIMemDrv->freeVirtBuf(&it->memBuf))
                {
                    CAM_LOGE("m_pIMemDrv->freeVirtBuf() error");
                }
                else
                {
                    mvImgBufMap.erase(it);
                }
                break;
            }
        }
    }
#endif
}

/******************************************************************************
*
*******************************************************************************/
IImageBuffer*
Mhal_facebeauty::
resizeMem(IImageBuffer* pBuf, MUINT32 w, MUINT32 h)
{
    if( pBuf->getImgFormat() == eImgFmt_JPEG )
        return NULL;

// TODO just re-allocate memory now
// can not confirm this is alright
MUINT32 fmt =  pBuf->getImgFormat();
deallocMem(pBuf);
return allocMem(fmt, w, h);


#if 0
    CAM_LOGD("[resizeMem] %x: (%d,%d) to (%d,%d)",pBuf,pBuf->getImgSize().w,pBuf->getImgSize().h,w,h);
    for (Vector<ImageBufferMap>::iterator it = mvImgBufMap.begin();
            it != mvImgBufMap.end();
            it++)
    {
        if (it->pImgBuf == pBuf)
        {
            MUINT32 fmt = pBuf->getImgFormat();
            MUINT32 plane = pBuf->getPlaneCount();
            IImageBuffer* pNewBuf;

            pBuf->unlockBuf(LOG_TAG);
            pBuf->decStrong(pBuf);

            /* Map to new ImageBuffer */
            size_t bufBoundaryInBytes[3] = {0, 0, 0};
            size_t bufStridesInBytes[3] = {0};

            for (MUINT32 i = 0; i < plane; i++)
            {
                bufStridesInBytes[i] = NSCam::Utils::Format::queryPlaneWidthInPixels(fmt,i, w) * NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt,i) / 8;
            }
            IImageBufferAllocator::ImgParam imgParam(
                    fmt,
                    MSize(w,h),
                    bufStridesInBytes,
                    bufBoundaryInBytes,
                    plane
                    );

            PortBufInfo_v1 portBufInfo = PortBufInfo_v1(
                                            it->memBuf.memID,
                                            it->memBuf.virtAddr,
                                            it->memBuf.useNoncache,
                                            it->memBuf.bufSecu,
                                            it->memBuf.bufCohe);

            sp<ImageBufferHeap> pHeap = ImageBufferHeap::create(
                                                            LOG_TAG,
                                                            imgParam,
                                                            portBufInfo);
            if(pHeap == 0)
            {
                CAM_LOGE("pHeap is NULL");
                return NULL;
            }
            //
            pNewBuf = pHeap->createImageBuffer();
            pNewBuf->incStrong(pNewBuf);

            if (!pNewBuf || !pNewBuf->lockBuf( LOG_TAG, eBUFFER_USAGE_HW_CAMERA_READWRITE | eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN ) )
            {
                CAM_LOGE("Null allocated or lock Buffer failed\n");
                return  NULL;
            }

            it->pImgBuf = pNewBuf;
            CAM_LOGD("[resizeMem] return %x",pNewBuf);
            return pNewBuf;
        }
    }
    return NULL;
#endif


}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
requestBufs()
{
    //CAM_TRACE_BEGIN("requestBufs");

    MBOOL   fgRet = MFALSE;
    if (!mSDKMode)
    {
        //mu4W_yuv = mShotParam.mi4PictureWidth;
        //mu4H_yuv = mShotParam.mi4PictureHeight;
    }
    else
    {
        mu4W_yuv = mSrcImgBuffer->getImgSize().w;
        mu4H_yuv = mSrcImgBuffer->getImgSize().h;
        if (mu4W_yuv < mu4H_yuv)
        {
            mu4W_yuv = mu4H_yuv;
            mu4H_yuv = mSrcImgBuffer->getImgSize().w;
        }
    }
    #if 0//def Debug_Mode
    mu4W_yuv = 640;
    mu4H_yuv = 480;
    #endif
    if((mu4W_yuv*3) == (mu4H_yuv*4))
    {
        mDSWidth = 640;
        mDSHeight = 480;

        FDWidth = 320;
        FDHeight = 240;
    }
    else if((mu4W_yuv*9) == (mu4H_yuv*16))
    {
        mDSWidth = 640;
        mDSHeight = 360;

        FDWidth = 320;
        FDHeight = 180;
    }
    else if((mu4W_yuv*3) == (mu4H_yuv*5))
    {
        mDSWidth = 640;
        mDSHeight = 384;

        FDWidth = 320;
        FDHeight = 192;

    }
    else
    {
        mDSWidth = 640;
        FDWidth = 320;

        if(mu4W_yuv != 0) {
          mDSHeight = 640 * mu4H_yuv/mu4W_yuv;
          FDHeight = (640 * mu4H_yuv/mu4W_yuv)/2;
        } else {
          mDSHeight = 480;
          FDHeight = 240;
        }
    }

    CAM_LOGD("[requestBufs] mu4W_yuv %d mu4H_yuv %d",mu4W_yuv,mu4H_yuv);
    //  (1)

    //if (mSDKMode || ((mu4W_yuv*mu4H_yuv) >= (mShotParam.mi4PostviewWidth * mShotParam.mi4PostviewHeight)))
    {
        //mpSource = allocMem(eImgFmt_I422, mu4W_yuv, mu4H_yuv);
        CAM_LOGD("[requestBufs] mpSource alloc format I422");
        mpSource = allocMem(eImgFmt_I422, mu4W_yuv, mu4H_yuv);

    }
    //else
    //{
        //mpSource = allocMem(eImgFmt_I422, mShotParam.mi4PostviewWidth, mShotParam.mi4PostviewHeight);
    //}
    if(!mpSource)
    {
        CAM_LOGE("[requestBufs] mpSource alloc fail");
        goto lbExit;
    }
    CAM_LOGD("mpSource:VA(%x,%x,%x),PA(%x,%x,%x)",mpSource->getBufVA(0),mpSource->getBufVA(1),mpSource->getBufVA(2),mpSource->getBufPA(0),mpSource->getBufPA(1),mpSource->getBufPA(2));

    qUtilityStatus.push(3);
    sem_post(&semUtilitythread);

    //CAM_TRACE_END();

    fgRet = MTRUE;
lbExit:
    if  ( ! fgRet )
    {
        releaseBufs();
    }
    return  fgRet;
}


/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
releaseBufs()
{
    deallocMem(mpSource);
    deallocMem(mpBlurImg);
    deallocMem(mpAmap);
    //deallocMem(mpAmapForStep5);
    deallocMem(mpDSImg);
    free(mpWorkingBuferr);

#if 0
    if( mSensorType == SENSOR_TYPE_RAW && mSDKMode != MTRUE)
    {
        CAM_LOGD("RAW sensor: releaseBufs +");
        deallocMem(mpSource);
        deallocMem(mpOutputbuffer);
        uninit_VFB();
        UninitP2b();
        CAM_LOGD("RAW sensor: releaseBufs -");
    } else {
        deallocMem(mpSource);
        deallocMem(mpBlurImg);
        deallocMem(mpAmap);
        //deallocMem(mpAmapForStep5);
        deallocMem(mpDSImg);
        free(mpWorkingBuferr);
    }
#endif
    delete mpFDWorkingBuffer;

    return  MTRUE;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
SaveJpg()
{
#if 0
    //optimize jpg save in step2
    CAM_LOGD(" Save JPG %s",mShotParam.ms8ShotFileName.string());
    if(mShotParam.ms8ShotFileName.string()!=NULL)
    {
        CAM_LOGD("Save JPG");
        sem_init(&semJPGDone, 0, 0);
        qUtilityStatus.push(2);
        sem_post(&semUtilitythread);
    }
#endif
    return  MTRUE;
}

/*******************************************************************************
*
*******************************************************************************/
MBOOL
Mhal_facebeauty::
WaitSaveDone()
{
#if 0
    //optimize jpg save in step2
    CAM_LOGD(" WaitSaveDone %s",mShotParam.ms8ShotFileName.string());
    if(mShotParam.ms8ShotFileName.string()!=NULL)
    {
        sem_wait(&semJPGDone);
    }
#endif
    return  MTRUE;
}

