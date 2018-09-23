#define LOG_TAG "MtkCam/MfllCore/MfllCapturer"

#include "MfllCapturer.h"
#include "MfllMfb.h"
#include "MfllLog.h"
#include "MfllCore.h"

using namespace mfll;

IMfllCapturer* IMfllCapturer::createInstance(void)
{
    return (IMfllCapturer*)new MfllCapturer;
}

void IMfllCapturer::destroyInstance(void)
{
    this->decStrong((void*)this);
}

MfllCapturer::MfllCapturer(void)
: m_frameNum(0)
, m_captureMode(static_cast<MfllMode>(0))
, m_postNrType(NoiseReductionType_None)
, m_pCore(NULL)
, m_mutexRaw(PTHREAD_MUTEX_INITIALIZER)
, m_condRaw(PTHREAD_COND_INITIALIZER)
, m_mutexGmv(PTHREAD_MUTEX_INITIALIZER)
, m_condGmv(PTHREAD_COND_INITIALIZER)
{
}

MfllCapturer::~MfllCapturer(void)
{
    mfllLogD("Delete MfllCapturer");
}

enum MfllErr MfllCapturer::captureFrames(
    unsigned int captureFrameNum,
    std::vector< sp<IMfllImageBuffer>* > &raws,
    std::vector< sp<IMfllImageBuffer>* > &qyuvs,
    std::vector<MfllMotionVector_t*>     &gmvs,
    std::vector<int>                     &status
    )
{
    std::vector< sp<IMfllImageBuffer>* > ____NULL;
    return __captureFrames(
            captureFrameNum,
            raws,       // raw
            ____NULL,   // yuv
            qyuvs,      // qyuv
            gmvs,
            status
            );      // gmv
}

enum MfllErr MfllCapturer::captureFrames(
        unsigned int captureFrameNum,
        std::vector< sp<IMfllImageBuffer>* > &raws,
        std::vector< sp<IMfllImageBuffer>* > &yuvs,
        std::vector< sp<IMfllImageBuffer>* > &qyuvs,
        std::vector< MfllMotionVector_t* >   &gmvs,
        std::vector<int>                     &status
)
{
    return __captureFrames(
            captureFrameNum,
            raws,
            yuvs,
            qyuvs,
            gmvs,
            status
            );
}

