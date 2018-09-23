/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2012. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */


#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <utils/SystemClock.h>
#include <utils/Timers.h>
#include <string.h>
#include "Gesture.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "[Gesture Sensor]"
#endif

#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH "/dev/m_ges_misc"

GestureSensor::GestureSensor()
    : SensorBase(NULL, "GES_INPUTDEV_NAME"),
      mSensorReader(32){

    mPendingEvent.version = sizeof(sensors_event_t);
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;

    memset(mEnabledTime, 0, sizeof(mEnabledTime));
    memset(mEnabled, 0, sizeof(mEnabled));

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_ges_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
    ALOGD("misc path =%s", input_sysfs_path);
}

GestureSensor::~GestureSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}

int GestureSensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    ALOGE_IF(fd < 0, "couldn't find input device");
    return fd;
}

int GestureSensor::HandleToIndex(int handle) {
    switch (handle) {
    case ID_IN_POCKET:
        return inpocket;
    case ID_STATIONARY:
        return stationary;
    case ID_WAKE_GESTURE:
        return wake_gesture;
    case ID_GLANCE_GESTURE:
        return glance_gesture;
    case ID_PICK_UP_GESTURE:
        return pickup_gesture;
    case ID_ANSWER_CALL:
        return answer_call;
    defult:
        ALOGE("HandleToIndex(%d) err\n", handle);
    }

    return -1;
}
int GestureSensor::enable(int32_t handle, int en) {
    int fd;
    int index;
    char buf[8];
    int flags = en ? 1 : 0;

    ALOGD("enable: handle:%d, en:%d \r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "gesactive");
    ALOGD("path:%s \r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no gesture enable control attr\r\n");
        return -1;
    }

    index = HandleToIndex(handle);

    sprintf(buf, "%d : %d", handle, flags);
    if (flags) {
        mEnabled[index] = true;
        mEnabledTime[index] = getTimestamp() + IGNORE_EVENT_TIME;
    } else {
        mEnabled[index] = false;
    }

    write(fd, buf, sizeof(buf));
    close(fd);
    ALOGD("enable(%d) done", mEnabled[index]);
    return 0;
}

int GestureSensor::setDelay(int32_t handle, int64_t ns) {
    ALOGD("setDelay: regardless of the setDelay() value (handle=%d, ns=%lld)", handle, ns);
    return 0;
}

int GestureSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs) {
    int fd = -1;
    char buf[128] = {0};

    ALOGD("ges batch: handle:%d, flag:%d,samplingPeriodNs:%lld maxBatchReportLatencyNs:%lld\r\n",
        handle, flags,samplingPeriodNs, maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "gesbatch");
    ALOGD("ges batch path:%s\r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no ges batch control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int GestureSensor::flush(int handle) {
    ALOGD("onshot sensor do not support flush, handle:%d\r\n", handle);
    return -errno;
}

int GestureSensor::readEvents(sensors_event_t* data, int count) {
    int index;

    if (count < 1)
        return -EINVAL;

    ssize_t n = mSensorReader.fill(mdata_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    struct sensor_event const* event;

    while (count && mSensorReader.readEvent(&event)) {
        processEvent(event);
        index = HandleToIndex(event->handle);
        if (mEnabled[index] == true) {
            if (mPendingEvent.timestamp >= mEnabledTime[index]) {
                *data++ = mPendingEvent;
                numEventReceived++;
                count--;
                enable(event->handle, false);
            }
        }
        mSensorReader.next();
    }
    return numEventReceived;
}
void GestureSensor::processEvent(struct sensor_event const *event) {
    ALOGE("GestureSensor processEvent, handle:%d, flush_action:%d\n", event->handle, event->flush_action);
    switch (event->handle) {
    case ID_IN_POCKET:
        if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_IN_POCKET;
            mPendingEvent.type = SENSOR_TYPE_IN_POCKET;
            mPendingEvent.timestamp = android::elapsedRealtimeNano();
            mPendingEvent.data[0] = (float) event->word[0];
        }
        break;
    case ID_STATIONARY:
        if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_STATIONARY;
            mPendingEvent.type = SENSOR_TYPE_STATIONARY;
            mPendingEvent.timestamp = android::elapsedRealtimeNano();
            mPendingEvent.data[0] = (float) event->word[0];
        }
        break;
    case ID_WAKE_GESTURE:
        if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_WAKE_GESTURE;
            mPendingEvent.type = SENSOR_TYPE_WAKE_GESTURE;
            mPendingEvent.timestamp = android::elapsedRealtimeNano();
            mPendingEvent.data[0] = (float) event->word[0];
        }
        break;
    case ID_PICK_UP_GESTURE:
        if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_PICK_UP_GESTURE;
            mPendingEvent.type = SENSOR_TYPE_PICK_UP_GESTURE;
            mPendingEvent.timestamp = android::elapsedRealtimeNano();
            mPendingEvent.data[0] = (float) event->word[0];
        }
        break;
    case ID_GLANCE_GESTURE:
        if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_GLANCE_GESTURE;
            mPendingEvent.type = SENSOR_TYPE_GLANCE_GESTURE;
            mPendingEvent.timestamp = android::elapsedRealtimeNano();
            mPendingEvent.data[0] = (float) event->word[0];
        }
        break;
    case ID_ANSWER_CALL:
        if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_ANSWER_CALL;
            mPendingEvent.type = SENSOR_TYPE_ANSWER_CALL;
            mPendingEvent.timestamp = android::elapsedRealtimeNano();
            mPendingEvent.data[0] = (float) event->word[0];
        }
        break;
    }
}
