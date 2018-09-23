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
#define LOG_TAG "YUVEffectHal"
#define EFFECT_NAME "YUVEffect"
#define MAJOR_VERSION 0
#define MINOR_VERSION 1



#define UNUSED(x) (void)x

#include <cutils/log.h>
#include <cutils/properties.h>

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
#include <mtkcam/feature/effectHalBase/IEffectHal.h>
#include <mtkcam/feature/effectHalBase/EffectHalBase.h>
//
#include <mtkcam/feature/stereo/effecthal/YUVEffectHal.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/feature/eis/eis_ext.h>
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Misc.h>
#include <mtkcam/utils/std/Trace.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
using namespace NSCam::NSIoPipe;

/******************************************************************************
 *
 ******************************************************************************/
#define FUNCTION_LOG_START
#define FUNCTION_LOG_END

#define EIS_REGION_META_MV_X_LOC 6
#define EIS_REGION_META_MV_Y_LOC 7
#define EIS_REGION_META_FROM_RRZ_LOC 8

#define RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(request, rFrameInfo, BufID, rImgBuf) \
if (request->vOutputFrameInfo.indexOfKey(BufID) >= 0) \
{ \
    rFrameInfo = request->vOutputFrameInfo.valueFor(BufID);\
    rFrameInfo->getFrameBuffer(rImgBuf); \
}\
else\
{\
    MY_LOGE("Cannot find the frameBuffer in the effect request, frameID=%d! Return!", BufID); \
    return MFALSE; \
}

//  ============================  updateEntry  ============================
template <typename T>
inline MVOID
updateEntry(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T const& val
)
{
    if( pMetadata == NULL ) {
        MY_LOGW("pMetadata == NULL");
        return;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
}

//  ============================  tryGetMetadata  ============================
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if( pMetadata == NULL ) {
        MY_LOGW("pMetadata == NULL");
        return MFALSE;
    }
    //
    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if( !entry.isEmpty() ) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    return MFALSE;
}

namespace NSCam{
//************************************************************************
//
//************************************************************************
YUVEffectHal::
YUVEffectHal()
: miSensorIdx_Main1(-1),
  miSensorIdx_Main2(-1),
  mp3AHal_Main1(NULL),
  mp3AHal_Main2(NULL),
  mpINormalStream(NULL)
{
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("debug.yuveffect.dump", cLogLevel, "0");
    miDump = ::atoi(cLogLevel);
    if(miDump>0)
    {
        msFilename = std::string("/sdcard/vsdof/Cap/yuveffectout/");
        NSCam::Utils::makePath(msFilename.c_str(), 0660);
    }

    ::property_get("debug.camera.log.yuveffecthal", cLogLevel, "0");
    mLogLevel = atoi(cLogLevel);
    if ( mLogLevel == 2 ) {
        mbDebugLog = MTRUE;
    }

    ::property_get("debug.yuveffecthal.off3Ainfo", cLogLevel, "0");
    miOff3AInfo = ::atoi(cLogLevel);
}
//************************************************************************
//
//************************************************************************
YUVEffectHal::
~YUVEffectHal()
{

}
//************************************************************************
//
//************************************************************************
bool
YUVEffectHal::
allParameterConfigured()
{

    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
cleanUp()
{
    MY_LOGD("+");
    if(mpINormalStream != NULL)
    {
        mpINormalStream->uninit(LOG_TAG);
        mpINormalStream->destroyInstance();
        mpINormalStream = NULL;
    }

    if(mp3AHal_Main1)
    {
        mp3AHal_Main1->destroyInstance("YUV_EFFECTHAL_3A_MAIN1");
        mp3AHal_Main1 = NULL;
    }

    if(mp3AHal_Main2)
    {
        mp3AHal_Main2->destroyInstance("YUV_EFFECTHAL_3A_MAIN2");
        mp3AHal_Main2 = NULL;
    }

    MY_LOGD("-");
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
initImpl()
{
    FUNCTION_LOG_START;
    CAM_TRACE_BEGIN("YUVEffectHal::createInstance+init");

    // get sensor index
    if (!StereoSettingProvider::getStereoSensorIndex(miSensorIdx_Main1, miSensorIdx_Main2))
    {
        MY_LOGE("StereoSettingProvider getStereoSensorIndex failed!");
        return MFALSE;
    }

    mpINormalStream = NSCam::NSIoPipe::NSPostProc::INormalStream::createInstance(miSensorIdx_Main1);
    if (mpINormalStream == NULL)
    {
        MY_LOGE("mpINormalStream create instance for YUVEffectHal failed!");
        cleanUp();
        return MFALSE;
    }
    mpINormalStream->init(LOG_TAG);
    CAM_TRACE_END();

    // 3A: create instance
    // UT does not test 3A
    CAM_TRACE_BEGIN("YUVEffectHal::create_3A_instance");
    #ifndef GTEST
    mp3AHal_Main1 = MAKE_Hal3A(miSensorIdx_Main1, "YUV_EffectHal_3A_MAIN1");
    mp3AHal_Main2 = MAKE_Hal3A(miSensorIdx_Main2, "YUV_EffectHal_3A_MAIN2");
    MY_LOGD("3A create instance, Main1: %x Main2: %x", mp3AHal_Main1, mp3AHal_Main2);
    #endif
    CAM_TRACE_END();

    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
uninitImpl()
{
    FUNCTION_LOG_START;
    CAM_TRACE_NAME("YUVEffectHal::uninitImpl");
    cleanUp();
    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
prepareImpl()
{
    FUNCTION_LOG_START;
    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
releaseImpl()
{
    FUNCTION_LOG_START;

    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
getNameVersionImpl(
    EffectHalVersion &nameVersion) const
{
    FUNCTION_LOG_START;

    nameVersion.effectName = EFFECT_NAME;
    nameVersion.major = MAJOR_VERSION;
    nameVersion.minor = MINOR_VERSION;

    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
getCaptureRequirementImpl(
    EffectParameter *inputParam,
    Vector<EffectCaptureRequirement> &requirements) const
{
    FUNCTION_LOG_START;
    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
setParameterImpl(
    String8 &key,
    String8 &object)
{
    FUNCTION_LOG_START;
    UNUSED(key);
    UNUSED(object);
    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
setParametersImpl(sp<EffectParameter> parameter)
{
    FUNCTION_LOG_START;

    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
startImpl(
    uint64_t *uid)
{
    FUNCTION_LOG_START;
    UNUSED(uid);
    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
abortImpl(
    EffectResult &result,
    EffectParameter const *parameter)
{
    FUNCTION_LOG_START;
    UNUSED(result);
    UNUSED(parameter);
    //
    onFlush();
    //
    FUNCTION_LOG_END;
    return OK;
}
//************************************************************************
//
//************************************************************************
status_t
YUVEffectHal::
updateEffectRequestImpl(const EffectRequestPtr request)
{
    FUNCTION_LOG_START;
    Mutex::Autolock _l(mLock);
    MBOOL ret = MTRUE;
    //
    if (request == NULL)
    {
        MY_LOGE("Effect Request is NULL");
        return MFALSE;
    }

    EffectRequestPtr req = (EffectRequestPtr)request;

    //prepare enque parameter
    QParams enqueParams, dequeParams;
    //EnquedData *enquedData = new EnquedData(request, this);

    // get current state
    sp<EffectParameter> pReqParam = request->getRequestParameter();
    DualYUVNodeOpState eState = (DualYUVNodeOpState) pReqParam->getInt(DUAL_YUV_REQUEST_STATE_KEY);
    MY_LOGD_IF(mLogLevel >= 1,"+ begin reqID=%d eState=%d", request->getRequestNo(), eState);

    MBOOL bRet;
    if(eState == STATE_CAPTURE)
        bRet = buildQParams_CAP(req, enqueParams);
    else if (eState == STATE_THUMBNAIL)
        bRet = buildQParams_THUMBNAIL(req, enqueParams);
    else if (eState == STATE_NORMAL)
        bRet = buildQParams_NORMAL(req, enqueParams);
    else
    {
        MY_LOGE("Wrong Request op state.");
        goto lbExit;
    }

    debugQParams(enqueParams);
    if(!bRet)
    {
        MY_LOGE("Failed to build P2 enque parametes.");
        goto lbExit;
    }
    // callback
    //enqueParams.mpfnCallback = onP2Callback;
    //enqueParams.mpfnEnQFailCallback = onP2FailedCallback;
    //enqueParams.mpCookie = (MVOID*) enquedData;
    enqueParams.mpfnCallback = NULL;
    enqueParams.mpfnEnQFailCallback = NULL;
    enqueParams.mpCookie = NULL;

    MY_LOGD_IF(mLogLevel >= 1,"mpINormalStream enque start! reqID=%d", request->getRequestNo());
    CAM_TRACE_BEGIN("YUVEffectHal::NormalStream::enque");
    bRet = mpINormalStream->enque(enqueParams);

    if(!bRet)
    {
        MY_LOGE("mpINormalStream enque failed! reqID=%d", request->getRequestNo());
        goto lbExit;
    }

    bRet = mpINormalStream->deque(dequeParams);
    CAM_TRACE_END();
    MY_LOGD_IF(mLogLevel >= 1,"mpINormalStream deque end! reqID=%d", request->getRequestNo());

    if(bRet)
    {
        //handleP2Done(enqueParams, enquedData);
        if(miDump>0)
        {
            FrameInfoPtr pFramePtr;
            if (eState == STATE_NORMAL)
            {
                sp<IImageBuffer> frameBuf_MV_F, frameBuf_FD;
                RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(req, pFramePtr, BID_OUT_FD, frameBuf_FD);
                RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(req, pFramePtr, BID_OUT_MV_F, frameBuf_MV_F);

                std::string saveFileName = msFilename + std::string("/FD_") + std::string(".yuv");
                frameBuf_FD->saveToFile(saveFileName.c_str());
                saveFileName = msFilename + std::string("/MV_F_") + std::string(".yuv");
                frameBuf_MV_F->saveToFile(saveFileName.c_str());
            }
            else if (eState == STATE_CAPTURE)
            {
                sp<IImageBuffer> frameBuf_MV_F_Cap_main1, frameBuf_MV_F_Cap_main2;
                RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(req, pFramePtr, BID_OUT_MV_F_CAP_MAIN1, frameBuf_MV_F_Cap_main1);
                RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(req, pFramePtr, BID_OUT_MV_F_CAP_MAIN2, frameBuf_MV_F_Cap_main2);

                std::string saveFileName = msFilename + std::string("/MV_F_Cap1") + std::string(".yuv");
                frameBuf_MV_F_Cap_main1->saveToFile(saveFileName.c_str());
                saveFileName = msFilename + std::string("/MV_F_Cap2") + std::string(".yuv");
                frameBuf_MV_F_Cap_main2->saveToFile(saveFileName.c_str());
            }
            else if (eState == STATE_THUMBNAIL)
            {
                sp<IImageBuffer> frameBuf_Thumbnail_Cap;
                RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(req, pFramePtr, BID_OUT_THUMBNAIL_CAP, frameBuf_Thumbnail_Cap);

                std::string saveFileName = msFilename + std::string("/Thumbnail_Cap") + std::string(".yuv");
                frameBuf_Thumbnail_Cap->saveToFile(saveFileName.c_str());
            }

        }

    }
    else
    {
        MY_LOGE("mpINormalStream deque failed! reqID=%d", request->getRequestNo());
        goto lbExit;
    }


    MY_LOGD_IF(mLogLevel >= 1,"- end reqID=%d", request->getRequestNo());
    //delete enquedData;
    ReleaseQParam(enqueParams);
    ReleaseMain2HalMeta(request);

    return MTRUE;
lbExit:
    //handleP2Failed(enqueParams, enquedData);
    //delete enquedData;
    ReleaseQParam(enqueParams);
    ReleaseMain2HalMeta(request);
    return MFALSE;
}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
onP2Callback(QParams& rParams)
{
    EnquedData* pEnqueData = (EnquedData*) (rParams.mpCookie);
    YUVEffectHal* pYUVEffectHal = (YUVEffectHal*) (pEnqueData->mpYUVEffectHal);
    pYUVEffectHal->handleP2Done(rParams, pEnqueData);
}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
onP2FailedCallback(QParams& rParams)
{
    MY_LOGE("YUVEffectHal operations failed!!Check the following log:");
    EnquedData* pEnqueData = (EnquedData*) (rParams.mpCookie);
    YUVEffectHal* pYUVEffectHal = (YUVEffectHal*) (pEnqueData->mpYUVEffectHal);
    pYUVEffectHal->handleP2Failed(rParams, pEnqueData);
}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
handleP2Done(QParams& rParams, EnquedData* pEnqueData)
{
    CAM_TRACE_NAME("YUVEffectHal::handleP2Done");
    EffectRequestPtr request = pEnqueData->mRequest;
    MY_LOGD_IF(mLogLevel >= 1,"+ :reqID=%d", request->getRequestNo());

    if(request->mpOnRequestProcessed!=nullptr)
    {
        request->mpOnRequestProcessed(request->mpTag, String8("Done"), request);
    }
}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
ReleaseQParam(QParams& rParams)
{
    CAM_TRACE_NAME("YUVEffectHal::ReleaseQParam");
    //Free rParams
    rParams.mvIn.clear();
    rParams.mvOut.clear();
    rParams.mvStreamTag.clear();
    rParams.mvSensorIdx.clear();
    rParams.mvCropRsInfo.clear();
    rParams.mvModuleData.clear();
    for (int i=0; i < rParams.mvTuningData.size(); i++)
    {
        MVOID* pTuningBuffer = rParams.mvTuningData[i];
        free(pTuningBuffer);
    }

}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
ReleaseMain2HalMeta(const EffectRequestPtr request)
{
    sp<EffectFrameInfo> pFrame_HalMetaMain2 = request->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
    sp<EffectParameter> pEffParam = pFrame_HalMetaMain2->getFrameParameter();
    IMetadata* pMeta = reinterpret_cast<IMetadata*>(pEffParam->getPtr("Metadata"));
    delete pMeta;
}
//************************************************************************
//
//************************************************************************
MVOID
YUVEffectHal::
handleP2Failed(QParams& rParams, EnquedData* pEnqueData)
{
    CAM_TRACE_NAME("YUVEffectHal::handleP2Failed");
    EffectRequestPtr request = pEnqueData->mRequest;
    MY_LOGD_IF(mLogLevel >= 1,"+ :reqID=%d", request->getRequestNo());
    debugQParams(rParams);
    //
    if(request->mpOnRequestProcessed!=nullptr)
    {
        request->mpOnRequestProcessed(request->mpTag, String8("Failed"), request);
    }
}
//************************************************************************
//
//************************************************************************
MBOOL
YUVEffectHal::
buildQParams_CAP(EffectRequestPtr& rEffReqPtr, QParams& rEnqueParam)
{
    FUNCTION_LOG_START;

    MY_LOGD_IF(mLogLevel >= 1,"+, reqID=%d", rEffReqPtr->getRequestNo());
    // Get the input/output buffer inside the request
    FrameInfoPtr framePtr_inMain1FSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_DualYUV_IN_FSRAW1);
    FrameInfoPtr framePtr_inMain2FSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_DualYUV_IN_FSRAW2);
    FrameInfoPtr pFramePtr;
    MVOID* pVATuningBuffer_Main1 = NULL;
    MVOID* pVATuningBuffer_Main2 = NULL;

    MUINT iFrameNum = 0;
    FrameInfoPtr framePtr_inAppMeta = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_APP);
    //--> frame 0
    {
        // input StreamTag
        rEnqueParam.mvStreamTag.push_back(ENormalStreamTag_Normal);

        iFrameNum = 0;
        FrameInfoPtr framePtr_inHalMeta_Main2 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL_MAIN2);
        // Apply tuning data
        pVATuningBuffer_Main2 = (MVOID*) malloc(sizeof(dip_x_reg_t));
        memset(pVATuningBuffer_Main2, 0, sizeof(dip_x_reg_t));

        sp<IImageBuffer> frameBuf_MV_F_Main2;
        // Apply ISP tuning
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main2, mp3AHal_Main2, MFALSE};
        TuningParam rTuningParam = applyISPTuning(pVATuningBuffer_Main2, ispConfig);
        // insert tuning data
        ssize_t res = rEnqueParam.mvTuningData.insertAt(pVATuningBuffer_Main2, iFrameNum);

        // UT does not test 3A
        #ifndef GTEST
        if(rTuningParam.pLsc2Buf != NULL)
        {
            // input: LSC2 buffer (for tuning)
            IImageBuffer* pLSC2Src = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);

            Input input_DEPI;
            input_DEPI.mPortID = PORT_DEPI;
            input_DEPI.mPortID.group = iFrameNum;
            input_DEPI.mBuffer = pLSC2Src;
            rEnqueParam.mvIn.push_back(input_DEPI);
        }
        else
        {
            MY_LOGE("LSC2 buffer from 3A is NULL!!");
            return MFALSE;
        }
        #endif

        // make sure the output is 16:9, get crop size& point
        MSize cropSize;
        MPoint startPoint;
        calCropForScreen(framePtr_inMain2FSRAW, startPoint, cropSize);

        // input: Main1 Fullsize RAW
        {
            sp<IImageBuffer> pImgBuf;
            framePtr_inMain2FSRAW->getFrameBuffer(pImgBuf);

            Input input_IMGI;
            input_IMGI.mPortID = PORT_IMGI;
            input_IMGI.mPortID.group = iFrameNum;
            input_IMGI.mBuffer = pImgBuf.get();
            rEnqueParam.mvIn.push_back(input_IMGI);
        }

        // output : MV_F_CAP
        {
            RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(rEffReqPtr, pFramePtr, BID_OUT_MV_F_CAP_MAIN2, frameBuf_MV_F_Main2);

            // insert output
            Output output_WDMA;
            output_WDMA.mPortID = PORT_WDMAO;
            output_WDMA.mPortID.group = iFrameNum;
            output_WDMA.mTransform = 0;
            output_WDMA.mBuffer = frameBuf_MV_F_Main2.get();
            rEnqueParam.mvOut.push_back(output_WDMA);

            // setCrop
            MCrpRsInfo cropInfo;
            cropInfo.mGroupID = (MUINT32) eCROP_WDMA;
            cropInfo.mCropRect.p_fractional.x=0;
            cropInfo.mCropRect.p_fractional.y=0;
            cropInfo.mCropRect.p_integral.x=startPoint.x;
            cropInfo.mCropRect.p_integral.y=startPoint.y;
            cropInfo.mCropRect.s=cropSize;
            cropInfo.mResizeDst=frameBuf_MV_F_Main2->getImgSize();
            cropInfo.mFrameGroup = iFrameNum;
            rEnqueParam.mvCropRsInfo.push_back(cropInfo);
        }

    }

    //--> frame 1
    {
        // input StreamTag
        rEnqueParam.mvStreamTag.push_back(ENormalStreamTag_Normal);

        iFrameNum = 1;
        FrameInfoPtr framePtr_inHalMeta_Main1 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL);
        // Apply tuning data
        pVATuningBuffer_Main1 = (MVOID*) malloc(sizeof(dip_x_reg_t));
        memset(pVATuningBuffer_Main1, 0, sizeof(dip_x_reg_t));

        sp<IImageBuffer> frameBuf_MV_F_Main1;
        // Apply ISP tuning
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main1, mp3AHal_Main1, MFALSE};
        TuningParam rTuningParam = applyISPTuning(pVATuningBuffer_Main1, ispConfig);
        // insert tuning data
        ssize_t res = rEnqueParam.mvTuningData.insertAt(pVATuningBuffer_Main1, iFrameNum);

        // UT does not test 3A
        #ifndef GTEST
        if(rTuningParam.pLsc2Buf != NULL)
        {
            // input: LSC2 buffer (for tuning)
            IImageBuffer* pLSC2Src = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);

            Input input_DEPI;
            input_DEPI.mPortID = PORT_DEPI;
            input_DEPI.mPortID.group = iFrameNum;
            input_DEPI.mBuffer = pLSC2Src;
            rEnqueParam.mvIn.push_back(input_DEPI);
        }
        else
        {
            MY_LOGE("LSC2 buffer from 3A is NULL!!");
            return MFALSE;
        }
        #endif

        // make sure the output is 16:9, get crop size& point
        MSize cropSize;
        MPoint startPoint;
        calCropForScreen(framePtr_inMain1FSRAW, startPoint, cropSize);

        // input: Main1 Fullsize RAW
        {
            sp<IImageBuffer> pImgBuf;
            framePtr_inMain1FSRAW->getFrameBuffer(pImgBuf);

            Input input_IMGI;
            input_IMGI.mPortID = PORT_IMGI;
            input_IMGI.mPortID.group = iFrameNum;
            input_IMGI.mBuffer = pImgBuf.get();
            rEnqueParam.mvIn.push_back(input_IMGI);
        }

        // output : MV_F_CAP
        {
            RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(rEffReqPtr, pFramePtr, BID_OUT_MV_F_CAP_MAIN1, frameBuf_MV_F_Main1);

            // insert output
            Output output_WDMA;
            output_WDMA.mPortID = PORT_WDMAO;
            output_WDMA.mPortID.group = iFrameNum;
            output_WDMA.mTransform = 0;
            output_WDMA.mBuffer = frameBuf_MV_F_Main1.get();
            rEnqueParam.mvOut.push_back(output_WDMA);

            // setCrop
            MCrpRsInfo cropInfo;
            cropInfo.mGroupID = (MUINT32) eCROP_WDMA;
            cropInfo.mCropRect.p_fractional.x=0;
            cropInfo.mCropRect.p_fractional.y=0;
            cropInfo.mCropRect.p_integral.x=startPoint.x;
            cropInfo.mCropRect.p_integral.y=startPoint.y;
            cropInfo.mCropRect.s=cropSize;
            cropInfo.mResizeDst=frameBuf_MV_F_Main1->getImgSize();
            cropInfo.mFrameGroup = iFrameNum;
            rEnqueParam.mvCropRsInfo.push_back(cropInfo);
        }

    }


    MY_LOGD_IF(mLogLevel >= 1,"-, reqID=%d", rEffReqPtr->getRequestNo());
    FUNCTION_LOG_END;
    return MTRUE;
}
//************************************************************************
//
//************************************************************************
MBOOL
YUVEffectHal::
buildQParams_THUMBNAIL(EffectRequestPtr& rEffReqPtr, QParams& rEnqueParam)
{
    FUNCTION_LOG_START;

    MY_LOGD_IF(mLogLevel >= 1,"+, reqID=%d", rEffReqPtr->getRequestNo());
    // Get the input/output buffer inside the request
    FrameInfoPtr framePtr_inMain1FSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_DualYUV_IN_FSRAW1);
    FrameInfoPtr pFramePtr;
    MVOID* pVATuningBuffer_Main1 = NULL;

    MUINT iFrameNum = 0;
    FrameInfoPtr framePtr_inAppMeta = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_APP);
    IMetadata* pMeta_InApp  = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);
    //--> frame 0
    {
        // input StreamTag
        rEnqueParam.mvStreamTag.push_back(ENormalStreamTag_Normal);

        FrameInfoPtr framePtr_inHalMeta_Main1 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL);
        // Apply tuning data
        pVATuningBuffer_Main1 = (MVOID*) malloc(sizeof(dip_x_reg_t));
        memset(pVATuningBuffer_Main1, 0, sizeof(dip_x_reg_t));

