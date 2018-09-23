#define LOG_TAG "AudioALSACaptureDataProviderBTSCO"

#include "AudioALSACaptureDataProviderBTSCO.h"
#include <pthread.h>
#include "AudioType.h"
#include "WCNChipController.h"
#include "AudioALSACustomDeviceString.h"


namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/



/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderBTSCO *AudioALSACaptureDataProviderBTSCO::mAudioALSACaptureDataProviderBTSCO = NULL;
AudioALSACaptureDataProviderBTSCO *AudioALSACaptureDataProviderBTSCO::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderBTSCO == NULL)
    {
        mAudioALSACaptureDataProviderBTSCO = new AudioALSACaptureDataProviderBTSCO();
    }
    ASSERT(mAudioALSACaptureDataProviderBTSCO != NULL);
    return mAudioALSACaptureDataProviderBTSCO;
}

AudioALSACaptureDataProviderBTSCO::AudioALSACaptureDataProviderBTSCO() :
    mWCNChipController(WCNChipController::GetInstance())
{
    ALOGV("%s()", __FUNCTION__);

    mCaptureDataProviderType = CAPTURE_PROVIDER_BT_SCO;
}

AudioALSACaptureDataProviderBTSCO::~AudioALSACaptureDataProviderBTSCO()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureDataProviderBTSCO::open()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);

    // config attribute (will used in client SRC/Enh/... later)
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_MONO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = mWCNChipController->GetBTCurrentSamplingRateNumber();

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    // pcm config
    unsigned int latency = getLatencyTime();

    mConfig.channels = mStreamAttributeSource.num_channels;
    mConfig.rate = mStreamAttributeSource.sample_rate;
    mConfig.period_size = (latency * mConfig.rate) / 1000;
    mConfig.period_count = (latency == UPLINK_LOW_LATENCY_MS) ? 8 : 4;
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    mReadBufferSize = mConfig.period_size * mConfig.channels *
                      audio_bytes_per_sample(mStreamAttributeSource.audio_format);

    ALOGV("%s(), audio_format = %d, audio_channel_mask=%x, num_channels=%d, sample_rate=%d", __FUNCTION__,
          mStreamAttributeSource.audio_format, mStreamAttributeSource.audio_channel_mask, mStreamAttributeSource.num_channels, mStreamAttributeSource.sample_rate);

    ALOGV("%s(), format = %d, channels=%d, rate=%d", __FUNCTION__,
          mConfig.format, mConfig.channels, mConfig.rate);

    OpenPCMDump(LOG_TAG);

    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmBTSCO);
    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmBTSCO);

    // enable pcm
    ASSERT(mPcm == NULL);
    mPcm = pcm_open(cardindex, pcmindex, PCM_IN | PCM_MONOTONIC, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
    ALOGV("%s(), mPcm = %p", __FUNCTION__, mPcm);

    pcm_start(mPcm);

    return NO_ERROR;
}

status_t AudioALSACaptureDataProviderBTSCO::close()
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

    return NO_ERROR;
}

unsigned int AudioALSACaptureDataProviderBTSCO::getLatencyTime()
{
    unsigned int latency = UPLINK_NORMAL_LATENCY_MS;
#ifdef UPLINK_LOW_LATENCY
    if (HasLowLatencyCapture())
    {
      latency = UPLINK_LOW_LATENCY_MS;
    }
#endif
    return latency;
}

} // end of namespace android
