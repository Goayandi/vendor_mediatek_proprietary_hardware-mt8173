#ifndef ANDROID_AUDIO_ALSA_STREAM_MANAGER_H
#define ANDROID_AUDIO_ALSA_STREAM_MANAGER_H

#include <utils/threads.h>
#include <utils/KeyedVector.h>

#include <hardware_legacy/AudioMTKHardwareInterface.h>
#include <hardware_legacy/AudioSystemLegacy.h>

#include "AudioType.h"
#include "AudioLock.h"
#include "AudioMTKFilter.h"
#include "AudioPolicyParameters.h"
#include "SpeechType.h"
#include "AudioVolumeInterface.h"


namespace android
{

class AudioALSAStreamOut;
class AudioALSAStreamIn;

class AudioALSAPlaybackHandlerBase;
class AudioALSACaptureHandlerBase;
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
class AudioALSASpeechPhoneCallController;
class SpeechDriverFactory;
#endif

class AudioALSAVoiceWakeUpController;

class AudioSpeechEnhanceInfo;
class AudioALSAFMController;

class AudioALSAStreamManager
{
    public:
        virtual ~AudioALSAStreamManager();
        static AudioALSAStreamManager *getInstance();


        /**
         * open/close ALSA output stream
         */
        android_audio_legacy::AudioStreamOut *openOutputStream(
            uint32_t devices,
            int *format,
            uint32_t *channels,
            uint32_t *sampleRate,
            status_t *status);

        android_audio_legacy::AudioStreamOut *openOutputStreamWithFlags(
            uint32_t devices,
            int *format,
            uint32_t *channels,
            uint32_t *sampleRate,
            status_t *status,
            uint32_t output_flag);

        void closeOutputStream(android_audio_legacy::AudioStreamOut *out);


        /**
         * open/close ALSA input stream
         */
        android_audio_legacy::AudioStreamIn *openInputStream(
            uint32_t devices,
            int *format,
            uint32_t *channels,
            uint32_t *sampleRate,
            status_t *status,
            android_audio_legacy::AudioSystem::audio_in_acoustics acoustics,
            uint32_t input_flag = 0);

        void closeInputStream(android_audio_legacy::AudioStreamIn *in);


        /**
         * create/destroy ALSA playback/capture handler
         */
        AudioALSAPlaybackHandlerBase *createPlaybackHandler(stream_attribute_t *stream_attribute_source);
        AudioALSACaptureHandlerBase  *createCaptureHandler(stream_attribute_t *stream_attribute_target);

        status_t destroyPlaybackHandler(AudioALSAPlaybackHandlerBase *pPlaybackHandler);
        status_t destroyCaptureHandler(AudioALSACaptureHandlerBase   *pCaptureHandler);


        /**
         * volume related functions
         */
        status_t setVoiceVolume(float volume);
        status_t setMasterVolume(float volume);
        status_t setFmVolume(float volume);

        status_t setMicMute(bool state);
        bool     getMicMute();

        /**
         * mode / routing related functions
         */
        status_t setMode(audio_mode_t new_mode);
        status_t routingOutputDevice(AudioALSAStreamOut *pAudioALSAStreamOut,
                const audio_devices_t current_output_devices, audio_devices_t output_devices);
        status_t routingInputDevice(AudioALSAStreamIn *pAudioALSAStreamIn, const audio_devices_t current_input_device, audio_devices_t input_device);

        // check if headset has changed
        bool CheckHeadsetChange(const audio_devices_t current_input_device, audio_devices_t input_device);


        /**
         * FM radio related opeation
         */
        status_t setFmEnable(const bool enable, bool force_control = false, bool force_direct_conn = false);
        bool     getFmEnable();


        /**
         * suspend/resume all input/output stream
         */
        status_t setAllOutputStreamsSuspend(const bool suspend_on, const bool setModeRequest = false);
        status_t setAllInputStreamsSuspend(const bool suspend_on, const bool setModeRequest = false);
        status_t setAllStreamsSuspend(const bool suspend_on, const bool setModeRequest = false);


        /**
         * standby all input/output stream
         */
        status_t standbyAllOutputStreams(const bool setModeRequest = false);
        status_t standbyAllInputStreams(const bool setModeRequest = false);
        status_t standbyAllStreams(const bool setModeRequest = false);


        /**
         * audio mode status
         */
        inline bool isModeInPhoneCall() { return isModeInPhoneCall(mAudioMode); }
        inline bool isModeInVoipCall()  { return isModeInVoipCall(mAudioMode); }

        inline uint32_t getStreamOutVectorSize()  { return mStreamOutVector.size(); }
        inline AudioALSAStreamOut *getStreamOut(const size_t i)  { return mStreamOutVector[i]; }
        inline AudioALSAStreamIn *getStreamIn(const size_t i)  { return mStreamInVector[i]; }


        /**
         * stream in related
         */
        virtual size_t getInputBufferSize(uint32_t sampleRate, int format, int channelCount);


