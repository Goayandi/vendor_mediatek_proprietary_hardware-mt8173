#ifndef __MfllMfb_H__
#define __MfllMfb_H__

#include "IMfllMfb.h"
#include "MfllImageBuffer.h"

/* mtkcam related headers */
#include <IImageBuffer.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/aaa/IHal3A.h>

#include <utils/Mutex.h> // android::Mutex

#include <deque>

#define STAGE_BASE_YUV      0
#define STAGE_MFB           1
#define STAGE_GOLDEN_YUV    2
#define STAGE_MIX           3
#define STAGE_SIZE          4

using android::sp;
using android::Mutex;
using NSCam::IImageBuffer;
using NSCam::MRect;
using NSCam::MSize;
using NSCam::MPoint;
using NS3Av3::MetaSet_T;
using NSCam::NSIoPipe::NSPostProc::INormalStream;
using NS3Av3::IHal3A;
using NSCam::IMetadata;

namespace mfll {
class MfllMfb : public IMfllMfb {
public:
    MfllMfb(void);
    virtual ~MfllMfb(void);

/* implementations */
public:
    virtual enum MfllErr init(int sensorId);
    virtual void setMfllCore(IMfllCore *c) { m_pCore = c; }
    virtual void setShotMode(const enum MfllMode &mode) { m_shotMode = mode; }
    virtual void setPostNrType(const enum NoiseReductionType &type) { m_nrType = type; }
    virtual enum MfllErr blend(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt_in, IMfllImageBuffer *wt_out);
    virtual enum MfllErr mix(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt);
    virtual enum MfllErr setSyncPrivateData(const std::deque<void*>& dataset);

    /* Raw to Yuv */
    virtual enum MfllErr encodeRawToYuv(IMfllImageBuffer *input, IMfllImageBuffer *output, const enum YuvStage &s);
    virtual enum MfllErr encodeRawToYuv(
            IMfllImageBuffer *input,
            IMfllImageBuffer *output,
            IMfllImageBuffer *output_q,
            const MfllRect_t& output_crop,
            const MfllRect_t& output_q_crop,
            enum YuvStage s = YuvStage_RawToYuy2);

/* private interface */
protected:
    enum MfllErr encodeRawToYuv(
            IImageBuffer *src,
            IImageBuffer *dst,
            IImageBuffer *dst2,
            const MfllRect_t& output_crop,
            const MfllRect_t& output_q_crop,
            const enum YuvStage &s);

    enum MfllErr blend(IImageBuffer *base, IImageBuffer *ref, IImageBuffer *out, IImageBuffer *wt_in, IImageBuffer *wt_out);

/* attributes */
private:
    /* thread-safe protector */
    mutable Mutex m_mutex;

private:
    int m_sensorId;
    enum MfllMode m_shotMode;
    enum NoiseReductionType m_nrType;
    void *m_syncPrivateData;
    size_t m_syncPrivateDataSize;

    std::deque<MetaSet_T>       m_vMetaSet;
    std::deque<IMetadata*>      m_vMetaApp;
    std::deque<IMetadata*>      m_vMetaHal;
    std::deque<IImageBuffer*>   m_vImgLcsoRaw;

    IMfllCore *m_pCore;

    IHal3A          *m_p3A;
    INormalStream   *m_pNormalStream;

    volatile int m_encodeYuvCount;
    volatile int m_blendCount;
    volatile bool m_bIsInited;

    bool m_bExifDumpped[STAGE_SIZE];
};
};/* namespace mfll */
#endif//__MfllMfb_H__

