#ifndef __PQ_SERVICE_H__
#define __PQ_SERVICE_H__

#include <utils/threads.h>
#include "IPQService.h"

// HAL
#include <hardware/hardware.h>
#include <hardware/lights.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#include "PQPictureMode.h"

class BluLightDefender;
class PQAshmemProxy;

namespace android
{
    class PQService :
            public BinderService<PQService>,
            public BnPQService,
            public Thread
    {
        friend class BinderService<PQService>;
    public:
        PQService();
        ~PQService();

        static char const* getServiceName() { return "PQ"; }

        // IPQService interface

        virtual status_t dump(int fd, const Vector<String16>& args);
        virtual status_t setColorRegion(int32_t split_en,int32_t start_x,int32_t start_y,int32_t end_x,int32_t end_y);
        virtual status_t getColorRegion(DISP_PQ_WIN_PARAM *win_param);
        virtual status_t setPQMode(int32_t mode);
        virtual status_t getTDSHPFlag(int32_t *TDSHPFlag);
        virtual status_t setTDSHPFlag(int32_t TDSHPFlag);
        virtual status_t getMappedColorIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode);
        virtual status_t getMappedTDSHPIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode);
        virtual status_t getPQDCIndex(DISP_PQ_DC_PARAM *dcparam, int32_t index);
        virtual status_t getPQDSIndex(DISP_PQ_DS_PARAM_EX *dsparam);
        virtual status_t setPQDCIndex(int32_t level, int32_t index);
        virtual status_t getColorIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode);
        virtual status_t getTDSHPIndex(DISP_PQ_PARAM *index, int32_t scenario, int32_t mode);
        virtual status_t setPQIndex(int32_t level, int32_t  scenario, int32_t  tuning_mode, int32_t index);
        virtual status_t setDISPScenario(int32_t scenario);
        virtual status_t getColorCapInfo(MDP_COLOR_CAP *param);
        virtual status_t getTDSHPReg(MDP_TDSHP_REG_EX *param);
        virtual status_t setFeatureSwitch(IPQService::PQFeatureID id, uint32_t value);
        virtual status_t getFeatureSwitch(IPQService::PQFeatureID id, uint32_t *value);
        virtual status_t enableBlueLight(bool enable);
        virtual status_t getBlueLightEnabled(bool *isEnabled);
        virtual status_t setBlueLightStrength(int32_t strength);
        virtual status_t getBlueLightStrength(int32_t *strength);
        virtual status_t setTuningField(int32_t module, int32_t field, int32_t value);
        virtual status_t getTuningField(int32_t module, int32_t field, int32_t *value);
        virtual sp<IMemoryHeap> getAshmem(void);
        virtual status_t setGammaIndex(int32_t index);
        virtual status_t getGammaIndex(int32_t *index);

    private:

        status_t enableDisplayColor(uint32_t value);
        status_t enableContentColorVideo(uint32_t value);
        status_t enableContentColor(uint32_t value);
        status_t enableSharpness(uint32_t value);
        status_t enableDynamicContrast(uint32_t value);
        status_t enableDynamicSharpness(uint32_t value);
        status_t enableDisplayGamma(uint32_t value);
        status_t enableDisplayOverDrive(uint32_t value);
        status_t enableISOAdaptiveSharpness(uint32_t value);
        status_t enableUltraResolution(uint32_t value);

        int _getLcmIndexOfGamma();
        void _setGammaIndex(int index);
        void configGamma(int picMode);
        void setDebuggingPqparam();
        bool loadPqparamTable();
        bool loadPqparamMappingCoefficient();
        bool loadPqindexTable();
        void loadTdshpIndexTable();
        void loadGammaEntryTable();
        status_t configCcorrCoef(int32_t coefTableIdx);

        // The value should be the same as the PQFeatureID enum in IPQService
        enum PQFeatureID {
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

        PicModeStandard *m_pic_statndard;
        PicModeVivid *m_pic_vivid;
        PicModeUserDef *m_pic_userdef;

        DISP_PQ_MAPPING_PARAM m_pqparam_mapping;
        DISP_PQ_PARAM m_pqparam_table[PQ_PARAM_TABLE_SIZE];
        DISP_PQ_DS_PARAM_EX g_PQ_DS_Param;
        DISP_PQ_DC_PARAM g_PQ_DC_Param;
        MDP_TDSHP_REG_EX g_tdshp_reg;
        PQ_RSZ_PARAM rszparam;
        void initDefaultPQParam();

        virtual void onFirstRef();
        virtual status_t readyToRun();
        bool initDriverRegs();
        virtual bool threadLoop();
        unsigned int remapCcorrIndex(unsigned int ccorrindex);
        mutable Mutex mLock;
        bool mCcorrDebug;

        DISP_PQ_WIN_PARAM mdp_win_param;
        DISP_PQ_PARAM m_pqparam;    /* for temp m_pqparam calculation or ioctl */
        DISP_PQ_DC_PARAM pqdcparam;
        DISP_PQ_DS_PARAM pqdsparam;
        DISPLAY_PQ_T m_pqindex;
        DISPLAY_TDSHP_T m_tdshpindex; /* try to remove this one */
        gamma_entry_t m_CustGamma[GAMMA_LCM_MAX][GAMMA_INDEX_MAX];

        int32_t m_PQMode;
        int32_t m_PQScenario;
        int32_t m_drvID;
        uint32_t m_bFeatureSwitch[PQ_FEATURE_MAX];
        unsigned int mPQDebugFlag;
        sp<MemoryHeapBase> mAshMemHeap;
        bool m_isAshmemInit;

        enum PQTuningMode {
            PQ_TUNING_NORMAL,
            PQ_TUNING_READING,
            PQ_TUNING_OVERWRITTEN,
            PQ_TUNING_END
        };

        PQTuningMode mBLInputMode;
        void *mBLInput;
        PQTuningMode mBLOutputMode;
        void *mBLOutput;
        BluLightDefender *blueLight;
        unsigned int mBlueLightDebugFlag;

        PQAshmemProxy *m_AshmemProxy;

        void setSystemProperty(const char *feature, int32_t featureSwitch);
        void initBlueLight();
        bool composeColorRegisters(void *_colorReg, const DISP_PQ_PARAM *pqparam);
        static bool translateColorRegisters(void *algoReg, DISPLAY_COLOR_REG_T *drvReg);
        bool setPQParamWithFilter(int drvID, const DISP_PQ_PARAM *pqparam);
        bool getCCorrCoefByIndex(int32_t coefTableIdx, uint32_t coef[3][3]);
        bool configCCorrCoef(int drvID, uint32_t coef[3][3]);
        status_t configCCorrCoefByIndex(int32_t coefTableIdx, int32_t drvID);
        void refreshDisplay();
        void dumpColorRegisters(const char *prompt, void *_colorReg);
        void calcPQStrength(DISP_PQ_PARAM *pqparam_dst, DISP_PQ_PARAM *pqparam_src, int percentage);
        void translateToColorTuning(void *algoReg, void *tuningReg);
        void translateFromColorTuning(void *algoReg, void *tuningReg);
        int32_t getPQStrengthRatio(int scenario);
        int32_t getScenarioIndex(int32_t scenario);
        void getUserModePQParam();
        void setPQParamlevel(DISP_PQ_PARAM *pqparam_image_ptr, int32_t index, int32_t level);
        bool isStandardPictureMode(int32_t mode, int32_t scenario_index);
        bool isVividPictureMode(int32_t mode, int32_t scenario_index);
        bool isUserDefinedPictureMode(int32_t mode, int32_t scenario_index);
        void initPQProperty(void);
        void initPQCustTable();
        bool initAshmem();

private:
    /*bit0 for sharpness, bit1 for DC */
    uint32_t m_GlobalPQSwitch;
    /*
     * bit0-7 for video sharpness
     * bit8-15 for video DC
     * bit16-23 for ui sharpness
     * bit24-31 for ui DC
     */
    uint32_t m_GlobalPQStrength;
    uint32_t m_GlobalPQStrengthRange;

    GLOBAL_PQ_INDEX_T m_GlobalPQindex;

    int32_t m_GlobalPQSupport;

    void initGlobalPQ(void);

public:
    virtual status_t setGlobalPQSwitch(int32_t globalPQSwitch);
    virtual status_t getGlobalPQSwitch(int32_t *globalPQSwitch);
    virtual status_t setGlobalPQStrength(uint32_t globalPQStrength);
    virtual status_t getGlobalPQStrength(uint32_t *globalPQStrength);
    virtual status_t getGlobalPQStrengthRange(uint32_t *globalPQStrengthRange);
    virtual status_t getGlobalPQIndex(GLOBAL_PQ_INDEX_T *globalPQindex);

    };
};
#endif
