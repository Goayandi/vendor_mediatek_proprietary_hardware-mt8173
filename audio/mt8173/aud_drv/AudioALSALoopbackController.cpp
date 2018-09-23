#define LOG_TAG "AudioALSALoopbackController"

#include "V3/include/AudioALSALoopbackController.h"
#include "V3/include/AudioBTCVSDControl.h"
#include "V3/include/AudioALSADriverUtility.h"
#include "AudioALSAStreamManager.h"
#include "AudioALSAHardwareResourceManager.h"
#include "WCNChipController.h"


static char const *const kBTCVSDDeviceName = "/dev/ebc";
#define AUD_DRV_IOC_MAGIC 'C'
#define ALLOCATE_FREE_BTCVSD_BUF _IOWR(AUD_DRV_IOC_MAGIC, 0xE0, unsigned int)
#define SET_BTCVSD_STATE         _IOWR(AUD_DRV_IOC_MAGIC, 0xE1, unsigned int)
#define GET_BTCVSD_STATE         _IOWR(AUD_DRV_IOC_MAGIC, 0xE2, unsigned int)

namespace android
{
//static android_audio_legacy::AudioStreamOut *streamOutput;
android_audio_legacy::AudioStreamOut *streamOutput = NULL;
static struct mixer *mMixerForBtLoopback;

AudioALSALoopbackController *AudioALSALoopbackController::mAudioALSALoopbackController = NULL;
AudioALSALoopbackController *AudioALSALoopbackController::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSALoopbackController == NULL)
    {
        mAudioALSALoopbackController = new AudioALSALoopbackController();
    }
    ASSERT(mAudioALSALoopbackController != NULL);
    return mAudioALSALoopbackController;
}


AudioALSALoopbackController::AudioALSALoopbackController() :
    mHardwareResourceManager(AudioALSAHardwareResourceManager::getInstance()),
    mPcmDL(NULL),
    mPcmUL(NULL),
    mMixer(AudioALSADriverUtility::getInstance()->getMixer()),
    mFd2(0),
    mBtLoopbackWithCodec(false),
    mBtLoopbackWithoutCodec(false),
    mUseBtCodec(false)
{
    mMixerForBtLoopback = AudioALSADriverUtility::getInstance()->getMixer();

    memset((void *)&mConfig, 0, sizeof(mConfig));
}


AudioALSALoopbackController::~AudioALSALoopbackController()
{

}


