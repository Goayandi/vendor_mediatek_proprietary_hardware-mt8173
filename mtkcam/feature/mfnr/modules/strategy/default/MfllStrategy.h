#ifndef __MFLLSTRATEGY_H__
#define __MFLLSTRATEGY_H__

#include "IMfllStrategy.h"

using android::sp;

namespace mfll {
class MfllStrategy : public IMfllStrategy {
public:
    MfllStrategy() {};

public:
    enum MfllErr init(sp<IMfllNvram> &nvramProvider)
    { return MfllErr_NotImplemented; }
    enum MfllErr queryStrategy(
            const MfllStrategyConfig_t &cfg,
            MfllStrategyConfig_t *out)
    { return MfllErr_NotImplemented; }

protected:
    ~MfllStrategy(void) {};
};
}; /* namespace mfll */
#endif//__MFLLSTRATEGY_H__
