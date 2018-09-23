#define LOG_TAG "AudioALSAHardware"
//#define LOG_NDEBUG 0

#include "AudioALSAHardware.h"

#include <media/AudioSystem.h>
#include <binder/IServiceManager.h>
#include <media/IAudioPolicyService.h>

#include <CFG_AUDIO_File.h>
#include <AudioCustParam.h>


#include "AudioALSAStreamManager.h"

#include "V3/include/AudioFtm.h"
#include "V3/include/LoopbackManager.h"

#include "AudioALSAStreamIn.h"
#include "AudioALSAStreamOut.h"
#include "AudioMTKHardwareCommand.h"
#include "V3/include/AudioVolumeFactory.h"

#include "AudioALSAParamTuner.h"
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
#include "SpeechDriverFactory.h"
#include "SpeechEnhancementController.h"
#endif
#include "V3/include/AudioALSADeviceParser.h"
#include "WCNChipController.h"
#include "V3/include/AudioSpeechEnhanceInfo.h"
#include "V3/include/AudioALSAFMController.h"

#include "AudioToolkit.h"

namespace android
{

/*==============================================================================
 *                     setParameters keys
 *============================================================================*/
// Phone Call Related
static String8 keySetVTSpeechCall       = String8("SetVTSpeechCall");
static String8 keyBtHeadsetNrec         = String8("bt_headset_nrec");
// BT WB
static String8 keySetBTMode     = String8(AUDIO_PARAMETER_KEY_BT_SCO_WB);

// FM Rx Related
static String8 keySetFmEnable           = String8("AudioSetFmDigitalEnable");
static String8 keyGetFmEnable           = String8("GetFmEnable");
static String8 keySetFmVolume           = String8("SetFmVolume");

// FM Tx Related
static String8 keyGetFmTxEnable         = String8("GetFmTxEnable");
static String8 keySetFmTxEnable         = String8("SetFmTxEnable");

static String8 keyFMRXForceDisableFMTX  = String8("FMRXForceDisableFMTX");

// TDM record Related
static String8 keySetTDMRecEnable       = String8("SetTDMRecEnable");


//record left/right channel switch
//only support on dual MIC for switch LR input channel for video record when the device rotate
static String8 keyLR_ChannelSwitch = String8("LRChannelSwitch");

// BesRecord Related
static String8 keyHDREC_SET_VOICE_MODE = String8("HDREC_SET_VOICE_MODE");
static String8 keyHDREC_SET_VIDEO_MODE = String8("HDREC_SET_VIDEO_MODE");

static String8 keyGET_AUDIO_VOLUME_VER = String8("GET_AUDIO_VOLUME_VERSION");

// TTY
static String8 keySetTtyMode     = String8("tty_mode");

//AudioCmdHandlerService
static String8 keySetBuffer = String8("SetBuffer");
static String8 keyGetBuffer = String8("GetBuffer");
static String8 keySetCmd = String8("SetCmd");
static String8 keyGetCmd = String8("GetCmd");
#define MAX_BYTE_AUDIO_BUFFER 3000

// Audio Tool related
//<---for audio tool(speech/ACF/HCF/DMNR/HD/Audiotaste calibration) and HQA
static String8 keySpeechParams_Update = String8("UpdateSpeechParameter");
static String8 keySpeechVolume_Update = String8("UpdateSphVolumeParameter");
#if defined(MTK_DUAL_MIC_SUPPORT) || defined(MTK_AUDIO_HD_REC_SUPPORT)
static String8 keyDualMicParams_Update = String8("UpdateDualMicParameters");
static String8 keyDualMicRecPly = String8("DUAL_MIC_REC_PLAY");
static String8 keyDUALMIC_IN_FILE_NAME = String8("DUAL_MIC_IN_FILE_NAME");
static String8 keyDUALMIC_OUT_FILE_NAME = String8("DUAL_MIC_OUT_FILE_NAME");
static String8 keyDUALMIC_GET_GAIN = String8("DUAL_MIC_GET_GAIN");
static String8 keyDUALMIC_SET_UL_GAIN = String8("DUAL_MIC_SET_UL_GAIN");
static String8 keyDUALMIC_SET_DL_GAIN = String8("DUAL_MIC_SET_DL_GAIN");
static String8 keyDUALMIC_SET_HSDL_GAIN = String8("DUAL_MIC_SET_HSDL_GAIN");
static String8 keyDUALMIC_SET_UL_GAIN_HF = String8("DUAL_MIC_SET_UL_GAIN_HF");
#endif
static String8 keyBesRecordParams_Update = String8("UpdateBesRecordParameters");
static String8 keyMusicPlusSet      = String8("SetMusicPlusStatus");
static String8 keyMusicPlusGet      = String8("GetMusicPlusStatus");
static String8 keyHiFiDACSet      = String8("SetHiFiDACStatus");
static String8 keyHiFiDACGet      = String8("GetHiFiDACStatus");
static String8 keyHDRecTunningEnable    = String8("HDRecTunningEnable");
static String8 keyHDRecVMFileName   = String8("HDRecVMFileName");
static String8 keyACFHCF_Update = String8("UpdateACFHCFParameters");
static String8 keyBesLoudnessSet      = String8("SetBesLoudnessStatus");
static String8 keyBesLoudnessGet      = String8("GetBesLoudnessStatus");

#ifdef AUDIO_HQA_SUPPORT
// for Audio HQA use
static String8 keyHQA_RDMIC_P1   = String8("HQA_RDMIC_P1");
static String8 keyHQA_PGAGAIN_P1 = String8("HQA_PGAGAIN_P1");
static String8 keyHQA_PGAGAIN_P2 = String8("HQA_PGAGAIN_P2");
static String8 keyHQA_VDPG_P1 = String8("HQA_VDPG_P1");
static String8 keyHQA_VDPG_P2 = String8("HQA_VDPG_P2");
static String8 keyHQA_VDPG_P3 = String8("HQA_VDPG_P3");
static String8 keyHQA_AUDLINEPG_P1 = String8("HQA_AUDLINEPG_P1");
static String8 keyHQA_MICVUPG_P1 = String8("HQA_MICVUPG_P1");
static String8 keyHQA_MICVUPG_P2 = String8("HQA_MICVUPG_P2");
static String8 keyHQA_MEDPLY_P1 = String8("HQA_MEDPLY_P1");
static String8 keyHQA_I2SREC = String8("HQA_I2SREC");

static String8 keyHQA_AMP_MODESEL  = String8("HQA_AMP_MODESEL");
static String8 keyHQA_AMP_AMPEN    = String8("HQA_AMP_AMPEN");
static String8 keyHQA_AMP_AMPVOL   = String8("HQA_AMP_AMPVOL");
static String8 keyHQA_AMP_RECEIVER = String8("HQA_AMP_RECEIVER");
static String8 keyHQA_AMP_RECGAIN  = String8("HQA_AMP_RECGAIN");

static String8 keyHQA_I2S_OUTPUT_PLAY = String8("HQA_I2S_OUTPUT_PLAY");
static String8 keyHQA_I2S_OUTPUT_STOP = String8("HQA_I2S_OUTPUT_STOP");

#endif


// Dual Mic Noise Reduction, DMNR for Receiver
static String8 keyEnable_Dual_Mic_Setting = String8("Enable_Dual_Mic_Setting");
static String8 keyGet_Dual_Mic_Setting    = String8("Get_Dual_Mic_Setting");

// Dual Mic Noise Reduction, DMNR for Loud Speaker
static String8 keySET_LSPK_DMNR_ENABLE = String8("SET_LSPK_DMNR_ENABLE");
static String8 keyGET_LSPK_DMNR_ENABLE = String8("GET_LSPK_DMNR_ENABLE");

// Voice Clarity Engine, VCE
static String8 keySET_VCE_ENABLE = String8("SET_VCE_ENABLE");
static String8 keyGET_VCE_ENABLE = String8("GET_VCE_ENABLE");
static String8 keyGET_VCE_STATUS = String8("GET_VCE_STATUS"); // old name, rename to GET_VCE_ENABLE, but still reserve it

// Magic Conference Call
static String8 keySET_MAGIC_CON_CALL_ENABLE = String8("SET_MAGIC_CON_CALL_ENABLE");
static String8 keyGET_MAGIC_CON_CALL_ENABLE = String8("GET_MAGIC_CON_CALL_ENABLE");

//VoIP
//VoIP Dual Mic Noise Reduction, DMNR for Receiver
static String8 keySET_VOIP_RECEIVER_DMNR_ENABLE = String8("SET_VOIP_RECEIVER_DMNR_ENABLE");
static String8 keyGET_VOIP_RECEIVER_DMNR_ENABLE    = String8("GET_VOIP_RECEIVER_DMNR_ENABLE");

//VoIP Dual Mic Noise Reduction, DMNR for Loud Speaker
static String8 keySET_VOIP_LSPK_DMNR_ENABLE = String8("SET_VOIP_LSPK_DMNR_ENABLE");
static String8 keyGET_VOIP_LSPK_DMNR_ENABLE = String8("GET_VOIP_LSPK_DMNR_ENABLE");

//Vibration Speaker usage
static String8 keySET_VIBSPK_ENABLE = String8("SET_VIBSPK_ENABLE");
static String8 keySET_VIBSPK_RAMPDOWN = String8("SET_VIBSPK_RAMPDOWN");

// Voice UnLock
static String8 keyGetCaptureDropTime = String8("GetCaptureDropTime");

// low latency
static String8 keySCREEN_STATE = String8("screen_state");

// Loopbacks
static String8 keySET_LOOPBACK_USE_LOUD_SPEAKER = String8("SET_LOOPBACK_USE_LOUD_SPEAKER");
static String8 keySET_LOOPBACK_TYPE = String8("SET_LOOPBACK_TYPE");
static String8 keySET_LOOPBACK_MODEM_DELAY_FRAMES = String8("SET_LOOPBACK_MODEM_DELAY_FRAMES");

static String8 keyMTK_AUDENH_SUPPORT = String8("MTK_AUDENH_SUPPORT");
static String8 keyMTK_TTY_SUPPORT = String8("MTK_TTY_SUPPORT");
static String8 keyMTK_WB_SPEECH_SUPPORT = String8("MTK_WB_SPEECH_SUPPORT");
static String8 keyMTK_DUAL_MIC_SUPPORT = String8("MTK_DUAL_MIC_SUPPORT");
static String8 keyMTK_AUDIO_HD_REC_SUPPORT = String8("MTK_AUDIO_HD_REC_SUPPORT");
static String8 keyMTK_BESLOUDNESS_SUPPORT = String8("MTK_BESLOUDNESS_SUPPORT");
static String8 keyMTK_BESSURROUND_SUPPORT = String8("MTK_BESSURROUND_SUPPORT");
static String8 keyMTK_HDMI_MULTI_CHANNEL_SUPPORT = String8("MTK_HDMI_MULTI_CHANNEL_SUPPORT");


enum
{
    Normal_Coef_Index,
    Headset_Coef_Index,
    Handfree_Coef_Index,
    VOIPBT_Coef_Index,
    VOIPNormal_Coef_Index,
    VOIPHandfree_Coef_Index,
    AUX1_Coef_Index,
    AuX2_Coef_Index
};

#define FM_DEVICE_TO_DEVICE_SUPPORT_OUTPUT_DEVICES (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE)


/*==============================================================================
 *                     setParameters() keys for common
 *============================================================================*/

AudioALSAHardware::AudioALSAHardware() :
    mStreamManager(AudioALSAStreamManager::getInstance()),
    mAudioSpeechEnhanceInfoInstance(AudioSpeechEnhanceInfo::getInstance()),
    mAudioALSAVolumeController(AudioVolumeFactory::CreateAudioVolumeController()),
    mAudioALSAParamTunerInstance(AudioALSAParamTuner::getInstance()),
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    mSpeechPhoneCallController(AudioALSASpeechPhoneCallController::getInstance()),
#endif
    mAudioAlsaDeviceInstance(AudioALSADeviceParser::getInstance()),
    mFmTxEnable(false),
    mUseTuningVolume(false),
    mNextUniqueId(1)
{
    ALOGD("%s()", __FUNCTION__);

    // Use Audio Patch For Fm
    char property_value[PROPERTY_VALUE_MAX];
    property_get("persist.af.audio_patch_fm", property_value, "1");
    mUseAudioPatchForFm = (bool)atoi(property_value);
    valAudioCmd = 0;
    pAudioBuffer = NULL;
    pAudioBuffer = new char [MAX_BYTE_AUDIO_BUFFER];
    memset(pAudioBuffer, 0, MAX_BYTE_AUDIO_BUFFER);
    // for bybpass audio hw
    property_get("audio.hw.bypass", property_value, "0");
    mAudioHWBypass = atoi(property_value);
    if (mAudioHWBypass)
    {
        mStreamManager->setAllOutputStreamsSuspend(true, true);
    }

    memset((void*)&VolCache, 0, sizeof(VolCache));
}

AudioALSAHardware::~AudioALSAHardware()
{
    ALOGD("%s()", __FUNCTION__);
    if (mStreamManager != NULL) { delete mStreamManager; }
    if (pAudioBuffer != NULL) { delete pAudioBuffer; pAudioBuffer = NULL;}
    while (mAudioHalPatchVector.size())
    {
        AudioHalPatch *patch = mAudioHalPatchVector.valueAt(0);
        mAudioHalPatchVector.removeItemsAt(0);
        if (patch) delete patch;
    }
}

status_t AudioALSAHardware::initCheck()
{
    ALOGD("%s()", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioALSAHardware::setVoiceVolume(float volume)
{
    return mStreamManager->setVoiceVolume(volume);
}

status_t AudioALSAHardware::setMasterVolume(float volume)
{
    return mStreamManager->setMasterVolume(volume);
}

status_t AudioALSAHardware::setMode(int mode)
{
    return mStreamManager->setMode(static_cast<audio_mode_t>(mode));
}

status_t AudioALSAHardware::setMicMute(bool state)
{
    return mStreamManager->setMicMute(state);
}

status_t AudioALSAHardware::getMicMute(bool *state)
{
    if (state != NULL) { *state = mStreamManager->getMicMute(); }
    return NO_ERROR;
}

status_t AudioALSAHardware::setParameters(const String8 &keyValuePairs)
{
    ALOGV("+%s(): %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);

    /// parse key value pairs
    status_t status = NO_ERROR;
    int value = 0;
    float value_float = 0.0;
    String8 value_str;

    if (param.getInt(String8("ECCCI_Test"), value) == NO_ERROR)
    {
        param.remove(String8("ECCCI_Test"));
        #ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        SpeechDriverFactory::GetInstance()->GetSpeechDriver()->SetEnh1DownlinkGain(value);
        #endif
    }

    //AudioCmdHandlerService usage
    if (param.get(keySetCmd, value_str) == NO_ERROR)
    {
        param.remove(keySetCmd);
        size_t sz_in = value_str.size();
        size_t sz_out;//output buffer lenth
        size_t sz_dec;//decoded buffer length
        bool bEncode;

        // Decode string to char
        sz_out = Base64_OutputSize(false, sz_in);
        ALOGD("%s(), before decode (%s), sz_in(%d), sz_out(%d)", __FUNCTION__, value_str.string(), sz_in, sz_out);

        //allocate output buffer for decode
        unsigned char *buf_dec = new unsigned char[sz_out];


        sz_dec = Base64_Decode(value_str.string(), buf_dec, sz_in);
        //error check and alert
        if (sz_dec == 0)
        {
            ALOGE("%s(), Decode Error!!!after decode (%s), sz_in(%d), sz_out(%d), sz_dec(%d)", __FUNCTION__, buf_dec, sz_in, sz_out, sz_dec);
            status = BAD_VALUE;
        }
        else
        {
            ALOGD("%s(), after decode (%s), sz_in(%d), sz_dec(%d)", __FUNCTION__, buf_dec, sz_in, sz_dec);
        }
        for (int ii = 0; ii < sz_dec; ii++)
        {
            ALOGV("%s(), buf_dec(0x%x)", __FUNCTION__, *(buf_dec + ii));
        }
        int type_AudioData = *((int *) buf_dec);
        int par_AudioData = *(((int *) buf_dec) + 1);
        ALOGD("%s(), after decode (%s), type_AudioData(0x%x), par_AudioData(%d)", __FUNCTION__, buf_dec, type_AudioData, par_AudioData);

        SetAudioCommand(type_AudioData, par_AudioData);
        if (buf_dec != NULL)
        {
            delete[] buf_dec;
        }
        status = NO_ERROR;
        return status;


    }
    if (param.get(keySetBuffer, value_str) == NO_ERROR)
    {
        param.remove(keySetBuffer);
        size_t sz_in = value_str.size();
        size_t sz_out;//output buffer lenth
        size_t sz_dec;//decoded buffer length
        bool bEncode;

        // Decode string to char
        sz_out = Base64_OutputSize(false, sz_in);
        ALOGD("%s(), before decode (%s), sz_in(%d), sz_out(%d)", __FUNCTION__, value_str.string(), sz_in, sz_out);

        //allocate output buffer for decode
        unsigned char *buf_dec = new unsigned char[sz_out];

        sz_dec = Base64_Decode(value_str.string(), buf_dec, sz_in);
        //error check and alert
        if (sz_dec == 0)
        {
            ALOGE("%s(), Decode Error!!!after decode (%s), sz_in(%d), sz_out(%d), sz_dec(%d)", __FUNCTION__, buf_dec, sz_in, sz_out, sz_dec);
            status = BAD_VALUE;
        }
        else
        {
            ALOGD("%s(), after decode (%s), sz_in(%d), sz_dec(%d)", __FUNCTION__, buf_dec, sz_in, sz_dec);
        }
        for (int ii = 0; ii < sz_dec; ii++)
        {
            ALOGV("%s(), buf_dec(0x%x)", __FUNCTION__, *(buf_dec + ii));
        }
        int type_AudioData = *((int *) buf_dec);
        int sz_AudioData = *(((int *) buf_dec) + 1);
        ALOGD("%s(), after decode (%s), type_AudioData(0x%x), sz_AudioData(%d)", __FUNCTION__, buf_dec, type_AudioData, sz_AudioData);
        SetAudioData(type_AudioData, sz_AudioData, buf_dec + 8);
        if (buf_dec != NULL)
        {
            delete[] buf_dec;
        }
        status = NO_ERROR;
        return status;

    }

    // VT call (true) / Voice call (false)
    if (param.getInt(keySetVTSpeechCall, value) == NO_ERROR)
    {
        param.remove(keySetVTSpeechCall);
        mStreamManager->setVtNeedOn((bool)value);
    }

    // FM enable
    if (param.getInt(keySetFmEnable, value) == NO_ERROR)
    {
        param.remove(keySetFmEnable);
        if (mUseAudioPatchForFm == false)
        {
            mStreamManager->setFmEnable((bool)value);
        }
    }

    // Set FM volume
    if (param.getFloat(keySetFmVolume, value_float) == NO_ERROR)
    {
        param.remove(keySetFmVolume);
        if (mUseAudioPatchForFm == false)
        {
            mStreamManager->setFmVolume(value_float);
        }
    }

    // Set FM Tx enable
    if (param.getInt(keySetFmTxEnable, value) == NO_ERROR)
    {
        param.remove(keySetFmTxEnable);
        mFmTxEnable = (bool)value;
    }

    // Force dusable FM Tx due to FM Rx is ready to play
    if (param.getInt(keyFMRXForceDisableFMTX, value) == NO_ERROR)
    {
        param.remove(keyFMRXForceDisableFMTX);
        if (value == true)
        {
            mFmTxEnable = false;
        }
    }

    if (param.getInt(keyMusicPlusSet, value) == NO_ERROR)
    {
        param.remove(keyMusicPlusSet);
        mStreamManager->SetMusicPlusStatus(value ? true : false);
    }

    // set the LR channel switch
    if (param.getInt(keyLR_ChannelSwitch, value) == NO_ERROR)
    {
        ALOGD("keyLR_ChannelSwitch=%d", value);
        bool bIsLRSwitch = value;
        AudioALSAHardwareResourceManager::getInstance()->setMicInverse(bIsLRSwitch);
        param.remove(keyLR_ChannelSwitch);
    }

    // BesRecord Mode setting
    if (param.getInt(keyHDREC_SET_VOICE_MODE, value) == NO_ERROR)
    {
        ALOGD("HDREC_SET_VOICE_MODE=%d", value); // Normal, Indoor, Outdoor,
        param.remove(keyHDREC_SET_VOICE_MODE);
        //Get and Check Voice/Video Mode Offset
        AUDIO_HD_RECORD_SCENE_TABLE_STRUCT hdRecordSceneTable;
        GetHdRecordSceneTableFromNV(&hdRecordSceneTable);
        if (value < hdRecordSceneTable.num_voice_rec_scenes)
        {
            int32_t BesRecScene = value + 1;//1:cts verifier offset
            mAudioSpeechEnhanceInfoInstance->SetBesRecScene(BesRecScene);
        }
        else
        {
            ALOGE("HDREC_SET_VOICE_MODE=%d exceed max value(%d)\n", value, hdRecordSceneTable.num_voice_rec_scenes);
        }
    }

    if (param.getInt(keyHDREC_SET_VIDEO_MODE, value) == NO_ERROR)
    {
        ALOGD("HDREC_SET_VIDEO_MODE=%d", value); // Normal, Indoor, Outdoor,
        param.remove(keyHDREC_SET_VIDEO_MODE);
        //Get and Check Voice/Video Mode Offset
        AUDIO_HD_RECORD_SCENE_TABLE_STRUCT hdRecordSceneTable;
        GetHdRecordSceneTableFromNV(&hdRecordSceneTable);
        if (value < hdRecordSceneTable.num_video_rec_scenes)
        {
            uint32_t offset = hdRecordSceneTable.num_voice_rec_scenes + 1;//1:cts verifier offset
            int32_t BesRecScene = value + offset;
            mAudioSpeechEnhanceInfoInstance->SetBesRecScene(BesRecScene);
        }
        else
        {
            ALOGE("HDREC_SET_VIDEO_MODE=%d exceed max value(%d)\n", value, hdRecordSceneTable.num_video_rec_scenes);
        }
    }

#ifdef BTNREC_DECIDED_BY_DEVICE
    // BT NREC on/off
    if (param.get(keyBtHeadsetNrec, value_str) == NO_ERROR)
    {
        param.remove(keyBtHeadsetNrec);
        if (value_str == "on")
        {
            mStreamManager->SetBtHeadsetNrec(true);
        }
        else if (value_str == "off")
        {
            mStreamManager->SetBtHeadsetNrec(false);
        }
    }
#endif
    if (param.get(keySetBTMode, value_str) == NO_ERROR)
    {
        param.remove(keySetBTMode);
        if (value_str == "on")
        {
            WCNChipController::GetInstance()->SetBTCurrentSamplingRateNumber(16000);
            #ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
            mSpeechPhoneCallController->setBTMode(true);
            #endif
        }
        else if (value_str == "off")
        {
            WCNChipController::GetInstance()->SetBTCurrentSamplingRateNumber(8000);
            #ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
            mSpeechPhoneCallController->setBTMode(false);
            #endif
        }
    }

    if (param.getInt(keyBesRecordParams_Update, value) == NO_ERROR)
    {
        param.remove(keyBesRecordParams_Update);
        mAudioSpeechEnhanceInfoInstance->UpdateBesRecordParams();
    }

    //<---for audio tool(speech/ACF/HCF/DMNR/HD/Audiotaste calibration)
    // calibrate speech parameters
    if (param.getInt(keySpeechParams_Update, value) == NO_ERROR)
    {
        ALOGD("setParameters Update Speech Parames");

        mStreamManager->UpdateSpeechParams(value);
        param.remove(keySpeechParams_Update);
    }
#if defined(MTK_DUAL_MIC_SUPPORT)
    if (param.getInt(keyDualMicParams_Update, value) == NO_ERROR)
    {
        param.remove(keyDualMicParams_Update);
        mStreamManager->UpdateDualMicParams();
    }
#endif
    // calibrate speech volume
    if (param.getInt(keySpeechVolume_Update, value) == NO_ERROR)
    {
        ALOGD("setParameters Update Speech volume");
        mStreamManager->UpdateSpeechVolume();
        param.remove(keySpeechVolume_Update);
    }

    if (param.getInt(keyBesLoudnessSet, value) == NO_ERROR)
    {
        param.remove(keyBesLoudnessSet);
        mStreamManager->SetBesLoudnessStatus(value ? true : false);
    }

    // ACF/HCF parameters calibration
    if (param.getInt(keyACFHCF_Update, value) == NO_ERROR)
    {
        param.remove(keyACFHCF_Update);
        mStreamManager->UpdateACFHCF(value);
    }
    // HD recording and DMNR calibration
#if defined(MTK_DUAL_MIC_SUPPORT) || defined(MTK_AUDIO_HD_REC_SUPPORT)
    if (param.getInt(keyDualMicRecPly, value) == NO_ERROR)
    {
        unsigned short cmdType = value & 0x000F;
        bool bWB = (value >> 4) & 0x000F;
        status_t ret = NO_ERROR;
        switch (cmdType)
        {
                //dmnr tunning at ap side
            case DUAL_MIC_REC_PLAY_STOP:
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(false, bWB, OUTPUT_DEVICE_RECEIVER, RECPLAY_MODE);
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(false);
                break;
            case DUAL_MIC_REC:
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(true);
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(true, bWB, OUTPUT_DEVICE_RECEIVER, RECONLY_MODE);
                break;
            case DUAL_MIC_REC_PLAY:
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(true);
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(true, bWB, OUTPUT_DEVICE_RECEIVER, RECPLAY_MODE);
                break;
            case DUAL_MIC_REC_PLAY_HS:
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(true);
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(true, bWB, OUTPUT_DEVICE_HEADSET, RECPLAY_MODE);
                break;
            case DUAL_MIC_REC_HF:
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(true);
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(true, bWB, OUTPUT_DEVICE_RECEIVER, RECONLY_HF_MODE);
                break;
            case DUAL_MIC_REC_PLAY_HF:
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(true);
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(true, bWB, OUTPUT_DEVICE_RECEIVER, RECPLAY_HF_MODE);
                break;
            case DUAL_MIC_REC_PLAY_HF_HS:
                mAudioSpeechEnhanceInfoInstance->SetAPDMNRTuningEnable(true);
                ret = mAudioALSAParamTunerInstance->enableDMNRAtApSide(true, bWB, OUTPUT_DEVICE_HEADSET, RECPLAY_HF_MODE);
                break;
            default:
                ret = BAD_VALUE;
                break;
        }
        if (ret == NO_ERROR)
        {
            param.remove(keyDualMicRecPly);
        }
    }

    if (param.get(keyDUALMIC_IN_FILE_NAME, value_str) == NO_ERROR)
    {
        if (mAudioALSAParamTunerInstance->setPlaybackFileName(value_str.string()) == NO_ERROR)
        {
            param.remove(keyDUALMIC_IN_FILE_NAME);
        }
    }

    if (param.get(keyDUALMIC_OUT_FILE_NAME, value_str) == NO_ERROR)
    {
        if (mAudioALSAParamTunerInstance->setRecordFileName(value_str.string()) == NO_ERROR)
        {
#ifndef DMNR_TUNNING_AT_MODEMSIDE
            if (mAudioSpeechEnhanceInfoInstance->SetBesRecVMFileName(value_str.string()) == NO_ERROR)
#endif
                param.remove(keyDUALMIC_OUT_FILE_NAME);
        }
    }

    if (param.getInt(keyDUALMIC_SET_UL_GAIN, value) == NO_ERROR)
    {
        if (mAudioALSAParamTunerInstance->setDMNRGain(AUD_MIC_GAIN, value) == NO_ERROR)
        {
            param.remove(keyDUALMIC_SET_UL_GAIN);
        }
    }

    if (param.getInt(keyDUALMIC_SET_DL_GAIN, value) == NO_ERROR)
    {
        if (mAudioALSAParamTunerInstance->setDMNRGain(AUD_RECEIVER_GAIN, value) == NO_ERROR)
        {
            param.remove(keyDUALMIC_SET_DL_GAIN);
        }
    }

    if (param.getInt(keyDUALMIC_SET_HSDL_GAIN, value) == NO_ERROR)
    {
        if (mAudioALSAParamTunerInstance->setDMNRGain(AUD_HS_GAIN, value) == NO_ERROR)
        {
            param.remove(keyDUALMIC_SET_HSDL_GAIN);
        }
    }

    if (param.getInt(keyDUALMIC_SET_UL_GAIN_HF, value) == NO_ERROR)
    {
        if (mAudioALSAParamTunerInstance->setDMNRGain(AUD_MIC_GAIN_HF, value) == NO_ERROR)
        {
            param.remove(keyDUALMIC_SET_UL_GAIN_HF);
        }
    }
#endif

    if (param.getInt(keyHDRecTunningEnable, value) == NO_ERROR)
    {
        ALOGD("keyHDRecTunningEnable=%d", value);
        bool bEnable = value;
        mAudioSpeechEnhanceInfoInstance->SetBesRecTuningEnable(bEnable);
        param.remove(keyHDRecTunningEnable);
    }

    if (param.get(keyHDRecVMFileName, value_str) == NO_ERROR)
    {
        ALOGD("keyHDRecVMFileName=%s", value_str.string());
        if (mAudioSpeechEnhanceInfoInstance->SetBesRecVMFileName(value_str.string()) == NO_ERROR)
        {
            param.remove(keyHDRecVMFileName);
        }
    }
    // --->for audio tool(speech/ACF/HCF/DMNR/HD/Audiotaste calibration)

#if defined(MTK_DUAL_MIC_SUPPORT)
    // Dual Mic Noise Reduction, DMNR for Receiver
    if (param.getInt(keyEnable_Dual_Mic_Setting, value) == NO_ERROR)
    {
        param.remove(keyEnable_Dual_Mic_Setting);
        mStreamManager->Enable_DualMicSettng(SPH_ENH_DYNAMIC_MASK_DMNR, (bool) value);
    }

    // Dual Mic Noise Reduction, DMNR for Loud Speaker
    if (param.getInt(keySET_LSPK_DMNR_ENABLE, value) == NO_ERROR)
    {
        param.remove(keySET_LSPK_DMNR_ENABLE);
        mStreamManager->Set_LSPK_DlMNR_Enable(SPH_ENH_DYNAMIC_MASK_LSPK_DMNR, (bool) value);

    }

    // VoIP Dual Mic Noise Reduction, DMNR for Receiver
    if (param.getInt(keySET_VOIP_RECEIVER_DMNR_ENABLE, value) == NO_ERROR)
    {
        param.remove(keySET_VOIP_RECEIVER_DMNR_ENABLE);
        mAudioSpeechEnhanceInfoInstance->SetDynamicVoIPSpeechEnhancementMask(VOIP_SPH_ENH_DYNAMIC_MASK_DMNR, (bool)value);
    }

    // VoIP Dual Mic Noise Reduction, DMNR for Loud Speaker
    if (param.getInt(keySET_VOIP_LSPK_DMNR_ENABLE, value) == NO_ERROR)
    {
        param.remove(keySET_VOIP_LSPK_DMNR_ENABLE);
        mAudioSpeechEnhanceInfoInstance->SetDynamicVoIPSpeechEnhancementMask(VOIP_SPH_ENH_DYNAMIC_MASK_LSPK_DMNR, (bool)value);
    }

#endif

    // Voice Clarity Engine, VCE
    if (param.getInt(keySET_VCE_ENABLE, value) == NO_ERROR)
    {
        param.remove(keySET_VCE_ENABLE);
        mStreamManager->SetVCEEnable((bool) value);
    }

#if defined(MTK_MAGICONFERENCE_SUPPORT) && defined(MTK_DUAL_MIC_SUPPORT)
    // Magic Conference Call
    if (param.getInt(keySET_MAGI_CONFERENCE_ENABLE, value) == NO_ERROR)
    {
        param.remove(keySET_MAGI_CONFERENCE_ENABLE);
        mStreamManager->SetMagiConCallEnable((bool) value);
    }
#endif

    if (IsAudioSupportFeature(AUDIO_SUPPORT_VIBRATION_SPEAKER))
    {
        if (param.getInt(keySET_VIBSPK_ENABLE, value) == NO_ERROR)
        {
            param.remove(keySET_VIBSPK_ENABLE);
            AudioVIBSPKControl::getInstance()->setVibSpkEnable((bool)value);
            ALOGD("setParameters VibSPK!!, %x", value);
        }
        if (param.getInt(keySET_VIBSPK_RAMPDOWN, value) == NO_ERROR)
        {
            AudioFtmBase *mAudioFtm = AudioFtmBase::createAudioFtmInstance();
            param.remove(keySET_VIBSPK_RAMPDOWN);
            mAudioFtm->SetVibSpkRampControl(value);
            ALOGD("setParameters VibSPK_Rampdown!!, %x", value);
        }
    }

#ifdef  MTK_TTY_SUPPORT
    // Set TTY mode
    if (param.get(keySetTtyMode, value_str) == NO_ERROR)
    {
        param.remove(keySetTtyMode);
        tty_mode_t tty_mode;

        if (value_str == "tty_full")
        {
            tty_mode = AUD_TTY_FULL;
        }
        else if (value_str == "tty_vco")
        {
            tty_mode = AUD_TTY_VCO;
        }
        else if (value_str == "tty_hco")
        {
            tty_mode = AUD_TTY_HCO;
        }
        else if (value_str == "tty_off")
        {
            tty_mode = AUD_TTY_OFF;
        }
        else
        {
            ALOGD("setParameters tty_mode error !!");
            tty_mode = AUD_TTY_ERR;
        }
        #ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
        mSpeechPhoneCallController->setTtyMode(tty_mode);
        #endif
    }
#endif

    // Low latency mode
    if (param.get(keySCREEN_STATE, value_str) == NO_ERROR)
    {
        param.remove(keySCREEN_STATE);
        setScreenState(value_str == "on");
    }

    // Loopback use speaker or not
    static bool bForceUseLoudSpeakerInsteadOfReceiver = false;
    if (param.getInt(keySET_LOOPBACK_USE_LOUD_SPEAKER, value) == NO_ERROR)
    {
        param.remove(keySET_LOOPBACK_USE_LOUD_SPEAKER);
        bForceUseLoudSpeakerInsteadOfReceiver = value & 0x1;
    }

    // Assign delay frame for modem loopback // 1 frame = 20ms
    if (param.getInt(keySET_LOOPBACK_MODEM_DELAY_FRAMES, value) == NO_ERROR)
    {
        param.remove(keySET_LOOPBACK_MODEM_DELAY_FRAMES);
        SpeechDriverInterface *pSpeechDriver = NULL;
        for (int modem_index = MODEM_1; modem_index < NUM_MODEM; modem_index++)
        {
            #ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
            pSpeechDriver = SpeechDriverFactory::GetInstance()->GetSpeechDriverByIndex((modem_index_t)modem_index);
            if (pSpeechDriver != NULL) // Might be single talk and some speech driver is NULL
            {
                pSpeechDriver->SetAcousticLoopbackDelayFrames((int32_t)value);
            }
            #endif
        }
    }

    // Loopback
    if (param.get(keySET_LOOPBACK_TYPE, value_str) == NO_ERROR)
    {
        param.remove(keySET_LOOPBACK_TYPE);

        // parse format like "SET_LOOPBACK_TYPE=1" / "SET_LOOPBACK_TYPE=1+0"
        int type_value = NO_LOOPBACK;
        int device_value = -1;
        sscanf(value_str.string(), "%d,%d", &type_value, &device_value);
        ALOGV("type_value = %d, device_value = %d", type_value, device_value);

        const loopback_t loopback_type = (loopback_t)type_value;
        loopback_output_device_t loopback_output_device;

        if (loopback_type == NO_LOOPBACK) // close loopback
        {
            LoopbackManager::GetInstance()->SetLoopbackOff();
        }
        else // open loopback
        {
            if (device_value == LOOPBACK_OUTPUT_RECEIVER ||
                device_value == LOOPBACK_OUTPUT_EARPHONE ||
                device_value == LOOPBACK_OUTPUT_SPEAKER) // assign output device
            {
                loopback_output_device = (loopback_output_device_t)device_value;
            }
            else // not assign output device
            {
                if ((AudioSystem::getDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADSET, "") ==
                    (audio_policy_dev_state_t)android_audio_legacy::AudioSystem::DEVICE_STATE_AVAILABLE) ||
                    (AudioSystem::getDeviceConnectionState(AUDIO_DEVICE_OUT_WIRED_HEADPHONE, "") ==
                    (audio_policy_dev_state_t)android_audio_legacy::AudioSystem::DEVICE_STATE_AVAILABLE))
                {
                    loopback_output_device = LOOPBACK_OUTPUT_EARPHONE;
                }
                else if (bForceUseLoudSpeakerInsteadOfReceiver == true)
                {
                    loopback_output_device = LOOPBACK_OUTPUT_SPEAKER;
                }
                else
                {
                    loopback_output_device = LOOPBACK_OUTPUT_RECEIVER;
                }
            }
            LoopbackManager::GetInstance()->SetLoopbackOn(loopback_type, loopback_output_device);
        }
    }


    if (param.size())
    {
        ALOGV("%s(), still have param.size() = %zu, remain param = \"%s\"",
              __FUNCTION__, param.size(), param.toString().string());
        status = BAD_VALUE;
    }

    ALOGV("-%s(): %s ", __FUNCTION__, keyValuePairs.string());
    return status;
}

String8 AudioALSAHardware::getParameters(const String8 &keys)
{
    ALOGV("+%s(), key = %s", __FUNCTION__, keys.string());

    String8 value;
    int cmdType = 0, value_int;
    AudioParameter param = AudioParameter(keys);
    AudioParameter returnParam = AudioParameter();

    if (param.get(keyMTK_AUDENH_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_AUDENH_SUPPORT);
#ifdef MTK_AUDENH_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_AUDENH_SUPPORT, value);
    }
    if (param.get(keyMTK_TTY_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_TTY_SUPPORT);
#ifdef MTK_TTY_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_TTY_SUPPORT, value);
    }
    if (param.get(keyMTK_WB_SPEECH_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_WB_SPEECH_SUPPORT);
#ifdef MTK_WB_SPEECH_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_WB_SPEECH_SUPPORT, value);
    }
    if (param.get(keyMTK_DUAL_MIC_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_DUAL_MIC_SUPPORT);
#ifdef MTK_DUAL_MIC_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_DUAL_MIC_SUPPORT, value);
    }
    if (param.get(keyMTK_AUDIO_HD_REC_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_AUDIO_HD_REC_SUPPORT);
#ifdef MTK_AUDIO_HD_REC_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_AUDIO_HD_REC_SUPPORT, value);
    }
    if (param.get(keyMTK_BESLOUDNESS_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_BESLOUDNESS_SUPPORT);
#ifdef MTK_BESLOUDNESS_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_BESLOUDNESS_SUPPORT, value);
    }
    if (param.get(keyMTK_BESSURROUND_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_BESSURROUND_SUPPORT);
#ifdef MTK_BESSURROUND_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_BESSURROUND_SUPPORT, value);
    }
    if (param.get(keyMTK_HDMI_MULTI_CHANNEL_SUPPORT, value) == NO_ERROR)
    {
        param.remove(keyMTK_HDMI_MULTI_CHANNEL_SUPPORT);
#ifdef MTK_HDMI_MULTI_CHANNEL_SUPPORT
        value = "true";
#else
        value = "false";
#endif
        returnParam.add(keyMTK_HDMI_MULTI_CHANNEL_SUPPORT, value);
    }

    if (param.get(keyGetFmEnable, value) == NO_ERROR)
    {
        param.remove(keyGetFmEnable);

        value = (mStreamManager->getFmEnable() == true) ? "true" : "false";
        returnParam.add(keyGetFmEnable, value);
    }

#if defined(MTK_DUAL_MIC_SUPPORT)
    if (param.getInt(keyDUALMIC_GET_GAIN, cmdType) == NO_ERROR)
    {
        unsigned short gain = 0;
        char buf[32];

        if (mAudioALSAParamTunerInstance->getDMNRGain((unsigned short)cmdType, &gain) == NO_ERROR)
        {
            sprintf(buf, "%d", gain);
            returnParam.add(keyDUALMIC_GET_GAIN, String8(buf));
            param.remove(keyDUALMIC_GET_GAIN);
        }
    }
#endif

    if (param.get(keyGetFmTxEnable, value) == NO_ERROR)
    {
        param.remove(keyGetFmTxEnable);

        value = (mFmTxEnable == true) ? "true" : "false";
        returnParam.add(keyGetFmTxEnable, value);
    }

    if (param.get(keyMusicPlusGet, value) == NO_ERROR)
    {

        bool musicplus_status = mStreamManager->GetMusicPlusStatus();
        value = (musicplus_status) ? "1" : "0";
        param.remove(keyMusicPlusGet);
        returnParam.add(keyMusicPlusGet, value);
    }

    if (param.get(keyBesLoudnessGet, value) == NO_ERROR)
    {

        bool besloudness_status = mStreamManager->GetBesLoudnessStatus();
        value = (besloudness_status) ? "1" : "0";
        param.remove(keyBesLoudnessGet);
        returnParam.add(keyBesLoudnessGet, value);
    }

    // Audio Volume version
    if (param.get(keyGET_AUDIO_VOLUME_VER, value) == NO_ERROR)
    {
        param.remove(keyGET_AUDIO_VOLUME_VER);
        value = "1";
        returnParam.add(keyGET_AUDIO_VOLUME_VER, value);
    }

    // check if the LR channel switched
    if (param.get(keyLR_ChannelSwitch, value) == NO_ERROR)
    {
        bool bIsLRSwitch = AudioALSAHardwareResourceManager::getInstance()->getMicInverse();
        value = (bIsLRSwitch == true) ? "1" : "0";
        param.remove(keyLR_ChannelSwitch);
        returnParam.add(keyLR_ChannelSwitch, value);
    }

    // get the Capture drop time for voice unlock
    if (param.get(keyGetCaptureDropTime, value) == NO_ERROR)
    {
        char buf[32];
        sprintf(buf, "%d", CAPTURE_DROP_MS);
        param.remove(keyGetCaptureDropTime);
        returnParam.add(keyGetCaptureDropTime, String8(buf));
    }

    //AudioCmdHandlerService usage
    if (param.getInt(keyGetCmd, value_int) == NO_ERROR)
    {
        param.remove(keyGetCmd);
        status_t result_int = GetAudioCommand(value_int);
        ALOGD("%s(), keyGetCmd(%s), result_int=%d", __FUNCTION__, keyGetCmd.string(), result_int);
        returnParam.addInt(keyGetCmd, result_int);
    }

    if (param.get(keyGetBuffer, value) == NO_ERROR)
    {
        param.remove(keyGetBuffer);
        size_t sz_in = value.size();
        size_t sz_out;//output buffer lenth
        // Decode string to char
        sz_out = Base64_OutputSize(false, sz_in);
        ALOGD("%s(), before decode (%s), sz_in(%d), sz_out(%d)", __FUNCTION__, value.string(), sz_in, sz_out);

        //allocate output buffer for decode
        unsigned char *buf_uchar = new unsigned char[sz_out];
        size_t sz_dec = Base64_Decode(value.string(), buf_uchar, sz_in);
        //error check and alert
        if (sz_dec == 0)
        {
            ALOGE("%s(), Decode Error!!!after decode (%s), sz_in(%d), sz_out(%d), sz_dec(%d)", __FUNCTION__, buf_uchar, sz_in, sz_out, sz_dec);
        }
        else
        {
            ALOGD("%s(), after decode (%s), sz_in(%d), sz_dec(%d)", __FUNCTION__, buf_uchar, sz_in, sz_dec);
        }
        for (int ii = 0; ii < sz_dec; ii++)
        {
            ALOGV("%s(), buf_uchar(0x%x)", __FUNCTION__, *(buf_uchar + ii));
        }
        int type_AudioData = *((int *) buf_uchar);
        int sz_AudioData = *(((int *) buf_uchar) + 1);
        ALOGD("%s(), after decode (%s), type_AudioData(0x%x), sz_AudioData(%d)", __FUNCTION__, buf_uchar, type_AudioData, sz_AudioData);
        unsigned char *pAudioData = new unsigned char[sz_AudioData + 4]; //add one status_t(4 byte)

        //get audio data
        status_t rtnVal = GetAudioData(type_AudioData, (size_t)sz_AudioData, pAudioData + 4);
        *pAudioData = (int) rtnVal;

        if (buf_uchar != NULL)
        {
            delete[] buf_uchar;
        }
        //encode to string
        sz_in = sz_AudioData + 4;

        // Encode string to char
        sz_out = Base64_OutputSize(true, sz_in);
        ALOGD("%s(), before encode (%s), sz_in(%d), sz_out(%d)", __FUNCTION__, pAudioData, sz_in, sz_out);

        //allocate output buffer for encode
        char *buf_enc = new  char[sz_out];
        size_t sz_enc = Base64_Encode(pAudioData, buf_enc, sz_in);
        //error check and alert
        if (sz_enc == 0)
        {
            ALOGE("%s(), Encode Error!!!after encode (%s), sz_in(%d), sz_out(%d), sz_enc(%d)", __FUNCTION__, buf_enc, sz_in, sz_out, sz_enc);
        }
        else
        {
            ALOGD("%s(), after encode (%s), sz_in(%d), sz_enc(%d)", __FUNCTION__, buf_enc, sz_in, sz_enc);
        }

        //char to string8
        String8 valstr = String8(buf_enc, sz_enc);

        returnParam.add(keyGetBuffer, valstr);

        if (buf_enc != NULL)
        {
            delete[] buf_enc;
        }
        if (pAudioData != NULL)
        {
            delete[] pAudioData;
        }
    }

    const String8 keyValuePairs = returnParam.toString();
    ALOGV("-%s(), return \"%s\"", __FUNCTION__, keyValuePairs.string());
    return keyValuePairs;
}

size_t AudioALSAHardware::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    return mStreamManager->getInputBufferSize(sampleRate, format, channelCount);
}

android_audio_legacy::AudioStreamOut *AudioALSAHardware::openOutputStream(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status)
{
    return mStreamManager->openOutputStream(devices, format, channels, sampleRate, status);
}

android_audio_legacy::AudioStreamOut* AudioALSAHardware::openOutputStreamWithFlags(
    uint32_t devices,
    audio_output_flags_t flags,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status)
{
    return mStreamManager->openOutputStreamWithFlags(devices, format, channels, sampleRate, status, flags);
}

void AudioALSAHardware::closeOutputStream(android_audio_legacy::AudioStreamOut *out)
{
    mStreamManager->closeOutputStream(out);
}

android_audio_legacy::AudioStreamIn *AudioALSAHardware::openInputStream(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    android_audio_legacy::AudioSystem::audio_in_acoustics acoustics)
{
    return mStreamManager->openInputStream(devices, format, channels, sampleRate, status, acoustics);
}

android_audio_legacy::AudioStreamIn *AudioALSAHardware::openInputStreamWithFlags(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    android_audio_legacy::AudioSystem::audio_in_acoustics acoustics,
    audio_input_flags_t flags)
{
    return mStreamManager->openInputStream(devices, format, channels, sampleRate, status, acoustics, flags);
}

void AudioALSAHardware::closeInputStream(android_audio_legacy::AudioStreamIn *in)
{
    mStreamManager->closeInputStream(in);
}


status_t AudioALSAHardware::dumpState(int fd, const Vector<String16> &args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    (void)args;

    snprintf(buffer, SIZE, "\nAudioALSAHardware %p:\n", this);
    result.append(buffer);

    snprintf(buffer, SIZE, "Audio Patches:\n");
    result.append(buffer);
    write(fd, result.string(), result.size());

    for (size_t i = 0; i < mAudioHalPatchVector.size(); i++)
        mAudioHalPatchVector[i]->dump(fd, 2, i);

    return NO_ERROR;
}

status_t AudioALSAHardware::dump(int fd, const Vector<String16> &args)
{
    ALOGD("%s()", __FUNCTION__);
    (void)fd;
    (void)args;
    return NO_ERROR;
}

status_t  AudioALSAHardware::setMasterMute(bool muted __unused)
{
    return NO_ERROR;
}

int AudioALSAHardware::createAudioPatch(unsigned int num_sources,
    const struct audio_port_config *sources,
    unsigned int num_sinks,
    const struct audio_port_config *sinks,
    audio_patch_handle_t *handle)
{
    int ret = NO_ERROR;
    unsigned int i;
    AudioParameter param;
    audio_devices_t output_device;
    audio_devices_t eInputDeviceList = AUDIO_DEVICE_NONE;
    audio_source_t eInputSource = AUDIO_SOURCE_DEFAULT;

    ALOGV("+%s num_sources(%u) num_sinks(%u)", __FUNCTION__, num_sources, num_sinks);

    if (!sources || !sinks || !handle) {
        ret = BAD_VALUE;
        ALOGW("%s invalud pointer source(%p) sinks(%p) handle(%p)", __FUNCTION__, sources, sinks, handle);
        goto exit;
    }

    if (num_sources != 1) {
        ret = BAD_VALUE;
        ALOGW("%s num_sources(%u) not supported", __FUNCTION__, num_sources);
        goto exit;
    }

    if (num_sinks == 0 || num_sinks > AUDIO_PATCH_PORTS_MAX) {
        ret = BAD_VALUE;
        ALOGW("%s num_sinks(%u) not supported", __FUNCTION__, num_sinks);
        goto exit;
    }

#if LOG_NDEBUG == 0
    for (i = 0; i < num_sources; i++) {
        ALOGV("%s dump sources[%d/%d] port config >>", __FUNCTION__, i, num_sources);
        dumpAudioPortConfig(&sources[i]);
        ALOGV("%s dump sources[%d/%d] port config <<", __FUNCTION__, i, num_sources);
    }

    for (i = 0; i < num_sinks; i++) {
        ALOGV("%s dump sink[%d/%d] port config >>", __FUNCTION__, i, num_sinks);
        dumpAudioPortConfig(&sinks[i]);
        ALOGV("%s dump sink[%d/%d] port config <<", __FUNCTION__, i, num_sinks);
    }
#endif

    switch (sources[0].type)
    {
    case AUDIO_PORT_TYPE_MIX:
        {
            // only support mix to devices connection
            output_device = AUDIO_DEVICE_NONE;
            for (i = 0; i < num_sinks; i++) {
                if (sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
                    ALOGW("%s invalid sink type %d for mix source", __FUNCTION__, sinks[i].type);
                    ret = BAD_VALUE;
                    goto exit;
                }
                output_device |= sinks[i].ext.device.type;
            }

            // trigger keyRouting
            param.addInt(String8(AudioParameter::keyRouting), (int)output_device);
            ret = mStreamManager->setParameters(param.toString(), sources[0].ext.mix.handle);
        }
        break;
    case AUDIO_PORT_TYPE_DEVICE:
        {
            if (sinks[0].type == AUDIO_PORT_TYPE_MIX) {
                eInputDeviceList = sources[0].ext.device.type;
                eInputSource = sinks[0].ext.mix.usecase.source;
                // trigger keyRouting
                param.addInt(String8(AudioParameter::keyRouting), (int)eInputDeviceList);
                param.addInt(String8(AudioParameter::keyInputSource), (int)eInputSource);
                ret = mStreamManager->setParameters(param.toString(), sinks[0].ext.mix.handle);

                if (ret == NO_ERROR)
                {
                    ssize_t index;
                    ssize_t total = mAudioHalPatchVector.size();
                    for (index = total-1;index >= 0; index--)
                    {

                        if (mAudioHalPatchVector[index]->mAudioPatch.sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
                            mAudioHalPatchVector[index]->mAudioPatch.sinks[0].type == AUDIO_PORT_TYPE_MIX &&
                            sinks[0].ext.mix.handle == mAudioHalPatchVector[index]->mAudioPatch.sinks[0].ext.mix.handle)
                        {
                            AudioHalPatch *patch;
                            patch = mAudioHalPatchVector[index];
                            ALOGD("handlecheck createAudioPatch() removing patch handle %d index %zd", mAudioHalPatchVector[index]->mHalHandle,index);
                            mAudioHalPatchVector.removeItemsAt(index);
                            delete(patch);
                            break;
                        }
                    }

                    if (eInputDeviceList == AUDIO_DEVICE_IN_FM_TUNER)
                    {
                        if (mUseAudioPatchForFm == true)
                        {
                            ret = mStreamManager->setFmEnable(true, true, false);
                        }
                    }
                }
                else
                {
                    ALOGE("Err %s %d",__FUNCTION__,__LINE__);
                }
            } else if (sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
                // check input device
                if (AUDIO_DEVICE_IN_FM_TUNER != sources[0].ext.device.type) {
                    ALOGW("%s source device %d -> sink type %d not suppoprted",
                          __FUNCTION__, sources[0].ext.device.type, sinks[0].type);
                    ret = BAD_VALUE;
                    goto exit;
                }
                // check output device
                output_device = AUDIO_DEVICE_NONE;
                for (i = 0; i < num_sinks; i++) {
                    output_device |= sinks[i].ext.device.type;
                }

                if (!(output_device & FM_DEVICE_TO_DEVICE_SUPPORT_OUTPUT_DEVICES)) {
                    ALOGW("%s source type %d -> sink device %d not suppoprted",
                          __FUNCTION__, sources[0].type, output_device);
                    ret = BAD_VALUE;
                    goto exit;
                }

                if (AudioALSAFMController::getInstance()->checkFmNeedUseDirectConnectionMode() == false)
                {
                    ALOGW("[%s] [%d] InDirectConnectionMode", __FUNCTION__, __LINE__);
                    ret = INVALID_OPERATION;
                    goto exit;
                }

                if (mUseAudioPatchForFm == false)
                {
                    ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                    ret = INVALID_OPERATION;
                    goto exit;
                }

                param.addInt(String8(AudioParameter::keyRouting), (int)output_device);
                mStreamManager->setParametersToStreamOut(param.toString());

                mStreamManager->setFmVolume(0);

                // for bybpass audio hw
                if (mAudioHWBypass)
                {
                    goto exit;
                }

                mStreamManager->setFmEnable(false);
                ret = mStreamManager->setFmEnable(true, true, true);
            } else {
                ALOGW("%s source type %d -> sink type %d not suppoprted", __FUNCTION__, sources[0].type, sinks[0].type);
                ret = BAD_VALUE;
            }
        }
        break;
    default:
        ret = BAD_VALUE;
        break;
    }

exit:
    if (ret == NO_ERROR) {
        // check valid handle
        if (*handle != AUDIO_PATCH_HANDLE_NONE) {
            ssize_t index = mAudioHalPatchVector.indexOfKey(*handle);
            if (index >= 0) {
                ALOGV("%s removing patch handle %d", __FUNCTION__, *handle);
                AudioHalPatch *removePatch = mAudioHalPatchVector.valueAt(index);
                mAudioHalPatchVector.removeItemsAt(index);
                if (removePatch) delete removePatch;
            }
        }

        *handle = static_cast<audio_patch_handle_t>(android_atomic_inc(&mNextUniqueId));

        AudioHalPatch *patch = new AudioHalPatch(*handle);
        patch->mAudioPatch.num_sources = num_sources;
        for (i = 0; i < num_sources; i++)
            memcpy((void*)&patch->mAudioPatch.sources[i], (void*)&sources[i], sizeof(struct audio_port_config));

        patch->mAudioPatch.num_sinks = num_sinks;
        for (i = 0; i < num_sinks; i++)
            memcpy((void*)&patch->mAudioPatch.sinks[i], (void*)&sinks[i], sizeof(struct audio_port_config));

        mAudioHalPatchVector.add(patch->mHalHandle, patch);
        ALOGV("-%s", __FUNCTION__);
    } else {
        ALOGW("-%s fail %d", __FUNCTION__, ret);
    }
    return ret;
}

int AudioALSAHardware::releaseAudioPatch(audio_patch_handle_t handle)
{
    int ret = NO_ERROR;
    ssize_t index;
    int i, j;
    AudioHalPatch *patch = NULL;
    AudioParameter param;

    ALOGV("+%s handle(%d)", __FUNCTION__, handle);

    if (handle == AUDIO_PATCH_HANDLE_NONE) {
        ALOGW("%s null handle", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto exit;
    }

    index = mAudioHalPatchVector.indexOfKey(handle);
    if (index < 0) {
        ALOGW("%s handle not found %d", __FUNCTION__, handle);
        ret = INVALID_OPERATION;
        goto exit;
    }

    patch = mAudioHalPatchVector.valueAt(index);
    mAudioHalPatchVector.removeItemsAt(index);

    switch (patch->mAudioPatch.sources[0].type)
    {
    case AUDIO_PORT_TYPE_MIX:
        {
            for (i = 0; i < (int)patch->mAudioPatch.num_sinks; i++) {
                if (patch->mAudioPatch.sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
                    ALOGW("%s invalid sink type %d for mix source", __FUNCTION__, patch->mAudioPatch.sinks[i].type);
                    ret = BAD_VALUE;
                    goto exit;
                }
            }
            // check if there's another audio patch with output device routing
            for (i = 0; i < (int)mAudioHalPatchVector.size(); i++) {
                if ((mAudioHalPatchVector[i]->mAudioPatch.sources[0].type == AUDIO_PORT_TYPE_MIX) &&
                    (mAudioHalPatchVector[i]->mAudioPatch.sources[0].ext.mix.handle == patch->mAudioPatch.sources[0].ext.mix.handle)) {
                    for (j = 0; j < (int)mAudioHalPatchVector[i]->mAudioPatch.num_sinks; j++) {
                        if (mAudioHalPatchVector[i]->mAudioPatch.sinks[j].type == AUDIO_PORT_TYPE_DEVICE &&
                            mAudioHalPatchVector[i]->mAudioPatch.sinks[j].ext.device.type != AUDIO_DEVICE_NONE) {
                            // skip keyRouting
                            ALOGV("%s handle(%d) mix to device skip routing", __FUNCTION__, handle);
                            ret = NO_ERROR;
                            goto exit;
                        }
                    }
                }
            }

#if 0
            // trigger keyRouting
            param.addInt(String8(AudioParameter::keyRouting), (int)AUDIO_DEVICE_NONE);
            ret = mStreamManager->setParameters(param.toString(), patch->mAudioPatch.sources[0].ext.mix.handle);
#endif
        }
        break;
    case AUDIO_PORT_TYPE_DEVICE:
        {
            if (patch->mAudioPatch.sinks[0].type == AUDIO_PORT_TYPE_MIX) {
                // FM recording
                if (mUseAudioPatchForFm == true) {
                    if (patch->mAudioPatch.sources[0].ext.device.type == AUDIO_DEVICE_IN_FM_TUNER) {
                        bool keepFMActive = false;

                        for (i = mAudioHalPatchVector.size()-1; i >= 0; i--) {
                            for (j = 0; j < (int)mAudioHalPatchVector[i]->mAudioPatch.num_sources; j++) {
                                if ((mAudioHalPatchVector[i]->mAudioPatch.sources[j].type == AUDIO_PORT_TYPE_DEVICE) &&
                                    (mAudioHalPatchVector[i]->mAudioPatch.sources[j].ext.device.type == AUDIO_DEVICE_IN_FM_TUNER)) {
                                    ALOGD("%s release FM recording, still have audio patch need AUDIO_DEVICE_IN_FM_TUNER", __FUNCTION__);
                                    ret = NO_ERROR;
                                    keepFMActive = true;
                                    break;
                                }
                            }
                        }

                        if (!keepFMActive) {
                            mStreamManager->setFmEnable(false);
                        }
                    }
                }

                // check if there's another audio patch with input device routing
                for (i = 0; i < (int)mAudioHalPatchVector.size(); i++) {
                    if ((mAudioHalPatchVector[i]->mAudioPatch.sinks[0].type == AUDIO_PORT_TYPE_MIX) &&
                        (mAudioHalPatchVector[i]->mAudioPatch.sinks[0].ext.mix.handle == patch->mAudioPatch.sinks[0].ext.mix.handle)) {
                        for (j = 0; j < (int)mAudioHalPatchVector[i]->mAudioPatch.num_sources; j++) {
                            if (mAudioHalPatchVector[i]->mAudioPatch.sources[j].type == AUDIO_PORT_TYPE_DEVICE &&
                                mAudioHalPatchVector[i]->mAudioPatch.sources[j].ext.device.type != AUDIO_DEVICE_NONE) {
                                // skip keyRouting
                                ALOGV("%s handle(%d) device to mix skip routing", __FUNCTION__, handle);
                                ret = NO_ERROR;
                                goto exit;
                            }
                        }
                    }
                }
                // trigger keyRouting
                param.addInt(String8(AudioParameter::keyRouting), (int)AUDIO_DEVICE_NONE);
                ret = mStreamManager->setParameters(param.toString(), patch->mAudioPatch.sinks[0].ext.mix.handle);
            } else if (patch->mAudioPatch.sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
                if ((patch->mAudioPatch.sources[0].ext.device.type == AUDIO_DEVICE_IN_FM_TUNER) &&
                    (patch->mAudioPatch.sinks[0].ext.device.type & FM_DEVICE_TO_DEVICE_SUPPORT_OUTPUT_DEVICES)) {
                    // close FM if need (direct)
                    if (mUseAudioPatchForFm == false)
                    {
                        ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                        ret = INVALID_OPERATION;
                    }
                    else
                    {
                        // disable FM
                        mStreamManager->setFmVolume(0);
                        mStreamManager->setFmEnable(false);

                        // restore previous output device setting
                        audio_devices_t output_device = AUDIO_DEVICE_NONE;
                        for (i = mAudioHalPatchVector.size()-1; i >= 0; i--) {
                            for (j = 0; j < (int)mAudioHalPatchVector[i]->mAudioPatch.num_sinks; j++) {
                                if ((mAudioHalPatchVector[i]->mAudioPatch.sinks[j].type == AUDIO_PORT_TYPE_DEVICE) &&
                                    (mAudioHalPatchVector[i]->mAudioPatch.sinks[j].ext.device.type != AUDIO_DEVICE_NONE) &&
                                    (mAudioHalPatchVector[i]->mAudioPatch.sources[0].type == AUDIO_PORT_TYPE_MIX)) {
                                    output_device |= mAudioHalPatchVector[i]->mAudioPatch.sinks[j].ext.device.type;
                                    ret = NO_ERROR;
                                    ALOGD("%s release FM tuner, still have audio patch routing to output_device 0x%x",
                                          __FUNCTION__, output_device);
                                }
                            }
                            if (output_device)
                                break;
                        }
                        // trigger keyRouting
                        param.addInt(String8(AudioParameter::keyRouting), (int)output_device);
                        ret = mStreamManager->setParametersToStreamOut(param.toString());
                    }
                } else {
                    ret = BAD_VALUE;
                }
            } else {
                ALOGW("%s source type %d -> sink type %d not suppoprted", __FUNCTION__,
                      patch->mAudioPatch.sources[0].type, patch->mAudioPatch.sinks[0].type);
                ret = BAD_VALUE;
            }
        }
        break;
    default:
        ret = BAD_VALUE;
        break;
    }


exit:
    if (ret == NO_ERROR){
        if (patch) delete patch;
        ALOGV("-%s handle(%d)", __FUNCTION__, handle);
    } else {
        if (patch) mAudioHalPatchVector.add(patch->mHalHandle, patch);
        ALOGW("-%s handle(%d) fail %d", __FUNCTION__, handle, ret);
    }
    return ret;
}

int AudioALSAHardware::getAudioPort(struct audio_port *port __unused)
{
    // TODO: implement here
    return INVALID_OPERATION;
}

int AudioALSAHardware::setAudioPortConfig(const struct audio_port_config *config)
{
    ALOGV("%s", __FUNCTION__);

    int status = NO_ERROR;

    do
    {
        if (config == NULL)
        {
            ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
            status = BAD_VALUE;
            break;
        }

#if LOG_NDEBUG == 0
        dumpAudioPortConfig(config);
#endif

        if ((config->config_mask & AUDIO_PORT_CONFIG_GAIN) == 0)
        {
            ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
            status = INVALID_OPERATION;
            break;
        }

        if (config->type == AUDIO_PORT_TYPE_MIX)
        {
            if (config->role == AUDIO_PORT_ROLE_SOURCE)
            {
                //Apply Gain to MEMIF , don't support it so far
                ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                status = INVALID_OPERATION;
                break;
            }
            else if (config->role == AUDIO_PORT_ROLE_SINK)
            {
                ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                status = INVALID_OPERATION;
                break;
            }
            else
            {
                ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                status = BAD_VALUE;
                break;
            }
        }
        else if (config->type == AUDIO_PORT_TYPE_DEVICE)
        {
            if (mUseAudioPatchForFm == false)
            {
                ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                status = INVALID_OPERATION;
                break;
            }
            if (config->role == AUDIO_PORT_ROLE_SINK || config->role == AUDIO_PORT_ROLE_SOURCE)
            {
                //Support specific device eg. headphone/speaker
                size_t indexOfPatch;
                size_t indexOfSink;
                audio_port_config *pstCurConfig = NULL;
                bool bhit = false;

                for (indexOfPatch = 0; indexOfPatch < mAudioHalPatchVector.size() && !bhit; indexOfPatch++)
                {
                    for (indexOfSink = 0; indexOfSink < mAudioHalPatchVector[indexOfPatch]->mAudioPatch.num_sinks; indexOfSink++)
                    {
                        if ((config->ext.device.type == mAudioHalPatchVector[indexOfPatch]->mAudioPatch.sinks[indexOfSink].ext.device.type)
                            && (mAudioHalPatchVector[indexOfPatch]->mAudioPatch.sources[indexOfSink].ext.device.type == AUDIO_DEVICE_IN_FM_TUNER)
                            && (mAudioHalPatchVector[indexOfPatch]->mAudioPatch.sinks[indexOfSink].ext.device.type & FM_DEVICE_TO_DEVICE_SUPPORT_OUTPUT_DEVICES))
                        {
                            bhit = true;
                            pstCurConfig = &(mAudioHalPatchVector[indexOfPatch]->mAudioPatch.sinks[indexOfSink]);
                            break;
                        }
                    }
                }

                if (!bhit || pstCurConfig == NULL)
                {
                    ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                    status = INVALID_OPERATION;
                    break;
                }

                if (!config->gain.mode)
                {
                    ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                    status = INVALID_OPERATION;
                    break;
                }
#if 0
                int dGainDB = 0;
                unsigned int ramp_duration_ms = 0;
                if (config->gain.mode & (AUDIO_GAIN_MODE_JOINT | AUDIO_GAIN_MODE_CHANNELS)) //Hw support joint only
                {
                    dGainDB = config->gain.values[0] / 100;
                    if (dGainDB >= 0)//Support degrade only
                    {
                        dGainDB = 0;
                    }
                    else
                    {
                        dGainDB = (-1) * dGainDB;
                    }

                    dGainDB = dGainDB << 2;
                    if (dGainDB > 256)
                    {
                        dGainDB = 256;
                    }
                    dGainDB = 256 - dGainDB;
                }

                if (config->gain.mode & AUDIO_GAIN_MODE_RAMP)
                {
                    ramp_duration_ms = config->gain.ramp_duration_ms;
                }

#ifndef MTK_AUDIO_GAIN_TABLE
                //FMTODO : Gain setting
                float fFMVolume = AudioALSAVolumeController::linearToLog(dGainDB);
                ALOGD("fFMVolume %f", fFMVolume);
                if (fFMVolume >= 0 && fFMVolume <= 1.0)
                {
                    mStreamManager->setFmVolume(fFMVolume);
                }
                else
                {
                    ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                    status = BAD_VALUE;
                    break;
                }
#endif
#else
                int dGainDB = 0;
                unsigned int ramp_duration_ms = 0;
                float fFMVolume = 1.0;
#ifdef MTK_NEW_VOL_CONTROL
                fFMVolume = AudioMTKGainController::getInstance()->GetDigitalLogGain(config->gain.values[0],
                                                                                     pstCurConfig->ext.device.type,
                                                                                     AUDIO_STREAM_MUSIC);
#else
                if (config->gain.mode & (AUDIO_GAIN_MODE_JOINT | AUDIO_GAIN_MODE_CHANNELS)) //Hw support joint only
                {
                    fFMVolume = MappingFMVolofOutputDev(config->gain.values[0],pstCurConfig->ext.device.type);
                }
                else
                {
#ifndef MTK_AUDIO_GAIN_TABLE
                    fFMVolume = AudioALSAVolumeController::linearToLog(dGainDB);
#else
                    fFMVolume = AudioMTKGainController::linearToLog(dGainDB);
#endif
                }

                if (config->gain.mode & AUDIO_GAIN_MODE_RAMP)
                {
                    ramp_duration_ms = config->gain.ramp_duration_ms;
                }
#endif
                ALOGD("fFMVolume %f",fFMVolume);
                if (fFMVolume >=0 && fFMVolume <=1.0)
                {
                    mStreamManager->setFmVolume(fFMVolume);
                }
                else
                {
                    ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                    status = BAD_VALUE;
                    break;
                }
#endif
            }
            else
            {
                ALOGW("[%s] [%d]", __FUNCTION__, __LINE__);
                status = BAD_VALUE;
                break;
            }
        }
    }
    while (0);

    return status;
}

status_t AudioALSAHardware::SetEMParameter(void *ptr, int len)
{
    ALOGD("%s() len [%d] sizeof [%zu]", __FUNCTION__, len, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));

    if (len == sizeof(AUDIO_CUSTOM_PARAM_STRUCT))
    {
        AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB = (AUDIO_CUSTOM_PARAM_STRUCT *)ptr;
        mStreamManager->SetEMParameter(pSphParamNB);
        return NO_ERROR;
    }
    else
    {
        ALOGE("len [%d] != Sizeof(AUDIO_CUSTOM_PARAM_STRUCT) [%zu]", len, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));
        return UNKNOWN_ERROR;
    }
}

status_t AudioALSAHardware::GetEMParameter(void *ptr , int len)
{
    ALOGD("%s() len [%d] sizeof [%zu]", __FUNCTION__, len, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));

