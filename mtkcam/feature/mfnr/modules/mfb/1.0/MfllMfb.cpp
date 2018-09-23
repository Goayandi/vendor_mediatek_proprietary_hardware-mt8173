#define LOG_TAG "MtkCam/MfllCore/Mfb"

#include "MfllCore.h"
#include "MfllMfb.h"
#include "MfllLog.h"
#include "MfllExifInfo.h"
#include "MfllProperty.h"
#include "MfllFeatureDump.h"
#include "MfllIspProfiles.h"
#include "MfllOperationSync.h"

/* mtkcam related headers */
#include <mtk_platform_metadata_tag.h>
#include <drv/isp_reg.h> // dip_x_reg_t

#include <mtkcam/drv/iopipe/SImager/IImageTransform.h> // IImageTransform
#include <utils/Mutex.h> // android::Mutex

#include <memory>

//
// To print more debug information of this module
// #define __DEBUG


//
// To workaround device hang of MFB stage
// #define WORKAROUND_MFB_STAGE

// helper macro to check if ISP profile mapping ok or not
#define IS_WRONG_ISP_PROFILE(p) (p == static_cast<EIspProfile_T>(MFLL_ISP_PROFILE_ERROR))

/* platform dependent headers, which needs defines in MfllMfb.cpp */
#include "MfllMfb_platform.h"

static const char* sStrP2 = "P2";

using namespace NSCam::NSIoPipe::NSPostProc;
using namespace NSCam;
using namespace NSCam::Utils::Format;
using namespace NS3Av3;
using namespace mfll;
using namespace NSCam::NSIoPipe;

using android::sp;
using android::Mutex;
using NSCam::IImageBuffer;
using NSCam::NSIoPipe::EPortType_Memory;
using NSCam::NSIoPipe::NSSImager::IImageTransform;
using NSCam::NSIoPipe::NSPostProc::Input;
using NSCam::NSIoPipe::NSPostProc::Output;
using NSCam::NSIoPipe::NSPostProc::QParams;
using NSCam::NSIoPipe::PortID;


/**
 *  Describe the pass 2 port ID
 *
 *  Notice that, MCrpRsInfo::mGroupID is only works if only if input port is IMGI which describe:
 *  mGroupID = 1: Cropping of the source image
 *  mGroupID = 2: Cropping/Reszing of the first output image
 *  mGroupID = 3: Cropping/Reszing of the second output image
 *
 */
/******************************************************************************
 *  RAW2YUV
 *****************************************************************************/
#define RAW2YUV_PORT_IN             PORT_IMGI
#define RAW2YUV_PORT_LCE_IN         PORT_LCEI
#define RAW2YUV_PORT_OUT            PORT_WDMAO
#define RAW2YUV_PORT_OUT2           PORT_WROTO

/* Cropping group ID is related port ID, notice this ... */
#define RAW2YUV_GID_IN              1 // for PORT_IMGI
#define RAW2YUV_GID_OUT             2 // for PORT_WDMAO
#define RAW2YUV_GID_OUT2            3 // for PORT_WROTEO

/******************************************************************************
 *  MFB
 *****************************************************************************/
/* port */
#define MFB_PORT_IN_BASE_FRAME      PORT_IMGI
#define MFB_PORT_IN_REF_FRAME       PORT_IMGBI
#define MFB_PORT_IN_WEIGHTING       PORT_IMGCI
#define MFB_PORT_OUT_FRAME          PORT_IMG2O
#define MFB_PORT_OUT_WEIGHTING      PORT_MFBO

/* group ID in MFB stage, if not defined which means not support crop */
#define MFB_GID_OUT_FRAME           1 // IMG2O group id in MFB stage

/******************************************************************************
 *  MIX
 *****************************************************************************/
/* port */
#define MIX_PORT_IN_BASE_FRAME      PORT_IMGI
#define MIX_PORT_IN_GOLDEN_FRAME    PORT_VIPI
#define MIX_PORT_IN_WEIGHTING       PORT_IMGBI
#define MIX_PORT_OUT_FRAME          PORT_IMG2O

/*
 * Mutex for pass 2 operation
 *
 * make sure pass 2 is thread-safe, basically it's not ...
 */
static Mutex gMutexPass2Lock;

/* For P2 driver callback usage */
#define P2CBPARAM_MAGICNUM 0xABC
typedef struct _p2cbParam {
    MfllMfb         *self;
    Mutex           *mutex;
    unsigned int    magicNum;
    _p2cbParam ()
    {
        self = NULL;
        mutex = NULL;
        magicNum = P2CBPARAM_MAGICNUM;
    };
    _p2cbParam(MfllMfb *s, Mutex *m)
    {
        self = s;
        mutex = m;
        magicNum = P2CBPARAM_MAGICNUM;
    }
} p2cbParam_t;

static MVOID pass2CbFunc(QParams& rParams)
{
    mfllAutoLogFunc();
    p2cbParam_t *p = reinterpret_cast<p2cbParam_t*>(rParams.mpCookie);
    /* check Magic NUM */
    if (p->magicNum != P2CBPARAM_MAGICNUM) {
        mfllLogE("%s: pass2 param is weird, magic num is different", __FUNCTION__);
    }
    p->mutex->unlock();
}

template <typename T>
inline MVOID updateEntry(IMetadata* pMetadata, MUINT32 const tag, T const& val)
{
    if (pMetadata == NULL)
    {
        mfllLogE("pMetadata is NULL");
        return;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
}

template <class T>
inline MBOOL tryGetMetaData(IMetadata *pMetadata, MUINT32 const tag, T &rVal)
{
    if (pMetadata == NULL) {
        return MFALSE;
    }

    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if (!entry.isEmpty()) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }

    return MFALSE;
}

