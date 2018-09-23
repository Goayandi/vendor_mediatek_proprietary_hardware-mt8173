#define LOG_TAG "AudioALSAStreamManager"
//#define LOG_NDEBUG 0

#include "AudioALSAStreamManager.h"

#include <cutils/properties.h>

#include "AudioALSAStreamOut.h"
#include "AudioALSAStreamIn.h"

#include "AudioALSAPlaybackHandlerBase.h"
#include "AudioALSAPlaybackHandlerNormal.h"
#include "AudioALSAPlaybackHandlerFast.h"
#include "AudioALSAPlaybackHandlerBTSCO.h"
#include "AudioALSAPlaybackHandlerHDMI.h"
#include "AudioALSAPlaybackHandlerHDMIRaw.h"

#include "AudioALSACaptureHandlerBase.h"
#include "AudioALSACaptureHandlerNormal.h"
#include "AudioALSACaptureHandlerBTSCO.h"
#include "AudioALSACaptureHandlerAEC.h"
#include "AudioALSACaptureHandlerFMRadio.h"

#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
#include "AudioALSAPlaybackHandlerVoice.h"
#include "AudioALSACaptureHandlerVoice.h"
#include "AudioALSASpeechPhoneCallController.h"
#include "SpeechDriverInterface.h"
#include "SpeechDriverFactory.h"
#include "SpeechVMRecorder.h"
#include "SpeechEnhancementController.h"
#endif

#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSAHardware.h"

#include "AudioCompFltCustParam.h"
#include "AudioCustParam.h"
#include "V3/include/AudioSpeechEnhanceInfo.h"
#include "V3/include/AudioVolumeFactory.h"
#include "V3/include/AudioALSAFMController.h"


namespace android
{

/*==============================================================================
 *                     Singleton Pattern
 *============================================================================*/

AudioALSAStreamManager *AudioALSAStreamManager::mStreamManager = NULL;
AudioALSAStreamManager *AudioALSAStreamManager::getInstance()
{
    static AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mStreamManager == NULL)
    {
        mStreamManager = new AudioALSAStreamManager();
    }
    ASSERT(mStreamManager != NULL);
    return mStreamManager;
}


/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

AudioALSAStreamManager::AudioALSAStreamManager() :
    mPlaybackHandlerIndex(0),
    mCaptureHandlerIndex(0),
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    mSpeechPhoneCallController(AudioALSASpeechPhoneCallController::getInstance()),
    mSpeechDriverFactory(SpeechDriverFactory::GetInstance()),
    mPhoneCallSpeechOpen(false),
#endif
    mFMController(AudioALSAFMController::getInstance()),
    mAudioALSAVolumeController(AudioVolumeFactory::CreateAudioVolumeController()),
    mMicMute(false),
    mAudioMode(AUDIO_MODE_NORMAL),
    mBesLoudnessStatus(false),
    mBesLoudnessControlCallback(NULL),
    mAudioSpeechEnhanceInfoInstance(AudioSpeechEnhanceInfo::getInstance()),
    mHeadsetChange(false),
    mBypassPostProcessDL(false)
{
    ALOGV("%s()", __FUNCTION__);

    mStreamOutVector.clear();
    mStreamInVector.clear();

    mPlaybackHandlerVector.clear();
    mCaptureHandlerVector.clear();

    mFilterManagerVector.clear();

#ifdef MTK_BESLOUDNESS_SUPPORT
    unsigned int result = 0 ;
    AUDIO_AUDENH_CONTROL_OPTION_STRUCT audioParam;
    if (GetBesLoudnessControlOptionParamFromNV(&audioParam))
    {
        result = audioParam.u32EnableFlg;
    }
    mBesLoudnessStatus = (result ? true : false);
    ALOGD("AudioALSAStreamManager mBesLoudnessStatus [%d] (From NvRam) \n", mBesLoudnessStatus);
#else
    mBesLoudnessStatus = false;
    ALOGD("AudioALSAStreamManager mBesLoudnessStatus [%d] (Always) \n", mBesLoudnessStatus);
#endif
}


AudioALSAStreamManager::~AudioALSAStreamManager()
{
    ALOGD("%s()", __FUNCTION__);
}


/*==============================================================================
 *                     Implementations
 *============================================================================*/

android_audio_legacy::AudioStreamOut *AudioALSAStreamManager::openOutputStream(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status)
{
    return openOutputStreamWithFlags(devices, format, channels, sampleRate, status, 0);
}

android_audio_legacy::AudioStreamOut *AudioALSAStreamManager::openOutputStreamWithFlags(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    uint32_t output_flag)
{
    ALOGV("+%s()", __FUNCTION__);
    AudioAutoTimeoutLock streamVectorAutoTimeoutLock(mStreamVectorLock);
    AudioAutoTimeoutLock _l(mLock);

    if (format == NULL || channels == NULL || sampleRate == NULL || status == NULL)
    {
        ALOGE("%s(), NULL pointer!! format = %p, channels = %p, sampleRate = %p, status = %p",
              __FUNCTION__, format, channels, sampleRate, status);
        if (status != NULL) { *status = INVALID_OPERATION; }
        return NULL;
    }

    // stream out flags
    const uint32_t flags = output_flag;
    int output_handle = *status;

    if (mStreamOutVector.indexOfKey(output_handle) >= 0) {
        ALOGE("-%s() handle(%d) already existed", __FUNCTION__, output_handle);
        return NULL;
    }

    // create stream out
    AudioALSAStreamOut *pAudioALSAStreamOut = new AudioALSAStreamOut();
    pAudioALSAStreamOut->set(devices, format, channels, sampleRate, status, flags);
    if (*status != NO_ERROR)
    {
        ALOGW("-%s() stream out set fail", __FUNCTION__);
        delete pAudioALSAStreamOut;
        pAudioALSAStreamOut = NULL;
        return NULL;
    }

#ifdef MTK_HDMI_FORCE_AUDIO_CLK
    if (pAudioALSAStreamOut->getUseCase() == AudioALSAStreamOut::OUT_HDMI_STEREO_MIXED &&
        !isHdmiStreamOutExist()) {
        AudioALSAPlaybackHandlerHDMI::forceClkOn(pAudioALSAStreamOut->getStreamAttribute());
    }
#endif

    // save stream out object in vector
    pAudioALSAStreamOut->setIdentity(output_handle);
    mStreamOutVector.add(output_handle, pAudioALSAStreamOut);

    // setup Filter for ACF/HCF/AudEnh/VibSPK
    if (pAudioALSAStreamOut->useMTKFilter()) {
        AudioMTKFilterManager *pAudioFilterManagerHandler = new AudioMTKFilterManager(*sampleRate, android_audio_legacy::AudioSystem::popCount(*channels), *format, pAudioALSAStreamOut->bufferSize());
        if (pAudioFilterManagerHandler != NULL)
        {
            if (pAudioFilterManagerHandler->init() == NO_ERROR)
            {
                mFilterManagerVector.add(output_handle, pAudioFilterManagerHandler);
            }
            else
            {
                delete pAudioFilterManagerHandler;
            }
        }
    }

    ALOGV("-%s(), out = %p, status = 0x%x, mStreamOutVector.size() = %zu",
          __FUNCTION__, pAudioALSAStreamOut, *status, mStreamOutVector.size());

    return pAudioALSAStreamOut;
}

