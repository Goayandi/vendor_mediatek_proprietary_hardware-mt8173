#ifndef __MFLLNVRAM_H__
#define __MFLLNVRAM_H__

#include "IMfllNvram.h"

#include <memory>

namespace mfll {

class MfllNvram : public IMfllNvram {
public:
    MfllNvram() {};

/* implementation from IMfllNvram */
public:
    virtual enum MfllErr init(int sensorId)
    { return MfllErr_NotImplemented; }
    virtual std::shared_ptr<char> chunk(size_t *bufferSize)
    { return NULL; }
    virtual const char* getChunk(size_t *bufferSize)
    { return NULL; }

protected:
    virtual ~MfllNvram(void) {};
};
}; /* namespace mfll */
#endif//__MFLLNVRAM_H__