enum MfllErr MfllCapturer::__captureFrames(
    unsigned int captureFrameNum,
    std::vector< sp<IMfllImageBuffer>* > &raws,
    std::vector< sp<IMfllImageBuffer>* > &yuvs,
    std::vector< sp<IMfllImageBuffer>* > &qyuvs,
    std::vector< MfllMotionVector_t* >   &gmvs,
    std::vector<int>                     &status
)
{
    enum MfllErr err = MfllErr_Ok;

    /* is need full YUV */
    bool bFullYuv = yuvs.size() > 0 ? true : false;

    MfllEventStatus_t eventStatusRaw;
    MfllEventStatus_t eventStatusYuv;
    MfllEventStatus_t eventStatusEis;

    sp<IMfllMfb> pmfb = m_pCore->getMfb();
    if (pmfb.get() == NULL) {
        mfllLogE("%s: IMfllMfb instance is NULL", __FUNCTION__);
    }

    /* to read feeding RAWs if necessary */
    // {{{
    sp<IMfllImageBuffer> feedRaws[MFLL_MAX_FRAMES];
    bool bFeedRaws = [this, captureFrameNum, &feedRaws]()->bool {
        if (!m_pCore->isFeedRaw())
            return false;

        MfllFeedRawConfig_t feedCfg;
        if (MfllErr_Ok != m_pCore->readFeedRawConfiguration(&feedCfg))
            return false;

        // read RAW buffers from SDCARD
        for (size_t i = 0; i < captureFrameNum; i++) {
            // check param "RAW"
            std::string f = feedCfg.param[std::string("RAW").append(std::to_string(i))];
            if (f == "") {
                mfllLogE("%s: no RAW%zu specified", __FUNCTION__, i);
                return false;
            }

            mfllLogD("%s: reading RAW buffer from SDCARD", __FUNCTION__);
            mfllLogD("%s: file: %s", __FUNCTION__, f.c_str());
            mfllLogD("%s: (w x h) = (%d x %d)", __FUNCTION__,
                    feedCfg.width, feedCfg.height);
            sp<IMfllImageBuffer> raw = IMfllImageBuffer::createInstance();
            raw->setResolution(feedCfg.width, feedCfg.height);
            raw->setImageFormat(ImageFormat_Raw10);
            auto err = raw->initBuffer();
            if (err != MfllErr_Ok) {
                mfllLogE("%s: init buffer failed", __FUNCTION__);
                return false;
            }

            err = raw->loadFile(f.c_str());
            if (err != MfllErr_Ok) {
                mfllLogE("%s: load feeding RAW %s failed",
                        __FUNCTION__, f.c_str());
                return false;
            }
            feedRaws[i] = raw;
        }
        return true;
    }();
    // }}}

    /* handle RAW to YUV frames first (and then, GMVs) */
    for (size_t i = 0; i < captureFrameNum; i++) {
        /* capture RAW */
        sp<IMfllImageBuffer> r;
        dispatchOnEvent(EventType_CaptureRaw, eventStatusRaw, (void*)(long)i, NULL);
        if (eventStatusRaw.ignore == 0) {
            /* normal case */
            if (bFeedRaws == false)
                r = dequeFullSizeRaw();
            else
                r = feedRaws[i];
        }
        (*raws[i]) = r;
        dispatchDoneEvent(EventType_CaptureRaw, eventStatusRaw, (void*)(long)i, NULL);

        /* capture YUV */
        dispatchOnEvent(EventType_CaptureYuvQ, eventStatusYuv, (void*)(long)i, NULL);
        /* no raw or ignored */
        if (r.get() == NULL || eventStatusYuv.ignore != 0) { // No raw
            eventStatusYuv.err = MfllErr_NullPointer;
            eventStatusYuv.ignore = 1;
            status[i] = 1;
            dispatchDoneEvent(EventType_CaptureYuvQ, eventStatusYuv, (void*)(long)i, NULL);
        }
        else {
            err = reinterpret_cast<MfllCore*>(m_pCore)->doAllocQyuvBuffer((void*)(long)i);
            if (err != MfllErr_Ok) {
                mfllLogE("%s: alloc QYUV buffer %zu failed", __FUNCTION__, i);
                eventStatusYuv.err = err;
                eventStatusYuv.ignore = 1;
                status[i] = 1;
                dispatchDoneEvent(EventType_CaptureYuvQ, eventStatusYuv, (void*)(long)i, NULL);
            }
            else {
                /* tell MfllMfb what shot mode and post NR type is using */
                pmfb->setShotMode(m_captureMode);
                pmfb->setPostNrType(m_postNrType);

                if (bFullYuv) {
                    mfllLogD("%s: RAW to 2 YUVs, %dx%d, %dx%d.", __FUNCTION__,
                            (*yuvs[i])->getWidth(), (*yuvs[i])->getHeight(),
                            (*qyuvs[i])->getWidth(), (*qyuvs[i])->getHeight());

                    err = pmfb->encodeRawToYuv(
                            r.get(),
                            (*yuvs[i]).get(),
                            (*qyuvs[i]).get(),
                            MfllRect_t(), // no need cropping
                            MfllRect_t(), // no need cropping
                            YuvStage_RawToYv16
                            );
                    if (err != MfllErr_Ok) {
                        status[i] = 1;
                    }
                    else
                        status[i] = 0;
                }
                else {
                    err = pmfb->encodeRawToYuv(
                            r.get(),
                            (*qyuvs[i]).get(),
                            YuvStage_RawToYv16);
                    if (err != MfllErr_Ok) {
                        status[i] = 1;
                    }
                    else
                        status[i] = 0;
                }
                eventStatusYuv.err = err;
                dispatchDoneEvent(EventType_CaptureYuvQ, eventStatusYuv, (void*)(long)i, NULL);
            }
        }
    }

    /* GMV */
    for (size_t i = 0; i < captureFrameNum; i++) {
        /* GMV */
        dispatchOnEvent(EventType_CaptureEis, eventStatusEis, (void*)(long)i, NULL);
        MfllMotionVector_t mv;
        if (eventStatusEis.ignore == 0) {
            mv = dequeGmv();
            (*gmvs[i]) = mv;
        }
        dispatchDoneEvent(EventType_CaptureEis, eventStatusEis, (void*)(long)i, NULL);

    }

    mfllLogD("Capture Done");
    return MfllErr_Ok;
}

