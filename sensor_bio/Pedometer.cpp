/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <string.h>

#include <cutils/log.h>

#include "Pedometer.h"
#include <utils/SystemClock.h>
#include <utils/Timers.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "PEDO"
#endif

#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH           "/dev/m_pedo_misc"


/*****************************************************************************/
PedometerSensor::PedometerSensor()
    : SensorBase(NULL, "m_pedo_input"),  // PEDO_INPUTDEV_NAME
      mEnabled(0),
      mSensorReader(BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_PEDOMETER;
    mPendingEvent.type = SENSOR_TYPE_PEDOMETER;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;
    mEnabledTime =0;
    mDataDiv = 1;
    mPendingEvent.timestamp =0;
    input_sysfs_path_len = 0;
    memset(input_sysfs_path, 0, PATH_MAX);

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_pedo_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
    ALOGD("pedo misc path =%s", input_sysfs_path);

    char datapath[64] = {"/sys/class/sensor/m_pedo_misc/pedoactive"};
    int fd = open(datapath, O_RDWR);
    char buf[64];
    int len;
    if (fd >= 0) {
        len = read(fd, buf, sizeof(buf) - 1);
        if(len <= 0) {
            ALOGD("read div err buf(%s)", buf);
        } else {
            buf[len] = '\0';
            sscanf(buf, "%d", &mDataDiv);
            ALOGD("read div buf(%s)", datapath);
            ALOGD("mdiv %d", mDataDiv);
        }
        close(fd);
    } else {
        ALOGE("open pedo misc path %s fail", datapath);
    }
}

PedometerSensor::~PedometerSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}
int PedometerSensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    ALOGE_IF(fd < 0, "couldn't find input device");
    return fd;
}

int PedometerSensor::enable(int32_t handle, int en)
{
    int fd;
    int flags = en ? 1 : 0;

    ALOGD("pedo enable: handle:%d, en:%d\r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "pedoactive");
    ALOGD("path:%s\r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no pedo enable control attr\r\n");
        return -1;
    }

    mEnabled = flags;
    char buf[2];
    buf[1] = 0;
    if (flags) {
        buf[0] = '1';
        mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
    } else {
        buf[0] = '0';
    }
    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("pedo enable(%d) done", mEnabled );
    return 0;
}

int PedometerSensor::setDelay(int32_t handle, int64_t ns)
{
    int fd;
    ALOGD("setDelay: (handle=%d, ns=%lld)", handle, ns);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "pedodelay");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no pedo setDelay control attr\r\n");
        return -1;
    }
    char buf[80];
    sprintf(buf, "%lld", ns);
    write(fd, buf, strlen(buf) + 1);
    close(fd);
    return 0;
}

int PedometerSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd;
    char buf[128];

    ALOGD("batch: handle:%d, flag:%d,samplingPeriodNs:%lld maxBatchReportLatencyNs:%lld\r\n",
        handle, flags,samplingPeriodNs, maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "pedobatch");
    ALOGD("path:%s\r\n",input_sysfs_path);

    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no pedo batch control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int PedometerSensor::flush(int handle)
{
    int fd = -1;
    char buf[32] = {0};

    ALOGD("pedo flush\n");
    strcpy(&input_sysfs_path[input_sysfs_path_len], "pedoflush");
    ALOGD("Pedo flush path:%s\r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no pedo flush control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d", handle);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int PedometerSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mSensorReader.fill(mdata_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    struct sensor_event const* event;

    while (count && mSensorReader.readEvent(&event)) {
        processEvent(event);
        if (mEnabled) {
            if (mPendingEvent.timestamp > mEnabledTime) {
                *data++ = mPendingEvent;
                numEventReceived++;
                count--;
            }
        }
        mSensorReader.next();
    }
    return numEventReceived;
}

void PedometerSensor::processEvent(struct sensor_event const *event)
{
    if (event->flush_action == FLUSH_ACTION) {
        mPendingEvent.version = META_DATA_VERSION;
        mPendingEvent.sensor = 0;
        mPendingEvent.type = SENSOR_TYPE_META_DATA;
        mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
        mPendingEvent.meta_data.sensor = ID_PEDOMETER;
        // must fill timestamp, if not, readEvents may can not report flush to framework
        mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
        ALOGD("pedo flush complete\n");
    } else if (event->flush_action == DATA_ACTION) {
        mPendingEvent.version = sizeof(sensors_event_t);
        mPendingEvent.sensor = ID_PEDOMETER;
        mPendingEvent.type = SENSOR_TYPE_PEDOMETER;
        mPendingEvent.timestamp = event->time_stamp;
        mPendingEvent.data[0] = (float) event->word[0] /1000; //change from milli-meter to meter
        mPendingEvent.data[1] = (float) event->word[1] /1024; //frequency div SHIFT_VALUE (define in SCP)
        mPendingEvent.data[2] = event->word[2];
        mPendingEvent.data[3] = (float) event->word[3] /1000; //change from milli-meter to meter
    }
}
