#define LOG_TAG "AudioALSACaptureDataProviderNormal"

#include "AudioALSACaptureDataProviderNormal.h"
#include <pthread.h>
#include "AudioType.h"
#include "AudioALSACustomDeviceString.h"


namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/

static const uint32_t kNormalCaptureBufferSize = 0x4000;
static const uint32_t kReadBufferSize = 0x1e00;

static const uint32_t kNormalCaptureDurationMs = 20;
static const uint32_t kFastCaptureDurationMs = 5;


/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderNormal *AudioALSACaptureDataProviderNormal::mAudioALSACaptureDataProviderNormal = NULL;
AudioALSACaptureDataProviderNormal *AudioALSACaptureDataProviderNormal::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderNormal == NULL)
    {
        mAudioALSACaptureDataProviderNormal = new AudioALSACaptureDataProviderNormal();
    }
    ASSERT(mAudioALSACaptureDataProviderNormal != NULL);
    return mAudioALSACaptureDataProviderNormal;
}

AudioALSACaptureDataProviderNormal::AudioALSACaptureDataProviderNormal()
{
    ALOGV("%s()", __FUNCTION__);

    mCaptureDataProviderType = CAPTURE_PROVIDER_NORMAL;
}

AudioALSACaptureDataProviderNormal::~AudioALSACaptureDataProviderNormal()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureDataProviderNormal::open()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);
    const stream_attribute_t *targetAttr = AudioALSACaptureDataProviderBase::getStreamAttributeTarget();

    // config attribute (will used in client SRC/Enh/... later)
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = (IsAudioSupportFeature(AUDIO_SUPPORT_DMIC) &&
                                          targetAttr->input_device != AUDIO_DEVICE_IN_WIRED_HEADSET ) ? 32000 : 48000;

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    mConfig.channels = mStreamAttributeSource.num_channels;
    mConfig.rate = mStreamAttributeSource.sample_rate;
#ifdef UPLINK_LOW_LATENCY
    uint32_t captureDurationMs = getLatencyTime();
    mConfig.period_count = (captureDurationMs == UPLINK_LOW_LATENCY_MS) ? 8 : 4;
    mConfig.period_size = mStreamAttributeSource.sample_rate * captureDurationMs / 1000;
    mCaptureDropSize = 0;
    mReadBufferSize = mConfig.period_size * mConfig.period_count * audio_bytes_per_sample(mStreamAttributeSource.audio_format);
#else
    mConfig.period_count = 2;
    mConfig.period_size = (kNormalCaptureBufferSize / mConfig.channels) / audio_bytes_per_sample(mStreamAttributeSource.audio_format) / mConfig.period_count;
    mCaptureDropSize = ((mStreamAttributeSource.sample_rate * CAPTURE_DROP_MS << 2) / 1000);
    mReadBufferSize = kReadBufferSize;
#endif
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    ALOGV("%s() period_size = %d period_count = %d mCaptureDropSize = %d",
          __FUNCTION__, mConfig.period_size, mConfig.period_count, mCaptureDropSize);

    OpenPCMDump(LOG_TAG);

    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmUl1Capture);
    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmUl1Capture);

    // enable pcm
    ASSERT(mPcm == NULL);
    mPcm = pcm_open(cardindex, pcmindex, PCM_IN | PCM_MONOTONIC, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
    ALOGV("%s(), mPcm = %p", __FUNCTION__, mPcm);

    // trigger prepare ops to let codec driver have chance to update sample rate setting
    pcm_prepare(mPcm);

    return NO_ERROR;
}

uint32_t AudioALSACaptureDataProviderNormal::getLatencyTime()
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

status_t AudioALSACaptureDataProviderNormal::close()
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
