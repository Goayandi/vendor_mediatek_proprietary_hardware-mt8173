#ifndef __PQ_ASHMEMPROXY_H__
#define __PQ_ASHMEMPROXY_H__

#include <stdio.h>

#define MODULE_MAX_OFFSET (512)

enum ProxyTuning {
    PROXY_TUNING_FLAG = 0,
    PROXY_SYNC_FLAG,
    PROXY_SYNC_OK_FLAG,
    PROXY_PREPARE_FLAG,
    PROXY_READY_FLAG,
    PROXY_FLAG_MAX
};

enum ProxyTuningBuffer {
    PROXY_DEBUG = 0,
    PROXY_DS_INPUT,
    PROXY_DS_OUTPUT,
    PROXY_DS_SWREG,
    PROXY_DC_INPUT,
    PROXY_DC_OUTPUT,
    PROXY_DC_SWREG,
    PROXY_RSZ_INPUT,
    PROXY_RSZ_OUTPUT,
    PROXY_RSZ_SWREG,
    PROXY_COLOR_INPUT,
    PROXY_COLOR_OUTPUT,
    PROXY_COLOR_SWREG,
    PROXY_TDSHP_REG,
    PROXY_ULTRARESOLUTION,
    PROXY_DYNAMIC_CONTRAST,
    PROXY_MAX
};

enum ProxyTuningMode {
    PROXY_TUNING_NORMAL,
    PROXY_TUNING_READING,
    PROXY_TUNING_OVERWRITTEN,
    PROXY_TUNING_BYPASSHWACCESS,
    PROXY_TUNING_END
};

class PQAshmemProxy
{
public:
    PQAshmemProxy();
    ~PQAshmemProxy();

    static unsigned int getAshmemSize(void) { return s_ashmem_size; }
    static unsigned int getModuleSize(void) { return s_module_size; }
    static int getModuleMaxOffset(void) { return s_module_max_offset; }

    void setAshmemBase(unsigned int *ashmemBase);
    int32_t setTuningField(int32_t offset, int32_t value);
    int32_t getTuningField(int32_t offset, int32_t *value);
    bool setPQValueToAshmem(ProxyTuningBuffer proxyNum, int32_t field, int32_t value, int32_t scenario);
    bool getPQValueFromAshmem(ProxyTuningBuffer proxyNum, int32_t field, int32_t *value);

private:
    bool isOverwritten(int32_t offset);
    bool isReading(int32_t offset);
    int32_t pushToMDP(int32_t offset);
    int32_t pullFromMDP(int32_t offset, int32_t scenario);

private:
    static const int s_module_max_offset = MODULE_MAX_OFFSET;
    static const int s_ashmem_max_offset = PROXY_MAX * s_module_max_offset;
    static const unsigned int s_module_size = s_module_max_offset * sizeof(unsigned int);
    static const unsigned int s_ashmem_size = s_ashmem_max_offset * sizeof(unsigned int);

    const int m_tunning_magic_num = s_module_max_offset - (PROXY_TUNING_FLAG+1);
    const int m_sync_magic_num = s_module_max_offset - (PROXY_SYNC_FLAG+1);
    const int m_sync_ok_magic_num = s_module_max_offset - (PROXY_SYNC_OK_FLAG+1);
    const int m_prepare_magic_num = s_module_max_offset - (PROXY_PREPARE_FLAG+1);
    const int m_ready_magic_num = s_module_max_offset - (PROXY_READY_FLAG+1);

    unsigned int *m_ashmem_base;
};
#endif