    if (len == sizeof(AUDIO_CUSTOM_PARAM_STRUCT))
    {
        GetNBSpeechParamFromNVRam((AUDIO_CUSTOM_PARAM_STRUCT *)ptr);
        return NO_ERROR;
    }
    else
    {
        ALOGE("len [%d] != Sizeof(AUDIO_CUSTOM_PARAM_STRUCT) [%zu]", len, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));
        return UNKNOWN_ERROR;
    }
}

status_t AudioALSAHardware::SetAudioCommand(int par1, int par2)
{
    ALOGD("%s(), par1 = 0x%x, par2 = %d", __FUNCTION__, par1, par2);
    switch (par1)
    {
        case SETOUTPUTFIRINDEX:
        {
            UpdateOutputFIR(Normal_Coef_Index, par2);
            break;
        }
        case SETNORMALOUTPUTFIRINDEX:
        {
            UpdateOutputFIR(Normal_Coef_Index, par2);
            break;
        }
        case SETHEADSETOUTPUTFIRINDEX:
        {
            UpdateOutputFIR(Headset_Coef_Index, par2);
            break;
        }
        case SETSPEAKEROUTPUTFIRINDEX:
        {
            UpdateOutputFIR(Handfree_Coef_Index, par2);
            break;
        }
        case SET_LOAD_VOLUME_SETTING:
        {
            mAudioALSAVolumeController->initVolumeController();
            setMasterVolume(mAudioALSAVolumeController->getMasterVolume());
            const sp<IAudioPolicyService> &aps = AudioSystem::get_audio_policy_service();
            break;
        }
        case SET_SPEECH_VM_ENABLE:
        {
            ALOGD("+SET_SPEECH_VM_ENABLE(%d)", par2);

            mStreamManager->SetSpeechVmEnable(par2);
            ALOGD("-SET_SPEECH_VM_ENABLE(%d)", par2);
            break;
        }
        case SET_DUMP_SPEECH_DEBUG_INFO:
        {
            ALOGD(" SET_DUMP_SPEECH_DEBUG_INFO(%d)", par2);
            //mSpeechDriverFactory->GetSpeechDriver()->ModemDumpSpeechParam();
            break;
        }
        case SET_DUMP_AUDIO_DEBUG_INFO:
        {
            ALOGD(" SET_DUMP_AUDIO_DEBUG_INFO(%d)", par2);
            AudioALSAHardwareResourceManager::getInstance()->setAudioDebug(true);
            break;
        }
        case SET_DUMP_AUDIO_AEE_CHECK:
        {
            ALOGD(" SET_DUMP_AUDIO_AEE_CHECK(%d)", par2);
            if (par2 == 0)
            {
                property_set("streamout.aee.dump", "0");
            }
            else
            {
                property_set("streamout.aee.dump", "1");
            }
            break;
        }
        case SET_DUMP_AUDIO_STREAM_OUT:
        {
            ALOGD(" SET_DUMP_AUDIO_STREAM_OUT(%d)", par2);
            if (par2 == 0)
            {
                property_set("streamout.pcm.dump", "0");
                //::ioctl(mFd, AUDDRV_AEE_IOCTL, 0);
            }
            else
            {
                property_set("streamout.pcm.dump", "1");
                //::ioctl(mFd, AUDDRV_AEE_IOCTL, 1);
            }
            break;
        }
        case SET_DUMP_AUDIO_MIXER_BUF:
        {
            ALOGD(" SET_DUMP_AUDIO_MIXER_BUF(%d)", par2);
            if (par2 == 0)
            {
                property_set("af.mixer.pcm", "0");
            }
            else
            {
                property_set("af.mixer.pcm", "1");
            }
            break;
        }
        case SET_DUMP_AUDIO_TRACK_BUF:
        {
            ALOGD(" SET_DUMP_AUDIO_TRACK_BUF(%d)", par2);
            if (par2 == 0)
            {
                property_set("af.track.pcm", "0");
            }
            else
            {
                property_set("af.track.pcm", "1");
            }
            break;
        }
        case SET_DUMP_A2DP_STREAM_OUT:
        {
            ALOGD(" SET_DUMP_A2DP_STREAM_OUT(%d)", par2);
            if (par2 == 0)
            {
                property_set("a2dp.streamout.pcm", "0");
            }
            else
            {
                property_set("a2dp.streamout.pcm", "1");
            }
            break;
        }
        case SET_DUMP_AUDIO_STREAM_IN:
        {
            ALOGD(" SET_DUMP_AUDIO_STREAM_IN(%d)", par2);
            if (par2 == 0)
            {
                property_set("streamin.pcm.dump", "0");
            }
            else
            {
                property_set("streamin.pcm.dump", "1");
            }
            break;
        }
        case SET_DUMP_IDLE_VM_RECORD:
        {
            ALOGD(" SET_DUMP_IDLE_VM_RECORD(%d)", par2);
#if defined(MTK_AUDIO_HD_REC_SUPPORT)
            if (par2 == 0)
            {
                property_set("streamin.vm.dump", "0");
            }
            else
            {
                property_set("streamin.vm.dump", "1");
            }
#endif
            break;
        }
        case SET_DUMP_AP_SPEECH_EPL:
        {
            ALOGD(" SET_DUMP_AP_SPEECH_EPL(%d)", par2);
            if (par2 == 0)
            {
                property_set("streamin.epl.dump", "0");
            }
            else
            {
                property_set("streamin.epl.dump", "1");
            }
            break;
        }
        case SET_MagiASR_TEST_ENABLE:
        {
            ALOGD(" SET_MagiASR_TEST_ENABLE(%d)", par2);
            if (par2 == 0)
            {
                //disable MagiASR verify mode
                mAudioSpeechEnhanceInfoInstance->SetForceMagiASR(false);
            }
            else
            {
                //enable MagiASR verify mode
                mAudioSpeechEnhanceInfoInstance->SetForceMagiASR(true);
            }
            break;
        }
        case SET_AECREC_TEST_ENABLE:
        {
            ALOGD(" SET_AECREC_TEST_ENABLE(%d)", par2);
            if (par2 == 0)
            {
                //disable AECRec verify mode
                mAudioSpeechEnhanceInfoInstance->SetForceAECRec(false);
            }
            else
            {
                //enable AECRec verify mode
                mAudioSpeechEnhanceInfoInstance->SetForceAECRec(true);
            }
            break;
        }
        case SET_CUREENT_SENSOR_ENABLE:
        {

            ALOGD("SET_CUREENT_SENSOR_ENABLE");
            AudioALSAHardwareResourceManager::getInstance()->setSPKCurrentSensor((bool)par2);
            break;
        }
        case SET_CURRENT_SENSOR_RESET:
        {
            ALOGD("SET_CURRENT_SENSOR_RESET");
            AudioALSAHardwareResourceManager::getInstance()->setSPKCurrentSensorPeakDetectorReset((bool)par2);
            break;
        }
        case SET_SPEAKER_MONITOR_TEMP_UPPER_BOUND:
        {
            ALOGD("SET_SPEAKER_MONITOR_TEMP_UPPER_BOUND %d", par2);
            break;
        }
        case SET_SPEAKER_MONITOR_TEMP_LOWER_BOUND:
        {
            ALOGD("SET_SPEAKER_MONITOR_TEMP_LOWER_BOUND %d", par2);
            break;
        }
        case SET_TDM_LOOPBACK_I0I1_ENABLE:
        {
            // HDMI record enable

        }
        case TEST_AUDIODATA:
        {
            valAudioCmd = par2;
            ALOGD("%s(), TEST_AUDIODATA(0x%x), valAudioCmd = %d", __FUNCTION__, par1, valAudioCmd);
            break;
        }
        default:
            ALOGW("-%s(), Unknown command par1 = 0x%x, par2 = %d", __FUNCTION__, par1, par2);
            break;
    }
    return NO_ERROR;
}

