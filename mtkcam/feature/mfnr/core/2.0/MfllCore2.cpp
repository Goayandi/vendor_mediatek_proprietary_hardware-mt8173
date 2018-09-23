#define LOG_TAG "MtkCam/MfllCore2"

#include <MfllLog.h>

#include "MfllCore2.h"

using namespace mfll;

IMfllCore2* IMfllCore2::createInstance(int major_ver, int minor_ver)
{
    // default version
    if (major_ver < 0)      major_ver = MFLL_CORE_VERSION_MAJOR;
    if (minor_ver < 0)      minor_ver = MFLL_CORE_VERSION_MINOR;

    MfllCore2* pCore2 = NULL;
    MfllErr err = MfllErr_Ok;

    if (major_ver != 2) {
        err = MfllErr_BadArgument;
        goto lbExit;
    }

    switch (minor_ver) {
    case 0:
        pCore2 = new MfllCore2;
        break;
    default:
        err = MfllErr_BadArgument;
    }

lbExit:
    mfllLogD("MFNR Core Version: %d.%d.%d",
            MFLL_CORE_VERSION_MAJOR, MFLL_CORE_VERSION_MINOR, MFLL_CORE_VERSION_BUGFIX);

    if (err == MfllErr_Ok) {
        mfllLogD("Create MFNR core version: %d.%d.%d",
                major_ver, minor_ver, MFLL_CORE_VERSION_BUGFIX);
    }
    else
        mfllLogE("Fail to create MFNR core version version = %d.%d.%d",
                major_ver, minor_ver, MFLL_CORE_VERSION_BUGFIX);

    return static_cast<IMfllCore2*>(pCore2);
}

MfllCore2::MfllCore2() : MfllCore()
{
}



enum MfllErr MfllCore2::init(const MfllConfig_t& cfg)
{
    for (size_t i = 0; i < MFLL_MAX_FRAMES; i++)
        m_bypass.bypassAllocRawBuffer[i] = 1;

    m_spMfb = IMfllMfb::createInstance();
    if (m_spMfb.get() == NULL) {
        mfllLogE("create Mfll MFB instance failed");
        return MfllErr_UnexpectedError;
    }
    m_spMfb->setMfllCore(static_cast<MfllCore*>(this));

    return MfllCore::init(cfg);
}

// ----------------------------------------------------------------------------
// Implementation from IMfllCore2 and use the Implementations from MfllCore
// ----------------------------------------------------------------------------
// {{{
enum MfllErr MfllCore2::doMfll()
{
    return MfllCore::doMfll();
}

enum MfllErr MfllCore2::doCancel()
{
    return MfllCore::doCancel();
}

unsigned int MfllCore2::getBlendFrameNum()
{
    return MfllCore::getBlendFrameNum();
}

unsigned int MfllCore2::getCaptureFrameNum()
{
    return MfllCore::getCaptureFrameNum();
}

unsigned int MfllCore2::getFrameBlendedNum()
{
    return MfllCore::getFrameBlendedNum();
}

unsigned int MfllCore2::getFrameCapturedNum()
{
    return MfllCore::getFrameCapturedNum();
}

unsigned int MfllCore2::getMemcInstanceNum()
{
    return MfllCore::getMemcInstanceNum();
}

unsigned int MfllCore2::getIndexByNewIndex(const unsigned int& newIndex)
{
    return MfllCore::getIndexByNewIndex(newIndex);
}

int MfllCore2::getSensorId()
{
    return MfllCore::getSensorId();
}

unsigned int MfllCore2::getReversion()
{
    return MfllCore::getReversion();
}

std::string MfllCore2::getReversionString()
{
    return MfllCore::getReversionString();
}

bool MfllCore2::isShooted()
{
    return MfllCore::isShooted();
}

bool MfllCore2::isFullSizeMc()
{
    return MfllCore::isFullSizeMc();
}

bool MfllCore2::isFeedRaw()
{
    return MfllCore::isFeedRaw();
}

enum MfllErr MfllCore2::readFeedRawConfiguration(MfllFeedRawConfig_t* config)
{
    return MfllCore::readFeedRawConfiguration(config);
}

enum MfllErr MfllCore2::registerEventListener(const sp<IMfllEventListener>& e)
{
    return MfllCore::registerEventListener(e);
}

