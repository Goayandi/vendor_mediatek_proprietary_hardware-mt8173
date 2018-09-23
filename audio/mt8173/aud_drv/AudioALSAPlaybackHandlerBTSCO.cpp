#define LOG_TAG "AudioALSAPlaybackHandlerBTSCO"
//#define LOG_NDEBUG 0

#include "AudioALSAPlaybackHandlerBTSCO.h"
#include "WCNChipController.h"
#include "AudioALSACustomDeviceString.h"
#include "V3/include/AudioALSASampleRateController.h"

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

namespace android
{

AudioALSAPlaybackHandlerBTSCO::AudioALSAPlaybackHandlerBTSCO(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source),
    mWCNChipController(WCNChipController::GetInstance())
{
    ALOGV("%s()", __FUNCTION__);
    mPlaybackHandlerType = PLAYBACK_HANDLER_BT_SCO;
}

AudioALSAPlaybackHandlerBTSCO::~AudioALSAPlaybackHandlerBTSCO()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSAPlaybackHandlerBTSCO::open()
{
    ALOGV("+%s(), mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->output_devices);


    // HW attribute config
    mStreamAttributeTarget.audio_format = (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) ?
                                           AUDIO_FORMAT_PCM_8_24_BIT : AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeTarget.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeTarget.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeTarget.audio_channel_mask);
    mStreamAttributeTarget.sample_rate = mWCNChipController->GetBTCurrentSamplingRateNumber();
    mStreamAttributeTarget.buffer_size = mStreamAttributeSource->buffer_size * mStreamAttributeTarget.sample_rate /
                                         AudioALSASampleRateController::getInstance()->getPrimaryStreamOutSampleRate();
    mStreamAttributeTarget.buffer_size = ((mStreamAttributeTarget.buffer_size + 63) >> 6) << 6;

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

    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmBTSCO);
    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmBTSCO);

    // SRC
    initBliSrc();

    // bit conversion
    initBitConverter();

    initDataPending();

    // open pcm driver
    openPcmDriver(cardindex, pcmindex);

    if (mPcm) pcm_start(mPcm);

    // debug pcm dump
    OpenPCMDump(LOG_TAG);

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBTSCO::close()
{
    ALOGV("+%s()", __FUNCTION__);

    // close pcm driver
    closePcmDriver();

    DeinitDataPending();

    // bit conversion
    deinitBitConverter();

    // SRC
    deinitBliSrc();

    // debug pcm dump
    ClosePCMDump();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBTSCO::routing(const audio_devices_t output_devices __unused)
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBTSCO::pause()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBTSCO::resume()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBTSCO::flush()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBTSCO::setVolume(uint32_t vol __unused)
{
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerBTSCO::drain(audio_drain_type_t type __unused)
{
    return 0;
}

ssize_t AudioALSAPlaybackHandlerBTSCO::write(const void *buffer, size_t bytes)
{
    ALOGVV("%s(), buffer = %p, bytes = %d", __FUNCTION__, buffer, bytes);

    if (mPcm == NULL)
    {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return bytes;
    }

    // const -> to non const
    void *pBuffer = const_cast<void *>(buffer);
    ASSERT(pBuffer != NULL);


    // SRC
    void *pBufferAfterBliSrc = NULL;
    uint32_t bytesAfterBliSrc = 0;
    doBliSrc(pBuffer, bytes, &pBufferAfterBliSrc, &bytesAfterBliSrc);


    // bit conversion
    void *pBufferAfterBitConvertion = NULL;
    uint32_t bytesAfterBitConvertion = 0;
    doBitConversion(pBufferAfterBliSrc, bytesAfterBliSrc, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);


     // data pending
    void *pBufferAfterPending = NULL;
    uint32_t bytesAfterpending = 0;
    dodataPending(pBufferAfterBitConvertion, bytesAfterBitConvertion, &pBufferAfterPending, &bytesAfterpending);

    // write data to pcm driver
    WritePcmDumpData(pBufferAfterPending, bytesAfterpending);
    ALOGW_IF((bytesAfterpending & 63), "%s data(%u) not aligned", __FUNCTION__, bytesAfterpending);
    int retval = pcm_write(mPcm, pBufferAfterPending, bytesAfterpending);

    if (retval != 0)
    {
        ALOGE("%s(), pcm_write() error, retval = %d", __FUNCTION__, retval);
    }

    return bytes;
}


} // end of namespace android
