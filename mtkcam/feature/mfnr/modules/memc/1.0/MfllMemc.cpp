#define LOG_TAG "MtkCam/MfllCore/Memc"

#include "MfllMemc.h"
#include "MfllLog.h"

#include "MfllExifInfo.h"

/* Exif info */
#include <BuiltinTypes.h> // required by dbg_cam_param.h
#include <debug_exif/cam/dbg_cam_param.h> //required by dbg_cam_mf_param.h
#include <debug_exif/cam/dbg_cam_mf_param.h>

#include <camera_custom_nvram.h>

using android::sp;
using namespace mfll;

IMfllMemc* IMfllMemc::createInstance()
{
    return (IMfllMemc*)new MfllMemc();
}

void IMfllMemc::destroyInstance()
{
    decStrong((void*)this);
}

MfllMemc::MfllMemc()
: m_widthMe(0)
, m_heightMe(0)
, m_widthMc(0)
, m_heightMc(0)
, m_mcThreadNum(MFLL_MC_THREAD_NUM)
, m_isUsingFullMc(0)
, m_isIgnoredMc(0)
, m_pCore(NULL)
{
    m_mfbll = MTKMfbll::createInstance(DRV_MFBLL_OBJ_SW);
    if (m_mfbll == NULL) {
        mfllLogE("%s: create MTKMfbll failed", __FUNCTION__);
    }
}

MfllMemc::~MfllMemc()
{
    if (m_mfbll)
        m_mfbll->destroyInstance();
}

enum MfllErr MfllMemc::init(sp<IMfllNvram> &nvramProvider)
{
    if (m_widthMe == 0 || m_heightMe == 0) {
        mfllLogE("%s: init MTKMfbll needs information of the resolution of ME buffer", __FUNCTION__);
        return MfllErr_BadArgument;
    }

    if (nvramProvider.get() == NULL) {
        mfllLogE("%s: init MfllMemc failed due to no NVRAM provider", __FUNCTION__);
        return MfllErr_BadArgument;
    }

    m_nvramProvider = nvramProvider;

    MFBLL_INIT_PARAM_STRUCT initParam = {
        .Proc1_imgW = static_cast<MUINT16>(m_widthMe), // image width
        .Proc1_imgH = static_cast<MUINT16>(m_heightMe), // image height
        .core_num =   static_cast<MUINT32>(m_mcThreadNum),
    };

    if(m_mfbll->MfbllInit(&initParam, NULL) != S_MFBLL_OK) {
        mfllLogE("%s: init MTKMfbll failed", __FUNCTION__);
        return MfllErr_UnexpectedError;
    }

    mfllLogD("%s: init MEMC with MC threads num -> %d",
            __FUNCTION__,
            initParam.core_num);

    return MfllErr_Ok;
}

/******************************************************************************
 *
******************************************************************************/
enum MfllErr MfllMemc::getAlgorithmWorkBufferSize(size_t *size)
{
    enum MfllErr err = MfllErr_Ok;

    MFBLL_GET_PROC_INFO_STRUCT info;

    auto ret = m_mfbll->MfbllFeatureCtrl(
            MFBLL_FTCTRL_GET_PROC_INFO,
            NULL, // no need
            &info);

    if (S_MFBLL_OK != ret) {
        mfllLogE("%s: get MTKMfbll information failed", __FUNCTION__);
        return MfllErr_UnexpectedError;
    }

    *size = (size_t)info.Ext_mem_size;
    mfllLogD("%s: MTKMfbll working buffer size = %zu", __FUNCTION__, *size);
    return err;
}

enum MfllErr MfllMemc::motionEstimation()
{
    /* release soruce/destination of ME after motionEstimation() */
    typedef IMfllImageBuffer T;
    struct __ResourceHolder{
        sp<T> *a;
        sp<T> *b;
        __ResourceHolder(sp<T> *a, sp<T> *b)
        : a(a), b(b) {}
        ~__ResourceHolder()
        { *a = 0; *b = 0; }
    } __res_holder(&m_imgSrcMe, &m_imgRefMe);

