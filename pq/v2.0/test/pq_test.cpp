#define LOG_TAG "PQ-Test"

#include <cstdlib>
#include <iostream>
#include <cust_color.h>
#include <PQCommon.h>
#include <PQClient.h>
#include "cust_tdshp.h"
#include <PQDSImpl.h>
using namespace std;
using namespace android;

#define PQ_LOGD(fmt, arg...) ALOGD(fmt, ##arg)
#define PQ_LOGE(fmt, arg...) ALOGE(fmt, ##arg)

#define CHECK(stat) \
    do { \
        try \
        { \
            if (!(stat)) \
            { \
                cerr << __func__ << ":" << __LINE__ << endl; \
                cerr << "    " << #stat << " failed." << endl; \
            } \
        } \
        catch(...) \
        { \
            cerr << "Exception caught." << endl; \
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
void test_BluLightTuning()
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

    client.enableBlueLight(true);

    // Reading mode on/off
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 1);
    CHECK(ret == 0);
    ret = client.getTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, &value);
    CHECK(ret == 0);
    CHECK(value == 1);

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
    CHECK(ret != 0);
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0x5, 0);
    CHECK(ret != 0);

    // Error handing: invalid mode
    client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 1);
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 10);
    CHECK(ret != 0);
    ret = client.getTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, &value);
    CHECK(value == 1); // Should keep previous mode

    // Error handing: write in reading mode should be forbidden
    client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, 0xffff, 1);
    ret = client.setTuningField(IPQService::MOD_BLUE_LIGHT_INPUT, RegOffset(GLOBAL_SAT), 512);
    CHECK(ret != 0);

    #undef RegOffset

    CASE_DONE();
}


void test_CustParameters()
{
    CustParameters &cust = CustParameters::getPQCust();
    int var = 0;

    if (cust.isGood()) {
        CHECK(cust.loadVar("BaseOnly", &var));
        CHECK(var == 1111);

        CHECK(cust.loadVar("ProjectOnly", &var));
        CHECK(var == 2222);

        CHECK(cust.loadVar("ProjectOverwritten", &var));
        CHECK(var == 2222);
    } else {
        cerr << "Load libpq_cust*.so failed." << endl;
    }

    CASE_DONE();
}
#ifdef PQ_REFACTORING
void test_setPQMode(int32_t mode)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    char value[PROPERTY_VALUE_MAX];
    cout << "mode =" << mode  << endl;
    ret = client.setPQMode(mode);
    CHECK(ret == 0);
    property_get(PQ_PIC_MODE_PROPERTY_STR, value, PQ_PIC_MODE_DEFAULT);
    CHECK(atoi(value) == mode);

    CASE_DONE();
}

void test_setPQIndex(int32_t level, int32_t  scenario, int32_t  tuning_mode, int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    DISP_PQ_PARAM pqparam_test;

    cout << " index = " << index << " ,level = " << level << endl;

    ret = client.setPQIndex(level, scenario, tuning_mode, index);
    CHECK(ret == 0);

    //ret = client.getMappedColorIndex(&pqparam_test, scenario, 0);// mode parameter is not used anymore
    //CHECK(ret == 0);

    CASE_DONE();
}