void AudioALSAStreamManager::closeOutputStream(android_audio_legacy::AudioStreamOut *out)
{
    ALOGV("+%s(), out = %p, mStreamOutVector.size() = %zu", __FUNCTION__, out, mStreamOutVector.size());
    AudioAutoTimeoutLock streamVectorAutoTimeoutLock(mStreamVectorLock);
    AudioAutoTimeoutLock _l(mLock);

    if (out == NULL)
    {
        ALOGW("%s(), Cannot close null output stream!! return", __FUNCTION__);
        return;
    }

    AudioALSAStreamOut *pAudioALSAStreamOut = static_cast<AudioALSAStreamOut *>(out);
    ASSERT(pAudioALSAStreamOut != 0);
#ifdef MTK_HDMI_FORCE_AUDIO_CLK
    uint32_t useCase = pAudioALSAStreamOut->getUseCase();
#endif

    ssize_t dFltMngindex = mFilterManagerVector.indexOfKey(pAudioALSAStreamOut->getIdentity());

    if (dFltMngindex >= 0 && dFltMngindex < (ssize_t)mFilterManagerVector.size())
    {
        AudioMTKFilterManager *pAudioFilterManagerHandler = static_cast<AudioMTKFilterManager *>(mFilterManagerVector[dFltMngindex]);
        ALOGV("%s, remove mFilterManagerVector Success [%zd]/[%zu] [%d], pAudioFilterManagerHandler=%p",
              __FUNCTION__, dFltMngindex, mFilterManagerVector.size(), pAudioALSAStreamOut->getIdentity(), pAudioFilterManagerHandler);
        ASSERT(pAudioFilterManagerHandler != 0);
        mFilterManagerVector.removeItem(pAudioALSAStreamOut->getIdentity());
        delete pAudioFilterManagerHandler;
    }

    mStreamOutVector.removeItem(pAudioALSAStreamOut->getIdentity());
    delete pAudioALSAStreamOut;

#ifdef MTK_HDMI_FORCE_AUDIO_CLK
    if ((useCase == AudioALSAStreamOut::OUT_HDMI_STEREO_MIXED ||
         useCase == AudioALSAStreamOut::OUT_HDMI_DIRECT_OUT ||
         useCase == AudioALSAStreamOut::OUT_HDMI_RAW_DIRECT_OUT) &&
         !isHdmiStreamOutExist()) {
         AudioALSAPlaybackHandlerHDMI::forceClkOff();
    }
#endif

    ALOGV("-%s(), mStreamOutVector.size() = %zu", __FUNCTION__, mStreamOutVector.size());
}

android_audio_legacy::AudioStreamIn *AudioALSAStreamManager::openInputStream(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    android_audio_legacy::AudioSystem::audio_in_acoustics acoustics,
    uint32_t input_flag)
{
    ALOGV("+%s()", __FUNCTION__);
    AudioAutoTimeoutLock streamVectorAutoTimeoutLock(mStreamVectorLock);
    AudioAutoTimeoutLock _l(mLock);

    if (format == NULL || channels == NULL || sampleRate == NULL || status == NULL)
    {
        ALOGE("%s(), NULL pointer!! format = %p, channels = %p, sampleRate = %p, status = %p",
              __FUNCTION__, format, channels, sampleRate, status);
        if (status != NULL) { *status = INVALID_OPERATION; }
        return NULL;
    }

    int input_handle = *status;

    if (mStreamInVector.indexOfKey(input_handle) >= 0) {
        ALOGE("-%s() handle(%d) already existed", __FUNCTION__, input_handle);
        return NULL;
    }

    // create stream in
    AudioALSAStreamIn *pAudioALSAStreamIn = new AudioALSAStreamIn();
#ifdef UPLINK_LOW_LATENCY
    pAudioALSAStreamIn->set(devices, format, channels, sampleRate, status, acoustics, input_flag);
#else
    pAudioALSAStreamIn->set(devices, format, channels, sampleRate, status, acoustics);
#endif
    if (*status != NO_ERROR)
    {
        ALOGW("-%s() set fail, prefer format = 0x%x channels = 0x%x sampleRate = %d",
              __FUNCTION__, *format, *channels, *sampleRate);
        delete pAudioALSAStreamIn;
        pAudioALSAStreamIn = NULL;
        return NULL;
    }

    // save stream in object in vector
    pAudioALSAStreamIn->setIdentity(input_handle);
    mStreamInVector.add(input_handle, pAudioALSAStreamIn);

    ALOGV("-%s(), in = %p, status = 0x%x, mStreamInVector.size() = %zu",
          __FUNCTION__, pAudioALSAStreamIn, *status, mStreamInVector.size());
    return pAudioALSAStreamIn;
}

void AudioALSAStreamManager::closeInputStream(android_audio_legacy::AudioStreamIn *in)
{
    ALOGV("+%s(), in = %p", __FUNCTION__, in);
    AudioAutoTimeoutLock streamVectorAutoTimeoutLock(mStreamVectorLock);
    AudioAutoTimeoutLock _l(mLock);

    if (in == NULL)
    {
        ALOGW("%s(), Cannot close null input stream!! return", __FUNCTION__);
        return;
    }

    AudioALSAStreamIn *pAudioALSAStreamIn = static_cast<AudioALSAStreamIn *>(in);
    ASSERT(pAudioALSAStreamIn != 0);

    mStreamInVector.removeItem(pAudioALSAStreamIn->getIdentity());
    delete pAudioALSAStreamIn;

    ALOGV("-%s(), mStreamInVector.size() = %zu", __FUNCTION__, mStreamInVector.size());
}

AudioALSAPlaybackHandlerBase *AudioALSAStreamManager::createPlaybackHandler(
    stream_attribute_t *stream_attribute_source)
{
    ALOGV("+%s(), mAudioMode = %d, output_devices = 0x%x", __FUNCTION__, mAudioMode, stream_attribute_source->output_devices);

    // Init input stream attribute here
    stream_attribute_source->audio_mode = mAudioMode; // set mode to stream attribute for mic gain setting

    //for DMNR tuning
    stream_attribute_source->BesRecord_Info.besrecord_dmnr_tuningEnable = mAudioSpeechEnhanceInfoInstance->IsAPDMNRTuningEnable();
    stream_attribute_source->bBypassPostProcessDL = mBypassPostProcessDL;
    // create
    AudioALSAPlaybackHandlerBase *pPlaybackHandler = NULL;
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    if (isModeInPhoneCall() == true)
    {
        pPlaybackHandler = new AudioALSAPlaybackHandlerVoice(stream_attribute_source);
    }
    else
#endif
    {
        switch (stream_attribute_source->output_devices)
        {
            case AUDIO_DEVICE_OUT_EARPIECE:
            case AUDIO_DEVICE_OUT_SPEAKER:
            case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            case (AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_SPEAKER):
            case (AUDIO_DEVICE_OUT_WIRED_HEADPHONE|AUDIO_DEVICE_OUT_SPEAKER):
            {
#ifdef DOWNLINK_LOW_LATENCY
                if(AUDIO_OUTPUT_FLAG_FAST & stream_attribute_source->mAudioOutputFlags)
                    pPlaybackHandler = new AudioALSAPlaybackHandlerFast(stream_attribute_source);
#endif
#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
                if (!pPlaybackHandler && !(AUDIO_OUTPUT_FLAG_PRIMARY & stream_attribute_source->mAudioOutputFlags) &&
                    (stream_attribute_source->sample_rate == 192000))
                    pPlaybackHandler = new AudioALSAPlaybackHandlerFast(stream_attribute_source);
#endif
                if (!pPlaybackHandler)
                    pPlaybackHandler = new AudioALSAPlaybackHandlerNormal(stream_attribute_source);
                break;
            }
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            {
                pPlaybackHandler = new AudioALSAPlaybackHandlerBTSCO(stream_attribute_source);
                break;
            }
            case AUDIO_DEVICE_OUT_AUX_DIGITAL:
            {
                if (!audio_is_linear_pcm(stream_attribute_source->audio_format))
                    pPlaybackHandler = new AudioALSAPlaybackHandlerHDMIRaw(stream_attribute_source);
                else
                    pPlaybackHandler = new AudioALSAPlaybackHandlerHDMI(stream_attribute_source);
                break;
            }
            default:
            {
                ALOGW("%s(), No implement for such output_devices(0x%x)", __FUNCTION__, stream_attribute_source->output_devices);
#ifdef DOWNLINK_LOW_LATENCY
                if(AUDIO_OUTPUT_FLAG_FAST & stream_attribute_source->mAudioOutputFlags)
                    pPlaybackHandler = new AudioALSAPlaybackHandlerFast(stream_attribute_source);
                else
                    pPlaybackHandler = new AudioALSAPlaybackHandlerNormal(stream_attribute_source);
#else
                pPlaybackHandler = new AudioALSAPlaybackHandlerNormal(stream_attribute_source);
#endif
                break;
            }
        }

        ssize_t dFltMngindex = mFilterManagerVector.indexOfKey(stream_attribute_source->mStreamOutIndex);
        if (dFltMngindex >=0 && dFltMngindex < (ssize_t)mFilterManagerVector.size())
        {
            ALOGV("%s() ApplyFilter [%zd]/[%zu] Device [0x%x]", __FUNCTION__, dFltMngindex,
                  mFilterManagerVector.size(), stream_attribute_source->output_devices);
            pPlaybackHandler->setFilterMng(static_cast<AudioMTKFilterManager *>(mFilterManagerVector[dFltMngindex]));
            mFilterManagerVector[dFltMngindex]->setDevice(stream_attribute_source->output_devices);
        }
    }

    // save playback handler object in vector
    ASSERT(pPlaybackHandler != NULL);
    pPlaybackHandler->setIdentity(mPlaybackHandlerIndex);
    mPlaybackHandlerVector.add(mPlaybackHandlerIndex, pPlaybackHandler);
    mPlaybackHandlerIndex++;

    ALOGV("-%s(), mPlaybackHandlerVector.size() = %zu", __FUNCTION__, mPlaybackHandlerVector.size());
    return pPlaybackHandler;
}

