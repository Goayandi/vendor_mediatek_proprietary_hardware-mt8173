#define LOG_TAG "MtkCam/MfllCore/Nvram"

#include "MfllNvram.h"
#include "MfllLog.h"

#include <IHalSensor.h>
#include <mtkcam/aaa/INvBufUtil.h>

using android::sp;
using android::Mutex;

using NSCam::IHalSensorList;
using NSCam::SENSOR_DEV_NONE;
using namespace mfll;

//-----------------------------------------------------------------------------
// IMfllNvram methods
//-----------------------------------------------------------------------------
IMfllNvram* IMfllNvram::createInstance()
{
    return (IMfllNvram*)new MfllNvram;
}
//-----------------------------------------------------------------------------
void IMfllNvram::destroyInstance()
{
    decStrong((void*)this);
}
//-----------------------------------------------------------------------------
// MfllNvram implementation
//-----------------------------------------------------------------------------
MfllNvram::MfllNvram()
: m_chunkSize(0)
, m_sensorId(-1)
{
}
//-----------------------------------------------------------------------------
MfllNvram::~MfllNvram()
{
}
//-----------------------------------------------------------------------------
enum MfllErr MfllNvram::init(int sensorId)
{
    enum MfllErr err = MfllErr_Ok;
    Mutex::Autolock _l(&m_mutex);

    if (m_sensorId == -1
            || m_sensorId != sensorId
            || m_nvramChunk.get() == NULL)
    {
        size_t chunkSize = sizeof(NVRAM_CAMERA_FEATURE_STRUCT);
        std::shared_ptr<char> chunk(new char[chunkSize]);
        NVRAM_CAMERA_FEATURE_STRUCT *pNvram;
        MUINT sensorDev = SENSOR_DEV_NONE;
        {
            IHalSensorList* const pHalSensorList = MAKE_HalSensorList();
            if (pHalSensorList == NULL) {
                mfllLogE("%s: get IHalSensorList instance failed", __FUNCTION__);
                err = MfllErr_UnexpectedError;
                goto lbExit;
            }
            sensorDev = pHalSensorList->querySensorDevIdx(sensorId);
        }

        auto result = MAKE_NvBufUtil().getBufAndRead(
                CAMERA_NVRAM_DATA_FEATURE,
                sensorDev, (void*&)pNvram);
        if (result != 0) {
            mfllLogE("%s: read buffer chunk fail", __FUNCTION__);
            err = MfllErr_UnexpectedError;
            goto lbExit;
        }

        memcpy((void*)chunk.get(), (void*)pNvram, chunkSize);

        /* update info */
        m_chunkSize = chunkSize;
        m_nvramChunk = chunk;
        m_sensorId = sensorId;
    }

lbExit:
    return err;
}
//-----------------------------------------------------------------------------
std::shared_ptr<char> MfllNvram::chunk(size_t *bufferSize)
{
    if (getChunk(bufferSize) != NULL) {
        std::shared_ptr<char> __chunk(new char[*bufferSize]);
        memcpy((void*)__chunk.get(), (void*)m_nvramChunk.get(), m_chunkSize);
        return __chunk;
    }
    return NULL;
}
const char* MfllNvram::getChunk(size_t *bufferSize)
{
    Mutex::Autolock _l(&m_mutex);
    if (m_sensorId == -1 || m_nvramChunk.get() == NULL) {
        mfllLogE("%s: please init IMfllNvram first", __FUNCTION__);
        *bufferSize = 0;
        return NULL;
    }
    return (const char*)m_nvramChunk.get();
}