void test_getMappedPQIndex(int32_t scenario, int32_t mode)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t dlopen_error;
    int32_t scenario_index;

    DISP_PQ_PARAM pqparam_test;
    DISP_PQ_PARAM pqparam_user_def;
    DISP_PQ_PARAM pqparam_table[PQ_PARAM_TABLE_SIZE];
    DISP_PQ_MAPPING_PARAM pqparam_mapping;

    cout <<  " scenario = " << scenario << " mode = " << mode << endl;

    //int pqparam_table_index = (mode < PQ_PIC_MODE_USER_DEF)? (mode) * PQ_SCENARIO_COUNT + scenario : PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT
    dlopen_error = getPqparam_table(&pqparam_table[0]);
    CHECK(dlopen_error == 0);

    LoadUserModePQParam(&pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT]);

    dlopen_error = getPqparam_mapping(&pqparam_mapping);
    CHECK(dlopen_error == 0);

    ret = client.getMappedColorIndex(&pqparam_test, scenario, mode);
    CHECK(ret == 0);
    ret = client.getMappedTDSHPIndex(&pqparam_test, scenario, mode);
    CHECK(ret == 0);

    scenario_index = getScenarioIndex(scenario);

    if(mode == PQ_PIC_MODE_STANDARD || mode == PQ_PIC_MODE_VIVID)
    {
        CHECK(0 == memcmp(&pqparam_test, &pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], sizeof(DISP_PQ_PARAM) - sizeof(unsigned int)));
        cout << "pqparam_table_index " << (mode) * PQ_SCENARIO_COUNT + scenario_index << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], shp[%d] " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SHPGain << ",gsat[%d] " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatGain << ",cont[%d] " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4Contrast << ",bri[%d] " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4Brightness << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], hue0[%d]" << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[0] << ",hue1[%d] "  << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[1]  << ",hue2[%d] " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[2]  << ",hue3[%d] "<< pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4HueAdj[3] << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], sat0[%d]" << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[0] << ",sat1[%d] "  << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[1]  << ",sat2[%d] " << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[2]  << ",sat3[%d] "<< pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4SatAdj[3] << endl;
        cout << "pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index], partialY[%d]" << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4PartialY << ",u4Ccorr[%d] "  << pqparam_table[(mode) * PQ_SCENARIO_COUNT + scenario_index].u4Ccorr   << endl;

        cout << "pqparam_test, shp[%d] " << pqparam_test.u4SHPGain << ",gsat[%d] " << pqparam_test.u4SatGain << ",cont[%d] " << pqparam_test.u4Contrast << ",bri[%d] " << pqparam_test.u4Brightness << endl;
        cout << "pqparam_test, hue0[%d]" << pqparam_test.u4HueAdj[0] << ",hue1[%d] "  << pqparam_test.u4HueAdj[1]  << ",hue2[%d] " << pqparam_test.u4HueAdj[2]  << ",hue3[%d] "<< pqparam_test.u4HueAdj[3] << endl;
        cout << "pqparam_test, sat0[%d]" << pqparam_test.u4SatAdj[0] << ",sat1[%d] "  << pqparam_test.u4SatAdj[1]  << ",sat2[%d] " << pqparam_test.u4SatAdj[2]  << ",sat3[%d] "<< pqparam_test.u4SatAdj[3] << endl;
        cout << "pqparam_test, partialY[%d]" << pqparam_test.u4PartialY << ",u4Ccorr[%d] "  << pqparam_test.u4Ccorr   << endl;

    }
    else if ( mode == PQ_PIC_MODE_USER_DEF)
    {
        calcPQStrength(&pqparam_user_def, &pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT], getPQStrengthPercentage(scenario, &pqparam_mapping));
        CHECK(0 == memcmp(&pqparam_test, &pqparam_user_def, sizeof(DISP_PQ_PARAM) - sizeof(unsigned int)));

        cout << "pqparam_user_def, shp[%d] " << pqparam_user_def.u4SHPGain << ",gsat[%d] " << pqparam_user_def.u4SatGain << ",cont[%d] " << pqparam_user_def.u4Contrast << ",bri[%d] " << pqparam_user_def.u4Brightness << endl;
        cout << "pqparam_user_def, hue0[%d]" << pqparam_user_def.u4HueAdj[0] << ",hue1[%d] "  << pqparam_user_def.u4HueAdj[1]  << ",hue2[%d] " << pqparam_user_def.u4HueAdj[2]  << ",hue3[%d] "<< pqparam_user_def.u4HueAdj[3] << endl;
        cout << "pqparam_user_def, sat0[%d]" << pqparam_user_def.u4SatAdj[0] << ",sat1[%d] "  << pqparam_user_def.u4SatAdj[1]  << ",sat2[%d] " << pqparam_user_def.u4SatAdj[2]  << ",sat3[%d] "<< pqparam_user_def.u4SatAdj[3] << endl;
        cout << "pqparam_user_def, partialY[%d]" << pqparam_user_def.u4PartialY << ",u4Ccorr[%d] "  << pqparam_user_def.u4Ccorr   << endl;

        cout << "pqparam_test, shp[%d] " << pqparam_test.u4SHPGain << ",gsat[%d] " << pqparam_test.u4SatGain << ",cont[%d] " << pqparam_test.u4Contrast << ",bri[%d] " << pqparam_test.u4Brightness << endl;
        cout << "pqparam_test, hue0[%d]" << pqparam_test.u4HueAdj[0] << ",hue1[%d] "  << pqparam_test.u4HueAdj[1]  << ",hue2[%d] " << pqparam_test.u4HueAdj[2]  << ",hue3[%d] "<< pqparam_test.u4HueAdj[3] << endl;
        cout << "pqparam_test, sat0[%d]" << pqparam_test.u4SatAdj[0] << ",sat1[%d] "  << pqparam_test.u4SatAdj[1]  << ",sat2[%d] " << pqparam_test.u4SatAdj[2]  << ",sat3[%d] "<< pqparam_test.u4SatAdj[3] << endl;
        cout << "pqparam_test, partialY[%d]" << pqparam_test.u4PartialY << ",u4Ccorr[%d] "  << pqparam_test.u4Ccorr   << endl;

    }

    CASE_DONE();
}

