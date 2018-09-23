#ifndef __MFLLEXIFINFO_H__
#define __MFLLEXIFINFO_H__

#include "IMfllExifInfo.h"

/* Exif info */
#include <BuiltinTypes.h> // required by dbg_cam_param.h
#include <debug_exif/cam/dbg_cam_param.h> //required by dbg_cam_mf_param.h
#include <debug_exif/cam/dbg_cam_mf_param.h>

// STL
#include <mutex>

using android::sp;

namespace mfll {

class MfllExifInfo final : public IMfllExifInfo {
public:
    MfllExifInfo();
    ~MfllExifInfo();

/* implementation from IMfllNvram */
public:
    enum MfllErr init();
    enum MfllErr updateInfo(const MfllCoreDbgInfo_t &dbgInfo);
    enum MfllErr updateInfo(IMfllNvram *pNvram);
    enum MfllErr updateInfo(unsigned int key, uint32_t value);
    uint32_t getInfo(unsigned int key);
    const std::map<unsigned int, uint32_t>& getInfoMap();

    enum MfllErr sendCommand(
            const std::string& cmd,
            const std::deque<void*>& dataset);

    unsigned int getVersion();

/* Attributes */
private:
    std::map<unsigned int, uint32_t> m_dataMap;
    std::mutex m_mutex;
};
}; /* namespace mfll */
#endif//__MFLLEXIFINFO_H__
