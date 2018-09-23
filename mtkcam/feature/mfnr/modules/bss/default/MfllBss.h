#ifndef __MFLLBSS_H__
#define __MFLLBSS_H__

#include "IMfllBss.h"
#include "IMfllCore.h"

using std::vector;
using android::sp;

namespace mfll {
class MfllBss : public IMfllBss {
public:
    enum MfllErr init(sp<IMfllNvram>&)
    { return MfllErr_NotImplemented; }
    vector<int> bss(
            const vector< sp<IMfllImageBuffer> > &imgs,
            vector<MfllMotionVector_t> &mvs)
    { return vector<int>(); }

    enum MfllErr setMfllCore(IMfllCore *c)
    { return MfllErr_NotImplemented; }

    void setRoiPercentage(const int &p) { }

    size_t getSkipFrameCount() { return 0; }

protected:
    ~MfllBss() {};
}; // class MfllBss
}; // namespace mfll

#endif//__MFLLBSS_H__
