#include "SpeechDriverOpenDSP.h"

#include <utils/threads.h>
#include <cutils/properties.h>

#include <dlfcn.h>


#include <cutils/properties.h>

#include "AudioLock.h"
#include "AudioUtility.h"

#include "SpeechDriverLAD.h"
#include "SpeechDriverDummy.h"

#include "AudioMessengerIPI.h"

#include "audio_speech_msg_id.h"

#define LOG_TAG "SpeechDriverOpenDSP"


namespace android
{

/**
 * lib related
 */
static const char PROPERTY_KEY_SCP_CALL_STATE[NUM_MODEM][PROPERTY_KEY_MAX] =
{
    "persist.af.scp.call_state.md1",
    "persist.af.scp.call_state.md2",
    "persist.af.scp.call_state.md3"
};

static const char PROPERTY_KEY_PARAM_PATH[PROPERTY_KEY_MAX] = "persist.af.aurisys.param_path";
static const char PROPERTY_KEY_PRODUCT_NAME[PROPERTY_KEY_MAX] = "ro.product.model";
static const char PROPERTY_KEY_PCM_DUMP_ON[PROPERTY_KEY_MAX] = "persist.af.aurisys.pcm_dump_on";


#define AURISYS_PHONE_LIB_PATH "/system/lib/libfvaudio.so"
#define DEFAULT_PARAM_PATH     "/system/etc/FV-SAM-MTK2.dat"
#define DEFAULT_PRODUCT_NAME   "demo_phone"

#define DUMP_DSP_PCM_DATA_PATH "/sdcard/mtklog/aurisys_dump/"

static void *gAurisysLib = NULL;
static int   gEnhMode;

static char  gEnhParamFilePath[256];
static char  gPhoneProductName[256];

static int   gRetvalSetParameter;


typedef lib_status_t (*arsi_query_param_buf_size_t)(
    const arsi_task_config_t *p_arsi_task_config,
    const string_buf_t       *platform_name,
    const string_buf_t       *param_file_path,
    const int                 enhancement_mode,
    uint32_t                 *p_param_buf_size);

typedef lib_status_t (*arsi_parsing_param_file_t)(
    const arsi_task_config_t *p_arsi_task_config,
    const string_buf_t       *platform_name,
    const string_buf_t       *param_file_path,
    const int                 enhancement_mode,
    data_buf_t               *p_param_buf);

static arsi_query_param_buf_size_t fvsoft_arsi_query_param_buf_size = NULL;
static arsi_parsing_param_file_t fvsoft_arsi_parsing_param_file = NULL;
static bool gEnableLibLogHAL = false;
static bool gEnableDump;
static char svalue[256];


/*==============================================================================
 *                     Singleton Pattern
 *============================================================================*/

SpeechDriverOpenDSP *SpeechDriverOpenDSP::mSpeechDriverMD1 = NULL;
SpeechDriverOpenDSP *SpeechDriverOpenDSP::mSpeechDriverMD2 = NULL;
SpeechDriverOpenDSP *SpeechDriverOpenDSP::mSpeechDriverMDExt = NULL;

SpeechDriverInterface *SpeechDriverOpenDSP::GetInstance(
    modem_index_t modem_index)
{
    static AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    SpeechDriverOpenDSP *pSpeechDriver = NULL;
    ALOGD("%s(), modem_index = %d", __FUNCTION__, modem_index);

    switch (modem_index)
    {
        case MODEM_1:
            if (mSpeechDriverMD1 == NULL)
            {
                mSpeechDriverMD1 = new SpeechDriverOpenDSP(modem_index);
            }
            pSpeechDriver = mSpeechDriverMD1;
            break;
        case MODEM_2:
            if (mSpeechDriverMD2 == NULL)
            {
                mSpeechDriverMD2 = new SpeechDriverOpenDSP(modem_index);
            }
            pSpeechDriver = mSpeechDriverMD2;
            break;
        case MODEM_EXTERNAL:
            if (mSpeechDriverMDExt == NULL)
            {
                mSpeechDriverMDExt = new SpeechDriverOpenDSP(modem_index);
            }
            pSpeechDriver = mSpeechDriverMDExt;
            break;
        default:
            ALOGE("%s: no such modem_index %d", __FUNCTION__, modem_index);
            break;
    }

    ASSERT(pSpeechDriver != NULL);
    return pSpeechDriver;
}


/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

SpeechDriverOpenDSP::SpeechDriverOpenDSP(modem_index_t modem_index) :
    pSpeechDriverInternal(NULL),
    pIPI(NULL)
{
    char property_value[PROPERTY_VALUE_MAX];

    ALOGD("%s(), modem_index = %d", __FUNCTION__, modem_index);

    mModemIndex = modem_index;

    // control internal modem & FD216
#if 1
    pSpeechDriverInternal = SpeechDriverLAD::GetInstance(modem_index);
#else // debug
    pSpeechDriverInternal = new SpeechDriverDummy(modem_index);
#endif

    pIPI = AudioMessengerIPI::getInstance();

    InitArsiTaskConfig();


    mParamBuf.memory_size = 4096;
    mParamBuf.data_size = 0;
    mParamBuf.p_buffer = malloc(mParamBuf.memory_size);
    ASSERT(mParamBuf.p_buffer != NULL);

    // TODO: move to aurisys lib manager
    if (gAurisysLib == NULL)
    {
        gAurisysLib = dlopen(AURISYS_PHONE_LIB_PATH, RTLD_NOW);
        if (gAurisysLib == NULL)
        {
            ALOGE("%s gAurisysLib err", __FUNCTION__);
        }
        else
        {
            fvsoft_arsi_query_param_buf_size =
                (arsi_query_param_buf_size_t)dlsym(gAurisysLib, "arsi_query_param_buf_size");
            if (fvsoft_arsi_query_param_buf_size == NULL)
            {
                ALOGE("%s fvsoft_arsi_query_param_buf_size err", __FUNCTION__);
            }

            fvsoft_arsi_parsing_param_file =
                (arsi_parsing_param_file_t)dlsym(gAurisysLib, "arsi_parsing_param_file");
            if (fvsoft_arsi_parsing_param_file == NULL)
            {
                ALOGE("%s fvsoft_arsi_parsing_param_file err", __FUNCTION__);
            }
        }

        gEnhMode = 0;
        gRetvalSetParameter = -1;

        property_get(PROPERTY_KEY_PARAM_PATH, gEnhParamFilePath, DEFAULT_PARAM_PATH);
        ALOGV("%s(), gEnhParamFilePath = %s", __FUNCTION__, gEnhParamFilePath);


        property_get(PROPERTY_KEY_PRODUCT_NAME, gPhoneProductName, DEFAULT_PRODUCT_NAME);
        ALOGV("%s(), gPhoneProductName = %s", __FUNCTION__, gPhoneProductName);

        /* get pcm dump state */
        property_get(PROPERTY_KEY_PCM_DUMP_ON, property_value, "0");  //"0": default off
        gEnableDump = (atoi(property_value) == 0) ? false : true;
        ALOGD("gEnableDump = %d", gEnableDump);
        pIPI->configDumpPcmEnable(gEnableDump);
        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                         IPI_MSG_A2D_PCM_DUMP_ON, gEnableDump, 0,
                         NULL);
    }