        sp<IImageBuffer> frameBuf_Thumbnail_Cap;
        // Apply ISP tuning
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main1, mp3AHal_Main1, MFALSE};
        TuningParam rTuningParam = applyISPTuning(pVATuningBuffer_Main1, ispConfig);
        // insert tuning data
        ssize_t res = rEnqueParam.mvTuningData.insertAt(pVATuningBuffer_Main1, iFrameNum);

        // UT does not test 3A
        #ifndef GTEST
        if(rTuningParam.pLsc2Buf != NULL)
        {
            // input: LSC2 buffer (for tuning)
            IImageBuffer* pLSC2Src = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);

            Input input_DEPI;
            input_DEPI.mPortID = PORT_DEPI;
            input_DEPI.mPortID.group = iFrameNum;
            input_DEPI.mBuffer = pLSC2Src;
            rEnqueParam.mvIn.push_back(input_DEPI);
        }
        else
        {
            MY_LOGE("LSC2 buffer from 3A is NULL!!");
            return MFALSE;
        }
        #endif

        // make sure the output is 16:9, get crop size& point
        MSize cropSize;
        MPoint startPoint;
        calCropForScreen(framePtr_inMain1FSRAW, startPoint, cropSize);

        // input: Main1 Fullsize RAW
        {
            sp<IImageBuffer> pImgBuf;
            framePtr_inMain1FSRAW->getFrameBuffer(pImgBuf);

            Input input_IMGI;
            input_IMGI.mPortID = PORT_IMGI;
            input_IMGI.mPortID.group = iFrameNum;
            input_IMGI.mBuffer = pImgBuf.get();
            rEnqueParam.mvIn.push_back(input_IMGI);
        }

        // output : MV_F_CAP
        {
            RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(rEffReqPtr, pFramePtr, BID_OUT_THUMBNAIL_CAP, frameBuf_Thumbnail_Cap);
            MINT32 eTransJpeg = getJpegRotation(pMeta_InApp);

            // insert output
            Output output_WROT;
            output_WROT.mPortID = PORT_WROTO;
            output_WROT.mPortID.group = iFrameNum;
            output_WROT.mTransform = eTransJpeg;
            output_WROT.mBuffer = frameBuf_Thumbnail_Cap.get();
            rEnqueParam.mvOut.push_back(output_WROT);

            //StereoArea fullraw_crop;
            //fullraw_crop = sizePrvider->getBufferSize(E_BM_PREPROCESS_FULLRAW_CROP_1);

            // setCrop
            MCrpRsInfo cropInfo;
            cropInfo.mGroupID = (MUINT32) eCROP_WROT;
            cropInfo.mCropRect.p_fractional.x=0;
            cropInfo.mCropRect.p_fractional.y=0;
            cropInfo.mCropRect.p_integral.x=startPoint.x;
            cropInfo.mCropRect.p_integral.y=startPoint.y;
            cropInfo.mCropRect.s=cropSize;
            cropInfo.mResizeDst=frameBuf_Thumbnail_Cap->getImgSize();
            cropInfo.mFrameGroup = iFrameNum;
            rEnqueParam.mvCropRsInfo.push_back(cropInfo);
        }

    }


    MY_LOGD_IF(mLogLevel >= 1,"-, reqID=%d", rEffReqPtr->getRequestNo());
    FUNCTION_LOG_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MINT32
YUVEffectHal::
DegreeToeTransform(MINT32 degree)
{
    switch(degree)
    {
        case 0:
            return 0;
        case 90:
            return eTransform_ROT_90;
        case 180:
            return eTransform_ROT_180;
        case 270:
            return eTransform_ROT_270;
        default:
            MY_LOGE("Not support degree =%d", degree);
            return -1;
    }
}

/*******************************************************************************
 *
 ********************************************************************************/
MINT32
YUVEffectHal::
getJpegRotation(IMetadata* pMeta)
{
    MINT32 jpegRotationApp = 0;
    if(!tryGetMetadata<MINT32>(pMeta, MTK_JPEG_ORIENTATION, jpegRotationApp))
    {
        MY_LOGE("Get jpegRotationApp failed!");
    }

    MINT32 rotDegreeJpeg = jpegRotationApp;
    if(rotDegreeJpeg < 0){
        rotDegreeJpeg = rotDegreeJpeg + 360;
    }

    MY_LOGD("jpegRotationApp:%d, rotDegreeJpeg:%d", jpegRotationApp, rotDegreeJpeg);

    return DegreeToeTransform(rotDegreeJpeg);;
}
//************************************************************************
//
//************************************************************************
MBOOL
YUVEffectHal::
calCropForScreen(FrameInfoPtr& pFrameInfo, MPoint &rCropStartPt, MSize& rCropSize )
{
    sp<IImageBuffer> pImgBuf;
    pFrameInfo->getFrameBuffer(pImgBuf);
    MSize srcSize = pImgBuf->getImgSize();

    // calculate the required image hight according to image ratio
    int iHeight;
    switch(StereoSettingProvider::imageRatio())
    {
        case eRatio_4_3:
            iHeight = ((srcSize.w * 3 / 4) >> 1 ) <<1;
            break;
        case eRatio_16_9:
        default:
            iHeight = ((srcSize.w * 9 / 16) >> 1 ) <<1;
            break;
    }

    if(abs(iHeight-srcSize.h) == 0)
    {
        rCropStartPt = MPoint(0, 0);
        rCropSize = srcSize;
    }
    else
    {
        rCropStartPt.x = 0;
        rCropStartPt.y = (srcSize.h - iHeight)/2;
        rCropSize.w = srcSize.w;
        rCropSize.h = iHeight;
    }

    MY_LOGD_IF(mLogLevel >= 1,"calCropForScreen rCropStartPt: (%d, %d), \
                    rCropSize: %dx%d ", rCropStartPt.x, rCropStartPt.y, rCropSize.w, rCropSize.h);

    return MTRUE;

}
//************************************************************************
//
//************************************************************************
TuningParam
YUVEffectHal::
applyISPTuning(MVOID* pVATuningBuffer, const ISPTuningConfig& ispConfig)
{
    CAM_TRACE_NAME("YUVEffectHal::applyISPTuning");
    MY_LOGD_IF(mLogLevel >= 1,"+, reqID=%d bIsResized=%d", ispConfig.pInAppMetaFrame->getRequestNo(), ispConfig.bInputResizeRaw);

    TuningParam tuningParam = {NULL, NULL};
    tuningParam.pRegBuf = reinterpret_cast<void*>(pVATuningBuffer);

    MetaSet_T inMetaSet;
    IMetadata* pMeta_InApp  = getMetadataFromFrameInfoPtr(ispConfig.pInAppMetaFrame);
    IMetadata* pMeta_InHal  = getMetadataFromFrameInfoPtr(ispConfig.pInHalMetaFrame);

    inMetaSet.appMeta = *pMeta_InApp;
    inMetaSet.halMeta = *pMeta_InHal;

    // USE resize raw-->set PGN 0
    if(ispConfig.bInputResizeRaw)
        updateEntry<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 0);
    else
        updateEntry<MUINT8>(&(inMetaSet.halMeta), MTK_3A_PGN_ENABLE, 1);

    // UT do not test setIsp
    #ifndef GTEST
    {
        MetaSet_T resultMeta;
        ispConfig.p3AHAL->setIsp(0, inMetaSet, &tuningParam, miOff3AInfo ? NULL : &resultMeta);

        // DO NOT write ISP resultMeta back into input hal Meta since there are other node doing this concurrently
        // (*pMeta_InHal) += resultMeta.halMeta;
    }
    #else
    {   // UT: use default tuning
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_G2G, 0);
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_G2C, 0);
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_GGM, 0);
        SetDefaultTuning((dip_x_reg_t*)tuningParam.pRegBuf, (MUINT32*)tuningParam.pRegBuf, tuning_tag_UDM, 0);
    }
    #endif

    MY_LOGD_IF(mLogLevel >= 1,"-, reqID=%d", ispConfig.pInAppMetaFrame->getRequestNo());
    return tuningParam;
}
//************************************************************************
//
//************************************************************************
IMetadata*
YUVEffectHal::
getMetadataFromFrameInfoPtr(sp<EffectFrameInfo> pFrameInfo)
{
    IMetadata* result;
    sp<EffectParameter> effParam = pFrameInfo->getFrameParameter();
    result = reinterpret_cast<IMetadata*>(effParam->getPtr(EFFECT_PARAMS_KEY));
    return result;
}
//************************************************************************
//
//************************************************************************
MBOOL
YUVEffectHal::
buildQParams_NORMAL(EffectRequestPtr& rEffReqPtr, QParams& rEnqueParam)
{
    FUNCTION_LOG_START;
    CAM_TRACE_NAME("YUVEffectHal::buildQParams_NORMAL");
    MY_LOGD_IF(mLogLevel >= 1,"+, reqID=%d", rEffReqPtr->getRequestNo());

    // Get the input/output buffer inside the request
    FrameInfoPtr framePtr_inMain1RSRAW = rEffReqPtr->vInputFrameInfo.valueFor(BID_DualYUV_IN_RSRAW1);
    FrameInfoPtr pFramePtr;
    sp<IImageBuffer> frameBuf_RSRAW1, frameBuf_FD;

    framePtr_inMain1RSRAW->getFrameBuffer(frameBuf_RSRAW1);
    MY_LOGD_IF(mLogLevel >= 1,"RSRAW1=%d", frameBuf_RSRAW1->getImgSize());

    // Make sure the ordering inside the mvIn mvOut
    int mvInIndex = 0, mvOutIndex = 0;
    MUINT iFrameNum = 0;
    MPoint zeroPos(0,0);
    MVOID* pVATuningBuffer = NULL;

    FrameInfoPtr framePtr_inAppMeta = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_APP);
    IMetadata* pMeta_InApp = getMetadataFromFrameInfoPtr(framePtr_inAppMeta);
    FrameInfoPtr framePtr_inHalMeta;
    //--> frame 0
    {
        // input StreamTag
        rEnqueParam.mvStreamTag.push_back(ENormalStreamTag_Normal);
        rEnqueParam.mvSensorIdx.push_back(miSensorIdx_Main1);

        iFrameNum = 0;
        FrameInfoPtr framePtr_inHalMeta_Main1 = rEffReqPtr->vInputFrameInfo.valueFor(BID_META_IN_HAL);
        // Apply tuning data
        pVATuningBuffer = (MVOID*) malloc(sizeof(dip_x_reg_t));
        memset(pVATuningBuffer, 0, sizeof(dip_x_reg_t));

        sp<IImageBuffer> frameBuf_MV_F;
        // Apply ISP tuning
        ISPTuningConfig ispConfig = {framePtr_inAppMeta, framePtr_inHalMeta_Main1, mp3AHal_Main1, MTRUE};
        TuningParam rTuningParam = applyISPTuning(pVATuningBuffer, ispConfig);
        ssize_t res = rEnqueParam.mvTuningData.insertAt(pVATuningBuffer, iFrameNum);

        // input: Main1 RSRAW
        {
            sp<IImageBuffer> pImgBuf;
            framePtr_inMain1RSRAW->getFrameBuffer(pImgBuf);

            Input input_IMGI;
            input_IMGI.mPortID = PORT_IMGI;
            input_IMGI.mPortID.group = iFrameNum;
            input_IMGI.mBuffer = pImgBuf.get();
            rEnqueParam.mvIn.push_back(input_IMGI);
        }

        // output FD
        if(rEffReqPtr->vOutputFrameInfo.indexOfKey(BID_OUT_FD) >= 0)
        {
             // output: FD
            RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(rEffReqPtr, pFramePtr, BID_OUT_FD, frameBuf_FD);
            // insert FD output
            Output output_IMG2O;
            output_IMG2O.mPortID = PORT_IMG2O;
            output_IMG2O.mPortID.group = iFrameNum;
            output_IMG2O.mTransform = 0;
            output_IMG2O.mBuffer = frameBuf_FD.get();
            rEnqueParam.mvOut.push_back(output_IMG2O);

            // setCrop
            MCrpRsInfo cropInfo;
            cropInfo.mGroupID = (MUINT32) eCROP_CRZ;
            cropInfo.mCropRect.p_fractional.x=0;
            cropInfo.mCropRect.p_fractional.y=0;
            cropInfo.mCropRect.p_integral.x=zeroPos.x;
            cropInfo.mCropRect.p_integral.y=zeroPos.y;
            cropInfo.mCropRect.s=frameBuf_RSRAW1->getImgSize();
            cropInfo.mResizeDst=frameBuf_FD->getImgSize();
            cropInfo.mFrameGroup = iFrameNum;
            rEnqueParam.mvCropRsInfo.push_back(cropInfo);
        }

        // output: MV_F
        RETRIEVE_OFRAMEINFO_IMGBUF_ERROR_RETURN(rEffReqPtr, pFramePtr, BID_OUT_MV_F, frameBuf_MV_F);
        if(frameBuf_MV_F == nullptr)
        {
            MY_LOGE("frameBuf_MV_F is null request(0x%x)", rEffReqPtr.get());
        }
        // check EIS on/off
        if (isEISOn(pMeta_InApp))
        {
            IMetadata* pMeta_InHal = getMetadataFromFrameInfoPtr(framePtr_inHalMeta_Main1);
            eis_region region;
            // set MV_F crop for EIS
            if(queryEisRegion(pMeta_InHal, region, rEffReqPtr))
            {
                MY_LOGD_IF(mLogLevel >= 1,"queryEisRegion IN");
                // setCrop
                MCrpRsInfo cropInfo;
                cropInfo.mGroupID = (MUINT32) eCROP_WDMA;
                cropInfo.mCropRect.p_fractional.x=0;
                cropInfo.mCropRect.p_fractional.y=0;
                cropInfo.mCropRect.p_integral.x=region.x_int;
                cropInfo.mCropRect.p_integral.y=region.y_int;
                cropInfo.mCropRect.s=region.s;
                cropInfo.mResizeDst=frameBuf_MV_F->getImgSize();
                cropInfo.mFrameGroup = iFrameNum;
                rEnqueParam.mvCropRsInfo.push_back(cropInfo);
            }
            else
            {
                MY_LOGE("Query EIS Region Failed! reqID=%d.", rEffReqPtr->getRequestNo());
                return MFALSE;
            }
        }
        else
        {
            // MV_F crop
            MY_LOGD_IF(mLogLevel >= 1,"No EIS Crop IN");

            // setCrop
            MCrpRsInfo cropInfo;
            cropInfo.mGroupID = (MUINT32) eCROP_WDMA;
            cropInfo.mCropRect.p_fractional.x=0;
            cropInfo.mCropRect.p_fractional.y=0;
            cropInfo.mCropRect.p_integral.x=zeroPos.x;
            cropInfo.mCropRect.p_integral.y=zeroPos.y;
            cropInfo.mCropRect.s=frameBuf_RSRAW1->getImgSize();
            cropInfo.mResizeDst=frameBuf_MV_F->getImgSize();
            cropInfo.mFrameGroup = iFrameNum;
            rEnqueParam.mvCropRsInfo.push_back(cropInfo);
        }

        // insert output
        Output output_WDMA;
        output_WDMA.mPortID = PORT_WDMAO;
        output_WDMA.mPortID.group = iFrameNum;
        output_WDMA.mTransform = 0;
        output_WDMA.mBuffer = frameBuf_MV_F.get();
        rEnqueParam.mvOut.push_back(output_WDMA);
    }


    FUNCTION_LOG_END;
    return MTRUE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
