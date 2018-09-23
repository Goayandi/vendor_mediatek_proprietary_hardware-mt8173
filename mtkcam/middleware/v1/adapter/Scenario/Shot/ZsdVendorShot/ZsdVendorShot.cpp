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

#define LOG_TAG "MtkCam/Shot"
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Trace.h>
//


//
#include <mtkcam/drv/IHalSensor.h>
//
#include <mtkcam/middleware/v1/camshot/ICamShot.h>
#include <mtkcam/middleware/v1/camshot/ISingleShot.h>
#include <mtkcam/middleware/v1/camshot/ISmartShot.h>
//
#include <mtkcam/middleware/v1/IShot.h>
//
#include "ImpShot.h"
#include "ZsdVendorShot.h"
#include <ExifJpegUtils.h>
//
#include <mtkcam/utils/hw/CamManager.h>
using namespace NSCam::Utils;
//
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
#include <mtkcam/middleware/v1/camshot/_params.h>
//
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/ILegacyPipeline.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <utils/Vector.h>
//
#include <mtkcam/aaa/IDngInfo.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineUtils.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineBuilder.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProviderFactory.h>

#include <mtkcam/aaa/IHal3A.h>

using namespace android;
using namespace NSShot;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace NSCamHW;
using namespace NS3Av3;

