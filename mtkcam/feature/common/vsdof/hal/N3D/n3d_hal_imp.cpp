/*********************************************************************************************
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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "N3D_HAL"

#include <stdlib.h>     // for rand()
#include <bitset>


#include "n3d_hal_imp.h"         // For N3D_HAL class.
#include "../inc/stereo_dp_util.h"
#include <vsdof/hal/ProfileUtil.h>
#include <vsdof/hal/AffinityUtil.h>

#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/aaa/INvBufUtil.h>
#include <hal/inc/camera_custom_stereo.h>  // For CUST_STEREO_* definitions.
#include <ui/GraphicBuffer.h>
#include <math.h>

#include <vsdof/hal/rapidjson/writer.h>
#include <vsdof/hal/rapidjson/stringbuffer.h>
#include <vsdof/hal/rapidjson/document.h>     // rapidjson's DOM-style API
#include <vsdof/hal/rapidjson/prettywriter.h> // for stringify JSON
#include <vsdof/hal/rapidjson/filewritestream.h>
#include <vsdof/hal/rapidjson/writer.h>

#include "MaskSpliter.h"

#define N3D_HAL_DEBUG

#ifdef N3D_HAL_DEBUG    // Enable debug log.
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

#endif  // N3D_HAL_DEBUG

#define MY_LOGV_IF(cond, arg...)    if (cond) { MY_LOGV(arg); }
#define MY_LOGD_IF(cond, arg...)    if (cond) { MY_LOGD(arg); }
#define MY_LOGI_IF(cond, arg...)    if (cond) { MY_LOGI(arg); }
#define MY_LOGW_IF(cond, arg...)    if (cond) { MY_LOGW(arg); }
#define MY_LOGE_IF(cond, arg...)    if (cond) { MY_LOGE(arg); }
    
#include <mtkcam/utils/std/Log.h>    

using namespace NSCam;
using namespace android;
using namespace rapidjson;
using namespace std;
using namespace StereoHAL;
using android::Mutex;           // For android::Mutex in stereo_hal.h.

/**************************************************************************
 *                      D E F I N E S / M A C R O S                       *
 **************************************************************************/
#define PROPERTY_ENABLE_VERIFY  STEREO_PROPERTY_PREFIX"enable_verify"
#define PERPERTY_ENABLE_CC      STEREO_PROPERTY_PREFIX"enable_cc"
#define PERPERTY_DISABLE_GPU    STEREO_PROPERTY_PREFIX"disable_gpu"
#define PROPERTY_ALGO_BEBUG     STEREO_PROPERTY_PREFIX"dbgdump"
#define PROPERTY_DUMP_NVRAM     STEREO_PROPERTY_PREFIX"dump_nvram"
#define PROPERTY_DUMP_OUTPUT    STEREO_PROPERTY_PREFIX"dump_n3d"
#define PROPERTY_DUMP_CAPTURE   STEREO_PROPERTY_PREFIX"dump_n3d_capture"
#define PROPERTY_SKIP_N3D       STEREO_PROPERTY_PREFIX"skip_n3d"
#define PROPERTY_DUMP_JSON      STEREO_PROPERTY_PREFIX"dump_json"

const MUINT32   NOT_QUERY      = 0xFFFFFFFF;
const MFLOAT    RRZ_CAPIBILITY = 0.25f;
/**************************************************************************
 *     E N U M / S T R U C T / T Y P E D E F    D E C L A R A T I O N     *
 **************************************************************************/

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/

/**************************************************************************
 *       P R I V A T E    F U N C T I O N    D E C L A R A T I O N        *
 **************************************************************************/
Mutex N3D_HAL_IMP::mLock;
NVRAM_CAMERA_GEOMETRY_STRUCT* N3D_HAL_IMP::m_pVoidGeoData = NULL;
MTKStereoKernel *N3D_HAL_IMP::__previewStereoDrv = NULL;
MTKStereoKernel *N3D_HAL_IMP::__captureStereoDrv = NULL;
MINT32 N3D_HAL_IMP::m_nvramUserCount = 0;

/**************************************************************************
 *       Public Functions                                                 *
 **************************************************************************/

N3D_HAL *
N3D_HAL::createInstance(N3D_HAL_INIT_PARAM &n3dInitParam)
{
    return new N3D_HAL_IMP(n3dInitParam);
}

N3D_HAL_IMP::N3D_HAL_IMP(N3D_HAL_INIT_PARAM &n3dInitParam)
    : DUMP_START(checkStereoProperty("depthmap.pipe.dump.start", true, -1))
    , RUN_N3D(checkStereoProperty(PROPERTY_SKIP_N3D) != 1)
    , m_eScenario(eSTEREO_SCENARIO_UNKNOWN)
    , m_pWorkBuf(NULL)
    , m_workBufSize(0)
    , m_pAFTable{NULL, NULL}
    , m_isAF{false, false}
    , m_magicNumber{0, 0}
    , m_requestNumber(0)
    , m_pStereoDrv(NULL)
    , m_pDepthAFHAL(NULL)
    , m_main1Mask(NULL)
    , m_main2Mask(NULL)
    , m_main2MaskSize(0)
    , m_stereoExtraData(NULL)
    , m_queryIndex(NOT_QUERY)
{
    if(RUN_N3D) {
        m_pStereoDrv = MTKStereoKernel::createInstance();
    }
    _initN3DHAL(n3dInitParam);
}

N3D_HAL_IMP::~N3D_HAL_IMP()
{
    _saveNVRAM();

    if(m_main1Mask) {
        delete [] m_main1Mask;
        m_main1Mask = NULL;
    }

    if(m_main2Mask) {
        delete [] m_main2Mask;
        m_main2Mask = NULL;
    }

    if(m_outputGBMain2.get()) {
        StereoDpUtil::freeImageBuffer(LOG_TAG, m_outputGBMain2);
    }

    if (m_pWorkBuf) {
        delete [] m_pWorkBuf;
        m_pWorkBuf = NULL;
    }

    if(m_pDepthAFHAL) {
        m_pDepthAFHAL->destroyInstance();
        m_pDepthAFHAL = NULL;
    }

    if(__captureStereoDrv == m_pStereoDrv) {
        __captureStereoDrv = NULL;
    } else if(__previewStereoDrv == m_pStereoDrv) {
        __previewStereoDrv = NULL;
    } else {
        MY_LOGE("Stereo drv does not match: this %p, preview %p, capture %p",
                m_pStereoDrv, __previewStereoDrv, __captureStereoDrv);
    }

    if(m_pStereoDrv) {
        delete m_pStereoDrv;
        m_pStereoDrv = NULL;
    }

    if(m_stereoExtraData) {
        delete [] m_stereoExtraData;
        m_stereoExtraData = NULL;
    }
}

bool
N3D_HAL_IMP::_initN3DHAL(N3D_HAL_INIT_PARAM &n3dInitParam)
{
    LOG_ENABLED     = StereoSettingProvider::isLogEnabled(PERPERTY_DEPTHMAP_NODE_LOG);
    PROFILE_ENABLED = StereoSettingProvider::isProfileLogEnabled();

    m_eScenario = n3dInitParam.eScenario;
    switch(n3dInitParam.eScenario) {
        case eSTEREO_SCENARIO_CAPTURE:
            if( StereoSettingProvider::isDeNoise() )
            {
                m_algoInitInfo.scenario = STEREO_KERNEL_SCENARIO_ONLY_CAPTURE;
            } else {
                m_algoInitInfo.scenario = STEREO_KERNEL_SCENARIO_IMAGE_CAPTURE;
            }

            __captureStereoDrv = m_pStereoDrv;

            if( 1 == checkStereoProperty(PROPERTY_DUMP_CAPTURE) ) {
                _mkdir();
            }
            break;
        case eSTEREO_SCENARIO_PREVIEW:
        case eSTEREO_SCENARIO_RECORD:
            // m_algoInitInfo.scenario = STEREO_KERNEL_SCENARIO_IMAGE_PREVIEW;
            m_algoInitInfo.scenario = STEREO_KERNEL_SCENARIO_VIDEO_RECORD;
            __previewStereoDrv = m_pStereoDrv;

            if( 1 == checkStereoProperty(PROPERTY_DUMP_OUTPUT) ) {
                _mkdir();
            }
            break;
        default:
            break;
    }

    _initAFWinTransform(n3dInitParam.eScenario);

    _loadNVRAM();

    Pass2SizeInfo pass2SizeInfo;
    StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_2, m_eScenario, pass2SizeInfo);
    m_algoInitInfo.iio_main.inp_w = pass2SizeInfo.areaWDMA.size.w;
    m_algoInitInfo.iio_main.inp_h = pass2SizeInfo.areaWDMA.size.h;

    StereoArea areaLDC = StereoSizeProvider::getInstance()->getBufferSize(E_LDC, m_eScenario);
    MSize szLDCContent = areaLDC.contentSize();
    //240x136
    m_algoInitInfo.iio_cmp.inp_w = szLDCContent.w;
    m_algoInitInfo.iio_cmp.inp_h = szLDCContent.h;
    m_algoInitInfo.iio_cmp.src_w = szLDCContent.w;
    m_algoInitInfo.iio_cmp.src_h = szLDCContent.h;
    //272x144
    m_algoInitInfo.iio_cmp.out_w = areaLDC.size.w;
    m_algoInitInfo.iio_cmp.out_h = areaLDC.size.h;

    if(eSTEREO_SCENARIO_CAPTURE != m_eScenario) {
        //272x144
        m_algoInitInfo.iio_main.src_w = m_algoInitInfo.iio_main.inp_w;
        m_algoInitInfo.iio_main.src_h = m_algoInitInfo.iio_main.inp_h;

        MSize szOutput = StereoSizeProvider::getInstance()->getBufferSize(E_SV_Y, m_eScenario);
        m_algoInitInfo.iio_main.out_w = szOutput.w;
        m_algoInitInfo.iio_main.out_h = szOutput.h;

        Pass2SizeInfo pass2SizeInfo;
        StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_P_2, m_eScenario, pass2SizeInfo);
        m_algoInitInfo.iio_auxi.src_w = pass2SizeInfo.areaWDMA.size.w;
        m_algoInitInfo.iio_auxi.src_h = pass2SizeInfo.areaWDMA.size.h;
        m_algoInitInfo.iio_auxi.inp_w = pass2SizeInfo.areaWDMA.size.w;
        m_algoInitInfo.iio_auxi.inp_h = pass2SizeInfo.areaWDMA.size.h;
    } else {
        m_algoInitInfo.iio_main.src_w = pass2SizeInfo.areaWDMA.contentSize().w;
        m_algoInitInfo.iio_main.src_h = pass2SizeInfo.areaWDMA.contentSize().h;

        Pass2SizeInfo pass2SizeInfo;
        StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_P_2, m_eScenario, pass2SizeInfo);
        m_algoInitInfo.iio_auxi.src_w = m_algoInitInfo.iio_main.src_w;
        m_algoInitInfo.iio_auxi.src_h = m_algoInitInfo.iio_main.src_h;
        m_algoInitInfo.iio_auxi.inp_w = m_algoInitInfo.iio_main.inp_w;
        m_algoInitInfo.iio_auxi.inp_h = m_algoInitInfo.iio_main.inp_h;

        MSize szOutput = StereoSizeProvider::getInstance()->getBufferSize(E_SV_Y_LARGE, m_eScenario);
        m_algoInitInfo.iio_main.out_w = szOutput.w;
        m_algoInitInfo.iio_main.out_h = szOutput.h;

        //init m_main1Mask
        StereoArea areaMask = StereoSizeProvider::getInstance()->getBufferSize(E_MASK_M_Y, m_eScenario);
        MUINT32 length = areaMask.size.w * areaMask.size.h;
        if(length > 0) {
            if(NULL == m_main1Mask) {
                m_main1Mask = new MUINT8[length];
                ::memset(m_main1Mask, 0, sizeof(MUINT8)*length);
                MUINT8 *startPos = m_main1Mask + areaMask.startPt.x+areaMask.size.w*areaMask.startPt.y;
                const MUINT32 END_Y = areaMask.contentSize().h;
                const MUINT32 CONTENT_W = areaMask.contentSize().w * sizeof(MUINT8);
                for(unsigned int y = 0; y < END_Y; y++) {
                    ::memset(startPos, 0xFF, CONTENT_W);
                    startPos += areaMask.size.w;
                }
            }
        } else {
            MY_LOGE("Size of MASK_M_Y is 0");
            return false;
        }
    }

    // HWFE INPUT - the actual size for HWFE (after SRZ)
    m_algoInitInfo.geo_info  = n3dInitParam.fefmRound;   //N3D_HAL_INIT_PARAM.fefmRound
    m_algoInitInfo.geo_info |= 3<<2;
    _initN3DGeoInfo(m_algoInitInfo.geo_img);              //FEFM setting

    // COLOR CORRECTION INPUT
    _initCCImgInfo(m_algoInitInfo.pho_img);       //settings of main = auxi

    // Learning
    SensorFOV main1FOV, main2FOV;
    StereoSettingProvider::getStereoCameraFOV(main1FOV, main2FOV);
    m_algoInitInfo.fov_main[0]     = main1FOV.fov_horizontal;
    m_algoInitInfo.fov_main[1]     = main1FOV.fov_vertical;
    m_algoInitInfo.fov_auxi[0]     = main2FOV.fov_horizontal;
    m_algoInitInfo.fov_auxi[1]     = main2FOV.fov_vertical;

    m_algoInitInfo.baseline     = getStereoBaseline(StereoSettingProvider::stereoProfile());
    m_algoInitInfo.system_cfg   = 35;   //00000100011
    if( eSTEREO_SCENARIO_CAPTURE == m_eScenario &&
        checkStereoProperty(PROPERTY_ENABLE_VERIFY) > 0 )
    {
        m_algoInitInfo.system_cfg  |= (1<<2);
    }

    if( (STEREO_KERNEL_SCENARIO_IMAGE_CAPTURE == m_algoInitInfo.scenario ||
         STEREO_KERNEL_SCENARIO_ONLY_CAPTURE  == m_algoInitInfo.scenario) &&
        checkStereoProperty(PERPERTY_DISABLE_GPU) != 1)
    {
        m_algoInitInfo.system_cfg  |= (1<<3);  //enable GPU
    }

    if( checkStereoProperty(PERPERTY_ENABLE_CC, false) != 0 ) {
        m_algoInitInfo.system_cfg  |= (1<<4);
    }

    if( eRotate_90  == StereoSettingProvider::getModuleRotation() ||
        eRotate_270 == StereoSettingProvider::getModuleRotation() )
    {
        m_algoInitInfo.system_cfg  |= (1<<6);
    }

    if(StereoSettingProvider::getSensorRelativePosition()) {
        m_algoInitInfo.system_cfg |= (1<<7);
    }

    m_algoInitInfo.system_cfg |= (ENABLE_LDC[StereoSettingProvider::stereoProfile()]<<8);

    int32_t main1Idx, main2Idx;
    StereoSettingProvider::getStereoSensorIndex(main1Idx, main2Idx);
    _initLensInfo(main1Idx, m_pAFTable[0], m_isAF[0]);
    _initLensInfo(main2Idx, m_pAFTable[1], m_isAF[1]);
    m_algoInitInfo.system_cfg |= (m_isAF[1] <<9);
    m_algoInitInfo.system_cfg |= (m_isAF[0] <<10);

    m_algoInitInfo.dac_start_main = (m_pAFTable[0]) ? m_pAFTable[0]->af_dac_min : 0;
    m_algoInitInfo.dac_start_auxi = (m_pAFTable[1]) ? m_pAFTable[1]->af_dac_min : 0;

    // Learning Coordinates RE-MAPPING
    _getStereoRemapInfo(m_algoInitInfo.flow_main, m_algoInitInfo.flow_auxi, m_eScenario);