status_t AudioALSAHardware::GetAudioCommand(int par1)
{
    ALOGD("%s(), par1 = 0x%x", __FUNCTION__, par1);
    int result = 0 ;
    char value[PROPERTY_VALUE_MAX];

    switch (par1)
    {
        case GETOUTPUTFIRINDEX:
        {
            AUDIO_PARAM_MED_STRUCT pMedPara;
            GetMedParamFromNV(&pMedPara);
            result = pMedPara.select_FIR_output_index[Normal_Coef_Index];
            break;
        }
        case GETAUDIOCUSTOMDATASIZE:
        {
            int AudioCustomDataSize = sizeof(AUDIO_VOLUME_CUSTOM_STRUCT);
            ALOGD("GETAUDIOCUSTOMDATASIZE  AudioCustomDataSize = %d", AudioCustomDataSize);
            return AudioCustomDataSize;
        }
        case GETNORMALOUTPUTFIRINDEX:
        {
            AUDIO_PARAM_MED_STRUCT pMedPara;
            GetMedParamFromNV(&pMedPara);
            result = pMedPara.select_FIR_output_index[Normal_Coef_Index];
            break;
        }
        case GETHEADSETOUTPUTFIRINDEX:
        {
            AUDIO_PARAM_MED_STRUCT pMedPara;
            GetMedParamFromNV(&pMedPara);
            result = pMedPara.select_FIR_output_index[Headset_Coef_Index];
            break;
        }
        case GETSPEAKEROUTPUTFIRINDEX:
        {
            AUDIO_PARAM_MED_STRUCT pMedPara;
            GetMedParamFromNV(&pMedPara);
            result = pMedPara.select_FIR_output_index[Handfree_Coef_Index];
            break;
        }
        case GET_DUMP_AUDIO_AEE_CHECK:
        {
            property_get("streamout.aee.dump", value, "0");
            result = atoi(value);
            ALOGD(" GET_DUMP_AUDIO_STREAM_OUT=%d", result);
            break;
        }
        case GET_DUMP_AUDIO_STREAM_OUT:
        {
            property_get("streamout.pcm.dump", value, "0");
            result = atoi(value);
            ALOGD(" GET_DUMP_AUDIO_STREAM_OUT=%d", result);
            break;
        }
        case GET_DUMP_AUDIO_MIXER_BUF:
        {
            property_get("af.mixer.pcm", value, "0");
            result = atoi(value);
            ALOGD(" GET_DUMP_AUDIO_MIXER_BUF=%d", result);
            break;
        }
        case GET_DUMP_AUDIO_TRACK_BUF:
        {
            property_get("af.track.pcm", value, "0");
            result = atoi(value);
            ALOGD(" GET_DUMP_AUDIO_TRACK_BUF=%d", result);
            break;
        }
        case GET_DUMP_A2DP_STREAM_OUT:
        {
            property_get("a2dp.streamout.pcm", value, "0");
            result = atoi(value);
            ALOGD(" GET_DUMP_A2DP_STREAM_OUT=%d", result);
            break;
        }
        case GET_DUMP_AUDIO_STREAM_IN:
        {
            property_get("streamin.pcm.dump", value, "0");
            result = atoi(value);
            ALOGD(" GET_DUMP_AUDIO_STREAM_IN=%d", result);
            break;
        }
        case GET_DUMP_IDLE_VM_RECORD:
        {
#if defined(MTK_AUDIO_HD_REC_SUPPORT)
            property_get("streamin.vm.dump", value, "0");
            result = atoi(value);
#else
            result = 0;
#endif
            ALOGD(" GET_DUMP_IDLE_VM_RECORD=%d", result);
            break;
        }
        case GET_DUMP_AP_SPEECH_EPL:
        {
            property_get("streamin.epl.dump", value, "0");
            result = atoi(value);

            int result1 = 0 ;
            char value1[PROPERTY_VALUE_MAX];
            property_get("streamin.epl.dump", value1, "0");
            result1 = atoi(value1);

            if (result1 == 1)
            {
                result = 1;
            }
            ALOGD(" GET_DUMP_AP_SPEECH_EPL=%d", result);
            break;
        }
        case GET_MagiASR_TEST_ENABLE:
        {
            //get the MagiASR verify mode status
            result = mAudioSpeechEnhanceInfoInstance->GetForceMagiASRState();
            ALOGD(" GET_MagiASR_TEST_ENABLE=%d", result);
            break;
        }
        case GET_AECREC_TEST_ENABLE:
        {
            //get the AECRec verify mode status
            result = 0;
            if (mAudioSpeechEnhanceInfoInstance->GetForceAECRecState())
            {
                result = 1;
            }

            ALOGD(" GET_AECREC_TEST_ENABLE=%d", result);
            break;
        }
        case TEST_AUDIODATA:
        {
            result = valAudioCmd;
            ALOGD("%s(), TEST_AUDIODATA(0x%x), valAudioCmd = %d", __FUNCTION__, par1, valAudioCmd);
            break;
        }

        default:
            ALOGW("-%s(), Unknown command par1=0x%x", __FUNCTION__, par1);
            break;
    }

    // call fucntion want to get status adn return it.
    return result;
}

