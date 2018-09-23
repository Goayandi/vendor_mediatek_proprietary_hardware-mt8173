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

    param.u4TuningExpTH     = pNvram->mfll.ais_exp_th;
    param.u4TuningIsoTH     = pNvram->mfll.mfll_iso_th;
    param.u4TuningExpTH0    = pNvram->mfll.ais_exp_th; // TODO: no EXP_TH0 ??
    param.u4TuningIsoTH0    = pNvram->mfll.mfll_iso_th; // TODO: no ISO_TH0??
    param.u4EnMFNRIsoTH     = pNvram->mfll.mfll_iso_th;
    /* advanced tuning parameters */
    param.u4TuningAdvanceEn        = pNvram->mfll.ais_advanced_tuning_en;
    param.u4TuningAdvanceMaxExp    = pNvram->mfll.ais_advanced_max_exposure;
    param.u4TuningAdvanceMaxIso    = pNvram->mfll.ais_advanced_max_iso;

    mfllLogD("ais_exp_th = %d, mfll_iso_th = %d",
            pNvram->mfll.ais_exp_th, pNvram->mfll.mfll_iso_th);
    mfllLogD("ais_adv_en = %d, ais_adv_max_exp = %d, ais_adv_max_iso = %d",
            pNvram->mfll.ais_advanced_tuning_en,
            pNvram->mfll.ais_advanced_max_exposure,
            pNvram->mfll.ais_advanced_max_iso);

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
    // case: MFNR only
    //-------------------------------------------------------------------------
    /* not enable AIS, checks ISO only */
    if (cfg.isAis == 0) {
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
    // case: AIS
    //-------------------------------------------------------------------------
    else {
        AIS_PROC1_PARA_OUT paramOut;
        AIS_PROC1_PARA_IN paramIn;
        paramIn.u4CurrentIso = cfg.iso;
        paramIn.u4CurrentExp = cfg.exp;
        auto result = m_mtkAis->AisMain(AIS_PROC1, &paramIn, &paramOut);
        if (result != S_AIS_OK) {
            mfllLogE("%s: MTKAis::AisMain returns failed(%#x)", __FUNCTION__, result);
            return MfllErr_UnexpectedError;
        }

        /* update iso/exp if need */
        if (paramOut.bUpdateExpIso != 0) {
            finalCfg.iso = paramOut.u4Iso;
            finalCfg.exp = paramOut.u4Exp;
        }
        /* check if it's necessary to do MFNR */
        if (paramOut.bDoMFNR != 0) {
            finalCfg.frameBlend = 4; // FIXME: should be queried from MTKAis
            finalCfg.enableMfb = 1;
        }
        else {
            finalCfg.enableMfb = 0;
        }
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
