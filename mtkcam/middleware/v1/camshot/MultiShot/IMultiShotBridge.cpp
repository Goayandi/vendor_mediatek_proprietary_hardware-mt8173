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
#define LOG_TAG "CamShot/MultiShot"
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/def/common.h>
using namespace android;
#define MY_LOGV(fmt, arg...)    CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)    CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)    CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)    CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)    CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)
//
//#include <common/hw/hwstddef.h>
//
#include <mtkcam/middleware/v1/camshot/_callbacks.h>
#include <mtkcam/middleware/v1/camshot/_params.h>

#include <mtkcam/middleware/v1/camshot/ICamShot.h>
#include <mtkcam/middleware/v1/camshot/IMultiShot.h>
//
#include <mtkcam/utils/metadata/IMetadata.h>
using namespace NSCam;
//
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineBuilder.h>
using namespace NSCam::v1::NSLegacyPipeline;
#include <mtkcam/utils/hw/HwInfoHelper.h>
using namespace NSCamHW;

#include "../inc/CamShotImp.h"
#include "../inc/MultiShot.h"
//
#include <utils/Mutex.h>

/*******************************************************************************
*
********************************************************************************/
namespace NSCamShot {
////////////////////////////////////////////////////////////////////////////////


/*******************************************************************************
*
********************************************************************************/
class IMultiShotBridge : public IMultiShot
{
    friend  class   IMultiShot;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  ////
    //mutable android::Mutex      mLock;
    //android::Mutex&             getLockRef()    { return mLock; }
    MUINT32                     mu4InitRefCount;

protected:  ////    Implementor.
    sp<MultiShot>               mpMultiShotImp;
    inline  sp<MultiShot> const getImp() const  { return mpMultiShotImp; }
    inline  sp<MultiShot>       getImp()        { return mpMultiShotImp; }

protected:  ////    Constructor/Destructor.
                    IMultiShotBridge(MultiShot*const pMultiShot);
                    ~IMultiShotBridge();

private:    ////    Disallowed.
                    IMultiShotBridge(IMultiShotBridge const& obj);
    IMultiShotBridge&  operator=(IMultiShotBridge const& obj);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////    Instantiation.
    virtual MVOID   destroyInstance();
    virtual MBOOL   init();
    virtual MBOOL   uninit();

public:     ////    Attributes.
    virtual char const* getCamShotName() const;
    virtual EShotMode   getShotMode() const;
    virtual MINT32      getLastErrorCode() const;

public:     ////    Callbacks.
    virtual MVOID   setCallbacks(CamShotNotifyCallback_t notify_cb, CamShotDataCallback_t data_cb, MVOID* user);
    //
    //  notify callback
    virtual MBOOL   isNotifyMsgEnabled(MINT32 const i4MsgTypes) const;
    virtual MVOID   enableNotifyMsg(MINT32 const i4MsgTypes);
    virtual MVOID   disableNotifyMsg(MINT32 const i4MsgTypes);
    //
    //  data callback
    virtual MBOOL   isDataMsgEnabled(MINT32 const i4MsgTypes) const;
    virtual MVOID   enableDataMsg(MINT32 const i4MsgTypes);
    virtual MVOID   disableDataMsg(MINT32 const i4MsgTypes);

public:     ////    Operations.
    virtual MBOOL   start(SensorParam const & rSensorParam, MUINT32 u4ShotCount=0xFFFFFFFF);
    virtual MBOOL   startAsync(SensorParam const & rSensorParam) ;
    virtual MBOOL   startOne(SensorParam const & /*rSensorParam*/) {
                        return MFALSE;
                    }
    virtual MBOOL   startOne(SensorParam const & /*rSensorParam*/, IImageBuffer const * /*pImgBuffer*/) {
                        return MFALSE;
                    }
    virtual MBOOL   startOne(SensorParam const & /*rSensorParam*/, StreamBufferProvider* /*pProvider*/) {
                        return MFALSE;
                    }
    virtual MBOOL   startOne(SensorParam const & /*rSensorParam*/, StreamBufferProvider* /*pProvider*/, IMetadata* /*rOverwrite = NULL*/) {
                        return MFALSE;
                    }
    virtual MBOOL   startOne(SensorParam const & /*rSensorParam*/, Vector<StreamBufferProvider*>& /*pProviders*/) {
                        return MFALSE;
                    }
    virtual MBOOL   startOne(SensorParam const & /*rSensorParam*/,
                            IImageBuffer const * /*pImgBuf_0*/,
                            IImageBuffer const * /*pImgBuf_1*/,
                            IImageBuffer const * /*pBufPrv_0*/,
                            IImageBuffer const * /*pBufPrv_1*/) {
                        return MFALSE;
                    }
    virtual MBOOL   stop();


public:     ////    Settings.
    virtual MBOOL   setShotParam(ShotParam const & rParam);
    virtual MBOOL   setJpegParam(JpegParam const & rParam);

public:     ////    buffer setting.
    virtual MBOOL   registerImageBuffer(ECamShotImgBufType, IImageBuffer const *pImgBuffer);

public:     ////    Old style commnad.
    virtual MBOOL   sendCommand(MINT32 cmd, MINT32 arg1, MINT32 arg2, MINT32 arg3 , MVOID* arg4 = NULL);

public:
    virtual MBOOL   getResultMetadata(IMetadata & /*rAppResultMetadata*/, IMetadata & /*rHalResultMetadata*/) { return MFALSE;}

};


/*******************************************************************************
*
********************************************************************************/
IMultiShot*
IMultiShot::
createInstance(EShotMode const eShotMode, char const*const szCamShotName, EPipelineMode const ePipelineMode)
{
    MultiShot* pMultiShotImp = new MultiShot(eShotMode, szCamShotName, ePipelineMode);
    if  ( ! pMultiShotImp )
    {
        MY_LOGE("[IMultiShot] fail to new MultiShot");
        return  NULL;
    }
    //
    IMultiShotBridge*  pIMultiShot = new IMultiShotBridge(pMultiShotImp);
    if  ( ! pIMultiShot )
    {
        MY_LOGE("[IMultiShot] fail to new IMultiShotBridge");
        delete  pMultiShotImp;
        return  NULL;
    }
    //
    return  pIMultiShot;
}


/*******************************************************************************
*
********************************************************************************/
MVOID
IMultiShotBridge::
destroyInstance()
{
    mpMultiShotImp = NULL;
    delete  this;
}


/*******************************************************************************
*
********************************************************************************/
IMultiShotBridge::
IMultiShotBridge(MultiShot*const pMultiShot)
    : IMultiShot()
    //, mLock()
    , mu4InitRefCount(0)
    , mpMultiShotImp(pMultiShot)
{
}


/*******************************************************************************
*
********************************************************************************/
IMultiShotBridge::
~IMultiShotBridge()
{
}


/*******************************************************************************
*
********************************************************************************/
char const*
IMultiShotBridge::
getCamShotName() const
{
    return  getImp()->getCamShotName();
}


/*******************************************************************************
*
********************************************************************************/
EShotMode
IMultiShotBridge::
getShotMode() const
{
    return  getImp()->getShotMode();
}


/*******************************************************************************
*
********************************************************************************/
MINT32
IMultiShotBridge::
getLastErrorCode() const
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->getLastErrorCode();
}


