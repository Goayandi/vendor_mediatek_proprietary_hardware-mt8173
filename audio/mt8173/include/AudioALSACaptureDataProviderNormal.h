#ifndef ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_NORMAL_H
#define ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_NORMAL_H

#include "AudioALSACaptureDataProviderBase.h"

namespace android
{

class AudioALSACaptureDataProviderNormal : public AudioALSACaptureDataProviderBase
{
    public:
        virtual ~AudioALSACaptureDataProviderNormal();

        static AudioALSACaptureDataProviderNormal *getInstance();

        /**
         * open/close pcm interface when 1st attach & the last detach
         */
        status_t open();
        status_t close();
        uint32_t getLatencyTime();

    protected:
        AudioALSACaptureDataProviderNormal();

    private:
        /**
         * singleton pattern
         */
        static AudioALSACaptureDataProviderNormal *mAudioALSACaptureDataProviderNormal;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_NORMAL_H