#if 1//(1 == DEPTH_AF_SUPPORTED)
    if(NULL == m_pDepthAFHAL &&
       eSTEREO_SCENARIO_CAPTURE != m_eScenario)
    {
        m_pDepthAFHAL = StereoDepthHal::createInstance();    // init() has already run inside createInstance()

        if(m_pDepthAFHAL) {
            STEREODEPTH_HAL_INIT_PARAM_STRUCT stStereodepthHalInitParam;
            stStereodepthHalInitParam.stereo_fov_main       = m_algoInitInfo.fov_main[0];
            stStereodepthHalInitParam.stereo_fov_main2      = m_algoInitInfo.fov_auxi[0];
            stStereodepthHalInitParam.stereo_baseline       = m_algoInitInfo.baseline;
            stStereodepthHalInitParam.stereo_pxlarr_width   = m_algoInitInfo.flow_main.pixel_array_width;
            stStereodepthHalInitParam.stereo_pxlarr_height  = m_algoInitInfo.flow_main.pixel_array_height;
            stStereodepthHalInitParam.stereo_main12_pos     = StereoSettingProvider::getSensorRelativePosition();
            stStereodepthHalInitParam.pNvRamDataArray       = m_pVoidGeoData->StereoNvramData.DepthAfData;

            AutoProfileUtil profile(LOG_TAG, "Init StereoDepthHAL");
            m_pDepthAFHAL->StereoDepthInit(&stStereodepthHalInitParam);
        }
    }
#endif

    if(RUN_N3D) {
        MRESULT err = 0;
        err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_GET_DEFAULT_TUNING, NULL, &m_algoTuningInfo);
        if (err) {
            MY_LOGE("StereoKernelFeatureCtrl(GET_DEFAULT_TUNING) fail. error code: %d.", err);
            return false;
        } else {
            _dumpTuningInfo("", m_algoTuningInfo);
        }
    }

    if ( !_doStereoKernelInit() ) {
        MY_LOGE("Init N3D algo failed");
        return false;
    }

    return true;
}

bool
N3D_HAL_IMP::N3DHALRun(N3D_HAL_PARAM &n3dParams, N3D_HAL_OUTPUT &n3dOutput)
{
    if(eSTEREO_SCENARIO_CAPTURE == m_eScenario) {
        MY_LOGW("Cannot run capture here");
        return false;
    }

    AutoProfileUtil profile(LOG_TAG, "N3DHALRun(Preview/VR)");

    ENUM_STEREO_SCENARIO scenario = (n3dParams.eisData.isON) ? eSTEREO_SCENARIO_RECORD : eSTEREO_SCENARIO_PREVIEW;
    if(scenario != m_eScenario)
    {
        m_eScenario = scenario;

        Pass2SizeInfo pass2SizeInfo;
        StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_2, scenario, pass2SizeInfo);
        m_algoInitInfo.iio_main.src_w = pass2SizeInfo.areaWDMA.size.w;
        m_algoInitInfo.iio_main.src_h = pass2SizeInfo.areaWDMA.size.h;
        m_algoInitInfo.iio_main.inp_w = m_algoInitInfo.iio_main.src_w;
        m_algoInitInfo.iio_main.inp_h = m_algoInitInfo.iio_main.src_h;

        StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_P_2, scenario, pass2SizeInfo);
        m_algoInitInfo.iio_auxi.src_w = pass2SizeInfo.areaWDMA.size.w;
        m_algoInitInfo.iio_auxi.src_h = pass2SizeInfo.areaWDMA.size.h;
        m_algoInitInfo.iio_auxi.inp_w = pass2SizeInfo.areaWDMA.size.w;
        m_algoInitInfo.iio_auxi.inp_h = pass2SizeInfo.areaWDMA.size.h;

        MY_LOGI("Src size change: main1: %dx%d, main2: %dx%d, re-init N3D",
                m_algoInitInfo.iio_main.src_w, m_algoInitInfo.iio_main.src_h,
                m_algoInitInfo.iio_auxi.src_w, m_algoInitInfo.iio_auxi.src_h);

        _doStereoKernelInit();
    }

    _setN3DParams(n3dParams, n3dOutput);
    _runN3D(n3dOutput);
    if(RUN_N3D) {
        n3dOutput.convOffset = ((MFLOAT*)m_algoResult.out_p[STEREO_KERNEL_OUTPUT_DEPTH])[2];
    }

#if 1//(1 == DEPTH_AF_SUPPORTED)
    if(RUN_N3D &&
       m_isAF[0] &&
       NULL != m_pAFTable[0])
    {
        if(n3dParams.isDepthAFON ||
           n3dParams.isDistanceMeasurementON)
        {
            m_pAFTable[0]->is_daf_run |= E_DAF_RUN_DEPTH_ENGINE;   //Turn on Depth-AF in af_mgr
            _runDepthAF(m_magicNumber[0] % DAF_TBL_QLEN);

            if(n3dParams.isDistanceMeasurementON)
            {
                n3dOutput.distance = _getDistance();
            }
        } else {
            m_pAFTable[0]->is_daf_run &= ~E_DAF_RUN_DEPTH_ENGINE;
        }
    }
#endif

    char fileName[PATH_MAX+1];
    char folder[PATH_MAX+1];
    struct stat st;
    ::memset(&st, 0, sizeof(struct stat));
    FILE *fp = NULL;

#ifdef GTEST
    if(RUN_N3D)
    {
        sprintf(fileName, "%s/N3D_Algo_Debug.log", _getDumpFolderName(m_requestNumber, folder));
        fp = fopen(fileName, "w");
        if(fp) {
            m_pStereoDrv->StereoKernelFeatureCtrl( STEREO_KERNEL_FEATURE_DEBUG, fp, (void *)"XXX_DEBUG_INFO" );
            fflush(fp);
            fclose(fp);
            fp = NULL;
        }
    }
#endif

    //De-noise feature may not pass ldcMain1, we have to take care of it
    if(NULL == n3dOutput.ldcMain1 &&
       NULL != m_algoProcInfo.addr_ml)
    {
        delete [] m_algoProcInfo.addr_ml;
        m_algoProcInfo.addr_ml = NULL;
    }

    return true;
}

