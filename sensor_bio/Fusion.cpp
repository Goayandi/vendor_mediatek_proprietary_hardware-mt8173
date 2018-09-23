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
#include <cutils/log.h>
#include <utils/SystemClock.h>
#include <utils/Timers.h>
#include <string.h>
#include "Fusion.h"
#undef LOG_TAG
#define LOG_TAG "FUSION"


#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH           "/dev/m_fusion_misc"


/*****************************************************************************/
FusionSensor::FusionSensor()
    : SensorBase(NULL, "m_fusion_input"),//FUSION_INPUTDEV_NAME
      mSensorReader(BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS) {
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;

    memset(mEnabledTime, 0, sizeof(mEnabledTime));
    memset(mEnabled, 0, sizeof(mEnabled));
    memset(mFlushCnt, 0, sizeof(mFlushCnt));
    memset(mDataDiv, 0, sizeof(mDataDiv));
    input_sysfs_path_len = 0;

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_fusion_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
    //ALOGD("fusion misc path =%s", input_sysfs_path);

    char datapath[64]={"/sys/class/sensor/m_fusion_misc/fusionactive"};
    int fd = open(datapath, O_RDWR);
    char buf[64];
    int len;

    if (fd >= 0) {
        len = read(fd, buf, sizeof(buf)-1);
        if (len <= 0) {
            ALOGD("read div err buf(%s)", buf);
        } else {
            buf[len] = '\0';
            sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &mDataDiv[orientation], &mDataDiv[grv], &mDataDiv[gmrv],
                &mDataDiv[rv], &mDataDiv[la], &mDataDiv[grav], &mDataDiv[ungyro], &mDataDiv[unmag], &mDataDiv[pdr]);
        }
        close(fd);
    } else {
        ALOGE("open fusion misc path %s fail ", datapath);
    }
}

FusionSensor::~FusionSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}
int FusionSensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    //ALOGE_IF(fd<0, "couldn't find input device");
    return fd;
}
int FusionSensor::HandleToIndex(int handle) {
    switch (handle) {
    case ID_ORIENTATION:
        return orientation;
    case ID_GAME_ROTATION_VECTOR:
        return grv;
    case ID_GEOMAGNETIC_ROTATION_VECTOR:
        return gmrv;
    case ID_ROTATION_VECTOR:
        return rv;
    case ID_LINEAR_ACCELERATION:
        return la;
    case ID_GRAVITY:
        return grav;
    case ID_GYROSCOPE_UNCALIBRATED:
        return ungyro;
    case ID_MAGNETIC_UNCALIBRATED:
        return unmag;
    case ID_PDR:
        return pdr;
    defult:
        ALOGE("HandleToIndex(%d) err\n", handle);
    }

    return -1;
}
int FusionSensor::enable(int32_t handle, int en)
{
    int fd, index = -1;
    int flags = en ? 1 : 0;
    char buf[8] = {0};

    strcpy(&input_sysfs_path[input_sysfs_path_len], "fusionactive");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    index = HandleToIndex(handle);
    sprintf(buf, "%d,%d", handle, flags);
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

int FusionSensor::setDelay(int32_t handle, int64_t ns)
{
    int fd = -1;
    ALOGD("setDelay: (handle=%d, ns=%lld)", handle, ns);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "fusiondelay");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no fusion setDelay control attr\r\n");
        return -1;
    }
    char buf[80] = {0};
    sprintf(buf, "%d,%lld", handle, ns);
    write(fd, buf, strlen(buf)+1);
    close(fd);
    return 0;
}

int FusionSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd = -1;
    char buf[128] = {0};

    ALOGE("fusion batch: handle:%d, flag:%d,samplingPeriodNs:%lld maxBatchReportLatencyNs:%lld\r\n",
        handle, flags,samplingPeriodNs, maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "fusionbatch");
    ALOGD("fusion batch path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no fusion batch control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int FusionSensor::flush(int handle)
{
    int fd = -1, index = -1;
    char buf[32] = {0};

    index = HandleToIndex(handle);
    mFlushCnt[index]++;
    ALOGD("fusion flush, handle:%d, mFlushCnt:%d\n", handle, mFlushCnt[index]);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "fusionflush");
    ALOGD("fusion flush path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd < 0) {
        ALOGD("no fusion flush control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d", handle);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int FusionSensor::readEvents(sensors_event_t* data, int count)
{
    int index = -1;

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
        if (mEnabled[index]) {
            if (mPendingEvent.timestamp > mEnabledTime[index]) {
                *data++ = mPendingEvent;
                numEventReceived++;
                count--;
            }
        }
        mSensorReader.next();
    }
    return numEventReceived;
}

void FusionSensor::processEvent(struct sensor_event const *event)
{
    int index = HandleToIndex(event->handle);

    switch (event->handle) {
    case ID_ORIENTATION:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_ORIENTATION;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("orientation flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_ORIENTATION;
            mPendingEvent.type = SENSOR_TYPE_ORIENTATION;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.orientation.x = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.orientation.y = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.orientation.z = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.orientation.status = event->status;
        }
        break;
    case ID_GAME_ROTATION_VECTOR:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_GAME_ROTATION_VECTOR;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("grv flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_GAME_ROTATION_VECTOR;
            mPendingEvent.type = SENSOR_TYPE_GAME_ROTATION_VECTOR;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.data[0] = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.data[1] = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.data[2] = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.data[3] = (float)event->word[3] / mDataDiv[index];
        }
        break;
    case ID_GEOMAGNETIC_ROTATION_VECTOR:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_GEOMAGNETIC_ROTATION_VECTOR;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("gmrv flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_GEOMAGNETIC_ROTATION_VECTOR;
            mPendingEvent.type = SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.data[0] = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.data[1] = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.data[2] = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.data[3] = (float)event->word[3] / mDataDiv[index];
        }
        break;
    case ID_ROTATION_VECTOR:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_ROTATION_VECTOR;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("rv flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_ROTATION_VECTOR;
            mPendingEvent.type = SENSOR_TYPE_ROTATION_VECTOR;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.data[0] = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.data[1] = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.data[2] = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.data[3] = (float)event->word[3] / mDataDiv[index];
        }
        break;
    case ID_LINEAR_ACCELERATION:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_LINEAR_ACCELERATION;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("la flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_LINEAR_ACCELERATION;
            mPendingEvent.type = SENSOR_TYPE_LINEAR_ACCELERATION;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.acceleration.x = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.acceleration.y = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.acceleration.z = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.acceleration.status = event->status;
        }
        break;
    case ID_GRAVITY:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_GRAVITY;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("grav flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_GRAVITY;
            mPendingEvent.type = SENSOR_TYPE_GRAVITY;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.acceleration.x = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.acceleration.y = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.acceleration.z = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.acceleration.status = event->status;
        }
        break;
    case ID_GYROSCOPE_UNCALIBRATED:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_GYROSCOPE_UNCALIBRATED;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("ungyro flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_GYROSCOPE_UNCALIBRATED;
            mPendingEvent.type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.uncalibrated_gyro.x_uncalib = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.uncalibrated_gyro.y_uncalib = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.uncalibrated_gyro.z_uncalib = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.uncalibrated_gyro.x_bias = (float)event->word[3] / mDataDiv[index];
            mPendingEvent.uncalibrated_gyro.y_bias = (float)event->word[4] / mDataDiv[index];
            mPendingEvent.uncalibrated_gyro.z_bias = (float)event->word[5] / mDataDiv[index];
        }
        break;
    case ID_MAGNETIC_UNCALIBRATED:
        if (event->flush_action == FLUSH_ACTION) {
            mPendingEvent.version = META_DATA_VERSION;
            mPendingEvent.sensor = 0;
            mPendingEvent.type = SENSOR_TYPE_META_DATA;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.meta_data.sensor = ID_MAGNETIC_UNCALIBRATED;
            // must fill timestamp, if not, readEvents may can not report flush to framework
            mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
            mFlushCnt[index]--;
            ALOGD("unmag flush complete, mFlushCnt:%d\n", mFlushCnt[index]);
        } else if (event->flush_action == DATA_ACTION) {
            mPendingEvent.version = sizeof(sensors_event_t);
            mPendingEvent.sensor = ID_MAGNETIC_UNCALIBRATED;
            mPendingEvent.type = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
            mPendingEvent.timestamp = event->time_stamp;
            mPendingEvent.uncalibrated_magnetic.x_uncalib = (float)event->word[0] / mDataDiv[index];
            mPendingEvent.uncalibrated_magnetic.y_uncalib = (float)event->word[1] / mDataDiv[index];
            mPendingEvent.uncalibrated_magnetic.z_uncalib = (float)event->word[2] / mDataDiv[index];
            mPendingEvent.uncalibrated_magnetic.x_bias = (float)event->word[3] / mDataDiv[index];
            mPendingEvent.uncalibrated_magnetic.y_bias = (float)event->word[4] / mDataDiv[index];
            mPendingEvent.uncalibrated_magnetic.z_bias = (float)event->word[5] / mDataDiv[index];
        }
        break;
    }
}
