#include "MfllLog.h"
#include "MfllExifInfo.h"

using namespace mfll;

IMfllExifInfo* IMfllExifInfo::createInstance(void)
{
    return reinterpret_cast<IMfllExifInfo*>(new MfllExifInfo);
}

void IMfllExifInfo::destroyInstance(void)
{
    decStrong((void*)this);
}