status_t AudioALSAHardware::SetAudioCommonData(int par1, size_t len, void *ptr)
{
    ALOGD("%s(), par1 = 0x%x, len = %zu", __FUNCTION__, par1, len);
    switch (par1)
    {
        case SETMEDDATA:
        {
            ASSERT(len == sizeof(AUDIO_PARAM_MED_STRUCT));
            SetMedParamToNV((AUDIO_PARAM_MED_STRUCT *)ptr);
            break;
        }
        case SETAUDIOCUSTOMDATA:
        {
            ASSERT(len == sizeof(AUDIO_VOLUME_CUSTOM_STRUCT));
            SetAudioCustomParamToNV((AUDIO_VOLUME_CUSTOM_STRUCT *)ptr);
            mAudioALSAVolumeController->initVolumeController();
            setMasterVolume(mAudioALSAVolumeController->getMasterVolume());
            break;
        }
#if defined(MTK_DUAL_MIC_SUPPORT)
        case SET_DUAL_MIC_PARAMETER:
        {
            ASSERT(len == sizeof(AUDIO_CUSTOM_EXTRA_PARAM_STRUCT));
            SetDualMicSpeechParamToNVRam((AUDIO_CUSTOM_EXTRA_PARAM_STRUCT *)ptr);
            mAudioALSAVolumeController->initVolumeController();
            break;
        }
#endif
        case SET_NB_SPEECH_PARAMETER:
        {
            ALOGD("%s() len [%d] sizeof [%d]", __FUNCTION__, len, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));

            if (len == sizeof(AUDIO_CUSTOM_PARAM_STRUCT))
            {
                AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB = (AUDIO_CUSTOM_PARAM_STRUCT *)ptr;
                mStreamManager->SetEMParameter(pSphParamNB);
            }
            else
            {
                ALOGE("len [%d] != Sizeof(AUDIO_CUSTOM_PARAM_STRUCT) [%d] ", len, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));
            }
            break;
        }

