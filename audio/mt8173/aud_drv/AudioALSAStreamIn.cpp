#define LOG_TAG "AudioALSAStreamIn"

#include "AudioALSAStreamIn.h"

#include <inttypes.h>

#include "AudioALSAStreamManager.h"
#include "AudioALSACaptureHandlerBase.h"
#include "AudioUtility.h"
#include "V3/include/AudioSpeechEnhanceInfo.h"
#include <audio_effects/effect_aec.h>

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define NORMAL_BUFFER_TIME_MS 20
#define LOW_LATENCY_BUFFER_TIME_MS 5


namespace android
{

int AudioALSAStreamIn::mDumpFileNum = 0;

static const audio_format_t       kDefaultInputSourceFormat      = AUDIO_FORMAT_PCM_16_BIT;
static const audio_channel_mask_t kDefaultInputSourceChannelMask = AUDIO_CHANNEL_IN_STEREO;
static const uint32_t             kDefaultInputSourceSampleRate  = 48000;
static const uint32_t             kDefaultInputBufferSize = 4096;


AudioALSAStreamIn::AudioALSAStreamIn() :
    mStreamManager(AudioALSAStreamManager::getInstance()),
    mCaptureHandler(NULL),
    mLockCount(0),
    mIdentity(0xFFFFFFFF),
    mSuspendCount(0),
    mStandbyFrameCount(0),
    mStandby(true),
    mUpdateOutputDevice(false),
    mUpdateInputDevice(false),
    mAudioSpeechEnhanceInfoInstance(AudioSpeechEnhanceInfo::getInstance())
{
    ALOGV("%s()", __FUNCTION__);

    memset(&mStreamAttributeTarget, 0, sizeof(mStreamAttributeTarget));

    for (int i = 0; i < MAX_PREPROCESSORS; i++)
    {
        mPreProcessEffectBackup[i] = NULL;
    }
    mPreProcessEffectBackupCount = 0;
    mPCMDumpFile = NULL;
}

AudioALSAStreamIn::~AudioALSAStreamIn()
{
    ALOGV("%s()", __FUNCTION__);

    ASSERT(mStandby == true && mCaptureHandler == NULL);
}

bool AudioALSAStreamIn::checkOpenStreamFormat(int *format)
{
    if (*format != kDefaultInputSourceFormat)
    {
        ALOGW("%s(), wrong format 0x%x, use 0x%x instead.", __FUNCTION__, *format, kDefaultInputSourceFormat);
        *format = kDefaultInputSourceFormat;
        return false;
    }
    else
    {
        return true;
    }
}

bool AudioALSAStreamIn::checkOpenStreamChannels(uint32_t *channels)
{
    switch (audio_channel_count_from_in_mask(*channels))
    {
    case 1:
    case 2:
        return true;
    default:
        ALOGW("%s(), wrong channels 0x%x, use 0x%x instead.", __FUNCTION__, *channels, kDefaultInputSourceChannelMask);
        *channels = kDefaultInputSourceChannelMask;
        return false;
    }
}

bool AudioALSAStreamIn::checkOpenStreamSampleRate(const audio_devices_t devices, uint32_t *sampleRate)
{
    (void)devices;
    // Normal record
    if (*sampleRate != kDefaultInputSourceSampleRate)
    {
        ALOGV("%s(), origin sampleRate %d, kDefaultInputSourceSampleRate %d.", __FUNCTION__, *sampleRate, kDefaultInputSourceSampleRate);
        if (mStreamAttributeTarget.BesRecord_Info.besrecord_tuningEnable || mStreamAttributeTarget.BesRecord_Info.besrecord_dmnr_tuningEnable)
        {
            if (*sampleRate == 16000)
            {
                ALOGI("%s(), BesRecord 16K tuning", __FUNCTION__);
                mStreamAttributeTarget.BesRecord_Info.besrecord_tuning16K = true;
                *sampleRate = 48000;
                return true;
            }
        }

        return true;
    }
    else
    {
        if (mStreamAttributeTarget.BesRecord_Info.besrecord_tuningEnable || mStreamAttributeTarget.BesRecord_Info.besrecord_dmnr_tuningEnable)
        {
            mStreamAttributeTarget.BesRecord_Info.besrecord_tuning16K = false;
        }

        return true;
    }
}

status_t AudioALSAStreamIn::set(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    android_audio_legacy::AudioSystem::audio_in_acoustics acoustics,
    uint32_t flags)
{
    ALOGV("%s() devices = 0x%x format = 0x%x channels = 0x%x sampleRate = %d acoustics = 0x%x flags = 0x%x",
          __FUNCTION__, devices, *format, *channels, *sampleRate, acoustics, flags);

    AudioAutoTimeoutLock _l(mLock);

    *status = NO_ERROR;

    CheckBesRecordInfo();

    // check format
    if (checkOpenStreamFormat(format) == false)
    {
        *status = BAD_VALUE;
    }

    // check channel mask
    if (checkOpenStreamChannels(channels) == false)
    {
        *status = BAD_VALUE;
    }

    // check sample rate
    if (checkOpenStreamSampleRate(devices, sampleRate) == false)
    {
        *status = BAD_VALUE;
    }

    // config stream attribute
    if (*status == NO_ERROR)
    {
        if (audio_is_bluetooth_sco_device(devices)) {
            flags &= ~AUDIO_INPUT_FLAG_FAST;
        }

        mStreamAttributeTarget.mAudioInputFlags = static_cast<audio_input_flags_t>(flags);

        // format
        mStreamAttributeTarget.audio_format = static_cast<audio_format_t>(*format);

        // channel
        mStreamAttributeTarget.audio_channel_mask = *channels;
        mStreamAttributeTarget.num_channels = android_audio_legacy::AudioSystem::popCount(*channels);

        // sample rate
        mStreamAttributeTarget.sample_rate = *sampleRate;

        // devices
        mStreamAttributeTarget.input_device = static_cast<audio_devices_t>(devices);

        // acoustics flags
        mStreamAttributeTarget.acoustics_mask = static_cast<audio_in_acoustics_t>(acoustics);

#ifdef UPLINK_LOW_LATENCY
        uint32_t bufferDurationMs = (mStreamAttributeTarget.mAudioInputFlags & AUDIO_INPUT_FLAG_FAST) ?
                                     LOW_LATENCY_BUFFER_TIME_MS : NORMAL_BUFFER_TIME_MS;

        mStreamAttributeTarget.buffer_size = (mStreamAttributeTarget.sample_rate / 1000) *
                                             bufferDurationMs * mStreamAttributeTarget.num_channels *
                                             audio_bytes_per_sample(mStreamAttributeTarget.audio_format);
#else
        // Buffer size: 2048(period_size) * 2(period_count) * 2(ch) * 2(byte) = 16 kb
        mStreamAttributeTarget.buffer_size = kDefaultInputBufferSize;
#endif
    }

    return *status;
}

uint32_t AudioALSAStreamIn::sampleRate() const
{
    ALOGV("%s(), return %d", __FUNCTION__, mStreamAttributeTarget.sample_rate);
    return mStreamAttributeTarget.sample_rate;
}

size_t AudioALSAStreamIn::bufferSize() const
{
    ALOGV("%s(), return 0x%x", __FUNCTION__, mStreamAttributeTarget.buffer_size);
    return mStreamAttributeTarget.buffer_size;
}

uint32_t AudioALSAStreamIn::channels() const
{
    ALOGV("%s(), return 0x%x", __FUNCTION__, mStreamAttributeTarget.audio_channel_mask);
    return mStreamAttributeTarget.audio_channel_mask;
}

int AudioALSAStreamIn::format() const
{
    ALOGV("%s(), return 0x%x", __FUNCTION__, mStreamAttributeTarget.audio_format);
    return mStreamAttributeTarget.audio_format;
}

status_t AudioALSAStreamIn::setGain(float gain __unused)
{
    ALOGV("%s()", __FUNCTION__);
    return INVALID_OPERATION;
}

void AudioALSAStreamIn::SetInputMute(bool bEnable)
{
    ALOGI_IF(bEnable, "%s(), %d", __FUNCTION__, bEnable);
    //AudioAutoTimeoutLock _l(mLock);
    mStreamAttributeTarget.micmute = bEnable;
}

ssize_t AudioALSAStreamIn::read(void *buffer, ssize_t bytes)
{
    ALOGV("+%s() bytes = %zd", __FUNCTION__, bytes);
    ssize_t ret_size = bytes;

    if (mStreamAttributeTarget.mAudioInputFlags & AUDIO_INPUT_FLAG_FAST) {
        // fast track runs at RT thread so that read() will get mLock frequently with high priority.
        // it may cause other thread (e.g. setParameters for routing) can't get mLock in time.
        int tryCount = 5;
        while (mLockCount && tryCount--) {
            ALOGD("%s free CPU mLockCount %d tryCount %d", __FUNCTION__, mLockCount, tryCount);
            usleep(200);
        }
    }

    if (mSuspendCount > 0)
    {
        // here to sleep a buffer size latency and return.
        ALOGI("%s(), mSuspendCount = %u", __FUNCTION__, mSuspendCount);
        memset(buffer, 0, bytes);
        size_t frameSize = mStreamAttributeTarget.num_channels * audio_bytes_per_sample(mStreamAttributeTarget.audio_format);
        int sleepus = ((bytes * 1000) / ((mStreamAttributeTarget.sample_rate / 1000) * frameSize));
        ALOGI("%s(), sleepus = %d", __FUNCTION__, sleepus);
        usleep(sleepus);
        return bytes;
    }

    AudioAutoTimeoutLock _l(mLock);

    status_t status = NO_ERROR;
    if ((mUpdateOutputDevice == true) || (mUpdateInputDevice == true))
    {
        if (mStandby == false)
        {
            ASSERT(mStreamManager->isModeInPhoneCall() == false);
            ALOGD("%s(), close handler and reopen it", __FUNCTION__);
            status = close();
            ASSERT(status == NO_ERROR);
        }
    }

    /// check open
    if (mStandby == true)
    {
        status = open();
    }

    /// write pcm data
    ASSERT(mCaptureHandler != NULL);

    ret_size = mCaptureHandler->read(buffer, bytes);

    WritePcmDumpData(buffer, bytes);

    ALOGV("-%s() bytes = %zd", __FUNCTION__, ret_size);
    return ret_size;
}

status_t AudioALSAStreamIn::dump(int fd __unused, const Vector<String16> &args __unused)
{
    return NO_ERROR;
}

status_t AudioALSAStreamIn::standby()
{
    ALOGV("+%s()", __FUNCTION__);
    android_atomic_inc(&mLockCount);
    AudioAutoTimeoutLock _l(mLock);

    status_t status = NO_ERROR;

    /// check close
    if (mStandby == false)
    {
        /* Standby state will close data provider that will reset the capture position,
           therefore, we keep the frame counts before standby */
        int64_t time;
        int ret = mCaptureHandler->getCapturePosition(&mStandbyFrameCount, &time);
        ALOGD("%s(), keep the mStandbyFrameCount = %" PRIu64 ", ret = %d", __FUNCTION__, mStandbyFrameCount, ret);
        status = close();
    }

    android_atomic_dec(&mLockCount);
    ALOGV("-%s()", __FUNCTION__);
    return status;
}

status_t AudioALSAStreamIn::setParameters(const String8 &keyValuePairs)
{
    ALOGV("+%s(): %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);

    /// keys
    const String8 keyInputSource = String8(AudioParameter::keyInputSource);
    const String8 keyRouting     = String8(AudioParameter::keyRouting);

    /// parse key value pairs
    status_t status = NO_ERROR;
    int value = 0;

    /// intput source
    if (param.getInt(keyInputSource, value) == NO_ERROR)
    {
        param.remove(keyInputSource);
        android_atomic_inc(&mLockCount);
        AudioAutoTimeoutLock _l(mLock);
        ALOGV("%s() InputSource = %d", __FUNCTION__, value);
        mStreamAttributeTarget.input_source = static_cast<audio_source_t>(value);
        android_atomic_dec(&mLockCount);
    }

    /// routing
    if (param.getInt(keyRouting, value) == NO_ERROR)
    {
        param.remove(keyRouting);

        android_atomic_inc(&mLockCount);
        AudioAutoTimeoutLock _l(mLock);

        audio_devices_t inputdevice = static_cast<audio_devices_t>(value);
#if 0
        //only need to modify the device while VoIP
        if (mStreamAttributeTarget.BesRecord_Info.besrecord_voip_enable == true)
        {
            if (mStreamAttributeTarget.output_devices == AUDIO_DEVICE_OUT_SPEAKER)
            {
                if (inputdevice == AUDIO_DEVICE_IN_BUILTIN_MIC)
                {
                    if (USE_REFMIC_IN_LOUDSPK == 1)
                    {
                        inputdevice = AUDIO_DEVICE_IN_BACK_MIC;
                        ALOGI("%s() force change to back mic", __FUNCTION__);
                    }
                }
            }
        }
#endif
        status = mStreamManager->routingInputDevice(this, mStreamAttributeTarget.input_device, inputdevice);
        android_atomic_dec(&mLockCount);
    }

    if (param.size())
    {
        ALOGW("%s(), still have param.size() = %zu, remain param = \"%s\"",
              __FUNCTION__, param.size(), param.toString().string());
        status = BAD_VALUE;
    }

    ALOGV("-%s(): %s ", __FUNCTION__, keyValuePairs.string());
    return status;
}

String8 AudioALSAStreamIn::getParameters(const String8 &keys)
{
    ALOGV("%s()", __FUNCTION__);
    AudioParameter param = AudioParameter(keys);
    return param.toString();
}

unsigned int AudioALSAStreamIn::getInputFramesLost() const
{
    return 0;
}

int AudioALSAStreamIn::getCapturePosition(int64_t *frames, int64_t *time)
{
    ALOGV("+%s()", __FUNCTION__);
    android_atomic_inc(&mLockCount);
    AudioAutoTimeoutLock _l(mLock);

    if (mCaptureHandler == NULL || frames == NULL || time == NULL) {
        android_atomic_dec(&mLockCount);
        return -EINVAL;
    }

    int ret = mCaptureHandler->getCapturePosition(frames, time);
    *frames += mStandbyFrameCount;
    android_atomic_dec(&mLockCount);
    ALOGV("%s(), frames = %" PRIu64 " (mStandbyFrameCount = %" PRIu64 "), times = " PRIu64, __FUNCTION__, *frames, *time);

    return ret;
}

status_t AudioALSAStreamIn::addAudioEffect(effect_handle_t effect)
{
    ALOGV("%s(), %p", __FUNCTION__, effect);

    int status;
    effect_descriptor_t desc;

    //record the effect which need enabled and set to capture handle later (setup it while capture handle created)
    status = (*effect)->get_descriptor(effect, &desc);
    ALOGV("%s(), effect name:%s, BackupCount=%d", __FUNCTION__, desc.name, mPreProcessEffectBackupCount);

    if (mPreProcessEffectBackupCount >= MAX_PREPROCESSORS)
    {
        ALOGW("%s(), exceed the uplimit", __FUNCTION__);
        return NO_ERROR;
    }

    if (status != 0)
    {
        ALOGW("%s(), no corresponding effect", __FUNCTION__);
        return NO_ERROR;
    }
    else
    {
        android_atomic_inc(&mLockCount);
        AudioAutoTimeoutLock _l(mLock);

        for (int i = 0; i < mPreProcessEffectBackupCount; i++)
        {
            if (mPreProcessEffectBackup[i] == effect)
            {
                ALOGW("%s() already found %s at index %d", __FUNCTION__, desc.name, i);
                android_atomic_dec(&mLockCount);
                return NO_ERROR;
            }
        }

        //echo reference
        if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0)
        {
            ALOGD("%s(), AECOn, need reopen the capture handle", __FUNCTION__);
            if (mStandby == false)
            {
                close();
            }
            mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_AECOn = true;
        }

        mPreProcessEffectBackup[mPreProcessEffectBackupCount] = effect;
        mPreProcessEffectBackupCount++;

        mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Record[mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Count] = effect;
        mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Count++;
        mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Update = true;

        android_atomic_dec(&mLockCount);
    }

    ALOGV("%s()-", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAStreamIn::removeAudioEffect(effect_handle_t effect)
{
    ALOGV("%s(), %p", __FUNCTION__, effect);

    int i;
    int status;
    status_t RetStatus = -EINVAL;
    effect_descriptor_t desc;

    if (mPreProcessEffectBackupCount <= 0)
    {
        ALOGW("%s(), mPreProcessEffectBackupCount wrong", __FUNCTION__);
        return NO_ERROR;
    }


    status = (*effect)->get_descriptor(effect, &desc);
    ALOGV("%s(), effect name:%s, BackupCount=%d", __FUNCTION__, desc.name, mPreProcessEffectBackupCount);
    if (status != 0)
    {
        ALOGW("%s(), no corresponding effect", __FUNCTION__);
        return NO_ERROR;
    }

    android_atomic_inc(&mLockCount);
    AudioAutoTimeoutLock _l(mLock);

    for (i = 0; i < mPreProcessEffectBackupCount; i++)
    {
        if (RetStatus == 0)   /* status == 0 means an effect was removed from a previous slot */
        {
            mPreProcessEffectBackup[i - 1] = mPreProcessEffectBackup[i];
            mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Record[i - 1] = mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Record[i];
            ALOGD("%s() moving fx from %d to %d", __FUNCTION__, i, i - 1);
            continue;
        }
        if (mPreProcessEffectBackup[i] == effect)
        {
            ALOGV("%s() found fx at index %d, %p", __FUNCTION__, i,mPreProcessEffectBackup[i]);
            //            free(preprocessors[i].channel_configs);
            RetStatus = 0;
        }
    }

    if (RetStatus != 0)
    {
        ALOGD("%s() no effect found in backup queue", __FUNCTION__);
        android_atomic_dec(&mLockCount);
        return NO_ERROR;
    }

    //echo reference
    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0)
    {
        if (mStandby == false)
        {
            close();
        }
        mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_AECOn = false;
    }

    mPreProcessEffectBackupCount--;
    /* if we remove one effect, at least the last preproc should be reset */
    mPreProcessEffectBackup[mPreProcessEffectBackupCount] = NULL;

    mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Count--;
    /* if we remove one effect, at least the last preproc should be reset */
    mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Record[mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Count] = NULL;
    mStreamAttributeTarget.NativePreprocess_Info.PreProcessEffect_Update = true;

    android_atomic_dec(&mLockCount);
    ALOGV("%s()-", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAStreamIn::open()
{
    // call open() only when mLock is locked.
    ASSERT(mLock.tryLock() != 0);

    ALOGV("+%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    if (mStandby == true)
    {
        mStandby = false;
        mUpdateOutputDevice = false;
        mUpdateInputDevice = false;

        // create capture handler
        ASSERT(mCaptureHandler == NULL);
        mCaptureHandler = mStreamManager->createCaptureHandler(&mStreamAttributeTarget);

        // open audio hardware
        status = mCaptureHandler->open();
        if (status != NO_ERROR)
        {
            ALOGE("%s(), open() fail!!", __FUNCTION__);
            mCaptureHandler->close();
            delete mCaptureHandler;
            mCaptureHandler = NULL;
        }

        OpenPCMDump();
    }

    ALOGV("-%s()", __FUNCTION__);

    return status;
}

status_t AudioALSAStreamIn::close()
{
    // call close() only when mLock is locked.
    ASSERT(mLock.tryLock() != 0);

    ALOGV("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    if (mStandby == false)
    {
        mStandby = true;

        ASSERT(mCaptureHandler != NULL);

        // close audio hardware
        status = mCaptureHandler->close();
        if (status != NO_ERROR)
        {
            ALOGE("%s(), close() fail!!", __FUNCTION__);
        }

        ClosePCMDump();
        // destroy playback handler
        mStreamManager->destroyCaptureHandler(mCaptureHandler);
        mCaptureHandler = NULL;
    }

    ASSERT(mCaptureHandler == NULL);
    return status;
}

status_t AudioALSAStreamIn::routing(audio_devices_t input_device)
{
    bool is_lock_in_this_function = false;

    if (mLock.tryLock() == 0) // from another stream in setParameter routing
    {
        ALOGI("%s(), is_lock_in_this_function = true", __FUNCTION__);
        is_lock_in_this_function = true;
    }

    ALOGV("+%s(), input_device = 0x%x", __FUNCTION__, input_device);

    status_t status = NO_ERROR;

    if (input_device == mStreamAttributeTarget.input_device)
    {
        ALOGW("%s(), input_device = 0x%x, already the same input device as current using", __FUNCTION__, input_device);
        if (is_lock_in_this_function == true)
        {
            mLock.unlock();
        }
        return status;
    }

    if (mStandby == false)
    {
        ASSERT(mStreamManager->isModeInPhoneCall() == false);
        //status = close();
        mUpdateInputDevice = true;
    }

    mStreamAttributeTarget.input_device = input_device;

    if (is_lock_in_this_function == true)
    {
        mLock.unlock();
    }

    ALOGV("-%s()", __FUNCTION__);
    return status;
}

AudioALSACaptureHandlerBase* AudioALSAStreamIn::getStreamInCaptureHandler()
{
    ALOGV("%s(), mCaptureHandler=%p", __FUNCTION__, mCaptureHandler);
    return mCaptureHandler;
}

status_t AudioALSAStreamIn::updateOutputDeviceInfoForInputStream(audio_devices_t output_devices)
{
    ALOGV("+%s(), output_devices: 0x%x => 0x%x", __FUNCTION__, mStreamAttributeTarget.output_devices, output_devices);

    status_t status = NO_ERROR;
    bool bBesRecUpdate = false;
    audio_devices_t inputdevice = mStreamAttributeTarget.input_device;

    if (output_devices != mStreamAttributeTarget.output_devices)
    {
#if 0
        //only need to modify the input device under VoIP
        if (mStreamAttributeTarget.BesRecord_Info.besrecord_voip_enable == true)
        {
            if (output_devices == AUDIO_DEVICE_OUT_SPEAKER)
            {
                if (inputdevice == AUDIO_DEVICE_IN_BUILTIN_MIC)
                {
                    if (USE_REFMIC_IN_LOUDSPK == 1)
                    {
                        inputdevice = AUDIO_DEVICE_IN_BACK_MIC;
                        ALOGI("%s(), force using back mic", __FUNCTION__);
                    }
                }
            }
        }
#endif

        if (inputdevice != mStreamAttributeTarget.input_device)
        {
            //update output devices to input stream info
            ALOGV("%s(), input_device: 0x%x => 0x%x", __FUNCTION__, mStreamAttributeTarget.input_device, inputdevice);
            android_atomic_inc(&mLockCount);
            AudioAutoTimeoutLock _l(mLock);
            mStreamAttributeTarget.output_devices = output_devices;
            routing(inputdevice);
            android_atomic_dec(&mLockCount);
        }
        else    //if no input device update
        {
            //speaker/receiver(headphone) switch no input path change, but should use receiver params
            if (((output_devices == AUDIO_DEVICE_OUT_SPEAKER) && ((mStreamAttributeTarget.output_devices == AUDIO_DEVICE_OUT_EARPIECE) ||
                                                                  (mStreamAttributeTarget.output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE)))
                || (((output_devices == AUDIO_DEVICE_OUT_EARPIECE) || (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) && (mStreamAttributeTarget.output_devices == AUDIO_DEVICE_OUT_SPEAKER)))
            {
                ALOGV("%s(), BesRecord parameters update", __FUNCTION__);
                bBesRecUpdate = true;
            }
            //update output devices to input stream info
            mStreamAttributeTarget.output_devices = output_devices;

            if (bBesRecUpdate)
            {
                //update VoIP parameters config, only streamin has VoIP process
                if ((mStreamAttributeTarget.BesRecord_Info.besrecord_enable == true) &&
                    (mStreamAttributeTarget.BesRecord_Info.besrecord_voip_enable == true))
                {
#if 1
                    ALOGD("%s(), going to check UpdateBesRecParam", __FUNCTION__);
                    mUpdateOutputDevice = true;
#if 0
                    android_atomic_inc(&mLockCount);
                    AudioAutoTimeoutLock _l(mLock);
                    if (mStandby == false)
                    {
                        ASSERT(mStreamManager->isModeInPhoneCall() == false);
                        ALOGD("%s(), close handler and reopen it", __FUNCTION__);
                        status = close();
                    }
                    android_atomic_dec(&mLockCount);
#endif
#else
                    // [FIXME] Cannot trigger AEC resync successfully
                    ALOGD("%s(), going to UpdateBesRecParam", __FUNCTION__);
                    android_atomic_inc(&mLockCount);
                    AudioAutoTimeoutLock _l(mLock);
                    if (mCaptureHandler != NULL)
                    {
                        mCaptureHandler->UpdateBesRecParam();
                    }
                    else
                    {
                        ALOGV("%s(), mCaptureHandler is destroyed, no need to update", __FUNCTION__);
                    }
                    android_atomic_dec(&mLockCount);
#endif
                }
            }
        }

    }

    ALOGV("-%s()", __FUNCTION__);
    return status;
}

status_t AudioALSAStreamIn::setSuspend(const bool suspend_on)
{
    ALOGV("%s(), mSuspendCount = %u, suspend_on = %d", __FUNCTION__, mSuspendCount, suspend_on);

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

void AudioALSAStreamIn::CheckBesRecordInfo()
{
    ALOGV("+%s()", __FUNCTION__);

    if (mAudioSpeechEnhanceInfoInstance != NULL)
    {
        mStreamAttributeTarget.BesRecord_Info.besrecord_scene = mAudioSpeechEnhanceInfoInstance->GetBesRecScene();
        mAudioSpeechEnhanceInfoInstance->ResetBesRecScene();
        //for besrecord tuning
        mStreamAttributeTarget.BesRecord_Info.besrecord_tuningEnable = mAudioSpeechEnhanceInfoInstance->IsBesRecTuningEnable();

        //for DMNR tuning
        mStreamAttributeTarget.BesRecord_Info.besrecord_dmnr_tuningEnable = mAudioSpeechEnhanceInfoInstance->IsAPDMNRTuningEnable();

        memset(mStreamAttributeTarget.BesRecord_Info.besrecord_VMFileName, 0, VM_FILE_NAME_LEN_MAX);
        mAudioSpeechEnhanceInfoInstance->GetBesRecVMFileName(mStreamAttributeTarget.BesRecord_Info.besrecord_VMFileName);

        //for engineer mode
        if (mAudioSpeechEnhanceInfoInstance->GetForceMagiASRState() > 0)
        {
            mStreamAttributeTarget.BesRecord_Info.besrecord_ForceMagiASREnable = true;
        }
        if (mAudioSpeechEnhanceInfoInstance->GetForceAECRecState())
        {
            mStreamAttributeTarget.BesRecord_Info.besrecord_ForceAECRecEnable = true;
        }

        //for dynamic mask
        mStreamAttributeTarget.BesRecord_Info.besrecord_dynamic_mask = mAudioSpeechEnhanceInfoInstance->GetDynamicVoIPSpeechEnhancementMask();
    }
    ALOGV("-%s()", __FUNCTION__);
}

void AudioALSAStreamIn::UpdateDynamicFunctionMask(void)
{
    ALOGV("+%s()", __FUNCTION__);

    if (mAudioSpeechEnhanceInfoInstance != NULL)
    {
        //for dynamic mask
        mStreamAttributeTarget.BesRecord_Info.besrecord_dynamic_mask = mAudioSpeechEnhanceInfoInstance->GetDynamicVoIPSpeechEnhancementMask();
    }
    ALOGV("-%s()", __FUNCTION__);
}

size_t AudioALSAStreamIn::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    size_t bufferSize;
#ifdef UPLINK_LOW_LATENCY
    bufferSize = (sampleRate / 1000) * NORMAL_BUFFER_TIME_MS * channelCount * audio_bytes_per_sample(static_cast<audio_format_t>(format));
#else
    bufferSize = (kDefaultInputBufferSize * sampleRate) / kDefaultInputSourceSampleRate;
    bufferSize = bufferSize * channelCount / android_audio_legacy::AudioSystem::popCount(kDefaultInputSourceChannelMask);
    bufferSize = bufferSize * audio_bytes_per_sample(static_cast<audio_format_t>(format)) / audio_bytes_per_sample(kDefaultInputSourceFormat);
#endif
    /* 64 bytes alignedment */
    bufferSize += 0x3f;
    bufferSize &= ~0x3f;

    ALOGV("%s sampleRate = %d channelCount = %d format = %d bufferSize = %zu",
          __FUNCTION__, sampleRate, format, channelCount, bufferSize);
    return bufferSize;
}

bool AudioALSAStreamIn::isSupportConcurrencyInCall(void)
{
    ALOGV("+%s()", __FUNCTION__);
    bool bIsSupport = false;

    android_atomic_inc(&mLockCount);
    AudioAutoTimeoutLock _l(mLock);

    if (mCaptureHandler != NULL)
    {
        bIsSupport = mCaptureHandler->isSupportConcurrencyInCall();
    }
    else
    {
        ALOGW("%s mCaptureHandler is NULL", __FUNCTION__);
        bIsSupport = false;
    }
    android_atomic_dec(&mLockCount);
    ALOGV("-%s() bIsSupport = %d", __FUNCTION__, bIsSupport);
    return bIsSupport;
}

void AudioALSAStreamIn::OpenPCMDump()
{
    ALOGV("%s()", __FUNCTION__);

    String8 dump_file_name;
    dump_file_name.appendFormat("%s.%d.%d_%dch_format(0x%x).pcm", streamin, mDumpFileNum,
                                mStreamAttributeTarget.sample_rate, mStreamAttributeTarget.num_channels,
                                mStreamAttributeTarget.audio_format);

    mPCMDumpFile = AudioOpendumpPCMFile(dump_file_name.string(), streamin_propty);
    if (mPCMDumpFile != NULL)
    {
        ALOGV("%s DumpFileName = %s", __FUNCTION__, dump_file_name.string());
    }

    mDumpFileNum++;
    mDumpFileNum %= MAX_DUMP_NUM;
}

void AudioALSAStreamIn::ClosePCMDump()
{
    ALOGV("%s()", __FUNCTION__);
    if (mPCMDumpFile)
    {
        AudioCloseDumpPCMFile(mPCMDumpFile);
        ALOGV("%s(), close it", __FUNCTION__);
    }
}

void  AudioALSAStreamIn::WritePcmDumpData(void *buffer, ssize_t bytes)
{
    if (mPCMDumpFile)
    {
        AudioDumpPCMData((void *)buffer , bytes, mPCMDumpFile);
    }
}


}