status_t AudioALSAStreamManager::destroyPlaybackHandler(AudioALSAPlaybackHandlerBase *pPlaybackHandler)
{
    ALOGV("+%s(), mode = %d, pPlaybackHandler = %p", __FUNCTION__, mAudioMode, pPlaybackHandler);

    status_t status = NO_ERROR;

    mPlaybackHandlerVector.removeItem(pPlaybackHandler->getIdentity());
    delete pPlaybackHandler;

    ALOGV("-%s(), mPlaybackHandlerVector.size() = %zu", __FUNCTION__, mPlaybackHandlerVector.size());
    return status;
}

AudioALSACaptureHandlerBase *AudioALSAStreamManager::createCaptureHandler(
    stream_attribute_t *stream_attribute_target)
{
    ALOGV("+%s(), mAudioMode = %d, input_source = %d, input_device = 0x%x",
          __FUNCTION__, mAudioMode, stream_attribute_target->input_source, stream_attribute_target->input_device);
    AudioAutoTimeoutLock _l(mLock);

    // use primary stream out device
    const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                   ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                   : AUDIO_DEVICE_NONE;

    // Init input stream attribute here
    stream_attribute_target->audio_mode = mAudioMode; // set mode to stream attribute for mic gain setting
    stream_attribute_target->output_devices = current_output_devices; // set output devices to stream attribute for mic gain setting and BesRecord parameter
    stream_attribute_target->micmute = mMicMute;

    // BesRecordInfo
    stream_attribute_target->BesRecord_Info.besrecord_enable = false; // default set besrecord off


    // create
    AudioALSACaptureHandlerBase *pCaptureHandler = NULL;

    if (stream_attribute_target->input_source == AUDIO_SOURCE_FM_TUNER)
    {
        pCaptureHandler = new AudioALSACaptureHandlerFMRadio(stream_attribute_target);
    }
    else if (isModeInPhoneCall() == true &&
        (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_UPLINK
         || stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_DOWNLINK
         || stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_CALL))
    {
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        pCaptureHandler = new AudioALSACaptureHandlerVoice(stream_attribute_target);
#else
        pCaptureHandler = new AudioALSACaptureHandlerNormal(stream_attribute_target);
#endif
    }
    else if ((isModeInVoipCall() == true) ||
             (stream_attribute_target->NativePreprocess_Info.PreProcessEffect_AECOn == true) ||
             (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_COMMUNICATION)) //Normal REC with AEC
    {
        stream_attribute_target->BesRecord_Info.besrecord_enable = EnableBesRecord();

        if (isModeInVoipCall() == true || (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_COMMUNICATION))
        {
            stream_attribute_target->BesRecord_Info.besrecord_voip_enable = true;

            if (current_output_devices == AUDIO_DEVICE_OUT_SPEAKER)
            {
                if (stream_attribute_target->input_device == AUDIO_DEVICE_IN_BUILTIN_MIC)
                {
                    if (USE_REFMIC_IN_LOUDSPK == 1)
                    {
                        ALOGI("%s(), routing changed!! input_device: 0x%x => 0x%x",
                              __FUNCTION__, stream_attribute_target->input_device, AUDIO_DEVICE_IN_BACK_MIC);
                        stream_attribute_target->input_device = AUDIO_DEVICE_IN_BACK_MIC;
                    }
                }
            }
        }

        switch (stream_attribute_target->input_device)
        {
            case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            {
#if 0   //not enable BT AEC
                ALOGD("%s(), BT still use nonAEC handle for temp", __FUNCTION__);
                stream_attribute_target->BesRecord_Info.besrecord_voip_enable = false;
                pCaptureHandler = new AudioALSACaptureHandlerBTSCO(stream_attribute_target);
#else   //enable BT AEC
                ALOGV("%s(), BT use AEC handle", __FUNCTION__);
                pCaptureHandler = new AudioALSACaptureHandlerAEC(stream_attribute_target);
#endif
                break;
            }
            default:
            {
                pCaptureHandler = new AudioALSACaptureHandlerAEC(stream_attribute_target);
                break;
            }
        }
    }
    else
    {
        //no uplink preprocess for sample rate higher than 48k
        if ((stream_attribute_target->sample_rate > 48000) ||
            (stream_attribute_target->audio_format != AUDIO_FORMAT_PCM_16_BIT))
                stream_attribute_target->BesRecord_Info.besrecord_enable = false;
            else
                stream_attribute_target->BesRecord_Info.besrecord_enable = EnableBesRecord();

        switch (stream_attribute_target->input_device)
        {
            case AUDIO_DEVICE_IN_BUILTIN_MIC:
            case AUDIO_DEVICE_IN_BACK_MIC:
            case AUDIO_DEVICE_IN_WIRED_HEADSET:
            {
                pCaptureHandler = new AudioALSACaptureHandlerNormal(stream_attribute_target);
                break;
            }
            case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            {
                pCaptureHandler = new AudioALSACaptureHandlerBTSCO(stream_attribute_target);
                break;
            }
            default:
            {
                ALOGW("%s(), No implement for such input_device(0x%x)", __FUNCTION__, stream_attribute_target->input_device);
                pCaptureHandler = new AudioALSACaptureHandlerNormal(stream_attribute_target);
                break;
            }
        }
    }

    // save capture handler object in vector
    ASSERT(pCaptureHandler != NULL);
    pCaptureHandler->setIdentity(mCaptureHandlerIndex);
    mCaptureHandlerVector.add(mCaptureHandlerIndex, pCaptureHandler);
    mCaptureHandlerIndex++;

    ALOGV("-%s(), mCaptureHandlerVector.size() = %zu", __FUNCTION__, mCaptureHandlerVector.size());
    return pCaptureHandler;
}