#if defined(MTK_WB_SPEECH_SUPPORT)
        case SET_WB_SPEECH_PARAMETER:
        {
            ALOGD("%s(), len [%d] sizeof [%d]", __FUNCTION__, len, sizeof(AUDIO_CUSTOM_WB_PARAM_STRUCT));
            ASSERT(len == sizeof(AUDIO_CUSTOM_WB_PARAM_STRUCT));
            SetWBSpeechParamToNVRam((AUDIO_CUSTOM_WB_PARAM_STRUCT *)ptr);
            SpeechEnhancementController::GetInstance()->SetWBSpeechParametersToAllModem((AUDIO_CUSTOM_WB_PARAM_STRUCT *)ptr);
            mAudioALSAVolumeController->initVolumeController(); // for DRC2.0 need volume to get speech mode
            break;
        }
#endif
        case SET_AUDIO_VER1_DATA:
        {
            ASSERT(len == sizeof(AUDIO_VER1_CUSTOM_VOLUME_STRUCT));

            SetVolumeVer1ParamToNV((AUDIO_VER1_CUSTOM_VOLUME_STRUCT *)ptr);
            mAudioALSAVolumeController->initVolumeController();
            const sp<IAudioPolicyService> &aps = AudioSystem::get_audio_policy_service();
            setMasterVolume(mAudioALSAVolumeController->getMasterVolume());
            break;
        }
        case HOOK_FM_DEVICE_CALLBACK:
        {
            AudioALSAFMController::getInstance()->setFmDeviceCallback((AUDIO_DEVICE_CHANGE_CALLBACK_STRUCT *)ptr);
            break;
        }
        case UNHOOK_FM_DEVICE_CALLBACK:
        {
            AudioALSAFMController::getInstance()->setFmDeviceCallback(NULL);
            break;
        }
        case HOOK_BESLOUDNESS_CONTROL_CALLBACK:
        {
            mStreamManager->SetBesLoudnessControlCallback((BESLOUDNESS_CONTROL_CALLBACK_STRUCT *)ptr);
            break;
        }
        case UNHOOK_BESLOUDNESS_CONTROL_CALLBACK:
        {
            mStreamManager->SetBesLoudnessControlCallback(NULL);
            break;
        }
        case TEST_AUDIODATA:
        {
            ALOGD("%s(), TEST_AUDIODATA(0x%x), len [%d]", __FUNCTION__, par1, len);
            memcpy(pAudioBuffer, (char *) ptr,  len);
            ALOGD("%s(), TEST_AUDIODATA(0x%x), after write=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", __FUNCTION__, par1, pAudioBuffer, pAudioBuffer + 1, pAudioBuffer + 2, pAudioBuffer + 3, pAudioBuffer + 4, pAudioBuffer + 5, pAudioBuffer + 6, pAudioBuffer + 7, pAudioBuffer + 8, pAudioBuffer + 9);
            break;
        }
        default:
            ALOGW("-%s(), Unknown command par1=0x%x, len=%d", __FUNCTION__, par1, len);
            break;
    }
    return NO_ERROR;
}

