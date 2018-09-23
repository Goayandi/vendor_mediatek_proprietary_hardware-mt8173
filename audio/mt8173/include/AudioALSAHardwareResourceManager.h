#ifndef ANDROID_AUDIO_ALSA_HARDWARE_RESOURCE_MANAGER_H
#define ANDROID_AUDIO_ALSA_HARDWARE_RESOURCE_MANAGER_H

#include <tinyalsa/asoundlib.h>
#include <audio_route/audio_route.h>

#include "AudioType.h"
#include "AudioLock.h"
#include "AudioUtility.h"

namespace android
{

typedef enum
{
    AUDIO_SPEAKER_MODE_D = 0,
    AUDIO_SPEAKER_MODE_AB = 1,

} AUDIO_SPEAKER_MODE;


enum builtin_mic_specific_type
{
    BUILTIN_MIC_DEFAULT,
    BUILTIN_MIC_MIC1_ONLY,
    BUILTIN_MIC_MIC2_ONLY,
    BUILTIN_MIC_MIC3_ONLY,
};

enum audio_route_device_type
{
    ROUTE_DEVICE_OUT_RECEIVER = 0,
    ROUTE_DEVICE_OUT_SPEAKER,
    ROUTE_DEVICE_OUT_HEADPHONE,
    ROUTE_DEVICE_OUT_SPEAKER_AND_HEADPHONE,
    ROUTE_DEVICE_IN_BUILTIN_MIC,
    ROUTE_DEVICE_IN_BACK_MIC,
    ROUTE_DEVICE_IN_HEADSET_MIC,
    ROUTE_DEVICE_NUM,
};

enum audio_route_scenario_type
{
    ROUTE_NORMAL_PLAYBACK = 0,
    ROUTE_LOW_LATENCY_PLAYBACK
};

enum audio_route_path_type
{
    ROUTE_NORMARL_PLAYBACK_RECEIVER = 0,
    ROUTE_NORMARL_PLAYBACK_SPEAKER,
    ROUTE_NORMARL_PLAYBACK_HEADPHONE,
    ROUTE_NORMARL_PLAYBACK_SPEAKER_AND_HEADPHONE,
    ROUTE_LOW_LATENCY_PLAYBACK_RECEIVER,
    ROUTE_LOW_LATENCY_PLAYBACK_SPEAKER,
    ROUTE_LOW_LATENCY_PLAYBACK_HEADPHONE,
    ROUTE_LOW_LATENCY_PLAYBACK_SPEAKER_AND_HEADPHONE,
    ROUTE_PATH_NUM,
};

class AudioALSAHardwareResourceManager
{
    public:
        virtual ~AudioALSAHardwareResourceManager();
        static AudioALSAHardwareResourceManager *getInstance();
        virtual status_t ResetDevice(void);

        /**
         * output devices
         */
        virtual status_t setOutputDevice(const audio_devices_t new_devices, const uint32_t sample_rate);
        virtual status_t startOutputDevice(const audio_devices_t new_devices, const uint32_t SampleRate);
        virtual status_t stopOutputDevice();
        virtual status_t changeOutputDevice(const audio_devices_t new_devices);
        virtual bool isSharedOutDevice(audio_devices_t devices);
        virtual audio_devices_t getOutputDevice();

        /**
         * output devices - Before using them, we should keep the mlock
         */
        virtual status_t startOutputDevice_l(const audio_devices_t new_devices, const uint32_t SampleRate);
        virtual status_t stopOutputDevice_l();
        virtual status_t changeOutputDevice_l(const audio_devices_t new_devices, const uint32_t SampleRate);

        /**
         * input devices
         */
        virtual status_t setInputDevice(const audio_devices_t new_device);
        virtual status_t startInputDevice(const audio_devices_t new_device);
        virtual status_t stopInputDevice(const audio_devices_t stop_device);
        virtual status_t changeInputDevice(const audio_devices_t new_device);
        virtual audio_devices_t getInputDevice();


        /**
         * HW Gain2
         */
        virtual status_t setHWGain2DigitalGain(const uint32_t gain);


