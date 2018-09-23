#ifndef ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_BASE_H
#define ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_BASE_H

#include <utils/KeyedVector.h>

#include <tinyalsa/asoundlib.h>

#include <pthread.h>


#include "AudioType.h"
#include "AudioLock.h"
#include "AudioUtility.h"
#include "V3/include/AudioALSADeviceParser.h"

namespace android
{

class AudioLock;
class AudioALSACaptureDataClient;


/// Observer pattern: Subject
class AudioALSACaptureDataProviderBase
{
    public:
        virtual ~AudioALSACaptureDataProviderBase();

        /**
         * attach/detach client to capture data provider
         */
        void attach(AudioALSACaptureDataClient *pCaptureDataClient);
        void detach(AudioALSACaptureDataClient *pCaptureDataClient);
        void start(AudioALSACaptureDataClient *pCaptureDataClient);

        const stream_attribute_t *getStreamAttributeSource() { return &mStreamAttributeSource; }

        int getCapturePosition(int64_t *frames, int64_t *time);

        const stream_attribute_t *getStreamAttributeTarget();

        static int mDumpFileNum;


    protected:
        AudioALSACaptureDataProviderBase();

        /**
         * pcm driver open/close
         */
        status_t         openPcmDriver(const unsigned int device);
        status_t         closePcmDriver();

        /**
         * provide captrue data to all attached clients
         */
        void     provideCaptureDataToAllClients(const uint32_t open_index);


        //echoref+++
        /**
         * provide captrue data to all attached clients
         */
        void     provideEchoRefCaptureDataToAllClients(const uint32_t open_index);
        //echoref---


        /**
         * open/close pcm interface when 1st attach & the last detach
         */
        virtual status_t open() = 0;
        virtual status_t close() = 0;
        virtual uint32_t getLatencyTime() { return 0; };


        /**
         * pcm read time stamp
         */
        status_t GetCaptureTimeStamp(time_info_struct_t *Time_Info, unsigned int read_size);

        int64_t mCaptureFramesReaded;
        timespec mCaptureTimeStamp;

        /**
         * check if any attached clients has low latency requirement
         */
        bool HasLowLatencyCapture(void);

        /**
         * enable state
         */
        bool mEnable;


        /**
         * Provider Index
         */
        uint32_t mOpenIndex;


        /**
         * latency time
         */
        uint32_t mlatency;


        /**
         * lock
         */
        AudioLock mClientLock; // first
        AudioLock mEnableLock; // second
        AudioLock mTimeStampLock;  // Protect timestamp

        /**
         * client vector
         */
        KeyedVector<uint32_t, AudioALSACaptureDataClient *> mCaptureDataClientVector;
        uint32_t mCaptureDataClientIndex;

        /**
         * local ring buffer
         */
        RingBuf             mPcmReadBuf;


        /**
         * tinyalsa pcm interface
         */
        struct pcm_config mConfig;
        struct pcm *mPcm;


        /**
         * pcm read attribute
         */
        stream_attribute_t mStreamAttributeSource;

        capture_provider_t mCaptureDataProviderType;

        void  OpenPCMDump(const char *class_name);
        void  ClosePCMDump(void);
        void  WritePcmDumpData(void);

        status_t run();

        /**
         * pcm read thread
         */
        static void *captureReadThread(void *arg);

        FILE *mPCMDumpFile;

        pthread_t hReadThread;
        uint32_t mReadBufferSize;
        uint32_t mCaptureDropSize;
        struct timespec mNewtime, mOldtime; //for calculate latency
        double timerec[3]; //0=>threadloop, 1=>kernel delay, 2=>process delay
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_CAPTURE_DATA_PROVIDER_BASE_H
