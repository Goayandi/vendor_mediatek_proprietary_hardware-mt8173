#define LOG_TAG  "AudioALSAStreamOut"
//#define LOG_NDEBUG 0

#include "AudioALSAStreamOut.h"
#include "AudioALSAStreamManager.h"
#include "AudioALSAPlaybackHandlerBase.h"
#include "AudioUtility.h"
#include "V3/include/AudioALSASampleRateController.h"
#include "HDMITxController.h"
#include <inttypes.h>

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define CASE_LOGVV(x, ...) ALOGVV("[%s] " x, USE_CASE_TO_STRING(mUseCase), ##__VA_ARGS__)
#define CASE_LOGV(x, ...)  ALOGV("[%s] " x, USE_CASE_TO_STRING(mUseCase), ##__VA_ARGS__)
#define CASE_LOGD(x, ...)  ALOGD("[%s] " x, USE_CASE_TO_STRING(mUseCase), ##__VA_ARGS__)
#define CASE_LOGI(x, ...)  ALOGI("[%s] " x, USE_CASE_TO_STRING(mUseCase), ##__VA_ARGS__)
#define CASE_LOGW(x, ...)  ALOGW("[%s] " x, USE_CASE_TO_STRING(mUseCase), ##__VA_ARGS__)
#define CASE_LOGE(x, ...)  ALOGE("[%s] " x, USE_CASE_TO_STRING(mUseCase), ##__VA_ARGS__)


#define ENUM_TO_STRING(enum) #enum
#define USE_CASE_TO_STRING(case) ((case == OUT_NORMAL) ? "normal" : \
                                  (case == OUT_LOW_LATENCY) ? "low_latency" : \
                                  (case == OUT_HIRES_AUDIO) ? "hi_res" : \
                                  (case == OUT_HDMI_STEREO_MIXED) ? "hdmi_stereo" : \
                                  (case == OUT_HDMI_DIRECT_OUT) ? "hdmi_direct" : \
                                  (case == OUT_HDMI_RAW_DIRECT_OUT) ? "hdmi_raw_direct" : "")

