#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <hardware/fused_location.h>
#define LIBNAME "libmtkflp.so"

#ifndef UNUSED
#define UNUSED(x) (x)=(x)
#endif

static int open_flp(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device) {
    void *init;
    struct flp_device_t *dev = malloc(sizeof(struct flp_device_t));
    UNUSED(name);
    memset(dev, 0, sizeof(*dev));
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    init = dlopen(LIBNAME, RTLD_LAZY);
    if(init) {
        dev->get_flp_interface = dlsym(init, "mtk_flp_get_flp_interface");
    } else {
        dev->get_flp_interface = 0;
    }
    *device = (struct hw_device_t*)dev;
    return 0;
}

static struct hw_module_methods_t flp_module_methods = {
    .open = open_flp
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = FUSED_LOCATION_HARDWARE_MODULE_ID,
    .name = "Hardware FLP Module",
    .author = "The MTK FLP Source Project",
    .methods = &flp_module_methods,
};
