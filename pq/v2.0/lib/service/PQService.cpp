#define LOG_TAG "PQ"
#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/properties.h>

/*PQService */
#include <dlfcn.h>
#include <math.h>
#include <utils/SortedVector.h>
#include <binder/PermissionCache.h>
#include <android/native_window.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>
#include <cutils/memory.h>

 /*PQService */
#include <linux/disp_session.h>

#include "ddp_drv.h"
#include "cust_gamma.h"
#include "cust_color.h"
#include "cust_tdshp.h"

#include <dlfcn.h>
/*PQService */
#include <binder/BinderService.h>
#include <PQCommon.h>
#include <PQService.h>
#include <BluLight_Defender.h>

/* PQ ashmem proxy */
#include <PQAshmemProxy.h>

/*PQService */
namespace android {

#define DISP_DEV_NODE_PATH "/dev/mtk_disp_mgr"

// Property of blue light filter strength
#define MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME "persist.sys.bluelight.strength"

#define PQ_LOGD(fmt, arg...) ALOGD(fmt, ##arg)
#define PQ_LOGE(fmt, arg...) ALOGE(fmt, ##arg)

#define max( a, b )            (((a) > (b)) ? (a) : (b))

/* Structure */
struct ColorRegistersTuning
{
    unsigned int GLOBAL_SAT  ;
    unsigned int CONTRAST    ;
    unsigned int BRIGHTNESS  ;
    unsigned int PARTIAL_Y    [CLR_PARTIAL_Y_SIZE];
    unsigned int PURP_TONE_S  [CLR_PQ_PARTIALS_CONTROL][CLR_PURP_TONE_SIZE];
    unsigned int SKIN_TONE_S  [CLR_PQ_PARTIALS_CONTROL][CLR_SKIN_TONE_SIZE];
    unsigned int GRASS_TONE_S [CLR_PQ_PARTIALS_CONTROL][CLR_GRASS_TONE_SIZE];
    unsigned int SKY_TONE_S   [CLR_PQ_PARTIALS_CONTROL][CLR_SKY_TONE_SIZE];
    unsigned int PURP_TONE_H  [CLR_PURP_TONE_SIZE];
    unsigned int SKIN_TONE_H  [CLR_SKIN_TONE_SIZE];
    unsigned int GRASS_TONE_H [CLR_GRASS_TONE_SIZE];
    unsigned int SKY_TONE_H   [CLR_SKY_TONE_SIZE];
    unsigned int CCORR_COEF   [3][3];
};

template <typename _T>
static char *printIntArray(char *ori_buffer, int bsize, const _T *arr, int length)
{
    char *buffer = ori_buffer;
    int n;

    n = 4;
    while (length > 0 && bsize > 1) {
        while (length < n)
            n >>= 1;
        if (n == 0) break; // length == 0

        // We print n items at one time
        int pr_len;
        if (n == 4) {
            pr_len = snprintf(buffer, bsize, " %4d %4d %4d %4d",
                (int)(arr[0]), (int)(arr[1]), (int)(arr[2]), (int)(arr[3]));
        } else if (n == 2) {
            pr_len = snprintf(buffer, bsize, " %4d %4d", (int)(arr[0]), (int)(arr[1]));
        } else {
            pr_len = snprintf(buffer, bsize, " %4d", (int)(arr[0]));
        }

        buffer += pr_len;
        bsize -= pr_len;
        arr += n;
        length -= n;
    }

    ori_buffer[bsize - 1] = '\0';

    return ori_buffer;
}


static inline int valueOf(const char16_t ch)
{
    if (L'0' <= ch && ch <= L'9')
        return (ch - L'0');
    else if (L'a' <= ch && ch <= L'z')
        return (ch - L'a') + 10;
    else if (L'A' <= ch && ch <= L'Z')
        return (ch - L'A') + 10;

    return 0;
}


static int asInt(const String16& str)
{
    int val = 0;
    bool negative = false;
    int base = 10;

    const char16_t *char_p = str.string();

    if (*char_p == L'-') {
        negative = true;
        char_p++;
    } else if (*char_p == '+') {
        negative = false;
        char_p++;
    } else
        negative = false;

    if (*char_p == L'0' && *(char_p + 1) == L'x') {
        base = 16;
        char_p += 2;
    }

    for ( ; *char_p != L'\0'; char_p++) {
        val = val * base + valueOf(*char_p);
    }

    return (negative ? -val : val);
}


template <typename _D, typename _S>
int convertCopy(_D *dest, size_t destSize, const _S *src, size_t srcSize)
{
    size_t destLen = destSize / sizeof(_D);
    size_t srcLen = srcSize / sizeof(_S);

    if (srcLen < destLen)
        destLen = srcLen;

    for (size_t i = 0; i < destLen; i++) {
        dest[i] = static_cast<_D>(src[i]);
    }

    return destLen;
}

PQService::PQService()
{
    PQ_LOGD("[PQ_SERVICE] PQService constructor start");

    m_drvID = open(DISP_DEV_NODE_PATH, O_RDONLY, 0);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
    }

    // set all feature default off
    memset(m_bFeatureSwitch, 0, sizeof(uint32_t) * PQ_FEATURE_MAX);

    // update it by each feature
#ifndef DISP_COLOR_OFF
    m_bFeatureSwitch[DISPLAY_COLOR] = 1;
#endif
#ifdef MDP_COLOR_ENABLE
    m_bFeatureSwitch[CONTENT_COLOR] = 1;
#endif
    m_bFeatureSwitch[DYNAMIC_SHARPNESS] = 1;

    m_pic_statndard = NULL;
    m_pic_vivid = NULL;
    m_pic_userdef = NULL;

    blueLight = new BluLightDefender;
    mBLInputMode = PQ_TUNING_NORMAL;
    mBLInput = NULL;
    mBLOutputMode = PQ_TUNING_NORMAL;
    mBLOutput = NULL;

    mCcorrDebug = false;
    mdp_win_param.split_en = 0;
    mdp_win_param.start_x = 0;
    mdp_win_param.start_y = 0;
    mdp_win_param.end_x = 0xffff;
    mdp_win_param.end_y = 0xffff;
    memset(&m_pqparam, 0, sizeof(DISP_PQ_PARAM));

    char value[PROPERTY_VALUE_MAX];
    property_get(PQ_PIC_MODE_PROPERTY_STR, value, PQ_PIC_MODE_DEFAULT);
    m_PQMode = atoi(value);

    m_PQScenario = SCENARIO_PICTURE;
    mPQDebugFlag = 0;
    mBlueLightDebugFlag = 0;

    m_AshmemProxy = new PQAshmemProxy;

    initAshmem();

    initDefaultPQParam();

    m_GlobalPQSwitch = 0;
    m_GlobalPQStrength = 0;
    m_GlobalPQStrengthRange = 0;
    m_GlobalPQSupport = 0;
    bzero(&(m_GlobalPQindex.dcindex), sizeof(DISPLAY_DC_T));

#ifdef MTK_GLOBAL_PQ_SUPPORT
    initGlobalPQ();
#endif

    PQ_LOGD("[PQ_SERVICE] PQService constructor end");
}

PQService::~PQService()
{
    close(m_drvID);

    delete blueLight;
    if (mBLInput != NULL)
        delete static_cast<ColorRegistersTuning*>(mBLInput);
    if (mBLOutput != NULL)
        delete static_cast<ColorRegistersTuning*>(mBLOutput);

    delete m_pic_statndard;
    delete m_pic_vivid;
    delete m_pic_userdef;

    delete m_AshmemProxy;

    if (m_isAshmemInit == true)
        mAshMemHeap = 0;
}

void PQService::calcPQStrength(DISP_PQ_PARAM *pqparam_dst, DISP_PQ_PARAM *pqparam_src, int percentage)
{
    pqparam_dst->u4SatGain = pqparam_src->u4SatGain * percentage / 100;
    pqparam_dst->u4PartialY  = pqparam_src->u4PartialY;
    pqparam_dst->u4HueAdj[0] = pqparam_src->u4HueAdj[0];
    pqparam_dst->u4HueAdj[1] = pqparam_src->u4HueAdj[1];
    pqparam_dst->u4HueAdj[2] = pqparam_src->u4HueAdj[2];
    pqparam_dst->u4HueAdj[3] = pqparam_src->u4HueAdj[3];
    pqparam_dst->u4SatAdj[0] = pqparam_src->u4SatAdj[0];
    pqparam_dst->u4SatAdj[1] = pqparam_src->u4SatAdj[1] * percentage / 100;
    pqparam_dst->u4SatAdj[2] = pqparam_src->u4SatAdj[2] * percentage / 100;
    pqparam_dst->u4SatAdj[3] = pqparam_src->u4SatAdj[3] * percentage / 100;
    pqparam_dst->u4Contrast = pqparam_src->u4Contrast * percentage / 100;
    pqparam_dst->u4Brightness = pqparam_src->u4Brightness * percentage / 100;
    pqparam_dst->u4SHPGain = pqparam_src->u4SHPGain * percentage / 100;
    pqparam_dst->u4Ccorr = pqparam_src->u4Ccorr;
}