    enum MfllErr err = MfllErr_Ok;
    if (m_imgSrcMe.get() == NULL) {
        mfllLogE("%s: cannot do ME due to no source image", __FUNCTION__);
        err = MfllErr_BadArgument   ;
    }
    if (m_imgRefMe.get() == NULL) {
        mfllLogE("%s: cannot do ME due to no destination image", __FUNCTION__);
        err = MfllErr_BadArgument;
    }
    if (m_imgWorkingBuffer.get() == NULL) {
        mfllLogE("%s: cannot do ME due to no working buffer", __FUNCTION__);
        err = MfllErr_BadArgument;
    }
    if (err != MfllErr_Ok)
        return err;

    mfllLogD("%s: is full size MC = %d", __FUNCTION__, m_isUsingFullMc);

    /* read NVRAM for tuning data */
    unsigned int badRange = 255;
    unsigned int badTh    = 12707;

    size_t chunkSize = 0;
    const char *pChunk = m_nvramProvider->getChunk(&chunkSize);
    if (pChunk == NULL) {
        mfllLogE("%s: read NVRAM failed, use default", __FUNCTION__);
    }
    else {
        char *pMutableChunk = const_cast<char*>(pChunk);
        NVRAM_CAMERA_FEATURE_STRUCT *pNvram = reinterpret_cast<NVRAM_CAMERA_FEATURE_STRUCT*>(pMutableChunk);
        /* update bad range and bad threshold */
        badRange = pNvram->mfll.memc_bad_mv_range;
        badTh    = pNvram->mfll.memc_bad_mv_rate_th;
    }

    MFBLL_SET_PROC_INFO_STRUCT param = {
        .workbuf_addr   = (MUINT8*)m_imgWorkingBuffer->getVa(),
        .buf_size       = static_cast<MUINT32>(m_imgWorkingBuffer->getRealBufferSize()),
        .Proc1_base     = (MUINT8*)m_imgSrcMe->getVa(),
        .Proc1_ref      = (MUINT8*)m_imgRefMe->getVa(),
        .Proc1_width    = (MUINT32)m_imgSrcMe->getAlignedWidth(), // the buffer width
        .Proc1_height   = (MUINT32)m_imgSrcMe->getAlignedHeight(), // the buffer height
        .Proc1_V1       = m_globalMv.x / (m_isUsingFullMc ? 1 : 2),
        .Proc1_V2       = m_globalMv.y / (m_isUsingFullMc ? 1 : 2),
        .Proc1_bad_range = static_cast<MUINT8>(badRange),
        .Proc1_bad_TH   = badTh,
        .Proc1_ImgFmt   = PROC1_FMT_Y,
        .Proc2_full_size = m_isUsingFullMc, // 0: MC 1/4 size, 1: MC full size
    };

    /* update debug info */
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_BAD_MV_RANGE, (uint32_t)param.Proc1_bad_range);
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_BAD_MB_TH, (uint32_t)param.Proc1_bad_TH);

    /* print out information */
    mfllLogD("%s:---------------------------", __FUNCTION__);
    mfllLogD("%s: mc size %dx%d", __FUNCTION__, param.Proc1_width, param.Proc1_height);
    mfllLogD("%s: mc v1,v2=%d,%d", __FUNCTION__, param.Proc1_V1, param.Proc1_V2);
    mfllLogD("%s: mc badRange=%d", __FUNCTION__, param.Proc1_bad_range);
    mfllLogD("%s: mc badTh=%d", __FUNCTION__, param.Proc1_bad_TH);
    mfllLogD("%s:---------------------------", __FUNCTION__);

    auto ret = m_mfbll->MfbllFeatureCtrl(MFBLL_FTCTRL_SET_PROC_INFO, &param, NULL);
    if (S_MFBLL_OK != ret) {
        mfllLogE("%s: MTKMfbll set proc1 info fail, skip MC too", __FUNCTION__);
        m_isIgnoredMc = 1;
        return MfllErr_UnexpectedError;
    }

    MFBLL_PROC1_OUT_STRUCT paramOut;
    ret = m_mfbll->MfbllMain(MFBLL_PROC1, NULL, &paramOut);
    if (S_MFBLL_OK != ret) {
        mfllLogE("%s: do ME failed", __FUNCTION__);
        m_isIgnoredMc = 1;
        return MfllErr_UnexpectedError;
    }

    mfllLogD("%s: is skip MC --> %d", __FUNCTION__, paramOut.bSkip_proc);
    m_isIgnoredMc = paramOut.bSkip_proc;

    return MfllErr_Ok;
}