    /* get scp function state */
    property_get(PROPERTY_KEY_SCP_CALL_STATE[mModemIndex], property_value, "0");  //"0": default off
    mScpSideSpeechStatus = atoi(property_value);

    /* recovery scp state for mediaserver die */
    RecoverModemSideStatusToInitState();
}


SpeechDriverOpenDSP::~SpeechDriverOpenDSP()
{
    ALOGD("%s()", __FUNCTION__);

    pSpeechDriverInternal = NULL;
    if (mParamBuf.p_buffer != NULL)
    {
        free(mParamBuf.p_buffer);
    }

    if (gAurisysLib != NULL)
    {
        dlclose(gAurisysLib);
        gAurisysLib = NULL;
    }
}


/*==============================================================================
 *                     Lib Related
 *============================================================================*/

static void myprint(const char *message, ...)
{
    if (gEnableLibLogHAL)
    {
        static char printf_msg[256];

        va_list args;
        va_start(args, message);

        vsnprintf(printf_msg, sizeof(printf_msg), message, args);
        ALOGD("%s", printf_msg);

        va_end(args);
    }
}


int SpeechDriverOpenDSP::GetRetvalSetParameter()
{
    int retval = gRetvalSetParameter;
    gRetvalSetParameter = -1;
    return retval;
}


