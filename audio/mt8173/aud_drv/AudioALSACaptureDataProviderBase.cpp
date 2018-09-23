#define LOG_TAG "AudioALSACaptureDataProviderBase"

#include "AudioALSACaptureDataProviderBase.h"

#include <inttypes.h>
#include <utils/threads.h>
#include <sys/prctl.h>
#include "AudioType.h"
#include "AudioLock.h"

#include "AudioALSACaptureDataClient.h"

//#define DEBUG_CAPTURE_DELAY
//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)


namespace android
{
int AudioALSACaptureDataProviderBase::mDumpFileNum = 0;

AudioALSACaptureDataProviderBase::AudioALSACaptureDataProviderBase() :
    mCaptureFramesReaded(0),
    mEnable(false),
    mOpenIndex(0),
    mCaptureDataClientIndex(0),
    mPcm(NULL),
    mCaptureDataProviderType(CAPTURE_PROVIDER_BASE),
    mReadBufferSize(0),
    mCaptureDropSize(0)
{
    ALOGV("%s(), %p", __FUNCTION__, this);

    mCaptureDataClientVector.clear();

    memset((void *)&mPcmReadBuf, 0, sizeof(mPcmReadBuf));

    memset((void *)&mConfig, 0, sizeof(mConfig));

    memset((void *)&mStreamAttributeSource, 0, sizeof(mStreamAttributeSource));

    memset((void *)&mCaptureTimeStamp, 0, sizeof(timespec));

    mPCMDumpFile = NULL;

    mlatency = 0;
}

AudioALSACaptureDataProviderBase::~AudioALSACaptureDataProviderBase()
{
    ALOGV("%s(), %p", __FUNCTION__, this);
}

status_t AudioALSACaptureDataProviderBase::openPcmDriver(const unsigned int device)
{
    ALOGD("+%s(), pcm device = %d", __FUNCTION__, device);

    ASSERT(mPcm == NULL);
    mPcm = pcm_open(AudioALSADeviceParser::getInstance()->GetCardIndex(), device, PCM_IN, &mConfig);
    if (mPcm == NULL)
    {
        ALOGE("%s(), mPcm == NULL!!", __FUNCTION__);
    }
    else if (pcm_is_ready(mPcm) == false)
    {
        ALOGE("%s(), pcm_is_ready(%p) == false due to %s, close pcm.", __FUNCTION__, mPcm, pcm_get_error(mPcm));
        pcm_close(mPcm);
        mPcm = NULL;
    }
    else
    {
        pcm_start(mPcm);
    }

    ALOGD("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    ASSERT(mPcm != NULL);
    return NO_ERROR;
}

status_t AudioALSACaptureDataProviderBase::closePcmDriver()
{
    ALOGD("+%s(), mPcm = %p", __FUNCTION__, mPcm);

    if (mPcm != NULL)
    {
        pcm_stop(mPcm);
        pcm_close(mPcm);
        mPcm = NULL;
    }

    ALOGD("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    return NO_ERROR;
}

void AudioALSACaptureDataProviderBase::attach(AudioALSACaptureDataClient *pCaptureDataClient)
{
    ALOGV("%s(), %p", __FUNCTION__, this);
    AudioAutoTimeoutLock _l(mClientLock);

    pCaptureDataClient->setIdentity(mCaptureDataClientIndex);
    ALOGV("%s(), mCaptureDataClientIndex=%u, mCaptureDataClientVector.size()=%zd, Identity=%u", __FUNCTION__, mCaptureDataClientIndex, mCaptureDataClientVector.size(),
          pCaptureDataClient->getIdentity());
    mCaptureDataClientVector.add(pCaptureDataClient->getIdentity(), pCaptureDataClient);
    mCaptureDataClientIndex++;

    // open pcm interface when 1st attach
    if (mCaptureDataClientVector.size() == 1)
    {
        mOpenIndex++;
        open();
    }
    ALOGV("-%s()", __FUNCTION__);
}

void AudioALSACaptureDataProviderBase::detach(AudioALSACaptureDataClient *pCaptureDataClient)
{
    ALOGV("%s(),%p, Identity=%u, mCaptureDataClientVector.size()=%zd,mCaptureDataProviderType=%d, %p", __FUNCTION__, this, pCaptureDataClient->getIdentity(), mCaptureDataClientVector.size(),
          mCaptureDataProviderType, pCaptureDataClient);
    AudioAutoTimeoutLock _l(mClientLock);

    mCaptureDataClientVector.removeItem(pCaptureDataClient->getIdentity());
    // close pcm interface when there is no client attached
    if (mCaptureDataClientVector.size() == 0)
    {
        close();
        ALOGV("%s(), close finish", __FUNCTION__);
    }
    ALOGV("-%s()", __FUNCTION__);
}

void AudioALSACaptureDataProviderBase::start(AudioALSACaptureDataClient *pCaptureDataClient __unused)
{
    AudioAutoTimeoutLock _l(mClientLock);
    if (!mEnable) {
        run();
    }
}

void AudioALSACaptureDataProviderBase::provideCaptureDataToAllClients(const uint32_t open_index)
{
    ALOGVV("+%s()", __FUNCTION__);
    AudioAutoTimeoutLock _l(mClientLock);

    if (open_index != mOpenIndex)
    {
        ALOGW("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, mOpenIndex);
        return;
    }

    AudioALSACaptureDataClient *pCaptureDataClient = NULL;

    WritePcmDumpData();
    for (size_t i = 0; i < mCaptureDataClientVector.size(); i++)
    {
        pCaptureDataClient = mCaptureDataClientVector[i];
        pCaptureDataClient->copyCaptureDataToClient(mPcmReadBuf);
    }

    ALOGVV("-%s()", __FUNCTION__);
}

bool AudioALSACaptureDataProviderBase::HasLowLatencyCapture(void)
{
    ALOGV("+%s()", __FUNCTION__);
    //AudioAutoTimeoutLock _l(mClientLock);
    bool bRet = false;
    AudioALSACaptureDataClient *pCaptureDataClient = NULL;

    for (size_t i = 0; i < mCaptureDataClientVector.size(); i++)
    {
        pCaptureDataClient = mCaptureDataClientVector[i];
        if (pCaptureDataClient->IsLowLatencyCapture())
        {
            bRet = true;
            break;
        }
    }
    ALOGV("-%s(), bRet=%d", __FUNCTION__,bRet);
    return bRet;
}

void AudioALSACaptureDataProviderBase::OpenPCMDump(const char *class_name)
{
    ALOGV("%s(), mCaptureDataProviderType=%d", __FUNCTION__, mCaptureDataProviderType);
    String8 dump_file_name;
    dump_file_name.appendFormat("%s.%d.%s.%d_%dch_format(0x%x).pcm", streamin, mDumpFileNum, class_name,
                                mStreamAttributeSource.sample_rate, mStreamAttributeSource.num_channels,
                                mStreamAttributeSource.audio_format);

    mPCMDumpFile = AudioOpendumpPCMFile(dump_file_name.string(), streamin_propty);
    if (mPCMDumpFile != NULL)
    {
        ALOGD("%s DumpFileName = %s", __FUNCTION__, dump_file_name.string());

        mDumpFileNum++;
        mDumpFileNum %= MAX_DUMP_NUM;
    }
}

void AudioALSACaptureDataProviderBase::ClosePCMDump()
{
    if (mPCMDumpFile)
    {
        AudioCloseDumpPCMFile(mPCMDumpFile);
        ALOGV("%s(), mCaptureDataProviderType=%d", __FUNCTION__, mCaptureDataProviderType);
        mPCMDumpFile = NULL;
    }
}

void  AudioALSACaptureDataProviderBase::WritePcmDumpData(void)
{
    if (mPCMDumpFile)
    {
        AudioDumpPCMData((void *)mPcmReadBuf.pBufBase , mPcmReadBuf.bufLen - 1, mPCMDumpFile);
    }
}

//echoref+++
void AudioALSACaptureDataProviderBase::provideEchoRefCaptureDataToAllClients(const uint32_t open_index)
{
    ALOGVV("+%s()", __FUNCTION__);
    AudioAutoTimeoutLock _l(mClientLock);

    if (open_index != mOpenIndex)
    {
        ALOGW("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, mOpenIndex);
        return;
    }

    AudioALSACaptureDataClient *pCaptureDataClient = NULL;

    WritePcmDumpData();
    for (size_t i = 0; i < mCaptureDataClientVector.size(); i++)
    {
        pCaptureDataClient = mCaptureDataClientVector[i];
        pCaptureDataClient->copyEchoRefCaptureDataToClient(mPcmReadBuf);
    }

    ALOGVV("-%s()", __FUNCTION__);
}
//echoref---

status_t AudioALSACaptureDataProviderBase::GetCaptureTimeStamp(time_info_struct_t *Time_Info, unsigned int read_size)
{
    ASSERT(mPcm != NULL);

    Time_Info->timestamp_get.tv_sec  = 0;
    Time_Info->timestamp_get.tv_nsec = 0;
    Time_Info->frameInfo_get = 0;
    Time_Info->buffer_per_time = 0;
    Time_Info->kernelbuffer_ns = 0;

    //ALOGD("%s(), Going to check pcm_get_htimestamp", __FUNCTION__);
    if (pcm_get_htimestamp(mPcm, &Time_Info->frameInfo_get, &Time_Info->timestamp_get) == 0)
    {
        Time_Info->buffer_per_time = pcm_bytes_to_frames(mPcm, read_size);
        Time_Info->kernelbuffer_ns = 1000000000 / mStreamAttributeSource.sample_rate * (Time_Info->buffer_per_time + Time_Info->frameInfo_get);
        Time_Info->total_frames_readed += Time_Info->buffer_per_time;
        ALOGVV("%s(), pcm_get_htimestamp sec = %ld, nsec = %ld, frameInfo_get = %u, buffer_per_time = %u, ret_ns = %lu, read_size = %u\n",
              __FUNCTION__, Time_Info->timestamp_get.tv_sec, Time_Info->timestamp_get.tv_nsec, Time_Info->frameInfo_get,
              Time_Info->buffer_per_time, Time_Info->kernelbuffer_ns, read_size);

        // Write time stamp to cache to avoid getCapturePosition performance issue
        mTimeStampLock.lock();
        mCaptureFramesReaded = Time_Info->total_frames_readed;
        mCaptureTimeStamp = Time_Info->timestamp_get;
        mTimeStampLock.unlock();
#if 0
        if ((TimeStamp->tv_nsec - ret_ns) >= 0)
        {
            TimeStamp->tv_nsec -= ret_ns;
        }
        else
        {
            TimeStamp->tv_sec -= 1;
            TimeStamp->tv_nsec = 1000000000 + TimeStamp->tv_nsec - ret_ns;
        }

        ALOGD("%s[%d] calculate pcm_get_htimestamp sec= %ld, nsec=%ld, avail = %d, ret_ns = %ld\n",
              __FUNCTION__, mCaptureDataProviderType, TimeStamp->tv_sec, TimeStamp->tv_nsec, avail, ret_ns);
#endif
    }
    else
    {
        ALOGE("%s[%d] pcm_get_htimestamp fail %s\n", __FUNCTION__, mCaptureDataProviderType, pcm_get_error(mPcm));
    }
    return NO_ERROR;
}

int AudioALSACaptureDataProviderBase::getCapturePosition(int64_t *frames, int64_t *time)
{
    mTimeStampLock.lock();
    *frames = mCaptureFramesReaded;
    *time = mCaptureTimeStamp.tv_sec * 1000000000LL + mCaptureTimeStamp.tv_nsec;
    ALOGVV("%s(), return frames = %" PRIu64 ", time = %" PRIu64 "", __FUNCTION__, *frames, *time);
    mTimeStampLock.unlock();

    return 0;
}

const stream_attribute_t *AudioALSACaptureDataProviderBase::getStreamAttributeTarget()
{
    if (mCaptureDataClientVector[0])
        return mCaptureDataClientVector[0]->getStreamAttributeTarget();
    else
        return NULL;
}

status_t AudioALSACaptureDataProviderBase::run()
{
    AudioAutoTimeoutLock _l(mEnableLock);

    if (!mEnable) {
        mEnable = true;
        // create reading thread
        int ret = pthread_create(&hReadThread, NULL,
                            AudioALSACaptureDataProviderBase::captureReadThread, (void *)this);
        if (ret != 0) {
            ALOGE("%s() create thread fail!!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }
    return NO_ERROR;
}

void *AudioALSACaptureDataProviderBase::captureReadThread(void *arg)
{
    pthread_detach(pthread_self());

    status_t retval = NO_ERROR;
    AudioALSACaptureDataProviderBase *pDataProvider = static_cast<AudioALSACaptureDataProviderBase *>(arg);

    uint32_t open_index = pDataProvider->mOpenIndex;

    char nameset[32];
    sprintf(nameset, "%s%d", __FUNCTION__, pDataProvider->mCaptureDataProviderType);
    prctl(PR_SET_NAME, (unsigned long)nameset, 0, 0, 0);

    ALOGV("+%s[%d] pid: %d, tid: %d, mReadBufferSize=%x, open_index=%d",
          __FUNCTION__, pDataProvider->mCaptureDataProviderType, getpid(), gettid(), pDataProvider->mReadBufferSize, open_index);

    // read raw data from alsa driver
    uint32_t ReadBufferSize = pDataProvider->mReadBufferSize;
    char *linear_buffer = new char[ReadBufferSize];
    uint32_t Read_Size = ReadBufferSize;
    while (pDataProvider->mEnable == true)
    {
        if (open_index != pDataProvider->mOpenIndex)
        {
            ALOGW("%s[%d] open_index(%d) != mOpenIndex(%d), return",
                  __FUNCTION__, pDataProvider->mCaptureDataProviderType, open_index, pDataProvider->mOpenIndex);
            break;
        }

        retval = pDataProvider->mEnableLock.lock_timeout(300);
        ASSERT(retval == NO_ERROR);
        if (pDataProvider->mEnable == false)
        {
            pDataProvider->mEnableLock.unlock();
            break;
        }

        ASSERT(pDataProvider->mPcm != NULL);
#ifdef DEBUG_CAPTURE_DELAY
        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[0] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;
#endif

        if (pDataProvider->mCaptureDropSize == 0)
        {
            int retval = pcm_read(pDataProvider->mPcm, linear_buffer, ReadBufferSize);
            if (retval != 0)
            {
                ALOGE("%s[%d] pcm_read() error, retval = %d", __FUNCTION__, pDataProvider->mCaptureDataProviderType, retval);
            }
        }
        else
        {
            Read_Size = (pDataProvider->mCaptureDropSize > ReadBufferSize) ? ReadBufferSize : pDataProvider->mCaptureDropSize;
            int retval = pcm_read(pDataProvider->mPcm, linear_buffer, Read_Size);

            if (retval != 0)
            {
                ALOGE("%s[%d] pcm_read() drop error, retval = %d", __FUNCTION__, pDataProvider->mCaptureDataProviderType, retval);
            }
            ALOGV("%s[%d] mCaptureDropSize = %d Read_Size = %d",
                  __FUNCTION__, pDataProvider->mCaptureDataProviderType, pDataProvider->mCaptureDropSize, Read_Size);
            pDataProvider->mCaptureDropSize -= Read_Size;
            pDataProvider->mEnableLock.unlock();
            continue;
        }

#ifdef DEBUG_CAPTURE_DELAY
        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[1] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;
#endif
        //struct timespec tempTimeStamp;
        pDataProvider->GetCaptureTimeStamp(&pDataProvider->mStreamAttributeSource.Time_Info, ReadBufferSize);

        // use ringbuf format to save buffer info
        pDataProvider->mPcmReadBuf.pBufBase = linear_buffer;
        pDataProvider->mPcmReadBuf.bufLen   = ReadBufferSize + 1; // +1: avoid pRead == pWrite
        pDataProvider->mPcmReadBuf.pRead    = linear_buffer;
        pDataProvider->mPcmReadBuf.pWrite   = linear_buffer + ReadBufferSize;
        pDataProvider->mEnableLock.unlock();

        if (pDataProvider->mCaptureDataProviderType == CAPTURE_PROVIDER_ECHOREF ||
            pDataProvider->mCaptureDataProviderType == CAPTURE_PROVIDER_ECHOREF_BTSCO ||
            pDataProvider->mCaptureDataProviderType == CAPTURE_PROVIDER_ECHOREF_EXT) {
            pDataProvider->provideEchoRefCaptureDataToAllClients(open_index);
        } else {
            pDataProvider->provideCaptureDataToAllClients(open_index);
        }

#ifdef DEBUG_CAPTURE_DELAY
        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[2] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;
        ALOGV("%s[%d] latency_in_us,%1.6lf,%1.6lf,%1.6lf", __FUNCTION__, pDataProvider->mCaptureDataProviderType,
               pDataProvider->timerec[0], pDataProvider->timerec[1], pDataProvider->timerec[2]);
#endif
    }

    if (linear_buffer) {
        delete [] linear_buffer;
    }

    ALOGV("-%s[%d], pid: %d, tid: %d", __FUNCTION__, pDataProvider->mCaptureDataProviderType, getpid(), gettid());
    pthread_exit(NULL);
    return NULL;
}

} // end of namespace android

