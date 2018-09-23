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

#define LOG_TAG "MtkCam/P1NodeImp"
//
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/common.h>
#include "BaseNode.h"
#include "hwnode_utilities.h"
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
//
#include <utils/RWLock.h>
#include <utils/Thread.h>
//
#include <sys/prctl.h>
#include <sys/resource.h>
#include <system/thread_defs.h>

//
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/drv/iopipe/CamIO/IHalCamIO.h>
#include <mtkcam/drv/iopipe/CamIO/INormalPipe.h>
#include <vector>
#include <mtkcam/feature/eis/lmv_hal.h>
#include <mtkcam/feature/eis/eis_hal.h>
#include <mtkcam/feature/vhdr/vhdr_hal.h>
#include <mtkcam/aaa/lcs/lcs_hal.h>

//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#include "Profile.h"
#include <cutils/properties.h>
#include <utils/Atomic.h>
//
#include <mtkcam/utils/std/DebugScanLine.h>
//
#include <mtkcam/utils/metastore/IMetadataProvider.h>
//
#include <mtkcam/utils/imgbuf/IIonImageBufferHeap.h>
#include <mtkcam/utils/imgbuf/IDummyImageBufferHeap.h>
//
#if (HWNODE_HAVE_AEE_FEATURE)
#include <aee.h>
#ifdef AEE_ASSERT
#undef AEE_ASSERT
#endif
#define AEE_ASSERT(String) \
    do { \
        CAM_LOGE("ASSERT("#String") fail"); \
        aee_system_exception( \
            LOG_TAG, \
            NULL, \
            DB_OPT_DEFAULT, \
            String); \
    } while(0)
#else
#define AEE_ASSERT(String)
#endif
//
using namespace std;
using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
//using namespace NSCam::Utils;
using namespace NSCam::Utils::Sync;
using namespace NSCam::NSIoPipe;
using namespace NSCam::NSIoPipe::NSCamIOPipe;
using namespace NS3Av3;

/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)                  CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)                  CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)                  CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)                  CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)                  CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)                  CAM_LOGA("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)                  CAM_LOGF("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD_WITH_OPENID(fmt, arg...)      CAM_LOGD("[%s] [Cam::%d] " fmt, __FUNCTION__, getOpenId(), ##arg)
#define MY_LOGI_WITH_OPENID(fmt, arg...)      CAM_LOGI("[%s] [Cam::%d] " fmt, __FUNCTION__, getOpenId(), ##arg)

//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF_P1(cond, ...)    do { if ( (cond) ) { MY_LOGD_WITH_OPENID(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF_P1(cond, ...)    do { if ( (cond) ) { MY_LOGI_WITH_OPENID(__VA_ARGS__); } }while(0)

/******************************************************************************
 *
 ******************************************************************************/
#define SUPPORT_3A              (1)
#define SUPPORT_ISP             (1)
#define SUPPORT_PERFRAME_CTRL   (0)
#define SUPPORT_LMV             (1)
#if MTK_CAM_VHDR_SUPPORT
#define SUPPORT_VHDR            (1)
#else
#define SUPPORT_VHDR            (0)
#endif
#define SUPPORT_LCS             (1)

//#define SUPPORT_SCALING_CROP    (1)
//#define SUPPORT_SCALING_CROP_IMGO   (SUPPORT_SCALING_CROP && (0))
//#define SUPPORT_SCALING_CROP_RRZO   (SUPPORT_SCALING_CROP && (1))

#define FORCE_EIS_ON                (SUPPORT_LMV && (0))
#define FORCE_3DNR_ON               (SUPPORT_LMV && (0))
#define DISABLE_BLOB_DUMMY_BUF      (0)

#ifdef HAL3A_SIMULATOR_IMP
typedef NSCam::v3::IHal3ASimulator IHal3A_T;
#ifdef MAKE_Hal3A
#undef MAKE_Hal3A
#endif
#define MAKE_Hal3A(...) NSCam::v3::Hal3ASimulator::createInstance(__VA_ARGS__)
#else
typedef IHal3A IHal3A_T;
#endif

/******************************************************************************
 *
 ******************************************************************************/

#define MY_LOGD1(...)           MY_LOGD_IF_P1(1<=mLogLevel, __VA_ARGS__)
#define MY_LOGD2(...)           MY_LOGD_IF_P1(2<=mLogLevel, __VA_ARGS__)
#define MY_LOGD3(...)           MY_LOGD_IF_P1(3<=mLogLevel, __VA_ARGS__)

#define MY_LOGI1(...)           MY_LOGI_IF_P1(1<=mLogLevelI, __VA_ARGS__)
#define MY_LOGI2(...)           MY_LOGI_IF_P1(2<=mLogLevelI, __VA_ARGS__)
#define MY_LOGI3(...)           MY_LOGI_IF_P1(3<=mLogLevelI, __VA_ARGS__)

#define FUNCTION_S_IN           MY_LOGD_IF(1<=mLogLevel, "+");
#define FUNCTION_S_OUT          MY_LOGD_IF(1<=mLogLevel, "-");
#define FUNCTION_IN             MY_LOGD_IF(2<=mLogLevel, "+");
#define FUNCTION_OUT            MY_LOGD_IF(2<=mLogLevel, "-");
#define PUBLIC_APIS_IN          MY_LOGD_IF_P1(1<=mLogLevel, "API +");
#define PUBLIC_APIS_OUT         MY_LOGD_IF_P1(1<=mLogLevel, "API -");
#define PUBLIC_API_IN           MY_LOGD_IF_P1(2<=mLogLevel, "API +");
#define PUBLIC_API_OUT          MY_LOGD_IF_P1(2<=mLogLevel, "API -");
#define FUNCTION_P1S_IN         MY_LOGD_IF_P1(1<=mLogLevel, "+");
#define FUNCTION_P1S_OUT        MY_LOGD_IF_P1(1<=mLogLevel, "-");
#define FUNCTION_P1_IN          MY_LOGD_IF_P1(2<=mLogLevel, "+");
#define FUNCTION_P1_OUT         MY_LOGD_IF_P1(2<=mLogLevel, "-");

#define IS_P1_LOGI              (MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT >= 2) // for system use LOGI
#define IS_P1_LOGD              (MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT >= 3) // for system use LOGD
#define P1_LOG_LEN              (256)

/* P1_LOGD only use in P1NodeImp */
#if (IS_P1_LOGD) // for system use LOGD
#define P1_LOGD(fmt, arg...)                                        \
    do {                                                            \
        if (mLogLevel >= 1) {                                       \
            char strLog[P1_LOG_LEN] = {0};                          \
            snprintf(strLog, P1_LOG_LEN,                            \
                "Cam::%d R%d S%d E%d D%d O%d #%d@%d", getOpenId(),  \
                mTagReq.get(), mTagSet.get(),                       \
                mTagEnq.get(), mTagDeq.get(),                       \
                mTagOut.get(), mTagList.get(), mReqState);          \
            CAM_LOGD("[%s] [%s] " fmt, __FUNCTION__, strLog, ##arg);\
        }                                                           \
    } while(0)
#else
#define P1_LOGD(fmt, arg...)
#endif

#define P1THREAD_POLICY         (SCHED_OTHER)
#define P1THREAD_PRIORITY       (ANDROID_PRIORITY_FOREGROUND-2)

#define P1SOFIDX_INIT_VAL       (0)
#define P1SOFIDX_LAST_VAL       (0xFF)
#define P1SOFIDX_NULL_VAL       (0xFFFFFFFF)

#define P1GET_FRM_NUM(frame) ((frame == NULL) ?\
                                (MUINT32)0xFFFFFFFF : frame->getFrameNo())
#define P1GET_REQ_NUM(frame) ((frame == NULL) ?\
                                (MUINT32)0xFFFFFFFF : frame->getRequestNo())

#define P1INFO_NODE_STR "(%d)(%d:%d)@(%d)[T:%d O:0x%x R:0x%x S:%d C:%d F:%d]"
#define P1INFO_NODE_VAR(node) node.magicNum,\
    P1GET_FRM_NUM(node.appFrame), P1GET_REQ_NUM(node.appFrame), node.sofIdx,\
    node.reqType, node.reqOutSet, node.expRec, node.exeState, node.capType,\
    node.needFlush

#define P1INFO_NODE(LOG_LEVEL, node) MY_LOGD##LOG_LEVEL(\
    P1INFO_NODE_STR, P1INFO_NODE_VAR(node));

