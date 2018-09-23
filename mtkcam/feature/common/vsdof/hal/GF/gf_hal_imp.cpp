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
#define LOG_TAG "GF_HAL"

#include "gf_hal_imp.h"
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <vsdof/hal/stereo_tuning_provider.h>
#include <mtkcam/aaa/IHal3A.h>
#include <feature/common/vsdof/hal/inc/stereo_dp_util.h>
#include <vsdof/hal/ProfileUtil.h>
#include <vsdof/hal/AffinityUtil.h>

using namespace StereoHAL;

#define GF_HAL_DEBUG

#ifdef GF_HAL_DEBUG    // Enable debug log.

#if defined(__func__)
#undef __func__
#endif
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
#endif  // GF_HAL_DEBUG

#define MY_LOGV_IF(cond, arg...)    if (cond) { MY_LOGV(arg); }
#define MY_LOGD_IF(cond, arg...)    if (cond) { MY_LOGD(arg); }
#define MY_LOGI_IF(cond, arg...)    if (cond) { MY_LOGI(arg); }
#define MY_LOGW_IF(cond, arg...)    if (cond) { MY_LOGW(arg); }
#define MY_LOGE_IF(cond, arg...)    if (cond) { MY_LOGE(arg); }
    
#include <mtkcam/utils/std/Log.h>    

#define PERPERTY_IGNORE_AF_DONE   STEREO_PROPERTY_PREFIX"gf_ignore_af"

MPoint GF_HAL_IMP::m_lastFocusPoint = MPoint(120, 68);

GF_HAL *
GF_HAL::createInstance(ENUM_STEREO_SCENARIO eScenario)
{
    return new GF_HAL_IMP(eScenario);
}

GF_HAL_IMP::GF_HAL_IMP(ENUM_STEREO_SCENARIO eScenario)
    : m_pAFTable(NULL)
    , m_isAFSupported(true)
    , LOG_ENABLED(StereoSettingProvider::isLogEnabled(PERPERTY_BOKEH_NODE_LOG))
    , BENCHMARK_ENABLED(StereoSettingProvider::isProfileLogEnabled())
    , m_isAFTrigger(true)
{
    const char *HAL3A_QUERY_NAME = "MTKStereoCamera";
    int32_t main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);
    IHal3A *pHal3A = MAKE_Hal3A(main1Idx, HAL3A_QUERY_NAME);
    if(NULL == pHal3A) {
        MY_LOGE("Cannot get 3A HAL");
    } else {
        pHal3A->send3ACtrl(NS3Av3::E3ACtrl_GetAFDAFTable, (MUINTPTR)&m_pAFTable, 0);
        if(NULL == m_pAFTable) {
            MY_LOGE("Cannot get AF table");
        }

        FeatureParam_T rFeatureParam;
        if(pHal3A->send3ACtrl(NS3Av3::E3ACtrl_GetSupportedInfo, (MUINTPTR)&rFeatureParam, 0)) {
            m_isAFSupported = (rFeatureParam.u4MaxFocusAreaNum > 0);
        }

        pHal3A->destroyInstance(HAL3A_QUERY_NAME);
    }

    _initAFWinTransform();
    //
    m_pGfDrv = MTKGF::createInstance(DRV_GF_OBJ_SW);
    //Init GF
    ::memset(&m_initInfo, 0, sizeof(GFInitInfo));
    //=== Init sizes ===
    MSize inputSize = StereoSizeProvider::getInstance()->getBufferSize(E_MY_S, eScenario);
    m_initInfo.inputWidth  = inputSize.w;
    m_initInfo.inputHeight = inputSize.h;
    //
    MSize outputSize = StereoSizeProvider::getInstance()->getBufferSize(E_DMBG, eScenario);
    m_initInfo.outputWidth  = outputSize.w;
    m_initInfo.outputHeight = outputSize.h;

    //=== Init tuning info ===
    m_initInfo.pTuningInfo = new GFTuningInfo();
    m_initInfo.pTuningInfo->coreNumber = 1;
    std::vector<MINT32> tuningTable;
    StereoTuningProvider::getGFTuningInfo(tuningTable);
    m_initInfo.pTuningInfo->clrTblSize = tuningTable.size();
    m_initInfo.pTuningInfo->clrTbl     = NULL;
    if(m_initInfo.pTuningInfo->clrTblSize > 0) {
        //GF will copy this array, so we do not need to delete m_initInfo.pTuningInfo->clrTbl,
        //since tuningTable is a local variable
        m_initInfo.pTuningInfo->clrTbl = &tuningTable[0];
    }

    if(eScenario != eSTEREO_SCENARIO_CAPTURE) {
        m_initInfo.gfMode = GF_MODE_VR;
    } else {
        switch(CUSTOM_DEPTHMAP_SIZE) {     //Defined in camera_custom_stereo.cpp
        case STEREO_DEPTHMAP_1X:
        default:
            m_initInfo.gfMode = GF_MODE_CP;
            break;
        case STEREO_DEPTHMAP_2X:
            m_initInfo.gfMode = GF_MODE_CP_2X;
            break;
        case STEREO_DEPTHMAP_4X:
            m_initInfo.gfMode = GF_MODE_CP_4X;
            break;
        }
    }

    m_pGfDrv->Init((void *)&m_initInfo, NULL);
    //Get working buffer size
    m_pGfDrv->FeatureCtrl(GF_FEATURE_GET_WORKBUF_SIZE, NULL, &m_initInfo.workingBuffSize);

    //Allocate working buffer and set to GF
    if(m_initInfo.workingBuffSize > 0) {
        m_initInfo.workingBuffAddr = new(std::nothrow) MUINT8[m_initInfo.workingBuffSize];
        if(m_initInfo.workingBuffAddr) {
            MY_LOGD_IF(LOG_ENABLED, "Alloc %d bytes for GF working buffer", m_initInfo.workingBuffSize);
            m_pGfDrv->FeatureCtrl(GF_FEATURE_SET_WORKBUF_ADDR, &m_initInfo, NULL);
        } else {
            MY_LOGE("Cannot create GF working buffer of size %d", m_initInfo.workingBuffSize);
        }
    }

    _dumpInitData();
};

