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

#ifndef _MTKCAM_HWNODE_UTILIIIES_H_
#define _MTKCAM_HWNODE_UTILIIIES_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Trace.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Sync.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <cutils/properties.h>
#include <utils/String8.h>
#include <utils/Vector.h>
//
//
#include <sys/prctl.h>
#include <sys/resource.h>
#include <system/thread_defs.h>
//
//#include <utils/RWLock.h>
//#include <utils/Thread.h>
//
//#include <mtkcam/def/UITypes.h>
//
#include <mtkcam/aaa/IHal3A.h>
#if (USING_3A_SIMULATOR)
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
//#include <mtkcam/drv/iopipe/PostProc/INormalStream.h> // for setIsp() - clear tuning and return 0
#if (USING_3A_SIMULATOR_SOF)
#include <mtkcam/drv/iopipe/CamIO/INormalPipe.h>
#endif
#endif

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

// utilities for crop

inline MINT32 div_round(MINT32 const numerator, MINT32 const denominator) {
    return (( numerator < 0 ) ^ (denominator < 0 )) ?
        (( numerator - denominator/2)/denominator) : (( numerator + denominator/2)/denominator);
}

struct vector_f //vector with floating point
{
    MPoint  p;
    MPoint  pf;

                                vector_f(
                                        MPoint const& rP = MPoint(),
                                        MPoint const& rPf = MPoint()
                                        )
                                    : p(rP)
                                    , pf(rPf)
                                {}
};

struct simpleTransform
{
    // just support translation than scale, not a general formulation
    // translation
    MPoint    tarOrigin;
    // scale
    MSize     oldScale;
    MSize     newScale;

                                simpleTransform(
                                        MPoint rOrigin = MPoint(),
                                        MSize  rOldScale = MSize(),
                                        MSize  rNewScale = MSize()
                                        )
                                    : tarOrigin(rOrigin)
                                    , oldScale(rOldScale)
                                    , newScale(rNewScale)
                                {}
};

// transform MPoint
inline MPoint transform(simpleTransform const& trans, MPoint const& p) {
    return MPoint(
            div_round( (p.x - trans.tarOrigin.x) * trans.newScale.w, trans.oldScale.w),
            div_round( (p.y - trans.tarOrigin.y) * trans.newScale.h, trans.oldScale.h)
            );
};

inline MPoint inv_transform(simpleTransform const& trans, MPoint const& p) {
    return MPoint(
            div_round( p.x * trans.oldScale.w, trans.newScale.w) + trans.tarOrigin.x,
            div_round( p.y * trans.oldScale.h, trans.newScale.h) + trans.tarOrigin.y
            );
};

inline int int_floor(float x) {
    int i = (int)x;
    return i - (i > x);
}

// transform vector_f
inline vector_f transform(simpleTransform const& trans, vector_f const& p) {
    MFLOAT const x = (p.p.x + (p.pf.x/(MFLOAT)(1u<<31))) * trans.newScale.w / trans.oldScale.w;
    MFLOAT const y = (p.p.y + (p.pf.y/(MFLOAT)(1u<<31))) * trans.newScale.h / trans.oldScale.h;
    int const x_int = int_floor(x);
    int const y_int = int_floor(y);
    return vector_f(
            MPoint(x_int, y_int),
            MPoint((x - x_int) * (1u<<31), (y - y_int) * (1u<<31))
            );
};

inline vector_f inv_transform(simpleTransform const& trans, vector_f const& p) {
    MFLOAT const x = (p.p.x + (p.pf.x/(MFLOAT)(1u<<31))) * trans.oldScale.w / trans.newScale.w;
    MFLOAT const y = (p.p.y + (p.pf.y/(MFLOAT)(1u<<31))) * trans.oldScale.h / trans.newScale.h;
    int const x_int = int_floor(x);
    int const y_int = int_floor(y);
    return vector_f(
            MPoint(x_int, y_int),
            MPoint((x - x_int) * (1u<<31), (y - y_int) * (1u<<31))
            );
};

// transform MSize
inline MSize transform(simpleTransform const& trans, MSize const& s) {
    return MSize(
            div_round( s.w * trans.newScale.w, trans.oldScale.w),
            div_round( s.h * trans.newScale.h, trans.oldScale.h)
            );
};

inline MSize inv_transform(simpleTransform const& trans, MSize const& s) {
    return MSize(
            div_round( s.w * trans.oldScale.w, trans.newScale.w),
            div_round( s.h * trans.oldScale.h, trans.newScale.h)
            );
};

// transform MRect
inline MRect transform(simpleTransform const& trans, MRect const& r) {
    return MRect(transform(trans, r.p), transform(trans, r.s));
};

inline MRect inv_transform(simpleTransform const& trans, MRect const& r) {
    return MRect(inv_transform(trans, r.p), inv_transform(trans, r.s));
};


/******************************************************************************
 *  Metadata Access
 ******************************************************************************/
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata const* pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if (pMetadata == NULL) {
        CAM_LOGW("pMetadata == NULL");
        return MFALSE;
    }
    //
    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if(!entry.isEmpty()) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    //
    return MFALSE;
}

template <typename T>
inline MBOOL
trySetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T const& val
)
{
    if (pMetadata == NULL) {
        CAM_LOGW("pMetadata == NULL");
        return MFALSE;
    }
    //
    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    if (OK == pMetadata->update(tag, entry)) {
        return MTRUE;
    }
    //
    return MFALSE;
}

/******************************************************************************
 *  Simulate HAL 3A.
 ******************************************************************************/

using namespace NS3Av3;
using namespace android;

#ifdef HAL3A_SIMULATOR_IMP
#undef HAL3A_SIMULATOR_IMP
#endif

#if (USING_3A_SIMULATOR)

#ifdef HAL3A_SIMULATOR_REAL_SOF
#undef HAL3A_SIMULATOR_REAL_SOF
#endif
#if (USING_3A_SIMULATOR_SOF) // #if (0) // for force disable REAL_SOF
#define HAL3A_SIMULATOR_REAL_SOF (1)
#else
#define HAL3A_SIMULATOR_REAL_SOF (0)
#endif