namespace android
{

static const audio_format_t kDefaultOutputSourceFormat = AUDIO_FORMAT_PCM_16_BIT;
static const audio_channel_mask_t kDefaultOutputSourceChannelMask = AUDIO_CHANNEL_OUT_STEREO;
static const uint32_t kDefaultOutputSourceSampleRate = 44100;
static const uint32_t kDefaultOutputBufferFrameCOunt[AudioALSAStreamOut::OUT_USE_CASE_COUNT] = {
    [AudioALSAStreamOut::OUT_NORMAL] = 4096,
    [AudioALSAStreamOut::OUT_HDMI_STEREO_MIXED] = 4096,
    [AudioALSAStreamOut::OUT_HDMI_DIRECT_OUT] = 4096,
    [AudioALSAStreamOut::OUT_HDMI_RAW_DIRECT_OUT] = 4096,
    [AudioALSAStreamOut::OUT_LOW_LATENCY] = 480,
    [AudioALSAStreamOut::OUT_HIRES_AUDIO] = 16384,
};

uint32_t AudioALSAStreamOut::mSuspendCount = 0;

AudioALSAStreamOut::AudioALSAStreamOut() :
    mStreamManager(AudioALSAStreamManager::getInstance()),
    mPlaybackHandler(NULL),
    mPCMDumpFile(NULL),
    mDumpFileNum(0),
    mIdentity(0xFFFFFFFF),
    mStandby(true),
    mInternalSuspend(false),
    mPresentedBytes(0),
    mRenderedBytes(0),
    mWriteCount(0),
    mSelfRouting(false)
{
    memset(&mStreamAttributeSource, 0, sizeof(mStreamAttributeSource));
}

AudioALSAStreamOut::~AudioALSAStreamOut()
{
    ASSERT(mStandby == true && mPlaybackHandler == NULL);
}

status_t AudioALSAStreamOut::set(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    uint32_t flags)
{
    AudioAutoTimeoutLock _l(mLock);

    *status = NO_ERROR;

#if !defined(DOWNLINK_LOW_LATENCY) || defined(MTK_HIRES_192K_AUDIO_SUPPORT)
    flags &= ~AUDIO_OUTPUT_FLAG_FAST;
#endif

    if (devices == AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        if ((flags & AUDIO_OUTPUT_FLAG_DIRECT) &&
           ((*format == 0) || (!audio_is_linear_pcm((audio_format_t) *format))))
            mUseCase = OUT_HDMI_RAW_DIRECT_OUT;
        else
            mUseCase = (flags & AUDIO_OUTPUT_FLAG_DIRECT) ? OUT_HDMI_DIRECT_OUT : OUT_HDMI_STEREO_MIXED;
    } else if (flags & AUDIO_OUTPUT_FLAG_FAST) {
        mUseCase = OUT_LOW_LATENCY;
#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
    } else if (!(flags & AUDIO_OUTPUT_FLAG_PRIMARY) && (*sampleRate == 192000) && (devices &
               (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE))) {
        mUseCase = OUT_HIRES_AUDIO;
#endif
    } else {
        mUseCase = OUT_NORMAL;
    }

    CASE_LOGV("%s(), devices = 0x%x, format = 0x%x, channels = 0x%x, sampleRate = %d, flags = 0x%x",
              __FUNCTION__, devices, *format, *channels, *sampleRate, flags);

    if (mStreamManager->isStreamOutExist(mUseCase)) {
        CASE_LOGW("%s() stream out(%u) existed already", __FUNCTION__, mUseCase);
        *status = BAD_VALUE;
        return *status;
    }

    // handle default format
    if (*format == 0) *format = (mUseCase == OUT_HDMI_RAW_DIRECT_OUT) ? AUDIO_FORMAT_AC3 : AUDIO_FORMAT_PCM_16_BIT;
    if (*sampleRate == 0) *sampleRate = kDefaultOutputSourceSampleRate;
    if (*channels == 0) *channels = (mUseCase == OUT_HDMI_DIRECT_OUT) ? AUDIO_CHANNEL_OUT_5POINT1 : AUDIO_CHANNEL_OUT_STEREO;

    // check format
    if ((*format != AUDIO_FORMAT_PCM_16_BIT && *format != AUDIO_FORMAT_PCM_8_24_BIT &&
         *format != AUDIO_FORMAT_PCM_32_BIT) && (mUseCase != OUT_HDMI_RAW_DIRECT_OUT)) {
        CASE_LOGE("%s(), wrong format 0x%x, use 0x%x instead.", __FUNCTION__, *format, kDefaultOutputSourceFormat);
        *format = kDefaultOutputSourceFormat;
        *status = BAD_VALUE;
        return *status;
    }

    // check format, sample rate & channel count
    if (mUseCase == OUT_HDMI_DIRECT_OUT ||
        mUseCase == OUT_HDMI_STEREO_MIXED ||
        mUseCase == OUT_HDMI_RAW_DIRECT_OUT) {
        if (!mStreamManager->isHdmiStreamOutExist()) {
            HDMITxController::queryEdidInfo();
        }
        if (!HDMITxController::isSinkSupportedFormat(*format, *sampleRate, android_audio_legacy::AudioSystem::popCount(*channels))) {
            CASE_LOGW("%s() unsupported format 0x%x by sink", __FUNCTION__, *format);
            *status = BAD_VALUE;
            return *status;
        }
    } else {
        if (*format != AUDIO_FORMAT_PCM_16_BIT && *format != AUDIO_FORMAT_PCM_32_BIT) {
            CASE_LOGE("%s(), wrong format 0x%x, use 0x%x instead.", __FUNCTION__, *format, kDefaultOutputSourceFormat);
            *format = kDefaultOutputSourceFormat;
            *status = BAD_VALUE;
            return *status;
        }

        if (*channels != kDefaultOutputSourceChannelMask) {
            CASE_LOGE("%s(), wrong channels 0x%x, use 0x%x instead.", __FUNCTION__, *channels, kDefaultOutputSourceChannelMask);
            *channels = kDefaultOutputSourceChannelMask;
            *status = BAD_VALUE;
            return *status;
        }
        if (*sampleRate != 8000  && *sampleRate != 11025 && *sampleRate != 12000 &&
            *sampleRate != 16000 && *sampleRate != 22050 && *sampleRate != 24000 &&
            *sampleRate != 32000 && *sampleRate != 44100 && *sampleRate != 48000
#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
            && *sampleRate != 192000
#endif
            ) {
            CASE_LOGE("%s(), wrong sampleRate %d, use %d instead.", __FUNCTION__, *sampleRate, kDefaultOutputSourceSampleRate);
            *sampleRate = kDefaultOutputSourceSampleRate;
            *status = BAD_VALUE;
            return *status;
        }
    }

    mStreamAttributeSource.output_devices = static_cast<audio_devices_t>(devices);
    mStreamAttributeSource.mAudioOutputFlags = static_cast<audio_output_flags_t>(flags);
    mStreamAttributeSource.audio_format = static_cast<audio_format_t>(*format);
    mStreamAttributeSource.audio_channel_mask = *channels;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(*channels);
    mStreamAttributeSource.sample_rate = *sampleRate;
    uint32_t bufferFrameCount = kDefaultOutputBufferFrameCOunt[mUseCase];

    if (mStreamAttributeSource.audio_format == AUDIO_FORMAT_E_AC3) {
        bufferFrameCount *= 4;
    } else if (mUseCase == OUT_HDMI_STEREO_MIXED || mUseCase == OUT_HDMI_DIRECT_OUT) {
        if (mStreamAttributeSource.sample_rate == 88200 || mStreamAttributeSource.sample_rate == 96000)
            bufferFrameCount *= 2;
        else if (mStreamAttributeSource.sample_rate == 176400 || mStreamAttributeSource.sample_rate == 192000)
            bufferFrameCount *= 4;
    }

    if (mStreamAttributeSource.audio_format == AUDIO_FORMAT_E_AC3)
        mStreamAttributeSource.sample_rate *= 4;

    mStreamAttributeSource.buffer_size = bufferFrameCount * frameSize();
    mStreamAttributeSource.latency = (bufferFrameCount * 1000) / mStreamAttributeSource.sample_rate;

    if (mUseCase == OUT_LOW_LATENCY || mUseCase == OUT_HIRES_AUDIO) {
        mBufferSizeToFramework = mStreamAttributeSource.buffer_size >> 1;
    } else if (audio_is_linear_pcm(mStreamAttributeSource.audio_format)) {
        mBufferSizeToFramework = mStreamAttributeSource.buffer_size >> 2;
    } else {
        if (mStreamAttributeSource.audio_format == AUDIO_FORMAT_E_AC3)
            mBufferSizeToFramework = mStreamAttributeSource.buffer_size >> 2;
        else
            mBufferSizeToFramework = mStreamAttributeSource.buffer_size;
    }

    if (mUseCase == OUT_HDMI_DIRECT_OUT)
    {
        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_PCM_16_BIT, mStreamAttributeSource.sample_rate, 6))
        {
            mSupportedChannelMasks.add(String8(ENUM_TO_STRING(AUDIO_CHANNEL_OUT_5POINT1)));
        }
        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_PCM_16_BIT, mStreamAttributeSource.sample_rate, 8))
        {
            mSupportedChannelMasks.add(String8(ENUM_TO_STRING(AUDIO_CHANNEL_OUT_7POINT1)));
        }
    }
    else if (mUseCase == OUT_HDMI_STEREO_MIXED)
    {
        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_PCM_16_BIT, mStreamAttributeSource.sample_rate, 2))
        {
            mSupportedChannelMasks.add(String8(ENUM_TO_STRING(AUDIO_CHANNEL_OUT_STEREO)));
        }
    }
    else if (mUseCase == OUT_HDMI_RAW_DIRECT_OUT)
    {
        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_PCM_16_BIT, mStreamAttributeSource.sample_rate, 6))
        {
            mSupportedChannelMasks.add(String8(ENUM_TO_STRING(AUDIO_CHANNEL_OUT_5POINT1)));
        }
        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_PCM_16_BIT, mStreamAttributeSource.sample_rate, 8))
        {
            mSupportedChannelMasks.add(String8(ENUM_TO_STRING(AUDIO_CHANNEL_OUT_7POINT1)));
        }

        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_AC3, *sampleRate, android_audio_legacy::AudioSystem::popCount(*channels)))
        {
            mSupportedFormats.add(String8(ENUM_TO_STRING(AUDIO_FORMAT_AC3)));
        }
        if (HDMITxController::isSinkSupportedFormat(AUDIO_FORMAT_E_AC3, *sampleRate, android_audio_legacy::AudioSystem::popCount(*channels)))
        {
            mSupportedFormats.add(String8(ENUM_TO_STRING(AUDIO_FORMAT_E_AC3)));
        }
    }
    else if (mUseCase == OUT_NORMAL)
    {
        AudioALSASampleRateController::getInstance()->setPrimaryStreamOutSampleRate(*sampleRate);
    }

    return *status;
}