        /**
         * Interrupt Rate
         */
        virtual status_t setInterruptRate(const uint32_t rate);
        virtual status_t setInterruptRate2(const uint32_t rate);


        /**
         * sgen
         */
        virtual status_t setSgenMode(const sgen_mode_t sgen_mode);
        virtual status_t setSgenSampleRate(const sgen_mode_samplerate_t sample_rate);
        virtual status_t openAddaOutput(const uint32_t sample_rate);
        virtual status_t closeAddaOutput();

        /**
        * sidetone
        */
        virtual status_t EnableSideToneFilter(const bool enable);
        /**
        *   Current Sensing
        */
        virtual status_t setSPKCurrentSensor(bool bSwitch);

        virtual status_t setSPKCurrentSensorPeakDetectorReset(bool bSwitch);

        /**
         * MIC inverse
         */
        virtual status_t setMicInverse(bool bMicInverse);
        virtual bool getMicInverse(void);

        virtual void setMIC1Mode(bool isphonemic);
        virtual void setMIC2Mode(bool isphonemic);

        /**
         * build in mic specific type
         */
        virtual void     setBuiltInMicSpecificType(const builtin_mic_specific_type type) { mBuiltInMicSpecificType = type; }

         /**
         * build in mic specific type
         */
        virtual void     EnableAudBufClk(bool bEanble);


        /**
         * Headphone Change information
         */
        virtual void     setHeadPhoneChange(bool bchange) { mHeadchange = bchange; }
        virtual bool     getHeadPhoneChange(void) { return mHeadchange ; }

        /**
         * Headphone Change information
         */
        virtual void     setNumOfHeadsetPole(int pole) { mNumHSPole = pole; }
        virtual int      getNumOfHeadsetPole(void) { return mNumHSPole; }

        /**
         * debug dump register & DAC I2S Sgen
         */
        virtual void     setAudioDebug(const bool enable);

        void HpImpeDanceDetect(void);

        bool             isStereoSpeaker() { return mBoardChannelType == 0; };

        virtual status_t setRoutePath(const uint32_t scenario, const audio_devices_t devices);
        virtual status_t resetRoutePath(const uint32_t scenario, const audio_devices_t devices);

    protected:
        AudioALSAHardwareResourceManager();

    private:
        /**
         * singleton pattern
         */
        static AudioALSAHardwareResourceManager *mAudioALSAHardwareResourceManager;
        static const char* mAudioRouteDevice[ROUTE_DEVICE_NUM];
        static const char* mAudioRoutePath[ROUTE_PATH_NUM];

        void init();
        void applyRouteDevice(const uint32_t route_device, const bool enable);
        void applyRoutePath(const uint32_t route_path, const bool enable);

        /**
         *  low level for device open close
         */
        virtual status_t OpenReceiverPath(const uint32_t SampleRate);
        virtual status_t CloseReceiverPath();
        virtual status_t OpenHeadphonePath(const uint32_t SampleRate);
        virtual status_t CloseHeadphonePath();
        virtual status_t OpenSpeakerPath(const uint32_t SampleRate);
        virtual status_t CloseSpeakerPath();
        virtual status_t OpenHeadphoneSpeakerPath(const uint32_t SampleRate);
        virtual status_t CloseHeadphoneSpeakerPath();

        AudioLock mLock;
        AudioLock mLockin;

        struct mixer *mMixer;
        struct pcm *mPcmDL;

        audio_devices_t mOutputDevices;
        audio_devices_t mInputDevice;

        uint32_t mOutputDeviceSampleRate;
        uint32_t mInputDeviceSampleRate;

        bool mIsChangingInputDevice;

        uint32_t mStartOutputDevicesCount;
        uint32_t mStartInputDeviceCount;

        bool mMicInverse;
        builtin_mic_specific_type mBuiltInMicSpecificType;

        int mNumHSPole;
        bool mHeadchange;

        struct audio_route *mAudioRoute;
        uint32_t mBoardChannelType;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_HARDWARE_RESOURCE_MANAGER_H