#ifdef LDVT_TIMING_FACTOR
#undef LDVT_TIMING_FACTOR
#endif
#ifdef USING_MTK_LDVT
#define LDVT_TIMING_FACTOR 4 //(1 * 30) for 1sec
#else
#define LDVT_TIMING_FACTOR 1
#endif

#define HAL3A_SIMULATOR_IMP (1)

class IHal3ASimulator
{

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  //    Ctor/Dtor.
                        IHal3ASimulator(){};
    virtual             ~IHal3ASimulator(){};

private: // disable copy constructor and copy assignment operator
                        IHal3ASimulator(const IHal3ASimulator&);
    IHal3A&             operator=(const IHal3ASimulator&);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    #if 0
    /**
     * @brief Create instance of IHal3A
     * @param [in] i4SensorIdx
     */
    static IHal3ASimulator*      createInstance(
        MINT32 const i4SensorIdx, const char* strUser);
    #endif

    /**
     * @brief destroy instance of IHal3A
     */
    virtual MVOID       destroyInstance(const char* /*strUser*/) {}
    /**
     * @brief config 3A
     */
    virtual MINT32      config(MINT32 i4SubsampleCount=0) = 0;
    /**
     * @brief start 3A
     */
    virtual MINT32      start(MINT32 i4StartNum=0) = 0;
    /**
     * @brief stop 3A
     */
    virtual MINT32      stop() = 0;

    /**
     * @brief stop Stt
     */
    virtual MVOID      stopStt() = 0;

    // interfaces for metadata processing
    /**
     * @brief Set list of controls in terms of metadata via IHal3A
     * @param [in] controls list of MetaSet_T
     */
    virtual MINT32      set(const List<MetaSet_T>& controls) = 0;

    /**
     * @brief Set list of controls in terms of metadata of capture request via IHal3A
     * @param [in] controls list of MetaSet_T
     */
    virtual MINT32      startCapture(const List<MetaSet_T>& controls, MINT32 i4StartNum=0) = 0;

    /**
     * @brief Set pass2 tuning in terms of metadata via IHal3A
     * @param [in] flowType 0 for processed raw, 1 for pure raw
     * @param [in] control MetaSet_T
     * @param [out] pRegBuf buffer address for register setting
     * @param [out] result IMetadata
     */
    virtual MINT32      setIsp(MINT32 flowType, const MetaSet_T& control, TuningParam* pTuningBuf, MetaSet_T* pResult) = 0;

    /**
     * @brief Get dynamic result with specified frame ID via IHal3A
     * @param [in] frmId specified frame ID (magic number)
     * @param [out] result in terms of metadata
     */
    //virtual MINT32      get(MUINT32 frmId, IMetadata&  result) = 0;
    virtual MINT32      get(MUINT32 frmId, MetaSet_T& result) = 0;
    virtual MINT32      getCur(MUINT32 frmId, MetaSet_T& result) = 0;

    /**
     * @brief Attach callback for notifying
     * @param [in] eId Notification message type
     * @param [in] pCb Notification callback function pointer
     */
    virtual MINT32      attachCb(IHal3ACb::ECb_T eId, IHal3ACb* pCb) = 0;

    /**
     * @brief Dettach callback
     * @param [in] eId Notification message type
     * @param [in] pCb Notification callback function pointer
     */
    virtual MINT32      detachCb(IHal3ACb::ECb_T eId, IHal3ACb* pCb) = 0;


    /**
     * @brief Get delay frames via IHal3A
     * @param [out] delay_info in terms of metadata with MTK defined tags.
     */
    virtual MINT32      getDelay(IMetadata& delay_info) const= 0;

    /**
     * @brief Get delay frames via IHal3A
     * @param [in] tag belongs to control+dynamic
     * @return
     * - MINT32 delay frame.
     */
    virtual MINT32      getDelay(MUINT32 tag) const = 0;

    /**
     * @brief Get capacity of metadata list via IHal3A
     * @return
     * - MINT32 value of capacity.
     */
    virtual MINT32      getCapacity() const = 0;

    virtual MINT32      send3ACtrl(E3ACtrl_T e3ACtrl, MINTPTR i4Arg1, MINTPTR i4Arg2) = 0;

    /**
     * @brief set sensor mode
     * @param [in] i4SensorMode
     */
    virtual MVOID       setSensorMode(MINT32 i4SensorMode) = 0;

    virtual MVOID       notifyP1Done(MUINT32 u4MagicNum, MVOID* pvArg = 0) = 0;

    /**
     * @brief notify sensor power on
     */
    virtual MBOOL       notifyPwrOn() = 0;
    /**
     * @brief notify sensor power off
     */
    virtual MBOOL       notifyPwrOff() = 0;
    /**
     * @brief check whether flash on while capture
    */
    virtual MBOOL       checkCapFlash() = 0;

    virtual MVOID       setFDEnable(MBOOL fgEnable) = 0;

    virtual MBOOL       setFDInfo(MVOID* prFaces) = 0;

    virtual MBOOL       setOTInfo(MVOID* prOT) = 0;
};