static MRect calCrop(MRect const &rSrc, MRect const &rDst, uint32_t /* ratio */)
{
    #define ROUND_TO_2X(x) ((x) & (~0x1))

    MRect rCrop;

    // srcW/srcH < dstW/dstH
    if (rSrc.s.w * rDst.s.h < rDst.s.w * rSrc.s.h)
    {
        rCrop.s.w = rSrc.s.w;
        rCrop.s.h = rSrc.s.w * rDst.s.h / rDst.s.w;
    }
    // srcW/srcH > dstW/dstH
    else if (rSrc.s.w * rDst.s.h > rDst.s.w * rSrc.s.h)
    {
        rCrop.s.w = rSrc.s.h * rDst.s.w / rDst.s.h;
        rCrop.s.h = rSrc.s.h;
    }
    // srcW/srcH == dstW/dstH
    else
    {
        rCrop.s.w = rSrc.s.w;
        rCrop.s.h = rSrc.s.h;
    }

    rCrop.s.w =  ROUND_TO_2X(rCrop.s.w);
    rCrop.s.h =  ROUND_TO_2X(rCrop.s.h);

    rCrop.p.x = (rSrc.s.w - rCrop.s.w) / 2;
    rCrop.p.y = (rSrc.s.h - rCrop.s.h) / 2;

    rCrop.p.x += ROUND_TO_2X(rSrc.p.x);
    rCrop.p.y += ROUND_TO_2X(rSrc.p.y);

    #undef ROUND_TO_2X
    return rCrop;
}

static bool isRawFormat(const EImageFormat fmt)
{
    return  (fmt == eImgFmt_BAYER8)
        ||  (fmt == eImgFmt_BAYER10)
        ||  (fmt == eImgFmt_BAYER12)
        ||  (fmt == eImgFmt_BAYER14)
        ;
}

//-----------------------------------------------------------------------------

/**
 *  M F L L    M F B
 */
IMfllMfb* IMfllMfb::createInstance(void)
{
    return (IMfllMfb*)new MfllMfb();
}

void IMfllMfb::destroyInstance(void)
{
    decStrong((void*)this);
}

MfllMfb::MfllMfb(void)
{
    m_pNormalStream = NULL;
    m_p3A = NULL;
    m_shotMode = (enum MfllMode)0;
    m_syncPrivateData = NULL;
    m_syncPrivateDataSize = 0;
    m_encodeYuvCount = 0;
    m_blendCount = 0;
    m_bIsInited = false;
    m_nrType = NoiseReductionType_None;
    m_sensorId = 0;
    m_pCore = NULL;
    for (size_t i = 0; i < STAGE_SIZE; i++)
        m_bExifDumpped[i] = 0;
}

MfllMfb::~MfllMfb(void)
{
    mfllAutoLogFunc();

    if (m_pNormalStream) {
        m_pNormalStream->uninit(LOG_TAG);
        m_pNormalStream->destroyInstance();
        m_pNormalStream = NULL;
    }

    if (m_p3A) {
        m_p3A->destroyInstance(LOG_TAG);
    }
}

enum MfllErr MfllMfb::init(int sensorId)
{
    enum MfllErr err = MfllErr_Ok;
    Mutex::Autolock _l(m_mutex);
    mfllAutoLogFunc();

    if (m_bIsInited) { // do not init twice
        goto lbExit;
    }

    mfllLogD("Init MfllMfb with sensor id --> %d", sensorId);
    m_sensorId = sensorId;

    m_pNormalStream = INormalStream::createInstance(sensorId);
    if (m_pNormalStream == NULL) {
        mfllLogE("create INormalStream fail");
        err = MfllErr_UnexpectedError;
        goto lbExit;
    }
    else {
        m_pNormalStream->init(LOG_TAG);
    }

    m_p3A = MAKE_Hal3A(sensorId, LOG_TAG);
    if (m_p3A == NULL) {
        mfllLogE("create IHal3A fail");
        err = MfllErr_UnexpectedError;
        goto lbExit;
    }
    /* mark as inited */
    m_bIsInited = true;

lbExit:
    return err;
}

enum MfllErr MfllMfb::blend(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt_in, IMfllImageBuffer *wt_out)
{
    mfllAutoLogFunc();

    /* It MUST NOT be NULL, so don't do error handling */
    if (base == NULL) {
        mfllLogE("%s: base is NULL", __FUNCTION__);
        return MfllErr_NullPointer;
    }

    if (ref == NULL) {
        mfllLogE("%s: ref is NULL", __FUNCTION__);
        return MfllErr_NullPointer;
    }

    if (out == NULL) {
        mfllLogE("%s: out is NULL", __FUNCTION__);
        return MfllErr_NullPointer;
    }

    if (wt_out == NULL) {
        mfllLogE("%s: wt_out is NULL", __FUNCTION__);
        return MfllErr_NullPointer;
    }

    return blend(
            (IImageBuffer*)base->getImageBuffer(),
            (IImageBuffer*)ref->getImageBuffer(),
            (IImageBuffer*)out->getImageBuffer(),
            wt_in ? (IImageBuffer*)wt_in->getImageBuffer() : NULL,
            (IImageBuffer*)wt_out->getImageBuffer()
            );
}

enum MfllErr MfllMfb::blend(IImageBuffer *base, IImageBuffer *ref, IImageBuffer *out, IImageBuffer *wt_in, IImageBuffer *wt_out)
{
    enum MfllErr err = MfllErr_Ok;
    MBOOL bRet = MTRUE;

    mfllAutoLogFunc();
    mfllAutoTraceFunc();

    /* get member resource here */
    m_mutex.lock();
    enum NoiseReductionType nrType = m_nrType;
    EIspProfile_T profile = (EIspProfile_T)0; // which will be set later.
    int sensorId = m_sensorId;
    void *privateData = m_syncPrivateData;
    size_t privateDataSize = m_syncPrivateDataSize;
    size_t index = m_pCore->getIndexByNewIndex(0);
    m_blendCount++;
    /* get metaset */
    if (index >= m_vMetaSet.size()) {
        mfllLogE("%s: index(%zu) is out of size of metaset(%zu)", __FUNCTION__, index, m_vMetaSet.size());
        m_mutex.unlock();
        return MfllErr_UnexpectedError;
    }
    MetaSet_T metaset = m_vMetaSet[index];
    m_mutex.unlock();

    /**
     *  P A S S 2
     *
     *  Configure input parameters
     */
    QParams params;
    params.mvStreamTag.push_back(NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_MFB_Bld);

    /* input: source frame, base frame [IMGI port] */
    {
        Input p;
        p.mBuffer       = base;
        p.mPortID       = MFB_PORT_IN_BASE_FRAME;
        p.mPortID.group = 0;
        params.mvIn.push_back(p);
        /* IMGI in this stage doesn't supports cropping */
    }

    /* input: reference frame */
    {
        Input p;
        p.mBuffer       = ref;
        p.mPortID       = MFB_PORT_IN_REF_FRAME;
        p.mPortID.group = 0;
        params.mvIn.push_back(p);
        /* This input port in this stage doesn't support crop */
    }

