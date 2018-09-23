#define LOG_TAG  "AudioStreamAttribute"

#include "AudioStreamAttribute.h"
#include <utils/Log.h>

AudioStreamAttribute::AudioStreamAttribute()
{
    ALOGD("AudioStreamAttribute constructor \n");
    mFormat = 0;
    mDirection = 0 ;
    mSampleRate = 0 ;
    mChannels = 0 ;
    mSource = 0 ;
    mdevices = 0 ;
    mAcoustic = 0;
    mPredevices = 0;
    mIsDigitalMIC = false;
    mBufferSize = 0;
    mInterruptSample = 0;
#ifdef EXTCODEC_ECHO_REFERENCE_SUPPORT
    mEchoRefUse = false;
#endif

}

AudioStreamAttribute::~AudioStreamAttribute()
{
    ALOGD("AudioStreamAttribute destructor \n");
}


AudioMEMIFAttribute::AudioMEMIFAttribute()
{
    mMemoryInterFaceType = 0;
    mSampleRate = 0;
    mBufferSize = 0;
    mChannels = 0 ;
    mInterruptSample = 0;
    mFormat = 0;
    mDirection = 0;
    mClockInverse = 0;
    m = 0;
    mMonoSel = 0;
    mdupwrite = 0;
    mState = AudioMEMIFAttribute::STATE_FREE;
    mFetchFormatPerSample = AudioMEMIFAttribute::AFE_WLEN_16_BIT;
    ALOGD("AudioMEMIFAttribute constructor \n");
}

AudioMEMIFAttribute::~AudioMEMIFAttribute()
{
    ALOGD("AudioMEMIFAttribute destructor \n");
}