status_t AudioALSAHardware::GetAudioCommonData(int par1, size_t len, void *ptr)
{
    ALOGD("%s par1=%d, len=%zu", __FUNCTION__, par1, len);
    switch (par1)
    {
        case GETMEDDATA:
        {
            ASSERT(len == sizeof(AUDIO_PARAM_MED_STRUCT));
            GetMedParamFromNV((AUDIO_PARAM_MED_STRUCT *)ptr);
            break;
        }
        case GETAUDIOCUSTOMDATA:
        {
            ASSERT(len == sizeof(AUDIO_VOLUME_CUSTOM_STRUCT));
            GetAudioCustomParamFromNV((AUDIO_VOLUME_CUSTOM_STRUCT *)ptr);
            break;
        }
#if defined(MTK_DUAL_MIC_SUPPORT)
        case GET_DUAL_MIC_PARAMETER:
        {
            ASSERT(len == sizeof(AUDIO_CUSTOM_EXTRA_PARAM_STRUCT));
            GetDualMicSpeechParamFromNVRam((AUDIO_CUSTOM_EXTRA_PARAM_STRUCT *)ptr);
            break;
        }
#endif
        case GET_NB_SPEECH_PARAMETER:
        {
            ALOGD("%s(), sizeof(AUDIO_CUSTOM_PARAM_STRUCT) =%d", __FUNCTION__, sizeof(AUDIO_CUSTOM_PARAM_STRUCT));
            ASSERT(len == sizeof(AUDIO_CUSTOM_PARAM_STRUCT));
            GetNBSpeechParamFromNVRam((AUDIO_CUSTOM_PARAM_STRUCT *)ptr);
            AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB = (AUDIO_CUSTOM_PARAM_STRUCT *)ptr;
            ALOGD("%s(), uAutoVM = 0x%x, uMicbiasVolt=0x%x, debug_info[0] = %u, speech_common_para[0] = %u", __FUNCTION__,
                  pSphParamNB->uAutoVM, pSphParamNB->uMicbiasVolt, pSphParamNB->debug_info[0], pSphParamNB->speech_common_para[0]);
            break;
        }
#if defined(MTK_WB_SPEECH_SUPPORT)
        case GET_WB_SPEECH_PARAMETER:
        {
            ASSERT(len == sizeof(AUDIO_CUSTOM_WB_PARAM_STRUCT));
            GetWBSpeechParamFromNVRam((AUDIO_CUSTOM_WB_PARAM_STRUCT *) ptr);
            break;
        }
#endif
        case GET_AUDIO_VER1_DATA:
        {
            GetVolumeVer1ParamFromNV((AUDIO_VER1_CUSTOM_VOLUME_STRUCT *) ptr);
            break;
        }
        case GET_AUDIO_POLICY_VOL_FROM_VER1_DATA:
        {
            AUDIO_CUSTOM_VOLUME_STRUCT *pTarget = (AUDIO_CUSTOM_VOLUME_STRUCT *) ptr;

            if ((pTarget->bRev == CUSTOM_VOLUME_REV_1) && (len == sizeof(AUDIO_CUSTOM_VOLUME_STRUCT)))
            {
                AUDIO_VER1_CUSTOM_VOLUME_STRUCT Source;
                GetVolumeVer1ParamFromNV(&Source);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_VOICE_CALL], (void *)Source.audiovolume_sph, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_SIP], (void *)Source.audiovolume_sip, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)Source.audiovolume_ring[VOLUME_NORMAL_MODE], (void *)Source.audiovolume_ring[VOLUME_HEADSET_SPEAKER_MODE], sizeof(unsigned char)*AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)Source.audiovolume_ring[VOLUME_HEADSET_MODE], (void *)Source.audiovolume_ring[VOLUME_HEADSET_SPEAKER_MODE], sizeof(unsigned char)*AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_RING], (void *)Source.audiovolume_ring, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_ALARM], (void *)Source.audiovolume_ring, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_NOTIFICATION], (void *)Source.audiovolume_ring, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_MUSIC], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_ENFORCED_AUDIBLE], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_TTS], (void *)audiovolume_tts_nonspeaker, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_TTS][CUSTOM_VOLUME_SPEAKER_MODE], (void *)Source.audiovolume_media[VOLUME_SPEAKER_MODE], sizeof(unsigned char)* AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_BLUETOOTH_SCO], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_SYSTEM], (void *)audiovolume_system, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_DTMF], (void *)audiovolume_dtmf, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_ACCESSIBILITY], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_REROUTING], (void *)audiovolume_rerouting, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
                memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_PATCH], (void *)audiovolume_patch, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_VOICE_CALL] = Source.audiovolume_level[VER1_VOL_TYPE_SPH];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_SYSTEM] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_RING] = Source.audiovolume_level[VER1_VOL_TYPE_RING];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_MUSIC] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_ALARM] = Source.audiovolume_level[VER1_VOL_TYPE_RING];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_NOTIFICATION] = Source.audiovolume_level[VER1_VOL_TYPE_RING];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_BLUETOOTH_SCO] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_ENFORCED_AUDIBLE] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_DTMF] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_TTS] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_SIP] = Source.audiovolume_level[VER1_VOL_TYPE_SIP];
                pTarget->audiovolume_level[CUSTOM_VOL_TYPE_ACCESSIBILITY] = pTarget->audiovolume_level[CUSTOM_VOL_TYPE_REROUTING] = pTarget->audiovolume_level[CUSTOM_VOL_TYPE_PATCH] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
                pTarget->bReady = 1;
                mUseTuningVolume = true;
                memcpy((void*)&VolCache,(void*)pTarget,sizeof(AUDIO_CUSTOM_VOLUME_STRUCT));
                ALOGD("Get PolicyCustomVolume");
            }
            break;
        }
        case GET_VOICE_CUST_PARAM:
        {
            GetVoiceRecogCustParamFromNV((VOICE_RECOGNITION_PARAM_STRUCT *)ptr);
            break;
        }
        case GET_VOICE_FIR_COEF:
        {
            AUDIO_HD_RECORD_PARAM_STRUCT custHDRECParam;
            GetHdRecordParamFromNV(&custHDRECParam);
            ASSERT(len == sizeof(custHDRECParam.hd_rec_fir));
            memcpy(ptr, (void *)custHDRECParam.hd_rec_fir, len);
            break;
        }
        case GET_VOICE_GAIN:
        {
            AUDIO_VER1_CUSTOM_VOLUME_STRUCT custGainParam;
            GetVolumeVer1ParamFromNV(&custGainParam);
            uint16_t *pGain = (uint16_t *)ptr;
            *pGain = mAudioALSAVolumeController->MappingToDigitalGain(custGainParam.audiovolume_mic[VOLUME_NORMAL_MODE][7]);
            *(pGain + 1) = mAudioALSAVolumeController->MappingToDigitalGain(custGainParam.audiovolume_mic[VOLUME_HEADSET_MODE][7]);
            break;
        }
        case TEST_AUDIODATA:
        {
            ALOGD("%s(), TEST_AUDIODATA(0x%x), len [%d]", __FUNCTION__, par1, len);
            memcpy((char *)ptr, pAudioBuffer, len);
            ALOGD("%s(), TEST_AUDIODATA(0x%x), read print=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", __FUNCTION__, par1, pAudioBuffer, pAudioBuffer + 1, pAudioBuffer + 2, pAudioBuffer + 3, pAudioBuffer + 4, pAudioBuffer + 5, pAudioBuffer + 6, pAudioBuffer + 7, pAudioBuffer + 8, pAudioBuffer + 9);
            break;
        }
        default:
            ALOGW("-%s(), Unknown command par1=0x%x, len=%d", __FUNCTION__, par1, len);
            break;
    }
    return NO_ERROR;
}