status_t AudioALSAStreamManager::destroyCaptureHandler(AudioALSACaptureHandlerBase *pCaptureHandler)
{
    ALOGV("+%s(), mode = %d, pCaptureHandler = %p", __FUNCTION__, mAudioMode, pCaptureHandler);
    //AudioAutoTimeoutLock _l(mLock); // setparam -> routing -> close -> destroy deadlock

    status_t status = NO_ERROR;

    mCaptureHandlerVector.removeItem(pCaptureHandler->getIdentity());
    delete pCaptureHandler;

    ALOGV("-%s(), mCaptureHandlerVector.size() = %zu", __FUNCTION__, mCaptureHandlerVector.size());
    return status;
}

status_t AudioALSAStreamManager::setVoiceVolume(float volume)
{
    ALOGD("%s(), volume = %f", __FUNCTION__, volume);

    if (volume < 0.0 || volume > 1.0)
    {
        ALOGE("-%s(), strange volume level %f, something wrong!!", __FUNCTION__, volume);
        return BAD_VALUE;
    }

    AudioAutoTimeoutLock _l(mLock);

    if (mAudioALSAVolumeController)
    {
        // use primary stream out device
        const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                       ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                       : AUDIO_DEVICE_NONE;
        mAudioALSAVolumeController->setVoiceVolume(volume, mAudioMode , current_output_devices);
    }

    return NO_ERROR;
}


status_t AudioALSAStreamManager::setMasterVolume(float volume)
{
    ALOGD("%s(), volume = %f", __FUNCTION__, volume);

    if (volume < 0.0 || volume > 1.0)
    {
        ALOGE("-%s(), strange volume level %f, something wrong!!", __FUNCTION__, volume);
        return BAD_VALUE;
    }

    AudioAutoTimeoutLock _l(mLock);
    if (mAudioALSAVolumeController)
    {
        // use primary stream out device
        const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                       ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                       : AUDIO_DEVICE_OUT_SPEAKER;
        mAudioALSAVolumeController->setMasterVolume(volume, mAudioMode , current_output_devices);
    }

    return NO_ERROR;
}


status_t AudioALSAStreamManager::setFmVolume(float volume)
{
    ALOGV("+%s(), volume = %f", __FUNCTION__, volume);

    if (volume < 0.0 || volume > 1.0)
    {
        ALOGE("-%s(), strange volume level %f, something wrong!!", __FUNCTION__, volume);
        return BAD_VALUE;
    }

    AudioAutoTimeoutLock _l(mLock);
    mFMController->setFmVolume(volume);

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setMicMute(bool state)
{
    ALOGV("%s(), mMicMute: %d => %d", __FUNCTION__, mMicMute, state);
    AudioAutoTimeoutLock _l(mLock);
    if (isModeInPhoneCall() == true)
    {
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        AudioALSASpeechPhoneCallController::getInstance()->setMicMute(state);
#endif
    }
    else
    {
        SetInputMute(state);
    }
    mMicMute = state;
    return NO_ERROR;
}


bool AudioALSAStreamManager::getMicMute()
{
    ALOGV("%s(), mMicMute = %d", __FUNCTION__, mMicMute);
    AudioAutoTimeoutLock _l(mLock);

    return mMicMute;
}

void AudioALSAStreamManager::SetInputMute(bool bEnable)
{
    ALOGV("+%s(), %d", __FUNCTION__, bEnable);
    if (mStreamInVector.size() > 0)
    {
        for (size_t i = 0; i < mStreamInVector.size(); i++)
        {
            mStreamInVector[i]->SetInputMute(bEnable);
        }
    }
    ALOGV("-%s(), %d", __FUNCTION__, bEnable);
}

status_t AudioALSAStreamManager::setVtNeedOn(const bool vt_on)
{
    ALOGV("%s(), setVtNeedOn: %d", __FUNCTION__, vt_on);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    AudioALSASpeechPhoneCallController::getInstance()->setVtNeedOn(vt_on);
#endif
    return NO_ERROR;
}

status_t AudioALSAStreamManager::setMode(audio_mode_t new_mode)
{
    ALOGD("+%s(), mAudioMode: %d => %d", __FUNCTION__, mAudioMode, new_mode);
    bool isNeedResumeAllStreams = false;
    AudioAutoTimeoutLock streamVectorAutoTimeoutLock(mStreamVectorLock);

    // check value
    if ((new_mode < AUDIO_MODE_NORMAL) || (new_mode > AUDIO_MODE_MAX))
    {
        return BAD_VALUE;
    }

    if (new_mode == mAudioMode)
    {
        ALOGW("-%s(), mAudioMode: %d == %d, return", __FUNCTION__, mAudioMode, new_mode);
        return NO_ERROR;
    }

    // suspend and standby if needed
    if (isModeInPhoneCall(new_mode) == true || isModeInPhoneCall(mAudioMode) == true ||
        isModeInVoipCall(new_mode)  == true || isModeInVoipCall(mAudioMode) == true)
    {
        setAllStreamsSuspend(true, true);
        standbyAllStreams(true);
    }

    // close FM when mode swiching
    if (mFMController->getFmEnable() &&
        (isModeInPhoneCall(new_mode) || isModeInPhoneCall(mAudioMode)))
    {
        setFmEnable(false);
    }

    {
        AudioAutoTimeoutLock _l(mLock);

        // use primary stream out device
        const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                       ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                       : AUDIO_DEVICE_NONE;

#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        // close previous call if needed
        if (isModeInPhoneCall(mAudioMode) == true)
        {
            mSpeechPhoneCallController->close();
        }

        // open next call if needed
        if (isModeInPhoneCall(new_mode) == true)
        {
            mPhoneCallSpeechOpen = true;
        }
#endif

        // resume if needed
        if (isModeInPhoneCall(new_mode) == true || isModeInPhoneCall(mAudioMode) == true ||
            isModeInVoipCall(new_mode)  == true || isModeInVoipCall(mAudioMode) == true)
        {
            isNeedResumeAllStreams = true;
        }

        mAudioMode = new_mode;

        if (isModeInPhoneCall() == true)
        {
            mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(), mAudioMode , current_output_devices);
        }
        else
        {
            mAudioALSAVolumeController->setMasterVolume(mAudioALSAVolumeController->getMasterVolume(), mAudioMode , current_output_devices);
        }
    }

    if (isNeedResumeAllStreams == true)
    {
        setAllStreamsSuspend(false, true);
    }

    ALOGD("-%s(), mAudioMode = %d", __FUNCTION__, mAudioMode);
    return NO_ERROR;
}