    /* input: weighting table, for not the first blending, we need to give weighting map */
    if (wt_in) {
        Input p;
        p.mBuffer       = wt_in;
        p.mPortID       = MFB_PORT_IN_WEIGHTING;
        p.mPortID.group = 0;
        params.mvIn.push_back(p);
        /* This intput port doesn't support crop */
    }

    /* output: blended frame */
    {
        Output p;
        p.mBuffer       = out;
        p.mPortID       = MFB_PORT_OUT_FRAME;
        p.mPortID.group = 0;
        params.mvOut.push_back(p);

        /* crop info */
        MCrpRsInfo crop;
        crop.mGroupID                   = MFB_GID_OUT_FRAME;
        crop.mCropRect.p_integral.x     = 0; // position of cropping window, in pixel, integer
        crop.mCropRect.p_integral.y     = 0;
        crop.mCropRect.p_fractional.x   = 0;
        crop.mCropRect.p_fractional.y   = 0;
        crop.mCropRect.s.w              = base->getImgSize().w;
        crop.mCropRect.s.h              = base->getImgSize().h;
        crop.mResizeDst.w               = out->getImgSize().w;
        crop.mResizeDst.h               = out->getImgSize().h;
        crop.mFrameGroup                = 0;
        params.mvCropRsInfo.push_back(crop);
    }

    /* output: new weighting table */
    {
        Output p;
        p.mBuffer       = wt_out;
        p.mPortID       = MFB_PORT_OUT_WEIGHTING;
        p.mPortID.group = 0;
        params.mvOut.push_back(p);
        /* This port doesn't support cropping */
    }

    /* determine ISP profile */
    if (isZsdMode(m_shotMode)) {
        profile = (nrType == NoiseReductionType_SWNR)
            ? get_isp_profile(eMfllIspProfile_Mfb_Zsd_Swnr)
            : get_isp_profile(eMfllIspProfile_Mfb_Zsd);
        if (IS_WRONG_ISP_PROFILE(profile)) {
            mfllLogE("%s: error isp profile mapping", __FUNCTION__);
        }
    }
    else {
        profile = (nrType == NoiseReductionType_SWNR)
            ? get_isp_profile(eMfllIspProfile_Mfb_Swnr)
            : get_isp_profile(eMfllIspProfile_Mfb);
        if (IS_WRONG_ISP_PROFILE(profile)) {
            mfllLogE("%s: error isp profile mapping", __FUNCTION__);
        }
    }

    /* execute pass 2 operation */
    {
        /* pass 2 critical section */
        Mutex::Autolock _l(gMutexPass2Lock);

        /* add tuning data is necessary */
        std::unique_ptr<char> tuning_reg(new char[sizeof(dip_x_reg_t)]());
        void *tuning_lsc;

        mfllLogD("%s: create tuning register data chunk with size %zu",
                __FUNCTION__, sizeof(dip_x_reg_t));

        TuningParam rTuningParam = {NULL, NULL, NULL};
        rTuningParam.pRegBuf = tuning_reg.get();

        updateEntry<MUINT8>(&metaset.halMeta, MTK_3A_PGN_ENABLE, 0);
        updateEntry<MUINT8>(&metaset.halMeta, MTK_3A_ISP_PROFILE, profile);
        // to makes debug info different
        updateEntry<MINT32>(&metaset.halMeta, MTK_PIPELINE_REQUEST_NUMBER, m_blendCount - 1);

        MetaSet_T rMetaSet;

        if (m_p3A->setIsp(0, metaset, &rTuningParam, &rMetaSet) == 0) {
            mfllLogD("%s: get tuning data, reg=%p, lsc=%p",
                    __FUNCTION__, rTuningParam.pRegBuf, rTuningParam.pLsc2Buf);

            {
                int bilinearFilter = (base->getImgSize() == ref->getImgSize() ? 0 : 1);
                dip_x_reg_t *tuningDat = (dip_x_reg_t*)tuning_reg.get();

                workaround_MFB_stage(tuningDat);

                /* MFB CON */
                tuningDat->DIP_X_MFB_CON.Bits.BLD_LL_BRZ_EN = bilinearFilter;
                // 0: first time, 1: blend with input weighting
                tuningDat->DIP_X_MFB_CON.Bits.BLD_MODE      = (index == 0 ? 0 : 1);

                /* MFB_LL_CON3 */
                /* output resolution */
                tuningDat->DIP_X_MFB_LL_CON3.Raw =
                    (wt_out->getImgSize().h << 16) | wt_out->getImgSize().w;

                if (m_bExifDumpped[STAGE_MFB] == 0) {
                    dump_exif_info(m_pCore, tuningDat, STAGE_MFB);
                    m_bExifDumpped[STAGE_MFB] = 1;
                }
#ifdef __DEBUG
                debug_pass2_registers(tuningDat, STAGE_MFB);
#endif
            }
            params.mvTuningData.push_back((void*)tuning_reg.get()); // adding tuning data

        }
        else {
            mfllLogE("%s: set ISP profile failed...", __FUNCTION__);
            err = MfllErr_UnexpectedError;
            goto lbExit;
        }

        /**
         *  Finally, we're going to invoke P2 driver to encode Raw. Notice that,
         *  we must use async function call to trigger P2 driver, which means
         *  that we have to give a callback function to P2 driver.
         *
         *  E.g.:
         *      QParams::mpfnCallback = CALLBACK_FUNCTION
         *      QParams::mpCookie --> argument that CALLBACK_FUNCTION will get
         *
         *  Due to we wanna make the P2 driver as a synchronized flow, here we
         *  simply using Mutex to sync async call of P2 driver.
         */
        Mutex mutex;
        mutex.lock();

        p2cbParam_t p2cbPack(this, &mutex);

        params.mpfnCallback = pass2CbFunc;
        params.mpCookie = (void*)&p2cbPack;

        mfllTraceBegin(sStrP2);
        mfllLogD("%s: enque pass 2", __FUNCTION__);

        if (!m_pNormalStream->enque(params)) {
            mfllLogE("%s: pass 2 enque returns fail", __FUNCTION__);
            err = MfllErr_UnexpectedError;
            goto lbExit;
        }

        mfllLogD("%s: deque pass 2", __FUNCTION__);

        /* lock again, and wait */
        mutex.lock(); // locked, and wait unlock from pass2CbFunc.
        mutex.unlock(); // unlock.
        mfllTraceEnd();

        mfllLogD("mfb dequed");
    }
lbExit:
    return err;
}

