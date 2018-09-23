#ifndef __IMFLLCORE2_H__
#define __IMFLLCORE2_H__

#include "IMfllCore.h"

using android::sp;
using std::vector;

namespace mfll {

// MFNR Core version 2
//
// MFNR Core version 2 is base on MFNR Core version 1.6 and add new features
// into version 2. All the original features in 1.6 is still avaliable.
//
// Caller has no need to create IMfllMfb and IMfllCapturer anymore, but only
// needs to create IMfllCore2 instance. All the behaviors should be implemented
// in IMfllCore2.
class IMfllCore2 : public IMfllCore {
public:
    // Create an MFNR core library with specified version.
    //  @param major_ver    Major version, if not specified, use default.
    //  @param minor_ver    Minor version, if not specified, use default.
    //  @return If the version caller asks hasn't implemented, returns NULL.
    static IMfllCore2*      createInstance(int major_ver = -1, int minor_ver = -1);

// new APIs
// MFNR Core 2
public:
    using IMfllCore::init;  // tells compiler we want both
    virtual enum MfllErr    init(const MfllConfig2& cfg) = 0;

    virtual enum MfllErr    enqueFullSizeRaw(
                                sp<IMfllImageBuffer> raw,
                                MfllMotionVector_t gmv,
                                void* pMetaApp = NULL,
                                void* pMetaHal = NULL
                            ) = 0;

    virtual enum MfllErr    enqueFullSizeYuv(
                                sp<IMfllImageBuffer> yuv,
                                void* syncObject = NULL
                            ) = 0;

    virtual enum MfllErr    enqueGmvInfo(
                                const MfllMotionVector_t& mv
                            ) = 0;



// phase-out APIs
// these methods return MfllErr_NotImplemented
public:
    virtual enum MfllErr    setCapturer(const sp<IMfllCapturer> &capturer) override = 0;
    virtual enum MfllErr    setMfb(const sp<IMfllMfb> &mfb) override = 0;
    virtual sp<IMfllMfb>    getMfb() override = 0;

protected:
    virtual ~IMfllCore2(void) = default;

}; // class IMfllCore2
}; // namespace mfll
#endif