status_t SpeechDriverOpenDSP::SetParameter(const char *keyValuePair)
{
    char aurisys_param_file[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,PARAM_FILE";
    char aurisys_apply_param[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,APPLY_PARAM";
    char aurisys_addr_value[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,ADDR_VALUE";
    char aurisys_key_value[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,KEY_VALUE";
    char aurisys_key_dump[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,ENABLE_DUMP";
    char aurisys_key_log_dsp[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,ENABLE_LOG";
    char aurisys_key_log_hal[] = "AURISYS_SET_PARAM,HAL,PHONE_CALL,FVSAM,ENABLE_LOG";

    const int max_parse_len = 256;
    char parse_str[max_parse_len];

    if (strncmp(aurisys_param_file, keyValuePair, strlen(aurisys_param_file)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_param_file) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        ALOGV("param_file = %s", parse_str);

        SetParamFilePath(parse_str);
        gRetvalSetParameter = 1;

        return NO_ERROR;
    }

    if (strncmp(aurisys_apply_param, keyValuePair, strlen(aurisys_apply_param)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_apply_param) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enh_mode = 0;
        sscanf(parse_str, "%d", &enh_mode);
        ALOGV("enh mode = %d", enh_mode);

        status_t lib_ret = SetArsiSpeechParam(enh_mode);
        gRetvalSetParameter = (lib_ret == NO_ERROR) ? 1 : 0;

        return NO_ERROR;
    }

    if (strncmp(aurisys_addr_value, keyValuePair, strlen(aurisys_addr_value)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_addr_value) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        uint32_t aurisys_addr = 0;
        uint32_t aurisys_value = 0;
        sscanf(parse_str, "%x,%x", &aurisys_addr, &aurisys_value);
        ALOGD("addr = 0x%x, value = 0x%x", aurisys_addr, aurisys_value);

        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                         IPI_MSG_A2D_SET_ADDR_VALUE, aurisys_addr, aurisys_value,
                         NULL);

        ipi_msg_t ipi_msg;
        if (pIPI->recvIpiMsg(&ipi_msg) != NO_ERROR)
        {
            ALOGW("recvIpiMsg != NO_ERROR");
            gRetvalSetParameter = 0;
        }
        else
        {
            ALOGD("return %d", ipi_msg.param1);
            gRetvalSetParameter = (ipi_msg.param1 == 1) ? 1 : 0;
        }
        return NO_ERROR;
    }

    if (strncmp(aurisys_key_value, keyValuePair, strlen(aurisys_key_value)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_value) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';
        parse_str[strstr(parse_str, ",") - parse_str] = '=';

        ALOGD("key_value = %s", parse_str);

        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                         IPI_MSG_A2D_SET_KEY_VALUE, strlen(parse_str) + 1, strlen(parse_str) + 1,
                         parse_str);

        ipi_msg_t ipi_msg;
        if (pIPI->recvIpiMsg(&ipi_msg) != NO_ERROR)
        {
            ALOGW("recvIpiMsg != NO_ERROR");
            gRetvalSetParameter = 0;
        }
        else
        {
            ALOGD("return %d", ipi_msg.param1);
            gRetvalSetParameter = (ipi_msg.param1 == 1) ? 1 : 0;
        }
        return NO_ERROR;
    }

    if (strncmp(aurisys_key_dump, keyValuePair, strlen(aurisys_key_dump)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_dump) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enable_dump = 0;
        sscanf(parse_str, "%d", &enable_dump);
        gEnableDump = (enable_dump == 0) ? false : true;
        ALOGD("gEnableDump = %d", gEnableDump);
        property_set(PROPERTY_KEY_PCM_DUMP_ON, (gEnableDump == false) ? "0" : "1");

        pIPI->configDumpPcmEnable(gEnableDump);
        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                         IPI_MSG_A2D_PCM_DUMP_ON, gEnableDump, 0,
                         NULL);

        gRetvalSetParameter = 1;
        return NO_ERROR;
    }


    if (strncmp(aurisys_key_log_dsp, keyValuePair, strlen(aurisys_key_log_dsp)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_log_dsp) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enable_log = 0;
        sscanf(parse_str, "%d", &enable_log);
        ALOGV("enh mode = %d", enable_log);

        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                         IPI_MSG_A2D_LIB_LOG_ON, enable_log, 0,
                         NULL);

        gRetvalSetParameter = 1;
        return NO_ERROR;
    }

    if (strncmp(aurisys_key_log_hal, keyValuePair, strlen(aurisys_key_log_hal)) == 0)
    {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_log_hal) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enable_lib_log_hal = 0;
        sscanf(parse_str, "%d", &enable_lib_log_hal);
        gEnableLibLogHAL = (enable_lib_log_hal == 0) ? false : true;
        ALOGD("gEnableLibLogHAL = %d", gEnableLibLogHAL);

        gRetvalSetParameter = 1;
        return NO_ERROR;
    }

    return INVALID_OPERATION;
}


char *SpeechDriverOpenDSP::GetParameter(const char *key)
{
    char aurisys_param_file[] = "AURISYS_GET_PARAM,DSP,PHONE_CALL,FVSAM,PARAM_FILE";
    char aurisys_addr_value[] = "AURISYS_GET_PARAM,DSP,PHONE_CALL,FVSAM,ADDR_VALUE";
    char aurisys_key_value[] = "AURISYS_GET_PARAM,DSP,PHONE_CALL,FVSAM,KEY_VALUE";

    const int max_parse_len = 256;
    char parse_str[max_parse_len];

    if (strncmp(aurisys_param_file, key, strlen(aurisys_param_file)) == 0)
    {
        return GetParamFilePath();
    }

    if (strncmp(aurisys_addr_value, key, strlen(aurisys_addr_value)) == 0)
    {
        strncpy(parse_str, key + strlen(aurisys_addr_value) + 1, max_parse_len - 1); // -1: for '\0'

        uint32_t aurisys_addr = 0;
        sscanf(parse_str, "%x", &aurisys_addr);

        ALOGD("addr = 0x%x", aurisys_addr);
        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                         IPI_MSG_A2D_GET_ADDR_VALUE, aurisys_addr, 0,
                         NULL);

        ipi_msg_t ipi_msg;
        if (pIPI->recvIpiMsg(&ipi_msg) != NO_ERROR)
        {
            ALOGW("recvIpiMsg != NO_ERROR");
        }
        else
        {
            ALOGD("param1 0x%x, param2 0x%x", ipi_msg.param1, ipi_msg.param2);
        }

        if (ipi_msg.param1 == 1)
        {
            snprintf(svalue, 256 - 1, "0x%x", ipi_msg.param2); // -1: for '\0'
        }
        else
        {
            strncpy(svalue, "GET_FAIL", 256 - 1); // -1: for '\0'
        }
        return svalue;
    }

    if (strncmp(aurisys_key_value, key, strlen(aurisys_key_value)) == 0)
    {
        strncpy(parse_str, key + strlen(aurisys_key_value) + 1, max_parse_len - 1); // -1: for '\0'
        ALOGD("key = %s", parse_str);

        pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                         AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                         IPI_MSG_A2D_GET_KEY_VALUE, strlen(parse_str) + 1, max_parse_len,
                         parse_str);

        /* TODO: refine read msg - ack mechanism */
        ipi_msg_t ipi_msg;
        memset(&ipi_msg, 0, sizeof(ipi_msg_t));
        ipi_msg.magic      = IPI_MSG_MAGIC_NUMBER;
        ipi_msg.task_scene = TASK_SCENE_PHONE_CALL;
        ipi_msg.msg_layer  = AUDIO_IPI_LAYER_KERNEL;
        ipi_msg.data_type  = AUDIO_IPI_DMA;
        ipi_msg.ack_type   = AUDIO_IPI_MSG_NEED_ACK;
        ipi_msg.msg_id     = IPI_MSG_A2D_GET_KEY_VALUE;
        ipi_msg.param1     = strlen(parse_str) + 1;
        ipi_msg.param2     = max_parse_len;
        ipi_msg.dma_addr   = parse_str;
        ALOGV("ipi_msg.dma_addr = %p", ipi_msg.dma_addr);

        if (pIPI->recvIpiMsg(&ipi_msg) != NO_ERROR)
        {
            ALOGW("recvIpiMsg != NO_ERROR");
        }
        else
        {
            ALOGD("param1 0x%x, param2 0x%x", ipi_msg.param1, ipi_msg.param2);
        }
        ALOGD("key_value = %s", parse_str);

        char *p_eq = strstr(parse_str, "=");
        if (ipi_msg.param1 == 1 &&
            p_eq != NULL && p_eq < parse_str + max_parse_len - 1)
        {
            strncpy(svalue, strstr(parse_str, "=") + 1, 256 - 1); // -1: for '\0'
        }
        else
        {
            strncpy(svalue, "GET_FAIL", 256 - 1); // -1: for '\0'
        }
        return svalue;
    }

    return "GET_FAIL";
}


