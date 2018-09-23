#define LOG_TAG "PQ-Test"

#include <cstdlib>
#include <iostream>
#include <cust_color.h>
#include <PQCommon.h>
#include <PQClient.h>
#include "cust_tdshp.h"
#include "gtest/gtest.h"

using namespace std;
using namespace android;

#define PQ_LOGD(fmt, arg...) ALOGD(fmt, ##arg)
#define PQ_LOGE(fmt, arg...) ALOGE(fmt, ##arg)

#define CHECK(stat) \
    do { \
        if (!(stat)) { \
            cerr << __func__ << ":" << __LINE__ << endl; \
            cerr << "    " << #stat << " failed." << endl; \
        } \
    } while (0)

#define CASE_DONE() //do { cout << __func__ << " done." << endl; } while (0)


#define CLR_PARTIAL_Y_SIZE 16
#define CLR_PQ_PARTIALS_CONTROL 5
#define CLR_PURP_TONE_SIZE 3
#define CLR_SKIN_TONE_SIZE 8
#define CLR_GRASS_TONE_SIZE 6
#define CLR_SKY_TONE_SIZE 3

#define PQ_REFACTORING
#define OUT_OF_UPPERBOUND_ONE 0

bool checkResult(bool stat)
{
    if(!stat)
        return 1;
    else
        return 0;
}

int32_t getPqparam_table(DISP_PQ_PARAM *pqparam_table)
{
    DISP_PQ_PARAM *pq_table_ptr;

    /* open the needed object */
    CustParameters &cust = CustParameters::getPQCust();
    if (!cust.isGood()) {
        PQ_LOGD("[PQ_SERVICE] can't open libpq_cust.so\n");
        return 1;
    }

    /* find the address of function and data objects */
    pq_table_ptr = (DISP_PQ_PARAM *)cust.getSymbol("pqparam_table");
    if (!pq_table_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqparam_table is not found in libpq_cust.so\n");
        return 1;
    }

    memcpy(pqparam_table, pq_table_ptr, sizeof(DISP_PQ_PARAM) * PQ_PARAM_TABLE_SIZE);

    return 0;
}

int32_t getPqparam_mapping(DISP_PQ_MAPPING_PARAM *pqparam_mapping)
{
    DISP_PQ_MAPPING_PARAM *pq_mapping_ptr;

    /* open the needed object */
    CustParameters &cust = CustParameters::getPQCust();
    if (!cust.isGood()) {
        PQ_LOGD("[PQ_SERVICE] can't open libpq_cust.so\n");
        return 1;
    }
    /* find the address of function and data objects */

    pq_mapping_ptr = (DISP_PQ_MAPPING_PARAM *)cust.getSymbol("pqparam_mapping");
    if (!pq_mapping_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqparam_mapping is not found in libpq_cust.so\n");
        return 1;
    }
    memcpy(pqparam_mapping, pq_mapping_ptr, sizeof(DISP_PQ_MAPPING_PARAM));

    return 0;
}

int32_t getPQStrengthPercentage(int PQScenario, DISP_PQ_MAPPING_PARAM *pqparam_mapping)
{
    if (PQScenario ==  PQClient::SCENARIO_PICTURE) {
        return pqparam_mapping->image;
    }
    else  if (PQScenario == PQClient::SCENARIO_VIDEO) {
        return pqparam_mapping->video;
    }
    else  if (PQScenario == PQClient::SCENARIO_ISP_PREVIEW) {
        return pqparam_mapping->camera;
    }
    else {
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid scenario\n");
        return pqparam_mapping->image;
    }
}

void calcPQStrength(DISP_PQ_PARAM *pqparam_dst, DISP_PQ_PARAM *pqparam_src, int percentage)
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