status_t AudioALSALoopbackController::open(const audio_devices_t output_devices, const audio_devices_t input_device)
{
    ALOGD("+%s(), output_devices = 0x%x, input_device = 0x%x", __FUNCTION__, output_devices, input_device);
    AudioAutoTimeoutLock _l(mLock);

    int retval = 0;
    struct mixer *alsa_mixer = AudioALSADriverUtility::getInstance()->getMixer();

    if (input_device == AUDIO_DEVICE_IN_BUILTIN_MIC ||
        input_device == AUDIO_DEVICE_IN_BACK_MIC) {
        if (IsAudioSupportFeature(AUDIO_SUPPORT_DMIC)) {
                if (output_devices == AUDIO_DEVICE_OUT_SPEAKER || output_devices == AUDIO_DEVICE_OUT_EARPIECE) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_DMIC_TO_SPK");
                }
                else if (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                         output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_DMIC_TO_HP");
                }
        } else {
                if (output_devices == AUDIO_DEVICE_OUT_SPEAKER || output_devices == AUDIO_DEVICE_OUT_EARPIECE) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_AMIC_TO_SPK");
                    //wisky-lxh@20170410,fix loopback main mic->speaker
                    usleep(1*1000 * 1000); //1s
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Audio_Amp_Switch"), "On");
                    usleep(1*1000 * 1000); //1s
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Speaker_Amp_Switch"), "On");
                    //end-wisky-lxh
                }
                else if (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                         output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_AMIC_TO_HP");
                    
                }
        }
    } else if (input_device == AUDIO_DEVICE_IN_WIRED_HEADSET) {
        if (output_devices == AUDIO_DEVICE_OUT_SPEAKER || output_devices == AUDIO_DEVICE_OUT_EARPIECE)
        {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_HEADSET_MIC_TO_SPK");
        }
        else if (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                 output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET)
        {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_HEADSET_MIC_TO_HP");
        }
    }

    ALOGE_IF(retval != 0, "%s Codec_Loopback_Select fail", __FUNCTION__);

    // TODO: add delay
    //usleep(200*1000);

    if (input_device == AUDIO_DEVICE_IN_BUILTIN_MIC ||
        input_device == AUDIO_DEVICE_IN_BACK_MIC) {
        if (IsAudioSupportFeature(AUDIO_SUPPORT_DMIC)) {
                if (output_devices == AUDIO_DEVICE_OUT_SPEAKER || output_devices == AUDIO_DEVICE_OUT_EARPIECE) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_DMIC_TO_SPK");
                }
                else if (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                         output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_DMIC_TO_HP");
                }
        } else {
                if (output_devices == AUDIO_DEVICE_OUT_SPEAKER || output_devices == AUDIO_DEVICE_OUT_EARPIECE) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_AMIC_TO_SPK");
                }
                else if (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                         output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_AMIC_TO_HP");
                }
        }
    } else if (input_device == AUDIO_DEVICE_IN_WIRED_HEADSET) {
        if (output_devices == AUDIO_DEVICE_OUT_SPEAKER || output_devices == AUDIO_DEVICE_OUT_EARPIECE)
        {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_HEADSET_MIC_TO_SPK");
        }
        else if (output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                 output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET)
        {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_HEADSET_MIC_TO_HP");
        }
    }

    ALOGE_IF(retval != 0, "%s AP_Loopback_Select fail", __FUNCTION__);

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSALoopbackController::close()
{
    AudioAutoTimeoutLock _l(mLock);
    struct mixer *alsa_mixer = AudioALSADriverUtility::getInstance()->getMixer();
    ALOGD("+%s()", __FUNCTION__);

    int retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "Codec_Loopback_Select"), "CODEC_LOOPBACK_NONE");
    ALOGE_IF(retval != 0, "%s AP_Loopback_Select fail", __FUNCTION__);

    retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(alsa_mixer, "AP_Loopback_Select"), "AP_LOOPBACK_NONE");
    ALOGE_IF(retval != 0, "%s AP_Loopback_Select fail", __FUNCTION__);

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSALoopbackController::SetApBTCodec(bool enable_codec)
{
    ALOGD("+%s(), enable_codec = %d", __FUNCTION__, enable_codec);
    mUseBtCodec = enable_codec;
    return NO_ERROR;
}

bool AudioALSALoopbackController::IsAPBTLoopbackWithCodec(void)
{
    //ALOGD("+%s(), mBtLoopbackWithCodec = %d", __FUNCTION__, mBtLoopbackWithCodec);
    return mBtLoopbackWithCodec;
}

