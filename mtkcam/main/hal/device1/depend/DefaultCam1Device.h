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

#ifndef _MTK_HARDWARE_MTKCAM_MAIN_HAL_DEVICE1_DEPEND_DEFAULTCAM1DEVICE_H_
#define _MTK_HARDWARE_MTKCAM_MAIN_HAL_DEVICE1_DEPEND_DEFAULTCAM1DEVICE_H_

#include "Cam1DeviceBase.h"
//
#include <mtkcam/drv/IHalSensor.h>
//
#if '1'==MTKCAM_HAVE_3A_HAL
#include <mtkcam/aaa/IHal3A.h>
#endif
//
#include <pthread.h>


/******************************************************************************
 *
 ******************************************************************************/
namespace android {


/******************************************************************************
 *
 ******************************************************************************/
class DefaultCam1Device : public Cam1DeviceBase
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Data Members.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  ////
    //
    #define USER_NAME               "DefaultCam1Device"
    //
    NSCam::IHalSensor*              mpHalSensor;
    //
#if '1'==MTKCAM_HAVE_3A_HAL
    NS3Av3::IHal3A*                 mpHal3a;
#endif
    bool                            mbThreadRunning;
    bool                            mRet;
    pthread_t                       mThreadHandle;
    bool                            mDisableWaitSensorThread;
    //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Instantiation.

    virtual                         ~DefaultCam1Device();
                                    DefaultCam1Device(
                                        String8 const&          rDevName,
                                        int32_t const           i4OpenId
                                    );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  [Template method] Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  ////                    Operations.

    /**
     * [Template method] Called by initialize().
     * Initialize the device resources owned by this object.
     */
    virtual bool                    onInit();

    /**
     * [Template method] Called by uninitialize().
     * Uninitialize the device resources owned by this object. Note that this is
     * *not* done in the destructor.
     */
    virtual bool                    onUninit();

    /**
     * [Template method] Called by startPreview().
     */
    virtual bool                    onStartPreview();

    /**
     * [Template method] Called by stopPreview().
     */
    virtual void                    onStopPreview();

    /**
     * [Template method] Called by startRecording().
     */
    virtual bool                    onStartRecording();

    /**
     * [Template method] Called by setParameters().
     */
    virtual status_t                onSetParameters(const char* params);

    /**
     * to power on the sensor
     */
    virtual bool                    powerOnSensor();

    /**
     * the init function to be called in the thread
     */
    static void*                    doThreadInit(void* arg);

    /**
     * Wait for initialization in the thread done.
     */
    virtual bool                    waitThreadInitDone();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Cam1Device Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////
    virtual int32_t                 queryDisplayBufCount() const    { return 6; }
    virtual bool                    disableWaitSensorThread(bool diable);

};


/******************************************************************************
 *
 ******************************************************************************/
};  //namespace android
#endif  //_MTK_HARDWARE_MTKCAM_MAIN_HAL_DEVICE1_DEPEND_DEFAULTCAM1DEVICE_H_