void LoadUserModePQParam(DISP_PQ_PARAM *pqparam)
{
    char value[PROPERTY_VALUE_MAX];
    int i;

    property_get(PQ_TDSHP_PROPERTY_STR, value, PQ_TDSHP_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... tdshp[%d]", i);
    pqparam->u4SHPGain = i;

    property_get(PQ_GSAT_PROPERTY_STR, value, PQ_GSAT_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... gsat[%d]", i);
    pqparam->u4SatGain = i;

    property_get(PQ_CONTRAST_PROPERTY_STR, value, PQ_CONTRAST_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... contrast[%d]", i);
    pqparam->u4Contrast = i;

    property_get(PQ_PIC_BRIGHT_PROPERTY_STR, value, PQ_PIC_BRIGHT_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... pic bright[%d]", i);
    pqparam->u4Brightness = i;
}

int32_t getScenarioIndex(int32_t scenario)
{
    int32_t scenario_index;

    if (scenario ==  PQClient::SCENARIO_PICTURE) {
        scenario_index = 0;
    }
    else  if (scenario == PQClient::SCENARIO_VIDEO) {
        scenario_index = 1;
    }
    else  if (scenario == PQClient::SCENARIO_ISP_PREVIEW) {
        scenario_index = 2;
    }
    else {
        scenario_index = 0;
    }

    return scenario_index;
}
bool test_BluLightTuning()
{
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
    #define RegOffset(field) (unsigned int)((char*)&( ((ColorRegistersTuning*)(0))->field) - (char*)(0))


    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t value;
    bool err = 0;

    client.enableBlueLight(true);

    // Reading mode on/off
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 1);
    err |= checkResult(ret == 0);
    ret = client.getTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, &value);
    err |= checkResult(ret == 0);
    err |= checkResult(value == 1);

    // Now it is in reading mode, we verify the reading functionality
    client.setTuningField(IPQService::MOD_DISPLAY, 0x0, 0);
    // Read out CCORR_COEF
    static const size_t ccorrBase = RegOffset(CCORR_COEF);
    for (size_t i = 0; i < 9; i++) {
        client.getTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, ccorrBase + 4 * i, &value);
        cout << value << " ";
    }
    cout << endl;

    // Test input overwritten, we should observe on display panel
    static const int CCORR[9] = { 512, 0, 0,   0, 1023, 0,   0, 0, 1023 };
    for (size_t i = 0; i < 9; i++) {
        client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, ccorrBase + 4 * i, CCORR[i]);
    }
    client.setTuningField(IPQService::MOD_DISPLAY, 0x0, 0);

    // Error handing: access non-aligned address
    ret = client.getTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0x3, &value);
    err |= checkResult(ret != 0);
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0x5, 0);
    err |= checkResult(ret != 0);

    // Error handing: invalid mode
    client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 1);
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 10);
    err |= checkResult(ret != 0);
    ret = client.getTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, &value);
    err |= checkResult(value == 1); // Should keep previous mode

    // Error handing: write in reading mode should be forbidden
    client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 1);
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, RegOffset(GLOBAL_SAT), 512);
    err |= checkResult(ret != 0);

    #undef RegOffset

    CASE_DONE();

    return err;
}


bool test_CustParameters()
{
    CustParameters &cust = CustParameters::getPQCust();
    int var = 0;
    bool err = 0;

    if (cust.isGood()) {
        err |= checkResult(cust.loadVar("BaseOnly", &var));
        err |= checkResult(var == 1111);

        err |= checkResult(cust.loadVar("ProjectOnly", &var));
        err |= checkResult(var == 2222);

        err |= checkResult(cust.loadVar("ProjectOverwritten", &var));
        err |= checkResult(var == 2222);
    } else {
        cerr << "Load libpq_cust*.so failed." << endl;
    }

    CASE_DONE();
    return err;
}
#ifdef PQ_REFACTORING
bool test_setPQMode(volatile int32_t mode)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    bool err = 0;

    char value[PROPERTY_VALUE_MAX];
    cout << "mode =" << mode  << endl;
    ret = client.setPQMode(mode);
    err |=  checkResult(ret == 0);
    CHECK(ret == 0);
    property_get(PQ_PIC_MODE_PROPERTY_STR, value, PQ_PIC_MODE_DEFAULT);
    err |=  checkResult(atoi(value) == mode);
    CHECK(atoi(value) == mode);
    CASE_DONE();
    return err;
}

bool test_setPQIndex(volatile int32_t level, volatile int32_t  scenario, volatile int32_t  tuning_mode, volatile int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    DISP_PQ_PARAM pqparam_test;
    bool err = 0;

    cout << " index = " << index << " ,level = " << level << endl;

    ret = client.setPQIndex(level, scenario, tuning_mode, index);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    //ret = client.getMappedColorIndex(&pqparam_test, scenario, 0);// mode parameter is not used anymore
    //err |= checkResult(ret == 0);

    CASE_DONE();
    return err;
}