status_t AudioALSALoopbackController::OpenAudioLoopbackControlFlow(const audio_devices_t input_device, const audio_devices_t output_device)
{
    // check BT device
    const bool bt_device_on = android_audio_legacy::AudioSystem::isBluetoothScoDevice((android_audio_legacy::AudioSystem::audio_devices)output_device);

    ALOGD("+%s(), input_device = 0x%x, output_device = 0x%x", __FUNCTION__, input_device, output_device);
    ALOGD("+%s(), bt_device_on = %d, mUseBtCodec = %d, mBtLoopbackWithoutCodec: %d, mBtLoopbackWithCodec: %d",
          __FUNCTION__, bt_device_on, mUseBtCodec, mBtLoopbackWithoutCodec, mBtLoopbackWithCodec);
    if (bt_device_on == true)
    {
        // DAIBT
        if (WCNChipController::GetInstance()->BTUseCVSDRemoval() == true)
        {
        if (!mUseBtCodec)
        {
            mBtLoopbackWithoutCodec = 1;
            mFd2 = 0;
#ifndef MTK_SUPPORT_BTCVSD_ALSA
            mFd2 = ::open(kBTCVSDDeviceName, O_RDWR);
            ALOGD("+%s(), CVSD AP loopback without codec, mFd2: %d, AP errno: %d", __FUNCTION__, mFd2, errno);
            ::ioctl(mFd2, ALLOCATE_FREE_BTCVSD_BUF, 0); //allocate TX working buffers in kernel
            ::ioctl(mFd2, ALLOCATE_FREE_BTCVSD_BUF, 2); //allocate TX working buffers in kernel
            ::ioctl(mFd2, SET_BTCVSD_STATE, BT_SCO_TXSTATE_DIRECT_LOOPBACK); //set state to kernel
#else
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixerForBtLoopback, "BT_DIRECT_LOOPBACK"), "On"))
            {
                ALOGE("Error: BT_DIRECT_LOOPBACK invalid value");
            }
            else
            {
                ALOGD("+%s(), CVSD AP loopback without codec, BT_DIRECT_LOOPBACK==On", __FUNCTION__);
            }
#endif
        }
        else
        {
//#if defined(BTCVSD_LOOPBACK_WITH_CODEC)
#if 1 //0902
            int format = AUDIO_FORMAT_PCM_16_BIT;
            uint32_t channels = AUDIO_CHANNEL_OUT_MONO;
            uint32_t sampleRate = 8000;
            status_t status;
            mBtLoopbackWithCodec = 1;
            streamOutput = AudioALSAStreamManager::getInstance()->openOutputStream(output_device, &format, &channels, &sampleRate, &status);
            ALOGD("+%s(), CVSD AP loopback with codec, streamOutput: %p", __FUNCTION__, streamOutput);
            mBTCVSDLoopbackThread = new AudioMTKLoopbackThread();
            if (mBTCVSDLoopbackThread.get())
            {
                mBTCVSDLoopbackThread->run("mBTCVSDLoopbackThread");
            }
#endif
        }
        }
        else
        {
            //mAudioDigitalInstance->SetinputConnection(AudioDigitalType::Connection, AudioDigitalType::I02, AudioDigitalType::O02); // DAIBT_IN -> DAIBT_OUT
            //SetDAIBTAttribute(dl_samplerate);
            //mAudioDigitalInstance->SetDAIBTEnable(true);
        }
    }

    ALOGD("-%s()",__FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSALoopbackController::CloseAudioLoopbackControlFlow(void)
{
    AudioAutoTimeoutLock _l(mLock);
    ALOGD("+%s()", __FUNCTION__);

    //const bool bt_device_on = android_audio_legacy::AudioSystem::isBluetoothScoDevice((android_audio_legacy::AudioSystem::audio_devices)output_device);
    bool bt_device_on = true;

    ALOGD("%s(), bt_device_on = %d, mBtLoopbackWithoutCodec: %d, mBtLoopbackWithCodec: %d",
          __FUNCTION__, bt_device_on, mBtLoopbackWithoutCodec, mBtLoopbackWithCodec);

    if (bt_device_on)
    {
        if (WCNChipController::GetInstance()->BTUseCVSDRemoval() == true)
        {
            if (mBtLoopbackWithoutCodec)
            {
#ifndef MTK_SUPPORT_BTCVSD_ALSA
                ::ioctl(mFd2, ALLOCATE_FREE_BTCVSD_BUF, 1); //allocate TX working buffers in kernel
                ::ioctl(mFd2, ALLOCATE_FREE_BTCVSD_BUF, 3); //allocate TX working buffers in kernel
                ::ioctl(mFd2, SET_BTCVSD_STATE, BT_SCO_TXSTATE_IDLE); //set state to kernel
#else
                if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixerForBtLoopback, "BT_DIRECT_LOOPBACK"), "Off"))
                {
                    ALOGE("Error: BT_DIRECT_LOOPBACK invalid value");
                }
                else
                {
                    ALOGD("%s(), CVSD AP loopback without codec, BT_DIRECT_LOOPBACK==Off", __FUNCTION__);
                }
#endif
                mBtLoopbackWithoutCodec = 0;
            }
            else if (mBtLoopbackWithCodec)
            {
//#if defined(BTCVSD_LOOPBACK_WITH_CODEC)
#if 1 //0902
                streamOutput->standby();
                if (mBTCVSDLoopbackThread.get())
                {
                    int ret = 0;
                    //ret = mBTCVSDLoopbackThread->requestExitAndWait();
                    //if (ret == WOULD_BLOCK)
                    {
                        mBTCVSDLoopbackThread->requestExit();
                    }
                    mBTCVSDLoopbackThread.clear();
                }
                AudioALSAStreamManager::getInstance()->closeOutputStream(streamOutput);
                mBtLoopbackWithCodec = 0;
#endif
            }
        }
        else
        {
            //mAudioDigitalInstance->SetDAIBTEnable(false);
            //mAudioDigitalInstance->SetinputConnection(AudioDigitalType::DisConnect, AudioDigitalType::I02, AudioDigitalType::O02); // DAIBT_IN -> DAIBT_OUT
        }
    }

    ALOGD("-%s()", __FUNCTION__);

    return NO_ERROR;
}