GF_HAL_IMP::~GF_HAL_IMP()
{
    //Delete working buffer
    if(m_initInfo.pTuningInfo) {
        delete m_initInfo.pTuningInfo;
        m_initInfo.pTuningInfo = NULL;
    }

    if(m_initInfo.workingBuffAddr) {
        delete [] m_initInfo.workingBuffAddr;
        m_initInfo.workingBuffAddr = NULL;
    }

    _clearTransformedImages();

    if(m_pGfDrv) {
        m_pGfDrv->Reset();
        m_pGfDrv->destroyInstance(m_pGfDrv);
        m_pGfDrv = NULL;
    }
}

bool
GF_HAL_IMP::GFHALRun(GF_HAL_IN_DATA &inData, GF_HAL_OUT_DATA &outData)
{
    if(NULL == inData.depthMap) {
        if(outData.dmbg) {
            MSize szDMBG = StereoSizeProvider::getInstance()->getBufferSize(E_DMBG, inData.scenario);
            ::memset(outData.dmbg, 0, szDMBG.w*szDMBG.h);
        }

        if(outData.depthMap) {
            MSize szDepthMap = StereoSizeProvider::getInstance()->getBufferSize(E_DEPTH_MAP, inData.scenario);
            ::memset(outData.depthMap, 0, szDepthMap.w*szDepthMap.h);
        }

        return true;
    }

    AutoProfileUtil profile(LOG_TAG, "GFHALRun");
    if(eSTEREO_SCENARIO_CAPTURE == inData.scenario) {
        CPUMask cpuMask(CPUCoreB, 2);
        AutoCPUAffinity affinity(cpuMask);

        _setGFParams(inData);
        _runGF(outData);
    } else {
        _setGFParams(inData);
        _runGF(outData);
    }

    return true;
}

