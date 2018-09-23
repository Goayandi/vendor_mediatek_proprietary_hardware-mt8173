
#ifndef __IPQSERVICE_H__
#define __IPQSERVICE_H__

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/BinderService.h>
#include "ddp_drv.h"
#include "PQServiceCommon.h"
#include <binder/IMemory.h>
#include <binder/MemoryHeapBase.h>
#include "cust_tdshp.h"


// Not all version supports Android runtime tuning.
// Runtime tuning is available only if this macro is defined.
#define MTK_PQ_RUNTIME_TUNING_SUPPORT


namespace android
{
    enum
    {
        SET_PQ_SHP_GAIN = 0,
        SET_PQ_SAT_GAIN,
        SET_PQ_LUMA_ADJ,
        SET_PQ_HUE_ADJ_SKIN,
        SET_PQ_HUE_ADJ_GRASS,
        SET_PQ_HUE_ADJ_SKY,
        SET_PQ_SAT_ADJ_SKIN,
        SET_PQ_SAT_ADJ_GRASS,
        SET_PQ_SAT_ADJ_SKY,
        SET_PQ_CONTRAST,
        SET_PQ_BRIGHTNESS
    };


    //
    //  Holder service for pass objects between processes.
    //
    class IPQService : public IInterface
    {
    protected:
        enum
        {
            PQ_SET_INDEX = IBinder::FIRST_CALL_TRANSACTION,
            PQ_GET_COLOR_INDEX,
            PQ_GET_TDSHP_INDEX,
            PQ_GET_MAPPED_COLOR_INDEX,
            PQ_GET_MAPPED_TDSHP_INDEX,
            PQ_SET_MODE,
            PQ_SET_COLOR_REGION,
            PQ_GET_COLOR_REGION,
            PQ_GET_TDSHP_FLAG,
            PQ_SET_TDSHP_FLAG,
            PQ_SET_SCENARIO,
            PQ_GET_PQDC_INDEX,
            PQ_SET_PQDC_INDEX,
            PQ_GET_PQDS_INDEX,
            PQ_GET_COLOR_CAP,
            PQ_GET_TDSHP_REG,
            PQ_SET_FEATURE_SWITCH,
            PQ_GET_FEATURE_SWITCH,
            PQ_ENABLE_BLUE_LIGHT,
            PQ_GET_BLUE_LIGHT_ENABLED,
            PQ_SET_BLUE_LIGHT_STRENGTH,
            PQ_GET_BLUE_LIGHT_STRENGTH,
            PQ_SET_TUNING_FIELD,
            PQ_GET_TUNING_FIELD,
            PQ_GET_ASHMEM,
            PQ_SET_GAMMA_INDEX,
            PQ_GET_GAMMA_INDEX,
            PQ_SET_GLOBAL_PQ_SWITCH,
            PQ_GET_GLOBAL_PQ_SWITCH,
            PQ_SET_GLOBAL_PQ_STRENGTH,
            PQ_GET_GLOBAL_PQ_STRENGTH,
            PQ_GET_GLOBAL_PQ_STRENGTH_RANGE,
            PQ_GET_GLOBAL_PQ_INDEX
        };

    public:
        enum
        {
            MOD_DISPLAY =               0,
            MOD_DYNAMIC_SHARPNESS =     2,
            MOD_TDSHP_REG =             3,
            MOD_ULTRARESOLUTION =       4,
            MOD_DYNAMIC_CONTRAST =      7,
            MOD_BLUE_LIGHT_ALGO =       0x0010,
            MOD_BLUE_LIGHT_INPUT =      0x0011,
            MOD_BLUE_LIGHT_OUTPUT =     0x0012,
            MOD_DS_SWREG =              0x0110,
            MOD_DS_INPUT =              0x0111,
            MOD_DS_OUTPUT =             0x0112,
            MOD_DC_SWREG =              0x0120,
            MOD_DC_INPUT =              0x0121,
            MOD_DC_OUTPUT =             0x0122,
            MOD_RSZ_SWREG =             0x0130,
            MOD_RSZ_INPUT =             0x0131,
            MOD_RSZ_OUTPUT =            0x0132,
            MOD_COLOR_SWREG =           0x0140,
            MOD_COLOR_INPUT =           0x0141,
            MOD_COLOR_OUTPUT =          0x0142
        };

