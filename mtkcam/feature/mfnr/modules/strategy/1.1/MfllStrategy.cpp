#define LOG_TAG "MtkCam/MfllCore/Strategy"

#include "MfllStrategy.h"
#include "MfllLog.h"

#include <camera_custom_nvram.h>

using android::sp;
using namespace mfll;

//-----------------------------------------------------------------------------
IMfllStrategy* IMfllStrategy::createInstance()
{
    return (IMfllStrategy*)new MfllStrategy;
}
//-----------------------------------------------------------------------------
void IMfllStrategy::destroyInstance()
{
    decStrong((void*)this);
}
//-----------------------------------------------------------------------------
// MfllStrategy implementation
//-----------------------------------------------------------------------------
MfllStrategy::MfllStrategy()
: m_nvramChunk(NULL)
, m_captureFrameNum(MFLL_CAPTURE_FRAME)
, m_blendFrameNum(MFLL_BLEND_FRAME)
{
    m_mtkAis = std::shared_ptr<MTKAis>(
            MTKAis::createInstance(DRV_AIS_OBJ_SW),
            [](auto *obj)->void {
                if (obj) obj->destroyInstance();
            }
    );

    if (m_mtkAis.get() == NULL) {
        mfllLogE("create MTKAis failed");
        return;
    }

    auto result = m_mtkAis->AisInit(NULL, NULL);
    if (result != S_AIS_OK) {
        mfllLogE("init MTKAis failed with code %#x", result);
        return;
    }
}
//-----------------------------------------------------------------------------
MfllStrategy::~MfllStrategy()
{
}
//-----------------------------------------------------------------------------
enum MfllErr MfllStrategy::init(sp<IMfllNvram> &nvramProvider)
{
    if (nvramProvider.get() == NULL) {
        mfllLogE("%s: nvram provider is NULL", __FUNCTION__);
        return MfllErr_BadArgument;
    }

    if (m_mtkAis.get() == NULL) {
        mfllLogE("%s: MTKAis instance is NULL", __FUNCTION__);
        return MfllErr_NullPointer;
    }

    /* read NVRAM */
    size_t chunkSize = 0;
    const char *pChunk = nvramProvider->getChunk(&chunkSize);
    if (pChunk == NULL) {
        mfllLogE("%s: read NVRAM failed", __FUNCTION__);
        return MfllErr_UnexpectedError;
    }
    m_nvramChunk = pChunk;
    char *pMutableChunk = const_cast<char*>(pChunk);

    /* reading NVRAM */
    NVRAM_CAMERA_FEATURE_STRUCT *pNvram = reinterpret_cast<NVRAM_CAMERA_FEATURE_STRUCT*>(pMutableChunk);
    m_captureFrameNum = pNvram->mfll.capture_frame_number;
    m_blendFrameNum = pNvram->mfll.blend_frame_number;
    m_isFullSizeMc =  pNvram->mfll.full_size_mc;

    /* prepare data for MTKAis */
    AIS_SET_PROC1_IN_INFO param;
    mfllLogD("original mfll_iso_th=%d", pNvram->mfll.mfll_iso_th);

    param.u2IsoLvl1         = pNvram->mfll.iso_level1;
    param.u2IsoLvl2         = pNvram->mfll.iso_level2;
    param.u2IsoLvl3         = pNvram->mfll.iso_level3;
    param.u2IsoLvl4         = pNvram->mfll.iso_level4;
    param.u2IsoLvl5         = pNvram->mfll.iso_level5;
    /* u1FrmNum represents frame number for blending */
    param.u1FrmNum1         = pNvram->mfll.frame_num1;
    param.u1FrmNum2         = pNvram->mfll.frame_num2;
    param.u1FrmNum3         = pNvram->mfll.frame_num3;
    param.u1FrmNum4         = pNvram->mfll.frame_num4;
    param.u1FrmNum5         = pNvram->mfll.frame_num5;
    param.u1FrmNum6         = pNvram->mfll.frame_num6;
    /* u1SrcNum represents frame number for capture */
    param.u1SrcNum1         = pNvram->mfll.frame_num1;
    param.u1SrcNum2         = pNvram->mfll.frame_num2;
    param.u1SrcNum3         = pNvram->mfll.frame_num3;
    param.u1SrcNum4         = pNvram->mfll.frame_num4;
    param.u1SrcNum5         = pNvram->mfll.frame_num5;
    param.u1SrcNum6         = pNvram->mfll.frame_num6;

    auto result = m_mtkAis->AisFeatureCtrl(
            AIS_FTCTRL_SET_PROC1_INFO,
            (void*)&param,
            NULL);
    if (result != S_AIS_OK) {
        mfllLogE("%s: AisFeatureCtrl failed with code %#x", __FUNCTION__, result);
    }
    return MfllErr_Ok;
}
//-----------------------------------------------------------------------------
enum MfllErr MfllStrategy::queryStrategy(
        const MfllStrategyConfig_t &cfg,
        MfllStrategyConfig_t *out)
{
    if (m_mtkAis.get() == NULL) {
        mfllLogE("%s: MTKAis instance doesn't exist", __FUNCTION__);
        return MfllErr_UnexpectedError;
    }
    if (m_nvramChunk == NULL) {
        mfllLogE("%s: NVRAM chunk is NULL", __FUNCTION__);
        return MfllErr_UnexpectedError;
    }

    /* default AISConfig */
    MfllStrategyConfig_t finalCfg = cfg;

    finalCfg.frameCapture = m_captureFrameNum;
    finalCfg.frameBlend = m_blendFrameNum;
    finalCfg.isFullSizeMc = m_isFullSizeMc;

    //-------------------------------------------------------------------------
    // query frame number
    //-------------------------------------------------------------------------
    {
        AIS_PROC1_PARA_IN   paramIn;
        AIS_PROC1_PARA_OUT  paramOut;

        paramIn.u4CurrentIso = cfg.iso;
        auto result = m_mtkAis->AisMain(AIS_PROC1, &paramIn, &paramOut);
        if (result != S_AIS_OK) {
            mfllLogE("%s: MTKAis::AisMain with AIS_PROC1 returns fail(%#x)", __FUNCTION__, result);
        }
        else {
            finalCfg.frameCapture = static_cast<int>(paramOut.u1ReqSrcNum);
            finalCfg.frameBlend = static_cast<int>(paramOut.u1ReqFrmNum);
        }
    }

    //-------------------------------------------------------------------------
    // query if it's necessary to do MFNR
    //-------------------------------------------------------------------------
    /* checks ISO only */
    {
        size_t chunkSize;
        NVRAM_CAMERA_FEATURE_STRUCT *pNvram =
            reinterpret_cast<NVRAM_CAMERA_FEATURE_STRUCT*>(
                    const_cast<char*>(m_nvramChunk)
                    );
        if (cfg.iso > pNvram->mfll.mfll_iso_th)
            finalCfg.enableMfb = 1;
        else
            finalCfg.enableMfb = 0;
    }

    //-------------------------------------------------------------------------
    // always force on MFB
    //-------------------------------------------------------------------------
#if MFLL_MFB_ALWAYS_ON
    finalCfg.enableMfb = 1;
#endif

    *out = finalCfg;
    return MfllErr_Ok;
}
