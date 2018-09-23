#define LOG_TAG "AudioALSAPlaybackHandlerNormal"

#include "AudioALSAPlaybackHandlerNormal.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSAVolumeController.h"
#include "AudioMTKFilter.h"
#include "AudioALSACustomDeviceString.h"
#include "V3/include/AudioALSADeviceParser.h"

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

//#define DEBUG_PLAYBACK_LATENCY
#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec ) * 1000000 + ( x.tv_nsec - y.tv_nsec ) / 1000)

namespace android
{

AudioALSAPlaybackHandlerNormal::AudioALSAPlaybackHandlerNormal(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source)
{
    ALOGV("%s()", __FUNCTION__);
    mPlaybackHandlerType = PLAYBACK_HANDLER_NORMAL;
}

AudioALSAPlaybackHandlerNormal::~AudioALSAPlaybackHandlerNormal()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSAPlaybackHandlerNormal::open()
{
    ALOGV("+%s(), mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->output_devices);

    unsigned int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keyCustomPcmDl1Meida);
    unsigned int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keyCustomPcmDl1Meida);

    ListPcmDriver(cardindex, pcmindex);

    // HW attribute config
    mStreamAttributeTarget.buffer_size = mStreamAttributeSource->buffer_size;
    mStreamAttributeTarget.audio_format = (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) ? AUDIO_FORMAT_PCM_8_24_BIT : AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeTarget.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeTarget.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeTarget.audio_channel_mask);
#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
    mStreamAttributeTarget.sample_rate = 192000;
    if (mStreamAttributeTarget.sample_rate != mStreamAttributeSource->sample_rate) {
        mStreamAttributeTarget.buffer_size *= ((float)mStreamAttributeTarget.sample_rate / (float)mStreamAttributeSource->sample_rate);
    }
#else
    mStreamAttributeTarget.sample_rate = mStreamAttributeSource->sample_rate; // same as source stream
#endif
    mStreamAttributeTarget.output_devices= mStreamAttributeSource->output_devices;

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

    // post processing
    initPostProcessing();

    // SRC
    initBliSrc();

    // bit conversion
    initBitConverter();

    initDataPending();

    // open pcm driver
    openPcmDriver(cardindex, pcmindex);

    mHardwareResourceManager->setRoutePath(ROUTE_NORMAL_PLAYBACK,
                                           mStreamAttributeTarget.output_devices);

    if (mPcm) pcm_start(mPcm);

    // open codec driver
    mHardwareResourceManager->startOutputDevice(mStreamAttributeTarget.output_devices,
                                                mStreamAttributeTarget.sample_rate);

    // debug pcm dump
    OpenPCMDump(LOG_TAG);

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerNormal::close()
{
    ALOGV("+%s()", __FUNCTION__);

    // close codec driver
    mHardwareResourceManager->stopOutputDevice();

    // close pcm driver
    closePcmDriver();

    mHardwareResourceManager->resetRoutePath(ROUTE_NORMAL_PLAYBACK, mStreamAttributeTarget.output_devices);

    DeinitDataPending();

    // bit conversion
    deinitBitConverter();

    // SRC
    deinitBliSrc();

    // post processing
    deinitPostProcessing();

    // debug pcm dump
    ClosePCMDump();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerNormal::routing(const audio_devices_t output_devices)
{
    mHardwareResourceManager->changeOutputDevice(output_devices);
    if (mAudioFilterManagerHandler) { mAudioFilterManagerHandler->setDevice(output_devices); }
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerNormal::pause()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerNormal::resume()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerNormal::flush()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerNormal::setVolume(uint32_t vol __unused)
{
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerNormal::drain(audio_drain_type_t type __unused)
{
    return 0;
}

ssize_t AudioALSAPlaybackHandlerNormal::write(const void *buffer, size_t bytes)
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

#ifdef DEBUG_PLAYBACK_LATENCY
    clock_gettime(CLOCK_MONOTONIC, &mNewtime);
    latencyTime[0] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

    // stereo to mono for speaker
    doStereoToMonoConversionIfNeed(pBuffer, bytes);

    // post processing (can handle both Q1P16 and Q1P31 by audio_format_t)
    void *pBufferAfterPostProcessing = NULL;
    uint32_t bytesAfterPostProcessing = 0;
    doPostProcessing(pBuffer, bytes, &pBufferAfterPostProcessing, &bytesAfterPostProcessing);


    // SRC
    void *pBufferAfterBliSrc = NULL;
    uint32_t bytesAfterBliSrc = 0;
    doBliSrc(pBufferAfterPostProcessing, bytesAfterPostProcessing, &pBufferAfterBliSrc, &bytesAfterBliSrc);


    // bit conversion
    void *pBufferAfterBitConvertion = NULL;
    uint32_t bytesAfterBitConvertion = 0;
    doBitConversion(pBufferAfterBliSrc, bytesAfterBliSrc, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);

    // data pending
    void *pBufferAfterPending = NULL;
    uint32_t bytesAfterpending = 0;
    dodataPending(pBufferAfterBitConvertion, bytesAfterBitConvertion, &pBufferAfterPending, &bytesAfterpending);

    // pcm dump
    WritePcmDumpData(pBufferAfterPending, bytesAfterpending);

#ifdef DEBUG_PLAYBACK_LATENCY
    clock_gettime(CLOCK_MONOTONIC, &mNewtime);
    latencyTime[1] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

    ALOGW_IF((bytesAfterpending & 63), "%s data(%u) not aligned", __FUNCTION__, bytesAfterpending);

    // write data to pcm driver
    int retval = pcm_write(mPcm, pBufferAfterPending, bytesAfterpending);

#ifdef DEBUG_PLAYBACK_LATENCY
    clock_gettime(CLOCK_MONOTONIC, &mNewtime);
    latencyTime[2] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

    if (retval != 0)
    {
        ALOGE("%s(), pcm_write() error, retval = %d", __FUNCTION__, retval);
    }

#ifdef DEBUG_PLAYBACK_LATENCY
    ALOGD("%s latency in us: %llu %llu %llu", __FUNCTION__,
            latencyTime[0], latencyTime[1], latencyTime[2]);
#endif

    return bytes;
}

status_t AudioALSAPlaybackHandlerNormal::setFilterMng(AudioMTKFilterManager *pFilterMng)
{
    ALOGV("%s() mAudioFilterManagerHandler [%p]", __FUNCTION__, pFilterMng);
    mAudioFilterManagerHandler = pFilterMng;
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerNormal::getHardwareBufferInfo(time_info_struct_t *HWBuffer_Time_Info)
{

    status_t ret = AudioALSAPlaybackHandlerBase::getHardwareBufferInfo(HWBuffer_Time_Info);

#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
    if (mStreamAttributeTarget.sample_rate != mStreamAttributeSource->sample_rate) {
        float srcRatio = (float)mStreamAttributeSource->sample_rate / (float)mStreamAttributeTarget.sample_rate;
        HWBuffer_Time_Info->buffer_per_time *= srcRatio;
        HWBuffer_Time_Info->frameInfo_get *= srcRatio;
    }
#endif

    return ret;
}

} // end of namespace android