bool test_getMappedPQIndex(volatile int32_t scenario, volatile int32_t mode)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t dlopen_error;
    int32_t scenario_index;
    bool err = 0;

    DISP_PQ_PARAM pqparam_test;
    DISP_PQ_PARAM pqparam_user_def;
    DISP_PQ_PARAM pqparam_table[PQ_PARAM_TABLE_SIZE];
    DISP_PQ_MAPPING_PARAM pqparam_mapping;

    cout <<  " scenario = " << scenario << " mode = " << mode << endl;

    //int pqparam_table_index = (mode < PQ_PIC_MODE_USER_DEF)? (mode) * PQ_SCENARIO_COUNT + scenario : PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT
    dlopen_error = getPqparam_table(&pqparam_table[0]);
    err |= checkResult(dlopen_error == 0);
    CHECK(dlopen_error == 0);
    LoadUserModePQParam(&pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT]);

    dlopen_error = getPqparam_mapping(&pqparam_mapping);
    err |= checkResult(dlopen_error == 0);
    CHECK(dlopen_error == 0);
    ret = client.getMappedColorIndex(&pqparam_test, scenario, mode);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    ret = client.getMappedTDSHPIndex(&pqparam_test, scenario, mode);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    scenario_index = getScenarioIndex(scenario);

    if(mode == PQ_PIC_MODE_STANDARD || mode == PQ_PIC_MODE_VIVID)
    {
        err |= checkResult(0 == memcmp(&pqparam_test, &pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], sizeof(DISP_PQ_PARAM) - sizeof(unsigned int)));
        CHECK(0 == memcmp(&pqparam_test, &pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], sizeof(DISP_PQ_PARAM) - sizeof(unsigned int)));
        cout << "pqparam_table_index " << (mode) * PQ_SCENARIO_COUNT + scenario_index << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], shp " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SHPGain << ",gsat " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatGain << ",cont " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4Contrast << ",bri " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4Brightness << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], hue0 " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[0] << ",hue1 "  << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[1]  << ",hue2 " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[2]  << ",hue3 "<< pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[3] << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], sat0 " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[0] << ",sat1 "  << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[1]  << ",sat2 " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[2]  << ",sat3 "<< pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[3] << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], partialY" << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4PartialY   << endl;

        cout << "pqparam_test, shp " << pqparam_test.u4SHPGain << ",gsat " << pqparam_test.u4SatGain << ",cont " << pqparam_test.u4Contrast << ",bri " << pqparam_test.u4Brightness << endl;
        cout << "pqparam_test, hue0 " << pqparam_test.u4HueAdj[0] << ",hue1 "  << pqparam_test.u4HueAdj[1]  << ",hue2 " << pqparam_test.u4HueAdj[2]  << ",hue3 "<< pqparam_test.u4HueAdj[3] << endl;
        cout << "pqparam_test, sat0 " << pqparam_test.u4SatAdj[0] << ",sat1 "  << pqparam_test.u4SatAdj[1]  << ",sat2 " << pqparam_test.u4SatAdj[2]  << ",sat3 "<< pqparam_test.u4SatAdj[3] << endl;
        cout << "pqparam_test, partialY" << pqparam_test.u4PartialY  <<  endl;

    }
    else if ( mode == PQ_PIC_MODE_USER_DEF)
    {
        calcPQStrength(&pqparam_user_def, &pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT], getPQStrengthPercentage(scenario, &pqparam_mapping));
        err |= checkResult(0 == memcmp(&pqparam_test, &pqparam_user_def, sizeof(DISP_PQ_PARAM) - sizeof(unsigned int)));
        CHECK(0 == memcmp(&pqparam_test, &pqparam_user_def, sizeof(DISP_PQ_PARAM) - sizeof(unsigned int)));

        cout << "pqparam_user_def, shp " << pqparam_user_def.u4SHPGain << ",gsat " << pqparam_user_def.u4SatGain << ",cont " << pqparam_user_def.u4Contrast << ",bri " << pqparam_user_def.u4Brightness << endl;
        cout << "pqparam_user_def, hue0" << pqparam_user_def.u4HueAdj[0] << ",hue1 "  << pqparam_user_def.u4HueAdj[1]  << ",hue2 " << pqparam_user_def.u4HueAdj[2]  << ",hue3 "<< pqparam_user_def.u4HueAdj[3] << endl;
        cout << "pqparam_user_def, sat0" << pqparam_user_def.u4SatAdj[0] << ",sat1 "  << pqparam_user_def.u4SatAdj[1]  << ",sat2 " << pqparam_user_def.u4SatAdj[2]  << ",sat3 "<< pqparam_user_def.u4SatAdj[3] << endl;
        cout << "pqparam_user_def, partialY" << pqparam_user_def.u4PartialY  << endl;

        cout << "pqparam_test, shp " << pqparam_test.u4SHPGain << ",gsat " << pqparam_test.u4SatGain << ",cont " << pqparam_test.u4Contrast << ",bri " << pqparam_test.u4Brightness << endl;
        cout << "pqparam_test, hue0 " << pqparam_test.u4HueAdj[0] << ",hue1 "  << pqparam_test.u4HueAdj[1]  << ",hue2 " << pqparam_test.u4HueAdj[2]  << ",hue3 "<< pqparam_test.u4HueAdj[3] << endl;
        cout << "pqparam_test, sat0 " << pqparam_test.u4SatAdj[0] << ",sat1 "  << pqparam_test.u4SatAdj[1]  << ",sat2 " << pqparam_test.u4SatAdj[2]  << ",sat3 "<< pqparam_test.u4SatAdj[3] << endl;
        cout << "pqparam_test, partialY" << pqparam_test.u4PartialY  << endl;

    }

    CASE_DONE();
    return err;
}

