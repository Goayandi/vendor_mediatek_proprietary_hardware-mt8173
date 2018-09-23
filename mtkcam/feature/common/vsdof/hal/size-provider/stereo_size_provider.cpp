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
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/
#define LOG_TAG "StereoSizeProvider"

#include <math.h>
//
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include "pass2/pass2A_size_providers.h"

using android::Mutex;
#define LOG_TAG "StereoSizeProvider"

#define STEREO_SIZE_PROVIDER_DEBUG

#ifdef STEREO_SIZE_PROVIDER_DEBUG    // Enable debug log.

#undef __func__
#define __func__ __FUNCTION__

#ifndef GTEST
#define MY_LOGD(fmt, arg...)    CAM_LOGD("[%s]" fmt, __func__, ##arg)
#define MY_LOGI(fmt, arg...)    CAM_LOGI("[%s]" fmt, __func__, ##arg)
#define MY_LOGW(fmt, arg...)    CAM_LOGW("[%s] WRN(%5d):" fmt, __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    CAM_LOGE("[%s] %s ERROR(%5d):" fmt, __func__,__FILE__, __LINE__, ##arg)
#else
#define MY_LOGD(fmt, arg...)    printf("[D][%s]" fmt"\n", __func__, ##arg)
#define MY_LOGI(fmt, arg...)    printf("[I][%s]" fmt"\n", __func__, ##arg)
#define MY_LOGW(fmt, arg...)    printf("[W][%s] WRN(%5d):" fmt"\n", __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    printf("[E][%s] %s ERROR(%5d):" fmt"\n", __func__,__FILE__, __LINE__, ##arg)
#endif

#else   // Disable debug log.
#define MY_LOGD(a,...)
#define MY_LOGI(a,...)
#define MY_LOGW(a,...)
#define MY_LOGE(a,...)
#endif  // STEREO_SIZE_PROVIDER_DEBUG

#include <mtkcam/utils/std/Log.h>

//===============================================================
//  Singleton and init operations
//===============================================================
Mutex StereoSizeProvider::mLock;

StereoSizeProvider *
StereoSizeProvider::getInstance()
{
    Mutex::Autolock lock(mLock);
    static StereoSizeProvider _instance;
    return &_instance;
}

StereoSizeProvider::StereoSizeProvider()
{
    __updateCaptureImageSize();
}