status_t AudioALSAStreamManager::routingOutputDevice(AudioALSAStreamOut *pAudioALSAStreamOut,
    const audio_devices_t current_output_devices, audio_devices_t output_devices)
{
    status_t status = NO_ERROR;
    audio_devices_t streamOutDevice = pAudioALSAStreamOut->getStreamAttribute()->output_devices;

    ALOGV("+%s(), output_devices: 0x%x => 0x%x", __FUNCTION__, streamOutDevice, output_devices);

    // set original routing device to TTY
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    mSpeechPhoneCallController->setRoutingForTty((audio_devices_t)output_devices);
#endif

    // update if headset change
    mHeadsetChange= CheckHeadsetChange(current_output_devices, output_devices);
    if(mHeadsetChange == true && (mFMController->getFmEnable() == false))
    {
        AudioALSAHardwareResourceManager::getInstance()->setHeadPhoneChange(mHeadsetChange);
    }

    // When FM + (WFD, A2DP, SCO(44.1K -> 8/16K), ...), Policy will routing to AUDIO_DEVICE_NONE
    // Hence, use other device like AUDIO_DEVICE_OUT_REMOTE_SUBMIX instead to achieve FM routing.
    if (output_devices == AUDIO_DEVICE_NONE && mFMController->getFmEnable() == true)
    {
        ALOGD("%s(), Replace AUDIO_DEVICE_NONE with AUDIO_DEVICE_OUT_REMOTE_SUBMIX for AP-path FM routing", __FUNCTION__);
        output_devices = AUDIO_DEVICE_OUT_REMOTE_SUBMIX;
    }

    if (output_devices == AUDIO_DEVICE_NONE)
    {
        ALOGW("-%s(), output_devices == AUDIO_DEVICE_NONE(0x%x), return", __FUNCTION__, AUDIO_DEVICE_NONE);
        return NO_ERROR;
    }
    else if (output_devices == streamOutDevice)
    {
        if(isModeInPhoneCall() == true)
        {
            ALOGW("-%s(), output_devices == current_output_devices(0x%x), but phone call is enabled", __FUNCTION__, current_output_devices);
        }
        else
        {
            ALOGW("-%s(), output_devices == current_output_devices(0x%x), return", __FUNCTION__, current_output_devices);
            return NO_ERROR;
        }
    }

        // do routing
    if (isModeInPhoneCall())
    {
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        if (mPhoneCallSpeechOpen == true)
        {
            setAllStreamsSuspend(true, true);
            standbyAllStreams(true);
            mSpeechPhoneCallController->open(
                mAudioMode,
                output_devices,
                mSpeechPhoneCallController->getInputDeviceForPhoneCall(current_output_devices));
        }

        mSpeechPhoneCallController->routing(
            output_devices,
            mSpeechPhoneCallController->getInputDeviceForPhoneCall(output_devices));

        if (mPhoneCallSpeechOpen == true)
        {
            mPhoneCallSpeechOpen = false;
            setAllStreamsSuspend(false, true);
        }
#endif
    }

    Vector<AudioALSAStreamOut *> streamOutToRoute;
    AudioALSAHardwareResourceManager *hwResMng = AudioALSAHardwareResourceManager::getInstance();
    bool toSharedOut = hwResMng->isSharedOutDevice(output_devices);

    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        audio_devices_t curOutDevice = mStreamOutVector[i]->getStreamAttribute()->output_devices;
        bool curSharedOut = hwResMng->isSharedOutDevice(curOutDevice);

        // check if need routing
        if (curOutDevice != output_devices &&
            (pAudioALSAStreamOut == mStreamOutVector[i] ||   // route ourself
            (toSharedOut && curSharedOut))) // route shared output device streamout
        {
            // suspend streamout
            mStreamOutVector[i]->setSuspend(true);
            streamOutToRoute.add(mStreamOutVector[i]);
        }
    }

    for (size_t i = 0; i < streamOutToRoute.size(); i++)
    {
        // stream out routing
        status = streamOutToRoute[i]->routing(output_devices);
        ASSERT(status == NO_ERROR);
    }

    if (!isModeInPhoneCall() && mFMController->getFmEnable() == true  && (output_devices != streamOutDevice))
    {
        if(android_audio_legacy::AudioSystem::popCount(streamOutDevice)!= android_audio_legacy::AudioSystem::popCount(output_devices))
            mFMController->routing(streamOutDevice, output_devices);//switch between SPK+HP and HP (ringtone)
        else
            mFMController->setFmEnable(false,output_devices,false,false,true);
    }

    for (size_t i = 0; i < streamOutToRoute.size(); i++)
    {
        // resume streamout
        streamOutToRoute[i]->setSuspend(false);
    }

    if (!isModeInPhoneCall() && mStreamInVector.size() > 0)
    {
        // update the output device info for input stream (ex:for BesRecord parameters update or mic device change)
        ALOGD("%s(), mStreamInVector.size() = %zu", __FUNCTION__, mStreamInVector.size());
        for (size_t i = 0; i < mStreamInVector.size(); i++)
        {
            status = mStreamInVector[i]->updateOutputDeviceInfoForInputStream(output_devices);
            ASSERT(status == NO_ERROR);
        }
    }

    if (isModeInPhoneCall() == true)
    {
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        mSpeechPhoneCallController->setMicMute(1);
#endif
        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(), mAudioMode , output_devices);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        usleep(200*1000);
        mSpeechPhoneCallController->setMicMute(mMicMute);
#endif
    }
    else
    {
        mAudioALSAVolumeController->setMasterVolume(mAudioALSAVolumeController->getMasterVolume(), mAudioMode , output_devices);
    }

    ALOGV("-%s(), output_devices = 0x%x", __FUNCTION__, output_devices);
    return status;
}


status_t AudioALSAStreamManager::routingInputDevice(AudioALSAStreamIn *pAudioALSAStreamIn, const audio_devices_t current_input_device, audio_devices_t input_device)
{
    ALOGD("+%s(), input_device: 0x%x => 0x%x", __FUNCTION__, current_input_device, input_device);
    AudioAutoTimeoutLock _l(mLock);

    status_t status = NO_ERROR;

    if (input_device == AUDIO_DEVICE_NONE)
    {
        ALOGW("-%s(), input_device == AUDIO_DEVICE_NONE(0x%x), return", __FUNCTION__, AUDIO_DEVICE_NONE);
        return NO_ERROR;
    }
    else if (input_device == current_input_device)
    {
        ALOGW("-%s(), input_device == current_input_device(0x%x), return", __FUNCTION__, current_input_device);
        return NO_ERROR;
    }


    if (isModeInPhoneCall() == true)
    {
        ALOGW("-%s(), not route during phone call, return", __FUNCTION__);
        return INVALID_OPERATION;
    }
    else if (mStreamInVector.size() > 0)
    {
        for (size_t i = 0; i < mStreamInVector.size(); i++)
        {
            if (mStreamInVector[i]->getStreamAttribute()->input_device == current_input_device)
            {
                if((input_device == AUDIO_DEVICE_IN_FM_TUNER)|| (current_input_device == AUDIO_DEVICE_IN_FM_TUNER))
                {
                    if(pAudioALSAStreamIn == mStreamInVector[i])
                    {
                        status = mStreamInVector[i]->routing(input_device);
                        ASSERT(status == NO_ERROR);
                    }
                }
                else
                {
                        status = mStreamInVector[i]->routing(input_device);
                        ASSERT(status == NO_ERROR);
                }
            }
        }
    }
    return status;
}

// check if headset has changed
bool AudioALSAStreamManager::CheckHeadsetChange(const audio_devices_t current_output_devices, audio_devices_t output_device)
{
    ALOGV("+%s(), current_output_devices = %d output_device = %d ", __FUNCTION__, current_output_devices,output_device);
    if(current_output_devices == output_device)
    {
        return false;
    }
    if(current_output_devices == AUDIO_DEVICE_NONE || output_device == AUDIO_DEVICE_NONE)
    {
        return true;
    }
    if(current_output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET || current_output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE
       ||output_device == AUDIO_DEVICE_OUT_WIRED_HEADSET || output_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE )
    {
        return true;
    }
    return false;
}

status_t AudioALSAStreamManager::setFmEnable(const bool enable, bool force_control, bool force_direct_conn)
{
    ALOGV("+%s() enable = %d", __FUNCTION__, enable);
    AudioAutoTimeoutLock _l(mLock);

    // Reject set fm enable during phone call mode
    if (isModeInPhoneCall(mAudioMode))
    {
        ALOGW("-%s(), mAudioMode(%d) is in phone call mode, return.", __FUNCTION__, mAudioMode);
        return INVALID_OPERATION;
    }

    const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                   ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                   : AUDIO_DEVICE_NONE;

    mFMController->setFmEnable(enable, current_output_devices, force_control, force_direct_conn);


    ALOGV("-%s() enable = %d", __FUNCTION__, enable);
    return NO_ERROR;
}