void PQService::getUserModePQParam()
{
    char value[PROPERTY_VALUE_MAX];
    int i;

    property_get(PQ_TDSHP_PROPERTY_STR, value, PQ_TDSHP_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... tdshp[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4SatGain = i;

    property_get(PQ_GSAT_PROPERTY_STR, value, PQ_GSAT_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... gsat[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4SatGain = i;

    property_get(PQ_CONTRAST_PROPERTY_STR, value, PQ_CONTRAST_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... contrast[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4Contrast = i;

    property_get(PQ_PIC_BRIGHT_PROPERTY_STR, value, PQ_PIC_BRIGHT_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... pic bright[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4Brightness = i;
}

int32_t PQService::getPQStrengthRatio(int scenario)
{
    if (scenario ==  SCENARIO_PICTURE)
    {
        return m_pqparam_mapping.image;
    }
    else if (scenario == SCENARIO_VIDEO)
    {
        return m_pqparam_mapping.video;
    }
    else if (scenario == SCENARIO_ISP_PREVIEW)
    {
        return m_pqparam_mapping.camera;
    }
    else
    {
        PQ_LOGD("[PQ_SERVICE] invalid scenario %d\n",scenario);
        return m_pqparam_mapping.image;
    }
}

void PQService::initPQProperty(void)
{
    char value[PROPERTY_VALUE_MAX];

    property_get(PQ_DBG_SHP_EN_STR, value, PQ_DBG_SHP_EN_DEFAULT);
    property_set(PQ_DBG_SHP_EN_STR, value);
    m_AshmemProxy->setTuningField(SHP_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_DBG_SHP_EN_STR[%d]\n", atoi(value));

    property_get(PQ_DBG_DSHP_EN_STR, value, PQ_DBG_DSHP_EN_DEFAULT);
    property_set(PQ_DBG_DSHP_EN_STR, value);
    m_AshmemProxy->setTuningField(DSHP_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_DBG_DSHP_EN_STR[%d]\n", atoi(value));

    property_get(PQ_MDP_COLOR_EN_STR, value, PQ_MDP_COLOR_EN_DEFAULT);
    property_set(PQ_MDP_COLOR_EN_STR, value);
    m_AshmemProxy->setTuningField(VIDEO_CONTENT_COLOR_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_MDP_COLOR_EN_STR[%d]\n", atoi(value));

    property_get(PQ_ADL_PROPERTY_STR, value, PQ_ADL_INDEX_DEFAULT);
    property_set(PQ_ADL_PROPERTY_STR, value);
    m_AshmemProxy->setTuningField(DC_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_ADL_PROPERTY_STR[%d]\n", atoi(value));

    property_get(PQ_ISO_SHP_EN_STR, value, PQ_ISO_SHP_EN_DEFAULT);
    property_set(PQ_ISO_SHP_EN_STR, value);
    m_AshmemProxy->setTuningField(ISO_SHP_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_ISO_SHP_EN_STR[%d]\n", atoi(value));

    property_get(PQ_ULTRARES_EN_STR, value, PQ_ULTRARES_EN_DEFAULT);
    property_set(PQ_ULTRARES_EN_STR, value);
    m_AshmemProxy->setTuningField(UR_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_ULTRARES_EN_STR[%d]\n", atoi(value));

    property_get(PQ_MDP_COLOR_DBG_EN_STR, value, PQ_MDP_COLOR_DBG_EN_DEFAULT);
    property_set(PQ_MDP_COLOR_DBG_EN_STR, value);
    m_AshmemProxy->setTuningField(CONTENT_COLOR_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_MDP_COLOR_DBG_EN_STR[%d]\n", atoi(value));

    property_get(PQ_LOG_EN_STR, value, PQ_LOG_EN_DEFAULT);
    property_set(PQ_LOG_EN_STR, value);
    m_AshmemProxy->setTuningField(PQ_LOG_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_LOG_ENABLE[%d]\n", atoi(value));
}

int32_t PQService::getScenarioIndex(int32_t scenario)
{
    int32_t scenario_index;

    if (scenario ==  SCENARIO_PICTURE)
    {
        scenario_index = 0;
    }
    else if (scenario == SCENARIO_VIDEO)
    {
        scenario_index = 1;
    }
    else if (scenario == SCENARIO_ISP_PREVIEW)
    {
        scenario_index = 2;
    }
    else
    {
        scenario_index = 0;
    }

    return scenario_index;
}

bool PQService::isStandardPictureMode(int32_t mode, int32_t scenario_index)
{
    if (mode == PQ_PIC_MODE_STANDARD && scenario_index < PQ_SCENARIO_COUNT)
        return true;
    else
        return false;
}

bool PQService::isVividPictureMode(int32_t mode, int32_t scenario_index)
{
    if (mode == PQ_PIC_MODE_VIVID && scenario_index < PQ_SCENARIO_COUNT)
        return true;
    else
        return false;
}

bool PQService::isUserDefinedPictureMode(int32_t mode, int32_t scenario_index)
{
    if (mode == PQ_PIC_MODE_USER_DEF && scenario_index < PQ_SCENARIO_COUNT)
        return true;
    else
        return false;
}

void PQService::setPQParamlevel(DISP_PQ_PARAM *pqparam_image_ptr, int32_t index, int32_t level)
{
    switch (index) {
        case SET_PQ_SHP_GAIN:
            {
                pqparam_image_ptr->u4SHPGain = level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SHP_GAIN...[%d]\n", level);
            }
            break;
        case SET_PQ_SAT_GAIN:
            {
                pqparam_image_ptr->u4SatGain = level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_GAIN...[%d]\n", level);
            }
            break;
        case SET_PQ_LUMA_ADJ:
            {
                pqparam_image_ptr->u4PartialY= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_LUMA_ADJ...[%d]\n", level);
            }
            break;
        case  SET_PQ_HUE_ADJ_SKIN:
            {
                pqparam_image_ptr->u4HueAdj[1]= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_SKIN...[%d]\n", level);
            }
            break;
        case  SET_PQ_HUE_ADJ_GRASS:
            {
                pqparam_image_ptr->u4HueAdj[2]= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_GRASS...[%d]\n", level);
            }
            break;
        case  SET_PQ_HUE_ADJ_SKY:
            {
                pqparam_image_ptr->u4HueAdj[3]= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_SKY...[%d]\n", level);
            }
            break;
        case SET_PQ_SAT_ADJ_SKIN:
            {
                pqparam_image_ptr->u4SatAdj[1]= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_SKIN...[%d]\n", level);
            }
            break;
        case SET_PQ_SAT_ADJ_GRASS:
            {
                pqparam_image_ptr->u4SatAdj[2]= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_GRASS...[%d]\n", level);
            }
            break;
        case SET_PQ_SAT_ADJ_SKY:
            {
                pqparam_image_ptr->u4SatAdj[3]= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_GRASS...[%d]\n", level);
            }
            break;
        case SET_PQ_CONTRAST:
            {
                pqparam_image_ptr->u4Contrast= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_CONTRAST...[%d]\n", level);
            }
            break;
        case SET_PQ_BRIGHTNESS:
            {
                pqparam_image_ptr->u4Brightness= level;
                PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_BRIGHTNESS...[%d]\n", level);
            }
            break;
        default:
            PQ_LOGD("[PQ_SERVICE] setPQIndex default case...\n");
        }
}

status_t PQService::getColorRegion(DISP_PQ_WIN_PARAM *win_param)
{
    Mutex::Autolock _l(mLock);
    //for MDP split demo window, MDP should update this per frame???
    win_param->split_en = mdp_win_param.split_en;
    win_param->start_x = mdp_win_param.start_x;
    win_param->start_y = mdp_win_param.start_y;
    win_param->end_x = mdp_win_param.end_x;
    win_param->end_y = mdp_win_param.end_y;

    return NO_ERROR;
}

status_t PQService::setColorRegion(int32_t split_en,int32_t start_x,int32_t start_y,int32_t end_x,int32_t end_y)
{
    Mutex::Autolock _l(mLock);
#ifndef DISP_COLOR_OFF

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return UNKNOWN_ERROR ;
    }
    mdp_win_param.split_en = split_en;
    mdp_win_param.start_x = start_x;
    mdp_win_param.start_y = start_y;
    mdp_win_param.end_x = end_x;
    mdp_win_param.end_y = end_y;
    ioctl(m_drvID, DISP_IOCTL_PQ_SET_WINDOW, &mdp_win_param);
#endif
    return NO_ERROR;
}

status_t PQService::setPQMode(int32_t mode)
{
    Mutex::Autolock _l(mLock);
    m_PQMode = mode;
    char value[PROPERTY_VALUE_MAX];
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];

    snprintf(value, PROPERTY_VALUE_MAX, "%d\n", mode);
    property_set(PQ_PIC_MODE_PROPERTY_STR, value);
    PQ_LOGD("[PQ_SERVICE] property set... picture mode[%d]", mode);
#ifndef DISP_COLOR_OFF
    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return UNKNOWN_ERROR ;
    }

    if (mode == PQ_PIC_MODE_STANDARD || mode == PQ_PIC_MODE_VIVID)
    {
        if (mode == PQ_PIC_MODE_STANDARD) {
            p_pqparam = m_pic_statndard->getPQParam(0);
        } else if (mode == PQ_PIC_MODE_VIVID) {
            p_pqparam = m_pic_vivid->getPQParam(0);
        }

        PQ_LOGD("[PQ_SERVICE] setm_PQMode shp[%d],gsat[%d], cont[%d], bri[%d] ", p_pqparam->u4SHPGain, p_pqparam->u4SatGain, p_pqparam->u4Contrast, p_pqparam->u4Brightness);
        PQ_LOGD("[PQ_SERVICE] setm_PQMode hue0[%d], hue1[%d], hue2[%d], hue3[%d] ", p_pqparam->u4HueAdj[0], p_pqparam->u4HueAdj[1], p_pqparam->u4HueAdj[2], p_pqparam->u4HueAdj[3]);
        PQ_LOGD("[PQ_SERVICE] setm_PQMode sat0[%d], sat1[%d], sat2[%d], sat3[%d] ", p_pqparam->u4SatAdj[0], p_pqparam->u4SatAdj[1], p_pqparam->u4SatAdj[2], p_pqparam->u4SatAdj[3]);

        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (mode == PQ_PIC_MODE_USER_DEF)
    {
        p_pqparam = m_pic_userdef->getPQParam(0);

        PQ_LOGD("[PQ_SERVICE] setm_PQMode shp[%d], gsat[%d], cont[%d], bri[%d] ", m_pqparam.u4SHPGain, m_pqparam.u4SatGain, m_pqparam.u4Contrast, m_pqparam.u4Brightness);
        PQ_LOGD("[PQ_SERVICE] setm_PQMode hue0[%d], hue1[%d], hue2[%d], hue3[%d] ", m_pqparam.u4HueAdj[0], m_pqparam.u4HueAdj[1], m_pqparam.u4HueAdj[2], m_pqparam.u4HueAdj[3]);
        PQ_LOGD("[PQ_SERVICE] setm_PQMode sat0[%d], sat1[%d], sat2[%d], sat3[%d] ", m_pqparam.u4SatAdj[0], m_pqparam.u4SatAdj[1], m_pqparam.u4SatAdj[2], m_pqparam.u4SatAdj[3]);

        getUserModePQParam();

        calcPQStrength(&m_pqparam, p_pqparam, m_pqparam_mapping.image); //default scenario = image
    }
    else
    {
        PQ_LOGE("[PQ_SERVICE] unknown picture mode!!");

        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
#endif
    setPQParamWithFilter(m_drvID, &m_pqparam);

    return NO_ERROR;
}

status_t PQService::setPQIndex(int32_t level, int32_t  scenario, int32_t  tuning_mode, int32_t index)
{
    Mutex::Autolock _l(mLock);
    char value[PROPERTY_VALUE_MAX];
    DISP_PQ_PARAM *pqparam_image_ptr = &m_pqparam_table[0];
    DISP_PQ_PARAM *pqparam_video_ptr = &m_pqparam_table[1];

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return UNKNOWN_ERROR ;
    }

    m_PQScenario = scenario;

    if (m_PQMode == PQ_PIC_MODE_STANDARD)
    {
        pqparam_image_ptr = m_pic_statndard->getPQParamImage();
        pqparam_video_ptr = m_pic_statndard->getPQParamVideo();
    }
    else if (m_PQMode == PQ_PIC_MODE_VIVID)
    {
        pqparam_image_ptr = m_pic_vivid->getPQParamImage();
        pqparam_video_ptr = m_pic_vivid->getPQParamVideo();
    }
    else if (m_PQMode == PQ_PIC_MODE_USER_DEF)
    {
        pqparam_image_ptr = m_pic_userdef->getPQUserDefParam();
        pqparam_video_ptr = m_pic_userdef->getPQUserDefParam();
    }
    else
    {
        PQ_LOGE("[PQ_SERVICE] PQService : Unknown m_PQMode\n");
    }

    setPQParamlevel(pqparam_image_ptr, index, level);

    if (m_PQMode == PQ_PIC_MODE_STANDARD || m_PQMode == PQ_PIC_MODE_VIVID)
    {
        memcpy(&m_pqparam, pqparam_image_ptr, sizeof(DISP_PQ_PARAM));
    }
    else if (m_PQMode == PQ_PIC_MODE_USER_DEF)
    {
        calcPQStrength(&m_pqparam, pqparam_image_ptr, getPQStrengthRatio(scenario));
    }

    // if in Gallery PQ tuning mode, sync video param with image param
    if(tuning_mode == TDSHP_FLAG_TUNING)
    {
        *pqparam_video_ptr = *pqparam_image_ptr;
    }

    setPQParamWithFilter(m_drvID, &m_pqparam);

    return NO_ERROR;
}

status_t PQService::getMappedColorIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode)
{
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    m_PQScenario = scenario;
    int scenario_index = getScenarioIndex(m_PQScenario);
    PQ_LOGD("[PQ_SERVICE] getMappedColorIndex : m_PQScenario = %d, m_PQMode = %d\n", m_PQScenario, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        calcPQStrength(index, p_pqparam, getPQStrengthRatio(m_PQScenario));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedColorIndex, invalid mode or scenario\n");
    }

    return NO_ERROR;
}

status_t PQService::getMappedTDSHPIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode)
{

    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    m_PQScenario = scenario;
    int scenario_index = getScenarioIndex(m_PQScenario);
    PQ_LOGD("[PQ_SERVICE] getMappedTDSHPIndex : m_PQScenario = %d, m_PQMode = %d\n", m_PQScenario, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        calcPQStrength(index, p_pqparam, getPQStrengthRatio(m_PQScenario));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid mode or scenario\n");
    }

    return NO_ERROR;
}

status_t PQService::setPQDCIndex(int32_t level, int32_t index)
{
    Mutex::Autolock _l(mLock);

    g_PQ_DC_Param.param[index] = level;

    return NO_ERROR;
}

status_t PQService::getPQDCIndex(DISP_PQ_DC_PARAM *dcparam, int32_t index)
 {
    Mutex::Autolock _l(mLock);

    memcpy(dcparam, &g_PQ_DC_Param, sizeof(g_PQ_DC_Param));

    return NO_ERROR;
}
status_t PQService::getColorCapInfo(MDP_COLOR_CAP *param)
 {
    Mutex::Autolock _l(mLock);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ] open /proc/mtk_mira fail...");
        return UNKNOWN_ERROR ;
    }

    ioctl(m_drvID, DISP_IOCTL_PQ_GET_MDP_COLOR_CAP, param);

    return NO_ERROR;
}

status_t PQService::getTDSHPReg(MDP_TDSHP_REG_EX *param)
 {
    Mutex::Autolock _l(mLock);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ] open /proc/mtk_mira fail...");
        return UNKNOWN_ERROR ;
    }

    memcpy(param, &g_tdshp_reg, sizeof(g_tdshp_reg));

    return NO_ERROR;
}

status_t PQService::getPQDSIndex(DISP_PQ_DS_PARAM_EX *dsparam)
 {
    Mutex::Autolock _l(mLock);

    memcpy(dsparam, &g_PQ_DS_Param, sizeof(g_PQ_DS_Param));

    return NO_ERROR;
}

status_t PQService::getColorIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode)
{
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    m_PQScenario = scenario;
    int scenario_index = getScenarioIndex(m_PQScenario);
    PQ_LOGD("[PQ_SERVICE] PQService : m_PQScenario = %d, m_PQMode = %d\n", m_PQScenario, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid mode or scenario\n");
    }

    return NO_ERROR;
}

status_t PQService::getTDSHPIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode)
{
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    m_PQScenario = scenario;
    int scenario_index = getScenarioIndex(m_PQScenario);
    PQ_LOGD("[PQ_SERVICE] PQService : m_PQScenario = %d, m_PQMode = %d\n", m_PQScenario, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid mode or scenario\n");
    }

    return NO_ERROR;
}

status_t PQService::getTDSHPFlag(int32_t *TDSHPFlag)
{
    Mutex::Autolock _l(mLock);

    int32_t flag_value = 0;

    if (m_AshmemProxy->getTuningField(ASHMEM_TUNING_FLAG, &flag_value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] getTDSHPFlag : m_AshmemProxy->getTuningField() failed\n");
        *TDSHPFlag = 0;
        return UNKNOWN_ERROR;
    }

    *TDSHPFlag = flag_value;
    PQ_LOGD("[PQ_SERVICE] getTuningFlag[%x]", *TDSHPFlag);

    return NO_ERROR;
}

status_t PQService::setTDSHPFlag(int32_t TDSHPFlag)
{
    Mutex::Autolock _l(mLock);

    int32_t read_flag = 0;

    if (m_AshmemProxy->setTuningField(ASHMEM_TUNING_FLAG, TDSHPFlag) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] setTDSHPFlag : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    if (m_AshmemProxy->getTuningField(ASHMEM_TUNING_FLAG, &read_flag) == 0)
    {
        PQ_LOGD("[PQ_CMD] setTuningFlag[%x]\n", read_flag);
    }

    return NO_ERROR;
}


bool PQService::getCCorrCoefByIndex(int32_t coefTableIdx, uint32_t coef[3][3])
{
    unsigned int coef_sum = 0;

    PQ_LOGD("ccorr table index=%d", coefTableIdx);

    if (mCcorrDebug == true) { /* scenario debug mode */
        for (int y = 0; y < 3; y += 1) {
            for (int x = 0; x < 3; x += 1) {
                coef[y][x] = 0;
            }
        }
        int index = m_PQMode;
        if (m_PQScenario ==  SCENARIO_PICTURE)
            index += 0;
        else if (m_PQScenario ==  SCENARIO_VIDEO)
            index += 1;
        else
            index += 2;
        index = index % 3;
        coef[index][index] = 1024;
        coef_sum = 1024;
    } else { /* normal mode */
        for (int y = 0; y < 3; y += 1) {
            for (int x = 0; x < 3; x += 1) {
                coef[y][x] = m_pqindex.CCORR_COEF[coefTableIdx][y][x];
                coef_sum += coef[y][x];
            }
        }
    }

    if (coef_sum == 0) { /* zero coef protect */
        coef[0][0] = 1024;
        coef[1][1] = 1024;
        coef[2][2] = 1024;
        PQ_LOGD("ccorr coef all zero, prot on");
    }

    return true;
}


bool PQService::configCCorrCoef(int drvID, uint32_t coef[3][3])
{
    DISP_CCORR_COEF_T ccorr;

    ccorr.hw_id = DISP_CCORR0;
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            ccorr.coef[y][x] = coef[y][x];
        }
    }

    int ret = -1;
    ret = ioctl(drvID, DISP_IOCTL_SET_CCORR, &ccorr);
    if (ret == -1)
        PQ_LOGD("ccorr ioctl fail");

    return (ret == 0);
}


status_t PQService::configCCorrCoefByIndex(int32_t coefTableIdx, int32_t drvID)
{
#ifndef CCORR_OFF
    uint32_t coef[3][3];

    if (getCCorrCoefByIndex(coefTableIdx, coef))
        configCCorrCoef(drvID, coef);
#endif

    return NO_ERROR;
}


 status_t  PQService::setDISPScenario(int32_t scenario)
{
    Mutex::Autolock _l(mLock);
    m_PQScenario = scenario;
#ifndef DISP_COLOR_OFF
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    int percentage = 0;
    int scenario_index = getScenarioIndex(m_PQScenario);

    if (m_drvID < 0)
    {
        return UNKNOWN_ERROR;
    }

    PQ_LOGD("[PQ_SERVICE] PQService : m_PQScenario = %d, m_PQMode = %d\n",m_PQScenario,m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQParam(scenario_index);
        calcPQStrength(&m_pqparam, p_pqparam, getPQStrengthRatio(m_PQScenario));
    }
    else
    {
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid mode or scenario\n");
    }

    setPQParamWithFilter(m_drvID, &m_pqparam);
#endif

    return NO_ERROR;
}

status_t PQService::setFeatureSwitch(IPQService::PQFeatureID id, uint32_t value)
{
    Mutex::Autolock _l(mLock);

    uint32_t featureID = static_cast<PQFeatureID>(id);
    status_t ret = NO_ERROR;


    PQ_LOGD("[PQ_SERVICE] setFeatureSwitch(), feature[%d], value[%d]", featureID, value);

    switch (featureID) {
    case DISPLAY_COLOR:
        enableDisplayColor(value);
        break;
    case CONTENT_COLOR:
        enableContentColor(value);
        break;
    case CONTENT_COLOR_VIDEO:
        enableContentColorVideo(value);
        break;
    case SHARPNESS:
        enableSharpness(value);
        break;
    case DYNAMIC_CONTRAST:
        enableDynamicContrast(value);
        break;
    case DYNAMIC_SHARPNESS:
        enableDynamicSharpness(value);
        break;
    case DISPLAY_GAMMA:
        enableDisplayGamma(value);
        break;
    case DISPLAY_OVER_DRIVE:
        enableDisplayOverDrive(value);
        break;
    case ISO_ADAPTIVE_SHARPNESS:
        enableISOAdaptiveSharpness(value);
        break;
    case ULTRA_RESOLUTION:
        enableUltraResolution(value);
        break;

    default:
        PQ_LOGE("[PQ_SERVICE] setFeatureSwitch(), feature[%d] is not implemented!!", id);
        ret = UNKNOWN_ERROR;
        break;
    }
    return ret;
}

status_t PQService::getFeatureSwitch(IPQService::PQFeatureID id, uint32_t *value)
{
    Mutex::Autolock _l(mLock);

    uint32_t featureID = static_cast<PQFeatureID>(id);

    if (featureID < PQ_FEATURE_MAX)
    {
        *value = m_bFeatureSwitch[featureID];
        PQ_LOGD("[PQ_SERVICE] getFeatureSwitch(), feature[%d], value[%d]", featureID, *value);

        return NO_ERROR;
    }
    else
    {
        *value = 0;
        PQ_LOGE("[PQ_SERVICE] getFeatureSwitch(), unsupported feature[%d]", featureID);

        return UNKNOWN_ERROR;
    }
}

status_t PQService::enableDisplayColor(uint32_t value)
{
#ifndef DISP_COLOR_OFF
    int bypass;
    PQ_LOGD("[PQ_SERVICE] enableDisplayColor(), enable[%d]", value);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return UNKNOWN_ERROR ;

    }

    //  set bypass COLOR to disp driver.
    if (value > 0)
    {
        bypass = 0;
        ioctl(m_drvID, DISP_IOCTL_PQ_SET_BYPASS_COLOR, &bypass);
    }
    else
    {
        bypass = 1;
        ioctl(m_drvID, DISP_IOCTL_PQ_SET_BYPASS_COLOR, &bypass);
    }
    PQ_LOGD("[PQService] enableDisplayColor[%d]", value);

    m_bFeatureSwitch[DISPLAY_COLOR] = value;
#endif

    return NO_ERROR;
}

status_t PQService::enableContentColorVideo(uint32_t value)
{
#ifdef MDP_COLOR_ENABLE
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(VIDEO_CONTENT_COLOR_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableContentColorVideo : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_MDP_COLOR_EN_STR  , pvalue);
    PQ_LOGD("[PQService] enableContentColorVideo[%d]", value);

    m_bFeatureSwitch[CONTENT_COLOR_VIDEO] = value;
#endif

    return NO_ERROR;
}

status_t PQService::enableContentColor(uint32_t value)
{
#ifdef MDP_COLOR_ENABLE
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_MDP_COLOR_DBG_EN_STR  , pvalue);
    PQ_LOGD("[PQService] enableContentColor[%d]", value);

    m_bFeatureSwitch[CONTENT_COLOR] = value;
#endif

    return NO_ERROR;
}

status_t PQService::enableSharpness(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(SHP_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableSharpness : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_DBG_SHP_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableSharpness[%d]", value);

    m_bFeatureSwitch[SHARPNESS] = value;

    return NO_ERROR;
}

status_t PQService::enableDynamicContrast(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(DC_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableDynamicContrast : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_ADL_PROPERTY_STR, pvalue);
    PQ_LOGD("[PQService] enableDynamicContrast[%d]", value);

    m_bFeatureSwitch[DYNAMIC_CONTRAST] = value;

    return NO_ERROR;
}

status_t PQService::enableDynamicSharpness(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(DSHP_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableDynamicSharpness : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_DBG_DSHP_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableDynamicSharpness[%d]", value);

    m_bFeatureSwitch[DYNAMIC_SHARPNESS] = value;

    return NO_ERROR;
}

status_t PQService::enableDisplayGamma(uint32_t value)
{
    if (value > 0)
    {
        char pvalue[PROPERTY_VALUE_MAX];
        char dvalue[PROPERTY_VALUE_MAX];
        int index;

        snprintf(dvalue, PROPERTY_VALUE_MAX, "%d\n", GAMMA_INDEX_DEFAULT);
        property_get(GAMMA_INDEX_PROPERTY_NAME, pvalue, dvalue);
        index = atoi(pvalue);
        _setGammaIndex(index);
    }
    else
    {
        _setGammaIndex(GAMMA_INDEX_DEFAULT);
    }
    PQ_LOGD("[PQService] enableDisplayGamma[%d]", value);

    m_bFeatureSwitch[DISPLAY_GAMMA] = value;

    return NO_ERROR;
}

status_t PQService::enableDisplayOverDrive(uint32_t value)
{
    DISP_OD_CMD cmd;

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQService] enableDisplayOverDrive(), open device fail!!");
        return UNKNOWN_ERROR;
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.size = sizeof(cmd);
    cmd.type = 2;

    if (value > 0)
    {
        cmd.param0 = 1;
    }
    else
    {
        cmd.param0 = 0;
    }

    ioctl(m_drvID, DISP_IOCTL_OD_CTL, &cmd);

    PQ_LOGD("[PQService] enableDisplayOverDrive[%d]", value);

    m_bFeatureSwitch[DISPLAY_OVER_DRIVE] = value;

    return NO_ERROR;
}

status_t PQService::enableISOAdaptiveSharpness(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(ISO_SHP_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableISOAdaptiveSharpness : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_ISO_SHP_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableISOAdaptiveSharpness[%d]", value);

    m_bFeatureSwitch[ISO_ADAPTIVE_SHARPNESS] = value;

    return NO_ERROR;
}

status_t PQService::enableUltraResolution(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(UR_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableUltraResolution : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_ULTRARES_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableUltraResolution[%d]", value);

    m_bFeatureSwitch[ULTRA_RESOLUTION] = value;

    return NO_ERROR;
}

status_t PQService::enableBlueLight(bool enable)
{
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : enableBlueLight(%d)", (enable ? 1 : 0));

    blueLight->setEnabled(enable);

    refreshDisplay();

    return NO_ERROR;
}


status_t PQService::getBlueLightEnabled(bool *isEnabled)
{
    Mutex::Autolock _l(mLock);

    *isEnabled = blueLight->isEnabled();

    return NO_ERROR;
}


status_t PQService::setBlueLightStrength(int32_t strength)
{
    Mutex::Autolock _l(mLock);
    char value[PROPERTY_VALUE_MAX];

    PQ_LOGD("[PQ_SERVICE] PQService : setBlueLightStrength(%d)", strength);

    snprintf(value, PROPERTY_VALUE_MAX, "%d", strength);
    property_set(MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME, value);

    blueLight->setStrength(strength);

    refreshDisplay();

    return NO_ERROR;
}


status_t PQService::getBlueLightStrength(int32_t *strength)
{
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);

    property_get(MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME, value, "128");
    *strength = atoi(value);

    return NO_ERROR;
}


void PQService::initBlueLight()
{
    Mutex::Autolock _l(mLock);

    BluLightInitParam initParam;
    initParam.reserved = 0;
    blueLight->onInitCommon(initParam);
    blueLight->onInitPlatform(initParam);

    blueLight->setEnabled(false);

    char property[PROPERTY_VALUE_MAX];
    if (property_get(MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME, property, NULL) > 0) {
        int strength = (int)strtoul(property, NULL, 0);
        PQ_LOGD("[PQ_SERVICE] Blue-light init strength: %d", strength);
        blueLight->setStrength(strength);
    }
}


// Compose ColorRegisters by current scenario setting
bool PQService::composeColorRegisters(void *_colorReg, const DISP_PQ_PARAM *pqparam)
{
    bool result = false;
    ColorRegisters *colorReg = static_cast<ColorRegisters*>(_colorReg);
    const DISPLAY_PQ_T *pqTable = &m_pqindex;

    colorReg->GLOBAL_SAT = pqTable->GLOBAL_SAT[pqparam->u4SatGain];
    colorReg->CONTRAST = pqTable->CONTRAST[pqparam->u4Contrast];
    colorReg->BRIGHTNESS = pqTable->BRIGHTNESS[pqparam->u4Brightness];
    getCCorrCoefByIndex(pqparam->u4Ccorr, colorReg->CCORR_COEF);
    if (sizeof(colorReg->PARTIAL_Y) == sizeof(pqTable->PARTIAL_Y[0]) &&
        sizeof(colorReg->PURP_TONE_S) == sizeof(pqTable->PURP_TONE_S[0]) &&
        sizeof(colorReg->SKIN_TONE_S) == sizeof(pqTable->SKIN_TONE_S[0]) &&
        sizeof(colorReg->GRASS_TONE_S) == sizeof(pqTable->GRASS_TONE_S[0]) &&
        sizeof(colorReg->SKY_TONE_S) == sizeof(pqTable->SKY_TONE_S[0]) &&
        sizeof(colorReg->PURP_TONE_H) == sizeof(pqTable->PURP_TONE_H[0]) &&
        sizeof(colorReg->SKIN_TONE_H) == sizeof(pqTable->SKIN_TONE_H[0]) &&
        sizeof(colorReg->GRASS_TONE_H) == sizeof(pqTable->GRASS_TONE_H[0]) &&
        sizeof(colorReg->SKY_TONE_H) == sizeof(pqTable->SKY_TONE_H[0]))
    {
        memcpy(colorReg->PARTIAL_Y, pqTable->PARTIAL_Y[pqparam->u4PartialY], sizeof(colorReg->PARTIAL_Y));
        memcpy(colorReg->PURP_TONE_S, pqTable->PURP_TONE_S[pqparam->u4SatAdj[PURP_TONE]], sizeof(colorReg->PURP_TONE_S));
        memcpy(colorReg->SKIN_TONE_S, pqTable->SKIN_TONE_S[pqparam->u4SatAdj[SKIN_TONE]], sizeof(colorReg->SKIN_TONE_S));
        memcpy(colorReg->GRASS_TONE_S, pqTable->GRASS_TONE_S[pqparam->u4SatAdj[GRASS_TONE]], sizeof(colorReg->GRASS_TONE_S));
        memcpy(colorReg->SKY_TONE_S, pqTable->SKY_TONE_S[pqparam->u4SatAdj[SKY_TONE]], sizeof(colorReg->SKY_TONE_S));
        memcpy(colorReg->PURP_TONE_H, pqTable->PURP_TONE_H[pqparam->u4HueAdj[PURP_TONE]], sizeof(colorReg->PURP_TONE_H));
        memcpy(colorReg->SKIN_TONE_H, pqTable->SKIN_TONE_H[pqparam->u4HueAdj[SKIN_TONE]], sizeof(colorReg->SKIN_TONE_H));
        memcpy(colorReg->GRASS_TONE_H, pqTable->GRASS_TONE_H[pqparam->u4HueAdj[GRASS_TONE]], sizeof(colorReg->GRASS_TONE_H));
        memcpy(colorReg->SKY_TONE_H, pqTable->SKY_TONE_H[pqparam->u4HueAdj[SKY_TONE]], sizeof(colorReg->SKY_TONE_H));

        result = true;
    } else {
        PQ_LOGE("composeColorRegisters: Parameter size does not match (%d, %d) (%d, %d) (%d, %d)",
            sizeof(colorReg->PARTIAL_Y), sizeof(pqTable->PARTIAL_Y[0]),
            sizeof(colorReg->PURP_TONE_S), sizeof(pqTable->PURP_TONE_S[0]),
            sizeof(colorReg->SKIN_TONE_S), sizeof(pqTable->SKIN_TONE_S[0]));
        PQ_LOGE("composeColorRegisters: (%d, %d) (%d, %d) (%d, %d) (%d, %d) (%d, %d) (%d, %d)",
            sizeof(colorReg->GRASS_TONE_S), sizeof(pqTable->GRASS_TONE_S[0]),
            sizeof(colorReg->SKY_TONE_S), sizeof(pqTable->SKY_TONE_S[0]),
            sizeof(colorReg->PURP_TONE_H), sizeof(pqTable->PURP_TONE_H[0]),
            sizeof(colorReg->SKIN_TONE_H), sizeof(pqTable->SKIN_TONE_H[0]),
            sizeof(colorReg->GRASS_TONE_H), sizeof(pqTable->GRASS_TONE_H[0]),
            sizeof(colorReg->SKY_TONE_H), sizeof(pqTable->SKY_TONE_H[0]));
    }

    return result;
}


// Translate ColorRegisters to DISPLAY_COLOR_REG_T
bool PQService::translateColorRegisters(void *algoReg, DISPLAY_COLOR_REG_T *drvReg)
{
    size_t algoSize = offsetof(ColorRegisters, SKY_TONE_H) + sizeof(((ColorRegisters*)0)->SKY_TONE_H);
    size_t drvSize = offsetof(DISPLAY_COLOR_REG_T, SKY_TONE_H) + sizeof(drvReg->SKY_TONE_H);

    if (algoSize != drvSize) {
        PQ_LOGE("translateColorRegisters: Structure size changed SW:%u != DRV:%u",
            algoSize, drvSize);
        return false;
    }

    memcpy(drvReg, algoReg, sizeof(DISPLAY_COLOR_REG_T));

    return true;
}


// Convert _algoReg -> _tuningReg
void PQService::translateToColorTuning(void *_algoReg, void *_tuningReg)
{
    if (_algoReg == NULL || _tuningReg == NULL)
        return;

    ColorRegisters *algoReg = static_cast<ColorRegisters*>(_algoReg);
    ColorRegistersTuning *tuningReg = static_cast<ColorRegistersTuning*>(_tuningReg);

#define CONVERT_COPY(tuningReg, algoReg, field, type) \
    convertCopy(&(tuningReg->field type), sizeof(tuningReg->field), &(algoReg->field type), sizeof(algoReg->field))

    tuningReg->GLOBAL_SAT = algoReg->GLOBAL_SAT;
    tuningReg->CONTRAST = algoReg->CONTRAST;
    tuningReg->BRIGHTNESS = algoReg->BRIGHTNESS;
    CONVERT_COPY(tuningReg, algoReg, PARTIAL_Y, [0]);
    CONVERT_COPY(tuningReg, algoReg, PURP_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, SKIN_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, GRASS_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, SKY_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, PURP_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, SKIN_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, GRASS_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, SKY_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, CCORR_COEF, [0][0]);

#undef CONVERT_COPY
}


// Convert _algoReg <- _tuningReg
void PQService::translateFromColorTuning(void *_algoReg, void *_tuningReg)
{
    if (_algoReg == NULL || _tuningReg == NULL)
        return;

    ColorRegisters *algoReg = static_cast<ColorRegisters*>(_algoReg);
    ColorRegistersTuning *tuningReg = static_cast<ColorRegistersTuning*>(_tuningReg);

#define CONVERT_COPY(algoReg, tuningReg, field, type) \
    convertCopy(&(algoReg->field type), sizeof(algoReg->field), &(tuningReg->field type), sizeof(tuningReg->field))

    algoReg->GLOBAL_SAT = tuningReg->GLOBAL_SAT;
    algoReg->CONTRAST = tuningReg->CONTRAST;
    algoReg->BRIGHTNESS = tuningReg->BRIGHTNESS;
    CONVERT_COPY(algoReg, tuningReg, PARTIAL_Y, [0]);
    CONVERT_COPY(algoReg, tuningReg, PURP_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, SKIN_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, GRASS_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, SKY_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, PURP_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, SKIN_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, GRASS_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, SKY_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, CCORR_COEF, [0][0]);

#undef CONVERT_COPY
}


bool PQService::setPQParamWithFilter(int drvID, const DISP_PQ_PARAM *pqparam)
{
    int ret = -1;

    PQ_LOGD("[PQ_SERVICE] setPQParamWithFilter, gsat[%d], cont[%d], bri[%d] shp[%d]", pqparam->u4SatGain, pqparam->u4Contrast, pqparam->u4Brightness,pqparam->u4SHPGain);
    PQ_LOGD("[PQ_SERVICE] setPQParamWithFilter, hue0[%d], hue1[%d], hue2[%d], hue3[%d]", pqparam->u4HueAdj[0], pqparam->u4HueAdj[1], pqparam->u4HueAdj[2], pqparam->u4HueAdj[3]);
    PQ_LOGD("[PQ_SERVICE] setPQParamWithFilter, sat0[%d], sat1[%d], sat2[%d], sat3[%d]", pqparam->u4SatAdj[0], pqparam->u4SatAdj[1], pqparam->u4SatAdj[2], pqparam->u4SatAdj[3]);

#ifdef MTK_BLULIGHT_DEFENDER_SUPPORT
    DISPLAY_COLOR_REG_T *drvReg = new DISPLAY_COLOR_REG_T;
#ifdef COLOR_2_1
    const DISPLAY_PQ_T *pqTable = &m_pqindex;
#endif

    if (blueLight->isEnabled()) {
        ColorRegisters *inReg = new ColorRegisters;
        ColorRegisters *outReg = new ColorRegisters;

        if (composeColorRegisters(inReg, pqparam)) {
            if (mBLInputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                translateToColorTuning(inReg, mBLInput);
            } else if (mBLInputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                translateFromColorTuning(inReg, mBLInput);
            }

            dumpColorRegisters("Blue-light input:", inReg);
            blueLight->onCalculate(*inReg, outReg);
            dumpColorRegisters("Blue-light output:", outReg);

            if (mBLOutputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                translateToColorTuning(outReg, mBLOutput);
            } else if (mBLOutputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                translateFromColorTuning(outReg, mBLOutput);
            }

            if (translateColorRegisters(outReg, drvReg)) {
#ifdef COLOR_2_1
                memcpy(drvReg->S_GAIN_BY_Y, pqTable->S_GAIN_BY_Y, sizeof(drvReg->S_GAIN_BY_Y));
#endif
                ret = ioctl(drvID, DISP_IOCTL_SET_COLOR_REG, drvReg);
                configCCorrCoef(drvID, outReg->CCORR_COEF);
            }
        }

        delete inReg;
        delete outReg;
    } else {
        ColorRegisters *colorReg = new ColorRegisters;

        if (composeColorRegisters(colorReg, pqparam)) {
            if (translateColorRegisters(colorReg, drvReg)) {
#ifdef COLOR_2_1
                memcpy(drvReg->S_GAIN_BY_Y, pqTable->S_GAIN_BY_Y, sizeof(drvReg->S_GAIN_BY_Y));
#endif
                ret = ioctl(drvID, DISP_IOCTL_SET_COLOR_REG, drvReg);
                configCCorrCoef(drvID, colorReg->CCORR_COEF);
            }
        }

        delete colorReg;
    }

    delete drvReg;

    if (ret != 0) {
        PQ_LOGE("[PQ_SERVICE] setPQParamWithFilter: DISP_IOCTL_SET_COLOR_REG: ret = %d", ret);
    }

#else

    ret = ioctl(drvID, DISP_IOCTL_SET_PQPARAM, pqparam);
    PQ_LOGD("[PQ_SERVICE] setPQParamWithFilter: DISP_IOCTL_SET_PQPARAM: ret = %d", ret);
    configCCorrCoefByIndex(pqparam->u4Ccorr, drvID);

#endif

    return (ret == 0);
}

status_t PQService::setTuningField(int32_t module, int32_t field, int32_t value)
{
    bool isNoError = true;

    PQ_LOGD("[PQ_SERVICE] PQService : write module 0x%x, field 0x%x = 0x%x\n", module, field, value);

    switch (module) {
    case IPQService::MOD_DISPLAY:
        if (field == 0 && value == 0) {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_BLUE_LIGHT_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else if (field == 0xfffe && value == 0x0)
        {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        }
        else
        {
            if (blueLight->setTuningField(field, static_cast<unsigned int>(value)))
                return NO_ERROR;
        }
        break;
    case IPQService::MOD_BLUE_LIGHT_INPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mBLInput == NULL) {
                mBLInput = new ColorRegistersTuning;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mBLInputMode = static_cast<PQTuningMode>(value);
                if (mBLInputMode == PQ_TUNING_READING)
                    refreshDisplay();
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Blue-light input change tuning mode to %d\n", mBLInputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0){
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLInputMode == PQ_TUNING_OVERWRITTEN && mBLInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLInput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SERVICE] Blue-light input overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light: Not overwritten mode: %d\n", mBLInputMode);
            }
        }
        break;
    case IPQService::MOD_BLUE_LIGHT_OUTPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mBLOutput == NULL) {
                mBLOutput = new ColorRegistersTuning;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mBLOutputMode = static_cast<PQTuningMode>(value);
                if (mBLOutputMode == PQ_TUNING_READING)
                    refreshDisplay();
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Blue-light output change tuning mode to %d\n", mBLOutputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0) {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLOutputMode == PQ_TUNING_OVERWRITTEN && mBLOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLOutput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SERVICE] Blue-light output overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light: Not overwritten mode: %d\n", mBLOutputMode);
            }
        }
        break;
    case IPQService::MOD_DS_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DS_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DS_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DS_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DS_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DS_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DC_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DC_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DC_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DC_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DC_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DC_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_RSZ_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_RSZ_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_RSZ_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_RSZ_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_RSZ_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_RSZ_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_COLOR_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_COLOR_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_COLOR_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_COLOR_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_COLOR_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_COLOR_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_TDSHP_REG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_TDSHP_REG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_ULTRARESOLUTION:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_ULTRARESOLUTION, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DYNAMIC_CONTRAST:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DYNAMIC_CONTRAST, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    }
    return BAD_INDEX;
}