bool
StereoSizeProvider::getPass1Size( ENUM_STEREO_SENSOR sensor,
                                  EImageFormat format,
                                  EPortIndex port,
                                  ENUM_STEREO_SCENARIO scenario,
                                  MRect &tgCropRect,
                                  MSize &outSize,
                                  MUINT32 &strideInBytes
                                ) const
{
    // Get sensor senario
    int sensorScenario = getSensorSenario(scenario);

    // Prepare sensor hal
    IHalSensorList* sensorList = MAKE_HalSensorList();
    if(NULL == sensorList) {
        MY_LOGE("Cannot get sensor list");
        return false;
    }

    MINT32 err = 0;
    int32_t main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);
    int sendorDevIndex = sensorList->querySensorDevIdx((eSTEREO_SENSOR_MAIN1 == sensor) ? main1Idx : main2Idx);
    IHalSensor* pIHalSensor = sensorList->createSensor(LOG_TAG, (eSTEREO_SENSOR_MAIN1 == sensor) ? main1Idx : main2Idx);
    if(NULL == pIHalSensor) {
        MY_LOGE("Cannot get hal sensor");
        return false;
    }

    SensorStaticInfo sensorStaticInfo;
    memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
    sensorList->querySensorStaticInfo(sendorDevIndex, &sensorStaticInfo);

    StereoArea result;
    tgCropRect.p.x = 0;
    tgCropRect.p.y = 0;
    tgCropRect.s.w = (scenario == eSTEREO_SCENARIO_CAPTURE) ? sensorStaticInfo.captureWidth  : sensorStaticInfo.previewWidth;
    tgCropRect.s.h = (scenario == eSTEREO_SCENARIO_CAPTURE) ? sensorStaticInfo.captureHeight : sensorStaticInfo.previewHeight;
    outSize.w = tgCropRect.s.w;
    outSize.h = tgCropRect.s.h;

    if(EPortIndex_RRZO == port) {
        outSize.w = sensorStaticInfo.previewWidth;
        outSize.h = sensorStaticInfo.previewHeight;
    }

    ENUM_STEREO_RATIO currentRatio = (EPortIndex_IMGO == port) ? eRatio_4_3 : StereoSettingProvider::imageRatio();
    if(eRatio_Sensor != currentRatio) {
        int n = 1, m = 1;   // h = w * n/m
        switch(currentRatio) {
            case eRatio_16_9:
            default:
                n = 9;
                m = 16;
                break;
            case eRatio_4_3:
                n = 3;
                m = 4;
                break;
        }

        outSize.h = (outSize.w * n / m) & ~1;   //Size must be even
        int height = tgCropRect.s.h;
        tgCropRect.s.h = (tgCropRect.s.w * n / m) & ~1; //Size must be even
        if(height < tgCropRect.s.h) {
            tgCropRect.s.h = height;
            outSize.h      = height;
        } else {
            tgCropRect.p.y = (height - tgCropRect.s.h)/2;
        }
    }

    //Get FPS
    int defaultFPS = 0; //result will be 10xFPS, e.g. if 30 fps, defaultFPS = 300
    err = pIHalSensor->sendCommand(sendorDevIndex, SENSOR_CMD_GET_DEFAULT_FRAME_RATE_BY_SCENARIO,
                                   (MINTPTR)&sensorScenario, (MINTPTR)&defaultFPS, 0);
    if(err) {
        MY_LOGE("Cannot get default frame rate");
        pIHalSensor->destroyInstance(LOG_TAG);
        return false;
    }

    //Get pixel format
    MUINT32 pixelMode;
    defaultFPS /= 10;
    err = pIHalSensor->sendCommand(sendorDevIndex, SENSOR_CMD_GET_SENSOR_PIXELMODE,
                                   (MINTPTR)&sensorScenario, (MINTPTR)&defaultFPS, (MINTPTR)&pixelMode);
    if(err) {
        MY_LOGE("Cannot get pixel mode");
        pIHalSensor->destroyInstance(LOG_TAG);
        return false;
    }

    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo queryRst;
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryIn input;
    input.width = outSize.w;
    input.pixMode = (pixelMode == 0x0)? (NSCam::NSIoPipe::NSCamIOPipe::_1_PIX_MODE) : NSCam::NSIoPipe::NSCamIOPipe::_2_PIX_MODE;
    NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
            port,
            NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX|
            NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_PIX|
            NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE,
            format,
            input,
            queryRst);

    strideInBytes = queryRst.stride_byte;

    tgCropRect.p.x = (outSize.w - queryRst.x_pix)/2 * tgCropRect.s.w / outSize.w;
    outSize.w = queryRst.x_pix;

    //Precrop main1 image if target FOV is < sensor FOV
    if(eSTEREO_SENSOR_MAIN1 == sensor &&
       EPortIndex_RRZO == port )
    {
        const float CROP_RATIO = StereoSettingProvider::getFOVCropRatio();
        if(CROP_RATIO < 1.0f &&
           CROP_RATIO > 0.0f)
        {
            // MY_LOGD("TG (%d, %d) %dx%d",
            //         tgCropRect.p.x, tgCropRect.p.y, tgCropRect.s.w, tgCropRect.s.h);
            MSize tgSize = tgCropRect.s;
            tgCropRect.s.w = (int)(tgCropRect.s.w * CROP_RATIO) & ~1;
            tgCropRect.s.h = (int)(tgCropRect.s.h * CROP_RATIO) & ~1;
            tgCropRect.p.x += (tgSize.w - tgCropRect.s.w)/2;
            tgCropRect.p.y += (tgSize.h - tgCropRect.s.h)/2;
            MY_LOGD("Crop TG to ratio %.2f, (%d, %d) %dx%d",
                    CROP_RATIO, tgCropRect.p.x, tgCropRect.p.y, tgCropRect.s.w, tgCropRect.s.h);
        }
    }

    pIHalSensor->destroyInstance(LOG_TAG);
    return true;
}

template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata const* pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if (pMetadata == NULL) {
        MY_LOGW("pMetadata == NULL");
        return MFALSE;
    }
    //
    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if(!entry.isEmpty()) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    //
    return MFALSE;
}

