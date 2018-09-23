#define LOG_TAG "MfllCore/Bss"

#include "MfllBss.h"
#include "MfllLog.h"
#include "MfllExifInfo.h"

#include <libbss/MTKBss.h>

#include <camera_custom_mfll.h>

using std::vector;
using namespace mfll;

IMfllBss* IMfllBss::createInstance()
{
    return reinterpret_cast<IMfllBss*>(new MfllBss);
}

void IMfllBss::destroyInstance()
{
    decStrong((void*)this);
}

MfllBss::MfllBss()
: m_roiPercetange(MFLL_BSS_ROI_PERCENTAGE)
, m_pCore(NULL)
{
}

enum MfllErr MfllBss::init(sp<IMfllNvram> &nvramProvider)
{
    if (nvramProvider.get() == NULL) {
        mfllLogE("%s: init MfllBss fail, needs NVRAM provider but it's NULL", __FUNCTION__);
        return MfllErr_BadArgument;
    }
    m_nvramProvider = nvramProvider;
    return MfllErr_Ok;
}

vector<int> MfllBss::bss(
        const vector< sp<IMfllImageBuffer> >    &imgs,
        vector<MfllMotionVector_t>        &mvs
        )
{
    mfllAutoLogFunc();
    mfllAutoTraceFunc();

    MTKBss *mtkBss = NULL;
    int width = 0;
    int height = 0;
    size_t size = 0;
    int roiPercentage = m_roiPercetange;

    vector<int> newIndex;
    if (imgs.size() != mvs.size()) {
        mfllLogE("%s: the amount of image(%d) and GMV(%d) is different",
                __FUNCTION__, imgs.size(), mvs.size());
        goto lbExit;
    }

    width = imgs[0]->getWidth();
    height = imgs[0]->getHeight();
    size = imgs.size();

    mtkBss = MTKBss::createInstance(DRV_BSS_OBJ_SW);
    if (mtkBss == NULL) {
        mfllLogE("%s: create MTKBss instance failed", __FUNCTION__);
        goto lbExit;
    }

    if (mtkBss->BssInit(NULL, NULL) != S_BSS_OK) {
        mfllLogE("%s: init MTKBss failed", __FUNCTION__);
        goto lbExit;
    }

    /* set feature control */
    {
        /* calcuate ROI cropping width */
        int w = (width * roiPercentage + 5) / 100;
        int h = (height * roiPercentage + 5) / 100;
        int x = (width - w) / 2;
        int y = (height - h) / 2;

        BSS_PARAM_STRUCT p;
        p.BSS_ON = 1;
        p.BSS_ROI_WIDTH = w;
        p.BSS_ROI_HEIGHT = h;
        p.BSS_ROI_X0 = x;
        p.BSS_ROI_Y0 = y;
        p.BSS_SCALE_FACTOR = CUST_MFLL_BSS_SCALE_FACTOR;
        p.BSS_CLIP_TH0 = CUST_MFLL_BSS_CLIP_TH0;
        p.BSS_CLIP_TH1 = CUST_MFLL_BSS_CLIP_TH1;
        p.BSS_ZERO = CUST_MFLL_BSS_ZERO;
        p.BSS_FRAME_NUM = (MUINT32)imgs.size();

        /* print out information */
        mfllLogD("%s: BSS_ROI(x,y,w,h) =(%d,%d,%d,%d)", __FUNCTION__, x, y, w, h);
        mfllLogD("%s: BSS_SCALE_FACTOR = %d", __FUNCTION__, p.BSS_SCALE_FACTOR);
        mfllLogD("%s: BSS_CLIP_TH0     = %d", __FUNCTION__, p.BSS_CLIP_TH0);
        mfllLogD("%s: BSS_CLIP_TH1     = %d", __FUNCTION__, p.BSS_CLIP_TH1);
        mfllLogD("%s: BSS_ZERO         = %d", __FUNCTION__, p.BSS_ZERO);

        /* update debug info */
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_ROI_WIDTH, (uint32_t)p.BSS_ROI_WIDTH);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_ROI_HEIGHT, (uint32_t)p.BSS_ROI_HEIGHT);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_SCALE_FACTOR, (uint32_t)p.BSS_SCALE_FACTOR);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_CLIP_TH0, (uint32_t)p.BSS_CLIP_TH0);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_CLIP_TH1, (uint32_t)p.BSS_CLIP_TH1);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_ZERO, (uint32_t)p.BSS_ZERO);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_ROI_X0, (uint32_t)p.BSS_ROI_X0);
        m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_ROI_Y0, (uint32_t)p.BSS_ROI_Y0);

        auto b = mtkBss->BssFeatureCtrl(BSS_FTCTRL_SET_PROC_INFO, (void*)&p, NULL);
        if (b != S_BSS_OK) {
            mfllLogE("%s: set info to MTKBss failed (%d)", __FUNCTION__, (int)b);
            goto lbExit;
        }
    }

    /* main process */
    {
        BSS_OUTPUT_DATA outParam;
        BSS_INPUT_DATA inParam;

        inParam.inWidth = width;
        inParam.inHeight = height;
        unsigned int dbgIndex = (unsigned int)MF_TAG_GMV_00;
        for (size_t i = 0; i < size; i++) {
            inParam.apbyBssInImg[i] = (MUINT8*)imgs[i]->getVa();
            inParam.gmv[i].x = mvs[i].x;
            inParam.gmv[i].y = mvs[i].y;
            /* update debug info */
            m_pCore->updateExifInfo(
                    dbgIndex + i,
                    ((short)(mvs[i].y) << 16) | ((short)(mvs[i].x))
                    );
        }

        auto b = mtkBss->BssMain(BSS_PROC1, &inParam, &outParam);
        if (b != S_BSS_OK) {
            mfllLogE("%s: MTKBss::Main returns failed (%d)", __FUNCTION__, (int)b);
            goto lbExit;
        }

        for (size_t i = 0; i < size; i++) {
            newIndex.push_back(outParam.originalOrder[i]);
            mvs[i].x = outParam.gmv[i].x;
            mvs[i].y = outParam.gmv[i].y;
        }
    }

    m_pCore->updateExifInfo((unsigned int)MF_TAG_BSS_BEST_IDX, (uint32_t)newIndex[0]);

lbExit:
    if (mtkBss)
        mtkBss->destroyInstance();
    return newIndex;
}
