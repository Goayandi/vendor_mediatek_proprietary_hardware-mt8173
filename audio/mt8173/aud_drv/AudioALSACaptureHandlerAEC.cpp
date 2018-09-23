#define LOG_TAG "AudioALSACaptureHandlerAEC"

#include "AudioALSACaptureHandlerAEC.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSACaptureDataClient.h"
#include "AudioALSACaptureDataProviderNormal.h"
#include "AudioALSACaptureDataProviderBTSCO.h"
#include "AudioALSACaptureDataProviderEchoRef.h"
#include "AudioALSACaptureDataProviderEchoRefBTSCO.h"
#include "AudioALSACaptureDataProviderEchoRefExt.h"
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

AudioALSACaptureHandlerAEC::AudioALSACaptureHandlerAEC(stream_attribute_t *stream_attribute_target) :
    AudioALSACaptureHandlerBase(stream_attribute_target)
{
    ALOGV("%s()", __FUNCTION__);
    init();
}

AudioALSACaptureHandlerAEC::~AudioALSACaptureHandlerAEC()
{
    ALOGV("%s()", __FUNCTION__);
}

status_t AudioALSACaptureHandlerAEC::init()
{
    ALOGV("%s()", __FUNCTION__);
    mCaptureHandlerType = CAPTURE_HANDLER_AEC;
    memset(&mStreamAttributeTargetEchoRef, 0, sizeof(mStreamAttributeTargetEchoRef));
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerAEC::open()
{
    ALOGV("+%s(), input_device = 0x%x, input_source = 0x%x, sample_rate=%d, num_channels=%d, output_devices=0x%x",
          __FUNCTION__, mStreamAttributeTarget->input_device, mStreamAttributeTarget->input_source, mStreamAttributeTarget->sample_rate,
          mStreamAttributeTarget->num_channels, mStreamAttributeTarget->output_devices);

    ASSERT(mCaptureDataClient == NULL);
#if 0   //enable for check EchoRef data is correct
    //mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderNormal::getInstance(), mStreamAttributeTarget);
    //mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderEchoRef::getInstance(), mStreamAttributeTarget);
#else
    if (mStreamAttributeTarget->input_device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
    {
        //open BT  data provider
        mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderBTSCO::getInstance(), mStreamAttributeTarget);
    }
    else if (IsAudioSupportFeature(AUDIO_SUPPORT_EXTERNAL_BUILTIN_MIC) &&
             (mStreamAttributeTarget->input_device != AUDIO_DEVICE_IN_WIRED_HEADSET))
    {
        mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderExternal::getInstance(), mStreamAttributeTarget);
    }
    else
    {
        mCaptureDataClient = new AudioALSACaptureDataClient(AudioALSACaptureDataProviderNormal::getInstance(), mStreamAttributeTarget);
    }

    //open echoref data provider
    if (mCaptureDataClient != NULL)
    {
        memcpy(&mStreamAttributeTargetEchoRef, mStreamAttributeTarget, sizeof(stream_attribute_t));
        if (mStreamAttributeTarget->input_device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
        {
            //open BT  echoref data provider
            mCaptureDataClient->AddEchoRefDataProvider(AudioALSACaptureDataProviderEchoRefBTSCO::getInstance(), &mStreamAttributeTargetEchoRef);
        }
        else if (mStreamAttributeTarget->output_devices == AUDIO_DEVICE_OUT_SPEAKER &&
                 IsAudioSupportFeature(AUDIO_SUPPORT_EXTERNAL_ECHO_REFERENCE))
        {
            mCaptureDataClient->AddEchoRefDataProvider(AudioALSACaptureDataProviderEchoRefExt::getInstance(), &mStreamAttributeTargetEchoRef);
        }
        else
        {
            mCaptureDataClient->AddEchoRefDataProvider(AudioALSACaptureDataProviderEchoRef::getInstance(), &mStreamAttributeTargetEchoRef);
        }
    }
#endif

#if 0
    pOutFile = fopen("/sdcard/mtklog/RecRaw.pcm", "wb");
    if (pOutFile == NULL)
    {
        ALOGD("%s(), open file fail ", __FUNCTION__);
    }
#endif

    if (mStreamAttributeTarget->input_device != AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
    {
        //no need to config analog part while BT case
        mHardwareResourceManager->startInputDevice(mStreamAttributeTarget->input_device);
    }

    mCaptureDataClient->start();

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerAEC::close()
{
    ALOGV("+%s()", __FUNCTION__);

    if (mStreamAttributeTarget->input_device != AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
    {
        //no need to config analog part while BT case
        mHardwareResourceManager->stopInputDevice(mHardwareResourceManager->getInputDevice());
    }

    ASSERT(mCaptureDataClient != NULL);
    delete mCaptureDataClient;
    mCaptureDataClient = NULL;

#if 0
    if (pOutFile != NULL)
    {
        fclose(pOutFile);
    }
#endif

    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSACaptureHandlerAEC::routing(const audio_devices_t input_device)
{
    mHardwareResourceManager->changeInputDevice(input_device);
    return NO_ERROR;
}

ssize_t AudioALSACaptureHandlerAEC::read(void *buffer, ssize_t bytes)
{
    ALOGVV("%s()", __FUNCTION__);

    mCaptureDataClient->read(buffer, bytes);
#if 0
    if (pOutFile != NULL)
    {
        fwrite(buffer, sizeof(char), bytes, pOutFile);
    }
#endif

    return bytes;
}

status_t AudioALSACaptureHandlerAEC::UpdateBesRecParam()
{
    ALOGV("+%s()", __FUNCTION__);
    if (mCaptureDataClient != NULL)
    {
        mCaptureDataClient->UpdateBesRecParam();
    }
    ALOGV("-%s()", __FUNCTION__);
    return NO_ERROR;
}

} // end of namespace android
