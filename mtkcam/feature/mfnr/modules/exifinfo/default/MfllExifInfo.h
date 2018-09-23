#ifndef __MFLLEXIFINFO_H__
#define __MFLLEXIFINFO_H__

#include "IMfllExifInfo.h"

using android::sp;

namespace mfll {

class MfllExifInfo final : public IMfllExifInfo {
public:
    MfllExifInfo() {};
    ~MfllExifInfo() {};

/* implementation from IMfllNvram */
public:
    enum MfllErr init()
    { return MfllErr_NotImplemented; }

    enum MfllErr updateInfo(const MfllCoreDbgInfo_t &dbgInfo)
    { return MfllErr_NotImplemented; }

    enum MfllErr updateInfo(IMfllNvram *pNvram)
    { return MfllErr_NotImplemented; }

    enum MfllErr updateInfo(unsigned int key, uint32_t value)
    { return MfllErr_NotImplemented; }

    uint32_t getInfo(unsigned int key)
    { return 0; }

    const std::map<unsigned int, uint32_t>& getInfoMap()
    { return m_dataMap; }

    enum MfllErr sendCommand(
            const std::string& cmd              __attribute__((unused)),
            const std::deque<void*>& dataset    __attribute__((unused)))
    { return MfllErr_NotImplemented; }

    unsigned int getVersion()
    { return 0; }

/* Attributes */
private:
    std::map<unsigned int, uint32_t> m_dataMap;
};
}; /* namespace mfll */
#endif//__MFLLEXIFINFO_H__
