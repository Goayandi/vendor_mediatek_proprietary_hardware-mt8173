#include "AudioSmartPaController.h"
#include "SpeechDriverFactory.h"
#include "AudioParamParser.h"
#include "AudioUtility.h"

#include <string>
#include <dlfcn.h>

#define LOG_TAG "AudioSmartPaController"

namespace android
{

/*
 * singleton
 */
AudioSmartPaController *AudioSmartPaController::mAudioSmartPaController = NULL;
AudioSmartPaController *AudioSmartPaController::getInstance()
{
    static AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioSmartPaController == NULL)
    {
        mAudioSmartPaController = new AudioSmartPaController();
    }

    ASSERT(mAudioSmartPaController != NULL);
    return mAudioSmartPaController;
}

/*
 * constructor / destructor
 */
AudioSmartPaController::AudioSmartPaController() :
    mMixer(AudioALSADriverUtility::getInstance()->getMixer()),
    mLibHandle(NULL),
    mtk_smartpa_init(NULL)
{
    // init variables
    memset(&mSmartPa, 0, sizeof(mSmartPa));

    // init process
    init();
};

AudioSmartPaController::~AudioSmartPaController()
{
    deinit();

    if (mLibHandle)
    {
        if (dlclose(mLibHandle))
        {
            ALOGE("%s(), dlclose failed, dlerror = %s", __FUNCTION__, dlerror());
        }
    }
};

/*
 * function implementations
 */
int AudioSmartPaController::init()
{
    int ret;

    ret = initSmartPaAttribute();
    if (ret)
    {
        ALOGE("%s(), initSmartPaAttribute failed, ret = %d", __FUNCTION__, ret);
        ASSERT(ret != 0);
        return ret;
    }

    ret = initSmartPaRuntime();
    if (ret)
    {
        ALOGE("%s(), initSmartPaRuntime failed, ret = %d", __FUNCTION__, ret);
        ASSERT(ret != 0);
        return ret;
    }

    // load lib
    ALOGD("%s(), dlopen lib path: %s", __FUNCTION__, mSmartPa.attribute.spkLibPath);
    mLibHandle = dlopen(mSmartPa.attribute.spkLibPath, RTLD_NOW);

    if (!mLibHandle)
    {
        ALOGW("%s(), dlopen failed, dlerror = %s", __FUNCTION__, dlerror());
    }
    else
    {
        mtk_smartpa_init = (int (*)(struct SmartPa *))dlsym(mLibHandle, "mtk_smartpa_init");
        if (!mtk_smartpa_init)
        {
            ALOGW("%s(), dlsym failed, dlerror = %s", __FUNCTION__, dlerror());
        }
    }

    // lib init
    if (mtk_smartpa_init)
    {
        ret = mtk_smartpa_init(&mSmartPa);
        if (ret)
        {
            ALOGE("%s(), mtk_smartpa_init failed, ret = %d", __FUNCTION__, ret);
            ASSERT(ret != 0);
            return ret;
        }
    }

    // callback init
    if (mSmartPa.ops.init)
    {
        mSmartPa.ops.init(&mSmartPa);
    }

    if (mSmartPa.attribute.isApllNeeded)
    {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_always_hd_Switch"), "On"))
        {
            ALOGE("Error: Audio_always_hd_Switch invalid value");
        }
    }

    if (strlen(mSmartPa.attribute.codecCtlName))
    {
        ret = speakerOff();
    }

    return ret;
}

int AudioSmartPaController::deinit()
{
    if (mSmartPa.ops.deinit)
    {
        mSmartPa.ops.deinit();
    }

    return 0;
}

