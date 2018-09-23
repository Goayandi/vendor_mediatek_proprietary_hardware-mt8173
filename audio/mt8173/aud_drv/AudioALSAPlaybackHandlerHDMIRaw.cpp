#define LOG_TAG "AudioALSAPlaybackHandlerHDMIRaw"

#include "AudioALSAPlaybackHandlerHDMIRaw.h"
#include "AudioALSACustomDeviceString.h"
#include "HDMITxController.h"

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif


namespace android
{

AudioALSAPlaybackHandlerHDMIRaw::AudioALSAPlaybackHandlerHDMIRaw(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source), mIEC_SPDIFEncoder(this)
{
    ALOGV("%s()", __FUNCTION__);
    mPlaybackHandlerType = PLAYBACK_HANDLER_HDMI;
}

AudioALSAPlaybackHandlerHDMIRaw::~AudioALSAPlaybackHandlerHDMIRaw()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSAPlaybackHandlerHDMIRaw::open()
{
    ALOGV("+%s(), mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->output_devices);

    mStreamAttributeTarget.buffer_size = mStreamAttributeSource->buffer_size;
    //mStreamAttributeTarget.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeTarget.audio_format = mStreamAttributeSource->audio_format;
    //mStreamAttributeTarget.audio_channel_mask = mStreamAttributeSource->audio_channel_mask;
    mStreamAttributeTarget.audio_channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    mStreamAttributeTarget.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeTarget.audio_channel_mask);
    mStreamAttributeTarget.sample_rate = mStreamAttributeSource->sample_rate; // same as source stream

    // HW pcm config
    mConfig.channels = mStreamAttributeTarget.num_channels;
    mConfig.rate = mStreamAttributeTarget.sample_rate;
    mConfig.period_count = 2;
    //mConfig.period_size = mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count * audio_bytes_per_sample(mStreamAttributeTarget.audio_format));
    mConfig.period_size = mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count * audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT));
    mConfig.format = transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    ALOGV("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d",
          __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format);

    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmHDMIRaw);
    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmHDMIRaw);

    // open pcm driver
    openPcmDriver(cardindex, pcmindex);

    if (mPcm) pcm_start(mPcm);

    HDMITxController::notifyAudioSetting(mStreamAttributeTarget.audio_format, mStreamAttributeTarget.sample_rate,
                                         mStreamAttributeTarget.audio_channel_mask);

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerHDMIRaw::close()
{
    ALOGV("+%s()", __FUNCTION__);

    // close pcm driver
    closePcmDriver();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerHDMIRaw::routing(const audio_devices_t output_devices)
{
    (void)output_devices;
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMIRaw::pause()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMIRaw::resume()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMIRaw::flush()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMIRaw::setVolume(uint32_t vol __unused)
{
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerHDMIRaw::drain(audio_drain_type_t type __unused)
{
    return 0;
}

ssize_t AudioALSAPlaybackHandlerHDMIRaw::write_to_pcm(const void *buffer, size_t bytes)
{
    ALOGVV("%s(), buffer = %p, bytes = %d", __FUNCTION__, buffer, bytes);

    if (mPcm == NULL)
    {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return bytes;
    }

    // write data to pcm driver
    int retval = pcm_write(mPcm, buffer, bytes);
    ALOGE_IF(retval != 0, "%s(), pcm_write() error, retval = %d", __FUNCTION__, retval);

    return bytes;
}

ssize_t AudioALSAPlaybackHandlerHDMIRaw::write(const void *buffer, size_t bytes)
{
#if 1 // Raw data
    return mIEC_SPDIFEncoder.write(buffer, bytes);
#else // IEC61937 format
    //return write_to_pcm(buffer, bytes);
#endif
}


} // end of namespace android
