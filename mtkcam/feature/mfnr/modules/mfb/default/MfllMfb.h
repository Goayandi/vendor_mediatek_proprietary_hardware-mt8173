#ifndef __MFLLMFB_H__
#define __MFLLMFB_H__

#include "IMfllMfb.h"

namespace mfll {
class MfllMfb : public IMfllMfb {
public:
    MfllMfb(void);
    virtual ~MfllMfb(void);

public:
    virtual enum MfllErr init(int sensorId);
    virtual enum MfllErr blend(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt_in, IMfllImageBuffer *wt_out);
    virtual enum MfllErr mix(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt);
    virtual enum MfllErr setWeightingBuffer(sp<IMfllImageBuffer> input, sp<IMfllImageBuffer> output);
    virtual enum MfllErr setSyncPrivateData(const std::deque<void*>& dataset);
    virtual sp<IMfllImageBuffer> getWeightingBufferInput(void);
    virtual sp<IMfllImageBuffer> getWeightingBufferOutput(void);
    virtual enum MfllErr encodeRawToYuv(IMfllImageBuffer *input, IMfllImageBuffer *output, const enum YuvStage &s);
    virtual enum MfllErr encodeRawToYuv(
            IMfllImageBuffer*   /* input */         ,
            IMfllImageBuffer*   /* output */        ,
            IMfllImageBuffer*   /* output_q */      ,
            const MfllRect_t&   /* output_crop */   ,
            const MfllRect_t&   /* output_q_crop */ ,
            enum YuvStage s = YuvStage_RawToYuy2)
    { return MfllErr_NotImplemented; }

    void setMfllCore(IMfllCore*){}
    void setShotMode(const enum MfllMode &){}
    void setPostNrType(const enum NoiseReductionType&){}

};
}; /* namespace mfll */

#endif//__MFLLMFB_H__

