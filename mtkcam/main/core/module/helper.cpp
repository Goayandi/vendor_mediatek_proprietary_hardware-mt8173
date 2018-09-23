/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#define LOG_TAG "mtkcam-module"
//
//c++
#include <mutex>
#include <atomic>
//
//bionic
#include <limits.h>
#include <dlfcn.h>
//
//system
#include <log/log.h>
//
//mtk
#include <mtkcam/main/core/module.h>
//
/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        ALOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        ALOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        ALOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        ALOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        ALOGE("[%s]{#%d:%s} " fmt, __FUNCTION__, __LINE__, __FILE__, ##arg)


/******************************************************************************
 *
 ******************************************************************************/
namespace
{
typedef void* (*FACTORY_T)(unsigned int moduleId);
char const szTargetSymbol[]     = "MtkCam_getModuleFactory";
char const szTargetLibPath[]    = "libcam_platform.so";

class MyHolder
{
protected:
    typedef             FACTORY_T   MY_T;

    MY_T                mTarget_ctor;   //from constructor
    std::atomic<MY_T>   mTarget_atomic; //from atomic
    std::mutex          mMutex;
    void*               mLibrary;

protected:
    static void         load(void*& rpLib, MY_T& rTarget)
                        {
                            void* pfnEntry = nullptr;
                            void* lib = ::dlopen(szTargetLibPath, RTLD_NOW);
                            if  ( ! lib ) {
                                char const *err_str = ::dlerror();
                                MY_LOGE("dlopen: %s error:%s", szTargetLibPath, (err_str ? err_str : "unknown"));
                                goto lbExit;
                            }
                            //
                            pfnEntry = ::dlsym(lib, szTargetSymbol);
                            if  ( ! pfnEntry ) {
                                char const *err_str = ::dlerror();
                                MY_LOGE("dlsym: %s (@%s) error:%s", szTargetSymbol, szTargetLibPath, (err_str ? err_str : "unknown"));
                                goto lbExit;
                            }
                            //
                            MY_LOGI("%s(%p) @ %s", szTargetSymbol, pfnEntry, szTargetLibPath);
                            rpLib = lib;
                            rTarget = reinterpret_cast<MY_T>(pfnEntry);
                            return;
                            //
                        lbExit:
                            if  ( lib ) {
                                ::dlclose(lib);
                                lib = nullptr;
                            }
                        }

public:
                        ~MyHolder()
                        {
                            if  ( mLibrary ) {
                                ::dlclose(mLibrary);
                                mLibrary = nullptr;
                            }
                        }

                        MyHolder()
                            : mTarget_ctor(nullptr)
                            , mTarget_atomic()
                            , mMutex()
                            , mLibrary(nullptr)
                        {
                            load(mLibrary, mTarget_ctor);
                        }

    MY_T                get()
                        {
                            //Usually the target can be established during constructor.
                            //Note: in this case we don't need any lock here.
                            if  ( mTarget_ctor ) {
                                return mTarget_ctor;
                            }

                            //Since the target cannot be established during constructor,
                            //we're trying to do it again now.
                            //
                            //Double-checked locking
                            MY_T tmp = mTarget_atomic.load(std::memory_order_relaxed);
                            std::atomic_thread_fence(std::memory_order_acquire);
                            //
                            if (tmp == nullptr) {
                                std::lock_guard<std::mutex> _l(mMutex);
                                tmp = mTarget_atomic.load(std::memory_order_relaxed);
                                if (tmp == nullptr) {
                                    MY_LOGW("fail to establish it during constructor, so we're trying to do now");
                                    load(mLibrary, tmp);
                                    //
                                    std::atomic_thread_fence(std::memory_order_release);
                                    mTarget_atomic.store(tmp, std::memory_order_relaxed);
                                }
                            }
                            return tmp;
                        }
};
}


/******************************************************************************
 *
 ******************************************************************************/
void*
getMtkCamModuleFactory(unsigned int moduleId)
{
    static MyHolder singleton;
    if  ( FACTORY_T factory = singleton.get() ) {
        return factory(moduleId);
    }
    //
    return NULL;
}

