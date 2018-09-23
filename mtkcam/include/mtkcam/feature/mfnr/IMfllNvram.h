#ifndef __IMFLLNVRAM_H__
#define __IMFLLNVRAM_H__

#include "MfllDefs.h"
#include "MfllTypes.h"

#include <utils/RefBase.h> // android::RefBase
#include <memory>

using android::sp;

/**
 *  MfllNvram has responsibility to read NVRAM data chunk. Different platform
 *  may have different structures to format the chunk. Here only provide a way
 *  to read the NVRAM data chunk, without format it to the certain structures
 */
namespace mfll {
class IMfllNvram : public android::RefBase {
public:
    static IMfllNvram* createInstance(void);
    virtual void destroyInstance(void);

/* interfaces */
public:
    /* NVRAM data chunk is takes sensor ID to be found */
    virtual enum MfllErr init(int sensorId) = 0;

    /* Returns a copied data chunk */
    virtual std::shared_ptr<char> chunk(size_t *bufferSize) = 0;

    /* Returns data chunk address */
    virtual const char* getChunk(size_t *bufferSize) = 0;

protected:
    virtual ~IMfllNvram() { };
};
}; /* namespace mfll */

#endif//__IMFLLNVRAM_H__
