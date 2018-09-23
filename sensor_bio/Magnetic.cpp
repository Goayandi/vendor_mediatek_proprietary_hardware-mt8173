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
#include <string.h>

#include <cutils/log.h>

#include "Magnetic.h"
#include <utils/SystemClock.h>
#include <utils/Timers.h>
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Magnetic"
#endif


#define IGNORE_EVENT_TIME 0
#define DEVICE_PATH           "/dev/m_mag_misc"
#define MAG_BIAS_SAVED_DIR    "/data/misc/sensor/mag_saved.json"
#define MAG_SETTING_SAVED_DIR "/data/misc/sensor/mag_setting.json"
#define MAG_TAG_NAME          "mag"
/*****************************************************************************/
MagneticSensor::MagneticSensor()
    : SensorBase(NULL, "m_mag_input"),//ACC_INPUTDEV_NAME
      mEnabled(0),
      mSensorReader(BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS)
{
    input_sysfs_path_len = 0;
    memset(input_sysfs_path, 0, PATH_MAX);
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_MAGNETIC;
    mPendingEvent.type = SENSOR_TYPE_MAGNETIC_FIELD;
    mPendingEvent.magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));


    mDataDiv = 1;
    mEnabledTime = 0;
    mFlushCnt = 0;
    char datapath1[64]={"/sys/class/sensor/m_mag_misc/magactive"};
    int fd = -1;
    char buf_m[64]={0};
    int len_m;
    android::sp<android::JSONObject> bias_saved;
    android::sp<android::JSONObject> settings_saved;
    float magBias[3] = {0};
    int32_t sendBias[3] = {0};

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/sensor/m_mag_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    } else {
        ALOGE("couldn't find input device\n");
        return;
    }
    ALOGD("mag misc path =%s", input_sysfs_path);

    fd = open(datapath1, O_RDWR);
    if (fd >= 0) {
        len_m = read(fd,buf_m,sizeof(buf_m) - 1);
        if (len_m <= 0) {
            ALOGD("read div err, len_m = %d", len_m);
        } else {
            buf_m[len_m] = '\0';
            sscanf(buf_m, "%d", &mDataDiv);
            ALOGD("read div buf(%s), mdiv_M %d\n", datapath1, mDataDiv);
        }
        close(fd);
    } else {
        ALOGE("open mag misc path %s fail\n", datapath1);
    }
    mSensorCali.loadSensorSettings(MAG_BIAS_SAVED_DIR, &bias_saved,
        MAG_SETTING_SAVED_DIR, &settings_saved);
    if (mSensorCali.getCalibrationFloat(bias_saved, MAG_TAG_NAME, magBias)) {
        ALOGE("mag read bias: [%f, %f, %f]\n", magBias[0], magBias[1], magBias[2]);
        strcpy(&input_sysfs_path[input_sysfs_path_len], "magcali");
        fd = open(input_sysfs_path, O_RDWR);
        if (fd >= 0) {
            sendBias[0] = (int32_t)(magBias[0] * mDataDiv);
            sendBias[1] = (int32_t)(magBias[1] * mDataDiv);
            sendBias[2] = (int32_t)(magBias[2] * mDataDiv);
            write(fd, sendBias, sizeof(sendBias));
            close(fd);
        } else
            ALOGE("no magntic cali attr\r\n");
    } else {
        strcpy(&input_sysfs_path[input_sysfs_path_len], "magcali");
        fd = open(input_sysfs_path, O_RDWR);
        if (fd >= 0) {
            sendBias[0] = (int32_t)(magBias[0] * mDataDiv);
            sendBias[1] = (int32_t)(magBias[1] * mDataDiv);
            sendBias[2] = (int32_t)(magBias[2] * mDataDiv);
            write(fd, sendBias, sizeof(sendBias));
            close(fd);
        } else
            ALOGE("no magntic cali attr\r\n");
    }
}

MagneticSensor::~MagneticSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}