class Hal3ASimulator
    : public IHal3ASimulator//IHal3A
{

public:
    Hal3ASimulator(
        MINT32 const i4SensorIdx,
        const char* strUser)
        : IHal3ASimulator()//IHal3A()
        , mLogLevel(0)
        , mUserName(strUser)
        , mSensorIdx(i4SensorIdx)
        , mCapacity(3)
        , mSubsampleCount(1)
        , mNumQueueLock()
        , mCallbackLock()
        , mCallbackCond()
        , mCallbackEnable(MFALSE)
        , mCallbackSkip(MFALSE)
        , mCallbackCount(0)
        , mLastCallbackTime(0)
        , mCallbackTimeIntervalUs(33333 * LDVT_TIMING_FACTOR)
        #if HAL3A_SIMULATOR_REAL_SOF
        , mpCamIO(NULL)
        , mUserKey(0)
        , mTimeoutMs(0)
        #endif
        {
            char cLogLevel[PROPERTY_VALUE_MAX] = {0};
            ::property_get("debug.camera.log", cLogLevel, "0");
            mLogLevel = ::atoi(cLogLevel);
            #if 0
            #warning "[FIXME] force enable Hal3ASimulator log"
            if (mLogLevel < 2) {
                mLogLevel = 2;
            }
            #endif
            //
            {
                Mutex::Autolock _l(mCallbackLock);
                for (int i = 0; i < IHal3ACb::eID_MSGTYPE_NUM; i++) {
                    mCbSet[i] = NULL;
                }
            }
            mvNumQueue.clear();
            mpCallbackThread =  new CallbackThread(this);
            CAM_LOGI("Hal3ASimulator(%d, %s) LogLevel(%d)",
                i4SensorIdx, strUser, mLogLevel);
        };

    Hal3ASimulator()
    {
        CAM_LOGD("Hal3ASimulator()");
    };
    virtual ~Hal3ASimulator()
    {
        CAM_LOGD("~Hal3ASimulator()");
    };

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IHal3A Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    //
    /**
     * @brief Create instance of IHal3A
     * @param [in] eVersion, E_Camera_1, E_Camera_3
     * @param [in] i4SensorIdx
     */
    static /*IHal3A*/Hal3ASimulator*      createInstance(
                            MINT32 const i4SensorIdx,
                            const char* strUser)
    {
        return new Hal3ASimulator(i4SensorIdx, strUser);
    };

    /**
     * @brief destroy instance of IHal3A
     */
    virtual MVOID       destroyInstance(const char* strUser)
    {
        if (mUserName == strUser) {
            CAM_LOGD_IF(mLogLevel >= 2, "[%s] 3A simulator destroy user[%s] ",
                mUserName.string(), strUser);
            delete this;
        } else {
            CAM_LOGW("[%s] 3A simulator user[%s] not found",
                mUserName.string(), strUser);
        }
    };

    /**
     * @brief config 3A
     */
    virtual MINT32      config(MINT32 i4SubsampleCount=0)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator config (%d)",
            mUserName.string(), i4SubsampleCount);
        mSubsampleCount = (i4SubsampleCount > 0) ? i4SubsampleCount : 1;
        return 0;
    };

    /**
     * @brief start 3A
     */
    virtual MINT32      start(MINT32 i4StartNum=0)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator start (%d)",
            mUserName.string(), i4StartNum);
        #if HAL3A_SIMULATOR_REAL_SOF
        if (mpCamIO == NULL) {
            #ifdef USING_MTK_LDVT /*[EP_TEMP]*/ //[FIXME] TempTestOnly
            mpCamIO = NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::
                        createInstance(mSensorIdx, "iopipeUseTM3AS");
            #else
            mpCamIO = NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::
                        createInstance(mSensorIdx, mUserName.string());
            #endif
            //
            if (mpCamIO == NULL) {
                CAM_LOGW("[%s] CamIO create fail", mUserName.string());
                return 0;
            }
            mUserKey = mpCamIO->attach("S3A_CB");
            mTimeoutMs = mCallbackTimeIntervalUs / 50;
            // driver default timeout = AvgFrameTimeIntervalMs x 20
            //                        = (mCallbackTimeIntervalUs / 1000) * 20;
            //                        = mCallbackTimeIntervalUs / 50;
        }
        #endif
        mLastCallbackTime = 0;
        mpCallbackThread->run("Hal3ASimulatorCallback");
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator thread start",
            mUserName.string());
        return 0;
    };

    /**
     * @brief stop 3A
     */
    virtual MINT32      stop()
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator stop",
            mUserName.string());
        {
            Mutex::Autolock _l(mCallbackLock);
            mCallbackEnable = MFALSE;
            mCallbackSkip = MFALSE;
            mCallbackCount = 0;
        }
        mpCallbackThread->requestExit();
        #if HAL3A_SIMULATOR_REAL_SOF
        if (mpCamIO != NULL) {
            CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator stop signal",
                mUserName.string());
            mpCamIO->signal(
                NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal_SOF,
                mUserKey);
        }
        #endif
        mpCallbackThread->join();
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator thread stop",
            mUserName.string());
        //
        #if HAL3A_SIMULATOR_REAL_SOF
        if (mpCamIO != NULL) {
            #ifdef USING_MTK_LDVT /*[EP_TEMP]*/ //[FIXME] TempTestOnly
            mpCamIO->destroyInstance("iopipeUseTM");
            #else
            mpCamIO->destroyInstance(mUserName.string());
            #endif
            mpCamIO = NULL;
        }
        #endif
        return 0;
    };

    /**
     * @brief stop Stt
     */
    virtual MVOID      stopStt()
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator stopStt",
            mUserName.string());
        return;
    };

    // interfaces for metadata processing
    /**
     * @brief Set list of controls in terms of metadata via IHal3A
     * @param [in] controls list of MetaSet_T
     */
    virtual MINT32      set(const List<MetaSet_T>& controls)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator set",
            mUserName.string());
        return setMetaList(controls);
    };

    /**
     * @brief Set list of controls in terms of metadata of capture request via IHal3A
     * @param [in] controls list of MetaSet_T
     */
    virtual MINT32      startCapture(const List<MetaSet_T>& controls,
                            MINT32 i4StartNum=0)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator startCapture (%d)",
            mUserName.string(), i4StartNum);
        start(i4StartNum);
        set(controls);
        return ESTART_CAP_NORMAL;//ESTART_CAP_MANUAL;
    };

    /**
     * @brief Set pass2 tuning in terms of metadata via IHal3A
     * @param [in] flowType 0 for processed raw, 1 for pure raw
     * @param [in] control MetaSet_T
     * @param [out] pRegBuf buffer address for register setting
     * @param [out] result IMetadata
     */
    virtual MINT32      setIsp(
                            MINT32 flowType,
                            const MetaSet_T& control,
                            TuningParam* pRegBuf,
                            MetaSet_T* pResult)
    {
        CAM_LOGD_IF(mLogLevel >= 1,
            "[%s] 3A simulator setIsp type(%d) reg(%p) result(%p)",
            mUserName.string(), flowType, pRegBuf, (void*)pResult);
        if (pResult != NULL) {
            pResult->appMeta = control.appMeta;
            pResult->halMeta = control.halMeta;
            CAM_LOGD_IF(mLogLevel >= 1,
                "[%s] 3A simulator setIsp - assign control metadata to result",
                mUserName.string());
        } else {
            CAM_LOGD_IF(mLogLevel >= 1,
                "[%s] 3A simulator setIsp - ignore control metadata to result",
                mUserName.string());
        }
        #if 0 // clear tuning and return 0
        if ((pRegBuf != NULL) && (pRegBuf->pRegBuf != NULL)){
            char* name = "3A-simulator-setIsp";
            NSIoPipe::NSPostProc::INormalStream* pipe = NULL;
            pipe = NSIoPipe::NSPostProc::INormalStream::createInstance(mSensorIdx);
            if (pipe == NULL) {
                CAM_LOGE("normal stream - create failed");
                return (-1);
            }
            if (!pipe->init(name)) {
                CAM_LOGE("normal stream - init failed");
                if (!pipe->uninit(name)) {
                    CAM_LOGE("normal stream - uninit failed");
                }
                pipe->destroyInstance();
                return (-1);
            }
            unsigned int tuningsize = pipe->getRegTableSize();//sizeof(dip_x_reg_t);
            CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator setIsp - "
                "get tuning RegBuf Size(%d)", mUserName.string(), tuningsize);
            ::memset((unsigned char*)(pRegBuf->pRegBuf), 0, tuningsize);
            CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator setIsp - "
                "clear tuning RegBuf", mUserName.string());
            if (!pipe->uninit(name)) {
                CAM_LOGE("normal stream - uninit failed");
            }
            pipe->destroyInstance();
            return (0);
        }
        #endif
        return (-1);
    };

    /**
     * @brief Get dynamic result with specified frame ID via IHal3A
     * @param [in] frmId specified frame ID (magic number)
     * @param [out] result in terms of metadata
     */
    //virtual MINT32      get(MUINT32 frmId, IMetadata&  result) = 0;
    virtual MINT32      get(MUINT32 frmId, MetaSet_T& /*result*/)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator get frmId (%d)",
            mUserName.string(), frmId);
        return 0;
    };

    virtual MINT32      getCur(MUINT32 frmId, MetaSet_T& /*result*/)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator getCur frmId (%d)",
            mUserName.string(), frmId);
        return 0;
    };

    /**
     * @brief Attach callback for notifying
     * @param [in] eId Notification message type
     * @param [in] pCb Notification callback function pointer
     */
    virtual MINT32      attachCb(
                            IHal3ACb::ECb_T eId,
                            IHal3ACb* pCb)
    {
        Mutex::Autolock _l(mCallbackLock);
        if (eId < IHal3ACb::eID_MSGTYPE_NUM) {
            mCbSet[eId] = pCb;
        } else {
            CAM_LOGW("[%s] 3A simulator can not attachCb at [%d]",
                mUserName.string(), eId);
        }
        return 0;
    };

    /**
     * @brief Dettach callback
     * @param [in] eId Notification message type
     * @param [in] pCb Notification callback function pointer
     */
    virtual MINT32      detachCb(
                            IHal3ACb::ECb_T eId,
                            IHal3ACb* pCb)
    {
        Mutex::Autolock _l(mCallbackLock);
        if (eId < IHal3ACb::eID_MSGTYPE_NUM) {
            if (mCbSet[eId] == pCb) {
                mCbSet[eId] = NULL;
            } else {
                CAM_LOGW("[%s] 3A simulator can not found detachCb %p at [%d]",
                mUserName.string(), (void*)pCb, eId);
            }
        } else {
            CAM_LOGW("[%s] 3A simulator can not detachCb at [%d]",
                mUserName.string(), eId);
        }
        return 0;
    };

    /**
     * @brief Get delay frames via IHal3A
     * @param [out] delay_info in terms of metadata with MTK defined tags.
     */
    virtual MINT32      getDelay(IMetadata& /*delay_info*/) const
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator getDelay",
            mUserName.string());
        return 0;
    };

    /**
     * @brief Get delay frames via IHal3A
     * @param [in] tag belongs to control+dynamic
     * @return
     * - MINT32 delay frame.
     */
    virtual MINT32      getDelay(MUINT32 tag) const
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator getDelay tag (%d)",
            mUserName.string(), tag);
        return 0;
    };

    /**
     * @brief Get capacity of metadata list via IHal3A
     * @return
     * - MINT32 value of capacity.
     */
    virtual MINT32      getCapacity() const
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator getCapacity (%d)",
            mUserName.string(), mCapacity);
        return mCapacity;
    };

    virtual MINT32      send3ACtrl(
                            E3ACtrl_T e3ACtrl,
                            MINTPTR i4Arg1,
                            MINTPTR i4Arg2)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator send3ACtrl (0x%x) 0x%zx 0x%zx",
            mUserName.string(), e3ACtrl, i4Arg1, i4Arg2);
        return 0;
    };

    /**
     * @brief set sensor mode
     * @param [in] i4SensorMode
     */
    virtual MVOID       setSensorMode(MINT32 i4SensorMode)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator setSensorMode (0x%x)",
            mUserName.string(), i4SensorMode);
        return;
    };

    virtual MVOID       notifyP1Done(MUINT32 u4MagicNum, MVOID* pvArg = 0)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator notifyP1Done (%d) %p",
            mUserName.string(), u4MagicNum, pvArg);
        return;
    };

    virtual MVOID       setFDEnable(MBOOL fgEnable)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator setFDEnable (%d)",
            mUserName.string(), fgEnable);
        return;
    };

    virtual MBOOL       setFDInfo(MVOID* prFaces)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator setFDInfo (%p)",
            mUserName.string(), prFaces);
        return MFALSE;
    };


    #if 1 // temp for IHal3A in v3
    /**
     * @brief notify sensor power on
     */
    virtual MBOOL       notifyPwrOn()
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator notifyPwrOn",
            mUserName.string());
        return MFALSE;
    };
    /**
     * @brief notify sensor power off
     */
    virtual MBOOL       notifyPwrOff()
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator notifyPwrOff",
            mUserName.string());
        return MFALSE;
    };
    /**
     * @brief check whether flash on while capture
     */
    virtual MBOOL       checkCapFlash()
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator checkCapFlash",
            mUserName.string());
        return MFALSE;
    };

    virtual MBOOL       setOTInfo(MVOID* prOT)
    {
        CAM_LOGD_IF(mLogLevel >= 1, "[%s] 3A simulator setOTInfo (%p)",
            mUserName.string(), prOT);
        return MFALSE;
    };
    #endif

    class CallbackThread
        : public Thread
    {

    public:

        CallbackThread(Hal3ASimulator* pHal3ASimulatorImp)
            : mpSim3A(pHal3ASimulatorImp)
        {};

        ~CallbackThread()
        {};

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Thread Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    public:
        virtual status_t    readyToRun()
        {
            CAM_LOGD("[%s] readyToRun callback thread",
                mpSim3A->mUserName.string());

            // set name
            ::prctl(PR_SET_NAME, (unsigned long)"Cam@3ASimulator", 0, 0, 0);
            // set normal
            struct sched_param sched_p;
            sched_p.sched_priority = 0;
            ::sched_setscheduler(0, (SCHED_OTHER), &sched_p);
            ::setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DISPLAY);
                                            //  Note: "priority" is nice value.
            //
            ::sched_getparam(0, &sched_p);
            CAM_LOGD(
                "Tid: %d, policy: %d, priority: %d"
                , ::gettid(), ::sched_getscheduler(0)
                , sched_p.sched_priority
            );
            //
            return OK;
        };
    private:
        virtual bool        threadLoop()
        {
            return callbackLoop();
        };

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Data Member.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    private:
        Hal3ASimulator* mpSim3A;

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Function Member.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    private:
        MBOOL callbackLoop()
        {
            CAM_LOGD_IF(mpSim3A->mLogLevel >= 2, "enter callback thread");
            if (mpSim3A == NULL) {
                CAM_LOGD("caller not exist - exit callback thread");
                return MFALSE;
            }
            if (!exitPending()) {
                CAM_LOGD_IF(mpSim3A->mLogLevel >= 2, "go-on callback thread");
                MINT64 current_time = 0;
                struct timeval tv;
                //
                MINT32 idx = 0;
                //
                gettimeofday(&tv, NULL);
                current_time = tv.tv_sec * 1000000 + tv.tv_usec;
                {
                    Mutex::Autolock _l(mpSim3A->mCallbackLock);
                    if (!mpSim3A->mCallbackEnable) {
                        CAM_LOGD("CB wait+");
                        mpSim3A->mCallbackCond.wait(mpSim3A->mCallbackLock);
                        CAM_LOGD("CB wait-");
                        mpSim3A->mLastCallbackTime = current_time;
                        return MTRUE;
                    }
                }
                //
                if (mpSim3A->mLastCallbackTime == 0) {
                    CAM_LOGD_IF(mpSim3A->mLogLevel >= 2,
                        "CB Thread Time not set %" PRId64 " / %" PRId64 ,
                        mpSim3A->mLastCallbackTime, current_time);
                    mpSim3A->mLastCallbackTime = current_time;
                    return MTRUE;
                }
                //
                #if HAL3A_SIMULATOR_REAL_SOF
                if (mpSim3A->mpCamIO != NULL) {
                    MBOOL res = MFALSE;
                    CAM_LOGD_IF(mpSim3A->mLogLevel >= 2,
                        "CB wait SOF +++ (%" PRId64 ")", current_time);
                    res = mpSim3A->mpCamIO->wait(
                        NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal_SOF,
                        NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal_ClearWait,
                        mpSim3A->mUserKey,
                        mpSim3A->mTimeoutMs);
                    if (mpSim3A->mLogLevel >= 2) {
                        gettimeofday(&tv, NULL);
                        current_time = tv.tv_sec * 1000000 + tv.tv_usec;
                    }
                    if (!res) {
                        CAM_LOGE("CB wait SOF TIMEOUT:%dms (%" PRId64 ")",
                            mpSim3A->mTimeoutMs, current_time);
                        return MFALSE;
                    }
                    CAM_LOGD_IF(mpSim3A->mLogLevel >= 2,
                        "CB wait SOF --- (%" PRId64 ")", current_time);
                    //
                    if (mpSim3A->mCallbackSkip) {
                        mpSim3A->mCallbackSkip = MFALSE;
                    } else {
                        mpSim3A->performCallback(mpSim3A->mCallbackCount);
                        mpSim3A->mCallbackCount ++;
                    }
                    //
                }
                #else
                if ((current_time - mpSim3A->mLastCallbackTime) >
                    mpSim3A->mCallbackTimeIntervalUs) {

                    mpSim3A->mLastCallbackTime = current_time;
                    CAM_LOGD_IF(mpSim3A->mLogLevel >= 2,
                        "Current CB Thread Time = %lld",
                        mpSim3A->mLastCallbackTime);
                    //
                    if (mpSim3A->mCallbackSkip) {
                        mpSim3A->mCallbackSkip = MFALSE;
                    } else {
                        mpSim3A->performCallback(mpSim3A->mCallbackCount);
                        mpSim3A->mCallbackCount ++;
                    }
                    //
                } else {
                    CAM_LOGD_IF(mpSim3A->mLogLevel >= 2,
                        "CB Thread Time = %lld / %lld",
                        mpSim3A->mLastCallbackTime, current_time);
                    // next time for check, the time interval can be adjusted
                    usleep(mpSim3A->mCallbackTimeIntervalUs >> 4);
                }
                #endif
                return MTRUE;
            }
            CAM_LOGD("exit callback thread");
            return MFALSE;
        };

    };

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Data Member.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:

    MINT32              mLogLevel;
    android::String8    mUserName;
    MINT32              mSensorIdx;
    IHal3ACb*           mCbSet[IHal3ACb::eID_MSGTYPE_NUM];
    MINT32              mCapacity;
    //
    MINT32              mSubsampleCount;
    //
    mutable Mutex       mNumQueueLock;
    android::Vector<MINT32> mvNumQueue;
    //
    mutable Mutex       mCallbackLock;
    Condition           mCallbackCond;
    MBOOL               mCallbackEnable;
    MBOOL               mCallbackSkip;
    MINT32              mCallbackCount;
    //
    MINT64              mLastCallbackTime;
    MINT64              mCallbackTimeIntervalUs;
    //
    sp<CallbackThread>  mpCallbackThread;
    //
