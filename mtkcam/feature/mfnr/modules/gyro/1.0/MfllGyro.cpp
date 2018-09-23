/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2016. All rights reserved.
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
#define LOG_TAG "MfllCore/MfllGyro"

#include "MfllGyro.h"
#include "MfllLog.h"

// AOSP
#include <utils/SystemClock.h> // elapsedRealtime(), uptimeMillis()

// STL
#include <algorithm> // std::find_if
#include <future> // std::future

// gyro queue size is supposed to be defined in makefile
#ifndef MFLL_GYRO_QUEUE_SIZE
#define MFLL_GYRO_QUEUE_SIZE 20
#endif

// enable additional debug info
// #define __DEBUG

using namespace mfll;
using android::sp;

IMfllGyro* IMfllGyro::createInstance()
{
    return reinterpret_cast<IMfllGyro*>(new MfllGyro);
}

void IMfllGyro::destroyInstance()
{
    decStrong((void*)this);
}


// ----------------------------------------------------------------------------
// static members, methods here
//
static std::deque<MfllGyro*>    sGyroInstances; // contains alives MfllGyro instance
static std::mutex               sLockGyroInstances; // locker to prevent race condition of sGyroInstances

static void register_gyro_instance_locked(MfllGyro* g)
{
    std::lock_guard<std::mutex> __l(sLockGyroInstances);

    // add to stack if only if the instance hasn't been added
    auto itr = std::find(sGyroInstances.begin(), sGyroInstances.end(), g);
    if (itr == sGyroInstances.end()) // not found
        sGyroInstances.push_back(g);
}

static void unregister_gyro_instance_locked(const MfllGyro* g)
{
    std::lock_guard<std::mutex> __l(sLockGyroInstances);
    auto itr = std::find(sGyroInstances.begin(), sGyroInstances.end(), g);
    if (itr != sGyroInstances.end())
        sGyroInstances.erase(itr);
}

static void sensor_listener_callback(ASensorEvent event)
{
    IMfllGyro::MfllGyroInfo gyroInfo;
    bool bUpdate = false;

    switch (event.type) {
    case ASENSOR_TYPE_GYROSCOPE:
        gyroInfo.timestamp = event.timestamp;
        gyroInfo.vector.x() = event.vector.x;
        gyroInfo.vector.y() = event.vector.y;
        gyroInfo.vector.z() = event.vector.z;
#ifdef __DEBUG
        mfllLogD("%s: (ts,x,y,z)=(%lld,%f,%f,%f)",
                __FUNCTION__,
                gyroInfo.timestamp,
                gyroInfo.vector.x(),
                gyroInfo.vector.y(),
                gyroInfo.vector.z()
                );
#endif
        bUpdate = true;
        break;
    default:
        break;
    }

    if (bUpdate) {
        std::lock_guard<std::mutex> __l(sLockGyroInstances);
        for (auto& itr : sGyroInstances) {
            if (itr != NULL)
                itr->addGyroInfoLocked(gyroInfo);
        }
    }
}

// returns the durtion of deep sleep
// domian: millisecond
static inline int64_t get_deep_sleep_duration()
{
    return android::uptimeMillis() - android::elapsedRealtime();
}


// ----------------------------------------------------------------------------
// member methods
//
enum MfllErr MfllGyro::init(sp<IMfllNvram>& /* nvramProvider */)
{
    // not implement, we don't need now
    return MfllErr_Ok;
}

enum MfllErr MfllGyro::start(unsigned int interval)
{
    enum MfllErr err = MfllErr_Ok;

    std::lock_guard<std::mutex> __lk(mLockOperation);

    if (mpSensorListener.get() == NULL) {
        mfllLogE("SensorListener is NULL, cannot start");
        return MfllErr_NullPointer;
    }

    // check if it's been already started, if yes, do not enableSensor again
    if (mStatus != 0) {
        mfllLogD("SensorListener has been started already, ignore this operation");
        return MfllErr_Ok;
    }

    if (mpSensorListener->enableSensor(SensorListener::SensorType_Gyro, interval)) {
        mStatus = 1;
    }
    else {
        mfllLogE("SensorListener::enableSensor returns fail");
        err = MfllErr_UnexpectedError;
    }

    return err;
}

enum MfllErr MfllGyro::stop()
{
    enum MfllErr err = MfllErr_Ok;

    std::lock_guard<std::mutex> __lk(mLockOperation);

    if (mpSensorListener.get() == NULL) {
        mfllLogE("SensorListener is NULL, cannot stop");
        return MfllErr_NullPointer;
    }

    // has stopped, do not disableSensor again
    if (mStatus == 0) {
        mfllLogD("SensorListener has been stopped already, ignore this operation");
        return MfllErr_Ok;
    }

    if (mpSensorListener->disableSensor(SensorListener::SensorType_Gyro)) {
        // ok good, stopped it
    }
    else {
        mfllLogE("SensorListener::disableSensor returns fail");
        err = MfllErr_UnexpectedError;
    }

    mStatus = 0; // no matter stop successfully or not, marks as stopped.

    return err;
}