status_t PQService::getTuningField(int32_t module, int32_t field, int32_t *value)
{
    unsigned int uvalue;
    bool isNoError = true;

    switch (module) {
    case IPQService::MOD_DISPLAY:
        break;
    case IPQService::MOD_BLUE_LIGHT_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else
        {
            if (blueLight->getTuningField(field, &uvalue))
            {
                *value = static_cast<int32_t>(uvalue);
                return NO_ERROR;
            }
        }
        break;
    case IPQService::MOD_BLUE_LIGHT_INPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mBLInputMode;
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLInput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light input: Not reading mode: %d\n", mBLInputMode);
            }
        }
        break;
    case IPQService::MOD_BLUE_LIGHT_OUTPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mBLOutputMode;
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLOutput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light output: Not reading mode: %d\n", mBLOutputMode);
            }
        }
        break;
    case IPQService::MOD_DS_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DS_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DS_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DS_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DS_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DS_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DC_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DC_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DC_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DC_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DC_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DC_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_RSZ_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_RSZ_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_RSZ_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_RSZ_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_RSZ_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_RSZ_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_COLOR_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_COLOR_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_COLOR_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_COLOR_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_COLOR_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_COLOR_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_TDSHP_REG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_TDSHP_REG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_ULTRARESOLUTION:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_ULTRARESOLUTION, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case IPQService::MOD_DYNAMIC_CONTRAST:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DYNAMIC_CONTRAST, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    }

    *value = 0;
    return BAD_INDEX;
}


