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

#define LOG_TAG "MtkCam/AsdClient"
//
#include "AsdClient.h"
#include <mtkcam/utils/fwk/MtkCamera.h>
#include <faces.h>
//
using namespace NSCam;
using namespace NSCamClient;
using namespace NSAsdClient;
using namespace NS3Av3;
//
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


/******************************************************************************
 *
 ******************************************************************************/
sp<IAsdClient>
IAsdClient::
createInstance(sp<IParamsManager> pParamsMgr)
{
    return  new AsdClient(pParamsMgr);
}


/******************************************************************************
 *
 ******************************************************************************/
AsdClient::
AsdClient(sp<IParamsManager> pParamsMgr)
    : mModuleMtx()
    , mpCamMsgCbInfo(new CamMsgCbInfo)
    , mpParamsMgr(pParamsMgr)
    , mIsAsdEnabled(0)
    //
    , mpFaceInfo(0)
    , mpWorkingBuf(0)
    , mpHal3A(0)
    , mpHalASDObj(0)
{
    MY_LOGD("+ this(%p)", this);
}


/******************************************************************************
 *
 ******************************************************************************/
AsdClient::
~AsdClient()
{
    MY_LOGD("-");
}


/******************************************************************************
 *
 ******************************************************************************/
bool
AsdClient::
init()
{
    bool ret = true;

    //Please fixed me
    eSensorType = SENSOR_TYPE_RAW;
    //Create Working Buffer
    mpWorkingBuf = (MUINT8*)malloc(MHAL_ASD_WORKING_BUF_SIZE);
    if(mpWorkingBuf == NULL)
    {
        MY_LOGW("memory is not enough");
        return false;
    }

    //Create FD Buffer
    mpFaceInfo = new MtkCameraFaceMetadata;
    if ( NULL != mpFaceInfo )
    {
        MtkCameraFace *faces = new MtkCameraFace[AsdClient::mDetectedFaceNum];
        MtkFaceInfo *posInfo = new MtkFaceInfo[AsdClient::mDetectedFaceNum];

        if ( NULL != faces &&  NULL != posInfo)
        {
            mpFaceInfo->faces = faces;

            mpFaceInfo->posInfo = posInfo;
            mpFaceInfo->number_of_faces = 0;
        }
    }

    mSceneCur = mhal_ASD_DECIDER_UI_AUTO;

    mpHal3A = MAKE_Hal3A(mpParamsMgr->getOpenId(), LOG_TAG);
    //
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
AsdClient::
uninit()
{
    bool ret = true;

    if(mpHalASDObj != NULL)
    {
        mpHalASDObj->mHalAsdUnInit();
        mpHalASDObj->destroyInstance();
        mpHalASDObj = NULL;
    }

    if(mpWorkingBuf != NULL)
    {
        free(mpWorkingBuf);
        mpWorkingBuf = NULL;
    }

    if(mpHal3A != NULL)
    {
        mpHal3A->destroyInstance(LOG_TAG);
        mpHal3A = NULL;
    }

    if ( mpFaceInfo != NULL )
    {
        if ( mpFaceInfo->faces != NULL )
        {
            delete [] mpFaceInfo->faces;
            mpFaceInfo->faces = NULL;
        }

        if ( mpFaceInfo->posInfo != NULL)
        {
            delete [] mpFaceInfo->posInfo;
            mpFaceInfo->posInfo = NULL;
        }

        delete mpFaceInfo;
        mpFaceInfo = NULL;
    }

    mSceneCur = mhal_ASD_DECIDER_UI_AUTO;

    return ret;
}


/******************************************************************************
 * Set camera message-callback information.
 ******************************************************************************/
void
AsdClient::
setCallbacks(sp<CamMsgCbInfo> const& rpCamMsgCbInfo)
{
    Mutex::Autolock _l(mModuleMtx);
    //
    //  value copy
    *mpCamMsgCbInfo = *rpCamMsgCbInfo;
}


/******************************************************************************
 *
 ******************************************************************************/
void
AsdClient::
enable(bool fgEnable)
{
    ::android_atomic_write(fgEnable ? 1 : 0, &mIsAsdEnabled);
}


/******************************************************************************
 *
 ******************************************************************************/
bool
AsdClient::
isEnabled() const
{
    return  0 != ::android_atomic_release_load(&mIsAsdEnabled);
}


/******************************************************************************
 *
 ******************************************************************************/
int
AsdClient::
update(MUINT8 * OT_Buffer, MINT32 a_Buffer_width, MINT32 a_Buffer_height, MINT32 a_FaceNum)
{
    MUINT32 u4Scene = 0;

    ASDInfo_T ASDInfo;
    bool const isAsdEnabled = mpParamsMgr->getShotModeStr() == MtkCameraParameters::CAPTURE_MODE_ASD_SHOT;
    enable(isAsdEnabled);
    if  ( ! isEnabled() )
    {
        return -1;
    }

    if (mpHal3A)
    {
        mpHal3A->send3ACtrl(E3ACtrl_GetAsdInfo, (MINTPTR)(&ASDInfo), 0);
    }

    //Asd Pipe Init.
    if(mpHalASDObj == NULL)
    {
        mpHalASDObj = halASDBase::createInstance(HAL_ASD_OBJ_AUTO);
        if(mpHalASDObj == NULL)
        {
            MY_LOGE("mpHalASDObj createInstance fail");
        }
        mpHalASDObj->mHalAsdInit((void*)&ASDInfo, mpWorkingBuf, (eSensorType==SENSOR_TYPE_RAW)?0:1, a_Buffer_height, a_Buffer_height);
    }

    //Asd Pipe Decider
    mpHalASDObj->mHalAsdDecider((void*)&ASDInfo, a_FaceNum ,mSceneCur);
    u4Scene = mSceneCur;
    MY_LOGD("u4Scene:%d ", u4Scene);
    mpHalASDObj->mHalAsdDoSceneDet((void*)OT_Buffer, a_Buffer_height, a_Buffer_height);

    return u4Scene;
}