#define CHECK_OBJECT(x)  do{                                        \
    if (x == nullptr) { MY_LOGE("Null %s Object", #x); return MFALSE;} \
} while(0)

#include <cutils/properties.h>
#define DUMP_KEY  "debug.zsdvendorshot.dump"
#define DUMP_PATH "/sdcard/zsdvendorshot"
#define VENDORSHOT_SOURCE_COUNT     (1)
/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("(%d)(%s)[%s] " fmt, ::gettid(), getShotName(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
#define FUNC_START                  MY_LOGD("+")
#define FUNC_END                    MY_LOGD("-")

/******************************************************************************
 *
 ******************************************************************************/
extern
sp<IShot>
createInstance_ZsdVendorShot(
    char const*const    pszShotName,
    uint32_t const      u4ShotMode,
    int32_t const       i4OpenId
)
{
    sp<IShot>  pShot            = NULL;
    sp<ZsdVendorShot>  pImpShot = NULL;
    //
    //  (1.1) new Implementator.
    pImpShot = new ZsdVendorShot(pszShotName, u4ShotMode, i4OpenId);
    if  ( pImpShot == 0 ) {
        CAM_LOGE("[%s] new ZsdVendorShot", __FUNCTION__);
        goto lbExit;
    }
    //
    //  (1.2) initialize Implementator if needed.
    if  ( ! pImpShot->onCreate() ) {
        CAM_LOGE("[%s] onCreate()", __FUNCTION__);
        goto lbExit;
    }
    //
    //  (2)   new Interface.
    pShot = new IShot(pImpShot);
    if  ( pShot == 0 ) {
        CAM_LOGE("[%s] new IShot", __FUNCTION__);
        goto lbExit;
    }
    //
lbExit:
    //
    //  Free all resources if this function fails.
    if  ( pShot == 0 && pImpShot != 0 ) {
        pImpShot->onDestroy();
        pImpShot = NULL;
    }
    //
    return  pShot;
}

/******************************************************************************
 *
 ******************************************************************************/
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata const* const pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if( pMetadata == NULL ) {
        //MY_LOGE("pMetadata == NULL");
        return MFALSE;
    }

    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if( !entry.isEmpty() ) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    return MFALSE;
}


/******************************************************************************
 *  This function is invoked when this object is firstly created.
 *  All resources can be allocated here.
 ******************************************************************************/
bool
ZsdVendorShot::
onCreate()
{
    bool ret = true;
    return ret;
}


/******************************************************************************
 *  This function is invoked when this object is ready to destryoed in the
 *  destructor. All resources must be released before this returns.
 ******************************************************************************/
void
ZsdVendorShot::
onDestroy()
{

}

/******************************************************************************
 *
 ******************************************************************************/
ZsdVendorShot::
ZsdVendorShot(
    char const*const pszShotName,
    uint32_t const u4ShotMode,
    int32_t const i4OpenId
)
    : ImpShot(pszShotName, u4ShotMode, i4OpenId)
    , mpPipeline(NULL)
    , mpJpegPool()
    , mCapReqNo(0)
    , mEncDoneCond()
    , mEncJobLock()
    , mEncJob()
{
    mDumpFlag = ::property_get_int32(DUMP_KEY, 0);
    if( mDumpFlag ) {
        MY_LOGD("enable dump flag 0x%x", mDumpFlag);
        mkdir(DUMP_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    }
}


/******************************************************************************
 *
 ******************************************************************************/
ZsdVendorShot::
~ZsdVendorShot()
{
    MY_LOGD("~ZsdVendorShot()");
    if( mResultMetadataSetMap.size() > 0 )
    {
        int n = mResultMetadataSetMap.size();
        for(int i=0; i<n; i++)
        {
            MY_LOGW("requestNo(%d) doesn't clear before ZsdVendorShot destroyed",mResultMetadataSetMap.keyAt(i));
            mResultMetadataSetMap.editValueAt(i).selectorGetBufs.clear();
        }
    }
    mResultMetadataSetMap.clear();
}


/******************************************************************************
 *
 ******************************************************************************/
bool
ZsdVendorShot::
sendCommand(
    uint32_t const  cmd,
    MUINTPTR const  arg1,
    uint32_t const  arg2,
    uint32_t const  arg3
)
{
    bool ret = true;
    //
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
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
ZsdVendorShot::
onCmd_reset()
{
    bool ret = true;
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
bool
ZsdVendorShot::
onCmd_capture()
{
    CAM_TRACE_NAME("Zsd vendor Capture");

    // TODO: decide different shot. Ex. zsh with flash light, HDR..
    MINT32 type;
    decideSettingType(type);

    // prepare necessary setting for capture
    // 1. get sensor size
    // 2. get streamBufferProvider
    // 3. runtime allocate full raw buffer [optional]
    // TODO
    MINT32 runtimeAllocateCount = 0;
    beginCapture( runtimeAllocateCount );
    //
    // decide raw buffer source & count
    // TODO: add different setting here [optional]
    MINT32 shotCount = VENDORSHOT_SOURCE_COUNT;
    MINT32 YuvBufferCount = shotCount;
    Vector<NS3Av3::CaptureParam_T> vHdrCaptureParams;
    mEncJob.setSourceCnt(shotCount);
    {
        MBOOL res = MTRUE;
        //
        Vector< SettingSet > vSettings;
        switch(type) {
            // zsd + flash light
            case SETTING_FLASH_ACQUIRE:
                shotCount = 1;
                mEncJob.setSourceCnt(shotCount);
                //
                vSettings.push_back(
                    SettingSet{
                        .appSetting = mShotParam.mAppSetting,
                        .halSetting = mShotParam.mHalSetting
                    }
                );
                //
                // submit to zsd preview pipeline
                res = applyRawBufferSettings( vSettings, shotCount );
                break;
            case SETTING_HDR:
                {
                    updateCaptureParams(shotCount, vHdrCaptureParams);
                    mEncJob.setSourceCnt(shotCount);
                    for ( size_t i = 0; i < vHdrCaptureParams.size(); ++i) {
                        IMetadata appSetting = mShotParam.mAppSetting;
                        IMetadata halSetting = mShotParam.mHalSetting;
                        {
                            IMetadata::Memory capParams;
                            capParams.resize(sizeof(NS3Av3::CaptureParam_T));
                            memcpy(capParams.editArray(), &vHdrCaptureParams[i],
                                    sizeof(NS3Av3::CaptureParam_T));

                            IMetadata::IEntry entry(MTK_3A_AE_CAP_PARAM);
                            entry.push_back(capParams, Type2Type< IMetadata::Memory >());
                            halSetting.update(entry.tag(), entry);
                        }
                        {
                            // pause AF for (N - 1) frames and resume for the last frame
                            IMetadata::IEntry entry(MTK_FOCUS_PAUSE);
                            entry.push_back(((i+1)==shotCount) ? 0 : 1, Type2Type< MUINT8 >());
                            halSetting.update(entry.tag(), entry);
                        }
                        {
                            IMetadata::IEntry entry(MTK_CONTROL_SCENE_MODE);
                            entry.push_back(MTK_CONTROL_SCENE_MODE_HDR, Type2Type< MUINT8 >());
                            appSetting.update(entry.tag(), entry);
                        }
                        //
                        vSettings.push_back(
                            SettingSet{
                                appSetting,
                                halSetting
                            }
                        );
                        //
                        // submit to zsd preview pipeline
                        res = applyRawBufferSettings( vSettings, shotCount );
                    }
                } break;
            // default zsd flow
            default:
                break;
        }

        if(res == MFALSE){
            MY_LOGE("applySettings Fail!");
            return MFALSE;
        }
        //
    }
    //
#warning "TODO: set yuv & jpeg format / size / buffer in createStreams()"
    if ( ! createStreams(YuvBufferCount) ) {
        MY_LOGE("createStreams failed");
        return MFALSE;
    }
#warning "TODO: create jpeg buffer here"
    mpJpegPool = new BufferPoolImp(mpInfo_Jpeg);
    mpJpegPool->allocateBuffer(LOG_TAG, mpInfo_Jpeg->getMaxBufNum(), mpInfo_Jpeg->getMinInitBufNum());

    constructCapturePipeline();

    // get multiple raw buffer and send to capture pipeline
    for ( MINT32 i = 0; i < shotCount; ++i ) {
        // get Selector Data (buffer & metadata)
        status_t status = OK;
        android::sp<IImageBuffer> pBuf = NULL; // full raw buffer
        IMetadata selectorAppMetadata; // app setting for this raw buffer. Ex.3A infomation
        status = getSelectorData(
                    selectorAppMetadata,
                    mShotParam.mHalSetting,
                    pBuf
                );
        if( status != OK ) {
            MY_LOGE("GetSelectorData Fail!");
            return MFALSE;
        }
        //
        // TODO: modify capture setting here! [optional] ex. diff EV
        IMetadata appSetting = mShotParam.mAppSetting;
        IMetadata halSetting = mShotParam.mHalSetting;

        // submit setting to capture pipeline
        if (OK != submitCaptureSetting(appSetting, halSetting) ) {
            MY_LOGE("Submit capture setting fail.");
            return MFALSE;
        }
        //
        mCapReqNo++;
    }

    // 1. wait pipeline done
    // 2. set selector back to default zsd selector
    // 3. set full raw buffer count back to normal
    endCapture();
    //
    return MTRUE;
}


/******************************************************************************
 *
 ******************************************************************************/
void
ZsdVendorShot::
onCmd_cancel()
{
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdVendorShot::
handlePostViewData(MUINT8* const /*puBuf*/, MUINT32 const /*u4Size*/)
{
#if 0
    MY_LOGD("+ (puBuf, size) = (%p, %d)", puBuf, u4Size);
    mpShotCallback->onCB_PostviewDisplay(0,
                                         u4Size,
                                         reinterpret_cast<uint8_t const*>(puBuf)
                                        );

    MY_LOGD("-");
#endif
    return  MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdVendorShot::
handleJpegData(IImageBuffer* pJpeg)
{
    CAM_TRACE_CALL();
    //
    mpShotCallback->onCB_Shutter(true,0);
    class scopedVar
    {
    public:
                    scopedVar(IImageBuffer* pBuf)
                    : mBuffer(pBuf) {
                        if( mBuffer )
                            mBuffer->lockBuf(LOG_TAG, eBUFFER_USAGE_SW_READ_MASK);
                    }
                    ~scopedVar() {
                        if( mBuffer )
                            mBuffer->unlockBuf(LOG_TAG);
                    }
    private:
        IImageBuffer* mBuffer;
    } _local(pJpeg);
    //
    uint8_t const* puJpegBuf = (uint8_t const*)pJpeg->getBufVA(0);
    MUINT32 u4JpegSize = pJpeg->getBitstreamSize();

    MY_LOGD("+ (puJpgBuf, jpgSize) = (%p, %d)",
            puJpegBuf, u4JpegSize);

    // dummy raw callback
    mpShotCallback->onCB_RawImage(0, 0, NULL);

    // Jpeg callback
    mpShotCallback->onCB_CompressedImage_packed(0,
                                         u4JpegSize,
                                         puJpegBuf,
                                         0,                       //callback index
                                         true                     //final image
                                         );
    MY_LOGD("-");

    return MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MVOID
ZsdVendorShot::
decideSettingType( MINT32& type )
{
    sp<IFrameInfo> pFrameInfo = IResourceContainer::getInstance(getOpenId())->queryLatestFrameInfo();
    if(pFrameInfo == NULL)
    {
        MY_LOGW("Can't query Latest FrameInfo!");
    }
    else
    {
        //The FlashRequired condition must be the same with PreCapture required condition in CamAdapter::takePicture()
        IMetadata meta;
        pFrameInfo->getFrameMetadata(meta);
        MUINT8 AeState = 0, AeMode = 0;
        tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_STATE, AeState);
        tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_MODE, AeMode);
        //
        if( AeMode == MTK_CONTROL_AE_MODE_ON_ALWAYS_FLASH ||
            (AeMode == MTK_CONTROL_AE_MODE_ON_AUTO_FLASH && AeState == MTK_CONTROL_AE_STATE_FLASH_REQUIRED) )
            type = SETTING_FLASH_ACQUIRE;
        /*else if (1)
            type = SETTING_HDR;*/
        else
            type = SETTING_NONE;
        MY_LOGD("query AE mode(%d) AE state(%d) type(%d)", AeMode, AeState, type);
    }
}


/******************************************************************************
*
*******************************************************************************/
MERROR
ZsdVendorShot::
updateCaptureParams(
    MINT32 shotCount,
    Vector<CaptureParam_T>& vHdrCaptureParams
)
{
    IHal3A *hal3A = MAKE_Hal3A(
                    getOpenId(), LOG_TAG);
    //
    ExpSettingParam_T rExpSetting;
    hal3A->send3ACtrl( E3ACtrl_GetExposureInfo,
        reinterpret_cast<MINTPTR>(&rExpSetting), 0);
    CaptureParam_T tmpCap3AParam;
    hal3A->send3ACtrl( E3ACtrl_GetExposureParam,
        reinterpret_cast<MINTPTR>(&tmpCap3AParam), 0);
    //
    MUINT32 delayedFrames = 0;
    hal3A->send3ACtrl( E3ACtrl_GetCaptureDelayFrame,
            reinterpret_cast<MINTPTR>(&delayedFrames), 0);
    //
    for (MINT32 i = 0; i < shotCount; i++)
    {
        // copy original capture parameter
        CaptureParam_T modifiedCap3AParam = tmpCap3AParam;
#warning "modify EV settings here"
        modifiedCap3AParam.u4Eposuretime  = tmpCap3AParam.u4Eposuretime;
        modifiedCap3AParam.u4AfeGain      = tmpCap3AParam.u4AfeGain;
        //
        modifiedCap3AParam.u4IspGain      = 1024; // fix ISP gain to 1x
        modifiedCap3AParam.u4FlareOffset  = tmpCap3AParam.u4FlareOffset;
        MY_LOGD_IF( 1, "Modified ExposureParam[%d] w/ Exp(%d) Gain(%d)",
                    i, tmpCap3AParam.u4Eposuretime, tmpCap3AParam.u4AfeGain);
        vHdrCaptureParams.push_back(modifiedCap3AParam);
    }
    if (hal3A != NULL) hal3A->destroyInstance(LOG_TAG);
    //
    //  append another blank EVs in tails, for AE converge
    for ( size_t i = 0; i < delayedFrames; ++i) {
        vHdrCaptureParams.push_back(tmpCap3AParam);
    }
    //
    return OK;
}

/******************************************************************************
*
*******************************************************************************/
static
MVOID
prepare_stream(BufferList& vDstStreams, StreamId id, MBOOL criticalBuffer)
{
    vDstStreams.push_back(
        BufferSet{
            .streamId       = id,
            .criticalBuffer = criticalBuffer,
        }
    );
}

MBOOL
ZsdVendorShot::
applyRawBufferSettings(
    Vector< SettingSet > vSettings,
    MINT32 shotCount
)
{
    MY_LOGD("Apply user's setting.");
    //
    sp<StreamBufferProvider> pProvider = mpConsumer.promote();
    if(pProvider == NULL) {
        MY_LOGE("pProvider is NULL!");
        return MFALSE;
    }
    //
    sp<IFeatureFlowControl> pFeatureFlowControl = IResourceContainer::getInstance(getOpenId())->queryFeatureFlowControl();
    if(pFeatureFlowControl == NULL) {
        MY_LOGE("IFeatureFlowControl is NULL!");
        return MFALSE;
    }
    //
    HwInfoHelper helper(getOpenId());
    MSize sensorSize;
    if( ! helper.updateInfos() ) {
        MY_LOGE("cannot properly update infos!");
        return MFALSE;
    }
    //
    if( ! helper.getSensorSize( mu4Scenario, sensorSize) ) {
        MY_LOGE("cannot get params about sensor!");
        return MFALSE;
    }
    MY_LOGD("sensorMode(%d), sensorSize(%d,%d)", mu4Scenario, sensorSize.w, sensorSize.h);
    //
    for ( size_t i = 0; i < vSettings.size(); ++i ) {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
        entry.push_back(sensorSize, Type2Type< MSize >());
        vSettings.editItemAt(i).halSetting.update(entry.tag(), entry);
    }
    //
    BufferList           vDstStreams;
    Vector< MINT32 >     vRequestNo;

    prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_OPAQUE,  true);
    //prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_RAW_RESIZER, false);
    //prepare_stream(vDstStreams, eSTREAMID_IMAGE_PIPE_YUV_00,      false);

    if( OK != pFeatureFlowControl->submitRequest(
                vSettings,
                vDstStreams,
                vRequestNo
                )
      )
    {
        MY_LOGE("submitRequest failed");
        return MFALSE;
    }
    //
    Vector< MINT32 >     vActualRequestNo;
    if ( shotCount < vRequestNo.size() ) {
        for ( MINT32 i = 0; i < shotCount; ++i ) vActualRequestNo.push_back(vRequestNo[i]);
    } else {
        vActualRequestNo = vRequestNo;
    }
    //
    sp<ZsdRequestSelector> pSelector = new ZsdRequestSelector();
    pSelector->setWaitRequestNo(vActualRequestNo);
    status_t status = pProvider->setSelector(pSelector);
    // TODO add LCSO consumer set the same Selector
    if(status != OK)
    {
        MY_LOGE("change to ZSD Flash selector Fail!");
        return MFALSE;
    }
    MY_LOGD("change to ZSD Flash selector");
    //
    return MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MERROR
ZsdVendorShot::
submitCaptureSetting(
    IMetadata appSetting,
    IMetadata halSetting
)
{

    IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
    entry.push_back(mSensorSize, Type2Type< MSize >());
    halSetting.update(entry.tag(), entry);
    //
    ILegacyPipeline::ResultSet resultSet;
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        resultSet.add(eSTREAMID_META_APP_DYNAMIC_P1, mResultMetadataSetMap.editValueFor(mCapReqNo).selectorAppMetadata);
    }
    // submit setting to capture pipeline
    MY_LOGD("submitSetting %d",mCapReqNo);
    if( OK != mpPipeline->submitSetting(
                mCapReqNo,
                appSetting,
                halSetting,
                &resultSet
                )
      )
    {
        MY_LOGE("submitRequest failed");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/******************************************************************************
*
*******************************************************************************/
MVOID
ZsdVendorShot::
endCapture()
{
    // 1. change to ZSD selector
    sp<StreamBufferProvider> pConsumer = mpConsumer.promote();
    if(pConsumer != NULL) {
        sp<ZsdSelector> pSelector = new ZsdSelector();
        pConsumer->setSelector(pSelector);
    }
    //
    if ( !mResultMetadataSetMap.isEmpty() ) {
        Mutex::Autolock _l(mEncJobLock);
        mEncDoneCond.wait(mEncJobLock);
    }
    //
    mvFutures.clear();

    // 2. wait pipeline done
    mpPipeline->waitUntilDrained();
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdVendorShot::
createStreams( MINT32 aBufferCount )
{
    CAM_TRACE_CALL();
    MUINT32 const openId        = getOpenId();
    MUINT32 const sensorMode    = mu4Scenario;
    MUINT32 const bitDepth      = mu4Bitdepth;
    //
    MSize const previewsize     = MSize(mShotParam.mi4PostviewWidth, mShotParam.mi4PostviewHeight);
    MINT const previewfmt       = static_cast<EImageFormat>(mShotParam.miPostviewDisplayFormat);
    MINT const yuvfmt_main      = eImgFmt_YUY2;
    MINT const yuvfmt_thumbnail = eImgFmt_YUY2;
    //
    MSize const thumbnailsize = MSize(mJpegParam.mi4JpegThumbWidth, mJpegParam.mi4JpegThumbHeight);
    //
    //
#if 0
    // postview
    if( isDataMsgEnabled(ECamShot_DATA_MSG_POSTVIEW) )
    {
        MSize size        = previewsize;
        MINT format       = previewfmt;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = 0;
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "SingleShot:Postview",
                    eSTREAMID_IMAGE_PIPE_YUV_00,
                    eSTREAMTYPE_IMAGE_INOUT,
                    1, 1,
                    usage, format, size, transform
                    );
        if( pStreamInfo == nullptr ) {
            return BAD_VALUE;
        }
        //
        mpInfo_YuvPostview = pStreamInfo;
    }
#endif
    //
    // Yuv
    {
        MSize size        = mJpegsize;
        MINT format       = yuvfmt_main;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = mShotParam.mu4Transform;
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "ZsdVendorShot:MainYuv",
                    eSTREAMID_IMAGE_PIPE_YUV_JPEG,
                    eSTREAMTYPE_IMAGE_INOUT,
                    aBufferCount, aBufferCount,
                    usage, format, size, transform
                    );
        if( pStreamInfo == nullptr ) {
            return BAD_VALUE;
        }
        //
        mpInfo_Yuv = pStreamInfo;
    }
    //
    // Thumbnail Yuv
    {
        MSize size        = thumbnailsize;
        MINT format       = yuvfmt_thumbnail;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = 0;
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "ZsdVendorShot:ThumbnailYuv",
                    eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL,
                    eSTREAMTYPE_IMAGE_INOUT,
                    aBufferCount, aBufferCount,
                    usage, format, size, transform
                    );
        if( pStreamInfo == nullptr ) {
            return BAD_VALUE;
        }
        //
        mpInfo_YuvThumbnail = pStreamInfo;
    }
    //
    // Jpeg
    {
        MSize size        = mJpegsize;
        MINT format       = eImgFmt_BLOB;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = 0;
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "ZsdVendorShot:Jpeg",
                    eSTREAMID_IMAGE_JPEG,
                    eSTREAMTYPE_IMAGE_INOUT,
                    1, 1,
                    usage, format, size, transform
                    );
        if( pStreamInfo == nullptr ) {
            return BAD_VALUE;
        }
        //
        mpInfo_Jpeg = pStreamInfo;
    }
    //
    return MTRUE;

}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdVendorShot::
createPipeline()
{
    CAM_TRACE_CALL();
    FUNC_START;
    LegacyPipelineBuilder::ConfigParams LPBConfigParams;
    LPBConfigParams.mode = getLegacyPipelineMode();
    LPBConfigParams.enableEIS = MFALSE;
    LPBConfigParams.enableLCS = MFALSE;
    //
    HwInfoHelper helper(getOpenId());
    if( ! helper.updateInfos() ) {
        MY_LOGE("cannot properly update infos");
        return BAD_VALUE;
    }
    //
    if (helper.getDualPDAFSupported(mu4Scenario))
    {
        LPBConfigParams.enableDualPD = MTRUE;
    }
    //
    sp<LegacyPipelineBuilder> pBuilder = LegacyPipelineBuilder::createInstance(
                                            getOpenId(),
                                            "ZsdVendorShot",
                                            LPBConfigParams);
    CHECK_OBJECT(pBuilder);

    mpImageCallback = new ZvImageCallback(this, 0);
    CHECK_OBJECT(mpImageCallback);

    sp<BufferCallbackHandler> pCallbackHandler = new BufferCallbackHandler(getOpenId());
    CHECK_OBJECT(pCallbackHandler);
    pCallbackHandler->setImageCallback(mpImageCallback);
    mpCallbackHandler = pCallbackHandler;

    sp<StreamBufferProviderFactory> pFactory =
                StreamBufferProviderFactory::createInstance();
    //
    Vector<PipelineImageParam> vImageParam;
    //
    {
        MY_LOGD("createPipeline for ZSD vendor shot");
        // zsd flow
        Vector<PipelineImageParam> vImgSrcParams;
        sp<IImageStreamInfo> pStreamInfo = mpInfo_FullRaw;
        //
        sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
        pCallbackHandler->setBufferPool(pPool);
        //
        pFactory->setImageStreamInfo(pStreamInfo);
        pFactory->setUsersPool(
                    pCallbackHandler->queryBufferPool(pStreamInfo->getStreamId())
                );
        PipelineImageParam imgParam = {
            .pInfo     = pStreamInfo,
            .pProvider = pFactory->create(false),
            .usage     = 0
        };
        vImgSrcParams.push_back(imgParam);
        //
        if( OK != pBuilder->setSrc(vImgSrcParams) ) {
            MY_LOGE("setSrc failed");
            return MFALSE;
        //
        }
    }
    //
    {
        if( mpInfo_Yuv.get() )
        {
            sp<IImageStreamInfo> pStreamInfo = mpInfo_Yuv;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                pPool->allocateBuffer(
                                  pStreamInfo->getStreamName(),
                                  pStreamInfo->getMaxBufNum(),
                                  pStreamInfo->getMinInitBufNum()
                                  );
                pCallbackHandler->setBufferPool(pPool);
            }
            //
            pFactory->setImageStreamInfo(pStreamInfo);
            pFactory->setUsersPool(
                        pCallbackHandler->queryBufferPool(pStreamInfo->getStreamId())
                    );
            //
            PipelineImageParam imgParam = {
                .pInfo     = pStreamInfo,
                .pProvider = pFactory->create(false),
                .usage     = 0
            };
            vImageParam.push_back(imgParam);
        }
        //
        if( mpInfo_YuvPostview.get() )
        {
            sp<IImageStreamInfo> pStreamInfo = mpInfo_YuvPostview;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                pPool->allocateBuffer(
                                  pStreamInfo->getStreamName(),
                                  pStreamInfo->getMaxBufNum(),
                                  pStreamInfo->getMinInitBufNum()
                                  );
            }
            pCallbackHandler->setBufferPool(pPool);
            //
            pFactory->setImageStreamInfo(pStreamInfo);
            pFactory->setUsersPool(
                        pCallbackHandler->queryBufferPool(pStreamInfo->getStreamId())
                    );
            //
            PipelineImageParam imgParam = {
                .pInfo     = pStreamInfo,
                .pProvider = pFactory->create(false),
                .usage     = 0
            };
            vImageParam.push_back(imgParam);
        }
        //
        if( mpInfo_YuvThumbnail.get() )
        {
            sp<IImageStreamInfo> pStreamInfo = mpInfo_YuvThumbnail;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                pPool->allocateBuffer(
                                  pStreamInfo->getStreamName(),
                                  pStreamInfo->getMaxBufNum(),
                                  pStreamInfo->getMinInitBufNum()
                                  );
            }
            pCallbackHandler->setBufferPool(pPool);
            //
            pFactory->setImageStreamInfo(pStreamInfo);
            pFactory->setUsersPool(
                        pCallbackHandler->queryBufferPool(pStreamInfo->getStreamId())
                    );
            //
            PipelineImageParam imgParam = {
                .pInfo     = pStreamInfo,
                .pProvider = pFactory->create(false),
                .usage     = 0
            };
            vImageParam.push_back(imgParam);
        }
#if 0
        //
        if( mpInfo_Jpeg.get() )
        {
            sp<IImageStreamInfo> pStreamInfo = mpInfo_Jpeg;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                pPool->allocateBuffer(
                                  pStreamInfo->getStreamName(),
                                  pStreamInfo->getMaxBufNum(),
                                  pStreamInfo->getMinInitBufNum()
                                  );
            }
            pCallbackHandler->setBufferPool(pPool);
            //
            pFactory->setImageStreamInfo(pStreamInfo);
            pFactory->setUsersPool(
                        pCallbackHandler->queryBufferPool(pStreamInfo->getStreamId())
                    );
            //
            PipelineImageParam imgParam = {
                .pInfo     = pStreamInfo,
                .pProvider = pFactory->create(),
                .usage     = 0
            };
            vImageParam.push_back(imgParam);
        }
#endif
        //
        if( OK != pBuilder->setDst(vImageParam) ) {
            MY_LOGE("setDst failed");
            return MFALSE;
        }
    }
    //
    mpPipeline = pBuilder->create();
    //
    FUNC_END;
    return MTRUE;

}

