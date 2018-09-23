#define LOG_TAG "AudioALSACaptureHandlerFMRadio"

#include "AudioALSACaptureHandlerFMRadio.h"
#include "AudioALSACaptureDataClient.h"
#include "AudioALSACaptureDataProviderFMRadio.h"
#include "AudioALSAStreamManager.h"

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

namespace android
{

AudioALSACaptureHandlerFMRadio::AudioALSACaptureHandlerFMRadio(stream_attribute_t *stream_attribute_target) :
    AudioALSACaptureHandlerBase(stream_attribute_target)
{
    ALOGV("%s()", __FUNCTION__);
    mSupportConcurrencyInCall = false;
    init();
}

AudioALSACaptureHandlerFMRadio::~AudioALSACaptureHandlerFMRadio()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureHandlerFMRadio::init()
{
    ALOGV("%s()", __FUNCTION__);
    mCaptureHandlerType = CAPTURE_HANDLER_FM_RADIO;
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerFMRadio::open()
{
    ALOGV("+%s(), input_device = 0x%x, input_source = 0x%x",
          __FUNCTION__, mStreamAttributeTarget->input_device, mStreamAttributeTarget->input_source);

    //For Google FM, app keeps recording when entering into InCall or InCommunication Mode
    if (mSupportConcurrencyInCall == false && (AudioALSAStreamManager::getInstance()->isModeInPhoneCall() == true || AudioALSAStreamManager::getInstance()->isModeInVoipCall() == true))
    {
        mCaptureDataClient = NULL;
        ALOGD("-%s() don't support FM Record at incall mode", __FUNCTION__);
        return NO_ERROR;
    }
    else if (AudioALSAStreamManager::getInstance()->getFmEnable() == false)
    {
        // For Google FM, app keeps recording when entering into Normal Mode from InCall
        ALOGW("%s() resume FM enable(App keep reading,howerver HAL disable FM for InCall)", __FUNCTION__);
        AudioALSAStreamManager::getInstance()->setFmEnable(true, true, false);
    }

    ASSERT(mCaptureDataClient == NULL);
    mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderFMRadio::getInstance(), mStreamAttributeTarget);
    mCaptureDataClient->start();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerFMRadio::close()
{
    ALOGD("+%s()", __FUNCTION__);

    ASSERT(mCaptureDataClient != NULL);
    delete mCaptureDataClient;

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerFMRadio::routing(const audio_devices_t input_device __unused)
{
    return INVALID_OPERATION;
}

ssize_t AudioALSACaptureHandlerFMRadio::read(void *buffer, ssize_t bytes)
{
    ALOGVV("%s()", __FUNCTION__);

    if (mCaptureDataClient == NULL)
    {
        size_t frameSize = audio_bytes_per_sample(mStreamAttributeTarget->audio_format) * mStreamAttributeTarget->num_channels;
        int sleepUs = ((bytes * 1000) / ((mStreamAttributeTarget->sample_rate / 1000) * frameSize));
        usleep(sleepUs);
        return bytes;
    }

    return mCaptureDataClient->read(buffer, bytes);
}

} // end of namespace android
