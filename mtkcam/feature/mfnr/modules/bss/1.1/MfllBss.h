#ifndef __MFLLBSS_H__
#define __MFLLBSS_H__

#include "IMfllBss.h"

#define MFLL_BSS_ROI_PERCENTAGE 95

using std::vector;

namespace mfll {
class MfllBss : public IMfllBss {
public:
    MfllBss();

public:
    enum MfllErr init(sp<IMfllNvram> &nvramProvider);

    vector<int> bss(
            const vector< sp<IMfllImageBuffer> > &imgs,
            vector<MfllMotionVector_t> &mvs);

    enum MfllErr setMfllCore(IMfllCore *c) { m_pCore = c; return MfllErr_Ok; }

    void setRoiPercentage(const int &p) { m_roiPercetange = p; }

    size_t getSkipFrameCount(void) { return m_skipFrmCnt; }

protected:
    int m_roiPercetange;
    sp<IMfllNvram> m_nvramProvider;
    IMfllCore *m_pCore;
    size_t m_skipFrmCnt;

protected:
    ~MfllBss() {};
}; // class MfllBss
}; // namespace mfll

#endif//__MFLLBSS_H__