uint32_t AudioALSAStreamOut::sampleRate() const
{
    CASE_LOGV("%s() return %d", __FUNCTION__, mStreamAttributeSource.sample_rate);
    return mStreamAttributeSource.sample_rate;
}

size_t AudioALSAStreamOut::bufferSize() const
{
    CASE_LOGV("%s() return %zu", __FUNCTION__, mBufferSizeToFramework);
    return mBufferSizeToFramework;
}

uint32_t AudioALSAStreamOut::channels() const
{
    CASE_LOGVV("%s() return 0x%x", __FUNCTION__, mStreamAttributeSource.audio_channel_mask);
    return mStreamAttributeSource.audio_channel_mask;
}

int AudioALSAStreamOut::format() const
{
    CASE_LOGVV("%s() return 0x%x", __FUNCTION__, mStreamAttributeSource.audio_format);
    return mStreamAttributeSource.audio_format;
}

uint32_t AudioALSAStreamOut::latency() const
{
    CASE_LOGVV("%s() return %d", __FUNCTION__, mStreamAttributeSource.latency);
    return mStreamAttributeSource.latency;
}

status_t AudioALSAStreamOut::setVolume(float left, float right)
{
    (void)left;
    (void)right;
    return INVALID_OPERATION;
}

ssize_t AudioALSAStreamOut::write(const void *buffer, size_t bytes)
{
    CASE_LOGVV("%s(), buffer = %p, bytes = %d", __FUNCTION__, buffer, bytes);

    size_t outputSize = 0;
    if (mSuspendCount > 0)
    {
        CASE_LOGV("%s(), mSuspendCount = %u", __FUNCTION__, mSuspendCount);
        usleep(dataToDurationUs(bytes));
        mPresentedBytes += bytes;
        mRenderedBytes += bytes;
        mWriteCount++;
        return bytes;
    }

    mStreamManager->enableStreamLock();
    AudioAutoTimeoutLock _l(mLock);

    status_t status = NO_ERROR;

    /// check open
    if (mStandby == true)
    {
        status = open();
        if (mPlaybackHandler)
            mPlaybackHandler->setFirstDataWriteFlag(true);
    }
    else
    {
        if (mPlaybackHandler)
            mPlaybackHandler->setFirstDataWriteFlag(false);
    }

    mStreamManager->disableStreamLock();

    WritePcmDumpData(buffer, bytes);

    if (!mInternalSuspend)
    {
        /// write pcm data
        ASSERT(mPlaybackHandler != NULL);
        if (buffer && bytes > 0)
            outputSize = mPlaybackHandler->write(buffer, bytes);
    }
    else
    {
        CASE_LOGV("%s() internal suspend", __FUNCTION__);
        usleep(dataToDurationUs(bytes));
        outputSize = bytes;
    }

    mPresentedBytes += outputSize;
    mRenderedBytes += outputSize;
    mWriteCount++;

    //CASE_LOGV("%s(), outputSize = %d, bytes = %d,mPresentedBytes=%d", __FUNCTION__, outputSize, bytes, mPresentedBytes);
    return outputSize;
}

