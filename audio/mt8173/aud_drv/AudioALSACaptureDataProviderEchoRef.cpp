#define LOG_TAG "AudioALSACaptureDataProviderEchoRef"

#include "AudioALSACaptureDataProviderEchoRef.h"
#include <pthread.h>
#include "AudioType.h"
#include "V3/include/AudioALSASampleRateController.h"
#include "AudioALSACustomDeviceString.h"


namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/




/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderEchoRef *AudioALSACaptureDataProviderEchoRef::mAudioALSACaptureDataProviderEchoRef = NULL;
AudioALSACaptureDataProviderEchoRef *AudioALSACaptureDataProviderEchoRef::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderEchoRef == NULL)
    {
        mAudioALSACaptureDataProviderEchoRef = new AudioALSACaptureDataProviderEchoRef();
    }
    ASSERT(mAudioALSACaptureDataProviderEchoRef != NULL);
    return mAudioALSACaptureDataProviderEchoRef;
}

AudioALSACaptureDataProviderEchoRef::AudioALSACaptureDataProviderEchoRef()
{
    ALOGV("%s()", __FUNCTION__);
    mCaptureDataProviderType = CAPTURE_PROVIDER_ECHOREF;
}

AudioALSACaptureDataProviderEchoRef::~AudioALSACaptureDataProviderEchoRef()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureDataProviderEchoRef::open()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);

    AudioALSASampleRateController *pAudioALSASampleRateController = AudioALSASampleRateController::getInstance();
    pAudioALSASampleRateController->setScenarioStatus(PLAYBACK_SCENARIO_ECHO_REF);

    // config attribute (will used in client SRC/Enh/... later)
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
    mStreamAttributeSource.sample_rate = 192000;
#else
    mStreamAttributeSource.sample_rate = pAudioALSASampleRateController->getPrimaryStreamOutSampleRate();
#endif

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    uint32_t latencyMs = getLatencyTime();

    mConfig.channels = mStreamAttributeSource.num_channels ;
    mConfig.rate = mStreamAttributeSource.sample_rate;
    mConfig.period_size = latencyMs * mConfig.rate / 1000;
    mConfig.period_count = 8;
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    mReadBufferSize = (mStreamAttributeSource.sample_rate * mStreamAttributeSource.num_channels *
                       audio_bytes_per_sample(mStreamAttributeSource.audio_format) / 1000) * latencyMs;
    mReadBufferSize &= ~63;

    // latency time, set as hardware buffer size
    mStreamAttributeSource.latency = (mConfig.period_size * mConfig.period_count * 1000) / mConfig.rate;

    ALOGV("%s() audio_format = %d audio_channel_mask = %x num_channels = %d sample_rate = %d latency = %d ms",
          __FUNCTION__, mStreamAttributeSource.audio_format, mStreamAttributeSource.audio_channel_mask,
          mStreamAttributeSource.num_channels, mStreamAttributeSource.sample_rate, mStreamAttributeSource.latency);

    ALOGV("%s() format = %d channels=%d rate=%d", __FUNCTION__,
          mConfig.format, mConfig.channels, mConfig.rate);

    OpenPCMDump(LOG_TAG);

    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmDl1AwbCapture);
    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmDl1AwbCapture);

    // enable pcm
    ASSERT(mPcm == NULL);
    mPcm = pcm_open(cardindex, pcmindex, PCM_IN | PCM_MONOTONIC, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);

    pcm_start(mPcm);

    return NO_ERROR;
}

uint32_t AudioALSACaptureDataProviderEchoRef::getLatencyTime()
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

status_t AudioALSACaptureDataProviderEchoRef::close()
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

    AudioALSASampleRateController *pAudioALSASampleRateController = AudioALSASampleRateController::getInstance();
    pAudioALSASampleRateController->resetScenarioStatus(PLAYBACK_SCENARIO_ECHO_REF);

    return NO_ERROR;
}

} // end of namespace android
