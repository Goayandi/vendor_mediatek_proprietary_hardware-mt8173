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
#ifndef _GF_HAL_IMP_H_
#define _GF_HAL_IMP_H_

#include <vsdof/hal/gf_hal.h>
#include <libgf/MTKGF.h>
#include <mtkcam/aaa/aaa_hal_common.h> // For DAF_TBL_STRUCT
#include <vector>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>

using namespace NS3Av3;

class GF_HAL_IMP : public GF_HAL
{
public:
    GF_HAL_IMP(ENUM_STEREO_SCENARIO eScenario);
    virtual ~GF_HAL_IMP();

    virtual bool GFHALRun(GF_HAL_IN_DATA &inData, GF_HAL_OUT_DATA &outData);
protected:

private:
    void _setGFParams(GF_HAL_IN_DATA &gfHalParam);
    void _runGF(GF_HAL_OUT_DATA &gfHalOutput);
    //
    void _dumpInitData();
    void _dumpSetProcData();
    void _dumpGFResult();
    void _dumpGFBufferInfo(const GFBufferInfo &buf, int index = -1);
    //
    void _initAFWinTransform();
    MPoint _getAFPoint(const int AF_INDEX, ENUM_STEREO_SCENARIO scenario);
    MPoint _getTouchPoint(MPoint ptIn, ENUM_STEREO_SCENARIO scenario);

    void _clearTransformedImages();
    bool _rotateResult(ENUM_BUFFER_NAME bufferName, GFBufferInfo &gfResult, MUINT8 *targetBuffer);
private:
    MTKGF           *m_pGfDrv;
    //
    GFInitInfo      m_initInfo;
    GFProcInfo      m_procInfo;
    GFResultInfo    m_resultInfo;
    //
    const bool      LOG_ENABLED;
    const bool      BENCHMARK_ENABLED;
    //
    DAF_TBL_STRUCT          *m_pAFTable;
    bool                     m_isAFSupported;
    bool                     m_isAFTrigger;
    static MPoint            m_lastFocusPoint;
    //index 0: preview/vr; 1: capture/zsd
    float                    m_afScaleW[2];
    float                    m_afScaleH[2];
    int                      m_afOffsetX[2];
    int                      m_afOffsetY[2];

    std::vector<sp<IImageBuffer>>    m_transformedImages;
};

#endif