bool
StereoSizeProvider::getPass1ActiveArrayCrop(ENUM_STEREO_SENSOR sensor, MRect &cropRect)
{
    int main1Id, main2Id;
    StereoSettingProvider::getStereoSensorIndex(main1Id, main2Id);
    int sensorId = -1;
    switch(sensor)
    {
        case eSTEREO_SENSOR_MAIN1:
            sensorId = main1Id;
            break;
        case eSTEREO_SENSOR_MAIN2:
            sensorId = main2Id;
            break;
        default:
            break;
    }

    if(sensorId < 0) {
        MY_LOGW("Wrong sensor: %d", sensorId);
        return false;
    }

    sp<IMetadataProvider> pMetadataProvider = NSMetadataProviderManager::valueFor(sensorId);
    if( ! pMetadataProvider.get() ) {
        MY_LOGE("MetadataProvider is NULL");
        return false;
    }

    bool result = false;
    IMetadata static_meta = pMetadataProvider->geMtktStaticCharacteristics();
    if( tryGetMetadata<MRect>(&static_meta, MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION, cropRect) ) {
        cropRect.s.w &= ~1;
        cropRect.s.h &= ~1;
        if(eRatio_16_9 == StereoSettingProvider::imageRatio()) {
            int newHeight = cropRect.s.h;
            newHeight = (cropRect.s.w * 9 / 16) & ~1;
            cropRect.p.y = (cropRect.s.h-newHeight)/2;
            cropRect.s.h = newHeight;
        }

        result = true;
    }

    return result;
}

bool
StereoSizeProvider::getPass2SizeInfo(ENUM_PASS2_ROUND round, ENUM_STEREO_SCENARIO eScenario, Pass2SizeInfo &pass2SizeInfo)
{
    bool isSuccess = true;
    switch(round) {
        case PASS2A:
            pass2SizeInfo = Pass2A_SizeProvider::instance()->sizeInfo(eScenario);
            if(eSTEREO_SCENARIO_CAPTURE == eScenario) {
                pass2SizeInfo.areaWDMA.size = m_captureSize;
            }
            break;
        case PASS2A_2:
            pass2SizeInfo = Pass2A_2_SizeProvider::instance()->sizeInfo(eScenario);
            break;
        case PASS2A_3:
            pass2SizeInfo = Pass2A_3_SizeProvider::instance()->sizeInfo(eScenario);
            break;
        case PASS2A_P:
            pass2SizeInfo = Pass2A_P_SizeProvider::instance()->sizeInfo(eScenario);
            break;
        case PASS2A_P_2:
            pass2SizeInfo = Pass2A_P_2_SizeProvider::instance()->sizeInfo(eScenario);
            break;
        case PASS2A_P_3:
            pass2SizeInfo = Pass2A_P_3_SizeProvider::instance()->sizeInfo(eScenario);
            break;
        default:
            isSuccess = false;
    }

    return isSuccess;
}

