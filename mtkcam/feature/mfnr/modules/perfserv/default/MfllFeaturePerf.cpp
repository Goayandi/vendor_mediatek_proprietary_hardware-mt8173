#define LOG_TAG "MtkCam/MfllCore/Perf"

#include "MfllFeaturePerf.h"
#include "MfllLog.h"

using namespace mfll;


static std::vector<enum EventType> EVENTS_TO_LISTEN_INITIALIZER(void)
{
    std::vector<enum EventType> v;
    #define LISTEN(x) v.push_back(x)
    #undef LISTEN
    return v;
}
static vector<enum EventType> g_eventsToListen = EVENTS_TO_LISTEN_INITIALIZER();


MfllFeaturePerf::MfllFeaturePerf(void)
{
}

MfllFeaturePerf::~MfllFeaturePerf(void)
{
}

void MfllFeaturePerf::onEvent(enum EventType, MfllEventStatus_t&, void*, void*, void*)
{
}

void MfllFeaturePerf::doneEvent(enum EventType, MfllEventStatus_t&, void*, void*, void*)
{
}

vector<enum EventType> MfllFeaturePerf::getListenedEventTypes(void)
{
    return g_eventsToListen;
}