bool
N3D_HAL_IMP::N3DHALRun(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput)
{
    CPUMask cpuMask(CPUCoreB, 2);
    AutoCPUAffinity affinity(cpuMask);

    AutoProfileUtil profile(LOG_TAG, "N3DHALRun(Capture)");

    //If the module has only capture feature, no need to sync nvram
    if(STEREO_KERNEL_SCENARIO_ONLY_CAPTURE != m_algoInitInfo.scenario) {
        _syncInternalNVRAM(__previewStereoDrv, __captureStereoDrv);   //nvram: preview->capture
    }

    if( 1 == checkStereoProperty(PROPERTY_DUMP_NVRAM)  ||
        1 == checkStereoProperty(PROPERTY_DUMP_CAPTURE) )
    {
        MY_LOGD("=== Dump capture NVRAM in ===");
        char dumpPath[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(dumpPath, "%s/N3D_nvram_in", _getDumpFolderName(m_requestNumber, folder));
        FILE *fpNVRAM = NULL;
        fpNVRAM = fopen(dumpPath, "wb");
        if(fpNVRAM) {
            fwrite(m_pVoidGeoData->StereoNvramData.StereoData, 1, sizeof(MUINT32)*MTK_STEREO_KERNEL_NVRAM_LENGTH, fpNVRAM);
            fflush(fpNVRAM);
            fclose(fpNVRAM);
            fpNVRAM = NULL;
        } else {
            MY_LOGE("Cannot dump NVRAM to %s, error: %s", dumpPath, strerror(errno));
        }
    }

    _setN3DCaptureParams(n3dParams, n3dOutput);
    bool isSuccess = _runN3DCapture(n3dParams, n3dOutput);
    n3dOutput.convOffset        = ((MFLOAT*)m_algoResult.out_p[STEREO_KERNEL_OUTPUT_DEPTH])[2];
    n3dOutput.warpingMatrixSize = m_algoResult.out_n[STEREO_KERNEL_OUTPUT_RECT_CAP];
    if(n3dOutput.warpingMatrix) {
        const MUINT32 RESULT_SIZE = m_algoResult.out_n[STEREO_KERNEL_OUTPUT_RECT_CAP] * sizeof(MFLOAT);
        if( RESULT_SIZE <= StereoSettingProvider::getMaxWarpingMatrixBufferSizeInBytes() )
        {
            ::memcpy(n3dOutput.warpingMatrix,
                     m_algoResult.out_p[STEREO_KERNEL_OUTPUT_RECT_CAP],
                     RESULT_SIZE);
        } else {
            MY_LOGE("Warpping result size (%d) exceeds max size(%d)",
                    RESULT_SIZE,
                    StereoSettingProvider::getMaxWarpingMatrixBufferSizeInBytes());
        }
    }

    if( RUN_N3D &&
        checkStereoProperty(PROPERTY_DUMP_CAPTURE) == 1 )
    {
        char dumpPath[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(dumpPath, "%s/N3D_Algo_Debug.log", _getDumpFolderName(m_requestNumber, folder));
        FILE *fp = NULL;
        fp = fopen(dumpPath, "w");
        if(fp) {
            m_pStereoDrv->StereoKernelFeatureCtrl( STEREO_KERNEL_FEATURE_DEBUG, fp, (void *)"XXX_DEBUG_INFO" );
            fflush(fp);
            fclose(fp);
            fp = NULL;
        } else {
            MY_LOGE("Cannot dump NVRAM to %s, error: %s", dumpPath, strerror(errno));
        }
    }

    //If only capture feature, this is for updating m_pVoidGeoData->StereoNvramData.StereoData
    _syncInternalNVRAM(__captureStereoDrv, __previewStereoDrv);   //nvram: capture->preview

    if( 1 == checkStereoProperty(PROPERTY_DUMP_NVRAM) ||
        1 == checkStereoProperty(PROPERTY_DUMP_CAPTURE) )
    {
        MY_LOGD("=== Dump capture NVRAM out ===");
        char dumpPath[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(dumpPath, "%s/N3D_nvram_out", _getDumpFolderName(m_requestNumber, folder));
        FILE *fpNVRAM = NULL;
        fpNVRAM = fopen(dumpPath, "wb");
        if(fpNVRAM) {
            fwrite(m_pVoidGeoData->StereoNvramData.StereoData, 1, sizeof(MUINT32)*MTK_STEREO_KERNEL_NVRAM_LENGTH, fpNVRAM);
            fflush(fpNVRAM);
            fclose(fpNVRAM);
            fpNVRAM = NULL;
        } else {
            MY_LOGE("Cannot dump NVRAM to %s, error: %s", dumpPath, strerror(errno));
        }
    }
    //
    if(NULL == n3dOutput.ldcMain1 &&
       NULL != m_algoProcInfo.addr_ml)
    {
        delete [] m_algoProcInfo.addr_ml;
        m_algoProcInfo.addr_ml = NULL;
    }

    return isSuccess;
}

bool
N3D_HAL_IMP::N3DHALRun(N3D_HAL_PARAM_CAPTURE_SWFE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput)
{
    _setN3DSWFECaptureParams(n3dParams, n3dOutput);
    bool isSuccess = _runN3DCaptureSWFE(n3dOutput);
    return isSuccess;
}

char *
N3D_HAL_IMP::getStereoExtraData()
{
    //Only support capture
    if(eSTEREO_SCENARIO_CAPTURE != m_eScenario) {
        return NULL;
    }

    if(NULL == m_stereoExtraData) {
        _prepareStereoExtraData();
    }

    return m_stereoExtraData;
}
/**************************************************************************
 *       Private Functions                                                *
 **************************************************************************/
bool
N3D_HAL_IMP::_getFEOInputInfo(ENUM_PASS2_ROUND pass2Round,
                              ENUM_STEREO_SCENARIO eScenario,
                              STEREO_KERNEL_IMG_INFO_STRUCT &imgInfo)
{
    imgInfo.depth    = 1;    //pixel depth, YUV:1, RGB: 3, RGBA: 4
    imgInfo.format   = 0;    //YUV:0, RGB: 1

    Pass2SizeInfo pass2Info;
    StereoSizeProvider::getInstance()->getPass2SizeInfo(pass2Round, eScenario, pass2Info);

    imgInfo.width        = pass2Info.areaFEO.size.w;
    imgInfo.height       = pass2Info.areaFEO.size.h;
    imgInfo.stride       = imgInfo.width;
    imgInfo.act_width    = pass2Info.areaFEO.size.w - pass2Info.areaFEO.padding.w;
    imgInfo.act_height   = pass2Info.areaFEO.size.h - pass2Info.areaFEO.padding.h;

    if(pass2Round <= PASS2A_3) {
        imgInfo.offset_x = 0;
        imgInfo.offset_y = 0;
    } else {
        imgInfo.offset_x = (imgInfo.width - imgInfo.act_width)>>1;
        imgInfo.offset_y = (imgInfo.height - imgInfo.act_height)>>1;
    }

    return true;
}

bool
N3D_HAL_IMP::_initN3DGeoInfo(STEREO_KERNEL_GEO_INFO_STRUCT geo_img[])
{
    ::memset(geo_img, 0, MAX_GEO_LEVEL * sizeof(STEREO_KERNEL_GEO_INFO_STRUCT));
//    if(MAX_GEO_LEVEL > 0) {
//        geo_img[0].size = StereoSettingProvider::fefmBlockSize(1);    //16
//        _getFEOInputInfo(PASS2A,        m_eScenario,    geo_img[0].img_main);
//        _getFEOInputInfo(PASS2A_P,      m_eScenario,    geo_img[0].img_auxi);
//    }

//    if(MAX_GEO_LEVEL > 1) {
        geo_img[0].size = StereoSettingProvider::fefmBlockSize(1);    //16
        _getFEOInputInfo(PASS2A_2,      m_eScenario,    geo_img[0].img_main);
        _getFEOInputInfo(PASS2A_P_2,    m_eScenario,    geo_img[0].img_auxi);
//    }

//    if(MAX_GEO_LEVEL > 2) {
        geo_img[1].size = StereoSettingProvider::fefmBlockSize(2);    //8
        _getFEOInputInfo(PASS2A_3,      m_eScenario,    geo_img[1].img_main);
        _getFEOInputInfo(PASS2A_P_3,    m_eScenario,    geo_img[1].img_auxi);
//    }

    return true;
}

bool
N3D_HAL_IMP::_initCCImgInfo(STEREO_KERNEL_IMG_INFO_STRUCT &ccImgInfo)
{
    Pass2SizeInfo pass2Info;
    StereoSizeProvider::getInstance()->getPass2SizeInfo(PASS2A_3, m_eScenario, pass2Info);
    MSize szCCImg = pass2Info.areaIMG2O;
    //
    ccImgInfo.width         = szCCImg.w;
    ccImgInfo.height        = szCCImg.h;
    ccImgInfo.depth         = 1;            //pixel depth, YUV:1, RGB: 3, RGBA: 4
    ccImgInfo.stride        = szCCImg.w;
    ccImgInfo.format        = 4;//0;            //YUV:0, RGB: 1
    ccImgInfo.act_width     = szCCImg.w;
    ccImgInfo.act_height    = szCCImg.h;
    ccImgInfo.offset_x      = 0;
    ccImgInfo.offset_y      = 0;
    //
    return true;
}

MUINT8 *
N3D_HAL_IMP::_initWorkingBuffer(const MUINT32 BUFFER_SIZE)
{
    if(!RUN_N3D) {
        return NULL;
    }

    if(m_workBufSize < BUFFER_SIZE) {
        if (m_pWorkBuf) {
            delete [] m_pWorkBuf;
            m_pWorkBuf = NULL;
        }

        const MUINT32 BUFFER_LEN = sizeof(MUINT8) * BUFFER_SIZE;
        m_pWorkBuf = new(std::nothrow) MUINT8[BUFFER_LEN];
        if(NULL == m_pWorkBuf) {
            MY_LOGE("Cannot create N3D working buffer of size %d", BUFFER_LEN);
            return NULL;
        } else {
            MY_LOGD_IF(LOG_ENABLED, "Alloc %d bytes for N3D working buffer", BUFFER_LEN);
            ::memset(m_pWorkBuf, 0, BUFFER_LEN);
        }

        m_workBufSize = BUFFER_SIZE;
    }

    MINT32 err = 0; // 0: no error. other value: error.
    // Allocate working buffer.
    //     Allocate memory
    //     Set WorkBufInfo
    m_algoWorkBufInfo.ext_mem_size       = BUFFER_SIZE;
    m_algoWorkBufInfo.ext_mem_start_addr = m_pWorkBuf;

    err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SET_WORK_BUF_INFO,
                                                &m_algoWorkBufInfo, NULL);

    if (err)
    {
        MY_LOGE("StereoKernelFeatureCtrl(SET_WORK_BUF_INFO) fail. error code: %d.", err);
        return NULL;
    }

    return m_pWorkBuf;
}

bool
N3D_HAL_IMP::_doStereoKernelInit()
{
    if(!RUN_N3D) {
        return true;
    }

    bool result = false;
    AutoProfileUtil profile(LOG_TAG, "N3D init");

    MRESULT err = m_pStereoDrv->StereoKernelInit(&m_algoInitInfo, &m_algoTuningInfo);
    if (err) {
        MY_LOGE("Init N3D algo failed(err %d)", err);
    } else {
        err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_GET_WORK_BUF_INFO, NULL,
                                                    &m_algoInitInfo.working_buffer_size);
        if(err) {
            MY_LOGD("Get working buffer size failed, err: %d", err);
        } else {
            result = true;
            _initWorkingBuffer(m_algoInitInfo.working_buffer_size);
            _loadLensInfo();
            _dumpInitInfo(m_algoInitInfo);
        }
    }

    return result;
}

void
N3D_HAL_IMP::_setN3DCommonParams(N3D_HAL_PARAM_COMMON &n3dParams, N3D_HAL_OUTPUT &n3dOutput, STEREO_KERNEL_SET_PROC_INFO_STRUCT &setprocInfo)
{
    ::memset(&setprocInfo, 0, sizeof(STEREO_KERNEL_SET_PROC_INFO_STRUCT));
    m_magicNumber[0]   = n3dParams.magicNumber[0] + 1;
    m_magicNumber[1]   = n3dParams.magicNumber[1] + 1;
    m_requestNumber    = n3dParams.requestNumber;

    //buffer is squential, so getBufVA(0) can get whole image
    setprocInfo.addr_ms  = (MUINT8*)n3dParams.rectifyImgMain1->getBufVA(0);
    setprocInfo.addr_md  = (MUINT8*)n3dOutput.rectifyImgMain1->getBufVA(0);
    setprocInfo.addr_mm  = n3dOutput.maskMain1;
    //De-noise feature may not pass ldcMain1, we have to take care of it
    if(n3dOutput.ldcMain1) {
        setprocInfo.addr_ml = n3dOutput.ldcMain1;
    } else {
        MSize szLDC = StereoSizeProvider::getInstance()->getBufferSize(E_LDC, m_eScenario);
        setprocInfo.addr_ml = new MUINT8[szLDC.w*szLDC.h];
    }

    // for Photometric Correction
    //buffer is squential, so getBufVA(0) can get whole image
    setprocInfo.addr_mp   = (MUINT8*)n3dParams.ccImage[0]->getBufVA(0);
    setprocInfo.addr_ap   = (MUINT8*)n3dParams.ccImage[1]->getBufVA(0);
    // HWFE
    for(int i = 0; i < MAX_GEO_LEVEL; i++) {
        setprocInfo.addr_me[i] = n3dParams.hwfefmData.geoDataMain1[i];
        setprocInfo.addr_ae[i] = n3dParams.hwfefmData.geoDataMain2[i];
        setprocInfo.addr_lr[i] = n3dParams.hwfefmData.geoDataLeftToRight[i];
        setprocInfo.addr_rl[i] = n3dParams.hwfefmData.geoDataRightToLeft[i];
    }

    // AF INFO
    if(m_isAF[0] &&
       NULL != m_pAFTable[0])
    {
        const int AF_INDEX = m_magicNumber[0] % DAF_TBL_QLEN;
        setprocInfo.af_main.dac_i = m_pAFTable[0]->daf_vec[AF_INDEX].af_dac_pos;
        setprocInfo.af_main.dac_v = m_pAFTable[0]->daf_vec[AF_INDEX].is_af_stable;
        setprocInfo.af_main.dac_c = m_pAFTable[0]->daf_vec[AF_INDEX].af_confidence;
        setprocInfo.af_main.dac_w[0] = m_pAFTable[0]->daf_vec[AF_INDEX].af_win_start_x;
        setprocInfo.af_main.dac_w[1] = m_pAFTable[0]->daf_vec[AF_INDEX].af_win_start_y;
        setprocInfo.af_main.dac_w[2] = m_pAFTable[0]->daf_vec[AF_INDEX].af_win_end_x;
        setprocInfo.af_main.dac_w[3] = m_pAFTable[0]->daf_vec[AF_INDEX].af_win_end_y;
        m_pAFTable[0]->is_daf_run |= E_DAF_RUN_STEREO;  //Backup plan, should be set when init
    }

    if(m_isAF[1] &&
       NULL != m_pAFTable[1])
    {
        const int AF_INDEX = m_magicNumber[1] % DAF_TBL_QLEN;
        setprocInfo.af_auxi.dac_i = m_pAFTable[1]->daf_vec[AF_INDEX].af_dac_pos;
        setprocInfo.af_auxi.dac_v = m_pAFTable[1]->daf_vec[AF_INDEX].is_af_stable;
        setprocInfo.af_auxi.dac_c = m_pAFTable[1]->daf_vec[AF_INDEX].af_confidence;
        setprocInfo.af_auxi.dac_w[0] = m_pAFTable[1]->daf_vec[AF_INDEX].af_win_start_x;
        setprocInfo.af_auxi.dac_w[1] = m_pAFTable[1]->daf_vec[AF_INDEX].af_win_start_y;
        setprocInfo.af_auxi.dac_w[2] = m_pAFTable[1]->daf_vec[AF_INDEX].af_win_end_x;
        setprocInfo.af_auxi.dac_w[3] = m_pAFTable[1]->daf_vec[AF_INDEX].af_win_end_y;
        m_pAFTable[1]->is_daf_run |= E_DAF_RUN_STEREO;  //Backup plan, should be set when init
    }
}

void
N3D_HAL_IMP::_setN3DParams(N3D_HAL_PARAM &n3dParams, N3D_HAL_OUTPUT &n3dOutput)
{
    _setN3DCommonParams(n3dParams, n3dOutput, m_algoProcInfo);

    StereoArea areaLDC = StereoSizeProvider::getInstance()->getBufferSize(E_LDC, m_eScenario);
    MSize szLDCContent = areaLDC.contentSize();
    //240x136
    m_algoProcInfo.iio_cap.inp_w = szLDCContent.w;
    m_algoProcInfo.iio_cap.inp_h = szLDCContent.h;
    m_algoProcInfo.iio_cap.src_w = szLDCContent.w;
    m_algoProcInfo.iio_cap.src_h = szLDCContent.h;
    //272x144
    m_algoProcInfo.iio_cap.out_w = areaLDC.size.w;
    m_algoProcInfo.iio_cap.out_h = areaLDC.size.h;

    //buffer is squential, so getBufVA(0) can get whole image
    m_algoProcInfo.addr_as = (MUINT8*)n3dParams.rectifyImgMain2->getBufVA(0);
    m_algoProcInfo.addr_ad = (MUINT8*)n3dOutput.rectifyImgMain2->getBufVA(0);
    m_algoProcInfo.addr_am = n3dOutput.maskMain2;

    // EIS INFO.
    if(n3dParams.eisData.isON) {
        m_algoProcInfo.eis[0] = n3dParams.eisData.eisOffset.x - (m_algoInitInfo.flow_main.rrz_out_width  - n3dParams.eisData.eisImgSize.w)/2;
        m_algoProcInfo.eis[1] = n3dParams.eisData.eisOffset.y - (m_algoInitInfo.flow_main.rrz_out_height - n3dParams.eisData.eisImgSize.h)/2;
        m_algoProcInfo.eis[2] = m_algoInitInfo.flow_main.rrz_out_width;
        m_algoProcInfo.eis[3] = m_algoInitInfo.flow_main.rrz_out_height;
    } else {
        m_algoProcInfo.eis[0] = 0;
        m_algoProcInfo.eis[1] = 0;
        m_algoProcInfo.eis[2] = 0;
        m_algoProcInfo.eis[3] = 0;
    }

    if(RUN_N3D) {
        AutoProfileUtil profile(LOG_TAG, "N3D set proc(Preview/VR)");
        MINT32 err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SET_PROC_INFO, &m_algoProcInfo, NULL);
        if (err) {
            MY_LOGE("StereoKernelFeatureCtrl(SET_PROC) fail. error code: %d.", err);
        } else {
            _dumpSetProcInfo("", m_algoProcInfo);
        }
    }
}

void
N3D_HAL_IMP::_setN3DCaptureParams(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput)
{
    AutoProfileUtil profile(LOG_TAG, "Set params(Capture)");

    m_captureOrientation = n3dParams.captureOrientation;
    //Common Part
    _setN3DCommonParams(n3dParams, n3dOutput, m_algoProcInfo);

    //Capture Part
    MSize captureSize;
    if(StereoSettingProvider::isDeNoise()) {
        captureSize = StereoSizeProvider::getInstance()->getBufferSize(E_BM_PREPROCESS_W_1, m_eScenario).contentSize();
    } else {
        captureSize = StereoSizeProvider::getInstance()->captureImageSize();    //Not used actually
    }

    m_algoProcInfo.iio_cap.inp_w = captureSize.w;
    m_algoProcInfo.iio_cap.inp_h = captureSize.h;
    m_algoProcInfo.iio_cap.src_w = captureSize.w;
    m_algoProcInfo.iio_cap.src_h = captureSize.h;
    m_algoProcInfo.iio_cap.out_w = captureSize.w;
    m_algoProcInfo.iio_cap.out_h = captureSize.h;

    if( checkStereoProperty(PROPERTY_DUMP_CAPTURE) == 1 ) {
        MSize szSrc = StereoSizeProvider::getInstance()->getBufferSize(E_SV_Y_LARGE, m_eScenario).size;
        char dumpPath[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(dumpPath, "%s/N3D_main2_GB_in_%dx%d.yuv", _getDumpFolderName(m_requestNumber, folder), szSrc.w, szSrc.h);
        n3dParams.rectifyGBMain2->saveToFile(dumpPath);
    }
    m_algoProcInfo.src_gb.mGraphicBuffer = n3dParams.rectifyGBMain2->getImageBufferHeap()->getGraphicBuffer();
    if(NULL == m_outputGBMain2.get()) {
        MSize szSrc = StereoSizeProvider::getInstance()->getBufferSize(E_SV_Y_LARGE, m_eScenario).size;
        if( !StereoDpUtil::allocImageBuffer(LOG_TAG, eImgFmt_RGBA8888, szSrc, IS_ALLOC_GB, m_outputGBMain2) ) {
            MY_LOGE("Cannot alloc image buffer");
        }
    }
    m_algoProcInfo.dst_gb.mGraphicBuffer = (void *)m_outputGBMain2.get()->getImageBufferHeap()->getGraphicBuffer();

    sp<GraphicBuffer>* srcGBArray[1];
    sp<GraphicBuffer>* dstGBArray[1];
    srcGBArray[0] = (sp<GraphicBuffer>*)m_algoProcInfo.src_gb.mGraphicBuffer;
    dstGBArray[0] = (sp<GraphicBuffer>*)m_algoProcInfo.dst_gb.mGraphicBuffer;
    m_algoProcInfo.InputGB   = (void*)&srcGBArray;
    m_algoProcInfo.OutputGB  = (void*)&dstGBArray;

    if(RUN_N3D) {
        AutoProfileUtil profile2(LOG_TAG, "N3D set proc(Capture)");
        MINT32 err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SET_PROC_INFO, &m_algoProcInfo, NULL);
        if (err) {
            MY_LOGE("StereoKernelFeatureCtrl(SET_PROC) fail. error code: %d.", err);
        } else {
            _dumpSetProcInfo("", m_algoProcInfo);
        }
    }
}

