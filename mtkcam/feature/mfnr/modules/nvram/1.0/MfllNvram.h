#ifndef __MFLLNVRAM_H__
#define __MFLLNVRAM_H__

#include "IMfllNvram.h"

#include <camera_custom_nvram.h>

#include <utils/Mutex.h>
#include <memory>

using android::sp;
using android::Mutex;

namespace mfll {

class MfllNvram : public IMfllNvram {
public:
    MfllNvram();

/* implementation from IMfllNvram */
public:
    virtual enum MfllErr init(int sensorId);
    virtual std::shared_ptr<char> chunk(size_t *bufferSize);
    virtual const char* getChunk(size_t *bufferSize);

/* Attributes */
private:
    std::shared_ptr<char> m_nvramChunk;
    size_t m_chunkSize;
    Mutex m_mutex;
    int m_sensorId;

protected:
    virtual ~MfllNvram(void);
};
}; /* namespace mfll */
#endif//__MFLLNVRAM_H__
