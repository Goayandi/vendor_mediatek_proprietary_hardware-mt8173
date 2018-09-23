#ifndef ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_ECHOREF_H
#define ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_ECHOREF_H

#include "AudioALSACaptureDataProviderBase.h"

namespace android
{

class AudioALSACaptureDataProviderEchoRef : public AudioALSACaptureDataProviderBase
{
    public:
        virtual ~AudioALSACaptureDataProviderEchoRef();

        static AudioALSACaptureDataProviderEchoRef *getInstance();

        /**
         * open/close pcm interface when 1st attach & the last detach
         */
        status_t open();
        status_t close();
        uint32_t getLatencyTime();

    protected:
        AudioALSACaptureDataProviderEchoRef();

    private:
        /**
         * singleton pattern
         */
        static AudioALSACaptureDataProviderEchoRef *mAudioALSACaptureDataProviderEchoRef;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_ECHOREF_H