status_t SpeechDriverOpenDSP::SetParamFilePath(const char *file_path)
{
    ALOGD("file_path %s => %s", gEnhParamFilePath, file_path);
    strncpy(gEnhParamFilePath, file_path, 256 - 1); // -1: for '\0'

    property_set(PROPERTY_KEY_PARAM_PATH, file_path);
    return NO_ERROR;
}

char *SpeechDriverOpenDSP::GetParamFilePath()
{
    ALOGD("file_path %s", gEnhParamFilePath);
    return gEnhParamFilePath;
}


void SpeechDriverOpenDSP::InitArsiTaskConfig()
{
    /* task info */
    mArsiTaskConfig.api_version = ARSI_API_VERSION;

    mArsiTaskConfig.vendor_id = 0x0E8D; // TODO: adb shell getprop sys.usb.vid

    mArsiTaskConfig.task_scene = TASK_SCENE_PHONE_CALL;
    mArsiTaskConfig.frame_size_ms = 20;

    mArsiTaskConfig.debug_log = myprint;

    /* uplink stream */
    mArsiTaskConfig.stream_uplink.device = (uint16_t)TASK_DEVICE_IN_NONE;
    mArsiTaskConfig.stream_uplink.device_extra_info = 0;

    mArsiTaskConfig.stream_uplink.sample_rate_in  = 16000;
    mArsiTaskConfig.stream_uplink.sample_rate_out = 16000;

    mArsiTaskConfig.stream_uplink.bit_format_in  = BIT_FORMAT_S16_LE;
    mArsiTaskConfig.stream_uplink.bit_format_out = BIT_FORMAT_S16_LE;

    mArsiTaskConfig.stream_uplink.num_channels_in  = 2;
    mArsiTaskConfig.stream_uplink.num_channels_out = 1;

    /* downlink stream */
    mArsiTaskConfig.stream_downlink.device = (uint16_t)TASK_DEVICE_OUT_NONE;
    mArsiTaskConfig.stream_downlink.device_extra_info = 0;

    mArsiTaskConfig.stream_downlink.sample_rate_in  = 16000;
    mArsiTaskConfig.stream_downlink.sample_rate_out = 16000;

    mArsiTaskConfig.stream_downlink.bit_format_in  = BIT_FORMAT_S16_LE;
    mArsiTaskConfig.stream_downlink.bit_format_out = BIT_FORMAT_S16_LE;

    mArsiTaskConfig.stream_downlink.num_channels_in  = 1;
    mArsiTaskConfig.stream_downlink.num_channels_out = 1;

    /* echo ref stream */
    mArsiTaskConfig.stream_echo_ref.device = 0;
    mArsiTaskConfig.stream_echo_ref.device_extra_info = 0;

    mArsiTaskConfig.stream_echo_ref.sample_rate_in  = 16000;
    mArsiTaskConfig.stream_echo_ref.sample_rate_out = 16000;

    mArsiTaskConfig.stream_echo_ref.bit_format_in  = BIT_FORMAT_S16_LE;
    mArsiTaskConfig.stream_echo_ref.bit_format_out = BIT_FORMAT_S16_LE;

    mArsiTaskConfig.stream_echo_ref.num_channels_in  = 1;
    mArsiTaskConfig.stream_echo_ref.num_channels_out = 1;
}


/*==============================================================================
 *                     Speech Control
 *============================================================================*/

void SpeechDriverOpenDSP::SetArsiTaskConfigByDevice(
    const audio_devices_t input_device,
    const audio_devices_t output_device)
{
    /* set input device */
    switch (input_device)
    {
        case AUDIO_DEVICE_IN_BUILTIN_MIC:
        case AUDIO_DEVICE_IN_BACK_MIC:
            mArsiTaskConfig.stream_uplink.device = (uint16_t)TASK_DEVICE_IN_PHONE_MIC;
            mArsiTaskConfig.stream_uplink.num_channels_in  = 2;
            break;
        case AUDIO_DEVICE_IN_WIRED_HEADSET:
            mArsiTaskConfig.stream_uplink.device = (uint16_t)TASK_DEVICE_IN_HEADSET_MIC;
            mArsiTaskConfig.stream_uplink.num_channels_in  = 1;
            break;
        case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            mArsiTaskConfig.stream_uplink.device = (uint16_t)TASK_DEVICE_IN_BLUETOOTH_MIC;
            mArsiTaskConfig.stream_uplink.num_channels_in  = 1;
            break;
        default:
            ALOGW("%s(), device 0x%x not support, use phone mic", __FUNCTION__, input_device);
            mArsiTaskConfig.stream_uplink.device = (uint16_t)TASK_DEVICE_IN_PHONE_MIC;
            mArsiTaskConfig.stream_uplink.num_channels_in  = 2;
    }
    ASSERT(mArsiTaskConfig.stream_uplink.device != (uint16_t)TASK_DEVICE_IN_NONE);

    /* set output device */
    switch (output_device)
    {
        case AUDIO_DEVICE_OUT_EARPIECE:
            mArsiTaskConfig.stream_downlink.device = (uint16_t)TASK_DEVICE_OUT_RECEIVER;
            break;
        case AUDIO_DEVICE_OUT_SPEAKER:
            mArsiTaskConfig.stream_downlink.device = (uint16_t)TASK_DEVICE_OUT_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            mArsiTaskConfig.stream_downlink.device = (uint16_t)TASK_DEVICE_OUT_HEADPHONE;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            mArsiTaskConfig.stream_downlink.device = (uint16_t)TASK_DEVICE_OUT_BLUETOOTH;
            break;
        default:
            ALOGW("%s(), device 0x%x not support, use receiver", __FUNCTION__, output_device);
            mArsiTaskConfig.stream_downlink.device = (uint16_t)TASK_DEVICE_OUT_RECEIVER;
    }
    ASSERT(mArsiTaskConfig.stream_downlink.device != (uint16_t)TASK_DEVICE_OUT_NONE);

    /* set task config to SCP */
    ALOGD("arsi device: in 0x%x, out 0x%x",
          mArsiTaskConfig.stream_uplink.device,
          mArsiTaskConfig.stream_downlink.device);
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_TASK_CFG, sizeof(mArsiTaskConfig), 0,
                     (char *)&mArsiTaskConfig);
}


