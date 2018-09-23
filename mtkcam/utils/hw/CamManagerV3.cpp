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

#define LOG_TAG "MtkCam/Utils/CamMgrV3"
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/hw/CamManagerV3.h>
//
using namespace NSCam::Utils;
//
#include <stdlib.h>

/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s] " fmt, __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
//

class CamManagerV3Imp
    : public CamManagerV3
{

public:
    struct eisOwner {
                                eisOwner()
                                    : mEnable(MFALSE)
                                    , mOpenId(-1)
                                {}

        MVOID                   clear()
                                {
                                    mEnable = MFALSE;
                                    mOpenId = -1;
                                }
                                //
        MBOOL                   mEnable;
        MINT32                  mOpenId;
    };

public:     ////                Operations.
                                CamManagerV3Imp()
                                    : mLock()
                                    , mvDevice()
                                    , mEisOwner()
                                {
                                    MY_LOGD("CamManagerV3 start");
                                    mvDevice.clear();
                                }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                Operations.
    //
    virtual MBOOL               getEisOwner(MINT32 openId);
    virtual MBOOL               releaseEisOwner(MINT32 openId);
    virtual MBOOL               isEisOwner(MINT32 openId);
    //
    virtual MINT32              incDevice(MINT32 openId, const char* userName=0);
    virtual MINT32              decDevice(MINT32 openId, const char* userName=0);
    //
    virtual MBOOL               isMultiDevice();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Data Members.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    mutable Mutex                       mLock;
    DefaultKeyedVector<MUINT32, MINT32> mvDevice;

    // eis
    mutable Mutex                       mLockEis;
    eisOwner                            mEisOwner;

    // pip
};

static CamManagerV3Imp singleton;
//
//
/******************************************************************************
 *
 ******************************************************************************/
CamManagerV3*
CamManagerV3::
getInstance()
{
    return &singleton;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
CamManagerV3Imp::
getEisOwner(MINT32 openId)
{
     Mutex::Autolock _l(mLockEis);
     if ( mEisOwner.mEnable )
     {
        MY_LOGW( "[dev:%d] current eis owner: device(%d)",
                 openId, mEisOwner.mOpenId);
        return MFALSE;
     }
     mEisOwner.mOpenId = openId;
     mEisOwner.mEnable = MTRUE;
     MY_LOGW( "[dev:%d] get eis owner", openId);
     return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
CamManagerV3Imp::
releaseEisOwner(MINT32 openId)
{
     Mutex::Autolock _l(mLockEis);
     if ( mEisOwner.mEnable && mEisOwner.mOpenId== openId )
     {
        MY_LOGD( "[dev:%d] release eis ownership", openId);
        mEisOwner.clear();
        return MTRUE;
     }
     MY_LOGW( "[dev:%d] release fail, current eis ownership(%d:%d)",
              openId, mEisOwner.mOpenId, mEisOwner.mEnable);
     return MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
CamManagerV3Imp::
isEisOwner(MINT32 openId)
{
    Mutex::Autolock _l(mLockEis);
    return (mEisOwner.mEnable && mEisOwner.mOpenId== openId)? MTRUE : MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
CamManagerV3Imp::
incDevice(MINT32 openId, const char* userName)
{
    Mutex::Autolock _l(mLock);
    ssize_t index = mvDevice.indexOfKey(openId);
    if ( index<0 )
        mvDevice.add(openId, 1);
    else
        mvDevice.replaceValueAt(index, mvDevice[(size_t)index]+1);
    //
    MY_LOGD("[%s] dev(%d) count: %d", userName, openId, mvDevice.valueFor(openId));
    return mvDevice.valueFor(openId);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
CamManagerV3Imp::
decDevice(MINT32 openId, const char* userName)
{
    Mutex::Autolock _l(mLock);
    ssize_t index = mvDevice.indexOfKey(openId);
    if ( index<0 )
    {
        MY_LOGE("[%s] dev(%d) not inited", userName, openId);
        return -1;
    }
    else if ( mvDevice[(size_t)index]==0 )
    {
        MY_LOGE("[%s] dev(%d) has no current user: %d", userName, openId, mvDevice[(size_t)index]);
        return -1;
    }
    else if ( mvDevice[(size_t)index]==1 )
    {   // final one, remove device
        if ( isEisOwner(openId) )
            releaseEisOwner(openId);
        //
        mvDevice.removeItem(openId);
        MY_LOGD("[%s] dev(%d) count: %d", userName, openId, 0);
        return 0;
    }
    //
    mvDevice.replaceValueAt(index, mvDevice[(size_t)index]-1);
    MY_LOGD("[%s] dev(%d) count: %d", userName, openId, mvDevice.valueFor(openId));
    return mvDevice.valueFor(openId);
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
CamManagerV3Imp::
isMultiDevice()
{
    Mutex::Autolock _l(mLock);
    // for( size_t i = 0; i<mvDevice.size(); i++)
    //     MY_LOGD("dev(%d:%d:%d)", i, mvDevice.keyAt(i), mvDevice.valueAt(i) );
    return ( mvDevice.size()>1 )? MTRUE : MFALSE;
}
