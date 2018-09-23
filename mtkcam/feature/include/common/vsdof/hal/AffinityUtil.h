/********************************************************************************************
 *     LEGAL DISCLAIMER
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION,
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/
#ifndef _AFFINITY_UTIL_H_
#define _AFFINITY_UTIL_H_

#include <bitset>

#include <linux/mt_sched.h>

#define TAG "Affinity"

enum ENUM_CPU_CORE
{
    CPUCoreLL = 0,      //LL: 0~3
    CPUCoreL  = 0x10,   //L : 4~7
    CPUCoreB  = 0x100   //B : 8~9
};

typedef unsigned int MASK_T;

#define ALL_CORE 0x3FF  //10 cores

class CPUMask
{
public:
    CPUMask()
    {
        setAllCPUCore();
    }

    CPUMask(MASK_T mask)
    {
        __mask = mask & ALL_CORE;  //10 cores
    }

    CPUMask(ENUM_CPU_CORE core, MASK_T count)
    {
        reset();
        setCore(core, count);
    }

    ~CPUMask() {}

    void reset()
    {
        __mask = 0;
    }

    void setAllCPUCore()
    {
        __mask = ALL_CORE;
    }

    void setCore(ENUM_CPU_CORE core, MASK_T count)
    {
        int shift = 0;
        switch(core) {
        case CPUCoreLL:
            if(count > 4) count = 4;
            __mask |= (core + count-1);
            break;
        case CPUCoreL:
            shift = 4;
            if(count > 4) count = 4;
            break;
        case CPUCoreB:
            shift = 8;
            if(count > 2) count = 2;
        default:
            break;
        }

        for(int n = 1; n <= count; n++) {
            __mask |= (n << shift);
        }
    }

    MASK_T getMask()
    {
        return __mask;
    }

private:
    MASK_T __mask;
};

class CPUAffinity
{
public:
    CPUAffinity() {}
    virtual ~CPUAffinity() {}

    virtual void enable(CPUMask &cpuMask)
    {
        ALOGD("[%s] Pid = %d , Tid = %d\n", TAG, getpid(), gettid());

        MASK_T cpu_msk = cpuMask.getMask();
        if(cpu_msk==0)
        {
            cpu_msk = 1;
        }
        std::string maskString = std::bitset<10>(cpu_msk).to_string();
        ALOGD("[%s] Set CPU Affinity Mask= %s\n", TAG, maskString.c_str());

        cpu_set_t cpuset;
        int s,j;
        CPU_ZERO(&cpuset);

        for(MASK_T Msk=1, cpu_no=0; Msk <= cpu_msk; Msk<<=1, cpu_no++)
        {
            if(Msk&cpu_msk)
            {
                CPU_SET(cpu_no, &cpuset);
                ALOGD("[%s] Set CPU %d(%s)", TAG, cpu_no, _cpusetToString(cpuset));
            }
        }

        cpu_set_t mt_mask;
        cpu_set_t cpuold;
        s = mt_sched_getaffinity(gettid(), sizeof(cpu_set_t), &cpuold, &mt_mask);
        if(s == 0) {
            ALOGD("[%s] Thread %d Old CPU Set %s, MT Mask %s", TAG, gettid(), _cpusetToString(cpuold), _cpusetToString(mt_mask));
        } else {
            ALOGD("[%s] Get affinity fail %d(err: %s)", TAG, s, strerror(errno));
        }

        ALOGD("[%s] Try to set affinity %s", TAG, _cpusetToString(cpuset));
        s = mt_sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpuset);
        if (s != 0)
        {
            ALOGD("[%s] Set affinity fail %d(err: %s)", TAG, s, strerror(errno));
        }
        else
        {
            mIsEnabled = true;
            s = mt_sched_getaffinity(gettid(), sizeof(cpu_set_t), &cpuset, &mt_mask);
            ALOGD("[%s] Thread %d New CPU Set %s, MT Mask %s", TAG, gettid(), _cpusetToString(cpuset), _cpusetToString(mt_mask));
        }
    }

    virtual void disable()
    {
        if(!mIsEnabled) {
            return;
        }

        // clear affinity
        int s = mt_sched_exitaffinity(gettid());
        if (s == 0)
        {
            ALOGD("[%s] Thread %d call mt_sched_exitaffinity successfully", TAG, gettid());
        }
        else
        {
            ALOGD("[%s] Thread %d call mt_sched_exitaffinity failed", TAG, gettid());
        }
    }
private:
    //For debugging
    const char *_cpusetToString(cpu_set_t &cpuset)
    {
        std::ostringstream oss("");
        for(int i = 10-1; i >= 0; i--) {
            oss << CPU_ISSET(i, &cpuset);
        }

        return oss.str().c_str();
    }

private:
    bool mIsEnabled;
};

class AutoCPUAffinity : CPUAffinity
{
public:
    AutoCPUAffinity(CPUMask &cpuMask)
    {
        enable(cpuMask);
    }

    ~AutoCPUAffinity()
    {
        disable();
    }
};

#endif