status_t SpeechDriverOpenDSP::SetArsiSpeechParam(const int enh_mode)
{
    lib_status_t ret;
    uint32_t param_buf_size = 0;


    gEnhMode = enh_mode;
    ALOGD("gEnhMode = %d", gEnhMode);


    string_buf_t platform_name;
    platform_name.memory_size = strlen(gPhoneProductName) + 1;
    platform_name.string_size = strlen(gPhoneProductName);
    platform_name.p_string = gPhoneProductName;
    ALOGD("platform_name.p_string = %s", platform_name.p_string);


    string_buf_t file_path;
    file_path.memory_size = strlen(gEnhParamFilePath) + 1;
    file_path.string_size = strlen(gEnhParamFilePath);
    file_path.p_string = gEnhParamFilePath;
    ALOGD("file_path.p_string = %s", file_path.p_string);

    if (fvsoft_arsi_query_param_buf_size == NULL)
    {
        ALOGE("%s fvsoft_arsi_query_param_buf_size == NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    ret = fvsoft_arsi_query_param_buf_size(&mArsiTaskConfig,
                                           &platform_name,
                                           &file_path,
                                           gEnhMode,
                                           &param_buf_size);
    ALOGD("param_buf_size = %u\n", param_buf_size);
    if (ret != LIB_OK)
    {
        ALOGW("fvsoft_arsi_query_param_buf_size fail");
        return UNKNOWN_ERROR;
    }



    if (fvsoft_arsi_parsing_param_file == NULL)
    {
        ALOGE("%s fvsoft_arsi_parsing_param_file == NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    memset(mParamBuf.p_buffer, 0, mParamBuf.memory_size);
    ret = fvsoft_arsi_parsing_param_file(&mArsiTaskConfig,
                                         &platform_name,
                                         &file_path,
                                         gEnhMode,
                                         &mParamBuf);
    ALOGD("mParamBuf.data_size = %u\n", mParamBuf.data_size);
    if (ret != LIB_OK)
    {
        ALOGW("fvsoft_arsi_parsing_param_file fail");
        return UNKNOWN_ERROR;
    }


    /* set speech param to SCP */
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_SPH_PARAM, mParamBuf.data_size, 0,
                     (char *)mParamBuf.p_buffer);

    return NO_ERROR;
}


void SpeechDriverOpenDSP::createDumpFolder()
{
    if (gEnableDump == true)
    {
        int ret = AudiocheckAndCreateDirectory(DUMP_DSP_PCM_DATA_PATH);
        if (ret < 0)
        {
            ALOGE("AudiocheckAndCreateDirectory(%s) fail!", DUMP_DSP_PCM_DATA_PATH);
            gEnableDump = 0;
            pIPI->configDumpPcmEnable(false);
            pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                             AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                             IPI_MSG_A2D_PCM_DUMP_ON, false, 0,
                             NULL);
        }
    }
}


status_t SpeechDriverOpenDSP::ScpSpeechOn()
{
    ALOGD("%s(+)", __FUNCTION__);

    createDumpFolder();

    pIPI->registerScpFeature(true);
    pIPI->setSpmApMdSrcReq(true);
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_SPH_ON, true, mModemIndex,
                     NULL);
    ipi_msg_t ipi_msg;
    if (pIPI->recvIpiMsg(&ipi_msg) != NO_ERROR) // TODO: ack mechanism
    {
        ALOGW("%s(), recvIpiMsg != NO_ERROR", __FUNCTION__);
    }
    else
    {
        usleep(10 * 1000); // sleep 10 ms
        ASSERT(ipi_msg.msg_id == IPI_MSG_A2D_SPH_ON &&
               ipi_msg.param1 == true);
    }

    ALOGD("%s(-)", __FUNCTION__);
    return NO_ERROR;
}


status_t SpeechDriverOpenDSP::ScpSpeechOff()
{
    ALOGD("%s(+)", __FUNCTION__);

    static const char PROPERTY_KEY_MODEM_STATUS[NUM_MODEM][PROPERTY_KEY_MAX] =
    {
        "af.modem_1.status", "af.modem_2.status", "af.modem_ext.status"
    };

    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_MODEM_STATUS[mModemIndex], property_value, "0");

    for (int i = 0; i < 50; i++)
    {
        if (property_value[0] == '0')
        {
            break;
        }
        else
        {
            ALOGW("%s(), sleep 10ms, i = %d", __FUNCTION__, i);
            usleep(10000); // sleep 10 ms
        }
    }

    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_SPH_ON, false, mModemIndex,
                     NULL);
    ipi_msg_t ipi_msg;
    if (pIPI->recvIpiMsg(&ipi_msg) != NO_ERROR) // TODO: ack mechanism
    {
        ALOGW("%s(), recvIpiMsg != NO_ERROR", __FUNCTION__);
    }
    else
    {
        usleep(100 * 1000); // sleep 100 ms
        ASSERT(ipi_msg.msg_id == IPI_MSG_A2D_SPH_ON &&
               ipi_msg.param1 == false);
    }

    pIPI->setSpmApMdSrcReq(false);
    pIPI->registerScpFeature(false);

    ALOGD("%s(-)", __FUNCTION__);
    return NO_ERROR;
}