#if HAL3A_SIMULATOR_REAL_SOF
    NSCam::NSIoPipe::NSCamIOPipe::INormalPipe*        mpCamIO;
    MINT32              mUserKey;
    MINT32              mTimeoutMs;
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Function Member.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    void performCallback(MINT32 count)
    {
        CAM_LOGD_IF(mLogLevel >= 2, "CB CNT(%d)", count);
        // callback if exist
        MINT32 msgType = 0;
        //
        msgType = IHal3ACb::eID_NOTIFY_VSYNC_DONE;
        if (mCbSet[msgType] != NULL) {
            IHal3ACb* pCb = mCbSet[msgType];
            if (count > 0) {
                MINTPTR ext1 = (MINTPTR)(NULL);
                MINTPTR ext2 = (MINTPTR)(NULL);
                MINTPTR ext3 = (MINTPTR)(NULL);
                CAM_LOGD_IF(mLogLevel >= 2,
                    "do notify (%d) [%d] +++", count, msgType);
                //
                pCb->doNotifyCb(msgType, ext1, ext2, ext3);
                //
                CAM_LOGD_IF(mLogLevel >= 2,
                    "do notify (%d) [%d] ---", count, msgType);
            }
        }
        //
        msgType = IHal3ACb::eID_NOTIFY_3APROC_FINISH;
        if (mCbSet[msgType] != NULL) {
            IHal3ACb* pCb = mCbSet[msgType];
            //
            #if 1
            // after the SOF arrival,
            // it might need to take some time for this calculation
            // then, it can perform callback
            // for simulation, adjust the sleep time
            usleep(mCallbackTimeIntervalUs >> 4);
            #endif
            //
            MINT32 idx = 0;
            MINT32 num = 0;
            RequestSet_T reqSet;
            CapParam_T cap_param;
            //
            #if HAL3A_SIMULATOR_REAL_SOF
            mpCamIO->sendCommand(
                NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_CUR_SOF_IDX,
                (MINTPTR)&idx, 0, 0);
            #endif
            //
            MINTPTR ext1 = (MINTPTR)(NULL);
            MINTPTR ext2 = (MINTPTR)(idx);
            MINTPTR ext3 = (MINTPTR)(NULL);
            if (mvNumQueue.size() >= (size_t)mSubsampleCount) {
                for (MINT32 i = 0; i < mSubsampleCount; i++) {
                    num = deQueueNum();
                    if (num > 0) {
                        reqSet.vNumberSet.push_back(num);
                    } else {
                        CAM_LOGW("cannot find the num at %d", i);
                        dumpQueueNum();
                        break;
                    }
                }
            } else {
                dumpQueueNum();
            }
            ext1 = (MINTPTR)(&reqSet);
            ext3 = (MINTPTR)(&cap_param);
            //
            CAM_LOGD_IF(mLogLevel >= 2,
                "do notify (%d) [%d] %d %d +++", count, msgType, num, idx);
            //
            pCb->doNotifyCb(msgType, ext1, ext2, ext3);
            //
            CAM_LOGD_IF(mLogLevel >= 2,
                "do notify (%d) [%d] %d %d ---", count, msgType, num, idx);
        }
    };

    MINT32 setMetaList(const List<MetaSet_T>& controls)
    {
        List<MetaSet_T>::const_iterator it = controls.begin();
        MINT32 nListSize = controls.size();
        MINT32 nMagicNum = 0;

        #ifdef HAL3A_SIMULATOR_REQ_PROC_KEY
        #undef HAL3A_SIMULATOR_REQ_PROC_KEY
        #endif
        #define HAL3A_SIMULATOR_REQ_PROC_KEY 2

        if (nListSize >= (HAL3A_SIMULATOR_REQ_PROC_KEY + 1) * mSubsampleCount) {
            for (int i = 0;
                i < ((HAL3A_SIMULATOR_REQ_PROC_KEY + 1) * mSubsampleCount) &&
                    it != controls.end(); i++, it++) {
                if (i >= (HAL3A_SIMULATOR_REQ_PROC_KEY * mSubsampleCount)) {
                    if (!NSCam::v3::tryGetMetadata<MINT32>(&(it->halMeta),
                        MTK_P1NODE_PROCESSOR_MAGICNUM, nMagicNum)) {
                        CAM_LOGW("[%s] Set Meta List fail", mUserName.string());
                        break;
                    }
                    enQueueNum(nMagicNum);
                }
            }
        } else {
            CAM_LOGI("[%s] Set Meta List size not enough : %d < (%d * %d)",
                mUserName.string(), nListSize, mSubsampleCount,
                (HAL3A_SIMULATOR_REQ_PROC_KEY + 1));
        }
        if (mLogLevel >= 2) {
            dumpQueueNum();
        }

        #undef HAL3A_SIMULATOR_REQ_PROC_KEY

        {
            Mutex::Autolock _l(mCallbackLock);
            if (!mCallbackEnable) {
                #if 0
                // after the first set,
                // it might need to take some time for the first calculation
                // then, it can start to callback
                // for simulation, adjust the sleep time
                usleep(mCallbackTimeIntervalUs / 2);
                #endif
                mCallbackEnable = MTRUE;
                mCallbackSkip = MTRUE;
                mCallbackCount = 0;
                mCallbackCond.broadcast();
            }
        }

        return nMagicNum;
    };

    void enQueueNum(MINT32 num)
    {
        Mutex::Autolock _l(mNumQueueLock);
        mvNumQueue.push_back(num);
        return;
    };

    MINT32 deQueueNum(void)
    {
        Mutex::Autolock _l(mNumQueueLock);
        MINT32 num = -1;
        if (mvNumQueue.size() > 0) {
            android::Vector<MINT32>::iterator it = mvNumQueue.begin();
            num = *(it);
            mvNumQueue.erase(it);
        }
        return num;
    };

    void dumpQueueNum(void)
    {
        Mutex::Autolock _l(mNumQueueLock);
        android::String8 str = android::String8::format("Q[%d] = { ", (int)(mvNumQueue.size()));
        android::Vector<MINT32>::iterator it = mvNumQueue.begin();
        for(; it != mvNumQueue.end(); it++) {
            str += android::String8::format(" %d ", (*it));
        }
        str += android::String8::format(" }");
        CAM_LOGD("%s", str.string());
        return;
    };

};

