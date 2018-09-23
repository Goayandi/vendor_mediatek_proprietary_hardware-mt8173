#include "MfllMfb.h"

using namespace mfll;
using android::sp;

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
}

MfllMfb::~MfllMfb(void)
{
}

enum MfllErr MfllMfb::init(int sensorId)
{
    return MfllErr_Ok;
}

enum MfllErr MfllMfb::blend(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt_in, IMfllImageBuffer *wt_out)
{
    return MfllErr_Ok;
}

enum MfllErr MfllMfb::mix(IMfllImageBuffer *base, IMfllImageBuffer *ref, IMfllImageBuffer *out, IMfllImageBuffer *wt)
{
    return MfllErr_Ok;
}

enum MfllErr MfllMfb::setWeightingBuffer(sp<IMfllImageBuffer> input, sp<IMfllImageBuffer> output)
{
    return MfllErr_Ok;
}

enum MfllErr MfllMfb::setSyncPrivateData(const std::deque<void*>& /* dataset */)
{
    return MfllErr_Ok;
}

sp<IMfllImageBuffer> MfllMfb::getWeightingBufferInput(void)
{
    return sp<IMfllImageBuffer>(NULL);
}

sp<IMfllImageBuffer> MfllMfb::getWeightingBufferOutput(void)
{
    return sp<IMfllImageBuffer>(NULL);
}

enum MfllErr MfllMfb::encodeRawToYuv(IMfllImageBuffer *input, IMfllImageBuffer *output, const enum YuvStage &s)
{
    return MfllErr_Ok;
}