status_t AudioALSAHardware::SetAudioData(int par1, size_t len, void *ptr)
{
    ALOGD("%s(), par1 = 0x%x, len = %zu", __FUNCTION__, par1, len);
    return SetAudioCommonData(par1, len, ptr);
}

status_t AudioALSAHardware::GetAudioData(int par1, size_t len, void *ptr)
{
    ALOGD("%s(), par1 = 0x%x, len = %zu", __FUNCTION__, par1, len);
    return GetAudioCommonData(par1, len, ptr);
}

// ACF Preview parameter
status_t AudioALSAHardware::SetACFPreviewParameter(void *ptr , int len)
{
    ALOGD("%s()", __FUNCTION__);
    mStreamManager->SetACFPreviewParameter(ptr, len);
    return NO_ERROR;
}

status_t AudioALSAHardware::SetHCFPreviewParameter(void *ptr , int len)
{
    ALOGD("%s()", __FUNCTION__);
    mStreamManager->SetHCFPreviewParameter(ptr, len);
    return NO_ERROR;
}

void AudioALSAHardware::dumpAudioPortConfig(const struct audio_port_config *port_config)
{
    const char* strAudioPatchRole[] = { "AUDIO_PORT_ROLE_NONE", "AUDIO_PORT_ROLE_SOURCE", "AUDIO_PORT_ROLE_SINK" };
    const char* strAudioPatchType[] = { "AUDIO_PORT_TYPE_NONE", "AUDIO_PORT_TYPE_DEVICE", "AUDIO_PORT_TYPE_MIX", "AUDIO_PORT_TYPE_SESSION" };
    unsigned int i;

    ALOGV("id(%d) role(%s) type(%s) config_mask(0x%x)",
          port_config->id, strAudioPatchRole[port_config->role], strAudioPatchType[port_config->type],
          port_config->config_mask);
    ALOGV("sample_rate = %u channel_mask = 0x%x format = 0x%x",
          port_config->sample_rate, port_config->channel_mask, port_config->format);
    ALOGV("audio_gain_config index = %d mode = 0x%x channel_mask = 0x%x ramp_duration_ms = %u",
          port_config->gain.index, port_config->gain.mode, port_config->gain.channel_mask,
          port_config->gain.ramp_duration_ms);

    for (i = 0; i < (sizeof(port_config->gain.values)/sizeof(port_config->gain.values[0])); i++) {
        ALOGV("audio_gain_config values[%d] = %d", i, port_config->gain.values[i]);
    }

    if (port_config->type == AUDIO_PORT_TYPE_DEVICE) {
        ALOGV("device hw_module = %d type = 0x%x address = %s",
              port_config->ext.device.hw_module, port_config->ext.device.type, port_config->ext.device.address);
    } else if (port_config->type == AUDIO_PORT_TYPE_MIX) {
        ALOGV("mix hw_module = %d handle = %d stream = %d source = %d",
              port_config->ext.mix.hw_module, port_config->ext.mix.handle,
              port_config->ext.mix.usecase.stream, port_config->ext.mix.usecase.source);
    } else if (port_config->type == AUDIO_PORT_TYPE_SESSION) {
        ALOGV("session session = %d", port_config->ext.session.session);
    }
}

