#ifndef __MFLLSTRATEGY_H__
#define __MFLLSTRATEGY_H__

#include "IMfllStrategy.h"

#include <libais/MTKAis.h>

#include <memory>

using android::sp;

namespace mfll {
class MfllStrategy : public IMfllStrategy {
public:
    MfllStrategy();

public:
    enum MfllErr init(sp<IMfllNvram> &nvramProvider);
    enum MfllErr queryStrategy(
            const MfllStrategyConfig_t &cfg,
            MfllStrategyConfig_t *out);

/**
 *  Attributes
 */
private:
    sp<IMfllNvram> m_nvramProvider;
    std::shared_ptr<MTKAis> m_mtkAis;
    const char *m_nvramChunk;
    int m_captureFrameNum;
    int m_blendFrameNum;
    int m_isFullSizeMc;

protected:
    ~MfllStrategy(void);
};
}; /* namespace mfll */
#endif//__MFLLSTRATEGY_H__