status_t SpeechDriverOpenDSP::SetSpeechMode(const audio_devices_t input_device,
                                            const audio_devices_t output_device)
{
    ALOGD("%s(+)", __FUNCTION__);

    SetArsiTaskConfigByDevice(input_device, output_device);
    SetArsiSpeechParam(gEnhMode);

    usleep(5000); ////wait to all data is played

    pSpeechDriverInternal->SetSpeechMode(input_device, output_device);
    ALOGD("%s(-)", __FUNCTION__);

    return NO_ERROR;
}


status_t SpeechDriverOpenDSP::setMDVolumeIndex(int stream, int device,
                                               int index)
{
    return pSpeechDriverInternal->setMDVolumeIndex(stream, device, index);
}


status_t SpeechDriverOpenDSP::SpeechOn()
{
    ALOGD("%s(), mModemIndex = %d", __FUNCTION__, mModemIndex);

    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(SPEECH_STATUS_MASK);
    SetScpSideSpeechStatus(SPEECH_STATUS_MASK);

    ScpSpeechOn();
    pSpeechDriverInternal->SpeechOn();

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SpeechOff()
{
    ALOGD("%s(), mModemIndex = %d", __FUNCTION__, mModemIndex);

    ResetScpSideSpeechStatus(SPEECH_STATUS_MASK);
    ResetApSideModemStatus(SPEECH_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    // Clean gain value and mute status
    CleanGainValueAndMuteStatus();

    pSpeechDriverInternal->SpeechOff();
    ScpSpeechOff();

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::VideoTelephonyOn()
{
    ALOGD("%s()", __FUNCTION__);
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(VT_STATUS_MASK);
    SetScpSideSpeechStatus(VT_STATUS_MASK);

    ScpSpeechOn();
    pSpeechDriverInternal->VideoTelephonyOn();
    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::VideoTelephonyOff()
{
    ALOGD("%s()", __FUNCTION__);
    ResetScpSideSpeechStatus(VT_STATUS_MASK);
    ResetApSideModemStatus(VT_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    // Clean gain value and mute status
    CleanGainValueAndMuteStatus();

    pSpeechDriverInternal->VideoTelephonyOff();
    ScpSpeechOff();

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SpeechRouterOn()
{
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::SpeechRouterOff()
{
    return INVALID_OPERATION;
}


/*==============================================================================
 *                     Recording Control
 *============================================================================*/

status_t SpeechDriverOpenDSP::RecordOn()
{
    return pSpeechDriverInternal->RecordOn();
}

status_t SpeechDriverOpenDSP::RecordOn(record_type_t type_record)
{
    return pSpeechDriverInternal->RecordOn(type_record);
}

status_t SpeechDriverOpenDSP::RecordOff()
{
    return pSpeechDriverInternal->RecordOff();
}

status_t SpeechDriverOpenDSP::RecordOff(record_type_t type_record)
{
    return pSpeechDriverInternal->RecordOff(type_record);
}

status_t SpeechDriverOpenDSP::SetPcmRecordType(record_type_t type_record)
{
    return pSpeechDriverInternal->SetPcmRecordType(type_record);
}

status_t SpeechDriverOpenDSP::VoiceMemoRecordOn()
{
    ALOGD("%s()", __FUNCTION__);
    SetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return pSpeechDriverInternal->VoiceMemoRecordOn();
}

status_t SpeechDriverOpenDSP::VoiceMemoRecordOff()
{
    ALOGD("%s()", __FUNCTION__);
    ResetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return pSpeechDriverInternal->VoiceMemoRecordOff();
}

uint16_t SpeechDriverOpenDSP::GetRecordSampleRate() const
{
    return pSpeechDriverInternal->GetRecordSampleRate();
}

uint16_t SpeechDriverOpenDSP::GetRecordChannelNumber() const
{
    return pSpeechDriverInternal->GetRecordChannelNumber();
}


/*==============================================================================
 *                     Background Sound
 *============================================================================*/

status_t SpeechDriverOpenDSP::BGSoundOn()
{
    return pSpeechDriverInternal->BGSoundOn();
}

status_t SpeechDriverOpenDSP::BGSoundConfig(uint8_t ul_gain, uint8_t dl_gain)
{
    return pSpeechDriverInternal->BGSoundConfig(ul_gain, dl_gain);
}

status_t SpeechDriverOpenDSP::BGSoundOff()
{
    return pSpeechDriverInternal->BGSoundOff();
}

/*==============================================================================
*                     PCM 2 Way
*============================================================================*/
status_t SpeechDriverOpenDSP::PCM2WayPlayOn()
{
    return pSpeechDriverInternal->PCM2WayPlayOn();
}


status_t SpeechDriverOpenDSP::PCM2WayPlayOff()
{
    return pSpeechDriverInternal->PCM2WayPlayOff();
}


status_t SpeechDriverOpenDSP::PCM2WayRecordOn()
{
    return pSpeechDriverInternal->PCM2WayRecordOn();
}


status_t SpeechDriverOpenDSP::PCM2WayRecordOff()
{
    return pSpeechDriverInternal->PCM2WayRecordOff();
}


status_t SpeechDriverOpenDSP::PCM2WayOn(const bool wideband_on)
{
    return pSpeechDriverInternal->PCM2WayOn(wideband_on);
}


status_t SpeechDriverOpenDSP::PCM2WayOff()
{
    return pSpeechDriverInternal->PCM2WayOff();
}


status_t SpeechDriverOpenDSP::DualMicPCM2WayOn(const bool wideband_on,
                                               const bool record_only)
{
    return pSpeechDriverInternal->DualMicPCM2WayOn(wideband_on, record_only);
}


status_t SpeechDriverOpenDSP::DualMicPCM2WayOff()
{
    return pSpeechDriverInternal->DualMicPCM2WayOff();
}


/*==============================================================================
 *                     TTY-CTM Control
 *============================================================================*/
status_t SpeechDriverOpenDSP::TtyCtmOn(tty_mode_t ttyMode)
{
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_TTY_ON, true, ttyMode,
                     NULL);

    return pSpeechDriverInternal->TtyCtmOn(ttyMode);
}

status_t SpeechDriverOpenDSP::TtyCtmOff()
{
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_TTY_ON, false, AUD_TTY_OFF,
                     NULL);

    return pSpeechDriverInternal->TtyCtmOff();
}

status_t SpeechDriverOpenDSP::TtyCtmDebugOn(bool tty_debug_flag)
{
    return pSpeechDriverInternal->TtyCtmDebugOn(tty_debug_flag);
}

/*==============================================================================
 *                     Acoustic Loopback
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetAcousticLoopback(bool loopback_on)
{
    ALOGD("%s(), loopback_on = %d", __FUNCTION__, loopback_on);

    if (loopback_on == true)
    {
        CheckApSideModemStatusAllOffOrDie();
        SetApSideModemStatus(LOOPBACK_STATUS_MASK);
        SetScpSideSpeechStatus(LOOPBACK_STATUS_MASK);

        ScpSpeechOn();
        pSpeechDriverInternal->SetAcousticLoopback(loopback_on);
    }
    else
    {
        ResetScpSideSpeechStatus(LOOPBACK_STATUS_MASK);
        ResetApSideModemStatus(LOOPBACK_STATUS_MASK);
        CheckApSideModemStatusAllOffOrDie();

        // Clean gain value and mute status
        CleanGainValueAndMuteStatus();

        pSpeechDriverInternal->SetAcousticLoopback(loopback_on);
        ScpSpeechOff();
    }

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SetAcousticLoopbackBtCodec(bool enable_codec)
{
    return pSpeechDriverInternal->SetAcousticLoopbackBtCodec(enable_codec);
}

status_t SpeechDriverOpenDSP::SetAcousticLoopbackDelayFrames(
    int32_t delay_frames)
{
    return pSpeechDriverInternal->SetAcousticLoopbackDelayFrames(delay_frames);
}

/*==============================================================================
 *                     Volume Control
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetDownlinkGain(int16_t gain)
{
    ALOGD("%s(), gain = 0x%x, old mDownlinkGain = 0x%x",
          __FUNCTION__, gain, mDownlinkGain);
    if (gain == mDownlinkGain) { return NO_ERROR; }

    mDownlinkGain = gain;
    return pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                            IPI_MSG_A2D_DL_GAIN, gain, 0/*TODO*/,
                            NULL);
}

status_t SpeechDriverOpenDSP::SetEnh1DownlinkGain(int16_t gain)
{
    return INVALID_OPERATION; // not support anymore
}

status_t SpeechDriverOpenDSP::SetUplinkGain(int16_t gain)
{
    ALOGD("%s(), gain = 0x%x, old mUplinkGain = 0x%x",
          __FUNCTION__, gain, mUplinkGain);
    if (gain == mUplinkGain) { return NO_ERROR; }

    mUplinkGain = gain;
    return pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                            IPI_MSG_A2D_UL_GAIN, gain, 0/*TODO*/,
                            NULL);
}

status_t SpeechDriverOpenDSP::SetDownlinkMute(bool mute_on)
{
    ALOGD("%s(), mute_on = %d, old mDownlinkMuteOn = %d",
          __FUNCTION__, mute_on, mDownlinkMuteOn);
    if (mute_on == mDownlinkMuteOn) { return NO_ERROR; }

    mDownlinkMuteOn = mute_on;

    // mute voice dl + bgs
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_DL_MUTE_ON, mute_on, 0,
                     NULL);
    return pSpeechDriverInternal->SetDownlinkMute(mute_on); // for bgs
}

status_t SpeechDriverOpenDSP::SetUplinkMute(bool mute_on)
{
    ALOGD("%s(), mute_on = %d, old mUplinkMuteOn = %d",
          __FUNCTION__, mute_on, mUplinkMuteOn);
    if (mute_on == mUplinkMuteOn) { return NO_ERROR; }

    mUplinkMuteOn = mute_on;

    // mute voice ul + bgs
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_UL_MUTE_ON, mute_on, 0,
                     NULL);
    return pSpeechDriverInternal->SetUplinkMute(mute_on); // for bgs
}