#define P1_CHECK_STREAM(TYPE, stream)\
    if (stream < (STREAM_##TYPE)STREAM_ITEM_START ||\
        stream >= STREAM_##TYPE##_NUM) {\
        MY_LOGE("stream index invalid %d/%d", stream, STREAM_##TYPE##_NUM);\
        return INVALID_OPERATION;\
    }

#define P1_CHECK_NODE_STREAM(TYPE, node, stream)\
    if (node.appFrame == NULL) {\
        MY_LOGW("pipeline frame is NULL %d@%d", stream, node.magicNum);\
        return INVALID_OPERATION;\
    };\
    if (mvStream##TYPE[stream] == NULL) {\
        MY_LOGW("StreamId is NULL %d@%d", stream, node.magicNum);\
        return BAD_VALUE;\
    };\
    if (!node.streamBuf##TYPE[stream].bExist) {\
        MY_LOGD1("stream is not exist %d@%d", stream, node.magicNum);\
        return OK;\
    };


/******************************************************************************
 *
 ******************************************************************************/
namespace {
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#if 0
class AAAResult {
    protected:
        struct info{
            sp<IPipelineFrame> spFrame;
            IMetadata          resultVal;
            MUINT32            mFlag;
            info()
                : spFrame(0)
                , resultVal()
                , mFlag(0)
                {}
        };

    protected:
        enum KeyType {
            KeyType_StrobeState = 1,
            KeyType_Rest        = 2, //1,2,4,8,...
        };

    protected:
        mutable Mutex              mLock;
        KeyedVector<MUINT32, info> mData; //key: magicnum, val: info
        MUINT32                    mAllKey;

    public:

        AAAResult()
            : mLock()
            , mData()
            , mAllKey(KeyType_Rest)
            //, mAllKey(KeyType_StrobeState|KeyType_Rest)
            {}

        void add(MUINT32 magicNum, MUINT32 key, MUINT32 val)
        {
             Mutex::Autolock lock(mLock);
             if(key != MTK_FLASH_STATE) {
                 //unSupported
                 return;
             }

             IMetadata::IEntry entry(MTK_FLASH_STATE);
             entry.push_back(val, Type2Type< MUINT8 >()); //{MTK_FLASH_STATE, MUINT8}
             ssize_t i = mData.indexOfKey(magicNum);
             if(i < 0) {
                 info data;
                 data.resultVal.update(MTK_FLASH_STATE, entry);

    data.mFlag |= KeyType_StrobeState;
                 mData.add(magicNum, data);
             } else {
                 info& data = mData.editValueFor(magicNum);
                 data.resultVal.update(MTK_FLASH_STATE, entry);

    data.mFlag |= KeyType_StrobeState;
             }
        }

        void add(MUINT32 magicNum, sp<IPipelineFrame> pframe, IMetadata &rVal)
        {
             Mutex::Autolock lock(mLock);
             ssize_t i = mData.indexOfKey(magicNum);
             if(i < 0) {
                 info data;
                 data.spFrame = pframe;
                 data.resultVal = rVal;

data.mFlag |= KeyType_Rest;
                 mData.add(magicNum, data);
             } else {
                 info& data = mData.editValueFor(magicNum);
                 data.spFrame = pframe;
                 data.resultVal += rVal;
                 data.mFlag |= KeyType_Rest;
             }
        }

        const info& valueFor(const MUINT32& magicNum) const {
            return mData.valueFor(magicNum);
        }

        bool isCompleted(MUINT32 magicNum) {
            Mutex::Autolock lock(mLock);
            return (mData.valueFor(magicNum).mFlag & mAllKey) == mAllKey;
        }

        void removeItem(MUINT32 key) {
            Mutex::Autolock lock(mLock);
            mData.removeItem(key);
        }

        void clear() {
            debug();
            Mutex::Autolock lock(mLock);
            mData.clear();
        }

        void debug() {
            Mutex::Autolock lock(mLock);
            for(size_t i = 0; i < mData.size(); i++) {
                MY_LOGW_IF((mData.valueAt(i).mFlag & KeyType_StrobeState) == 0,
                           "No strobe result: (%d)", mData.keyAt(i));
                MY_LOGW_IF((mData.valueAt(i).mFlag & KeyType_Rest) == 0,
                           "No rest result: (%d)", mData.keyAt(i));
            }
        }
};
#endif
#if 0
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class Storage {

    protected:
        typedef DefaultKeyedVector<MINTPTR, sp<IImageBuffer> >  MapType;
        MapType                    mvStorageQ;
        mutable Mutex              mStorageLock;
        MINT32                     mLogEnable;
    public:
                                   Storage()
                                       : mvStorageQ()
                                       , mStorageLock()
                                       , mLogEnable(0)
                                       {}

        virtual                   ~Storage(){};

        void                       init(MINT32 logEnable)
                                   {
                                       mvStorageQ.clear();
                                       mLogEnable = logEnable;
                                   }

        void                       uninit()
                                   {
                                       mvStorageQ.clear();
                                   }

        void                       enque(sp<IImageStreamBuffer> const& key, sp<IImageBuffer> &value) {
                                       Mutex::Autolock lock(mStorageLock);
                                       MY_LOGD_IF(mLogEnable, "Storage-enque::(key)0x%x/(val)0x%x",
                                           key.get(), value.get());
                                       MY_LOGD_IF(mLogEnable, "Info::(val-pa)0x%x/%d/%d/%d/%d/%d",
                                        value->getBufPA(0),value->getImgSize().w, value->getImgSize().h,
                                        value->getBufStridesInBytes(0), value->getBufSizeInBytes(0), value->getPlaneCount());

                                       mvStorageQ.add(reinterpret_cast<MINTPTR>(key.get()), value);
                                   };


        sp<IImageBuffer>           deque(MINTPTR key) {
                                       Mutex::Autolock lock(mStorageLock);
                                       sp<IImageBuffer> pframe = mvStorageQ.valueFor(key);
                                       if (pframe != NULL)
                                       {
                                           mvStorageQ.removeItem(key); //should un-mark
                                           MY_LOGD_IF(mLogEnable, "Storage-deque::(key)0x%x/(val)0x%x",
                                            key, pframe.get());
                                           MY_LOGD_IF(mLogEnable, "(val-pa)0x%x",
                                            pframe->getBufPA(0));
                                           return pframe;
                                       }
                                       return NULL;
                                   }
        sp<IImageBuffer>           query(MINTPTR key) {
                                       Mutex::Autolock lock(mStorageLock);
                                       sp<IImageBuffer> pframe = mvStorageQ.valueFor(key);
                                       if (pframe != NULL)
                                       {
                                           MY_LOGD_IF(mLogEnable, "Storage-deque::(key)0x%x/(val)0x%x",
                                            key, pframe.get());
                                           MY_LOGD_IF(mLogEnable, "Info::(val-pa)0x%x",
                                            pframe->getBufPA(0));
                                           return pframe;
                                       }
                                       return NULL;
                                   }
};
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef ALIGN_UPPER
#undef ALIGN_UPPER
#endif
#ifdef ALIGN_LOWER
#undef ALIGN_LOWER
#endif
#define ALIGN_UPPER(x,a)    (((x)+((typeof(x))(a)-1))&~((typeof(x))(a)-1))
#define ALIGN_LOWER(x,a)    (((x))&~((typeof(x))(a)-1))


#ifdef NEED_LOCK
#undef NEED_LOCK
#endif
#define NEED_LOCK(need, mutex)\
    if (need) {\
        mutex.lock();\
    }
#ifdef NEED_UNLOCK
#undef NEED_UNLOCK
#endif
#define NEED_UNLOCK(need, mutex)\
    if (need) {\
        mutex.unlock();\
    }

#ifdef CHECK_LAST_FRAME_SKIPPED
#undef CHECK_LAST_FRAME_SKIPPED
#endif
#define CHECK_LAST_FRAME_SKIPPED(LAST_SOF_IDX, THIS_SOF_IDX)\
    ((LAST_SOF_IDX == P1SOFIDX_NULL_VAL) ?\
        (true) :\
        ((LAST_SOF_IDX == P1SOFIDX_LAST_VAL) ?\
            ((THIS_SOF_IDX != 0) ? (true) : (false)) :\
            ((THIS_SOF_IDX != (LAST_SOF_IDX + 1)) ? (true) : (false))));

#ifdef RESIZE_RATIO_MAX_10X
#undef RESIZE_RATIO_MAX_10X
#endif
#define RESIZE_RATIO_MAX_10X    (4)

#ifdef P1_EISO_MIN_HEIGHT
#undef P1_EISO_MIN_HEIGHT
#endif
#define P1_EISO_MIN_HEIGHT      (160)

#ifdef P1_STUFF_BUF_HEIGHT
#undef P1_STUFF_BUF_HEIGHT
#endif
#define P1_STUFF_BUF_HEIGHT(rrzo, config)   (\
    (rrzo && IS_PORT(CONFIG_PORT_EISO, config)) ? (P1_EISO_MIN_HEIGHT) :\
    (rrzo) ? (2) : (1))
//
#ifdef P1_IMGO_DEF_FMT
#undef P1_IMGO_DEF_FMT
#endif
#define P1_IMGO_DEF_FMT (eImgFmt_BAYER10)
//
enum EXE_STATE
{
    EXE_STATE_NULL          = 0,
    EXE_STATE_REQUESTED,
    EXE_STATE_PROCESSING,
    EXE_STATE_DONE
};
//
enum REQ_TYPE
{
    REQ_TYPE_UNKNOWN        = 0,
    REQ_TYPE_NORMAL,
    REQ_TYPE_INITIAL,
    //REQ_TYPE_PADDING,
    //REQ_TYPE_DUMMY,
    REQ_TYPE_REDO,
    REQ_TYPE_YUV
};
//
#ifdef REQ_SET
#undef REQ_SET
#endif
#define REQ_SET(bit)        ((MUINT32)(0x1 << bit))
#ifdef REQ_SET_NONE
#undef REQ_SET_NONE
#endif
#define REQ_SET_NONE        (0x0)
#ifdef IS_OUT
#undef IS_OUT
#endif
#define IS_OUT(out, set)    ((set & REQ_SET(out)) == REQ_SET(out))
enum REQ_OUT
{
    REQ_OUT_RESIZER         = 0,    // 0x 01
    REQ_OUT_RESIZER_STUFF,          // 0x 02
    REQ_OUT_LCSO,                   // 0x 04
    REQ_OUT_LCSO_STUFF,             // 0x 08
    REQ_OUT_FULL_PURE,              // 0x 10
    REQ_OUT_FULL_PROC,              // 0x 20
    REQ_OUT_FULL_OPAQUE,            // 0x 40
    REQ_OUT_FULL_STUFF,             // 0x 80
    REQ_OUT_MAX
};
//
#ifdef EXP_REC
#undef EXP_REC
#endif
#define EXP_REC(bit)        ((MUINT32)(0x1 << bit))
#ifdef EXP_REC_NONE
#undef EXP_REC_NONE
#endif
#define EXP_REC_NONE        (0x0)
#ifdef IS_EXP
#undef IS_EXP
#endif
#define IS_EXP(exp, rec)    ((rec & EXP_REC(exp)) == EXP_REC(exp))
enum EXP_EVT
{
    EXP_EVT_UNKNOWN         = 0,
    EXP_EVT_NOBUF_RRZO,
    EXP_EVT_NOBUF_IMGO,
    EXP_EVT_NOBUF_EISO,
    EXP_EVT_NOBUF_LCSO,
    EXP_EVT_MAX
};
//
#ifdef IS_PORT
#undef IS_PORT
#endif
#define IS_PORT(port, set)  ((set & port) == port)
enum CONFIG_PORT
{
    CONFIG_PORT_NONE        = (0x0),
    CONFIG_PORT_RRZO        = (0x1 << 0),
    CONFIG_PORT_IMGO        = (0x1 << 1),
    CONFIG_PORT_EISO        = (0x1 << 2),
    CONFIG_PORT_LCSO        = (0x1 << 3),
    CONFIG_PORT_ALL         = (0xFFFFFFFF) // MUINT32
};

#ifdef BIN_RESIZE
#undef BIN_RESIZE
#endif
#define BIN_RESIZE(x)  (x = (x >> 1))

#ifdef BIN_REVERT
#undef BIN_REVERT
#endif
#define BIN_REVERT(x)  (x = (x << 1))

/******************************************************************************
 *
 ******************************************************************************/
inline MBOOL
isEISOn(
    IMetadata* const inApp
)
{
    if (inApp == NULL) {
        return false;
    }
    MUINT8 eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    if(!tryGetMetadata<MUINT8>(inApp,
        MTK_CONTROL_VIDEO_STABILIZATION_MODE, eisMode)) {
        MY_LOGW_IF(0, "no MTK_CONTROL_VIDEO_STABILIZATION_MODE");
    }
#if FORCE_EIS_ON
    eisMode = MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
#endif
    return eisMode == MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON;
}

/******************************************************************************
 *
 ******************************************************************************/
inline MBOOL
is3DNROn(
    IMetadata* const inApp
)
{
    if (inApp == NULL) {
        return false;
    }
    MINT32 e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;
    if(!tryGetMetadata<MINT32>(inApp,
        MTK_NR_FEATURE_3DNR_MODE, e3DnrMode)) {
        MY_LOGW_IF(0, "no MTK_NR_FEATURE_3DNR_MODE");
    }
#if FORCE_3DNR_ON
    e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_ON;
#endif
    return e3DnrMode == MTK_NR_FEATURE_3DNR_MODE_ON;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL calculateCropInfoFull(
    MUINT32 pixelMode,
    MSize const& sensorSize,
    MSize const& bufferSize,
    MRect const& querySrcRect,
    MRect& resultSrcRect,
    MSize& resultDstSize,
    MINT32 mLogLevel = 0
)
{
    if ((querySrcRect.size().w == sensorSize.w) &&
        (querySrcRect.size().h == sensorSize.h)) {
        return false;
    }
    if ((querySrcRect.size().w > bufferSize.w || // cannot over buffer size
        querySrcRect.size().h > bufferSize.h) ||
        (((querySrcRect.leftTop().x + querySrcRect.size().w) > sensorSize.w) ||
        ((querySrcRect.leftTop().y + querySrcRect.size().h) > sensorSize.h))
        ) {
        MY_LOGD_IF((2 <= mLogLevel), "calculateCropInfoFull input invalid "
            "pixelMode(%d) sensorSize(%dx%d) bufferSize(%dx%d) "
            "querySrcRect_size(%dx%d) querySrcRect_start(%d,%d)", pixelMode,
            sensorSize.w, sensorSize.h, bufferSize.w, bufferSize.h,
            querySrcRect.size().w, querySrcRect.size().h,
            querySrcRect.leftTop().x, querySrcRect.leftTop().y);
        return false;
    }
    // TODO: query the valid value, currently do not crop in IMGO
    resultDstSize = MSize(sensorSize.w, sensorSize.h);
    resultSrcRect = MRect(MPoint(0, 0), resultDstSize);

    return true;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL calculateCropInfoResizer(
    MUINT32 pixelMode,
    MUINT32 imageFormat,
    MSize const& sensorSize,
    MSize const& bufferSize,
    MRect const& querySrcRect,
    MRect& resultSrcRect,
    MSize& resultDstSize,
    MINT32 mLogLevel = 0
)
{
    if ((querySrcRect.size().w == sensorSize.w) &&
        (querySrcRect.size().h == sensorSize.h)) {
        return false;
    }
    if ((((querySrcRect.leftTop().x + querySrcRect.size().w) > sensorSize.w) ||
        ((querySrcRect.leftTop().y + querySrcRect.size().h) > sensorSize.h))
        ) {
        MY_LOGD_IF((2 <= mLogLevel), "calculateCropInfoResizer input invalid "
            "pixelMode(%d) sensorSize(%dx%d) bufferSize(%dx%d) "
            "querySrcRect_size(%dx%d) querySrcRect_start(%d,%d)", pixelMode,
            sensorSize.w, sensorSize.h, bufferSize.w, bufferSize.h,
            querySrcRect.size().w, querySrcRect.size().h,
            querySrcRect.leftTop().x, querySrcRect.leftTop().y);
        return false;
    }
    //
    MPoint::value_type src_crop_x = querySrcRect.leftTop().x;
    MPoint::value_type src_crop_y = querySrcRect.leftTop().y;
    MSize::value_type src_crop_w = querySrcRect.size().w;
    MSize::value_type src_crop_h = querySrcRect.size().h;
    MSize::value_type dst_size_w = 0;
    MSize::value_type dst_size_h = 0;
    if (querySrcRect.size().w < bufferSize.w) {
        dst_size_w = querySrcRect.size().w;
        // check start.x
        {
            NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo info;
            NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
                NSCam::NSIoPipe::PORT_RRZO.index,
                NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_START_X,
                (EImageFormat)imageFormat,
                src_crop_x, info);
            src_crop_x = info.crop_x;
        }
        // check size.w
        {
            NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo info;
            NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
                NSCam::NSIoPipe::PORT_RRZO.index,
                NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX|
                NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE,
                (EImageFormat)imageFormat,
                dst_size_w, info);
            dst_size_w = info.x_pix;
        }
        //
        dst_size_w = MIN(dst_size_w, sensorSize.w);
        src_crop_w = dst_size_w;
        if (src_crop_w > querySrcRect.size().w) {
            if ((src_crop_x + src_crop_w) > sensorSize.w) {
                src_crop_x = sensorSize.w - src_crop_w;
            }
        }
    } else {
        if ((src_crop_w * RESIZE_RATIO_MAX_10X) > (bufferSize.w * 10)) {
            MY_LOGW("calculateCropInfoResizer resize width invalid "
                    "(%d):(%d)", src_crop_w, bufferSize.w);
            return false;
        }
        dst_size_w = bufferSize.w;
    }
    if (querySrcRect.size().h < bufferSize.h) {
        dst_size_h = querySrcRect.size().h;
        dst_size_h = MIN(ALIGN_UPPER(dst_size_h, 2), sensorSize.h);
        src_crop_h = dst_size_h;
        if (src_crop_h > querySrcRect.size().h) {
            if ((src_crop_y + src_crop_h) > sensorSize.h) {
                src_crop_y = sensorSize.h - src_crop_h;
            }
        }
    } else {
        if ((src_crop_h * RESIZE_RATIO_MAX_10X) > (bufferSize.h * 10)) {
            MY_LOGW("calculateCropInfoResizer resize height invalid "
                    "(%d):(%d)", src_crop_h, bufferSize.h);
            return false;
        }
        dst_size_h = bufferSize.h;
    }
    resultDstSize = MSize(dst_size_w, dst_size_h);
    resultSrcRect = MRect(MPoint(src_crop_x, src_crop_y),
                            MSize(src_crop_w, src_crop_h));
    return true;
}
//#endif

#if 1
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class StuffBufferPool {

    #define STUFF_BUFFER_WATER_MARK 8   // "0" the pool will not store buffer
    #define STUFF_BUFFER_MAX_AMOUNT 16  // the max amount for general use case

    enum BUF_STATE
    {
        BUF_STATE_RELEASED  = 0,
        BUF_STATE_ACQUIRED
    };

    struct BufNote {
        public:
                            BufNote()
                                : msName("")
                                , mState(BUF_STATE_RELEASED)
                            {};
                            BufNote(android::String8 name, BUF_STATE state)
                                : msName(name)
                                , mState(state)
                            {};
            virtual         ~BufNote()
                            {};

            android::String8    msName;
            BUF_STATE           mState;
    };

public:
                        StuffBufferPool()
                            : mLogLevel(0)
                            , msName("")
                            , mFormat(0)
                            , mSize(0, 0)
                            , mStride(0)
                            , mUsage(0)
                            , mSerialNum(0)
                            , mWaterMark(STUFF_BUFFER_WATER_MARK)
                            , mMaxAmount(STUFF_BUFFER_MAX_AMOUNT)
                        {
                            MY_LOGD_IF(mLogLevel > 1, "+++");
                            mUsage = (GRALLOC_USAGE_SW_READ_OFTEN |
                                        GRALLOC_USAGE_HW_CAMERA_READ |
                                        GRALLOC_USAGE_HW_CAMERA_WRITE);
                            mvInfoMap.clear();
                            MY_LOGD_IF(mLogLevel > 1, "---");
                        };

                        StuffBufferPool(
                            char const * szName,
                            MINT32 format,
                            MSize size,
                            MUINT32 stride,
                            MUINT8 multiple,    // for burst mode
                            MBOOL writable,     // for SW write
                            MINT32  log
                        )
                            : mLogLevel(log)
                            , msName(szName)
                            , mFormat(format)
                            , mSize(size)
                            , mStride(stride)
                            , mUsage(0)
                            , mSerialNum(0)
                            , mWaterMark(STUFF_BUFFER_WATER_MARK * multiple)
                            , mMaxAmount(STUFF_BUFFER_MAX_AMOUNT * multiple)
                        {
                            //MY_LOGD_IF(mLogLevel > 1, "+++");
                            MY_LOGD_IF(mLogLevel > 0, "[%s]"
                                " 0x%x-%dx%d-%d *%d +%d", szName, format,
                                size.w, size.h, stride, multiple, writable);
                            mUsage = (GRALLOC_USAGE_SW_READ_OFTEN |
                                        GRALLOC_USAGE_HW_CAMERA_READ |
                                        GRALLOC_USAGE_HW_CAMERA_WRITE);
                            if(writable) {
                                mUsage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
                            }
                            mvInfoMap.clear();
                            //MY_LOGD_IF(mLogLevel > 1, "---");
                        };

    virtual             ~StuffBufferPool()
                        {
                            //MY_LOGD_IF(mLogLevel > 1, "+++");
                            while (mvInfoMap.size() > 0) {
                                destroyBuffer(0); // it remove one of mvInfoMap
                            }
                            mvInfoMap.clear();
                            MY_LOGD_IF(mLogLevel > 0, "[%s] 0x%x-%dx%d-%d",
                                msName.string(), mFormat, mSize.w, mSize.h,
                                mStride);
                            //MY_LOGD_IF(mLogLevel > 1, "---");
                        };

    MBOOL               compareLayout(
                            MINT32 format,
                            MSize size,
                            MUINT32 stride
                        )
                        {
                            return ((format == mFormat) &&
                                    (stride == mStride) &&
                                    (size == mSize));
                        };

    MERROR              acquireBuffer(
                            sp<IImageBuffer> & imageBuffer
                        );

    MERROR              releaseBuffer(
                            sp<IImageBuffer> & imageBuffer
                        );

    MERROR              createBuffer(
                            sp<IImageBuffer> & imageBuffer
                        );

    MERROR              destroyBuffer(
                            sp<IImageBuffer> & imageBuffer
                        );

    MERROR              destroyBuffer(
                            size_t index
                        );

private:
    MINT32              mLogLevel;
    android::String8    msName;
    MINT32              mFormat;
    MSize               mSize;
    MUINT32             mStride;
    MUINT               mUsage;
    MUINT32             mSerialNum;
    // it will destroy buffer while releasing, if pool_size > WaterMark
    MUINT32             mWaterMark;
    // it will not create buffer while acquiring, if pool_size >= MaxAmount
    MUINT32             mMaxAmount;
    DefaultKeyedVector< sp<IImageBuffer>, BufNote >
                        mvInfoMap;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferPool::
acquireBuffer(sp<IImageBuffer> & imageBuffer)
{
    FUNCTION_IN;
    //
    MERROR ret = OK;
    sp<IImageBuffer> pImgBuf = NULL;
    BufNote bufNote;
    size_t i = 0;
    imageBuffer = NULL;
    //
    for (i = 0; i < mvInfoMap.size(); i++) {
        bufNote = mvInfoMap.editValueAt(i);
        if (BUF_STATE_RELEASED == bufNote.mState) {
            sp<IImageBuffer> const pImageBuffer = mvInfoMap.keyAt(i);
            bufNote.mState = BUF_STATE_ACQUIRED;
            mvInfoMap.replaceValueAt(i, bufNote);
            pImgBuf = pImageBuffer;
            break;
        }
    }
    //
    if (pImgBuf != NULL) {
        MY_LOGD_IF(mLogLevel > 1, "Acquire Stuff Buffer (%s) index(%zu) (%zu/%d)",
            bufNote.msName.string(), i, mvInfoMap.size(), mWaterMark);
        if (!(pImgBuf->lockBuf(bufNote.msName.string(), mUsage))) {
            MY_LOGE("[%s] Stuff ImgBuf lock fail", bufNote.msName.string());
            return BAD_VALUE;
        }
        imageBuffer = pImgBuf;
        return OK;
    }
    //
    MY_LOGD_IF(mLogLevel > 1, "StuffBuffer-Acquire (NoAvailable) (%zu/%d)",
            mvInfoMap.size(), mWaterMark);
    //
    ret = createBuffer(imageBuffer);
    FUNCTION_OUT;
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferPool::
releaseBuffer(sp<IImageBuffer> & imageBuffer)
{
    FUNCTION_IN;
    //
    MERROR ret = OK;
    if (imageBuffer == NULL) {
        MY_LOGW("Stuff ImageBuffer not exist");
        return BAD_VALUE;
    }
    ssize_t index = mvInfoMap.indexOfKey(imageBuffer);
    if (index < 0 || (size_t)index >= mvInfoMap.size()) {
        MY_LOGW("ImageBuffer(%p) not found (%zd/%zu)",
            imageBuffer.get(), index, mvInfoMap.size());
        return BAD_VALUE;
    }
    imageBuffer->unlockBuf(mvInfoMap.valueAt(index).msName.string());
    BufNote bufNote = mvInfoMap.editValueAt(index);
    bufNote.mState = BUF_STATE_RELEASED;
    mvInfoMap.replaceValueAt(index, bufNote);
    //
    if (mvInfoMap.size() > mWaterMark) {
        ret = destroyBuffer(index);
    }
    //
    MY_LOGD_IF(mLogLevel > 1, "StuffBuffer-Release (%s) index(%zu) (%zu/%d)",
        bufNote.msName.string(), index, mvInfoMap.size(), mWaterMark);
    //
    FUNCTION_OUT;
    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferPool::
createBuffer(sp<IImageBuffer> & imageBuffer)
{
    FUNCTION_IN;
    //
    imageBuffer = NULL;
    // add information to buffer name
    android::String8 imgBufName = android::String8(msName);
    char str[256] = {0};
    snprintf(str, sizeof(str), ":Size%dx%d:Stride%d:Sn%d",
        mSize.w, mSize.h, mStride, ++mSerialNum);
    imgBufName += str;
    //
    if (mvInfoMap.size() >= mMaxAmount) {
        MY_LOGW("[%s] the pool size is over max amount, "
            "please check the buffer usage and situation (%zu/%d)",
            imgBufName.string(), mvInfoMap.size(), mMaxAmount);
        return NO_MEMORY;
    }
    // create buffer
    MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
    MUINT32 bufStridesInBytes[3] = {mStride, 0, 0};
    IImageBufferAllocator::ImgParam imgParam =
        IImageBufferAllocator::ImgParam((EImageFormat)mFormat,
            mSize, bufStridesInBytes, bufBoundaryInBytes, 1);
    sp<IIonImageBufferHeap> pHeap =
        IIonImageBufferHeap::create(imgBufName.string(), imgParam);
    if (pHeap == NULL) {
        MY_LOGE("[%s] Stuff ImageBufferHeap create fail", imgBufName.string());
        return BAD_VALUE;
    }
    sp<IImageBuffer> pImgBuf = pHeap->createImageBuffer();
    if (pImgBuf == NULL) {
        MY_LOGE("[%s] Stuff ImageBuffer create fail", imgBufName.string());
        return BAD_VALUE;
    }
    // lock buffer
    if (!(pImgBuf->lockBuf(imgBufName.string(), mUsage))) {
        MY_LOGE("[%s] Stuff ImageBuffer lock fail", imgBufName.string());
        return BAD_VALUE;
    }
    BufNote bufNote(imgBufName, BUF_STATE_ACQUIRED);
    mvInfoMap.add(pImgBuf, bufNote);
    imageBuffer = pImgBuf;
    //
    MY_LOGD_IF(mLogLevel > 1, "StuffBuffer-Create (%s) (%zu/%d) "
        "ImgBuf(%p)(0x%X)(%dx%d,%zu,%zu)(P:0x%zx)(V:0x%zx)",
        imgBufName.string(), mvInfoMap.size(), mWaterMark,
        imageBuffer.get(), imageBuffer->getImgFormat(),
        imageBuffer->getImgSize().w, imageBuffer->getImgSize().h,
        imageBuffer->getBufStridesInBytes(0), imageBuffer->getBufSizeInBytes(0),
        imageBuffer->getBufPA(0), imageBuffer->getBufVA(0));
    //
    FUNCTION_OUT;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferPool::
destroyBuffer(sp<IImageBuffer> & imageBuffer)
{
    FUNCTION_IN;
    //
    MERROR ret = OK;
    if (imageBuffer == NULL) {
        MY_LOGW("Stuff ImageBuffer not exist");
        return BAD_VALUE;
    }
    //
    ssize_t index = mvInfoMap.indexOfKey(imageBuffer);
    if (index < 0 || (size_t)index >= mvInfoMap.size()) {
        MY_LOGW("ImageBuffer(%p) not found (%zd/%zu)",
            imageBuffer.get(), index, mvInfoMap.size());
        return BAD_VALUE;
    }
    ret = destroyBuffer(index);
    FUNCTION_OUT;
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferPool::
destroyBuffer(size_t index)
{
    FUNCTION_IN;
    //
    if (index >= mvInfoMap.size()) {
        MY_LOGW("index(%zu) not exist, size(%zu)", index, mvInfoMap.size());
        return BAD_VALUE;
    }
    BufNote bufNote = mvInfoMap.valueAt(index);
    sp<IImageBuffer> const pImageBuffer = mvInfoMap.keyAt(index);
    MY_LOGD_IF(mLogLevel > 1, "StuffBuffer-Destroy (%s) index(%zu) state(%d) "
        "(%zu/%d)", bufNote.msName.string(), index, bufNote.mState,
        mvInfoMap.size(), mWaterMark);
    if (bufNote.mState == BUF_STATE_ACQUIRED) {
        sp<IImageBuffer> pImgBuf = pImageBuffer;
        pImgBuf->unlockBuf(bufNote.msName.string());
    }
    // destroy buffer
    mvInfoMap.removeItemsAt(index);
    //pImgBuf = NULL;
    //
    FUNCTION_OUT;
    return OK;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class StuffBufferManager {

public:
                        StuffBufferManager()
                            : mLogLevel(0)
                            , mLock()
                        {
                            MY_LOGD_IF(mLogLevel > 1, "+++");
                            mvPoolSet.clear();
                            MY_LOGD_IF(mLogLevel > 1, "---");
                        };

                        StuffBufferManager(MINT32 log)
                            : mLogLevel(log)
                            , mLock()
                        {
                            MY_LOGD_IF(mLogLevel > 1, "+++");
                            mvPoolSet.clear();
                            MY_LOGD_IF(mLogLevel > 1, "---");
                        };

    virtual             ~StuffBufferManager()
                        {
                            MY_LOGD_IF(mLogLevel > 1, "+++");
                            mvPoolSet.clear();
                            MY_LOGD_IF(mLogLevel > 1, "---");
                        };

    void                setLog(MINT32 log)
                        {
                            MY_LOGD("StuffBufferManager log(%d)", log);
                            Mutex::Autolock _l(mLock);
                            mLogLevel = log;
                        }

    MERROR              acquireStoreBuffer(
                            sp<IImageBuffer> & imageBuffer,
                            char const * szName,
                            MINT32 format,
                            MSize size,
                            MUINT32 stride,
                            MUINT8 multiple = 1,    // for burst mode
                            MBOOL writable = MFALSE // for SW write
                        );

    MERROR              releaseStoreBuffer(
                            sp<IImageBuffer> & imageBuffer
                        );

private:
    MINT32              mLogLevel;
    mutable Mutex       mLock;
    Vector<StuffBufferPool>
                        mvPoolSet;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferManager::
acquireStoreBuffer(sp<IImageBuffer> & imageBuffer, char const* szName,
    MINT32 format, MSize size, MUINT32 stride, MUINT8 multiple, MBOOL writable)
{
    FUNCTION_IN;
    //
    //CAM_TRACE_FMT_BEGIN("Stuff(%dx%d)%s", size.w, size.h, szName);
    //
    Mutex::Autolock _l(mLock);
    MERROR ret = OK;
    //
    StuffBufferPool* bufPool = NULL;
    imageBuffer = NULL;
    //
    Vector<StuffBufferPool>::iterator it = mvPoolSet.begin();
    for(; it != mvPoolSet.end(); it++) {
        if (it->compareLayout(format, size, stride)) {
            bufPool = it;
            break;
        }
    }
    //
    if (bufPool == NULL) {
        StuffBufferPool newPool(szName, format, size, stride,
            multiple, writable, mLogLevel);
        mvPoolSet.push_back(newPool);
        it = mvPoolSet.end();
        bufPool = (it - 1);
        MY_LOGD_IF(mLogLevel > 0, "PoolSet.size(%zu)", mvPoolSet.size());
    }
    //
    if (bufPool == NULL) {
        MY_LOGE("Cannot create stuff buffer pool");
        return BAD_VALUE;
    } else {
        ret = bufPool->acquireBuffer(imageBuffer);
    }
    //
    //CAM_TRACE_FMT_END();
    //
    FUNCTION_OUT;
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StuffBufferManager::
releaseStoreBuffer(sp<IImageBuffer> & imageBuffer)
{
    FUNCTION_IN;
    //
    Mutex::Autolock _l(mLock);
    //
    if (imageBuffer == NULL) {
        MY_LOGW("Stuff ImageBuffer not exist");
        return BAD_VALUE;
    }
    //
    MINT const format = imageBuffer->getImgFormat();
    MSize const size = imageBuffer->getImgSize();
    MUINT32 const stride = imageBuffer->getBufStridesInBytes(0);
    //
    StuffBufferPool* bufPool = NULL;
    Vector<StuffBufferPool>::iterator it = mvPoolSet.begin();
    for(; it != mvPoolSet.end(); it++) {
        if (it->compareLayout(format, size, stride)) {
            bufPool = it;
            break;
        }
    }
    //
    if (bufPool == NULL) {
        MY_LOGE("Cannot find stuff buffer pool");
        return BAD_VALUE;
    } else {
        bufPool->releaseBuffer(imageBuffer);
    }
    //
    FUNCTION_OUT;
    return OK;
}
#endif

class LongExposureStatus
{
    #define P1_LONG_EXP_TIME_TH (500 * 1000000) //(1ms = 1000000ns)
    public:
                        LongExposureStatus()
                            : mLock()
                            , mThreshold(P1_LONG_EXP_TIME_TH)
                            , mRunning(MFALSE)
                        {
                            //MY_LOGD("+++");

                            #if 0 // for Long Expousre IT
                            #warning "[FIXME] force to change P1 LE threshold"
                            {
                                MUINT32 thd_ms =
                                    ::property_get_int32("debug.camera.p1exp",
                                    0);
                                if (thd_ms > 0) {
                                    mThreshold = thd_ms * 1000000;
                                }
                                MY_LOGI("debug.camera.p1exp = %d - "
                                    "Threshold = %lld", thd_ms, mThreshold);
                            }
                            #endif

                            //MY_LOGD("---");
                        };

                        ~LongExposureStatus()
                        {
                            //MY_LOGD("+++");

                            //MY_LOGD("---");
                        };

        MBOOL           reset(MINT num)
                        {
                            Mutex::Autolock _l(mLock);
                            if (mvSet.empty()) {
                                return MFALSE;
                            }
                            Vector<MINT32>::iterator it = mvSet.begin();
                            for (; it != mvSet.end(); it++) {
                                if (num == *it) {
                                    mvSet.erase(it);
                                    break;
                                }
                            }
                            if (mvSet.empty()) {
                                mRunning = MFALSE;
                            }
                            MY_LOGI("(%d/%zu) LongExposure[%d]",
                                    num, mvSet.size(), mRunning);
                            return MTRUE;
                        };

        MBOOL           set(MINT num, MINT64 exp_ns)
                        {
                            Mutex::Autolock _l(mLock);
                            if (exp_ns >= mThreshold && num > 0) {
                                MBOOL isFound = MFALSE;
                                Vector<MINT32>::iterator it = mvSet.begin();
                                for (; it != mvSet.end(); it++) {
                                    if (num == *it) {
                                        isFound = MTRUE;
                                        break;
                                    }
                                }
                                if (!isFound) {
                                    mvSet.push_back(num);
                                    mRunning = MTRUE;
                                }
                                MY_LOGI("(%d/%zu) LongExposure[%d]",
                                        num, mvSet.size(), mRunning);
                                return MTRUE;
                            }
                            return MFALSE;
                        };

        MBOOL           get(void)
                        {
                            Mutex::Autolock _l(mLock);
                            MBOOL isRunning = mRunning;
                            #if (!SUPPORT_LONG_EXPOSURE_ABORT)
                            isRunning = MFALSE;
                            #endif
                            return isRunning;
                        };

    private:
        mutable Mutex   mLock;
        MINT64          mThreshold;
        MBOOL           mRunning;
        Vector<MINT32>  mvSet;
};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  .
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class P1NodeImp
    : public BaseNode
    , public P1Node
    , public IHal3ACb
    , protected Thread
{

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////
    //
    #define STREAM_ITEM_START       (0)
    //
    enum STREAM_IMG
    {
        STREAM_IMG_IN_YUV           = STREAM_ITEM_START,
        STREAM_IMG_IN_OPAQUE,
        STREAM_IMG_OUT_OPAQUE,
        STREAM_IMG_OUT_FULL,
        STREAM_IMG_OUT_RESIZE,
        STREAM_IMG_OUT_LCS,
        STREAM_IMG_NUM
    };
    #define IS_IN_STREAM_IMG(img)   (  (img == STREAM_IMG_IN_YUV)\
                                    || (img == STREAM_IMG_IN_OPAQUE)\
                                    )
    //
    enum STREAM_META
    {
        STREAM_META_IN_APP          = STREAM_ITEM_START,
        STREAM_META_IN_HAL,
        STREAM_META_OUT_APP,
        STREAM_META_OUT_HAL,
        STREAM_META_NUM
    };
    #define IS_IN_STREAM_META(meta) (  (meta == STREAM_META_IN_APP)\
                                    || (meta == STREAM_META_IN_HAL)\
                                    )
    //
    enum STREAM_BUF_LOCK
    {
        STREAM_BUF_LOCK_NONE        = 0,
        STREAM_BUF_LOCK_R,
        STREAM_BUF_LOCK_W
    };
    //
    enum IMG_BUF_SRC
    {
        IMG_BUF_SRC_NULL           = 0,
        IMG_BUF_SRC_POOL,
        IMG_BUF_SRC_STUFF,
        IMG_BUF_SRC_FRAME
    };

    struct NodeStreamMeta {
        MBOOL                           bExist;
        sp<IMetaStreamBuffer>           spStreamBuf;
        STREAM_BUF_LOCK                 eLockState;
        IMetadata*                      pMetadata;
        NodeStreamMeta()
            : bExist(MFALSE)
            , spStreamBuf(NULL)
            , eLockState(STREAM_BUF_LOCK_NONE)
            , pMetadata(NULL)
        {};
    };

    struct NodeStreamImage {
        MBOOL                           bExist;
        sp<IImageStreamBuffer>          spStreamBuf;
        STREAM_BUF_LOCK                 eLockState;
        //sp<IImageBufferHeap>            spHeap;
        sp<IImageBuffer>                spImgBuf;
        IMG_BUF_SRC                     eSrcType;
        NodeStreamImage()
            : bExist(MFALSE)
            , spStreamBuf(NULL)
            , eLockState(STREAM_BUF_LOCK_NONE)
            //, spHeap(NULL)
            , spImgBuf(NULL)
            , eSrcType(IMG_BUF_SRC_NULL)
        {};
    };

    struct QueNode_T {
        MUINT32                             magicNum;
        MUINT32                             sofIdx;
        sp<IPipelineFrame>                  appFrame;
        sp<IImageBuffer>                    buffer_eiso;
        sp<IImageBuffer>                    buffer_lcso; // no use, need remove when lcso run imageStream
        NodeStreamImage                     streamBufImg[STREAM_IMG_NUM];
        NodeStreamMeta                      streamBufMeta[STREAM_META_NUM];
        MUINT32                             reqType;    /*REQ_TYPE*/
        MUINT32                             reqOutSet;  /*REQ_SET(REQ_OUT)*/
        MUINT32                             expRec;     /*EXP_REC(EXP_EVT)*/
        MUINT32                             exeState;   /*EXE_STATE*/
        MUINT32                             capType;    /*E_CAPTURE_TYPE*/
        MINT64                              frameExpDuration;
        MINT64                              frameTimeStamp;
        MBOOL                               needFlush;
        MSize                               dstSize_full;
        MSize                               dstSize_resizer;
        MRect                               cropRect_full;
        MRect                               cropRect_resizer;
        QueNode_T()
            : magicNum(0)
            , sofIdx(P1SOFIDX_INIT_VAL)
            , appFrame(NULL)
            , buffer_eiso(NULL)
            , buffer_lcso(NULL)
            , reqType(REQ_TYPE_UNKNOWN)
            , reqOutSet(REQ_SET_NONE)
            , expRec(EXP_REC_NONE)
            , exeState(EXE_STATE_NULL)
            , capType(E_CAPTURE_NORMAL)
            , frameExpDuration(0)
            , frameTimeStamp(0)
            , needFlush(MFALSE)
        {}
    };

    struct QueJob_T {
    public:
        Vector<QueNode_T>           mSet;
        MUINT8                      mMaxNum;
        MUINT32                     mFirstMagicNum;
                                    QueJob_T()
                                    : mMaxNum(1)
                                    , mFirstMagicNum(0)
                                    {
                                        mSet.clear();
                                        mSet.setCapacity(mMaxNum);
                                    };
                                    QueJob_T(MUINT8 num)
                                    : mMaxNum(num)
                                    , mFirstMagicNum(0)
                                    {
                                        mSet.clear();
                                        mSet.setCapacity(mMaxNum);
                                    };
        virtual                     ~QueJob_T()
                                    {
                                        mSet.clear();
                                    };
    };

    typedef Vector<QueJob_T>        Que_T;
    //
    class DeliverMgr
        : public Thread
    {

    public:

        DeliverMgr()
            : mpP1NodeImp(NULL)
            , mLogLevel(0)
            , mLoopRunning(MFALSE)
            , mLoopState(LOOP_STATE_INIT)
            , mSentNum(0)
        {
            mNumList.clear();
            mNodeQueue.clear();
        };

        ~DeliverMgr()
        {
            mNumList.clear();
            mNodeQueue.clear();
        };

        void init(sp<P1NodeImp> pP1NodeImp) {
            mpP1NodeImp = pP1NodeImp;
            if (mpP1NodeImp != NULL) {
                mLogLevel = mpP1NodeImp->mLogLevel;
                mNodeQueue.setCapacity((size_t)(mpP1NodeImp->mBurstNum >> 3));
            }
        };

        void uninit(void) {
            exit();
            if (mpP1NodeImp != NULL) {
                mpP1NodeImp = NULL;
            }
        };

        void runningSet(MBOOL bRunning) {
            Mutex::Autolock _l(mDeliverLock);
            mLoopRunning = bRunning;
            return;
        };

        MBOOL runningGet(void) {
            Mutex::Autolock _l(mDeliverLock);
            return mLoopRunning;
        };

        void exit(void) {
            MY_LOGD("DeliverMgr loop exit");
            Thread::requestExit();
            trigger();
            MY_LOGD("DeliverMgr loop join");
            Thread::join();
            MY_LOGD("DeliverMgr loop finish");
        };

    public:

        MBOOL registerNodeList(MINT32 num);

        MBOOL sendNodeQueue(QueNode_T node, MBOOL needTrigger);

        MBOOL waitFlush(MBOOL needTrigger);

        MBOOL trigger(void);

    private:

        void dumpNumList(MBOOL isLock = MFALSE);

        void dumpNodeQueue(MBOOL isLock = MFALSE);

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Thread Interface.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    public:
        virtual status_t    readyToRun()
        {
            MY_LOGD("readyToRun DeliverMgr thread");

            // set name
            ::prctl(PR_SET_NAME, (unsigned long)"Cam@P1NodeMgr", 0, 0, 0);
            // set normal
            struct sched_param sched_p;
            sched_p.sched_priority = 0;
            ::sched_setscheduler(0, P1THREAD_POLICY, &sched_p);
            ::setpriority(PRIO_PROCESS, 0, P1THREAD_PRIORITY);
            //  Note: "priority" is nice value.
            //
            ::sched_getparam(0, &sched_p);
            MY_LOGD(
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
            if (exitPending()) {
                MY_LOGD("DeliverMgr try to leave");

                Mutex::Autolock _l(mDeliverLock);
                if (mNodeQueue.size() > 0) {
                    MY_LOGI("the deliver queue is not empty, go-on the loop");
                } else {
                    MY_LOGI("DeliverMgr Leaving");
                    return MFALSE;
                }
            }

            return deliverLoop();
        };

        enum LOOP_STATE
        {
            LOOP_STATE_INIT         = 0,
            LOOP_STATE_WAITING,
            LOOP_STATE_PROCESSING,
            LOOP_STATE_DONE
        };

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Data Member.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    private:
        sp<P1NodeImp>       mpP1NodeImp;
        MINT32              mLogLevel;
        MBOOL               mLoopRunning;
        LOOP_STATE          mLoopState;
        Condition           mDoneCond;
        Condition           mDeliverCond;
        mutable Mutex       mDeliverLock;
        MINT32              mSentNum;
        List<MINT32>        mNumList;
        Vector<QueNode_T>   mNodeQueue;

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Function Member.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    private:
        MBOOL deliverLoop();


    };
    //
    class Tag {
        public:
                                    Tag()
                                    : mInfo(0)
                                    , mLock()
                                    {
                                    };
                                    ~Tag()
                                    {
                                    };
            void                    clear(void)
                                    {
                                        RWLock::AutoWLock _wl(mLock);
                                        mInfo = 0;
                                    };
            void                    set(MUINT32 info)
                                    {
                                        //RWLock::AutoWLock _wl(mLock);
                                        mInfo = info;
                                    };
            MUINT32                 get(void)
                                    {
                                        //RWLock::AutoRLock _rl(mLock);
                                        return mInfo;
                                    }
        private:
            MUINT32                 mInfo;
            mutable RWLock          mLock;
    };
    //
    enum REQ_STATE
    {
        REQ_STATE_WAITING           = 0,
        REQ_STATE_ONGOING,
        REQ_STATE_RECEIVE,
        REQ_STATE_CREATED
    };
    //
    enum START_CAP_STATE
    {
        START_CAP_STATE_NONE        = 0,
        START_CAP_STATE_WAIT_REQ,
        START_CAP_STATE_WAIT_CB,
        START_CAP_STATE_READY
    };
    //
protected:  ////                    Data Members. (Config)
    mutable RWLock                  mConfigRWLock;
    mutable Mutex                   mInitLock;
    MBOOL                           mInit;

    SortedVector<StreamId_T>        mInStreamIds;
    sp<IMetaStreamInfo>             mvStreamMeta[STREAM_META_NUM];
    sp<IImageStreamInfo>            mvStreamImg[STREAM_IMG_NUM];
    SensorParams                    mSensorParams;
    //RawParams                       mRawParams;
    sp<IImageStreamBufferPoolT>     mpStreamPool_full;
    sp<IImageStreamBufferPoolT>     mpStreamPool_resizer;
    sp<IImageStreamBufferPoolT>     mpStreamPool_lcso;
    MUINT8                          mBurstNum;
    QueJob_T                        mQueueJob;

    /**
     * the raw default type, if the request do not set the raw type,
     * it will use this setting to driver
     */
    MUINT32                         mRawDefType;

    /**
     * the raw type option means the capability recorded in each bit,
     * it is decided after the driver configuration
     */
    MUINT32                         mRawOption;
    MBOOL                           mDisableFrontalBinning;

    MBOOL                           mEnableEISO;
    MBOOL                           mEnableLCSO;
    MBOOL                           mEnableDualPD;

    MINT                            mRawFormat;
    MUINT32                         mRawStride;
    MUINT32                         mRawLength;

    REV_MODE                        mReceiveMode;
    MUINT                           mSensorFormatOrder;

    LongExposureStatus              mLongExp;
    //Storage                         mImageStorage;

protected:  ////                    Data Members. (System capability)
    static const int                mNumInMeta = 2;
    static const int                mNumOutMeta = 3;
    int                             m3AProcessedDepth;
    int                             mNumHardwareBuffer;
    int                             mDelayframe;

protected:  ////
    MUINT32                         mLastNum;
    mutable Mutex                   mLastNumLock;
    MUINT32                         mLastSofIdx;
    MINT32                          mLastSetNum;

protected:  ////                    Data Members. (Hardware)
    mutable Mutex                   mHardwareLock;
    mutable Mutex                   mActiveLock;
    MBOOL                           mActive;
    mutable Mutex                   mReadyLock;
    MBOOL                           mReady;
    INormalPipe*                    mpCamIO;
    IHal3A_T*                       mp3A;
    #if SUPPORT_LMV
    LMVHal*                         mpLMV;
    EisHal*                         mpEIS;
    MINT32                          mEisMode;
    #endif
    #if SUPPORT_VHDR
    VHdrHal*                        mpVhdr;
    #endif
    #if SUPPORT_LCS
    LcsHal*                         mpLCS;
    #endif
    //
    MRect                           mActiveArray;
    MUINT32                         mPixelMode;
    //
    MUINT32                         mConfigPort;
    MUINT32                         mConfigPortNum;
    MBOOL                           mIsBinEn;
    //
    DefaultKeyedVector< sp<IImageBuffer>, android::String8 >
                                    mvStuffBufferInfo;
    mutable Mutex                   mStuffBufferLock;
    //
    StuffBufferManager              mStuffBufMgr;
    //
    #define DRAWLINE_PORT_RRZO      0x1
    #define DRAWLINE_PORT_IMGO      0x2
    MUINT32                         mDebugScanLineMask;
    DebugScanLine*                  mpDebugScanLine;

protected:  ////                    Data Members. (Queue: Request)
    mutable Mutex                   mRequestQueueLock;
    Condition                       mRequestQueueCond;
    Que_T                           mRequestQueue;

protected:  ////                    Data Members. (Queue: 3Alist)
    mutable Mutex                   mControls3AListLock;
    List<MetaSet_T>                 mControls3AList;
    Condition                       mControls3AListCond;
    //
    mutable Mutex                   mFrameSetLock;
    MBOOL                           mFrameSetAlready;
    //
    REQ_STATE                       mReqState;
    MBOOL                           mFirstReceived;
    //
    mutable Mutex                   mStartCaptureLock;
    Condition                       mStartCaptureCond;
    START_CAP_STATE                 mStartCaptureState;
    MUINT32                         mStartCaptureType;
    MINT64                          mStartCaptureExp;
    MBOOL                           mStartCaptureInitEnque;

protected:  ////
    //AAAResult                       m3AStorage;

protected:  ////                    Data Members. (Queue: Processing)
    mutable Mutex                   mProcessingQueueLock;
    Condition                       mProcessingQueueCond;
    Que_T                           mProcessingQueue;

protected:  ////                    Data Members. (Queue: drop)
    mutable Mutex                   mDropQueueLock;
    Vector<MUINT>                   mDropQueue;

protected:  ////                    Data Members.
    mutable Mutex                   mThreadLock;
    Condition                       mThreadCond;

protected:  ////                    Data Members.
    DurationProfile                 mDequeThreadProfile;

protected:  ////                    Data Members.
    mutable Mutex                   mPublicLock;

protected:  ////                    Data Members.
    MINT32                          mInFlightRequestCnt;

protected:  ////                    Data Members.
    sp<DeliverMgr>                  mpDeliverMgr;

protected:
    MINT32                          mLogLevel;
    MINT32                          mLogLevelI;
    MINT32                          mEnableDumpRaw;
    Tag                             mTagReq;
    Tag                             mTagSet;
    Tag                             mTagEnq;
    Tag                             mTagDeq;
    Tag                             mTagOut;
    Tag                             mTagList;

protected:  ////                    Data Members.
    MBOOL                           mEnableCaptureFlow;
    MBOOL                           mEnableFrameSync;

protected:  ////                    Operations.

    MVOID                           setActive(
                                        MBOOL active
                                    );

    MBOOL                           getActive(
                                        void
                                    );

    MVOID                           setReady(
                                        MBOOL ready
                                    );

    MBOOL                           getReady(
                                        void
                                    );

    MVOID                           setInit(
                                        MBOOL init
                                    );

    MBOOL                           getInit(
                                        void
                                    );

    MVOID                           onRequestFrameSet(
                                        MBOOL initial = MFALSE
                                    );


    MVOID                           onSyncEnd(
                                        void
                                    );

    MVOID                           onSyncBegin(
                                        MBOOL initial,
                                        RequestSet_T* reqSet = NULL,//MUINT32 magicNum = 0,
                                        MUINT32 sofIdx = P1SOFIDX_INIT_VAL,
                                        CapParam_T* capParam = NULL
                                    );

#if 0
    MVOID                           onProcess3AResult(
                                        MUINT32 magicNum,
                                        MUINT32 key,
                                        MUINT32 val
                                    );
#endif

    MVOID                           onProcessLMV(
                                        QueNode_T const &node,
                                        IMetadata &resultLMV,
                                        QBufInfo const &deqBuf,
                                        MINT32 eisMode,
                                        MUINT8 captureIntent,
                                        MINT64 exposureTime,
                                        MUINT32 const index = 0
                                    );

    MERROR                          onProcessEnqueFrame(
                                        QueJob_T &job
                                    );

    MERROR                          onProcessDequeFrame(
                                    );

    MERROR                          onProcessDropFrame(
                                        MBOOL isTrigger = MFALSE
                                    );

    MBOOL                           getProcessingFrame_ByAddr(
                                        IImageBuffer* const imgBuffer,
                                        MUINT32 magicNum,
                                        QueJob_T &job
                                    );

    QueJob_T                        getProcessingFrame_ByNumber(
                                        MUINT32 magicNum
                                    );


    MVOID                           onHandleFlush(
                                        MBOOL wait
                                    );

    MVOID                           processRedoFrame(
                                        QueNode_T & node
                                    );

    MVOID                           processYuvFrame(
                                        QueNode_T & node
                                    );

    MVOID                           onReturnFrame(
                                        QueNode_T & node,
                                        MBOOL isFlush,
                                        MBOOL isTrigger = MTRUE
                                    );

    MVOID                           releaseNode(
                                        QueNode_T & node
                                    );

    MVOID                           onProcessResult(
                                        QueNode_T & node,
                                        QBufInfo const &deqBuf,
                                        MetaSet_T const &result3A,
                                        IMetadata const &resultLMV,
                                        MUINT32 const index = 0
                                    );

    MERROR                          findRequestStream(
                                        QueNode_T & node
                                    );

    MVOID                           createNode(sp<IPipelineFrame> appframe,
                                               QueJob_T *job,
                                               Que_T *Queue,
                                               Mutex *QueLock,
                                               List<MetaSet_T> *list,
                                               Mutex *listLock
                                    );

    MVOID                           createNode(List<MetaSet_T> *list,
                                               Mutex *listLock
                                    );

    MVOID                           createNode(Que_T &Queue);

protected:  ////                    Hardware Operations.
    MERROR                          hardwareOps_start(
                                    );

    MERROR                          hardwareOps_enque(
                                        QueJob_T &job,
                                        MBOOL toPush = false
                                    );

    MERROR                          hardwareOps_deque(
                                        QBufInfo &deqBuf
                                    );

    MERROR                          hardwareOps_stop(
                                    );

    MERROR                          hardwareOps_capture(
                                    );

    MERROR                          setupNode(
                                        QueNode_T & node,
                                        QBufInfo & info
                                    );

    MERROR                          createStuffBuffer(
                                        sp<IImageBuffer> & imageBuffer,
                                        sp<IImageStreamInfo> const& streamInfo,
                                        NSCam::MSize::value_type const
                                            changeHeight = 0
                                    );

    MERROR                          createStuffBuffer(
                                        sp<IImageBuffer> & imageBuffer,
                                        char const * szName,
                                        MINT32 format,
                                        MSize size,
                                        MUINT32 stride
                                    );

    MERROR                          destroyStuffBuffer(
                                        sp<IImageBuffer> & imageBuffer
                                    );

    MVOID                           generateAppMeta(
                                        QueNode_T & node,
                                        MetaSet_T const &result3A,
                                        QBufInfo const &deqBuf,
                                        IMetadata &appMetadata,
                                        MUINT32 const index = 0
                                    );

    MVOID                           generateAppTagIndex(
                                        IMetadata &appMetadata,
                                        IMetadata &appTagIndex
                                    );

    MVOID                           generateHalMeta(
                                        MetaSet_T const &result3A,
                                        QBufInfo const &deqBuf,
                                        IMetadata const &resultLMV,
                                        IMetadata const &inHalMetadata,
                                        IMetadata &halMetadata,
                                        MUINT32 const index = 0
                                    );

    MVOID                           prepareCropInfo(
                                       IMetadata* pAppMetadata,
                                       IMetadata* pHalMetadata,
                                       QueNode_T& node
                                    );

    MERROR                          check_config(
                                        ConfigParams const& rParams
                                    );

    MERROR                          requestMetadataEarlyCallback(
                                        QueNode_T const & node,
                                        STREAM_META const streamMeta,
                                        IMetadata * pMetadata
                                    );

    MERROR                          frameMetadataInit(
                                        QueNode_T & node,
                                        STREAM_META const streamMeta,
                                        sp<IMetaStreamBuffer> &
                                        pMetaStreamBuffer
                                    );

    MERROR                          frameMetadataGet(
                                        QueNode_T & node,
                                        STREAM_META const streamMeta,
                                        IMetadata * pOutMetadata,
                                        MBOOL toWrite = MFALSE,
                                        IMetadata * pInMetadata = NULL
                                    );

    MERROR                          frameMetadataPut(
                                        QueNode_T & node,
                                        STREAM_META const streamMeta
                                    );

    MERROR                          frameImageInit(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg,
                                        sp<IImageStreamBuffer> &
                                        pImageStreamBuffer
                                    );

    MERROR                          frameImageGet(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg,
                                        sp<IImageBuffer> &rImgBuf
                                    );

    MERROR                          frameImagePut(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg
                                    );

    MERROR                          poolImageGet(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg,
                                        sp<IImageBuffer> &rImgBuf
                                    );

    MERROR                          poolImagePut(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg
                                    );

    MERROR                          stuffImageGet(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg,
                                        MSize const dstSize,
                                        sp<IImageBuffer> &rImgBuf
                                    );

    MERROR                          stuffImagePut(
                                        QueNode_T & node,
                                        STREAM_IMG const streamImg
                                    );

    MUINT32                         get_and_increase_magicnum()
                                    {
                                        Mutex::Autolock _l(mLastNumLock);
                                        MUINT32 ret = mLastNum++;
                                        //skip num = 0 as 3A would callback 0 when request stack is empty
                                        //skip -1U as a reserved number to indicate that which would never happen in 3A queue
                                        if(ret==0 || ret==-1U) ret=mLastNum=1;
                                        return ret;
                                    }

    MBOOL                           isRevMode(REV_MODE mode)
                                    {
                                        return (mode == mReceiveMode) ?
                                            (MTRUE) : (MFALSE);
                                    };

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Operations in base class Thread
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////
    // Ask this object's thread to exit. This function is asynchronous, when the
    // function returns the thread might still be running. Of course, this
    // function can be called from a different thread.
    virtual void                    requestExit();

    // Good place to do one-time initializations
    virtual status_t                readyToRun();

private:
    // Derived class must implement threadLoop(). The thread starts its life
    // here. There are two ways of using the Thread object:
    // 1) loop: if threadLoop() returns true, it will be called again if
    //          requestExit() wasn't called.
    // 2) once: if threadLoop() returns false, the thread will exit upon return.
    virtual bool                    threadLoop();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.
                                    P1NodeImp();
    virtual                        ~P1NodeImp();
    virtual MERROR                  config(ConfigParams const& rParams);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipelineNode Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Operations.

    virtual MERROR                  init(InitParams const& rParams);

    virtual MERROR                  uninit();

    virtual MERROR                  flush();

    virtual MERROR                  flush(
                                        android::sp<IPipelineFrame> const &pFrame
                                    );

    virtual MERROR                  queue(
                                        sp<IPipelineFrame> pFrame
                                    );

public:     ////                    Operations.

    virtual void                    doNotifyCb (
                                        MINT32  _msgType,
                                        MINTPTR _ext1,
                                        MINTPTR _ext2,
                                        MINTPTR _ext3
                                    );

    static void                     doNotifyDropframe(MUINT magicNum, void* cookie);
};
};  //namespace


/******************************************************************************
 *
 ******************************************************************************/
P1NodeImp::
P1NodeImp()
    : BaseNode()
    , P1Node()
    , mConfigRWLock()
    , mInitLock()
    , mInit(MTRUE)
    //
    , mpStreamPool_full(NULL)
    , mpStreamPool_resizer(NULL)
    , mpStreamPool_lcso(NULL)
    , mBurstNum(1)
    , mQueueJob(1)
    , mRawDefType(EPipe_PURE_RAW)
    , mRawOption(0)
    , mDisableFrontalBinning(MFALSE)
    , mEnableEISO(MFALSE)
    , mEnableLCSO(MFALSE)
    , mEnableDualPD(MFALSE)
    //
    , mRawFormat(P1_IMGO_DEF_FMT)
    , mRawStride(0)
    , mRawLength(0)
    //
    , mReceiveMode(REV_MODE_NORMAL)
    , mSensorFormatOrder(SENSOR_FORMAT_ORDER_NONE)
    //
    //, mImageStorage()
    //
    , m3AProcessedDepth(3)
    , mNumHardwareBuffer(3)
    , mDelayframe(3)
    , mLastNum(1)
    , mLastNumLock()
    , mLastSofIdx(P1SOFIDX_NULL_VAL)
    , mLastSetNum(0)
    , mHardwareLock()
    , mActiveLock()
    , mActive(MFALSE)
    , mReadyLock()
    , mReady(MFALSE)
    , mpCamIO(NULL)
    , mp3A(NULL)
    #if SUPPORT_LMV
    , mpLMV(NULL)
    , mpEIS(NULL)
    , mEisMode(0)
    #endif
    #if SUPPORT_VHDR
    , mpVhdr(NULL)
    #endif
    #if SUPPORT_LCS
    , mpLCS(NULL)
    #endif
    //
    , mPixelMode(0)
    //
    , mConfigPort(CONFIG_PORT_NONE)
    , mConfigPortNum(0)
    , mIsBinEn(false)
    //
    , mStuffBufMgr()
    //
    , mDebugScanLineMask(0)
    , mpDebugScanLine(NULL)
    //
    , mRequestQueueLock()
    , mRequestQueueCond()
    , mRequestQueue()
    //
    , mControls3AListLock()
    , mControls3AList()
    , mControls3AListCond()
    //
    , mFrameSetLock()
    , mFrameSetAlready(MFALSE)
    //
    , mReqState(REQ_STATE_CREATED)
    , mFirstReceived(MFALSE)
    //
    , mStartCaptureLock()
    , mStartCaptureCond()
    , mStartCaptureState(START_CAP_STATE_NONE)
    , mStartCaptureType(E_CAPTURE_NORMAL)
    , mStartCaptureExp(0)
    , mStartCaptureInitEnque(MFALSE)
    //
    //, m3AStorage()
    //
    , mProcessingQueueLock()
    , mProcessingQueueCond()
    , mProcessingQueue()
    //
    , mDropQueueLock()
    , mDropQueue()
    //
    , mThreadLock()
    , mThreadCond()
    //
    , mDequeThreadProfile("P1Node::deque", 15000000LL)
    , mInFlightRequestCnt(0)
    //
    , mpDeliverMgr(NULL)
    //
    , mLogLevel(0)
    , mLogLevelI(0)
    , mEnableDumpRaw(0)
    , mTagReq()
    , mTagSet()
    , mTagEnq()
    , mTagDeq()
    , mTagOut()
    , mTagList()
    //
    , mEnableCaptureFlow(MFALSE)
    , mEnableFrameSync(MFALSE)
{
    MINT32 cam_log = ::property_get_int32("debug.camera.log", 0);
    MINT32 p1_log = ::property_get_int32("debug.camera.log.p1node", 1);
    mLogLevel = MAX(cam_log, p1_log);
#if 0 // force to enable all p1 node log
    #warning "[FIXME] force to enable P1Node log"
    if (mLogLevel < 2) {
        mLogLevel = 2;
    }
#endif
    MBOOL buildLogD = MFALSE;
    MBOOL buildLogI = MFALSE;
#if (IS_P1_LOGI)
    //#warning "IS_P1_LOGI build LogI"
    mLogLevelI = (mLogLevel > 0) ? (mLogLevel - 1) : (mLogLevel);
    buildLogI = MTRUE;
#endif
#if (IS_P1_LOGD)
    //#warning "IS_P1_LOGD build LogD"
    mLogLevelI = mLogLevel;
    buildLogD = MTRUE;
#endif
    MINT32 p1_logi = ::property_get_int32("debug.camera.log.p1nodei", 0);
    if (p1_logi > 0) {
        mLogLevelI = p1_logi;
    }
    mStuffBufMgr.setLog(mLogLevel);
    //
    mEnableDumpRaw = property_get_int32("debug.feature.forceEnableIMGO", 0);
    //
    mDebugScanLineMask = ::property_get_int32("debug.camera.scanline.p1", 0);
    if ( mDebugScanLineMask != 0)
    {
        mpDebugScanLine = DebugScanLine::createInstance();
    }
    //
    MY_LOGI("LOGD[%d](%d) LOGI[%d](%d) prop(%d,%d,%d) Dp(%d) Db(%d)",
        buildLogD, mLogLevel, buildLogI, mLogLevelI,
        cam_log, p1_log, p1_logi, mEnableDumpRaw, mDebugScanLineMask);
}


/******************************************************************************
 *
 ******************************************************************************/
P1NodeImp::
~P1NodeImp()
{
    MY_LOGD("");
    if( mpDebugScanLine != NULL )
    {
        mpDebugScanLine->destroyInstance();
        mpDebugScanLine = NULL;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
init(InitParams const& rParams)
{
    FUNCTION_S_IN;

    {
        RWLock::AutoWLock _l(mConfigRWLock);
        //
        mOpenId  = rParams.openId;
        mNodeId  = rParams.nodeId;
        mNodeName= rParams.nodeName;
    }

    MERROR err = run("P1NodeImp::init");

    mpDeliverMgr = new DeliverMgr();
    if (mpDeliverMgr != NULL) {
        mpDeliverMgr->init(this);
    } else {
        MY_LOGE("DeliverMgr create fail");
        return NO_MEMORY;
    }

    FUNCTION_S_OUT;

    return err;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
uninit()
{
    PUBLIC_APIS_IN;

    // flush the left frames if exist
    onHandleFlush(MFALSE);

    requestExit();

    //mvStreamMeta.clear();
    for (int stream = STREAM_ITEM_START; stream < STREAM_META_NUM; stream++) {
        mvStreamMeta[stream] = NULL;
    }

    //mvStreamImg.clear();
    for (int stream = STREAM_ITEM_START; stream < STREAM_IMG_NUM; stream++) {
        mvStreamImg[stream] = NULL;
    }

    if (mpDeliverMgr != NULL) {
        mpDeliverMgr->uninit();
        mpDeliverMgr = NULL;
    }

    PUBLIC_APIS_OUT;

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
check_config(ConfigParams const& rParams)
{
    CAM_TRACE_NAME("P1:check_config");

    if (rParams.pInAppMeta == NULL) {
        MY_LOGE("in app metadata is null");
        return BAD_VALUE;
    }

    if (rParams.pInHalMeta == NULL) {
        MY_LOGE("in hal metadata is null");
        return BAD_VALUE;
    }

    if (rParams.pOutAppMeta == NULL) {
        MY_LOGE("out app metadata is null");
        return BAD_VALUE;
    }

    if (rParams.pOutHalMeta == NULL) {
        MY_LOGE("out hal metadata is null");
        return BAD_VALUE;
    }

    if (rParams.pvOutImage_full.size() == 0 &&
        rParams.pOutImage_resizer == NULL) {
        MY_LOGE("image is empty");
        return BAD_VALUE;
    }

    if (rParams.pStreamPool_full != NULL &&
        rParams.pvOutImage_full.size() == 0) {
        MY_LOGE("wrong full input");
        return BAD_VALUE;
    }

    if (rParams.pStreamPool_resizer != NULL &&
        rParams.pOutImage_resizer == NULL) {
        MY_LOGE("wrong resizer input");
        return BAD_VALUE;
    }
    #if SUPPORT_LCS
    if (rParams.pStreamPool_lcso != NULL &&
        rParams.pOutImage_lcso == NULL) {
        MY_LOGE("wrong lcso input");
        return BAD_VALUE;
    }
    if (rParams.enableLCS == MTRUE &&
        rParams.pOutImage_lcso == NULL) {
        MY_LOGE("LCS enable but no lcso input");
        return BAD_VALUE;
    }
    #endif
    //
    if (mpDeliverMgr != NULL && mpDeliverMgr->runningGet()/*isRunning()*/) {
        MY_LOGI("DeliverMgr thread is running");
        mpDeliverMgr->requestExit();
        mpDeliverMgr->trigger();
        mpDeliverMgr->join();
        mpDeliverMgr->runningSet(MFALSE);
    }

    // Get sensor format
    IHalSensorList *const pIHalSensorList = MAKE_HalSensorList();
    if (pIHalSensorList) {
        MUINT32 sensorDev = (MUINT32) pIHalSensorList->querySensorDevIdx(getOpenId());

        NSCam::SensorStaticInfo sensorStaticInfo;
        memset(&sensorStaticInfo, 0, sizeof(NSCam::SensorStaticInfo));
        pIHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
        mSensorFormatOrder = sensorStaticInfo.sensorFormatOrder;
    }

    //
    {
        RWLock::AutoWLock _l(mConfigRWLock);
        //
        for (int meta = STREAM_ITEM_START; meta < STREAM_META_NUM; meta++) {
            mvStreamMeta[meta] = NULL;
        }
        if (rParams.pInAppMeta != 0) {
            mvStreamMeta[STREAM_META_IN_APP] = rParams.pInAppMeta;
            //mInAppMeta = rParams.pInAppMeta;
        };
        if (rParams.pInHalMeta != 0) {
            mvStreamMeta[STREAM_META_IN_HAL] = rParams.pInHalMeta;
            //mInHalMeta = rParams.pInHalMeta;
        };
        if (rParams.pOutAppMeta != 0) {
            mvStreamMeta[STREAM_META_OUT_APP] = rParams.pOutAppMeta;
            //mOutAppMeta = rParams.pOutAppMeta;
        };
        if (rParams.pOutHalMeta != 0) {
            mvStreamMeta[STREAM_META_OUT_HAL] = rParams.pOutHalMeta;
            //mOutHalMeta = rParams.pOutHalMeta;
        };
        //
        for (int img = STREAM_ITEM_START; img < STREAM_IMG_NUM; img++) {
            mvStreamImg[img] = NULL;
        }
        #if 1
        if (rParams.pInImage_yuv != 0) {
            mvStreamImg[STREAM_IMG_IN_YUV] = rParams.pInImage_yuv;
        };
        if (rParams.pInImage_opaque != 0) {
            mvStreamImg[STREAM_IMG_IN_OPAQUE] = rParams.pInImage_opaque;
        };
        if (rParams.pOutImage_opaque != 0) {
            mvStreamImg[STREAM_IMG_OUT_OPAQUE] = rParams.pOutImage_opaque;
        };
        #if 1
        for (size_t i  = 0; i < rParams.pvOutImage_full.size(); i++) {
            if (rParams.pvOutImage_full[i] != NULL) { // pick the first item
                mvStreamImg[STREAM_IMG_OUT_FULL] = rParams.pvOutImage_full[i];
                break;
            }
        }
        #else
        if (rParams.pvOutImage_full.size() > 0) {
            mvStreamImg[STREAM_IMG_OUT_FULL] = rParams.pvOutImage_full[0];
            //mvStreamImg[STREAM_IMG_OUT_FULL] = rParams.pOutImage_full;
        };
        #endif
        if (rParams.pOutImage_resizer != 0) {
            mvStreamImg[STREAM_IMG_OUT_RESIZE] = rParams.pOutImage_resizer;
        };
        #if SUPPORT_LCS
        if (rParams.pOutImage_lcso != 0) {
            mvStreamImg[STREAM_IMG_OUT_LCS] = rParams.pOutImage_lcso;
        };
        #endif
        //
        mpStreamPool_full = (rParams.pStreamPool_full != NULL) ?
            rParams.pStreamPool_full : NULL;
        mpStreamPool_resizer = (rParams.pStreamPool_resizer != NULL) ?
            rParams.pStreamPool_resizer : NULL;
        #if SUPPORT_LCS
        mpStreamPool_lcso = (rParams.pStreamPool_lcso != NULL) ?
            rParams.pStreamPool_lcso : NULL;
        #endif
        //
        {
            MBOOL deliver_mgr_send = MTRUE;
            #if 0
            #warning "[FIXME] force to change p1 dispatch frame directly"
            {
                MUINT8 dispatch =
                    ::property_get_int32("debug.camera.p1dispatch", 0);
                MY_LOGI("debug.camera.p1dispatch = %d", dispatch);
                if (dispatch > 0) {
                    deliver_mgr_send = MFALSE;
                };
            }
            #endif
            if (mvStreamImg[STREAM_IMG_IN_YUV] != NULL ||
                mvStreamImg[STREAM_IMG_IN_OPAQUE] != NULL) {
                deliver_mgr_send = MTRUE; // for reprocessing flow
            };
            MY_LOGD2("USE DeliverMgr Thread Loop : %d", deliver_mgr_send);
            if (deliver_mgr_send) {
                if (mpDeliverMgr != NULL &&
                    NO_ERROR == mpDeliverMgr->run("P1NodeImp::config")) {
                    MY_LOGD2("RUN DeliverMgr Thread OK");
                    mpDeliverMgr->runningSet(MTRUE);
                } else {
                    MY_LOGE("RUN DeliverMgr Thread FAIL");
                    return BAD_VALUE;
                }
            }
        }
        #endif
        //
        #if 0
        #warning "[FIXME] force to change p1 not use pool"
        {
            MUINT8 no_pool =
                    ::property_get_int32("debug.camera.p1nopool", 0);
            if (no_pool > 0) {
                mpStreamPool_full = NULL;
                mpStreamPool_resizer = NULL;
                mpStreamPool_lcso = NULL;
            }
            MY_LOGI("debug.camera.p1nopool = %d", no_pool);
        }
        #endif
        //
        mBurstNum = MAX(rParams.burstNum, 1);
        #if 0 // for SMVR IT
        #warning "[FIXME] force to change p1 burst number"
        {
            MUINT8 burst_num =
                    ::property_get_int32("debug.camera.p1burst", 0);
            if (burst_num > 0) {
                mBurstNum = burst_num;
            }
            MY_LOGI("debug.camera.p1burst = %d  -  BurstNum = %d",
                burst_num, mBurstNum);
        }
        #endif
        //
        mReceiveMode = rParams.receiveMode;
        if ((mBurstNum > 1)
            || (mvStreamImg[STREAM_IMG_IN_OPAQUE] != NULL)
            || (mvStreamImg[STREAM_IMG_IN_YUV] != NULL)
            ) {
            mReceiveMode = REV_MODE_CONSERVATIVE;
        }
        #if 0 // receive mode IT
        #warning "[FIXME] force to change p1 receive mode"
        {
            MUINT8 rev_mode =
                    ::property_get_int32("debug.camera.p1rev", 0);
            if (rev_mode > 0) {
                mReceiveMode = (REV_MODE)rev_mode;
            }
            MY_LOGI("debug.camera.p1rev = %d  - RevMode=%d BurstNum=%d",
                rev_mode, mReceiveMode, mBurstNum);
        }
        #endif
        //
        mSensorParams = rParams.sensorParams;
        //
        mEnableDualPD = rParams.enableDualPD;
        //
        mRawDefType = EPipe_PURE_RAW;
        mRawOption = (1 << EPipe_PURE_RAW);
        if (rParams.rawProcessed == MTRUE || mEnableDualPD == MTRUE) {
            // DualPD must be processed raw
            mRawDefType = EPipe_PROCESSED_RAW;
            mRawOption |= (1 << EPipe_PROCESSED_RAW);
        }
        //
        mDisableFrontalBinning = rParams.disableFrontalBinning;
        //
        mEnableEISO = rParams.enableEIS;
        mEnableLCSO = rParams.enableLCS;
        //
        mEnableCaptureFlow = rParams.enableCaptureFlow;
        mEnableFrameSync = rParams.enableFrameSync;
        if (mEnableCaptureFlow) { // disable - acquire init buffer from pool
            mpStreamPool_full = NULL;
            mpStreamPool_resizer = NULL;
        }
    }
    //
    if (mvStreamImg[STREAM_IMG_OUT_OPAQUE] != NULL) {
        if (mvStreamImg[STREAM_IMG_OUT_FULL] != NULL) {
            mRawFormat = mvStreamImg[STREAM_IMG_OUT_FULL]->getImgFormat();
            mRawStride = mvStreamImg[STREAM_IMG_OUT_FULL]->getBufPlanes().
                itemAt(0).rowStrideInBytes;
            mRawLength = mvStreamImg[STREAM_IMG_OUT_FULL]->getBufPlanes().
                itemAt(0).sizeInBytes;
        } else {
            mRawFormat = P1_IMGO_DEF_FMT;
            NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo queryRst;
            NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
                    NSCam::NSIoPipe::PORT_IMGO.index,
                    NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE,
                    (EImageFormat)mRawFormat,
                    mSensorParams.size.w,
                    queryRst
                    );
            mRawStride = queryRst.stride_byte;
            mRawLength = mRawStride * mSensorParams.size.h;
        }
    }
    //
    //MY_LOGI("CAM_LOG_LEVEL %d", CAM_LOG_LEVEL);
    #if (IS_P1_LOGI)
    {
        android::String8 strInfo("");
        strInfo += String8::format("S(%d,%d,%d,%d,%dx%d)R(0x%x,%d,%d,%d,0x%x)"
            "E(%d,%d,%d,%d,%d,%d)M(%d,%d)",
            mSensorParams.mode, mSensorParams.fps, mSensorParams.pixelMode,
            mSensorParams.vhdrMode, mSensorParams.size.w, mSensorParams.size.h,
            mRawFormat, mRawStride, mRawLength, mRawDefType, mRawOption,
            mDisableFrontalBinning, mEnableEISO, mEnableLCSO, mEnableDualPD,
            mEnableCaptureFlow, mEnableFrameSync,
            mBurstNum, mReceiveMode);
        strInfo += String8::format("D(%d) ", mpDeliverMgr->runningGet());
        strInfo += String8::format("Pool(%p,%p,%p) ",
            (mpStreamPool_full != NULL) ?
            (mpStreamPool_full.get()) : (NULL),
            (mpStreamPool_resizer != NULL) ?
            (mpStreamPool_resizer.get()) : (NULL),
            (mpStreamPool_lcso != NULL) ?
            (mpStreamPool_lcso.get()) : (NULL));
        strInfo += String8::format(
            "MIA:%#" PRIxPTR ",MIH:%#" PRIxPTR ","
            "MOA:%#" PRIxPTR ",MOH:%#" PRIxPTR " ",
            (mvStreamMeta[STREAM_META_IN_APP] == NULL) ? (MINTPTR)(-1) :
            mvStreamMeta[STREAM_META_IN_APP]->getStreamId(),
            (mvStreamMeta[STREAM_META_IN_HAL] == NULL) ? (MINTPTR)(-1) :
            mvStreamMeta[STREAM_META_IN_HAL]->getStreamId(),
            (mvStreamMeta[STREAM_META_OUT_APP] == NULL) ? (MINTPTR)(-1) :
            mvStreamMeta[STREAM_META_OUT_APP]->getStreamId(),
            (mvStreamMeta[STREAM_META_OUT_HAL] == NULL) ? (MINTPTR)(-1) :
            mvStreamMeta[STREAM_META_OUT_HAL]->getStreamId());
        char const * img_name[STREAM_IMG_NUM] =
            {"YUVi", "RAWi", "RAWo", "IMGo", "RRZo", "LCSo"};
        for (int i = STREAM_ITEM_START; i < STREAM_IMG_NUM; i++) {
            if (mvStreamImg[i] != NULL) {
                strInfo += String8::format("%s:%d:%#" PRIxPTR "(%dx%d) ",
                    img_name[i], i, mvStreamImg[i]->getStreamId(),
                    mvStreamImg[i]->getImgSize().w,
                    mvStreamImg[i]->getImgSize().h);
            };
        }
        //
        MY_LOGI("%s", strInfo.string());
    }
    #endif

    {
        sp<IMetadataProvider> pMetadataProvider =
            NSMetadataProviderManager::valueFor(getOpenId());
        if( ! pMetadataProvider.get() ) {
            MY_LOGE(" ! pMetadataProvider.get() ");
            return DEAD_OBJECT;
        }
        IMetadata static_meta =
            pMetadataProvider->geMtktStaticCharacteristics();
        if( tryGetMetadata<MRect>(&static_meta,
            MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION, mActiveArray) ) {
            MY_LOGD_IF(mLogLevel > 0, "active array(%d, %d, %dx%d)",
                    mActiveArray.p.x, mActiveArray.p.y,
                    mActiveArray.s.w, mActiveArray.s.h);
        } else {
            MY_LOGE("no static info: MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION");
            #ifdef USING_MTK_LDVT
            mActiveArray = MRect(mSensorParams.size.w, mSensorParams.size.h);
            MY_LOGI("set sensor size to active array(%d, %d, %dx%d)",
                    mActiveArray.p.x, mActiveArray.p.y,
                    mActiveArray.s.w, mActiveArray.s.h);
            #else
            return UNKNOWN_ERROR;
            #endif
        }
    }

    return OK;

}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
config(ConfigParams const& rParams)
{
    PUBLIC_APIS_IN;

    Mutex::Autolock _l(mPublicLock);

    if(getActive()) {
        MY_LOGD("active=%d", getActive());
        onHandleFlush(MFALSE);
        if (getInit()) {
            Mutex::Autolock _l(mRequestQueueLock);
            setInit(MFALSE);
            mRequestQueueCond.broadcast();
        }
    }

    //(1) check
    MERROR err = check_config(rParams);
    if (err != OK) {
        MY_LOGE("Config Param - Check fail (%d)", err);
        return err;
    }

    //(2) configure hardware
    err = hardwareOps_start();
    if (err != OK) {
        MY_LOGE("Config Param - HW start fail (%d)", err);
        return err;
    }

    PUBLIC_APIS_OUT;

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
queue(
    sp<IPipelineFrame> pFrame
)
{
    PUBLIC_API_IN;
    //
    CAM_TRACE_FMT_BEGIN("P1:queue(%d,%d)",
        P1GET_FRM_NUM(pFrame), P1GET_REQ_NUM(pFrame));
    MY_LOGD2("active=%d", getActive());

    if (!getActive()) {
        MY_LOGI("HW start +++");
        MERROR err = hardwareOps_start();
        if (err != OK) {
            MY_LOGE("Queue Frame - HW start fail (%d)", err);
            return err;
        }
        MY_LOGI("HW start ---");
    }

    if (mEnableCaptureFlow && (!getReady())) {
        {
            Mutex::Autolock _l(mStartCaptureLock);
            mStartCaptureInitEnque = MTRUE;
        }
        {
            // create the init enque node
            QueJob_T job(mBurstNum);
            for (MUINT8 n = 0; n < mBurstNum; n++) {
                createNode(NULL, &job, &mProcessingQueue, &mProcessingQueueLock,
                                     &mControls3AList, &mControls3AListLock);
            }
            //
            mQueueJob.mMaxNum = mBurstNum;
            mQueueJob.mSet.setCapacity(mBurstNum);
            mQueueJob.mSet.clear();
        }
    }

    size_t padding = (m3AProcessedDepth * mBurstNum);
    MBOOL isStartSet = MFALSE;

    if (isRevMode(REV_MODE_NORMAL)) {
        #if 1// for REV_MODE_NORMAL
        if (mFirstReceived) {
            CAM_TRACE_NAME("P1:queue_wait_sync");
            Mutex::Autolock _l(mControls3AListLock);
            mReqState = REQ_STATE_WAITING;
            P1_LOGD("[P1::QUE] wait[%d]: %zu > %d", mReceiveMode,
                mControls3AList.size(), m3AProcessedDepth);
            mControls3AListCond.wait(mControls3AListLock);
            mReqState = REQ_STATE_ONGOING;
        } else {
            mReqState = REQ_STATE_RECEIVE;
            MY_LOGI("the first request received");
        }
        #endif

        {
            Mutex::Autolock _l(mControls3AListLock);
            mReqState = REQ_STATE_RECEIVE;
            //compensate to the number of mProcessedDepth
            #if 1// for REV_MODE_NORMAL
            padding--;
            #endif
            while (mControls3AList.size() < padding
                && mQueueJob.mSet.empty())
            {
                createNode(&mControls3AList, NULL);
            }

            //push node from appFrame
            createNode(pFrame, &mQueueJob, &mRequestQueue, &mRequestQueueLock,
                &mControls3AList, NULL);
            //
            #if 1// for REV_MODE_NORMAL
            mReqState = REQ_STATE_CREATED;
            mControls3AListCond.broadcast();
            #endif
            android_atomic_inc(&mInFlightRequestCnt);
            ATRACE_INT("P1_request_cnt",
                android_atomic_acquire_load(&mInFlightRequestCnt));

            #if 1// for REV_MODE_NORMAL
            if (!mFirstReceived) {
                Mutex::Autolock _ll(mRequestQueueLock);
                if (!mRequestQueue.empty()) {
                    mFirstReceived = MTRUE;
                    isStartSet = MTRUE;
                    MY_LOGI1("[%s#%d] start to set ReqQ[%zu] List[%zu]",
                        __FUNCTION__, __LINE__,
                        mRequestQueue.size(), mControls3AList.size());
                }
            }
            #endif
        }
    } else if (isRevMode(REV_MODE_AGGRESSIVE)) {

        {
            Mutex::Autolock _l(mControls3AListLock);
            mReqState = REQ_STATE_RECEIVE;
            //compensate to the number of mProcessedDepth
            #if 1// for REV_MODE_AGGRESSIVE
            padding--;
            #endif
            while (mControls3AList.size() < padding
                && mQueueJob.mSet.empty())
            {
                createNode(&mControls3AList, NULL);
            }

            //push node from appFrame
            createNode(pFrame, &mQueueJob, &mRequestQueue, &mRequestQueueLock,
                &mControls3AList, NULL);
            //
            #if 1// for REV_MODE_AGGRESSIVE
            mReqState = REQ_STATE_CREATED;
            mControls3AListCond.broadcast();
            #endif
            //CAM_TRACE_END();
            android_atomic_inc(&mInFlightRequestCnt);
            ATRACE_INT("P1_request_cnt",
                android_atomic_acquire_load(&mInFlightRequestCnt));

            if (!mFirstReceived) {
                Mutex::Autolock _ll(mRequestQueueLock);
                if (!mRequestQueue.empty()) {
                    mFirstReceived = MTRUE;
                    isStartSet = MTRUE;
                    MY_LOGI1("[%s#%d] start to set ReqQ[%zu] List[%zu]",
                        __FUNCTION__, __LINE__,
                        mRequestQueue.size(), mControls3AList.size());
                }
            }
        }

        #if 1// for REV_MODE_AGGRESSIVE
        if (!isStartSet) {
            CAM_TRACE_NAME("P1:queue_wait_send");
            Mutex::Autolock _l(mControls3AListLock);
            mReqState = REQ_STATE_WAITING;
            P1_LOGD("[P1::QUE] wait[%d]: %zu > %d", mReceiveMode,
                mControls3AList.size(), m3AProcessedDepth);
            mControls3AListCond.wait(mControls3AListLock);
            mReqState = REQ_STATE_ONGOING;
        } else {
            MY_LOGI("the first request received");
        }
        #endif
    } else { // REV_MODE_CONSERVATIVE

        {
            Mutex::Autolock _l(mControls3AListLock);

            #if 1// for REV_MODE_CONSERVATIVE
            //block condition 1: if pipeline is full
            while (mControls3AList.size() >
                (size_t)(((m3AProcessedDepth + 1) * mBurstNum) - 1))
            {
                CAM_TRACE_NAME("P1:queue_wait_size");
                P1_LOGD("[P1::QUE] wait[%d]: %zu > %d", mReceiveMode,
                    mControls3AList.size(), m3AProcessedDepth);
                status_t status = mControls3AListCond.wait(mControls3AListLock);
                MY_LOGD2("wait-");
                if  ( OK != status ) {
                    MY_LOGW(
                        "wait status:%d:%s, mControls3AList.size:%zu",
                        status, ::strerror(-status), mControls3AList.size()
                    );
                }
            }
            #endif
            //compensate to the number of mProcessedDepth
            #if 1// for REV_MODE_CONSERVATIVE
            if (mEnableCaptureFlow && (!mFirstReceived)) {
                padding --;
            }
            #endif
            while (mControls3AList.size() < padding
                && mQueueJob.mSet.empty())
            {
                createNode(&mControls3AList, NULL);
            }

            //push node from appFrame
            createNode(pFrame, &mQueueJob, &mRequestQueue, &mRequestQueueLock,
                &mControls3AList, NULL);
            android_atomic_inc(&mInFlightRequestCnt);
            ATRACE_INT("P1_request_cnt",
                android_atomic_acquire_load(&mInFlightRequestCnt));

            if (!mFirstReceived) {
                Mutex::Autolock _ll(mRequestQueueLock);
                if (!mRequestQueue.empty()) {
                    mFirstReceived = MTRUE;
                    isStartSet = MTRUE;
                    MY_LOGI1("[%s#%d] start to set ReqQ[%zu] List[%zu]",
                        __FUNCTION__, __LINE__,
                        mRequestQueue.size(), mControls3AList.size());
                }
            }
        }
    }
    //
    #if 1 //SET_REQUEST_BEFORE_FIRST_DONE
    if (isStartSet) {
        if (mEnableCaptureFlow && (!getReady())) {
            MY_LOGI("HW capture +++");
            MERROR err = hardwareOps_capture();
            if (err != OK) {
                MY_LOGE("Queue Frame - HW capture fail (%d)", err);
                return err;
            }
            MY_LOGI("HW capture ---");
        } else {
            onRequestFrameSet(MTRUE);
        }
    }
    #endif
    //
    if (getInit() && ((!mRequestQueue.empty())||(mEnableCaptureFlow))) {
        if (mEnableCaptureFlow) {
            MY_LOGI1("Capture - init waiting (%d)", getInit());
            mRequestQueueCond.broadcast();
        } else {
            Mutex::Autolock _l(mRequestQueueLock);
            if (!mRequestQueue.empty()) {
                MY_LOGI1("Request - init waiting (%d)", getInit());
                mRequestQueueCond.broadcast();
            }
        }
    }
    //
    #if 1// for REV_MODE_AGGRESSIVE
    if (isStartSet && (!mEnableCaptureFlow)) {
        if (isRevMode(REV_MODE_AGGRESSIVE)) {
            CAM_TRACE_NAME("P1:queue_wait_send_first");
            Mutex::Autolock _l(mControls3AListLock);
            mReqState = REQ_STATE_WAITING;
            P1_LOGD("[P1::QUE] first-wait[%d]: %zu > %d", mReceiveMode,
                mControls3AList.size(), m3AProcessedDepth);
            mControls3AListCond.wait(mControls3AListLock);
            mReqState = REQ_STATE_ONGOING;
        }
    }
    #endif
    //
    CAM_TRACE_FMT_END();
    //
    PUBLIC_API_OUT;
    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
flush(android::sp<IPipelineFrame> const &pFrame)
{
    return BaseNode::flush(pFrame);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
flush()
{
    PUBLIC_APIS_IN;

    CAM_TRACE_NAME("P1:flush");

    Mutex::Autolock _l(mPublicLock);

    onHandleFlush(MFALSE);

    //[TODO]
    //wait until deque thread going back to waiting state;
    //in case next node receives queue() after flush()

    PUBLIC_APIS_OUT;

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
void
P1NodeImp::
requestExit()
{
    FUNCTION_P1S_IN;

    //let deque thread back
    Thread::requestExit();
    {
        Mutex::Autolock _l(mThreadLock);
        mThreadCond.broadcast();
    }
    //
    if (getInit()) {
        Mutex::Autolock _l(mRequestQueueLock);
        setInit(MFALSE);
        mRequestQueueCond.broadcast();
    }
    join();

    //let enque thread back
    Mutex::Autolock _l(mControls3AListLock);
    mControls3AListCond.broadcast();

    FUNCTION_P1S_OUT;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
P1NodeImp::
readyToRun()
{
    // set name
    ::prctl(PR_SET_NAME, (unsigned long)"Cam@P1NodeImp", 0, 0, 0);

    // set normal
    struct sched_param sched_p;
    sched_p.sched_priority = 0;
    ::sched_setscheduler(0, P1THREAD_POLICY, &sched_p);
    ::setpriority(PRIO_PROCESS, 0, P1THREAD_PRIORITY);   //  Note: "priority" is nice value.
    //
    ::sched_getparam(0, &sched_p);
    MY_LOGD(
        "Tid: %d, policy: %d, priority: %d"
        , ::gettid(), ::sched_getscheduler(0)
        , sched_p.sched_priority
    );

    //
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
bool
P1NodeImp::
threadLoop()
{
    // check if going to leave thread
    CAM_TRACE_NAME("P1:threadLoop");
    {
        Mutex::Autolock _l(mThreadLock);

        if (!getReady()) {
            MY_LOGD("wait+");
            mThreadCond.wait(mThreadLock);
            MY_LOGD("wait-");
        }

        if (exitPending()) {
            MY_LOGD("leaving");
            return false;
        }
    }

    // deque buffer, and handle frame and metadata
    onProcessDequeFrame();


    // trigger point for the first time
    {
        if (getInit()) {
            {
                Mutex::Autolock _l(mRequestQueueLock);
                MY_LOGI1("Init(%d) Req(%zu) Cap(%d)", getInit(),
                    mRequestQueue.size(), mEnableCaptureFlow);
                while(getInit() &&
                    ((mRequestQueue.empty()) && (!mEnableCaptureFlow))) {
                    MY_LOGI1("init request wait+");
                    mRequestQueueCond.wait(mRequestQueueLock);
                    MY_LOGI1("init request wait-");
                }
            }
        #if 1 //SET_REQUEST_BEFORE_FIRST_DONE
            // move to the first queue execution
        #else
            onRequestFrameSet(MTRUE);
        #endif
            setInit(MFALSE);
        }
    }

    if ((mpDeliverMgr != NULL) && (!mpDeliverMgr->runningGet())) {
        onProcessDropFrame();
    };

    return true;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
setActive(
    MBOOL active
)
{
    Mutex::Autolock _l(mActiveLock);
    mActive = active;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::
getActive(
    void
)
{
    Mutex::Autolock _l(mActiveLock);
    return mActive;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
setReady(
    MBOOL ready
)
{
    Mutex::Autolock _l(mReadyLock);
    mReady = ready;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::
getReady(
    void
)
{
    Mutex::Autolock _l(mReadyLock);
    return mReady;
}



/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
setInit(
    MBOOL init
)
{
    Mutex::Autolock _l(mInitLock);
    mInit = init;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::
getInit(
    void
)
{
    Mutex::Autolock _l(mInitLock);
    return mInit;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onRequestFrameSet(
    MBOOL initial
)
{
    FUNCTION_P1_IN;
    //
    Mutex::Autolock _ll(mFrameSetLock);
    if (!initial && !mFrameSetAlready) {
        MY_LOGI("frame set not init complete");
        return;
    }
    //
    size_t size = 0;
    size_t index = (m3AProcessedDepth - 1) * mBurstNum;
    size_t depth = m3AProcessedDepth * mBurstNum;
    MBOOL set = MFALSE;
    //
    #if (IS_P1_LOGD)
    String8 str("");
    #endif
    //
    if (isRevMode(REV_MODE_NORMAL) || isRevMode(REV_MODE_AGGRESSIVE)) {
        MBOOL isSetWaiting = MFALSE;
        if (!initial) {
            {
                CAM_TRACE_NAME("ReqSet_GetListLock_E");
                Mutex::Autolock _l(mControls3AListLock);
                size_t idx = index;
                MINT32 curNum = 0;
                if (mControls3AList.size() > index) {
                    List<MetaSet_T>::iterator it = mControls3AList.begin();
                    for (; it != mControls3AList.end(); it++) {
                        if (idx-- == 0) {
                            curNum = it->halMeta.entryFor
                                (MTK_P1NODE_PROCESSOR_MAGICNUM).
                                itemAt(0, Type2Type< MINT32 >());
                            if (1 == it->halMeta.entryFor
                                (MTK_HAL_REQUEST_DUMMY).
                                itemAt(0, Type2Type< MUINT8 >())) {
                                P1_LOGD("Check-Padding (%d)", curNum);
                                curNum = 0;
                            }
                            break;
                        }
                    }
                }
                if (mLastSetNum >= curNum) {
                    for (MUINT8 n = 0; (n < mBurstNum) &&
                        (!mControls3AList.empty()); n++) {
                        mControls3AList.erase(mControls3AList.begin());
                    }
                } else {
                    P1_LOGD("Check-CtrlList (%d < %d)", mLastSetNum, curNum);
                }
                mTagList.set(mControls3AList.size());
                if (mControls3AList.size() < depth) {
                    if (mReqState == REQ_STATE_WAITING) {
                        isSetWaiting = MTRUE;
                    }
                    mControls3AListCond.broadcast();
                } else {
                    P1_LOGD("Check-Request (%d) (%zu)", mLastSetNum, depth);
                }
            }
            //
            if (isSetWaiting) {
                CAM_TRACE_NAME("ReqSet_GetListLock_W");
                Mutex::Autolock _l(mControls3AListLock);
                if ((mReqState != REQ_STATE_CREATED) &&
                    (mControls3AList.size() < depth)) {
                    mControls3AListCond.wait(mControls3AListLock);
                }
            }
        }
    } else { // REV_MODE_CONSERVATIVE
        CAM_TRACE_NAME("ReqSet_GetListLock_B");
        Mutex::Autolock _l(mControls3AListLock);
        size_t idx = index;
        MINT32 curNum = 0;
        if (mControls3AList.size() > index) {
            List<MetaSet_T>::iterator it = mControls3AList.begin();
            for (; it != mControls3AList.end(); it++) {
                if (idx-- == 0) {
                    curNum = it->halMeta.entryFor
                        (MTK_P1NODE_PROCESSOR_MAGICNUM).
                        itemAt(0, Type2Type< MINT32 >());
                    if (1 == it->halMeta.entryFor
                        (MTK_HAL_REQUEST_DUMMY).
                        itemAt(0, Type2Type< MUINT8 >())) {
                        P1_LOGD("Check-Padding (%d)", curNum);
                        curNum = 0;
                    }
                    break;
                }
            }
        }
        if (mLastSetNum >= curNum) {
            for (MUINT8 n = 0; (n < mBurstNum) &&
                (!mControls3AList.empty()); n++) {
                mControls3AList.erase(mControls3AList.begin());
            }
        } else {
            P1_LOGD("Check-CtrlList (%d < %d)", mLastSetNum, curNum);
        }
        mTagList.set(mControls3AList.size());
        mControls3AListCond.broadcast();
    }
    // check control list
    {
        Mutex::Autolock _l(mControls3AListLock);
        MINT32 num = 0;
        size_t idx = 0;
        size = mControls3AList.size();
        List<MetaSet_T>::iterator it = mControls3AList.begin();
        for (; it != mControls3AList.end(); it++) {
            num = it->halMeta.entryFor(MTK_P1NODE_PROCESSOR_MAGICNUM).
                itemAt(0, Type2Type< MINT32 >());
            #if (IS_P1_LOGD)
            if (mLogLevel > 0) {
                if ((idx > 0) && (idx % mBurstNum == 0)) {
                    str += String8::format(", ");
                }
                str += String8::format("%d ", num);
            }
            #endif
            if ((idx == index) && (size >= depth)) {
                mLastSetNum = num;
            }
            if (idx == (depth - 1)) {
                mTagSet.set(num);
            }
            idx++;
        }
        if (size < depth) {
            P1_LOGD("Check-Size (%zu < %zu)", size, depth);
            #if SUPPORT_3A
            if (mp3A) {
                CAM_TRACE_BEGIN("P1:3A-set");
                mp3A->set(mControls3AList);
                CAM_TRACE_END();
            }
            #endif
        } else {
            set = MTRUE;
        }
    }
    // call 3A.set
    if (set) {
        #if SUPPORT_3A
        if (mp3A) {
            CAM_TRACE_FMT_BEGIN("P1:3A-set(%d)", mLastSetNum);
            mp3A->set(mControls3AList);
            CAM_TRACE_FMT_END();
        }
        #endif
    }
    //
    mFrameSetAlready = MTRUE;
    //dump
#if (IS_P1_LOGD)
    if (mLogLevel > 0) {
        P1_LOGD("[P1::SET] (%d)(%d) Ctrl3AList[%zu] = [%s]", mLastSetNum, set,
            size, str.string());
    }
#endif
    //
    FUNCTION_P1_OUT;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onSyncEnd()
{
    FUNCTION_P1_IN;
    //
    if (mEnableCaptureFlow && (!getReady())) {
        Mutex::Autolock _l(mStartCaptureLock);
        MY_LOGD2("StartCaptureState(%d)", mStartCaptureState);
        if (mStartCaptureState != START_CAP_STATE_READY) {
            MY_LOGI("should not callback before capture ready (%d)",
                mStartCaptureState);
            return;
        }
    }
    //
    if (getInit()) {
        MY_LOGI("the init is not complete");
    }
    {
        Mutex::Autolock _ll(mFrameSetLock);
        if (!mFrameSetAlready) {
            MY_LOGI("should not callback before first set");
            return;
        }
    }
    //
    CAM_TRACE_FMT_BEGIN("onSyncEnd (%d)", mLastSetNum);
    //
    if (isRevMode(REV_MODE_NORMAL) || isRevMode(REV_MODE_AGGRESSIVE)) {
        onRequestFrameSet(MFALSE);
    }
    //
    CAM_TRACE_FMT_END();
    //
    FUNCTION_P1_OUT;
    return;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onSyncBegin(
    MBOOL initial,
    RequestSet_T* reqSet,//MUINT32 magicNum,
    MUINT32 sofIdx,
    CapParam_T* capParam
)
{
    FUNCTION_P1_IN;
    MUINT32 magicNum = 0;
    //
    if (mEnableCaptureFlow && (!getReady())) {
        Mutex::Autolock _l(mStartCaptureLock);
        MY_LOGD2("StartCaptureState(%d)", mStartCaptureState);
        if (mStartCaptureState == START_CAP_STATE_WAIT_CB) {
            if (capParam != NULL) {
                mStartCaptureType = capParam->u4CapType;
                mStartCaptureExp = MAX(capParam->i8ExposureTime, 0);
                if (reqSet != NULL && reqSet->vNumberSet.size() > 0 &&
                    mBurstNum <= 1) {
                    mLongExp.set(reqSet->vNumberSet[0], mStartCaptureExp);
                }
            }
            mStartCaptureState = START_CAP_STATE_READY;
            mStartCaptureCond.broadcast();
            MY_LOGI1("StartCaptureReady - init(%d) CapType(%d) CapExp(%" PRId64 ")",
                getInit(), mStartCaptureType, mStartCaptureExp);
            return;
        } else if (mStartCaptureState == START_CAP_STATE_WAIT_REQ) {
            MY_LOGI("should not callback before capture set (%d)",
                mStartCaptureState);
            return;
        }
    }
    //
    if (getInit()) {
        MY_LOGI("the init is not complete");
    }
    {
        Mutex::Autolock _ll(mFrameSetLock);
        if (!mFrameSetAlready) {
            MY_LOGI("should not callback before first set");
            return;
        }
    }
    //
    if (reqSet != NULL && reqSet->vNumberSet.size() > 0) {
        magicNum = reqSet->vNumberSet[0];
    }
    //
    CAM_TRACE_FMT_BEGIN("onSyncBegin (%d)", magicNum);
    //
    //(1)
    if((!initial) && (getReady())) { // [TODO] && VALID 3A PROCESSED NOTIFY
        QueJob_T job(mBurstNum);
        bool exist = false;
        {
            Mutex::Autolock _l(mRequestQueueLock);
            Que_T::iterator it = mRequestQueue.begin();
            for(; it != mRequestQueue.end(); it++) {
                if ((*it).mFirstMagicNum == magicNum) {
                    job = *it;
                    for (MUINT8 i = 0; i < job.mSet.size(); i++) {
                        job.mSet.editItemAt(i).sofIdx = sofIdx;
                        if (capParam != NULL) {
                            job.mSet.editItemAt(i).capType =
                                capParam->u4CapType;
                            job.mSet.editItemAt(i).frameExpDuration =
                                MAX(capParam->i8ExposureTime, 0);
                            if (mBurstNum <= 1) {
                                mLongExp.set(job.mSet[i].magicNum,
                                    job.mSet[i].frameExpDuration);
                            }
                            MY_LOGI_IF(((capParam->i8ExposureTime >= 400000000)
                                || (capParam->i8ExposureTime <= 0)),
                                "check CB num(%d) cap(%d) exp(%" PRId64 ")", magicNum,
                                capParam->u4CapType, capParam->i8ExposureTime);
                            if ((job.mSet[i].capType != E_CAPTURE_NORMAL) &&
                                (job.mSet[i].appFrame != NULL)) {
                                MY_LOGI_IF((mLogLevel > 1),
                                    "Job(%d) - Cap(%d)(%" PRId64 ") - "
                                    P1INFO_NODE_STR,
                                    job.mFirstMagicNum,
                                    capParam->u4CapType,
                                    capParam->i8ExposureTime,
                                    P1INFO_NODE_VAR(job.mSet[i]));
                            }
                        } else {
                            MY_LOGW("cannot find cap param (%d)", magicNum);
                        }
                    }
                    mRequestQueue.erase(it);
                    exist = true;
                    break;
                }
            }
        }
        if (exist) {
            if (OK != onProcessEnqueFrame(job)) {
                MY_LOGE("frame en-queue fail (%d)", magicNum);
                for (MUINT8 i = 0; i < job.mSet.size(); i++) {
                    onReturnFrame(job.mSet.editItemAt(i), MTRUE, MTRUE);
                }
            } else {
                if ((mBurstNum <= 1) && // exclude burst mode
                    (job.mSet.size() == 1 && job.mSet[0].appFrame != NULL) &&
                    (capParam != NULL && capParam->metadata.count() > 0)) {
                    requestMetadataEarlyCallback(job.mSet[0], // const node
                        STREAM_META_OUT_HAL, &(capParam->metadata));
                }
            }
        } else {
            //MY_LOGW_IF(magicNum!=0, "no: %d", magicNum);
            #if (IS_P1_LOGI)
            Mutex::Autolock _l(mRequestQueueLock);
            String8 str;
            str += String8::format("[req(%d)/size(%d)]: ",
                magicNum, (int)(mRequestQueue.size()));
            Que_T::iterator it = mRequestQueue.begin();
            for(; it != mRequestQueue.end(); it++) {
                str += String8::format("%d ", (*it).mFirstMagicNum);
            }
            MY_LOGI("%s", str.string());
            #endif
        }
    }
    //
    if (isRevMode(REV_MODE_CONSERVATIVE)) {
        onRequestFrameSet(MFALSE);
    }
    //
    CAM_TRACE_FMT_END();
    //
    FUNCTION_P1_OUT;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
onProcessEnqueFrame(
    QueJob_T &job
)
{
    FUNCTION_P1_IN;

    //(1)
    //pass request directly if it's a reprocessing one
    //[TODO]
    //if( mInHalMeta == NULL) {
    //    onDispatchFrame(pFrame);
    //    return;
    //}

    //(2)
    return hardwareOps_enque(job, true);

    FUNCTION_P1_OUT;
}


/******************************************************************************
 *
 ******************************************************************************/
P1NodeImp::QueJob_T
P1NodeImp::
getProcessingFrame_ByNumber(MUINT32 magicNum)
{
    FUNCTION_P1_IN;
    QueJob_T job(mBurstNum);

    Mutex::Autolock _l(mProcessingQueueLock);
    if (mProcessingQueue.empty()) {
        MY_LOGE("mProcessingQueue is empty");
        return job;
    }

    #if 1
        Que_T::iterator it = mProcessingQueue.begin();
        for (; it != mProcessingQueue.end(); it++) {
            if ((*it).mFirstMagicNum == magicNum) {
                break;
            }
        }
        if (it == mProcessingQueue.end()) {
            MY_LOGI("cannot find the right node for num: %d", magicNum);
            job.mSet.clear();
            return job;
        }
        else {
            job = *it;
            mProcessingQueue.erase(it);
            mProcessingQueueCond.broadcast();
        }
    #else
        job = *mProcessingQueue.begin();
        mProcessingQueue.erase(mProcessingQueue.begin());
        mProcessingQueueCond.broadcast();
    #endif

    FUNCTION_P1_OUT;
    //
    return job;
}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::
getProcessingFrame_ByAddr(IImageBuffer* const imgBuffer,
                          MUINT32 magicNum,
                          QueJob_T &job
)
{
    FUNCTION_P1_IN;

    MBOOL ret = MFALSE;
    if (imgBuffer == NULL) {
        MY_LOGE("imgBuffer == NULL");
        return ret;
    }

    // get the right node from mProcessingQueue
    Mutex::Autolock _l(mProcessingQueueLock);
    if (mProcessingQueue.empty()) {
        MY_LOGE("ProQ is empty");
        return ret;
    }

    Que_T::iterator it = mProcessingQueue.begin();
    for (; it != mProcessingQueue.end(); it++) {
        if (imgBuffer ==
            (*it).mSet[0].streamBufImg[STREAM_IMG_OUT_FULL].spImgBuf.get() ||
            imgBuffer ==
            (*it).mSet[0].streamBufImg[STREAM_IMG_OUT_OPAQUE].spImgBuf.get() ||
            imgBuffer ==
            (*it).mSet[0].streamBufImg[STREAM_IMG_OUT_RESIZE].spImgBuf.get() ||
            imgBuffer ==
            (*it).mSet[0].streamBufImg[STREAM_IMG_OUT_LCS].spImgBuf.get()) {
            if ((*it).mFirstMagicNum == magicNum) {
                ret = MTRUE;
            } else {
                #if SUPPORT_PERFRAME_CTRL
                MY_LOGE("magicNum from driver(%d), should(%d)",
                       magicNum, (*it).mFirstMagicNum);
                #else
                if((magicNum & 0x40000000) != 0) {
                    MY_LOGW("magicNum from driver(0x%x) is uncertain",
                          magicNum);
                    ret = MFALSE;
                } else {
                    ret = MTRUE;
                    MY_LOGW("magicNum from driver(%d), should(%d)",
                          magicNum, (*it).mFirstMagicNum);
                }
                #endif
                // reset node from 3A info
                for (size_t i = 0; i < (*it).mSet.size(); i++) {
                    (*it).mSet.editItemAt(i).capType = E_CAPTURE_NORMAL;
                    (*it).mSet.editItemAt(i).frameExpDuration = 0;
                }
            }
            break;
        } else {
            continue;
        }
    }

    if (it == mProcessingQueue.end()) {
        MY_LOGE("no node with imagebuf(%p), PA(0x%zx), num(%d)",
                 imgBuffer, imgBuffer->getBufPA(0), magicNum);
        #if 0 // dump RequestQueue/ProcessingQueue
        for (Que_T::iterator it = mProcessingQueue.begin(); it != mProcessingQueue.end(); it++) {
            MY_LOGW("[proQ] num(%d)", (*it).mSet[0].magicNum);
            MY_LOGW_IF((*it).mSet[0].img_full!=NULL, "[proQ] imagebuf(0x%x), PA(0x%x)",
                (*it).mSet[0].img_full.get(), (*it).mSet[0].img_full->getBufPA(0));
            MY_LOGW_IF((*it).mSet[0].img_resizer!=NULL, "[proQ] imagebuf(0x%x), PA(0x%x)",
                (*it).mSet[0].img_resizer.get(), (*it).mSet[0].img_resizer->getBufPA(0));
            MY_LOGW_IF((*it).mSet[0].img_lcso!=NULL, "[proQ] imagebuf(0x%x), PA(0x%x)",
                (*it).mSet[0].img_lcso.get(), (*it).mSet[0].img_lcso->getBufPA(0));
        }
        for (Que_T::iterator it = mRequestQueue.begin(); it != mRequestQueue.end(); it++) {
            MY_LOGW("[reqQ] magic %d:", (*it).mFirstMagicNum);
        }
        #endif
    }
    else {
        //
        job = *it;
        mProcessingQueue.erase(it);
        mProcessingQueueCond.broadcast();
        MY_LOGD2("magic: %d", magicNum);
    }
    //
    FUNCTION_P1_OUT;
    //
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
onProcessDropFrame(MBOOL isTrigger)
{
    Vector<QueNode_T > nodeQ;
    {
        Mutex::Autolock _l(mDropQueueLock);
        for(size_t i = 0; i < mDropQueue.size(); i++) {
            QueJob_T job = getProcessingFrame_ByNumber(mDropQueue[i]);
            // if getProcessingFrame_ByNumber can not find the job
            // the job set size is 0
            for (MUINT8 i = 0; i < job.mSet.size(); i++) {
                QueNode_T node = job.mSet.editItemAt(i);
                nodeQ.push_back(node);
            }
            MY_LOGI("drop: %d", mDropQueue[i]);
            P1_LOGD("DropQueue[%zu/%zu] = %d",
                i, mDropQueue.size(), mDropQueue[i]);
        }
        mDropQueue.clear();
    }

    for(size_t i = 0; i < nodeQ.size(); i++) {
        //
        if (mBurstNum <= 1) {
            mLongExp.reset(nodeQ[i].magicNum);
        }
        //
        #if SUPPORT_LMV
        if (mpLMV && (nodeQ[i].buffer_eiso != 0)) {
            sp<IImageBuffer> pImgBuf = nodeQ[i].buffer_eiso;
            mpLMV->NotifyLMV(pImgBuf);
        }
        #endif
        nodeQ.editItemAt(i).exeState = EXE_STATE_DONE;
        onReturnFrame(nodeQ.editItemAt(i), MTRUE,
            ((isTrigger) && (i == (nodeQ.size() - 1))) ? MTRUE : MFALSE);
    }

    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
onProcessDequeFrame()
{

#if 0
    // [FIXME]  temp-WA for DRV currently not implement self-signal
    //          the dequeue might be blocked while en-queue empty
    //          it should be removed after DRV self-signal ready
    {
        Mutex::Autolock _ll(mProcessingQueueLock);
        if (mProcessingQueue.empty()) {
            return OK;
        }
    }
#endif

    FUNCTION_P1_IN;

    CAM_TRACE_NAME("P1:ProcessDequedFrame");

    MERROR ret= OK;
    QBufInfo deqBuf;
    MetaSet_T result3A;
    if(hardwareOps_deque(deqBuf) != OK) {
        return BAD_VALUE;
    }

    if (deqBuf.mvOut.size() == 0) {
        MY_LOGI("DeqBuf Out Size is 0");
        return ((getActive()) ? BAD_VALUE : OK);
    }

    QueJob_T job(mBurstNum);
    MBOOL match = getProcessingFrame_ByAddr(deqBuf.mvOut[0].mBuffer,
                    deqBuf.mvOut[0].mMetaData.mMagicNum_hal, job);
    //
    if (mBurstNum <= 1) {
        mLongExp.reset(deqBuf.mvOut[0].mMetaData.mMagicNum_hal);
    }
    //
    for (MUINT8 i = 0; i < job.mSet.size(); i++) {
        QueNode_T node = job.mSet.editItemAt(i);

        // camera display systrace - DeQ
        if  ( ATRACE_ENABLED() && node.appFrame != 0 )
        {
            MINT64 const timestamp = deqBuf.mvOut[i].mMetaData.mTimeStamp;
            String8 const str = String8::format(
                "Cam:%d:IspP1:deq|timestamp(ns):%" PRId64
                " duration(ns):%" PRId64
                " request:%d frame:%d",
                getOpenId(), timestamp, ::systemTime()-timestamp,
                node.appFrame->getRequestNo(), node.appFrame->getFrameNo()
            );
            CAM_TRACE_BEGIN(str.string());
            CAM_TRACE_END();
        }

        MY_LOGD2("job(%d)[%d] = node(%d)",
            job.mFirstMagicNum, i, node.magicNum);
        mTagDeq.set(node.magicNum);
        #if SUPPORT_3A
        if (getActive() && mp3A && node.reqType == REQ_TYPE_NORMAL) {
            mp3A->notifyP1Done(node.magicNum);
            if (match && node.capType == E_CAPTURE_HIGH_QUALITY_CAPTURE) {
                CAM_TRACE_FMT_BEGIN("P1:3A-getCur(%d)", node.magicNum);
                MY_LOGD1("GetCurrent(%d) +++", node.magicNum);
                mp3A->getCur(node.magicNum, result3A);
                MY_LOGD1("GetCurrent(%d) ---", node.magicNum);
                CAM_TRACE_FMT_END();
            } else {
                mp3A->get(node.magicNum, result3A);
            }
        }
        #endif

        // check the ReqExpRec
        if (match && (node.expRec != EXP_REC_NONE)) {
            MY_LOGI("check ExpRec " P1INFO_NODE_STR, P1INFO_NODE_VAR(node));
            #if 0 // flush this frame
            match = MFALSE;
            #endif
        }
        // check the result of raw type
        if (match) {
            MBOOL raw_match = MTRUE;
            for (size_t j = 0; j < deqBuf.mvOut.size(); j++) {
                if (deqBuf.mvOut.at(j).mPortID.index == PORT_IMGO.index) {
                    if (IS_OUT(REQ_OUT_FULL_PROC, node.reqOutSet) &&
                        deqBuf.mvOut[(j + i)].mMetaData.mRawType != EPipe_PROCESSED_RAW) {
                        raw_match = MFALSE;
                    }
                    break;
                }
            }
            //MY_LOGD2("raw match: %d", raw_match);
            if (!raw_match) {
                MY_LOGE("RawType mismatch "
                    P1INFO_NODE_STR, P1INFO_NODE_VAR(node));
                #if 1 // flush this frame
                match = MFALSE;
                #endif
            }
        }
        //
        #if 0
        if (node.magicNum > 0 && node.magicNum < 15) {
            printf("[%d]node.img_resizer.get() = [%p]\n",
                node.magicNum, node.img_resizer.get());
            if (node.img_resizer.get() != NULL) {
                char filename[256] = {0};
                sprintf(filename, "/sdcard/raw/P1B_%d_%dx%d.raw",
                    node.magicNum,
                    node.img_resizer->getImgSize().w,
                    node.img_resizer->getImgSize().h
                    );
                printf("SAVE BUF [%s]\n", filename);
                node.img_resizer->saveToFile(filename);
            }
        }
        #endif
        node.frameTimeStamp = deqBuf.mvOut[i].mMetaData.mTimeStamp;
        node.exeState = EXE_STATE_DONE;
        if (match == MFALSE || node.reqType == REQ_TYPE_INITIAL) {
            onReturnFrame(node, MTRUE, MTRUE);
            ret = BAD_VALUE;
        }
        else {
            IMetadata resultLMV;
            IMetadata inAPP,inHAL;
            MINT32 eisMode = 0;

            if ((IS_PORT(CONFIG_PORT_EISO, mConfigPort)) &&
                (OK == frameMetadataGet(node, STREAM_META_IN_APP, &inAPP))) {
                if (OK == frameMetadataGet(node, STREAM_META_IN_HAL, &inHAL)) {
                    IMetadata::IEntry entry = inHAL.entryFor(MTK_EIS_MODE);
                    if (entry.count() > 0) {
                        eisMode = entry.itemAt(0, Type2Type<MINT32>());
                    }
                }

                if (isEISOn(&inAPP) || is3DNROn(&inAPP) ||
                    EIS_MODE_IS_CALIBRATION_ENABLED(eisMode)) {

                    MUINT8 mode = 0;
                    MINT64 exposureTime = 0;

                    if (!tryGetMetadata<MUINT8>(&inAPP, MTK_CONTROL_CAPTURE_INTENT, mode)) {
                        MY_LOGW("no MTK_CONTROL_CAPTURE_INTENT");
                    }

                    if (!tryGetMetadata<MINT64>(&result3A.appMeta, MTK_SENSOR_EXPOSURE_TIME, exposureTime)) {
                        MY_LOGW("no MTK_SENSOR_EXPOSURE_TIME");
                    }

                    onProcessLMV(node, resultLMV, deqBuf, eisMode, mode, exposureTime, i);
                }
            }
            onProcessResult(node, deqBuf, result3A, resultLMV, i);
            mLastSofIdx = node.sofIdx;
            ret = OK;
        }
    }

    #if SUPPORT_LMV
    if (IS_PORT(CONFIG_PORT_EISO, mConfigPort)) {
        // call LMV notify function
        if (mpLMV)
        {
            mpLMV->NotifyLMV(deqBuf);
        }
    }
    #endif

    FUNCTION_P1_OUT;

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onHandleFlush(
    MBOOL wait
)
{
    FUNCTION_P1S_IN;
    CAM_TRACE_NAME("P1:HandleFlush");

    //wake up queue thread.
    {
        Mutex::Autolock _l(mControls3AListLock);
        mControls3AListCond.broadcast();
    }

    //stop hardware
    if (!wait) {
        hardwareOps_stop(); //include hardware and 3A
    }

    //(1) clear request queue
    {
        Mutex::Autolock _l(mRequestQueueLock);
        //P1_LOGD("Check-RQ (%d)", mRequestQueue.size());
        while(!mRequestQueue.empty()) {
            QueJob_T job = *mRequestQueue.begin();
            mRequestQueue.erase(mRequestQueue.begin());
            for (MUINT8 i = 0; i < job.mSet.size(); i++) {
                QueNode_T node = job.mSet[i];
                onReturnFrame(node, MTRUE, MFALSE);
            }
        }
        if (!mQueueJob.mSet.empty()) {
            for (MUINT8 i = 0; i < mQueueJob.mSet.size(); i++) {
                QueNode_T node = mQueueJob.mSet[i];
                onReturnFrame(node, MTRUE, MFALSE);
            }
            mQueueJob.mSet.clear();
        }
    }

    //(2) clear processing queue
    //     wait until processing frame coming out
    if (wait) {
        Mutex::Autolock _l(mProcessingQueueLock);
        while(!mProcessingQueue.empty()) {
            mProcessingQueueCond.wait(mProcessingQueueLock);
        }
    } else {
        // must guarantee hardware has been stopped.
        Mutex::Autolock _l(mProcessingQueueLock);
        //P1_LOGD("Check-PQ (%d)", mProcessingQueue.size());
        while(!mProcessingQueue.empty()) {
            QueJob_T job = *mProcessingQueue.begin();
            mProcessingQueue.erase(mProcessingQueue.begin());
            for (MUINT8 i = 0; i < job.mSet.size(); i++) {
                QueNode_T node = job.mSet[i];
                onReturnFrame(node, MTRUE, MFALSE);
            }
        }
    }

    //(3) clear dummy queue

    //(4) clear drop frame queue
    onProcessDropFrame();

    if (!mpDeliverMgr->waitFlush(MTRUE)) {
        MY_LOGW("request not done");
    };

    //(5) clear all
    mRequestQueue.clear(); //suppose already clear
    mProcessingQueue.clear(); //suppose already clear
    mControls3AList.clear();
    //mImageStorage.uninit();
    //m3AStorage.clear();
    mLastNum = 1;

    FUNCTION_P1S_OUT;
}


#if 0
/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onProcess3AResult(
    MUINT32 magicNum,
    MUINT32 key,
    MUINT32 val
)
{
    MY_LOGD2("%d", magicNum);

    if(magicNum == 0) return;

    m3AStorage.add(magicNum, key, val);
    if(m3AStorage.isCompleted(magicNum)) {
        sp<IPipelineFrame> spFrame = m3AStorage.valueFor(magicNum).spFrame;
        StreamId_T const streamId_OutAppMeta = mOutAppMeta->getStreamId();
        IMetadata appMetadata = m3AStorage.valueFor(magicNum).resultVal;
        lock_and_returnMetadata(spFrame, streamId_OutAppMeta, appMetadata);
        m3AStorage.removeItem(magicNum);

        IStreamBufferSet& rStreamBufferSet  = spFrame->getStreamBufferSet();
        rStreamBufferSet.applyRelease(getNodeId());
    }
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onProcessLMV(
    QueNode_T const &node,
    IMetadata &resultLMV,
    QBufInfo const &deqBuf,
    MINT32 eisMode,
    MUINT8 captureIntent,
    MINT64 exposureTime,
    MUINT32 const index
)
{
    if(node.appFrame == NULL) {
        MY_LOGW("request not exist (%d)", node.magicNum);
        return;
    }
    if (deqBuf.mvOut.size() == 0) {
        MY_LOGW("DeQ Buf is empty, result count (%d)", resultLMV.count());
        return;
    }
    #if SUPPORT_LMV
    if(mpLMV == NULL) {
        MY_LOGW("LMV not ready (%d)", node.magicNum);
        return;
    }

    //One-shot only for a session
    if (EIS_MODE_IS_CALIBRATION_ENABLED(eisMode)) {//Calibration Mode

        LMV_HAL_CONFIG_DATA  config;
        mpEIS->ConfigCalibration(config);
    }

    mpLMV->DoLMVCalc(EIS_PASS_1, deqBuf);
    MBOOL isLastSkipped = CHECK_LAST_FRAME_SKIPPED(mLastSofIdx, node.sofIdx);
    MUINT32 X_INT, Y_INT, X_FLOAT, Y_FLOAT, WIDTH, HEIGHT, ISFROMRRZ;
    MINT32 GMV_X, GMV_Y, MVtoCenterX, MVtoCenterY,iExpTime,ihwTS,ilwTS;
    MUINT32 ConfX,ConfY;
    EIS_STATISTIC_STRUCT  lmvData;
    const MINT64 aTimestamp = deqBuf.mvOut[index].mMetaData.mTimeStamp;

    mpLMV->GetLMVResult(X_INT, X_FLOAT, Y_INT, Y_FLOAT, WIDTH, HEIGHT, MVtoCenterX, MVtoCenterY, ISFROMRRZ);
    mpLMV->GetGmv(GMV_X, GMV_Y, &ConfX, &ConfY);
    mpLMV->GetEisStatistic(&lmvData);

    iExpTime = exposureTime/1000;//(ns to us) << frame duration
    ihwTS = (aTimestamp >> 32)&0xFFFFFFFF; //High word
    ilwTS = (aTimestamp & 0xFFFFFFFF);     //Low word
    IMetadata::IEntry entry(MTK_EIS_REGION);
    entry.push_back(X_INT, Type2Type< MINT32 >());
    entry.push_back(X_FLOAT, Type2Type< MINT32 >());
    entry.push_back(Y_INT, Type2Type< MINT32 >());
    entry.push_back(Y_FLOAT, Type2Type< MINT32 >());
    entry.push_back(WIDTH, Type2Type< MINT32 >());
    entry.push_back(HEIGHT, Type2Type< MINT32 >());
    entry.push_back(MVtoCenterX, Type2Type< MINT32 >());
    entry.push_back(MVtoCenterY, Type2Type< MINT32 >());
    entry.push_back(ISFROMRRZ, Type2Type< MINT32 >());
    entry.push_back(GMV_X, Type2Type< MINT32 >());
    entry.push_back(GMV_Y, Type2Type< MINT32 >());
    entry.push_back(ConfX, Type2Type< MINT32 >());
    entry.push_back(ConfY, Type2Type< MINT32 >());
    entry.push_back(iExpTime, Type2Type< MINT32 >());
    entry.push_back(ihwTS, Type2Type< MINT32 >());
    entry.push_back(ilwTS, Type2Type< MINT32 >());
    resultLMV.update(MTK_EIS_REGION, entry);
    MY_LOGD2("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
              X_INT, X_FLOAT, Y_INT, Y_FLOAT, WIDTH, HEIGHT, MVtoCenterX, MVtoCenterY,
              GMV_X, GMV_Y, ConfX, ConfY, isLastSkipped);

    if (mEisMode != eisMode) {
        if (EIS_MODE_IS_CALIBRATION_ENABLED(eisMode)) {
            //Disable OIS
            mp3A ->send3ACtrl(E3ACtrl_SetEnableOIS, 0, 0);
            MY_LOGD1("[EisHal] mEisMode:%d eisMode: %d => disable OIS \n", mEisMode, eisMode);

        }else if(eisMode == EIS_MODE_OFF)
        {
            //Enable OIS
            mp3A ->send3ACtrl(E3ACtrl_SetEnableOIS, 1, 0);
            MY_LOGD1("[EisHal] mEisMode:%d eisMode: %d => Enable OIS \n", mEisMode, eisMode);
        }
        mEisMode = eisMode;
    }
    if (EIS_MODE_IS_CALIBRATION_ENABLED(eisMode)) {//Calibration Mode
        LMV_HAL_CONFIG_DATA  config;

        MY_LOGD1("[EisHal] captureIntent: %d, eisMode: %d \n", captureIntent, eisMode);
        for(MINT32 i=0; i<EIS_MAX_WIN_NUM; i++)
        {
            config.lmvData.i4LMV_X[i] = lmvData.i4LMV_X[i];
            config.lmvData.i4LMV_Y[i] = lmvData.i4LMV_Y[i];
            config.lmvData.NewTrust_X[i] = lmvData.NewTrust_X[i];
            config.lmvData.NewTrust_Y[i] = lmvData.NewTrust_Y[i];
        }

        //GIS calibration is in preview ONLY!!
        if (captureIntent ==  MTK_CONTROL_CAPTURE_INTENT_PREVIEW)
        {
            mpEIS->DoCalibration(&config, aTimestamp, exposureTime);
        }
    }
    #else
    MY_LOGD2("EIS NotSupport : %d, %d, %d, %d",
        eisMode, captureIntent, exposureTime, index);
    #endif
}


/******************************************************************************
 *
 ******************************************************************************/
void
P1NodeImp::
doNotifyCb(
    MINT32  _msgType,
    MINTPTR _ext1,
    MINTPTR _ext2,
    MINTPTR _ext3
)
{
    FUNCTION_P1_IN;
    MY_LOGD2("P1 doNotifyCb(%d) 0x%zd 0x%zd 0x%zd", _msgType, _ext1, _ext2, _ext3);
    switch(_msgType)
    {
        case IHal3ACb::eID_NOTIFY_3APROC_FINISH:
            if (_ext3 == 0) {
                MY_LOGE("CapParam NULL (%d) 0x%zd 0x%zd", _msgType, _ext1, _ext2);
            } else {
                RequestSet_T set = *(RequestSet_T*)(_ext1);
                CapParam_T param = *(CapParam_T*)(_ext3);
                onSyncBegin(MFALSE, &set, (MUINT32)_ext2, &param);
            }
            break;
        case IHal3ACb::eID_NOTIFY_CURR_RESULT:
            //onProcess3AResult((MUINT32)_ext1,(MUINT32)_ext2, (MUINT32)_ext3); //magic, key, val
            break;
        case IHal3ACb::eID_NOTIFY_VSYNC_DONE:
            onSyncEnd();
            break;
        default:
            break;
    }
    FUNCTION_P1_OUT;
}


/******************************************************************************
 *
 ******************************************************************************/
void
P1NodeImp::
doNotifyDropframe(MUINT magicNum, void* cookie)
{
    MY_LOGI("notify drop frame (%d)", magicNum);
    if (cookie == NULL) {
       MY_LOGE("return cookie is NULL");
       return;
    }

    {
        Mutex::Autolock _l(reinterpret_cast<P1NodeImp*>(cookie)->mDropQueueLock);
        reinterpret_cast<P1NodeImp*>(cookie)->mDropQueue.push_back(magicNum);
    }

    if ((reinterpret_cast<P1NodeImp*>(cookie)->mpDeliverMgr != NULL) &&
        (reinterpret_cast<P1NodeImp*>(cookie)->mpDeliverMgr->runningGet())) {
        MY_LOGI("process drop frame");
        reinterpret_cast<P1NodeImp*>(cookie)->onProcessDropFrame(MTRUE);
    };
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
createStuffBuffer(sp<IImageBuffer> & imageBuffer,
    sp<IImageStreamInfo> const& streamInfo,
    NSCam::MSize::value_type const changeHeight)
{
    MUINT32 stride = streamInfo->getBufPlanes()[0].rowStrideInBytes;
    MSize size = streamInfo->getImgSize();
    // change the height while changeHeight > 0
    if (changeHeight > 0) {
        size.h = changeHeight;
    }
    //
    return createStuffBuffer(imageBuffer,
        streamInfo->getStreamName(), streamInfo->getImgFormat(), size, stride);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
createStuffBuffer(sp<IImageBuffer> & imageBuffer,
    char const* szName, MINT32 format, MSize size, MUINT32 stride)
{
    return mStuffBufMgr.acquireStoreBuffer(imageBuffer, szName, format,
        size, stride, mBurstNum, (mDebugScanLineMask != 0) ? MTRUE : MFALSE);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
destroyStuffBuffer(sp<IImageBuffer> & imageBuffer)
{
    if (imageBuffer == NULL) {
        MY_LOGW("Stuff ImageBuffer not exist");
        return BAD_VALUE;
    }
    return mStuffBufMgr.releaseStoreBuffer(imageBuffer);
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
hardwareOps_start()
{
#if SUPPORT_ISP
    FUNCTION_P1S_IN;
    CAM_TRACE_NAME("P1:hardwareOps_start");

    Mutex::Autolock _l(mHardwareLock);

    setActive(MTRUE);
    setInit(MTRUE);
    mLastSofIdx = P1SOFIDX_NULL_VAL;
    mLastSetNum = 0;

    #if SUPPORT_LMV
    mpLMV = NULL;
    mpEIS = NULL;
    #endif
    #if SUPPORT_VHDR
    mpVhdr = NULL;
    #endif
    #if SUPPORT_LCS
    mpLCS = NULL;
    #endif
    mConfigPort = CONFIG_PORT_NONE;
    mConfigPortNum = 0;

    mReqState = REQ_STATE_CREATED;
    mFirstReceived = MFALSE;
    //
    {
        Mutex::Autolock _ll(mFrameSetLock);
        mFrameSetAlready = MFALSE;
    }
    //
    mTagReq.clear();
    mTagSet.clear();
    mTagEnq.clear();
    mTagDeq.clear();
    mDequeThreadProfile.reset();
    //mImageStorage.init(mLogLevel);

    //
    CAM_TRACE_BEGIN("P1:CamIO-init");
    #ifdef USING_MTK_LDVT
    mpCamIO = INormalPipe::createInstance(getOpenId(), "iopipeUseTM");
    #else
    mpCamIO = INormalPipe::createInstance(getOpenId(), getNodeName());
    #endif
    MY_LOGI1("mpCamIO->init +++");
    if(!mpCamIO || !mpCamIO->init())
    {
        MY_LOGE("hardware init fail");
        return DEAD_OBJECT;
    }
    MY_LOGI1("mpCamIO->init ---");
    CAM_TRACE_END();

#if SUPPORT_LCS
    if (mEnableLCSO) {
        CAM_TRACE_BEGIN("P1:lcs-config");
        mpLCS = MAKE_LcsHal(LOG_TAG, getOpenId());
        if(mpLCS == NULL)
        {
            MY_LOGE("mpLCS is NULL");
            return DEAD_OBJECT;
        }
        if( mpLCS->Init() != LCS_RETURN_NO_ERROR)
        {
            mpLCS->DestroyInstance(LOG_TAG);
            mpLCS = NULL;
        }
        CAM_TRACE_END();
    }
#endif
#if SUPPORT_VHDR
    CAM_TRACE_BEGIN("P1:vhdr-config");
    mpVhdr = VHdrHal::CreateInstance(LOG_TAG, getOpenId());
    if(mpVhdr == NULL)
    {
        MY_LOGE("mpVhdr is NULL");
        return DEAD_OBJECT;
    }
    if( mpVhdr->Init(mSensorParams.vhdrMode) != VHDR_RETURN_NO_ERROR)
    {
        //mpVhdr->DestroyInstance(LOG_TAG); // instance always exist until process kill
        mpVhdr = NULL;
    }
    CAM_TRACE_END();
#endif

#if SUPPORT_LMV
    sp<IImageBuffer> pEISOBuf = NULL;
    LMV_HAL_CONFIG_DATA  config;
    if (mEnableEISO) {
        CAM_TRACE_BEGIN("P1:eis-config");
        mpLMV = LMVHal::CreateInstance(LOG_TAG, getOpenId());
        mpLMV->Init();
        config.sensorType = MAKE_HalSensorList()->queryType(getOpenId());
        mpEIS = EisHal::CreateInstance(LOG_TAG, getOpenId());
        mpEIS->Init();
        CAM_TRACE_END();

        // [TODO] get pEISOBuf from EIS
        mpLMV->GetBufLMV(pEISOBuf);
    }
#endif

    //
    IHalSensor::ConfigParam sensorCfg =
    {
        (MUINT)getOpenId(),                 /* index            */
        mSensorParams.size,                 /* crop */
        mSensorParams.mode,                 /* scenarioId       */
        0,                                  /* isBypassScenario */
        1,                                  /* isContinuous     */
#if SUPPORT_VHDR
        mSensorParams.vhdrMode,             /* vHDROn mode          */
#else
        MFALSE,                             /* vHDROn mode          */
#endif
        #ifdef USING_MTK_LDVT
        1,
        #else
        mSensorParams.fps,                  /* framerate        */
        #endif
        0,                                  /* two pixel on     */
        0,                                  /* debugmode        */
    };

    vector<IHalSensor::ConfigParam> vSensorCfg;
    vSensorCfg.push_back(sensorCfg);

    //
    vector<portInfo> vPortInfo;
    if (mvStreamImg[STREAM_IMG_OUT_FULL] != NULL)
    {
        portInfo OutPort(
                PORT_IMGO,
                (EImageFormat)mvStreamImg[STREAM_IMG_OUT_FULL]->getImgFormat(),
                mvStreamImg[STREAM_IMG_OUT_FULL]->getImgSize(),
                MRect(MPoint(0,0), mSensorParams.size),
                mvStreamImg[STREAM_IMG_OUT_FULL]->getBufPlanes().itemAt(0).rowStrideInBytes,
                0, //pPortCfg->mStrideInByte[1],
                0, //pPortCfg->mStrideInByte[2],
                0, // pureraw
                MTRUE);              //packed

        vPortInfo.push_back(OutPort);
        mConfigPort |= CONFIG_PORT_IMGO;
        mConfigPortNum ++;
    } else if (mvStreamImg[STREAM_IMG_OUT_OPAQUE] != NULL) {
        portInfo OutPort(
                PORT_IMGO,
                (EImageFormat)mRawFormat,
                mSensorParams.size,
                MRect(MPoint(0,0), mSensorParams.size),
                mRawStride,
                0, //pPortCfg->mStrideInByte[1],
                0, //pPortCfg->mStrideInByte[2],
                0, // pureraw
                MTRUE);              //packed

        vPortInfo.push_back(OutPort);
        mConfigPort |= CONFIG_PORT_IMGO;
        mConfigPortNum ++;
    }
    //
    if (mvStreamImg[STREAM_IMG_OUT_RESIZE] != NULL)
    {
        portInfo OutPort(
                PORT_RRZO,
                (EImageFormat)mvStreamImg[STREAM_IMG_OUT_RESIZE]->getImgFormat(),
                mvStreamImg[STREAM_IMG_OUT_RESIZE]->getImgSize(),
                MRect(MPoint(0,0), mSensorParams.size),
                mvStreamImg[STREAM_IMG_OUT_RESIZE]->getBufPlanes().itemAt(0).rowStrideInBytes,
                0, //pPortCfg->mStrideInByte[1],
                0, //pPortCfg->mStrideInByte[2],
                0, // pureraw
                MTRUE);              //packed

        vPortInfo.push_back(OutPort);
        mConfigPort |= CONFIG_PORT_RRZO;
        mConfigPortNum ++;
    }

    if (mEnableLCSO && mvStreamImg[STREAM_IMG_OUT_LCS] != NULL)
    {
        portInfo OutPort(
                PORT_LCSO,
                (EImageFormat)mvStreamImg[STREAM_IMG_OUT_LCS]->getImgFormat(),
                mvStreamImg[STREAM_IMG_OUT_LCS]->getImgSize(),
                MRect(MPoint(0,0),  mvStreamImg[STREAM_IMG_OUT_LCS]->getImgSize()),
                mvStreamImg[STREAM_IMG_OUT_LCS]->getBufPlanes().itemAt(0).rowStrideInBytes,
                0, //pPortCfg->mStrideInByte[1],
                0, //pPortCfg->mStrideInByte[2],
                0, // pureraw
                MTRUE);              //packed

        vPortInfo.push_back(OutPort);
        mConfigPort |= CONFIG_PORT_LCSO;
        mConfigPortNum ++;
    }

    #if SUPPORT_LMV
    if (pEISOBuf != NULL)
    {
        portInfo OutPort(
                PORT_EISO,
                (EImageFormat)pEISOBuf->getImgFormat(),
                pEISOBuf->getImgSize(),
                MRect(MPoint(0,0),  pEISOBuf->getImgSize()),
                pEISOBuf->getBufStridesInBytes(0),
                0, //pPortCfg->mStrideInByte[1],
                0, //pPortCfg->mStrideInByte[2],
                0, // pureraw
                MTRUE);              //packed

        vPortInfo.push_back(OutPort);
        mConfigPort |= CONFIG_PORT_EISO;
        mConfigPortNum ++;
    }
    #endif

    MBOOL bDynamicRawType = MTRUE;  // true:[ON] ; false:[OFF]
    QInitParam halCamIOinitParam(
               0,                           /*sensor test pattern */
               10,                          /* bit depth*/
               vSensorCfg,
               vPortInfo,
               bDynamicRawType);
    halCamIOinitParam.m_Func.Raw = 0;
    if (((mRawOption & (1 << EPipe_PROCESSED_RAW)) > 0)
        || (mDisableFrontalBinning)
        ) {
        // In DualPD case, the frontal binning will be decided by driver.
        // therefore, it do not need to adjust for this case.
        halCamIOinitParam.m_Func.Bits.OFF_BIN = 1;
        //halCamIOinitParam.m_bOffBin = MTRUE;
    }
    //
    halCamIOinitParam.m_Func.Bits.DATA_PATTERN =
        (mEnableDualPD) ? (EPipe_Dual_pix) : (EPipe_Normal);
    halCamIOinitParam.m_DropCB = doNotifyDropframe;
    halCamIOinitParam.m_returnCookie = this;
    MY_LOGD1("ConfigPipe-Func(0x%X)", halCamIOinitParam.m_Func.Raw);

    // enable frame sync
    if(mEnableFrameSync){
      MY_LOGI("P1 node(%d) is in synchroized mode", getOpenId());
      halCamIOinitParam.m_bN3D = MTRUE;
    }else{
      halCamIOinitParam.m_bN3D = MFALSE;
    }

    //
    MSize binInfoSize = mSensorParams.size;
    mIsBinEn = false;
    CAM_TRACE_BEGIN("P1:CamIO-configPipe");
    MY_LOGI1("mpCamIO->configPipe +++");
    if(!mpCamIO->configPipe(halCamIOinitParam, mBurstNum))
    {
        MY_LOGE("hardware config pipe fail");
        return BAD_VALUE;
    } else {
        if (mpCamIO->sendCommand(ENPipeCmd_GET_BIN_INFO,
            (MINTPTR)&binInfoSize.w, (MINTPTR)&binInfoSize.h, (MINTPTR)NULL)) {
            if (binInfoSize.w < mSensorParams.size.w ||
                binInfoSize.h < mSensorParams.size.h) {
                mIsBinEn = true;
            }
        }
    }
    MY_LOGI1("mpCamIO->configPipe ---");
    CAM_TRACE_END();

#if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-create-setSensorMode");
    mp3A = MAKE_Hal3A(getOpenId(), getNodeName());
    if(mp3A == NULL) {
        MY_LOGE("mp3A is NULL");
        return DEAD_OBJECT;
    }
    mp3A->setSensorMode(mSensorParams.mode);
    CAM_TRACE_END();
#endif

    #if SUPPORT_LMV
    if ((mpLMV != NULL) && (mpLMV->GetLMVSupportInfo(getOpenId())))
    {
        mpLMV->ConfigLMV(config);
    }
    #endif

    #if SUPPORT_VHDR
    if(mpVhdr)
    {
        VHDR_HAL_CONFIG_DATA vhdrConfig;
        vhdrConfig.cameraVer = VHDR_CAMERA_VER_3;
        mpVhdr->ConfigVHdr(vhdrConfig);
    }
    #endif

    #if SUPPORT_LCS
    if(mpLCS)
    {
        LCS_HAL_CONFIG_DATA lcsConfig;
        lcsConfig.cameraVer = LCS_CAMERA_VER_3;
        if (mvStreamImg[STREAM_IMG_OUT_LCS] != NULL) {
            lcsConfig.lcsOutWidth =
                mvStreamImg[STREAM_IMG_OUT_LCS]->getImgSize().w;
            lcsConfig.lcsOutHeight =
                mvStreamImg[STREAM_IMG_OUT_LCS]->getImgSize().h;
        } else {
            MY_LOGI("LCS enable but no LCS stream info");
            lcsConfig.lcsOutWidth = 0;
            lcsConfig.lcsOutHeight = 0;
        }
        mpLCS->ConfigLcsHal(lcsConfig);
    }
    #endif

    #if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-config");
    MY_LOGI1("mp3A->config +++");
    if (mp3A) {
        if (mEnableDualPD) {
            mp3A ->send3ACtrl(E3ACtrl_SetEnablePBin, 1, 0);
        }
        mp3A->attachCb(IHal3ACb::eID_NOTIFY_3APROC_FINISH, this);
        mp3A->attachCb(IHal3ACb::eID_NOTIFY_CURR_RESULT, this);
        mp3A->attachCb(IHal3ACb::eID_NOTIFY_VSYNC_DONE, this);
        mp3A->config((MINT32)(mBurstNum > 1) ? mBurstNum : 1);
        //m3AProcessedDepth = mp3A->getCapacity();
    }
    MY_LOGI1("mp3A->config ---");
    CAM_TRACE_END();
    #endif
    //
    if (mEnableCaptureFlow) {
        Mutex::Autolock _l(mStartCaptureLock);
        mStartCaptureState = START_CAP_STATE_WAIT_REQ;
        mStartCaptureType = E_CAPTURE_NORMAL;
        mStartCaptureExp = 0;
        mStartCaptureInitEnque = MFALSE;
        MY_LOGI("EnableCaptureFlow(%d) return", mEnableCaptureFlow);
        return OK;
    }

    #if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-start");
    MY_LOGI1("mp3A->start +++");
    if (mp3A) {
        mp3A->start();
    }
    MY_LOGI1("mp3A->start ---");
    CAM_TRACE_END();
    #endif


    //register 3 real frames and 3 dummy frames
    //[TODO] in case that delay frame is above 3 but memeory has only 3, pending aquirefromPool
    CAM_TRACE_BEGIN("Create nodes");
    {
        QueJob_T job(mBurstNum);
        for (MUINT8 n = 0; n < mBurstNum; n++) {
            createNode(NULL, &job, &mProcessingQueue, &mProcessingQueueLock,
                                 &mControls3AList, &mControls3AListLock);
        }
        MERROR status = hardwareOps_enque(
            mProcessingQueue.editItemAt(mProcessingQueue.size()-1));
        if (status != OK) {
            MY_LOGE("hardware init-enque fail (%d)", status);
            return status;
        }
        // Due to pipeline latency, delay frame should be above 3
        // if delay frame is more than 3, add node to mProcessingQueue here.
        for (int i = 0; i < mDelayframe - mNumHardwareBuffer; i++) {
            for (MUINT8 n = 0; n < mBurstNum; n++) {
                createNode(&mControls3AList, &mControls3AListLock);
            }
        }
        mQueueJob.mMaxNum = mBurstNum;
        mQueueJob.mSet.setCapacity(mBurstNum);
        mQueueJob.mSet.clear();
    }
    CAM_TRACE_END();
    //
    CAM_TRACE_BEGIN("P1:CamIO-start");
    #if 1
    MY_LOGI1("mpCamIO->start +++");
    if(!mpCamIO->start()) {
        MY_LOGE("hardware start fail");
        return BAD_VALUE;
    }
    MY_LOGI1("mpCamIO->start ---");
    #endif
    CAM_TRACE_END();


    {
        Mutex::Autolock _l(mThreadLock);
        setReady(MTRUE);
        mThreadCond.broadcast();
    }

    MY_LOGI("Cam::%d BinSize:%dx%d BinEn:%d ConfigPort[%d]:0x%x",
        getOpenId(), binInfoSize.w, binInfoSize.h, mIsBinEn,
        mConfigPortNum, mConfigPort);

    FUNCTION_P1S_OUT;

    return OK;
#else
    return OK;
#endif

}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
hardwareOps_capture()
{
#if SUPPORT_ISP
    FUNCTION_P1S_IN;
    //
    Mutex::Autolock _l(mHardwareLock);
    //
    MINT32 num = 0;
    MBOOL isManualCap = MFALSE;
    //
    if (mEnableCaptureFlow) {
        Mutex::Autolock _l(mStartCaptureLock);
        mStartCaptureState = START_CAP_STATE_WAIT_CB;
    }
    //
#if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-startCapture");
    if (mp3A) {
        MINT32 type = ESTART_CAP_NORMAL;
        {
            Mutex::Autolock _ll(mFrameSetLock);
            type = mp3A->startCapture(mControls3AList);//mp3A->start();
            mFrameSetAlready = MTRUE;
        }
        if (type != ESTART_CAP_NORMAL) {
            isManualCap = MTRUE;
            MY_LOGI("capture in manual flow %d", type);
        }
    }
    CAM_TRACE_END();
#endif
    //
    {
        MERROR status = hardwareOps_enque(
            mProcessingQueue.editItemAt(mProcessingQueue.size()-1));
        if (status != OK) {
            MY_LOGE("hardware cap-init-enque fail (%d)", status);
            return status;
        }
        Mutex::Autolock _l(mStartCaptureLock);
        mStartCaptureInitEnque = MFALSE;
    }
    //
    if (!isManualCap) {
        CAM_TRACE_BEGIN("Cap Normal EnQ");
        {
            QueJob_T job(mBurstNum);
            {
                Mutex::Autolock _l(mRequestQueueLock);
                Que_T::iterator it = mRequestQueue.begin();
                job = *it;
                mRequestQueue.erase(it);
            }
            MERROR status = onProcessEnqueFrame(job);
            if (status != OK) {
                MY_LOGE("hardware cap-enque-normal fail (%d)", status);
                return status;
            }
            num = job.mSet[0].magicNum;
        }
        CAM_TRACE_END();
        //
        if (num > 0) {
            Mutex::Autolock _l(mControls3AListLock);
            mLastSetNum = num;
            mTagSet.set(num);
        }
    }
    //
    CAM_TRACE_BEGIN("P1:CamIO-start");
#if 1
    MY_LOGD2("mpCamIO->start +++");
    if(!mpCamIO->start()) {
        MY_LOGE("hardware start fail");
        return BAD_VALUE;
    }
    MY_LOGD2("mpCamIO->start ---");
#endif
    CAM_TRACE_END();
    //
    if (isManualCap) {
        CAM_TRACE_BEGIN("Cap Manual EnQ");
        {
            QueJob_T job(mBurstNum);
            {
                Mutex::Autolock _l(mRequestQueueLock);
                Que_T::iterator it = mRequestQueue.begin();
                job = *it;
                mRequestQueue.erase(it);
            }
            MERROR status = onProcessEnqueFrame(job);
            if (status != OK) {
                MY_LOGE("hardware cap-enque-manual fail (%d)", status);
                return status;
            }
            num = job.mSet[0].magicNum;
        }
        CAM_TRACE_END();
        //
        if (num > 0) {
            Mutex::Autolock _l(mControls3AListLock);
            mLastSetNum = num;
            mTagSet.set(num);
        }
    }
    //
    {
        Mutex::Autolock _l(mThreadLock);
        setReady(MTRUE);
        mThreadCond.broadcast();
    }
    //
    MY_LOGI("Cam::%d BinEn:%d ConfigPort[%d]:0x%x",
        getOpenId(), mIsBinEn,
        mConfigPortNum, mConfigPort);

    FUNCTION_P1S_OUT;

    return OK;
#else
    return OK;
#endif
}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
generateAppMeta(QueNode_T & node, MetaSet_T const &result3A,
    QBufInfo const &deqBuf, IMetadata &appMetadata, MUINT32 const index)
{
    if (node.appFrame == NULL) {
        MY_LOGW("pipeline frame is NULL (%d)", node.magicNum);
        return;
    }
    sp<IPipelineFrame> request = node.appFrame;

    //[3A/Flash/sensor section]
    appMetadata = result3A.appMeta;

    //[request section]
    // android.request.frameCount
    {
        IMetadata::IEntry entry(MTK_REQUEST_FRAME_COUNT);
        entry.push_back( request->getFrameNo(), Type2Type< MINT32 >());
        appMetadata.update(MTK_REQUEST_FRAME_COUNT, entry);
    }
    // android.request.metadataMode
    {
        IMetadata::IEntry entry(MTK_REQUEST_METADATA_MODE);
        entry.push_back(MTK_REQUEST_METADATA_MODE_FULL, Type2Type< MUINT8 >());
        appMetadata.update(MTK_REQUEST_METADATA_MODE, entry);
    }

    //[sensor section]
    // android.sensor.timestamp
    {
        MINT64 frame_duration = 0;
        //IMetadata::IEntry entry(MTK_SENSOR_FRAME_DURATION);
        //should get from control.
        #if 1 // modify timestamp
        frame_duration = node.frameExpDuration;
        #endif
        MINT64 timestamp =
            deqBuf.mvOut[index].mMetaData.mTimeStamp - frame_duration;
        IMetadata::IEntry entry(MTK_SENSOR_TIMESTAMP);
        entry.push_back(timestamp, Type2Type< MINT64 >());
        appMetadata.update(MTK_SENSOR_TIMESTAMP, entry);
    }

    //[sensor section]
    // android.sensor.rollingshutterskew
    // [TODO] should query from sensor
    {
        IMetadata::IEntry entry(MTK_SENSOR_ROLLING_SHUTTER_SKEW);
        entry.push_back(33000000, Type2Type< MINT64 >());
        appMetadata.update(MTK_SENSOR_ROLLING_SHUTTER_SKEW, entry);
    }


}


/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
generateAppTagIndex(IMetadata &appMetadata, IMetadata &appTagIndex)
{
    IMetadata::IEntry entryTagIndex(MTK_P1NODE_METADATA_TAG_INDEX);

    for (size_t i = 0; i < appMetadata.count(); ++i) {
        IMetadata::IEntry entry = appMetadata.entryAt(i);
        entryTagIndex.push_back((MINT32)entry.tag(), Type2Type<MINT32>());
    }

    if (OK != appTagIndex.update(entryTagIndex.tag(), entryTagIndex)) {
        MY_LOGE("fail to update index");
    }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
generateHalMeta(MetaSet_T const &result3A, QBufInfo const &deqBuf,
    IMetadata const &resultLMV, IMetadata const &inHalMetadata,
    IMetadata &halMetadata, MUINT32 const index)
{
    if (deqBuf.mvOut.size() == 0) {
        MY_LOGE("deqBuf is empty");
        return;
    }

    //3a tuning
    halMetadata = result3A.halMeta;

    //eis
    halMetadata += resultLMV;

    // in hal meta
    halMetadata += inHalMetadata;

    {
        IMetadata::IEntry entry(MTK_P1NODE_SENSOR_MODE);
        entry.push_back(mSensorParams.mode, Type2Type< MINT32 >());
        halMetadata.update(MTK_P1NODE_SENSOR_MODE, entry);
    }

    {
        IMetadata::IEntry entry(MTK_P1NODE_SENSOR_VHDR_MODE);
        entry.push_back(mSensorParams.vhdrMode, Type2Type< MINT32 >());
        halMetadata.update(MTK_P1NODE_SENSOR_VHDR_MODE, entry);
    }
    //rrzo
    for (size_t i = index; i < deqBuf.mvOut.size(); i += mBurstNum) {
        if (deqBuf.mvOut[i].mPortID.index == PORT_RRZO.index) {
            //crop region
            {
                MRect crop = deqBuf.mvOut[i].mMetaData.mCrop_s;
                if (mIsBinEn) {
                    BIN_REVERT(crop.p.x);
                    BIN_REVERT(crop.p.y);
                    BIN_REVERT(crop.s.w);
                    BIN_REVERT(crop.s.h);
                }
                IMetadata::IEntry entry(MTK_P1NODE_SCALAR_CROP_REGION);
                entry.push_back(crop, Type2Type< MRect >());
                halMetadata.update(MTK_P1NODE_SCALAR_CROP_REGION, entry);
            }
            //
            {
                IMetadata::IEntry entry(MTK_P1NODE_DMA_CROP_REGION);
                entry.push_back(deqBuf.mvOut[i].mMetaData.mCrop_d, Type2Type< MRect >());
                halMetadata.update(MTK_P1NODE_DMA_CROP_REGION, entry);
            }
            //
            {
                IMetadata::IEntry entry(MTK_P1NODE_RESIZER_SIZE);
                entry.push_back(deqBuf.mvOut[i].mMetaData.mDstSize, Type2Type< MSize >());
                halMetadata.update(MTK_P1NODE_RESIZER_SIZE, entry);
            }
            /*
            MY_LOGD("[CropInfo] CropS(%d, %d, %dx%d) "
                "CropD(%d, %d, %dx%d) DstSize(%dx%d)",
                deqBuf.mvOut[i].mMetaData.mCrop_s.leftTop().x,
                deqBuf.mvOut[i].mMetaData.mCrop_s.leftTop().y,
                deqBuf.mvOut[i].mMetaData.mCrop_s.size().w,
                deqBuf.mvOut[i].mMetaData.mCrop_s.size().h,
                deqBuf.mvOut[i].mMetaData.mCrop_d.leftTop().x,
                deqBuf.mvOut[i].mMetaData.mCrop_d.leftTop().y,
                deqBuf.mvOut[i].mMetaData.mCrop_d.size().w,
                deqBuf.mvOut[i].mMetaData.mCrop_d.size().h,
                deqBuf.mvOut[i].mMetaData.mDstSize.w,
                deqBuf.mvOut[i].mMetaData.mDstSize.h);
            */
            break;
        } else {
            continue;
        }
    }

}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
setupNode(
    QueNode_T &node,
    QBufInfo &info
)
{
    FUNCTION_P1_IN;
#if SUPPORT_ISP
    MUINT32 out = 0;
    //
    sp<IImageStreamInfo> pImgStreamInfo = NULL;
    sp<IImageBuffer> pImgBuf = NULL;
    //
    NSCam::NSIoPipe::PortID portID = NSCam::NSIoPipe::PortID();
    MSize dstSize = MSize(0, 0);
    MRect cropRect = MRect(MPoint(0, 0), MSize(0, 0));
    MUINT32 rawOutFmt = 0;
    //
    STREAM_IMG streamImg = STREAM_IMG_NUM;
    //
    if ((node.reqType == REQ_TYPE_UNKNOWN) ||
        (node.reqType == REQ_TYPE_REDO) ||
        (node.reqType == REQ_TYPE_YUV)) {
        MY_LOGW("mismatch node type " P1INFO_NODE_STR, P1INFO_NODE_VAR(node));
        return BAD_VALUE;
    }
    //
    CAM_TRACE_FMT_BEGIN("setupNode(%d)", node.magicNum);
    //
    #if (IS_P1_LOGD)
    android::String8 strInfo("");
    #endif
    //
    for (out = 0; out < REQ_OUT_MAX; out++) {
        if (!(IS_OUT(out, node.reqOutSet))) {
            continue;
        }
        CAM_TRACE_FMT_BEGIN("REQ_OUT_%d", out);
        //pBufPool = NULL;
        pImgStreamInfo = NULL;
        //pImgStreamBuf = NULL;
        pImgBuf = NULL;
        switch (out) {
            case REQ_OUT_LCSO:
            case REQ_OUT_LCSO_STUFF:
                streamImg = STREAM_IMG_OUT_LCS;
                portID = PORT_LCSO;
                dstSize = mvStreamImg[streamImg]->getImgSize();
                cropRect = MRect(MPoint(0, 0), mvStreamImg[streamImg]->getImgSize());
                rawOutFmt = (EPipe_PROCESSED_RAW);
                if (out == REQ_OUT_LCSO_STUFF) {
                    // use stuff buffer with height:1
                    //dstSize.h = P1_STUFF_BUF_HEIGHT(MTRUE, mConfigPort); .// TODO LCSO can use height 1 ???
                    cropRect.s = dstSize;
                }
                break;

            case REQ_OUT_RESIZER:
            case REQ_OUT_RESIZER_STUFF:
                streamImg = STREAM_IMG_OUT_RESIZE;
                portID = PORT_RRZO;
                dstSize = node.dstSize_resizer;
                cropRect = node.cropRect_resizer;
                rawOutFmt = (EPipe_PROCESSED_RAW);
                if (out == REQ_OUT_RESIZER_STUFF) {
                    // use stuff buffer with height:1
                    dstSize.h = P1_STUFF_BUF_HEIGHT(MTRUE, mConfigPort);
                    cropRect.s = dstSize;
                }
                break;

            case REQ_OUT_FULL_PROC:
            case REQ_OUT_FULL_PURE:
            case REQ_OUT_FULL_OPAQUE:
            case REQ_OUT_FULL_STUFF:
                streamImg = STREAM_IMG_OUT_FULL;
                if (out == REQ_OUT_FULL_OPAQUE || (out == REQ_OUT_FULL_STUFF &&
                    node.streamBufImg[STREAM_IMG_OUT_OPAQUE].bExist)) {
                    streamImg = STREAM_IMG_OUT_OPAQUE;
                } else if (mvStreamImg[STREAM_IMG_OUT_FULL] != NULL) {
                    streamImg = STREAM_IMG_OUT_FULL;
                } else if (mvStreamImg[STREAM_IMG_OUT_OPAQUE] != NULL) {
                    streamImg = STREAM_IMG_OUT_OPAQUE;
                };
                portID = PORT_IMGO;
                dstSize = node.dstSize_full;
                cropRect = node.cropRect_full;
                rawOutFmt = (MUINT32)(((out == REQ_OUT_FULL_PROC) ||
                            ((out == REQ_OUT_FULL_STUFF) &&
                            (mRawDefType == EPipe_PROCESSED_RAW))) ?
                            (EPipe_PROCESSED_RAW) : (EPipe_PURE_RAW));
                if (out == REQ_OUT_FULL_STUFF) {
                    if (mEnableDumpRaw && node.reqType == REQ_TYPE_NORMAL) {
                        // If user wants to dump pure raw, Full Raw can not use height 1
                    } else {
                        dstSize.h = P1_STUFF_BUF_HEIGHT(MFALSE, mConfigPort);
                    }
                    cropRect.s = dstSize;
                };
                break;

            //case REQ_OUT_MAX:
            // for this loop, all cases should be listed
            // and the default is an unreachable path
            /*
            default:
                continue;
            */
        };
        //
        if (streamImg < STREAM_IMG_NUM) {
            pImgStreamInfo = mvStreamImg[streamImg];
        } else {
            MY_LOGW("cannot find the StreamImg num:%d out:%d "
                "streamImg:%d", node.magicNum, out, streamImg);
            return BAD_VALUE;
        }
        if (pImgStreamInfo == NULL) {
            MY_LOGW("cannot find the ImgStreamInfo num:%d out:%d "
                "streamImg:%d", node.magicNum, out, streamImg);
            return BAD_VALUE;
        }
        //
        MERROR err = OK;
        if (out == REQ_OUT_FULL_STUFF || out == REQ_OUT_RESIZER_STUFF ||
            out == REQ_OUT_LCSO_STUFF) {
            err = stuffImageGet(node, streamImg, dstSize, pImgBuf);
        } else if (node.reqType == REQ_TYPE_INITIAL) {
            // the initial node with the pool, it do not use the stuff buffer
            err = poolImageGet(node, streamImg, pImgBuf);
        } else { // REQ_TYPE_NORMAL
            if (OK != frameImageGet(node, streamImg, pImgBuf)) {
                #if 1 // keep en-queue/de-queue processing
                MY_LOGI("(%d) lockImageBuffer cannot get StreamId=%#" PRIxPTR ,
                    node.magicNum, pImgStreamInfo->getStreamId());
                if (out == REQ_OUT_LCSO ||
                    (mEnableDumpRaw && (out == REQ_OUT_FULL_PURE ||
                    out == REQ_OUT_FULL_PROC || out == REQ_OUT_FULL_OPAQUE))) {
                    MY_LOGI("keep the output size out:%d", out);
                } else {
                    dstSize.h = P1_STUFF_BUF_HEIGHT(
                        (out == REQ_OUT_RESIZER ? MTRUE : MFALSE), mConfigPort);
                    cropRect.s.h = dstSize.h;
                };
                err = stuffImageGet(node, streamImg, dstSize, pImgBuf);
                if (out == REQ_OUT_RESIZER) {
                    node.expRec |= EXP_REC(EXP_EVT_NOBUF_RRZO);
                } else if (out == REQ_OUT_LCSO) {
                    node.expRec |= EXP_REC(EXP_EVT_NOBUF_LCSO);
                } else {
                    node.expRec |= EXP_REC(EXP_EVT_NOBUF_IMGO);
                }
                #else
                MY_LOGE("(%d) frameImageGet failed on StreamId=0x%X",
                    node.magicNum, pImgStreamInfo->getStreamId());
                err = BAD_VALUE;
                #endif
            };
        }
        //
        if ((pImgBuf == NULL) || (err != OK)) {
            MY_LOGE("(%d) Cannot get image buffer : (%d)", node.magicNum, err);
            return BAD_VALUE;
        }
        //
        #if (IS_P1_LOGD)
        if (mLogLevel > 0) {
            strInfo += String8::format(
                "[%s](%d)"
                "[Buf](%dx%d-%d-%d P:%p V:%p)"
                "[Crop](%d,%d-%dx%d)(%dx%d) ",
                ((portID.index == PORT_RRZO.index) ? "RRZ" : "IMG"), out,
                pImgBuf->getImgSize().w, pImgBuf->getImgSize().h,
                (int)pImgBuf->getBufStridesInBytes(0),
                (int)pImgBuf->getBufSizeInBytes(0),
                (void*)pImgBuf->getBufPA(0), (void*)pImgBuf->getBufVA(0),
                cropRect.p.x, cropRect.p.y, cropRect.s.w, cropRect.s.h,
                dstSize.w, dstSize.h
                );
        }
        #endif
        NSCam::NSIoPipe::NSCamIOPipe::BufInfo rBufInfo(
            portID,
            pImgBuf.get(),
            dstSize,
            cropRect,
            node.magicNum,
            node.sofIdx,
            rawOutFmt);
        info.mvOut.push_back(rBufInfo);
        CAM_TRACE_FMT_END();
    }; // end of the loop for each out
    //
    {

        MSize dstSizeNone = MSize(0, 0);
        MRect cropRectNone = MRect(MPoint(0, 0), MSize(0, 0));
        // EISO
        #if SUPPORT_LMV
        if (IS_PORT(CONFIG_PORT_EISO, mConfigPort)) {
            sp<IImageBuffer> pImgBuf = NULL;
            // [TODO] get pImgBuf from LMV
            mpLMV->GetBufLMV(pImgBuf);
            //MY_LOGD1("GetBufLMV: %p ",pImgBuf->getBufVA(0));
            node.buffer_eiso = pImgBuf;
            #if (IS_P1_LOGD)
            if (mLogLevel > 0) {
                if (pImgBuf != NULL) {
                    strInfo += String8::format("[LMV](P:%p V:%p) ",
                        (void*)pImgBuf->getBufPA(0),
                        (void*)pImgBuf->getBufVA(0));
                }
            }
            #endif
            NSCam::NSIoPipe::NSCamIOPipe::BufInfo rBufInfo(
                PORT_EISO,
                pImgBuf.get(),
                pImgBuf->getImgSize(),
                MRect(MPoint(0, 0), pImgBuf->getImgSize()),
                node.magicNum,
                node.sofIdx);
            info.mvOut.push_back(rBufInfo);
        }
        #endif
    }
    mTagEnq.set(node.magicNum);
    P1_LOGD("[P1::ENQ] " P1INFO_NODE_STR " %s",
        P1INFO_NODE_VAR(node), strInfo.string());
    //
    CAM_TRACE_FMT_END();
#endif
    FUNCTION_P1_OUT;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
hardwareOps_enque(
    QueJob_T &job,
    MBOOL toPush
)
{
    FUNCTION_P1_IN;
    CAM_TRACE_NAME("P1:enque");

    if (!getActive()) {
        return BAD_VALUE;
    }
#if SUPPORT_ISP
    QBufInfo enBuf;
    for (size_t i = 0; i < job.mSet.size(); i++) {
        MY_LOGD2("job(%d)(%zu/%zu)", job.mFirstMagicNum, i, job.mSet.size());
        if (OK != setupNode(job.mSet.editItemAt(i), enBuf)) {
            MY_LOGE("setup enque node fail");
            return BAD_VALUE;
        }
        if (i == 0 && job.mSet.editItemAt(i).reqType == REQ_TYPE_NORMAL) {
            enBuf.mShutterTimeNs = job.mSet.editItemAt(i).frameExpDuration;
        }
        job.mSet.editItemAt(i).exeState = EXE_STATE_PROCESSING;
    }
    //
    if (mEnableCaptureFlow && (!getReady())) {
        Mutex::Autolock _l(mStartCaptureLock);
        MY_LOGD2("StartCaptureState(%d)", mStartCaptureState);
        if (mStartCaptureState == START_CAP_STATE_WAIT_CB &&
            (!mStartCaptureInitEnque)) {
            CAM_TRACE_BEGIN("StartCapture wait");
            mStartCaptureCond.wait(mStartCaptureLock);
            CAM_TRACE_END();
        }
        if (!mStartCaptureInitEnque) {
            job.mSet.editItemAt(0).capType = mStartCaptureType;
            job.mSet.editItemAt(0).frameExpDuration = mStartCaptureExp;
            enBuf.mShutterTimeNs = mStartCaptureExp;
        }
    }
    //
    if (toPush) {
        Mutex::Autolock _l(mProcessingQueueLock);
        mProcessingQueue.push_back(job);
        MY_LOGD2("Push(%d) to ProQ(%zu)",
            job.mFirstMagicNum, mProcessingQueue.size());
    }
    //
    CAM_TRACE_FMT_BEGIN("P1:CamIO-enque #(%d)(%d,%d)", job.mSet[0].magicNum,
        P1GET_FRM_NUM(job.mSet[0].appFrame), P1GET_REQ_NUM(job.mSet[0].appFrame)
        );
    MY_LOGD2("mpCamIO->enque +++");
    if(!mpCamIO->enque(enBuf)) {
        MY_LOGE("enque fail");
        CAM_TRACE_FMT_END();
        if (toPush) {
            Mutex::Autolock _l(mProcessingQueueLock);
            Que_T::iterator it = mProcessingQueue.begin();
            for (; it != mProcessingQueue.end(); it++) {
                if ((*it).mFirstMagicNum == job.mFirstMagicNum) {
                    break;
                }
            }
            if (it != mProcessingQueue.end()) {
                mProcessingQueue.erase(it);
            }
            MY_LOGD2("Erase(%d) from ProQ(%zu)",
                job.mFirstMagicNum, mProcessingQueue.size());
        }
        return BAD_VALUE;
    }
    MY_LOGD2("mpCamIO->enque ---");
    CAM_TRACE_FMT_END();
#endif
    FUNCTION_P1_OUT;
    return OK;
}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
hardwareOps_deque(QBufInfo &deqBuf)
{

#if SUPPORT_ISP

    FUNCTION_P1_IN;
    //CAM_TRACE_NAME("P1:deque");

    if (!getActive()) {
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mHardwareLock);
    if (!getActive()) {
        return BAD_VALUE;
    }

    {
        // deque buffer, and handle frame and metadata
        MY_LOGD2("%" PRId64 ", %f", mDequeThreadProfile.getAvgDuration(), mDequeThreadProfile.getFps());
        QPortID PortID;
        if (IS_PORT(CONFIG_PORT_IMGO, mConfigPort)) {//(mvOutImage_full.size() > 0) {
            PortID.mvPortId.push_back(PORT_IMGO);
        }
        if (IS_PORT(CONFIG_PORT_RRZO, mConfigPort)) {//(mOutImage_resizer != NULL) {
            PortID.mvPortId.push_back(PORT_RRZO);
        }
        if (IS_PORT(CONFIG_PORT_LCSO, mConfigPort)) {
            PortID.mvPortId.push_back(PORT_LCSO);
        }
        #if SUPPORT_LMV
        if (IS_PORT(CONFIG_PORT_EISO, mConfigPort)) {
            PortID.mvPortId.push_back(PORT_EISO);
        }
        #endif

        // for mBurstNum: 4, the buffer is as
        // [I1][I2][I3][I4][R1][R2][R3][R4][E1][E2][E3][E4][L1][L2][L3][L4]
        mDequeThreadProfile.pulse_down();
        //
        CAM_TRACE_FMT_BEGIN("P1:CamIO-deque @[0x%X]", mConfigPort);
        MY_LOGD2("mpCamIO->deque +++");
        if(!mpCamIO->deque(PortID, deqBuf)) {
            if(getActive()) {
                MY_LOGE("deque fail");
            } else {
                MY_LOGE("deque fail - after stop");
            }
            CAM_TRACE_FMT_END();
            AEE_ASSERT("\nCRDISPATCH_KEY:MtkCam/P1Node:ISP pass1 deque fail");
            return BAD_VALUE;
        }
        MY_LOGD2("mpCamIO->deque ---");
        CAM_TRACE_FMT_END();
        //
        mDequeThreadProfile.pulse_up();
    }
    //
    if( mDebugScanLineMask != 0 &&
        mpDebugScanLine != NULL)
    {
        for(size_t i = 0; i < deqBuf.mvOut.size(); i++)
        {
            if( (   deqBuf.mvOut[i].mPortID.index == PORT_RRZO.index &&
                    mDebugScanLineMask & DRAWLINE_PORT_RRZO  ) ||
                (   deqBuf.mvOut[i].mPortID.index == PORT_IMGO.index &&
                    mDebugScanLineMask & DRAWLINE_PORT_IMGO  )   )
            {
                mpDebugScanLine->drawScanLine(
                                    deqBuf.mvOut[i].mBuffer->getImgSize().w,
                                    deqBuf.mvOut[i].mBuffer->getImgSize().h,
                                    (void*)(deqBuf.mvOut[i].mBuffer->getBufVA(0)),
                                    deqBuf.mvOut[i].mBuffer->getBufSizeInBytes(0),
                                    deqBuf.mvOut[i].mBuffer->getBufStridesInBytes(0));

            }
        }
    }
    static bool shouldPrint = false;
    if (shouldPrint) {
        for(size_t i = 0; i < deqBuf.mvOut.size(); i++) {
            char filename[256];
            sprintf(filename, "/data/P1_%d_%d_%d.raw", deqBuf.mvOut.at(i).mMetaData.mMagicNum_hal,
                deqBuf.mvOut.at(i).mBuffer->getImgSize().w,
                deqBuf.mvOut.at(i).mBuffer->getImgSize().h);
            NSCam::Utils::saveBufToFile(filename, (unsigned char*)deqBuf.mvOut.at(i).mBuffer->getBufVA(0), deqBuf.mvOut.at(i).mBuffer->getBufSizeInBytes(0));
            shouldPrint = false;
        }
    }
    #if 1
    if (mEnableDumpRaw && deqBuf.mvOut.size() > 0) {
        MUINT32 num = deqBuf.mvOut.at(0).mMetaData.mMagicNum_hal;

        /* Record previous "debug.p1.pureraw_dump" prop value.
        * When current prop value is not equal to previous prop value, it will dump raw.
        */
        static MINT32 prevDumpProp = 0;

        /* If current "debug.p1.pureraw_dump" prop value < 0, this variable will save it.
        * This variable is used to continuous dump raws.
        * For example, assume current prop value is -20. When pipeline starts, it will dump frames with magic num < 20.
        */
        static MUINT32 continueDump = 0;

        MINT32 currentDump = property_get_int32("debug.p1.pureraw_dump",0);
        if(currentDump == 0){
            prevDumpProp = 0;
            continueDump = 0;
        }else if(currentDump < 0){
            continueDump = (MUINT32)(-currentDump);
        }
        if (prevDumpProp != currentDump || (num < continueDump))
        {
            prevDumpProp = currentDump;
            for(size_t i = 0; i < deqBuf.mvOut.size(); i++) {
                char filename[256] = {0};
                sprintf(filename, "/sdcard/raw/p1_%u_%d_%04dx%04d_%04d_%d.raw",
                    num,
                    ((deqBuf.mvOut.at(i).mPortID.index == PORT_RRZO.index) ?
                    (0) : (1)),
                    (int)deqBuf.mvOut.at(i).mBuffer->getImgSize().w,
                    (int)deqBuf.mvOut.at(i).mBuffer->getImgSize().h,
                    (int)deqBuf.mvOut.at(i).mBuffer->getBufStridesInBytes(0),
                    (int)mSensorFormatOrder );
                deqBuf.mvOut.at(i).mBuffer->saveToFile(filename);
                MY_LOGI("save to file : %s", filename);
            }
        }
    }
    #endif


    FUNCTION_P1_OUT;

    return OK;
#else
    return OK;
#endif

}


/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
hardwareOps_stop()
{
#if SUPPORT_ISP
    CAM_TRACE_NAME("P1:hardwareOps_stop");

    FUNCTION_P1S_IN;

    //(1) handle active flag
    if (!getActive()) {
        MY_LOGD("active=%d", getActive());
        return OK;
    }
    setActive(MFALSE);
    setReady(MFALSE);

    if (getInit()) {
        MY_LOGI("mHardwareLock waiting");
        Mutex::Autolock _l(mHardwareLock);
    }
    MY_LOGI("Cam::%d Req=%d Set=%d Enq=%d Deq=%d", getOpenId(),
        mTagReq.get(), mTagSet.get(), mTagEnq.get(), mTagDeq.get());

    //(2.2) stop 3A stt
    #if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-stopStt");
    MY_LOGI1("mp3A->stopStt +++");
    if (mp3A) {
        mp3A->stopStt();
    }
    MY_LOGI1("mp3A->stopStt ---");
    CAM_TRACE_END();
    #endif

    //(2.3) stop isp
    {
        //Mutex::Autolock _l(mHardwareLock);
        if (mLongExp.get()) {
            CAM_TRACE_BEGIN("P1:CamIO-abort");
            MY_LOGI1("mpCamIO->abort +++");
            if(!mpCamIO || !mpCamIO->abort()) {
                MY_LOGE("hardware abort fail");
                return BAD_VALUE;
            }
            MY_LOGI1("mpCamIO->abort ---");
            CAM_TRACE_END();
        } else {
            CAM_TRACE_BEGIN("P1:CamIO-stop");
            MY_LOGI1("mpCamIO->stop +++");
            if(!mpCamIO || !mpCamIO->stop(MTRUE)) {
                MY_LOGE("hardware stop fail");
                return BAD_VALUE;
            }
            MY_LOGI1("mpCamIO->stop ---");
            CAM_TRACE_END();
        }
        /*
        if(IS_PORT(CONFIG_PORT_RRZO, mConfigPort))
            mpCamIO->abortDma(PORT_RRZO,getNodeName());
        if(IS_PORT(CONFIG_PORT_IMGO, mConfigPort))
            mpCamIO->abortDma(PORT_IMGO,getNodeName());
        if(IS_PORT(CONFIG_PORT_EISO, mConfigPort))
            mpCamIO->abortDma(PORT_EISO,getNodeName());
        if(IS_PORT(CONFIG_PORT_LCSO, mConfigPort))
            mpCamIO->abortDma(PORT_LCSO,getNodeName());
        */
    }

    //(3.0) stop 3A
    #if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-stop");
    if (mp3A) {
        #if SUPPORT_LMV
        if (EIS_MODE_IS_CALIBRATION_ENABLED(mEisMode) && mpLMV) {
            //Enable OIS
            MY_LOGD2("[EisHal] mEisMode:%d => Enable OIS \n", mEisMode);
            mp3A ->send3ACtrl(E3ACtrl_SetEnableOIS, 1, 0);
            mEisMode = EIS_MODE_OFF;
        }
        #endif

        MY_LOGI1("mp3A->stop +++");
        mp3A->detachCb(IHal3ACb::eID_NOTIFY_3APROC_FINISH, this);
        mp3A->detachCb(IHal3ACb::eID_NOTIFY_CURR_RESULT, this);
        mp3A->detachCb(IHal3ACb::eID_NOTIFY_VSYNC_DONE, this);
        mp3A->stop();
        MY_LOGI1("mp3A->stop ---");
    }
    CAM_TRACE_END();
    #endif

    //(3.1) destroy 3A
    #if SUPPORT_3A
    CAM_TRACE_BEGIN("P1:3A-destroy");
    if (mp3A) {
        MY_LOGI1("mp3A->destroyInstance +++");
        mp3A->destroyInstance(getNodeName());
        MY_LOGI1("mp3A->destroyInstance ---");
        mp3A = NULL;
    }
    CAM_TRACE_END();
    #endif

    //(3.2) destroy isp
    {
        MY_LOGI1("HwLockWait +++");
        Mutex::Autolock _l(mHardwareLock);
        MY_LOGI1("HwLockWait ---");
        #if SUPPORT_LMV
        if(mpLMV) {
            mpLMV->Uninit();
            mpLMV->DestroyInstance(LOG_TAG);
            mpLMV = NULL;
        }

        if(mpEIS) {
            mpEIS->Uninit();
            mpEIS->DestroyInstance(LOG_TAG);
            mpEIS = NULL;
        }
        #endif

        #if SUPPORT_VHDR
        if(mpVhdr)
        {
            mpVhdr->Uninit();
            //mpVhdr->DestroyInstance(LOG_TAG); // instance always exist until process kill
            mpVhdr = NULL;
        }
        #endif
        #if SUPPORT_LCS
        if(mpLCS)
        {
            mpLCS->Uninit();
            mpLCS->DestroyInstance(LOG_TAG); // instance always exist until process kill
            mpLCS = NULL;
        }
        #endif
        //
        CAM_TRACE_BEGIN("P1:CamIO-uninit");
        MY_LOGI1("mpCamIO->uninit +++");
        if(!mpCamIO->uninit() )
        {
            MY_LOGE("hardware uninit fail");
            return BAD_VALUE;
        }
        MY_LOGI1("mpCamIO->uninit ---");
        MY_LOGI1("mpCamIO->destroyInstance +++");
        #ifdef USING_MTK_LDVT
        mpCamIO->destroyInstance("iopipeUseTM");
        #else
        mpCamIO->destroyInstance(getNodeName());
        #endif
        MY_LOGI1("mpCamIO->destroyInstance ---");
        mpCamIO = NULL;
        CAM_TRACE_END();
    }
    //
    {
        Mutex::Autolock _ll(mFrameSetLock);
        mFrameSetAlready = MFALSE;
    }
    //
    FUNCTION_P1S_OUT;

    return OK;

#else
    return OK;
#endif

}

/******************************************************************************
 *
 *****************************************************************************/
MVOID
P1NodeImp::
prepareCropInfo(
    IMetadata* pAppMetadata,
    IMetadata* pHalMetadata,
    QueNode_T& node)
{
    MSize refSensorSize = mSensorParams.size;
    MBOOL isFullBin = MFALSE;
    if (mIsBinEn) {
        BIN_RESIZE(refSensorSize.w);
        BIN_RESIZE(refSensorSize.h);
        if (node.reqType == REQ_TYPE_NORMAL &&
            (IS_OUT(REQ_OUT_FULL_PROC, node.reqOutSet))) {
            isFullBin = MTRUE;
        }
    }
    MY_LOGD2("IsBinEn:%d IsFullBin:%d sensor(%dx%d) ref(%dx%d)",
        mIsBinEn, isFullBin,
        mSensorParams.size.w, mSensorParams.size.h,
        refSensorSize.w, refSensorSize.h);
    if (mvStreamImg[STREAM_IMG_OUT_FULL] != NULL) {
        node.dstSize_full = mvStreamImg[STREAM_IMG_OUT_FULL]->getImgSize();
        node.cropRect_full = MRect(MPoint(0, 0),
            (isFullBin) ? refSensorSize : mSensorParams.size);
    } else if (mvStreamImg[STREAM_IMG_OUT_OPAQUE] != NULL) {
        node.dstSize_full = mSensorParams.size;
        node.cropRect_full = MRect(MPoint(0, 0),
            (isFullBin) ? refSensorSize : mSensorParams.size);
    } else {
        node.dstSize_full = MSize(0, 0);
        node.cropRect_full = MRect(MPoint(0, 0), MSize(0, 0));
    }
    if (mvStreamImg[STREAM_IMG_OUT_RESIZE] != NULL) {
        node.dstSize_resizer = mvStreamImg[STREAM_IMG_OUT_RESIZE]->getImgSize();
        node.cropRect_resizer = MRect(MPoint(0, 0), refSensorSize);
    } else {
        node.dstSize_resizer= MSize(0, 0);
        node.cropRect_resizer = MRect(MPoint(0, 0), MSize(0, 0));
    }
    //
    if (pAppMetadata != NULL && pHalMetadata != NULL) {
        MRect cropRect_metadata;    // get from metadata
        MRect cropRect_control;     // set to node

        if( !tryGetMetadata<MRect>(pAppMetadata, MTK_SCALER_CROP_REGION, cropRect_metadata) ) {
            MY_LOGI("Metadata exist - no MTK_SCALER_CROP_REGION, "
                "crop size set to full(%dx%d) resizer(%dx%d)",
                node.dstSize_full.w, node.dstSize_full.h,
                node.dstSize_resizer.w, node.dstSize_resizer.h);
        } else {
            if( !tryGetMetadata<MRect>(pHalMetadata, MTK_P1NODE_SENSOR_CROP_REGION, cropRect_control) )
            {
                MY_LOGD2("cannot get MTK_P1NODE_SENSOR_CROP_REGION, use MTK_SCALER_CROP_REGION");
                if (mIsBinEn) {
                    BIN_RESIZE(cropRect_metadata.p.x);
                    BIN_RESIZE(cropRect_metadata.p.y);
                    BIN_RESIZE(cropRect_metadata.s.w);
                    BIN_RESIZE(cropRect_metadata.s.h);
                }
                simpleTransform tranActive2Sensor = simpleTransform(
                        MPoint(0,0), mActiveArray.size(), mSensorParams.size);
                cropRect_control.p = transform(tranActive2Sensor,
                                                cropRect_metadata.leftTop());
                cropRect_control.s = transform(tranActive2Sensor,
                                                cropRect_metadata.size());
            }
            else
            {
                MY_LOGD2("get MTK_P1NODE_SENSOR_CROP_REGION success");
                if (mIsBinEn) {
                    BIN_RESIZE(cropRect_control.p.x);
                    BIN_RESIZE(cropRect_control.p.y);
                    BIN_RESIZE(cropRect_control.s.w);
                    BIN_RESIZE(cropRect_control.s.h);
                }
            }
            MY_LOGD2("crop size set to (%d,%d,%dx%d))",
                cropRect_control.p.x, cropRect_control.p.y,
                cropRect_control.s.w, cropRect_control.s.h);

            #if SUPPORT_LMV
            if (mpLMV)
            {
                MSize videoSize = MSize (0,0);
                MBOOL isEisOn = false;
                MRect const requestRect = MRect(cropRect_control);
                MSize const sensorSize = MSize(mSensorParams.size);
                MPoint const requestCenter=
                    MPoint((requestRect.p.x + (requestRect.s.w >> 1)),
                            (requestRect.p.y + (requestRect.s.h >> 1)));
                isEisOn = isEISOn(pAppMetadata);

                if (!tryGetMetadata<MSize>(pHalMetadata, MTK_EIS_VIDEO_SIZE, videoSize))
                {
                    MY_LOGD2("cannot get MTK_EIS_VIDEO_SIZE");
                }

                cropRect_control.s = mpLMV->QueryMinSize(isEisOn, sensorSize, videoSize,
                                                         requestRect.size());

                if(mEnableFrameSync && isEisOn)
                {
                    MY_LOGD2("EIS minimun size not supported in dual cam mode");
                    cropRect_control = MRect(requestRect);
                }

                if (cropRect_control.s.w != requestRect.size().w)
                {
                    MSize::value_type half_len =
                        ((cropRect_control.s.w + 1) >> 1);
                    if (requestCenter.x < half_len) {
                        cropRect_control.p.x = 0;
                    } else if ((requestCenter.x + half_len) > sensorSize.w) {
                        cropRect_control.p.x = sensorSize.w -
                                                cropRect_control.s.w;
                    } else {
                        cropRect_control.p.x = requestCenter.x - half_len;
                    }
                }
                if (cropRect_control.s.h != requestRect.size().h)
                {
                    MSize::value_type half_len =
                        ((cropRect_control.s.h + 1) >> 1);
                    if (requestCenter.y < half_len) {
                        cropRect_control.p.y = 0;
                    } else if ((requestCenter.y + half_len) > sensorSize.h) {
                        cropRect_control.p.y = sensorSize.h -
                                                cropRect_control.s.h;
                    } else {
                        cropRect_control.p.y = requestCenter.y - half_len;
                    }
                }
            }
            #endif
            /*
            MY_LOGD("[CropInfo] metadata(%d, %d, %dx%d) "
                "control(%d, %d, %dx%d) "
                "active(%d, %d, %dx%d) "
                "sensor(%dx%d)",
                cropRect_metadata.leftTop().x,
                cropRect_metadata.leftTop().y,
                cropRect_metadata.size().w, cropRect_metadata.size().h,
                cropRect_control.leftTop().x,
                cropRect_control.leftTop().y,
                cropRect_control.size().w, cropRect_control.size().h,
                mActiveArray.leftTop().x,
                mActiveArray.leftTop().y,
                mActiveArray.size().w, mActiveArray.size().h,
                mSensorParams.size.w, mSensorParams.size.h);
            */
            // TODO: check more case about crop region
            if ((cropRect_control.size().w < 0) ||
                (cropRect_control.size().h < 0) ||
                (cropRect_control.leftTop().x < 0) ||
                (cropRect_control.leftTop().y < 0) ||
                (cropRect_control.leftTop().x >= refSensorSize.w) ||
                (cropRect_control.leftTop().y >= refSensorSize.h)) {
                MY_LOGW("Metadata exist - invalid cropRect_control"
                    "(%d, %d, %dx%d) sensor(%dx%d)",
                    cropRect_control.leftTop().x,
                    cropRect_control.leftTop().y,
                    cropRect_control.size().w, cropRect_control.size().h,
                    refSensorSize.w, refSensorSize.h);
                return;
            }
            if ((cropRect_control.p.x + cropRect_control.s.w) >
                refSensorSize.w) {
                cropRect_control.s.w = refSensorSize.w -
                                        cropRect_control.p.x;
            }
            if ((cropRect_control.p.y + cropRect_control.s.h) >
                refSensorSize.h) {
                cropRect_control.s.h = refSensorSize.h -
                                        cropRect_control.p.y;
            }
            // calculate the crop region validity
            if (mvStreamImg[STREAM_IMG_OUT_FULL] != NULL) {
                MRect cropRect_full = cropRect_control;
                if (isFullBin) {
                    BIN_REVERT(cropRect_full.p.x);
                    BIN_REVERT(cropRect_full.p.y);
                    BIN_REVERT(cropRect_full.s.w);
                    BIN_REVERT(cropRect_full.s.h);
                }
                calculateCropInfoFull(
                    mSensorParams.pixelMode,
                    (isFullBin) ? refSensorSize : mSensorParams.size,
                    mvStreamImg[STREAM_IMG_OUT_FULL]->getImgSize(),
                    (isFullBin) ? (cropRect_control) :
                    cropRect_full,
                    node.cropRect_full,
                    node.dstSize_full,
                    mLogLevel);
            }
            if (mvStreamImg[STREAM_IMG_OUT_RESIZE] != NULL) {
                calculateCropInfoResizer(
                    mSensorParams.pixelMode,
                    (mvStreamImg[STREAM_IMG_OUT_RESIZE]->getImgFormat()),
                    refSensorSize,
                    (mvStreamImg[STREAM_IMG_OUT_RESIZE]->getImgSize()),
                    cropRect_control,
                    node.cropRect_resizer,
                    node.dstSize_resizer,
                    mLogLevel);
            }
        }
    }
    MY_LOGD2("Crop-Info F(%d,%d,%dx%d)(%dx%d) R(%d,%d,%dx%d)(%dx%d)",
            node.cropRect_full.p.x, node.cropRect_full.p.y,
            node.cropRect_full.s.w, node.cropRect_full.s.h,
            node.dstSize_full.w, node.dstSize_full.h,
            node.cropRect_resizer.p.x, node.cropRect_resizer.p.y,
            node.cropRect_resizer.s.w, node.cropRect_resizer.s.h,
            node.dstSize_resizer.w, node.dstSize_resizer.h);
}

/******************************************************************************
 *
 *****************************************************************************/
MVOID
P1NodeImp::
createNode(sp<IPipelineFrame> appframe,
           QueJob_T *job,
           Que_T *Queue,
           Mutex *QueLock,
           List<MetaSet_T> *list,
           Mutex *listLock)
{
    //create queue node
    MUINT32 newNum = get_and_increase_magicnum();
    MetaSet_T metaInfo;
    IMetadata* pAppMeta = NULL;
    IMetadata* pHalMeta = NULL;
    //
    MINT32 meta_raw_type = (MINT32)mRawDefType;
    MBOOL meta_raw_exist = MFALSE;
    //
    QueNode_T node;
    //
    CAM_TRACE_FMT_BEGIN("createNode(%d)(%d,%d)", newNum,
        P1GET_FRM_NUM(appframe), P1GET_REQ_NUM(appframe));
    //
    node.magicNum = newNum;
    /*
    // set as constructor
    node.sofIdx = P1SOFIDX_INIT_VAL;
    node.appFrame = NULL;
    node.reqType = REQ_TYPE_UNKNOWN;
    node.reqOutSet = REQ_SET_NONE;
    node.expRec = EXP_REC_NONE;
    node.exeState = EXE_STATE_NULL;
    */
    //
    if (appframe != NULL) { // for REQ_TYPE_NORMAL
        mTagReq.set(newNum);
        node.appFrame = appframe;
        findRequestStream(node);
        //
        if (mvStreamMeta[STREAM_META_IN_APP] != NULL) {
            if (OK == frameMetadataGet(
                node, STREAM_META_IN_APP, &metaInfo.appMeta)) {
                pAppMeta = &(metaInfo.appMeta);
            } else {
                MY_LOGI("can not lock the app metadata");
            }
        }
        if (mvStreamMeta[STREAM_META_IN_HAL] != NULL) {
            if (OK == frameMetadataGet(
                node, STREAM_META_IN_HAL, &metaInfo.halMeta)) {
                pHalMeta = &(metaInfo.halMeta);
            } else {
                MY_LOGI("can not lock the hal metadata");
            }
        }
        #if 1 // add raw type from hal meta
        if (pHalMeta != NULL) {
            MINT32 raw_type = meta_raw_type;
            if (mEnableDualPD == MFALSE && // DualPD must be processed raw
                tryGetMetadata<MINT32>(
                pHalMeta, MTK_P1NODE_RAW_TYPE, raw_type)) {
                MY_LOGD1("raw type set from outside %d", raw_type);
                if ((mRawOption & (MUINT32)(1 << raw_type)) > 0) {
                    meta_raw_type = raw_type;
                    meta_raw_exist = MTRUE;
                }
            }
        }
        #endif
        //
        if (node.streamBufImg[STREAM_IMG_IN_YUV].bExist) {
            node.reqType = REQ_TYPE_YUV;
        } else if (node.streamBufImg[STREAM_IMG_IN_OPAQUE].bExist) {
            node.reqType = REQ_TYPE_REDO;
        } else {
            node.reqType = REQ_TYPE_NORMAL;
            if (node.streamBufImg[STREAM_IMG_OUT_OPAQUE].bExist) {
                node.reqOutSet |= REQ_SET(REQ_OUT_FULL_OPAQUE);
            }
            if (node.streamBufImg[STREAM_IMG_OUT_FULL].bExist) {
                if (meta_raw_type == EPipe_PROCESSED_RAW) {
                    node.reqOutSet |= REQ_SET(REQ_OUT_FULL_PROC);
                } else {
                    node.reqOutSet |= REQ_SET(REQ_OUT_FULL_PURE);
                }
            }
            if (node.streamBufImg[STREAM_IMG_OUT_RESIZE].bExist) {
                node.reqOutSet |= REQ_SET(REQ_OUT_RESIZER);
            }
            if (node.streamBufImg[STREAM_IMG_OUT_LCS].bExist) {
                node.reqOutSet |= REQ_SET(REQ_OUT_LCSO);
            }
            //MY_LOGD2("normal (%d) 0x%x raw(%d %d)", node.magicNum,
            //    node.reqOutSet, meta_raw_exist, meta_raw_type);
        }
    } else {
        node.reqType = REQ_TYPE_INITIAL;
        if (IS_PORT(CONFIG_PORT_IMGO, mConfigPort) && mpStreamPool_full != NULL) {
            if (meta_raw_type == EPipe_PROCESSED_RAW) {
                node.reqOutSet |= REQ_SET(REQ_OUT_FULL_PROC);
            } else {
                node.reqOutSet |= REQ_SET(REQ_OUT_FULL_PURE);
            }
        }
        if (IS_PORT(CONFIG_PORT_RRZO, mConfigPort) && mpStreamPool_resizer != NULL) {
            node.reqOutSet |= REQ_SET(REQ_OUT_RESIZER);
        }
        if (IS_PORT(CONFIG_PORT_LCSO, mConfigPort) && mpStreamPool_lcso != NULL) {
            node.reqOutSet |= REQ_SET(REQ_OUT_LCSO);
        }
    }
    //
    if ((list != NULL) && (node.reqType == REQ_TYPE_NORMAL ||
        node.reqType == REQ_TYPE_INITIAL || node.reqType == REQ_TYPE_YUV)) {
        IMetadata::IEntry entry_num(MTK_P1NODE_PROCESSOR_MAGICNUM);
        entry_num.push_back(newNum, Type2Type< MINT32 >());
        metaInfo.halMeta.update(MTK_P1NODE_PROCESSOR_MAGICNUM, entry_num);
        //
        MUINT8 is_dummy = (node.reqType == REQ_TYPE_NORMAL) ? 0 : 1;
        IMetadata::IEntry entry_dummy(MTK_HAL_REQUEST_DUMMY);
        entry_dummy.push_back(is_dummy, Type2Type< MUINT8 >());
        metaInfo.halMeta.update(MTK_HAL_REQUEST_DUMMY, entry_dummy);
        //
        #if 0
        IMetadata::IEntry entry_repeat(MTK_HAL_REQUEST_REPEAT);
        entry_repeat.push_back(0, Type2Type< MUINT8 >());
        metaInfo.halMeta.update(MTK_HAL_REQUEST_REPEAT, entry_repeat);
        #endif
        //
        #if 1 // add raw type to hal meta
        if (node.reqType == REQ_TYPE_NORMAL && !meta_raw_exist) {
            IMetadata::IEntry entryRawType(MTK_P1NODE_RAW_TYPE);
            entryRawType.push_back(meta_raw_type, Type2Type< MINT32 >());
            metaInfo.halMeta.update(MTK_P1NODE_RAW_TYPE, entryRawType);
        }
        #endif
        //
        if(listLock != NULL) {
            Mutex::Autolock _l(*listLock);
            (*list).push_back(metaInfo);
            mTagList.set((*list).size());
        } else {
            (*list).push_back(metaInfo);
            mTagList.set((*list).size());
        }
    }
    //
    if (node.reqType != REQ_TYPE_UNKNOWN) {
        mpDeliverMgr->registerNodeList(newNum);
    }
    //
    if (node.reqType == REQ_TYPE_NORMAL || node.reqType == REQ_TYPE_INITIAL) {
        if (IS_PORT(CONFIG_PORT_IMGO, mConfigPort)
            && (0 == (IS_OUT(REQ_OUT_FULL_PROC, node.reqOutSet) ||
                    IS_OUT(REQ_OUT_FULL_PURE, node.reqOutSet) ||
                    IS_OUT(REQ_OUT_FULL_OPAQUE, node.reqOutSet))
                )
            ) {
            node.reqOutSet |= REQ_SET(REQ_OUT_FULL_STUFF);
        }
        if (IS_PORT(CONFIG_PORT_RRZO, mConfigPort)
            && (0 == IS_OUT(REQ_OUT_RESIZER, node.reqOutSet))) {
            node.reqOutSet |= REQ_SET(REQ_OUT_RESIZER_STUFF);
        }
        if (IS_PORT(CONFIG_PORT_LCSO, mConfigPort)
            && (0 == IS_OUT(REQ_OUT_LCSO, node.reqOutSet))) {
            node.reqOutSet |= REQ_SET(REQ_OUT_LCSO_STUFF);
        }
        //
        prepareCropInfo(pAppMeta, pHalMeta, node);
        //
        if (Queue == &mProcessingQueue) {
            node.exeState = EXE_STATE_PROCESSING;
        } else if (Queue == &mRequestQueue) {
            node.exeState = EXE_STATE_REQUESTED;
        }
        //
        job->mSet.push_back(node);
        if (job->mSet.size() == 1) {
            job->mFirstMagicNum = newNum;
        }
        //
        if(Queue != NULL) {
            if(QueLock != NULL) {
                Mutex::Autolock _l(*QueLock);
                if (job->mSet.size() == job->mMaxNum) {
                    (*Queue).push_back((*job));
                    job->mSet.clear();
                    job->mFirstMagicNum = 0;
                }
            } else {
                if (job->mSet.size() == job->mMaxNum) {
                    (*Queue).push_back((*job));
                    job->mSet.clear();
                    job->mFirstMagicNum = 0;
                }
            }
        }
        //MY_LOGD2("normal/initial (%d) 0x%x raw(%d %d) exe:%d", node.magicNum,
        //    node.reqOutSet, meta_raw_exist, meta_raw_type, node.exeState);
    } else if (node.reqType == REQ_TYPE_YUV || node.reqType == REQ_TYPE_REDO) {
        node.exeState = EXE_STATE_DONE;
        MY_LOGD2("send the ZSL request and try to trigger");
        onReturnFrame(node, MFALSE, MTRUE);
    }
    //
    P1_LOGD("[P1::REQ] [%s] " P1INFO_NODE_STR " - job(%d)(%zu/%d)",
        (appframe != NULL) ? "New-Request" : "New-Dummy", P1INFO_NODE_VAR(node),
        job->mFirstMagicNum, job->mSet.size(), job->mMaxNum);
    //
    CAM_TRACE_FMT_END();
    //
    return;
}


/******************************************************************************
 *
 *****************************************************************************/
MVOID
P1NodeImp::
createNode(List<MetaSet_T> *list,
           Mutex *listLock)
{
    if (list == NULL) {
        MY_LOGW("list not exist");
        return;
    }
    MUINT32 newNum = get_and_increase_magicnum();
    MetaSet_T metaInfo;
    //fill in hal metadata
    IMetadata::IEntry entry1(MTK_P1NODE_PROCESSOR_MAGICNUM);
    entry1.push_back(newNum, Type2Type< MINT32 >());
    metaInfo.halMeta.update(MTK_P1NODE_PROCESSOR_MAGICNUM, entry1);
    //
    #if 0
    IMetadata::IEntry entry2(MTK_HAL_REQUEST_REPEAT);
    entry2.push_back(0, Type2Type< MUINT8 >());
    metaInfo.halMeta.update(MTK_HAL_REQUEST_REPEAT, entry2);
    #endif
    //
    IMetadata::IEntry entry3(MTK_HAL_REQUEST_DUMMY);
    entry3.push_back(1, Type2Type< MUINT8 >());
    metaInfo.halMeta.update(MTK_HAL_REQUEST_DUMMY, entry3);

    if(listLock != NULL) {
        Mutex::Autolock _l(*listLock);
        (*list).push_back(metaInfo);
        mTagList.set((*list).size());
    } else {
        (*list).push_back(metaInfo);
        mTagList.set((*list).size());
    }

    P1_LOGD("[New-Padding] (%d)", newNum);
    return;
}


/******************************************************************************
 *
 *****************************************************************************/
MVOID
P1NodeImp::
createNode(Que_T &Queue)
{
    QueJob_T job(mBurstNum);
    for (MUINT8 n = 0; n < mBurstNum; n++) {
        MUINT32 newNum = get_and_increase_magicnum();
        {
            QueNode_T node;
            node.magicNum = newNum;
            node.sofIdx = P1SOFIDX_INIT_VAL;
            node.appFrame = NULL;
            job.mSet.push_back(node);
            //job.mCurNum ++;
            if (n == 0) {
                job.mFirstMagicNum = newNum;
            }
        }
        P1_LOGD("[New Dummy] M(%d) job(%d)-(%zu/%d)",
            newNum, job.mFirstMagicNum, job.mSet.size(), job.mMaxNum);
    }

    Queue.push_back(job);

    return;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
onProcessResult(
    QueNode_T & node,
    QBufInfo const &deqBuf,
    MetaSet_T const &result3A,
    IMetadata const &resultLMV,
    MUINT32 const index
)
{
    FUNCTION_P1_IN;
    //
    CAM_TRACE_FMT_BEGIN("Return(%d)(%d,%d)", node.magicNum,
        P1GET_FRM_NUM(node.appFrame), P1GET_REQ_NUM(node.appFrame));
    //
    if (node.appFrame != 0) {

        // APP out Meta Stream
        if (mvStreamMeta[STREAM_META_OUT_APP] != NULL) {
            IMetadata appMetadata;
            generateAppMeta(node, result3A, deqBuf, appMetadata, index);
            if ((IS_OUT(REQ_OUT_FULL_OPAQUE, node.reqOutSet)) &&
                (node.streamBufImg[STREAM_IMG_OUT_OPAQUE].spImgBuf != NULL) &&
                (!IS_EXP(EXP_EVT_NOBUF_IMGO, node.expRec))) {
                // app metadata index
                IMetadata appTagIndex;
                generateAppTagIndex(appMetadata, appTagIndex);
                sp<IImageBufferHeap> pImageBufferHeap =
                    node.streamBufImg[STREAM_IMG_OUT_OPAQUE].spImgBuf->
                    getImageBufferHeap();
                MERROR status = OpaqueReprocUtil::setAppMetadataToHeap(
                    pImageBufferHeap,
                    appTagIndex);
                MY_LOGD2("setAppMetadataToHeap (%d)", status);
            }
            frameMetadataGet(node, STREAM_META_OUT_APP, NULL,
                MTRUE, &appMetadata);
        }

        // HAL out Meta Stream
        if (mvStreamMeta[STREAM_META_OUT_HAL] != NULL) {
            IMetadata inHalMetadata;
            IMetadata outHalMetadata;
            frameMetadataGet(node, STREAM_META_IN_HAL, &inHalMetadata);
            generateHalMeta(result3A, deqBuf, resultLMV, inHalMetadata,
                outHalMetadata, index);
            if ((IS_OUT(REQ_OUT_FULL_OPAQUE, node.reqOutSet)) &&
                (node.streamBufImg[STREAM_IMG_OUT_OPAQUE].spImgBuf != NULL) &&
                (!IS_EXP(EXP_EVT_NOBUF_IMGO, node.expRec))) {
                sp<IImageBufferHeap> pImageBufferHeap =
                    node.streamBufImg[STREAM_IMG_OUT_OPAQUE].spImgBuf->
                    getImageBufferHeap();
                MERROR status = OpaqueReprocUtil::setHalMetadataToHeap(
                    pImageBufferHeap,
                    outHalMetadata);
                MY_LOGD2("setHalMetadataToHeap (%d)", status);
            }
            frameMetadataGet(node, STREAM_META_OUT_HAL, NULL,
                MTRUE, &outHalMetadata);
        }

        #if (IS_P1_LOGD)
        if (mLogLevel > 0) {
            android::String8 strInfo("");
            strInfo += String8::format("[P1::DEQ] " P1INFO_NODE_STR
                " job(%d/%d) ", P1INFO_NODE_VAR(node), index, mBurstNum);
            for (size_t n = index; n < deqBuf.mvOut.size(); n += mBurstNum) {
                if (deqBuf.mvOut[n].mPortID.index == PORT_IMGO.index) {
                    strInfo += String8::format("IMG(%s) ", (deqBuf.mvOut[n].
                        mMetaData.mRawType == EPipe_PROCESSED_RAW) ?
                        "proc" : "pure");
                } else if (deqBuf.mvOut[n].mPortID.index == PORT_RRZO.index) {
                    MRect crop_s = deqBuf.mvOut[n].mMetaData.mCrop_s;
                    MRect crop_d = deqBuf.mvOut[n].mMetaData.mCrop_d;
                    MSize size_d = deqBuf.mvOut[n].mMetaData.mDstSize;
                    strInfo += String8::format(
                        "RRZ%d(%d-%d-%dx%d)(%d-%d-%dx%d)(%dx%d) ", mIsBinEn,
                        crop_s.p.x, crop_s.p.y, crop_s.s.w, crop_s.s.h,
                        crop_d.p.x, crop_d.p.y, crop_d.s.w, crop_d.s.h,
                        size_d.w, size_d.h);
               }
            }
            P1_LOGD("%sT(%" PRId64 ")(%" PRId64 ")", strInfo.string(),
                node.frameExpDuration, node.frameTimeStamp);
        }
        #endif
    }
    //
    #if 1 // trigger only at the end of this job
    onReturnFrame(node, MFALSE,
        ((mBurstNum <= 1) || (index == (MUINT32)(mBurstNum - 1))) ?
        MTRUE : MFALSE);
    #else
    onReturnFrame(node, MFALSE, MTRUE);
    #endif
    //
    CAM_TRACE_FMT_END();
    //
    FUNCTION_P1_OUT;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
processRedoFrame(
    QueNode_T & node
)
{
    FUNCTION_P1_IN;
    //
    if (node.needFlush) {
        MY_LOGD("need to flush, skip frame processing");
        return;
    };
    //MBOOL outAppMetaSuccess = MFALSE;
    //MBOOL outHalMetaSuccess = MFALSE;
    IMetadata appMeta;
    IMetadata halMeta;
    sp<IImageBuffer> imgBuf;
    //
    if (OK != frameImageGet(node, STREAM_IMG_IN_OPAQUE, imgBuf)) {
        MY_LOGE("Can not get in-opaque buffer from frame");
    } else {
        sp<IImageBufferHeap> pHeap = imgBuf->getImageBufferHeap();
        IMetadata appMetaTagIndex;
        if (OK == OpaqueReprocUtil::getAppMetadataFromHeap
            (pHeap, appMetaTagIndex)) {
            // get the input of app metadata
            IMetadata metaInApp;
            frameMetadataGet(node, STREAM_META_IN_APP, &metaInApp);
            // get p1node's tags from opaque buffer
            IMetadata::IEntry entryTagIndex =
                appMetaTagIndex.entryFor(MTK_P1NODE_METADATA_TAG_INDEX);
            for (MUINT i = 0; i < entryTagIndex.count(); i++) {
                MUINT32 tag =
                    (MUINT32)entryTagIndex.editItemAt(i, Type2Type<MINT32>());
                IMetadata::IEntry& entryInApp = metaInApp.editEntryFor(tag);
                appMeta.update(tag, entryInApp);
            }
            // Workaround: do not return the duplicated key for YUV reprocessing
            appMeta.remove(MTK_JPEG_THUMBNAIL_SIZE);
            appMeta.remove(MTK_JPEG_ORIENTATION);
            frameMetadataGet(node, STREAM_META_OUT_APP, NULL,
                MTRUE, &appMeta);
        } else {
            MY_LOGW("Can not get app meta from in-opaque buffer");
        }
        if (OK == OpaqueReprocUtil::getHalMetadataFromHeap(pHeap, halMeta)) {
            IMetadata::IEntry entry(MTK_HAL_REQUEST_REQUIRE_EXIF);
            entry.push_back(1, Type2Type<MUINT8>());
            halMeta.update(entry.tag(), entry);
            frameMetadataGet(node, STREAM_META_OUT_HAL, NULL,
                MTRUE, &halMeta);
        } else {
            MY_LOGW("Can not get hal meta from in-opaque buffer");
        }
    }
    //
    FUNCTION_P1_OUT;
    //
    return;
};

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
processYuvFrame(
    QueNode_T & node
)
{
    FUNCTION_P1_IN;
    //
    if (node.needFlush) {
        MY_LOGD("need to flush, skip frame processing");
        return;
    };
    IMetadata inAppMetadata;
    IMetadata outAppMetadata;
    IMetadata inHalMetadata;
    IMetadata outHalMetadata;
    MINT64 timestamp = 0;
    // APP in Meta Stream
    if (OK != frameMetadataGet(node, STREAM_META_IN_APP, &inAppMetadata)) {
        MY_LOGW("cannot get in-app-metadata");
    } else {
        if (tryGetMetadata< MINT64 >(
            &inAppMetadata, MTK_SENSOR_TIMESTAMP, timestamp)) {
            MY_LOGD1("timestamp from in-app %" PRId64 , timestamp);
        } else {
            MY_LOGI("cannot find timestamp from in-app");
            timestamp = 0;
        }
    };
    // APP out Meta Stream
    if (trySetMetadata< MINT64 > (
        &outAppMetadata, MTK_SENSOR_TIMESTAMP, timestamp)) {
        if (OK != frameMetadataGet(node, STREAM_META_OUT_APP, NULL,
                    MTRUE, &outAppMetadata)) {
            MY_LOGW("cannot write out-app-metadata");
        }
    } else {
        MY_LOGW("cannot update MTK_SENSOR_TIMESTAMP");
    }
    // HAL in/out Meta Stream
    if (OK != frameMetadataGet(node, STREAM_META_IN_HAL, &inHalMetadata)) {
        MY_LOGW("cannot get in-hal-metadata");
    } else {
        outHalMetadata = inHalMetadata;
        if (!trySetMetadata< MINT32 > (
            &outHalMetadata, MTK_P1NODE_SENSOR_MODE, mSensorParams.mode)) {
            MY_LOGW("cannot update MTK_P1NODE_SENSOR_MODE");
        }
        if (!trySetMetadata< MINT32 > (
            &outHalMetadata, MTK_P1NODE_SENSOR_VHDR_MODE, mSensorParams.vhdrMode)) {
            MY_LOGW("cannot update MTK_P1NODE_SENSOR_MODE");
        }
        if (!trySetMetadata< MRect > (
            &outHalMetadata, MTK_P1NODE_SCALAR_CROP_REGION,
            MRect(mSensorParams.size.w, mSensorParams.size.h))) {
            MY_LOGW("cannot update MTK_P1NODE_SCALAR_CROP_REGION");
        }
        if (!trySetMetadata< MRect > (
            &outHalMetadata, MTK_P1NODE_DMA_CROP_REGION,
            MRect(mSensorParams.size.w, mSensorParams.size.h))) {
            MY_LOGW("cannot update MTK_P1NODE_DMA_CROP_REGION");
        }
        if (!trySetMetadata< MSize > (
            &outHalMetadata, MTK_P1NODE_RESIZER_SIZE, mSensorParams.size)) {
            MY_LOGW("cannot update MTK_P1NODE_RESIZER_SIZE");
        }
        if (OK != frameMetadataGet(node, STREAM_META_OUT_HAL, NULL,
                    MTRUE, &outHalMetadata)) {
            MY_LOGW("cannot write out-hal-metadata");
        }
    };
    //
    FUNCTION_P1_OUT;
    //
    return;
};

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::
releaseNode(
    QueNode_T & node
)
{
    FUNCTION_P1_IN;
    //
    CAM_TRACE_FMT_BEGIN("Release(%d)(%d,%d)", node.magicNum,
        P1GET_FRM_NUM(node.appFrame), P1GET_REQ_NUM(node.appFrame));
    //
    MY_LOGD3(P1INFO_NODE_STR " begin", P1INFO_NODE_VAR(node));
    //
    if (!node.needFlush) {
        if (node.reqType == REQ_TYPE_REDO) {
            processRedoFrame(node);
        } else if (node.reqType == REQ_TYPE_YUV) {
            processYuvFrame(node);
        }
    };
    //
    for (int stream = STREAM_ITEM_START; stream < STREAM_META_NUM; stream++) {
        if (node.streamBufMeta[stream].bExist) {
            frameMetadataPut(node, (STREAM_META)stream);
        };
    };
    //
    for (int stream = STREAM_ITEM_START; stream < STREAM_IMG_NUM; stream++) {
        // TODO: input image buffer return
        if ((!node.streamBufImg[stream].bExist) && // for INITIAL node
            (node.streamBufImg[stream].eSrcType == IMG_BUF_SRC_NULL)) {
            continue; // this stream is not existent and no pool/stuff buffer
        }
        switch (node.streamBufImg[stream].eSrcType) {
            case IMG_BUF_SRC_STUFF:
                stuffImagePut(node, (STREAM_IMG)stream);
                break;
            case IMG_BUF_SRC_POOL:
                poolImagePut(node, (STREAM_IMG)stream);
                break;
            case IMG_BUF_SRC_FRAME:
            case IMG_BUF_SRC_NULL: // for flush node, buf src is not decided
                frameImagePut(node, (STREAM_IMG)stream);
                break;
            default:
                MY_LOGW("node buffer source is not defined");
                MY_LOGW("check node exe "
                    P1INFO_NODE_STR, P1INFO_NODE_VAR(node));
                break;
        };
    };
    //
    if (node.reqType == REQ_TYPE_INITIAL) {
        MY_LOGD3(P1INFO_NODE_STR " INITIAL return", P1INFO_NODE_VAR(node));
        CAM_TRACE_FMT_END();
        return;
    }
    //
    MY_LOGD3(P1INFO_NODE_STR " applyrelease", P1INFO_NODE_VAR(node));
    //
    // Apply buffers to release
    IStreamBufferSet& rStreamBufferSet  = node.appFrame->getStreamBufferSet();
    rStreamBufferSet.applyRelease(getNodeId());

    #if 1
    // camera display systrace - Dispatch
    if (node.reqType == REQ_TYPE_NORMAL &&
        node.appFrame != NULL &&
        node.frameTimeStamp > 0) {
        if  ( ATRACE_ENABLED() ) {
            MINT64 const timestamp = node.frameTimeStamp;
            String8 const str = String8::format(
                "Cam:%d:IspP1:dispatch|timestamp(ns):%" PRId64
                " duration(ns):%" PRId64
                " request:%d frame:%d",
                getOpenId(), timestamp, ::systemTime()- timestamp,
                node.appFrame->getRequestNo(), node.appFrame->getFrameNo()
            );
            CAM_TRACE_BEGIN(str.string());
            CAM_TRACE_END();
        }
    }
    #endif
    //
    MY_LOGD3(P1INFO_NODE_STR " dispatch", P1INFO_NODE_VAR(node));
    //
    // dispatch to next node
    CAM_TRACE_BEGIN("P1:onDispatchFrame");
    onDispatchFrame(node.appFrame);
    CAM_TRACE_END();

    android_atomic_dec(&mInFlightRequestCnt);
    ATRACE_INT("P1_request_cnt",
        android_atomic_acquire_load(&mInFlightRequestCnt));
    //
    MY_LOGD3(P1INFO_NODE_STR " end", P1INFO_NODE_VAR(node));
    //
    mTagOut.set(node.magicNum);
    #if (IS_P1_LOGD)
    if (mLogLevel > 0) {
        android::String8 strInfo("");
        strInfo += String8::format("[P1::OUT] [Release-%d] " P1INFO_NODE_STR,
            node.needFlush, P1INFO_NODE_VAR(node));
        P1_LOGD("%s", strInfo.string());
    }
    #endif
    //
    CAM_TRACE_FMT_END();
    //
    FUNCTION_P1_OUT;
    //
    return;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
requestMetadataEarlyCallback(
    QueNode_T const & node, STREAM_META const streamMeta,
    IMetadata * pMetadata
)
{
    P1_CHECK_STREAM(META, streamMeta);
    P1_CHECK_NODE_STREAM(Meta, node, streamMeta);
    //
    if (pMetadata == NULL) {
        MY_LOGD1("Result Metadata is Null");
        return BAD_VALUE;
    }
    if (pMetadata->count() == 0) {
        MY_LOGD1("Result Metadata is Empty");
        return OK;
    }
    MY_LOGD2("Meta[%d]=(%d) EarlyCB " P1INFO_NODE_STR,
        streamMeta, pMetadata->count(), P1INFO_NODE_VAR(node));
    //
    IMetadata outMetadata = *(pMetadata);
    android::String8 strInfo("");
    strInfo += String8::format("Meta[%d]=(%d) EarlyCB " P1INFO_NODE_STR,
        streamMeta, pMetadata->count(), P1INFO_NODE_VAR(node));
    DurationProfile duration(strInfo.string(), 5000000LL); // 5ms
    duration.pulse_up();
    onEarlyCallback(node.appFrame, mvStreamMeta[streamMeta]->getStreamId(),
        outMetadata);
    duration.pulse_down();
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
frameMetadataInit(QueNode_T & node, STREAM_META const streamMeta,
    sp<IMetaStreamBuffer> &pMetaStreamBuffer)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(META, streamMeta);
    //
    StreamId_T const streamId = mvStreamMeta[streamMeta]->getStreamId();
    IStreamBufferSet& rStreamBufferSet =
        node.appFrame->getStreamBufferSet();
    MERROR const err = ensureMetaBufferAvailable_(
        node.appFrame->getFrameNo(),
        streamId,
        rStreamBufferSet,
        pMetaStreamBuffer
    );
    if (err != OK) {
        MY_LOGW("err != OK @%d", node.magicNum);
        return err;
    }
    if (pMetaStreamBuffer != NULL) {
        node.streamBufMeta[streamMeta].spStreamBuf = pMetaStreamBuffer;
        node.streamBufMeta[streamMeta].eLockState = STREAM_BUF_LOCK_NONE;
    } else {
        MY_LOGI("cannot get MetaStreamBuffer(%d) ID(%#" PRIxPTR ") @%d",
            streamMeta, streamId, node.magicNum);
        return BAD_VALUE;
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
frameMetadataGet(
    QueNode_T & node, STREAM_META const streamMeta, IMetadata * pOutMetadata,
    MBOOL toWrite, IMetadata * pInMetadata)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(META, streamMeta);
    P1_CHECK_NODE_STREAM(Meta, node, streamMeta);
    //
    sp<IMetaStreamBuffer> pMetaStreamBuffer = NULL;
    pMetaStreamBuffer = node.streamBufMeta[streamMeta].spStreamBuf;
    if (pMetaStreamBuffer == NULL && // call frameMetadataInit while NULL
        OK != frameMetadataInit(node, streamMeta, pMetaStreamBuffer)) {
        MY_LOGE("get IMetaStreamBuffer but NULL");
        return BAD_VALUE;
    }
    //
    STREAM_BUF_LOCK curLock = node.streamBufMeta[streamMeta].eLockState;
    // current-lock != needed-lock
    if (((toWrite) && (curLock == STREAM_BUF_LOCK_R))
        || ((!toWrite) && (curLock == STREAM_BUF_LOCK_W))) {
        if (node.streamBufMeta[streamMeta].pMetadata == NULL) {
            MY_LOGE("previous pMetadata is NULL, Meta:%d Lock:%d Write:%d",
                streamMeta, curLock, toWrite);
            return BAD_VALUE;
        }
        pMetaStreamBuffer->unlock(getNodeName(),
            node.streamBufMeta[streamMeta].pMetadata);
        //
        node.streamBufMeta[streamMeta].eLockState = STREAM_BUF_LOCK_NONE;
        node.streamBufMeta[streamMeta].pMetadata = NULL;
    }
    // current-lock == STREAM_BUF_LOCK_NONE
    if (node.streamBufMeta[streamMeta].eLockState == STREAM_BUF_LOCK_NONE) {
        IMetadata* pMetadata = NULL;
        if (toWrite) {
            pMetadata = pMetaStreamBuffer->tryWriteLock(getNodeName());
        } else {
            pMetadata = pMetaStreamBuffer->tryReadLock(getNodeName());
        }
        if (pMetadata == NULL) {
            MY_LOGE("get pMetadata is NULL, Meta:%d Lock:%d Write:%d",
                streamMeta, curLock, toWrite);
            return BAD_VALUE;
        };
        //
        node.streamBufMeta[streamMeta].eLockState = (toWrite) ?
            STREAM_BUF_LOCK_W : STREAM_BUF_LOCK_R;
        node.streamBufMeta[streamMeta].pMetadata = pMetadata;
    }
    //
    if (node.streamBufMeta[streamMeta].pMetadata == NULL) {
        MY_LOGE("stored pMetadata == NULL, Meta:%d", streamMeta);
        return BAD_VALUE;
    }
    //
    if (toWrite && (pInMetadata != NULL)) {
        pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
        *(node.streamBufMeta[streamMeta].pMetadata) = *pInMetadata;
    }
    if (pOutMetadata != NULL) {
        *pOutMetadata = *(node.streamBufMeta[streamMeta].pMetadata);
    }
    MY_LOGD3("MetaGet(%p)(%p), write(%d) Stream:%d Lock:%d>%d " P1INFO_NODE_STR,
        pOutMetadata, pInMetadata, toWrite, streamMeta, curLock,
        node.streamBufMeta[streamMeta].eLockState, P1INFO_NODE_VAR(node));
    //
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
frameMetadataPut(QueNode_T & node, STREAM_META const streamMeta)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(META, streamMeta);
    P1_CHECK_NODE_STREAM(Meta, node, streamMeta);
    //
    StreamId_T const streamId = mvStreamMeta[streamMeta]->getStreamId();
    //
    STREAM_BUF_LOCK curLock = node.streamBufMeta[streamMeta].eLockState;
    //
    #if 1 // keep input stream status
    if (!IS_IN_STREAM_META(streamMeta))
    #endif
    {
        if (node.needFlush) {
            sp<IMetaStreamBuffer> pMetaStreamBuffer = NULL;
            pMetaStreamBuffer = node.streamBufMeta[streamMeta].spStreamBuf;
            if (pMetaStreamBuffer == NULL && // call frameMetaInit while NULL
                OK != frameMetadataInit(node, streamMeta, pMetaStreamBuffer)) {
                MY_LOGE("get IMetaStreamBuffer but NULL");
                return BAD_VALUE;
            }
            pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
        }
    }
    //
    if (curLock != STREAM_BUF_LOCK_NONE) {
        if (node.streamBufMeta[streamMeta].spStreamBuf != NULL) {
            if (node.streamBufMeta[streamMeta].pMetadata != NULL) {
                node.streamBufMeta[streamMeta].spStreamBuf->unlock(
                    getNodeName(), node.streamBufMeta[streamMeta].pMetadata);
                node.streamBufMeta[streamMeta].eLockState =
                    STREAM_BUF_LOCK_NONE;
            } else {
                MY_LOGW("MetaStream locked but no Metadata, Stream:%d:%#" PRIxPTR " "
                    "Lock:%d>%d @%d", streamMeta, streamId, curLock,
                    node.streamBufMeta[streamMeta].eLockState, node.magicNum);
            }
        } else {
            MY_LOGW("MetaStream locked but no StreamBuf, Stream:%d:%#" PRIxPTR " "
                "Lock:%d>%d @%d", streamMeta, streamId, curLock,
                node.streamBufMeta[streamMeta].eLockState, node.magicNum);
        }
    }
    //
    IStreamBufferSet& rStreamBufferSet  = node.appFrame->getStreamBufferSet();
    rStreamBufferSet.markUserStatus(
        streamId, getNodeId(),
        #if 0 // if it is not used, only mark RELEASE
        (curLock == STREAM_BUF_LOCK_NONE &&
        node.exeState == EXE_STATE_REQUESTED) ?
        (IUsersManager::UserStatus::RELEASE) :
        #endif
        (IUsersManager::UserStatus::RELEASE | IUsersManager::UserStatus::USED)
    );
    //
    MY_LOGD3("MetaPut, Stream:%d:%#" PRIxPTR " Lock:%d>%d " P1INFO_NODE_STR,
        streamMeta, streamId, curLock,
        node.streamBufMeta[streamMeta].eLockState, P1INFO_NODE_VAR(node));
    node.streamBufMeta[streamMeta].pMetadata = NULL;
    node.streamBufMeta[streamMeta].spStreamBuf = NULL;
    //
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
frameImageInit(QueNode_T & node, STREAM_IMG const streamImg,
    sp<IImageStreamBuffer> &pImageStreamBuffer)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    //
    StreamId_T const streamId = mvStreamImg[streamImg]->getStreamId();
    IStreamBufferSet& rStreamBufferSet =
        node.appFrame->getStreamBufferSet();
    MERROR const err = ensureImageBufferAvailable_(
        node.appFrame->getFrameNo(),
        streamId,
        rStreamBufferSet,
        pImageStreamBuffer
    );
    if (err != OK) {
        MY_LOGW("err != OK @%d", node.magicNum);
        return err;
    }
    if (pImageStreamBuffer != NULL) {
        node.streamBufImg[streamImg].spStreamBuf = pImageStreamBuffer;
        node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
    } else {
        MY_LOGI("cannot get ImageStreamBuffer(%d) ID(%#" PRIxPTR ") @%d",
            streamImg, streamId, node.magicNum);
        return BAD_VALUE;
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
frameImageGet(
    QueNode_T & node, STREAM_IMG const streamImg, sp<IImageBuffer> &rImgBuf)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    P1_CHECK_NODE_STREAM(Img, node, streamImg);
    //
    sp<IImageStreamBuffer> pImageStreamBuffer = NULL;
    pImageStreamBuffer = node.streamBufImg[streamImg].spStreamBuf;
    if (pImageStreamBuffer == NULL && // call frameImageInit while NULL
        OK != frameImageInit(node, streamImg, pImageStreamBuffer)) {
        MY_LOGE("get ImageStreamBuffer but NULL");
        return BAD_VALUE;
    }
    //
    STREAM_BUF_LOCK curLock = node.streamBufImg[streamImg].eLockState;
    // for stream image, only implement Write Lock
    if (curLock == STREAM_BUF_LOCK_NONE) {
        MUINT groupUsage = pImageStreamBuffer->queryGroupUsage(getNodeId());
        if(mDebugScanLineMask != 0) {
            groupUsage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
        }
        sp<IImageBufferHeap>  pImageBufferHeap =
            pImageStreamBuffer->tryWriteLock(getNodeName());
        if (pImageBufferHeap == NULL) {
            MY_LOGE("ImageBufferHeap == NULL");
            return BAD_VALUE;
        }
        #if 1 // for opaque out image stream add info
        if (streamImg == STREAM_IMG_OUT_OPAQUE) {
            pImageBufferHeap->lockBuf(getNodeName());
            if (OK != OpaqueReprocUtil::setOpaqueInfoToHeap(
                pImageBufferHeap,
                mSensorParams.size,
                mRawFormat,
                mRawStride,
                mRawLength)) {
                MY_LOGW("OUT_OPAQUE setOpaqueInfoToHeap fail");
            };
            pImageBufferHeap->unlockBuf(getNodeName());
        }
        #endif
        // get ImageBuffer from ImageBufferHeap
        sp<IImageBuffer> pImageBuffer = NULL;
        if (streamImg == STREAM_IMG_OUT_OPAQUE ||
            streamImg == STREAM_IMG_IN_OPAQUE) {
            pImageBufferHeap->lockBuf(getNodeName());
            MERROR status = OpaqueReprocUtil::getImageBufferFromHeap(
                    pImageBufferHeap,
                    pImageBuffer);
            pImageBufferHeap->unlockBuf(getNodeName());
            if ( status != OK) {
                MY_LOGE("Cannot get ImageBuffer from opaque ImageBufferHeap");
                return BAD_VALUE;
            }
        } else {
            pImageBuffer = pImageBufferHeap->createImageBuffer();
        }
        if (pImageBuffer == NULL) {
            MY_LOGE("ImageBuffer == NULL");
            return BAD_VALUE;
        } else {
            pImageBuffer->lockBuf(getNodeName(), groupUsage);
            node.streamBufImg[streamImg].spImgBuf = pImageBuffer;
            node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_W;
            node.streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_FRAME;
        }
    }
    //
    if (node.streamBufImg[streamImg].eLockState == STREAM_BUF_LOCK_W) {
        if (node.streamBufImg[streamImg].spImgBuf == NULL) {
            MY_LOGE("stored ImageBuffer is NULL");
            return BAD_VALUE;
        }
        rImgBuf = node.streamBufImg[streamImg].spImgBuf;
    }
    MY_LOGD3("ImgGet-frame, Stream:%d Lock:%d>%d " P1INFO_NODE_STR,
        streamImg, curLock, node.streamBufImg[streamImg].eLockState,
        P1INFO_NODE_VAR(node));
    //
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
frameImagePut(QueNode_T & node, STREAM_IMG const streamImg)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    P1_CHECK_NODE_STREAM(Img, node, streamImg);
    //
    StreamId_T const streamId = mvStreamImg[streamImg]->getStreamId();
    //
    STREAM_BUF_LOCK curLock = node.streamBufImg[streamImg].eLockState;
    //
    #if 1 // keep input stream status
    if (!IS_IN_STREAM_IMG(streamImg))
    #endif
    {
        if (node.exeState == EXE_STATE_REQUESTED ||
            node.exeState == EXE_STATE_PROCESSING ||
            node.exeState == EXE_STATE_DONE) {
            sp<IImageStreamBuffer> pImageStreamBuffer = NULL;
            pImageStreamBuffer = node.streamBufImg[streamImg].spStreamBuf;
            if (pImageStreamBuffer == NULL && // call frameImgInit while NULL
                OK != frameImageInit(node, streamImg, pImageStreamBuffer)) {
                MY_LOGE("get ImageStreamBuffer but NULL");
                return BAD_VALUE;
            }
            pImageStreamBuffer->markStatus((node.needFlush) ?
                STREAM_BUFFER_STATUS::WRITE_ERROR :
                STREAM_BUFFER_STATUS::WRITE_OK);
        }
    }
    //
    if (curLock != STREAM_BUF_LOCK_NONE) {
        if (node.streamBufImg[streamImg].spStreamBuf != NULL) {
            if (node.streamBufImg[streamImg].spImgBuf != NULL) {
                node.streamBufImg[streamImg].spImgBuf->unlockBuf(getNodeName());
                node.streamBufImg[streamImg].spStreamBuf->unlock(getNodeName(),
                    node.streamBufImg[streamImg].spImgBuf->getImageBufferHeap()
                    );
                node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
            } else {
                MY_LOGW("ImageStream locked but no ImageBuffer, Stream:%d:%#" PRIxPTR " "
                    "Lock:%d>%d @%d", streamImg, streamId, curLock,
                    node.streamBufImg[streamImg].eLockState, node.magicNum);
            }
        } else {
            MY_LOGW("ImageStream locked but no StreamBuf, Stream:%d:%#" PRIxPTR " "
                "Lock:%d>%d @%d", streamImg, streamId, curLock,
                node.streamBufImg[streamImg].eLockState, node.magicNum);
        }
    }
    //
    IStreamBufferSet& rStreamBufferSet = node.appFrame->getStreamBufferSet();
    rStreamBufferSet.markUserStatus(
        streamId, getNodeId(),
        #if 0 // if it is not used, only mark RELEASE
        (curLock == STREAM_BUF_LOCK_NONE &&
        node.exeState == EXE_STATE_REQUESTED) ?
        (IUsersManager::UserStatus::RELEASE) :
        #endif
        (IUsersManager::UserStatus::RELEASE | IUsersManager::UserStatus::USED)
    );
    //
    MY_LOGD3("ImgPut-frame, Stream:%d:%#" PRIxPTR " Lock:%d>%d " P1INFO_NODE_STR,
        streamImg, streamId, curLock, node.streamBufImg[streamImg].eLockState,
        P1INFO_NODE_VAR(node));
    node.streamBufImg[streamImg].spImgBuf = NULL;
    node.streamBufImg[streamImg].spStreamBuf = NULL;
    node.streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_NULL;
    //
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
poolImageGet(
    QueNode_T & node, STREAM_IMG const streamImg, sp<IImageBuffer> &rImgBuf)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    //
    MERROR err = OK;
    sp<IImageStreamBufferPoolT> pStreamBufPool = NULL;
    switch (streamImg) {
        case STREAM_IMG_OUT_FULL:
        case STREAM_IMG_OUT_OPAQUE:
            pStreamBufPool = mpStreamPool_full;
            break;
        case STREAM_IMG_OUT_RESIZE:
            pStreamBufPool = mpStreamPool_resizer;
            break;
        case STREAM_IMG_OUT_LCS:
            pStreamBufPool = mpStreamPool_lcso;
            break;
        default:
            MY_LOGE("INVALID POOL %d", streamImg);
            return INVALID_OPERATION;
    };
    if (pStreamBufPool == NULL) {
        MY_LOGE("(%d) StreamBufPool:%d is NULL", node.magicNum, streamImg);
        return BAD_VALUE;
    }
    err = pStreamBufPool->acquireFromPool(
        getNodeName(), node.streamBufImg[streamImg].spStreamBuf, ::s2ns(300));
    if (err != OK) {
        if(err == TIMED_OUT) {
            MY_LOGW("(%d) acquire timeout", node.magicNum);
        } else {
            MY_LOGE("(%d) acquire failed", node.magicNum);
        }
        pStreamBufPool->dumpPool();
        return BAD_VALUE;
    }
    //
    if (node.streamBufImg[streamImg].spStreamBuf == NULL) {
        MY_LOGE("(%d) ImageStreamBuffer:%d is NULL", node.magicNum, streamImg);
        return BAD_VALUE;
    }
    //
    MUINT usage = GRALLOC_USAGE_SW_READ_OFTEN |
        GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    if(mDebugScanLineMask != 0) {
        usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    }
    sp<IImageBufferHeap> pImageBufferHeap =
        node.streamBufImg[streamImg].spStreamBuf->tryWriteLock(getNodeName());
    if (pImageBufferHeap == NULL) {
        MY_LOGE("pImageBufferHeap == NULL");
        return BAD_VALUE;
    }
    rImgBuf = pImageBufferHeap->createImageBuffer();
    if (rImgBuf == NULL) {
        MY_LOGE("pImageBuffer == NULL");
        return BAD_VALUE;
    }
    rImgBuf->lockBuf(getNodeName(), usage);
    node.streamBufImg[streamImg].spImgBuf = rImgBuf;
    node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_W;
    node.streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_POOL;
    //
    MY_LOGD3("ImgGet-pool, streamBuf: %p, heap: %p, buf: %p, usage: 0x%x "
        "streamImg(%d) " P1INFO_NODE_STR,
        node.streamBufImg[streamImg].spStreamBuf.get(),
        rImgBuf->getImageBufferHeap(), rImgBuf.get(), usage, streamImg,
        P1INFO_NODE_VAR(node));
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
poolImagePut(QueNode_T & node, STREAM_IMG const streamImg)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    //
    sp<IImageStreamBufferPoolT> pStreamBufPool = NULL;
    switch (streamImg) {
        case STREAM_IMG_OUT_FULL:
        case STREAM_IMG_OUT_OPAQUE:
            pStreamBufPool = mpStreamPool_full;
            break;
        case STREAM_IMG_OUT_RESIZE:
            pStreamBufPool = mpStreamPool_resizer;
            break;
        case STREAM_IMG_OUT_LCS:
            pStreamBufPool = mpStreamPool_lcso;
            break;
        default:
            MY_LOGE("INVALID POOL %d", streamImg);
            return INVALID_OPERATION;
    };
    if (pStreamBufPool == NULL) {
        MY_LOGE("(%d) StreamBufPool:%d is NULL", node.magicNum, streamImg);
        return BAD_VALUE;
    }
    if (node.streamBufImg[streamImg].eLockState != STREAM_BUF_LOCK_NONE) {
        node.streamBufImg[streamImg].spImgBuf->unlockBuf(getNodeName());
        node.streamBufImg[streamImg].spStreamBuf->unlock(getNodeName(),
            node.streamBufImg[streamImg].spImgBuf->getImageBufferHeap());
    }
    //
    pStreamBufPool->releaseToPool(getNodeName(),
        node.streamBufImg[streamImg].spStreamBuf);
    //
    node.streamBufImg[streamImg].spImgBuf = NULL;
    node.streamBufImg[streamImg].spStreamBuf = NULL;
    node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
    node.streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_NULL;
    MY_LOGD3("ImgPut-pool, streamBuf: state(%d) type(%d) "
        "streamImg(%d) " P1INFO_NODE_STR,
        node.streamBufImg[streamImg].eLockState,
        node.streamBufImg[streamImg].eSrcType, streamImg,
        P1INFO_NODE_VAR(node));
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
stuffImageGet(
    QueNode_T & node, STREAM_IMG const streamImg,
    MSize const dstSize, sp<IImageBuffer> &rImgBuf)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    //
    MERROR err = OK;
    if (streamImg == STREAM_IMG_OUT_OPAQUE) {
        char const* szName = "Hal:Image:P1:OPAQUESTUFFraw";
        err = createStuffBuffer(rImgBuf, szName, mRawFormat,
            MSize(mSensorParams.size.w, dstSize.h), mRawStride);
    } else {
        if (mvStreamImg[streamImg] == NULL) {
            MY_LOGE("(%d) create stuff buffer without stream info (%d)",
                node.magicNum, streamImg);
            return BAD_VALUE;
        }
        err = createStuffBuffer(rImgBuf, mvStreamImg[streamImg], dstSize.h);
    };
    if (err != OK) {
        MY_LOGE("(%d) create stuff buffer with stream info failed",
            node.magicNum);
        return BAD_VALUE;
    };
    MY_LOGD3("ImgGet-stuff, stream(%d) ImgBuf(%p) " P1INFO_NODE_STR, streamImg,
        node.streamBufImg[streamImg].spImgBuf.get(), P1INFO_NODE_VAR(node));
    //
    if (rImgBuf != NULL) {
        node.streamBufImg[streamImg].spImgBuf = rImgBuf;
        node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_W;
        node.streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_STUFF;
    };
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
stuffImagePut(QueNode_T & node, STREAM_IMG const streamImg)
{
    P1INFO_NODE(3, node);
    //
    P1_CHECK_STREAM(IMG, streamImg);
    //
    if (node.streamBufImg[streamImg].spImgBuf == NULL) {
        MY_LOGE("(%d) destroy stuff buffer without ImageBuffer (%d)",
            node.magicNum, streamImg);
        return BAD_VALUE;
    }
    if (node.streamBufImg[streamImg].eLockState == STREAM_BUF_LOCK_NONE) {
        MY_LOGI("(%d) destroy stuff buffer skip", node.magicNum);
        return BAD_VALUE;
    }
    //
    MY_LOGD3("ImgPut-stuff, stream(%d) ImgBuf(%p) " P1INFO_NODE_STR, streamImg,
        node.streamBufImg[streamImg].spImgBuf.get(), P1INFO_NODE_VAR(node));
    //
    destroyStuffBuffer(node.streamBufImg[streamImg].spImgBuf);
    //
    node.streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
    node.streamBufImg[streamImg].spImgBuf = NULL;
    node.streamBufImg[streamImg].spStreamBuf = NULL;
    node.streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_NULL;
    //
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeImp::
findRequestStream(QueNode_T & node)
{
    if (node.appFrame == NULL) {
        return OK;
    }
    IPipelineFrame::InfoIOMapSet rIOMapSet;
    if(OK != node.appFrame->queryInfoIOMapSet(getNodeId(), rIOMapSet)) {
        MY_LOGE("queryInfoIOMap failed");
        return BAD_VALUE;
    }
    //
    IPipelineFrame::ImageInfoIOMapSet& imageIOMapSet =
                                            rIOMapSet.mImageInfoIOMapSet;
    if(imageIOMapSet.isEmpty()) {
        MY_LOGW("no imageIOMap in frame");
        return BAD_VALUE;
    }
    //
    IPipelineFrame::MetaInfoIOMapSet& metaIOMapSet =
                                            rIOMapSet.mMetaInfoIOMapSet;
    if(metaIOMapSet.isEmpty()) {
        MY_LOGW("no metaIOMap in frame");
        return BAD_VALUE;
    }
    //
    #define REG_STREAM_IMG(id, item)    \
        if ((!node.streamBufImg[item].bExist) &&\
            (mvStreamImg[item] != NULL) &&\
            (mvStreamImg[item]->getStreamId() == id)) {\
            node.streamBufImg[item].bExist |= MTRUE; continue; };
    #define REG_STREAM_META(id, item)    \
        if ((!node.streamBufMeta[item].bExist) &&\
            (mvStreamMeta[item] != NULL) &&\
            (mvStreamMeta[item]->getStreamId() == id)) {\
            node.streamBufMeta[item].bExist |= MTRUE; continue; };
    //
    for (size_t i = 0; i < imageIOMapSet.size(); i++) {
        IPipelineFrame::ImageInfoIOMap const& imageIOMap = imageIOMapSet[i];
        if (imageIOMap.vIn.size() > 0) {
            for (size_t j = 0; j < imageIOMap.vIn.size(); j++) {
                StreamId_T const streamId = imageIOMap.vIn.keyAt(j);
                REG_STREAM_IMG(streamId, STREAM_IMG_IN_YUV);
                REG_STREAM_IMG(streamId, STREAM_IMG_IN_OPAQUE);
            }
        }
        if (imageIOMap.vOut.size() > 0) {
            for (size_t j = 0; j < imageIOMap.vOut.size(); j++) {
                StreamId_T const streamId = imageIOMap.vOut.keyAt(j);
                REG_STREAM_IMG(streamId, STREAM_IMG_OUT_OPAQUE);
                REG_STREAM_IMG(streamId, STREAM_IMG_OUT_FULL);
                REG_STREAM_IMG(streamId, STREAM_IMG_OUT_RESIZE);
                REG_STREAM_IMG(streamId, STREAM_IMG_OUT_LCS);
            }
        }
    }
    //
    for (size_t i = 0; i < metaIOMapSet.size(); i++) {
        IPipelineFrame::MetaInfoIOMap const& metaIOMap = metaIOMapSet[i];
        if (metaIOMap.vIn.size() > 0) {
            for (size_t j = 0; j < metaIOMap.vIn.size(); j++) {
                StreamId_T const streamId = metaIOMap.vIn.keyAt(j);
                REG_STREAM_META(streamId, STREAM_META_IN_APP);
                REG_STREAM_META(streamId, STREAM_META_IN_HAL);
            }
        }
        if (metaIOMap.vOut.size() > 0) {
            for (size_t j = 0; j < metaIOMap.vOut.size(); j++) {
                StreamId_T const streamId = metaIOMap.vOut.keyAt(j);
                REG_STREAM_META(streamId, STREAM_META_OUT_APP);
                REG_STREAM_META(streamId, STREAM_META_OUT_HAL);
            }
        }
    }
    //
    #undef REG_STREAM_IMG
    #undef REG_STREAM_META
    //
    #if (IS_P1_LOGD)
    if (mLogLevel > 1) {
        android::String8 strInfo("");
        strInfo += String8::format("(%d) IMG[", node.magicNum);
        for (int stream = STREAM_ITEM_START; stream < STREAM_IMG_NUM;
            stream++) {
            strInfo +=
                String8::format("%d ", node.streamBufImg[stream].bExist);
        };
        strInfo += String8::format("] META[");
        for (int stream = STREAM_ITEM_START; stream < STREAM_META_NUM;
            stream++) {
            strInfo +=
                String8::format("%d ", node.streamBufMeta[stream].bExist);
        };
        strInfo += String8::format("]");
        MY_LOGD2("%s", strInfo.string());
    }
    #endif
    //
    return OK;
};

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::DeliverMgr::
deliverLoop() {
    MBOOL res = MTRUE;
    Vector<QueNode_T> outQueue;

    {
        Mutex::Autolock _l(mDeliverLock);
        MINT32 currentNum = 0;
        //
        mLoopState = LOOP_STATE_INIT;
        currentNum = (mNodeQueue.empty()) ? (0) :
            ((mNodeQueue.end() - 1)->magicNum);
        if (!exitPending()) {
            //MY_LOGD("deliverLoop Num(%d == %d)", currentNum, mSentNum);
            if (currentNum == mSentNum) {
                mLoopState = LOOP_STATE_WAITING;
                MY_LOGD_IF(mLogLevel > 1, "deliverLoop wait ++");
                mDeliverCond.wait(mDeliverLock);
                MY_LOGD_IF(mLogLevel > 1, "deliverLoop wait --");
            } // else , there is new coming node need to check
        } else {
            MY_LOGD("deliverLoop need to exit");
        }
        //
        mLoopState = LOOP_STATE_PROCESSING;
        mSentNum = currentNum;
        //
        //dumpNumList();
        //dumpNodeQueue();
        //
        List<MINT32>::iterator it_list = mNumList.begin();
        Vector<QueNode_T>::iterator it_node = mNodeQueue.begin();
        MBOOL isFound = MFALSE;
        size_t i = mNumList.size();
        //MY_LOGD_IF(mLogLevel > 1, "mNodeQueue(%d) +++", mNodeQueue.size());
        for (; it_list != mNumList.end() && i > 0; i--) {
            isFound = MFALSE;
            it_node = mNodeQueue.begin();
            for(; it_node != mNodeQueue.end(); ) {
                if (it_node->magicNum == (MUINT32)(*it_list)) {
                    outQueue.push_back(*it_node);
                    it_node = mNodeQueue.erase(it_node);
                    isFound = MTRUE;
                    break;
                } else {
                    it_node++;
                }
            }
            if (isFound) {
                it_list = mNumList.erase(it_list);
            } else {
                break;
            }
        }
        //MY_LOGD_IF(mLogLevel > 1, "mNodeQueue(%d) ---", mNodeQueue.size());
    }
    //
    if (outQueue.empty()) {
        //MY_LOGD_IF(mLogLevel > 1, "there is no node need to deliver");
    } else {
        Vector<QueNode_T>::iterator it = outQueue.begin();
        //MY_LOGD_IF(mLogLevel > 1, "outQueue.size(%d)", outQueue.size());
        for(; it != outQueue.end(); it++) {
            //MY_LOGD_IF(mLogLevel > 1, "it->magicNum : (%d)", it->magicNum);
            mpP1NodeImp->releaseNode(*it);
        }
        outQueue.clear();
    }

    {
        Mutex::Autolock _l(mDeliverLock);
        mLoopState = LOOP_STATE_DONE;
        mDoneCond.broadcast();
    }

    return res;
};

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::DeliverMgr::
registerNodeList(MINT32 num)
{
    Mutex::Autolock _l(mDeliverLock);
    if (!mLoopRunning) {
        return MFALSE;
    }
    mNumList.push_back(num);
    return (!mNumList.empty());
};

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeImp::onReturnFrame(QueNode_T & node, MBOOL isFlush, MBOOL isTrigger)
{
    node.needFlush = isFlush;
    if (node.needFlush) {
        MY_LOGD("need flush node " P1INFO_NODE_STR, P1INFO_NODE_VAR(node));
    };
    //
    if (mpDeliverMgr != NULL && mpDeliverMgr->runningGet()) {
        mpDeliverMgr->sendNodeQueue(node, isTrigger);
    } else {
        releaseNode(node);
    }
    return;
};

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::DeliverMgr::
sendNodeQueue(QueNode_T node, MBOOL needTrigger)
{
    {
        Mutex::Autolock _l(mDeliverLock);
        mNodeQueue.push_back(node);
    }
    if (needTrigger) {
        trigger();
    };
    //
    return MTRUE;
};

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::DeliverMgr::
waitFlush(MBOOL needTrigger)
{
    size_t queueSize = 0;
    MBOOL isListEmpty = MFALSE;
    if (!runningGet()) {
        return MTRUE;
    };
    //
    {
        Mutex::Autolock _l(mDeliverLock);
        queueSize = mNodeQueue.size();
        isListEmpty = (mNumList.size() > 0) ? (MFALSE) : (MTRUE);
    }
    //MY_LOGI("QueSize(%d) ListEmpty(%d) - start", queueSize, isListEmpty);
    while (queueSize > 0) {
        if (needTrigger) {
            trigger();
        }
        {
            Mutex::Autolock _l(mDeliverLock);
            if ((mNodeQueue.size() > 0 && mLoopState == LOOP_STATE_WAITING) ||
                mLoopState == LOOP_STATE_PROCESSING) {
                MY_LOGD("doneLoop wait ++");
                mDoneCond.wait(mDeliverLock);
                MY_LOGD("doneLoop wait --");
            }
            queueSize = mNodeQueue.size();
            isListEmpty = (mNumList.size() > 0) ? (MFALSE) : (MTRUE);
        }
        //MY_LOGI("QueSize(%d) ListEmpty(%d) - next", queueSize, isListEmpty);
    };
    //MY_LOGI("QueSize(%d) ListEmpty(%d) - end", queueSize, isListEmpty);
    //
    if (!isListEmpty) {
        MY_LOGW("ListEmpty(%d)", isListEmpty);
        dumpNumList(MTRUE);
        dumpNodeQueue(MTRUE);
        return MFALSE;
    }
    return MTRUE;
};

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeImp::DeliverMgr::
trigger(void)
{
    Mutex::Autolock _l(mDeliverLock);
    /*if (!mNodeQueue.empty())*/
    if (mLoopRunning) {
        MY_LOGD_IF(mLogLevel > 1, "DeliverMgr trigger (%zu)", mNodeQueue.size());
        mDeliverCond.broadcast();
    }
    return MTRUE;
};

/******************************************************************************
 *
 ******************************************************************************/
void
P1NodeImp::DeliverMgr::
dumpNumList(MBOOL isLock)
{
    String8 str("");
    size_t size = 0;
    //
    NEED_LOCK(isLock, mDeliverLock);
    //
    size= mNumList.size();
    List<MINT32>::iterator it = mNumList.begin();
    for (; it != mNumList.end(); it++) {
        str += String8::format("%d ", *it);
    }
    //
    NEED_UNLOCK(isLock, mDeliverLock);
    //
    MY_LOGI("dump NumList[%zu] = {%s}", size, str.string());
};

/******************************************************************************
 *
 ******************************************************************************/
void
P1NodeImp::DeliverMgr::
dumpNodeQueue(MBOOL isLock)
{
    String8 str("");
    size_t size = 0;
    //
    NEED_LOCK(isLock, mDeliverLock);
    //
    size = mNodeQueue.size();
    Vector<QueNode_T>::iterator it = mNodeQueue.begin();
    for(; it != mNodeQueue.end(); it++) {
        str += String8::format("%d ", (*it).magicNum);
    }
    //
    NEED_UNLOCK(isLock, mDeliverLock);
    //
    MY_LOGI("dump NodeQueue[%zu] = {%s}", size, str.string());
};

/******************************************************************************
 *
 ******************************************************************************/
sp<P1Node>
P1Node::
createInstance()
{
    return new P1NodeImp();

}


