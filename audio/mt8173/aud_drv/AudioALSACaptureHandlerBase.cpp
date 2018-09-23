#define LOG_TAG "AudioALSACaptureHandlerBase"

#include "AudioALSACaptureHandlerBase.h"
#include "AudioALSACaptureDataClient.h"
#include "AudioALSAHardwareResourceManager.h"
//#include "AudioALSACaptureDataProvider.h"


namespace android
{

AudioALSACaptureHandlerBase::AudioALSACaptureHandlerBase(stream_attribute_t *stream_attribute_target) :
    mHardwareResourceManager(AudioALSAHardwareResourceManager::getInstance()),
    mCaptureDataClient(NULL),
    mStreamAttributeTarget(stream_attribute_target),
    mSupportConcurrencyInCall(false),
    mCaptureHandlerType(CAPTURE_HANDLER_BASE),
    mIdentity(0xFFFFFFFF)
{
    init();
}

AudioALSACaptureHandlerBase::~AudioALSACaptureHandlerBase()
{
}

status_t AudioALSACaptureHandlerBase::init()
{
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerBase::UpdateBesRecParam()
{
    return NO_ERROR;
}

bool AudioALSACaptureHandlerBase::isSupportConcurrencyInCall()
{
    return mSupportConcurrencyInCall;
}

capture_handler_t AudioALSACaptureHandlerBase::getCaptureHandlerType()
{
    return mCaptureHandlerType;
}

int AudioALSACaptureHandlerBase::getCapturePosition(int64_t *frames, int64_t *time)
{
    if (mCaptureDataClient == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }

    return mCaptureDataClient->getCapturePosition(frames, time);
}

} // end of namespace android