status_t SpeechDriverOpenDSP::SetUplinkSourceMute(bool mute_on)
{
    ALOGD("%s(), mute_on = %d, old mUplinkSourceMuteOn = %d",
          __FUNCTION__, mute_on, mUplinkSourceMuteOn);
    if (mute_on == mUplinkSourceMuteOn) { return NO_ERROR; }

    mUplinkSourceMuteOn = mute_on;
    return pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                            IPI_MSG_A2D_UL_MUTE_ON, mute_on, 0,
                            NULL);
}

status_t SpeechDriverOpenDSP::SetSidetoneGain(int16_t gain)
{
    ALOGD("%s(), gain = 0x%x, old mSideToneGain = 0x%x",
          __FUNCTION__, gain, mSideToneGain);
    if (gain == mSideToneGain) { return NO_ERROR; }

    mSideToneGain = gain;
    return pSpeechDriverInternal->SetSidetoneGain(gain);
}


/*==============================================================================
 *                     Device related Config
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetModemSideSamplingRate(uint16_t sample_rate)
{
    return pSpeechDriverInternal->SetModemSideSamplingRate(sample_rate);
}


/*==============================================================================
 *                     Speech Enhancement Control
 *============================================================================*/
status_t SpeechDriverOpenDSP::SetSpeechEnhancement(bool enhance_on)
{
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_UL_ENHANCE_ON, enhance_on, 0,
                     NULL);
    pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                     AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                     IPI_MSG_A2D_DL_ENHANCE_ON, enhance_on, 0,
                     NULL);
    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SetSpeechEnhancementMask(const
                                                       sph_enh_mask_struct_t &mask)
{
    return INVALID_OPERATION; // not support anymore
}

