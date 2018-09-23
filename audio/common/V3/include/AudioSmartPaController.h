#ifndef ANDROID_AUDIO_SMART_PA_CONTROLLER_H
#define ANDROID_AUDIO_SMART_PA_CONTROLLER_H

#ifdef __cplusplus
#include "AudioLock.h"
#include "AudioType.h"
#include "AudioALSADriverUtility.h"
#endif

struct SmartPaOps
{
    int (*init)(struct SmartPa *smartPa);
    int (*speakerOn)(struct SmartPaRuntime *runtime);
    int (*speakerOff)();
    int (*deinit)();
};

struct SmartPaRuntime
{
    unsigned int sampleRate;
    int echoReferenceConfig;
};

struct SmartPaAttribute
{
    int haveDsp;
    int outputPort;
    unsigned int chipDelayUs;

    char spkLibPath[128];

    unsigned int supportedRateList[32];
    unsigned int supportedRateMax;
    unsigned int supportedRateMin;

    char codecCtlName[128];
    int isAlsaCodec;
    int isI2sInitBeforeAmpOn;
    int isApllNeeded;
};

struct SmartPa
{
    struct SmartPaOps ops;
    struct SmartPaRuntime runtime;
    struct SmartPaAttribute attribute;
};

#ifdef __cplusplus
namespace android
{
class AudioSmartPaController
{
    AudioSmartPaController();
    ~AudioSmartPaController();

    int init();
    int deinit();

    int initSmartPaAttribute();
    int initSmartPaRuntime();


    static AudioSmartPaController *mAudioSmartPaController;
    struct SmartPa mSmartPa;

    struct mixer *mMixer;

    void *mLibHandle;
    int (*mtk_smartpa_init)(struct SmartPa *smartPa);
public:
    static AudioSmartPaController *getInstance();

    int speakerOn(unsigned int sampleRate);
    int speakerOff();

    int dspOnBoardSpeakerOn(unsigned int SampleRate);
    int dspOnBoardSpeakerOff();

    enum sgen_mode_t getOutputPort();
    unsigned int getSmartPaDelayUs();

    unsigned int getMaxSupportedRate();
    unsigned int getMinSupportedRate();
    bool isRateSupported(unsigned int rate);

    bool isEchoReferenceSupport();
    bool isAlsaCodec();
    bool isApSideSpkProtect();
    bool isI2sInitBeforeAmpOn();
};

}
#endif
#endif