bool
N3D_HAL_IMP::_runN3DCommon()
{
    bool bResult = true;
    if(!RUN_N3D) {
        return true;
    }

    MINT32 err = 0; // 0: no error. other value: error.

    MY_LOGD_IF(LOG_ENABLED, "StereoKernelMain +");
    {
        AutoProfileUtil profile(LOG_TAG, "Run N3D main");
        err = m_pStereoDrv->StereoKernelMain();
    }
    MY_LOGD_IF(LOG_ENABLED, "StereoKernelMain -");

    if (err) {
        MY_LOGE("StereoKernelMain() fail. error code: %d.", err);
        bResult = MFALSE;
    } else {
        // Get result.
        AutoProfileUtil profile(LOG_TAG, "Get N3D result");
        err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_GET_RESULT, NULL, &m_algoResult);
        if (err) {
            MY_LOGE("StereoKernelFeatureCtrl(GET_RESULT) fail. error code: %d.", err);
            bResult = MFALSE;
        }
    }

    if(!err) {
        _dumpResult("[N3D Result]", m_algoResult);

        //Depth AF
        m_algoResult.out_n[0] ;
        m_algoResult.out_p[0] ;
        m_algoResult.out_n[1] ;
        m_algoResult.out_p[1] ;
    }

    return bResult;
}

bool
N3D_HAL_IMP::_runN3D(N3D_HAL_OUTPUT &n3dOutput __attribute__((unused)))
{
    bool bResult = true;
    bResult = _runN3DCommon();

    return bResult;
}

void
N3D_HAL_IMP::_initAFWinTransform(ENUM_STEREO_SCENARIO scenario)
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
        //========= Get main1 size =========
        IHalSensor* pIHalSensor = sensorList->createSensor(LOG_TAG, main1SensorIndex);
        if(NULL == pIHalSensor) {
            MY_LOGE("Cannot get hal sensor of main1");
            err = 1;
        } else {
            SensorCropWinInfo sensorCropInfo;
            ::memset(&sensorCropInfo, 0, sizeof(SensorCropWinInfo));
            int sensorScenario = getSensorSenario(scenario);
            err = pIHalSensor->sendCommand(main1SensorDevIndex, SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO,
                                           (MUINTPTR)&sensorScenario, (MUINTPTR)&sensorCropInfo, 0);

            if(err) {
                MY_LOGE("Cannot get sensor crop info for scenario %d", scenario);
            } else {
                if(eSTEREO_SCENARIO_CAPTURE == scenario) {
                    //For image refocus
                    switch(StereoSettingProvider::imageRatio()) {
                    case eRatio_4_3:
                        {
                            m_afOffsetX = sensorCropInfo.x2_tg_offset;
                            m_afOffsetY = sensorCropInfo.y2_tg_offset;
                            m_afScaleW = 2000.0f / sensorCropInfo.w2_tg_size;
                            m_afScaleH = 2000.0f / sensorCropInfo.h2_tg_size;
                        }
                        break;
                    case eRatio_16_9:
                    default:
                        {
                            //4:3->16:9
                            m_afOffsetX = sensorCropInfo.x2_tg_offset;
                            m_afOffsetY = sensorCropInfo.y2_tg_offset + sensorCropInfo.h2_tg_size/8;
                            m_afScaleW = 2000.0f / sensorCropInfo.w2_tg_size;
                            m_afScaleH = 2000.0f / (sensorCropInfo.h2_tg_size * 3 / 4);
                        }
                        break;
                    }
                } else {
                    //For Depth-AF
                    m_afOffsetX = sensorCropInfo.x2_tg_offset;
                    m_afOffsetY = sensorCropInfo.y2_tg_offset;
                    m_afScaleW = sensorCropInfo.full_w / sensorCropInfo.w2_tg_size;
                    m_afScaleH = sensorCropInfo.full_h / sensorCropInfo.h2_tg_size;
                }

                MY_LOGD_IF(LOG_ENABLED,
                           "AF Transform: scenario: %d, offset(%d, %d), scale(%f, %f)",
                           scenario, m_afOffsetX, m_afOffsetY, m_afScaleW, m_afScaleH);
            }

            pIHalSensor->destroyInstance(LOG_TAG);
        }
    }
}

void
N3D_HAL_IMP::_transferAFWin(const AF_WIN_COORDINATE_STRUCT &in, AF_WIN_COORDINATE_STRUCT &out)
{

    if(eSTEREO_SCENARIO_CAPTURE == m_eScenario) {
        //For image refocus
        //ROI correction
        int x_correction = 0;
        if(in.af_win_start_x < m_afOffsetX) {
            x_correction = m_afOffsetX - in.af_win_start_x;
        }

        int y_correction = 0;
        if(in.af_win_start_y < m_afOffsetY) {
            x_correction = m_afOffsetY - in.af_win_start_y;
        }

        int correction = MAX(x_correction, y_correction);

        //ROI correction should not change the center point
        out.af_win_start_x = (in.af_win_start_x - m_afOffsetX + correction) * m_afScaleW - 1000.0f;
        out.af_win_start_y = (in.af_win_start_y - m_afOffsetY + correction) * m_afScaleH - 1000.0f;
        out.af_win_end_x   = (in.af_win_end_x   - m_afOffsetX - correction) * m_afScaleW - 1000.0f;
        out.af_win_end_y   = (in.af_win_end_y   - m_afOffsetY - correction) * m_afScaleH - 1000.0f;
    } else {
        //For Depth AF
        out.af_win_start_x = in.af_win_start_x - m_afOffsetX;
        out.af_win_start_y = in.af_win_start_y - m_afOffsetY;
        out.af_win_end_x   = in.af_win_end_x   - m_afOffsetX;
        out.af_win_end_y   = in.af_win_end_y   - m_afOffsetY;

        if(eSTEREO_SCENARIO_RECORD == m_eScenario) {
            out.af_win_start_x *= m_afScaleW;
            out.af_win_start_y *= m_afScaleH;
            out.af_win_end_x   *= m_afScaleW;
            out.af_win_end_y   *= m_afScaleH;
        }
    }

    MY_LOGD_IF(LOG_ENABLED,
               "AF ROI: %d %d %d %d -> %d %d %d %d",
               in.af_win_start_x, in.af_win_start_y, in.af_win_end_x, in.af_win_end_y,
               out.af_win_start_x, out.af_win_start_y, out.af_win_end_x, out.af_win_end_y);
}

//Must run after getting result
bool
N3D_HAL_IMP::_runDepthAF(const int AF_INDEX)
{
    if(eSTEREO_SCENARIO_CAPTURE == m_eScenario ||
       NULL == m_pDepthAFHAL ||
       NULL == m_algoResult.out_p[1] ||
       NULL == m_pAFTable[0])
    {
        MY_LOGW("Run DAF fail: m_eScenario: %d, HAL: %p, out_p: %p, AF Table %p",
                m_eScenario, m_pDepthAFHAL, m_algoResult.out_p[1], m_pAFTable[0]);
        return false;
    }
    //
    STEREODEPTH_HAL_INIT_PARAM_STRUCT stStereodepthHalInitParam;
    stStereodepthHalInitParam.pCoordTransParam = (MFLOAT *)m_algoResult.out_p[1];
    m_pDepthAFHAL->StereoDepthSetParams(&stStereodepthHalInitParam);

    AF_WIN_COORDINATE_STRUCT afWin(m_pAFTable[0]->daf_vec[AF_INDEX].af_win_start_x,
                                   m_pAFTable[0]->daf_vec[AF_INDEX].af_win_start_y,
                                   m_pAFTable[0]->daf_vec[AF_INDEX].af_win_end_x,
                                   m_pAFTable[0]->daf_vec[AF_INDEX].af_win_end_y);
    AF_WIN_COORDINATE_STRUCT dafWin;
    _transferAFWin(afWin, dafWin);

    static int sGetDistanceIndex = 0;
    if(NOT_QUERY == m_pAFTable[0]->is_query_happen) {   //Not query, a.k.a. is learning
        if (0 != m_algoResult.out_n[0] &&
            NULL != m_algoResult.out_p[0] &&
            m_pAFTable[0]->daf_vec[AF_INDEX].is_af_stable) //not to learn when dac is not stable
        {
            MY_LOGD_IF(LOG_ENABLED, "[DAF][Learning] out_n[0] = %d, out_p[0] = %p", m_algoResult.out_n[0], m_algoResult.out_p[0]);
            AutoProfileUtil profile(LOG_TAG, "DAF learning");
            m_pDepthAFHAL->StereoDepthRunLearning(m_algoResult.out_n[0], (MFLOAT *)m_algoResult.out_p[0], &dafWin);
        } else {
            MY_LOGD_IF(PROFILE_ENABLED, "[DAF][Learning] Ignore, af done: %d, out_n[0] = %d, out_p[0] = %p",
                       m_pAFTable[0]->daf_vec[AF_INDEX].is_af_stable, m_algoResult.out_n[0], m_algoResult.out_p[0]);
        }
    } else {
        MY_LOGD_IF(LOG_ENABLED,
                   "[DAF][Query] is_query_happen %d, af idx %d, out_n[0] = %d, out_p[0] = %p, AF win: (%d, %d), (%d, %d)",
                    m_pAFTable[0]->is_query_happen,
                    AF_INDEX,
                    m_algoResult.out_n[0], m_algoResult.out_p[0],
                    dafWin.af_win_start_x, dafWin.af_win_start_y,
                    dafWin.af_win_end_x, dafWin.af_win_end_y);

        AutoProfileUtil profile(LOG_TAG, "DAF query");

        m_queryIndex = AF_INDEX;
        m_pDepthAFHAL->StereoDepthQuery(m_algoResult.out_n[0], (MFLOAT *)m_algoResult.out_p[0], &dafWin);

        m_ptQueryDistance = dafWin.centerPoint();
    }

    return true;
}

MFLOAT
N3D_HAL_IMP::_getDistance()
{
    if(eSTEREO_SCENARIO_CAPTURE == m_eScenario ||
       NULL == m_pDepthAFHAL ||
       NULL == m_pAFTable[0])
    {
        return 0.0f;
    }

    AutoProfileUtil profile(LOG_TAG, "Get distance");
    MFLOAT distance = 0.0f;
    if(NOT_QUERY != m_queryIndex &&
       m_pAFTable[0]->daf_vec[m_queryIndex].is_af_stable)  //Get distance after dac is stable
    {
        distance = m_pAFTable[0]->daf_vec[m_queryIndex].daf_distance;
        //TODO: use m_ptQueryDistance to query distance from algo
        // distance = 1.2f;    //UNIT: meter
        m_queryIndex = NOT_QUERY;
        MY_LOGD_IF(LOG_ENABLED, "[DAF][Distance] %f, confidence: %d", distance, m_pAFTable[0]->daf_vec[m_queryIndex].daf_confidence);
    }

    return distance;
}