status_t SpeechDriverOpenDSP::SetBtHeadsetNrecOn(const bool bt_headset_nrec_on)
{
    return pIPI->sendIpiMsg(TASK_SCENE_PHONE_CALL,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                            IPI_MSG_A2D_BT_NREC_ON, bt_headset_nrec_on, 0,
                            NULL);
}


/*==============================================================================
 *                     Speech Enhancement Parameters
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetNBSpeechParameters(const AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB)
{
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::SetDualMicSpeechParameters(
    const AUDIO_CUSTOM_EXTRA_PARAM_STRUCT *pSphParamDualMic)
{
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::SetMagiConSpeechParameters(
    const AUDIO_CUSTOM_MAGI_CONFERENCE_STRUCT *pSphParamMagiCon)
{
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::SetHACSpeechParameters(const AUDIO_CUSTOM_HAC_PARAM_STRUCT *pSphParamHAC)
{
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::SetWBSpeechParameters(const AUDIO_CUSTOM_WB_PARAM_STRUCT *pSphParamWB)
{
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::GetVibSpkParam(void *eVibSpkParam)
{
    return pSpeechDriverInternal->GetVibSpkParam(eVibSpkParam);
}

status_t SpeechDriverOpenDSP::SetVibSpkParam(void *eVibSpkParam)
{
    return pSpeechDriverInternal->SetVibSpkParam(eVibSpkParam);
}


/*==============================================================================
 *                     Recover State
 *============================================================================*/

bool SpeechDriverOpenDSP::GetScpSideSpeechStatus(const modem_status_mask_t modem_status_mask)
{
    AudioAutoTimeoutLock _l(mScpSideSpeechStatusLock);
    return ((mScpSideSpeechStatus & modem_status_mask) > 0);
}


void SpeechDriverOpenDSP::SetScpSideSpeechStatus(const modem_status_mask_t modem_status_mask)
{
    ALOGD("%s(), modem_status_mask = 0x%x, mScpSideSpeechStatus = 0x%x",
          __FUNCTION__, modem_status_mask, mScpSideSpeechStatus);

    AudioAutoTimeoutLock _l(mScpSideSpeechStatusLock);

    ASSERT(((mScpSideSpeechStatus & modem_status_mask) > 0) == false);
    mScpSideSpeechStatus |= modem_status_mask;

    char property_value[PROPERTY_VALUE_MAX];
    sprintf(property_value, "%u", mScpSideSpeechStatus);
    property_set(PROPERTY_KEY_SCP_CALL_STATE[mModemIndex], property_value);
}


void SpeechDriverOpenDSP::ResetScpSideSpeechStatus(const modem_status_mask_t modem_status_mask)
{
    ALOGD("%s(), modem_status_mask = 0x%x, mScpSideSpeechStatus = 0x%x",
          __FUNCTION__, modem_status_mask, mScpSideSpeechStatus);

    AudioAutoTimeoutLock _l(mScpSideSpeechStatusLock);

    ASSERT(((mScpSideSpeechStatus & modem_status_mask) > 0) == true);
    mScpSideSpeechStatus &= (~modem_status_mask);

    char property_value[PROPERTY_VALUE_MAX];
    sprintf(property_value, "%u", mScpSideSpeechStatus);
    property_set(PROPERTY_KEY_SCP_CALL_STATE[mModemIndex], property_value);
}


void SpeechDriverOpenDSP::RecoverModemSideStatusToInitState()
{
    // Phone Call / Loopback
    if (GetScpSideSpeechStatus(SPEECH_STATUS_MASK) == true)
    {
        ALOGD("%s(), modem_index = %d, speech_on = true", __FUNCTION__, mModemIndex);
        ResetScpSideSpeechStatus(SPEECH_STATUS_MASK);
        ScpSpeechOff();
    }
    else if (GetScpSideSpeechStatus(VT_STATUS_MASK) == true)
    {
        ALOGD("%s(), modem_index = %d, vt_on = true", __FUNCTION__, mModemIndex);
        ResetScpSideSpeechStatus(VT_STATUS_MASK);
        ScpSpeechOff();
    }
    else if (GetScpSideSpeechStatus(LOOPBACK_STATUS_MASK) == true)
    {
        ALOGD("%s(), modem_index = %d, loopback_on = true", __FUNCTION__, mModemIndex);
        ResetScpSideSpeechStatus(LOOPBACK_STATUS_MASK);
        ScpSpeechOff();
    }
}


/*==============================================================================
 *                     Check Modem Status
 *============================================================================*/
bool SpeechDriverOpenDSP::CheckModemIsReady()
{
    // TODO: [OpenDSP] scp ready
    return pSpeechDriverInternal->CheckModemIsReady();
}


/*==============================================================================
 *                     Debug Info
 *============================================================================*/
status_t SpeechDriverOpenDSP::ModemDumpSpeechParam()
{
    return INVALID_OPERATION;
}


/*==============================================================================
 *                     Speech Enhancement Parameters
 *============================================================================*/
status_t SpeechDriverOpenDSP::SetDynamicSpeechParameters(const int type,
                                                         const void *param_arg)
{
    return pSpeechDriverInternal->SetDynamicSpeechParameters(type, param_arg);
}


} // end of namespace android