#endif

/******************************************************************************
 *  Opaque Reprocessing Utility
 ******************************************************************************/
#define MAX_SIZE        (200000)
//#include <mtkcam/utils/metadata/IMetadata.h>

class OpaqueReprocUtil
{
private:
    typedef struct
    {
        MINT8 alignByte;
        MSize imgRawSize;
        MINT imgRawFormat;
        size_t strideInByte;
        size_t sizeInByte;
        size_t appMetaOffset;
        size_t appMetaLength;
        size_t halMetaOffset;
        size_t halMetaLength;
    } opaqueReprocInfo_t;

    static MVOID    dumpOpaqueReprocInfo(opaqueReprocInfo_t *pInfo)
    {
        CAM_LOGD( "[opaque] alignByte(0x%x) RawSize(%d,%d) RawFormat(0x%x) "
                  "strideInByte(%zu) sizeInByte(%zu) appMetaOffset(%zu) "
                  "appMetaLength(%zu) halMetaOffset(%zu) halMetaLength(%zu)",
                  pInfo->alignByte, pInfo->imgRawSize.w, pInfo->imgRawSize.h,
                  pInfo->imgRawFormat, pInfo->strideInByte, pInfo->sizeInByte,
                  pInfo->appMetaOffset, pInfo->appMetaLength,
                  pInfo->halMetaOffset, pInfo->halMetaLength
                );
    }

public:

