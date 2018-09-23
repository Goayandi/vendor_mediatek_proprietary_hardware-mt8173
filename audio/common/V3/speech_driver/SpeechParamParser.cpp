#include "SpeechParamParser.h"
#include <utils/Log.h>
#include "AudioUtility.h"//Mutex/assert
#include "AudioALSAStreamManager.h"
#include <inttypes.h>

#define LOG_TAG "SpeechParamParser"

namespace android
{
#define XML_FOLDER "/system/etc/audio_param/"

#define MAX_BYTE_PARAM_SPEECH 3434

//--------------------------------------------------------------------------------
//audio type: Speech
#define MAX_NUM_CATEGORY_TYPE_SPEECH 4
#define MAX_NUM_PARAM_SPEECH 3
#define NUM_VOLUME_SPEECH 7
const String8 audioType_Speech_CategoryType[ ] = {String8("Band"), String8("Profile"), String8("VolIndex"), String8("Network")};
const char audioType_Speech_CategoryName2[SPEECH_PROFILE_MAX_NUM][128] = {"Normal", "4_pole_Headset", "Handsfree", "BT_Earphone", "BT_NREC_Off", "MagiConference", "HAC", "Lpbk_Handset", "Lpbk_Headset", "Lpbk_Handsfree", "3_pole_Headset", "5_pole_Headset", "5_pole_Headset+ANC", "Tty_Handset", "Tty_Handsfree"};
const char audioType_Speech_CategoryName3[NUM_VOLUME_SPEECH][128] = {"0", "1", "2", "3", "4", "5", "6"};
const String8 audioType_Speech_ParamName[ ] = {String8("speech_mode_para"), String8("sph_in_fir"), String8("sph_out_fir")};

//--------------------------------------------------------------------------------
//audio type: SpeechDMNR
#define MAX_NUM_CATEGORY_TYPE_SPEECH_DMNR 2
#define MAX_NUM_PARAM_SPEECH_DMNR 1
const String8 audioType_SpeechDMNR_CategoryType[ ] = {String8("Band"), String8("Profile")};
const char audioType_SpeechDMNR_CategoryName1[2][128] = {"NB", "WB"};
const char audioType_SpeechDMNR_CategoryName2[2][128] = {"Handset", "MagiConference"};
const String8 audioType_SpeechDMNR_ParamName[ ] = {String8("dmnr_para")};
const char audioType_SpeechDMNR_NumPerCategory[2] = {2, 2};

//--------------------------------------------------------------------------------
//audio type: SpeechGeneral
#define MAX_NUM_CATEGORY_TYPE_SPEECH_GENERAL 1
#define MAX_NUM_PARAM_SPEECH_GENERAL 2
const String8 audioType_SpeechGeneral_CategoryType[ ] = {String8("CategoryLayer")};
const char audioType_SpeechGeneral_CategoryName1[1][128] = {"Common"};
const String8 audioType_SpeechGeneral_ParamName[ ] = {String8("speech_common_para"), String8("debug_info")};

//--------------------------------------------------------------------------------
//audio type: SpeechMagiClarity
#define MAX_NUM_CATEGORY_TYPE_SPEECH_MAGICLARITY 1
#define MAX_NUM_PARAM_SPEECH_MAGICLARITY 1
const String8 audioType_SpeechMagiClarity_CategoryType[ ] = {String8("CategoryLayer")};
const char audioType_SpeechMagiClarity_CategoryName1[1][128] = {"Common"};
const String8 audioType_SpeechMagiClarity_ParamName[ ] = {String8("shape_rx_fir_para")};

//--------------------------------------------------------------------------------
//audio type: SpeechNetwork
#define MAX_NUM_CATEGORY_TYPE_SPEECH_NETWORK 1
#define MAX_NUM_PARAM_SPEECH_NETWORK 1
const String8 audioType_SpeechNetwork_CategoryType[ ] = {String8("Network")};
const String8 audioType_SpeechNetwork_ParamName[ ] = {String8("speech_network_support")};


//--------------------------------------------------------------------------------
const unsigned short nb_speech_mode_para_table[6][48] =
{
    {96, 253, 16388, 31, 57351, 799, 405, 64, 80, 4325, 611, 0, 20488, 0, 0, 8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 189, 10756, 31, 57351, 31, 405, 64, 80, 4325, 611, 0, 20488, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {96, 224, 5256, 31, 57351, 24607, 405, 132, 84, 4325, 611, 0, 20488, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 253, 10756, 31, 53263, 31, 405, 0, 80, 4325, 41571, 0, 53256, 0, 0, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {96, 224, 5256, 31, 57351, 24607, 405, 132, 84, 4325, 611, 0, 8200, 883, 23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {96, 253, 16388, 31, 57351, 799, 405, 64, 80, 4325, 611, 0, 20488, 0, 0, 8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const unsigned short wb_speech_mode_para_table[6][48] =
{
    {96, 253, 16388, 31, 57607, 799, 405, 64, 80, 4325, 611, 0, 16392, 0, 0, 8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 189, 10756, 31, 57607, 31, 405, 64, 80, 4325, 611, 0, 16392, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {96, 224, 5256, 31, 57607, 24607, 405, 132, 84, 4325, 611, 0, 16392, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 253, 10756, 31, 53519, 31, 405, 0, 80, 4325, 41571, 0, 49160, 0, 0, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {96, 224, 5256, 31, 57607, 24607, 405, 132, 84, 4325, 611, 0, 8200, 883, 23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {96, 253, 16388, 31, 57607, 799, 405, 64, 80, 4325, 611, 0, 16392, 0, 0, 8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

};

const short nb_sph_in_fir_table[6][45] = {{32767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

};

const short nb_sph_out_fir_table[6][45] = {{32767, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

};

const short wb_sph_in_fir_table[6][90] = {{32767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};


const short wb_sph_out_fir_table[6][90] = {{32767, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {32767, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/*==============================================================================
 *                     Singleton Pattern
 *============================================================================*/

SpeechParamParser *SpeechParamParser::UniqueSpeechParamParser = NULL;


SpeechParamParser *SpeechParamParser::getInstance()
{
    static Mutex mGetInstanceLock;
    Mutex::Autolock _l(mGetInstanceLock);
    ALOGV("%s()", __FUNCTION__);

    if (UniqueSpeechParamParser == NULL)
    {
        UniqueSpeechParamParser = new SpeechParamParser();
    }
    ASSERT(UniqueSpeechParamParser != NULL);
    return UniqueSpeechParamParser;
}
/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

SpeechParamParser::SpeechParamParser()
{
    ALOGD("%s()", __FUNCTION__);
    mSphParamInfo.SpeechMode = SPEECH_MODE_NORMAL;
    mSphParamInfo.u4VolumeIndex = 3;
    mSphParamInfo.bBtHeadsetNrecOn = false;
    mSphParamInfo.bLPBK = false;
    mSphParamInfo.uHeadsetPole = 4;
    mSphParamInfo.bTTY = false;
    sphNetworkSupport = false;
    sphTTYSupport = false;
    mSpeechParamVerFirst = 0;
    mSpeechParamVerLast = 0;

    Init();
}

SpeechParamParser::~SpeechParamParser()
{
    ALOGD("%s()", __FUNCTION__);

}

void SpeechParamParser::Init()
{
    ALOGD("%s()", __FUNCTION__);
    InitAppParser();
    InitSpeechNetwork();

    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL)
    {
        ALOGE("Error %s %d", __FUNCTION__, __LINE__);
        ASSERT(0);
    }

    const char *strSphVersion = appOps->appHandleGetFeatureOptionValue(mAppHandle, "SPH_PARAM_VERSION");
    sscanf(strSphVersion, "%" SCNd8 ".%" SCNd8, &mSpeechParamVerFirst, &mSpeechParamVerLast);

    switch (mSpeechParamVerFirst)
    {
        case 1:
            sphNetworkSupport = true;
            break;
        default:
            sphNetworkSupport = false;
            break;

    }
    const char *strSphTTY = appOps->appHandleGetFeatureOptionValue(mAppHandle, "SPH_PARAM_TTY");

    if (strcmp(strSphTTY, "yes") == 0)
    {
        sphTTYSupport = true;
    }
    else
    {

        sphTTYSupport = false;
    }
}

void SpeechParamParser::Deinit()
{
    ALOGD("%s()", __FUNCTION__);
}


/*==============================================================================
 *                     SpeechParamParser Imeplementation
 *============================================================================*/
bool SpeechParamParser::GetSpeechParamSupport(void)
{
    bool mSupport;

#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
    mSupport = true;
#else
    mSupport = false;
#endif
    ALOGD("%s(), GetSpeechParamSupport:%d", __FUNCTION__, mSupport);
    return mSupport;
}


void SpeechParamParser::InitAppParser()
{
    ALOGD("+%s()", __FUNCTION__);
#if defined(APP_PARSER_SUPPORT)
    /* Init AppHandle */
    ALOGD("%s() appHandleGetInstance", __FUNCTION__);
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL)
    {
        ALOGE("Error %s %d", __FUNCTION__, __LINE__);
        ASSERT(0);
        return;
    }

    mAppHandle = appOps->appHandleGetInstance();
    ALOGD("%s() appHandleRegXmlChangedCb", __FUNCTION__);

#endif
}

status_t SpeechParamParser::SpeechDataDump(uint16_t uSpeechTypeIndex, const char *nameParam, const char *SpeechParamData)
{
    ALOGV("+%s(), uSpeechTypeIndex=%d", __FUNCTION__, uSpeechTypeIndex);
    char SphDumpStr[500];
    int u4I = 0, idxDump = 0, DataTypePrint = 0;
    //speech parameter dump

    switch (uSpeechTypeIndex)
    {
        case AUDIO_TYPE_SPEECH:
        {
            if (strcmp(nameParam, "speech_mode_para") == 0)
            {
                idxDump = 16;
            }
            else if (strcmp(nameParam, "sph_in_fir") == 0)
            {
                idxDump = 5;
            }
            else if (strcmp(nameParam, "sph_out_fir") == 0)
            {
                idxDump = 5;
            }
            break;
        }
        case AUDIO_TYPE_SPEECH_GENERAL:
        {
            if (strcmp(nameParam, "speech_common_para") == 0)
            {
                idxDump = 12;
            }
            else if (strcmp(nameParam, "debug_info") == 0)
            {
                idxDump = 8;
            }
            break;
        }
        case AUDIO_TYPE_SPEECH_NETWORK:
        {
            if (strcmp(nameParam, "speech_network_support") == 0)
            {
                idxDump = 1;
            }
            break;
        }

    }
    if(idxDump != 0)
    ALOGV("%s(), idxDump=%d", __FUNCTION__, idxDump);
    sprintf(SphDumpStr, "%s[%d]= ", nameParam, idxDump);
    for (u4I = 0; u4I < idxDump; u4I++)
    {
        char SphDumpTemp[100];
        if (DataTypePrint == 1)
        {
            sprintf(SphDumpTemp, "[%d]0x%x, ", u4I, *((uint16_t *)SpeechParamData + u4I));
        }
        else
        {
            sprintf(SphDumpTemp, "[%d]%d, ", u4I, *((uint16_t *)SpeechParamData + u4I));
        }
        strcat(SphDumpStr, SphDumpTemp);


        // ALOGD("%s() SpeechParam[%d]=%d", __FUNCTION__, u4I, *((uint16_t *)SpeechParamData + u4I));
    }

    if (u4I != 0)
    {
        ALOGD("%s(), %s, %s", __FUNCTION__, audioTypeNameList[uSpeechTypeIndex], SphDumpStr);
    }
    return NO_ERROR;
}


status_t SpeechParamParser::GetSpeechParamFromAppParser(uint16_t uSpeechTypeIndex, AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT *mParamLayerInfo, char *pPackedParamUnit, uint16_t *sizeByteTotal)
{
    ALOGV("+%s(), mParamLayerInfo->numCategoryType=0x%x", __FUNCTION__, mParamLayerInfo->numCategoryType);
#if defined(APP_PARSER_SUPPORT)

    if (mAppHandle == NULL)
    {
        ALOGE("%s() mAppHandle == NULL, Assert!!!", __FUNCTION__);
        ASSERT(0);
        return UNKNOWN_ERROR;
    }

    char *categoryPath = NULL;
    UT_string *uts_categoryPath = NULL;
    ParamUnit *paramUnit = NULL;
    uint16_t  sizeByteParam = 0, u4Index;
    Param  *SpeechParam;

    /* If user select a category path, just like "NarrowBand / Normal of Handset / Level0" */
    if (uts_categoryPath == NULL)
    {
        utstring_new(uts_categoryPath);
    }

    if (uts_categoryPath)
    {

        ALOGV("%s(), categoryType.size=%d, paramName.size=%d", __FUNCTION__, mParamLayerInfo->categoryType.size(), mParamLayerInfo->paramName.size());
        for (u4Index = 0; u4Index < mParamLayerInfo->categoryType.size() ; u4Index++)
        {
            ALOGV("%s(), categoryType[%d]= %s", __FUNCTION__, u4Index, mParamLayerInfo->categoryType.at(u4Index).string());
        }
        for (u4Index = 0; u4Index < mParamLayerInfo->categoryName.size() ; u4Index++)
        {
            ALOGV("%s(), categoryName[%d]= %s", __FUNCTION__, u4Index, mParamLayerInfo->categoryName.at(u4Index).string());
        }


        for (u4Index = 0; u4Index < mParamLayerInfo->numCategoryType ; u4Index++)
        {
            if (u4Index == mParamLayerInfo->numCategoryType - 1)
            {
                //last time concat
                utstring_printf(uts_categoryPath, "%s,%s", (char *)(mParamLayerInfo->categoryType.at(u4Index).string()), (char *)(mParamLayerInfo->categoryName.at(u4Index).string()));
            }
            else
            {
                utstring_printf(uts_categoryPath, "%s,%s,", (char *)(mParamLayerInfo->categoryType.at(u4Index).string()), (char *)(mParamLayerInfo->categoryName.at(u4Index).string()));
            }
        }
        categoryPath = strdup(utstring_body(uts_categoryPath));
        utstring_free(uts_categoryPath);
    }

    ALOGV("%s() audioTypeName=%s", __FUNCTION__, mParamLayerInfo->audioTypeName);
    /* Query AudioType */
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL)
    {
        ALOGE("Error %s %d", __FUNCTION__, __LINE__);
        ASSERT(0);
        return UNKNOWN_ERROR;
    }

    AudioType *audioType = appOps->appHandleGetAudioTypeByName(mAppHandle, mParamLayerInfo->audioTypeName);

    if (!audioType)
    {
        free(categoryPath);
        ALOGE("%s() can't find audioTypeName=%s, Assert!!!", __FUNCTION__, mParamLayerInfo->audioTypeName);
        ASSERT(0);
        return UNKNOWN_ERROR;
    }
    ALOGV("%s() audioType=%s", __FUNCTION__, audioType->name);

    /* Query the ParamUnit */
    appOps->audioTypeReadLock(audioType, __FUNCTION__);
    paramUnit = appOps->audioTypeGetParamUnit(audioType, categoryPath);
    if (!paramUnit)
    {
        free(categoryPath);
        appOps->audioTypeUnlock(audioType);
        ALOGE("%s() can't find paramUnit, Assert!!!", __FUNCTION__);
        ASSERT(0);
        return UNKNOWN_ERROR;
    }

    for (u4Index = 0; u4Index < (*mParamLayerInfo).numParam ; u4Index++)
    {

        SpeechParam = appOps->paramUnitGetParamByName(paramUnit, (const char *)mParamLayerInfo->paramName.at(u4Index).string());
        if (SpeechParam)
        {
            sizeByteParam = sizeByteParaData((DATA_TYPE)SpeechParam->paramInfo->dataType, SpeechParam->arraySize);
            memcpy(pPackedParamUnit + *sizeByteTotal, SpeechParam->data, sizeByteParam);
            *sizeByteTotal += sizeByteParam;
            ALOGV("%s() paramName=%s, sizeByteParam=%d", __FUNCTION__, mParamLayerInfo->paramName.at(u4Index).string(), sizeByteParam);
            //speech parameter dump
            SpeechDataDump(uSpeechTypeIndex, (const char *)mParamLayerInfo->paramName.at(u4Index).string(), (const char *)SpeechParam->data);
        }
    }

    appOps->audioTypeUnlock(audioType);
    free(categoryPath);
#endif
    return NO_ERROR;
}

uint16_t SpeechParamParser::sizeByteParaData(DATA_TYPE dataType, uint16_t arraySize)
{
    uint16_t sizeUnit = 4;
    switch (dataType)
    {
        case TYPE_INT:
            sizeUnit = 4;
            break;
        case TYPE_UINT:
            sizeUnit = 4;
            break;
        case TYPE_FLOAT:
            sizeUnit = 4;
            break;
        case TYPE_BYTE_ARRAY:
            sizeUnit = arraySize;
            break;
        case TYPE_UBYTE_ARRAY:
            sizeUnit = arraySize;
            break;
        case TYPE_SHORT_ARRAY:
            sizeUnit = arraySize << 1;
            break;
        case TYPE_USHORT_ARRAY:
            sizeUnit = arraySize << 1;
            break;
        case TYPE_INT_ARRAY:
            sizeUnit = arraySize << 2;
            break;
        case TYPE_UINT_ARRAY:
            sizeUnit = arraySize << 2;
            break;
        default:
            ALOGE("%s(), Not an available dataType(%d)", __FUNCTION__, dataType);

            break;

    }

    ALOGV("-%s(), arraySize=%d, sizeUnit=%d", __FUNCTION__, arraySize, sizeUnit);

    return sizeUnit;


}


int SpeechParamParser::GetDmnrParamUnit(char *pPackedParamUnit)
{
    ALOGD("+%s()", __FUNCTION__);
    uint16_t size = 0, u4Index = 0, u4Index2 = 0, sizeByteFromApp = 0;
    uint16_t DataHeader;
    SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT eParamUnitHdr;
    memset(&eParamUnitHdr, 0, sizeof(eParamUnitHdr));

    eParamUnitHdr.SphParserVer = 1;
    eParamUnitHdr.NumLayer = 0x2;
    eParamUnitHdr.NumEachLayer = 0x22;
    eParamUnitHdr.ParamHeader[0] = 0x3;//OutputDeviceType
    eParamUnitHdr.ParamHeader[1] = 0x3;//VoiceBand
    eParamUnitHdr.SphUnitMagiNum = 0xAA03;
    ALOGD("%s(), eParamUnitHdr.SphUnitMagiNum= 0x%x", __FUNCTION__, eParamUnitHdr.SphUnitMagiNum);

    memcpy(pPackedParamUnit + size, &eParamUnitHdr, sizeof(eParamUnitHdr));
    size += sizeof(eParamUnitHdr);

    char *pPackedParamUnitFromApp = new char [MAX_BYTE_PARAM_SPEECH];
    memset(pPackedParamUnitFromApp, 0, MAX_BYTE_PARAM_SPEECH);

#ifdef APP_PARSER_SUPPORT
    ALOGD("%s(), APP_PARSER_SUPPORT", __FUNCTION__);
    AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT pParamLayerInfo;

    pParamLayerInfo.audioTypeName = (char *) audioTypeNameList[AUDIO_TYPE_SPEECH_DMNR];
    pParamLayerInfo.numCategoryType = MAX_NUM_CATEGORY_TYPE_SPEECH_DMNR;//4
    pParamLayerInfo.numParam = MAX_NUM_PARAM_SPEECH_DMNR;//4

    pParamLayerInfo.categoryType.assign(audioType_SpeechDMNR_CategoryType, audioType_SpeechDMNR_CategoryType + pParamLayerInfo.numCategoryType);
    pParamLayerInfo.paramName.assign(audioType_SpeechDMNR_ParamName, audioType_SpeechDMNR_ParamName + pParamLayerInfo.numParam);

    ALOGD("%s(), categoryType.size=%d, paramName.size=%d", __FUNCTION__, pParamLayerInfo.categoryType.size(), pParamLayerInfo.paramName.size());
    for (u4Index = 0; u4Index < pParamLayerInfo.paramName.size() ; u4Index++)
    {
        ALOGV("%s(), paramName[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.paramName.at(u4Index).string());
    }


    for (u4Index = 0; u4Index < audioType_SpeechDMNR_NumPerCategory[0] ; u4Index++) //NB, WB
    {
        for (u4Index2 = 0; u4Index2 < audioType_SpeechDMNR_NumPerCategory[1] ; u4Index2++)
        {
            sizeByteFromApp = 0;
            DataHeader = ((u4Index + 1) << 4) + (u4Index2 + 1);
            memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
            size += sizeof(DataHeader);

            pParamLayerInfo.categoryName.push_back(String8(audioType_SpeechDMNR_CategoryName1[u4Index]));
            pParamLayerInfo.categoryName.push_back(String8(audioType_SpeechDMNR_CategoryName2[u4Index2]));

            GetSpeechParamFromAppParser(AUDIO_TYPE_SPEECH_DMNR, &pParamLayerInfo, pPackedParamUnitFromApp, &sizeByteFromApp);

            memcpy(pPackedParamUnit + size, pPackedParamUnitFromApp, sizeByteFromApp);
            size += sizeByteFromApp;
            ALOGD("%s(), DataHeader=0x%x, categoryName[%d,%d]= %s,%s, sizeByteFromApp=%d", __FUNCTION__, DataHeader, u4Index, u4Index2, pParamLayerInfo.categoryName.at(0).string(), pParamLayerInfo.categoryName.at(1).string(), sizeByteFromApp);
            pParamLayerInfo.categoryName.pop_back();
            pParamLayerInfo.categoryName.pop_back();

        }
    }


#else

    unsigned short dmnr_para_nb[44] =
    {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        68, 0, 0, 0
    };


    unsigned short dmnr_para_wb[76] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 68, 0, 0, 0

    };
    DataHeader = 0x0011;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);
    memcpy(pPackedParamUnit + size, &dmnr_para_nb, sizeof(dmnr_para_nb));
    size += sizeof(dmnr_para_nb);
    ALOGD("%s(), After NB size=%d", __FUNCTION__, size);

    DataHeader = 0x0012;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);
    memcpy(pPackedParamUnit + size, &dmnr_para_nb, sizeof(dmnr_para_nb));

    size += sizeof(dmnr_para_nb);
    ALOGD("%s(), After NB size=%d", __FUNCTION__, size);


    DataHeader = 0x0021;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);
    memcpy(pPackedParamUnit + size, &dmnr_para_wb, sizeof(dmnr_para_wb));
    size += sizeof(dmnr_para_wb);
    ALOGD("%s(), After WB size=%d", __FUNCTION__, size);

    DataHeader = 0x0022;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);
    memcpy(pPackedParamUnit + size, &dmnr_para_wb, sizeof(dmnr_para_wb));
    size += sizeof(dmnr_para_wb);

#endif
    if (pPackedParamUnitFromApp != NULL)
    {
        delete[] pPackedParamUnitFromApp;
    }

    ALOGD("-%s(), total size byte=%d", __FUNCTION__, size);
    return size;
}

int SpeechParamParser::GetGeneralParamUnit(char *pPackedParamUnit)
{
    ALOGD("+%s()", __FUNCTION__);
    uint16_t size = 0, u4Index = 0, u4Index2 = 0, sizeByteFromApp = 0;
    uint16_t DataHeader;
    SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT eParamUnitHdr;
    memset(&eParamUnitHdr, 0, sizeof(eParamUnitHdr));

    eParamUnitHdr.SphParserVer = 1;
    eParamUnitHdr.NumLayer = 0x1;
    eParamUnitHdr.NumEachLayer = 0x1;
    eParamUnitHdr.ParamHeader[0] = 0x1;//Common
    eParamUnitHdr.SphUnitMagiNum = 0xAA02;
    ALOGD("%s(), eParamUnitHdr.SphUnitMagiNum= 0x%x", __FUNCTION__, eParamUnitHdr.SphUnitMagiNum);

    memcpy(pPackedParamUnit + size, &eParamUnitHdr, sizeof(eParamUnitHdr));
    size += sizeof(eParamUnitHdr);

    char *pPackedParamUnitFromApp = new char [MAX_BYTE_PARAM_SPEECH];
    memset(pPackedParamUnitFromApp, 0, MAX_BYTE_PARAM_SPEECH);
#ifdef APP_PARSER_SUPPORT
    AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT pParamLayerInfo;

    pParamLayerInfo.audioTypeName = (char *) audioTypeNameList[AUDIO_TYPE_SPEECH_GENERAL];
    pParamLayerInfo.numCategoryType = MAX_NUM_CATEGORY_TYPE_SPEECH_GENERAL;//4
    pParamLayerInfo.numParam = MAX_NUM_PARAM_SPEECH_GENERAL;//4

    pParamLayerInfo.categoryType.assign(audioType_SpeechGeneral_CategoryType, audioType_SpeechGeneral_CategoryType + pParamLayerInfo.numCategoryType);
    pParamLayerInfo.paramName.assign(audioType_SpeechGeneral_ParamName, audioType_SpeechGeneral_ParamName + pParamLayerInfo.numParam);

    ALOGV("%s(), categoryType.size=%d, paramName.size=%d", __FUNCTION__, pParamLayerInfo.categoryType.size(), pParamLayerInfo.paramName.size());
    for (u4Index = 0; u4Index < pParamLayerInfo.paramName.size() ; u4Index++)
    {
        ALOGV("%s(), paramName[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.paramName.at(u4Index).string());
    }


    DataHeader = 0x000F;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);

    pParamLayerInfo.categoryName.push_back(String8(audioType_SpeechGeneral_CategoryName1[0]));

    GetSpeechParamFromAppParser(AUDIO_TYPE_SPEECH_GENERAL, &pParamLayerInfo, pPackedParamUnitFromApp, &sizeByteFromApp);

    memcpy(pPackedParamUnit + size, pPackedParamUnitFromApp, sizeByteFromApp);
    size += sizeByteFromApp;



#else

    //NB
    DataHeader = 0x000F;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);
    ALOGD("%s(), After hdr size=%d", __FUNCTION__, size);


    SPEECH_GENERAL_PARAM_STRUCT eSphParamGeneral;

    memset(&eSphParamGeneral, 0, sizeof(eSphParamGeneral));

    unsigned short speech_common_para[12] =
    {
        0,  55997,  31000,    10752,      32769,      0,      0,      0, \
        0,      0,      0,      0
    };

    unsigned short debug_info[16] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0
    };

    memcpy(&eSphParamGeneral.speech_common_para, &speech_common_para, sizeof(speech_common_para));
    memcpy(&eSphParamGeneral.debug_info, &debug_info, sizeof(debug_info));

    memcpy(pPackedParamUnit + size, &eSphParamGeneral, sizeof(eSphParamGeneral));
    size += sizeof(eSphParamGeneral);
    ALOGD("%s(), After NB size=%d", __FUNCTION__, size);
#endif
    ALOGD("-%s(), total size byte=%d", __FUNCTION__, size);
    if (pPackedParamUnitFromApp != NULL)
    {
        delete[] pPackedParamUnitFromApp;
    }

    return size;
}

speech_profile_t SpeechParamParser::GetSpeechProfile(const speech_mode_t IdxMode, bool bBtHeadsetNrecOn)
{
    speech_profile_t index_speech_profile;

    if (mSphParamInfo.bLPBK)
    {
        switch (IdxMode)
        {
            case SPEECH_MODE_NORMAL:
                index_speech_profile = SPEECH_PROFILE_LPBK_HANDSET;

                break;

            case SPEECH_MODE_EARPHONE:
                index_speech_profile = SPEECH_PROFILE_LPBK_HEADSET;
                break;
            case SPEECH_MODE_LOUD_SPEAKER:
                index_speech_profile = SPEECH_PROFILE_LPBK_HANDSFREE;
                break;
            default:
                index_speech_profile = SPEECH_PROFILE_LPBK_HANDSET;

                break;
        }
    }
    else
    {
        switch (IdxMode)
        {
            case SPEECH_MODE_NORMAL:
                if (mSphParamInfo.bTTY)
                {
                    index_speech_profile = SPEECH_PROFILE_TTY_HANDSET;
                }
                else
                {
                    index_speech_profile = SPEECH_PROFILE_HANDSET;
                }

                break;

            case SPEECH_MODE_EARPHONE:
            {
                switch (mSphParamInfo.uHeadsetPole)
                {
                    case 3:
                        index_speech_profile = SPEECH_PROFILE_3_POLE_HEADSET;
                        break;
                    case 4:
                        index_speech_profile = SPEECH_PROFILE_4_POLE_HEADSET;
                        break;
                    case 5:
                        index_speech_profile = SPEECH_PROFILE_5_POLE_HEADSET;
                        break;
                    default:
                        index_speech_profile = SPEECH_PROFILE_4_POLE_HEADSET;
                        break;
                }
                break;
            }
            case SPEECH_MODE_LOUD_SPEAKER:

                if (mSphParamInfo.bTTY)
                {
                    index_speech_profile = SPEECH_PROFILE_TTY_HANDSFREE;
                }
                else
                {
                    index_speech_profile = SPEECH_PROFILE_HANDSFREE;
                }
                break;
            case SPEECH_MODE_BT_EARPHONE:
            case SPEECH_MODE_BT_CORDLESS:
            case SPEECH_MODE_BT_CARKIT:
                if (bBtHeadsetNrecOn == true)
                {
                    index_speech_profile = SPEECH_PROFILE_BT_EARPHONE;
                }
                else
                {
                    index_speech_profile = SPEECH_PROFILE_BT_NREC_OFF;
                }
                break;
            case SPEECH_MODE_MAGIC_CON_CALL:
                index_speech_profile = SPEECH_PROFILE_MAGICONFERENCE;
                break;
            case SPEECH_MODE_HAC:
                index_speech_profile = SPEECH_PROFILE_HAC;
                break;
            default:
                index_speech_profile = SPEECH_PROFILE_HANDSET;

                break;
        }
    }
    ALOGD("%s(), IdxMode = %d, IdxPfrfile = %d", __FUNCTION__, IdxMode, index_speech_profile);

    return index_speech_profile;
}

status_t SpeechParamParser::SetMDParamUnitHdr(speech_type_dynamic_param_t idxAudioType, SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT *sParamUnitHdr, uint16_t NumBand)
{
    switch (idxAudioType)
    {
        case AUDIO_TYPE_SPEECH:
            sParamUnitHdr->SphUnitMagiNum = 0xAA01;
            sParamUnitHdr->SphParserVer = 1;
            sParamUnitHdr->NumLayer = 0x2;
            sParamUnitHdr->ParamHeader[0] = 0x1F;//all network use, while modem not check it
            //Network: bit0: GSM, bit1: WCDMA,.bit2: CDMA, bit3: VoLTE, bit4:C2K
            switch (NumBand)
            {
                case 1:
                    sParamUnitHdr->ParamHeader[1] = 0x1;//voice band:NB
                    break;
                case 2:
                    sParamUnitHdr->ParamHeader[1] = 0x3;//voice band:NB,WB
                    break;
                case 3:
                    sParamUnitHdr->ParamHeader[1] = 0x7;//voice band:NB,WB,SWB
                    break;
                case 4:
                    sParamUnitHdr->ParamHeader[1] = 0xF;//voice band:NB,WB,SWB,FB
                    break;
                default:
                    sParamUnitHdr->ParamHeader[1] = 0x3;//voice band:NB,WB
                    break;
            }
            sParamUnitHdr->ParamHeader[2] = (mSpeechParamVerFirst << 4) + mSpeechParamVerLast;
            ALOGD("%s(), SphUnitMagiNum = 0x%x, Version = 0x%x", __FUNCTION__, sParamUnitHdr->SphUnitMagiNum, sParamUnitHdr->ParamHeader[2]);
            break;
        default:
            break;
    }
    return NO_ERROR;
}

uint16_t SpeechParamParser::SetMDParamDataHdr(SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT sParamUnitHdr, Category *cateBand, Category *cateNetwork)
{

    uint16_t u4Index = 0;
    uint16_t DataHeader = 0, MaskNetwork = 0;
    bool bNetworkMatch = false;
    if (strcmp(cateBand->name, "NB") == 0)//All netwrok use
    {
        DataHeader = 0x1000;
    }
    else if (strcmp(cateBand->name, "WB") == 0)
    {
        DataHeader = 0x2000;
    }
    else if (strcmp(cateBand->name, "SWB") == 0)
    {
        DataHeader = 0x3000;
    }
    else if (strcmp(cateBand->name, "FB") == 0)
    {
        DataHeader = 0x4000;
    }
    //-----------
    for (u4Index = 0; u4Index < numSpeechNetwork ; u4Index++)
    {
        ALOGV("%s(), cateNetwork= %s, ListSpeechNetwork[%d]=%s", __FUNCTION__, cateNetwork->name, u4Index, ListSpeechNetwork[u4Index].name);
        if (strcmp(cateNetwork->name, ListSpeechNetwork[u4Index].name) == 0)
        {
            MaskNetwork = ListSpeechNetwork[u4Index].supportBit;
            ALOGV("%s(), cateNetwork= %s, ListSpeechNetwork[%d]=%s, MaskNetwork=0x%x", __FUNCTION__, cateNetwork->name, u4Index, ListSpeechNetwork[u4Index].name, MaskNetwork);
            bNetworkMatch = true;
            break;
        }

    }
    if (!bNetworkMatch)
    {
        ALOGE("%s(), cateNetwork= %s, ListSpeechNetwork[%d]=%s, bNetworkMatch=%d, NO match!!!", __FUNCTION__, cateNetwork->name, u4Index, ListSpeechNetwork[u4Index].name, bNetworkMatch);

    }
    if (!sphNetworkSupport)
    {
        DataHeader = DataHeader >> 8;
        MaskNetwork = 0xF;
    }
    DataHeader |= MaskNetwork;
    ALOGD("-%s(), DataHeader=0x%x, MaskNetwork=0x%x, cateBand=%s", __FUNCTION__, DataHeader, MaskNetwork, cateBand->name);

    return DataHeader;
}


int SpeechParamParser::InitSpeechNetwork(void)
{
    uint16_t size = 0, u4Index, sizeByteFromApp = 0;
    char *pPackedParamUnitFromApp = new char [10];
    memset(pPackedParamUnitFromApp, 0, 10);

    AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT pParamLayerInfo;
    //-------------
    pParamLayerInfo.audioTypeName = (char *) audioTypeNameList[AUDIO_TYPE_SPEECH_NETWORK];

    if (mAppHandle == NULL)
    {
        ALOGE("%s() mAppHandle == NULL, Assert!!!", __FUNCTION__);
        ASSERT(0);
    }

    /* Query AudioType */
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL)
    {
        ALOGE("Error %s %d", __FUNCTION__, __LINE__);
        ASSERT(0);
    }
    AudioType *audioType = appOps->appHandleGetAudioTypeByName(mAppHandle, pParamLayerInfo.audioTypeName);

    pParamLayerInfo.numCategoryType = appOps->audioTypeGetNumOfCategoryType(audioType);//1

    pParamLayerInfo.numParam = MAX_NUM_PARAM_SPEECH_NETWORK;//4

    pParamLayerInfo.categoryType.assign(audioType_SpeechNetwork_CategoryType, audioType_SpeechNetwork_CategoryType + pParamLayerInfo.numCategoryType);
    pParamLayerInfo.paramName.assign(audioType_SpeechNetwork_ParamName, audioType_SpeechNetwork_ParamName + pParamLayerInfo.numParam);

    ALOGV("%s(), categoryType.size=%d, paramName.size=%d", __FUNCTION__, pParamLayerInfo.categoryType.size(), pParamLayerInfo.paramName.size());
    for (u4Index = 0; u4Index < pParamLayerInfo.categoryType.size() ; u4Index++)
    {
        ALOGV("%s(), categoryType[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.categoryType.at(u4Index).string());
    }
    for (u4Index = 0; u4Index < pParamLayerInfo.paramName.size() ; u4Index++)
    {
        ALOGV("%s(), paramName[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.paramName.at(u4Index).string());
    }
    //-----------
    //parse layer
    CategoryType *categoryNetwork = appOps->audioTypeGetCategoryTypeByName(audioType, audioType_SpeechNetwork_CategoryType[0].string());
    numSpeechNetwork = appOps->categoryTypeGetNumOfCategory(categoryNetwork);

    //parse network
    for (int i = 0; i < numSpeechNetwork; i++)
    {
        Category *CateNetwork = appOps->categoryTypeGetCategoryByIndex(categoryNetwork, i);
        sizeByteFromApp = 0;
        //clear
        while (!pParamLayerInfo.categoryName.empty())
        {
            pParamLayerInfo.categoryName.pop_back();
        }
        strcpy(ListSpeechNetwork[i].name, CateNetwork->name);

        pParamLayerInfo.categoryName.push_back(String8(CateNetwork->name));//Network

        GetSpeechParamFromAppParser(AUDIO_TYPE_SPEECH_NETWORK, &pParamLayerInfo, pPackedParamUnitFromApp, &sizeByteFromApp);
        ListSpeechNetwork[i].supportBit = *((uint16_t *)pPackedParamUnitFromApp);
        size += sizeByteFromApp;

        ALOGD("%s(), i=%d, sizeByteFromApp=%d, supportBit=0x%x", __FUNCTION__, i, sizeByteFromApp, ListSpeechNetwork[i].supportBit);

    }

    if (pPackedParamUnitFromApp != NULL)
    {
        delete[] pPackedParamUnitFromApp;
    }
    ALOGD("-%s(), total size byte=%d", __FUNCTION__, size);
    return size;
}


int SpeechParamParser::GetSpeechParamUnit(char *pPackedParamUnit, int *p4ParamArg)
{
    uint16_t size = 0, u4Index, sizeByteFromApp = 0;
    uint16_t DataHeader, IdxInfo = 0, IdxTmp = 0, NumOfBand = 0, NumOfNetwork = 0, NumOfVolume = 0;
    short IdmVolumeFIR = 0;
    SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT eParamUnitHdr;

    char *pPackedParamUnitFromApp = new char [MAX_BYTE_PARAM_SPEECH];
    memset(pPackedParamUnitFromApp, 0, MAX_BYTE_PARAM_SPEECH);

    speech_mode_t IdxMode = (speech_mode_t) * ((int *)p4ParamArg);
    int IdxVolume = *((int *)p4ParamArg + 1);
    bool bBtHeadsetNrecOn = (bool) * ((int *)p4ParamArg + 2);
    mSphParamInfo.bBtHeadsetNrecOn = bBtHeadsetNrecOn;
    mSphParamInfo.u4VolumeIndex = IdxVolume;
    mSphParamInfo.SpeechMode = IdxMode;

    ALOGV("+%s(), IdxMode=0x%x, IdxVolume=0x%x, bBtHeadsetNrecOn=0x%x", __FUNCTION__, IdxMode, IdxVolume, bBtHeadsetNrecOn);
    int IdxPfrfile = GetSpeechProfile(IdxMode, bBtHeadsetNrecOn);
    memset(&eParamUnitHdr, 0, sizeof(eParamUnitHdr));

    AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT pParamLayerInfo;
    //-------------
    pParamLayerInfo.audioTypeName = (char *) audioTypeNameList[AUDIO_TYPE_SPEECH];

    if (mAppHandle == NULL)
    {
        ALOGE("%s() mAppHandle == NULL, Assert!!!", __FUNCTION__);
        ASSERT(0);
    }

    /* Query AudioType */
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL)
    {
        ALOGE("Error %s %d", __FUNCTION__, __LINE__);
        ASSERT(0);
    }
    AudioType *audioType = appOps->appHandleGetAudioTypeByName(mAppHandle, pParamLayerInfo.audioTypeName);

    //    pParamLayerInfo.numCategoryType = MAX_NUM_CATEGORY_TYPE_SPEECH;//4
    pParamLayerInfo.numCategoryType = appOps->audioTypeGetNumOfCategoryType(audioType);

    pParamLayerInfo.numParam = MAX_NUM_PARAM_SPEECH;//4


    pParamLayerInfo.categoryType.assign(audioType_Speech_CategoryType, audioType_Speech_CategoryType + pParamLayerInfo.numCategoryType);
    pParamLayerInfo.paramName.assign(audioType_Speech_ParamName, audioType_Speech_ParamName + pParamLayerInfo.numParam);

    ALOGV("%s(), categoryType.size=%d, paramName.size=%d", __FUNCTION__, pParamLayerInfo.categoryType.size(), pParamLayerInfo.paramName.size());
    for (u4Index = 0; u4Index < pParamLayerInfo.categoryType.size() ; u4Index++)
    {
        ALOGV("%s(), categoryType[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.categoryType.at(u4Index).string());
    }
    for (u4Index = 0; u4Index < pParamLayerInfo.paramName.size() ; u4Index++)
    {
        ALOGV("%s(), paramName[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.paramName.at(u4Index).string());
    }
    //-----------

    //parse layer
    CategoryType *categoryNetwork = appOps->audioTypeGetCategoryTypeByName(audioType, audioType_Speech_CategoryType[3].string());
    NumOfNetwork = appOps->categoryTypeGetNumOfCategory(categoryNetwork);
    CategoryType *categoryBand = appOps->audioTypeGetCategoryTypeByName(audioType, audioType_Speech_CategoryType[0].string());
    NumOfBand = appOps->categoryTypeGetNumOfCategory(categoryBand);

    CategoryType *categoryVolume = appOps->audioTypeGetCategoryTypeByName(audioType, audioType_Speech_CategoryType[2].string());
    CategoryGroup *categoryGroupVolume = appOps->categoryTypeGetCategoryGroupByIndex(categoryVolume, 0);
    NumOfVolume = appOps->categoryGroupGetNumOfCategory(categoryGroupVolume);
    IdxTmp = (NumOfBand & 0xF) << 4;
    eParamUnitHdr.NumEachLayer = IdxTmp + (NumOfNetwork & 0xF);
    ALOGV("%s(), SphUnitMagiNum= 0x%x, NumEachLayer=0x%x", __FUNCTION__, eParamUnitHdr.SphUnitMagiNum, eParamUnitHdr.NumEachLayer);
    SetMDParamUnitHdr(AUDIO_TYPE_SPEECH, &eParamUnitHdr, NumOfBand);
    ALOGD("%s(), SphUnitMagiNum= 0x%x, NumEachLayer=0x%x", __FUNCTION__, eParamUnitHdr.SphUnitMagiNum, eParamUnitHdr.NumEachLayer);
    ALOGV("%s(), categoryNetwork= %s, categoryBand = %s, categoryVolume = %s", __FUNCTION__, categoryNetwork->name, categoryBand->name, categoryVolume->name);
    ALOGV("%s(), NumOfNetwork= %d, NumOfBand = %d, NumOfVolume = %d", __FUNCTION__, NumOfNetwork, NumOfBand, NumOfVolume);

    memcpy(pPackedParamUnit + size, &eParamUnitHdr, sizeof(eParamUnitHdr));
    size += sizeof(eParamUnitHdr);

    IdxInfo = IdxMode & 0xF;
    ALOGV("%s(), add mode IdxInfo=0x%x", __FUNCTION__, IdxInfo);
    IdxTmp = IdxVolume << 4;
    IdxInfo += IdxTmp;
    ALOGV("%s(), add volume<<4 IdxInfo=0x%x, IdxTmp=0x%x", __FUNCTION__, IdxInfo, IdxTmp);

    memcpy(pPackedParamUnit + size, &IdxInfo, sizeof(IdxInfo));
    size += sizeof(IdxInfo);

    //parse network
    for (int i = 0; i < NumOfNetwork; i++)
    {
        Category *CateNetwrok =  appOps->categoryTypeGetCategoryByIndex(categoryNetwork, i);
        //parse band
        for (int j = 0; j < NumOfBand; j++)
        {
            sizeByteFromApp = 0;
            Category *CateBand =  appOps->categoryTypeGetCategoryByIndex(categoryBand, j);

            DataHeader = SetMDParamDataHdr(eParamUnitHdr, CateBand, CateNetwrok);
            memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
            size += sizeof(DataHeader);
            while (!pParamLayerInfo.categoryName.empty())
            {
                pParamLayerInfo.categoryName.pop_back();
            }
            //Band
            //pParamLayerInfo.categoryName.at(0) = String8(CateBand->name);//Band
            pParamLayerInfo.categoryName.push_back(String8(CateBand->name));//Band
            //Profile
            //pParamLayerInfo.categoryName.at(1) = String8(audioType_Speech_CategoryName2[IdxPfrfile]);//Profile
            pParamLayerInfo.categoryName.push_back(String8(audioType_Speech_CategoryName2[IdxPfrfile]));
            //Volume
            if (IdxVolume > 6 || IdxVolume < 0)
            {
                //           pParamLayerInfo.categoryName.at(2) = String8(audioType_Speech_CategoryName3[3]);//Volume
                pParamLayerInfo.categoryName.push_back(String8(audioType_Speech_CategoryName3[3]));//volume
                ALOGE("%s(), Invalid IdxVolume=0x%x, use 3 !!!", __FUNCTION__, IdxVolume);
            }
            else
            {
                //            pParamLayerInfo.categoryName.at(2) = String8(audioType_Speech_CategoryName3[IdxVolume]);//Volume
                pParamLayerInfo.categoryName.push_back(String8(audioType_Speech_CategoryName3[IdxVolume]));
            }
            //Network
            //           pParamLayerInfo.categoryName.at(3) = String8(CateNetwrok->name);//Network
            pParamLayerInfo.categoryName.push_back(String8(CateNetwrok->name));//Network

            for (u4Index = 0; u4Index < pParamLayerInfo.categoryName.size() ; u4Index++)
            {
                ALOGV("%s(), categoryName[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.categoryName.at(u4Index).string());
            }

            GetSpeechParamFromAppParser(AUDIO_TYPE_SPEECH, &pParamLayerInfo, pPackedParamUnitFromApp, &sizeByteFromApp);

            memcpy(pPackedParamUnit + size, pPackedParamUnitFromApp, sizeByteFromApp);
            size += sizeByteFromApp;

            ALOGD("%s(), iNetwork=%d, jBand=%d, total size byte=%d", __FUNCTION__, i, j, size);

        }


    }


    if (pPackedParamUnitFromApp != NULL)
    {
        delete[] pPackedParamUnitFromApp;
    }
    ALOGD("-%s(), total size byte=%d", __FUNCTION__, size);
    return size;
}


status_t SpeechParamParser::SetParamInfo(const String8 &keyParamPairs)
{
    ALOGD("+%s(): %s", __FUNCTION__, keyParamPairs.string());
    AudioParameter param = AudioParameter(keyParamPairs);
    int value;
    if (param.getInt(String8("ParamSphLpbk"), value) == NO_ERROR)
    {
        param.remove(String8("ParamSphLpbk"));
#if defined(MTK_AUDIO_SPH_LPBK_PARAM)
        mSphParamInfo.bLPBK = (value == 1) ? true : false;
#else
        mSphParamInfo.bLPBK = false;

#endif
        ALOGD("%s(): mSphParamInfo.bLPBK = %d", __FUNCTION__, mSphParamInfo.bLPBK);
    }
    if (param.getInt(String8("ParamHeadsetPole"), value) == NO_ERROR)
    {
        param.remove(String8("ParamHeadsetPole"));
        if (value == 3 || value == 4 || value == 5)
        {
            mSphParamInfo.uHeadsetPole = value;
        }
        else
        {
            mSphParamInfo.uHeadsetPole = 4;
            ALOGE("%s(): Invalid HeadsetPole(%d), set default 4_pole!!!", __FUNCTION__, value);
        }
        ALOGD("%s(): mSphParamInfo.uHeadsetPole = %d", __FUNCTION__, mSphParamInfo.uHeadsetPole);
    }
    if (param.getInt(String8("ParamSphTty"), value) == NO_ERROR)
    {
        param.remove(String8("ParamSphTty"));
        if (sphTTYSupport)
        {
            mSphParamInfo.bTTY = (value == 1) ? true : false;
        }
        ALOGD("%s(): mSphParamInfo.bTTY = %d", __FUNCTION__, mSphParamInfo.bTTY);
    }

    ALOGD("-%s(): %s", __FUNCTION__, keyParamPairs.string());
    return NO_ERROR;
}

int SpeechParamParser::GetMagiClarityParamUnit(char *pPackedParamUnit)
{
    ALOGD("+%s()", __FUNCTION__);
    uint16_t size = 0, u4Index = 0, u4Index2 = 0, sizeByteFromApp = 0;
    uint16_t DataHeader;
    SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT eParamUnitHdr;
    memset(&eParamUnitHdr, 0, sizeof(eParamUnitHdr));

    eParamUnitHdr.SphParserVer = 1;
    eParamUnitHdr.NumLayer = 0x1;
    eParamUnitHdr.NumEachLayer = 0x1;
    eParamUnitHdr.ParamHeader[0] = 0x1;//Common
    eParamUnitHdr.SphUnitMagiNum = 0xAA04;

    memcpy(pPackedParamUnit + size, &eParamUnitHdr, sizeof(eParamUnitHdr));
    size += sizeof(eParamUnitHdr);

    char *pPackedParamUnitFromApp = new char [MAX_BYTE_PARAM_SPEECH];
    memset(pPackedParamUnitFromApp, 0, MAX_BYTE_PARAM_SPEECH);
    AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT pParamLayerInfo;

    pParamLayerInfo.audioTypeName = (char *) audioTypeNameList[AUDIO_TYPE_SPEECH_MAGICLARITY];
    pParamLayerInfo.numCategoryType = MAX_NUM_CATEGORY_TYPE_SPEECH_MAGICLARITY;//4
    pParamLayerInfo.numParam = MAX_NUM_PARAM_SPEECH_MAGICLARITY;//4

    pParamLayerInfo.categoryType.assign(audioType_SpeechMagiClarity_CategoryType, audioType_SpeechMagiClarity_CategoryType + pParamLayerInfo.numCategoryType);
    pParamLayerInfo.paramName.assign(audioType_SpeechMagiClarity_ParamName, audioType_SpeechMagiClarity_ParamName + pParamLayerInfo.numParam);

    ALOGV("%s(), categoryType.size=%d, paramName.size=%d", __FUNCTION__, pParamLayerInfo.categoryType.size(), pParamLayerInfo.paramName.size());
    for (u4Index = 0; u4Index < pParamLayerInfo.paramName.size() ; u4Index++)
    {
        ALOGV("%s(), paramName[%d]= %s", __FUNCTION__, u4Index, pParamLayerInfo.paramName.at(u4Index).string());
    }
    DataHeader = 0x000F;
    memcpy(pPackedParamUnit + size, &DataHeader, sizeof(DataHeader));
    size += sizeof(DataHeader);

    pParamLayerInfo.categoryName.push_back(String8(audioType_SpeechMagiClarity_CategoryName1[0]));

    GetSpeechParamFromAppParser(AUDIO_TYPE_SPEECH_MAGICLARITY, &pParamLayerInfo, pPackedParamUnitFromApp, &sizeByteFromApp);

    memcpy(pPackedParamUnit + size, pPackedParamUnitFromApp, sizeByteFromApp);
    size += sizeByteFromApp;

    ALOGD("-%s(), total size byte=%d", __FUNCTION__, size);

    if (pPackedParamUnitFromApp != NULL)
    {
        delete[] pPackedParamUnitFromApp;
    }
    return size;
}


}







//namespace android