enum MfllErr MfllMemc::motionCompensation()
{
    /* release soruce/destination of ME after motionEstimation() */
    typedef IMfllImageBuffer T;
    struct __ResourceHolder{
        sp<T> *a;
        sp<T> *b;
        sp<T> *c;
        __ResourceHolder(sp<T> *a, sp<T> *b, sp<T> *c)
        : a(a), b(b), c(c) {}
        ~__ResourceHolder()
        { *a = 0; *b = 0; *c=0; }
    } __res_holder(&m_imgRefMc, &m_imgDstMc, &m_imgWorkingBuffer);

    enum MfllErr err = MfllErr_Ok;
    /* Check if the MC should be ignored */
    if (isIgnoredMotionCompensation()) {
        mfllLogD("%s: Ignored MC", __FUNCTION__);
        return MfllErr_Ok;
    }

    if (m_imgDstMc.get() == NULL) {
        mfllLogE("%s: cannot do MC due to no destination image", __FUNCTION__);
        err = MfllErr_BadArgument;
    }
    if (m_imgRefMc.get() == NULL) {
        mfllLogE("%s: cannot do MC due to no reference image", __FUNCTION__);
        err = MfllErr_BadArgument;
    }
    if (err != MfllErr_Ok)
        return err;

    MFBLL_PROC2_OUT_STRUCT param = {
        .ImgFmt         = PROC1_FMT_YUY2,
        /* reference */
        .pbyInImg       = (MUINT8*)m_imgRefMc->getVa(),
        .i4InWidth      = (MUINT32)m_imgRefMc->getAlignedWidth(), // the buffer width
        .i4InHeight     = (MUINT32)m_imgRefMc->getAlignedHeight(), // the buffer height
        /* destination */
        .pbyOuImg       = (MUINT8*)m_imgDstMc->getVa(),
        .i4OuWidth      = (MUINT32)m_imgDstMc->getWidth(), // the image width
        .i4OuHeight     = (MUINT32)m_imgDstMc->getHeight() // the image height
    };

    /* update dbg info */
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_INPUT_FORMAT, (uint32_t)PROC1_FMT_YV16);
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_OUTPUT_FORMAT, (uint32_t)param.ImgFmt);
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_WIDTH, (uint32_t)param.i4OuWidth);
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_HEIGHT, (uint32_t)param.i4OuHeight);
    m_pCore->updateExifInfo((unsigned int)MF_TAG_MEMC_MCFULLSIZE, (uint32_t)m_isUsingFullMc);

    mfllLogD("%s: input size:  %dx%d", __FUNCTION__, param.i4InWidth, param.i4InHeight);
    mfllLogD("%s: output size: %dx%d", __FUNCTION__, param.i4OuWidth, param.i4OuHeight);

    auto ret = m_mfbll->MfbllMain(MFBLL_PROC2, NULL, &param);
    if (S_MFBLL_OK != ret) {
        mfllLogE("%s: do MC failed", __FUNCTION__);
        m_isIgnoredMc = 1;
        return MfllErr_UnexpectedError;
    }

    mfllLogD("do MC ok!");

    return MfllErr_Ok;
}

