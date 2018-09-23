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

#include "Activity.h"
#include <utils/SystemClock.h>
#include <utils/Timers.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "ACT"
#endif

#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH           "/dev/m_act_misc"


/*****************************************************************************/
ActivitySensor::ActivitySensor()
    : SensorBase(NULL, "m_act_input"),//ACT_INPUTDEV_NAME
      mEnabled(0),
      mSensorReader(BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_ACTIVITY;
    mPendingEvent.type = SENSOR_TYPE_ACTIVITY;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;
    mEnabledTime =0;
    mDataDiv = 1;
    mPendingEvent.timestamp =0;
    input_sysfs_path_len = 0;
    //input_sysfs_path[PATH_MAX] = {0};
    memset(input_sysfs_path, 0, sizeof(char)*PATH_MAX);
    m_act_last_ts = 0;
    m_act_delay= 0;
    batchMode=0;
    firstData = 1;
    char datapath[64]={"/sys/class/sensor/m_act_misc/actactive"};
    int fd = -1;
    char buf[64];
    int len;

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_act_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    } else {
        ALOGE("couldn't find input device ");
        return;
    }
    ALOGD("act misc path =%s", input_sysfs_path);

    fd = open(datapath, O_RDWR);
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

ActivitySensor::~ActivitySensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}
int ActivitySensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    ALOGE_IF(fd < 0, "couldn't find input device");
    return fd;
}

int ActivitySensor::enable(int32_t handle, int en)
{
    int fd;
    int flags = en ? 1 : 0;

    ALOGD("Act enable: handle:%d, en:%d \r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "actactive");
    ALOGD("path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no Act enable control attr\r\n" );
        return -1;
    }

    mEnabled = flags;
    char buf[2];
    buf[1] = 0;
    if (flags)  {
        buf[0] = '1';
        mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
    } else {
        buf[0] = '0';
    }
    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("Act enable(%d) done", mEnabled);
    return 0;
}

int ActivitySensor::setDelay(int32_t handle, int64_t ns)
{
    int fd;
    ALOGD("setDelay: (handle=%d, ns=%lld)", handle, ns);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "actdelay");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no ACT setDelay control attr \r\n" );
        return -1;
    }
    char buf[80];
    sprintf(buf, "%lld", ns);
    write(fd, buf, strlen(buf)+1);
    close(fd);
    return 0;
}

int ActivitySensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd = -1;
    char buf[128] = {0};

    ALOGD("act batch: handle:%d, flag:%d,samplingPeriodNs:%lld maxBatchReportLatencyNs:%lld\r\n",
        handle, flags,samplingPeriodNs, maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "actbatch");
    ALOGD("act batch path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no act batch control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int ActivitySensor::flush(int handle)
{
    int fd = -1;
    char buf[32] = {0};

    ALOGD("act flush\n");
    strcpy(&input_sysfs_path[input_sysfs_path_len], "actflush");
    ALOGD("act flush path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no act flush control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d", handle);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int ActivitySensor::readEvents(sensors_event_t* data, int count)
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

void ActivitySensor::processEvent(struct sensor_event const *event)
{
    if (event->flush_action == FLUSH_ACTION) {
        mPendingEvent.version = META_DATA_VERSION;
        mPendingEvent.sensor = 0;
        mPendingEvent.type = SENSOR_TYPE_META_DATA;
        mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
        mPendingEvent.meta_data.sensor = ID_ACTIVITY;
        // must fill timestamp, if not, readEvents may can not report flush to framework
        mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
        ALOGD("activity flush complete\n");
    } else if (event->flush_action == DATA_ACTION) {
        mPendingEvent.version = sizeof(sensors_event_t);
        mPendingEvent.sensor = ID_ACTIVITY;
        mPendingEvent.type = SENSOR_TYPE_ACTIVITY;
        mPendingEvent.timestamp = event->time_stamp;
        mPendingEvent.data[0] = event->byte[0];
        mPendingEvent.data[1] = event->byte[1];
        mPendingEvent.data[2] = event->byte[2];
        mPendingEvent.data[3] = event->byte[3];
        mPendingEvent.data[4] = event->byte[4];
        mPendingEvent.data[5] = event->byte[5];
        mPendingEvent.data[6] = event->byte[6];
        mPendingEvent.data[7] = event->byte[7];
        mPendingEvent.data[8] = event->byte[8];
        mPendingEvent.data[9] = event->byte[9];
        mPendingEvent.data[10] = event->byte[10];
        mPendingEvent.data[11] = event->byte[11];
    }
}