    static intptr_t getBufferAddress(
            android::sp<IImageBufferHeap> const& inImageBufferHeap)
    {
        return inImageBufferHeap->getBufVA(0);
    }

    static MERROR setOpaqueInfoToHeap(
            android::sp<IImageBufferHeap> &inImageBufferHeap,
            MSize const imgRawSize,
            MINT const imgRawFormat,
            size_t const strideInByte,
            size_t const sizeInByte)
    {
        ssize_t rawInfoOffset = (ssize_t)inImageBufferHeap->getBufSizeInBytes(0) - sizeof(opaqueReprocInfo_t);
        if(rawInfoOffset < 0)
            return BAD_VALUE;

        opaqueReprocInfo_t *pZslRawInfo = (opaqueReprocInfo_t *) (getBufferAddress(inImageBufferHeap) + rawInfoOffset);

        pZslRawInfo->alignByte = 0x00;
        pZslRawInfo->imgRawSize = imgRawSize;
        pZslRawInfo->imgRawFormat = imgRawFormat;
        pZslRawInfo->strideInByte = strideInByte;
        pZslRawInfo->sizeInByte = sizeInByte;
        pZslRawInfo->appMetaOffset = 0;
        pZslRawInfo->appMetaLength = 0;
        pZslRawInfo->halMetaOffset = 0;
        pZslRawInfo->halMetaLength = 0;
        // dumpOpaqueReprocInfo(pZslRawInfo);
        return OK;
    }