void
GF_HAL_IMP::_initAFWinTransform()
{
    MINT32 err = 0;
    int main1SensorIndex, main2SensorIndex;
    StereoSettingProvider::getStereoSensorIndex(main1SensorIndex, main2SensorIndex);

    int main1SensorDevIndex, main2SensorDevIndex;
    StereoSettingProvider::getStereoSensorDevIndex(main1SensorDevIndex, main2SensorDevIndex);

    IHalSensorList* sensorList = MAKE_HalSensorList();
    IHalSensor* pIHalSensor = NULL;

    if(NULL == sensorList) {
        MY_LOGE("Cannot get sensor list");
    } else {
        MUINT32 junkStride;
        //========= Get main1 size =========
        IHalSensor* pIHalSensor = sensorList->createSensor(LOG_TAG, main1SensorIndex);
        if(NULL == pIHalSensor) {
            MY_LOGE("Cannot get hal sensor of main1");
            err = 1;
        } else {
            SensorCropWinInfo   sensorCropInfoCaptureZSD;
            SensorCropWinInfo   sensorCropInfoPreviewVR;

            ::memset(&sensorCropInfoCaptureZSD, 0, sizeof(SensorCropWinInfo));
            int sensorScenario = getSensorSenario(eSTEREO_SCENARIO_CAPTURE);
            err = pIHalSensor->sendCommand(main1SensorDevIndex, SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO,
                                           (MUINTPTR)&sensorScenario, (MUINTPTR)&sensorCropInfoCaptureZSD, 0);

            if(err) {
                MY_LOGE("Cannot get sensor crop info for capture/ZSD");
            }

            ::memset(&sensorCropInfoPreviewVR, 0, sizeof(SensorCropWinInfo));
            sensorScenario = getSensorSenario(eSTEREO_SCENARIO_RECORD);
            err = pIHalSensor->sendCommand(main1SensorDevIndex, SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO,
                                           (MUINTPTR)&sensorScenario, (MUINTPTR)&sensorCropInfoPreviewVR, 0);
            if(err) {
                MY_LOGE("Cannot get sensor crop info for preview/record");
            } else {
                MSize szDMBG = StereoSizeProvider::getInstance()->getBufferSize(E_DMBG);
                switch(StereoSettingProvider::imageRatio()) {
                    case eRatio_4_3:
                        {
                            m_afOffsetX[0] = sensorCropInfoPreviewVR.x0_offset;
                            m_afOffsetY[0] = sensorCropInfoPreviewVR.y0_offset;
                            m_afScaleW[0] = (float)szDMBG.w / sensorCropInfoPreviewVR.scale_w;
                            m_afScaleH[0] = (float)szDMBG.h / sensorCropInfoPreviewVR.scale_h;

                            m_afOffsetX[1] = sensorCropInfoCaptureZSD.x0_offset;
                            m_afOffsetY[1] = sensorCropInfoCaptureZSD.y0_offset;
                            m_afScaleW[1] = (float)szDMBG.w / sensorCropInfoCaptureZSD.scale_w;
                            m_afScaleH[1] = (float)szDMBG.h / sensorCropInfoCaptureZSD.scale_h;
                        }
                        break;
                    case eRatio_16_9:
                    default:
                        {
                            //4:3->16:9
                            m_afOffsetX[0] = sensorCropInfoPreviewVR.x0_offset;
                            m_afOffsetY[0] = sensorCropInfoPreviewVR.y0_offset + sensorCropInfoPreviewVR.scale_h/8;
                            m_afScaleW[0] = (float)szDMBG.w / sensorCropInfoPreviewVR.scale_w;
                            m_afScaleH[0] = (float)szDMBG.h / (sensorCropInfoPreviewVR.scale_h * 3 / 4);

                            m_afOffsetX[1] = sensorCropInfoCaptureZSD.x0_offset;
                            m_afOffsetY[1] = sensorCropInfoCaptureZSD.y0_offset + sensorCropInfoCaptureZSD.scale_h/8;
                            m_afScaleW[1] = (float)szDMBG.w / sensorCropInfoCaptureZSD.scale_w;
                            m_afScaleH[1] = (float)szDMBG.h / (sensorCropInfoCaptureZSD.scale_h * 3 / 4);
                        }
                        break;
                }

                MY_LOGD_IF(LOG_ENABLED, "AF Transform: offset0(%d, %d), scale0(%f, %f), offset1(%d, %d), scale1(%f, %f)",
                           m_afOffsetX[0], m_afOffsetY[0], m_afScaleW[0], m_afScaleH[0],
                           m_afOffsetX[1], m_afOffsetY[1], m_afScaleW[1], m_afScaleH[1]);
            }
            pIHalSensor->destroyInstance(LOG_TAG);
        }
    }
}