void test_getPQIndex(int32_t scenario, int32_t mode)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t dlopen_error;
    int32_t scenario_index;

    cout <<  " scenario = " << scenario << " mode = " << mode << endl;

    DISP_PQ_PARAM pqparam_test;
    DISP_PQ_PARAM pqparam_user_def;
    DISP_PQ_PARAM pqparam_table[PQ_PARAM_TABLE_SIZE];
    DISP_PQ_MAPPING_PARAM pqparam_mapping;

    scenario_index = getScenarioIndex(scenario);

    int pqparam_table_index = (mode < PQ_PIC_MODE_USER_DEF)? (mode) * PQ_SCENARIO_COUNT + scenario_index : PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT;

    dlopen_error = getPqparam_table(&pqparam_table[0]);
    CHECK(dlopen_error == 0);

    LoadUserModePQParam(&pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT]);

    ret = client.getColorIndex(&pqparam_test, scenario, mode);
    CHECK(ret == 0);
    ret = client.getTDSHPIndex(&pqparam_test, scenario, mode);
    CHECK(ret == 0);

    CHECK(0 == memcmp(&pqparam_test, &pqparam_table[pqparam_table_index], sizeof(DISP_PQ_PARAM) - sizeof(unsigned int))); // discard ccorr
    cout << "pqparam_table_index " << pqparam_table_index << endl;
    cout << "pqparam_table[pqparam_table_index], shp[%d] " << pqparam_table[pqparam_table_index].u4SHPGain << ",gsat[%d] " << pqparam_table[pqparam_table_index].u4SatGain << ",cont[%d] " << pqparam_table[pqparam_table_index].u4Contrast << ",bri[%d] " << pqparam_table[pqparam_table_index].u4Brightness << endl;
    cout << "pqparam_table[pqparam_table_index], hue0[%d]" << pqparam_table[pqparam_table_index].u4HueAdj[0] << ",hue1[%d] "  << pqparam_table[pqparam_table_index].u4HueAdj[1]  << ",hue2[%d] " << pqparam_table[pqparam_table_index].u4HueAdj[2]  << ",hue3[%d] "<< pqparam_table[pqparam_table_index].u4HueAdj[3] << endl;
    cout << "pqparam_table[pqparam_table_index], sat0[%d]" << pqparam_table[pqparam_table_index].u4SatAdj[0] << ",sat1[%d] "  << pqparam_table[pqparam_table_index].u4SatAdj[1]  << ",sat2[%d] " << pqparam_table[pqparam_table_index].u4SatAdj[2]  << ",sat3[%d] "<< pqparam_table[pqparam_table_index].u4SatAdj[3] << endl;
    cout << "pqparam_table[pqparam_table_index], partialY[%d]" << pqparam_table[pqparam_table_index].u4PartialY << ",u4Ccorr[%d] "  << pqparam_table[pqparam_table_index].u4Ccorr   << endl;


    cout << "pqparam_test, shp[%d] " << pqparam_test.u4SHPGain << ",gsat[%d] " << pqparam_test.u4SatGain << ",cont[%d] " << pqparam_test.u4Contrast << ",bri[%d] " << pqparam_test.u4Brightness << endl;
    cout << "pqparam_test, hue0[%d]" << pqparam_test.u4HueAdj[0] << ",hue1[%d] "  << pqparam_test.u4HueAdj[1]  << ",hue2[%d] " << pqparam_test.u4HueAdj[2]  << ",hue3[%d] "<< pqparam_test.u4HueAdj[3] << endl;
    cout << "pqparam_test, sat0[%d]" << pqparam_test.u4SatAdj[0] << ",sat1[%d] "  << pqparam_test.u4SatAdj[1]  << ",sat2[%d] " << pqparam_test.u4SatAdj[2]  << ",sat3[%d] "<< pqparam_test.u4SatAdj[3] << endl;
    cout << "pqparam_test, partialY[%d]" << pqparam_test.u4PartialY << ",u4Ccorr[%d] "  << pqparam_test.u4Ccorr   << endl;
    CASE_DONE();
}