enum MfllErr MfllMfb::mix(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt)
{
    mfllAutoLogFunc();
    mfllAutoTraceFunc();

    enum MfllErr err = MfllErr_Ok;

    IImageBuffer *img_src = 0;
    IImageBuffer *img_ref = 0;
    IImageBuffer *img_dst = 0;
    IImageBuffer *img_wt = 0;

    /* check buffers */
    if (base == 0 || ref == 0 || out == 0 || wt == 0) {
        mfllLogE("%s: any argument cannot be NULL", __FUNCTION__);
        return MfllErr_BadArgument;
    }

    img_src = (IImageBuffer*)base->getImageBuffer();
    img_ref = (IImageBuffer*)ref->getImageBuffer();
    img_dst = (IImageBuffer*)out->getImageBuffer();
    img_wt =  (IImageBuffer*)wt->getImageBuffer();

    /* check resolution */
    {
        MSize size_src = img_src->getImgSize();
        MSize size_dst = img_dst->getImgSize();
        MSize size_wt  = img_wt->getImgSize();

        if (size_src != size_dst || size_src != size_wt) {
            mfllLogE("%s: Resolution of images are not the same", __FUNCTION__);
            mfllLogE("%s: src=%dx%d, dst=%dx%d, wt=%dx%d",
                    __FUNCTION__,
                    size_src.w, size_src.h,
                    size_dst.w, size_dst.h,
                    size_wt.w, size_wt.h);
            return MfllErr_BadArgument;
        }
    }

    m_mutex.lock();
    int sensorId = m_sensorId;
    enum NoiseReductionType nrType = m_nrType;
    enum MfllMode shotMode = m_shotMode;
    EIspProfile_T profile = (EIspProfile_T)0; // which will be set later.
    void *privateData = m_syncPrivateData;
    size_t privateDataSize = m_syncPrivateDataSize;
    int index = m_pCore->getIndexByNewIndex(0);
    /* get metaset */
    if (index >= static_cast<int>(m_vMetaSet.size())) {
        mfllLogE("%s: index(%d) is out of size of metaset(%zu)", __FUNCTION__, index, m_vMetaSet.size());
        m_mutex.unlock();
        return MfllErr_UnexpectedError;
    }
    MetaSet_T metaset = m_vMetaSet[index];
    m_mutex.unlock();

    MBOOL ret = MTRUE;

    /**
     * P A S S 2
     */
    QParams params;

    /* Mixing */
    params.mvStreamTag.push_back(NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_MFB_Mix);

    // input: blended frame [IMGI port]
    {
        Input p;
        p.mPortID       = MIX_PORT_IN_BASE_FRAME; // should be IMGI port
        p.mPortID.group = 0;
        p.mBuffer       = img_src;
        params.mvIn.push_back(p);

        /* cropping info */
        // Even though no cropping necessary, but we have to pass in a crop info
        // with group id = 1 because pass 2 driver MUST need it
        MCrpRsInfo crop;
        crop.mGroupID       = 1;
        crop.mCropRect.p_integral.x = 0; // position pixel, in integer
        crop.mCropRect.p_integral.y = 0;
        crop.mCropRect.p_fractional.x = 0; // always be 0
        crop.mCropRect.p_fractional.y = 0;
        crop.mCropRect.s.w  = img_src->getImgSize().w;
        crop.mCropRect.s.h  = img_src->getImgSize().h;
        crop.mResizeDst.w   = img_src->getImgSize().w;
        crop.mResizeDst.h   = img_src->getImgSize().h;
        crop.mFrameGroup    = 0;
        params.mvCropRsInfo.push_back(crop);
    }

    // input: golden frame
    {
        Input p;
        p.mPortID       = MIX_PORT_IN_GOLDEN_FRAME;
        p.mPortID.group = 0;
        p.mBuffer       = img_ref;
        params.mvIn.push_back(p);
        /* this port is not support crop info */
    }

    // input: weighting
    {
        Input p;
        p.mPortID       = MIX_PORT_IN_WEIGHTING;
        p.mPortID.group = 0;
        p.mBuffer       = img_wt;
        params.mvIn.push_back(p);
        /* this port is not support crop info */
    }

    // output: a frame
    {
        Output p;
        p.mPortID       = MIX_PORT_OUT_FRAME;
        p.mPortID.group = 0;
        p.mBuffer       = img_dst;
        params.mvOut.push_back(p);
        /* this port is not support crop info */
    }

    /* determine ISP profile */
    if (isZsdMode(shotMode)) {
        profile = (nrType == NoiseReductionType_SWNR)
            ? get_isp_profile(eMfllIspProfile_AfterBlend_Zsd_Swnr)
            : get_isp_profile(eMfllIspProfile_AfterBlend_Zsd);
        if (IS_WRONG_ISP_PROFILE(profile)) {
            mfllLogE("%s: error isp profile mapping", __FUNCTION__);
        }
    }
    else {
        profile = (nrType == NoiseReductionType_SWNR)
            ? get_isp_profile(eMfllIspProfile_AfterBlend_Swnr)
            : get_isp_profile(eMfllIspProfile_AfterBlend);
        if (IS_WRONG_ISP_PROFILE(profile)) {
            mfllLogE("%s: error isp profile mapping", __FUNCTION__);
        }
    }