void AudioALSALoopbackController::setLoopbackUseLCh(bool enable)
{
    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    int retval = 0;

    ctl = mixer_get_ctl_by_name(mMixer, "LPBK_IN_USE_LCH");

    if (ctl == NULL)
    {
        ALOGE("LPBK_IN_USE_LCH not support");
        return;
    }

    if (enable == true)
    {
        retval = mixer_ctl_set_enum_by_string(ctl, "On");
        ASSERT(retval == 0);
    }
    else
    {
        retval = mixer_ctl_set_enum_by_string(ctl, "Off");
        ASSERT(retval == 0);
    }
}


#if 1 //0902

extern void CVSDLoopbackResetBuffer();
extern void CVSDLoopbackReadDataDone(uint32_t len);
extern void CVSDLoopbackGetReadBuffer(uint8_t **buffer, uint32_t *buf_len);
extern int32_t CVSDLoopbackGetDataCount();

AudioALSALoopbackController::AudioMTKLoopbackThread::AudioMTKLoopbackThread()
{
    ALOGD("BT_SW_CVSD AP loopback AudioMTKLoopbackThread constructor");
}

AudioALSALoopbackController::AudioMTKLoopbackThread::~AudioMTKLoopbackThread()
{
    ALOGD("BT_SW_CVSD AP loopback ~AudioMTKLoopbackThread");
}

void AudioALSALoopbackController::AudioMTKLoopbackThread::onFirstRef()
{
    ALOGD("BT_SW_CVSD AP loopback AudioMTKLoopbackThread::onFirstRef");
    run(mName, ANDROID_PRIORITY_URGENT_AUDIO);
}

status_t  AudioALSALoopbackController::AudioMTKLoopbackThread::readyToRun()
{
    ALOGD("BT_SW_CVSD AP loopback AudioMTKLoopbackThread::readyToRun()");
    return NO_ERROR;
}

bool AudioALSALoopbackController::AudioMTKLoopbackThread::threadLoop()
{
    uint8_t *pReadBuffer;
    uint32_t uReadByte, uWriteDataToBT;
    CVSDLoopbackResetBuffer();
    while (!(exitPending() == true))
    {
        ALOGD("BT_SW_CVSD AP loopback threadLoop(+)");
        uWriteDataToBT = 0;
        CVSDLoopbackGetReadBuffer(&pReadBuffer, &uReadByte);
        uReadByte &= 0xFFFFFFFE;
        if (uReadByte)
        {
            uWriteDataToBT = streamOutput->write(pReadBuffer, uReadByte);
            CVSDLoopbackReadDataDone(uWriteDataToBT);
        }
        else
        {
            usleep(5 * 1000); //5ms
        }
        ALOGD("BT_SW_CVSD AP loopback threadLoop(-), uReadByte: %d, uWriteDataToBT: %d", uReadByte, uWriteDataToBT);
    }
    ALOGD("BT_SW_CVSD AP loopback threadLoop exit");
    return false;
}

#endif



} // end of namespace android
