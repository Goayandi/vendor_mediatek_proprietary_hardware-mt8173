#define LOG_TAG "MtkCam/SensorListener"
//
#include <semaphore.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/mman.h>
//
#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/List.h>
#include <utils/Looper.h>
//
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
//
#include <gui/SensorManager.h>
#include <gui/Sensor.h>
//
using namespace android;
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Trace.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/sys/SensorListener.h>
//-----------------------------------------------------------------------------
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
//
#define MY_LOGD_IF(cond, arg...)    if (cond) { MY_LOGD(arg); }
#define MY_LOGW_IF(cond, arg...)    if (cond) { MY_LOGW(arg); }
#define MY_LOGE_IF(cond, arg...)    if (cond) { MY_LOGE(arg); }
//
#define FUNCTION_NAME               MY_LOGD("")
#define FUNCTION_IN                 MY_LOGD("+")
#define FUNCTION_OUT                MY_LOGD("-")
//-----------------------------------------------------------------------------
android::Mutex android::SensorManager::sLock;
std::map<String16, SensorManager*> android::SensorManager::sPackageInstances;
//-----------------------------------------------------------------------------
static
bool
setThreadPriority(
    int policy,
    int priority)
{
    struct sched_param sched_p;
    //
    ::sched_getparam(0, &sched_p);
    if (policy == SCHED_OTHER) {
        sched_p.sched_priority = 0;
        ::sched_setscheduler(0, policy, &sched_p);
        ::setpriority(PRIO_PROCESS, 0, priority);   //  Note: "priority" is nice value.
        MY_LOGD("tid(%d) policy(SCHED_OTHER:%d) priority(%d)", ::gettid(), policy, priority);
    } else {
        sched_p.sched_priority = priority;          //  Note: "priority" is real-time priority.
        ::sched_setscheduler(0, policy, &sched_p);
        MY_LOGD("tid(%d) policy(Real-Time:%d) priority(%d)", ::gettid(), policy, priority);
    }

    return true;
}
//-----------------------------------------------------------------------------
static
bool
getThreadPriority(
    int& policy,
    int& priority)
{
    policy = ::sched_getscheduler(0);
    if (policy == SCHED_OTHER) {
        // a conventional process has only the static priority
        priority = ::getpriority(PRIO_PROCESS, 0);
        MY_LOGD("tid(%d) policy(SCHED_OTHER:%d) priority(%d)", ::gettid(), policy, priority);
    } else {
        // a real-time process has both the static priority and real-time priority.
        struct sched_param sched_p;
        ::sched_getparam(0, &sched_p);
        priority = sched_p.sched_priority;
        MY_LOGD("tid(%d) policy(Real-Time:%d) priority(%d)", ::gettid(), policy, priority);
    }

    return true;
}
//-----------------------------------------------------------------------------
class SensorListenerImpThread : public Thread
{
    public:
        SensorListenerImpThread(Looper* looper) : Thread(false)
        {
            mLooper = sp<Looper>(looper);
        }
        //
        ~SensorListenerImpThread()
        {
            mLooper.clear();
        }
        //
        status_t readyToRun()
        {
            int policy = SCHED_OTHER, priority = 0;
            //
            ::prctl(PR_SET_NAME,"Mtkcam@SensorListenerImpThread", 0, 0, 0);
            //
            setThreadPriority(policy,priority);
            getThreadPriority(policy,priority);
            MY_LOGD("policy(%d),priority(%d)",policy,priority);
            //
            return NO_ERROR;
        }
        //
        virtual bool threadLoop()
        {
            //MY_LOGD("pollOnce E");
            mLooper->pollOnce(-1);
            //MY_LOGD("pollOnce X");
            return true;
        }
        // force looper wake up
        void wake() {
            mLooper->wake();
        }
    private:
        sp<Looper> mLooper;
};
//-----------------------------------------------------------------------------
class SensorListenerImp : public SensorListener
{
    public:
        SensorListenerImp();
        ~SensorListenerImp();
        //
        virtual void    destroyInstance(void);
        virtual MBOOL   setListener(Listener func);
        virtual MBOOL   enableSensor(
                            SensorTypeEnum  sensorType,
                            MUINT32         periodInMs);
        virtual MBOOL   disableSensor(SensorTypeEnum sensorType);
        //
        virtual MBOOL   init(void);
        virtual MBOOL   uninit(void);
        virtual MBOOL   getEvents(void);
        virtual MBOOL   isSupport(void);
        //
    private:
        #define SENSOR_TYPE_AMOUNT  (SensorType_Proxi+1)
        //
        mutable Mutex               mLock;
        volatile MINT32             mUser;
        Sensor const*               mpSensor[SENSOR_TYPE_AMOUNT];
        volatile MUINT32            mSensorEnableCnt;
        sp<Looper>                  mspLooper;
        sp<SensorEventQueue>        mspSensorEventQueue;
        SensorManager*              mpSensorManager;
        Listener                    mpListener;
        sp<SensorListenerImpThread> mspThread;
};
//-----------------------------------------------------------------------------
int SensorListenerImp_callback(
    int     /*fd*/,
    int     /*events*/,
    void*   pData)
{
    //MY_LOGD("+");
    SensorListenerImp* pInstance = (SensorListenerImp*)pData;
    pInstance->getEvents();
    //MY_LOGD("-");
    //should return 1 to continue receiving callbacks, or 0 to unregister
    return 1;
}
//-----------------------------------------------------------------------------
SensorListenerImp::
SensorListenerImp()
{
    FUNCTION_NAME;
    mUser = 0;
}
//----------------------------------------------------------------------------
SensorListenerImp::
~SensorListenerImp()
{
    FUNCTION_NAME;
}
//-----------------------------------------------------------------------------
SensorListener*
SensorListener::
createInstance(void)
{
    FUNCTION_NAME;
    SensorListenerImp* pInstance = new SensorListenerImp();
    pInstance->init();
    return pInstance;
}
//----------------------------------------------------------------------------
MVOID
SensorListenerImp::
destroyInstance(void)
{
    FUNCTION_IN;
    //
    uninit();
    delete this;
    //
    FUNCTION_OUT;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
setListener(Listener func)
{
    Mutex::Autolock lock(mLock);
    //
    if(mUser <= 0)
    {
        MY_LOGW("No user(%d)",mUser);
        return MFALSE;
    }
    //
    if(func == NULL)
    {
        MY_LOGW("func is NULL");
        return MFALSE;
    }
    //
    MY_LOGD("func(0x%08" PRIxPTR ")",(MUINTPTR)func);
    mpListener = func;
    //
    return MTRUE;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
enableSensor(
    SensorTypeEnum  sensorType,
    MUINT32         periodInMs)
{
    CAM_TRACE_NAME("SensorListener::enableSensor");
    //
    MBOOL ret = MTRUE;
    //
    MY_LOGD("+Type(%d),Rate(%d)",
            sensorType,
            periodInMs);
    //
    Mutex::Autolock lock(mLock);
    //
    if(mUser <= 0)
    {
        MY_LOGW("No user");
        ret = MFALSE;
        goto EXIT;
    }
    //
    if(sensorType >= SENSOR_TYPE_AMOUNT)
    {
        MY_LOGE("unknown sensorType(%d)",sensorType);
        ret = MFALSE;
    }
    //
    if(ret)
    {
        if(mpSensor[sensorType] != NULL)
        {
            #if 1
            if(mSensorEnableCnt == 0)
            {
                MY_LOGD("new SensorListenerImpThread");
                mspThread = new SensorListenerImpThread(mspLooper.get());
                MY_LOGD("run");
                mspThread->run("SensorListener::enableSensor");
            }
            #endif
            //
            MY_LOGD("enableSensor");
            CAM_TRACE_BEGIN("enableSensor");
            mspSensorEventQueue->enableSensor(mpSensor[sensorType]);
            CAM_TRACE_END();
            MY_LOGD("setEventRate");
            mspSensorEventQueue->setEventRate(mpSensor[sensorType],ms2ns(periodInMs));
            mSensorEnableCnt++;
        }
        else
        {
            MY_LOGW("sensorType(%d) does not exist",sensorType);
            ret = MFALSE;
        }
        //
        MY_LOGD("-Type(%d),Rate(%d),Cnt(%d)",
                sensorType,
                periodInMs,
                mSensorEnableCnt);
    }
    //
    EXIT:
    return ret;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
disableSensor(SensorTypeEnum sensorType)
{
    CAM_TRACE_NAME("SensorListener::disableSensor");
    //
    MBOOL ret = MTRUE;
    //
    MY_LOGD("+Type(%d)",sensorType);
    //
    Mutex::Autolock lock(mLock);
    //
    if(mUser <= 0)
    {
        MY_LOGW("No user");
        ret = MFALSE;
        goto EXIT;
    }
    //
    if(sensorType >= SENSOR_TYPE_AMOUNT)
    {
        MY_LOGE("unknown sensorType(%d)",sensorType);
        ret = MFALSE;
    }
    //
    if(ret)
    {
        if(mSensorEnableCnt > 0)
        {
            if(mpSensor[sensorType] != NULL)
            {
                #if 1
                if(mSensorEnableCnt == 1)
                {
                    MY_LOGD("requestExit");
                    mspThread->requestExit();
                    MY_LOGD("wake");
                    mspThread->wake();
                    MY_LOGD("join");
                    mspThread->join();
                    MY_LOGD("set mspThread to NULL");
                    mspThread = NULL;
                }
                #endif
                MY_LOGD("disableSensor");
                CAM_TRACE_BEGIN("disableSensor");
                mspSensorEventQueue->disableSensor(mpSensor[sensorType]);
                CAM_TRACE_END();
                mSensorEnableCnt--;
            }
            else
            {
                MY_LOGW("sensorType(%d) does not exist",sensorType);
                ret = MFALSE;
            }
            //
            MY_LOGD("-Type(%d),Cnt(%d)",
                    sensorType,
                    mSensorEnableCnt);
        }
        else
        {
            MY_LOGW("-sensorType(%d),userCnt is 0",sensorType);
            ret = MFALSE;
        }
    }
    //
    EXIT:
    return ret;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
init(void)
{
    CAM_TRACE_NAME("SensorListener::init");
    //
    MBOOL Result = MTRUE;
    MUINT32 i;
    //
    if(!isSupport())
    {
        return MFALSE;
    }
    //
    Mutex::Autolock lock(mLock);
    //
    if(mUser == 0)
    {
        MY_LOGD("First user(%d)",mUser);
    }
    else
    {
        MY_LOGD("More user(%d)",mUser);
        android_atomic_inc(&mUser);
        goto EXIT;
    }
    //
    for(i=0; i<SENSOR_TYPE_AMOUNT; i++)
    {
        mpSensor[i] = NULL;
    }
    mSensorEnableCnt = 0;
    mpListener = NULL;
    //
    CAM_TRACE_BEGIN("getInstance");
    mpSensorManager = &SensorManager::getInstanceForPackage(String16("MtkCam/SensorListener"));
    CAM_TRACE_END();
    //
    CAM_TRACE_BEGIN("getDefaultSensor");
    MY_LOGD("getDefaultSensor:Acc");
    mpSensor[SensorType_Acc] = mpSensorManager->getDefaultSensor(ASENSOR_TYPE_ACCELEROMETER);
    MY_LOGD("getDefaultSensor:Mag");
    mpSensor[SensorType_Mag] = mpSensorManager->getDefaultSensor(ASENSOR_TYPE_MAGNETIC_FIELD);
    MY_LOGD("getDefaultSensor:Gyro");
    mpSensor[SensorType_Gyro] = mpSensorManager->getDefaultSensor(ASENSOR_TYPE_GYROSCOPE);
    MY_LOGD("getDefaultSensor:Light");
    mpSensor[SensorType_Light] = mpSensorManager->getDefaultSensor(ASENSOR_TYPE_LIGHT);
    MY_LOGD("getDefaultSensor:Proxi");
    mpSensor[SensorType_Proxi] = mpSensorManager->getDefaultSensor(ASENSOR_TYPE_PROXIMITY);
    CAM_TRACE_END();
    //
    MY_LOGD("createEventQueue");
    CAM_TRACE_BEGIN("createEventQueue");
    mspSensorEventQueue = mpSensorManager->createEventQueue();
    CAM_TRACE_END();
    //
    MY_LOGD("new Looper");
    mspLooper = new Looper(false);
    MY_LOGD("addFd");
    mspLooper->addFd(
                mspSensorEventQueue->getFd(),
                0,
                ALOOPER_EVENT_INPUT,
                SensorListenerImp_callback,
                this);

    #if 0
    MY_LOGD("new SensorListenerImpThread");
    mspThread = new SensorListenerImpThread(mspLooper.get());
    MY_LOGD("run");
    mspThread->run("SensorListener::init");
    #endif
    //
    android_atomic_inc(&mUser);
    //
    EXIT:
    FUNCTION_OUT;
    return Result;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
uninit(void)
{
    CAM_TRACE_NAME("SensorListener::uninit");
    //
    MBOOL Result = MTRUE;
    MUINT32 i;
    status_t status;
    //
    Mutex::Autolock lock(mLock);
    //
    if(mUser <= 0)
    {
        MY_LOGW("No user(%d)",mUser);
        goto EXIT;
    }
    //
    android_atomic_dec(&mUser);
    //
    if(mUser == 0)
    {
        MY_LOGD("Last user(%d)",mUser);
    }
    else
    {
        MY_LOGD("More user(%d)",mUser);
        goto EXIT;
    }
    #if 0
    MY_LOGD("requestExit");
    mspThread->requestExit();
    MY_LOGD("wake");
    mspThread->wake();
    MY_LOGD("join");
    mspThread->join();
    MY_LOGD("set mspThread to NULL");
    mspThread = NULL;
    #endif
    //
    MY_LOGD("removeFd");
    mspLooper->removeFd(mspSensorEventQueue->getFd());
    //
    mSensorEnableCnt = 0;
    mspLooper = NULL;
    mspSensorEventQueue = NULL;
    mpSensorManager = NULL;
    mpListener = NULL;
    //
    for(i=0; i<SENSOR_TYPE_AMOUNT; i++)
    {
        mpSensor[i] = NULL;
    }
    //
    EXIT:
    FUNCTION_OUT;
    return Result;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
getEvents(void)
{
    //Mutex::Autolock lock(mLock);
    //
    //MY_LOGD("+");
    if(mUser <= 0)
    {
        MY_LOGW("No user(%d)",mUser);
        return MFALSE;
    }
    //
    ASensorEvent event;
    //
    if(mspSensorEventQueue != NULL)
    {
        while (mspSensorEventQueue->read(&event, 1) > 0)
        {
            if(mpListener != NULL)
            {
                //MY_LOGD("mpListener E");
                mpListener(event);
                //MY_LOGD("mpListener X");
            }
        }
    }
    //MY_LOGD("-");
    //
    return MTRUE;
}
//----------------------------------------------------------------------------
MBOOL
SensorListenerImp::
isSupport(void)
{
    int fd = -1;
    size_t s;
    char boot_mode[4] = {'0'};
    //MT_NORMAL_BOOT 0 , MT_META_BOOT 1, MT_RECOVERY_BOOT 2, MT_SW_REBOOT 3
    //MT_FACTORY_BOOT 4, MT_ADVMETA_BOOT 5
    fd = open("/sys/class/BOOT/BOOT/boot/boot_mode", O_RDONLY);
    if (fd < 0)
    {
        MY_LOGE("fail to open: %s", "/sys/class/BOOT/BOOT/boot/boot_mode");
        return 0;
    }

    s = read(fd, (void *)&boot_mode, sizeof(boot_mode) - 1);
    close(fd);

    if(s <= 0)
    {
        MY_LOGE("could not read boot mode sys file");
        return 0;
    }

    boot_mode[s] = '\0';
    MY_LOGD("Boot Mode %d",atoi(boot_mode));
    if( atoi(boot_mode) == 1 ||
        atoi(boot_mode) == 4)
    {
        return MFALSE;
    }
    else
    {
        return MTRUE;
    }
}

