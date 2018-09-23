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
#ifndef _N3D_HAL_IMP_H_
#define _N3D_HAL_IMP_H_

#include <vsdof/hal/n3d_hal.h>

#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <vector>
#include <cutils/atomic.h>
#include <libstereocam/MTKStereoKernel.h>
#include <mtkcam/aaa/IHal3A.h>
#include <hal/inc/camera_custom_nvram.h>
#include "DepthAF/stereodepth_hal.h"

using namespace android;
using namespace NS3Av3;
using android::Mutex;

class N3D_HAL_IMP : public N3D_HAL
{
public:
    N3D_HAL_IMP(N3D_HAL_INIT_PARAM &n3dInitParam);
    virtual ~N3D_HAL_IMP();

    virtual bool N3DHALRun(N3D_HAL_PARAM &n3dParams, N3D_HAL_OUTPUT &n3dOutput);

    virtual bool N3DHALRun(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput);

    virtual bool N3DHALRun(N3D_HAL_PARAM_CAPTURE_SWFE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput);

    virtual char *getStereoExtraData();

private:
    bool _initN3DHAL(N3D_HAL_INIT_PARAM &n3dInitParam);
    void _initAFWinTransform(ENUM_STEREO_SCENARIO scenario);
    bool _saveNVRAM();
    bool _loadNVRAM();
    bool _syncInternalNVRAM(MTKStereoKernel *pSrcStereoDrv, MTKStereoKernel *pDstStereoDrv);

    //For capture
    void _compressMask(std::vector<RUN_LENGTH_DATA> &compressedMask);
    const char *_prepareStereoExtraData();

    bool _getFEOInputInfo(ENUM_PASS2_ROUND pass2Round, ENUM_STEREO_SCENARIO eScenario, STEREO_KERNEL_IMG_INFO_STRUCT &imgInfo);
    bool _initN3DGeoInfo(STEREO_KERNEL_GEO_INFO_STRUCT geo_img[]);
    bool _initCCImgInfo(STEREO_KERNEL_IMG_INFO_STRUCT &ccImgInfo);
    bool _loadLensInfo();
    MUINT8 *_initWorkingBuffer(const MUINT32 BUFFER_SIZE);
    bool _doStereoKernelInit();

    void _dumpInitInfo(const STEREO_KERNEL_SET_ENV_INFO_STRUCT &initInfo);
    void _dumpImgInfo(const char *prefix, const STEREO_KERNEL_IMG_INFO_STRUCT &imgInfo);
    void _dumpRemapInfo(const char *prefix, const STEREO_KERNEL_FLOW_INFO_STRUCT &remapInfo);
    void _dumpTuningInfo(const char *prefix, const STEREO_KERNEL_TUNING_PARA_STRUCT &tuningInfo);
    void _dumpSetProcInfo(const char *prefix, const STEREO_KERNEL_SET_PROC_INFO_STRUCT &setprocInfo);
    void _dumpResult(const char *prefix, const STEREO_KERNEL_RESULT_STRUCT &n3dResult);

    void _setN3DCommonParams(N3D_HAL_PARAM_COMMON &n3dParams, N3D_HAL_OUTPUT &n3dOutput, STEREO_KERNEL_SET_PROC_INFO_STRUCT &setprocInfo);
    void _setN3DParams(N3D_HAL_PARAM &n3dParams, N3D_HAL_OUTPUT &n3dOutput);
    void _setN3DCaptureParams(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput);
    void _setN3DSWFECaptureParams(N3D_HAL_PARAM_CAPTURE_SWFE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput);

    bool _runN3DCommon();
    bool _runN3D(N3D_HAL_OUTPUT &n3dOutput);    //For preview/VR, with depth-AF
    bool _runN3DCapture(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput);
    bool _runN3DCaptureSWFE(N3D_HAL_OUTPUT_CAPTURE &n3dOutput __attribute__((unused))) { return false; }
    void _splitMask();
    bool _initLensInfo(const int32_t SENSOR_INDEX, DAF_TBL_STRUCT *&pAFTable, bool &isAF);
    bool _getStereoRemapInfo(STEREO_KERNEL_FLOW_INFO_STRUCT &infoMain1,
                             STEREO_KERNEL_FLOW_INFO_STRUCT &infoMain2,
                             ENUM_STEREO_SCENARIO eScenario);
    void _transferAFWin(const AF_WIN_COORDINATE_STRUCT &in, AF_WIN_COORDINATE_STRUCT &out);
    bool _runDepthAF(const int AF_INDEX);
    MFLOAT _getDistance();

    //make output dir for debug
    char *_getDumpFolderName(int folderNumber, char path[]);
    void _mkdir();
private:
    static Mutex            mLock; //static member is mutable, so we no need to add mutable
    static MINT32           m_nvramUserCount;

    bool                    LOG_ENABLED;        //StereoSettingProvider::isLogEnabled(PERPERTY_DEPTHMAP_NODE_LOG)
    bool                    PROFILE_ENABLED;    //StereoSettingProvider::isProfileLogEnabled()
    const MINT32            DUMP_START;
    const bool              RUN_N3D;

    ENUM_STEREO_SCENARIO     m_eScenario;
    MUINT8                  *m_pWorkBuf;        // Working Buffer
    MUINT32                  m_workBufSize;

    DAF_TBL_STRUCT          *m_pAFTable[2];
    bool                     m_isAF[2];
    MUINT32                  m_magicNumber[2];
    MUINT32                  m_requestNumber;

    MTKStereoKernel         *m_pStereoDrv;
    STEREO_KERNEL_SET_ENV_INFO_STRUCT       m_algoInitInfo;
    STEREO_KERNEL_SET_WORK_BUF_INFO_STRUCT  m_algoWorkBufInfo;
    STEREO_KERNEL_SET_PROC_INFO_STRUCT      m_algoProcInfo;
    STEREO_KERNEL_TUNING_PARA_STRUCT        m_algoTuningInfo;
    STEREO_KERNEL_RESULT_STRUCT             m_algoResult;   //Rectify and verifications

    StereoDepthHal          *m_pDepthAFHAL;
    MUINT8                  *m_main1Mask;       //warp_msk_addr_main
    MUINT8                  *m_main2Mask;       //For capture, extract from alpha channel of m_outputGBMain2
    MUINT32                  m_main2MaskSize;
    sp<IImageBuffer>         m_outputGBMain2;   //RGBA, 2176x1152

    char                    *m_stereoExtraData;
    MUINT32                  m_captureOrientation;
    MUINT32                  m_queryIndex;          //0xFFFFFFFF if not query
    MPoint                   m_ptQueryDistance;     //screen position

    static NVRAM_CAMERA_GEOMETRY_STRUCT*    m_pVoidGeoData; //Shared between Preview/VR <-> Capture

    static MTKStereoKernel  *__previewStereoDrv;
    static MTKStereoKernel  *__captureStereoDrv;

    float                    m_afScaleW;
    float                    m_afScaleH;
    int                      m_afOffsetX;
    int                      m_afOffsetY;
};

#endif  // _N3D_HAL_IMP_H_