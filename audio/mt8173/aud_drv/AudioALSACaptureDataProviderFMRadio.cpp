#define LOG_TAG "AudioALSACaptureDataProviderFMRadio"

#include "AudioALSACaptureDataProviderFMRadio.h"
#include <pthread.h>
#include "AudioType.h"
#include "V3/include/AudioALSAFMController.h"


namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/

static const uint32_t kReadBufferSize = 0x2000; // 8k


/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderFMRadio *AudioALSACaptureDataProviderFMRadio::mAudioALSACaptureDataProviderFMRadio = NULL;
AudioALSACaptureDataProviderFMRadio *AudioALSACaptureDataProviderFMRadio::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderFMRadio == NULL)
    {
        mAudioALSACaptureDataProviderFMRadio = new AudioALSACaptureDataProviderFMRadio();
    }
    ASSERT(mAudioALSACaptureDataProviderFMRadio != NULL);
    return mAudioALSACaptureDataProviderFMRadio;
}

AudioALSACaptureDataProviderFMRadio::AudioALSACaptureDataProviderFMRadio()
{
    ALOGV("%s()", __FUNCTION__);

    mCaptureDataProviderType = CAPTURE_PROVIDER_FM_RADIO;
}

AudioALSACaptureDataProviderFMRadio::~AudioALSACaptureDataProviderFMRadio()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureDataProviderFMRadio::open()
{
    ALOGV("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);

    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = AudioALSAFMController::getInstance()->getFmUplinkSamplingRate();

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    mConfig.channels = mStreamAttributeSource.num_channels;
    mConfig.rate = mStreamAttributeSource.sample_rate;
    mConfig.period_size = 2048;
    mConfig.period_count = 8;
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    mCaptureDropSize = 0;
    mReadBufferSize = kReadBufferSize;

    OpenPCMDump(LOG_TAG);

    unsigned int card_index = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmMRGrxCapture);
    unsigned int pcm_index = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmMRGrxCapture);

    // enable pcm
    ASSERT(mPcm == NULL);
    mPcm = pcm_open(card_index, pcm_index, PCM_IN | PCM_MONOTONIC, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
    ALOGV("%s(), mPcm = %p", __FUNCTION__, mPcm);

    pcm_start(mPcm);

    return NO_ERROR;
}

status_t AudioALSACaptureDataProviderFMRadio::close()
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
