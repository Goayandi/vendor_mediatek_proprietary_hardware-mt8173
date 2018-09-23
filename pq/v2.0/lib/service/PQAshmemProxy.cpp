#include "PQAshmemProxy.h"
#include <PQServiceCommon.h>
// Log interface, do NOT modify
#define LOG_TAG "PQProxy"
#include <cutils/log.h>
#define PQPROXY_LOGD(fmt, arg...) ALOGD(fmt, ##arg)

PQAshmemProxy::PQAshmemProxy()
{
   m_ashmem_base = NULL;
}

PQAshmemProxy::~PQAshmemProxy()
{
}

void PQAshmemProxy::setAshmemBase(unsigned int *ashmemBase)
{
    m_ashmem_base = ashmemBase;
}

bool PQAshmemProxy::isOverwritten(int32_t offset)
{
    int32_t tuning_offset = offset + m_tunning_magic_num;
    int32_t tuning_value = *(m_ashmem_base + tuning_offset);

    return (tuning_value == PROXY_TUNING_OVERWRITTEN) ? (true) : (false);
}

bool PQAshmemProxy::isReading(int32_t offset)
{
    int32_t tuning_offset = offset + m_tunning_magic_num;
    int32_t tuning_value = *(m_ashmem_base + tuning_offset);

    return (tuning_value == PROXY_TUNING_READING) ? (true) : (false);
}

int32_t PQAshmemProxy::setTuningField(int32_t offset, int32_t value)
{
    if (m_ashmem_base == NULL || m_ashmem_base == (unsigned int*)-1) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : Ashmem is not ready\n");
        return -1;
    }

    if (offset < 0 || offset > s_ashmem_max_offset) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : offset (0x%08x) is invalid\n", offset*sizeof(unsigned int));
        return -1;
    }

    *(m_ashmem_base + offset) = value;
    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : set offset 0x%08x = 0x%08x\n", offset*sizeof(unsigned int), value);

    return 0;
}

int32_t PQAshmemProxy::getTuningField(int32_t offset, int32_t *value)
{
    if (m_ashmem_base == NULL || m_ashmem_base == (unsigned int*)-1) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : Ashmem is not ready\n");
        return -1;
    }

    if (offset < 0 || offset > s_ashmem_max_offset) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : offset (0x%08x) is invalid\n", offset*sizeof(unsigned int));
        return -1;
    }

    // get field value
    *value = *(m_ashmem_base + offset);
    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : get offset 0x%08x = 0x%08x\n", offset*sizeof(unsigned int), *value);

    return 0;
}

int32_t PQAshmemProxy::pushToMDP(int32_t offset)
{
    int32_t sync_offset = offset + m_sync_magic_num;
    int32_t ok_offset = offset + m_sync_ok_magic_num;

    if (m_ashmem_base == NULL || m_ashmem_base == (unsigned int*)-1) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : Ashmem is not ready\n");
        return -1;
    }

    if (offset < 0 || offset > s_ashmem_max_offset) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : offset (0x%08x) is invalid\n", offset*sizeof(unsigned int));
        return -1;
    }

    *(m_ashmem_base + sync_offset) = 1;
    *(m_ashmem_base + ok_offset) = 0;

    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : sync offset (0x%08x) is 1\n", sync_offset*sizeof(unsigned int));
    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : sync ok offset (0x%08x) is 0\n", ok_offset*sizeof(unsigned int));

    return 0;
}

int32_t PQAshmemProxy::pullFromMDP(int32_t offset, int32_t scenario)
{
    int32_t tuning_offset = offset + m_tunning_magic_num;
    int32_t prepare_offset = offset + m_prepare_magic_num;
    int32_t ready_offset = offset + m_ready_magic_num;

    if (m_ashmem_base == NULL || m_ashmem_base == (unsigned int*)-1) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : Ashmem is not ready\n");
        return -1;
    }

    if (offset < 0 || offset > s_ashmem_max_offset) {
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : offset (0x%08x) is invalid\n", offset*sizeof(unsigned int));
        return -1;
    }

    *(m_ashmem_base + tuning_offset) = PROXY_TUNING_READING;
    *(m_ashmem_base + prepare_offset) = 1;

    if (scenario == SCENARIO_PICTURE)
    {
        //*(m_ashmem_base + ready_offset) = 1;
    }
    else
    {
        *(m_ashmem_base + ready_offset) = 0;
    }

    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : Tunning offset (0x%08x) is reading mode\n", tuning_offset*sizeof(unsigned int));
    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : prepare offset (0x%08x) is 1\n", prepare_offset*sizeof(unsigned int));
    PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : ready offset (0x%08x) is 0\n", ready_offset*sizeof(unsigned int));

    return 0;
}

bool PQAshmemProxy::setPQValueToAshmem(ProxyTuningBuffer proxyNum, int32_t field, int32_t value, int32_t scenario)
{
    int32_t offset = proxyNum * s_module_max_offset;
    size_t field_type_size = sizeof(unsigned int);
    int32_t max_allowed_field = s_module_size - field_type_size * PROXY_FLAG_MAX;

    if (field == 0xffff) { // Change mode
        if (value == PROXY_TUNING_READING) {// Reading
            pullFromMDP(offset, scenario);
        } else if (value == PROXY_TUNING_OVERWRITTEN || value == PROXY_TUNING_NORMAL
                    || value == PROXY_TUNING_BYPASSHWACCESS) {
            offset += m_tunning_magic_num;
            setTuningField(offset, value);
        } else {
            return false;
        }
        PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : change tuning mode to %d\n", value);
    } else if (field == 0xfffe && value == 0x0){ // sync buffer
        pushToMDP(offset);
    } else if (field >= 0 && field < max_allowed_field && (field & 0x3) == 0) {
        if (isOverwritten(offset) == true) {
            offset += (field / field_type_size);
            setTuningField(offset, value);
        } else {
            PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : not overwritten mode\n");
            return false;
        }
    } else {
        return false;
    }

    return true;
}

bool PQAshmemProxy::getPQValueFromAshmem(ProxyTuningBuffer proxyNum, int32_t field, int32_t *value)
{
    int32_t offset = proxyNum * s_module_max_offset;
    size_t field_type_size = sizeof(unsigned int);
    int32_t max_allowed_field = s_module_size - field_type_size * PROXY_FLAG_MAX;

    if (field == 0xffff) { // mode
        offset += m_tunning_magic_num;
    } else if (field == 0xfffe) { // sync buffer
        if (isOverwritten(offset) == true) {
            offset += m_sync_ok_magic_num;
        } else if (isReading(offset) == true) {
            offset += m_ready_magic_num;
        } else {
            PQPROXY_LOGD("[PQ_Proxy] PQAshmemProxy : sync buffer error due to incorrect mode\n");
            return false;
        }
    } else if (field >= 0 && field < max_allowed_field && (field & 0x3) == 0) {
        offset += (field / field_type_size);
    } else {
        return false;
    }

    getTuningField(offset, value);
    return true;
}