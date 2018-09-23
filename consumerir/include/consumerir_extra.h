/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_INCLUDE_HARDWARE_CONSUMERIR_EXTRA_H
#define ANDROID_INCLUDE_HARDWARE_CONSUMERIR_EXTRA_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer_defs.h>
#include <hardware/consumerir.h>

typedef struct consumerir_extra_device {
    /**
     * Common methods of the consumer IR device.  This *must* be the first member of
     * consumerir_device as users of this structure will cast a hw_device_t to
     * consumerir_device pointer in contexts where it's known the hw_device_t references a
     * consumerir_device.
     */
    struct consumerir_device common;

    /*
     * (*receive)() is called to by the ConsumerIrExtraService to receive an IR pattern
     * and carrier_freq from IR receiver hardware
     *
     * The pattern is alternating series of carrier on and off periods measured in
     * microseconds.
     *
     * This call should return when the receive is complete or encounters an error.
     *
     * returns: 0 on success. A negative error code on error.
     */
    int (*receive)(struct consumerir_extra_device *dev,unsigned int *sample_rate,
                   unsigned char *buffer, int *len);

} consumerir_extra_device_t;

#endif /* ANDROID_INCLUDE_HARDWARE_CONSUMERIR_EXTRA_H */