int MagneticSensor::FindDataFd() {
    int fd = -1;

    fd = open(DEVICE_PATH, O_RDONLY);
    ALOGE_IF(fd<0, "couldn't find sensor device");
    return fd;
}
int MagneticSensor::enable(int32_t handle, int en)
{
    int fd = -1;
    int flags = en ? 1 : 0;
    int err = 0;
    char buf[2] = {0};
    int index = 0;
    ALOGD("fwq enable: handle:%d, en:%d \r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "magactive");
    ALOGD("handle(%d),path:%s\r\n", handle, input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no magntic enable attr\r\n");
        return -1;
    }

    if (0 == en) {
       mEnabled = false;
       buf[1] = 0;
       buf[0] = '0';
    }

    if (1 == en) {
        mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
        mEnabled = true;
        buf[1] = 0;
        buf[0] = '1';
    }

    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("mag(%d)  mEnabled(0x%x) ----\r\n", handle, mEnabled);
    return 0;
}
int MagneticSensor::setDelay(int32_t handle, int64_t ns)
{
    //uint32_t ms=0;
    //ms = ns/1000000;
    int err;
    int fd;
    strcpy(&input_sysfs_path[input_sysfs_path_len], "magdelay");

    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no MAG setDelay control attr\r\n" );
        return -1;
    }

    ALOGD("setDelay: (handle=%d, ms=%lld)", handle, ns);
    char buf[80] = {0};
    sprintf(buf, "%lld", (long long int)ns);
    write(fd, buf, strlen(buf)+1);

    close(fd);

    ALOGD("really setDelay: (handle=%d, ns=%lld)", handle, ns);
    return 0;
}
int MagneticSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd = -1;
    int flag = 0;
    char buf[128] = {0};

    ALOGD("Mag batch: handle:%d, en:%d, samplingPeriodNs:%lld,maxBatchReportLatencyNs:%lld\r\n",
        handle, flags, samplingPeriodNs,maxBatchReportLatencyNs);

    strcpy(&input_sysfs_path[input_sysfs_path_len], "magbatch");

    ALOGD("handle(%d),path:%s\r\n", handle, input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no magntic enable attr\r\n");
        return -1;
    }
    sprintf(buf, "%d,%d,%lld,%lld", handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}

int MagneticSensor::flush(int handle)
{
    int fd = -1;
    char buf[32] = {0};

    strcpy(&input_sysfs_path[input_sysfs_path_len], "magflush");
    mFlushCnt++;
    ALOGD("Magnetic flush, flushCnt:%d\n", mFlushCnt);
    ALOGD("handle: %d, flush path:%s\n", handle, input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no handle: %d flush control attr\n", handle);
        return -1;
    }
    sprintf(buf, "%d", handle);
    write(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}
int MagneticSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1) {
        return -EINVAL;
    }

    ssize_t n = mSensorReader.fill(mdata_fd);
    if (n < 0) {
        return n;
    }
    int numEventReceived = 0;
    struct sensor_event const* event;

    while (count && mSensorReader.readEvent(&event)) {
        processEvent(event);
        if (event->flush_action != BIAS_ACTION) {
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

void MagneticSensor::processEvent(struct sensor_event const *event)
{
    float magBias[3] = {0};

    if (event->flush_action == FLUSH_ACTION) {
        mPendingEvent.version = META_DATA_VERSION;
        mPendingEvent.sensor = 0;
        mPendingEvent.type = SENSOR_TYPE_META_DATA;
        mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
        mPendingEvent.meta_data.sensor = ID_MAGNETIC;
        // must fill timestamp, if not, readEvents may can not report flush to framework
        mPendingEvent.timestamp = android::elapsedRealtimeNano() + IGNORE_EVENT_TIME;
        mFlushCnt--;
        ALOGD("magnetic flush complete, flushCnt:%d\n", mFlushCnt);
    } else if (event->flush_action == DATA_ACTION) {
        mPendingEvent.version = sizeof(sensors_event_t);
        mPendingEvent.sensor = ID_MAGNETIC;
        mPendingEvent.type = SENSOR_TYPE_MAGNETIC_FIELD;
        mPendingEvent.timestamp = event->time_stamp;
        mPendingEvent.magnetic.status = event->status;
        mPendingEvent.magnetic.x = (float)event->word[0] / (float)mDataDiv;
        mPendingEvent.magnetic.y = (float)event->word[1] / (float)mDataDiv;
        mPendingEvent.magnetic.z = (float)event->word[2] / (float)mDataDiv;
    } else if (event->flush_action == BIAS_ACTION) {
        magBias[0] = (float)event->word[0] / (float)mDataDiv;
        magBias[1] = (float)event->word[1] / (float)mDataDiv;
        magBias[2] = (float)event->word[2] / (float)mDataDiv;
        mSensorCali.saveSensorSettings(MAG_BIAS_SAVED_DIR, MAG_TAG_NAME, magBias);
        ALOGE("mag write bias: [%f, %f, %f]\n", magBias[0], magBias[1], magBias[2]);
    }
}
