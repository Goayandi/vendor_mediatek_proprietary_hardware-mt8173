#define LOG_TAG "AudioALSAPlaybackHandlerHDMI"

#include "AudioALSAPlaybackHandlerHDMI.h"
#include "AudioALSACustomDeviceString.h"
#include "HDMITxController.h"
#include "V3/include/AudioALSADriverUtility.h"

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif


namespace android
{

AudioALSAPlaybackHandlerHDMI::AudioALSAPlaybackHandlerHDMI(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source)
{
    ALOGV("%s()", __FUNCTION__);
    mPlaybackHandlerType = PLAYBACK_HANDLER_HDMI;
}

AudioALSAPlaybackHandlerHDMI::~AudioALSAPlaybackHandlerHDMI()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSAPlaybackHandlerHDMI::open()
{
    ALOGV("+%s(), mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->output_devices);

    mStreamAttributeTarget.buffer_size = mStreamAttributeSource->buffer_size;
    mStreamAttributeTarget.audio_format = (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                                           mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_8_24_BIT) ?
                                           AUDIO_FORMAT_PCM_8_24_BIT : AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeTarget.audio_channel_mask = mStreamAttributeSource->audio_channel_mask;
    mStreamAttributeTarget.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeTarget.audio_channel_mask);
    mStreamAttributeTarget.sample_rate = mStreamAttributeSource->sample_rate; // same as source stream

    // HW pcm config
    mConfig.channels = mStreamAttributeTarget.num_channels;
    mConfig.rate = mStreamAttributeTarget.sample_rate;
    mConfig.period_count = 2;
    mConfig.period_size = mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count * audio_bytes_per_sample(mStreamAttributeTarget.audio_format));
    mConfig.format = transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = 0;

    ALOGV("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d",
          __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format);

    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmHDMI);
    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmHDMI);

    // bit conversion
    initBitConverter();

    // open pcm driver
    openPcmDriver(cardindex, pcmindex);

    if (mPcm) pcm_start(mPcm);

    HDMITxController::notifyAudioSetting(mStreamAttributeTarget.audio_format, mStreamAttributeTarget.sample_rate,
                                         mStreamAttributeTarget.audio_channel_mask);

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerHDMI::close()
{
    ALOGV("+%s()", __FUNCTION__);

    // close pcm driver
    closePcmDriver();

    // bit conversion
    deinitBitConverter();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerHDMI::routing(const audio_devices_t output_devices)
{
    (void)output_devices;
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMI::pause()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMI::resume()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMI::flush()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerHDMI::setVolume(uint32_t vol __unused)
{
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerHDMI::drain(audio_drain_type_t type __unused)
{
    return 0;
}

ssize_t AudioALSAPlaybackHandlerHDMI::write(const void *buffer, size_t bytes)
{
    ALOGVV("%s(), buffer = %p, bytes = %d", __FUNCTION__, buffer, bytes);

    if (mPcm == NULL)
    {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return bytes;
    }

    void *pBufferAfterBitConvertion = NULL;
    uint32_t bytesAfterBitConvertion = 0;
    doBitConversion((void *)buffer, bytes, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);

    // write data to pcm driver
    int retval = pcm_write(mPcm, pBufferAfterBitConvertion, bytesAfterBitConvertion);
    ALOGE_IF(retval != 0, "%s(), pcm_write() error, retval = %d", __FUNCTION__, retval);

    return bytes;
}

void AudioALSAPlaybackHandlerHDMI::forceClkOn(const stream_attribute_t *stream_attribute_source)
{
    ALOGV("+%s()", __FUNCTION__);

    struct mixer *mixerInstance = AudioALSADriverUtility::getInstance()->getMixer();
    mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mixerInstance, "HDMI_Force_Clk_Switch"), "On");

    // trigger clock output in advance
    AudioALSAPlaybackHandlerHDMI *handler = new AudioALSAPlaybackHandlerHDMI(stream_attribute_source);
    handler->open();
    handler->close();
    delete handler;

    ALOGV("-%s()", __FUNCTION__);
}

void AudioALSAPlaybackHandlerHDMI::forceClkOff()
{
    ALOGV("+%s()", __FUNCTION__);

    struct mixer *mixerInstance = AudioALSADriverUtility::getInstance()->getMixer();
    mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mixerInstance, "HDMI_Force_Clk_Switch"), "Off");

    ALOGV("-%s()", __FUNCTION__);
}

} // end of namespace android