void AudioALSAHardware::setScreenState(bool mode)
{
    mStreamManager->setScreenState(mode);
}

bool AudioALSAHardware::UpdateOutputFIR(int mode , int index)
{
    ALOGD("%s(),  mode = %d, index = %d", __FUNCTION__, mode, index);

    // save index to MED with different mode.
    AUDIO_PARAM_MED_STRUCT eMedPara;
    GetMedParamFromNV(&eMedPara);
    eMedPara.select_FIR_output_index[mode] = index;

    // copy med data into audio_custom param
    AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
    GetNBSpeechParamFromNVRam(&eSphParamNB);

    for (int i = 0; i < NB_FIR_NUM;  i++)
    {
        ALOGD("eSphParamNB.sph_out_fir[%d][%d] = %d <= eMedPara.speech_output_FIR_coeffs[%d][%d][%d] = %d",
              mode, i, eSphParamNB.sph_out_fir[mode][i],
              mode, index, i, eMedPara.speech_output_FIR_coeffs[mode][index][i]);
    }

    memcpy((void *)eSphParamNB.sph_out_fir[mode],
           (void *)eMedPara.speech_output_FIR_coeffs[mode][index],
           sizeof(eSphParamNB.sph_out_fir[mode]));

    // set to nvram
    SetNBSpeechParamToNVRam(&eSphParamNB);
    SetMedParamToNV(&eMedPara);

    // set to modem side
#ifdef MTK_AUDIO_PHONE_CALL_SUPPORT
    SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(&eSphParamNB);
#endif

    return true;
}

status_t AudioALSAHardware::AudioHalPatch::dump(int fd, int spaces, int index) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%*sAudio Patch %d:\n", spaces, "", index + 1);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- hal handle: %2d\n", spaces, "", mHalHandle);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- %d sources:\n", spaces, "", mAudioPatch.num_sources);
    result.append(buffer);
    for (size_t i = 0; i < mAudioPatch.num_sources; i++) {
        if (mAudioPatch.sources[i].type == AUDIO_PORT_TYPE_DEVICE) {
            snprintf(buffer, SIZE, "%*s- Device ID %d Device 0x%x\n", spaces + 2, "",
                     mAudioPatch.sources[i].id, mAudioPatch.sources[i].ext.device.type);
        } else if (mAudioPatch.sources[i].type == AUDIO_PORT_TYPE_MIX) {
            snprintf(buffer, SIZE, "%*s- Mix ID %d I/O handle %d\n", spaces + 2, "",
                     mAudioPatch.sources[i].id,  mAudioPatch.sources[i].ext.mix.handle);
        } else if (mAudioPatch.sources[i].type == AUDIO_PORT_TYPE_SESSION) {
            snprintf(buffer, SIZE, "%*s- Session ID %d Session Type %d\n", spaces + 2, "",
                     mAudioPatch.sources[i].id, mAudioPatch.sources[i].ext.session.session);
        } else {
            snprintf(buffer, SIZE, "%*s- ID %d Type %d\n", spaces + 2, "",
                     mAudioPatch.sources[i].id, mAudioPatch.sources[i].type);
        }
        result.append(buffer);
    }
    snprintf(buffer, SIZE, "%*s- %d sinks:\n", spaces, "", mAudioPatch.num_sinks);
    result.append(buffer);
    for (size_t i = 0; i < mAudioPatch.num_sinks; i++) {
        if (mAudioPatch.sinks[i].type == AUDIO_PORT_TYPE_DEVICE) {
            snprintf(buffer, SIZE, "%*s- Device ID %d Device 0x%x\n", spaces + 2, "",
                     mAudioPatch.sinks[i].id, mAudioPatch.sinks[i].ext.device.type);
        } else if (mAudioPatch.sinks[i].type == AUDIO_PORT_TYPE_MIX) {
            snprintf(buffer, SIZE, "%*s- Mix ID %d I/O handle %d\n", spaces + 2, "",
                     mAudioPatch.sinks[i].id,  mAudioPatch.sinks[i].ext.mix.handle);
        } else if (mAudioPatch.sinks[i].type == AUDIO_PORT_TYPE_SESSION) {
            snprintf(buffer, SIZE, "%*s- Session ID %d Session Type %d\n", spaces + 2, "",
                     mAudioPatch.sinks[i].id, mAudioPatch.sinks[i].ext.session.session);
        } else {
            snprintf(buffer, SIZE, "%*s- ID %d Type %d\n", spaces + 2, "",
                     mAudioPatch.sinks[i].id, mAudioPatch.sinks[i].type);
        }
        result.append(buffer);
    }

    write(fd, result.string(), result.size());
    return NO_ERROR;
}

status_t AudioALSAHardware::GetAudioCustomVol(AUDIO_CUSTOM_VOLUME_STRUCT *pAudioCustomVol,int dStructLen)
{
    AUDIO_CUSTOM_VOLUME_STRUCT *pTarget = (AUDIO_CUSTOM_VOLUME_STRUCT *) pAudioCustomVol;
    ALOGD("%s Get PolicyCustomVolume",__FUNCTION__);
    if ((pTarget->bRev == CUSTOM_VOLUME_REV_1) && (dStructLen == sizeof(AUDIO_CUSTOM_VOLUME_STRUCT)))
    {
        AUDIO_VER1_CUSTOM_VOLUME_STRUCT Source;
        GetVolumeVer1ParamFromNV(&Source);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_VOICE_CALL], (void *)Source.audiovolume_sph, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_SIP], (void *)Source.audiovolume_sip, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)Source.audiovolume_ring[VOLUME_NORMAL_MODE], (void *)Source.audiovolume_ring[VOLUME_HEADSET_SPEAKER_MODE], sizeof(unsigned char)*AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)Source.audiovolume_ring[VOLUME_HEADSET_MODE], (void *)Source.audiovolume_ring[VOLUME_HEADSET_SPEAKER_MODE], sizeof(unsigned char)*AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_RING], (void *)Source.audiovolume_ring, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_ALARM], (void *)Source.audiovolume_ring, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_NOTIFICATION], (void *)Source.audiovolume_ring, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_MUSIC], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_ENFORCED_AUDIBLE], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_TTS], (void *)audiovolume_tts_nonspeaker, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_TTS][CUSTOM_VOLUME_SPEAKER_MODE], (void *)Source.audiovolume_media[VOLUME_SPEAKER_MODE], sizeof(unsigned char)* AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_BLUETOOTH_SCO], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_SYSTEM], (void *)audiovolume_system, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_DTMF], (void *)audiovolume_dtmf, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_ACCESSIBILITY], (void *)Source.audiovolume_media, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_REROUTING], (void *)audiovolume_rerouting, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);
        memcpy((void *)pTarget->audiovolume_steamtype[CUSTOM_VOL_TYPE_PATCH], (void *)audiovolume_patch, sizeof(unsigned char)*NUM_OF_VOL_MODE * AUDIO_MAX_VOLUME_STEP);

        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_VOICE_CALL] = Source.audiovolume_level[VER1_VOL_TYPE_SPH];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_SYSTEM] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_RING] = Source.audiovolume_level[VER1_VOL_TYPE_RING];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_MUSIC] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_ALARM] = Source.audiovolume_level[VER1_VOL_TYPE_RING];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_NOTIFICATION] = Source.audiovolume_level[VER1_VOL_TYPE_RING];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_BLUETOOTH_SCO] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_ENFORCED_AUDIBLE] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_DTMF] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_TTS] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_SIP] = Source.audiovolume_level[VER1_VOL_TYPE_SIP];
        pTarget->audiovolume_level[CUSTOM_VOL_TYPE_ACCESSIBILITY] = pTarget->audiovolume_level[CUSTOM_VOL_TYPE_REROUTING] = pTarget->audiovolume_level[CUSTOM_VOL_TYPE_PATCH] = Source.audiovolume_level[VER1_VOL_TYPE_MEDIA];
        pTarget->bReady = 1;
        mUseTuningVolume = true;
        memcpy((void*)&VolCache,(void*)pTarget,sizeof(AUDIO_CUSTOM_VOLUME_STRUCT));
        ALOGD("%s Get PolicyCustomVolume OK",__FUNCTION__);
    }

    return NO_ERROR;
}

float AudioALSAHardware::MappingFMVolofOutputDev(int Gain, audio_devices_t eOutput)
{
    float fFMVolume;

    if (!(eOutput & FM_DEVICE_TO_DEVICE_SUPPORT_OUTPUT_DEVICES)) {
        ALOGE("%s invalid device 0x%x", __FUNCTION__, eOutput);
        return 1.0;
    }

    if (mUseTuningVolume == false)
    {
        // TODO: refine the gain mapping
        int dGainDB = 0;
        dGainDB = Gain / 100;

        if (dGainDB >= 0) //Support degrade only
            dGainDB = 0;
        else
            dGainDB = (-1)*dGainDB;

        dGainDB = dGainDB << 2;

        if (dGainDB > 256)
            dGainDB = 256;

        dGainDB = 256 - dGainDB;

        fFMVolume = AudioALSAVolumeController::linearToLog(dGainDB);

        if (fFMVolume < 0)
            fFMVolume = 0;
        else if (fFMVolume > 1.0)
            fFMVolume = 1.0;
    }
    else
    {
        const float fCUSTOM_VOLUME_MAPPING_STEP = 256.0f;
        unsigned char* array;

        if (eOutput & AUDIO_DEVICE_OUT_SPEAKER)
            array = VolCache.audiovolume_steamtype[CUSTOM_VOL_TYPE_MUSIC][CUSTOM_VOLUME_SPEAKER_MODE];
        else
            array = VolCache.audiovolume_steamtype[CUSTOM_VOL_TYPE_MUSIC][CUSTOM_VOLUME_HEADSET_MODE];

        int dIndex = 15 - (((-1)*Gain)/300);
        int dMaxIndex = VolCache.audiovolume_level[CUSTOM_VOL_TYPE_MUSIC];

        if (dIndex > 15)
            dIndex = 15;
        else if (dIndex < 0)
            dIndex = 0;

        float vol = (fCUSTOM_VOLUME_MAPPING_STEP * dIndex) / dMaxIndex;
        float volume = 0.0;

        if (vol == 0) {
            volume = vol;
        } else {    // map volume value to custom volume
            float unitstep = fCUSTOM_VOLUME_MAPPING_STEP/dMaxIndex;
            if (vol < fCUSTOM_VOLUME_MAPPING_STEP/dMaxIndex) {
                volume = array[0];
            } else {
                int Index = (vol+0.5)/unitstep;
                vol -= (Index*unitstep);
                float Remind = (1.0 - (float)vol/unitstep);
                if (Index != 0) {
                    volume = ((array[Index]  - (array[Index] - array[Index-1]) * Remind)+0.5);
                } else {
                    volume = 0;
                }
            }
            // -----clamp for volume
            if ( volume > 253.0) {
                volume = fCUSTOM_VOLUME_MAPPING_STEP;
            } else if ( volume <= array[0]) {
                volume = array[0];
            }
        }

        fFMVolume = AudioALSAVolumeController::linearToLog(volume);
    }


    ALOGD("%s fFMVolume = %f", __FUNCTION__, fFMVolume);
    return fFMVolume;
}

}

extern "C" android_audio_legacy::AudioMTKHardwareInterface *createAudioMTKHardware(void)
{
    return android_audio_legacy::AudioMTKHardwareInterface::create();
}

namespace android_audio_legacy
{

AudioStreamOut::~AudioStreamOut() {}

status_t AudioStreamOut::getNextWriteTimestamp(int64_t *timestamp)
{
    (void)timestamp;
    return INVALID_OPERATION;
}

status_t AudioStreamOut::getPresentationPosition(uint64_t *frames __unused, struct timespec *timestamp __unused)
{
    return INVALID_OPERATION;
}

AudioMTKStreamInInterface::~AudioMTKStreamInInterface() {}

AudioMTKStreamOutInterface::~AudioMTKStreamOutInterface() {}

AudioStreamIn::~AudioStreamIn() {}

AudioMTKHardwareInterface *AudioMTKHardwareInterface::create()
{
    AudioMTKHardwareInterface *hw = new android::AudioALSAHardware();
    return hw;
}

extern "C" AudioMTKHardwareInterface *createMTKAudioHardware()
{
    /*
     * FIXME: This code needs to instantiate the correct audio device
     * interface. For now - we use compile-time switches.
     */
    return AudioMTKHardwareInterface::create();

}

}; // namespace android_audio_legacy

