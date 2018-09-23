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

#include <utils/Timers.h>
#include <cutils/log.h>

#include "Gyroscope.h"
#include <utils/SystemClock.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "GYRO"
#endif

#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH           "/dev/m_gyro_misc"
#define GYRO_BIAS_SAVED_DIR    "/data/misc/sensor/gyro_saved.json"
#define GYRO_SETTING_SAVED_DIR "/data/misc/sensor/gyro_setting.json"
#define GYRO_TAG_NAME          "gyro"
/*****************************************************************************/
GyroscopeSensor::GyroscopeSensor()
    : SensorBase(NULL, "m_gyro_input"),//GYRO_INPUTDEV_NAME
      mEnabled(0),
      mSensorReader(BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_GYROSCOPE;
    mPendingEvent.type = SENSOR_TYPE_GYROSCOPE;
    mPendingEvent.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;
    mEnabledTime =0;
    mFlushCnt = 0;
    mDataDiv = 1;
    mPendingEvent.timestamp = 0;
    input_sysfs_path_len = 0;
    memset(input_sysfs_path, 0, PATH_MAX);

    char datapath[64]={"/sys/class/sensor/m_gyro_misc/gyroactive"};
    int fd = -1;
    char buf[64]={0};
    int len;
    android::sp<android::JSONObject> bias_saved;
    android::sp<android::JSONObject> settings_saved;
    float gyroBias[3] = {0};
    int32_t sendBias[3] = {0};

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_gyro_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    } else {
        ALOGE("couldn't find input device ");
        return;
    }
    ALOGD("gyro misc path =%s", input_sysfs_path);

    fd = open(datapath, O_RDWR);
    if (fd >= 0) {
        len = read(fd,buf,sizeof(buf)-1);
        if (len <= 0) {
            ALOGD("read div err, len = %d", len);
        } else {
            buf[len] = '\0';
            sscanf(buf, "%d", &mDataDiv);
            ALOGD("read div buf(%s), mdiv %d", datapath, mDataDiv);
        }
        close(fd);
    } else {
        ALOGE("open gyro misc path %s fail ", datapath);
    }
    mSensorCali.loadSensorSettings(GYRO_BIAS_SAVED_DIR, &bias_saved,
        GYRO_SETTING_SAVED_DIR, &settings_saved);
    if (mSensorCali.getCalibrationFloat(bias_saved, GYRO_TAG_NAME, gyroBias)) {
        ALOGE("gyro read bias: [%f, %f, %f]\n", gyroBias[0], gyroBias[1], gyroBias[2]);
        strcpy(&input_sysfs_path[input_sysfs_path_len], "gyrocali");
        fd = open(input_sysfs_path, O_RDWR);
        if (fd >= 0) {
            sendBias[0] = (int32_t)(gyroBias[0] * mDataDiv);
            sendBias[1] = (int32_t)(gyroBias[1] * mDataDiv);
            sendBias[2] = (int32_t)(gyroBias[2] * mDataDiv);
            write(fd, sendBias, sizeof(sendBias));
            close(fd);
        } else
            ALOGE("no gyroscope cali attr\r\n");
    } else {
        strcpy(&input_sysfs_path[input_sysfs_path_len], "gyrocali");
        fd = open(input_sysfs_path, O_RDWR);
        if (fd >= 0) {
            sendBias[0] = (int32_t)(gyroBias[0] * mDataDiv);
            sendBias[1] = (int32_t)(gyroBias[1] * mDataDiv);
            sendBias[2] = (int32_t)(gyroBias[2] * mDataDiv);
            write(fd, sendBias, sizeof(sendBias));
            close(fd);
        } else
            ALOGE("no gyroscope cali attr\r\n");
    }
}

GyroscopeSensor::~GyroscopeSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}
int GyroscopeSensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    ALOGE_IF(fd<0, "couldn't find sensor device");
    return fd;
}

