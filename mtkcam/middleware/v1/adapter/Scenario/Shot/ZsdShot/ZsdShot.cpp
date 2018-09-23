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
#include <mtkcam/utils/std/Trace.h>
#include <mtkcam/def/common.h>
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
#include "ZsdShot.h"
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


using namespace android;
using namespace NSShot;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace NSCamHW;

#define DNG_META_CB 1

#define CHECK_OBJECT(x)  do{                                        \
    if (x == nullptr) { MY_LOGE("Null %s Object", #x); return MFALSE;} \
} while(0)

#include <cutils/properties.h>
#define DUMP_KEY  "debug.zsdshot.dump"
#define DUMP_PATH "/sdcard/zsdshot"

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
createInstance_ZsdShot(
    char const*const    pszShotName,
    uint32_t const      u4ShotMode,
    int32_t const       i4OpenId
)
{
    sp<IShot>       pShot = NULL;
    sp<ZsdShot>  pImpShot = NULL;
    //
    //  (1.1) new Implementator.
    pImpShot = new ZsdShot(pszShotName, u4ShotMode, i4OpenId);
    if  ( pImpShot == 0 ) {
        CAM_LOGE("[%s] new ZsdShot", __FUNCTION__);
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
ZsdShot::
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
ZsdShot::
onDestroy()
{

}


/******************************************************************************
 *
 ******************************************************************************/
ZsdShot::
ZsdShot(
    char const*const pszShotName,
    uint32_t const u4ShotMode,
    int32_t const i4OpenId
)
    : ImpShot(pszShotName, u4ShotMode, i4OpenId)
    , mShotState(NotWorking)
    , mInProcessingCnt(0)
    , mCapReqNo(0)
    , mLastPipelineMode(-1)
    , mLastJpegsize(0,0)
    , mLastRotation(0)
    , mpPipeline(NULL)
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
ZsdShot::
~ZsdShot()
{
    MY_LOGD("~ZsdShot()");
    Mutex::Autolock _l(mResultMetadataSetLock);
    if( mResultMetadataSetMap.size() > 0 )
    {
        int n = mResultMetadataSetMap.size();
        for(int i=0; i<n; i++)
        {
            MY_LOGW("requestNo(%d) doesn't clear before ZsdShot destroyed",mResultMetadataSetMap.keyAt(i));
            mResultMetadataSetMap.editValueAt(i).selectorGetBufs.clear();
        }
    }
    mResultMetadataSetMap.clear();
}


/******************************************************************************
 *
 ******************************************************************************/
bool
ZsdShot::
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
ZsdShot::
onCmd_reset()
{
    bool ret = true;
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
ZsdShot::
onCmd_capture()
{
    CAM_TRACE_NAME("Zsd Capture");
    MY_LOGD("ZSD Shot capture");

    Mutex::Autolock _l(mPipelineLock);
    MY_LOGD("onCmd_capture get lock");

    MBOOL ret = MTRUE;
    //
    mCapReqNo++;
    if(mCapReqNo>1000)
    {
        mCapReqNo=0;
    }
    MY_LOGD("CapReqNo(%d)",mCapReqNo);
    //
    // control previous capture flow
    previousCapFlowCtrl();
    //
    // prepare necessary setting for this time capture
    prepareSetting();
    //
    // Flash init flow
    CAM_TRACE_BEGIN("Flash init");
    bool bFlashRequired = CheckIsFlashRequired();
    if(bFlashRequired)
    {
        CAM_TRACE_NAME("InitFlashFlow");
        MBOOL res = InitFlashFlow();
        if(res == MFALSE)
        {
            MY_LOGE("InitFlashFlow Fail!");
            return MFALSE;
        }
    }
    CAM_TRACE_END();
    //
    // Notify AP to trigger shutter sound & re-light shutter button
    if( mpShotCallback != NULL )
    {
        if( !isDataMsgEnabled(EIShot_DATA_MSG_RAW))
        {
            CAM_TRACE_BEGIN("Callback Shutter and P2done");
            MY_LOGD("Ready to notify p2done & Shutter");
            //
            mpShotCallback->onCB_P2done();
            mpShotCallback->onCB_Shutter(true,0);
            //
            CAM_TRACE_END();
        }
    }
    //
    // get Selector Data (buffer & metadata)
    CAM_TRACE_BEGIN("getSelectorData");
    status_t status = OK;
    Vector<BufferItemSet> bufferSet;
    IMetadata selectorAppMetadata;
    status = getSelectorData(
                selectorAppMetadata,
                mShotParam.mHalSetting,
                bufferSet);
    if(status != OK)
    {
        MY_LOGE("GetSelectorData Fail!");
        return MFALSE;
    }
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        mResultMetadataSetMap.editValueFor(mCapReqNo).selectorGetBufs = bufferSet;
        mResultMetadataSetMap.editValueFor(mCapReqNo).selectorAppMetadata = selectorAppMetadata;
    }

    MY_LOGD("GetSelectorData bufferSet(%s)",ISelector::logBufferSets(bufferSet));
    if( bufferSet.size() == 0 )
    {
        MY_LOGE("Get selector buffer is NULL!");
        return MFALSE;
    }
    CAM_TRACE_END();
    //
    // handle DNG CB
#if DNG_META_CB
    if(isDataMsgEnabled(EIShot_DATA_MSG_RAW))
    {
        for (size_t i = 0; i < bufferSet.size() ; i++)
        {
            if(bufferSet[i].id == eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)
            {
                sp<IImageBuffer> pBuf = bufferSet[i].heap->createImageBuffer();
                if(pBuf != NULL)
                {
                    pBuf->lockBuf(LOG_TAG, eBUFFER_USAGE_SW_READ_MASK);
                    handleRaw16CB(pBuf.get());
                    pBuf->unlockBuf(LOG_TAG);
                }
            }
        }
    }
#endif
    //
    bool NeedCreatePipeline = CheckIsNeedCreatePipeline();
    if( NeedCreatePipeline )
    {
        constructNewPipelineFlow(bufferSet);
    }
    else
    {
        reUsePipelineFlow(bufferSet);
    }
    //
    // update setting before submit
    IMetadata appSetting = mShotParam.mAppSetting;
    IMetadata halSetting = mShotParam.mHalSetting;
    //
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
        entry.push_back(mSensorSize, Type2Type< MSize >());
        halSetting.update(entry.tag(), entry);
    }
    //
    ILegacyPipeline::ResultSet resultSet;
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        resultSet.add(eSTREAMID_META_APP_DYNAMIC_P1, mResultMetadataSetMap.editValueFor(mCapReqNo).selectorAppMetadata);
    }
    // submit setting to pipeline to capture
    CAM_TRACE_FMT_BEGIN("submitSetting %d",mCapReqNo);
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
        return MFALSE;
    }
    // in processing cnt +1
    incProcessingCnt();
    CAM_TRACE_FMT_END();
    //
    // wait pipeline done if necessary
    if(isDataMsgEnabled(EIShot_DATA_MSG_RAW))
    {
        MY_LOGD("waitUntilDrained (%d) start",mCapReqNo);
        CAM_TRACE_BEGIN("waitUntilDrained");
        mpPipeline->waitUntilDrained();
        CAM_TRACE_END();
        MY_LOGD("waitUntilDrained (%d) end",mCapReqNo);
    }
    //
    // Uninit flash flow
    if(bFlashRequired)
    {
        CAM_TRACE_NAME("UninitFlashFlow");
        MBOOL res = UninitFlashFlow();
        if(res == MFALSE)
        {
            MY_LOGE("UninitFlashFlow Fail!");
            return MFALSE;
        }
    }
    // update last info
    mLastJpegsize = mJpegsize;
    mLastRotation = getRotation();
    mLastPipelineMode = getLegacyPipelineMode();
    //
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
void
ZsdShot::
onCmd_cancel()
{
    FUNC_START;
    Mutex::Autolock _l(mPipelineLock);
    MY_LOGD("onCmd_cancel get lock");

    if( mpPipeline != NULL )
    {
        mpPipeline->waitUntilDrained();
        mpPipeline->flush();
        mpPipeline = NULL;
    }
    FUNC_END;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ZsdShot::
fgCamShotNotifyCb(MVOID* user, NSCamShot::CamShotNotifyInfo const msg)
{
#warning "Need to implement if necessary"
    return MFALSE;

#if 0
    ZsdShot *pZsdShot = reinterpret_cast <ZsdShot *>(user);
    if (NULL != pZsdShot)
    {
        if ( NSCamShot::ECamShot_NOTIFY_MSG_EOF == msg.msgType)
        {
            pZsdShot->mpShotCallback->onCB_Shutter(true,
                                                      0
                                                     );
        }
        else if ( NSCamShot::ECamShot_NOTIFY_MSG_P2DONE == msg.msgType)
        {
            pZsdShot->mpShotCallback->onCB_P2done();
        }
    }

    return MTRUE;
#endif
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ZsdShot::
fgCamShotDataCb(MVOID* /*user*/, NSCamShot::CamShotDataInfo const /*msg*/)
{
#warning "Need to implement if necessary"

    return MFALSE;
#if 0
    ZsdShot *pZsdShot = reinterpret_cast<ZsdShot *>(user);
    if (NULL != pZsdShot)
    {
        if (NSCamShot::ECamShot_DATA_MSG_POSTVIEW == msg.msgType)
        {
            //pZsdShot->handlePostViewData( msg.puData, msg.u4Size);
        }
        else if (NSCamShot::ECamShot_DATA_MSG_JPEG == msg.msgType)
        {
            pZsdShot->handleDNGMetaCB();//dng metadata callback
            pZsdShot->handleJpegData(msg.pBuffer);
        }
#if DNG_META_CB
        else if (NSCamShot::ECamShot_DATA_MSG_DNG == msg.msgType)//raw data callback to adapter
        {
            msg.pBuffer->lockBuf(LOG_TAG, eBUFFER_USAGE_SW_READ_MASK);
            pZsdShot->handleRaw16CB(msg.pBuffer);
            msg.pBuffer->unlockBuf(LOG_TAG);
        }
        else if (NSCamShot::ECamShot_DATA_MSG_APPMETA == msg.msgType)//raw data callback to adapter
        {
            pZsdShot->mAppResultMetadata = *(reinterpret_cast<IMetadata *>(msg.ext1));
        }
        else if (NSCamShot::ECamShot_DATA_MSG_HALMETA == msg.msgType)//raw data callback to adapter
        {
            pZsdShot->mHalResultMetadata = *(reinterpret_cast<IMetadata *>(msg.ext1));
        }
#endif
    }
    return MTRUE;
#endif
}


/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdShot::
handlePostViewData(IImageBuffer *pImgBuffer)
{
    CAM_TRACE_CALL();
#if 1
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
    } _local(pImgBuffer);

    mpShotCallback->onCB_PostviewClient(0,
                                        pImgBuffer
                                        );

    MY_LOGD("-");
#endif
    return  MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdShot::
handleRaw16CB(IImageBuffer const *pImgBuffer)
{
    CAM_TRACE_CALL();
    MY_LOGD("+ (pImgBuffer) = (%p)", pImgBuffer);
    mpShotCallback->onCB_Raw16Image(reinterpret_cast<IImageBuffer const*>(pImgBuffer));
    return  MTRUE;
}


/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdShot::
handleDNGMetaCB(MUINT32 const requestNo)
{
    MY_LOGD("handleDNGMetaCB(%d)",requestNo);
    CAM_TRACE_CALL();
    //Mutex::Autolock _l(mResultMetadataSetLock);
    int idx = mResultMetadataSetMap.indexOfKey(requestNo);
    if(idx < 0 )
    {
        MY_LOGE("mResultMetadataSetMap can't find requestNo(%d)",requestNo);
        return MFALSE;
    }
    else
    {
        const IMetadata& rHalMeta = mResultMetadataSetMap.editValueFor(requestNo).halResultMetadata;
        IMetadata& rAppMeta = mResultMetadataSetMap.editValueFor(requestNo).appResultMetadata;
        IMetadata rDngMeta = MAKE_DngInfo(LOG_TAG, getOpenId())->getShadingMapFromHal(rHalMeta, rAppMeta);
        //
        MY_LOGD("rDngMeta(%d) size(%d)",requestNo,rDngMeta.count());
        MY_LOGD("before append: mResultMetadataSetMap(%d) size(%d)",requestNo,rAppMeta.count());
        rAppMeta+=rDngMeta;
        MY_LOGD("after append: mResultMetadataSetMap(%d) size(%d)",requestNo,rAppMeta.count());
        //
        mpShotCallback->onCB_DNGMetaData((MUINTPTR)&rAppMeta);
    }
    return  MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdShot::
handleJpegData(IImageBuffer* pJpeg)
{
    CAM_TRACE_CALL();
    //
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
MBOOL
ZsdShot::
CheckIsFlashRequired()
{
    MBOOL bFlashRequired = false;
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
        MUINT8 AeState = 0;
        tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_STATE, AeState);
        MUINT8 AeMode = 0;
        tryGetMetadata< MUINT8 >(&meta, MTK_CONTROL_AE_MODE, AeMode);
        if( AeMode == MTK_CONTROL_AE_MODE_ON_ALWAYS_FLASH ||
            (AeMode == MTK_CONTROL_AE_MODE_ON_AUTO_FLASH && AeState == MTK_CONTROL_AE_STATE_FLASH_REQUIRED) )
            bFlashRequired = true;
        else
            bFlashRequired = false;
        MY_LOGD("query AE mode(%d) AE state(%d) bFlashRequired(%d)", AeMode, AeState, bFlashRequired);
    }
    return bFlashRequired;
}

/******************************************************************************
*
*******************************************************************************/
static
MVOID
PREPARE_STREAM(BufferList& vDstStreams, StreamId id, MBOOL criticalBuffer)
{
    vDstStreams.push_back(
        BufferSet{
            .streamId       = id,
            .criticalBuffer = criticalBuffer,
        }
    );
}

static
MVOID
ADD_DUMMY_FRAME(
    int aNumber,
    Vector< SettingSet >& vSettings,
    Vector< BufferList >& vDstStreams,
    SettingSet dummy,
    sp<StreamBufferProvider> pConsumer
)
{
    Vector<sp<IImageStreamInfo>> rawInputInfos;
    pConsumer->querySelector()->queryCollectImageStreamInfo( rawInputInfos );

    BufferList vDstStreams_raw;
    for( size_t i = 0; i < rawInputInfos.size() ; i++) {
        PREPARE_STREAM(vDstStreams_raw, rawInputInfos[i]->getStreamId(), false);
    }

    //
    for ( int i = 0; i < aNumber; ++i ) {
        vSettings.push_back(dummy);
        vDstStreams.push_back(vDstStreams_raw);
    }
}

template<typename T>
inline MINT32 UPDATE_ENTRY(IMetadata& metadata, MINT32 entry_tag, T value)
{
    IMetadata::IEntry entry(entry_tag);
    entry.push_back(value, Type2Type< T >());
    return metadata.update(entry_tag, entry);
}

#define DUMMY_FRONT 2
#define DUMMY_BACK 3

MBOOL
ZsdShot::
InitFlashFlow()
{
    MY_LOGD("ZSD in FlashRequired mode");
    //
    sp<StreamBufferProvider> pConsumer = mpConsumer.promote();
    if(pConsumer == NULL)
    {
        MY_LOGE("mpConsumer is NULL!");
        return MFALSE;
    }
    if(pConsumer->querySelector() == NULL)
    {
        MY_LOGE("mpConsumer's Selector is NULL!");
        return MFALSE;
    }

    Vector<sp<IImageStreamInfo>> rawInputInfos;
    pConsumer->querySelector()->queryCollectImageStreamInfo( rawInputInfos );

    //
    sp<IFeatureFlowControl> pFeatureFlowControl = IResourceContainer::getInstance(getOpenId())->queryFeatureFlowControl();
    if(pFeatureFlowControl == NULL)
    {
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
    IMetadata appSetting = mShotParam.mAppSetting;
    IMetadata halSetting = mShotParam.mHalSetting;
    //
    {
        IMetadata::IEntry entry(MTK_HAL_REQUEST_SENSOR_SIZE);
        entry.push_back(sensorSize, Type2Type< MSize >());
        halSetting.update(entry.tag(), entry);
    }

    Vector< SettingSet > vSettings;
    Vector< BufferList > vDstStreams;
    Vector< MINT32 >     vRequestNo;

    int iCaptureRequestNo = 0;
    SettingSet setting;
    setting.appSetting = appSetting;
    setting.halSetting = halSetting;
    //
    BufferList vDstStreams_disp;
    for( size_t i = 0; i < rawInputInfos.size() ; i++) {
        PREPARE_STREAM(vDstStreams_disp, rawInputInfos[i]->getStreamId(),  false);
    }
    PREPARE_STREAM(vDstStreams_disp, eSTREAMID_IMAGE_PIPE_YUV_00,      false);
    //
    // front dummy frame setting, reduce frame duration
    SettingSet setting_short_dummy;
    UPDATE_ENTRY(setting_short_dummy.appSetting, MTK_CONTROL_AE_MODE,      (MUINT8)MTK_CONTROL_AE_MODE_OFF);
    UPDATE_ENTRY(setting_short_dummy.appSetting, MTK_SENSOR_EXPOSURE_TIME, (MINT64)33333333);
    UPDATE_ENTRY(setting_short_dummy.appSetting, MTK_SENSOR_SENSITIVITY,   (MINT32)1000);

    // dummy frame in front
    ADD_DUMMY_FRAME(DUMMY_FRONT, vSettings, vDstStreams, setting_short_dummy, pConsumer);
    // capture frame
    vSettings.push_back(setting);
    vDstStreams.push_back(vDstStreams_disp);
    // back dummy frame
    SettingSet setting_dummy;
    ADD_DUMMY_FRAME(DUMMY_BACK, vSettings, vDstStreams, setting_dummy, pConsumer);

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
    iCaptureRequestNo = vRequestNo[DUMMY_FRONT];
    MY_LOGD("FeatureFlowControl submitSetting (%d)", iCaptureRequestNo);
    //
    if(pConsumer == NULL)
    {
        MY_LOGE("Consumer is NULL!");
        return MFALSE;
    }
    sp<ZsdFlashSelector> pSelector = new ZsdFlashSelector();
    pSelector->setWaitRequestNo(iCaptureRequestNo);
    status_t status = pConsumer->switchSelector(pSelector);// If other consumer exist, they will auto switch.
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
MBOOL
ZsdShot::
UninitFlashFlow()
{
    //(1)wait pipeline done
    MY_LOGD("waitUntilDrained (%d) start",mCapReqNo);
    CAM_TRACE_BEGIN("waitUntilDrained");
    //
    mpPipeline->waitUntilDrained();
    //
    CAM_TRACE_END();
    MY_LOGD("waitUntilDrained (%d) end",mCapReqNo);
    //
    //(2)change to ZSD selector
    sp<StreamBufferProvider> pConsumer = mpConsumer.promote();
    if(pConsumer == NULL)
    {
        MY_LOGE("mpConsumer is NULL!");
        return MFALSE;
    }
    sp<ZsdSelector> pSelector = new ZsdSelector();
    pConsumer->switchSelector(pSelector);// If other consumer exist, they will auto switch.

    MY_LOGD("change to ZSD selector");
    return MTRUE;
}

/******************************************************************************
*
*******************************************************************************/
MUINT32
ZsdShot::
getRotation() const
{
    return mShotParam.mu4Transform;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdShot::
CheckIsNeedCreatePipeline()
{
    MBOOL ret = MFALSE;
    //
    if( mpPipeline == NULL )
    {
        MY_LOGD("mpPipeline is NULL, need to create pipeline");
        return MTRUE;
    }
    //
    if( mLastPipelineMode != getLegacyPipelineMode() )
    {
        MY_LOGD("LegacyPipelineMode changed (%d)->(%d), need to create pipeline",mLastPipelineMode,getLegacyPipelineMode());
        return MTRUE;
    }
    //
#warning "Need to use run-time change Rotation method to avoid re-create pipeline"
    if( mLastRotation != getRotation() )
    {
        MY_LOGD("Rotation changed (%d)->(%d), need to create pipeline",mLastRotation,getRotation());
        return MTRUE;
    }
    //
#warning "Need to use run-time change JpegSize method to avoid re-create pipeline"
    if( mLastJpegsize != mJpegsize )
    {
        MY_LOGD("JpegSize changed (%dx%d)->(%dx%d), need to create pipeline",
            mLastJpegsize.w,mLastJpegsize.h,
            mJpegsize.w,mJpegsize.h);
        return MTRUE;
    }
    //
    MY_LOGD("no need to create pipeline");
    return ret;
}

/******************************************************************************
*
*******************************************************************************/
MBOOL
ZsdShot::
createStreams(NSCam::v1::StreamBufferProvider* rpProvider)
{
    CAM_TRACE_CALL();
    MUINT32 const openId     = getOpenId();
    MUINT32 const sensorMode = mu4Scenario;
    MUINT32 const bitDepth   = mu4Bitdepth;
    //
    MSize const previewsize   = MSize(mShotParam.mi4PostviewWidth, mShotParam.mi4PostviewHeight);
    MINT const previewfmt     = static_cast<EImageFormat>(mShotParam.miPostviewDisplayFormat);
    MINT const yuvfmt         = eImgFmt_YUY2;
    //
    MSize const thumbnailsize = MSize(mJpegParam.mi4JpegThumbWidth, mJpegParam.mi4JpegThumbHeight);
    //
    // zsd flow: query IImageStreamInfo of raw stream from provider
    Vector<sp<IImageStreamInfo>> rawInputInfos;
    rpProvider->querySelector()->queryCollectImageStreamInfo( rawInputInfos );
    //
    for(size_t i = 0 ; i < rawInputInfos.size() ; i++)
    {
        MY_LOGD("zsd raw(%d) stream %#" PRIxPTR , "(%s) size(%dx%d), fmt 0x%x",
            i,
            rawInputInfos[i]->getStreamId(),
            rawInputInfos[i]->getStreamName(),
            rawInputInfos[i]->getImgSize().w,
            rawInputInfos[i]->getImgSize().h,
            rawInputInfos[i]->getImgFormat()
           );
    }
    //
#if 1
    // postview
    if( isDataMsgEnabled(EIShot_DATA_MSG_POSTVIEW) )
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
    //if( isDataMsgEnabled(ECamShot_DATA_MSG_YUV|ECamShot_DATA_MSG_JPEG) )
    {
        MSize size        = mJpegsize;
        MINT format       = yuvfmt;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = getRotation();
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "ZsdShot:Yuv",
                    eSTREAMID_IMAGE_PIPE_YUV_JPEG,
                    eSTREAMTYPE_IMAGE_INOUT,
                    2, 2,
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
    //if( isDataMsgEnabled(ECamShot_DATA_MSG_JPEG) && thumbnailsize != MSize(0,0))
    {
        MSize size        = thumbnailsize;
        MINT format       = yuvfmt;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = 0;
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "ZsdShot:ThumbnailYuv",
                    eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL,
                    eSTREAMTYPE_IMAGE_INOUT,
                    2, 2,
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
    //if( isDataMsgEnabled(ECamShot_DATA_MSG_JPEG) )
    {
        MSize size        = mJpegsize;
        MINT format       = eImgFmt_BLOB;
        MUINT const usage = 0; //not necessary here
        MUINT32 transform = 0;
        sp<IImageStreamInfo>
            pStreamInfo = createImageStreamInfo(
                    "ZsdShot:Jpeg",
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
ZsdShot::
createPipeline(Vector<BufferItemSet>& rBufferSet)
{
    CAM_TRACE_CALL();
    FUNC_START;
    LegacyPipelineBuilder::ConfigParams LPBConfigParams;
    LPBConfigParams.mode = getLegacyPipelineMode();
    LPBConfigParams.enableEIS = MFALSE;
    LPBConfigParams.enableLCS = MFALSE; // P1 don't need LCS generate because ZSD don't need P1.
    // check LCSO buffer exist or not
    for(size_t i = 0; i < rBufferSet.size() ; i++)
    {
        if(rBufferSet[i].id == eSTREAMID_IMAGE_PIPE_RAW_LCSO)
            LPBConfigParams.enableLCS = MTRUE;
    }
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
                                            "ZsdShot",
                                            LPBConfigParams);
    CHECK_OBJECT(pBuilder);

    sp<ImageCallback> pCallback = new ImageCallback(this, 0);
    mpCallback = pCallback;

    sp<BufferCallbackHandler> pCallbackHandler = new BufferCallbackHandler(getOpenId());
    pCallbackHandler->setImageCallback(pCallback);
    mpCallbackHandler = pCallbackHandler;

    sp<StreamBufferProviderFactory> pFactory =
                StreamBufferProviderFactory::createInstance();
    //
    {
        CAM_TRACE_NAME("pBuilder->setSrc");
        MY_LOGD("createPipeline for ZSD");
        Vector<PipelineImageParam> imgSrcParam;
        // zsd flow
        sp<IImageStreamInfo> pStreamInfo = NULL;
        for(size_t i = 0; i < rBufferSet.size() ; i++)
        {
            pStreamInfo = rBufferSet[i].streamInfo;
            //
            if(pStreamInfo == NULL)
            {
                MY_LOGE("Can not find stream info (0x%x),(%p)", rBufferSet[i].id, rBufferSet[i].heap.get());
                return MFALSE;
            }
            sp<IImageBuffer> pBuf = rBufferSet[i].heap->createImageBuffer();
            if(pBuf == NULL)
            {
                MY_LOGE("Can not build image buffer (0x%x),(%p)", rBufferSet[i].id, rBufferSet[i].heap.get());
                return MFALSE;
            }
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            pPool->addBuffer(pBuf);
            pCallbackHandler->setBufferPool(pPool);
            //
            pFactory->setImageStreamInfo(pStreamInfo);
            pFactory->setUsersPool(
                    pCallbackHandler->queryBufferPool(pStreamInfo->getStreamId())
                );
            PipelineImageParam imgParam = {
                .pInfo     = pStreamInfo,
                .pProvider = pFactory->create(),
                .usage     = 0
            };
            imgSrcParam.push_back(imgParam);
        }
        //
        if( OK != pBuilder->setSrc(imgSrcParam) ) {
            MY_LOGE("setSrc failed");
            return MFALSE;
        //
        }
    }
    //
    Vector<PipelineImageParam> vImageParam;
    {
        CAM_TRACE_NAME("pBuilder->setDst");
        if( mpInfo_Yuv.get() )
        {
            CAM_TRACE_NAME("set mpInfo_Yuv");
            sp<IImageStreamInfo> pStreamInfo = mpInfo_Yuv;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                CAM_TRACE_NAME("allocateBuffer");
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
                .pProvider = pFactory->create(),
                .usage     = 0
            };
            vImageParam.push_back(imgParam);
        }
        //
        if( mpInfo_YuvPostview.get() )
        {
            CAM_TRACE_NAME("set mpInfo_YuvPostview");
            sp<IImageStreamInfo> pStreamInfo = mpInfo_YuvPostview;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                CAM_TRACE_NAME("allocateBuffer");
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
        //
        if( mpInfo_YuvThumbnail.get() )
        {
            CAM_TRACE_NAME("set mpInfo_YuvThumbnail");
            sp<IImageStreamInfo> pStreamInfo = mpInfo_YuvThumbnail;
            //
            PipelineImageParam imgParam = {
                .pInfo     = pStreamInfo,
                .pProvider = nullptr,
                .usage     = 0
            };
            vImageParam.push_back(imgParam);
        }
        //
        if( mpInfo_Jpeg.get() )
        {
            CAM_TRACE_NAME("set mpInfo_Jpeg");
            sp<IImageStreamInfo> pStreamInfo = mpInfo_Jpeg;
            //
            sp<CallbackBufferPool> pPool = new CallbackBufferPool(pStreamInfo);
            {
                CAM_TRACE_NAME("allocateBuffer");
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
        //
        if( OK != pBuilder->setDst(vImageParam) ) {
            MY_LOGE("setDst failed");
            return MFALSE;
        }
    }
    //
    CAM_TRACE_BEGIN("pBuilder->create");
    mpPipeline = pBuilder->create();
    CAM_TRACE_END();
    //
    FUNC_END;
    return MTRUE;

}

/******************************************************************************
*
*******************************************************************************/
MINT
ZsdShot::
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
ZsdShot::
getSelectorData(IMetadata& rAppSetting,
                      IMetadata& rHalSetting,
                      Vector<BufferItemSet>& rBufferSet)
{
    CAM_TRACE_CALL();
    //
    sp<StreamBufferProvider> pConsumer = mpConsumer.promote();
    if(pConsumer == NULL)
    {
        MY_LOGE("mpConsumer is NULL!");
        return UNKNOWN_ERROR;
    }
    //
    sp< ISelector > pSelector = pConsumer->querySelector();
    if(pSelector == NULL)
    {
        MY_LOGE("can't find Selector in Consumer");
        return UNKNOWN_ERROR;
    }
    //
    status_t status = OK;
    MINT32 rRequestNo;
    Vector<ISelector::MetaItemSet> rvResultMeta;
    android::sp<IImageBufferHeap> rpHeap;
    status = pSelector->getResult(rRequestNo, rvResultMeta, rBufferSet);

    CAM_TRACE_FMT_BEGIN("getSelectorData_%d",rRequestNo);

    if(status!=OK)
    {
        MY_LOGE("Selector getResult Fail!");
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }

    if(rvResultMeta.size()!=2)
    {
        MY_LOGE("ZsdSelect getResult rvResultMeta != 2");
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }

    MBOOL bAppSetting = MFALSE, bHalSetting = MFALSE;
    for(MUINT32 i=0; i<rvResultMeta.size(); i++)
    {
        if(rvResultMeta.editItemAt(i).id == eSTREAMID_META_APP_DYNAMIC_P1)
        {
            rAppSetting = rvResultMeta.editItemAt(i).meta;
            bAppSetting = MTRUE;
        }
        else
        if(rvResultMeta.editItemAt(i).id == eSTREAMID_META_HAL_DYNAMIC_P1)
        {
            // overwrite some setting
            rHalSetting += rvResultMeta.editItemAt(i).meta;
            bHalSetting = MTRUE;
        }
    }
    //
    if( !bHalSetting ||
        !bAppSetting)
    {
        MY_LOGE("can't find App(%d)/Hal(%d) Setting from ZsdSelect getResult rvResultMeta",
                bAppSetting,
                bHalSetting);
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }
    //
#if 0
    pBuffer = rpHeap->createImageBuffer();
    //
    if(pBuffer == NULL)
    {
        MY_LOGE("get buffer is NULL!");
        CAM_TRACE_FMT_END();
        return UNKNOWN_ERROR;
    }
#endif

    CAM_TRACE_FMT_END();
    return OK;
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdShot::
onMetaReceived(
    MUINT32         const requestNo,
    StreamId_T      const streamId,
    MBOOL           const errorResult,
    IMetadata       const result
)
{
    CAM_TRACE_FMT_BEGIN("onMetaReceived No%d,StreamID %#" PRIxPTR, requestNo,streamId);
    MY_LOGD("requestNo %d, error %d stream %#" PRIxPTR, requestNo, errorResult, streamId);
    //
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        int idx = mResultMetadataSetMap.indexOfKey(requestNo);
        if(idx < 0 )
        {
            MY_LOGE("mResultMetadataSetMap can't find requestNo(%d)",requestNo);
            return;
        }
    }
    //
    if (streamId == eSTREAMID_META_APP_FULL)
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        mResultMetadataSetMap.editValueFor(requestNo).appResultMetadata = result;
        //
        mResultMetadataSetMap.editValueFor(requestNo).mMetaCnt++;
        MY_LOGD("mMetaCnt %d", mResultMetadataSetMap.editValueFor(requestNo).mMetaCnt);
        //
#if DNG_META_CB
        if(mResultMetadataSetMap.editValueFor(requestNo).mMetaCnt == 2)
        {
            if(isDataMsgEnabled(EIShot_DATA_MSG_RAW))
            {
                handleDNGMetaCB(requestNo);//dng metadata callback
            }
        }
#endif
        //
        mResultMetadataSetMap.editValueFor(requestNo).callbackState |= FULL_APP_META_CB_DONE;
        //
        if( mResultMetadataSetMap.editValueFor(requestNo).callbackState == (JPEG_CB_DONE|FULL_APP_META_CB_DONE|FULL_HAL_META_CB_DONE) )
        {
            //clear result metadata
            mResultMetadataSetMap.removeItem(requestNo);
            MY_LOGD("clear mResultMetadataSetMap(%d)",requestNo);
        }
    }
    else if (streamId == eSTREAMID_META_HAL_FULL)
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        mResultMetadataSetMap.editValueFor(requestNo).halResultMetadata = result;
        //
        mResultMetadataSetMap.editValueFor(requestNo).mMetaCnt++;
        MY_LOGD("mMetaCnt %d", mResultMetadataSetMap.editValueFor(requestNo).mMetaCnt);
        //
#if DNG_META_CB
        if(mResultMetadataSetMap.editValueFor(requestNo).mMetaCnt == 2)
        {
            if(isDataMsgEnabled(EIShot_DATA_MSG_RAW))
            {
                handleDNGMetaCB(requestNo);//dng metadata callback
            }
        }
#endif
        //
        mResultMetadataSetMap.editValueFor(requestNo).callbackState |= FULL_HAL_META_CB_DONE;
        //
        if( mResultMetadataSetMap.editValueFor(requestNo).callbackState == (JPEG_CB_DONE|FULL_APP_META_CB_DONE|FULL_HAL_META_CB_DONE) )
        {
            //clear result metadata
            mResultMetadataSetMap.removeItem(requestNo);
            MY_LOGD("clear mResultMetadataSetMap(%d)",requestNo);
        }
    }
    else if (streamId == eSTREAMID_META_HAL_DYNAMIC_P2) //p2 done
    {
        {
            Mutex::Autolock _l(mShotStateLock);
            mShotState = JpegOnWorking;
            mCondShotState.signal();
            MY_LOGD("start JpegOnWorking");
        }
        if( isDataMsgEnabled(EIShot_DATA_MSG_RAW))
        {
            MY_LOGD("Ready to notify p2done & Shutter");
            //mpShotCallback->onCB_P2done();
            mpShotCallback->onCB_Shutter(true,0);
        }
    }
    //
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        int idx = mResultMetadataSetMap.indexOfKey(requestNo);
        if(idx >= 0 )
        {
            mResultMetadataSetMap.editValueFor(requestNo).appResultMetadata += mResultMetadataSetMap.editValueFor(requestNo).selectorAppMetadata;
        }
    }
    CAM_TRACE_FMT_END();
}


/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdShot::
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
        CAM_TRACE_NAME("handleDataCallback");
        if (streamId == eSTREAMID_IMAGE_PIPE_YUV_00)
        {
            handlePostViewData( pBuffer.get() );
        }
        else if (streamId == eSTREAMID_IMAGE_JPEG)
        {
            //
            {
                Mutex::Autolock _l(mResultMetadataSetLock);
                if( !mResultMetadataSetMap.editValueFor(requestNo).isAlreadyReturnedSelectorBuf )
                {
                    CAM_TRACE_BEGIN("return Selector buffer");
                    sp<StreamBufferProvider> pConsumer = IResourceContainer::getInstance(getOpenId())->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
                    MY_LOGD("Query Consumer OpenID(%d) StreamID(%d)", getOpenId(), eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
                    if(pConsumer == NULL)
                    {
                        MY_LOGE("can't find StreamBufferProvider in ConsumerContainer");
                    }
                    else
                    {
                        sp< ISelector > pSelector = pConsumer->querySelector();
                        Vector<BufferItemSet>& bufferSet = mResultMetadataSetMap.editValueFor(requestNo).selectorGetBufs;
                        if(pSelector == NULL)
                        {
                            MY_LOGE("can't find Selector in Consumer when p2done");
                        }
                        else if(bufferSet.size() == 0)
                        {
                            MY_LOGE("selectorGetBuf is NULL when p2done");
                        }
                        else
                        {
                            for(size_t i = 0; i < bufferSet.size() ; i++)
                                pSelector->returnBuffer(bufferSet.editItemAt(i));
                            mResultMetadataSetMap.editValueFor(requestNo).selectorGetBufs.clear();
                            mResultMetadataSetMap.editValueFor(requestNo).isAlreadyReturnedSelectorBuf = true;
                        }
                    }
                    CAM_TRACE_END();
                }
            }
            //
            {
                Mutex::Autolock _l(mShotStateLock);
                mShotState = NotWorking;
                mCondShotState.signal();
                MY_LOGD("start NotWorking");
                //
                decProcessingCnt();
            }
            //
            handleJpegData(pBuffer.get());
            //
            {
                Mutex::Autolock _l(mResultMetadataSetLock);
                mResultMetadataSetMap.editValueFor(requestNo).callbackState |= JPEG_CB_DONE;
                //
                if( mResultMetadataSetMap.editValueFor(requestNo).callbackState == (JPEG_CB_DONE|FULL_APP_META_CB_DONE|FULL_HAL_META_CB_DONE) )
                {
                    //clear result metadata
                    mResultMetadataSetMap.removeItem(requestNo);
                    MY_LOGD("clear mResultMetadataSetMap(%d)",requestNo);
                }
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
            String8 filename = String8::format("%s/zsdshot_%dx%d",
                    DUMP_PATH, pBuffer->getImgSize().w, pBuffer->getImgSize().h);
            switch( data )
            {
                case NSCamShot::ECamShot_DATA_MSG_RAW:
                    filename += String8::format("_%d.raw", pBuffer->getBufStridesInBytes(0));
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
MVOID
ZsdShot::
incProcessingCnt()
{
    Mutex::Autolock _l(mProcessingCntLock);
    mInProcessingCnt++;
    MY_LOGD("inc processing cnt to %d", mInProcessingCnt);
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdShot::
decProcessingCnt()
{
    Mutex::Autolock _l(mProcessingCntLock);
    mInProcessingCnt--;
    MY_LOGD("dec processing cnt to %d", mInProcessingCnt);
#define MAX_PROCESSING_CNT 2
    if( mInProcessingCnt < MAX_PROCESSING_CNT )
    {
        mCondProcessingCnt.signal();
    }
#undef MAX_PROCESSING_CNT

}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdShot::
waitPreviousProcessing()
{
    Mutex::Autolock _l(mProcessingCntLock);
    //
#define WAIT_BUFFER_TIMEOUT (3000000000) //ns
#define MAX_PROCESSING_CNT_FOR_RAW 0
#define MAX_PROCESSING_CNT_FOR_COMMON 2
    //
    status_t status = OK;
    if(isDataMsgEnabled(EIShot_DATA_MSG_RAW))
    {
        if( mInProcessingCnt > MAX_PROCESSING_CNT_FOR_RAW )
        {
            MY_LOGD("RAW case: wait for processing cnt(%d) to (%d), start waiting %lld ns", mInProcessingCnt,MAX_PROCESSING_CNT_FOR_RAW, WAIT_BUFFER_TIMEOUT);
            status = mCondProcessingCnt.waitRelative(mProcessingCntLock,WAIT_BUFFER_TIMEOUT);
        }
    }
    else
    {
        if( mInProcessingCnt >= MAX_PROCESSING_CNT_FOR_COMMON )
        {
            MY_LOGD("wait for processing cnt(%d) to (%d), start waiting %lld ns", mInProcessingCnt,MAX_PROCESSING_CNT_FOR_COMMON-1, WAIT_BUFFER_TIMEOUT);
            status = mCondProcessingCnt.waitRelative(mProcessingCntLock,WAIT_BUFFER_TIMEOUT);
        }
    }
    //
    if( status != OK )
    {
        MY_LOGW("wait time out!");
    }
    //
#undef WAIT_BUFFER_TIMEOUT
#undef MAX_PROCESSING_CNT_FOR_COMMON
#undef MAX_PROCESSING_CNT_FOR_RAW
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdShot::
prepareSetting()
{
    CAM_TRACE_BEGIN("Prepare Setting");
    //
    {
        Mutex::Autolock _l(mResultMetadataSetLock);
        IMetadata resultAppMetadata;
        IMetadata resultHalMetadata;
        IMetadata selectorAppMetadata;
        Vector<BufferItemSet> bufferSet;
        mResultMetadataSetMap.add(static_cast<MUINT32>(mCapReqNo),ResultSet_T{static_cast<MUINT32>(mCapReqNo), resultAppMetadata, resultHalMetadata, selectorAppMetadata, bufferSet, false, 0, NONE_CB_DONE});
    }
    //
    // update post nr settings
    MBOOL isMfbShot = MFALSE;
    updatePostNRSetting(MTK_NR_MODE_AUTO, isMfbShot, &mShotParam.mHalSetting);
    //
    mu4Scenario = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
    mu4Bitdepth = 10;
    //
     HwInfoHelper helper(getOpenId());
     if( ! helper.updateInfos() ) {
         MY_LOGE("cannot properly update infos");
     }
     if( ! helper.getSensorSize( mu4Scenario, mSensorSize) )
     {
         MY_LOGE("cannot get params about sensor");
     }
     //
     mJpegsize      = (getRotation() & eTransform_ROT_90) ?
         MSize(mShotParam.mi4PictureHeight, mShotParam.mi4PictureWidth):
         MSize(mShotParam.mi4PictureWidth, mShotParam.mi4PictureHeight);
    //
    //sp<StreamBufferProvider> pConsumer = IResourceContainer::getInstance(getOpenId())->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
     mpConsumer = IResourceContainer::getInstance(getOpenId())->queryConsumer( eSTREAMID_IMAGE_PIPE_RAW_OPAQUE );
     MY_LOGD("Query Consumer OpenID(%d) StreamID(%d)", getOpenId(), eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
     if(mpConsumer.promote() == NULL)
     {
         MY_LOGE("can't find StreamBufferProvider in ConsumerContainer");
     }

    CAM_TRACE_END();
}

/*******************************************************************************
*
********************************************************************************/
MVOID
ZsdShot::
previousCapFlowCtrl()
{
#define WAIT_BUFFER_TIMEOUT (3000000000) //ns
    {
        CAM_TRACE_BEGIN("Wait Pre P2 Working");
        Mutex::Autolock _l(mShotStateLock);
        if( mShotState == P2OnWorking )
        {
            MY_LOGD("wait for mShotState is not P2OnWorking, start waiting %lld ns", WAIT_BUFFER_TIMEOUT);
            mCondShotState.waitRelative(mShotStateLock, WAIT_BUFFER_TIMEOUT);
        }
        else if ( mShotState == NotWorking )
        {
            MY_LOGD("start P2OnWorking");
            mShotState = P2OnWorking;
        }
        else
        {
            MY_LOGD("JpegOnWorking still on working");
        }
        CAM_TRACE_END();
    }
    //
    {
        MY_LOGD("waitPreviousProcessing(%d) start",mCapReqNo-1);
        CAM_TRACE_BEGIN("waitPreviousProcessing");
        //
        waitPreviousProcessing();
        //
        MY_LOGD("waitPreviousProcessing(%d) end InProcess cnt(%d)",mCapReqNo-1,mInProcessingCnt);
        CAM_TRACE_END();
    }
#undef WAIT_BUFFER_TIMEOUT
}
/*******************************************************************************
*
********************************************************************************/
MBOOL
ZsdShot::
constructNewPipelineFlow(Vector<BufferItemSet>& rBufSet)
{
    CAM_TRACE_NAME("createPipeline");
    MY_LOGD("start to create new pipeline");
    // create new pipeline
    if ( ! createStreams(mpConsumer.promote().get()) ) {
        MY_LOGE("createStreams failed");
        return MFALSE;
    }
    //
    if ( ! createPipeline(rBufSet) ) {
        MY_LOGE("createPipeline failed");
        return MFALSE;
    }
    //
    CHECK_OBJECT(mpPipeline);
    //
    {
        CAM_TRACE_NAME("setup ResultProcessor");
        sp<ResultProcessor> pResultProcessor = mpPipeline->getResultProcessor().promote();
        CHECK_OBJECT(pResultProcessor);

        //partial
        sp<MetadataListener> pListenerPartial = new MetadataListener(this);
        mpListenerPartial = pListenerPartial;
        //
        pResultProcessor->registerListener( 0, 1000, true, pListenerPartial);
        //
        //full
        sp<MetadataListener> pListener = new MetadataListener(this);
        mpListenerFull = pListener;
        //
        pResultProcessor->registerListener( 0, 1000, false, pListener);
        //
        if( rBufSet.size() != 0 )
        {
            IMetadata selectorAppMetadata;
            {
                Mutex::Autolock _l(mResultMetadataSetLock);
                selectorAppMetadata = mResultMetadataSetMap.editValueFor(mCapReqNo).selectorAppMetadata;
            }
            sp<TimestampProcessor> pTimestampProcessor = mpPipeline->getTimestampProcessor().promote();

            pTimestampProcessor->onResultReceived(
                                    mCapReqNo,
                                    eSTREAMID_META_APP_DYNAMIC_P1,
                                    MFALSE,
                                    selectorAppMetadata);
        }
    }
    return MTRUE;
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
ZsdShot::
reUsePipelineFlow(Vector<BufferItemSet>& rBufSet)
{
    CAM_TRACE_NAME("preaparePipeline");
    MY_LOGD("No need to create new pipeline");

    //update p1 buffer to pipeline pool
    for(size_t i =0 ; i < rBufSet.size() ; i++)
    {
        sp<CallbackBufferPool> pPool = mpCallbackHandler->queryBufferPool( rBufSet[i].id );
        if( pPool == NULL)
        {
            MY_LOGE("query Pool Fail!");
            return MFALSE;
        }
        sp<IImageBuffer> pBuf = rBufSet[i].heap->createImageBuffer();
        if(pBuf == NULL)
        {
            MY_LOGE("Can not build image buffer (0x%x),(%p)", rBufSet[i].id, rBufSet[i].heap.get());
            return MFALSE;
        }
        pPool->flush();
        pPool->addBuffer(pBuf);
    }
    //
    {
        IMetadata selectorAppMetadata;
        {
            Mutex::Autolock _l(mResultMetadataSetLock);
            selectorAppMetadata = mResultMetadataSetMap.editValueFor(mCapReqNo).selectorAppMetadata;
        }
        sp<TimestampProcessor> pTimestampProcessor = mpPipeline->getTimestampProcessor().promote();
        pTimestampProcessor->onResultReceived(
                                mCapReqNo,
                                eSTREAMID_META_APP_DYNAMIC_P1,
                                MFALSE,
                                selectorAppMetadata);
    }
    return MTRUE;
}