bool
N3D_HAL_IMP::_runN3DCapture(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput)
{
    MY_LOGD_IF(LOG_ENABLED, "+");

    bool isSuccess = _runN3DCommon();
    if(!isSuccess) {
        return false;
    }

    if(eSTEREO_SCENARIO_CAPTURE != m_eScenario) {
        MY_LOGW("Wrong scenario, expect %d, fact: %d", eSTEREO_SCENARIO_CAPTURE, m_eScenario);
        return false;
    }

    if( RUN_N3D &&
        1 == checkStereoProperty(PROPERTY_ALGO_BEBUG) )
    {
        static MUINT snLogCount = 0;
        m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SAVE_LOG, &snLogCount, NULL);
    }

    //Copy mask main1
    StereoArea areaMask = StereoSizeProvider::getInstance()->getBufferSize(E_MASK_M_Y, m_eScenario);
    MUINT32 length = areaMask.size.w * areaMask.size.h;
    ::memcpy(n3dOutput.maskMain1, m_main1Mask, length * sizeof(MUINT8));

    //=== Split mask ===
    _splitMask();

    //=== Transfer data to JSON ===
    _prepareStereoExtraData();

    MSize szSrc = StereoSizeProvider::getInstance()->getBufferSize(E_SV_Y_LARGE, m_eScenario).size;
    const MUINT32 FULL_LEN   = szSrc.w * szSrc.h;
    StereoArea newArea = StereoSizeProvider::getInstance()->getBufferSize(E_SV_Y, m_eScenario);
    MSize newSize = newArea.size;
    const MUINT32 RESIZE_LEN = newSize.w * newSize.h;
    MUINT32 offset = 0;
    sp<IImageBuffer> srcImg;
    sp<IImageBuffer> dstImg;

    //Use MDP to centerize main1 image
    //1. Use MDP to resize and centerize main1 image, n3dOutput.rectifyImgMain1
    {
        AutoProfileUtil profile(LOG_TAG, "Resize and ceterize main1");

        // Centralize by MDP
        StereoArea areaSrc = StereoSizeProvider::getInstance()->getBufferSize(E_MV_Y_LARGE, m_eScenario);
        MSize srcContentSize = areaSrc.contentSize();
        DpRect srcROI(0, 0, srcContentSize.w, srcContentSize.h);
        DpRect dstROI(newArea.startPt.x, newArea.startPt.y, newArea.contentSize().w, newArea.contentSize().h);
        if(StereoDpUtil::transformImage(n3dParams.rectifyImgMain1, n3dOutput.rectifyImgMain1, eRotate_0, &srcROI, &dstROI)) {
            if( checkStereoProperty(PROPERTY_DUMP_CAPTURE) == 1 ) {
                char dumpPath[PATH_MAX+1];
                char folder[PATH_MAX+1];
                _getDumpFolderName(m_requestNumber, folder);

                sprintf(dumpPath, "%s/N3D_main1_in_%dx%d.yuv", folder, areaSrc.size.w, areaSrc.size.h);
                n3dParams.rectifyImgMain1->saveToFile(dumpPath);

                sprintf(dumpPath, "%s/N3D_main1_out_%dx%d.yuv", folder, newSize.w, newSize.h);
                n3dOutput.rectifyImgMain1->saveToFile(dumpPath);
            }
        } else {
            MY_LOGE("Resize main1 image fail");
            return false;
        }
    }

    //2. Use MDP to convert main2 image(m_outputGBMain2) from RGBA to YV12 for JPS, n3dOutput.jpsImgMain2
    {
        AutoProfileUtil profile(LOG_TAG, "Convert main2 from RGBA to YV12");

        if( checkStereoProperty(PROPERTY_DUMP_CAPTURE) == 1 ) {
            char dumpPath[PATH_MAX+1];
            char folder[PATH_MAX+1];
            _getDumpFolderName(m_requestNumber, folder);
            sprintf(dumpPath, "%s/N3D_main2_GB_out_%dx%d.rgba", _getDumpFolderName(m_requestNumber, folder),
                    m_outputGBMain2.get()->getImgSize().w, m_outputGBMain2.get()->getImgSize().h);
            m_outputGBMain2.get()->saveToFile(dumpPath);
        }

        //Transform to YV12 by MDP
        sp<IImageBuffer> jpsImgMain2;
        bool isLocalCreated = false;
        //De-noise feature may not pass jpsImgMain2, we have to take care of it
        if(n3dOutput.jpsImgMain2) {
            jpsImgMain2 = n3dOutput.jpsImgMain2;
        } else {
            isLocalCreated = true;
            StereoDpUtil::allocImageBuffer(LOG_TAG, eImgFmt_YV12, szSrc, !IS_ALLOC_GB, jpsImgMain2);
        }

        if(NULL != jpsImgMain2.get()) {
            if( !StereoDpUtil::transformImage(m_outputGBMain2.get(), jpsImgMain2.get()) ) {
                MY_LOGE("Convert main2 format fail");
                if(isLocalCreated) {
                    StereoDpUtil::freeImageBuffer(LOG_TAG, jpsImgMain2);
                }
                return false;
            }

            if( checkStereoProperty(PROPERTY_DUMP_CAPTURE) == 1 ) {
                char dumpPath[PATH_MAX+1];
                char folder[PATH_MAX+1];
                sprintf(dumpPath, "%s/N3D_main2_out_%dx%d.yuv", _getDumpFolderName(m_requestNumber, folder),
                        jpsImgMain2.get()->getImgSize().w, jpsImgMain2.get()->getImgSize().h);
                jpsImgMain2.get()->saveToFile(dumpPath);
            }

            //3. Use MDP to resize main2 image, n3dOutput.rectifyImgMain2
            if(StereoDpUtil::transformImage(jpsImgMain2.get(), n3dOutput.rectifyImgMain2)) {
                if( checkStereoProperty(PROPERTY_DUMP_CAPTURE) == 1 ) {
                    char dumpPath[PATH_MAX+1];
                    char folder[PATH_MAX+1];
                    sprintf(dumpPath, "%s/N3D_main2_out_%dx%d.yuv", _getDumpFolderName(m_requestNumber, folder),
                            n3dOutput.rectifyImgMain2->getImgSize().w, n3dOutput.rectifyImgMain2->getImgSize().h);
                    n3dOutput.rectifyImgMain2->saveToFile(dumpPath);
                }
            } else {
                MY_LOGE("Resize main2 image fail");
                if(isLocalCreated) {
                    StereoDpUtil::freeImageBuffer(LOG_TAG, jpsImgMain2);
                }
                return false;
            }

            if(isLocalCreated) {
                StereoDpUtil::freeImageBuffer(LOG_TAG, jpsImgMain2);
            }
        }
    }

    //4. Use MDP to resize main2 mask and truccate, n3dOutput.maskMain2
    {
        AutoProfileUtil profile(LOG_TAG, "Resize main2 and truncate mask");

        ::memset(n3dOutput.maskMain2, 0, RESIZE_LEN);
        sp<IImageBuffer> maskImg;
        if(StereoDpUtil::allocImageBuffer(LOG_TAG, eImgFmt_Y8, szSrc, !IS_ALLOC_GB, maskImg)) {
            //Copy mask to maskImg
            MY_LOGD_IF(LOG_ENABLED, "Copy main2 mask, Y8, len: %d", FULL_LEN);
            ::memcpy((MUINT8*)maskImg.get()->getBufVA(0), m_main2Mask, FULL_LEN);

            sp<IImageBuffer> resizedMask;
            if(StereoDpUtil::allocImageBuffer(LOG_TAG, eImgFmt_Y8, newSize, !IS_ALLOC_GB, resizedMask)) {
                if(StereoDpUtil::transformImage(maskImg.get(), resizedMask.get())) {
                    //Copy data to n3dOutput.maskMain2
                    MY_LOGD_IF(LOG_ENABLED, "Copy resized main2 mask, len: %d", RESIZE_LEN);
                    ::memcpy(n3dOutput.maskMain2,         (MUINT8*)resizedMask.get()->getBufVA(0), RESIZE_LEN);
                } else {
                    MY_LOGE("Resize main2 mask fail");
                    StereoDpUtil::freeImageBuffer(LOG_TAG, maskImg);
                    StereoDpUtil::freeImageBuffer(LOG_TAG, resizedMask);
                    return false;
                }
                StereoDpUtil::freeImageBuffer(LOG_TAG, resizedMask);
            } else {
                StereoDpUtil::freeImageBuffer(LOG_TAG, maskImg);
                MY_LOGE("Cannot alloc resized mask image buffer, Y8, size %dx%d", newSize.w, newSize.h);
                return false;
            }

            StereoDpUtil::freeImageBuffer(LOG_TAG, maskImg);
        } else {
            MY_LOGE("Cannot alloc original mask image buffer, Y8, size %dx%d", szSrc.w, szSrc.h);
            return false;
        }

        //Truncate
        MY_LOGD_IF(LOG_ENABLED, "Truncate resized main2 mask");
        for(MUINT32 i = 0; i < RESIZE_LEN; i++) {
            *(n3dOutput.maskMain2+i) &= 0XFF;
        }
    }

    MY_LOGD_IF(LOG_ENABLED, "-");

    return isSuccess;
}

void
N3D_HAL_IMP::_setN3DSWFECaptureParams(N3D_HAL_PARAM_CAPTURE_SWFE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput)
{
    _setN3DCommonParams(n3dParams, n3dOutput, m_algoProcInfo);
//    MUINT8* addr_mg[MAX_GEO_LEVEL] ;  //SWFEFM_DATA.geo_src_image_main1
//    MUINT8* addr_ag[MAX_GEO_LEVEL] ;  //SWFEFM_DATA.geo_src_image_main2

    _dumpSetProcInfo("", m_algoProcInfo);
}

bool
N3D_HAL_IMP::_initLensInfo(const int32_t SENSOR_INDEX, DAF_TBL_STRUCT *&pAFTable, bool &isAF)
{
    const int DEFAULT_DAC = 0;
    isAF = false;

    IHal3A *pHal3A = MAKE_Hal3A(SENSOR_INDEX, LOG_TAG);
    if(NULL == pHal3A) {
        MY_LOGE("Cannot get 3A HAL of sensor %d", SENSOR_INDEX);
        return false;
    } else {
        FeatureParam_T rFeatureParam;
        if(pHal3A->send3ACtrl(NS3Av3::E3ACtrl_GetSupportedInfo, (MUINTPTR)&rFeatureParam, 0)) {
            isAF = (rFeatureParam.u4MaxFocusAreaNum > 0);
        } else {
            isAF = SENSOR_AF[SENSOR_INDEX];
            MY_LOGW("Cannot query AF ability from 3A, use default setting from custom: %d", isAF);
        }

        if(!isAF) {
            MY_LOGD_IF(LOG_ENABLED, "[FF] Use default min dac of sensor %d: %d", SENSOR_INDEX, DEFAULT_DAC);
        } else {
            pHal3A->send3ACtrl(NS3Av3::E3ACtrl_GetAFDAFTable, (MUINTPTR)&pAFTable, 0);
            if(NULL == pAFTable) {
                MY_LOGE("[AF] Cannot get AF table of sensor %d", SENSOR_INDEX);
                pHal3A->destroyInstance(LOG_TAG);
                return false;
            } else {
                //Since af_mgr::init may run later, we have to wait for it
                for(int nTimes = 10; nTimes > 0; nTimes--) {
                    if (0 != pAFTable->af_dac_min) {
                        break;
                    } else {
                        MY_LOGW("[AF] Waiting for af_dac_min of sensor %d...", SENSOR_INDEX);
                        usleep(20 * 1000);
                    }
                }
            }

            if (0 == pAFTable->af_dac_min) {
                pAFTable->af_dac_min = DEFAULT_DAC;
                MY_LOGW("[AF] Cannot get af_dac_min, set min dac of sensor %d to %d", SENSOR_INDEX, pAFTable->af_dac_min);
            } else {
                pAFTable->is_daf_run |= E_DAF_RUN_STEREO;
                MY_LOGD_IF(LOG_ENABLED, "[AF] Min dac of sensor %d: %d", SENSOR_INDEX, pAFTable->af_dac_min);
            }
        }

        pHal3A->destroyInstance(LOG_TAG);
    }

    return true;
}

bool
N3D_HAL_IMP::_getStereoRemapInfo(STEREO_KERNEL_FLOW_INFO_STRUCT &infoMain1,
                                 STEREO_KERNEL_FLOW_INFO_STRUCT &infoMain2,
                                 ENUM_STEREO_SCENARIO eScenario)
{
    int sensorScenario = getSensorSenario(eScenario);

    int sensorIndex[2];
    StereoSettingProvider::getStereoSensorIndex(sensorIndex[0], sensorIndex[1]);

    int sensorDevIndex[2];
    StereoSettingProvider::getStereoSensorDevIndex(sensorDevIndex[0], sensorDevIndex[1]);

    STEREO_KERNEL_FLOW_INFO_STRUCT *kernelRemapInfos[2] { &infoMain1, &infoMain2 };
    ENUM_STEREO_SENSOR sensorEnums[2] { eSTEREO_SENSOR_MAIN1, eSTEREO_SENSOR_MAIN2 };

    IHalSensorList* sensorList = MAKE_HalSensorList();
    IHalSensor* pIHalSensor = NULL;
    SensorCropWinInfo rSensorCropInfo;

    if(NULL == sensorList) {
        MY_LOGE("Cannot get sensor list");
    } else {
        MUINT32 junkStride;
        MSize   szMain1RRZO;
        MRect   tgCropRect;
        MINT32  err = 0;
        //========= Get main1 size =========
        for(int sensor = 0 ; sensor < 2; sensor++) {
            IHalSensor* pIHalSensor = sensorList->createSensor(LOG_TAG, sensorIndex[sensor]);
            if(NULL == pIHalSensor) {
                MY_LOGE("Cannot get hal sensor of sensor %d", sensorIndex[sensor]);
                err = 1;
            } else {
                ::memset(&rSensorCropInfo, 0, sizeof(SensorCropWinInfo));
                err = pIHalSensor->sendCommand(sensorDevIndex[sensor], SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO,
                                               (MUINTPTR)&sensorScenario, (MUINTPTR)&rSensorCropInfo, 0);
            }

            if(!err) {
                STEREO_KERNEL_FLOW_INFO_STRUCT *kernelInfo = kernelRemapInfos[sensor];

                kernelInfo->pixel_array_width  = rSensorCropInfo.full_w;
                kernelInfo->pixel_array_height = rSensorCropInfo.full_h ;
                kernelInfo->sensor_offset_x0   = rSensorCropInfo.x0_offset ;
                kernelInfo->sensor_offset_y0   = rSensorCropInfo.y0_offset ;
                kernelInfo->sensor_size_w0     = rSensorCropInfo.w0_size ;
                kernelInfo->sensor_size_h0     = rSensorCropInfo.h0_size ;
                kernelInfo->sensor_scale_w     = rSensorCropInfo.scale_w ;
                kernelInfo->sensor_scale_h     = rSensorCropInfo.scale_h ;
                kernelInfo->sensor_offset_x1   = rSensorCropInfo.x1_offset ;
                kernelInfo->sensor_offset_y1   = rSensorCropInfo.y1_offset ;
                kernelInfo->tg_offset_x        = rSensorCropInfo.x2_tg_offset ;
                kernelInfo->tg_offset_y        = rSensorCropInfo.y2_tg_offset ;
                kernelInfo->sensor_size_w1     = rSensorCropInfo.w1_size ;
                kernelInfo->sensor_size_h1     = rSensorCropInfo.h1_size ;
                kernelInfo->tg_size_w          = rSensorCropInfo.w2_tg_size ;
                kernelInfo->tg_size_h          = rSensorCropInfo.h2_tg_size ;
                kernelInfo->mdp_rotate         = (MODULE_ROTATION[sensorIndex[sensor]] == 90 ||
                                                  MODULE_ROTATION[sensorIndex[sensor]] == 270) ;

                //Pre-crop main1 FOV if difference of FOV is not good enough
                const float CROP_RATIO = StereoSettingProvider::getFOVCropRatio();
                if(0 == sensor &&
                   CROP_RATIO < 1.0f &&
                   CROP_RATIO > 0.0f)
                {
                    MSize croppedSize;
                    croppedSize.w = (int)(rSensorCropInfo.w2_tg_size * CROP_RATIO) & ~1;
                    switch(StereoSettingProvider::imageRatio()) {
                    case eRatio_16_9:
                    default:
                        croppedSize.h = croppedSize.w*9/16;
                        break;
                    case eRatio_4_3:
                        croppedSize.h = croppedSize.w*3/4;
                        break;
                    }
                    croppedSize.h &= ~1;

                    MY_LOGD("PreCrop main1: ratio %.2f, size: %dx%d -> %dx%d", CROP_RATIO,
                            rSensorCropInfo.w2_tg_size, rSensorCropInfo.h2_tg_size,
                            croppedSize.w, croppedSize.h);

                    kernelInfo->rrz_usage_width  = croppedSize.w;
                    kernelInfo->rrz_usage_height = croppedSize.h;
                } else {    //No-crop
                    kernelInfo->rrz_usage_width    = rSensorCropInfo.w2_tg_size;      //sensor out width;
                    switch(StereoSettingProvider::imageRatio()) {
                    case eRatio_16_9:
                    default:
                        kernelInfo->rrz_usage_height   = (rSensorCropInfo.w2_tg_size*9/16) & ~1;
                        break;
                    case eRatio_4_3:
                        kernelInfo->rrz_usage_height   = rSensorCropInfo.h2_tg_size;
                        break;
                    }
                }

                StereoSizeProvider::getInstance()->getPass1Size( sensorEnums[sensor],
                                                                 eImgFmt_FG_BAYER10,
                                                                 EPortIndex_RRZO,
                                                                 eScenario,
                                                                 //below are outputs
                                                                 tgCropRect,
                                                                 szMain1RRZO,
                                                                 junkStride);
                MINT32 uMaxRRZSize = (MUINT32)ceil(kernelInfo->rrz_usage_width * RRZ_CAPIBILITY);
                uMaxRRZSize += (uMaxRRZSize & 0x1); //rrz_out_width must be even number
                if(uMaxRRZSize > szMain1RRZO.w) {
                    kernelInfo->rrz_out_width  = uMaxRRZSize;
                } else {
                    kernelInfo->rrz_out_width  = szMain1RRZO.w;
                }

                //rrz_out_height must be an even number
                uMaxRRZSize = (MUINT32)ceil(kernelInfo->rrz_usage_height * RRZ_CAPIBILITY);
                uMaxRRZSize += (uMaxRRZSize & 0x1); //rrz_out_height must be even number
                if(uMaxRRZSize > szMain1RRZO.h) {
                    kernelInfo->rrz_out_height  = uMaxRRZSize;
                } else {
                    kernelInfo->rrz_out_height  = szMain1RRZO.h;
                }

                kernelInfo->rrz_offset_x    = ((rSensorCropInfo.w2_tg_size - kernelInfo->rrz_usage_width )>>1 ) ;
                kernelInfo->rrz_offset_y    = ((rSensorCropInfo.h2_tg_size - kernelInfo->rrz_usage_height)>>1 ) ;
            }

            if(pIHalSensor) {
                pIHalSensor->destroyInstance(LOG_TAG);
            }
        }
    }

    return true;
}