        status_t SetMusicPlusStatus(bool bEnable);
        bool GetMusicPlusStatus();
        status_t SetBesLoudnessStatus(bool bEnable);
        bool GetBesLoudnessStatus();
        status_t SetBesLoudnessControlCallback(const BESLOUDNESS_CONTROL_CALLBACK_STRUCT *callback_data);
        status_t UpdateACFHCF(int value);
        status_t SetACFPreviewParameter(void *ptr , int len);
        status_t SetHCFPreviewParameter(void *ptr , int len);

        status_t SetSpeechVmEnable(const int enable);
        status_t SetEMParameter(AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB);
        status_t UpdateSpeechParams(const int speech_band);
        status_t UpdateDualMicParams();
        status_t UpdateSpeechMode();
        status_t UpdateSpeechVolume();
        status_t SetVCEEnable(bool bEnable);
        status_t Enable_DualMicSettng(sph_enh_dynamic_mask_t sphMask, bool bEnable);
        status_t Set_LSPK_DlMNR_Enable(sph_enh_dynamic_mask_t sphMask, bool bEnable);
        status_t setSpkOutputGain(int32_t gain, uint32_t ramp_sample_cnt);
        status_t setSpkFilterParam(uint32_t fc, uint32_t bw, int32_t th);
        status_t setVtNeedOn(const bool vt_on);
        bool EnableBesRecord(void);

        /**
         * Magic Conference Call
         */
        status_t SetMagiConCallEnable(bool bEnable);
        bool GetMagiConCallEnable(void);

        /**
         * VM Log
         */
        status_t SetVMLogConfig(unsigned short mVMConfig);
        unsigned short GetVMLogConfig(void);

        /**
         * Cust XML
         */
        status_t SetCustXmlEnable(unsigned short enable);
        unsigned short GetCustXmlEnable(void);

        /**
         * VoIP dynamic function
         */
        void UpdateDynamicFunctionMask(void);

        /**
        * BT NREC
        */
        status_t SetBtHeadsetNrec(bool bEnable);
        bool GetBtHeadsetNrecStatus(void);

        /**
         * low latency
         */
        status_t setScreenState(bool mode);
        /**
         * Bypass DL Post Process Flag
         */
        status_t setBypassDLProcess(bool flag);

        void enableStreamLock(void);
        void disableStreamLock(void);
        void putStreamOutIntoStandy(uint32_t use_case);
        bool isStreamOutExist(uint32_t use_case);
        bool isHdmiStreamOutExist(void);
        bool isStreamOutActive(uint32_t use_case);

        status_t setParametersToStreamOut(const String8 &keyValuePairs);
        status_t setParameters(const String8 &keyValuePairs, int audio_io_handle);


    protected:
        AudioALSAStreamManager();

        inline bool isModeInPhoneCall(const audio_mode_t audio_mode)
        {
            return (audio_mode == AUDIO_MODE_IN_CALL);
        }

        inline bool isModeInVoipCall(const audio_mode_t audio_mode)
        {
            return (audio_mode == AUDIO_MODE_IN_COMMUNICATION);
        }


        void SetInputMute(bool bEnable);

    private:
        /**
         * singleton pattern
         */
        static AudioALSAStreamManager *mStreamManager;


        /**
         * stream manager lock
         */
        AudioLock mStreamVectorLock; // used in setMode & open/close input/output stream
        AudioLock mLock;


        /**
         * stream in/out vector
         */
        KeyedVector<uint32_t, AudioALSAStreamOut *> mStreamOutVector;
        KeyedVector<uint32_t, AudioALSAStreamIn *>  mStreamInVector;


        /**
         * stream playback/capture handler vector
         */
        KeyedVector<uint32_t, AudioALSAPlaybackHandlerBase *> mPlaybackHandlerVector;
        KeyedVector<uint32_t, AudioALSACaptureHandlerBase *>  mCaptureHandlerVector;
        uint32_t mPlaybackHandlerIndex;
        uint32_t mCaptureHandlerIndex;


        /**
         * speech phone call controller
         */
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        AudioALSASpeechPhoneCallController *mSpeechPhoneCallController;
        SpeechDriverFactory *mSpeechDriverFactory;
        bool mPhoneCallSpeechOpen;
#endif

       /**
        * FM radio
        */
        AudioALSAFMController *mFMController;


        /**
         * volume controller
         */
        AudioVolumeInterface *mAudioALSAVolumeController;


        /**
         * volume related variables
         */
        bool mMicMute;


        /**
         * audio mode
         */
        audio_mode_t mAudioMode;

        /**
         * stream in/out vector
         */
        KeyedVector<uint32_t, AudioMTKFilterManager *> mFilterManagerVector;

        bool mBesLoudnessStatus;

        void (*mBesLoudnessControlCallback)(void *data);

        /**
         * Speech EnhanceInfo Instance
         */
        AudioSpeechEnhanceInfo *mAudioSpeechEnhanceInfoInstance;

        /**
         * headphone change flag
         */
         bool mHeadsetChange;
        
        /**
        * Bypass DL Post Process Flag
        */
        bool mBypassPostProcessDL;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_STREAM_MANAGER_H