MPoint
GF_HAL_IMP::_getAFPoint(const int AF_INDEX, ENUM_STEREO_SCENARIO scenario)
{
    DAF_VEC_STRUCT *afVec = &m_pAFTable->daf_vec[AF_INDEX];
    MPoint afPoint( (afVec->af_win_start_x + afVec->af_win_end_x)/2,
                    (afVec->af_win_start_y + afVec->af_win_end_y)/2 );
    MY_LOGD_IF(LOG_ENABLED, "AF point(original): (%d, %d)", afPoint.x, afPoint.y);

    if(scenario == eSTEREO_SCENARIO_RECORD)
    {
        return MPoint( (afPoint.x - m_afOffsetX[0]) * m_afScaleW[0] ,
                       (afPoint.y - m_afOffsetY[0]) * m_afScaleH[0] );
    } else {    //Preview is in ZSD mode
        return MPoint( (afPoint.x - m_afOffsetX[1]) * m_afScaleW[1] ,
                       (afPoint.y - m_afOffsetY[1]) * m_afScaleH[1] );
    }
}

MPoint
GF_HAL_IMP::_getTouchPoint(MPoint ptIn, ENUM_STEREO_SCENARIO scenario)
{
    MPoint ptResult;

    //TODO: modify for 4:3
    MSize szDMBG = StereoSizeProvider::getInstance()->getBufferSize(E_DMBG);
    ptResult.x = (ptIn.x + 1000.0f) / 2000.0f * szDMBG.w;
    ptResult.y = (ptIn.y + 1000.0f) / 2000.0f * szDMBG.h;

    return ptResult;
}