    /* execute pass 2 operation */
    {
        /* pass 2 critical section */
        Mutex::Autolock _l(gMutexPass2Lock);

        /* add tuning data is necessary */
        std::unique_ptr<char> tuning_reg(new char[sizeof(dip_x_reg_t)]());
        void *tuning_lsc;

        mfllLogD("%s: create tuning register data chunk with size %zu",
                __FUNCTION__, sizeof(dip_x_reg_t));

        TuningParam rTuningParam = {NULL, NULL, NULL};
        rTuningParam.pRegBuf = tuning_reg.get();

        updateEntry<MUINT8>(&metaset.halMeta, MTK_3A_PGN_ENABLE, 0);
        updateEntry<MUINT8>(&metaset.halMeta, MTK_3A_ISP_PROFILE, profile);

        MetaSet_T rMetaSet;

        if (m_p3A->setIsp(0, metaset, &rTuningParam, &rMetaSet) == 0) {

            mfllLogD("%s: get tuning data, reg=%p, lsc=%p",
                    __FUNCTION__, rTuningParam.pRegBuf, rTuningParam.pLsc2Buf);

            dip_x_reg_t *tuningDat = (dip_x_reg_t*)tuning_reg.get();

            if (m_bExifDumpped[STAGE_MIX] == 0) {
                dump_exif_info(m_pCore, tuningDat, STAGE_MIX);
                m_bExifDumpped[STAGE_MIX] = 1;
            }

#ifdef __DEBUG
            debug_pass2_registers(tuningDat, STAGE_MIX);
#endif

            params.mvTuningData.push_back(tuning_reg.get()); // adding tuning data
        }
        else {
            mfllLogE("%s: set ISP profile failed...", __FUNCTION__);
            err = MfllErr_UnexpectedError;
            return err;
        }

        /**
         *  Finally, we're going to invoke P2 driver to encode Raw. Notice that,
         *  we must use async function call to trigger P2 driver, which means
         *  that we have to give a callback function to P2 driver.
         *
         *  E.g.:
         *      QParams::mpfnCallback = CALLBACK_FUNCTION
         *      QParams::mpCookie --> argument that CALLBACK_FUNCTION will get
         *
         *  Due to we wanna make the P2 driver as a synchronized flow, here we
         *  simply using Mutex to sync async call of P2 driver.
         */
        Mutex mutex;
        mutex.lock();

        p2cbParam_t p2cbPack(this, &mutex);

        params.mpfnCallback = pass2CbFunc;
        params.mpCookie = (void*)&p2cbPack;

        mfllTraceBegin(sStrP2);
        if (!m_pNormalStream->enque(params)) {
            mfllLogE("%s: pass 2 enque returns fail", __FUNCTION__);
            err = MfllErr_UnexpectedError;
            goto lbExit;
        }

        /* lock again, and wait */
        mutex.lock(); // locked, and wait unlock from pass2CbFunc.
        mutex.unlock(); // unlock.
        mfllTraceEnd();

        mfllLogD("mix dequed");
    }

lbExit:
    return err;
}

enum MfllErr MfllMfb::setSyncPrivateData(const std::deque<void*>& dataset)
{
    mfllLogD("dataset size=%zu", dataset.size());
    if (dataset.size() <= 0) {
        mfllLogW("set sync private data receieve NULL, ignored");
        return MfllErr_Ok;
    }

    Mutex::Autolock _l(m_mutex);

    /**
     *  dataset is supposed to be contained two address, the first one is the
     *  address of App IMetadata, the second one is Hal. Please make sure the
     *  caller also send IMetadata addresses.
     */
    if (dataset.size() < 3) {
        mfllLogE(
                "%s: missed data in dataset, dataset size is %zu, " \
                "and it's supposed to be: "                         \
                "[0]: addr of app metadata, "                       \
                "[1]: addr of hal metadata, "                       \
                "[2]: addr of an IImageBuffer of LCSO",
                __FUNCTION__, dataset.size());
        return MfllErr_BadArgument;
    }
    IMetadata *pAppMeta = static_cast<IMetadata*>(dataset[0]);
    IMetadata *pHalMeta = static_cast<IMetadata*>(dataset[1]);
    IImageBuffer *pLcsoImg = static_cast<IImageBuffer*>(dataset[2]);

    MetaSet_T m;
    m.halMeta = *pHalMeta;
    m.appMeta = *pAppMeta;
    m_vMetaSet.push_back(m);
    m_vMetaApp.push_back(pAppMeta);
    m_vMetaHal.push_back(pHalMeta);
    m_vImgLcsoRaw.push_back(pLcsoImg);
    mfllLogD("saves MetaSet_T, size = %zu", m_vMetaSet.size());
    return MfllErr_Ok;
}

/******************************************************************************
 * encodeRawToYuv
 *
 * Interface for encoding a RAW buffer to an YUV buffer
 *****************************************************************************/
enum MfllErr MfllMfb::encodeRawToYuv(IMfllImageBuffer *input, IMfllImageBuffer *output, const enum YuvStage &s)
{
    return encodeRawToYuv(
            input,
            output,
            NULL,
            MfllRect_t(),
            MfllRect_t(),
            s);
}

/******************************************************************************
 * encodeRawToYuv
 *
 * Interface for encoding a RAW buffer to two YUV buffers
 *****************************************************************************/
enum MfllErr MfllMfb::encodeRawToYuv(
            IMfllImageBuffer *input,
            IMfllImageBuffer *output,
            IMfllImageBuffer *output_q,
            const MfllRect_t& output_crop,
            const MfllRect_t& output_q_crop,
            enum YuvStage s /* = YuvStage_RawToYuy2 */)
{
    enum MfllErr err = MfllErr_Ok;
    IImageBuffer *iimgOut2 = NULL;

    if (input == NULL) {
        mfllLogE("%s: input buffer is NULL", __FUNCTION__);
        err = MfllErr_BadArgument;
    }
    if (output == NULL) {
        mfllLogE("%s: output buffer is NULL", __FUNCTION__);
        err = MfllErr_BadArgument;
    }
    if (err != MfllErr_Ok)
        goto lbExit;

    if (output_q) {
        iimgOut2 = (IImageBuffer*)output_q->getImageBuffer();
    }

    err = encodeRawToYuv(
            (IImageBuffer*)input->getImageBuffer(),
            (IImageBuffer*)output->getImageBuffer(),
            iimgOut2,
            output_crop,
            output_q_crop,
            s);

lbExit:
    return err;

}

/******************************************************************************
 * encodeRawToYuv
 *
 * Real implmentation that to control PASS 2 drvier for encoding RAW to YUV
 *****************************************************************************/
