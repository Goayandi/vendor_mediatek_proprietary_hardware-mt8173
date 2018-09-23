#define LOG_TAG "AudioALSACaptureHandlerBTSCO"

#include "AudioALSACaptureHandlerBTSCO.h"
#include "AudioALSACaptureDataClient.h"
#include "AudioALSACaptureDataProviderBTSCO.h"

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

namespace android
{

AudioALSACaptureHandlerBTSCO::AudioALSACaptureHandlerBTSCO(stream_attribute_t *stream_attribute_target) :
    AudioALSACaptureHandlerBase(stream_attribute_target)
{
    ALOGV("%s()", __FUNCTION__);
    init();
}

AudioALSACaptureHandlerBTSCO::~AudioALSACaptureHandlerBTSCO()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureHandlerBTSCO::init()
{
    mCaptureHandlerType = CAPTURE_HANDLER_BT;
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerBTSCO::open()
{
    ALOGV("+%s(), input_device = 0x%x, input_source = 0x%x",
          __FUNCTION__, mStreamAttributeTarget->input_device, mStreamAttributeTarget->input_source);

    ASSERT(mCaptureDataClient == NULL);
    mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderBTSCO::getInstance(), mStreamAttributeTarget);
    mCaptureDataClient->start();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerBTSCO::close()
{
    ALOGV("+%s()", __FUNCTION__);

    ASSERT(mCaptureDataClient != NULL);
    delete mCaptureDataClient;

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerBTSCO::routing(const audio_devices_t input_device)
{
    (void)input_device;
    return INVALID_OPERATION;
}

ssize_t AudioALSACaptureHandlerBTSCO::read(void *buffer, ssize_t bytes)
{
    ALOGVV("%s()", __FUNCTION__);

    return mCaptureDataClient->read(buffer, bytes);
}

} // end of namespace android