    public:


        DECLARE_META_INTERFACE(PQService);
        virtual status_t getTDSHPReg(MDP_TDSHP_REG_EX *param) = 0;
        virtual status_t getColorCapInfo(MDP_COLOR_CAP *param) = 0;
        virtual status_t getPQDCIndex(DISP_PQ_DC_PARAM *dcparam, int32_t index) = 0;
        virtual status_t setPQDCIndex(int32_t level, int32_t index) = 0;
        virtual status_t getPQDSIndex(DISP_PQ_DS_PARAM_EX *dsparam) = 0;
        virtual status_t setTDSHPFlag(int32_t TDSHPFlag) = 0;
        virtual status_t getTDSHPFlag(int32_t *TDSHPFlag) = 0;
        virtual status_t getColorIndex(DISP_PQ_PARAM *index, int32_t  scenario, int32_t  mode) = 0;
        virtual status_t getMappedTDSHPIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode) = 0;
        virtual status_t getMappedColorIndex(DISP_PQ_PARAM *index, int32_t  scenario, int32_t  mode) = 0;
        virtual status_t getTDSHPIndex(DISP_PQ_PARAM *index, int32_t  scenario, int32_t  mode) = 0;
        virtual status_t setPQIndex(int32_t level, int32_t  scenario, int32_t  tuning_mode, int32_t index) = 0;
        virtual status_t setPQMode(int32_t mode) = 0;
        virtual status_t setColorRegion(int32_t split_en,int32_t start_x,int32_t start_y,int32_t end_x,int32_t end_y) = 0;
        virtual status_t getColorRegion(DISP_PQ_WIN_PARAM *win_param) = 0;
        virtual status_t setDISPScenario(int32_t scenario) = 0;
        virtual sp<IMemoryHeap> getAshmem(void) = 0;

        enum PQFeatureID
        {
            DISPLAY_COLOR,
            CONTENT_COLOR,
            CONTENT_COLOR_VIDEO,
            SHARPNESS,
            DYNAMIC_CONTRAST,
            DYNAMIC_SHARPNESS,
            DISPLAY_CCORR,
            DISPLAY_GAMMA,
            DISPLAY_OVER_DRIVE,
            ISO_ADAPTIVE_SHARPNESS,
            ULTRA_RESOLUTION,
            PQ_FEATURE_MAX,
        };

        virtual status_t setFeatureSwitch(PQFeatureID id, uint32_t value) = 0;
        virtual status_t getFeatureSwitch(PQFeatureID id, uint32_t *value) = 0;

        virtual status_t enableBlueLight(bool enable) = 0;
        virtual status_t getBlueLightEnabled(bool *isEnabled) = 0;
        virtual status_t setBlueLightStrength(int32_t strength) = 0;
        virtual status_t getBlueLightStrength(int32_t *strength) = 0;
        virtual status_t setTuningField(int32_t module, int32_t field, int32_t value) = 0;
        virtual status_t getTuningField(int32_t module, int32_t field, int32_t *value) = 0;

        virtual status_t setGammaIndex(int32_t index) = 0;
        virtual status_t getGammaIndex(int32_t *index) = 0;
        virtual status_t setGlobalPQSwitch(int32_t globalPQSwitch) = 0;
        virtual status_t getGlobalPQSwitch(int32_t *globalPQSwitch) = 0;
        virtual status_t setGlobalPQStrength(uint32_t globalPQStrength) = 0;
        virtual status_t getGlobalPQStrength(uint32_t *globalPQStrength) = 0;
        virtual status_t getGlobalPQStrengthRange(uint32_t *globalPQStrengthRange) = 0;
        virtual status_t getGlobalPQIndex(GLOBAL_PQ_INDEX_T *globalPQindex) = 0;
    };

    class BnPQService : public BnInterface<IPQService>
    {
        virtual status_t onTransact(uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
    };
};
#endif