void test_PQDCIndex(int32_t level, int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    DISP_PQ_DC_PARAM dcparam;
    int32_t value;

    cout <<  " index = " << index <<  endl;

    ret = client.setPQDCIndex(level,index);
    CHECK(ret == 0);

    ret = client.getPQDCIndex(&dcparam, index);
    CHECK(ret == 0);
    CHECK(dcparam.param[index] == level);
    ret = client.getTuningField(IPQService::MOD_DYNAMIC_CONTRAST, index, &value);
    CHECK(ret == 0);
    CHECK(value == level);

    ret = client.setTuningField(IPQService::MOD_DYNAMIC_CONTRAST, index, level + 1);
    CHECK(ret == 0);

    ret = client.getPQDCIndex(&dcparam, index);
    CHECK(ret == 0);
    CHECK(dcparam.param[index] == level + 1);
    ret = client.getTuningField(IPQService::MOD_DYNAMIC_CONTRAST, index, &value);
    CHECK(ret == 0);
    CHECK(value == level + 1);

    CASE_DONE();
}

void test_PQDSIndex(int32_t level, int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    DISP_PQ_DS_PARAM_EX dsparam;
    int32_t value;

    cout <<  " index = " << index <<  endl;

    ret = client.setTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, index, level);
    CHECK(ret == 0);

    ret = client.getPQDSIndex(&dsparam);
    CHECK(ret == 0);
    CHECK(dsparam.param[index] == level);
    ret = client.getTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, index, &value);
    CHECK(ret == 0);
    CHECK(value == level);

    CASE_DONE();
}

void test_TuningFlag(int32_t FlagEnum)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t  TuninfFlag;


    cout <<  " Flag = " << FlagEnum <<  endl;

    ret = client.setTDSHPFlag(FlagEnum);
    CHECK(ret == 0);

    ret = client.getTDSHPFlag(&TuninfFlag);
    CHECK(ret == 0);
    CHECK(TuninfFlag == FlagEnum);

    CASE_DONE();
}

void test_setDISPScenario(int32_t scenario)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;

    cout <<  " scenario = " << scenario <<  endl;

    ret = client.setDISPScenario(scenario);
    CHECK(ret == 0);

    CASE_DONE();
}

void test_FeatureSwitch(IPQService::PQFeatureID id, uint32_t value)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    uint32_t test_value;

    cout <<  " PQFeatureID = " << id <<  " value = " << value <<  endl;

    ret = client.setFeatureSwitch(id, value);
    CHECK(ret == 0);

    ret = client.getFeatureSwitch(id, &test_value);
    CHECK(ret == 0);

    CHECK(test_value == value);

    CASE_DONE();
}

void test_TDSHPReg(int32_t level, int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t value;
    MDP_TDSHP_REG_EX tdshp_reg;

    cout <<  " index = " << index <<  endl;

    ret = client.setTuningField(IPQService::MOD_TDSHP_REG, index, level);
    CHECK(ret == 0);

    client.getTDSHPReg(&tdshp_reg);
    CHECK(ret == 0);
    CHECK(tdshp_reg.param[index] == level);
    ret = client.getTuningField(IPQService::MOD_TDSHP_REG, index, &value);
    CHECK(ret == 0);
    CHECK(value == level);

    CASE_DONE();
}

void test_UltraResolutionReg(int32_t level, int32_t index)
{
    PQClient &client(PQClient::getInstance());
    status_t ret;
    int32_t value;

    cout <<  " index = " << index <<  endl;

    ret = client.setTuningField(IPQService::MOD_ULTRARESOLUTION, index, level);
    CHECK(ret == 0);

    ret = client.getTuningField(IPQService::MOD_ULTRARESOLUTION, index, &value);
    CHECK(ret == 0);
    CHECK(value == level);

    CASE_DONE();
}
#endif