enum MfllErr MfllCore2::registerEventListenerNoLock(const sp<IMfllEventListener>& e, int useInsert)
{
    return MfllCore::registerEventListenerNoLock(e, useInsert);
}

enum MfllErr MfllCore2::removeEventListener(IMfllEventListener* e)
{
    return MfllCore::removeEventListener(e);
}

enum MfllErr MfllCore2::setBypassOption(const MfllBypassOption_t& b)
{
    return MfllCore::setBypassOption(b);
}

enum MfllErr MfllCore2::setBypassOptionAsSingleFrame()
{
    return MfllCore::setBypassOptionAsSingleFrame();
}

enum MfllErr MfllCore2::setCaptureResolution(unsigned int width, unsigned int height)
{
    return MfllCore::setCaptureResolution(width, height);
}

enum MfllErr MfllCore2::setPostviewBuffer(
    const sp<IMfllImageBuffer>& buffer,
    const MfllRect_t& srcCropRgn)
{
    return MfllCore::setPostviewBuffer(buffer, srcCropRgn);
}

sp<IMfllImageBuffer> MfllCore2::retrieveBuffer(
    const enum MfllBuffer& s,
    int index)
{
    return MfllCore::retrieveBuffer(s, index);
}

enum MfllErr MfllCore2::releaseBuffer(
    const enum MfllBuffer& s,
    int index)
{
    return MfllCore::releaseBuffer(s, index);
}

enum MfllErr MfllCore2::updateExifInfo(unsigned int key, uint32_t value)
{
    return MfllCore::updateExifInfo(key, value);
}

const sp<IMfllExifInfo>& MfllCore2::getExifContainer()
{
    return MfllCore::getExifContainer();
}
// }}}


// ----------------------------------------------------------------------------
// Re-implementations of MfllCore
// ----------------------------------------------------------------------------
enum MfllErr MfllCore2::doCapture(JOB_VOID)
{
    MfllErr err = MfllErr_Ok;
    MfllEventStatus_t status;

    /* invokes events */
    m_event->onEvent(EventType_Capture, status, this);
    /* display enter function log */
    mfllFunctionIn();
    mfllTraceCall();

