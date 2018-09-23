#ifndef ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_ECHOREF_EXT_H
#define ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_ECHOREF_EXT_H

#include "AudioALSACaptureDataProviderBase.h"

namespace android
{

class AudioALSACaptureDataProviderEchoRefExt : public AudioALSACaptureDataProviderBase
{
    public:
        virtual ~AudioALSACaptureDataProviderEchoRefExt();

        static AudioALSACaptureDataProviderEchoRefExt *getInstance();

        /**
         * open/close pcm interface when 1st attach & the last detach
         */
        status_t open();
        status_t close();
        uint32_t getLatencyTime();

    protected:
        AudioALSACaptureDataProviderEchoRefExt();

    private:
        /**
         * singleton pattern
         */
        static AudioALSACaptureDataProviderEchoRefExt *mAudioALSACaptureDataProviderEchoRefExt;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_ECHOREF_EXT_H