    static MERROR getImageBufferFromHeap(
            android::sp<IImageBufferHeap>& inImageBufferHeap,
            android::sp<IImageBuffer>& outImageBuffer)
    {
        size_t rawInfoOffset = inImageBufferHeap->getBufSizeInBytes(0) - sizeof(opaqueReprocInfo_t);
        opaqueReprocInfo_t *pZslRawInfo = (opaqueReprocInfo_t *) (getBufferAddress(inImageBufferHeap) + rawInfoOffset);

        if(pZslRawInfo->alignByte != 0x00)
            return NO_INIT;

        // create outImageBuffer from inImageBufferHeap
        size_t strideInBytes[3]{
                pZslRawInfo->strideInByte == 0 ? inImageBufferHeap->getBufStridesInBytes(0) : pZslRawInfo->strideInByte,
                0, 0};
        outImageBuffer = inImageBufferHeap->createImageBuffer_FromBlobHeap(
                0,
                pZslRawInfo->imgRawFormat == 0 ? inImageBufferHeap->getImgFormat() : pZslRawInfo->imgRawFormat,
                pZslRawInfo->imgRawSize == 0 ? inImageBufferHeap->getImgSize() : pZslRawInfo->imgRawSize,
                strideInBytes
        );
        // dumpOpaqueReprocInfo(pZslRawInfo);
        return OK;
    }

