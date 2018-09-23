#include "MfllLog.h"
#include "MfllExifInfo.h"

#include <camera_custom_nvram.h>

#include <functional> // std::function
#include <unordered_map> // std::unordered_map

using namespace mfll;

IMfllExifInfo* IMfllExifInfo::createInstance(void)
{
    return reinterpret_cast<IMfllExifInfo*>(new MfllExifInfo);
}

void IMfllExifInfo::destroyInstance(void)
{
    decStrong((void*)this);
}

// extension function for sendCommand
// the implementation should be implemented in platform-dependent part
inline const std::unordered_map<
    std::string,  // key
    std::function<enum MfllErr(IMfllExifInfo*, const std::deque<void*>&)>
    >*
getExtFunctionMap();

//-----------------------------------------------------------------------------
// platform dependent part
//-----------------------------------------------------------------------------
#include "MfllExifInfo_platform.h"

MfllExifInfo::MfllExifInfo()
{
    m_dataMap[MF_TAG_VERSION] = MFLL_MF_TAG_VERSION;
}

MfllExifInfo::~MfllExifInfo()
{
}

enum MfllErr MfllExifInfo::init()
{
    std::unique_lock<std::mutex> _l(m_mutex);
    return MfllErr_Ok;
}

enum MfllErr MfllExifInfo::updateInfo(unsigned int key, uint32_t value)
{
    std::unique_lock<std::mutex> _l(m_mutex);
    m_dataMap[key] = value;
    return MfllErr_Ok;
}

uint32_t MfllExifInfo::getInfo(unsigned int key)
{
    std::unique_lock<std::mutex> _l(m_mutex);
    return m_dataMap[key];
}

const std::map<unsigned int, uint32_t>& MfllExifInfo::getInfoMap()
{
    return m_dataMap;
}

enum MfllErr MfllExifInfo::sendCommand(
        const std::string& cmd,
        const std::deque<void*>& dataset)
{
    auto pExtFuncMap = getExtFunctionMap();
    if (pExtFuncMap == NULL)
        return MfllErr_NotImplemented;

    auto func = pExtFuncMap->at(cmd.c_str());
    if (func) {
        return func(static_cast<IMfllExifInfo*>(this), dataset);
    }
    return MfllErr_NotImplemented;
}

unsigned int MfllExifInfo::getVersion()
{
    return (unsigned int)m_dataMap[MF_TAG_VERSION];
}