int AudioSmartPaController::initSmartPaAttribute()
{
    struct SmartPaAttribute *attr = &mSmartPa.attribute;
    AppOps* appOps = appOpsGetInstance();
    if (appOps == NULL) {
        ALOGE("%s(), Error: AppOps == NULL", __FUNCTION__);
        ASSERT(false);
        return -ENOENT;
    }

    AppHandle *appHandle = appOps->appHandleGetInstance();
    const char *spkType = appOps->appHandleGetFeatureOptionValue(appHandle, "MTK_AUDIO_SPEAKER_PATH");
    const char audioTypeName[] = "SmartPa";

    ALOGD("%s(), spkType = %s", __FUNCTION__, spkType);

    // extract parameters from xml
    AudioType *audioType;
    audioType = appOps->appHandleGetAudioTypeByName(appHandle, audioTypeName);
    if (!audioType)
    {
        ALOGW("error: get audioType fail, audioTypeName = %s", audioTypeName);
        ASSERT(false);
        return -ENOENT;
    }

    // Read lock
    appOps->audioTypeReadLock(audioType, __FUNCTION__);

    ParamUnit *paramUnit;
    std::string paramName(spkType);
    paramName = "Speaker type," + paramName;
    paramUnit = appOps->audioTypeGetParamUnit(audioType, paramName.c_str());
    if (!paramUnit)
    {
        ALOGW("error: get paramUnit fail, spkType = %s", paramName.c_str());
        appOps->audioTypeUnlock(audioType);
        ASSERT(false);
        return -ENOENT;
    }

    Param *param;
    param = appOps->paramUnitGetParamByName(paramUnit, "have_dsp");
    ASSERT(param);
    attr->haveDsp = *(int*)param->data;
    ALOGD("\tattr->haveDsp = %d", attr->haveDsp);

    param = appOps->paramUnitGetParamByName(paramUnit, "output_port");
    ASSERT(param);
    attr->outputPort = *(int*)param->data;
    ALOGD("\tattr->outputPort = %d", attr->outputPort);

    param = appOps->paramUnitGetParamByName(paramUnit, "chip_delay_us");
    ASSERT(param);
    attr->chipDelayUs = *(unsigned int*)param->data;
    ALOGD("\tattr->chipDelayUs = %d", attr->chipDelayUs);

    // load lib path
    if (In64bitsProcess())
    {
        param = appOps->paramUnitGetParamByName(paramUnit, "spk_lib64_path");
    }
    else
    {
        param = appOps->paramUnitGetParamByName(paramUnit, "spk_lib_path");
    }
    ASSERT(param);
    ASSERT(sizeof(attr->codecCtlName) / sizeof(char) > strlen((char *)param->data));
    memcpy(attr->spkLibPath, param->data, strlen((char *)param->data));
    ALOGD("\tattr->spkLibPath = %s", attr->spkLibPath);

    // get supported sample rate list, max rate, min rate
    param = appOps->paramUnitGetParamByName(paramUnit, "supported_rate_list");
    ASSERT(param);
    ALOGD("\tsupported_rate_list param->arraySize = %d", param->arraySize);

    if (param->arraySize * sizeof(attr->supportedRateList[0]) < sizeof(attr->supportedRateList))
    {
        memcpy(attr->supportedRateList, param->data, param->arraySize * sizeof(unsigned int));
    }
    else
    {
        ALOGE("%s(), support rate list too much", __FUNCTION__);
    }

    //get if is alsa codec
    param = appOps->paramUnitGetParamByName(paramUnit, "is_alsa_codec");
    ASSERT(param);
    attr->isAlsaCodec = *(int*)param->data;
    ALOGD("\tattr->is_alsa_codec = %d", attr->isAlsaCodec);

    //get codec control name, for not dsp supported SmartPA
    param = appOps->paramUnitGetParamByName(paramUnit, "codec_ctl_name");
    ASSERT(param);
    ASSERT(sizeof(attr->codecCtlName) / sizeof(char) > strlen((char *)param->data));
    memcpy(attr->codecCtlName, param->data, strlen((char *)param->data));
    ALOGD("\tattr->codec_ctl_name = %s", attr->codecCtlName);

    //get is_i2s_init_before_amp_on
    param = appOps->paramUnitGetParamByName(paramUnit, "is_i2s_init_before_amp_on");
    ASSERT(param);
    attr->isI2sInitBeforeAmpOn = *(int*)param->data;
    ALOGD("\tattr->isI2sInitBeforeAmpOn = %d", attr->isI2sInitBeforeAmpOn);

    //get is_apll_needed
    param = appOps->paramUnitGetParamByName(paramUnit, "is_apll_needed");
    ASSERT(param);
    attr->isApllNeeded = *(int*)param->data;
    ALOGD("\tattr->is_apll_needed = %d", attr->isApllNeeded);

    // Unlock
    appOps->audioTypeUnlock(audioType);

    ALOGD("%s(), supported rate:", __FUNCTION__);
    attr->supportedRateMax = 0;
    attr->supportedRateMin = UINT_MAX;
    for (size_t i = 0; i * sizeof(attr->supportedRateList[0]) < sizeof(attr->supportedRateList); i++)
    {
        if(attr->supportedRateList[i] == 0)
        {
            break;
        }

        if (attr->supportedRateList[i] > attr->supportedRateMax)
        {
            attr->supportedRateMax = attr->supportedRateList[i];
        }

        if (attr->supportedRateList[i] < attr->supportedRateMin)
        {
            attr->supportedRateMin = attr->supportedRateList[i];
        }

        ALOGD("\t[%d] = %d Hz", i, attr->supportedRateList[i]);
    }

    return 0;
}

int AudioSmartPaController::initSmartPaRuntime()
{
    mSmartPa.runtime.sampleRate = 44100;
#ifdef EXTCODEC_ECHO_REFERENCE_SUPPORT
    mSmartPa.runtime.echoReferenceConfig = 1;
#else
    mSmartPa.runtime.echoReferenceConfig = 0;
#endif

    return 0;
}

int AudioSmartPaController::speakerOn(unsigned int sampleRate)
{
    ALOGD("%s(), sampleRate = %d", __FUNCTION__, sampleRate);
    int ret = 0;

    // set runtime
    mSmartPa.runtime.sampleRate = sampleRate;

    // mixer ctrl
    if (mSmartPa.attribute.isI2sInitBeforeAmpOn == 0 && mSmartPa.attribute.haveDsp)
    {
        ret = dspOnBoardSpeakerOn(sampleRate);
    }

    if (strlen(mSmartPa.attribute.codecCtlName))
    {
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, mSmartPa.attribute.codecCtlName), "On");
        if (ret)
        {
            ALOGE("Error: %s invalid value, ret = %d", mSmartPa.attribute.codecCtlName, ret);
        }
    }

    // speakerOn callback
    if (mSmartPa.ops.speakerOn)
    {
        mSmartPa.ops.speakerOn(&mSmartPa.runtime);
    }

    return ret;
}

