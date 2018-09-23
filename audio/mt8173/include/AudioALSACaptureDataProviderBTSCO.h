#ifndef ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_BT_SCO_H
#define ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_BT_SCO_H

#include "AudioALSACaptureDataProviderBase.h"

namespace android
{

class WCNChipController;

class AudioALSACaptureDataProviderBTSCO : public AudioALSACaptureDataProviderBase
{
    public:
        virtual ~AudioALSACaptureDataProviderBTSCO();

        static AudioALSACaptureDataProviderBTSCO *getInstance();

        /**
         * open/close pcm interface when 1st attach & the last detach
         */
        status_t open();
        status_t close();

        uint32_t getLatencyTime();

    protected:
        AudioALSACaptureDataProviderBTSCO();

    private:
        /**
         * singleton pattern
         */
        static AudioALSACaptureDataProviderBTSCO *mAudioALSACaptureDataProviderBTSCO;

        WCNChipController *mWCNChipController;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_BT_SCO_H
