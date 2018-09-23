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

#include "Tilt.h"
#include <utils/SystemClock.h>
#include <utils/Timers.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "TILT"
#endif

#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH           "/dev/m_tilt_misc"


/*****************************************************************************/
TiltSensor::TiltSensor()
    : SensorBase(NULL, "m_tilt_input"),//_INPUTDEV_NAME
      mEnabled(0),
      mSensorReader(32)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_TILT_DETECTOR;
    mPendingEvent.type = SENSOR_TYPE_TILT_DETECTOR;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;
    mEnabledTime =0;
    mDataDiv = 1;
    mPendingEvent.timestamp =0;
    input_sysfs_path_len = 0;
    //input_sysfs_path[PATH_MAX] = {0};
    memset(input_sysfs_path, 0, sizeof(char)*PATH_MAX);

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_tilt_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
    ALOGD("Tilt misc path =%s", input_sysfs_path);

    char datapath[64]={"/sys/class/sensor/m_tilt_misc/tiltactive"};
    int fd = open(datapath, O_RDWR);
    char buf[64];
    int len;
    if (fd >= 0) {
        len = read(fd,buf,sizeof(buf)-1);
        if (len <= 0) {
            ALOGD("read div err buf(%s)",buf );
        } else {
            buf[len] = '\0';
            ALOGE("len = %d, buf = %s",len, buf);
            sscanf(buf, "%d", &mDataDiv);
            ALOGD("read div buf(%s)", datapath);
            ALOGD("mdiv %d",mDataDiv );
        }
        close(fd);
    } else {
        ALOGE("open acc misc path %s fail ", datapath);
    }
}

TiltSensor::~TiltSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}

int TiltSensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    ALOGE_IF(fd<0, "couldn't find sensor device");
    return fd;
}

int TiltSensor::enable(int32_t handle, int en)
{
    int fd;
    int flags = en ? 1 : 0;

    ALOGD("tilt enable: handle:%d, en:%d \r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "tiltactive");
    ALOGD("path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no tilt enable control attr\r\n" );
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
    ALOGD("tilt enable(%d) done", mEnabled);
    return 0;
}
int TiltSensor::setDelay(int32_t handle, int64_t ns)
{
    ALOGD("setDelay: regardless of the setDelay() value (handle=%d, ns=%lld)", handle, ns);
    return 0;
}
int TiltSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd = -1;
    char buf[128] = {0};

    ALOGD("tilt batch: handle:%d, flag:%d,samplingPeriodNs:%lld maxBatchReportLatencyNs:%lld\r\n",
        handle, flags,samplingPeriodNs, maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "tiltbatch");
    ALOGD("tilt batch path:%s\r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no tilt batch control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int TiltSensor::flush(int handle)
{
    int fd = -1;
    char buf[32] = {0};

    ALOGD("tilt flush\n");
    strcpy(&input_sysfs_path[input_sysfs_path_len], "tiltflush");
    ALOGD("tilt flush path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no tilt flush control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d", handle);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int TiltSensor::readEvents(sensors_event_t* data, int count)
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
            if (mPendingEvent.timestamp >= mEnabledTime) {
                *data++ = mPendingEvent;
                numEventReceived++;
                count--;
            }
        }
        mSensorReader.next();
    }
    return numEventReceived;
}

void TiltSensor::processEvent(struct sensor_event const* event)
{
    if (event->flush_action == FLUSH_ACTION) {
        mPendingEvent.version = META_DATA_VERSION;
        mPendingEvent.sensor = 0;
        mPendingEvent.type = SENSOR_TYPE_META_DATA;
        mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
        mPendingEvent.meta_data.sensor = ID_TILT_DETECTOR;
        // must fill timestamp, if not, readEvents may can not report flush to framework
        mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
        ALOGD("tilt flush complete\n");
    } else if (event->flush_action == DATA_ACTION) {
        mPendingEvent.version = sizeof(sensors_event_t);
        mPendingEvent.sensor = ID_TILT_DETECTOR;
        mPendingEvent.type = SENSOR_TYPE_TILT_DETECTOR;
        mPendingEvent.timestamp = android::elapsedRealtimeNano();
        mPendingEvent.data[0] = (float) event->word[0];
    }
}