bool test_getPQIndex(volatile int32_t scenario,volatile int32_t mode)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t dlopen_error;
    int32_t scenario_index;
    bool err = 0;

    cout <<  " scenario = " << scenario << " mode = " << mode << endl;

    DISP_PQ_PARAM pqparam_test;
    DISP_PQ_PARAM pqparam_user_def;
    DISP_PQ_PARAM pqparam_table[PQ_PARAM_TABLE_SIZE];
    DISP_PQ_MAPPING_PARAM pqparam_mapping;

    scenario_index = getScenarioIndex(scenario);

    int pqparam_table_index = (mode < PQ_PIC_MODE_USER_DEF)? (mode) * PQ_SCENARIO_COUNT + scenario_index : PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT;

    dlopen_error = getPqparam_table(&pqparam_table[0]);
    err |= checkResult(dlopen_error == 0);
    CHECK(dlopen_error == 0);
    LoadUserModePQParam(&pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT]);
    cout <<  " scenario = " << scenario << " mode = " << mode << " scenario_index = " << scenario_index  <<  endl;

    ret = client.setPQMode(mode);
    err |=  checkResult(ret == 0);
    CHECK(ret == 0);

    ret = client.getColorIndex(&pqparam_test, scenario, mode);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    ret = client.getTDSHPIndex(&pqparam_test, scenario, mode);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    err |= checkResult(0 == memcmp(&pqparam_test, &pqparam_table[pqparam_table_index], sizeof(DISP_PQ_PARAM) - sizeof(unsigned int))); // discard ccorr
    CHECK(0 == memcmp(&pqparam_test, &pqparam_table[pqparam_table_index], sizeof(DISP_PQ_PARAM) - sizeof(unsigned int))); // discard ccorr
    cout << "pqparam_table_index " << pqparam_table_index << endl;
    cout << "pqparam_table[pqparam_table_index], shp " << pqparam_table[pqparam_table_index].u4SHPGain << ",gsat " << pqparam_table[pqparam_table_index].u4SatGain << ",cont " << pqparam_table[pqparam_table_index].u4Contrast << ",bri " << pqparam_table[pqparam_table_index].u4Brightness << endl;
    cout << "pqparam_table[pqparam_table_index], hue0 " << pqparam_table[pqparam_table_index].u4HueAdj[0] << ",hue1 "  << pqparam_table[pqparam_table_index].u4HueAdj[1]  << ",hue2 " << pqparam_table[pqparam_table_index].u4HueAdj[2]  << ",hue3 "<< pqparam_table[pqparam_table_index].u4HueAdj[3] << endl;
    cout << "pqparam_table[pqparam_table_index], sat0 " << pqparam_table[pqparam_table_index].u4SatAdj[0] << ",sat1 "  << pqparam_table[pqparam_table_index].u4SatAdj[1]  << ",sat2 " << pqparam_table[pqparam_table_index].u4SatAdj[2]  << ",sat3 "<< pqparam_table[pqparam_table_index].u4SatAdj[3] << endl;
    cout << "pqparam_table[pqparam_table_index], partialY" << pqparam_table[pqparam_table_index].u4PartialY  << endl;


    cout << "pqparam_test, shp " << pqparam_test.u4SHPGain << ",gsat " << pqparam_test.u4SatGain << ",cont " << pqparam_test.u4Contrast << ",bri " << pqparam_test.u4Brightness << endl;
    cout << "pqparam_test, hue0 " << pqparam_test.u4HueAdj[0] << ",hue1 "  << pqparam_test.u4HueAdj[1]  << ",hue2 " << pqparam_test.u4HueAdj[2]  << ",hue3 "<< pqparam_test.u4HueAdj[3] << endl;
    cout << "pqparam_test, sat0 " << pqparam_test.u4SatAdj[0] << ",sat1 "  << pqparam_test.u4SatAdj[1]  << ",sat2 " << pqparam_test.u4SatAdj[2]  << ",sat3 "<< pqparam_test.u4SatAdj[3] << endl;
    cout << "pqparam_test, partialY" << pqparam_test.u4PartialY  << endl;
    CASE_DONE();
    return err;
}