void
N3D_HAL_IMP::_splitMask()
{
    AutoProfileUtil profile(LOG_TAG, "Split Mask");
    //====================================================================
    //  SPLITER: Split and rotate mask according to module orientation
    //  Result is stored in m_main2Mask(2176x1152)
    //====================================================================
    const MUINT32 IMAGE_SIZE = m_outputGBMain2.get()->getImgSize().w * m_outputGBMain2.get()->getImgSize().h;

    // init other memory for save rotate image.
    if(m_main2MaskSize < IMAGE_SIZE) {
        if(NULL != m_main2Mask) {
            delete [] m_main2Mask;
            m_main2Mask = NULL;
        }
    }
    m_main2MaskSize = IMAGE_SIZE;

    if(NULL == m_main2Mask) {
        m_main2Mask = new MUINT8[IMAGE_SIZE];
    }
    ::memset(m_main2Mask, 0, IMAGE_SIZE*sizeof(MUINT8));

    MaskSpliter::splitMask(m_outputGBMain2.get(), m_main2Mask);
}

bool
N3D_HAL_IMP::_loadLensInfo()
{
    if(!RUN_N3D) {
        return true;
    }

    MRESULT err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_LOAD_LENS_INFO,
                                                        (STEREO_SENSOR_PROFILE_FRONT_FRONT == StereoSettingProvider::stereoProfile())
                                                        ? (float *)LENS_INFO_FRONT : (float *)LENS_INFO_REAR,
                                                        NULL);
    if(err) {
        return false;
    }

    return true;
}

bool
N3D_HAL_IMP::_saveNVRAM()
{
    Mutex::Autolock lock(mLock);
    m_nvramUserCount--;
    MY_LOGD_IF(LOG_ENABLED, "NVRAM User: %d", m_nvramUserCount);
    if(m_nvramUserCount > 0) {
        return true;
    } else if(m_nvramUserCount < 0) {
        MY_LOGW("Should not happen: NVRAM user %d", m_nvramUserCount);
        return false;
    }

    if(NULL == m_pVoidGeoData) {
        return false;
    }

    AutoProfileUtil profile(LOG_TAG, "Save NARAM");

    MBOOL bResult = MTRUE;
    MINT32 err = 0; // 0: no error. other value: error.

    if(RUN_N3D) {
        err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SAVE_NVRAM,
                                                    (void*)&m_pVoidGeoData->StereoNvramData.StereoData, NULL);
    }

    if (err)
    {
        MY_LOGE("StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SAVE_NVRAM) fail. error code: %d.", err);
        return false;
    }

    if( RUN_N3D &&
        ( 1 == checkStereoProperty(PROPERTY_DUMP_NVRAM) ||
          (eSTEREO_SCENARIO_CAPTURE == m_eScenario &&
           1 == checkStereoProperty(PROPERTY_DUMP_CAPTURE)
          )
        )
      )
    {
        if(eSTEREO_SCENARIO_CAPTURE == m_eScenario) {
            MY_LOGD("=== Dump saved NVRAM from capture ===");
        } else {
            MY_LOGD("=== Dump saved NVRAM from preview ===");
        }

        char dumpPath[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(dumpPath, "%s/N3D_nvram_out", _getDumpFolderName(m_requestNumber, folder));
        FILE *fpNVRAM = fopen(dumpPath, "wb");
        if(fpNVRAM) {
            fwrite(m_pVoidGeoData->StereoNvramData.StereoData, 1, sizeof(MUINT32)*MTK_STEREO_KERNEL_NVRAM_LENGTH, fpNVRAM);
            fflush(fpNVRAM);
            fclose(fpNVRAM);
        } else {
            MY_LOGE("Cannot dump NVRAM to %s, error: %s", dumpPath, strerror(errno));
        }
    }

    int32_t main1DevIdx, main2DevIdx;
    StereoSettingProvider::getStereoSensorDevIndex(main1DevIdx, main2DevIdx);
    err = MAKE_NvBufUtil().write(CAMERA_NVRAM_DATA_GEOMETRY, main1DevIdx);
    if (err) {
        MY_LOGE("Write to NVRAM fail.");
        bResult = MFALSE;
    }

    m_pVoidGeoData = NULL;

    return bResult;
}

bool
N3D_HAL_IMP::_loadNVRAM()
{
    Mutex::Autolock lock(mLock);
    if(m_nvramUserCount < 0) {
        MY_LOGW("Should not happen: NVRAM user %d", m_nvramUserCount);
    }

    AutoProfileUtil profile(LOG_TAG, "Load NVRAM");

    MY_LOGD_IF(LOG_ENABLED, "NVRAM User: %d", m_nvramUserCount);
    m_nvramUserCount++;
    MINT32 err = 0;
    if(NULL == m_pVoidGeoData) {
        int32_t main1DevIdx, main2DevIdx;
        StereoSettingProvider::getStereoSensorDevIndex(main1DevIdx, main2DevIdx);
        err = MAKE_NvBufUtil().getBufAndRead(CAMERA_NVRAM_DATA_GEOMETRY, main1DevIdx, (void*&)m_pVoidGeoData);
        if(err ||
           NULL == m_pVoidGeoData)
        {
            MY_LOGE("Read NVRAM fail, data: %p", m_pVoidGeoData);
            return false;
        }
#ifdef GTEST
        if(NULL == m_pVoidGeoData) {
            m_pVoidGeoData = new NVRAM_CAMERA_GEOMETRY_STRUCT;
        }
        // === Prepare learning data ===
        const char *LEARN_FILE = "/data/nativetest/VSDoF_HAL_Test/N3D_UT/in/LearnINFO.n3d";
        MY_LOGD_IF(LOG_ENABLED, "Load Learning data from %s(len: %d)", LEARN_FILE, MTK_STEREO_KERNEL_NVRAM_LENGTH);
        FILE *fdata = fopen(LEARN_FILE, "r") ;
        if( fdata )
        {
            memset( &m_pVoidGeoData->StereoNvramData.StereoData, 0, sizeof(MINT32)*MTK_STEREO_KERNEL_NVRAM_LENGTH );
            for( int i=0 ; i < MTK_STEREO_KERNEL_NVRAM_LENGTH ; i++ )
            {
                if(EOF == fscanf( fdata, "%d", &m_pVoidGeoData->StereoNvramData.StereoData[i] ) ) {
                    break;
                }
            }

            fclose(fdata) ;
        } else {
            if(errno) {
                MY_LOGE("Cannot read learning data, error: %s", ::strerror(errno));
            } else {
                MY_LOGE("Cannot read learning data");
            }
        }
#endif
    }

    if( RUN_N3D &&
        ( 1 == checkStereoProperty(PROPERTY_DUMP_NVRAM) ||
          (eSTEREO_SCENARIO_CAPTURE == m_eScenario &&
           1 == checkStereoProperty(PROPERTY_DUMP_CAPTURE)
          )
        )
      )
    {
        if(eSTEREO_SCENARIO_CAPTURE == m_eScenario) {
            MY_LOGD("=== Dump loaded NVRAM from capture ===");
        } else {
            MY_LOGD("=== Dump loaded NVRAM from preview ===");
        }

        char dumpPath[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(dumpPath, "%s/N3D_nvram_in", _getDumpFolderName(m_requestNumber, folder));
        FILE *fpNVRAM = fopen(dumpPath, "wb");
        if(fpNVRAM) {
            fwrite(m_pVoidGeoData->StereoNvramData.StereoData, 1, sizeof(MUINT32)*MTK_STEREO_KERNEL_NVRAM_LENGTH, fpNVRAM);
            fflush(fpNVRAM);
            fclose(fpNVRAM);
        } else {
            MY_LOGE("Cannot dump NVRAM to %s, error: %s", dumpPath, strerror(errno));
        }
    }

    if(RUN_N3D) {
        err = m_pStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_LOAD_NVRAM,
                                                    (void*)&m_pVoidGeoData->StereoNvramData.StereoData, NULL);
        if (err)
        {
            MY_LOGE("StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_LOAD_NVRAM) fail. error code: %d.", err);
            return false;
        }
    }

    return true;
}

bool
N3D_HAL_IMP::_syncInternalNVRAM(MTKStereoKernel *pSrcStereoDrv, MTKStereoKernel *pDstStereoDrv)
{
    if(!RUN_N3D) {
        return true;
    }

    AutoProfileUtil profile(LOG_TAG, "Sync NVRAM");

    MINT32 err = 0; // 0: no error. other value: error.
    //Step 1: get nvram data from pSrcStereoDrv
    if(pSrcStereoDrv) {
        err = pSrcStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SAVE_NVRAM,
                                                     (void*)&m_pVoidGeoData->StereoNvramData.StereoData, NULL);
        if (err)
        {
            MY_LOGE("StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_SAVE_NVRAM) fail. error code: %d.", err);
            return false;
        }
    } else {
        MY_LOGW("No source stereo drv");
        err = 1;
    }

    //Step 2: save nvram data to pDstStereoDrv
    if(!err) {
        if(pDstStereoDrv) {
            err = pDstStereoDrv->StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_LOAD_NVRAM,
                                                         (void*)&m_pVoidGeoData->StereoNvramData.StereoData, NULL);
            if (err)
            {
                MY_LOGE("StereoKernelFeatureCtrl(STEREO_KERNEL_FEATURE_LOAD_NVRAM) fail. error code: %d.", err);
                return false;
            }
        } else {
            MY_LOGW("No dst stereo drv");
        }
    }

    return true;
}

void
N3D_HAL_IMP::_compressMask(std::vector<RUN_LENGTH_DATA> &compressedMask)
{
    AutoProfileUtil profile(LOG_TAG, "Compress mask");

    compressedMask.clear();

    const int IMAGE_SIZE = m_algoInitInfo.iio_main.out_w * m_algoInitInfo.iio_main.out_h;
    MUINT32 len = 0;
    MUINT32 offset = 0;

    const int CMP_LEN = 128;
    MUINT8 *FF_MASK = new MUINT8[CMP_LEN];
    ::memset(FF_MASK, 0xFF, CMP_LEN);

    MUINT8 *mask = m_main2Mask;
    for(int i = 0; i < IMAGE_SIZE; i += CMP_LEN) {
        if(0 == memcmp(mask, FF_MASK, CMP_LEN)) {
            if(0 == len) {
                offset = i;
            }

            len += CMP_LEN;
            mask += CMP_LEN;
        } else {
            for(int j = 0; j < CMP_LEN; j++, mask++) {
                if(0 != *mask) {
                    if(0 != len) {
                        ++len;
                    } else {
                        len = 1;
                        offset = i+j;
                    }
                } else {
                    if(0 != len) {
                        compressedMask.push_back(RUN_LENGTH_DATA(offset, len));
                        len = 0;
                    }
                }
            }
        }
    }

    if(0 != len) {
        compressedMask.push_back(RUN_LENGTH_DATA(offset, len));
    }

    delete [] FF_MASK;
}