YUVEffectHal::
isEISOn(IMetadata* const inApp)
{
    MUINT8 eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    if( !tryGetMetadata<MUINT8>(inApp, MTK_CONTROL_VIDEO_STABILIZATION_MODE, eisMode) ) {
        MY_LOGD_IF(mLogLevel >= 1,"no MTK_CONTROL_VIDEO_STABILIZATION_MODE");
    }

    return eisMode == MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
YUVEffectHal::
queryEisRegion(
    IMetadata* const inHal,
    eis_region& region,
    EffectRequestPtr request)
{
    IMetadata::IEntry entry = inHal->entryFor(MTK_EIS_REGION);

#if SUPPORT_EIS_MV
    // get EIS's motion vector
    if (entry.count() > 8)
    {
        MINT32 x_mv         = entry.itemAt(EIS_REGION_META_MV_X_LOC, Type2Type<MINT32>());
        MINT32 y_mv         = entry.itemAt(EIS_REGION_META_MV_Y_LOC, Type2Type<MINT32>());
        region.is_from_zzr  = entry.itemAt(EIS_REGION_META_FROM_RRZ_LOC, Type2Type<MINT32>());
        MBOOL x_mv_negative = x_mv >> 31;
        MBOOL y_mv_negative = y_mv >> 31;
        // convert to positive for getting parts of int and float if negative
        if (x_mv_negative) x_mv = ~x_mv + 1;
        if (y_mv_negative) y_mv = ~y_mv + 1;
        //
        region.x_mv_int   = (x_mv & (~0xFF)) >> 8;
        region.x_mv_float = (x_mv & (0xFF)) << 31;
        if(x_mv_negative){
            region.x_mv_int   = ~region.x_mv_int + 1;
            region.x_mv_float = ~region.x_mv_float + 1;
        }
        region.y_mv_int   = (y_mv& (~0xFF)) >> 8;
        region.y_mv_float = (y_mv& (0xFF)) << 31;
        if(y_mv_negative){
            region.y_mv_int   = ~region.y_mv_int + 1;
            region.y_mv_float = ~region.x_mv_float + 1;
        }
        // calculate x_int/y_int from mv
        FrameInfoPtr sourceFrameInfo;
        sp<IImageBuffer> pSrcImgBuf;
        DepthMapBufferID srcBID = BID_DualYUV_IN_RSRAW1; // main1 resize raw for preview
        // get frame buffer
        if(request->vInputFrameInfo.indexOfKey(srcBID) >= 0)
        {
            FrameInfoPtr frameInfo = request->vInputFrameInfo.valueFor(BID_DualYUV_IN_RSRAW1);
            frameInfo->getFrameBuffer(pSrcImgBuf);
        }
        else
        {
            MY_LOGE("Cannot find input frame info, BID=%d", srcBID);
            return MFALSE;
        }

        MSize sourceSize = pSrcImgBuf->getImgSize();
         // eisCenterStart is the left-up position of eis crop region when no eis offset, that is the eis crop region is located in the center.
        MPoint eisCenterStart;
        eisCenterStart.x = sourceSize.w / (EIS_FACTOR/100.0) * (EIS_FACTOR-100)/100 * 0.5;
        eisCenterStart.y = sourceSize.h / (EIS_FACTOR/100.0) * (EIS_FACTOR-100)/100 * 0.5;

        region.x_int = eisCenterStart.x + mv_x;
        region.y_int = eisCenterStart.y + mv_y;
        region.s = sourceSize / (EIS_FACTOR/100.0);

        return MTRUE;
    }
#else
    // get EIS's region
    if (entry.count() > 5)
    {
        region.x_int        = entry.itemAt(0, Type2Type<MINT32>());
        region.x_float      = entry.itemAt(1, Type2Type<MINT32>());
        region.y_int        = entry.itemAt(2, Type2Type<MINT32>());
        region.y_float      = entry.itemAt(3, Type2Type<MINT32>());
        region.s.w          = entry.itemAt(4, Type2Type<MINT32>());
        region.s.h          = entry.itemAt(5, Type2Type<MINT32>());

       return MTRUE;
    }
#endif
    MY_LOGE("wrong eis region count %zu", entry.count());
    return MFALSE;
}
/*******************************************************************************
 *
 ********************************************************************************/
MVOID
YUVEffectHal::
debugQParams(const QParams& rInputQParam)
{
    if(!mbDebugLog)
        return;

    MY_LOGD_IF(mLogLevel >= 2,"debugQParams start");
    MY_LOGD_IF(mLogLevel >= 2,"Size of mvStreamTag=%d, mvTuningData=%d mvIn=%d mvOut=%d, mvCropRsInfo=%d, mvModuleData=%d\n",
                rInputQParam.mvStreamTag.size(), rInputQParam.mvTuningData.size(), rInputQParam.mvIn.size(),
                rInputQParam.mvOut.size(), rInputQParam.mvCropRsInfo.size(), rInputQParam.mvModuleData.size());

    MY_LOGD_IF(mLogLevel >= 2,"mvStreamTag section");
    for(size_t i=0;i<rInputQParam.mvStreamTag.size(); i++)
    {
        MY_LOGD_IF(mLogLevel >= 2,"Index = %d mvStreamTag = %d", i, rInputQParam.mvStreamTag[i]);
    }

    MY_LOGD_IF(mLogLevel >= 2,"mvTuningData section");
    for(size_t i=0;i<rInputQParam.mvTuningData.size(); i++)
    {
        dip_x_reg_t* data = (dip_x_reg_t*) rInputQParam.mvTuningData[i];
        MY_LOGD_IF(mLogLevel >= 2,"========\nIndex = %d", i);

        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FE_CTRL1.Raw = %x", data->DIP_X_FE_CTRL1.Raw);
        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FE_IDX_CTRL.Raw = %x", data->DIP_X_FE_IDX_CTRL.Raw);
        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FE_CROP_CTRL1.Raw = %x", data->DIP_X_FE_CROP_CTRL1.Raw);
        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FE_CROP_CTRL2.Raw = %x", data->DIP_X_FE_CROP_CTRL2.Raw);
        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FE_CTRL2.Raw = %x", data->DIP_X_FE_CTRL2.Raw);
        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FM_SIZE.Raw = %x", data->DIP_X_FM_SIZE.Raw);
        MY_LOGD_IF(mLogLevel >= 2,"DIP_X_FM_TH_CON0.Raw = %x", data->DIP_X_FM_TH_CON0.Raw);
    }

    MY_LOGD_IF(mLogLevel >= 2,"mvIn section");
    for(size_t i=0;i<rInputQParam.mvIn.size(); i++)
    {
        Input data = rInputQParam.mvIn[i];
        MY_LOGD_IF(mLogLevel >= 2,"========\nIndex = %d", i);

        MY_LOGD_IF(mLogLevel >= 2,"mvIn.PortID.index = %d", data.mPortID.index);
        MY_LOGD_IF(mLogLevel >= 2,"mvIn.PortID.type = %d", data.mPortID.type);
        MY_LOGD_IF(mLogLevel >= 2,"mvIn.PortID.inout = %d", data.mPortID.inout);
        MY_LOGD_IF(mLogLevel >= 2,"mvIn.PortID.group = %d", data.mPortID.group);

        MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer=%x", data.mBuffer);
        if(data.mBuffer !=NULL)
        {
            MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer->getImgSize = %dx%d", data.mBuffer->getImgSize().w,
                                                data.mBuffer->getImgSize().h);

            MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer->getImgFormat = %x", data.mBuffer->getImgFormat());
            MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer->getPlaneCount = %d", data.mBuffer->getPlaneCount());
            for(int j=0;j<data.mBuffer->getPlaneCount();j++)
            {
                MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer->getBufVA(%d) = %X", j, data.mBuffer->getBufVA(j));
                MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer->getBufStridesInBytes(%d) = %d", j, data.mBuffer->getBufStridesInBytes(j));
            }
        }
        else
        {
            MY_LOGD_IF(mLogLevel >= 2,"mvIn.mBuffer is NULL!!");
        }


        MY_LOGD_IF(mLogLevel >= 2,"mvIn.mTransform = %d", data.mTransform);

    }

    MY_LOGD_IF(mLogLevel >= 2,"mvOut section");
    for(size_t i=0;i<rInputQParam.mvOut.size(); i++)
    {
        Output data = rInputQParam.mvOut[i];
        MY_LOGD_IF(mLogLevel >= 2,"========\nIndex = %d", i);

        MY_LOGD_IF(mLogLevel >= 2,"mvOut.PortID.index = %d", data.mPortID.index);
        MY_LOGD_IF(mLogLevel >= 2,"mvOut.PortID.type = %d", data.mPortID.type);
        MY_LOGD_IF(mLogLevel >= 2,"mvOut.PortID.inout = %d", data.mPortID.inout);
        MY_LOGD_IF(mLogLevel >= 2,"mvOut.PortID.group = %d", data.mPortID.group);

        MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer=%x", data.mBuffer);
        if(data.mBuffer != NULL)
        {
            MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer->getImgSize = %dx%d", data.mBuffer->getImgSize().w,
                                                data.mBuffer->getImgSize().h);

            MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer->getImgFormat = %x", data.mBuffer->getImgFormat());
            MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer->getPlaneCount = %d", data.mBuffer->getPlaneCount());
            for(size_t j=0;j<data.mBuffer->getPlaneCount();j++)
            {
                MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer->getBufVA(%d) = %X", j, data.mBuffer->getBufVA(j));
                MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer->getBufStridesInBytes(%d) = %d", j, data.mBuffer->getBufStridesInBytes(j));
            }
        }
        else
        {
            MY_LOGD_IF(mLogLevel >= 2,"mvOut.mBuffer is NULL!!");
        }
        MY_LOGD_IF(mLogLevel >= 2,"mvOut.mTransform = %d", data.mTransform);
    }

    MY_LOGD_IF(mLogLevel >= 2,"mvCropRsInfo section");
    for(size_t i=0;i<rInputQParam.mvCropRsInfo.size(); i++)
    {
        MCrpRsInfo data = rInputQParam.mvCropRsInfo[i];
        MY_LOGD_IF(mLogLevel >= 2,"========\nIndex = %d", i);

        MY_LOGD_IF(mLogLevel >= 2,"CropRsInfo.mGroupID=%d", data.mGroupID);
        MY_LOGD_IF(mLogLevel >= 2,"CropRsInfo.mFrameGroup=%d", data.mFrameGroup);
        MY_LOGD_IF(mLogLevel >= 2,"CropRsInfo.mResizeDst=%dx%d", data.mResizeDst.w, data.mResizeDst.h);
        MY_LOGD_IF(mLogLevel >= 2,"CropRsInfo.mCropRect.p_fractional=(%d,%d) ", data.mCropRect.p_fractional.x, data.mCropRect.p_fractional.y);
        MY_LOGD_IF(mLogLevel >= 2,"CropRsInfo.mCropRect.p_integral=(%d,%d) ", data.mCropRect.p_integral.x, data.mCropRect.p_integral.y);
        MY_LOGD_IF(mLogLevel >= 2,"CropRsInfo.mCropRect.s=%dx%d ", data.mCropRect.s.w, data.mCropRect.s.h);
    }

    MY_LOGD_IF(mLogLevel >= 2,"mvModuleData section");
    for(size_t i=0;i<rInputQParam.mvModuleData.size(); i++)
    {
        ModuleInfo data = rInputQParam.mvModuleData[i];
        MY_LOGD_IF(mLogLevel >= 2,"========\nIndex = %d", i);

        MY_LOGD_IF(mLogLevel >= 2,"ModuleData.moduleTag=%d", data.moduleTag);
        MY_LOGD_IF(mLogLevel >= 2,"ModuleData.frameGroup=%d", data.frameGroup);

        _SRZ_SIZE_INFO_ *SrzInfo = (_SRZ_SIZE_INFO_ *) data.moduleStruct;
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->in_w=%d", SrzInfo->in_w);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->in_h=%d", SrzInfo->in_h);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->crop_w=%d", SrzInfo->crop_w);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->crop_h=%d", SrzInfo->crop_h);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->crop_x=%d", SrzInfo->crop_x);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->crop_y=%d", SrzInfo->crop_y);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->crop_floatX=%d", SrzInfo->crop_floatX);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->crop_floatY=%d", SrzInfo->crop_floatY);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->out_w=%d", SrzInfo->out_w);
        MY_LOGD_IF(mLogLevel >= 2,"SrzInfo->out_h=%d", SrzInfo->out_h);
    }
}
/*******************************************************************************
 *
 ********************************************************************************/