StereoArea
StereoSizeProvider::getBufferSize(ENUM_BUFFER_NAME eName, ENUM_STEREO_SCENARIO eScenario) const
{
    const bool IS_DENOISE = (StereoSettingProvider::isDeNoise() && eScenario == eSTEREO_SCENARIO_CAPTURE);

    switch(eName) {
        //N3D before MDP for capture
        case E_MV_Y_LARGE:
        case E_MASK_M_Y_LARGE:
        case E_SV_Y_LARGE:
        case E_MASK_S_Y_LARGE:
            return StereoArea(MSize(2176, 1144), MSize(256, 64), MPoint(128, 32))
                   .applyRatio(StereoSettingProvider::imageRatio(), E_KEEP_HEIGHT)
                   .rotatedByModule()
                   .apply16Align();
            break;

        //N3D Output
        case E_MV_Y:
        case E_MASK_M_Y:
        case E_SV_Y:
        case E_MASK_S_Y:

        //DPE Output
        case E_DMP_H:
        case E_CFM_H:
        case E_RESPO:
        case E_LDC:
            {
                StereoArea area(MSize(272, 144), MSize(32,  8), MPoint(16, 4)); //4:3 -> 208+24 x 156+6
                if(IS_DENOISE) {
                    area *= 2.0f;
                }

                area
                .applyRatio(StereoSettingProvider::imageRatio())
                .rotatedByModule()
                .apply8AlignToContentWidth()
                .apply2Align();

                return area;
            }
            break;

        //OCC Output
        case E_MY_S:
        case E_DMH:

        //WMF Output
        case E_DMW:
            return StereoArea(240, 136)
                   .applyRatio(StereoSettingProvider::imageRatio())
                   .rotatedByModule()
                   .apply8AlignToContentWidth()
                   .apply2Align();
            break;

        //GF Output
        case E_DMG:
        case E_DMBG:
            return StereoArea(240, 136)
                   .applyRatio(StereoSettingProvider::imageRatio())
                   .apply2Align();
            break;

        case E_DEPTH_MAP:
            {
                StereoArea area1x = getInstance()->getBufferSize(E_DMBG);
                switch(CUSTOM_DEPTHMAP_SIZE) {
                case STEREO_DEPTHMAP_1X:
                    return area1x;
                    break;
                case STEREO_DEPTHMAP_2X:
                default:
                    return area1x * 2;
                    break;
                case STEREO_DEPTHMAP_4X:
                    return area1x * 4;
                    break;
                }
            }
            break;

        //Bokeh Output
        case E_BOKEH_WROT: //Saved image
            switch(eScenario) {
                case eSTEREO_SCENARIO_PREVIEW:
                    return STEREO_AREA_ZERO;
                    break;
                case eSTEREO_SCENARIO_RECORD:   //1920x1088 (16-align)
                    {
                        Pass2SizeInfo pass2SizeInfo;
                        getInstance()->getPass2SizeInfo(PASS2A, eScenario, pass2SizeInfo);
                        return pass2SizeInfo.areaWDMA;
                    }
                    break;
                case eSTEREO_SCENARIO_CAPTURE:  //3072x1728
                    {
                        return StereoArea(m_captureSize);
                    }
                    break;
                default:
                    break;
            }
            break;
        case E_BOKEH_WDMA:
            switch(eScenario) {
                case eSTEREO_SCENARIO_PREVIEW:  //Display
                case eSTEREO_SCENARIO_RECORD:
                    return (eRatio_16_9 == StereoSettingProvider::imageRatio()) ? StereoArea(1920, 1080) : StereoArea(1440, 1080);
                    break;
                case eSTEREO_SCENARIO_CAPTURE:  //Clean image
                    {
                        return StereoArea(m_captureSize);
                    }
                    break;
                default:
                    break;
            }
            break;
        case E_BOKEH_3DNR:
            switch(eScenario) {
                case eSTEREO_SCENARIO_PREVIEW:
                case eSTEREO_SCENARIO_RECORD:
                    {
                        Pass2SizeInfo pass2SizeInfo;
                        getInstance()->getPass2SizeInfo(PASS2A, eScenario, pass2SizeInfo);
                        return pass2SizeInfo.areaWDMA;
                    }
                    break;
                default:
                    break;
            }
            break;
        case E_BM_PREPROCESS_FULLRAW_CROP_1:
            return StereoArea(m_BMDeNoiseFullRawCropSize_main1.s.w, m_BMDeNoiseFullRawCropSize_main1.s.h,0,0,m_BMDeNoiseFullRawCropSize_main1.p.x,m_BMDeNoiseFullRawCropSize_main1.p.y);
        case E_BM_PREPROCESS_FULLRAW_CROP_2:
            return StereoArea(m_BMDeNoiseFullRawCropSize_main2.s.w, m_BMDeNoiseFullRawCropSize_main2.s.h,0,0,m_BMDeNoiseFullRawCropSize_main2.p.x,m_BMDeNoiseFullRawCropSize_main2.p.y);
        case E_BM_PREPROCESS_MFBO_1:
            return StereoArea(m_BMDeNoiseFullRawSize_main1);
        case E_BM_PREPROCESS_MFBO_2:
            return StereoArea(m_BMDeNoiseFullRawSize_main2);
        case E_BM_PREPROCESS_MFBO_FINAL_1:
            return StereoArea(m_BMDeNoiseAlgoSize_main1)
                   .rotatedByModule(false)
                   .addPaddingToHeight(0.5);// padded for ALG
        case E_BM_PREPROCESS_MFBO_FINAL_2:
            return StereoArea(m_BMDeNoiseAlgoSize_main2)
                   .rotatedByModule(false)
                   .addPaddingToHeight(0.5);// padded for ALG
        case E_BM_PREPROCESS_W_1:
            return StereoArea(m_BMDeNoiseAlgoSize_main1)
                   .rotatedByModule(false)
                   .addPaddingToHeight(3.0);// padded for ALG
        case E_BM_PREPROCESS_W_2:
            return StereoArea(m_BMDeNoiseAlgoSize_main2)
                   .rotatedByModule(false)
                   .addPaddingToHeight(1.0);// padded for ALG
        case E_BM_DENOISE_HAL_OUT:
            return StereoArea(m_BMDeNoiseAlgoSize_main1)
                   .rotatedByModule(false)
                   .addPaddingToHeight(0.5);// padded for ALG
        case E_BM_DENOISE_HAL_OUT_ROT_BACK:
            return StereoArea(m_BMDeNoiseAlgoSize_main1);
        default:
            break;
    }

    return STEREO_AREA_ZERO;
}