const char *
N3D_HAL_IMP::_prepareStereoExtraData()
{
    AutoProfileUtil profile(LOG_TAG, "Encode extra data");

    if(m_stereoExtraData) {
        delete m_stereoExtraData;
        m_stereoExtraData = NULL;
    }

//    "JPS_size": {
//        "width": 4352,
//        "height": 1152
//    },
    Document document(kObjectType);
    Document::AllocatorType& allocator = document.GetAllocator();

    Value JPS_size(kObjectType);
    JPS_size.AddMember("width", m_algoInitInfo.iio_main.inp_w*2, allocator);
    JPS_size.AddMember("height", m_algoInitInfo.iio_main.inp_h, allocator);
    document.AddMember("JPS_size", JPS_size, allocator);

//    "output_image_size" : {
//        "width": 2176,
//        "height": 1152
//    },
    Value output_image_size(kObjectType);
    output_image_size.AddMember("width", m_algoInitInfo.iio_main.out_w, allocator);
    output_image_size.AddMember("height", m_algoInitInfo.iio_main.out_h, allocator);
    document.AddMember("output_image_size", output_image_size, allocator);

//    "main_cam_align_shift" : {
//        "x": 30,
//        "y": 10
//    },
    Value main_cam_align_shift(kObjectType);
    main_cam_align_shift.AddMember("x", ((MFLOAT *)m_algoResult.out_p[STEREO_KERNEL_OUTPUT_DEPTH])[0], allocator);
    main_cam_align_shift.AddMember("y", ((MFLOAT *)m_algoResult.out_p[STEREO_KERNEL_OUTPUT_DEPTH])[1], allocator);
    document.AddMember("main_cam_align_shift", main_cam_align_shift, allocator);

//    "input_image_size": {
//        "width": 1920,
//        "height": 1080
//    },
    Value input_image_size(kObjectType);
    input_image_size.AddMember("width", m_algoInitInfo.iio_main.src_w, allocator);
    input_image_size.AddMember("height", m_algoInitInfo.iio_main.src_h, allocator);
    document.AddMember("input_image_size", input_image_size, allocator);

//    "capture_orientation": {
//        "orientations_values": ["0: none", "1: flip_horizontal", "2: flip_vertical", "4: 90", "3: 180", "7: 270"],
//        "orientation": 0
//    },
    int cap_ori = 0;
    switch(m_captureOrientation){
        case 90:
            cap_ori = 4;
            break;
        case 180:
            cap_ori = 3;
            break;
        case 270:
            cap_ori = 7;
            break;
        case 0:
        default:
            cap_ori = 0;
            break;
    }

    Value capture_orientation(kObjectType);
    Value orientations_values(kArrayType);
    orientations_values.PushBack(Value("0: none").Move(), allocator);
    // orientations_values.PushBack(Value("1: flip_horizontal").Move(), allocator);
    // orientations_values.PushBack(Value("2: flip_vertical").Move(), allocator);
    orientations_values.PushBack(Value("4: 90 degrees CW").Move(), allocator);
    orientations_values.PushBack(Value("3: 180 degrees CW").Move(), allocator);
    orientations_values.PushBack(Value("7: 270 degrees CW").Move(), allocator);
    capture_orientation.AddMember("orientations_values", orientations_values, allocator);
    capture_orientation.AddMember("orientation", Value(cap_ori).Move(), allocator);
    document.AddMember("capture_orientation", capture_orientation, allocator);

//    "depthmap_orientation": {
//        "depthmap_orientation_values": ["0: none", "90: 90 degrees CW", "180: 180 degrees CW", "270: 270 degrees CW"],
//        "orientation": 0
//    },
    Value depthmap_orientation(kObjectType);
    Value depthmap_orientation_values(kArrayType);
    depthmap_orientation_values.PushBack(Value("0: none").Move(), allocator);
    depthmap_orientation_values.PushBack(Value("90: 90 degrees CW").Move(), allocator);
    depthmap_orientation_values.PushBack(Value("180: 180 degrees CW").Move(), allocator);
    depthmap_orientation_values.PushBack(Value("270: 270 degrees CW").Move(), allocator);
    depthmap_orientation.AddMember("depthmap_orientation_values", depthmap_orientation_values, allocator);
    depthmap_orientation.AddMember("orientation", Value(m_captureOrientation-StereoSettingProvider::getModuleRotation()).Move(), allocator);
    document.AddMember("depthmap_orientation", depthmap_orientation, allocator);

//    "sensor_relative_position": {
//        "relative_position_values": ["0: main-minor", "1: minor-main"],
//        "relative_position": 0
//    },
    Value sensor_relative_position(kObjectType);
    Value relative_position_values(kArrayType);
    relative_position_values.PushBack(Value("0: main-minor").Move(), allocator);
    relative_position_values.PushBack(Value("1: minor-main").Move(), allocator);
    sensor_relative_position.AddMember("relative_position_values", relative_position_values, allocator);
    sensor_relative_position.AddMember("relative_position", Value((int)StereoSettingProvider::getSensorRelativePosition()).Move(), allocator);
    document.AddMember("sensor_relative_position", sensor_relative_position, allocator);

//    "focus_roi": {
//        "top": 0,
//        "left": 10,
//        "bottom": 10,
//        "right": 30
//    },
    AF_WIN_COORDINATE_STRUCT apAFWin(0, 0, 0, 0);
    if(m_isAF[0] &&
       NULL != m_pAFTable[0])
    {
        const int AF_INDEX = m_magicNumber[0] % DAF_TBL_QLEN;
        AF_WIN_COORDINATE_STRUCT afWin(m_pAFTable[0]->daf_vec[AF_INDEX].af_win_start_x, m_pAFTable[0]->daf_vec[AF_INDEX].af_win_start_y,
                                       m_pAFTable[0]->daf_vec[AF_INDEX].af_win_end_x, m_pAFTable[0]->daf_vec[AF_INDEX].af_win_end_y);
        _transferAFWin(afWin, apAFWin);
    }
    Value focus_roi(kObjectType);
    focus_roi.AddMember("top",      Value( apAFWin.af_win_start_y ).Move(), allocator);
    focus_roi.AddMember("left",     Value( apAFWin.af_win_start_x ).Move(), allocator);
    focus_roi.AddMember("bottom",   Value( apAFWin.af_win_end_y   ).Move(), allocator);
    focus_roi.AddMember("right",    Value( apAFWin.af_win_end_x   ).Move(), allocator);
    document.AddMember("focus_roi", focus_roi, allocator);

//    "verify_geo_data": {
//        "quality_level_values": ["0: PASS","1: Cond.Pass","2: FAIL"],
//        "quality_level": 0,
//        "statistics": [0,0,0,0,0,0]
//    },
    MINT32 *verityOutputs = (MINT32*)m_algoResult.out_p[STEREO_KERNEL_OUTPUT_VERIFY];
    Value verify_geo_data(kObjectType);
    Value quality_level_values(kArrayType);
    quality_level_values.PushBack(Value("0: PASS").Move(), allocator);
    quality_level_values.PushBack(Value("1: Cond.Pass").Move(), allocator);
    quality_level_values.PushBack(Value("2: FAIL").Move(), allocator);
    verify_geo_data.AddMember("quality_level_values", quality_level_values, allocator);
    verify_geo_data.AddMember("quality_level", Value(verityOutputs[0]).Move(), allocator);
    Value geo_statistics(kArrayType);
    for(int i = 0; i < 6; i++) {
        geo_statistics.PushBack(Value(verityOutputs[i+2]).Move(), allocator);
    }
    verify_geo_data.AddMember("statistics", geo_statistics, allocator);
    document.AddMember("verify_geo_data", verify_geo_data, allocator);

//    "verify_pho_data": {
//        "quality_level_values": ["0: PASS","1: Cond.Pass","2: FAIL"],
//        "quality_level": 0,
//        "statistics": [0,0,0,0]
//    },
    Value verify_pho_data(kObjectType);
    Value pho_quality_level_values(kArrayType);
    pho_quality_level_values.PushBack(Value("0: PASS").Move(), allocator);
    pho_quality_level_values.PushBack(Value("1: Cond.Pass").Move(), allocator);
    pho_quality_level_values.PushBack(Value("2: FAIL").Move(), allocator);
    verify_pho_data.AddMember("quality_level_values", pho_quality_level_values, allocator);
    verify_pho_data.AddMember("quality_level", Value(verityOutputs[1]).Move(), allocator);
    Value pho_statistics(kArrayType);
    for(int i = 0; i < 4; i++) {
        pho_statistics.PushBack(Value(verityOutputs[i+8]).Move(), allocator);
    }
    verify_pho_data.AddMember("statistics", pho_statistics, allocator);
    document.AddMember("verify_pho_data", verify_pho_data, allocator);

//    "verify_mtk_cha": {
//        "check_values": ["0: PASS","1: FAIL"],
//        "check": 0,
//        "score": 0
//    },
    Value verify_mtk_cha(kObjectType);
    Value verify_mtk_cha_values(kArrayType);
    verify_mtk_cha_values.PushBack(Value("0: PASS").Move(), allocator);
    verify_mtk_cha_values.PushBack(Value("1: FAIL").Move(), allocator);
    verify_mtk_cha.AddMember("check_values", verify_mtk_cha_values, allocator);
    verify_mtk_cha.AddMember("check", Value(verityOutputs[12]).Move(), allocator);
    verify_mtk_cha.AddMember("score", Value(verityOutputs[13]).Move(), allocator);
    document.AddMember("verify_mtk_cha", verify_mtk_cha, allocator);

//    "depth_buffer_size": {
//        "width": 480,
//        "height": 270
//    },
    Value depth_buffer_size(kObjectType);
    MSize szDepthMap = StereoSizeProvider::getInstance()->getBufferSize(E_DEPTH_MAP, m_eScenario);
    depth_buffer_size.AddMember("width", szDepthMap.w, allocator);
    depth_buffer_size.AddMember("height", szDepthMap.h, allocator);
    document.AddMember("depth_buffer_size", depth_buffer_size, allocator);

//    "ldc_size": {
//        "width": 272,
//        "height": 144
//    },
    Value ldc_size(kObjectType);
    MSize szLDC = StereoSizeProvider::getInstance()->getBufferSize(E_LDC, m_eScenario);
    ldc_size.AddMember("width", szLDC.w, allocator);
    ldc_size.AddMember("height", szLDC.h, allocator);
    document.AddMember("ldc_size", ldc_size, allocator);

//    "GFocus": {
//        "BlurAtInfinity": 0.014506519,
//        "FocalDistance": 25.612852,
//        "FocalPointX": 0.5,
//        "FocalPointY": 0.5
//    },
    Value GFocus(kObjectType);
    GFocus.AddMember("BlurAtInfinity", 0.014506519, allocator);
    GFocus.AddMember("FocalDistance", 25.612852, allocator);
    GFocus.AddMember("FocalPointX", 0.5, allocator);
    GFocus.AddMember("FocalPointY", 0.5, allocator);
    document.AddMember("GFocus", GFocus, allocator);

//    "GImage" : {
//        "Mime": "image/jpeg"
//    },
    Value GImage(kObjectType);
    GImage.AddMember("Mime", "image/jpeg", allocator);
    document.AddMember("GImage", GImage, allocator);

//    "GDepth": {
//        "Format": "RangeInverse",
//        "Near": 15.12828254699707,
//        "Far": 97.0217514038086,
//        "Mime": "image/png"
//    },
    Value GDepth(kObjectType);
    GDepth.AddMember("Format", "RangeInverse", allocator);
    GDepth.AddMember("Near", 15.12828254699707, allocator);
    GDepth.AddMember("Far", 97.0217514038086, allocator);
    GDepth.AddMember("Mime", "image/png", allocator);
    document.AddMember("GDepth", GDepth, allocator);

//    "mask_info" : {
//        "width":2176,
//        "height":1152,
//        "mask description": "Data(0xFF), format: [offset,length]",
//        "mask": [[28,1296],[1372,1296],[2716,1296],...]
//    }
    Value mask_info(kObjectType);
    mask_info.AddMember("width", m_algoInitInfo.iio_main.out_w, allocator);
    mask_info.AddMember("height", m_algoInitInfo.iio_main.out_h, allocator);
    mask_info.AddMember("mask description", "Data(0xFF), format: [offset,length]", allocator);

    std::vector<RUN_LENGTH_DATA> runLengthMaskData;
    _compressMask(runLengthMaskData);

    Value mask(kArrayType);
    for(std::vector<RUN_LENGTH_DATA>::iterator it = runLengthMaskData.begin(); it != runLengthMaskData.end(); ++it) {
        Value maskData(kArrayType);
        maskData.PushBack(Value(it->offset).Move(), allocator);
        maskData.PushBack(Value(it->len).Move(), allocator);
        mask.PushBack(maskData.Move(), allocator);
    }
    mask_info.AddMember("mask", mask, allocator);
    document.AddMember("mask_info", mask_info, allocator);

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    document.Accept(writer);    // Accept() traverses the DOM and generates Handler events.

    const char *stereoExtraData = sb.GetString();
    if(stereoExtraData) {
        const int STR_LEN = strlen(stereoExtraData);
        if(STR_LEN > 0) {
            m_stereoExtraData = new char[STR_LEN+1];
            strcpy(m_stereoExtraData, stereoExtraData);
        }
    }

    if(checkStereoProperty(PROPERTY_DUMP_JSON) == 1) {
        _mkdir();

        MY_LOGD("m_stereoExtraData %p, length: %d", m_stereoExtraData, strlen(m_stereoExtraData));
        char fileName[PATH_MAX+1];
        char folder[PATH_MAX+1];
        sprintf(fileName, "%s/N3D_ExtraData.json", _getDumpFolderName(m_requestNumber, folder));
        FILE *fp = fopen(fileName, "w");
        if(fp) {
            fwrite(m_stereoExtraData, 1, strlen(m_stereoExtraData)+1, fp);
            fflush(fp);
            fclose(fp);
        }
    }

    return m_stereoExtraData;
}

char *
N3D_HAL_IMP::_getDumpFolderName(int folderNumber, char path[])
{
    const char *DIRECTION = (StereoSettingProvider::stereoProfile() == STEREO_SENSOR_PROFILE_FRONT_FRONT) ? "Front" : "Rear";
    if(eSTEREO_SCENARIO_CAPTURE == m_eScenario) {
        sprintf(path, "/sdcard/vsdof/capture/%s/%d/N3DNode", DIRECTION, folderNumber); //tmp solution
    } else {
        sprintf(path, "/sdcard/vsdof/pv_vr/%s/%d/N3DNode", DIRECTION, folderNumber);
    }

    return path;
}


void
N3D_HAL_IMP::_mkdir()
{
    char path[PATH_MAX+1] = {0};
    _getDumpFolderName(m_requestNumber, path);
    MY_LOGD("mkdir %s", path);
    #define DELIM "/"

    struct stat st;
    ::memset(&st, 0, sizeof(struct stat));
    if(stat(path, &st) == -1) {
        char *folder = strtok(path, DELIM);
        char createPath[PATH_MAX+1] = {0};
        createPath[0] = '/';
        while(folder) {
            strcat(createPath, folder);
            if (mkdir (createPath, 0755) != 0 && errno != EEXIST) {
                MY_LOGE("Create %s failed, error: %s", createPath, strerror(errno));
                break;
            }

            folder = strtok(NULL, DELIM);
            strcat(createPath, DELIM);
        }
    }
}