std::deque<IMfllGyro::MfllGyroInfo> MfllGyro::getGyroInfo(
        int64_t ts_start,
        int64_t ts_end) const
{
    std::lock_guard<std::mutex> __lk(mLockGyroInfo);
    std::deque<IMfllGyro::MfllGyroInfo> rQueGyroInfo;

    // add deep sleep duration because the timestamps of gyro includes deep sleep time
    int64_t deepSleepDuration = get_deep_sleep_duration() * 1000000L; // ms -> ns
    ts_start += deepSleepDuration;
    ts_end   += deepSleepDuration;

    mfllLogD("%s: [ts_start, ts_end]=[%" PRId64 ", %" PRId64 "]", __FUNCTION__, ts_start, ts_end);

    // apporch O(n), w/ copy constructors.
    std::copy_if(
            mqGyroInfo.begin(),
            mqGyroInfo.end(),
            rQueGyroInfo.begin(),
            [&ts_start, &ts_end](const IMfllGyro::MfllGyroInfo& itr) -> bool {
                return (itr.timestamp >= ts_start && itr.timestamp <= ts_end);
            });

    return rQueGyroInfo;
    // return std::move(rQueGyroInfo);
    // we don't invoke std::movet to move rQueGyroInfo because rQueGyroInfo is
    // an rvalue and compiler shall optimize it
}

std::deque<IMfllGyro::MfllGyroInfo> MfllGyro::takeGyroInfo(
        int64_t ts_start,
        int64_t ts_end,
        bool clearInvalidated)
{
    std::lock_guard<std::mutex> __lk(mLockGyroInfo);
    std::deque<IMfllGyro::MfllGyroInfo> rQueGyroInfo;

    // add deep sleep duration because the timestamps of gyro includes deep sleep time
    int64_t deepSleepDuration = get_deep_sleep_duration() * 1000000L; // ms -> ns
    ts_start += deepSleepDuration;
    ts_end   += deepSleepDuration;

    mfllLogD("%s: [ts_start, ts_end]=[%" PRId64 ", %" PRId64 "]", __FUNCTION__, ts_start, ts_end);

    // apporch O(n), w/o copy constructor, use std::move to move objects.
    auto itr = mqGyroInfo.begin();
    while (itr != mqGyroInfo.end()) {
        itr = std::find_if(
                itr,
                mqGyroInfo.end(),
                [&ts_start, &ts_end](const IMfllGyro::MfllGyroInfo& i) -> bool {
                    return (i.timestamp >= ts_start && i.timestamp <= ts_end);
                });
        if (itr != mqGyroInfo.end()) {
            rQueGyroInfo.push_back(std::move(*itr));
            if (clearInvalidated)
                itr = mqGyroInfo.erase(mqGyroInfo.begin(), itr + 1); // clear [begin, current] elements
            else
                itr = mqGyroInfo.erase(itr);
        }
    }
    return rQueGyroInfo;
    // return std::move(rQueGyroInfo);
    // we don't invoke std::movet to move rQueGyroInfo because rQueGyroInfo is
    // an rvalue and compiler shall optimize it
}

std::deque<IMfllGyro::MfllGyroInfo> MfllGyro::takeGyroInfoQueue()
{
    std::lock_guard<std::mutex> __lk(mLockGyroInfo);
    return std::move(mqGyroInfo);
}

enum MfllErr MfllGyro::sendCommand(
        const std::string&          /* cmd */,
        const std::deque<void*>&    /* dataset */
        )
{
    return MfllErr_NotImplemented;
}


// ----------------------------------------------------------------------------
// MfllGyroInfo operations
//
void MfllGyro::addGyroInfoLocked(IMfllGyro::MfllGyroInfo info)
{
    std::lock_guard<std::mutex> __lk(mLockGyroInfo);
    while (mqGyroInfo.size() >= MFLL_GYRO_QUEUE_SIZE)
        mqGyroInfo.pop_front();
    mqGyroInfo.push_back(std::move(info));
    mCondGyroInfo.notify_one();
}

void MfllGyro::clearGyroInfosLocked()
{
    std::lock_guard<std::mutex> __lk(mLockGyroInfo);
    mqGyroInfo.clear();
}

size_t MfllGyro::getGyroInfoQueueSizeLocked()
{
    std::lock_guard<std::mutex> __lk(mLockGyroInfo);
    return mqGyroInfo.size();
}


// ----------------------------------------------------------------------------
// constructor
//
MfllGyro::MfllGyro()
    : mStatus(0)
{
    register_gyro_instance_locked(this); // register this instance to stack first

    mpSensorListener = std::unique_ptr<SensorListener, std::function<void(SensorListener*)>>
        (
            SensorListener::createInstance(),
            [](SensorListener* self)->void { if (self) self->destroyInstance(); }
        );

    if (mpSensorListener.get() == NULL) {
        mfllLogE("create SensorListener fail");
    }
    else {
        // set listener
        auto b = mpSensorListener->setListener(sensor_listener_callback);
        if (!b) {
            mfllLogE("set sensor listener returns fail");
            unregister_gyro_instance_locked(this); // if fail, remove this instance from stack
        }
    }
}

MfllGyro::~MfllGyro()
{
    stop(); // don't forget to stop !
    unregister_gyro_instance_locked(this);
}