MBOOL
YUVEffectHal::
onFlush()
{
    FUNCTION_LOG_START;
    FUNCTION_LOG_END;
    return true;
}
/*******************************************************************************
 *
 ********************************************************************************/
 void
 YUVEffectHal::
 SetDefaultTuning(dip_x_reg_t* pIspReg, MUINT32* tuningBuf, tuning_tag tag, int enFgMode)
{
    printf("SetDefaultTuning (%d), enFgMode(%d)n", tag, enFgMode);
    MUINT32 fgModeRegBit = (enFgMode&0x01)<<10;
    switch(tag)
    {
        case tuning_tag_G2G:
            pIspReg->DIP_X_G2G_CNV_1.Raw = 0x00000200;
            pIspReg->DIP_X_G2G_CNV_2.Raw = 0x00000000;
            pIspReg->DIP_X_G2G_CNV_3.Raw = 0x02000000;
            pIspReg->DIP_X_G2G_CNV_4.Raw = 0x00000000;
            pIspReg->DIP_X_G2G_CNV_5.Raw = 0x00000000;
            pIspReg->DIP_X_G2G_CNV_6.Raw = 0x00000200;
            pIspReg->DIP_X_G2G_CTRL.Raw = 0x00000009;
            pIspReg->DIP_X_CTL_RGB_EN.Bits.G2G_EN = 1;
            break;
        case tuning_tag_G2C:
            pIspReg->DIP_X_G2C_CONV_0A.Raw = 0x012D0099;
            pIspReg->DIP_X_G2C_CONV_0B.Raw = 0x0000003A;
            pIspReg->DIP_X_G2C_CONV_1A.Raw = 0x075607AA;
            pIspReg->DIP_X_G2C_CONV_1B.Raw = 0x00000100;
            pIspReg->DIP_X_G2C_CONV_2A.Raw = 0x072A0100;
            pIspReg->DIP_X_G2C_CONV_2B.Raw = 0x000007D6;
            pIspReg->DIP_X_G2C_SHADE_CON_1.Raw = 0x0118000E;
            pIspReg->DIP_X_G2C_SHADE_CON_2.Raw = 0x0074B740;
            pIspReg->DIP_X_G2C_SHADE_CON_3.Raw = 0x00000133;
            pIspReg->DIP_X_G2C_SHADE_TAR.Raw = 0x079F0A5A;
            pIspReg->DIP_X_G2C_SHADE_SP.Raw = 0x00000000;
            pIspReg->DIP_X_G2C_CFC_CON_1.Raw = 0x03f70080;
            pIspReg->DIP_X_G2C_CFC_CON_2.Raw = 0x29485294;
            pIspReg->DIP_X_CTL_YUV_EN.Bits.G2C_EN = 1;
            break;
        case tuning_tag_GGM:
            //tuningBuf[0x1000>>2] = 0x08000800;
            tuningBuf[0x00001000>>2] = 0x08000800; /* 0x15023000: DIP_A_GGM_LUT_RB[0] */
            tuningBuf[0x00001004>>2] = 0x08020802; /* 0x15023004: DIP_A_GGM_LUT_RB[1] */
            tuningBuf[0x00001008>>2] = 0x08040804; /* 0x15023008: DIP_A_GGM_LUT_RB[2] */
            tuningBuf[0x0000100C>>2] = 0x08060806; /* 0x1502300C: DIP_A_GGM_LUT_RB[3] */
            tuningBuf[0x00001010>>2] = 0x08080808; /* 0x15023010: DIP_A_GGM_LUT_RB[4] */
            tuningBuf[0x00001014>>2] = 0x080A080A; /* 0x15023014: DIP_A_GGM_LUT_RB[5] */
            tuningBuf[0x00001018>>2] = 0x080C080C; /* 0x15023018: DIP_A_GGM_LUT_RB[6] */
            tuningBuf[0x0000101C>>2] = 0x080E080E; /* 0x1502301C: DIP_A_GGM_LUT_RB[7] */
            tuningBuf[0x00001020>>2] = 0x08100810; /* 0x15023020: DIP_A_GGM_LUT_RB[8] */
            tuningBuf[0x00001024>>2] = 0x08120812; /* 0x15023024: DIP_A_GGM_LUT_RB[9] */
            tuningBuf[0x00001028>>2] = 0x08140814; /* 0x15023028: DIP_A_GGM_LUT_RB[10] */
            tuningBuf[0x0000102C>>2] = 0x08160816; /* 0x1502302C: DIP_A_GGM_LUT_RB[11] */
            tuningBuf[0x00001030>>2] = 0x08180818; /* 0x15023030: DIP_A_GGM_LUT_RB[12] */
            tuningBuf[0x00001034>>2] = 0x081A081A; /* 0x15023034: DIP_A_GGM_LUT_RB[13] */
            tuningBuf[0x00001038>>2] = 0x081C081C; /* 0x15023038: DIP_A_GGM_LUT_RB[14] */
            tuningBuf[0x0000103C>>2] = 0x081E081E; /* 0x1502303C: DIP_A_GGM_LUT_RB[15] */
            tuningBuf[0x00001040>>2] = 0x08200820; /* 0x15023040: DIP_A_GGM_LUT_RB[16] */
            tuningBuf[0x00001044>>2] = 0x08220822; /* 0x15023044: DIP_A_GGM_LUT_RB[17] */
            tuningBuf[0x00001048>>2] = 0x08240824; /* 0x15023048: DIP_A_GGM_LUT_RB[18] */
            tuningBuf[0x0000104C>>2] = 0x08260826; /* 0x1502304C: DIP_A_GGM_LUT_RB[19] */
            tuningBuf[0x00001050>>2] = 0x08280828; /* 0x15023050: DIP_A_GGM_LUT_RB[20] */
            tuningBuf[0x00001054>>2] = 0x082A082A; /* 0x15023054: DIP_A_GGM_LUT_RB[21] */
            tuningBuf[0x00001058>>2] = 0x082C082C; /* 0x15023058: DIP_A_GGM_LUT_RB[22] */
            tuningBuf[0x0000105C>>2] = 0x082E082E; /* 0x1502305C: DIP_A_GGM_LUT_RB[23] */
            tuningBuf[0x00001060>>2] = 0x08300830; /* 0x15023060: DIP_A_GGM_LUT_RB[24] */
            tuningBuf[0x00001064>>2] = 0x08320832; /* 0x15023064: DIP_A_GGM_LUT_RB[25] */
            tuningBuf[0x00001068>>2] = 0x08340834; /* 0x15023068: DIP_A_GGM_LUT_RB[26] */
            tuningBuf[0x0000106C>>2] = 0x08360836; /* 0x1502306C: DIP_A_GGM_LUT_RB[27] */
            tuningBuf[0x00001070>>2] = 0x08380838; /* 0x15023070: DIP_A_GGM_LUT_RB[28] */
            tuningBuf[0x00001074>>2] = 0x083A083A; /* 0x15023074: DIP_A_GGM_LUT_RB[29] */
            tuningBuf[0x00001078>>2] = 0x083C083C; /* 0x15023078: DIP_A_GGM_LUT_RB[30] */
            tuningBuf[0x0000107C>>2] = 0x083E083E; /* 0x1502307C: DIP_A_GGM_LUT_RB[31] */
            tuningBuf[0x00001080>>2] = 0x08400840; /* 0x15023080: DIP_A_GGM_LUT_RB[32] */
            tuningBuf[0x00001084>>2] = 0x08420842; /* 0x15023084: DIP_A_GGM_LUT_RB[33] */
            tuningBuf[0x00001088>>2] = 0x08440844; /* 0x15023088: DIP_A_GGM_LUT_RB[34] */
            tuningBuf[0x0000108C>>2] = 0x08460846; /* 0x1502308C: DIP_A_GGM_LUT_RB[35] */
            tuningBuf[0x00001090>>2] = 0x08480848; /* 0x15023090: DIP_A_GGM_LUT_RB[36] */
            tuningBuf[0x00001094>>2] = 0x084A084A; /* 0x15023094: DIP_A_GGM_LUT_RB[37] */
            tuningBuf[0x00001098>>2] = 0x084C084C; /* 0x15023098: DIP_A_GGM_LUT_RB[38] */
            tuningBuf[0x0000109C>>2] = 0x084E084E; /* 0x1502309C: DIP_A_GGM_LUT_RB[39] */
            tuningBuf[0x000010A0>>2] = 0x08500850; /* 0x150230A0: DIP_A_GGM_LUT_RB[40] */
            tuningBuf[0x000010A4>>2] = 0x08520852; /* 0x150230A4: DIP_A_GGM_LUT_RB[41] */
            tuningBuf[0x000010A8>>2] = 0x08540854; /* 0x150230A8: DIP_A_GGM_LUT_RB[42] */
            tuningBuf[0x000010AC>>2] = 0x08560856; /* 0x150230AC: DIP_A_GGM_LUT_RB[43] */
            tuningBuf[0x000010B0>>2] = 0x08580858; /* 0x150230B0: DIP_A_GGM_LUT_RB[44] */
            tuningBuf[0x000010B4>>2] = 0x085A085A; /* 0x150230B4: DIP_A_GGM_LUT_RB[45] */
            tuningBuf[0x000010B8>>2] = 0x085C085C; /* 0x150230B8: DIP_A_GGM_LUT_RB[46] */
            tuningBuf[0x000010BC>>2] = 0x085E085E; /* 0x150230BC: DIP_A_GGM_LUT_RB[47] */
            tuningBuf[0x000010C0>>2] = 0x08600860; /* 0x150230C0: DIP_A_GGM_LUT_RB[48] */
            tuningBuf[0x000010C4>>2] = 0x08620862; /* 0x150230C4: DIP_A_GGM_LUT_RB[49] */
            tuningBuf[0x000010C8>>2] = 0x08640864; /* 0x150230C8: DIP_A_GGM_LUT_RB[50] */
            tuningBuf[0x000010CC>>2] = 0x08660866; /* 0x150230CC: DIP_A_GGM_LUT_RB[51] */
            tuningBuf[0x000010D0>>2] = 0x08680868; /* 0x150230D0: DIP_A_GGM_LUT_RB[52] */
            tuningBuf[0x000010D4>>2] = 0x086A086A; /* 0x150230D4: DIP_A_GGM_LUT_RB[53] */
            tuningBuf[0x000010D8>>2] = 0x086C086C; /* 0x150230D8: DIP_A_GGM_LUT_RB[54] */
            tuningBuf[0x000010DC>>2] = 0x086E086E; /* 0x150230DC: DIP_A_GGM_LUT_RB[55] */
            tuningBuf[0x000010E0>>2] = 0x08700870; /* 0x150230E0: DIP_A_GGM_LUT_RB[56] */
            tuningBuf[0x000010E4>>2] = 0x08720872; /* 0x150230E4: DIP_A_GGM_LUT_RB[57] */
            tuningBuf[0x000010E8>>2] = 0x08740874; /* 0x150230E8: DIP_A_GGM_LUT_RB[58] */
            tuningBuf[0x000010EC>>2] = 0x08760876; /* 0x150230EC: DIP_A_GGM_LUT_RB[59] */
            tuningBuf[0x000010F0>>2] = 0x08780878; /* 0x150230F0: DIP_A_GGM_LUT_RB[60] */
            tuningBuf[0x000010F4>>2] = 0x087A087A; /* 0x150230F4: DIP_A_GGM_LUT_RB[61] */
            tuningBuf[0x000010F8>>2] = 0x087C087C; /* 0x150230F8: DIP_A_GGM_LUT_RB[62] */
            tuningBuf[0x000010FC>>2] = 0x087E087E; /* 0x150230FC: DIP_A_GGM_LUT_RB[63] */
            tuningBuf[0x00001100>>2] = 0x10801080; /* 0x15023100: DIP_A_GGM_LUT_RB[64] */
            tuningBuf[0x00001104>>2] = 0x10841084; /* 0x15023104: DIP_A_GGM_LUT_RB[65] */
            tuningBuf[0x00001108>>2] = 0x10881088; /* 0x15023108: DIP_A_GGM_LUT_RB[66] */
            tuningBuf[0x0000110C>>2] = 0x108C108C; /* 0x1502310C: DIP_A_GGM_LUT_RB[67] */
            tuningBuf[0x00001110>>2] = 0x10901090; /* 0x15023110: DIP_A_GGM_LUT_RB[68] */
            tuningBuf[0x00001114>>2] = 0x10941094; /* 0x15023114: DIP_A_GGM_LUT_RB[69] */
            tuningBuf[0x00001118>>2] = 0x10981098; /* 0x15023118: DIP_A_GGM_LUT_RB[70] */
            tuningBuf[0x0000111C>>2] = 0x109C109C; /* 0x1502311C: DIP_A_GGM_LUT_RB[71] */
            tuningBuf[0x00001120>>2] = 0x10A010A0; /* 0x15023120: DIP_A_GGM_LUT_RB[72] */
            tuningBuf[0x00001124>>2] = 0x10A410A4; /* 0x15023124: DIP_A_GGM_LUT_RB[73] */
            tuningBuf[0x00001128>>2] = 0x10A810A8; /* 0x15023128: DIP_A_GGM_LUT_RB[74] */
            tuningBuf[0x0000112C>>2] = 0x10AC10AC; /* 0x1502312C: DIP_A_GGM_LUT_RB[75] */
            tuningBuf[0x00001130>>2] = 0x10B010B0; /* 0x15023130: DIP_A_GGM_LUT_RB[76] */
            tuningBuf[0x00001134>>2] = 0x10B410B4; /* 0x15023134: DIP_A_GGM_LUT_RB[77] */
            tuningBuf[0x00001138>>2] = 0x10B810B8; /* 0x15023138: DIP_A_GGM_LUT_RB[78] */
            tuningBuf[0x0000113C>>2] = 0x10BC10BC; /* 0x1502313C: DIP_A_GGM_LUT_RB[79] */
            tuningBuf[0x00001140>>2] = 0x10C010C0; /* 0x15023140: DIP_A_GGM_LUT_RB[80] */
            tuningBuf[0x00001144>>2] = 0x10C410C4; /* 0x15023144: DIP_A_GGM_LUT_RB[81] */
            tuningBuf[0x00001148>>2] = 0x10C810C8; /* 0x15023148: DIP_A_GGM_LUT_RB[82] */
            tuningBuf[0x0000114C>>2] = 0x10CC10CC; /* 0x1502314C: DIP_A_GGM_LUT_RB[83] */
            tuningBuf[0x00001150>>2] = 0x10D010D0; /* 0x15023150: DIP_A_GGM_LUT_RB[84] */
            tuningBuf[0x00001154>>2] = 0x10D410D4; /* 0x15023154: DIP_A_GGM_LUT_RB[85] */
            tuningBuf[0x00001158>>2] = 0x10D810D8; /* 0x15023158: DIP_A_GGM_LUT_RB[86] */
            tuningBuf[0x0000115C>>2] = 0x10DC10DC; /* 0x1502315C: DIP_A_GGM_LUT_RB[87] */
            tuningBuf[0x00001160>>2] = 0x10E010E0; /* 0x15023160: DIP_A_GGM_LUT_RB[88] */
            tuningBuf[0x00001164>>2] = 0x10E410E4; /* 0x15023164: DIP_A_GGM_LUT_RB[89] */
            tuningBuf[0x00001168>>2] = 0x10E810E8; /* 0x15023168: DIP_A_GGM_LUT_RB[90] */
            tuningBuf[0x0000116C>>2] = 0x10EC10EC; /* 0x1502316C: DIP_A_GGM_LUT_RB[91] */
            tuningBuf[0x00001170>>2] = 0x10F010F0; /* 0x15023170: DIP_A_GGM_LUT_RB[92] */
            tuningBuf[0x00001174>>2] = 0x10F410F4; /* 0x15023174: DIP_A_GGM_LUT_RB[93] */
            tuningBuf[0x00001178>>2] = 0x10F810F8; /* 0x15023178: DIP_A_GGM_LUT_RB[94] */
            tuningBuf[0x0000117C>>2] = 0x10FC10FC; /* 0x1502317C: DIP_A_GGM_LUT_RB[95] */
            tuningBuf[0x00001180>>2] = 0x21002100; /* 0x15023180: DIP_A_GGM_LUT_RB[96] */
            tuningBuf[0x00001184>>2] = 0x21082108; /* 0x15023184: DIP_A_GGM_LUT_RB[97] */
            tuningBuf[0x00001188>>2] = 0x21102110; /* 0x15023188: DIP_A_GGM_LUT_RB[98] */
            tuningBuf[0x0000118C>>2] = 0x21182118; /* 0x1502318C: DIP_A_GGM_LUT_RB[99] */
            tuningBuf[0x00001190>>2] = 0x21202120; /* 0x15023190: DIP_A_GGM_LUT_RB[100] */
            tuningBuf[0x00001194>>2] = 0x21282128; /* 0x15023194: DIP_A_GGM_LUT_RB[101] */
            tuningBuf[0x00001198>>2] = 0x21302130; /* 0x15023198: DIP_A_GGM_LUT_RB[102] */
            tuningBuf[0x0000119C>>2] = 0x21382138; /* 0x1502319C: DIP_A_GGM_LUT_RB[103] */
            tuningBuf[0x000011A0>>2] = 0x21402140; /* 0x150231A0: DIP_A_GGM_LUT_RB[104] */
            tuningBuf[0x000011A4>>2] = 0x21482148; /* 0x150231A4: DIP_A_GGM_LUT_RB[105] */
            tuningBuf[0x000011A8>>2] = 0x21502150; /* 0x150231A8: DIP_A_GGM_LUT_RB[106] */
            tuningBuf[0x000011AC>>2] = 0x21582158; /* 0x150231AC: DIP_A_GGM_LUT_RB[107] */
            tuningBuf[0x000011B0>>2] = 0x21602160; /* 0x150231B0: DIP_A_GGM_LUT_RB[108] */
            tuningBuf[0x000011B4>>2] = 0x21682168; /* 0x150231B4: DIP_A_GGM_LUT_RB[109] */
            tuningBuf[0x000011B8>>2] = 0x21702170; /* 0x150231B8: DIP_A_GGM_LUT_RB[110] */
            tuningBuf[0x000011BC>>2] = 0x21782178; /* 0x150231BC: DIP_A_GGM_LUT_RB[111] */
            tuningBuf[0x000011C0>>2] = 0x21802180; /* 0x150231C0: DIP_A_GGM_LUT_RB[112] */
            tuningBuf[0x000011C4>>2] = 0x21882188; /* 0x150231C4: DIP_A_GGM_LUT_RB[113] */
            tuningBuf[0x000011C8>>2] = 0x21902190; /* 0x150231C8: DIP_A_GGM_LUT_RB[114] */
            tuningBuf[0x000011CC>>2] = 0x21982198; /* 0x150231CC: DIP_A_GGM_LUT_RB[115] */
            tuningBuf[0x000011D0>>2] = 0x21A021A0; /* 0x150231D0: DIP_A_GGM_LUT_RB[116] */
            tuningBuf[0x000011D4>>2] = 0x21A821A8; /* 0x150231D4: DIP_A_GGM_LUT_RB[117] */
            tuningBuf[0x000011D8>>2] = 0x21B021B0; /* 0x150231D8: DIP_A_GGM_LUT_RB[118] */
            tuningBuf[0x000011DC>>2] = 0x21B821B8; /* 0x150231DC: DIP_A_GGM_LUT_RB[119] */
            tuningBuf[0x000011E0>>2] = 0x21C021C0; /* 0x150231E0: DIP_A_GGM_LUT_RB[120] */
            tuningBuf[0x000011E4>>2] = 0x21C821C8; /* 0x150231E4: DIP_A_GGM_LUT_RB[121] */
            tuningBuf[0x000011E8>>2] = 0x21D021D0; /* 0x150231E8: DIP_A_GGM_LUT_RB[122] */
            tuningBuf[0x000011EC>>2] = 0x21D821D8; /* 0x150231EC: DIP_A_GGM_LUT_RB[123] */
            tuningBuf[0x000011F0>>2] = 0x21E021E0; /* 0x150231F0: DIP_A_GGM_LUT_RB[124] */
            tuningBuf[0x000011F4>>2] = 0x21E821E8; /* 0x150231F4: DIP_A_GGM_LUT_RB[125] */
            tuningBuf[0x000011F8>>2] = 0x21F021F0; /* 0x150231F8: DIP_A_GGM_LUT_RB[126] */
            tuningBuf[0x000011FC>>2] = 0x21F821F8; /* 0x150231FC: DIP_A_GGM_LUT_RB[127] */
            tuningBuf[0x00001200>>2] = 0x82008200; /* 0x15023200: DIP_A_GGM_LUT_RB[128] */
            tuningBuf[0x00001204>>2] = 0x82208220; /* 0x15023204: DIP_A_GGM_LUT_RB[129] */
            tuningBuf[0x00001208>>2] = 0x82408240; /* 0x15023208: DIP_A_GGM_LUT_RB[130] */
            tuningBuf[0x0000120C>>2] = 0x82608260; /* 0x1502320C: DIP_A_GGM_LUT_RB[131] */
            tuningBuf[0x00001210>>2] = 0x82808280; /* 0x15023210: DIP_A_GGM_LUT_RB[132] */
            tuningBuf[0x00001214>>2] = 0x82A082A0; /* 0x15023214: DIP_A_GGM_LUT_RB[133] */
            tuningBuf[0x00001218>>2] = 0x82C082C0; /* 0x15023218: DIP_A_GGM_LUT_RB[134] */
            tuningBuf[0x0000121C>>2] = 0x82E082E0; /* 0x1502321C: DIP_A_GGM_LUT_RB[135] */
            tuningBuf[0x00001220>>2] = 0x83008300; /* 0x15023220: DIP_A_GGM_LUT_RB[136] */
            tuningBuf[0x00001224>>2] = 0x83208320; /* 0x15023224: DIP_A_GGM_LUT_RB[137] */
            tuningBuf[0x00001228>>2] = 0x83408340; /* 0x15023228: DIP_A_GGM_LUT_RB[138] */
            tuningBuf[0x0000122C>>2] = 0x83608360; /* 0x1502322C: DIP_A_GGM_LUT_RB[139] */
            tuningBuf[0x00001230>>2] = 0x83808380; /* 0x15023230: DIP_A_GGM_LUT_RB[140] */
            tuningBuf[0x00001234>>2] = 0x83A083A0; /* 0x15023234: DIP_A_GGM_LUT_RB[141] */
            tuningBuf[0x00001238>>2] = 0x83C083C0; /* 0x15023238: DIP_A_GGM_LUT_RB[142] */
            tuningBuf[0x0000123C>>2] = 0x7FE07FE0; /* 0x1502323C: DIP_A_GGM_LUT_RB[143] */
            tuningBuf[0x00001240>>2] = 0x00000800; /* 0x15023240: DIP_A_GGM_LUT_G[0] */
            tuningBuf[0x00001244>>2] = 0x00000802; /* 0x15023244: DIP_A_GGM_LUT_G[1] */
            tuningBuf[0x00001248>>2] = 0x00000804; /* 0x15023248: DIP_A_GGM_LUT_G[2] */
            tuningBuf[0x0000124C>>2] = 0x00000806; /* 0x1502324C: DIP_A_GGM_LUT_G[3] */
            tuningBuf[0x00001250>>2] = 0x00000808; /* 0x15023250: DIP_A_GGM_LUT_G[4] */
            tuningBuf[0x00001254>>2] = 0x0000080A; /* 0x15023254: DIP_A_GGM_LUT_G[5] */
            tuningBuf[0x00001258>>2] = 0x0000080C; /* 0x15023258: DIP_A_GGM_LUT_G[6] */
            tuningBuf[0x0000125C>>2] = 0x0000080E; /* 0x1502325C: DIP_A_GGM_LUT_G[7] */
            tuningBuf[0x00001260>>2] = 0x00000810; /* 0x15023260: DIP_A_GGM_LUT_G[8] */
            tuningBuf[0x00001264>>2] = 0x00000812; /* 0x15023264: DIP_A_GGM_LUT_G[9] */
            tuningBuf[0x00001268>>2] = 0x00000814; /* 0x15023268: DIP_A_GGM_LUT_G[10] */
            tuningBuf[0x0000126C>>2] = 0x00000816; /* 0x1502326C: DIP_A_GGM_LUT_G[11] */
            tuningBuf[0x00001270>>2] = 0x00000818; /* 0x15023270: DIP_A_GGM_LUT_G[12] */
            tuningBuf[0x00001274>>2] = 0x0000081A; /* 0x15023274: DIP_A_GGM_LUT_G[13] */
            tuningBuf[0x00001278>>2] = 0x0000081C; /* 0x15023278: DIP_A_GGM_LUT_G[14] */
            tuningBuf[0x0000127C>>2] = 0x0000081E; /* 0x1502327C: DIP_A_GGM_LUT_G[15] */
            tuningBuf[0x00001280>>2] = 0x00000820; /* 0x15023280: DIP_A_GGM_LUT_G[16] */
            tuningBuf[0x00001284>>2] = 0x00000822; /* 0x15023284: DIP_A_GGM_LUT_G[17] */
            tuningBuf[0x00001288>>2] = 0x00000824; /* 0x15023288: DIP_A_GGM_LUT_G[18] */
            tuningBuf[0x0000128C>>2] = 0x00000826; /* 0x1502328C: DIP_A_GGM_LUT_G[19] */
            tuningBuf[0x00001290>>2] = 0x00000828; /* 0x15023290: DIP_A_GGM_LUT_G[20] */
            tuningBuf[0x00001294>>2] = 0x0000082A; /* 0x15023294: DIP_A_GGM_LUT_G[21] */
            tuningBuf[0x00001298>>2] = 0x0000082C; /* 0x15023298: DIP_A_GGM_LUT_G[22] */
            tuningBuf[0x0000129C>>2] = 0x0000082E; /* 0x1502329C: DIP_A_GGM_LUT_G[23] */
            tuningBuf[0x000012A0>>2] = 0x00000830; /* 0x150232A0: DIP_A_GGM_LUT_G[24] */
            tuningBuf[0x000012A4>>2] = 0x00000832; /* 0x150232A4: DIP_A_GGM_LUT_G[25] */
            tuningBuf[0x000012A8>>2] = 0x00000834; /* 0x150232A8: DIP_A_GGM_LUT_G[26] */
            tuningBuf[0x000012AC>>2] = 0x00000836; /* 0x150232AC: DIP_A_GGM_LUT_G[27] */
            tuningBuf[0x000012B0>>2] = 0x00000838; /* 0x150232B0: DIP_A_GGM_LUT_G[28] */
            tuningBuf[0x000012B4>>2] = 0x0000083A; /* 0x150232B4: DIP_A_GGM_LUT_G[29] */
            tuningBuf[0x000012B8>>2] = 0x0000083C; /* 0x150232B8: DIP_A_GGM_LUT_G[30] */
            tuningBuf[0x000012BC>>2] = 0x0000083E; /* 0x150232BC: DIP_A_GGM_LUT_G[31] */
            tuningBuf[0x000012C0>>2] = 0x00000840; /* 0x150232C0: DIP_A_GGM_LUT_G[32] */
            tuningBuf[0x000012C4>>2] = 0x00000842; /* 0x150232C4: DIP_A_GGM_LUT_G[33] */
            tuningBuf[0x000012C8>>2] = 0x00000844; /* 0x150232C8: DIP_A_GGM_LUT_G[34] */
            tuningBuf[0x000012CC>>2] = 0x00000846; /* 0x150232CC: DIP_A_GGM_LUT_G[35] */
            tuningBuf[0x000012D0>>2] = 0x00000848; /* 0x150232D0: DIP_A_GGM_LUT_G[36] */
            tuningBuf[0x000012D4>>2] = 0x0000084A; /* 0x150232D4: DIP_A_GGM_LUT_G[37] */
            tuningBuf[0x000012D8>>2] = 0x0000084C; /* 0x150232D8: DIP_A_GGM_LUT_G[38] */
            tuningBuf[0x000012DC>>2] = 0x0000084E; /* 0x150232DC: DIP_A_GGM_LUT_G[39] */
            tuningBuf[0x000012E0>>2] = 0x00000850; /* 0x150232E0: DIP_A_GGM_LUT_G[40] */
            tuningBuf[0x000012E4>>2] = 0x00000852; /* 0x150232E4: DIP_A_GGM_LUT_G[41] */
            tuningBuf[0x000012E8>>2] = 0x00000854; /* 0x150232E8: DIP_A_GGM_LUT_G[42] */
            tuningBuf[0x000012EC>>2] = 0x00000856; /* 0x150232EC: DIP_A_GGM_LUT_G[43] */
            tuningBuf[0x000012F0>>2] = 0x00000858; /* 0x150232F0: DIP_A_GGM_LUT_G[44] */
            tuningBuf[0x000012F4>>2] = 0x0000085A; /* 0x150232F4: DIP_A_GGM_LUT_G[45] */
            tuningBuf[0x000012F8>>2] = 0x0000085C; /* 0x150232F8: DIP_A_GGM_LUT_G[46] */
            tuningBuf[0x000012FC>>2] = 0x0000085E; /* 0x150232FC: DIP_A_GGM_LUT_G[47] */
            tuningBuf[0x00001300>>2] = 0x00000860; /* 0x15023300: DIP_A_GGM_LUT_G[48] */
            tuningBuf[0x00001304>>2] = 0x00000862; /* 0x15023304: DIP_A_GGM_LUT_G[49] */
            tuningBuf[0x00001308>>2] = 0x00000864; /* 0x15023308: DIP_A_GGM_LUT_G[50] */
            tuningBuf[0x0000130C>>2] = 0x00000866; /* 0x1502330C: DIP_A_GGM_LUT_G[51] */
            tuningBuf[0x00001310>>2] = 0x00000868; /* 0x15023310: DIP_A_GGM_LUT_G[52] */
            tuningBuf[0x00001314>>2] = 0x0000086A; /* 0x15023314: DIP_A_GGM_LUT_G[53] */
            tuningBuf[0x00001318>>2] = 0x0000086C; /* 0x15023318: DIP_A_GGM_LUT_G[54] */
            tuningBuf[0x0000131C>>2] = 0x0000086E; /* 0x1502331C: DIP_A_GGM_LUT_G[55] */
            tuningBuf[0x00001320>>2] = 0x00000870; /* 0x15023320: DIP_A_GGM_LUT_G[56] */
            tuningBuf[0x00001324>>2] = 0x00000872; /* 0x15023324: DIP_A_GGM_LUT_G[57] */
            tuningBuf[0x00001328>>2] = 0x00000874; /* 0x15023328: DIP_A_GGM_LUT_G[58] */
            tuningBuf[0x0000132C>>2] = 0x00000876; /* 0x1502332C: DIP_A_GGM_LUT_G[59] */
            tuningBuf[0x00001330>>2] = 0x00000878; /* 0x15023330: DIP_A_GGM_LUT_G[60] */
            tuningBuf[0x00001334>>2] = 0x0000087A; /* 0x15023334: DIP_A_GGM_LUT_G[61] */
            tuningBuf[0x00001338>>2] = 0x0000087C; /* 0x15023338: DIP_A_GGM_LUT_G[62] */
            tuningBuf[0x0000133C>>2] = 0x0000087E; /* 0x1502333C: DIP_A_GGM_LUT_G[63] */
            tuningBuf[0x00001340>>2] = 0x00001080; /* 0x15023340: DIP_A_GGM_LUT_G[64] */
            tuningBuf[0x00001344>>2] = 0x00001084; /* 0x15023344: DIP_A_GGM_LUT_G[65] */
            tuningBuf[0x00001348>>2] = 0x00001088; /* 0x15023348: DIP_A_GGM_LUT_G[66] */
            tuningBuf[0x0000134C>>2] = 0x0000108C; /* 0x1502334C: DIP_A_GGM_LUT_G[67] */
            tuningBuf[0x00001350>>2] = 0x00001090; /* 0x15023350: DIP_A_GGM_LUT_G[68] */
            tuningBuf[0x00001354>>2] = 0x00001094; /* 0x15023354: DIP_A_GGM_LUT_G[69] */
            tuningBuf[0x00001358>>2] = 0x00001098; /* 0x15023358: DIP_A_GGM_LUT_G[70] */
            tuningBuf[0x0000135C>>2] = 0x0000109C; /* 0x1502335C: DIP_A_GGM_LUT_G[71] */
            tuningBuf[0x00001360>>2] = 0x000010A0; /* 0x15023360: DIP_A_GGM_LUT_G[72] */
            tuningBuf[0x00001364>>2] = 0x000010A4; /* 0x15023364: DIP_A_GGM_LUT_G[73] */
            tuningBuf[0x00001368>>2] = 0x000010A8; /* 0x15023368: DIP_A_GGM_LUT_G[74] */
            tuningBuf[0x0000136C>>2] = 0x000010AC; /* 0x1502336C: DIP_A_GGM_LUT_G[75] */
            tuningBuf[0x00001370>>2] = 0x000010B0; /* 0x15023370: DIP_A_GGM_LUT_G[76] */
            tuningBuf[0x00001374>>2] = 0x000010B4; /* 0x15023374: DIP_A_GGM_LUT_G[77] */
            tuningBuf[0x00001378>>2] = 0x000010B8; /* 0x15023378: DIP_A_GGM_LUT_G[78] */
            tuningBuf[0x0000137C>>2] = 0x000010BC; /* 0x1502337C: DIP_A_GGM_LUT_G[79] */
            tuningBuf[0x00001380>>2] = 0x000010C0; /* 0x15023380: DIP_A_GGM_LUT_G[80] */
            tuningBuf[0x00001384>>2] = 0x000010C4; /* 0x15023384: DIP_A_GGM_LUT_G[81] */
            tuningBuf[0x00001388>>2] = 0x000010C8; /* 0x15023388: DIP_A_GGM_LUT_G[82] */
            tuningBuf[0x0000138C>>2] = 0x000010CC; /* 0x1502338C: DIP_A_GGM_LUT_G[83] */
            tuningBuf[0x00001390>>2] = 0x000010D0; /* 0x15023390: DIP_A_GGM_LUT_G[84] */
            tuningBuf[0x00001394>>2] = 0x000010D4; /* 0x15023394: DIP_A_GGM_LUT_G[85] */
            tuningBuf[0x00001398>>2] = 0x000010D8; /* 0x15023398: DIP_A_GGM_LUT_G[86] */
            tuningBuf[0x0000139C>>2] = 0x000010DC; /* 0x1502339C: DIP_A_GGM_LUT_G[87] */
            tuningBuf[0x000013A0>>2] = 0x000010E0; /* 0x150233A0: DIP_A_GGM_LUT_G[88] */
            tuningBuf[0x000013A4>>2] = 0x000010E4; /* 0x150233A4: DIP_A_GGM_LUT_G[89] */
            tuningBuf[0x000013A8>>2] = 0x000010E8; /* 0x150233A8: DIP_A_GGM_LUT_G[90] */
            tuningBuf[0x000013AC>>2] = 0x000010EC; /* 0x150233AC: DIP_A_GGM_LUT_G[91] */
            tuningBuf[0x000013B0>>2] = 0x000010F0; /* 0x150233B0: DIP_A_GGM_LUT_G[92] */
            tuningBuf[0x000013B4>>2] = 0x000010F4; /* 0x150233B4: DIP_A_GGM_LUT_G[93] */
            tuningBuf[0x000013B8>>2] = 0x000010F8; /* 0x150233B8: DIP_A_GGM_LUT_G[94] */
            tuningBuf[0x000013BC>>2] = 0x000010FC; /* 0x150233BC: DIP_A_GGM_LUT_G[95] */
            tuningBuf[0x000013C0>>2] = 0x00002100; /* 0x150233C0: DIP_A_GGM_LUT_G[96] */
            tuningBuf[0x000013C4>>2] = 0x00002108; /* 0x150233C4: DIP_A_GGM_LUT_G[97] */
            tuningBuf[0x000013C8>>2] = 0x00002110; /* 0x150233C8: DIP_A_GGM_LUT_G[98] */
            tuningBuf[0x000013CC>>2] = 0x00002118; /* 0x150233CC: DIP_A_GGM_LUT_G[99] */
            tuningBuf[0x000013D0>>2] = 0x00002120; /* 0x150233D0: DIP_A_GGM_LUT_G[100] */
            tuningBuf[0x000013D4>>2] = 0x00002128; /* 0x150233D4: DIP_A_GGM_LUT_G[101] */
            tuningBuf[0x000013D8>>2] = 0x00002130; /* 0x150233D8: DIP_A_GGM_LUT_G[102] */
            tuningBuf[0x000013DC>>2] = 0x00002138; /* 0x150233DC: DIP_A_GGM_LUT_G[103] */
            tuningBuf[0x000013E0>>2] = 0x00002140; /* 0x150233E0: DIP_A_GGM_LUT_G[104] */
            tuningBuf[0x000013E4>>2] = 0x00002148; /* 0x150233E4: DIP_A_GGM_LUT_G[105] */
            tuningBuf[0x000013E8>>2] = 0x00002150; /* 0x150233E8: DIP_A_GGM_LUT_G[106] */
            tuningBuf[0x000013EC>>2] = 0x00002158; /* 0x150233EC: DIP_A_GGM_LUT_G[107] */
            tuningBuf[0x000013F0>>2] = 0x00002160; /* 0x150233F0: DIP_A_GGM_LUT_G[108] */
            tuningBuf[0x000013F4>>2] = 0x00002168; /* 0x150233F4: DIP_A_GGM_LUT_G[109] */
            tuningBuf[0x000013F8>>2] = 0x00002170; /* 0x150233F8: DIP_A_GGM_LUT_G[110] */
            tuningBuf[0x000013FC>>2] = 0x00002178; /* 0x150233FC: DIP_A_GGM_LUT_G[111] */
            tuningBuf[0x00001400>>2] = 0x00002180; /* 0x15023400: DIP_A_GGM_LUT_G[112] */
            tuningBuf[0x00001404>>2] = 0x00002188; /* 0x15023404: DIP_A_GGM_LUT_G[113] */
            tuningBuf[0x00001408>>2] = 0x00002190; /* 0x15023408: DIP_A_GGM_LUT_G[114] */
            tuningBuf[0x0000140C>>2] = 0x00002198; /* 0x1502340C: DIP_A_GGM_LUT_G[115] */
            tuningBuf[0x00001410>>2] = 0x000021A0; /* 0x15023410: DIP_A_GGM_LUT_G[116] */
            tuningBuf[0x00001414>>2] = 0x000021A8; /* 0x15023414: DIP_A_GGM_LUT_G[117] */
            tuningBuf[0x00001418>>2] = 0x000021B0; /* 0x15023418: DIP_A_GGM_LUT_G[118] */
            tuningBuf[0x0000141C>>2] = 0x000021B8; /* 0x1502341C: DIP_A_GGM_LUT_G[119] */
            tuningBuf[0x00001420>>2] = 0x000021C0; /* 0x15023420: DIP_A_GGM_LUT_G[120] */
            tuningBuf[0x00001424>>2] = 0x000021C8; /* 0x15023424: DIP_A_GGM_LUT_G[121] */
            tuningBuf[0x00001428>>2] = 0x000021D0; /* 0x15023428: DIP_A_GGM_LUT_G[122] */
            tuningBuf[0x0000142C>>2] = 0x000021D8; /* 0x1502342C: DIP_A_GGM_LUT_G[123] */
            tuningBuf[0x00001430>>2] = 0x000021E0; /* 0x15023430: DIP_A_GGM_LUT_G[124] */
            tuningBuf[0x00001434>>2] = 0x000021E8; /* 0x15023434: DIP_A_GGM_LUT_G[125] */
            tuningBuf[0x00001438>>2] = 0x000021F0; /* 0x15023438: DIP_A_GGM_LUT_G[126] */
            tuningBuf[0x0000143C>>2] = 0x000021F8; /* 0x1502343C: DIP_A_GGM_LUT_G[127] */
            tuningBuf[0x00001440>>2] = 0x00008200; /* 0x15023440: DIP_A_GGM_LUT_G[128] */
            tuningBuf[0x00001444>>2] = 0x00008220; /* 0x15023444: DIP_A_GGM_LUT_G[129] */
            tuningBuf[0x00001448>>2] = 0x00008240; /* 0x15023448: DIP_A_GGM_LUT_G[130] */
            tuningBuf[0x0000144C>>2] = 0x00008260; /* 0x1502344C: DIP_A_GGM_LUT_G[131] */
            tuningBuf[0x00001450>>2] = 0x00008280; /* 0x15023450: DIP_A_GGM_LUT_G[132] */
            tuningBuf[0x00001454>>2] = 0x000082A0; /* 0x15023454: DIP_A_GGM_LUT_G[133] */
            tuningBuf[0x00001458>>2] = 0x000082C0; /* 0x15023458: DIP_A_GGM_LUT_G[134] */
            tuningBuf[0x0000145C>>2] = 0x000082E0; /* 0x1502345C: DIP_A_GGM_LUT_G[135] */
            tuningBuf[0x00001460>>2] = 0x00008300; /* 0x15023460: DIP_A_GGM_LUT_G[136] */
            tuningBuf[0x00001464>>2] = 0x00008320; /* 0x15023464: DIP_A_GGM_LUT_G[137] */
            tuningBuf[0x00001468>>2] = 0x00008340; /* 0x15023468: DIP_A_GGM_LUT_G[138] */
            tuningBuf[0x0000146C>>2] = 0x00008360; /* 0x1502346C: DIP_A_GGM_LUT_G[139] */
            tuningBuf[0x00001470>>2] = 0x00008380; /* 0x15023470: DIP_A_GGM_LUT_G[140] */
            tuningBuf[0x00001474>>2] = 0x000083A0; /* 0x15023474: DIP_A_GGM_LUT_G[141] */
            tuningBuf[0x00001478>>2] = 0x000083C0; /* 0x15023478: DIP_A_GGM_LUT_G[142] */
            tuningBuf[0x0000147C>>2] = 0x00007FE0; /* 0x1502347C: DIP_A_GGM_LUT_G[143] */
            tuningBuf[0x00001480>>2] = 0x00000000; /* 0x15023480: DIP_A_GGM_CTRL */
            tuningBuf[0x00003000>>2] = 0x51FC32A2; /* 0x15025000: DIP_B_GGM_LUT_RB[0] */
            tuningBuf[0x00003004>>2] = 0x7CD35AAB; /* 0x15025004: DIP_B_GGM_LUT_RB[1] */
            tuningBuf[0x00003008>>2] = 0x832D3C37; /* 0x15025008: DIP_B_GGM_LUT_RB[2] */
            tuningBuf[0x0000300C>>2] = 0x19837F05; /* 0x1502500C: DIP_B_GGM_LUT_RB[3] */
            tuningBuf[0x00003010>>2] = 0xC5849CE7; /* 0x15025010: DIP_B_GGM_LUT_RB[4] */
            tuningBuf[0x00003014>>2] = 0xF302BF18; /* 0x15025014: DIP_B_GGM_LUT_RB[5] */
            tuningBuf[0x00003018>>2] = 0xAFB97024; /* 0x15025018: DIP_B_GGM_LUT_RB[6] */
            tuningBuf[0x0000301C>>2] = 0x4D0CC69E; /* 0x1502501C: DIP_B_GGM_LUT_RB[7] */
            tuningBuf[0x00003020>>2] = 0xC4F0D74B; /* 0x15025020: DIP_B_GGM_LUT_RB[8] */
            tuningBuf[0x00003024>>2] = 0x66A581F1; /* 0x15025024: DIP_B_GGM_LUT_RB[9] */
            tuningBuf[0x00003028>>2] = 0x4FF5E73F; /* 0x15025028: DIP_B_GGM_LUT_RB[10] */
            tuningBuf[0x0000302C>>2] = 0xC4718E8E; /* 0x1502502C: DIP_B_GGM_LUT_RB[11] */
            tuningBuf[0x00003030>>2] = 0x9C86DCB9; /* 0x15025030: DIP_B_GGM_LUT_RB[12] */
            tuningBuf[0x00003034>>2] = 0xBC3F0C2E; /* 0x15025034: DIP_B_GGM_LUT_RB[13] */
            tuningBuf[0x00003038>>2] = 0xA0204380; /* 0x15025038: DIP_B_GGM_LUT_RB[14] */
            tuningBuf[0x0000303C>>2] = 0x70314E74; /* 0x1502503C: DIP_B_GGM_LUT_RB[15] */
            tuningBuf[0x00003040>>2] = 0x5C52EDA8; /* 0x15025040: DIP_B_GGM_LUT_RB[16] */
            tuningBuf[0x00003044>>2] = 0x0C77F482; /* 0x15025044: DIP_B_GGM_LUT_RB[17] */
            tuningBuf[0x00003048>>2] = 0x2CE55E8C; /* 0x15025048: DIP_B_GGM_LUT_RB[18] */
            tuningBuf[0x0000304C>>2] = 0xC2B86EF5; /* 0x1502504C: DIP_B_GGM_LUT_RB[19] */
            tuningBuf[0x00003050>>2] = 0x118F80F7; /* 0x15025050: DIP_B_GGM_LUT_RB[20] */
            tuningBuf[0x00003054>>2] = 0xF430D1D9; /* 0x15025054: DIP_B_GGM_LUT_RB[21] */
            tuningBuf[0x00003058>>2] = 0x543E5522; /* 0x15025058: DIP_B_GGM_LUT_RB[22] */
            tuningBuf[0x0000305C>>2] = 0xF0F5A76C; /* 0x1502505C: DIP_B_GGM_LUT_RB[23] */
            tuningBuf[0x00003060>>2] = 0x446E7727; /* 0x15025060: DIP_B_GGM_LUT_RB[24] */
            tuningBuf[0x00003064>>2] = 0xE1A63E64; /* 0x15025064: DIP_B_GGM_LUT_RB[25] */
            tuningBuf[0x00003068>>2] = 0xEC939B44; /* 0x15025068: DIP_B_GGM_LUT_RB[26] */
            tuningBuf[0x0000306C>>2] = 0xC8ABC865; /* 0x1502506C: DIP_B_GGM_LUT_RB[27] */
            tuningBuf[0x00003070>>2] = 0x8BE4B9AD; /* 0x15025070: DIP_B_GGM_LUT_RB[28] */
            tuningBuf[0x00003074>>2] = 0x0CE97EDB; /* 0x15025074: DIP_B_GGM_LUT_RB[29] */
            tuningBuf[0x00003078>>2] = 0x38FD6F8D; /* 0x15025078: DIP_B_GGM_LUT_RB[30] */
            tuningBuf[0x0000307C>>2] = 0xA34FC705; /* 0x1502507C: DIP_B_GGM_LUT_RB[31] */
            tuningBuf[0x00003080>>2] = 0x875961C7; /* 0x15025080: DIP_B_GGM_LUT_RB[32] */
            tuningBuf[0x00003084>>2] = 0x1C4F550D; /* 0x15025084: DIP_B_GGM_LUT_RB[33] */
            tuningBuf[0x00003088>>2] = 0x7B40D1D2; /* 0x15025088: DIP_B_GGM_LUT_RB[34] */
            tuningBuf[0x0000308C>>2] = 0xB069AF63; /* 0x1502508C: DIP_B_GGM_LUT_RB[35] */
            tuningBuf[0x00003090>>2] = 0x8AD7D124; /* 0x15025090: DIP_B_GGM_LUT_RB[36] */
            tuningBuf[0x00003094>>2] = 0xF9EDA6D1; /* 0x15025094: DIP_B_GGM_LUT_RB[37] */
            tuningBuf[0x00003098>>2] = 0x5D907A88; /* 0x15025098: DIP_B_GGM_LUT_RB[38] */
            tuningBuf[0x0000309C>>2] = 0x2E233543; /* 0x1502509C: DIP_B_GGM_LUT_RB[39] */
            tuningBuf[0x000030A0>>2] = 0x80AD5315; /* 0x150250A0: DIP_B_GGM_LUT_RB[40] */
            tuningBuf[0x000030A4>>2] = 0xC3E272CB; /* 0x150250A4: DIP_B_GGM_LUT_RB[41] */
            tuningBuf[0x000030A8>>2] = 0x846AD653; /* 0x150250A8: DIP_B_GGM_LUT_RB[42] */
            tuningBuf[0x000030AC>>2] = 0x9A89C881; /* 0x150250AC: DIP_B_GGM_LUT_RB[43] */
            tuningBuf[0x000030B0>>2] = 0x5A90B9ED; /* 0x150250B0: DIP_B_GGM_LUT_RB[44] */
            tuningBuf[0x000030B4>>2] = 0x0BD2F910; /* 0x150250B4: DIP_B_GGM_LUT_RB[45] */
            tuningBuf[0x000030B8>>2] = 0xEFE451E9; /* 0x150250B8: DIP_B_GGM_LUT_RB[46] */
            tuningBuf[0x000030BC>>2] = 0x0E57DB60; /* 0x150250BC: DIP_B_GGM_LUT_RB[47] */
            tuningBuf[0x000030C0>>2] = 0x1444C642; /* 0x150250C0: DIP_B_GGM_LUT_RB[48] */
            tuningBuf[0x000030C4>>2] = 0x0E04AFD6; /* 0x150250C4: DIP_B_GGM_LUT_RB[49] */
            tuningBuf[0x000030C8>>2] = 0x2BD9F148; /* 0x150250C8: DIP_B_GGM_LUT_RB[50] */
            tuningBuf[0x000030CC>>2] = 0x9D1D1E6E; /* 0x150250CC: DIP_B_GGM_LUT_RB[51] */
            tuningBuf[0x000030D0>>2] = 0xEBEE3E03; /* 0x150250D0: DIP_B_GGM_LUT_RB[52] */
            tuningBuf[0x000030D4>>2] = 0x1EBB3E11; /* 0x150250D4: DIP_B_GGM_LUT_RB[53] */
            tuningBuf[0x000030D8>>2] = 0x2CC18D75; /* 0x150250D8: DIP_B_GGM_LUT_RB[54] */
            tuningBuf[0x000030DC>>2] = 0xEA162348; /* 0x150250DC: DIP_B_GGM_LUT_RB[55] */
            tuningBuf[0x000030E0>>2] = 0xE3E7EB69; /* 0x150250E0: DIP_B_GGM_LUT_RB[56] */
            tuningBuf[0x000030E4>>2] = 0x7ACFE8FD; /* 0x150250E4: DIP_B_GGM_LUT_RB[57] */
            tuningBuf[0x000030E8>>2] = 0xA4253C0A; /* 0x150250E8: DIP_B_GGM_LUT_RB[58] */
            tuningBuf[0x000030EC>>2] = 0x8B03FFA2; /* 0x150250EC: DIP_B_GGM_LUT_RB[59] */
            tuningBuf[0x000030F0>>2] = 0xE8994F52; /* 0x150250F0: DIP_B_GGM_LUT_RB[60] */
            tuningBuf[0x000030F4>>2] = 0xCF5DDB50; /* 0x150250F4: DIP_B_GGM_LUT_RB[61] */
            tuningBuf[0x000030F8>>2] = 0xA6BF21A2; /* 0x150250F8: DIP_B_GGM_LUT_RB[62] */
            tuningBuf[0x000030FC>>2] = 0xB98A101C; /* 0x150250FC: DIP_B_GGM_LUT_RB[63] */
            tuningBuf[0x00003100>>2] = 0x9BDA2515; /* 0x15025100: DIP_B_GGM_LUT_RB[64] */
            tuningBuf[0x00003104>>2] = 0xF9A256DF; /* 0x15025104: DIP_B_GGM_LUT_RB[65] */
            tuningBuf[0x00003108>>2] = 0xD84834D4; /* 0x15025108: DIP_B_GGM_LUT_RB[66] */
            tuningBuf[0x0000310C>>2] = 0x9FD42127; /* 0x1502510C: DIP_B_GGM_LUT_RB[67] */
            tuningBuf[0x00003110>>2] = 0x357C27D1; /* 0x15025110: DIP_B_GGM_LUT_RB[68] */
            tuningBuf[0x00003114>>2] = 0x053D1CD3; /* 0x15025114: DIP_B_GGM_LUT_RB[69] */
            tuningBuf[0x00003118>>2] = 0x758FA6EB; /* 0x15025118: DIP_B_GGM_LUT_RB[70] */
            tuningBuf[0x0000311C>>2] = 0xB4D85B4C; /* 0x1502511C: DIP_B_GGM_LUT_RB[71] */
            tuningBuf[0x00003120>>2] = 0x85F839D1; /* 0x15025120: DIP_B_GGM_LUT_RB[72] */
            tuningBuf[0x00003124>>2] = 0xFFE9F8ED; /* 0x15025124: DIP_B_GGM_LUT_RB[73] */
            tuningBuf[0x00003128>>2] = 0x45EF466C; /* 0x15025128: DIP_B_GGM_LUT_RB[74] */
            tuningBuf[0x0000312C>>2] = 0xF2875267; /* 0x1502512C: DIP_B_GGM_LUT_RB[75] */
            tuningBuf[0x00003130>>2] = 0x396A9866; /* 0x15025130: DIP_B_GGM_LUT_RB[76] */
            tuningBuf[0x00003134>>2] = 0xB3DE96C4; /* 0x15025134: DIP_B_GGM_LUT_RB[77] */
            tuningBuf[0x00003138>>2] = 0x1085FA77; /* 0x15025138: DIP_B_GGM_LUT_RB[78] */
            tuningBuf[0x0000313C>>2] = 0x97BBB97B; /* 0x1502513C: DIP_B_GGM_LUT_RB[79] */
            tuningBuf[0x00003140>>2] = 0xD9B2A1B9; /* 0x15025140: DIP_B_GGM_LUT_RB[80] */
            tuningBuf[0x00003144>>2] = 0x5FCFCE67; /* 0x15025144: DIP_B_GGM_LUT_RB[81] */
            tuningBuf[0x00003148>>2] = 0x969A3174; /* 0x15025148: DIP_B_GGM_LUT_RB[82] */
            tuningBuf[0x0000314C>>2] = 0xF94C1AF7; /* 0x1502514C: DIP_B_GGM_LUT_RB[83] */
            tuningBuf[0x00003150>>2] = 0x0A95B6C1; /* 0x15025150: DIP_B_GGM_LUT_RB[84] */
            tuningBuf[0x00003154>>2] = 0x0D245257; /* 0x15025154: DIP_B_GGM_LUT_RB[85] */
            tuningBuf[0x00003158>>2] = 0xF042416F; /* 0x15025158: DIP_B_GGM_LUT_RB[86] */
            tuningBuf[0x0000315C>>2] = 0x270D5666; /* 0x1502515C: DIP_B_GGM_LUT_RB[87] */
            tuningBuf[0x00003160>>2] = 0xD1DF52FC; /* 0x15025160: DIP_B_GGM_LUT_RB[88] */
            tuningBuf[0x00003164>>2] = 0xA93F054E; /* 0x15025164: DIP_B_GGM_LUT_RB[89] */
            tuningBuf[0x00003168>>2] = 0xDA58E517; /* 0x15025168: DIP_B_GGM_LUT_RB[90] */
            tuningBuf[0x0000316C>>2] = 0x827E0CCA; /* 0x1502516C: DIP_B_GGM_LUT_RB[91] */
            tuningBuf[0x00003170>>2] = 0x4E469D0B; /* 0x15025170: DIP_B_GGM_LUT_RB[92] */
            tuningBuf[0x00003174>>2] = 0xD9D8B48C; /* 0x15025174: DIP_B_GGM_LUT_RB[93] */
            tuningBuf[0x00003178>>2] = 0xFC7ACAF0; /* 0x15025178: DIP_B_GGM_LUT_RB[94] */
            tuningBuf[0x0000317C>>2] = 0x85EA04C7; /* 0x1502517C: DIP_B_GGM_LUT_RB[95] */
            tuningBuf[0x00003180>>2] = 0x8292AA9D; /* 0x15025180: DIP_B_GGM_LUT_RB[96] */
            tuningBuf[0x00003184>>2] = 0xA75FFC20; /* 0x15025184: DIP_B_GGM_LUT_RB[97] */
            tuningBuf[0x00003188>>2] = 0xA4CDA426; /* 0x15025188: DIP_B_GGM_LUT_RB[98] */
            tuningBuf[0x0000318C>>2] = 0xF4CF7855; /* 0x1502518C: DIP_B_GGM_LUT_RB[99] */
            tuningBuf[0x00003190>>2] = 0x1ABC3489; /* 0x15025190: DIP_B_GGM_LUT_RB[100] */
            tuningBuf[0x00003194>>2] = 0x1DD15E31; /* 0x15025194: DIP_B_GGM_LUT_RB[101] */
            tuningBuf[0x00003198>>2] = 0x52944250; /* 0x15025198: DIP_B_GGM_LUT_RB[102] */
            tuningBuf[0x0000319C>>2] = 0xBC53E69F; /* 0x1502519C: DIP_B_GGM_LUT_RB[103] */
            tuningBuf[0x000031A0>>2] = 0x40B1AF29; /* 0x150251A0: DIP_B_GGM_LUT_RB[104] */
            tuningBuf[0x000031A4>>2] = 0x91AEEBAE; /* 0x150251A4: DIP_B_GGM_LUT_RB[105] */
            tuningBuf[0x000031A8>>2] = 0xDFCC883F; /* 0x150251A8: DIP_B_GGM_LUT_RB[106] */
            tuningBuf[0x000031AC>>2] = 0xFE93F521; /* 0x150251AC: DIP_B_GGM_LUT_RB[107] */
            tuningBuf[0x000031B0>>2] = 0xC3FE399C; /* 0x150251B0: DIP_B_GGM_LUT_RB[108] */
            tuningBuf[0x000031B4>>2] = 0xB06CCBF5; /* 0x150251B4: DIP_B_GGM_LUT_RB[109] */
            tuningBuf[0x000031B8>>2] = 0xA1941704; /* 0x150251B8: DIP_B_GGM_LUT_RB[110] */
            tuningBuf[0x000031BC>>2] = 0xC8A9CE6A; /* 0x150251BC: DIP_B_GGM_LUT_RB[111] */
            tuningBuf[0x000031C0>>2] = 0xB466055C; /* 0x150251C0: DIP_B_GGM_LUT_RB[112] */
            tuningBuf[0x000031C4>>2] = 0xD2CF41C1; /* 0x150251C4: DIP_B_GGM_LUT_RB[113] */
            tuningBuf[0x000031C8>>2] = 0x2F900ED7; /* 0x150251C8: DIP_B_GGM_LUT_RB[114] */
            tuningBuf[0x000031CC>>2] = 0xD0A13E6D; /* 0x150251CC: DIP_B_GGM_LUT_RB[115] */
            tuningBuf[0x000031D0>>2] = 0x23EB776C; /* 0x150251D0: DIP_B_GGM_LUT_RB[116] */
            tuningBuf[0x000031D4>>2] = 0xF8329688; /* 0x150251D4: DIP_B_GGM_LUT_RB[117] */
            tuningBuf[0x000031D8>>2] = 0x04BF03BA; /* 0x150251D8: DIP_B_GGM_LUT_RB[118] */
            tuningBuf[0x000031DC>>2] = 0xFE0383A3; /* 0x150251DC: DIP_B_GGM_LUT_RB[119] */
            tuningBuf[0x000031E0>>2] = 0xB9F354D1; /* 0x150251E0: DIP_B_GGM_LUT_RB[120] */
            tuningBuf[0x000031E4>>2] = 0x1FC774F3; /* 0x150251E4: DIP_B_GGM_LUT_RB[121] */
            tuningBuf[0x000031E8>>2] = 0x8950DC74; /* 0x150251E8: DIP_B_GGM_LUT_RB[122] */
            tuningBuf[0x000031EC>>2] = 0x58C6BA69; /* 0x150251EC: DIP_B_GGM_LUT_RB[123] */
            tuningBuf[0x000031F0>>2] = 0x807E3B2D; /* 0x150251F0: DIP_B_GGM_LUT_RB[124] */
            tuningBuf[0x000031F4>>2] = 0xF2342D86; /* 0x150251F4: DIP_B_GGM_LUT_RB[125] */
            tuningBuf[0x000031F8>>2] = 0x809CBF51; /* 0x150251F8: DIP_B_GGM_LUT_RB[126] */
            tuningBuf[0x000031FC>>2] = 0x2FFC258F; /* 0x150251FC: DIP_B_GGM_LUT_RB[127] */
            tuningBuf[0x00003200>>2] = 0x9FE0EF0E; /* 0x15025200: DIP_B_GGM_LUT_RB[128] */
            tuningBuf[0x00003204>>2] = 0x05BE409B; /* 0x15025204: DIP_B_GGM_LUT_RB[129] */
            tuningBuf[0x00003208>>2] = 0x3FEDF830; /* 0x15025208: DIP_B_GGM_LUT_RB[130] */
            tuningBuf[0x0000320C>>2] = 0xD991AED8; /* 0x1502520C: DIP_B_GGM_LUT_RB[131] */
            tuningBuf[0x00003210>>2] = 0x95B77374; /* 0x15025210: DIP_B_GGM_LUT_RB[132] */
            tuningBuf[0x00003214>>2] = 0x92B3573D; /* 0x15025214: DIP_B_GGM_LUT_RB[133] */
            tuningBuf[0x00003218>>2] = 0x267E8F95; /* 0x15025218: DIP_B_GGM_LUT_RB[134] */
            tuningBuf[0x0000321C>>2] = 0x9F030BEC; /* 0x1502521C: DIP_B_GGM_LUT_RB[135] */
            tuningBuf[0x00003220>>2] = 0x1C0B9A54; /* 0x15025220: DIP_B_GGM_LUT_RB[136] */
            tuningBuf[0x00003224>>2] = 0x53454A3A; /* 0x15025224: DIP_B_GGM_LUT_RB[137] */
            tuningBuf[0x00003228>>2] = 0xC44FF7FB; /* 0x15025228: DIP_B_GGM_LUT_RB[138] */
            tuningBuf[0x0000322C>>2] = 0x8B920BAF; /* 0x1502522C: DIP_B_GGM_LUT_RB[139] */
            tuningBuf[0x00003230>>2] = 0xBB36387E; /* 0x15025230: DIP_B_GGM_LUT_RB[140] */
            tuningBuf[0x00003234>>2] = 0x19D78E97; /* 0x15025234: DIP_B_GGM_LUT_RB[141] */
            tuningBuf[0x00003238>>2] = 0x1B3BBF42; /* 0x15025238: DIP_B_GGM_LUT_RB[142] */
            tuningBuf[0x0000323C>>2] = 0x1086B7BD; /* 0x1502523C: DIP_B_GGM_LUT_RB[143] */
            tuningBuf[0x00003240>>2] = 0x0000AF3A; /* 0x15025240: DIP_B_GGM_LUT_G[0] */
            tuningBuf[0x00003244>>2] = 0x00004C1D; /* 0x15025244: DIP_B_GGM_LUT_G[1] */
            tuningBuf[0x00003248>>2] = 0x000061FD; /* 0x15025248: DIP_B_GGM_LUT_G[2] */
            tuningBuf[0x0000324C>>2] = 0x000088A7; /* 0x1502524C: DIP_B_GGM_LUT_G[3] */
            tuningBuf[0x00003250>>2] = 0x00002412; /* 0x15025250: DIP_B_GGM_LUT_G[4] */
            tuningBuf[0x00003254>>2] = 0x0000332D; /* 0x15025254: DIP_B_GGM_LUT_G[5] */
            tuningBuf[0x00003258>>2] = 0x0000F137; /* 0x15025258: DIP_B_GGM_LUT_G[6] */
            tuningBuf[0x0000325C>>2] = 0x00007A64; /* 0x1502525C: DIP_B_GGM_LUT_G[7] */
            tuningBuf[0x00003260>>2] = 0x000064ED; /* 0x15025260: DIP_B_GGM_LUT_G[8] */
            tuningBuf[0x00003264>>2] = 0x000083FA; /* 0x15025264: DIP_B_GGM_LUT_G[9] */
            tuningBuf[0x00003268>>2] = 0x0000C410; /* 0x15025268: DIP_B_GGM_LUT_G[10] */
            tuningBuf[0x0000326C>>2] = 0x000076FD; /* 0x1502526C: DIP_B_GGM_LUT_G[11] */
            tuningBuf[0x00003270>>2] = 0x0000310F; /* 0x15025270: DIP_B_GGM_LUT_G[12] */
            tuningBuf[0x00003274>>2] = 0x000076DC; /* 0x15025274: DIP_B_GGM_LUT_G[13] */
            tuningBuf[0x00003278>>2] = 0x0000B6B4; /* 0x15025278: DIP_B_GGM_LUT_G[14] */
            tuningBuf[0x0000327C>>2] = 0x00003CEF; /* 0x1502527C: DIP_B_GGM_LUT_G[15] */
            tuningBuf[0x00003280>>2] = 0x000077F2; /* 0x15025280: DIP_B_GGM_LUT_G[16] */
            tuningBuf[0x00003284>>2] = 0x0000902D; /* 0x15025284: DIP_B_GGM_LUT_G[17] */
            tuningBuf[0x00003288>>2] = 0x00009E30; /* 0x15025288: DIP_B_GGM_LUT_G[18] */
            tuningBuf[0x0000328C>>2] = 0x0000FA5C; /* 0x1502528C: DIP_B_GGM_LUT_G[19] */
            tuningBuf[0x00003290>>2] = 0x0000A3AB; /* 0x15025290: DIP_B_GGM_LUT_G[20] */
            tuningBuf[0x00003294>>2] = 0x0000F9E5; /* 0x15025294: DIP_B_GGM_LUT_G[21] */
            tuningBuf[0x00003298>>2] = 0x0000F6DA; /* 0x15025298: DIP_B_GGM_LUT_G[22] */
            tuningBuf[0x0000329C>>2] = 0x00002058; /* 0x1502529C: DIP_B_GGM_LUT_G[23] */
            tuningBuf[0x000032A0>>2] = 0x00001DB8; /* 0x150252A0: DIP_B_GGM_LUT_G[24] */
            tuningBuf[0x000032A4>>2] = 0x00003EE7; /* 0x150252A4: DIP_B_GGM_LUT_G[25] */
            tuningBuf[0x000032A8>>2] = 0x0000B8B1; /* 0x150252A8: DIP_B_GGM_LUT_G[26] */
            tuningBuf[0x000032AC>>2] = 0x000090F3; /* 0x150252AC: DIP_B_GGM_LUT_G[27] */
            tuningBuf[0x000032B0>>2] = 0x00001637; /* 0x150252B0: DIP_B_GGM_LUT_G[28] */
            tuningBuf[0x000032B4>>2] = 0x00007895; /* 0x150252B4: DIP_B_GGM_LUT_G[29] */
            tuningBuf[0x000032B8>>2] = 0x00003BF4; /* 0x150252B8: DIP_B_GGM_LUT_G[30] */
            tuningBuf[0x000032BC>>2] = 0x0000AF04; /* 0x150252BC: DIP_B_GGM_LUT_G[31] */
            tuningBuf[0x000032C0>>2] = 0x0000119A; /* 0x150252C0: DIP_B_GGM_LUT_G[32] */
            tuningBuf[0x000032C4>>2] = 0x0000A4C0; /* 0x150252C4: DIP_B_GGM_LUT_G[33] */
            tuningBuf[0x000032C8>>2] = 0x000000B9; /* 0x150252C8: DIP_B_GGM_LUT_G[34] */
            tuningBuf[0x000032CC>>2] = 0x0000531E; /* 0x150252CC: DIP_B_GGM_LUT_G[35] */
            tuningBuf[0x000032D0>>2] = 0x0000BEAC; /* 0x150252D0: DIP_B_GGM_LUT_G[36] */
            tuningBuf[0x000032D4>>2] = 0x00004E6B; /* 0x150252D4: DIP_B_GGM_LUT_G[37] */
            tuningBuf[0x000032D8>>2] = 0x00005BE5; /* 0x150252D8: DIP_B_GGM_LUT_G[38] */
            tuningBuf[0x000032DC>>2] = 0x000008F7; /* 0x150252DC: DIP_B_GGM_LUT_G[39] */
            tuningBuf[0x000032E0>>2] = 0x0000D97D; /* 0x150252E0: DIP_B_GGM_LUT_G[40] */
            tuningBuf[0x000032E4>>2] = 0x000069F5; /* 0x150252E4: DIP_B_GGM_LUT_G[41] */
            tuningBuf[0x000032E8>>2] = 0x00002048; /* 0x150252E8: DIP_B_GGM_LUT_G[42] */
            tuningBuf[0x000032EC>>2] = 0x000088A8; /* 0x150252EC: DIP_B_GGM_LUT_G[43] */
            tuningBuf[0x000032F0>>2] = 0x0000C246; /* 0x150252F0: DIP_B_GGM_LUT_G[44] */
            tuningBuf[0x000032F4>>2] = 0x0000EFE7; /* 0x150252F4: DIP_B_GGM_LUT_G[45] */
            tuningBuf[0x000032F8>>2] = 0x00002821; /* 0x150252F8: DIP_B_GGM_LUT_G[46] */
            tuningBuf[0x000032FC>>2] = 0x00005662; /* 0x150252FC: DIP_B_GGM_LUT_G[47] */
            tuningBuf[0x00003300>>2] = 0x0000EF56; /* 0x15025300: DIP_B_GGM_LUT_G[48] */
            tuningBuf[0x00003304>>2] = 0x00000C9A; /* 0x15025304: DIP_B_GGM_LUT_G[49] */
            tuningBuf[0x00003308>>2] = 0x0000C677; /* 0x15025308: DIP_B_GGM_LUT_G[50] */
            tuningBuf[0x0000330C>>2] = 0x0000D528; /* 0x1502530C: DIP_B_GGM_LUT_G[51] */
            tuningBuf[0x00003310>>2] = 0x0000A1AA; /* 0x15025310: DIP_B_GGM_LUT_G[52] */
            tuningBuf[0x00003314>>2] = 0x0000C734; /* 0x15025314: DIP_B_GGM_LUT_G[53] */
            tuningBuf[0x00003318>>2] = 0x0000E931; /* 0x15025318: DIP_B_GGM_LUT_G[54] */
            tuningBuf[0x0000331C>>2] = 0x00001E41; /* 0x1502531C: DIP_B_GGM_LUT_G[55] */
            tuningBuf[0x00003320>>2] = 0x0000FF7E; /* 0x15025320: DIP_B_GGM_LUT_G[56] */
            tuningBuf[0x00003324>>2] = 0x0000827C; /* 0x15025324: DIP_B_GGM_LUT_G[57] */
            tuningBuf[0x00003328>>2] = 0x0000AC78; /* 0x15025328: DIP_B_GGM_LUT_G[58] */
            tuningBuf[0x0000332C>>2] = 0x000070F7; /* 0x1502532C: DIP_B_GGM_LUT_G[59] */
            tuningBuf[0x00003330>>2] = 0x000054D4; /* 0x15025330: DIP_B_GGM_LUT_G[60] */
            tuningBuf[0x00003334>>2] = 0x0000950D; /* 0x15025334: DIP_B_GGM_LUT_G[61] */
            tuningBuf[0x00003338>>2] = 0x0000D624; /* 0x15025338: DIP_B_GGM_LUT_G[62] */
            tuningBuf[0x0000333C>>2] = 0x00002151; /* 0x1502533C: DIP_B_GGM_LUT_G[63] */
            tuningBuf[0x00003340>>2] = 0x00004241; /* 0x15025340: DIP_B_GGM_LUT_G[64] */
            tuningBuf[0x00003344>>2] = 0x00001A91; /* 0x15025344: DIP_B_GGM_LUT_G[65] */
            tuningBuf[0x00003348>>2] = 0x0000C2E7; /* 0x15025348: DIP_B_GGM_LUT_G[66] */
            tuningBuf[0x0000334C>>2] = 0x0000FBF6; /* 0x1502534C: DIP_B_GGM_LUT_G[67] */
            tuningBuf[0x00003350>>2] = 0x0000CDD3; /* 0x15025350: DIP_B_GGM_LUT_G[68] */
            tuningBuf[0x00003354>>2] = 0x00005C1F; /* 0x15025354: DIP_B_GGM_LUT_G[69] */
            tuningBuf[0x00003358>>2] = 0x00002A50; /* 0x15025358: DIP_B_GGM_LUT_G[70] */
            tuningBuf[0x0000335C>>2] = 0x0000ED09; /* 0x1502535C: DIP_B_GGM_LUT_G[71] */
            tuningBuf[0x00003360>>2] = 0x00006FA8; /* 0x15025360: DIP_B_GGM_LUT_G[72] */
            tuningBuf[0x00003364>>2] = 0x0000BBC4; /* 0x15025364: DIP_B_GGM_LUT_G[73] */
            tuningBuf[0x00003368>>2] = 0x00003E82; /* 0x15025368: DIP_B_GGM_LUT_G[74] */
            tuningBuf[0x0000336C>>2] = 0x0000BE3C; /* 0x1502536C: DIP_B_GGM_LUT_G[75] */
            tuningBuf[0x00003370>>2] = 0x0000756E; /* 0x15025370: DIP_B_GGM_LUT_G[76] */
            tuningBuf[0x00003374>>2] = 0x00009E14; /* 0x15025374: DIP_B_GGM_LUT_G[77] */
            tuningBuf[0x00003378>>2] = 0x0000EF5C; /* 0x15025378: DIP_B_GGM_LUT_G[78] */
            tuningBuf[0x0000337C>>2] = 0x0000B770; /* 0x1502537C: DIP_B_GGM_LUT_G[79] */
            tuningBuf[0x00003380>>2] = 0x000057C0; /* 0x15025380: DIP_B_GGM_LUT_G[80] */
            tuningBuf[0x00003384>>2] = 0x0000A47B; /* 0x15025384: DIP_B_GGM_LUT_G[81] */
            tuningBuf[0x00003388>>2] = 0x000041B0; /* 0x15025388: DIP_B_GGM_LUT_G[82] */
            tuningBuf[0x0000338C>>2] = 0x0000E787; /* 0x1502538C: DIP_B_GGM_LUT_G[83] */
            tuningBuf[0x00003390>>2] = 0x000067E3; /* 0x15025390: DIP_B_GGM_LUT_G[84] */
            tuningBuf[0x00003394>>2] = 0x00002BC6; /* 0x15025394: DIP_B_GGM_LUT_G[85] */
            tuningBuf[0x00003398>>2] = 0x0000BBD8; /* 0x15025398: DIP_B_GGM_LUT_G[86] */
            tuningBuf[0x0000339C>>2] = 0x0000057A; /* 0x1502539C: DIP_B_GGM_LUT_G[87] */
            tuningBuf[0x000033A0>>2] = 0x00003BFF; /* 0x150253A0: DIP_B_GGM_LUT_G[88] */
            tuningBuf[0x000033A4>>2] = 0x00000122; /* 0x150253A4: DIP_B_GGM_LUT_G[89] */
            tuningBuf[0x000033A8>>2] = 0x0000D958; /* 0x150253A8: DIP_B_GGM_LUT_G[90] */
            tuningBuf[0x000033AC>>2] = 0x000035A9; /* 0x150253AC: DIP_B_GGM_LUT_G[91] */
            tuningBuf[0x000033B0>>2] = 0x0000A94E; /* 0x150253B0: DIP_B_GGM_LUT_G[92] */
            tuningBuf[0x000033B4>>2] = 0x0000D3F6; /* 0x150253B4: DIP_B_GGM_LUT_G[93] */
            tuningBuf[0x000033B8>>2] = 0x00000D3F; /* 0x150253B8: DIP_B_GGM_LUT_G[94] */
            tuningBuf[0x000033BC>>2] = 0x00009276; /* 0x150253BC: DIP_B_GGM_LUT_G[95] */
            tuningBuf[0x000033C0>>2] = 0x0000E1DC; /* 0x150253C0: DIP_B_GGM_LUT_G[96] */
            tuningBuf[0x000033C4>>2] = 0x0000AFB4; /* 0x150253C4: DIP_B_GGM_LUT_G[97] */
            tuningBuf[0x000033C8>>2] = 0x0000F79F; /* 0x150253C8: DIP_B_GGM_LUT_G[98] */
            tuningBuf[0x000033CC>>2] = 0x00001FF1; /* 0x150253CC: DIP_B_GGM_LUT_G[99] */
            tuningBuf[0x000033D0>>2] = 0x00000A7B; /* 0x150253D0: DIP_B_GGM_LUT_G[100] */
            tuningBuf[0x000033D4>>2] = 0x0000BC4D; /* 0x150253D4: DIP_B_GGM_LUT_G[101] */
            tuningBuf[0x000033D8>>2] = 0x0000F204; /* 0x150253D8: DIP_B_GGM_LUT_G[102] */
            tuningBuf[0x000033DC>>2] = 0x00008334; /* 0x150253DC: DIP_B_GGM_LUT_G[103] */
            tuningBuf[0x000033E0>>2] = 0x0000A4A4; /* 0x150253E0: DIP_B_GGM_LUT_G[104] */
            tuningBuf[0x000033E4>>2] = 0x000001E0; /* 0x150253E4: DIP_B_GGM_LUT_G[105] */
            tuningBuf[0x000033E8>>2] = 0x00009C5D; /* 0x150253E8: DIP_B_GGM_LUT_G[106] */
            tuningBuf[0x000033EC>>2] = 0x00008D42; /* 0x150253EC: DIP_B_GGM_LUT_G[107] */
            tuningBuf[0x000033F0>>2] = 0x00006921; /* 0x150253F0: DIP_B_GGM_LUT_G[108] */
            tuningBuf[0x000033F4>>2] = 0x0000AD72; /* 0x150253F4: DIP_B_GGM_LUT_G[109] */
            tuningBuf[0x000033F8>>2] = 0x00006E43; /* 0x150253F8: DIP_B_GGM_LUT_G[110] */
            tuningBuf[0x000033FC>>2] = 0x0000D9C8; /* 0x150253FC: DIP_B_GGM_LUT_G[111] */
            tuningBuf[0x00003400>>2] = 0x00008FBE; /* 0x15025400: DIP_B_GGM_LUT_G[112] */
            tuningBuf[0x00003404>>2] = 0x00005E0B; /* 0x15025404: DIP_B_GGM_LUT_G[113] */
            tuningBuf[0x00003408>>2] = 0x0000CBB1; /* 0x15025408: DIP_B_GGM_LUT_G[114] */
            tuningBuf[0x0000340C>>2] = 0x0000C41C; /* 0x1502540C: DIP_B_GGM_LUT_G[115] */
            tuningBuf[0x00003410>>2] = 0x000080D3; /* 0x15025410: DIP_B_GGM_LUT_G[116] */
            tuningBuf[0x00003414>>2] = 0x0000F698; /* 0x15025414: DIP_B_GGM_LUT_G[117] */
            tuningBuf[0x00003418>>2] = 0x0000F16F; /* 0x15025418: DIP_B_GGM_LUT_G[118] */
            tuningBuf[0x0000341C>>2] = 0x00009D18; /* 0x1502541C: DIP_B_GGM_LUT_G[119] */
            tuningBuf[0x00003420>>2] = 0x00006923; /* 0x15025420: DIP_B_GGM_LUT_G[120] */
            tuningBuf[0x00003424>>2] = 0x000009FA; /* 0x15025424: DIP_B_GGM_LUT_G[121] */
            tuningBuf[0x00003428>>2] = 0x0000CBF8; /* 0x15025428: DIP_B_GGM_LUT_G[122] */
            tuningBuf[0x0000342C>>2] = 0x0000E856; /* 0x1502542C: DIP_B_GGM_LUT_G[123] */
            tuningBuf[0x00003430>>2] = 0x00005476; /* 0x15025430: DIP_B_GGM_LUT_G[124] */
            tuningBuf[0x00003434>>2] = 0x00002008; /* 0x15025434: DIP_B_GGM_LUT_G[125] */
            tuningBuf[0x00003438>>2] = 0x0000E70F; /* 0x15025438: DIP_B_GGM_LUT_G[126] */
            tuningBuf[0x0000343C>>2] = 0x0000DAFB; /* 0x1502543C: DIP_B_GGM_LUT_G[127] */
            tuningBuf[0x00003440>>2] = 0x00001F75; /* 0x15025440: DIP_B_GGM_LUT_G[128] */
            tuningBuf[0x00003444>>2] = 0x0000D91F; /* 0x15025444: DIP_B_GGM_LUT_G[129] */
            tuningBuf[0x00003448>>2] = 0x00004430; /* 0x15025448: DIP_B_GGM_LUT_G[130] */
            tuningBuf[0x0000344C>>2] = 0x0000375E; /* 0x1502544C: DIP_B_GGM_LUT_G[131] */
            tuningBuf[0x00003450>>2] = 0x000027CB; /* 0x15025450: DIP_B_GGM_LUT_G[132] */
            tuningBuf[0x00003454>>2] = 0x0000D6E6; /* 0x15025454: DIP_B_GGM_LUT_G[133] */
            tuningBuf[0x00003458>>2] = 0x0000BD2D; /* 0x15025458: DIP_B_GGM_LUT_G[134] */
            tuningBuf[0x0000345C>>2] = 0x00004148; /* 0x1502545C: DIP_B_GGM_LUT_G[135] */
            tuningBuf[0x00003460>>2] = 0x0000E03E; /* 0x15025460: DIP_B_GGM_LUT_G[136] */
            tuningBuf[0x00003464>>2] = 0x0000B386; /* 0x15025464: DIP_B_GGM_LUT_G[137] */
            tuningBuf[0x00003468>>2] = 0x00005405; /* 0x15025468: DIP_B_GGM_LUT_G[138] */
            tuningBuf[0x0000346C>>2] = 0x00007CF4; /* 0x1502546C: DIP_B_GGM_LUT_G[139] */
            tuningBuf[0x00003470>>2] = 0x0000810A; /* 0x15025470: DIP_B_GGM_LUT_G[140] */
            tuningBuf[0x00003474>>2] = 0x0000AF77; /* 0x15025474: DIP_B_GGM_LUT_G[141] */
            tuningBuf[0x00003478>>2] = 0x000032BE; /* 0x15025478: DIP_B_GGM_LUT_G[142] */
            tuningBuf[0x0000347C>>2] = 0x000019F6; /* 0x1502547C: DIP_B_GGM_LUT_G[143] */
            tuningBuf[0x00003480>>2] = 0x00000001; /* 0x15025480: DIP_B_GGM_CTRL */
            pIspReg->DIP_X_CTL_RGB_EN.Bits.GGM_EN = 1;
            break;
        case tuning_tag_UDM:
            pIspReg->DIP_X_UDM_INTP_CRS.Raw = 0x38303060;
            pIspReg->DIP_X_UDM_INTP_NAT.Raw = 0x1430063F;
            pIspReg->DIP_X_UDM_INTP_AUG.Raw = 0x00600600;
            pIspReg->DIP_X_UDM_LUMA_LUT1.Raw = 0x07FF0100;
            pIspReg->DIP_X_UDM_LUMA_LUT2.Raw = 0x02008020;
            pIspReg->DIP_X_UDM_SL_CTL.Raw = 0x003FFFE0;
            pIspReg->DIP_X_UDM_HFTD_CTL.Raw = 0x08421000;
            pIspReg->DIP_X_UDM_NR_STR.Raw = 0x81028000;
            pIspReg->DIP_X_UDM_NR_ACT.Raw = 0x00000050;
            pIspReg->DIP_X_UDM_HF_STR.Raw = 0x00000000;
            pIspReg->DIP_X_UDM_HF_ACT1.Raw = 0x145034DC;
            pIspReg->DIP_X_UDM_HF_ACT2.Raw = 0x0034FF55;
            pIspReg->DIP_X_UDM_CLIP.Raw = 0x00DF2064;
            pIspReg->DIP_X_UDM_DSB.Raw = 0x007FA800|fgModeRegBit;
            pIspReg->DIP_X_UDM_TILE_EDGE.Raw = 0x0000000F;
            pIspReg->DIP_X_UDM_DSL.Raw = 0x00000000;
            //pIspReg->DIP_X_UDM_SPARE_1.Raw = 0x00000000;
            pIspReg->DIP_X_UDM_SPARE_2.Raw = 0x00000000;
            pIspReg->DIP_X_UDM_SPARE_3.Raw = 0x00000000;
            pIspReg->DIP_X_CTL_RGB_EN.Bits.UDM_EN = 1;
            break;
        default:
            break;
    }
}
};