int main(int, char *[])
{
    int i,j;

   // test_BluLightTuning();

    // test_CustParameters();
#if 0
{   // test_setPQMode
    test_setPQMode(PQ_PIC_MODE_STANDARD);
    test_setPQMode(PQ_PIC_MODE_VIVID);
    test_setPQMode(PQ_PIC_MODE_USER_DEF);
}



{   // test_getMappedPQIndex
    for (i = PQClient::SCENARIO_VIDEO; i <= PQClient::SCENARIO_ISP_PREVIEW + OUT_OF_UPPERBOUND_ONE; i++)
    {
        for (j = PQ_PIC_MODE_STANDARD; j <= PQ_PIC_MODE_USER_DEF; j++)
        {
            test_setPQMode(j);
            test_getMappedPQIndex(i, j);
        }
    }

}

{   // test_getPQIndex
    for (i = PQClient::SCENARIO_VIDEO; i <= PQClient::SCENARIO_ISP_PREVIEW + OUT_OF_UPPERBOUND_ONE; i++)
    {
        for (j = PQ_PIC_MODE_STANDARD; j <= PQ_PIC_MODE_USER_DEF; j++)
        {
            test_setPQMode(j);
            test_getPQIndex(i, j);
        }
    }
}

{   // test_PQDCIndex
    for (i = BlackEffectEnable ; i < PQDC_INDEX_MAX + OUT_OF_UPPERBOUND_ONE; i++)
    {
        test_PQDCIndex(0, i);
    }
    // test_PQDSIndex
    for (i = PQDS_DS_en ; i < PQDS_INDEX_MAX + OUT_OF_UPPERBOUND_ONE; i++)
    {
        test_PQDSIndex(0, i);
    }
    // test_TDSHPReg
    for (i = TDSHP_GAIN_MID; i < TDSHP_REG_INDEX_MAX + OUT_OF_UPPERBOUND_ONE; i++)
    {
        test_TDSHPReg(0, i);
    }
    // test UltraResolutionReg
    for (i = RSZ_tableMode; i < RSZ_INDEX_MAX + OUT_OF_UPPERBOUND_ONE; i++)
    {
        test_UltraResolutionReg(0, i);
    }

}

{   // test_TuningFlag
    test_TuningFlag(TDSHP_FLAG_NCS_SHP_TUNING );
    test_TuningFlag(TDSHP_FLAG_NORMAL);
}

{   // test_setDISPScenario
    for (i = PQClient::SCENARIO_PICTURE; i <= PQClient::SCENARIO_ISP_PREVIEW + OUT_OF_UPPERBOUND_ONE; i++)
    {
        test_setDISPScenario(i);
    }

}

{
    // test_FeatureSwitch
    for (i = IPQService::DISPLAY_COLOR; i < IPQService::PQ_FEATURE_MAX + OUT_OF_UPPERBOUND_ONE; i++)
    {
#ifndef MDP_COLOR_ENABLE
        if (i == IPQService::CONTENT_COLOR || i == IPQService::CONTENT_COLOR_VIDEO)
            continue;
#endif
        if (i == IPQService::DISPLAY_CCORR)  //CCORR feature switch has not been implemented
            continue;

        test_FeatureSwitch(static_cast<IPQService::PQFeatureID>(i), 0);
        test_FeatureSwitch(static_cast<IPQService::PQFeatureID>(i), 1);
    }

}

{   // test_setPQIndex
    for (i = SET_PQ_SHP_GAIN; i <= SET_PQ_BRIGHTNESS + OUT_OF_UPPERBOUND_ONE; i++)
    {
        test_setPQIndex(9, PQClient::SCENARIO_PICTURE, TDSHP_FLAG_NORMAL, i);
        test_setPQIndex(0, PQClient::SCENARIO_PICTURE, TDSHP_FLAG_NORMAL, i);
    }

}
#endif
    #define DSRegOffset(field) (unsigned int)((char*)&( ((DSReg*)(0))->field) - (char*)(0))


    PQClient &client(PQClient::getInstance());
    client.setTuningField(IPQService::MOD_DS_SWREG, 0xffff, 1);
    int32_t value;
    while (1){
        client.getTuningField(IPQService::MOD_DS_SWREG, 0xfffe, &value);
        PQ_LOGD("0xfffe = [%d]",value);
        if(value == 1)
            break;
    }


    client.setTuningField(IPQService::MOD_DS_SWREG, 0xffff, 2);
    client.setTuningField(IPQService::MOD_DS_SWREG, 0, 0);

    client.setTuningField(IPQService::MOD_DS_SWREG, 0xfffe, 0);
    client.setTuningField(IPQService::MOD_DS_SWREG, 0xffff, 0);
    if (0){
    client.setTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, 0xffff, 1);
    //client.setTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, PQDS_DS_en, 0);
    int32_t value;
    while (1){
    client.getTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, 0xfffe, &value);
    PQ_LOGD("0xfffe = [%d]",value);
    if(value == 1)
        break;
        }
    client.getTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, PQDS_iGain_clip2, &value);
    //client.setTuningField(IPQService::MOD_DYNAMIC_SHARPNESS, 0xfffe, 0);
    PQ_LOGD("PQDS_iGain_clip2 = [%d]",value);
    }
    PQ_LOGD("[PQ_TEST] Refactor");
    return 0;
}