void PQService::refreshDisplay(void)
{
    setPQParamWithFilter(m_drvID, &m_pqparam);
}


void PQService::dumpColorRegisters(const char *prompt, void *_colorReg)
{
    ColorRegisters *colorReg = static_cast<ColorRegisters*>(_colorReg);

    if (mBlueLightDebugFlag & 0x1) {
        static const int buf_size = 512;
        char *buffer = new char[buf_size];

        PQ_LOGD("[PQ_SERVICE] %s", prompt);
        PQ_LOGD("[PQ_SERVICE] ColorRegisters: Sat:%d, Con:%d, Bri: %d",
            colorReg->GLOBAL_SAT, colorReg->CONTRAST, colorReg->BRIGHTNESS);

        #define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

        PQ_LOGD("[PQ_SERVICE] PARTIAL_Y:%s",
            printIntArray(buffer, buf_size, &colorReg->PARTIAL_Y[0], ARR_LEN(colorReg->PARTIAL_Y)));

        for (int i = 0; i < CLR_PQ_PARTIALS_CONTROL; i++) {
            PQ_LOGD("[PQ_SERVICE] PURP_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->PURP_TONE_S[i][0], ARR_LEN(colorReg->PURP_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] SKIN_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->SKIN_TONE_S[i][0], ARR_LEN(colorReg->SKIN_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] GRASS_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->GRASS_TONE_S[i][0], ARR_LEN(colorReg->GRASS_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] SKY_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->SKY_TONE_S[i][0], ARR_LEN(colorReg->SKY_TONE_S[0])));
        }

        PQ_LOGD("[PQ_SERVICE] PURP_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->PURP_TONE_H[0], ARR_LEN(colorReg->PURP_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] SKIN_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->SKIN_TONE_H[0], ARR_LEN(colorReg->SKIN_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] GRASS_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->GRASS_TONE_H[0], ARR_LEN(colorReg->GRASS_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] SKY_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->SKY_TONE_H[0], ARR_LEN(colorReg->SKY_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] CCORR_COEF:%s",
            printIntArray(buffer, buf_size, reinterpret_cast<unsigned int*>(colorReg->CCORR_COEF), 9));

        #undef ARR_LEN

        delete [] buffer;
    }
}

sp<IMemoryHeap> PQService::getAshmem(void)
{
    return mAshMemHeap;
}

status_t PQService::setGammaIndex(int32_t index)
{
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setGammaIndex(%d)", index);

    if (0 <= index && index < GAMMA_INDEX_MAX) {
        char value[PROPERTY_VALUE_MAX];
        snprintf(value, PROPERTY_VALUE_MAX, "%d", index);
        property_set(GAMMA_INDEX_PROPERTY_NAME, value);

        _setGammaIndex(index);
    }

    return NO_ERROR;
}

status_t PQService::getGammaIndex(int32_t *index)
{
    Mutex::Autolock _l(mLock);
    char value[PROPERTY_VALUE_MAX];
    int32_t p_index = 0;

    property_get(GAMMA_INDEX_PROPERTY_NAME, value, "-1");
    p_index = atoi(value);
    if (p_index < 0)
        p_index = GAMMA_INDEX_DEFAULT;
    *index = p_index;

    return NO_ERROR;
}

void PQService::onFirstRef()
{
    run("PQServiceMain", PRIORITY_DISPLAY);
    PQ_LOGD("[PQ_SERVICE] PQService : onFirstRef");
}

status_t PQService::readyToRun()
{
    PQ_LOGD("[PQ_SERVICE] PQService is ready to run.");
    return NO_ERROR;
}

bool PQService::initDriverRegs()
{
    return NO_ERROR;
}

bool PQService::initAshmem()
{
    mAshMemHeap = new MemoryHeapBase(ASHMEM_SIZE);
    unsigned int *base = (unsigned int *) mAshMemHeap->getBase();

    if (base == (unsigned int*)-1)
    {
        m_isAshmemInit = false;
        PQ_LOGE("[PQ_SERVICE] initAshmem failed\n");
    }
    else
    {
        m_isAshmemInit = true;
        memset(base, 0, ASHMEM_SIZE);
        /* set ashmem to PQ ashmem proxy */
        m_AshmemProxy->setAshmemBase(base);
        initPQProperty();
        PQ_LOGD("[PQ_SERVICE] initAshmem sccuess\n");
    }

    return m_isAshmemInit;
}

void PQService::initDefaultPQParam()
{
    /*cust PQ default setting*/
    m_pqparam_mapping =
    {
        image:80,
        video:100,
        camera:20,
    } ;

    DISP_PQ_PARAM pqparam_table_temp[PQ_PARAM_TABLE_SIZE] =
    {
        //std_image
        {
            u4SHPGain:2,
            u4SatGain:4,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{0,6,10,10},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        },

        //std_video
        {
            u4SHPGain:3,
            u4SatGain:4,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{0,6,12,12},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        },

        //std_camera
        {
            u4SHPGain:2,
            u4SatGain:4,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{0,6,10,10},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        },

        //viv_image
        {
            u4SHPGain:2,
            u4SatGain:9,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{16,16,16,16},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        },

        //viv_video
        {
            u4SHPGain:3,
            u4SatGain:9,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{16,16,18,18},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        },

        //viv_camera
        {
            u4SHPGain:2,
            u4SatGain:4,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{0,6,10,10},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        },

        //pqparam_usr
        {
            u4SHPGain:2,
            u4SatGain:9,
            u4PartialY:0,
            u4HueAdj:{9,9,12,12},
            u4SatAdj:{16,16,16,16},
            u4Contrast:4,
            u4Brightness:4,
            u4Ccorr:0
        }
    };
    memcpy(&m_pqparam_table, &pqparam_table_temp, sizeof(DISP_PQ_PARAM)*PQ_PARAM_TABLE_SIZE);

    m_pic_statndard = new PicModeStandard(&m_pqparam_table[0]);
    m_pic_vivid = new PicModeVivid(&m_pqparam_table[0]);
    m_pic_userdef = new PicModeUserDef(&m_pqparam_table[0]);

    g_PQ_DS_Param = {
    param:
        {1, -4, 1024, -4, 1024,
         1, 400, 200, 1600, 800,
         128, 8, 4, 12, 16,
         8, 24, -8, -4, -12,
         0, 0, 0,
         8, 4, 12, 16, 8, 24, -8, -4, -12,
         8, 4, 12, 16, 8, 24, -8, -4, -12,
         8, 4, 12, 8, 4, 12,
         1,
         4096, 2048, 1024, 34, 35, 51, 50,
         -1, -2, -4, 0, -4, 0, -1, -2, -1, -2}
    };

    g_PQ_DC_Param = {
    param:
      {
       1, 1, 0, 0, 0, 0, 0, 0, 0, 0x0A,
       0x30, 0x40, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
       0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
       0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80, 1}
    };

    g_tdshp_reg = {
    param:
        {0x10, 0x20, 0x10, 0x4, 0x2, 0x20, 0x3,
         0x2, 0x4, 0x10, 0x3, 0x2, 0x4, 0x10, 0x3,
         0x10, 0x10, 0x10, 0x8, 0x4,
         0}
    };


    rszparam = {
    param:
        {0, 10, 10, 1, 1, 1, 1, 1, 1,
         1, 24,
         1024, 2048, 4096, 3, 8, 0, 31,
         1024, 1536, 2048, 0, -7, 0, 15,
         4, 1024, 2048, 4096, 0, -2, 0, 255,
         8, 1024, 2048, 4096, 0, -7, 0, 31}

    };
}

bool PQService::threadLoop()
{
    char value[PROPERTY_VALUE_MAX];
    int i;
    int32_t  status;
    int percentage = m_pqparam_mapping.image;  //default scenario = image
    DISP_PQ_PARAM *p_pqparam = NULL;

    if (m_isAshmemInit == false)
    {
        if (initAshmem() == false)
        {
            return false;
        }
    }

    /* open the needed object */
    bool configDispColor = false;
    CustParameters &cust = CustParameters::getPQCust();
    if (!cust.isGood())
    {
        PQ_LOGD("[PQ_SERVICE] can't open libpq_cust.so, bypass init config\n");
    }
    /* find the address of function and data objects */
    else
    {

        configDispColor = loadPqparamMappingCoefficient() & loadPqparamTable()
                       & loadPqindexTable();
        loadGammaEntryTable();
        initBlueLight();
    }

    if (configDispColor)
    {
#ifndef DISP_COLOR_OFF

        PQ_LOGD("[PQ_SERVICE] DISP PQ init start...");

        if (m_drvID < 0)
        {
            PQ_LOGE("PQ device open failed!!");
        }

        // pq index
        ioctl(m_drvID, DISP_IOCTL_SET_PQINDEX, &m_pqindex);

        p_pqparam = &m_pqparam_table[0];
        if (m_PQMode == PQ_PIC_MODE_STANDARD || m_PQMode == PQ_PIC_MODE_VIVID)
        {
            if (m_PQMode == PQ_PIC_MODE_STANDARD) {
                p_pqparam = m_pic_statndard->getPQParam(0);
            } else if (m_PQMode == PQ_PIC_MODE_VIVID) {
                p_pqparam = m_pic_vivid->getPQParam(0);
            }
            memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
            //should be move out of #ifndef DISP_COLOR_OFF
            property_get(PQ_TDSHP_PROPERTY_STR, value, PQ_TDSHP_STANDARD_DEFAULT);
            property_set(PQ_TDSHP_PROPERTY_STR, value);
        }
        else if (m_PQMode == PQ_PIC_MODE_USER_DEF)
        {
            getUserModePQParam();

            p_pqparam = m_pic_userdef->getPQParam(0);
            calcPQStrength(&m_pqparam, p_pqparam, percentage);

            PQ_LOGD("[PQ_SERVICE] --Init_PQ_Userdef, gsat[%d], cont[%d], bri[%d] ", m_pqparam.u4SatGain, m_pqparam.u4Contrast, m_pqparam.u4Brightness);
            PQ_LOGD("[PQ_SERVICE] --Init_PQ_Userdef, hue0[%d], hue1[%d], hue2[%d], hue3[%d] ", m_pqparam.u4HueAdj[0], m_pqparam.u4HueAdj[1], m_pqparam.u4HueAdj[2], m_pqparam.u4HueAdj[3]);
            PQ_LOGD("[PQ_SERVICE] --Init_PQ_Userdef, sat0[%d], sat1[%d], sat2[%d], sat3[%d] ", m_pqparam.u4SatAdj[0], m_pqparam.u4SatAdj[1], m_pqparam.u4SatAdj[2], m_pqparam.u4SatAdj[3]);
        }
        else
        {
            memcpy(&m_pqparam, p_pqparam, sizeof(m_pqparam));
            PQ_LOGE("[PQ][main pq] main, property get... unknown pic_mode[%d]", m_PQMode);
        }

        setPQParamWithFilter(m_drvID, &m_pqparam);

        //status = ioctl(m_drvID, DISP_IOCTL_SET_TDSHPINDEX, &m_tdshpindex);
        //PQ_LOGD("[PQ_SERVICE] DISP_IOCTL_SET_TDSHPINDEX %d...",status);

        PQ_LOGD("[PQ_SERVICE] DISP PQ init end...");

#else // DISP_COLOR_OFF

#ifdef MTK_BLULIGHT_DEFENDER_SUPPORT
        // We need a default m_pqparam
        p_pqparam = &m_pqparam_table[0];
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
        setPQParamWithFilter(m_drvID, &m_pqparam);
#endif // MTK_BLULIGHT_DEFENDER_SUPPORT

#endif // DISP_COLOR_OFF
        PQ_LOGD("[PQ_SERVICE] threadLoop config User_Def PQ... end");
    }  // end of if (configDispColor)
    while(1)
    {
        sleep(10);
    }

    return true;
}

int PQService::_getLcmIndexOfGamma()
{
    static int lcmIdx = -1;

    if (lcmIdx == -1) {
        int ret = ioctl(m_drvID, DISP_IOCTL_GET_LCMINDEX, &lcmIdx);
        if (ret == 0) {
            if (lcmIdx < 0 || GAMMA_LCM_MAX <= lcmIdx) {
                PQ_LOGE("Invalid LCM index %d, GAMMA_LCM_MAX = %d", lcmIdx, GAMMA_LCM_MAX);
                lcmIdx = 0;
            }
        } else {
            PQ_LOGE("ioctl(DISP_IOCTL_GET_LCMINDEX) return %d", ret);
            lcmIdx = 0;
        }
    }

    PQ_LOGD("LCM index: %d/%d", lcmIdx, GAMMA_LCM_MAX);

    return lcmIdx;
}


void PQService::_setGammaIndex(int index)
{
    if (index < 0 || GAMMA_INDEX_MAX <= index)
        index = GAMMA_INDEX_DEFAULT;

    DISP_GAMMA_LUT_T *driver_gamma = new DISP_GAMMA_LUT_T;

    int lcm_id = _getLcmIndexOfGamma();

    const gamma_entry_t *entry = &(m_CustGamma[lcm_id][index]);
    driver_gamma->hw_id = DISP_GAMMA0;
    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
        driver_gamma->lut[i] = GAMMA_ENTRY((*entry)[0][i], (*entry)[1][i], (*entry)[2][i]);
    }

    ioctl(m_drvID, DISP_IOCTL_SET_GAMMALUT, driver_gamma);

    delete driver_gamma;
}

