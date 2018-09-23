#ifndef ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_HDMI_RAW_H
#define ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_HDMI_RAW_H

#include "AudioALSAPlaybackHandlerBase.h"
#include <audio_utils/spdif/SPDIFEncoder.h>

namespace android
{

class AudioALSAPlaybackHandlerHDMIRaw : public AudioALSAPlaybackHandlerBase
{
    public:
        AudioALSAPlaybackHandlerHDMIRaw(const stream_attribute_t *stream_attribute_source);
        virtual ~AudioALSAPlaybackHandlerHDMIRaw();


        /**
         * open/close audio hardware
         */
        virtual status_t open();
        virtual status_t close();
        virtual status_t routing(const audio_devices_t output_devices);
        virtual status_t pause();
        virtual status_t resume();
        virtual status_t flush();
        virtual int drain(audio_drain_type_t type);
        virtual status_t setVolume(uint32_t vol);


        /**
         * write data to audio hardware
         */
        virtual ssize_t  write(const void *buffer, size_t bytes);


    /* Encoded Raw Data (AC3/EAC3) into IEC61937 format */
    class IEC_SPDIFEncoder : public SPDIFEncoder
    {
        public:
            IEC_SPDIFEncoder(AudioALSAPlaybackHandlerHDMIRaw *playbackHandler) : mPlaybackHandler(playbackHandler) {};

        virtual ssize_t writeOutput(const void* buffer, size_t bytes)
        {
            return mPlaybackHandler->write_to_pcm(buffer, bytes);
        }
        protected:
            AudioALSAPlaybackHandlerHDMIRaw * const mPlaybackHandler;
    };

    private:
        ssize_t  write_to_pcm(const void *buffer, size_t bytes);
        IEC_SPDIFEncoder mIEC_SPDIFEncoder;

};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_HDMI_RAW_H