void
GF_HAL_IMP::_setGFParams(GF_HAL_IN_DATA &gfHalParam)
{
    AutoProfileUtil profile(LOG_TAG, "Set Proc");

    const int AF_INDEX = gfHalParam.magicNumber % DAF_TBL_QLEN;
    const bool IS_AF_STABLE = m_pAFTable->daf_vec[AF_INDEX].is_af_stable;
    m_procInfo.cOffset = gfHalParam.convOffset;
    m_procInfo.touchTrigger = false;
    MPoint focusPoint;
    if(m_isAFSupported) {
        if(IS_AF_STABLE) {
            focusPoint = _getAFPoint(AF_INDEX, gfHalParam.scenario);
        }
    } else {
        focusPoint = _getTouchPoint(MPoint(gfHalParam.touchPosX, gfHalParam.touchPosY), gfHalParam.scenario);
    }

    //Rotate point according to module rotation
    MSize szDMBG = StereoSizeProvider::getInstance()->getBufferSize(E_DMBG);
    MPoint newFocusPoint = focusPoint;
    switch(StereoSettingProvider::getModuleRotation()) {
    case eRotate_90:
        newFocusPoint.x = szDMBG.h - focusPoint.y;
        newFocusPoint.y = focusPoint.x;
        break;
    case eRotate_180:
        newFocusPoint.x = szDMBG.w - focusPoint.x;
        newFocusPoint.y = szDMBG.h - focusPoint.y;
        break;
    case eRotate_270:
        newFocusPoint.x = focusPoint.y;
        newFocusPoint.y = szDMBG.w - focusPoint.x;
        break;
    default:
        break;
    }

    if(newFocusPoint.x < 0) {
        MY_LOGW("Invalid AF point: (%d, %d)", newFocusPoint.x, newFocusPoint.y);
        newFocusPoint.x = 0;
    } else if(newFocusPoint.x >= szDMBG.w) {
        MY_LOGW("Invalid AF point: (%d, %d)", newFocusPoint.x, newFocusPoint.y);
        newFocusPoint.x = szDMBG.w - 1;
    }

    if(newFocusPoint.y < 0) {
        MY_LOGW("Invalid AF point: (%d, %d)", newFocusPoint.x, newFocusPoint.y);
        newFocusPoint.y = 0;
    } else if(newFocusPoint.y >= szDMBG.h) {
        MY_LOGW("Invalid AF point: (%d, %d)", newFocusPoint.x, newFocusPoint.y);
        newFocusPoint.y = szDMBG.h - 1;
    }

    if(newFocusPoint.x != m_lastFocusPoint.x) {
        m_lastFocusPoint.x = newFocusPoint.x;
        m_isAFTrigger = true;
    }

    if(newFocusPoint.y != m_lastFocusPoint.y) {
        m_lastFocusPoint.y = newFocusPoint.y;
        m_isAFTrigger = true;
    }

    if(gfHalParam.isCapture ||
       eSTEREO_SCENARIO_CAPTURE == gfHalParam.scenario)
    {
        m_isAFTrigger = true;
    }

    //GF should wait until AF is done
    if(checkStereoProperty(PERPERTY_IGNORE_AF_DONE) == 1) {
        m_procInfo.touchTrigger = m_isAFTrigger;
    } else {
        MY_LOGD_IF(LOG_ENABLED, "AF stable: %d, Magic # %d", m_pAFTable->daf_vec[AF_INDEX].is_af_stable, gfHalParam.magicNumber);
        if(!m_isAFSupported ||  //FF
           IS_AF_STABLE)        //AF
        {
            if(m_isAFTrigger) {
                m_isAFTrigger = false;

                m_procInfo.touchTrigger = true;
                m_procInfo.touchX = newFocusPoint.x;
                m_procInfo.touchY = newFocusPoint.y;
            }
        } else {
            m_procInfo.touchTrigger = false;
            m_isAFTrigger = true;   //Should be stable later, so we set it true
        }
    }

    MY_LOGD_IF((LOG_ENABLED && m_procInfo.touchTrigger),
               "AF point(DMBG): (%d, %d), scenario %d", newFocusPoint.x, newFocusPoint.y, gfHalParam.scenario);

    m_procInfo.dof = gfHalParam.dofLevel;
    m_procInfo.numOfBuffer = 1; //depthMap
    MSize size = StereoSizeProvider::getInstance()->getBufferSize(E_MY_S);
    m_procInfo.bufferInfo[0].type       = GF_BUFFER_TYPE_DEPTH;
    m_procInfo.bufferInfo[0].format     = GF_IMAGE_YONLY;
    m_procInfo.bufferInfo[0].width      = size.w;
    m_procInfo.bufferInfo[0].height     = size.h;
    m_procInfo.bufferInfo[0].planeAddr0 = (PEL*)gfHalParam.depthMap;
    m_procInfo.bufferInfo[0].planeAddr1 = NULL;
    m_procInfo.bufferInfo[0].planeAddr2 = NULL;
    m_procInfo.bufferInfo[0].planeAddr3 = NULL;

    std::vector<IImageBuffer *>::iterator it = gfHalParam.images.begin();
    IImageBuffer *image = NULL;
    for(;it != gfHalParam.images.end(); ++it, ++m_procInfo.numOfBuffer) {
        if((*it)->getImgFormat() == eImgFmt_YV12) {
            image = *it;
        } else {
            sp<IImageBuffer> transformedImage;
            StereoDpUtil::allocImageBuffer(LOG_TAG, eImgFmt_YV12, (*it)->getImgSize(), !IS_ALLOC_GB, transformedImage);
            StereoDpUtil::transformImage(*it, transformedImage.get());
            m_transformedImages.push_back(transformedImage);
            image = transformedImage.get();
        }

        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].format        = GF_IMAGE_YV12;
        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].width         = image->getImgSize().w;
        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].height        = image->getImgSize().h;
        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].planeAddr0    = (PEL*)image->getBufVA(0);
        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].planeAddr1    = (PEL*)image->getBufVA(1);
        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].planeAddr2    = (PEL*)image->getBufVA(2);
        m_procInfo.bufferInfo[m_procInfo.numOfBuffer].planeAddr3    = NULL;

        if(size.w * 2 == image->getImgSize().w) {
            m_procInfo.bufferInfo[m_procInfo.numOfBuffer].type      = GF_BUFFER_TYPE_DS_2X;
        } else if(size.w * 4 == image->getImgSize().w) {
            m_procInfo.bufferInfo[m_procInfo.numOfBuffer].type      = GF_BUFFER_TYPE_DS_4X;
        } else {
            m_procInfo.bufferInfo[m_procInfo.numOfBuffer].type      = GF_BUFFER_TYPE_DS;
        }
    }

    _dumpSetProcData();

    m_pGfDrv->FeatureCtrl(GF_FEATURE_SET_PROC_INFO, &m_procInfo, NULL);
}

