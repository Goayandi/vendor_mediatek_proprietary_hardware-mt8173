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

#define LOG_TAG "MtkCam/devicemgr"
//
#include <cutils/properties.h>
#include "MyUtils.h"
#include <mtkcam/main/hal/Cam1Device.h>
#include <mtkcam/main/hal/Cam3Device.h>
//
//
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


/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerBase::
OpenInfo::
~OpenInfo()
{
}


/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerBase::
OpenInfo::
OpenInfo()
    : RefBase()
    , pDevice(0)
    , uDeviceVersion(0)
    , i8OpenTimestamp(0)
{
}


/******************************************************************************
 *
 ******************************************************************************/
static
String8 const
queryClientAppMode()
{
    /*
    Before opening camera, client must call
    Camera::setProperty(
        String8(MtkCameraParameters::PROPERTY_KEY_CLIENT_APPMODE),
        String8(MtkCameraParameters::APP_MODE_NAME_MTK_xxx)
    ),
    where MtkCameraParameters::APP_MODE_NAME_MTK_xxx = one of the following:
        MtkCameraParameters::APP_MODE_NAME_DEFAULT
        MtkCameraParameters::APP_MODE_NAME_MTK_ENG
        MtkCameraParameters::APP_MODE_NAME_MTK_ATV
        MtkCameraParameters::APP_MODE_NAME_MTK_S3D
        MtkCameraParameters::APP_MODE_NAME_MTK_VT
    */
    String8 const s8ClientAppModeKey(MtkCameraParameters::PROPERTY_KEY_CLIENT_APPMODE);
    String8       s8ClientAppModeVal(MtkCameraParameters::APP_MODE_NAME_DEFAULT);

    //  (1) get Client's property.
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    //
    ::property_get("client.appmode", value, "");
    s8ClientAppModeVal = value;
    MY_LOGD("device: %s %s", s8ClientAppModeKey.string(), s8ClientAppModeVal.string());
    //
    if  ( s8ClientAppModeVal.isEmpty() ) {

        //  (1) get Client's property.
        Utils::Property::tryGet(s8ClientAppModeKey, s8ClientAppModeVal);
        //
        MY_LOGD("UtilsGet - device: %s %s", s8ClientAppModeKey.string(), s8ClientAppModeVal.string());
        //
        if  ( s8ClientAppModeVal.isEmpty() ) {
            s8ClientAppModeVal = MtkCameraParameters::APP_MODE_NAME_DEFAULT;
        }
        //
        //  (2) reset Client's property.
        Utils::Property::set(s8ClientAppModeKey, String8::empty());
        MY_LOGD("UtilsSet - device: %s %s", s8ClientAppModeKey.string(), s8ClientAppModeVal.string());
    }
    //
    return  s8ClientAppModeVal;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
CamDeviceManagerBase::
detachDeviceLocked(android::sp<ICamDevice> pDevice)
{
    sp<OpenInfo> pOpenInfo;
    int32_t const openId = pDevice->getOpenId();
    //
    String8 const s8ClientAppMode = queryClientAppMode();
    //
    ssize_t const index = mOpenMap.indexOfKey(openId);
#if MTKCAM_HAVE_CAMERAMOUNT
    // check external device
    if( index < 0 ){
        MY_LOGD("Not local deviceId:%d", openId);
        return closeExternalDevice(pDevice);
    }
#endif
    pOpenInfo = mOpenMap.valueAt(index);
    if  (
            index < 0
        ||  pOpenInfo == 0
        ||  pOpenInfo->pDevice != pDevice
        )
    {
        MY_LOGE("device %d: not found!!! mOpenMap.size:%zu index:%zd pOpenInfo:%p", openId, mOpenMap.size(), index, pOpenInfo.get());
        MY_LOGE_IF(pOpenInfo != 0, "device %p %p", pOpenInfo->pDevice.get(), pDevice.get());
        return  NAME_NOT_FOUND;
    }
    //
    mOpenMap.removeItemsAt(index);
    MY_LOGD("device: %s %d", pDevice->getDevName(), pDevice->getOpenId());
    // Unlock torch when all cameras closed
    setTorchModeLocked(openId, false, false);
    setTorchAvailableLocked(openId, true);
    //

    // set Android log detect to default level
    setLogLevelToEngLoad(((0 == mOpenMap.size())?1:0),0);

    return  OK;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
CamDeviceManagerBase::
attachDeviceLocked(android::sp<ICamDevice> pDevice, uint32_t device_version)
{
    sp<OpenInfo> pOpenInfo;
    int32_t const openId = pDevice->getOpenId();
    //
    //
    pOpenInfo = mOpenMap.valueFor(openId);
    if  ( pOpenInfo != 0 )
    {
        sp<ICamDevice> const pDev = pOpenInfo->pDevice;
        MY_LOGE(
            "Busy deviceId:%d; device:%p has already been opend with version:0x%x OpenTimestamp:%" PRId64,
            openId, pDev.get(), pOpenInfo->uDeviceVersion, pOpenInfo->i8OpenTimestamp
        );
        MY_LOGE_IF(pDev != 0, "device: %s %d", pDev->getDevName(), pDev->getOpenId());

        return  ALREADY_EXISTS;
    }
    //
    //
    pOpenInfo = new OpenInfo;
    pOpenInfo->pDevice = pDevice;
    pOpenInfo->uDeviceVersion = device_version;
    pOpenInfo->i8OpenTimestamp = ::systemTime();
    //
    mOpenMap.add(openId, pOpenInfo);
    MY_LOGD(
        "device: %s %d version:0x%x OpenTimestamp:%" PRId64,
        pDevice->getDevName(),
        pDevice->getOpenId(),
        pOpenInfo->uDeviceVersion,
        pOpenInfo->i8OpenTimestamp
    );
    // Lock torch when first camera opened
    setTorchModeLocked(openId, false, false);
    setTorchAvailableLocked(openId, false);
    //

    // set Android log detect to higher level
    setLogLevelToEngLoad(((1 == mOpenMap.size())?1:0),1);

    return  OK;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
CamDeviceManagerBase::
validateOpenLocked(int32_t i4OpenId, uint32_t device_version) const
{
    sp<EnumInfo> pEnumInfo = mEnumMap.valueFor(i4OpenId);
    if  ( pEnumInfo == 0 ) {
        MY_LOGE(
            "Bad OpenId:%d - version:0x%x mEnumMap.size:%zu DeviceNum:%d",
            i4OpenId, device_version, mEnumMap.size(), mi4DeviceNum
        );
        /*
         * -EINVAL:     The input arguments are invalid, i.e. the id is invalid,
         *              and/or the module is invalid.
         */
        return -EINVAL;
    }
    //
    sp<OpenInfo> pOpenInfo = mOpenMap.valueFor(i4OpenId);
    if  ( pOpenInfo != 0 ) {
        sp<ICamDevice> const pDev = pOpenInfo->pDevice;
        MY_LOGE(
            "Busy deviceId:%d; device:%p has already been opend with version:0x%x OpenTimestamp:%" PRId64,
            i4OpenId, pDev.get(), pOpenInfo->uDeviceVersion, pOpenInfo->i8OpenTimestamp
        );
        MY_LOGE_IF(pDev != 0, "device: %s %d", pDev->getDevName(), pDev->getOpenId());
        /*
         * -EBUSY:      The camera device was already opened for this camera id
         *              (by using this method or common.methods->open method),
         *              regardless of the device HAL version it was opened as.
         */
        return -EBUSY;
    }
    //
    return  validateOpenLocked(i4OpenId);
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
CamDeviceManagerBase::
closeDeviceLocked(android::sp<ICamDevice> pDevice)
{
    //  reset Client's property.
    String8 const s8ClientAppModeKey(MtkCameraParameters::PROPERTY_KEY_CLIENT_APPMODE);
    Utils::Property::set(s8ClientAppModeKey, String8::empty());

    return  detachDeviceLocked(pDevice);
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
CamDeviceManagerBase::
openDeviceLocked(
    hw_device_t** device,
    hw_module_t const* module,
    int32_t const i4OpenId,
    uint32_t device_version
)
{
    status_t status = OK;
    sp<ICamDevice> pDevice = NULL;
    //
    String8 const s8ClientAppMode = queryClientAppMode();
    //
    MY_LOGD(
        "+ OpenId:%d with version 0x%x - mOpenMap.size:%zu mEnumMap.size:%zu",
        i4OpenId, device_version, mOpenMap.size(), mEnumMap.size()
    );
    //
    //  [1] check to see whether it's ready to open.
    if  ( OK != (status = validateOpenLocked(i4OpenId, device_version)) )
    {
        return  status;
    }
    //
    //  [2] create device based on device version.
    if  ( device_version == CAMERA_DEVICE_API_VERSION_1_0 )
    {
        int i4DebugOpenID = -1;
        // try get property from system property
        char value[PROPERTY_VALUE_MAX] = {'\0'};
        property_get( "debug.camera.open", value, "-1");
        i4DebugOpenID = atoi(value);
        if( i4DebugOpenID != -1 )
        {
            pDevice = ::createCam1Device(s8ClientAppMode, i4DebugOpenID);
            MY_LOGD("force to open camera:%d", i4DebugOpenID);
        }
        else
        {
            pDevice = ::createCam1Device(s8ClientAppMode, i4OpenId);
        }
    }
    else
    if  ( device_version >= CAMERA_DEVICE_API_VERSION_3_0 )
    {
        pDevice = ::createCam3Device(s8ClientAppMode, i4OpenId);
    }
    else
    {
        MY_LOGE("Unsupported version:0x%x", device_version);
        return  -EOPNOTSUPP;
    }
    //
    if  ( pDevice == 0 )
    {
        MY_LOGE("device creation failure");
        return  -ENODEV;
    }
    //
    //  [4] open device successfully.
    {
        *device = const_cast<hw_device_t*>(pDevice->get_hw_device());
        //
        pDevice->set_hw_module(module);
        pDevice->set_module_callbacks(mpModuleCallbacks);
        pDevice->setDeviceManager(this);
        //
        attachDeviceLocked(pDevice, device_version);
    }
    //
    return  OK;
}
