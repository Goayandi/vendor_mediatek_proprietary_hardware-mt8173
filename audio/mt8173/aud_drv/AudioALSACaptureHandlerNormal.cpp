#define LOG_TAG "AudioALSACaptureHandlerNormal"

#include "AudioALSACaptureHandlerNormal.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSACaptureDataClient.h"
#include "AudioALSACaptureDataProviderNormal.h"
#include "AudioALSACaptureDataProviderExternal.h"


//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

namespace android
{

//static FILE *pOutFile = NULL;

AudioALSACaptureHandlerNormal::AudioALSACaptureHandlerNormal(stream_attribute_t *stream_attribute_target) :
    AudioALSACaptureHandlerBase(stream_attribute_target)
{
    ALOGV("%s()", __FUNCTION__);

    init();
}

AudioALSACaptureHandlerNormal::~AudioALSACaptureHandlerNormal()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureHandlerNormal::init()
{
    mCaptureHandlerType = CAPTURE_HANDLER_NORMAL;
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerNormal::open()
{
    ALOGV("+%s(), input_device = 0x%x, input_source = 0x%x, sample_rate=%d, num_channels=%d",
          __FUNCTION__, mStreamAttributeTarget->input_device, mStreamAttributeTarget->input_source,
          mStreamAttributeTarget->sample_rate, mStreamAttributeTarget->num_channels);

    ASSERT(mCaptureDataClient == NULL);

    if (IsAudioSupportFeature(AUDIO_SUPPORT_EXTERNAL_BUILTIN_MIC) &&
        (mStreamAttributeTarget->input_device != AUDIO_DEVICE_IN_WIRED_HEADSET)) {
        mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderExternal::getInstance(), mStreamAttributeTarget);
    } else {
        mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderNormal::getInstance(), mStreamAttributeTarget);
    }

#if 0
    pOutFile = fopen("/sdcard/mtklog/RecRaw.pcm", "wb");
    if (pOutFile == NULL)
    {
        ALOGD("%s(), fopen fail", __FUNCTION__);
    }
#endif

    mHardwareResourceManager->startInputDevice(mStreamAttributeTarget->input_device);

    mCaptureDataClient->start();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerNormal::close()
{
    ALOGV("+%s()", __FUNCTION__);

    mHardwareResourceManager->stopInputDevice(mHardwareResourceManager->getInputDevice());

    ASSERT(mCaptureDataClient != NULL);
    delete mCaptureDataClient;

#if 0
    if (pOutFile != NULL)
    {
        fclose(pOutFile);
    }
#endif
    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerNormal::routing(const audio_devices_t input_device)
{
    mHardwareResourceManager->changeInputDevice(input_device);
    return NO_ERROR;
}

ssize_t AudioALSACaptureHandlerNormal::read(void *buffer, ssize_t bytes)
{
    ALOGVV("%s()", __FUNCTION__);

    mCaptureDataClient->read(buffer, bytes);
#if 0   //remove dump here which might cause process too long due to SD performance
    if (pOutFile != NULL)
    {
        fwrite(buffer, sizeof(char), bytes, pOutFile);
    }
#endif
    return bytes;
}

} // end of namespace android