// Logger
void
N3D_HAL_IMP::_dumpInitInfo(const STEREO_KERNEL_SET_ENV_INFO_STRUCT &initInfo)
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("========= N3D Init Info =========");
    MY_LOGD("[scenario]     %d", initInfo.scenario);

    // ALGORITHM INPUT and SbS OUTPUT
    MY_LOGD("[iio_main.src_w]   %d", initInfo.iio_main.src_w);
    MY_LOGD("[iio_main.src_h]   %d", initInfo.iio_main.src_h);
    MY_LOGD("[iio_main.out_w]   %d", initInfo.iio_main.out_w);
    MY_LOGD("[iio_main.out_h]   %d", initInfo.iio_main.out_h);
    MY_LOGD("[iio_main.inp_w]   %d", initInfo.iio_main.inp_w);
    MY_LOGD("[iio_main.inp_h]   %d", initInfo.iio_main.inp_h);

    MY_LOGD("[iio_auxi.src_w]   %d", initInfo.iio_auxi.src_w);
    MY_LOGD("[iio_auxi.src_h]   %d", initInfo.iio_auxi.src_h);
    MY_LOGD("[iio_auxi.inp_w]   %d", initInfo.iio_auxi.inp_w);
    MY_LOGD("[iio_auxi.inp_h]   %d", initInfo.iio_auxi.inp_h);
    MY_LOGD("[iio_auxi.out_w]   %d", initInfo.iio_auxi.out_w);
    MY_LOGD("[iio_auxi.out_h]   %d", initInfo.iio_auxi.out_h);

    MY_LOGD("[iio_cmp.src_w]    %d", initInfo.iio_cmp.src_w);
    MY_LOGD("[iio_cmp.src_h]    %d", initInfo.iio_cmp.src_h);
    MY_LOGD("[iio_cmp.inp_w]    %d", initInfo.iio_cmp.inp_w);
    MY_LOGD("[iio_cmp.inp_h]    %d", initInfo.iio_cmp.inp_h);
    MY_LOGD("[iio_cmp.out_w]    %d", initInfo.iio_cmp.out_w);
    MY_LOGD("[iio_cmp.out_h]    %d", initInfo.iio_cmp.out_h);

    // HWFE INPUT - the actual size for HWFE (after SRZ)
    MY_LOGD("[geo_info]  %d", initInfo.geo_info);

    char logPrefix[32];
    int i = 0;
    for(i = 0; i < MAX_GEO_LEVEL; i++) {
        if(initInfo.geo_img[i].size <= 0) {
            continue;
        }

        MY_LOGD("[geo_img][%d][size] %d", i, initInfo.geo_img[i].size);

        sprintf(logPrefix, "[geo_img][%d][main]", i);
        _dumpImgInfo(logPrefix, initInfo.geo_img[i].img_main);

        sprintf(logPrefix, "[geo_img][%d][auxi]", i);
        _dumpImgInfo(logPrefix, initInfo.geo_img[i].img_auxi);
    }

    // COLOR CORRECTION INPUT
    _dumpImgInfo("[pho_img]", initInfo.pho_img);

    // Learning
    MY_LOGD("[fov_main]         h: %.1f v: %.1f", initInfo.fov_main[0], initInfo.fov_main[1]);
    MY_LOGD("[fov_auxi]         h: %.1f v: %.1f", initInfo.fov_auxi[0], initInfo.fov_auxi[1]);
    MY_LOGD("[baseline]         %.1f", initInfo.baseline);
    std::string s = std::bitset<11>(initInfo.system_cfg).to_string();
    MY_LOGD("[system_cfg]       %s", s.c_str());
    MY_LOGD("[dac_start_main]   %d", initInfo.dac_start_main);
    MY_LOGD("[dac_start_auxi]   %d", initInfo.dac_start_auxi);

    // Learning Coordinates RE-MAPPING
    _dumpRemapInfo("[flow_main]", initInfo.flow_main);
    _dumpRemapInfo("[flow_auxi]", initInfo.flow_auxi);

    // OUTPUT after Initialization
    MUINT32 working_buffer_size ;
    MY_LOGD("[working_buffer_size]  %d", initInfo.working_buffer_size);
}

void
N3D_HAL_IMP::_dumpImgInfo(const char *prefix, const STEREO_KERNEL_IMG_INFO_STRUCT &imgInfo)
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("%s[width]      %d", prefix, imgInfo.width);
    MY_LOGD("%s[height]     %d", prefix, imgInfo.height);
    MY_LOGD("%s[depth]      %d", prefix, imgInfo.depth);
    MY_LOGD("%s[stride]     %d", prefix, imgInfo.stride);
    MY_LOGD("%s[format]     %d", prefix, imgInfo.format);
    MY_LOGD("%s[act_width]  %d", prefix, imgInfo.act_width);
    MY_LOGD("%s[act_height] %d", prefix, imgInfo.act_height);
    MY_LOGD("%s[offset_x]   %d", prefix, imgInfo.offset_x);
    MY_LOGD("%s[offset_y]   %d", prefix, imgInfo.offset_y);
}

void
N3D_HAL_IMP::_dumpRemapInfo(const char *prefix, const STEREO_KERNEL_FLOW_INFO_STRUCT &remapInfo)
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("%s[pixel_array_width]    %d", prefix, remapInfo.pixel_array_width);
    MY_LOGD("%s[pixel_array_height]   %d", prefix, remapInfo.pixel_array_height);
    MY_LOGD("%s[sensor_offset_x0]     %d", prefix, remapInfo.sensor_offset_x0);
    MY_LOGD("%s[sensor_offset_y0]     %d", prefix, remapInfo.sensor_offset_y0);
    MY_LOGD("%s[sensor_size_w0]       %d", prefix, remapInfo.sensor_size_w0);
    MY_LOGD("%s[sensor_size_h0]       %d", prefix, remapInfo.sensor_size_h0);
    MY_LOGD("%s[sensor_scale_w]       %d", prefix, remapInfo.sensor_scale_w);
    MY_LOGD("%s[sensor_scale_h]       %d", prefix, remapInfo.sensor_scale_h);
    MY_LOGD("%s[sensor_offset_x1]     %d", prefix, remapInfo.sensor_offset_x1);
    MY_LOGD("%s[sensor_offset_y1]     %d", prefix, remapInfo.sensor_offset_y1);
    MY_LOGD("%s[tg_offset_x]          %d", prefix, remapInfo.tg_offset_x);
    MY_LOGD("%s[tg_offset_y]          %d", prefix, remapInfo.tg_offset_y);
    MY_LOGD("%s[sensor_size_w1]       %d", prefix, remapInfo.sensor_size_w1);
    MY_LOGD("%s[sensor_size_h1]       %d", prefix, remapInfo.sensor_size_h1);
    MY_LOGD("%s[tg_size_w]            %d", prefix, remapInfo.tg_size_w);
    MY_LOGD("%s[tg_size_h]            %d", prefix, remapInfo.tg_size_h);
    MY_LOGD("%s[mdp_rotate]           %d", prefix, remapInfo.mdp_rotate);
    MY_LOGD("%s[rrz_offset_x]         %d", prefix, remapInfo.rrz_offset_x);
    MY_LOGD("%s[rrz_offset_y]         %d", prefix, remapInfo.rrz_offset_y);
    MY_LOGD("%s[rrz_usage_width]      %d", prefix, remapInfo.rrz_usage_width);
    MY_LOGD("%s[rrz_usage_height]     %d", prefix, remapInfo.rrz_usage_height);
    MY_LOGD("%s[rrz_out_width]        %d", prefix, remapInfo.rrz_out_width);
    MY_LOGD("%s[rrz_out_height]       %d", prefix, remapInfo.rrz_out_height);
}

void
N3D_HAL_IMP::_dumpTuningInfo(const char *prefix, const STEREO_KERNEL_TUNING_PARA_STRUCT &tuningInfo)
{
    if(!LOG_ENABLED) {
        return;
    }

    std::string s = std::bitset<32>(tuningInfo.p_tune).to_string();
    MY_LOGD("%s[p_tune]   %s", prefix, s.c_str());
    s = std::bitset<32>(tuningInfo.s_tune).to_string();
    MY_LOGD("%s[s_tune]   %s", prefix, s.c_str());
}

void
N3D_HAL_IMP::_dumpSetProcInfo(const char *prefix, const STEREO_KERNEL_SET_PROC_INFO_STRUCT &setprocInfo)
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("%s[Magic]      %d, %d", prefix, m_magicNumber[0], m_magicNumber[1]);
    MY_LOGD("%s[Request]    %d", prefix, m_requestNumber);
    MY_LOGD("%s[addr_ms]    %p", prefix, setprocInfo.addr_ms);
    MY_LOGD("%s[addr_md]    %p", prefix, setprocInfo.addr_md);
    MY_LOGD("%s[addr_mm]    %p", prefix, setprocInfo.addr_mm);
    MY_LOGD("%s[addr_ml]    %p", prefix, setprocInfo.addr_ml);

    MY_LOGD("%s[addr_as]    %p", prefix, setprocInfo.addr_as);
    MY_LOGD("%s[addr_ad]    %p", prefix, setprocInfo.addr_ad);
    MY_LOGD("%s[addr_am]    %p", prefix, setprocInfo.addr_am);

    MY_LOGD("%s[addr_mp]    %p", prefix, setprocInfo.addr_mp);
    MY_LOGD("%s[addr_ap]    %p", prefix, setprocInfo.addr_ap);

    MY_LOGD("%s[addr_mg]    %p %p %p", prefix, setprocInfo.addr_mg[0], setprocInfo.addr_mg[1], setprocInfo.addr_mg[2]);
    MY_LOGD("%s[addr_ag]    %p %p %p", prefix, setprocInfo.addr_ag[0], setprocInfo.addr_ag[1], setprocInfo.addr_ag[2]);

    MY_LOGD("%s[addr_me]    %p %p %p", prefix, setprocInfo.addr_me[0], setprocInfo.addr_me[1], setprocInfo.addr_me[2]);
    MY_LOGD("%s[addr_ae]    %p %p %p", prefix, setprocInfo.addr_ae[0], setprocInfo.addr_ae[1], setprocInfo.addr_ae[2]);
    MY_LOGD("%s[addr_rl]    %p %p %p", prefix, setprocInfo.addr_rl[0], setprocInfo.addr_rl[1], setprocInfo.addr_rl[2]);
    MY_LOGD("%s[addr_lr]    %p %p %p", prefix, setprocInfo.addr_lr[0], setprocInfo.addr_lr[1], setprocInfo.addr_lr[2]);

    MY_LOGD("%s[dac_i#0]    %d", prefix, setprocInfo.af_main.dac_i);
    MY_LOGD("%s[dac_v#0]    %d", prefix, setprocInfo.af_main.dac_v);
    MY_LOGD("%s[dac_c#0]    %d", prefix, setprocInfo.af_main.dac_c);
    MY_LOGD("%s[dac_w#0]    %d %d %d %d", prefix,
                                          setprocInfo.af_main.dac_w[0], setprocInfo.af_main.dac_w[1],
                                          setprocInfo.af_main.dac_w[2], setprocInfo.af_main.dac_w[3]);
    MY_LOGD("%s[dac_i#1]    %d", prefix, setprocInfo.af_auxi.dac_i);
    MY_LOGD("%s[dac_v#1]    %d", prefix, setprocInfo.af_auxi.dac_v);
    MY_LOGD("%s[dac_c#1]    %d", prefix, setprocInfo.af_auxi.dac_c);
    MY_LOGD("%s[dac_w#1]    %d %d %d %d", prefix,
                                          setprocInfo.af_auxi.dac_w[0], setprocInfo.af_auxi.dac_w[1],
                                          setprocInfo.af_auxi.dac_w[2], setprocInfo.af_auxi.dac_w[3]);

    if(eSTEREO_SCENARIO_RECORD == m_eScenario) {
        MY_LOGD("%s[eis]        %d %d %d %d", prefix, setprocInfo.eis[0], setprocInfo.eis[1], setprocInfo.eis[2], setprocInfo.eis[3]);
    }

    MY_LOGD("%s[iio_cap.src_w]    %d", prefix, setprocInfo.iio_cap.src_w);
    MY_LOGD("%s[iio_cap.src_h]    %d", prefix, setprocInfo.iio_cap.src_h);
    MY_LOGD("%s[iio_cap.inp_w]    %d", prefix, setprocInfo.iio_cap.inp_w);
    MY_LOGD("%s[iio_cap.inp_h]    %d", prefix, setprocInfo.iio_cap.inp_h);
    MY_LOGD("%s[iio_cap.out_w]    %d", prefix, setprocInfo.iio_cap.out_w);
    MY_LOGD("%s[iio_cap.out_h]    %d", prefix, setprocInfo.iio_cap.out_h);

    if(eSTEREO_SCENARIO_CAPTURE == m_eScenario) {
        MY_LOGD("%s[src_gb]     GraphicBuffer %p, EGLImage %p", prefix, setprocInfo.src_gb.mGraphicBuffer, setprocInfo.src_gb.mEGLImage);
        MY_LOGD("%s[dst_gb]     GraphicBuffer %p, EGLImage %p", prefix, setprocInfo.dst_gb.mGraphicBuffer, setprocInfo.dst_gb.mEGLImage);
        MY_LOGD("%s[eglDisplay] %p", prefix, setprocInfo.eglDisplay);
        MY_LOGD("%s[InputGB]    %p", prefix, setprocInfo.InputGB);
        MY_LOGD("%s[OutputGB]   %p", prefix, setprocInfo.OutputGB);
    }
}

void
N3D_HAL_IMP::_dumpResult(const char *prefix, const STEREO_KERNEL_RESULT_STRUCT &n3dResult)
{
    if(!LOG_ENABLED) {
        return;
    }

    MY_LOGD("%s[out_n]       %d %d %d %d %d %d",
            prefix, n3dResult.out_n[0], n3dResult.out_n[1], n3dResult.out_n[2], n3dResult.out_n[3], n3dResult.out_n[4], n3dResult.out_n[5]);
    MY_LOGD("%s[out_p]       %p %p %p %p %p %p",
            prefix, n3dResult.out_p[0], n3dResult.out_p[1], n3dResult.out_p[2], n3dResult.out_p[3], n3dResult.out_p[4], n3dResult.out_p[5]);

    MINT32 *verityOutputs = (MINT32*)n3dResult.out_p[STEREO_KERNEL_OUTPUT_VERIFY];
    MY_LOGD("%s[verify[0]]   %d", prefix, verityOutputs[0]);
    MY_LOGD("%s[verify[1]]   %d", prefix, verityOutputs[1]);
    std::string s = std::bitset<4>(verityOutputs[2]).to_string();
    MY_LOGD("%s[verify[2]]   %s", prefix, s.c_str());
    for(int i = 3; i < 14; i++) {
        MY_LOGD("%s[verify[%d]]   %d", prefix, i, verityOutputs[i]);
    }
}
