#include "MfllNvram.h"
#include "MfllLog.h"

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