bool AudioALSAStreamManager::getFmEnable()
{
    AudioAutoTimeoutLock _l(mLock);
    return mFMController->getFmEnable();
}

status_t AudioALSAStreamManager::setAllOutputStreamsSuspend(const bool suspend_on, const bool setModeRequest __unused)
{
    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        ASSERT(mStreamOutVector[i]->setSuspend(suspend_on) == NO_ERROR);
    }

    return NO_ERROR;
}


status_t AudioALSAStreamManager::setAllInputStreamsSuspend(const bool suspend_on, const bool setModeRequest)
{
    ALOGV("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    AudioALSAStreamIn *pAudioALSAStreamIn = NULL;

    for (size_t i = 0; i < mStreamInVector.size(); i++)
    {
        pAudioALSAStreamIn = mStreamInVector[i];
        if (setModeRequest == true)
        {
            if (pAudioALSAStreamIn->isSupportConcurrencyInCall())
            {
                ALOGD("%s(), mStreamInVector[%zu] support concurrency!!", __FUNCTION__, i);
                continue;
            }
        }
        status = pAudioALSAStreamIn->setSuspend(suspend_on);
        if (status != NO_ERROR)
        {
            ALOGE("%s(), mStreamInVector[%zu] setSuspend() fail!!", __FUNCTION__, i);
        }
    }

    return status;
}


status_t AudioALSAStreamManager::setAllStreamsSuspend(const bool suspend_on, const bool setModeRequest)
{
    ALOGV("%s(), suspend_on = %d", __FUNCTION__, suspend_on);

    status_t status = NO_ERROR;

    status = setAllOutputStreamsSuspend(suspend_on, setModeRequest);
    status = setAllInputStreamsSuspend(suspend_on, setModeRequest);

    return status;
}


status_t AudioALSAStreamManager::standbyAllOutputStreams(const bool setModeRequest __unused)
{
    ALOGV("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        pAudioALSAStreamOut = mStreamOutVector[i];
        status = pAudioALSAStreamOut->internalStandby();
        if (status != NO_ERROR)
        {
            ALOGE("%s(), mStreamOutVector[%zu] standby() fail!!", __FUNCTION__, i);
        }
    }

    return status;
}


status_t AudioALSAStreamManager::standbyAllInputStreams(const bool setModeRequest)
{
    ALOGV("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    AudioALSAStreamIn *pAudioALSAStreamIn = NULL;

    for (size_t i = 0; i < mStreamInVector.size(); i++)
    {
        pAudioALSAStreamIn = mStreamInVector[i];
        if (setModeRequest == true)
        {
            if (pAudioALSAStreamIn->isSupportConcurrencyInCall())
            {
                ALOGD("%s(), mStreamInVector[%zu] support concurrency!!", __FUNCTION__, i);
                continue;
            }
        }
        status = pAudioALSAStreamIn->standby();
        if (status != NO_ERROR)
        {
            ALOGE("%s(), mStreamInVector[%zu] standby() fail!!", __FUNCTION__, i);
        }
    }

    return status;
}


status_t AudioALSAStreamManager::standbyAllStreams(const bool setModeRequest)
{
    ALOGV("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    status = standbyAllOutputStreams(setModeRequest);
    status = standbyAllInputStreams(setModeRequest);

    return status;
}

size_t AudioALSAStreamManager::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    return AudioALSAStreamIn::getInputBufferSize(sampleRate, format, channelCount);
}


// set musicplus to streamout
status_t AudioALSAStreamManager::SetMusicPlusStatus(bool bEnable)
{
    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setParamFixed(bEnable ? true : false);
    }

    return NO_ERROR;
}

bool AudioALSAStreamManager::GetMusicPlusStatus()
{
    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        bool musicplus_status = pTempFilter->isParamFixed();
        if (musicplus_status)
        {
            return true;
        }
    }

    return false;
}

status_t AudioALSAStreamManager::SetBesLoudnessStatus(bool bEnable)
{
#ifdef MTK_BESLOUDNESS_SUPPORT
    ALOGD("mBesLoudnessStatus() flag %d", bEnable);
    mBesLoudnessStatus = bEnable;
    AUDIO_AUDENH_CONTROL_OPTION_STRUCT audioParam;
    audioParam.u32EnableFlg = bEnable ? 1 : 0;
    SetBesLoudnessControlOptionParamToNV(&audioParam);
    if (mBesLoudnessControlCallback != NULL)
    {
        mBesLoudnessControlCallback((void *)mBesLoudnessStatus);
    }
#else
    ALOGD("Unsupport set mBesLoudnessStatus()");
#endif
    return NO_ERROR;
}

bool AudioALSAStreamManager::GetBesLoudnessStatus()
{
    return mBesLoudnessStatus;
}

