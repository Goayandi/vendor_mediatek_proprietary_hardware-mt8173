/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "ThermalHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/thermal.h>



#define TH_LOG_TAG "thermal_hal"
#define TH_LOG(_priority_, _fmt_, args...)  LOG_PRI(_priority_, TH_LOG_TAG, _fmt_, ##args)


#define CPU_LABEL               "CPU"
#define MAX_LENGTH              50

#define CPU_USAGE_FILE          "/proc/stat"
#define TEMPERATURE_DIR         "/proc/mtktz"
#define THERMAL_DIR             "mtkts"
#define CPU_ONLINE_FILE_FORMAT  "/sys/devices/system/cpu/cpu%d/online"
#define UNKNOWN_LABEL           "UNKNOWN"

static ssize_t get_temperatures(thermal_module_t *module, temperature_t *list, size_t size) {
    char file_name[MAX_LENGTH];
    FILE *file;
    float temp;
    size_t idx = 0;
    DIR *dir;
    struct dirent *de;

    /** Read all available temperatures from
     * /sys/class/thermal/thermal_zone[0-9]+/temp files.
     * Don't guarantee that all temperatures are in Celsius. */
    dir = opendir(TEMPERATURE_DIR);
    if (dir == 0) {
        ALOGE("%s: failed to open directory %s: %s", __func__, TEMPERATURE_DIR, strerror(-errno));
        return -errno;
    }

    while ((de = readdir(dir))) {
        if (!strncmp(de->d_name, THERMAL_DIR, strlen(THERMAL_DIR))) {
            snprintf(file_name, MAX_LENGTH, "%s/%s", TEMPERATURE_DIR, de->d_name);
            file = fopen(file_name, "r");
            if (file == NULL) {
                continue;
            }
            if (1 != fscanf(file, "%f", &temp)) {
                fclose(file);
                continue;
            }

            if (list != NULL && idx < size) {
                list[idx] = (temperature_t) {
                    .name = UNKNOWN_LABEL,
                    .type = DEVICE_TEMPERATURE_UNKNOWN,
                    .current_value = temp,
                    .throttling_threshold = UNKNOWN_TEMPERATURE,
                    .shutdown_threshold = UNKNOWN_TEMPERATURE,
                    .vr_throttling_threshold = UNKNOWN_TEMPERATURE,
                };
            }
            fclose(file);
            idx++;
        }
    }
    closedir(dir);
    return idx;
}


static ssize_t get_cpu_usages(thermal_module_t *module, cpu_usage_t *list) {
    int vals, cpu_num, online;
    ssize_t read;
    uint64_t user, nice, system, idle, active, total;
    char *line = NULL;
    size_t len = 0;
    size_t size = 0;
    char file_name[MAX_LENGTH];
    FILE *cpu_file;
    FILE *file = fopen(CPU_USAGE_FILE, "r");

    if (file == NULL) {
        ALOGE("%s: failed to open: %s", __func__, strerror(errno));
        return -errno;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        // Skip non "cpu[0-9]" lines.
        if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
            free(line);
            line = NULL;
            len = 0;
            continue;
        }
        vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                &nice, &system, &idle);

        free(line);
        line = NULL;
        len = 0;

        if (vals != 5) {
            ALOGE("%s: failed to read CPU information from file: %s", __func__, strerror(errno));
            fclose(file);
            return errno ? -errno : -EIO;
        }

        active = user + nice + system;
        total = active + idle;

        // Read online CPU information.
        snprintf(file_name, MAX_LENGTH, CPU_ONLINE_FILE_FORMAT, cpu_num);
        cpu_file = fopen(file_name, "r");
        online = 0;
        if (cpu_file == NULL) {
            ALOGE("%s: failed to open file: %s (%s)", __func__, file_name, strerror(errno));
            // /sys/devices/system/cpu/cpu0/online is missing on some systems, because cpu0 can't
            // be offline.
            online = cpu_num == 0;
        } else if (1 != fscanf(cpu_file, "%d", &online)) {
            ALOGE("%s: failed to read CPU online information from file: %s (%s)", __func__,
                    file_name, strerror(errno));
            fclose(file);
            fclose(cpu_file);
            return errno ? -errno : -EIO;
        }
        fclose(cpu_file);

        if (list != NULL) {
            list[size] = (cpu_usage_t) {
                .name = CPU_LABEL,
                .active = active,
                .total = total,
                .is_online = online
            };
        }

        size++;
    }

    fclose(file);
    return size;
}

static ssize_t get_cooling_devices(thermal_module_t *module, cooling_device_t *list, size_t size) {
    return 0;
}

static struct hw_module_methods_t mtk_thermal_module_methods = {
    .open = NULL,
};

thermal_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = THERMAL_HARDWARE_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = THERMAL_HARDWARE_MODULE_ID,
        .name = "MTK Thermal HAL",
        .author = "The Android Open Source Project",
        .methods = &mtk_thermal_module_methods,
    },

    .getTemperatures = get_temperatures,
    .getCpuUsages = get_cpu_usages,
    .getCoolingDevices = get_cooling_devices,
};