status_t AudioALSAStreamOut::standby()
{
    CASE_LOGV("+%s()", __FUNCTION__);
    mStreamManager->enableStreamLock();
    mLock.lock();

    status_t status = NO_ERROR;

    /// check close
    if (mStandby == false)
    {
        status = close();
        mRenderedBytes = 0;
    }

    mLock.unlock();
    mStreamManager->disableStreamLock();
    CASE_LOGV("-%s()", __FUNCTION__);
    return status;
}

status_t AudioALSAStreamOut::dump(int fd, const Vector<String16> &args)
{
    (void)fd;
    (void)args;
    return NO_ERROR;
}

status_t AudioALSAStreamOut::setParameters(const String8 &keyValuePairs)
{
    CASE_LOGV("+%s(): %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);

    /// keys
    const String8 keyRouting = String8(AudioParameter::keyRouting);

    /// parse key value pairs
    status_t status = NO_ERROR;
    int value = 0;

    /// routing
    if (param.getInt(keyRouting, value) == NO_ERROR)
    {
        param.remove(keyRouting);

        if (mUseCase == OUT_NORMAL || mUseCase == OUT_LOW_LATENCY || mUseCase == OUT_HIRES_AUDIO) {
            mStreamManager->enableStreamLock();
            mLock.lock();
            mSelfRouting = true;
            status = mStreamManager->routingOutputDevice(this, mStreamAttributeSource.output_devices, static_cast<audio_devices_t>(value));
            mSelfRouting = false;
            mLock.unlock();
            mStreamManager->disableStreamLock();
        }
    }

    if (param.size())
    {
        CASE_LOGV("%s() still have param.size() = %zu remain param = \"%s\"",
              __FUNCTION__, param.size(), param.toString().string());
    }

    CASE_LOGV("-%s(): %s ", __FUNCTION__, keyValuePairs.string());
    return status;
}

String8 AudioALSAStreamOut::getParameters(const String8 &keys)
{
    CASE_LOGV("+%s() %s", __FUNCTION__, keys.string());

    AudioParameter param = AudioParameter(keys);
    String8 value;

    if (mUseCase == OUT_HDMI_STEREO_MIXED ||
        mUseCase == OUT_HDMI_DIRECT_OUT ||
        mUseCase == OUT_HDMI_RAW_DIRECT_OUT)
    {
        if (param.get(String8(AUDIO_PARAMETER_STREAM_SUP_CHANNELS), value) == NO_ERROR)
        {
            String8 support_channel;
            for (size_t i = 0; i < mSupportedChannelMasks.size(); i++)
            {
                support_channel.append(mSupportedChannelMasks[i]);
                if (i != (mSupportedChannelMasks.size() - 1))
                {
                    support_channel.append("|");
                }
            }
            param.add(String8(AUDIO_PARAMETER_STREAM_SUP_CHANNELS), support_channel);
        }
    }

    if (mUseCase == OUT_HDMI_RAW_DIRECT_OUT)
    {
        // formats
        if (param.get(String8(AUDIO_PARAMETER_STREAM_SUP_FORMATS), value) == NO_ERROR)
        {
            String8 support_format;
            for (size_t i = 0; i < mSupportedFormats.size(); i++)
            {
                support_format.append(mSupportedFormats[i]);
                if (i != (mSupportedFormats.size() - 1))
                {
                    support_format.append("|");
                }
            }
            param.add(String8(AUDIO_PARAMETER_STREAM_SUP_FORMATS), support_format);
        }
    }

    CASE_LOGV("-%s() %s", __FUNCTION__, param.toString().string());
    return param.toString();
}

status_t AudioALSAStreamOut::getRenderPosition(uint32_t *dspFrames)
{
    AudioAutoTimeoutLock _l(mLock);
    status_t ret = INVALID_OPERATION;

    CASE_LOGV("%s()", __FUNCTION__);

    if (dspFrames == NULL)
        return ret;
    else
        *dspFrames = 0;

    if (mUseCase == OUT_HDMI_RAW_DIRECT_OUT)
    {
        if (mPlaybackHandler != NULL)
        {
            time_info_struct_t HW_Buf_Time_Info;
            memset(&HW_Buf_Time_Info, 0, sizeof(HW_Buf_Time_Info));
            //query remaining hardware buffer size
            if (NO_ERROR == mPlaybackHandler->getHardwareBufferInfo(&HW_Buf_Time_Info))
            {
                uint64_t RenderedFrames = mRenderedBytes / (uint64_t)(sizeof(int16_t) * 2);
                uint64_t frameInfo_get = 0;
                uint64_t retFrames = 0;

                if (mStreamAttributeSource.audio_format == AUDIO_FORMAT_E_AC3)
                    frameInfo_get = HW_Buf_Time_Info.frameInfo_get/16;
                else
                    frameInfo_get = HW_Buf_Time_Info.frameInfo_get;

                retFrames =  RenderedFrames - (uint64_t)(HW_Buf_Time_Info.buffer_per_time - frameInfo_get);

                *dspFrames = retFrames;

                ret = NO_ERROR;
                CASE_LOGVV("%s() buffer_per_time = %d frameInfo_get = %d RenderedFrames = %llu retFrames = %llu",
                           __FUNCTION__, HW_Buf_Time_Info.buffer_per_time, frameInfo_get, RenderedFrames, retFrames);
            }
            else
            {
                if (mWriteCount > 4)
                    CASE_LOGW("%s() getHardwareBufferInfo[%" PRIu64 "] fail", __FUNCTION__, mWriteCount);
                else
                    CASE_LOGV("%s() getHardwareBufferInfo[%" PRIu64 "] initial fail", __FUNCTION__, mWriteCount);
            }
        }
        else
        {
            CASE_LOGW("%s() no playback handler!?", __FUNCTION__);
        }
        return ret;
    }
    else
    {
        // not supported
        return INVALID_OPERATION;
    }
}

status_t AudioALSAStreamOut::getPresentationPosition(uint64_t *frames, struct timespec *timestamp)
{
    AudioAutoTimeoutLock _l(mLock);
    status_t ret = INVALID_OPERATION;

    if (mPlaybackHandler != NULL)
    {
        time_info_struct_t HW_Buf_Time_Info;
        memset(&HW_Buf_Time_Info, 0, sizeof(HW_Buf_Time_Info));
        //query remaining hardware buffer size
        if (NO_ERROR == mPlaybackHandler->getHardwareBufferInfo(&HW_Buf_Time_Info))
        {
            uint64_t PresentedFrames = mPresentedBytes / (uint64_t)frameSize();
            uint64_t retFrames =  PresentedFrames - (uint64_t)(HW_Buf_Time_Info.buffer_per_time - HW_Buf_Time_Info.frameInfo_get);
            *frames = retFrames;
            *timestamp = HW_Buf_Time_Info.timestamp_get;
            ret = NO_ERROR;
            CASE_LOGVV("%s() buffer_per_time = %d frameInfo_get = %d PresentedFrames = %llu retFrames = %llu",
                       __FUNCTION__, HW_Buf_Time_Info.buffer_per_time, HW_Buf_Time_Info.frameInfo_get, PresentedFrames, retFrames);
        }
        else
        {
            if (mWriteCount > 4)
                CASE_LOGW("%s() getHardwareBufferInfo[%" PRIu64 "] fail", __FUNCTION__, mWriteCount);
            else
                CASE_LOGV("%s() getHardwareBufferInfo[%" PRIu64 "] initial fail", __FUNCTION__, mWriteCount);
        }
    }
    else if (mInternalSuspend)
    {
        uint64_t PresentedFrames = mPresentedBytes / (uint64_t)frameSize();
        *frames = PresentedFrames;
    }
    else
    {
        CASE_LOGV("%s() no playback handler!? mSuspendCount = %u", __FUNCTION__, mSuspendCount);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamOut::internalStandby()
{
    AudioAutoTimeoutLock _l(mLock);

    status_t status = NO_ERROR;

    /// check close
    if (mStandby == false)
    {
        CASE_LOGV("+%s()", __FUNCTION__);
        status = close();
        CASE_LOGV("-%s()", __FUNCTION__);
    }

    return status;
}

bool AudioALSAStreamOut::useMTKFilter()
{
    switch (mUseCase)
    {
    case OUT_NORMAL:
    case OUT_LOW_LATENCY:
        return true;
    case OUT_HDMI_STEREO_MIXED:
    case OUT_HDMI_DIRECT_OUT:
    case OUT_HDMI_RAW_DIRECT_OUT:
    case OUT_HIRES_AUDIO:
    default:
        return false;
    }
}

status_t AudioALSAStreamOut::open()
{
    // call open() only when mLock is locked.
    ASSERT(mLock.tryLock() != 0);

    status_t status = NO_ERROR;

    if (mStandby == true)
    {
        CASE_LOGV("+%s()", __FUNCTION__);

        checkSuspendOutput();

        if (mInternalSuspend == false)
        {
            // create playback handler
            ASSERT(mPlaybackHandler == NULL);
            if (mUseCase == OUT_NORMAL || mUseCase == OUT_LOW_LATENCY || mUseCase == OUT_HIRES_AUDIO) {
                AudioALSASampleRateController::getInstance()->setScenarioStatus(PLAYBACK_SCENARIO_STREAM_OUT);
            }
            mPlaybackHandler = mStreamManager->createPlaybackHandler(&mStreamAttributeSource);

            // open audio hardware
            status = mPlaybackHandler->open();
            if (status != NO_ERROR)
            {
                CASE_LOGE("%s(), open() fail!!", __FUNCTION__);
                mPlaybackHandler->close();
                mStreamManager->destroyPlaybackHandler(mPlaybackHandler);
                mPlaybackHandler = NULL;
            }
            else
            {
                mStandby = false;
            }
        }
        else
        {
            mStandby = false;
        }

        OpenPCMDump(USE_CASE_TO_STRING(mUseCase));
        CASE_LOGV("-%s()", __FUNCTION__);
    }

    return status;
}

status_t AudioALSAStreamOut::close()
{
    // call close() only when mLock is locked.
    ASSERT(mLock.tryLock() != 0);

    status_t status = NO_ERROR;

    if (mStandby == false)
    {
        CASE_LOGV("+%s()", __FUNCTION__);
        mStandby = true;
        mWriteCount = 0;
        ClosePCMDump();

        if (mInternalSuspend == false)
        {
            ASSERT(mPlaybackHandler != NULL);

            // close audio hardware
            status = mPlaybackHandler->close();
            if (status != NO_ERROR)
            {
                CASE_LOGE("%s(), close() fail!!", __FUNCTION__);
            }
            // destroy playback handler
            mStreamManager->destroyPlaybackHandler(mPlaybackHandler);
            mPlaybackHandler = NULL;
            if (mUseCase == OUT_NORMAL || mUseCase == OUT_LOW_LATENCY || mUseCase == OUT_HIRES_AUDIO) {
                AudioALSASampleRateController::getInstance()->resetScenarioStatus(PLAYBACK_SCENARIO_STREAM_OUT);
            }
        }

        if (mUseCase == OUT_HDMI_RAW_DIRECT_OUT)
        {
             mStreamManager->putStreamOutIntoStandy(OUT_HDMI_DIRECT_OUT);
             mStreamManager->putStreamOutIntoStandy(OUT_HDMI_STEREO_MIXED);
        }
        else if (mUseCase == OUT_HDMI_DIRECT_OUT)
        {
             // let stereo stream have chance to leave internal suspend
             mStreamManager->putStreamOutIntoStandy(OUT_HDMI_STEREO_MIXED);
        }

        CASE_LOGV("-%s()", __FUNCTION__);
    }

    ASSERT(mPlaybackHandler == NULL);
    return status;
}

status_t AudioALSAStreamOut::routing(audio_devices_t output_devices)
{
    status_t status = NO_ERROR;

    if (mUseCase != OUT_NORMAL && mUseCase != OUT_LOW_LATENCY && mUseCase != OUT_HIRES_AUDIO)
        return status;

    if (output_devices == mStreamAttributeSource.output_devices)
    {
        CASE_LOGI("%s() routing to same device is not necessary", __FUNCTION__);
        return status;
    }

    bool is_lock_in_this_function = false;

    // triggered by another streamout setParameter routing
    // e.g. fast stream's routing can trigger normal stream's routing as well
    if (!mSelfRouting)
    {
        mLock.lock();
        CASE_LOGD("%s(), is_lock_in_this_function = true", __FUNCTION__);
        is_lock_in_this_function = true;
    }

    CASE_LOGV("+%s(), output_devices = 0x%x", __FUNCTION__, output_devices);

    if (mStandby == false)
    {
        ASSERT(mPlaybackHandler != NULL);

        status = close();
    }

    mStreamAttributeSource.output_devices = output_devices;

    if (is_lock_in_this_function == true)
    {
        mLock.unlock();
    }

    CASE_LOGV("-%s()", __FUNCTION__);
    return status;
}

status_t AudioALSAStreamOut::setScreenState(bool mode)
{
    if (NULL != mPlaybackHandler)
    {
        mPlaybackHandler->setScreenState(mode != 0, mStreamAttributeSource.buffer_size, 8192);
    }
    return NO_ERROR;
}

status_t AudioALSAStreamOut::setSuspend(const bool suspend_on)
{
    ALOGV("+%s(), mSuspendCount = %u, suspend_on = %d", __FUNCTION__, mSuspendCount, suspend_on);

    if (suspend_on == true)
    {
        mSuspendCount++;
    }
    else if (suspend_on == false)
    {
        ASSERT(mSuspendCount > 0);
        mSuspendCount--;
    }

    ALOGV("-%s(), mSuspendCount = %u", __FUNCTION__, mSuspendCount);
    return NO_ERROR;
}

void AudioALSAStreamOut::checkSuspendOutput(void)
{
    if (mUseCase == OUT_HDMI_RAW_DIRECT_OUT)
    {
        mStreamManager->putStreamOutIntoStandy(OUT_HDMI_DIRECT_OUT);
        mStreamManager->putStreamOutIntoStandy(OUT_HDMI_STEREO_MIXED);
    }
    else if (mUseCase == OUT_HDMI_DIRECT_OUT)
    {
        if (mStreamManager->isStreamOutActive(OUT_HDMI_RAW_DIRECT_OUT))
        {
            mInternalSuspend = true;
        }
        else
        {
            mStreamManager->putStreamOutIntoStandy(OUT_HDMI_STEREO_MIXED);
            mInternalSuspend = false;
        }
    }
    else if (mUseCase == OUT_HDMI_STEREO_MIXED &&
            (mStreamManager->isStreamOutActive(OUT_HDMI_DIRECT_OUT) ||
             mStreamManager->isStreamOutActive(OUT_HDMI_RAW_DIRECT_OUT)))
    {
        mInternalSuspend = true;
    }
    else
    {
        mInternalSuspend = false;
    }
}

uint32_t AudioALSAStreamOut::dataToDurationUs(size_t bytes)
{
    return (bytes * 1000 / (frameSize() * sampleRate() / 1000));
}

void AudioALSAStreamOut::OpenPCMDump(const char *class_name)
{
    String8 dump_file_name;
    dump_file_name.appendFormat("%s.%d.%s.%d_%dch_format(0x%x).pcm", streamout, mDumpFileNum, class_name,
                                mStreamAttributeSource.sample_rate, mStreamAttributeSource.num_channels,
                                mStreamAttributeSource.audio_format);

    mPCMDumpFile = AudioOpendumpPCMFile(dump_file_name.string(), streamout_propty);
    if (mPCMDumpFile != NULL)
    {
        CASE_LOGV("%s DumpFileName = %s", __FUNCTION__, dump_file_name.string());
        mDumpFileNum++;
        mDumpFileNum %= MAX_DUMP_NUM;
    }
}

void AudioALSAStreamOut::ClosePCMDump()
{
    if (mPCMDumpFile)
    {
        AudioCloseDumpPCMFile(mPCMDumpFile);
        mPCMDumpFile = NULL;
    }
}

void AudioALSAStreamOut::WritePcmDumpData(const void *buffer, ssize_t bytes)
{
    if (mPCMDumpFile)
    {
        AudioDumpPCMData((void *)buffer , bytes, mPCMDumpFile);
    }
}

status_t AudioALSAStreamOut::pause()
{
    return INVALID_OPERATION;
}

status_t AudioALSAStreamOut::resume()
{
    return INVALID_OPERATION;
}

int AudioALSAStreamOut::drain(audio_drain_type_t type __unused)
{
    return 0;
}

status_t AudioALSAStreamOut::flush()
{
    return INVALID_OPERATION;
}

status_t AudioALSAStreamOut::setCallBack(stream_callback_t callback __unused, void *cookie __unused)
{
    return INVALID_OPERATION;
}

status_t AudioALSAStreamOut::getNextWriteTimestamp(int64_t *timestamp __unused)
{
    return INVALID_OPERATION;
}

}
