#define LOG_TAG "AudioALSACaptureDataProviderExternal"

#include "AudioALSACaptureDataProviderExternal.h"
#include <pthread.h>
#include "AudioType.h"
#include "AudioALSACustomDeviceString.h"


namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/

static const uint32_t kExternalCaptureBufferSize = 0x4000;
static const uint32_t kReadBufferSize = 0x1e00;



/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderExternal *AudioALSACaptureDataProviderExternal::mAudioALSACaptureDataProviderExternal = NULL;
AudioALSACaptureDataProviderExternal *AudioALSACaptureDataProviderExternal::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderExternal == NULL)
    {
        mAudioALSACaptureDataProviderExternal = new AudioALSACaptureDataProviderExternal();
    }
    ASSERT(mAudioALSACaptureDataProviderExternal != NULL);
    return mAudioALSACaptureDataProviderExternal;
}

AudioALSACaptureDataProviderExternal::AudioALSACaptureDataProviderExternal()
{
    ALOGV("%s()", __FUNCTION__);

    mCaptureDataProviderType = CAPTURE_PROVIDER_EXTERNAL;
}

AudioALSACaptureDataProviderExternal::~AudioALSACaptureDataProviderExternal()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureDataProviderExternal::open()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);

    // config attribute (will used in client SRC/Enh/... later)
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);    
    mStreamAttributeSource.sample_rate = 48000;

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    mConfig.channels = mStreamAttributeSource.num_channels;
    mConfig.rate = mStreamAttributeSource.sample_rate;
#ifdef UPLINK_LOW_LATENCY
    uint32_t captureDurationMs = getLatencyTime();
    mConfig.period_count = (captureDurationMs == UPLINK_LOW_LATENCY_MS) ? 8 : 4;
    mConfig.period_size = mStreamAttributeSource.sample_rate * captureDurationMs / 1000;
    mReadBufferSize = mConfig.period_size * mConfig.period_count * audio_bytes_per_sample(mStreamAttributeSource.audio_format);
#else
    mConfig.period_count = 2;
    mConfig.period_size = (kExternalCaptureBufferSize / mConfig.channels) / audio_bytes_per_sample(mStreamAttributeSource.audio_format) / mConfig.period_count;
    mReadBufferSize = kReadBufferSize;
#endif
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    mCaptureDropSize = 0;

    ALOGV("%s(), mCaptureDropSize=%d, CAPTURE_DROP_MS=%d", __FUNCTION__, mCaptureDropSize, CAPTURE_DROP_MS);

    OpenPCMDump(LOG_TAG);

    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmUl2Capture);
    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmUl2Capture);

    // enable pcm
    ASSERT(mPcm == NULL);
    mPcm = pcm_open(cardindex, pcmindex, PCM_IN | PCM_MONOTONIC, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
    ALOGV("%s(), mPcm = %p", __FUNCTION__, mPcm);

    // trigger prepare ops to let codec driver have chance to update sample rate setting
    pcm_prepare(mPcm);

    return NO_ERROR;
}

uint32_t AudioALSACaptureDataProviderExternal::getLatencyTime()
{
    mlatency = UPLINK_NORMAL_LATENCY_MS ; //20ms
#ifdef UPLINK_LOW_LATENCY
    if (HasLowLatencyCapture())
    {
      mlatency = UPLINK_LOW_LATENCY_MS; //5ms
    }
#endif
    return mlatency;
}

status_t AudioALSACaptureDataProviderExternal::close()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class detach

    mEnable = false;
    AudioAutoTimeoutLock _l(mEnableLock);

    ClosePCMDump();

    if (mPcm)
    {
        pcm_stop(mPcm);
        pcm_close(mPcm);
        mPcm = NULL;
    }

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

} // end of namespace android