    static MERROR setAppMetadataToHeap(
            android::sp<IImageBufferHeap>& inImageBufferHeap,
            IMetadata& appMeta)
    {
        size_t rawInfoOffset = inImageBufferHeap->getBufSizeInBytes(0) - sizeof(opaqueReprocInfo_t);
        intptr_t ptrOpaqueBuffer = getBufferAddress(inImageBufferHeap);
        opaqueReprocInfo_t *pZslRawInfo = (opaqueReprocInfo_t *) (ptrOpaqueBuffer + rawInfoOffset);

        // validate
        if(pZslRawInfo->alignByte != 0x00)
            return NO_INIT;
        else if(pZslRawInfo->appMetaLength != 0)
            return ALREADY_EXISTS;

        // get flatten app meta
        // 10 KB
        size_t max_flatten_size = MAX_SIZE;
        void* strFlatten = (void*)malloc(MAX_SIZE);
        size_t buf_size = appMeta.flatten(strFlatten, max_flatten_size);
        CAM_LOGD_IF(0, "[opaque] app meta flatten to size: %zu ", buf_size);
        if(buf_size == 0)
        {
            free(strFlatten);
            return NO_MEMORY;
        }
        // get the length and offset of app meta
        pZslRawInfo->appMetaLength = buf_size;
        pZslRawInfo->appMetaOffset = ((pZslRawInfo->halMetaLength != 0) ? pZslRawInfo->halMetaOffset: rawInfoOffset)
                                     - pZslRawInfo->appMetaLength;

        // write to buffer
        void *pAppMata = (void *)(ptrOpaqueBuffer + pZslRawInfo->appMetaOffset);
        memcpy(pAppMata, strFlatten, buf_size);
        free(strFlatten);

        // dumpOpaqueReprocInfo(pZslRawInfo);
        return OK;
    }

    static MERROR setHalMetadataToHeap(
            android::sp<IImageBufferHeap>& inImageBufferHeap,
            IMetadata& halMeta)
    {
        size_t rawInfoOffset = inImageBufferHeap->getBufSizeInBytes(0) - sizeof(opaqueReprocInfo_t);
        intptr_t ptrOpaqueBuffer = getBufferAddress(inImageBufferHeap);
        opaqueReprocInfo_t *pZslRawInfo = (opaqueReprocInfo_t *) (ptrOpaqueBuffer + rawInfoOffset);

        // validate
        if(pZslRawInfo->alignByte != 0x00)
            return NO_INIT;
        else if(pZslRawInfo->halMetaLength != 0)
            return ALREADY_EXISTS;

        // get flatten app meta
        // 10 KB
        size_t max_flatten_size = MAX_SIZE;
        void* strFlatten = (void*)malloc(MAX_SIZE);
        size_t buf_size = halMeta.flatten(strFlatten, max_flatten_size);
        CAM_LOGD_IF(0, "[opaque] hal meta flatten to size: %zu", buf_size);
        if(buf_size == 0)
        {
            free(strFlatten);
            return NO_MEMORY;
        }
        // get the length and offset of hal meta
        pZslRawInfo->halMetaLength = buf_size;
        pZslRawInfo->halMetaOffset = ((pZslRawInfo->appMetaLength != 0) ? pZslRawInfo->appMetaOffset : rawInfoOffset)
                                     - pZslRawInfo->halMetaLength;
        // write to buffer
        void *pHalMata = (void *)(ptrOpaqueBuffer + pZslRawInfo->halMetaOffset);
        memcpy(pHalMata, strFlatten, buf_size);
        free(strFlatten);
        // dumpOpaqueReprocInfo(pZslRawInfo);
        return OK;
    }

    static MERROR getAppMetadataFromHeap(
            android::sp<IImageBufferHeap> const& inImageBufferHeap,
            IMetadata& appMeta)
    {
        size_t rawInfoOffset = inImageBufferHeap->getBufSizeInBytes(0) - sizeof(opaqueReprocInfo_t);
        intptr_t ptrOpaqueBuffer = getBufferAddress(inImageBufferHeap);

        opaqueReprocInfo_t *pZslRawInfo = (opaqueReprocInfo_t *) (ptrOpaqueBuffer + rawInfoOffset);
        if(pZslRawInfo->alignByte != 0x00)
            return NO_INIT;

        void *pAppMata = (void *)(ptrOpaqueBuffer + pZslRawInfo->appMetaOffset);
        // un-flatten appMeta from inImageBufferHeap
        size_t buf_size = appMeta.unflatten(pAppMata, pZslRawInfo->appMetaLength);
        CAM_LOGD_IF(0, "[opaque] app meta unflatten from size: %zu", buf_size);
        if(buf_size == 0)
            return NO_MEMORY;

        // dumpOpaqueReprocInfo(pZslRawInfo);
        return OK;
    }


    static MERROR getHalMetadataFromHeap(
            android::sp<IImageBufferHeap> const& inImageBufferHeap,
            IMetadata& halMeta)
    {
        size_t rawInfoOffset = inImageBufferHeap->getBufSizeInBytes(0) - sizeof(opaqueReprocInfo_t);
        intptr_t ptrOpaqueBuffer = getBufferAddress(inImageBufferHeap);

        opaqueReprocInfo_t *pZslRawInfo = (opaqueReprocInfo_t *)(ptrOpaqueBuffer + rawInfoOffset);
        if(pZslRawInfo->alignByte != 0x00)
            return NO_INIT;

        void *pHalMata = (void *)(ptrOpaqueBuffer + pZslRawInfo->halMetaOffset);
        // un-flatten halMeta from inImageBufferHeap
        size_t buf_size = halMeta.unflatten(pHalMata, pZslRawInfo->halMetaLength);
        CAM_LOGD_IF(0, "[opaque] hal meta unflatten from size: %zu", buf_size);
        if(buf_size == 0)
            return NO_MEMORY;

        // dumpOpaqueReprocInfo(pZslRawInfo);
        return OK;
    }
};
};
};

#endif //_MTKCAM_HWNODE_UTILIIIES_H_