    if (m_bypass.bypassCapture || status.ignore) {
        mfllLogD("%s: Bypass capture operation", __FUNCTION__);
        status.ignore = 1;
    }
    else {
        std::vector<int> vStatus(getCaptureFrameNum(), 0); // represents status

        for (size_t i = 0; i < getCaptureFrameNum(); i++){
            std::unique_lock<std::mutex> __lock(m_srcFullRaws.mx);
            if (m_srcFullRaws.srcPacks.size() <= 0) {
                m_srcFullRaws.cond.wait(__lock);
            }
            // pop the first RAW
            SourcePack srcPack = std::move(m_srcFullRaws.srcPacks.front());
            m_srcFullRaws.srcPacks.pop_front();

            // saves to MfllCore
            m_imgRaws[i] = srcPack.imgBuf;
            m_globalMv[i] = srcPack.gmv;

            // check input/output buffer is ok or not.
            if (srcPack.imgBuf.get() == NULL) {
                mfllLogE("src raw buffer is NULL, ignore. (idx=%zu)", i);
                vStatus[i] = 1;
                continue;
            }

            err = doAllocQyuvBuffer((void*)(long long)i);
            if (m_imgQYuvs[i].get() == NULL) {
                mfllLogE("dst QYuv is NULL, ignored. (idx=%zu)", i);
                vStatus[i] = 1;
                continue;
            }

            // if full size MC, check full size YUV buffer
            if (m_isFullSizeMc && m_imgYuvs[i].get() == NULL) {
                mfllLogE("dst full Yuv is NULL, ignored. (idx=%zu)", i);
                vStatus[i] = 1;
                continue;
            }

            // RAW to YUV stage
            // prepare sync data for MfllMfb
            std::deque<void*> privateData;
            privateData.push_back(srcPack.pMetaApp);
            privateData.push_back(srcPack.pMetaHal);
            privateData.push_back(srcPack.imgLcso->getImageBuffer());
            m_spMfb->setSyncPrivateData(privateData);

            if (m_isFullSizeMc) {
                err = m_spMfb->encodeRawToYuv(
                        m_imgRaws[i].get(),
                        m_imgYuvs[i].get(),
                        m_imgQYuvs[i].get(),
                        MfllRect_t(),
                        MfllRect_t(),
                        YuvStage_RawToYv16
                        );
                if (err != MfllErr_Ok) {
                    mfllLogE("encoding RAW to YUV fail (idx=%zu)", i);
                    vStatus[i] = 1;
                }
            }
            else {
                err = m_spMfb->encodeRawToYuv(
                        m_imgRaws[i].get(),
                        m_imgQYuvs[i].get(),
                        YuvStage_RawToYv16
                        );
                if (err != MfllErr_Ok) {
                    mfllLogE("encoding RAW to YUV fail (idx=%zu)", i);
                    vStatus[i] = 1;
                }
            }
        }

        // {{{ checks captured buffers
        /* check result, resort buffers, and update frame capture number and blend number if need */
        {
            std::deque< sp<IMfllImageBuffer> > r; // raw buffer;
            std::deque< sp<IMfllImageBuffer> > q; // qyuv buffer
            std::deque< sp<IMfllImageBuffer> > y; // full size yuv buffer
            std::deque< MfllMotionVector_t   > m; // motion vector

            size_t okCount = 0; // counting ok frame numbers

            /* check the status from Capturer, if status is 0 means ok */
            for (size_t i = 0; i < (size_t)getCaptureFrameNum(); i++) {
                /* If not failed, save to vector */
                if (vStatus[i] == 0) {
                    r.push_back(m_imgRaws[i]);
                    q.push_back(m_imgQYuvs[i]);
                    y.push_back(m_imgYuvs[i]);
                    m.push_back(m_globalMv[i]);
                    okCount++;
                }
            }

            m_dbgInfoCore.frameCapture = okCount;
            mfllLogD("capture done, ok count=%zu, num of capture = %d",
                    okCount, getCaptureFrameNum());

            m_caputredCount = okCount;

            /* if not equals, something wrong */
            if (okCount != (size_t)getCaptureFrameNum()) {
                /* sort available buffers continuously */
                for (size_t i = 0; i < okCount; i++) {
                    m_imgRaws[i] = r[i];
                    m_imgYuvs[i] = y[i];
                    m_imgQYuvs[i] = q[i];
                    m_globalMv[i] = m[i];
                }

                /* boundary blending frame number */
                if (getBlendFrameNum() > okCount) {
                    m_dbgInfoCore.frameBlend = okCount;
                    /* by pass un-necessary operation due to no buffer, included the last frame */
                    for (size_t i = (okCount <= 0 ? 0 : okCount - 1); i < getBlendFrameNum(); i++) {
                        m_bypass.bypassMotionEstimation[i] = 1;
                        m_bypass.bypassMotionCompensation[i] = 1;
                        m_bypass.bypassBlending[i] = 1;
                    }
                }
            }
        } // }}}
    }

lbExit:
    status.err = err;
    m_event->doneEvent(EventType_Capture, status, this);
    syncAnnounceDone(&m_syncCapture);
    mfllFunctionOut();
    return err;
}


// ----------------------------------------------------------------------------
// new API sensors
// ----------------------------------------------------------------------------
enum MfllErr MfllCore2::init(const MfllConfig2& cfg)
{
    return init(static_cast<MfllConfig_t>(cfg));
}

enum MfllErr MfllCore2::enqueFullSizeRaw(
        sp<IMfllImageBuffer> raw,
        MfllMotionVector_t gmv,
        void* pMetaApp,
        void* pMetaHal
)
{
    std::lock_guard<std::mutex> __lock(m_srcFullRaws.mx);
    SourcePack p;
    p.imgBuf = raw;
    p.gmv = gmv;
    p.pMetaApp = pMetaApp;
    p.pMetaHal = pMetaHal;
    m_srcFullRaws.srcPacks.push_back(std::move(p));
    m_srcFullRaws.cond.notify_one();

    return MfllErr_Ok;
}

enum MfllErr MfllCore2::enqueFullSizeYuv(
        sp<IMfllImageBuffer> yuv,
        void* syncObject)
{
    return MfllErr_NotImplemented;
}

enum MfllErr MfllCore2::enqueGmvInfo(const MfllMotionVector_t& mv)
{
    return MfllErr_NotImplemented;
}