/******************************************************************************
*
*******************************************************************************/
MINT
ZsdVendorShot::
getLegacyPipelineMode(void)
{
    int shotMode = getShotMode();
    EPipelineMode pipelineMode = getPipelineMode();
    int legacyPipeLineMode = LegacyPipelineMode_T::PipelineMode_Capture;
    switch(shotMode)
    {
        default:
            legacyPipeLineMode = (pipelineMode == ePipelineMode_Feature) ?
                LegacyPipelineMode_T::PipelineMode_Feature_Capture :
                LegacyPipelineMode_T::PipelineMode_Capture;
            break;
    }
    return legacyPipeLineMode;
}

/******************************************************************************
*
*******************************************************************************/
status_t
ZsdVendorShot::
getSelectorData(
    IMetadata& rAppSetting,
    IMetadata& rHalSetting,
    android::sp<IImageBuffer>& pBuffer
)
{
    CAM_TRACE_CALL();
    //
    sp<StreamBufferProvider> pConsumer = mpConsumer.promote();
    if(pConsumer == NULL) {
        MY_LOGE("mpConsumer is NULL!");
        return UNKNOWN_ERROR;
    }
    //
    sp< ISelector > pSelector = pConsumer->querySelector();
    if(pSelector == NULL) {
        MY_LOGE("can't find Selector in Consumer");
        return UNKNOWN_ERROR;
    }
    //
    status_t status = OK;
    MINT32 rRequestNo;
    Vector<ISelector::MetaItemSet> rvResultMeta;
    Vector<ISelector::BufferItemSet> rvBufferSet;
    status = pSelector->getResult(rRequestNo, rvResultMeta, rvBufferSet);

    if(rvBufferSet.size() != 1 || rvBufferSet[0].id != eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)
    {
        MY_LOGE("Selector get input raw buffer failed! bufferSetSize(%d)", rvBufferSet.size());
        return UNKNOWN_ERROR;
    }

    if(status != OK) {
        MY_LOGE("Selector getResult Fail!");
        return UNKNOWN_ERROR;
    }

    if(rvResultMeta.size() != 2) {
        MY_LOGE("ZsdSelect getResult rvResultMeta != 2");
        return UNKNOWN_ERROR;
    }

    // get app & hal metadata
    if(    rvResultMeta.editItemAt(0).id == eSTREAMID_META_APP_DYNAMIC_P1
        && rvResultMeta.editItemAt(1).id == eSTREAMID_META_HAL_DYNAMIC_P1 )
    {
        rAppSetting  = rvResultMeta.editItemAt(0).meta;
        rHalSetting += rvResultMeta.editItemAt(1).meta;
    }
    else
    if(    rvResultMeta.editItemAt(1).id == eSTREAMID_META_APP_DYNAMIC_P1
        && rvResultMeta.editItemAt(0).id == eSTREAMID_META_HAL_DYNAMIC_P1 )
    {
        rAppSetting  = rvResultMeta.editItemAt(1).meta;
        rHalSetting += rvResultMeta.editItemAt(0).meta;
    }
    else {
        MY_LOGE("Something wrong for selector metadata.");
        return UNKNOWN_ERROR;
    }

    //
    pBuffer = rvBufferSet[0].heap->createImageBuffer();
    //
    if(pBuffer == NULL) {
        MY_LOGE("get buffer is NULL!");
        return UNKNOWN_ERROR;
    }

   //update p1 buffer to pipeline pool
    sp<CallbackBufferPool> pPool = mpCallbackHandler->queryBufferPool( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
    if( pPool == NULL) {
        MY_LOGE("query Pool Fail!");
        return UNKNOWN_ERROR;
    }
    pPool->addBuffer(pBuffer);
    //
    {
        Mutex::Autolock _l(mResultMetadataSetLock);

        IMetadata resultAppMetadata;
        IMetadata resultHalMetadata;
        IMetadata selectorAppMetadata;
        mResultMetadataSetMap.add(static_cast<MUINT32>(mCapReqNo),ResultSet_T{static_cast<MUINT32>(mCapReqNo), resultAppMetadata, resultHalMetadata, selectorAppMetadata, rvBufferSet});
    }
    return OK;
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdVendorShot::
onMetaReceived(
    MUINT32         const requestNo,
    StreamId_T      const streamId,
    MBOOL           const errorResult,
    IMetadata       const result
)
{
    CAM_TRACE_FMT_BEGIN("onMetaReceived No%d,StreamID %#" PRIxPTR, requestNo,streamId);
    MY_LOGD("requestNo %d, stream %#" PRIxPTR", errResult:%d", requestNo, streamId, errorResult);
    //
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        int idx = mResultMetadataSetMap.indexOfKey(requestNo);
        if(idx < 0 )
        {
            MY_LOGE("mResultMetadataSetMap can't find requestNo(%d)",requestNo);
            for ( size_t i=0; i<mResultMetadataSetMap.size(); i++) {
                MY_LOGD( "mResultMetadataSetMap(%d/%d)  requestNo(%d) buf(%p)",
                         i, mResultMetadataSetMap.size(), mResultMetadataSetMap[i].requestNo,
                         mResultMetadataSetMap[i].selectorGetBufs[0].heap.get() );
            }
            return;
        }
    }
    //
#warning "Need to modify if pipeline change"
    if (streamId == eSTREAMID_META_HAL_DYNAMIC_P2) {
        // MY_LOGD("Ready to notify p2done & Shutter");
        // mpShotCallback->onCB_P2done();
        // mpShotCallback->onCB_Shutter(true,0);
        //
        //
        {
            Mutex::Autolock _l(mResultMetadataSetLock);
            mResultMetadataSetMap.editValueFor(requestNo).halResultMetadata = result;
        }
        {
            Mutex::Autolock _l(mEncJobLock);
            mEncJob.add(requestNo, EncJob::STREAM_HAL_META, result);
            checkStreamAndEncodeLocked(requestNo);
        }
    }
    else if (streamId == eSTREAMID_META_APP_DYNAMIC_P2) {
        {
            Mutex::Autolock _l(mResultMetadataSetLock);
            mResultMetadataSetMap.editValueFor(requestNo).appResultMetadata += result;
        }
        {
            Mutex::Autolock _l(mEncJobLock);
            IMetadata tmp = mResultMetadataSetMap.valueFor(requestNo).selectorAppMetadata;
            tmp += result;
            mEncJob.add(requestNo, EncJob::STREAM_APP_META, tmp);
            checkStreamAndEncodeLocked(requestNo);
        }
    }
    //
    CAM_TRACE_FMT_END();
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdVendorShot::
onDataReceived(
    MUINT32 const               requestNo,
    StreamId_T const            streamId,
    android::sp<IImageBuffer>&  pBuffer
)
{
    CAM_TRACE_FMT_BEGIN("onDataReceived No%d,streamId%d",requestNo,streamId);
    MY_LOGD("requestNo %d, streamId 0x%x, buffer %p", requestNo, streamId, pBuffer.get());
    //
    if( pBuffer != 0 )
    {
        if (streamId == eSTREAMID_IMAGE_PIPE_YUV_00)
        {
        }
        else if (streamId == eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL)
        {
            {
                Mutex::Autolock _l(mEncJobLock);
                mEncJob.add(requestNo, EncJob::STREAM_IMAGE_THUMB, pBuffer);
                checkStreamAndEncodeLocked(requestNo);
            }
        }
        else if (streamId == eSTREAMID_IMAGE_PIPE_YUV_JPEG)
        {
            sp<StreamBufferProvider> pProvider = IResourceContainer::getInstance(getOpenId())->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
            MY_LOGD("Query Consumer OpenID(%d) StreamID(%d)", getOpenId(), eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
            if( pProvider == NULL) {
                MY_LOGE("can't find StreamBufferProvider in ConsumerContainer");
            }
            else {
                Vector<ISelector::BufferItemSet> bufferSet;
                {
                    Mutex::Autolock _l(mResultMetadataSetLock);
                    bufferSet = mResultMetadataSetMap.editValueFor(requestNo).selectorGetBufs;
                };

                sp< ISelector > pSelector = pProvider->querySelector();
                if(pSelector == NULL) {
                    MY_LOGE("can't find Selector in Consumer when p2done");
                } else {
                    for(size_t i = 0; i < bufferSet.size() ; i++)
                        pSelector->returnBuffer(bufferSet.editItemAt(i));
                }
            }
            //
            {
                Mutex::Autolock _l(mResultMetadataSetLock);
                for (auto& itr : mResultMetadataSetMap.editValueFor(requestNo).selectorGetBufs) {
                    itr = ISelector::BufferItemSet();
                }
            }
            //
            {
                Mutex::Autolock _l(mEncJobLock);
                mEncJob.add(requestNo, EncJob::STREAM_IMAGE_MAIN, pBuffer);
                checkStreamAndEncodeLocked(requestNo);
            }
        }
        //
        // debug
        MINT32 data = NSCamShot::ECamShot_DATA_MSG_NONE;
        //
        switch (streamId)
        {
            case eSTREAMID_IMAGE_PIPE_RAW_OPAQUE:
                data = NSCamShot::ECamShot_DATA_MSG_RAW;
                break;
            case eSTREAMID_IMAGE_PIPE_YUV_JPEG:
                data = NSCamShot::ECamShot_DATA_MSG_YUV;
                break;
            case eSTREAMID_IMAGE_PIPE_YUV_00:
                data = NSCamShot::ECamShot_DATA_MSG_POSTVIEW;
                break;
            case eSTREAMID_IMAGE_JPEG:
                data = NSCamShot::ECamShot_DATA_MSG_JPEG;
                break;
            default:
                data = NSCamShot::ECamShot_DATA_MSG_NONE;
                break;
        }
        //
        if( mDumpFlag & data )
        {
            String8 filename = String8::format("%s/ZsdVendorShot_%dx%d",
                    DUMP_PATH, pBuffer->getImgSize().w, pBuffer->getImgSize().h);
            switch( data )
            {
                case NSCamShot::ECamShot_DATA_MSG_RAW:
                    filename += String8::format("_%zd.raw", pBuffer->getBufStridesInBytes(0));
                    break;
                case NSCamShot::ECamShot_DATA_MSG_YUV:
                case NSCamShot::ECamShot_DATA_MSG_POSTVIEW:
                    filename += String8(".yuv");
                    break;
                case NSCamShot::ECamShot_DATA_MSG_JPEG:
                    filename += String8(".jpeg");
                    break;
                default:
                    break;
            }
            pBuffer->lockBuf(LOG_TAG, eBUFFER_USAGE_SW_READ_MASK);
            pBuffer->saveToFile(filename);
            pBuffer->unlockBuf(LOG_TAG);
            //
            MY_LOGD("dump buffer in %s", filename.string());
        }
    }
    CAM_TRACE_FMT_END();
}

/*******************************************************************************
*
********************************************************************************/
MERROR
ZsdVendorShot::
checkStreamAndEncodeLocked( MUINT32 const /*requestNo*/)
{
    if ( ! mEncJob.isReady() )
        return NAME_NOT_FOUND;
    //
    mvFutures.push_back(
        std::async(std::launch::async,
            [ this ]( ) -> MERROR {
                MERROR err = postProcessing();
                if ( OK != err ) return err;

                //
                sp<IImageBufferHeap> dstBuffer;
                MUINT32              transform;
                mpJpegPool->acquireFromPool(
                                LOG_TAG,
                                0,
                                dstBuffer,
                                transform
                            );
                if ( ! dstBuffer.get() )
                    MY_LOGE("no destination");
                // encode jpeg here
                {
                    Mutex::Autolock _l(mEncJobLock);
                    mEncJob.setTarget(dstBuffer);
                    sp<ExifJpegUtils> pExifJpegUtils = ExifJpegUtils::createInstance(
                        getOpenId(), mEncJob.HalMetadata, mEncJob.AppMetadata,
                        mEncJob.mpDst, mEncJob.pSrc_main, mEncJob.pSrc_thumbnail );
                    if ( !pExifJpegUtils.get() ) {
                        MY_LOGE("create exif jpeg encode utils fail");
                        return -ENODEV;
                    }
                    err = pExifJpegUtils->execute();
                    if ( OK != err) {
                        MY_LOGE("Exif Jpeg encode utils: (%d)", err);
                        return err;
                    }
                }
                //
                handleJpegData(dstBuffer->createImageBuffer());
                dstBuffer->decStrong(dstBuffer.get());
                //clear result metadata
                {
                    Mutex::Autolock _l(mResultMetadataSetLock);
                    mResultMetadataSetMap.clear();
                }
                mEncDoneCond.signal();

                return err;
            }
        )
    );
    //
    return OK;
}

/*******************************************************************************
*
********************************************************************************/
MERROR
ZsdVendorShot::
postProcessing()
{
    MY_LOGD("+");
    // MUST to fill final metadata/imagebuffer for encode
    // ex. fetch sources for post processing
    // for ( ssize_t i=0; i<mEncJob.mvSource.size(); i++)
    // {
    // //    mEncJob.pSrc_main =
    // //           GET_GRAY_IMAGE(mEncJob.mvSource[index].pSrc_main);
    // }
    //
    // ex. directly assign last source for encode
    MINT32 index = 0;//mEncJob.mSrcCnt - 1;
    mEncJob.HalMetadata      = mEncJob.mvSource[index].HalMetadata;
    mEncJob.AppMetadata      = mEncJob.mvSource[index].AppMetadata;
    mEncJob.pSrc_main        = mEncJob.mvSource[index].pSrc_main;
    mEncJob.pSrc_thumbnail   = mEncJob.mvSource[index].pSrc_thumbnail;
    MY_LOGD("-");
    return OK;
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdVendorShot::
beginCapture(
    MINT32 rAllocateCount
)
{
    CAM_TRACE_CALL();
    //
    mu4Scenario = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
    mu4Bitdepth = 10;
    //
     HwInfoHelper helper(getOpenId());
     if( ! helper.updateInfos() ) {
         MY_LOGE("cannot properly update infos");
     }
     if( ! helper.getSensorSize( mu4Scenario, mSensorSize) ) {
         MY_LOGE("cannot get params about sensor");
     }
     //
     mJpegsize      = (mShotParam.mu4Transform & eTransform_ROT_90) ?
         MSize(mShotParam.mi4PictureHeight, mShotParam.mi4PictureWidth):
         MSize(mShotParam.mi4PictureWidth, mShotParam.mi4PictureHeight);
    //
    // TODO add other consumer
    mpConsumer = IResourceContainer::getInstance(getOpenId())->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
    MY_LOGD("Query Consumer OpenID(%d) StreamID(%d)", getOpenId(), eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
    sp<StreamBufferProvider> pProvider = mpConsumer.promote();
    if( pProvider == NULL) {
        MY_LOGE("can't find StreamBufferProvider in ConsumerContainer");
    }
    //
    sp<IImageStreamInfo> pInfo = pProvider->queryImageStreamInfo();
    pProvider->updateBufferCount("ZsdVendorShot", pInfo->getMaxBufNum() + rAllocateCount);
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
ZsdVendorShot::
constructCapturePipeline()
{
    CAM_TRACE_CALL();
    sp<StreamBufferProvider> pProvider = mpConsumer.promote();
    CHECK_OBJECT(pProvider);
    mpInfo_FullRaw = pProvider->queryImageStreamInfo();
    mpInfo_ResizedRaw = nullptr;
    //
    MY_LOGD("zsd raw stream %#" PRIxPTR , "(%s) size(%dx%d), fmt 0x%x",
            mpInfo_FullRaw->getStreamId(),
            mpInfo_FullRaw->getStreamName(),
            mpInfo_FullRaw->getImgSize().w,
            mpInfo_FullRaw->getImgSize().h,
            mpInfo_FullRaw->getImgFormat()
           );
    // create new pipeline
    if ( ! createPipeline() ) {
        MY_LOGE("createPipeline failed");
        return MFALSE;
    }
    CHECK_OBJECT(mpPipeline);
    //
    sp<ResultProcessor> pResultProcessor = mpPipeline->getResultProcessor().promote();
    CHECK_OBJECT(pResultProcessor);

    // metadata
    mpMetadataListener = new ZvMetadataListener(this);
    pResultProcessor->registerListener( 0, 1000, true, mpMetadataListener);

    return MTRUE;
}
