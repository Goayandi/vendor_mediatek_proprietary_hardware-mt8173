#ifndef ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_EXTERNAL_H
#define ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_EXTERNAL_H

#include "AudioALSACaptureDataProviderBase.h"

namespace android
{

class AudioALSACaptureDataProviderExternal : public AudioALSACaptureDataProviderBase
{
    public:
        virtual ~AudioALSACaptureDataProviderExternal();

        static AudioALSACaptureDataProviderExternal *getInstance();

        /**
         * open/close pcm interface when 1st attach & the last detach
         */
        status_t open();
        status_t close();
        uint32_t getLatencyTime();

    protected:
        AudioALSACaptureDataProviderExternal();

    private:
        /**
         * singleton pattern
         */
        static AudioALSACaptureDataProviderExternal *mAudioALSACaptureDataProviderExternal;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_EXTERNAL_H