int AudioSmartPaController::speakerOff()
{
    ALOGD("%s()", __FUNCTION__);

    int ret = 0;

    // speakerOff callback
    if (mSmartPa.ops.speakerOff)
    {
        mSmartPa.ops.speakerOff();
    }

    // mixer ctrl
    if (mSmartPa.attribute.isI2sInitBeforeAmpOn == 0 && mSmartPa.attribute.haveDsp)
    {
        ret = dspOnBoardSpeakerOff();
    }

    if (strlen(mSmartPa.attribute.codecCtlName))
    {
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, mSmartPa.attribute.codecCtlName), "Off");
        if (ret)
        {
            ALOGE("Error: %s invalid value, ret = %d", mSmartPa.attribute.codecCtlName, ret);
        }
    }

    return ret;
}

enum sgen_mode_t AudioSmartPaController::getOutputPort()
{
    return (enum sgen_mode_t)mSmartPa.attribute.outputPort;
}

unsigned int AudioSmartPaController::getSmartPaDelayUs()
{
    return mSmartPa.attribute.chipDelayUs;
}

unsigned int AudioSmartPaController::getMaxSupportedRate()
{
    return mSmartPa.attribute.supportedRateMax;
}

unsigned int AudioSmartPaController::getMinSupportedRate()
{
    return mSmartPa.attribute.supportedRateMin;
}

bool AudioSmartPaController::isRateSupported(unsigned int rate)
{
    struct SmartPaAttribute *attr = &mSmartPa.attribute;

    for (size_t i = 0; i * sizeof(attr->supportedRateList[0]) < sizeof(attr->supportedRateList); i++)
    {
        if (rate == attr->supportedRateList[i])
            return true;
    }
    return false;
}

bool AudioSmartPaController::isEchoReferenceSupport()
{
    if (mSmartPa.attribute.haveDsp &&
        mSmartPa.runtime.echoReferenceConfig)
    {
        return true;
    }

    return false;
}

bool AudioSmartPaController::isAlsaCodec()
{
    if (mSmartPa.attribute.isAlsaCodec)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool AudioSmartPaController::isApSideSpkProtect()
{
    if (mSmartPa.attribute.haveDsp)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool AudioSmartPaController::isI2sInitBeforeAmpOn()
{
    if (mSmartPa.attribute.isI2sInitBeforeAmpOn)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int AudioSmartPaController::dspOnBoardSpeakerOn(unsigned int SampleRate)
{
    ALOGD("+%s(), SampleRate = %d", __FUNCTION__, SampleRate);
    modem_index_t modem_index = SpeechDriverFactory::GetInstance()->GetActiveModemIndex();

    if (mSmartPa.attribute.isApllNeeded)
    {
        ALOGD("+%s(), Audio_i2s0_hd_Switch on", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "On"))
        {
            ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
        }
    }

    if (SampleRate == 48000)
    {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On48000"))
        {
            ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
        }
    }
    else if (SampleRate == 44100)
    {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On44100"))
        {
            ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
        }
    }
    else if (SampleRate == 32000)
    {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On32000"))
        {
            ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
        }
    }
    else if (SampleRate == 16000)
    {
        if (isEchoReferenceSupport())
        {
            ALOGD("%s(), Audio_ExtCodec_EchoRef_Switch on", __FUNCTION__);
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ExtCodec_EchoRef_Switch"), "On"))
            {
                ALOGE("Error: Audio_ExtCodec_EchoRef_Switch invalid value");
            }
        }
        if (modem_index == MODEM_1)
        {
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On16000"))
            {
                ALOGE("Error: Audio_i2s0_SideGen_Switch On16000 invalid value");
            }
        }
        else
        {
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On16000MD3"))
            {
                ALOGE("Error: Audio_i2s0_SideGen_Switch On16000MD3 invalid value");
            }
        }
    }
    else if (SampleRate == 8000)
    {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On8000"))
        {
            ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
        }
    }

    ALOGD("-%s()", __FUNCTION__);
    return 0;
}

int AudioSmartPaController::dspOnBoardSpeakerOff()
{
    ALOGD("+%s()", __FUNCTION__);

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "Off"))
    {
        ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
    }

    if (isEchoReferenceSupport())
    {
        ALOGD("%s(), Audio_ExtCodec_EchoRef_Switch off", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ExtCodec_EchoRef_Switch"), "Off"))
        {
            ALOGE("Error: Audio_ExtCodec_EchoRef_Switch invalid value");
        }
    }

    if (mSmartPa.attribute.isApllNeeded)
    {
        ALOGD("+%s(), Audio_i2s0_hd_Switch off", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "Off"))
        {
            ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
        }
    }

    ALOGD("-%s()", __FUNCTION__);
    return 0;
}

}
