#include <limits.h>
#include <gtest/gtest.h>
#include <string.h>
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/feature/stereo/hal/bmdn_hal.h>
#include "../../inc/stereo_dp_util.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BMDN_UT"

#define MY_LOGD(fmt, arg...)    printf("[D][%s]" fmt"\n", __func__, ##arg)
#define MY_LOGI(fmt, arg...)    printf("[I][%s]" fmt"\n", __func__, ##arg)
#define MY_LOGW(fmt, arg...)    printf("[W][%s] WRN(%5d):" fmt"\n", __func__, __LINE__, ##arg)
#define MY_LOGE(fmt, arg...)    printf("[E][%s] %s ERROR(%5d):" fmt"\n", __func__,__FILE__, __LINE__, ##arg)

#define BMDN_UT_CASE_PATH         "/data/nativetest/VSDoF_HAL_Test/BMDN_UT/"
#define BMDN_UT_CASE_IN_PATH      BMDN_UT_CASE_PATH"in"
#define BMDN_UT_CASE_OUT_PATH     BMDN_UT_CASE_PATH"out"
#define BMDN_UT_CASE_HAL_OUT_PATH BMDN_UT_CASE_OUT_PATH"/hal"
#define DEV_WIDTH 4208
#define DEV_HEIGHT 3120

using namespace NS_BMDN_HAL;

//=============================================================================
//  Interface Test
//=============================================================================
TEST(HAL_TEST, BMDNHAL_Interface_TEST)
{
    MY_LOGD("BMDNHAL_Interface_TEST +");

    // create instance
    sp<BMDN_HAL> myBMDNHal = BMDN_HAL::createInstance(LOG_TAG);

    EXPECT_EQ(true, myBMDNHal != nullptr) << "createInstance";

    // set configure param
    BMDN_HAL_CONFIGURE_DATA    configureData;
    configureData.initWidth = DEV_WIDTH;
    configureData.initHeight = DEV_HEIGHT;
    configureData.initPaddedWidth = DEV_WIDTH*2;
    configureData.initPaddedHeight = DEV_HEIGHT*2;

    EXPECT_EQ(true, myBMDNHal->BMDNHalConfigure(configureData)) << "configure";

    for(int req = 0 ; req < 10 ; req ++){
        MY_LOGD("req %d +", req);

        // set run-time param
        BMDN_HAL_IN_DATA    inData;

        BMDN_HAL_OUT_DATA   outData;

        // run after configuration
        unsigned short temp  = 1;
        inData.MonoProcessedRaw = &temp;
        EXPECT_EQ(true, myBMDNHal->BMDNHalRun(inData, outData)) << "run";

        MY_LOGD("req %d -", req);
    }

    myBMDNHal = nullptr;
    MY_LOGD("BMDNHAL_Interface_TEST-");
}