void
GF_HAL_IMP::_runGF(GF_HAL_OUT_DATA &gfHalOutput)
{
    AutoProfileUtil profile(LOG_TAG, "Run GF");
    //================================
    //  Run GF
    //================================
    m_pGfDrv->Main();

    //================================
    //  Get result
    //================================
    ::memset(&m_resultInfo, 0, sizeof(GFResultInfo));
    {
        AutoProfileUtil profile(LOG_TAG, "Get Result");
        m_pGfDrv->FeatureCtrl(GF_FEATURE_GET_RESULT, NULL, &m_resultInfo);
    }
    _dumpGFResult();

    //================================
    //  Copy result to GF_HAL_OUT_DATA
    //================================
    for(MUINT32 i = 0; i < m_resultInfo.numOfBuffer; i++) {
        if(GF_BUFFER_TYPE_BMAP == m_resultInfo.bufferInfo[i].type) {
            if(gfHalOutput.dmbg) {
                AutoProfileUtil profile(LOG_TAG, "Copy BMAP");
                _rotateResult(E_DMBG, m_resultInfo.bufferInfo[i], gfHalOutput.dmbg);
            }
        } else if(GF_BUFFER_TYPE_DMAP == m_resultInfo.bufferInfo[i].type) {
            if(gfHalOutput.depthMap) {
                AutoProfileUtil profile(LOG_TAG, "Copy DepthMap");
                _rotateResult(E_DEPTH_MAP, m_resultInfo.bufferInfo[i], gfHalOutput.depthMap);
            }
        }
    }

    _clearTransformedImages();
}

void
GF_HAL_IMP::_clearTransformedImages()
{
    std::vector<sp<IImageBuffer>>::iterator it = m_transformedImages.begin();
    for(; it != m_transformedImages.end(); ++it) {
        if(it->get()) {
            StereoDpUtil::freeImageBuffer(LOG_TAG, *it);
        }
        *it = NULL;
    }

    m_transformedImages.clear();
}

bool
GF_HAL_IMP::_rotateResult(ENUM_BUFFER_NAME bufferName, GFBufferInfo &gfResult, MUINT8 *targetBuffer)
{
    MSize targetSize = StereoSizeProvider::getInstance()->getBufferSize(bufferName);
    if(NULL == targetBuffer ||
       NULL == gfResult.planeAddr0 ||
       gfResult.width*gfResult.height != targetSize.w * targetSize.h)
    {
        return false;
    }

    ENUM_ROTATION targetRotation = eRotate_0;
    switch(StereoSettingProvider::getModuleRotation())
    {
    case eRotate_90:
        targetRotation = eRotate_270;
        break;
    case  eRotate_270:
        targetRotation = eRotate_90;
        break;
    case eRotate_180:
        targetRotation = eRotate_180;
        break;
    case eRotate_0:
    default:
        break;
    }

    return StereoDpUtil::rotateBuffer(LOG_TAG,
                                      gfResult.planeAddr0,
                                      MSize(gfResult.width, gfResult.height),
                                      targetBuffer,
                                      targetRotation,
                                      false);//checkStereoProperty("GF_MDP", true, 1));
}