void PQService::configGamma(int picMode)
{
#if (GAMMA_LCM_MAX > 0) && (GAMMA_INDEX_MAX > 0)
    int lcmIndex = 0;
    int gammaIndex = 0;
#endif

#if GAMMA_LCM_MAX > 1
    lcmIndex = _getLcmIndexOfGamma();
#endif

#if GAMMA_INDEX_MAX > 1
    // get gamma index from runtime property configuration
    char property[PROPERTY_VALUE_MAX];

    gammaIndex = GAMMA_INDEX_DEFAULT;
    if (picMode == PQ_PIC_MODE_USER_DEF &&
            property_get(GAMMA_INDEX_PROPERTY_NAME, property, NULL) > 0 &&
            strlen(property) > 0)
    {
        gammaIndex = atoi(property);
    }

    if (gammaIndex < 0 || GAMMA_INDEX_MAX <= gammaIndex)
        gammaIndex = GAMMA_INDEX_DEFAULT;

    PQ_LOGD("Gamma index: %d/%d", gammaIndex, GAMMA_INDEX_MAX);
#endif

#if (GAMMA_LCM_MAX > 0) && (GAMMA_INDEX_MAX > 0)
    DISP_GAMMA_LUT_T *driverGamma = new DISP_GAMMA_LUT_T;

    const gamma_entry_t *entry = &(m_CustGamma[lcmIndex][gammaIndex]);
    driverGamma->hw_id = DISP_GAMMA0;
    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
        driverGamma->lut[i] = GAMMA_ENTRY((*entry)[0][i], (*entry)[1][i], (*entry)[2][i]);
    }

    ioctl(m_drvID, DISP_IOCTL_SET_GAMMALUT, driverGamma);

    delete driverGamma;