MSize
StereoSizeProvider::getSBSImageSize()
{
    Pass2SizeInfo pass2SizeInfo;
    getPass2SizeInfo(PASS2A_2, eSTEREO_SCENARIO_CAPTURE, pass2SizeInfo);
    MSize result = pass2SizeInfo.areaWDMA.size;
    result.w *= 2;

    return result;
}

bool
StereoSizeProvider::__updateBMDeNoiseSizes()
{
    int main1Id = -1, main2Id = -1;
    IHalSensorList* sensorList = nullptr;

    StereoSettingProvider::getStereoSensorIndex(main1Id, main2Id);

    // Prepare sensor hal
    sensorList = MAKE_HalSensorList();
    if(NULL == sensorList) {
        MY_LOGE("Cannot get sensor list");
        return false;
    }

    // main1 full size
    {
        int sendorDevIndex = sensorList->querySensorDevIdx(main1Id);

        SensorStaticInfo sensorStaticInfo;
        memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
        sensorList->querySensorStaticInfo(sendorDevIndex, &sensorStaticInfo);

        MSize main1_full_size = MSize(sensorStaticInfo.captureWidth, sensorStaticInfo.captureHeight);

        MRect crop;
        __getCenterCrop(main1_full_size, crop);

        // dont know how to use?
        // NSImageio::NSIspio::ISP_QUERY_RST queryRst;
        // NSImageio::NSIspio::ISP_QuerySize( _BY_PASSS_PORT,
        //                                     ISP_QUERY_CROP_START_X|ISP_QUERY_CROP_X_PIX|ISP_QUERY_CROP_X_BYTE
        //                                    eImgFmt_BAYER10,
        //                                    outSize.w,
        //                                    queryRst,
        //                                    pixelMode
        //                              );

        m_BMDeNoiseFullRawSize_main1 = main1_full_size;
        m_BMDeNoiseFullRawCropSize_main1 = crop;
        m_BMDeNoiseAlgoSize_main1 = crop.s;
    }

    // main2 full size
    {
        int sendorDevIndex = sensorList->querySensorDevIdx(main2Id);

        SensorStaticInfo sensorStaticInfo;
        memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
        sensorList->querySensorStaticInfo(sendorDevIndex, &sensorStaticInfo);

        MSize main2_full_size = MSize(sensorStaticInfo.captureWidth, sensorStaticInfo.captureHeight);

        MRect crop;
        __getCenterCrop(main2_full_size, crop);

        // dont know how to use?
        // NSImageio::NSIspio::ISP_QUERY_RST queryRst;
        // NSImageio::NSIspio::ISP_QuerySize( _BY_PASSS_PORT,
        //                                     ISP_QUERY_CROP_START_X|ISP_QUERY_CROP_X_PIX|ISP_QUERY_CROP_X_BYTE
        //                                    eImgFmt_BAYER10,
        //                                    outSize.w,
        //                                    queryRst,
        //                                    pixelMode
        //                              );

        m_BMDeNoiseFullRawSize_main2 = main2_full_size;
        m_BMDeNoiseFullRawCropSize_main2 = crop;
        m_BMDeNoiseAlgoSize_main2 = crop.s;
    }

    return true;
}
/*******************************************************************************
 *
 ********************************************************************************/
bool
StereoSizeProvider::__getCenterCrop(MSize &srcSize, MRect &rCrop )
{
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
        rCrop.p = MPoint(0,0);
        rCrop.s = srcSize;
    }
    else
    {
        rCrop.p.x = 0;
        rCrop.p.y = (srcSize.h - iHeight)/2;
        rCrop.s.w = srcSize.w;
        rCrop.s.h = iHeight;
    }

    // MY_LOGD("srcSize:(%d,%c) ratio:%d, rCropStartPt:(%d, %d) rCropSize:(%d,%d)",
    //         srcSize.w, srcSize.h, StereoSettingProvider::imageRatio(),
    //         rCrop.p.x, rCrop.p.y, rCrop.s.w, rCrop.s.h);

    return MTRUE;
}

void
StereoSizeProvider::__updateCaptureImageSize()
{
    m_captureSize = Pass2A_SizeProvider::instance()->sizeInfo(eSTEREO_SCENARIO_CAPTURE).areaWDMA.contentSize();
}