void
GF_HAL_IMP::_dumpInitData()
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("========= GF Init Info =========");
    //m_initInfo
    MY_LOGD("[GF Mode]      %d", m_initInfo.gfMode);
    MY_LOGD("[InputWidth]   %d", m_initInfo.inputWidth);
    MY_LOGD("[InputHeight]  %d", m_initInfo.inputHeight);
    //
    MY_LOGD("[OutputWidth]  %d", m_initInfo.outputWidth);
    MY_LOGD("[OutputHeight] %d", m_initInfo.outputHeight);
    //=== Init tuning info ===
    MY_LOGD("[CoreNumber]   %d", m_initInfo.pTuningInfo->coreNumber);
}

void
GF_HAL_IMP::_dumpSetProcData()
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("========= GF Proc Data =========");
    //m_procInfo
    MY_LOGD("[TouchTrigger] %d", m_procInfo.touchTrigger);
    MY_LOGD("[TouchPos]     (%d, %d)", m_procInfo.touchX, m_procInfo.touchY);
    MY_LOGD("[DoF Level]    %d", m_procInfo.dof);
//    MY_LOGD("[Depth Value]    %d", m_procInfo.depthValue);
    MY_LOGD("[ClearRange]   %d", m_procInfo.clearRange);
    MY_LOGD("[ConvOffset]   %f", m_procInfo.cOffset);

    for(unsigned int i = 0; i < m_procInfo.numOfBuffer; i++) {
        _dumpGFBufferInfo(m_procInfo.bufferInfo[i], (int)i);
    }
}

void
GF_HAL_IMP::_dumpGFResult()
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("========= GF Result =========");
    //m_resultInfo
    MY_LOGD("[Return code] %d", m_resultInfo.RetCode);
    for(unsigned int i = 0; i < m_resultInfo.numOfBuffer; i++) {
        _dumpGFBufferInfo(m_resultInfo.bufferInfo[i], (int)i);
    }
}

void
GF_HAL_IMP::_dumpGFBufferInfo(const GFBufferInfo &buf, int index)
{
    if(!LOG_ENABLED) {
        return;
    }

    if(index >= 0) {
        MY_LOGD("[Buffer %d][Type]          %d", index, buf.type);
        MY_LOGD("[Buffer %d][Format]        %d", index, buf.format);
        MY_LOGD("[Buffer %d][Width]         %d", index, buf.width);
        MY_LOGD("[Buffer %d][Height]        %d", index, buf.height);
        MY_LOGD("[Buffer %d][PlaneAddr0]    %p", index, buf.planeAddr0);
        MY_LOGD("[Buffer %d][PlaneAddr1]    %p", index, buf.planeAddr1);
        MY_LOGD("[Buffer %d][PlaneAddr2]    %p", index, buf.planeAddr2);
        MY_LOGD("[Buffer %d][PlaneAddr3]    %p", index, buf.planeAddr3);
    } else {
        MY_LOGD("[Type]          %d", buf.type);
        MY_LOGD("[Format]        %d", buf.format);
        MY_LOGD("[Width]         %d", buf.width);
        MY_LOGD("[Height]        %d", buf.height);
        MY_LOGD("[PlaneAddr0]    %p", buf.planeAddr0);
        MY_LOGD("[PlaneAddr1]    %p", buf.planeAddr1);
        MY_LOGD("[PlaneAddr2]    %p", buf.planeAddr2);
        MY_LOGD("[PlaneAddr3]    %p", buf.planeAddr3);
    }
}