#endif
}

bool PQService::loadPqparamTable()
{
    CustParameters &cust = CustParameters::getPQCust();
    DISP_PQ_PARAM *pq_param_ptr;

    /* find the address of function and data objects */
    pq_param_ptr = (DISP_PQ_PARAM *)cust.getSymbol("pqparam_table");
    if (!pq_param_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqparam_table is not found in libpq_cust.so\n");
        return false;
    }
    else
    {
        memcpy(&m_pqparam_table[0], pq_param_ptr, sizeof(DISP_PQ_PARAM)*PQ_PARAM_TABLE_SIZE);
        return true;
    }
}

bool PQService::loadPqparamMappingCoefficient()
{
    CustParameters &cust = CustParameters::getPQCust();
    DISP_PQ_MAPPING_PARAM *pq_mapping_ptr;

    pq_mapping_ptr = (DISP_PQ_MAPPING_PARAM *)cust.getSymbol("pqparam_mapping");
    if (!pq_mapping_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqparam_mapping is not found in libpq_cust.so\n");
        return false;
    }
    else
    {
        memcpy(&m_pqparam_mapping, pq_mapping_ptr, sizeof(DISP_PQ_MAPPING_PARAM));
        return true;
    }
}

bool PQService::loadPqindexTable()
{
    CustParameters &cust = CustParameters::getPQCust();
    DISPLAY_PQ_T  *pq_table_ptr;

#ifdef MDP_COLOR_ENABLE
    pq_table_ptr = (DISPLAY_PQ_T *)cust.getSymbol("secondary_pqindex");
#else
    pq_table_ptr = (DISPLAY_PQ_T *)cust.getSymbol("primary_pqindex");
#endif

    if (!pq_table_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqindex is not found in libpq_cust.so\n");
        return false;
    }
    else
    {
        memcpy(&m_pqindex, pq_table_ptr, sizeof(DISPLAY_PQ_T));
        return true;
    }
}