unsigned int MfllCapturer::getCapturedFrameNum(void)
{
    return m_frameNum;
}

void MfllCapturer::setMfllCore(IMfllCore *c)
{
    m_pCore = c;
}

void MfllCapturer::setShotMode(const enum MfllMode &mode)
{
    m_captureMode = mode;
}

void MfllCapturer::setPostNrType(const enum NoiseReductionType &type)
{
    m_postNrType = type;
}

enum MfllErr MfllCapturer::registerEventDispatcher(const sp<IMfllEvents> &e)
{
    m_spEventDispatcher = e;
    return MfllErr_Ok;
}

enum MfllErr MfllCapturer::queueFullSizeRaw(sp<IMfllImageBuffer> &img)
{
    enum MfllErr err = MfllErr_Ok;
    pthread_mutex_t *mx = &m_mutexRaw;
    pthread_cond_t *cond = &m_condRaw;

    pthread_mutex_lock(mx);
    {
        m_vRaws.push_back(img);
        pthread_cond_signal(cond);
    }
    pthread_mutex_unlock(mx);
    return err;
}

sp<IMfllImageBuffer> MfllCapturer::dequeFullSizeRaw(void)
{
    sp<IMfllImageBuffer> img;
    pthread_mutex_t *mx = &m_mutexRaw;
    pthread_cond_t *cond = &m_condRaw;

    pthread_mutex_lock(mx); // wait if necessary
    {
        /* check buffer is available or not */
        if (m_vRaws.size() <= 0) {
            pthread_cond_wait(cond, mx);
        }
        img = m_vRaws[0];
        m_vRaws.erase(m_vRaws.begin());
    }
    pthread_mutex_unlock(mx);
    return img;
}

enum MfllErr MfllCapturer::queueGmv(const MfllMotionVector_t &mv)
{
    enum MfllErr err = MfllErr_Ok;
    pthread_mutex_t *mx = &m_mutexGmv;
    pthread_cond_t *cond = &m_condGmv;

    pthread_mutex_lock(mx);
    {
        m_vGmvs.push_back(mv);
        pthread_cond_signal(cond);
    }
    pthread_mutex_unlock(mx);
    return err;
}

MfllMotionVector_t MfllCapturer::dequeGmv(void)
{
    MfllMotionVector_t mv;
    pthread_mutex_t *mx = &m_mutexGmv;
    pthread_cond_t *cond = &m_condGmv;

    pthread_mutex_lock(mx);
    {
        if (m_vGmvs.size() <= 0) {
            pthread_cond_wait(cond, mx);
        }

        mv = m_vGmvs[0];
        m_vGmvs.erase(m_vGmvs.begin());
    }
    pthread_mutex_unlock(mx);
    return mv;
}

void MfllCapturer::dispatchOnEvent(const EventType &t, MfllEventStatus_t &s, void *p1, void *p2)
{
    if (m_spEventDispatcher.get()) {
        m_spEventDispatcher->onEvent(t, s, (void*)m_pCore, p1, p2);
    }
}

void MfllCapturer::dispatchDoneEvent(const EventType &t, MfllEventStatus_t &s, void *p1, void *p2)
{
    if (m_spEventDispatcher.get()) {
        m_spEventDispatcher->doneEvent(t, s, (void*)m_pCore, p1, p2);
    }
}