bool test_PQDCIndex(volatile int32_t level, volatile int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    DISP_PQ_DC_PARAM dcparam;
    int32_t value;
    bool err = 0;

    cout <<  " index = " << index <<  endl;

    ret = client.setPQDCIndex(level,index);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    ret = client.getPQDCIndex(&dcparam, index);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(dcparam.param[index] == level);
    CHECK(dcparam.param[index] == level);
    ret = client.getTuningField(IPQService::MOD_DYNAMIC_CONTRAST, index, &value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(value == level);
    CHECK(value == level);

    ret = client.setTuningField(IPQService::MOD_DYNAMIC_CONTRAST, index, level + 1);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    ret = client.getPQDCIndex(&dcparam, index);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(dcparam.param[index] == level + 1);
    CHECK(dcparam.param[index] == level + 1);
    ret = client.getTuningField(IPQService::MOD_DYNAMIC_CONTRAST, index, &value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(value == level + 1);
    CHECK(value == level + 1);
    CASE_DONE();
    return err;
}

bool test_PQDSIndex(volatile int32_t level, volatile int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    DISP_PQ_DS_PARAM_EX dsparam;
    int32_t value;
    bool err = 0;

    cout <<  " index = " << index <<  endl;

    ret = client.setTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, index, level);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    ret = client.getPQDSIndex(&dsparam);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(dsparam.param[index] == level);
    CHECK(dsparam.param[index] == level);
    ret = client.getTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, index, &value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(value == level);
    CHECK(value == level);

    CASE_DONE();
    return err;
}

bool test_TuningFlag(volatile int32_t FlagEnum)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t  TuninfFlag;
    bool err = 0;

    cout <<  " Flag = " << FlagEnum <<  endl;

    ret = client.setTDSHPFlag(FlagEnum);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    ret = client.getTDSHPFlag(&TuninfFlag);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(TuninfFlag == FlagEnum);
    CHECK(TuninfFlag == FlagEnum);

    CASE_DONE();
    return err;
}

bool test_setDISPScenario(volatile int32_t scenario)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    bool err = 0;

    cout <<  " scenario = " << scenario <<  endl;

    ret = client.setDISPScenario(scenario);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    CASE_DONE();
    return err;
}