void PQService::loadGammaEntryTable()
{
    CustParameters &cust = CustParameters::getPQCust();
    gamma_entry_t* ptr = (gamma_entry_t*)cust.getSymbol("cust_gamma");

    if (!ptr) {
        PQ_LOGD("[PQ_SERVICE] cust_gamma is not found in libpq_cust.so\n");
    }
    else
    {
        memcpy(m_CustGamma, ptr, sizeof(gamma_entry_t) * GAMMA_LCM_MAX * GAMMA_INDEX_MAX);
        configGamma(m_PQMode);
    }
}

void PQService::setDebuggingPqparam()
{
    if (mPQDebugFlag == 1) {
        m_pqparam_table[0].u4SHPGain = 2;
        m_pqparam_table[0].u4Brightness = 4;
        m_pqparam_table[1].u4SHPGain = 1;
        m_pqparam_table[1].u4Brightness = 6;
        m_pqparam_table[2].u4SHPGain = 0;
        m_pqparam_table[2].u4Brightness= 8;
    }
    else if (mPQDebugFlag == 0) {
        loadPqparamTable();
    }
}

void PQService::setSystemProperty(const char *feature, int32_t featureSwitch)
{
    char value[PROPERTY_VALUE_MAX];

    unsigned int *base = (unsigned int *) mAshMemHeap->getBase();

    if (base == (unsigned int*)-1)
    {
        PQ_LOGE("[PQ_SERVICE] setSystemProperty : mAshMemHeap->getBase() failed\n");
        return;
    }

    snprintf(value, PROPERTY_VALUE_MAX, "%d\n", featureSwitch);

    if (strcmp(feature, PQ_DBG_SHP_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(SHP_ENABLE, featureSwitch);
    }
    else if (strcmp(feature, PQ_DBG_DSHP_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(DSHP_ENABLE, featureSwitch);
    }
    else if (strcmp(feature, PQ_MDP_COLOR_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(VIDEO_CONTENT_COLOR_ENABLE, featureSwitch);
    }
    else if (strcmp(feature, PQ_MDP_COLOR_DBG_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(CONTENT_COLOR_ENABLE, featureSwitch);
    }
    else if (strcmp(feature, PQ_ADL_PROPERTY_STR) == 0)
    {
        m_AshmemProxy->setTuningField(DC_ENABLE, featureSwitch);
    }
    else if (strcmp(feature, PQ_ISO_SHP_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(ISO_SHP_ENABLE, featureSwitch);
        g_PQ_DS_Param.param[PQDS_iISO_en] = featureSwitch;
    }
    else if (strcmp(feature, PQ_ULTRARES_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(UR_ENABLE, featureSwitch);
        g_PQ_DS_Param.param[PQDS_iUltraRes_en] = featureSwitch;
        rszparam.param[RSZ_ultraResEnable] = featureSwitch;
    }
    else if (strcmp(feature, PQ_LOG_EN_STR) == 0)
    {
        m_AshmemProxy->setTuningField(PQ_LOG_ENABLE, featureSwitch);
    }
    else
    {
        PQ_LOGE("[PQ_SERVICE] setSystemProperty : Unknown Property [%s]\n", feature);
        return;
    }

    property_set(feature, value);
    PQ_LOGD("[PQ_SERVICE] setSystemProperty : feature  = %s, value = %s  \n", feature, value);
}

static const String16 sDump("android.permission.DUMP");
status_t PQService::dump(int fd, const Vector<String16>& args)
{
    static const size_t SIZE = 4096;
    char *buffer;
    String8 result;

    buffer = new char[SIZE];
    buffer[0] = '\0';

    PQ_LOGD("[PQ_SERVICE] PQService dump");
    if (!PermissionCache::checkCallingPermission(sDump)) {
        snprintf(buffer, SIZE, "Permission Denial: "
                "can't dump SurfaceFlinger from pid=%d, uid=%d\n",
                IPCThreadState::self()->getCallingPid(),
                IPCThreadState::self()->getCallingUid());
        result.append(buffer);
    } else {
        // Try to get the main lock, but don't insist if we can't
        // (this would indicate AALService is stuck, but we want to be able to
        // print something in dumpsys).
        int retry = 3;
        while (mLock.tryLock() < 0 && --retry >= 0) {
            usleep(500 * 1000);
        }
        const bool locked(retry >= 0);
        if (!locked) {
            snprintf(buffer, SIZE,
                    "PQService appears to be unresponsive, "
                    "dumping anyways (no locks held)\n");
            result.append(buffer);
        } else {
            size_t numArgs = args.size();
            for (size_t argIndex = 0; argIndex < numArgs; ) {
                if (args[argIndex] == String16("--ccorrdebug")) {
                    mCcorrDebug = true;
                    snprintf(buffer, SIZE, "CCORR Debug On");
                } else if (args[argIndex] == String16("--ccorrnormal")) {
                    mCcorrDebug = false;
                    snprintf(buffer, SIZE, "CCORR Debug Off");
                } else if (args[argIndex] == String16("--bl")) {
                    if (argIndex + 1 < numArgs) {
                        bool enabled = blueLight->isEnabled();
                        argIndex++;
                        if (args[argIndex] == String16("on") || args[argIndex] == String16("1"))
                            enabled = true;
                        else
                            enabled = false;
                        blueLight->setEnabled(enabled);
                        snprintf(buffer, SIZE, "Blue light: %s\n", (enabled ? "on" : "off"));
                        result.append(buffer);
                        snprintf(buffer, SIZE, "Strength: %d\n", blueLight->getStrength());
                        refreshDisplay();
                    } else {
                        snprintf(buffer, SIZE, "on/off?\n");
                    }
                } else if (args[argIndex] == String16("--bl_debug")) {
                    if (argIndex + 1 < numArgs) {
                        argIndex++;
                        mBlueLightDebugFlag = asInt(args[argIndex]);
                        blueLight->setDebugFlag(mBlueLightDebugFlag);
                        snprintf(buffer, SIZE, "Debug level = 0x%x\n", mBlueLightDebugFlag);
                    }
                } else if (args[argIndex] == String16("--pqparam_debug_on")) {
                    mPQDebugFlag = 1;
                    setDebuggingPqparam();
                    snprintf(buffer, SIZE, "pqparam_debug On\n");
                } else if (args[argIndex] == String16("--pqparam_debug_off")) {
                    mPQDebugFlag = 0;
                    setDebuggingPqparam();
                    snprintf(buffer, SIZE, "pqparam_debug Off\n");
                } else if (args[argIndex] == String16("--pqparam_debug_property")) {
                    char *feature;
                    int32_t feature_switch;

                    snprintf(buffer, SIZE, "Set pqparam_debug_property\n");

                    argIndex++;
                    // revised after 1st stage refactor => try using virtual class method
                    if (args[argIndex] == String16(PQ_DBG_SHP_EN_STR))
                    {
                        setSystemProperty(PQ_DBG_SHP_EN_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_DBG_DSHP_EN_STR))
                    {
                        setSystemProperty(PQ_DBG_DSHP_EN_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_MDP_COLOR_EN_STR))
                    {
                        setSystemProperty(PQ_MDP_COLOR_EN_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_MDP_COLOR_DBG_EN_STR))
                    {
                        setSystemProperty(PQ_MDP_COLOR_DBG_EN_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_ADL_PROPERTY_STR))
                    {
                        setSystemProperty(PQ_ADL_PROPERTY_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_ISO_SHP_EN_STR))
                    {
                        setSystemProperty(PQ_ISO_SHP_EN_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_ULTRARES_EN_STR))
                    {
                        setSystemProperty(PQ_ULTRARES_EN_STR, asInt(args[++argIndex]));
                    }
                    else if (args[argIndex] == String16(PQ_LOG_EN_STR))
                    {
                        setSystemProperty(PQ_LOG_EN_STR, asInt(args[++argIndex]));
                    }
                    else
                    {
                        snprintf(buffer, SIZE, "Set pqparam_debug_property failed\n");
                    }
                }
                argIndex += 1;
            }
            result.append(buffer);
        }

        if (locked) {
            mLock.unlock();
        }
    }

    write(fd, result.string(), result.size());

    delete [] buffer;

    return NO_ERROR;
}

int loadGlobalPQDCIndex(GLOBAL_PQ_INDEX_T *globalPQindex)
{
    CustParameters &cust = CustParameters::getPQCust();
    DISPLAY_DC_T *dcindex;

    /* find the address of function and data objects */
    dcindex = (DISPLAY_DC_T *)cust.getSymbol("dcindex");
    if (!dcindex) {
        PQ_LOGD("[PQ_SERVICE] dcindex is not found in libpq_cust.so\n");
        return false;
    }
    memcpy(&(globalPQindex->dcindex), dcindex, sizeof(DISPLAY_DC_T));

    return true;
}

void PQService::initGlobalPQ(void)
{
    char value[PROPERTY_VALUE_MAX];

    property_get(GLOBAL_PQ_SUPPORT_STR, value, GLOBAL_PQ_SUPPORT_DEFAULT);
    m_GlobalPQSupport = atoi(value);

    if (m_GlobalPQSupport == 0)
    {
        PQ_LOGD("Global PQ Not Support!");
        return;
    }

    PQ_LOGD("init Global PQ!");

    loadGlobalPQDCIndex(&m_GlobalPQindex);

    property_get(GLOBAL_PQ_ENABLE_STR, value, GLOBAL_PQ_ENABLE_DEFAULT);
    m_GlobalPQSwitch = atoi(value);

    property_get(GLOBAL_PQ_STRENGTH_STR, value, GLOBAL_PQ_STRENGTH_DEFAULT);
    m_GlobalPQStrength = atoi(value);

    m_GlobalPQStrengthRange =
        (GLOBAL_PQ_VIDEO_SHARPNESS_STRENGTH_RANGE << 0 ) |
        (GLOBAL_PQ_VIDEO_DC_STRENGTH_RANGE << 8 ) |
        (GLOBAL_PQ_UI_SHARPNESS_STRENGTH_RANGE << 16 ) |
        (GLOBAL_PQ_UI_DC_STRENGTH_RANGE << 24 );

}

status_t PQService::setGlobalPQSwitch(int32_t globalPQSwitch)
{
    Mutex::Autolock _l(mLock);

    if (m_GlobalPQSupport == 0)
    {
        PQ_LOGD("Global PQ Not Support!");
        return UNKNOWN_ERROR;
    }

    PQ_LOGD("[PQ_SERVICE] PQService : setGlobalPQSwitch(%d)", globalPQSwitch);

    char value[PROPERTY_VALUE_MAX];
    snprintf(value, PROPERTY_VALUE_MAX, "%d", globalPQSwitch);
    property_set(GLOBAL_PQ_ENABLE_STR, value);

    m_GlobalPQSwitch = globalPQSwitch;

    return NO_ERROR;
}

status_t PQService::getGlobalPQSwitch(int32_t *globalPQSwitch)
{
    Mutex::Autolock _l(mLock);

    *globalPQSwitch = m_GlobalPQSwitch;

    return NO_ERROR;
}

status_t PQService::setGlobalPQStrength(uint32_t globalPQStrength)
{
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setGlobalPQStrength(%d)", globalPQStrength);

    char value[PROPERTY_VALUE_MAX];

    snprintf(value, PROPERTY_VALUE_MAX, "%d", globalPQStrength);
    property_set(GLOBAL_PQ_STRENGTH_STR, value);

    m_GlobalPQStrength = globalPQStrength;

    return NO_ERROR;
}

status_t PQService::getGlobalPQStrength(uint32_t *globalPQStrength)
{
    Mutex::Autolock _l(mLock);

    *globalPQStrength = m_GlobalPQStrength;

    return NO_ERROR;
}

status_t PQService::getGlobalPQStrengthRange(uint32_t *globalPQStrengthRange)
{
    Mutex::Autolock _l(mLock);

    *globalPQStrengthRange = m_GlobalPQStrengthRange;

    return NO_ERROR;
}

status_t PQService::getGlobalPQIndex(GLOBAL_PQ_INDEX_T *globalPQindex)
{
    Mutex::Autolock _l(mLock);

    memcpy(m_GlobalPQindex.dcindex.entry[GLOBAL_PQ_DC_INDEX_MAX - 1], &(g_PQ_DC_Param.param[0]), sizeof(g_PQ_DC_Param));
    memcpy(globalPQindex, &m_GlobalPQindex, sizeof(m_GlobalPQindex));

    return NO_ERROR;
}

};