status_t AudioALSAStreamManager::SetBesLoudnessControlCallback(const BESLOUDNESS_CONTROL_CALLBACK_STRUCT *callback_data)
{
    if (callback_data == NULL)
    {
        mBesLoudnessControlCallback = NULL;
    }
    else
    {
        mBesLoudnessControlCallback = callback_data->callback;
        ASSERT(mBesLoudnessControlCallback != NULL);
        mBesLoudnessControlCallback((void *)mBesLoudnessStatus);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateACFHCF(int value)
{
    ALOGD("%s()", __FUNCTION__);

    AUDIO_ACF_CUSTOM_PARAM_STRUCT sACFHCFParam;

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        if (value == 0)
        {
            ALOGD("setParameters Update ACF Parames");
            getAudioCompFltCustParam(AUDIO_COMP_FLT_AUDIO, &sACFHCFParam);
            pTempFilter->setParameter(AUDIO_COMP_FLT_AUDIO, &sACFHCFParam);

        }
        else if (value == 1)
        {
            ALOGD("setParameters Update HCF Parames");
            getAudioCompFltCustParam(AUDIO_COMP_FLT_HEADPHONE, &sACFHCFParam);
            pTempFilter->setParameter(AUDIO_COMP_FLT_HEADPHONE, &sACFHCFParam);

        }
        else if (value == 2)
        {
            ALOGD("setParameters Update ACFSub Parames");
            getAudioCompFltCustParam(AUDIO_COMP_FLT_AUDIO_SUB, &sACFHCFParam);
            pTempFilter->setParameter(AUDIO_COMP_FLT_AUDIO_SUB, &sACFHCFParam);

        }
    }
    return NO_ERROR;
}

// ACF Preview parameter
status_t AudioALSAStreamManager::SetACFPreviewParameter(void *ptr , int len)
{
    ALOGD("%s()", __FUNCTION__);
    (void)len;

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setParameter(AUDIO_COMP_FLT_AUDIO, (AUDIO_ACF_CUSTOM_PARAM_STRUCT *)ptr);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::SetHCFPreviewParameter(void *ptr , int len)
{
    ALOGD("%s()", __FUNCTION__);
    (void)len;

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setParameter(AUDIO_COMP_FLT_HEADPHONE, (AUDIO_ACF_CUSTOM_PARAM_STRUCT *)ptr);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setSpkOutputGain(int32_t gain, uint32_t ramp_sample_cnt)
{
    ALOGD("%s(), gain = %d, ramp_sample_cnt = %u", __FUNCTION__, gain, ramp_sample_cnt);

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setSpkOutputGain(gain, ramp_sample_cnt);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setSpkFilterParam(uint32_t fc, uint32_t bw, int32_t th)
{
    ALOGD("%s(), fc %d, bw %d, th %d", __FUNCTION__, fc, bw, th);

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++)
    {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setSpkFilterParam(fc, bw, th);
    }

    return NO_ERROR;
}
status_t AudioALSAStreamManager::SetSpeechVmEnable(const int Type_VM)
{
    ALOGD("%s(), Type_VM=%d, only Normal VM", __FUNCTION__, Type_VM);

    AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
    GetNBSpeechParamFromNVRam(&eSphParamNB);
    if (Type_VM == 0) // normal VM
    {
        eSphParamNB.debug_info[0] = 0;
    }
    else // EPL
    {
        eSphParamNB.debug_info[0] = 3;
        if (eSphParamNB.speech_common_para[0] == 0) // if not assign EPL debug type yet, set a default one
        {
            eSphParamNB.speech_common_para[0] = 6;
        }
    }

    SetNBSpeechParamToNVRam(&eSphParamNB);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(&eSphParamNB);
#endif
    return NO_ERROR;
}

status_t AudioALSAStreamManager::SetEMParameter(AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB)
{
    ALOGD("%s()", __FUNCTION__);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    SetNBSpeechParamToNVRam(pSphParamNB);
    SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(pSphParamNB);
    // Speech Enhancement, VM, Speech Driver
    // update VM/EPL/TTY record capability & enable if needed
    SpeechVMRecorder::GetInstance()->SetVMRecordCapability(pSphParamNB);
#else
    (void)pSphParamNB;
#endif
    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateSpeechParams(const int speech_band)
{
    ALOGD("%s()", __FUNCTION__);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    if (speech_band == 0)
    {
        AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
        GetNBSpeechParamFromNVRam(&eSphParamNB);
        SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(&eSphParamNB);
    }
#if defined(MTK_WB_SPEECH_SUPPORT)
    else if (speech_band == 1)
    {
        AUDIO_CUSTOM_WB_PARAM_STRUCT eSphParamWB;
        GetWBSpeechParamFromNVRam(&eSphParamWB);
        SpeechEnhancementController::GetInstance()->SetWBSpeechParametersToAllModem(&eSphParamWB);
    }
#endif
    if (isModeInPhoneCall() == true) // get output device for in_call, and set speech mode
    {
        UpdateSpeechMode();
    }
#else
    (void)speech_band;
#endif
    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateDualMicParams()
{
    ALOGD("%s()", __FUNCTION__);
    AUDIO_CUSTOM_EXTRA_PARAM_STRUCT eSphParamDualMic;
    GetDualMicSpeechParamFromNVRam(&eSphParamDualMic);
#if defined(MTK_DUAL_MIC_SUPPORT) && defined(MTK_AUDIO_PHONE_CALL_SUPPORT)
    SpeechEnhancementController::GetInstance()->SetDualMicSpeechParametersToAllModem(&eSphParamDualMic);
#endif

    if (isModeInPhoneCall() == true) // get output device for in_call, and set speech mode
    {
        UpdateSpeechMode();
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateSpeechMode()
{
    ALOGD("%s()", __FUNCTION__);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    const audio_devices_t output_device = (audio_devices_t)AudioALSAHardwareResourceManager::getInstance()->getOutputDevice();
    const audio_devices_t input_device  = (audio_devices_t)AudioALSAHardwareResourceManager::getInstance()->getInputDevice();
    mSpeechDriverFactory->GetSpeechDriver()->SetSpeechMode(input_device, output_device);
#endif

    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateSpeechVolume()
{
    ALOGD("%s()", __FUNCTION__);
    mAudioALSAVolumeController->initVolumeController();

    if (isModeInPhoneCall() == true)
    {
        int32_t outputDevice = (audio_devices_t)AudioALSAHardwareResourceManager::getInstance()->getOutputDevice();
#ifndef MTK_AUDIO_GAIN_TABLE
        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(), mAudioMode, (uint32)outputDevice);
#endif
        switch (outputDevice)
        {
            case AUDIO_DEVICE_OUT_WIRED_HEADSET :
            {
#ifdef  MTK_TTY_SUPPORT
                AudioALSASpeechPhoneCallController *pSpeechPhoneCallController = AudioALSASpeechPhoneCallController::getInstance();
                if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_VCO)
                {
                    mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
                }
                else if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_HCO || pSpeechPhoneCallController->getTtyMode() == AUD_TTY_FULL)
                {
                    mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
                }
                else
                {
                    mAudioALSAVolumeController->ApplyMicGain(Headset_Mic, mAudioMode);
                }
#else
                mAudioALSAVolumeController->ApplyMicGain(Headset_Mic, mAudioMode);
#endif
                break;
            }
            case AUDIO_DEVICE_OUT_WIRED_HEADPHONE :
            {
#ifdef  MTK_TTY_SUPPORT
                AudioALSASpeechPhoneCallController *pSpeechPhoneCallController = AudioALSASpeechPhoneCallController::getInstance();
                if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_VCO)
                {
                    mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
                }
                else if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_HCO || pSpeechPhoneCallController->getTtyMode() == AUD_TTY_FULL)
                {
                    mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
                }
                else
                {
                    mAudioALSAVolumeController->ApplyMicGain(Handfree_Mic, mAudioMode);
                }
#else
                mAudioALSAVolumeController->ApplyMicGain(Handfree_Mic, mAudioMode);
#endif
                break;
            }
            case AUDIO_DEVICE_OUT_SPEAKER:
            {
                mAudioALSAVolumeController->ApplyMicGain(Handfree_Mic, mAudioMode);
                break;
            }
            case AUDIO_DEVICE_OUT_EARPIECE:
            {
                mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else
    {
        setMasterVolume(mAudioALSAVolumeController->getMasterVolume());
    }
    return NO_ERROR;

}

status_t AudioALSAStreamManager::SetVCEEnable(bool bEnable)
{
    ALOGD("%s()", __FUNCTION__);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPOR
    SpeechEnhancementController::GetInstance()->SetDynamicMaskOnToAllModem(SPH_ENH_DYNAMIC_MASK_VCE, bEnable);
#else
    (void)bEnable;
#endif
    return NO_ERROR;

}

status_t AudioALSAStreamManager::SetMagiConCallEnable(bool bEnable)
{
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    // enable/disable flag
    SpeechEnhancementController::GetInstance()->SetMagicConferenceCallOn(bEnable);
    if (isModeInPhoneCall() == true) // get output device for in_call, and set speech mode
    {
        UpdateSpeechMode();
    }
#endif
    return NO_ERROR;

}

bool AudioALSAStreamManager::GetMagiConCallEnable(void)
{
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    bool bEnable = SpeechEnhancementController::GetInstance()->GetMagicConferenceCallOn();
    ALOGD("-%s(), bEnable=%d", __FUNCTION__, bEnable);

    return bEnable;
#else
    return false;
#endif
}

status_t AudioALSAStreamManager::SetVMLogConfig(unsigned short mVMConfig)
{
    ALOGD("+%s(), mVMConfig=%d", __FUNCTION__, mVMConfig);

#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    if (GetVMLogConfig() == mVMConfig)
    {
        ALOGD("%s(), mVMConfig(%d) the same, retrun directly.", __FUNCTION__, mVMConfig);
        return NO_ERROR;
    }
    // Speech Enhancement, VM, Speech Driver
    // update VM/EPL/TTY record capability & enable if needed
    SpeechVMRecorder::GetInstance()->SetVMRecordCapability(mVMConfig);
#endif

    return NO_ERROR;

}

unsigned short AudioALSAStreamManager::GetVMLogConfig(void)
{
    unsigned short mVMConfig;

    AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT eParaAudioFuncSwitch;
    GetAudioFuncSwitchParamFromNV(&eParaAudioFuncSwitch);
    mVMConfig = eParaAudioFuncSwitch.vmlog_dump_config;

    ALOGD("-%s(), mVMConfig=%d", __FUNCTION__, mVMConfig);

    return mVMConfig;
}

status_t AudioALSAStreamManager::SetCustXmlEnable(unsigned short enable)
{
    ALOGD("+%s(), enable = %d", __FUNCTION__, enable);

#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT eParaAudioFuncSwitch;
    GetAudioFuncSwitchParamFromNV(&eParaAudioFuncSwitch);
    if (eParaAudioFuncSwitch.cust_xml_enable == enable)
    {
        ALOGD("%s(), enable(%d) the same, retrun directly.", __FUNCTION__, enable);
        return NO_ERROR;
    } else {
        eParaAudioFuncSwitch.cust_xml_enable = enable;
        SetAudioFuncSwitchParamToNV(&eParaAudioFuncSwitch);
        ALOGD("%s(), set CustXmlEnabl = %d\n", __FUNCTION__, enable);
    }
#endif

    return NO_ERROR;
}

unsigned short AudioALSAStreamManager::GetCustXmlEnable(void)
{
    unsigned short enable;

    AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT eParaAudioFuncSwitch;
    GetAudioFuncSwitchParamFromNV(&eParaAudioFuncSwitch);
    enable = eParaAudioFuncSwitch.cust_xml_enable;

    ALOGD("-%s(), enable = %d", __FUNCTION__, enable);

    return enable;
}
status_t AudioALSAStreamManager::Enable_DualMicSettng(sph_enh_dynamic_mask_t sphMask, bool bEnable)
{
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    SpeechEnhancementController::GetInstance()->SetDynamicMaskOnToAllModem(sphMask, bEnable);
#else
    (void)sphMask;
    (void)bEnable;
#endif
    return NO_ERROR;

}

status_t AudioALSAStreamManager::Set_LSPK_DlMNR_Enable(sph_enh_dynamic_mask_t sphMask, bool bEnable)
{
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    Enable_DualMicSettng(sphMask, bEnable);

    if (SpeechEnhancementController::GetInstance()->GetMagicConferenceCallOn() == true &&
        SpeechEnhancementController::GetInstance()->GetDynamicMask(sphMask) == true)
    {
        ALOGE("Cannot open MagicConCall & LoudSpeaker DMNR at the same time!!");
    }
#else
    (void)sphMask;
    (void)bEnable;
#endif
    return NO_ERROR;

}

void AudioALSAStreamManager::UpdateDynamicFunctionMask(void)
{
    ALOGD("+%s()", __FUNCTION__);
    if (mStreamInVector.size() > 0)
    {
        for (size_t i = 0; i < mStreamInVector.size(); i++)
        {
            mStreamInVector[i]->UpdateDynamicFunctionMask();
        }
    }
    ALOGD("-%s()", __FUNCTION__);
}

bool AudioALSAStreamManager::EnableBesRecord(void)
{
    bool bRet = false;
    if ((QueryFeatureSupportInfo()& SUPPORT_HD_RECORD) > 0)
    {
        bRet = true;
    }

    return bRet;
}

status_t AudioALSAStreamManager::SetBtHeadsetNrec(bool bEnable)
{
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    // enable/disable flag
    if (SpeechEnhancementController::GetInstance()->GetBtHeadsetNrecStatus() != bEnable)
    {
        SpeechEnhancementController::GetInstance()->SetBtHeadsetNrecOnToAllModem(bEnable);
    }

#if defined(MTK_AUDIO_BT_NREC_WO_ENH_MODE)
    bool mBtHeadsetNrecSwitchNeed = SpeechEnhancementController::GetInstance()->GetBtNrecSwitchNeed();
    if (isModeInPhoneCall() == true && mBtHeadsetNrecSwitchNeed) // get output device for in_call, and set speech mode
    {
        SpeechEnhancementController::GetInstance()->SetBtNrecSwitchNeed(false);
        UpdateSpeechMode();
    }
#endif

#endif
    return NO_ERROR;
}

bool AudioALSAStreamManager::GetBtHeadsetNrecStatus(void)
{
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    bool bEnable = SpeechEnhancementController::GetInstance()->GetBtHeadsetNrecStatus();
    ALOGD("-%s(), bEnable=%d", __FUNCTION__, bEnable);
    return bEnable;
#else
    return false;
#endif
}

status_t AudioALSAStreamManager::setScreenState(bool mode)
{
    AudioAutoTimeoutLock _l(mLock);
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        pAudioALSAStreamOut = mStreamOutVector[i];
        pAudioALSAStreamOut->setScreenState(mode);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setBypassDLProcess(bool flag)
{
    AudioAutoTimeoutLock _l(mLock);
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    mBypassPostProcessDL = flag;

    return NO_ERROR;
}

void AudioALSAStreamManager::enableStreamLock(void)
{
    mLock.lock();
}

void AudioALSAStreamManager::disableStreamLock(void)
{
    mLock.unlock();
}

void AudioALSAStreamManager::putStreamOutIntoStandy(uint32_t use_case)
{
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;
    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        pAudioALSAStreamOut = mStreamOutVector[i];
        if (pAudioALSAStreamOut->getUseCase() == use_case) {
            pAudioALSAStreamOut->internalStandby();
        }
    }
}

bool AudioALSAStreamManager::isStreamOutExist(uint32_t use_case)
{
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        pAudioALSAStreamOut = mStreamOutVector[i];
        if (pAudioALSAStreamOut->getUseCase() == use_case)
            return true;
    }
    return false;
}

bool AudioALSAStreamManager::isHdmiStreamOutExist(void)
{
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        pAudioALSAStreamOut = mStreamOutVector[i];
        if (pAudioALSAStreamOut->getUseCase() == AudioALSAStreamOut::OUT_HDMI_STEREO_MIXED ||
            pAudioALSAStreamOut->getUseCase() == AudioALSAStreamOut::OUT_HDMI_DIRECT_OUT ||
            pAudioALSAStreamOut->getUseCase() == AudioALSAStreamOut::OUT_HDMI_RAW_DIRECT_OUT) {
            return true;
        }
    }
    return false;
}

bool AudioALSAStreamManager::isStreamOutActive(uint32_t use_case)
{
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++)
    {
        pAudioALSAStreamOut = mStreamOutVector[i];
        if (pAudioALSAStreamOut->getUseCase() == use_case && pAudioALSAStreamOut->isActive())
            return true;
    }
    return false;
}

status_t AudioALSAStreamManager::setParametersToStreamOut(const String8 &keyValuePairs)
{
    if (mStreamOutVector.size() == 0) {
        return INVALID_OPERATION;
    }

    for (size_t i = 0; i < mStreamOutVector.size(); i++) {
        mStreamOutVector[i]->setParameters(keyValuePairs);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setParameters(const String8 &keyValuePairs, int audio_io_handle)
{
    ALOGV("+%s param(%s) handle(%d)", __FUNCTION__, keyValuePairs.string(), audio_io_handle);

    status_t ret = INVALID_OPERATION;

    ssize_t streamIndex = mStreamOutVector.indexOfKey(audio_io_handle);
    if (streamIndex >= 0 && streamIndex < (ssize_t)mStreamOutVector.size()) {
        ret = mStreamOutVector[streamIndex]->setParameters(keyValuePairs);
    } else {
        streamIndex = mStreamInVector.indexOfKey(audio_io_handle);
        if (streamIndex >= 0 && streamIndex < (ssize_t)mStreamInVector.size()) {
            ret = mStreamInVector[streamIndex]->setParameters(keyValuePairs);
        }
    }

    ALOGV("-%s param(%s) handle(%d) ret(%d)", __FUNCTION__, keyValuePairs.string(), audio_io_handle, ret);
    return ret;
}

} // end of namespace android