bool test_FeatureSwitch(volatile IPQService::PQFeatureID id, volatile uint32_t value)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    uint32_t test_value;
    bool err = 0;

    cout <<  " PQFeatureID = " << id <<  " value = " << value <<  endl;

    ret = client.setFeatureSwitch(id, value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    ret = client.getFeatureSwitch(id, &test_value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    err |= checkResult(test_value == value);
    CHECK(test_value == value);

    CASE_DONE();
    return err;
}

bool test_TDSHPReg(volatile int32_t level, volatile int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t value;
    MDP_TDSHP_REG_EX tdshp_reg;
    bool err = 0;

    cout <<  " index = " << index <<  endl;

    ret = client.setTuningField(IPQService::MOD_TDSHP_REG, index, level);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    client.getTDSHPReg(&tdshp_reg);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(tdshp_reg.param[index] == level);
    CHECK(tdshp_reg.param[index] == level);
    ret = client.getTuningField(IPQService::MOD_TDSHP_REG, index, &value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(value == level);
    CHECK(value == level);

    CASE_DONE();
    return err;
}

bool test_UltraResolutionReg(volatile int32_t level, volatile int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t value;
    bool err = 0;

    cout <<  " index = " << index <<  endl;

    ret = client.setTuningField(IPQService::MOD_ULTRARESOLUTION, index, level);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);

    ret = client.getTuningField(IPQService::MOD_ULTRARESOLUTION, index, &value);
    err |= checkResult(ret == 0);
    CHECK(ret == 0);
    err |= checkResult(value == level);
    CHECK(value == level);

    CASE_DONE();
    return err;
}
#endif


TEST (PQTest, PQ_001)
{
    EXPECT_EQ(0,test_setDISPScenario(PQClient::SCENARIO_PICTURE));
    EXPECT_EQ(0,test_setDISPScenario(PQClient::SCENARIO_ISP_PREVIEW));

    EXPECT_EQ(0,test_getPQIndex(PQClient::SCENARIO_PICTURE, PQ_PIC_MODE_STANDARD));
    EXPECT_EQ(0,test_getPQIndex(PQClient::SCENARIO_PICTURE, PQ_PIC_MODE_VIVID));
    EXPECT_EQ(0,test_getPQIndex(PQClient::SCENARIO_PICTURE, PQ_PIC_MODE_USER_DEF));
    EXPECT_EQ(0,test_getPQIndex(PQClient::SCENARIO_PICTURE, PQ_PIC_MODE_STANDARD));
    EXPECT_EQ(0,test_getPQIndex(PQClient::SCENARIO_ISP_PREVIEW, PQ_PIC_MODE_VIVID));
    EXPECT_EQ(0,test_getPQIndex(PQClient::SCENARIO_ISP_PREVIEW, PQ_PIC_MODE_USER_DEF));
}

TEST (PQTest, PQ_002)
{
    EXPECT_EQ(0,test_setPQMode(PQ_PIC_MODE_STANDARD));
    EXPECT_EQ(0,test_setPQMode(PQ_PIC_MODE_VIVID));
    EXPECT_EQ(0,test_setPQMode(PQ_PIC_MODE_USER_DEF));
}

TEST (PQTest, PQ_003)
{
    int i,j;

    for (i = SET_PQ_SHP_GAIN; i <= SET_PQ_BRIGHTNESS + OUT_OF_UPPERBOUND_ONE; i++)
    {
        EXPECT_EQ(0,test_setPQIndex(9, PQClient::SCENARIO_PICTURE, TDSHP_FLAG_NORMAL, i));
        EXPECT_EQ(0,test_setPQIndex(0, PQClient::SCENARIO_PICTURE, TDSHP_FLAG_NORMAL, i));
    }
}

TEST (PQTest, PQ_004)
{
    EXPECT_EQ(0,test_FeatureSwitch(static_cast<IPQService::PQFeatureID>(IPQService::DYNAMIC_CONTRAST), 0));
    EXPECT_EQ(0,test_FeatureSwitch(static_cast<IPQService::PQFeatureID>(IPQService::DYNAMIC_CONTRAST), 1));
}

TEST (PQTest, PQ_005)
{
#ifdef MDP_COLOR_ENABLE
    EXPECT_EQ(0,test_FeatureSwitch(static_cast<IPQService::PQFeatureID>(IPQService::CONTENT_COLOR_VIDEO), 0));
    EXPECT_EQ(0,test_FeatureSwitch(static_cast<IPQService::PQFeatureID>(IPQService::CONTENT_COLOR_VIDEO), 1));
#endif
}


