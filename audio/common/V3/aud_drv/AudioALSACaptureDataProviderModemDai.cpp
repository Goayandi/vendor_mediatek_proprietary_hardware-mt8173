#include "AudioALSACaptureDataProviderModemDai.h"
#include <pthread.h>
#include <sys/prctl.h>
#include "AudioALSADeviceParser.h"
#include "AudioALSADriverUtility.h"
#include "AudioType.h"

#define LOG_TAG "AudioALSACaptureDataProviderModemDai"

#define MODDAI_USING_24BITS

namespace android
{

/*==============================================================================
 *                     Constant
 *============================================================================*/

/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderModemDai *AudioALSACaptureDataProviderModemDai::mAudioALSACaptureDataProviderModemDai = NULL;
AudioALSACaptureDataProviderModemDai *AudioALSACaptureDataProviderModemDai::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderModemDai == NULL)
    {
        mAudioALSACaptureDataProviderModemDai = new AudioALSACaptureDataProviderModemDai();
    }
    ASSERT(mAudioALSACaptureDataProviderModemDai != NULL);
    return mAudioALSACaptureDataProviderModemDai;
}

AudioALSACaptureDataProviderModemDai::AudioALSACaptureDataProviderModemDai()
{
    ALOGD("%s()", __FUNCTION__);

    mConfig.channels = kModDaiChanel;
    mConfig.rate = kModDaiSampleRate;

    mConfig.period_size = kModDaiReadPeriodSize;
    mConfig.period_count = kModDaiReadBufferCount;

#ifdef MODDAI_USING_24BITS
    mConfig.format = PCM_FORMAT_S32_LE;
#else
    mConfig.format = PCM_FORMAT_S16_LE;
#endif

    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;

    mCaptureDataProviderType = CAPTURE_PROVIDER_AP_VOICE_DL;
    hReadThread = NULL;
}

AudioALSACaptureDataProviderModemDai::~AudioALSACaptureDataProviderModemDai()
{
    ALOGD("%s()", __FUNCTION__);
}


status_t AudioALSACaptureDataProviderModemDai::open()
{
    ALOGD("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);
    AudioAutoTimeoutLock _l2(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    ASSERT(mEnable == false);
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_MONO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = kModDaiSampleRate;

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;


    OpenPCMDump(LOG_TAG);

    // enable pcm
    int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceDaiCapture);
    int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceDaiCapture);
    ALOGD("AudioALSACaptureDataProviderModemDai::open() pcmindex = %d", pcmindex);

    openPcmDriver(pcmindex);

    // create reading thread
    mEnable = true;
    int ret = pthread_create(&hReadThread, NULL, AudioALSACaptureDataProviderModemDai::readThread, (void *)this);
    if (ret != 0)
    {
        ALOGE("%s() create thread fail!!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t AudioALSACaptureDataProviderModemDai::close()
{
    ALOGD("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class detach

    mEnable = false;
    ALOGD("%s() mEnableLock", __FUNCTION__);
    AudioAutoTimeoutLock _l(mEnableLock);
    ALOGD("%s() getStreamSramDramLock ", __FUNCTION__);
    AudioAutoTimeoutLock _l2(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    ClosePCMDump();

    closePcmDriver();

    return NO_ERROR;
}


void *AudioALSACaptureDataProviderModemDai::readThread(void *arg)
{
    prctl(PR_SET_NAME, (unsigned long)__FUNCTION__, 0, 0, 0);

#ifdef MTK_AUDIO_ADJUST_PRIORITY
    #define RTPM_PRIO_AUDIO_RECORD (77)
    // force to set priority
    struct sched_param sched_p;
    sched_getparam(0, &sched_p);
    sched_p.sched_priority = RTPM_PRIO_AUDIO_RECORD + 1;
    if (0 != sched_setscheduler(0, SCHED_RR, &sched_p))
    {
        ALOGE("[%s] failed, errno: %d", __FUNCTION__, errno);
    }
    else
    {
        sched_p.sched_priority = RTPM_PRIO_AUDIO_CCCI_THREAD;
        sched_getparam(0, &sched_p);
        ALOGD("sched_setscheduler ok, priority: %d", sched_p.sched_priority);
    }
#endif
    ALOGD("+%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());

    status_t retval = NO_ERROR;
    AudioALSACaptureDataProviderModemDai *pDataProvider = static_cast<AudioALSACaptureDataProviderModemDai *>(arg);

    uint32_t open_index = pDataProvider->mOpenIndex;
    int ReformatSize=0;
    // read raw data from alsa driver
    char linear_buffer[kModDaiReadBufferSize];
    while (pDataProvider->mEnable == true)
    {
        if (open_index != pDataProvider->mOpenIndex)
        {
            ALOGD("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, pDataProvider->mOpenIndex);
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
        int retval = pcm_read(pDataProvider->mPcm, linear_buffer, kModDaiReadBufferSize);

#ifdef MODDAI_USING_24BITS
        ReformatSize = FormatTransfer(PCM_FORMAT_S32_LE,PCM_FORMAT_S16_LE,(void*)linear_buffer,kModDaiReadBufferSize);
#else
        ReformatSize = FormatTransfer(PCM_FORMAT_S16_LE,PCM_FORMAT_S16_LE,(void*)linear_buffer,kModDaiReadBufferSize);
#endif

        //ALOGD("pcm_read kModDaiReadBufferSize= %d ReformatSize = %d",kModDaiReadBufferSize,ReformatSize);
        if (retval != 0)
        {
            ALOGE("%s(), pcm_read() error, retval = %d", __FUNCTION__, retval);
        }

        // use ringbuf format to save buffer info
        pDataProvider->mPcmReadBuf.pBufBase = linear_buffer;
        pDataProvider->mPcmReadBuf.bufLen   = ReformatSize + 1; // +1: avoid pRead == pWrite
        pDataProvider->mPcmReadBuf.pRead    = linear_buffer;
        pDataProvider->mPcmReadBuf.pWrite   = linear_buffer + ReformatSize;
        pDataProvider->mEnableLock.unlock();

        pDataProvider->provideCaptureDataToAllClients(open_index);
    }

    ALOGD("-%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    return NULL;
}

} // end of namespace android