enum MfllErr MfllMfb::encodeRawToYuv(
        IImageBuffer *src,
        IImageBuffer *dst,
        IImageBuffer *dst2,
        const MfllRect_t& output_crop,
        const MfllRect_t& output_q_crop,
        const enum YuvStage &s)
{
    enum MfllErr err = MfllErr_Ok;
    MBOOL bRet = MTRUE;
    EIspProfile_T profile = get_isp_profile(eMfllIspProfile_BeforeBlend);

    mfllAutoLogFunc();
    mfllTraceCall();

    /* If it's encoding base RAW ... */
    bool bBaseYuvStage = (s == YuvStage_BaseYuy2 || s == YuvStage_GoldenYuy2) ? true : false;

    m_mutex.lock();
    int index = m_encodeYuvCount++;
    int sensorId = m_sensorId;
    IImageBuffer *pInput = src;
    IImageBuffer *pOutput = dst;
    IImageBuffer *pOutputQ = dst2;
    int gid = 1;
    /* Do not increase YUV stage if it's encoding base YUV or golden YUV */
    if (bBaseYuvStage) {
        m_encodeYuvCount--;
        index = m_pCore->getIndexByNewIndex(0); // Use the first metadata
    }

    /* check if metadata is ok */
    if ((size_t)index >= m_vMetaSet.size()) {
        mfllLogE("%s: index(%d) is out of metadata set size(%zu) ",
                __FUNCTION__,
                index,
                m_vMetaSet.size());
        m_mutex.unlock();
        return MfllErr_UnexpectedError;
    }
    MetaSet_T metaset = m_vMetaSet[index]; // using copy
    IMetadata *pHalMeta = m_vMetaHal[index]; // get address of IMetadata
    IImageBuffer *pImgLcsoRaw = m_vImgLcsoRaw[index]; // get LCSO RAW
    m_mutex.unlock();

    bool bIsYuv2Yuv = !isRawFormat(static_cast<EImageFormat>(pInput->getImgFormat()));

    /**
     *  Select profile based on Stage:
     *  1) Raw to Yv16
     *  2) Encoding base YUV
     *  3) Encoding golden YUV
     *
     */
    switch(s) {
    case YuvStage_RawToYuy2:
    case YuvStage_RawToYv16:
    case YuvStage_BaseYuy2:
        if (isZsdMode(m_shotMode)) {
            profile = get_isp_profile(eMfllIspProfile_BeforeBlend_Zsd);// Not related with MNR
            if (IS_WRONG_ISP_PROFILE(profile)) {
                mfllLogE("%s: error isp profile mapping", __FUNCTION__);
            }
        }
        else {
            profile = get_isp_profile(eMfllIspProfile_BeforeBlend);
            if (IS_WRONG_ISP_PROFILE(profile)) {
                mfllLogE("%s: error isp profile mapping", __FUNCTION__);
            }
        }
        break;
    case YuvStage_GoldenYuy2:
        if (isZsdMode(m_shotMode)) {
            profile = (m_nrType == NoiseReductionType_SWNR)
                ? get_isp_profile(eMfllIspProfile_Single_Zsd_Swnr)
                : get_isp_profile(eMfllIspProfile_Single_Zsd);
            if (IS_WRONG_ISP_PROFILE(profile)) {
                mfllLogE("%s: error isp profile mapping", __FUNCTION__);
            }
        }
        else {
            profile = (m_nrType == NoiseReductionType_SWNR)
                ? get_isp_profile(eMfllIspProfile_Single_Swnr)
                : get_isp_profile(eMfllIspProfile_Single);
            if (IS_WRONG_ISP_PROFILE(profile)) {
                mfllLogE("%s: error isp profile mapping", __FUNCTION__);
            }
        }
        break;
    default:
        // do nothing
        break;
    } // switch

    /**
     *  Ok, we're going to configure P2 driver
     */
    QParams params;

    /* If using ZSD mode, to use Vss profile can improve performance */
    params.mvStreamTag.push_back(isZsdMode(m_shotMode)
            ? NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Vss
            : NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Normal
            );

    /* input: source frame [IMGI port] */
    {
        Input p;
        p.mBuffer          = pInput;
        p.mPortID          = RAW2YUV_PORT_IN;
        p.mPortID.group    = 0; // always be 0
        params.mvIn.push_back(p);

        /* cropping info */
        MCrpRsInfo crop;
        crop.mGroupID       = RAW2YUV_GID_IN;
        crop.mCropRect.p_integral.x = 0; // position pixel, in integer
        crop.mCropRect.p_integral.y = 0;
        crop.mCropRect.p_fractional.x = 0; // always be 0
        crop.mCropRect.p_fractional.y = 0;
        crop.mCropRect.s.w  = pInput->getImgSize().w;
        crop.mCropRect.s.h  = pInput->getImgSize().h;
        crop.mResizeDst.w   = pInput->getImgSize().w;
        crop.mResizeDst.h   = pInput->getImgSize().h;
        crop.mFrameGroup    = 0;
        params.mvCropRsInfo.push_back(crop);
        mfllLogD("output: srcCrop (x,y,w,h)=(%d,%d,%d,%d)",
                crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                crop.mCropRect.s.w, crop.mCropRect.s.h);
        mfllLogD("output: dstSize (w,h)=(%d,%d",
                crop.mResizeDst.w, crop.mResizeDst.h);
    }

    /* input: LCSO histogram [LCEI port], if need, only RAW->YUV needs */
    if (pImgLcsoRaw && (bIsYuv2Yuv == false)) {
        Input p;
        p.mBuffer          = pImgLcsoRaw;
        p.mPortID          = RAW2YUV_PORT_LCE_IN;
        p.mPortID.group    = 0;
        params.mvIn.push_back(p);
        /* no cropping info need */
        mfllLogD("enable LCEI port for LCSO buffer, size = %d x %d",
                pImgLcsoRaw->getImgSize().w,
                pImgLcsoRaw->getImgSize().h);
    }

    /* output: destination frame [WDMAO port] */
    {
        Output p;
        p.mBuffer          = pOutput;
        p.mPortID          = RAW2YUV_PORT_OUT;
        p.mPortID.group    = 0; // always be 0
        params.mvOut.push_back(p);

        /**
         *  Check resolution between input and output, if the ratio is different,
         *  a cropping window should be applied.
         */
        MRect srcCrop =
            (
                output_crop.size() <= 0
                // original size of source
                ? MRect(MPoint(0, 0), pInput->getImgSize())
                // user specified cropping window
                : MRect(MPoint(output_crop.x, output_crop.y),
                        MSize(output_crop.w, output_crop.h))
            );

        srcCrop = calCrop(
                srcCrop,
                MRect(MPoint(0,0), pOutput->getImgSize()),
                100);

        /**
         *  Cropping info, only works with input port is IMGI port.
         *  mGroupID should be the index of the MCrpRsInfo.
         */
        MCrpRsInfo crop;
        crop.mGroupID       = RAW2YUV_GID_OUT;
        crop.mCropRect.p_integral.x = srcCrop.p.x; // position pixel, in integer
        crop.mCropRect.p_integral.y = srcCrop.p.y;
        crop.mCropRect.p_fractional.x = 0; // always be 0
        crop.mCropRect.p_fractional.y = 0;
        crop.mCropRect.s.w  = srcCrop.s.w;
        crop.mCropRect.s.h  = srcCrop.s.h;
        crop.mResizeDst.w   = pOutput->getImgSize().w;
        crop.mResizeDst.h   = pOutput->getImgSize().h;
        crop.mFrameGroup    = 0;
        params.mvCropRsInfo.push_back(crop);
        mfllLogD("output: srcCrop (x,y,w,h)=(%d,%d,%d,%d)",
                crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                crop.mCropRect.s.w, crop.mCropRect.s.h);
        mfllLogD("output: dstSize (w,h)=(%d,%d",
                crop.mResizeDst.w, crop.mResizeDst.h);
    }

    /* output2: destination frame [WROTO prot] */
    if (pOutputQ) {
        Output p;
        p.mBuffer          = pOutputQ;
        p.mPortID          = RAW2YUV_PORT_OUT2;
        p.mPortID.group    = 0; // always be 0
        params.mvOut.push_back(p);

        /**
         *  Check resolution between input and output, if the ratio is different,
         *  a cropping window should be applied.
         */

        MRect srcCrop =
            (
                output_q_crop.size() <= 0
                // original size of source
                ? MRect(MPoint(0, 0), pInput->getImgSize())
                // user specified crop
                : MRect(MPoint(output_q_crop.x, output_q_crop.y),
                        MSize(output_q_crop.w, output_q_crop.h))
            );
        srcCrop = calCrop(
                srcCrop,
                MRect(MPoint(0,0), pOutputQ->getImgSize()),
                100);

        /**
         *  Cropping info, only works with input port is IMGI port.
         *  mGroupID should be the index of the MCrpRsInfo.
         */
        MCrpRsInfo crop;
        crop.mGroupID       = RAW2YUV_GID_OUT2;
        crop.mCropRect.p_integral.x = srcCrop.p.x; // position pixel, in integer
        crop.mCropRect.p_integral.y = srcCrop.p.y;
        crop.mCropRect.p_fractional.x = 0; // always be 0
        crop.mCropRect.p_fractional.y = 0;
        crop.mCropRect.s.w  = srcCrop.s.w;
        crop.mCropRect.s.h  = srcCrop.s.h;
        crop.mResizeDst.w   = pOutputQ->getImgSize().w;
        crop.mResizeDst.h   = pOutputQ->getImgSize().h;
        crop.mFrameGroup    = 0;
        params.mvCropRsInfo.push_back(crop);

        mfllLogD("output2: srcCrop (x,y,w,h)=(%d,%d,%d,%d)",
                crop.mCropRect.p_integral.x, crop.mCropRect.p_integral.y,
                crop.mCropRect.s.w, crop.mCropRect.s.h);
        mfllLogD("output2: dstSize (w,h)=(%d,%d",
                crop.mResizeDst.w, crop.mResizeDst.h);
    }

    /* wait JOB MFB done */
    do {
        auto status = MfllOperationSync::getInstance()->waitJob(MfllOperationSync::JOB_MFB, 200); // wait only for 200 ms
        if (status == std::cv_status::timeout) {
            mfllLogW("wait timeout(200ms) of job MFB, ignored");
        }
    } while(0);

    /* get ready to operate P2 driver, it's a long stroy ... */
    {
        /* pass 2 critical section */
        mfllLogD("%s: wait pass2 critical section", __FUNCTION__);
        Mutex::Autolock _l(gMutexPass2Lock);
        mfllLogD("%s: enter pass2 critical section", __FUNCTION__);

        /* add tuning data is necessary */
        std::unique_ptr<char> tuning_reg(new char[sizeof(dip_x_reg_t)]());
        void *tuning_lsc;

        mfllLogD("%s: create tuning register data chunk with size %zu",
                __FUNCTION__, sizeof(dip_x_reg_t));

        {
            TuningParam rTuningParam = {NULL, NULL, NULL};
            rTuningParam.pRegBuf = tuning_reg.get();
            rTuningParam.pLcsBuf = pImgLcsoRaw; // gives LCS buffer, even though it's NULL.
            // PGN only enable when it's converting RAW to YUV
            updateEntry<MUINT8>(
                    &metaset.halMeta,
                    MTK_3A_PGN_ENABLE,
                    (bIsYuv2Yuv ? MFALSE : MTRUE));

            // if shot mode is single frame, do not use MFNR related ISP profile
            mfllLogD("%s: shotMode=%#x", __FUNCTION__, m_shotMode);
            if (m_shotMode & (1 << MfllMode_Bit_SingleFrame))
                mfllLogD("%s: do not use Mfll related ISP profile for SF",__FUNCTION__);
            else
                updateEntry<MUINT8>(&metaset.halMeta, MTK_3A_ISP_PROFILE, profile);

            // debug {{{
            {
                MUINT8 __profile = 0;
                if (tryGetMetaData<MUINT8>(&metaset.halMeta, MTK_3A_ISP_PROFILE, __profile)) {
                    mfllLogD("%s: isp profile=%#x", __FUNCTION__, __profile);
                }
                else {
                    mfllLogD("%s: isp profile=NULL", __FUNCTION__);
                }
            }
            // }}}

            MetaSet_T rMetaSet;

            if (m_p3A->setIsp(0, metaset, &rTuningParam, &rMetaSet) == 0) {

                mfllLogD("%s: get tuning data, reg=%p, lsc=%p",
                        __FUNCTION__, rTuningParam.pRegBuf, rTuningParam.pLsc2Buf);

                // if dump RAW is enabled, dump ISP table too
                // {{{
                do {
                    MfllCore *c = reinterpret_cast<MfllCore*>(m_pCore);
                    if (MfllProperty::readProperty(Property_DumpRaw) <= 0)
                        break;
                    if (c && c->m_spDump.get() == NULL)
                        break;

                    std::string fileName = c->m_spDump->getFilePath()
                        .append("_")
                        .append("isp_table_")
                        .append(std::to_string(index))
                        .append(".bin");

                    FILE *fp = fopen(fileName.c_str(), "wb");
                    if (fp == NULL) {
                        mfllLogE("cannot write ISP table");
                        break;
                    }
                    fwrite(rTuningParam.pRegBuf, 1L, sizeof(dip_x_reg_t), fp);
                    fclose(fp);
                } while (0);
                // }}}

                // if using feed RAW, read ISP table
                // {{{
                do {
                    MfllCore *c = reinterpret_cast<MfllCore*>(m_pCore);
                    if (MfllProperty::readProperty(Property_FeedRaw) <= 0)
                        break;
                    MfllFeedRawConfig_t feedCfg;
                    auto err = c->readFeedRawConfiguration(&feedCfg);
                    if (err != MfllErr_Ok) {
                        mfllLogE("read feed RAW config failed");
                        break;
                    }

                    // read ISP table
                    std::string f = feedCfg.param[std::string("ISP")
                        .append(std::to_string(index))];

                    mfllLogD("%s: reading ISP table from SDCARD", __FUNCTION__);
                    mfllLogD("%s: file: %s", __FUNCTION__, f.c_str());

                    FILE *fp = fopen(f.c_str(), "rb");
                    if (fp == NULL) {
                        mfllLogE("%s: ISP table not exists: %s", __FUNCTION__, f.c_str());
                        break;
                    }

                    // get file size
                    fseek(fp, 0, SEEK_END);
                    size_t binarySize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    if (binarySize <= 0) {
                        mfllLogE("%s: ISP table size is <= 0", __FUNCTION__);
                        fclose(fp);
                        break;
                    }

                    // read chunk
                    std::unique_ptr<char> binaryIsp(new char[binarySize]);
                    fread(binaryIsp.get(), 1L, binarySize, fp);
                    fclose(fp);

                    // check file size vs table
                    if (binarySize != sizeof(dip_x_reg_t)) {
                        mfllLogE("%s: ISP table binary size is different \
                                between dip_x_reg_t (%zu vs %zu)",
                                __FUNCTION__,
                                binarySize, sizeof(dip_x_reg_t));
                        break;
                    }

                    // apply it
                    memcpy(tuning_reg.get(), binaryIsp.get(), binarySize);
                } while (0);
                // }}}

                dip_x_reg_t *tuningDat = (dip_x_reg_t*)tuning_reg.get();

                // update Exif info
                switch (s) {
                case YuvStage_BaseYuy2: // stage bfbld
                    if (m_bExifDumpped[STAGE_BASE_YUV] == 0) {
                        dump_exif_info(m_pCore, tuningDat, STAGE_BASE_YUV);
                        m_bExifDumpped[STAGE_BASE_YUV] = 1;
                    }
                case YuvStage_RawToYuy2: // stage bfbld
                case YuvStage_RawToYv16: // stage bfbld
                    /* update metadata within the one given by IHal3A only in stage bfbld*/
                    *pHalMeta += rMetaSet.halMeta;
                    break;

                case YuvStage_GoldenYuy2: // stage sf
                    if (m_bExifDumpped[STAGE_GOLDEN_YUV] == 0) {
                        dump_exif_info(m_pCore, tuningDat, STAGE_GOLDEN_YUV);
                        m_bExifDumpped[STAGE_GOLDEN_YUV] = 1;
                    }
                    break;

                default:
                    // do nothing
                    break;
                }

                params.mvTuningData.push_back(tuning_reg.get()); // adding tuning data

                /* LSC tuning data is constructed as an IImageBuffer, and we don't need to release */
                IImageBuffer* pSrc = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);
                if (pSrc != NULL) {
                    Input __src;
                    __src.mPortID         = PORT_DEPI;
                    __src.mPortID.group   = 0;
                    __src.mBuffer         = pSrc;
                    params.mvIn.push_back(__src);
                }
                else {
                    // check if it's process RAW, if yes, there's no LSC.
                    MINT32 __rawType = NSIspTuning::ERawType_Pure;
                    if (!tryGetMetaData<MINT32>(
                                &metaset.halMeta,
                                MTK_P1NODE_RAW_TYPE,
                                __rawType)) {
                        mfllLogW("get p1 node RAW type fail, assume it's pure RAW");
                    }

                    // check if LSC table should exists or not.
                    if (__rawType == NSIspTuning::ERawType_Proc) {
                        mfllLogD("process RAW, no LSC table");
                    }
                    else if (bIsYuv2Yuv) {
                        mfllLogD("Yuv->Yuv, no LSC table");
                    }
                    else {
                        mfllLogE("create LSC image buffer fail");
                        err = MfllErr_UnexpectedError;
                    }
                }
            }
            else {
                mfllLogE("%s: set ISP profile failed...", __FUNCTION__);
                err = MfllErr_UnexpectedError;
                goto lbExit;
            }
        }

        /**
         *  Finally, we're going to invoke P2 driver to encode Raw. Notice that,
         *  we must use async function call to trigger P2 driver, which means
         *  that we have to give a callback function to P2 driver.
         *
         *  E.g.:
         *      QParams::mpfnCallback = CALLBACK_FUNCTION
         *      QParams::mpCookie --> argument that CALLBACK_FUNCTION will get
         *
         *  Due to we wanna make the P2 driver as a synchronized flow, here we
         *  simply using Mutex to sync async call of P2 driver.
         */
        Mutex mutex;
        mutex.lock();

        p2cbParam_t p2cbPack(this, &mutex);

        params.mpfnCallback = pass2CbFunc;
        params.mpCookie = (void*)&p2cbPack;

        mfllLogD("%s: enque", __FUNCTION__);

#ifdef __DEBUG
        if (pOutputQ) {
            mfllLogD("%s: VA input=%p,output1=%p,output2=%p", __FUNCTION__,
                    pInput->getBufVA(0), pOutput->getBufVA(0), pOutputQ->getBufVA(0));
            mfllLogD("%s: PA input=%p,output1=%p,output2=%p", __FUNCTION__,
                    pInput->getBufPA(0), pOutput->getBufPA(0), pOutputQ->getBufPA(0));
        }
        else {
            mfllLogD("%s: VA input=%p,output1=%p", __FUNCTION__,
                    pInput->getBufVA(0), pOutput->getBufVA(0));
            mfllLogD("%s: PA input=%p,output1=%p", __FUNCTION__,
                    pInput->getBufPA(0), pOutput->getBufPA(0));
        }
#endif

        mfllTraceBegin(sStrP2);
        if (!m_pNormalStream->enque(params)) {
            mfllLogE("%s: pass 2 enque fail", __FUNCTION__);
            err = MfllErr_UnexpectedError;
            goto lbExit;
        }
        mfllLogD("%s: dequed", __FUNCTION__);

        /* lock again, and wait */
        mutex.lock(); // locked, and wait unlock from pass2CbFunc.
        mutex.unlock(); // unlock.
        mfllTraceEnd();

        mfllLogD("p2 dequed");
    }

lbExit:
    return err;
}