int GyroscopeSensor::enableNoHALData(int en) {
    int fd = 0;
    char buf[2] = {0};
    ALOGD("GYRO enable nodata en(%d) \r\n", en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "gyroenablenodata");
    ALOGD("path:%s \r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
          ALOGD("no GYRO enable nodata control attr\r\n");
          return -1;
    }

    buf[1] = 0;
    if (1 == en) {
        buf[0] = '1';
    }
    if (0 == en) {
        buf[0] = '0';
    }

    write(fd, buf, sizeof(buf));
      close(fd);

    ALOGD("GYRO enable nodata done");
    return 0;
}
int GyroscopeSensor::enable(int32_t handle, int en)
{
    int fd=-1;
    int flags = en ? 1 : 0;

    ALOGD("Gyro enable: handle:%d, en:%d\r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "gyroactive");
    ALOGD("path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no Gyro enable control attr\r\n" );
        return -1;
    }

    mEnabled = flags;
    char buf[2]={0};
    buf[1] = 0;
    if (flags) {
        buf[0] = '1';
        mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
    } else {
        buf[0] = '0';
    }
    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("Gyro enable(%d) done", mEnabled );
    return 0;
}

int GyroscopeSensor::setDelay(int32_t handle, int64_t ns)
{
    int fd=-1;
    ALOGD("setDelay: (handle=%d, ns=%lld)",handle, ns);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "gyrodelay");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no GYRO setDelay control attr \r\n" );
        return -1;
    }
    char buf[80]={0};
    sprintf(buf, "%lld", ns);
    write(fd, buf, strlen(buf)+1);
    close(fd);
    return 0;
}

int GyroscopeSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd = -1;
    char buf[128] = {0};

    ALOGD("gyro batch: handle:%d, flag:%d,samplingPeriodNs:%lld maxBatchReportLatencyNs:%lld\r\n",
        handle, flags,samplingPeriodNs, maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "gyrobatch");
    ALOGD("path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no gyro batch control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int GyroscopeSensor::flush(int handle)
{
    int fd = -1;
    char buf[32] = {0};

    mFlushCnt++;
    ALOGD("gyro flush, flushCnt:%d\n", mFlushCnt);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "gyroflush");
    ALOGD("gyro flush path:%s\r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no gyro flush control attr\r\n");
        return -1;
    }
    sprintf(buf, "%d", handle);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int GyroscopeSensor::readEvents(sensors_event_t* data, int count)
{
    //ALOGE("fwq read Event 1\r\n");
    if (count < 1)
        return -EINVAL;

    ssize_t n = mSensorReader.fill(mdata_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    struct sensor_event const* event;

    while (count && mSensorReader.readEvent(&event)) {
        processEvent(event);
        if (event->flush_action != BIAS_ACTION) {
            if (mEnabled) {
                if (mPendingEvent.timestamp > mEnabledTime) {
                    *data++ = mPendingEvent;
                    numEventReceived++;
                    count--;
                }
            }
        }
        mSensorReader.next();
    }
    return numEventReceived;
}

void GyroscopeSensor::processEvent(struct sensor_event const *event)
{
    float gyroBias[3] = {0};
    android::sp<android::JSONObject> bias_saved;

    if (event->flush_action == FLUSH_ACTION) {
        mPendingEvent.version = META_DATA_VERSION;
        mPendingEvent.sensor = 0;
        mPendingEvent.type = SENSOR_TYPE_META_DATA;
        mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
        mPendingEvent.meta_data.sensor = ID_GYROSCOPE;
        // must fill timestamp, if not, readEvents may can not report flush to framework
        mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
        mFlushCnt--;
        ALOGD("gyro flush complete, flushCnt:%d\n", mFlushCnt);
    } else if (event->flush_action == DATA_ACTION) {
        mPendingEvent.version = sizeof(sensors_event_t);
        mPendingEvent.sensor = ID_GYROSCOPE;
        mPendingEvent.type = SENSOR_TYPE_GYROSCOPE;
        mPendingEvent.timestamp = event->time_stamp;
        mPendingEvent.gyro.status = event->status;
        mPendingEvent.gyro.x = (float)event->word[0] / mDataDiv;
        mPendingEvent.gyro.y = (float)event->word[1] / mDataDiv;
        mPendingEvent.gyro.z = (float)event->word[2] / mDataDiv;
    } else if (event->flush_action == BIAS_ACTION) {
        gyroBias[0] = (float)event->word[0] / mDataDiv;
        gyroBias[1] = (float)event->word[1] / mDataDiv;
        gyroBias[2] = (float)event->word[2] / mDataDiv;
        mSensorCali.saveSensorSettings(GYRO_BIAS_SAVED_DIR, GYRO_TAG_NAME, gyroBias);
        ALOGE("gyro write bias: [%f, %f, %f]\n", gyroBias[0], gyroBias[1], gyroBias[2]);
    }
}