/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
init()
{
    MBOOL   ret = MTRUE;
    //Mutex::Autolock _lock(mLock);

    if  ( 0 != mu4InitRefCount )
    {
        mu4InitRefCount++;
    }
    else if ( (ret = getImp()->init()) )
    {
        mu4InitRefCount = 1;
    }
    MY_LOGD("- mu4InitRefCount(%d), ret(%d)", mu4InitRefCount, ret);
    return  ret;
}


/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
uninit()
{
    MBOOL   ret = MTRUE;
    //Mutex::Autolock _lock(mLock);

    if  ( 0 < mu4InitRefCount )
    {
        mu4InitRefCount--;
        if  ( 0 == mu4InitRefCount )
        {
            ret = getImp()->uninit();
        }
    }
    MY_LOGD("- mu4InitRefCount(%d), ret(%d)", mu4InitRefCount, ret);
    return  ret;
}


/*******************************************************************************
*
********************************************************************************/
MVOID
IMultiShotBridge::
setCallbacks(CamShotNotifyCallback_t notify_cb, CamShotDataCallback_t data_cb, MVOID* user)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->setCallbacks(notify_cb, data_cb, user);
}


/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
isNotifyMsgEnabled(MINT32 const i4MsgTypes) const
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->isNotifyMsgEnabled(i4MsgTypes);
}


/*******************************************************************************
*
********************************************************************************/
MVOID
IMultiShotBridge::
enableNotifyMsg(MINT32 const i4MsgTypes)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->enableNotifyMsg(i4MsgTypes);
}


/*******************************************************************************
*
********************************************************************************/
MVOID
IMultiShotBridge::
disableNotifyMsg(MINT32 const i4MsgTypes)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->disableNotifyMsg(i4MsgTypes);
}


/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
isDataMsgEnabled(MINT32 const i4MsgTypes) const
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->isDataMsgEnabled(i4MsgTypes);
}


/*******************************************************************************
*
********************************************************************************/
MVOID
IMultiShotBridge::
enableDataMsg(MINT32 const i4MsgTypes)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->enableDataMsg(i4MsgTypes);
}


/*******************************************************************************
*
********************************************************************************/
MVOID
IMultiShotBridge::
disableDataMsg(MINT32 const i4MsgTypes)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->disableDataMsg(i4MsgTypes);
}


/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
start(SensorParam const & rSensorParam, MUINT32 u4ShotCount)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->start(rSensorParam, u4ShotCount);
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
startAsync(SensorParam const & rSensorParam)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->startAsync(rSensorParam);
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
stop()
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->stop();
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
setShotParam(ShotParam const & rParam)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->setShotParam(rParam);
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
setJpegParam(JpegParam const & rParam)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->setJpegParam(rParam);
}


/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
registerImageBuffer(ECamShotImgBufType const eBufType, IImageBuffer const *pImgBuffer)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->registerImageBuffer(eBufType, pImgBuffer);
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
IMultiShotBridge::
sendCommand(MINT32 cmd, MINT32 arg1, MINT32 arg2, MINT32 arg3, MVOID* arg4)
{
    //Mutex::Autolock _lock(mLock);
    return  getImp()->sendCommand(cmd, arg1, arg2, arg3, arg4);
}

////////////////////////////////////////////////////////////////////////////////
};  //namespace NSCamShot

