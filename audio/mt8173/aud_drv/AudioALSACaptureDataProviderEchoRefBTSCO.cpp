#define LOG_TAG "AudioALSACaptureDataProviderEchoRefBTSCO"

#include "AudioALSACaptureDataProviderEchoRefBTSCO.h"
#include <pthread.h>
#include "AudioType.h"
#include "WCNChipController.h"
#include "AudioALSACustomDeviceString.h"


namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/

static const uint32_t kReadBufferTimeMs = 20;



/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderEchoRefBTSCO *AudioALSACaptureDataProviderEchoRefBTSCO::mAudioALSACaptureDataProviderEchoRefBTSCO = NULL;
AudioALSACaptureDataProviderEchoRefBTSCO *AudioALSACaptureDataProviderEchoRefBTSCO::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderEchoRefBTSCO == NULL)
    {
        mAudioALSACaptureDataProviderEchoRefBTSCO = new AudioALSACaptureDataProviderEchoRefBTSCO();
    }
    ASSERT(mAudioALSACaptureDataProviderEchoRefBTSCO != NULL);
    return mAudioALSACaptureDataProviderEchoRefBTSCO;
}

AudioALSACaptureDataProviderEchoRefBTSCO::AudioALSACaptureDataProviderEchoRefBTSCO() :
    mWCNChipController(WCNChipController::GetInstance())
{
    ALOGV("%s()", __FUNCTION__);
    mCaptureDataProviderType = CAPTURE_PROVIDER_ECHOREF_BTSCO;
}

AudioALSACaptureDataProviderEchoRefBTSCO::~AudioALSACaptureDataProviderEchoRefBTSCO()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureDataProviderEchoRefBTSCO::open()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);

    // config attribute (will used in client SRC/Enh/... later) // TODO(Sam): query the mConfig?
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = mWCNChipController->GetBTCurrentSamplingRateNumber();

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    mConfig.channels = mStreamAttributeSource.num_channels;
    mConfig.rate = mStreamAttributeSource.sample_rate;
    // Buffer size: 2048(period_size) * 2(ch) * 2(byte) * 8(period_count) = 64 kb
    mConfig.period_size = 2048;
    mConfig.period_count = 8;
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    mReadBufferSize = (mStreamAttributeSource.sample_rate * mStreamAttributeSource.num_channels *
                       audio_bytes_per_sample(mStreamAttributeSource.audio_format) / 1000) * kReadBufferTimeMs;
    mReadBufferSize &= ~63;

    //latency time, set as hardware buffer size
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

status_t AudioALSACaptureDataProviderEchoRefBTSCO::close()
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

} // end of namespace android
