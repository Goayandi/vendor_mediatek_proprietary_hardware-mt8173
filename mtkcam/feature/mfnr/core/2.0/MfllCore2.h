#ifndef __MFLLCORE2_H__
#define __MFLLCORE2_H__

#include <IMfllCore2.h>

#include "MfllCore.h"

#include <deque>
#include <mutex>
#include <condition_variable>

namespace mfll {

class MfllCore2 : public MfllCore, public IMfllCore2
{
public:
    MfllCore2(void);
    virtual ~MfllCore2(void) = default;


// implementations from IMfllCore2
public:
    virtual enum MfllErr            init(const MfllConfig2& cfg);

    virtual enum MfllErr            enqueFullSizeRaw(
                                        sp<IMfllImageBuffer> raw,
                                        MfllMotionVector_t gmv,
                                        void* pMetaApp = NULL,
                                        void* pMetaHal = NULL
                                    );

    virtual enum MfllErr            enqueFullSizeYuv(
                                        sp<IMfllImageBuffer> yuv,
                                        void* syncObject = NULL
                                    );

    virtual enum MfllErr            enqueGmvInfo(
                                        const MfllMotionVector_t& mv
                                    );


// attributes & types
public:
    struct SourcePack
    {
        sp<IMfllImageBuffer>        imgBuf;
        sp<IMfllImageBuffer>        imgLcso;
        MfllMotionVector_t          gmv;
        void*                       pMetaApp;
        void*                       pMetaHal;
        SourcePack() : pMetaApp(0), pMetaHal(0) {}
    };

protected:
    struct {
        std::deque<SourcePack>      srcPacks;
        std::mutex                  mx;
        std::condition_variable     cond;
    }
    m_srcFullRaws;


// override MfllCore methods
protected:
    virtual enum MfllErr            doCapture(JOB_VOID) override;


// override MfllCore methods and whicha are implementations of IMfllCore
public:
    virtual enum MfllErr            init(const MfllConfig_t& cfg) override;


// override implementations for IMfllCore using MfllCore's
public:
    virtual enum MfllErr            doMfll() override;
    virtual enum MfllErr            doCancel() override;
    virtual unsigned int            getBlendFrameNum(void) override;
    virtual unsigned int            getCaptureFrameNum(void) override;
    virtual unsigned int            getFrameBlendedNum(void) override;
    virtual unsigned int            getFrameCapturedNum(void) override;
    virtual unsigned int            getMemcInstanceNum(void) override;
    virtual unsigned int            getIndexByNewIndex(const unsigned int &newIndex) override;
    virtual int                     getSensorId(void) override;
    virtual unsigned int            getReversion(void) override;
    virtual std::string             getReversionString(void) override;
    virtual bool                    isShooted(void) override;
    virtual bool                    isFullSizeMc(void) override;
    virtual bool                    isFeedRaw(void) override;
    virtual enum MfllErr            readFeedRawConfiguration(MfllFeedRawConfig_t *config) override;
    virtual enum MfllErr            registerEventListener(const sp<IMfllEventListener> &e) override;
    virtual enum MfllErr            registerEventListenerNoLock(const sp<IMfllEventListener> &e, int useInsert = 0) override;
    virtual enum MfllErr            removeEventListener(IMfllEventListener *e) override;
    virtual enum MfllErr            setBypassOption(const MfllBypassOption_t &b) override;
    virtual enum MfllErr            setBypassOptionAsSingleFrame() override;
    virtual enum MfllErr            setCaptureResolution(unsigned int width, unsigned int height) override;
    virtual enum MfllErr            setPostviewBuffer(
                                        const sp<IMfllImageBuffer>& buffer,
                                        const MfllRect_t& srcCropRgn
                                    ) override;
    virtual sp<IMfllImageBuffer>    retrieveBuffer(const enum MfllBuffer &s, int index = 0) override;
    virtual enum MfllErr            releaseBuffer(const enum MfllBuffer &s, int index = 0) override;
    virtual enum MfllErr            updateExifInfo(unsigned int key, uint32_t value) override;
    virtual const sp<IMfllExifInfo>& getExifContainer() override;

// IMfllCore2 phase-out APIs
public:
    virtual enum MfllErr setCapturer(const sp<IMfllCapturer>& /* capturer */) override
    {
        return MfllErr_NotSupported;
    }

    virtual enum MfllErr setMfb(const sp<IMfllMfb>& /* mfb */) override
    {
        return MfllErr_NotSupported;
    }

    virtual sp<IMfllMfb> getMfb() override
    {
        return NULL;
    }

}; /* class MfllCore2 */
}; /* namespace mfll */
#endif /* __MFLLCORE2_H__ */
