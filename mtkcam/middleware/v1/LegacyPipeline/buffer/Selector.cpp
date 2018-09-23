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

#define LOG_TAG "MtkCam/Selector"
//
#include "MyUtils.h"
//
#include <utils/RWLock.h>
//
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/Selector.h>
//
#include <camera_custom_zsd.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#include <mtkcam/drv/IHalSensor.h>


using namespace android;
using namespace NSCam;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;
using namespace NSCam::Utils;

using namespace NSCam::v3::Utils;
/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s] " fmt, __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

#if 1
#define FUNC_START     MY_LOGD("+")
#define FUNC_END       MY_LOGD("-")
#else
#define FUNC_START
#define FUNC_END
#endif

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

/******************************************************************************
 *
 ******************************************************************************/
ZsdSelector::
ZsdSelector()
    : mDelayTimeMs(0)
    , mTolerationMs(0)
    , mIsBackupQueue(MFALSE)
    , mIsNeedWaitAfDone(MTRUE)
    , mIsAlreadyStartedCapture(MFALSE)
    , mInactiveCount(0)
{
    //
    mDelayTimeMs = get_zsd_cap_stored_delay_time_ms();
    mTolerationMs = get_zsd_cap_stored_delay_toleration_ms();
    //
    MY_LOGD("new ZsdSelector delay(%d) toler(%d)",mDelayTimeMs,mTolerationMs);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT64
ZsdSelector::
getTimeStamp(Vector<MetaItemSet> &rvResult)
{
    MINT64 newTimestamp = 0;
    for( size_t i = 0; i < rvResult.size(); i++ )
    {
        IMetadata::IEntry const entry = rvResult[i].meta.entryFor(MTK_SENSOR_TIMESTAMP);
        if(! entry.isEmpty())
        {
            newTimestamp = entry.itemAt(0, Type2Type<MINT64>());
            break;
        }
    }
    return newTimestamp;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ZsdSelector::
getAfInfo(Vector<MetaItemSet> &rvResult, MUINT8 &afMode, MUINT8 &afState, MUINT8 &lensState)
{
    MBOOL ret = MTRUE;
    afState = 0;
    afMode = 0;
    lensState = 0;
    bool bFind = false;
    //
    bFind = false;
    for( size_t i = 0; i < rvResult.size(); i++ )
    {
        IMetadata::IEntry const entry1 = rvResult[i].meta.entryFor(MTK_CONTROL_AF_STATE);
        IMetadata::IEntry const entry2 = rvResult[i].meta.entryFor(MTK_CONTROL_AF_MODE);
        IMetadata::IEntry const entry3 = rvResult[i].meta.entryFor(MTK_LENS_STATE);
        if(! entry1.isEmpty() && ! entry2.isEmpty() && ! entry3.isEmpty())
        {
            afState = entry1.itemAt(0, Type2Type<MUINT8>());
            afMode = entry2.itemAt(0, Type2Type<MUINT8>());
            lensState = entry3.itemAt(0, Type2Type<MUINT8>());
            bFind = true;
            break;
        }
    }
    if(!bFind)
    {
        MY_LOGW("Can't Find MTK_CONTROL_AF_STATE or MTK_CONTROL_AF_MODE or MTK_LENS_STATE");
        ret = MFALSE;
    }
    //
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
ZsdSelector::
updateQueueBuffer(MINT64 newTimestamp , int maxUsingBufferNum)
{
    if(mResultSetMap.size()==0)
    {
        MY_LOGD_IF( (2<=mLogLevel), "no buffer, immediately return");
        return;
    }
    //
    if( mIsBackupQueue )
    {
        //only reserve one buffer for backup
        int n = mResultSetMap.size();
        if( n > 1 )
        {
            MY_LOGD_IF( (2<=mLogLevel), "BackkupQueue size(%d) > 1, need to remove", n);
            for(int i=0; i<n-1; i++)
            {
                returnBuffersToPool( mResultSetMap.editItemAt(0).bufferSet);
                mResultSetMap.removeItemsAt(0);
            }
        }
    }
    else
    {
        //update and check all duration
        int n = mResultSetMap.size();
        int stop = -1;
        for( int i = 0; i < n; i++ )
        {
            MINT32 duration = newTimestamp - mResultSetMap.editItemAt(i).timestamp;
            mResultSetMap.editItemAt(i).duration = duration;
            //
            MY_LOGD_IF( (2<=mLogLevel), "que[%d](%s) Timestamp(%lld) duration(%d)",
                i, ISelector::logBufferSets(mResultSetMap.editItemAt(i).bufferSet),
                mResultSetMap.editItemAt(i).timestamp, duration/1000000);
            //
            if( duration >= (mDelayTimeMs + mTolerationMs)*1000000 )
            {
                stop = i;
            }
        }
        //
        MY_LOGD_IF( (2<=mLogLevel), "stop(%d)",stop);
        //
        //remove too old buffer
        for(int i=0; i<=stop; i++)
        {
            returnBuffersToPool( mResultSetMap.editItemAt(0).bufferSet);
            mResultSetMap.removeItemsAt(0);
        }
        //
        //remove buffer to be under maxUsingBufferNum
        int del_num = (mResultSetMap.size() + mvUsingBufSet.size()) - maxUsingBufferNum + 1;
        //
        MY_LOGD_IF( (2<=mLogLevel), "del_num(%d) = (que size(%d) + using(%d)) - max(%d) + 1",
            del_num, mResultSetMap.size(), mvUsingBufSet.size(), maxUsingBufferNum);
        //
        if( del_num > (int)mResultSetMap.size() )
        {
            del_num = mResultSetMap.size();
        }
        if( del_num > 0 )
        {
            for(int i=0; i<del_num; i++)
            {
                returnBuffersToPool( mResultSetMap.editItemAt(0).bufferSet);
                mResultSetMap.removeItemsAt(0);
            }
        }
    }
}

/******************************************************************************
*
******************************************************************************/
MBOOL
ZsdSelector::
isOkBuffer(MUINT8 afMode, MUINT8 afState, MUINT8 lensState)
{
    if( mIsNeedWaitAfDone )
    {
        if( afMode == MTK_CONTROL_AF_MODE_AUTO ||
            afMode == MTK_CONTROL_AF_MODE_MACRO )
        {
            mInactiveCount = 0;
            // INACTIVE CASE:
            // IN SCAN CASE:
            if( (afState == MTK_CONTROL_AF_STATE_INACTIVE && mIsAlreadyStartedCapture == MFALSE) ||
            afState == MTK_CONTROL_AF_STATE_PASSIVE_SCAN ||
            afState == MTK_CONTROL_AF_STATE_ACTIVE_SCAN  ||
            lensState == MTK_LENS_STATE_MOVING )
        {
                MY_LOGD_IF( (2<=mLogLevel), "afMode(%d) afState(%d) lensState(%d) startCap(%d) Not OK", afMode, afState, lensState, mIsAlreadyStartedCapture );
                return MFALSE;
            }
            //
            // SCAN DONE CASE:
            MY_LOGD_IF( (2<=mLogLevel), "afMode(%d) afState(%d) lensState(%d) startCap(%d) is OK", afMode, afState, lensState, mIsAlreadyStartedCapture );
        }
        else if( afMode == MTK_CONTROL_AF_MODE_CONTINUOUS_VIDEO ||
                 afMode == MTK_CONTROL_AF_MODE_CONTINUOUS_PICTURE )
        {
            // INACTIVE CASE:
#define MAX_WAIT_INACTIVE_NUM (5)
            if( afState == MTK_CONTROL_AF_STATE_INACTIVE )
            {
                mInactiveCount++;
                //
                if( mInactiveCount >= MAX_WAIT_INACTIVE_NUM )
                {
                    MY_LOGW("afMode(%d) AF in INACTIVE state over (%d) frame, count(%d), the buffer wiil be used!", afMode, MAX_WAIT_INACTIVE_NUM, mInactiveCount );
                    return MTRUE;
                }
                else    //MTK_CONTROL_AF_MODE_OFF or MTK_CONTROL_AF_MODE_EDOF
                {
                    MY_LOGD_IF( (2<=mLogLevel), "afMode(%d) afState(%d) is INACTIVE, count(%d)", afMode, afState, mInactiveCount );
            return MFALSE;
        }
    }
    else
    {
                mInactiveCount = 0;
    }
#undef MAX_WAIT_INACTIVE_NUM
            //
            // IN SCAN CASE:
            if( afState == MTK_CONTROL_AF_STATE_PASSIVE_SCAN ||
                afState == MTK_CONTROL_AF_STATE_ACTIVE_SCAN  ||
                lensState == MTK_LENS_STATE_MOVING )
            {
                MY_LOGD_IF( (2<=mLogLevel), "afMode(%d) afState(%d) lensState(%d) Not OK", afMode, afState, lensState );
                return MFALSE;
            }
            //
            // SCAN DONE CASE:
            MY_LOGD_IF( (2<=mLogLevel), "afMode(%d) afState(%d) lensState(%d) is OK", afMode, afState, lensState );
        }
        else
        {
            mInactiveCount = 0;
            MY_LOGD_IF( (2<=mLogLevel), "afState(%d) is OK", afState );
        }
    }
    else
    {
        mInactiveCount = 0;
        MY_LOGD_IF( (2<=mLogLevel), "No need to see afMode(%d) afState(%d), it is OK", afMode, afState );
    }
    //
    return MTRUE;
}

/******************************************************************************
*
******************************************************************************/
MVOID
ZsdSelector::removeExistBackupBuffer()
{
    if( mIsBackupQueue == MTRUE )
    {
        int n = mResultSetMap.size();
        if( n > 0 )
        {
            MY_LOGD_IF( (2<=mLogLevel), "removeExistBackupBuffer size(%d)", n);
            for(int i=0; i<n; i++)
            {
                returnBuffersToPool( mResultSetMap.editItemAt(0).bufferSet);
                mResultSetMap.removeItemsAt(0);
            }
        }
        mIsBackupQueue = MFALSE;
    }
}

/******************************************************************************
*
******************************************************************************/
MVOID
ZsdSelector::
insertOkBuffer(ResultSet_T &resultSet)
{
    MY_LOGD_IF( (2<=mLogLevel), "insert Ok Buffer buf(%s) timestamp(%lld) duration(%d) afState(%d)",
        ISelector::logBufferSets(resultSet.bufferSet), resultSet.timestamp,resultSet.duration,resultSet.afState);
    //
    mIsBackupQueue = MFALSE;
    mResultSetMap.push_back(resultSet);
}

/******************************************************************************
*
******************************************************************************/
MVOID
ZsdSelector::
insertBackupBuffer(ResultSet_T &resultSet)
{
    MY_LOGD_IF( (2<=mLogLevel), "insert Backup Buffer buf(%s) timestamp(%lld) duration(%d) afState(%d)",
        ISelector::logBufferSets(resultSet.bufferSet), resultSet.timestamp,resultSet.duration,resultSet.afState);
    //
    mIsBackupQueue = MTRUE;
    mResultSetMap.push_back(resultSet);
}

/******************************************************************************
 *
 ******************************************************************************/
android::status_t
ZsdSelector::
selectResult(
    MINT32                             rRequestNo,
    Vector<MetaItemSet>                rvResult,
    Vector<BufferItemSet>              rvBuffers,
    MBOOL                              errorResult
)
{
    Mutex::Autolock _l(mResultSetLock);
    //
    if( errorResult )
    {
        MY_LOGW("don't reserved errorResult(1) requestNo(%d)", rRequestNo);
        //
        returnBuffersToPool(rvBuffers);
        return UNKNOWN_ERROR;
    }
    //
    // raw dump
    if(mNeedDump)
    {
        MY_LOGD("sendDataToDumpThread start",mNeedDump);
        sendDataToDumpThread(rRequestNo, rvResult, rvBuffers);
        MY_LOGD("sendDataToDumpThread end",mNeedDump);
    }
    // get timestamp
    MINT64 newTimestamp = getTimeStamp(rvResult);
    if( newTimestamp == 0 )
    {
        MY_LOGW("timestamp == 0");
    }
    //
    // get AF Info
    MUINT8 afMode;
    MUINT8 afState;
    MUINT8 lensState;
    if( !getAfInfo(rvResult, afMode, afState, lensState) )
    {
        MY_LOGW("getAfState Fail!");
    }
    //
    //Update all queue buffer status & remove too old buffer
#define MAX_USING_BUFFER (4)
    updateQueueBuffer(newTimestamp, MAX_USING_BUFFER);
#undef MAX_USING_BUFFER
    //
    if( isOkBuffer(afMode, afState, lensState) )
    {
        if( mIsBackupQueue )
        {
            removeExistBackupBuffer();
        }
        //
        ResultSet_T resultSet = {static_cast<MINT32>(rRequestNo), rvBuffers, rvResult, newTimestamp, 0, afState};
        insertOkBuffer(resultSet);
        //
        mCondResultSet.signal();
    }
    else
    {
        if ( mResultSetMap.isEmpty() )
        {
            ResultSet_T resultSet = {static_cast<MINT32>(rRequestNo), rvBuffers, rvResult, newTimestamp, 0, afState};
            insertBackupBuffer(resultSet);
        }
        else if( mIsBackupQueue )
        {
            removeExistBackupBuffer();
            //
            ResultSet_T resultSet = {static_cast<MINT32>(rRequestNo), rvBuffers, rvResult, newTimestamp, 0, afState};
            insertBackupBuffer(resultSet);
        }
        else
        {
            returnBuffersToPool(rvBuffers);
        }
    }
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
ZsdSelector::
getResult(
    MINT32&                          rRequestNo,
    Vector<MetaItemSet>&             rvResultMeta,
    Vector<BufferItemSet>&           rvBuffers
)
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);
    MY_LOGD("mResultSetMap size(%d), mIsBackupQueue(%d)",mResultSetMap.size(),mIsBackupQueue);
    if( mResultSetMap.isEmpty() ||
        mIsBackupQueue)
    {
#define WAIT_BUFFER_TIMEOUT (3000000000) //ns
        MY_LOGD("Ready to wait condition for %lld ns",WAIT_BUFFER_TIMEOUT);
        MERROR err = mCondResultSet.waitRelative(mResultSetLock, WAIT_BUFFER_TIMEOUT);
#undef WAIT_BUFFER_TIMEOUT
        if ( err!= OK )
        {
            MY_LOGW("Timeout, no OK result can get");
        }
    }
    //
    if( mResultSetMap.isEmpty() )
    {
        MY_LOGE("mResultSetMap is empty!!");
        mIsAlreadyStartedCapture = MFALSE;
        return UNKNOWN_ERROR;
    }
    //
    rRequestNo   = mResultSetMap.editItemAt(0).requestNo;
    rvResultMeta = mResultSetMap.editItemAt(0).resultMeta;
    rvBuffers.clear();
    rvBuffers.appendVector(mResultSetMap.editItemAt(0).bufferSet);
    //

    MY_LOGD("get result Idx(%d) rNo(%d) buffer(%s) timestamp(%lld) duration(%d) afState(%d)",
        0, mResultSetMap.editItemAt(0).requestNo, ISelector::logBufferSets(rvBuffers),
        mResultSetMap.editItemAt(0).timestamp,
        mResultSetMap.editItemAt(0).duration,
        mResultSetMap.editItemAt(0).afState);
    //
    if( mResultSetMap.editItemAt(0).afState == MTK_CONTROL_AF_STATE_INACTIVE )
    {
        MY_LOGW("the buffer AF state is INACTIVE!");
    }
    //
    mResultSetMap.removeItemsAt(0);
    mvUsingBufSet.push_back(rvBuffers);
    mIsBackupQueue = MFALSE;
    mIsAlreadyStartedCapture = MFALSE;
    //
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
ZsdSelector::
returnBuffer(
    BufferItemSet&   rBuffer
)
{
    Mutex::Autolock _l(mResultSetLock);

    // remove buffer item in mvUsingBufSet
    for (size_t i = 0; i < mvUsingBufSet.size() ; i++)
    {
        size_t ind = 0;
        MBOOL find = MFALSE;
        for (size_t j = 0 ; j < mvUsingBufSet[i].size() ; j++)
        {
            if(mvUsingBufSet[i][j].id == rBuffer.id && mvUsingBufSet[i][j].heap.get() == rBuffer.heap.get())
            {
                ind = j;
                find = MTRUE;
                break;
            }
        }

        if (find)
        {
            mvUsingBufSet.editItemAt(i).removeAt(ind);
            if (mvUsingBufSet[i].size() == 0)
            {
                mvUsingBufSet.removeAt(i);
            }
            break;
        }
    }

    return returnBufferToPool(rBuffer);
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
ZsdSelector::
flush()
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);

    if ( !mResultSetMap.isEmpty()) {
#if 0
        sp<IConsumerPool> pPool = mpPool.promote();
        if ( pPool == 0 ) {
            MY_LOGE("Cannot promote consumer pool to return buffer.");
            FUNC_END;
            return UNKNOWN_ERROR;
        }
#endif

        for ( size_t i = 0; i < mResultSetMap.size(); ++i ) {
            returnBuffersToPool(mResultSetMap.editItemAt(i).bufferSet);
        }

        mResultSetMap.clear();
    }

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
void
ZsdSelector::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;
    flush();
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
void
ZsdSelector::
setDelayTime( MINT32 delayTimeMs, MINT32 tolerationTimeMs )
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);
    mDelayTimeMs = delayTimeMs;
    mTolerationMs = tolerationTimeMs;
    MY_LOGD("Set DelayTime (%d) ms Toleration (%d) ms", mDelayTimeMs, mTolerationMs);
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
android::status_t
ZsdSelector::
sendCommand( MINT32 cmd,
                 MINT32 arg1,
                 MINT32 arg2,
                 MINT32 arg3,
                 MVOID* arg4 )
{
    bool ret = true;
    //
    MY_LOGD("Cmd:(%d) arg1(%d) arg2(%d) arg3(%d) arg4(%p)",cmd, arg1, arg2, arg3, arg4);
    //
    switch  (cmd)
    {
        case eCmd_setNeedWaitAfDone:
            {
                Mutex::Autolock _l(mResultSetLock);
                mIsNeedWaitAfDone = MTRUE;
            }
            break;
        case eCmd_setNoWaitAfDone:
            {
                Mutex::Autolock _l(mResultSetLock);
                mIsNeedWaitAfDone = MFALSE;
            }
            break;
        case eCmd_setAlreadyStartedCapture:
            {
                Mutex::Autolock _l(mResultSetLock);
                mIsAlreadyStartedCapture = MTRUE;
            }
            break;
        default:
            break;
    }
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
ZsdFlashSelector::
ZsdFlashSelector()
{
    MY_LOGD("new ZsdFlashSelector");
}

/******************************************************************************
 *
 ******************************************************************************/
android::status_t
ZsdFlashSelector::
selectResult(
    MINT32                             rRequestNo,
    Vector<MetaItemSet>                rvResult,
    Vector<BufferItemSet>              rvBuffers,
    MBOOL                              errorResult
)
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);

    MY_LOGD("In ZsdFlashSelector selectResult error:%d rRequestNo(%d) %s", errorResult, rRequestNo, ISelector::logBufferSets(rvBuffers));

    if(mWaitRequestNo == rRequestNo)
    {
        if( mResultHeapSet.size() != 0 )
        {
            returnBuffersToPool(mResultHeapSet);
        }
        mResultHeapSet = rvBuffers;
        mResultMeta = rvResult;
        MY_LOGD("find data : rRequestNo(%d) rvResult size(%d) resultHeap(%s)",rRequestNo, rvResult.size(), ISelector::logBufferSets(mResultHeapSet));
        mResultQueueCond.broadcast();
    }
    else
    {
        returnBuffersToPool(rvBuffers);
    }
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
ZsdFlashSelector::
getResult(
    MINT32&                          rRequestNo,
    Vector<MetaItemSet>&             rvResultMeta,
    Vector<BufferItemSet>&           rvBuffers
)
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);

    MY_LOGD("In ZsdFlashSelector getResult waitRequestNo(%d)",mWaitRequestNo);

    nsecs_t timeout = 3000000000LL; //wait for 3 sec
    status_t status = mResultQueueCond.waitRelative(mResultSetLock,timeout);
    if(status != OK)
    {
        MY_LOGE("wait result timeout...");
        FUNC_END;
        return status;
    }

    rRequestNo   = mWaitRequestNo;
    rvResultMeta = mResultMeta;
    rvBuffers.clear();
    rvBuffers.appendVector(mResultHeapSet);
    //
    mResultHeapSet.clear();

    FUNC_END;
    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
ZsdFlashSelector::
returnBuffer(
    BufferItemSet&    rBuffer
)
{
    Mutex::Autolock _l(mResultSetLock);
    MY_LOGD("In ZsdFlashSelector");
    return returnBufferToPool(rBuffer);
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
ZsdFlashSelector::
flush()
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);
    MY_LOGD("In ZsdFlashSelector");

    if ( mResultHeapSet.size() != 0 ) {
        returnBuffersToPool(mResultHeapSet);
        mResultHeapSet.clear();
    }

    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
void
ZsdFlashSelector::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;
    MY_LOGD("In ZsdFlashSelector");
    flush();
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
void
ZsdFlashSelector::
setWaitRequestNo(MINT32 requestNo)
{
    MY_LOGD("setWaitRequestNo(%d)",requestNo);
    mWaitRequestNo = requestNo;
}


/******************************************************************************
 *
 ******************************************************************************/
VssSelector::
VssSelector()
{
}

/******************************************************************************
 *
 ******************************************************************************/
android::status_t
VssSelector::
selectResult(
    MINT32                             rRequestNo,
    Vector<MetaItemSet>                rvResult,
    Vector<BufferItemSet>              rvBuffers,
    MBOOL                              /*errorResult*/
)
{
    MY_LOGD("VSS selectResult rNo(%d)",rRequestNo);
    Mutex::Autolock _l(mResultSetLock);

    mResultSetMap.add( ResultSet_T{static_cast<MINT32>(rRequestNo), rvBuffers, rvResult} );

    if(mNeedDump)
    {
        MY_LOGD("sendDataToDumpThread start",mNeedDump);
        sendDataToDumpThread(rRequestNo, rvResult, rvBuffers);
        MY_LOGD("sendDataToDumpThread end",mNeedDump);
        returnBuffersToPool(rvBuffers);
    }

    MY_LOGD_IF( mLogLevel >= 1, "mResultSetMap.size:%d", mResultSetMap.size());
#if 1
    for ( size_t i = 0; i < mResultSetMap.size(); ++i )
        MY_LOGD_IF( mLogLevel >= 1, "mResultSetMap.size:%d request:%d", mResultSetMap.size(), mResultSetMap.editItemAt(i).requestNo );
#endif
    mCondResultSet.signal();
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
VssSelector::
getResult(
    MINT32&                          rRequestNo,
    Vector<MetaItemSet>&             rvResultMeta,
    Vector<BufferItemSet>&           rvBuffers
)
{

    Mutex::Autolock _l(mResultSetLock);
#define WAIT_BUFFER_TIMEOUT (3000000000) //ns
    if ( mResultSetMap.isEmpty() )
    {
        MY_LOGD("Wait result - E");
        mCondResultSet.waitRelative(mResultSetLock, WAIT_BUFFER_TIMEOUT);
        MY_LOGD("Wait result - X");
        if(mResultSetMap.isEmpty())
        {
            MY_LOGE("Time Out, no result can get.");
            return UNKNOWN_ERROR;
        }
    }
#undef WAIT_BUFFER_TIMEOUT
    rRequestNo   = mResultSetMap.editItemAt(0).requestNo;
    rvResultMeta = mResultSetMap.editItemAt(0).resultMeta;
    rvBuffers.clear();
    rvBuffers.appendVector(mResultSetMap.editItemAt(0).bufferSet);

    mResultSetMap.removeItemsAt(0);

    return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
status_t
VssSelector::
returnBuffer(
    BufferItemSet&    rBuffer
)
{
    Mutex::Autolock _l(mResultSetLock);
    return returnBufferToPool(rBuffer);
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
VssSelector::
flush()
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);

    if ( !mResultSetMap.isEmpty()) {

        for ( size_t i = 0; i < mResultSetMap.size(); ++i ) {
            returnBuffersToPool(mResultSetMap.editItemAt(i).bufferSet);
        }

        mResultSetMap.clear();
    }
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
void
VssSelector::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;
    flush();
    FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/

#define RAW_DUMP_FOLDER_PATH "/sdcard/rawdump/"
#define MAX_DUMPCNT 200

/******************************************************************************
 *
 ******************************************************************************/
SimpleDumpBufferSelector::
SimpleDumpBufferSelector()
{
    mLastStartId = property_get_int32("rawDump.selector.start", 0);
    mNeedDump = MFALSE;
    mEnableRawDump = MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/
SimpleDumpBufferSelector::
~SimpleDumpBufferSelector()
{
    sp<ISimpleRawDumpCmdQueThread> pSimpleRawDumpCmdQueThread = mpSimpleRawDumpCmdQueThread;
    if ( pSimpleRawDumpCmdQueThread  != 0 ) {
        MY_LOGD(
            "RawDumpCmdQ Thread: (tid, getStrongCount)=(%d, %d)",
            pSimpleRawDumpCmdQueThread->getTid(), pSimpleRawDumpCmdQueThread->getStrongCount()
        );
        pSimpleRawDumpCmdQueThread->requestExit();
    }
}

/******************************************************************************
 *
 ******************************************************************************/
void
SimpleDumpBufferSelector::
setDumpConfig(
    MINT32 const          i4OpenId
)
{
    mNeedDump = MTRUE;
    mOpenId = i4OpenId;
    //raw dump
    {
        MY_LOGD("create RawDumpThread instance");

        status_t status = NO_ERROR;

        NSCam::IHalSensorList *pSensorHalList = NULL;
        NSCam::SensorStaticInfo sensorStaticInfo;

        pSensorHalList = MAKE_HalSensorList();
        MUINT32 sensorDev = pSensorHalList->querySensorDevIdx(mOpenId);
        pSensorHalList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);

        MUINT32 rawSensorBit = 0;
        switch (sensorStaticInfo.rawSensorBit)
        {
            case RAW_SENSOR_8BIT:
                rawSensorBit = 8;
                break;
            case RAW_SENSOR_10BIT:
                rawSensorBit = 10;
                break;
            case RAW_SENSOR_12BIT:
                rawSensorBit = 12;
                break;
            case RAW_SENSOR_14BIT:
                rawSensorBit = 14;
                break;
        }
        //
        mpSimpleRawDumpCmdQueThread = ISimpleRawDumpCmdQueThread::createInstance(sensorStaticInfo.sensorFormatOrder, rawSensorBit);
        if  ( mpSimpleRawDumpCmdQueThread  == 0 || OK != (status = mpSimpleRawDumpCmdQueThread->run("SimpleRawDumpCmdQueThread")) )
        {
            MY_LOGE(
                "Fail to run mpSimpleRawDumpCmdQueThread  - mpSimpleRawDumpCmdQueThread .get(%p), status[%s(%d)]",
                mpSimpleRawDumpCmdQueThread.get(), ::strerror(-status), -status
            );
            mu4RawDumpCnt = 0;
            // goto lbExit;
        }
    }
    //
    mu4MaxRawDumpCnt = property_get_int32("rawDump.selector.maxCnt", MAX_DUMPCNT);
    MY_LOGD("mu4MaxRawDumpCnt:%d", mu4MaxRawDumpCnt);
    //eng mode video raw dump
}

void
SimpleDumpBufferSelector::
sendDataToDumpThread(
    MINT32                  rRequestNo,
    Vector<ISelector::MetaItemSet>     rvResult,
    Vector<ISelector::BufferItemSet>   rvBuffers
)
{
    MY_LOGD("sendDataToDumpThread rNO (%d) ", rRequestNo);
    //enable RawDump
    {
        MINT32 nowStartId = property_get_int32("rawDump.selector.start", 0);

        if ( nowStartId != mLastStartId )
        {
            MY_LOGD("RawDump start flag changed from %d to %d ", mLastStartId, nowStartId);
            mEnableRawDump = true;
            mu4RawDumpCnt = 0;
        }

        mLastStartId = nowStartId;
    }

    bool nonStop = true;
    if (mpSimpleRawDumpCmdQueThread != 0 && (mu4RawDumpCnt < mu4MaxRawDumpCnt) && mEnableRawDump == true )
    {
        MY_LOGD("u4RawDumpCnt, Max = (%d, %d)", mu4RawDumpCnt, mu4MaxRawDumpCnt); // debug
        //
        //dump debug info
        MINT32 magicNum = 0;
        MINT64 timestamp = 0;
        for(int i=0 ; i < (int)rvResult.size() ; i++)
        {
            //
            if(rvResult[i].id == NSCam::eSTREAMID_META_APP_DYNAMIC_P1 ||
               rvResult[i].id == NSCam::eSTREAMID_META_APP_DYNAMIC_P2 )
            {
                IMetadata::IEntry entry = rvResult[i].meta.entryFor(MTK_SENSOR_TIMESTAMP);
                if( !entry.isEmpty() ) {
                    timestamp = entry.itemAt(0, Type2Type<MINT64>());
                }
            }
            //
            if(rvResult[i].id == NSCam::eSTREAMID_META_HAL_DYNAMIC_P1 ||
               rvResult[i].id == NSCam::eSTREAMID_META_HAL_DYNAMIC_P2 )
            {
                IMetadata::IEntry entry = rvResult[i].meta.entryFor(MTK_P1NODE_PROCESSOR_MAGICNUM);
                if( !entry.isEmpty() ) {
                    magicNum = entry.itemAt(0, Type2Type<MINT32>());
                }
            }
            //
            if(rvResult[i].id == NSCam::eSTREAMID_META_HAL_DYNAMIC_P2)
            {
                IMetadata exifMeta;
                tryGetMetadata<IMetadata>(&rvResult[i].meta, MTK_3A_EXIF_METADATA, exifMeta);
                //
                {
                    // write buffer[0-#] into disc
                    String8 s8RawFilePath = String8(RAW_DUMP_FOLDER_PATH);
                    //
                    char mpszSuffix[256] = {0};
                    sprintf(mpszSuffix, "dumpDebugInfo__%04d_magic%04d.bin", rRequestNo, magicNum); /* info from EngShot::onCmd_capture */
                    s8RawFilePath.append(mpszSuffix);
                    //
                    nonStop = mpSimpleRawDumpCmdQueThread->postCommand(exifMeta, s8RawFilePath);
                    if(nonStop==false)
                    {
                        MY_LOGD("Stop");
                        mpSimpleRawDumpCmdQueThread->postCommand(NULL, 0, 0, String8(""));
                        mEnableRawDump=false;
                        mu4RawDumpCnt=0;
                    }
                }
            }
        }
        //
        MY_LOGD("magicNum=%d, timestamp=%" PRId64 ,magicNum,timestamp);
        //
        for(size_t i = 0; i < rvBuffers.size(); i++)
        {
            bool bFind = false;
            if(rvBuffers[i].id == eSTREAMID_IMAGE_PIPE_RAW_OPAQUE)
            {
                sp<IImageBuffer> buf = rvBuffers[i].heap->createImageBuffer();
                buf->setTimestamp(timestamp);
                //
                String8 PortName = String8("imgo");
                //
                nonStop = mpSimpleRawDumpCmdQueThread->postCommand(buf.get(), rRequestNo, magicNum, PortName);
                if(nonStop==false)
                {
                    MY_LOGD("Stop");
                    mpSimpleRawDumpCmdQueThread->postCommand(NULL, 0, 0, String8(""));
                    mEnableRawDump=false;
                    mu4RawDumpCnt=0;
                    break;
                }
                bFind = true;
            }
            else if(rvBuffers[i].id == eSTREAMID_IMAGE_PIPE_RAW_RESIZER)
            {
                sp<IImageBuffer> buf = rvBuffers[i].heap->createImageBuffer();
                buf->setTimestamp(timestamp);
                //
                String8 PortName = String8("rrzo");
                //
                nonStop = mpSimpleRawDumpCmdQueThread->postCommand(buf.get(), rRequestNo, magicNum, PortName);
                if(nonStop==false)
                {
                    MY_LOGD("Stop");
                    mpSimpleRawDumpCmdQueThread->postCommand(NULL, 0, 0, String8(""));
                    mEnableRawDump=false;
                    mu4RawDumpCnt=0;
                    break;
                }
                bFind = true;
            }
            //
            if(bFind)
            {
                mu4RawDumpCnt++;
            }
        }
    }
    else if (mu4RawDumpCnt == mu4MaxRawDumpCnt)
    {
        MY_LOGD("Stop");
        mpSimpleRawDumpCmdQueThread->postCommand(NULL, 0, 0, String8(""));
        mEnableRawDump=false;
        mu4RawDumpCnt=0;
    }
}

/******************************************************************************
 *
 ******************************************************************************/
DumpBufferSelector::
DumpBufferSelector()
{

}

/******************************************************************************
 *
 ******************************************************************************/
DumpBufferSelector::
~DumpBufferSelector()
{

}

/******************************************************************************
 *
 ******************************************************************************/
android::status_t
DumpBufferSelector::
selectResult(
    MINT32                             rRequestNo,
    Vector<MetaItemSet>                rvResult,
    Vector<BufferItemSet>              rvBuffers,
    MBOOL                              errorResult
)
{
    MY_LOGD("selectResult rNo(%d)",rRequestNo);
    //
    if( errorResult )
    {
        MY_LOGW("don't reserved errorResult(1) requestNo(%d)", rRequestNo);
        //
        returnBuffersToPool(rvBuffers);
        return UNKNOWN_ERROR;
    }
    //
    sendDataToDumpThread(rRequestNo, rvResult, rvBuffers);
    //
    returnBuffersToPool(rvBuffers);
    //
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DumpBufferSelector::
returnBuffer(
    BufferItemSet&    rBuffer
)
{
    return returnBufferToPool(rBuffer);
}

/******************************************************************************
 *
 ******************************************************************************/
status_t
DumpBufferSelector::
flush()
{
    FUNC_START;
    Mutex::Autolock _l(mResultSetLock);

    if ( ! mResultSetMap.empty() ) {
        for ( size_t i = 0; i < mResultSetMap.size(); ++i ) {
            returnBuffersToPool(mResultSetMap.editItemAt(i).bufferSet);
        }
    }

    mResultSetMap.clear();
    FUNC_END;
    return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
void
DumpBufferSelector::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;
    flush();
    FUNC_END;
}

