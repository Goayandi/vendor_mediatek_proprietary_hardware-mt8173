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

#ifndef _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_UTILS_H_
#define _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_UTILS_H_

#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Trace.h>
#include <mtkcam/drv/iopipe/Port.h>
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/feature/stereo/pipe/IDepthMapPipe.h>
#include <featurePipe/core/include/ImageBufferPool.h>
#include <drv/isp_reg.h>
#include "DataStorage.h"

#define PIPE_MODULE_TAG "DepthMapPipe"
#define PIPE_CLASS_TAG "Utils"
#include <featurePipe/core/include/PipeLog.h>

using namespace NSCam::NSIoPipe::NSPostProc;
using namespace android;
using NSCam::NSIoPipe::PortID;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {


/*******************************************************************************
* Enum Definition
********************************************************************************/

enum eCropGroup
{
    eCROP_CRZ=1,
    eCROP_WDMA=2,
    eCROP_WROT=3
};

enum eFrameIOType
{
    FRAME_INPUT,
    FRAME_OUTPUT
};

enum tuning_tag
{
    tuning_tag_G2G = 0,
    tuning_tag_G2C,
    tuning_tag_GGM,
    tuning_tag_UDM,
};

/*******************************************************************************
* Struct definition
********************************************************************************/

struct eis_region
{
    MUINT32 x_int;
    MUINT32 x_float;
    MUINT32 y_int;
    MUINT32 y_float;
    MSize   s;
#if SUPPORT_EIS_MV
    MUINT32 x_mv_int;
    MUINT32 x_mv_float;
    MUINT32 y_mv_int;
    MUINT32 y_mv_float;
    MUINT32 is_from_zzr;
#endif
};

/**
 * @struct DepthBufferInfo
 * @brief data to be queued during flow type:eDEPTH_FLOW_TYPE_QUEUED_DEPTH
 */

struct DepthBufferInfo
{
    DepthBufferInfo(): mpDepthBuffer(nullptr), mfConvOffset(0.0) {}

    SmartImageBuffer mpDepthBuffer;
    MFLOAT mfConvOffset;
};

typedef DataStorage<DepthBufferInfo> DepthInfoStorage;
/*******************************************************************************
* Function definition
********************************************************************************/

/**
 * Try to get metadata value
 *
 * @param[in]  pMetadata: IMetadata instance
 * @param[in]  tag: the metadata tag to retrieve
 * @param[out]  rVal: the metadata value
 *
 *
 * @return
 *  -  true if successful; otherwise false.
 */
template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if( pMetadata == NULL ) {
        MY_LOGW("pMetadata == NULL");
        return MFALSE;
    }
    //
    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if( !entry.isEmpty() ) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    return MFALSE;
}

/**
 * Try to set metadata value
 *
 * @param[in]  pMetadata: IMetadata instance
 * @param[in]  tag: the metadata tag to set
 * @param[in]  val: the metadata value to be configured
 *
 *
 * @return
 *  -  true if successful; otherwise false.
 */
template <typename T>
inline MVOID
trySetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T const& val
)
{
    if( pMetadata == NULL ) {
        MY_LOGW("pMetadata == NULL");
        return;
    }
    //
    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
}

template <typename T>
inline MVOID
updateEntry(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T const& val
)
{
    if( pMetadata == NULL ) {
        MY_LOGW("pMetadata == NULL");
        return;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
}

template <typename T>
MBOOL
tryGetMetadataInFrame(
    sp<EffectRequest> pEffectReq,
    eFrameIOType type,
    DepthMapBufferID bid,
    MUINT32 const tag,
    T & rVal)
{
    sp<EffectFrameInfo> pFrameInfo;
    if(type == FRAME_INPUT)
        pFrameInfo = pEffectReq->vInputFrameInfo.valueFor(bid);
    else
        pFrameInfo = pEffectReq->vOutputFrameInfo.valueFor(bid);
    //get metadata
    IMetadata* pMeta;
    sp<EffectParameter> effParam = pFrameInfo->getFrameParameter();
    pMeta = reinterpret_cast<IMetadata*>(effParam->getPtr(VSDOF_PARAMS_METADATA_KEY));

    if( !tryGetMetadata<T>(pMeta, tag, rVal) ) {
        return MFALSE;
    }

    return MTRUE;
}

/**
 * Get the metadata inside the FrameInfo
 *
 */
IMetadata* getMetadataFromFrameInfoPtr(sp<EffectFrameInfo> pFrameInfo);

/**
 * @brief EIS regions
 *
 */
MBOOL queryEisRegion(
        IMetadata* const inHal,
        eis_region& region,
        android::sp<EffectRequest> request);

MBOOL isEISOn(IMetadata* const inApp);

void SetDefaultTuning(dip_x_reg_t* pIspReg, MUINT32* tuningBuf, tuning_tag tag, int enFgMode);

/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * Store the location information during the template generator
 *
 **/
 struct TemplateLocStat
{
    // key = portID or groupID, value= position index
    KeyedVector<MUINT32, MUINT32> mKeyToLocMap;
};

/**
 * Store the location information of mvIn, mvOut, mvCropRsInfo for each frame
 *
 **/
struct TemplateStatistics
{
public:
    // key = frame ID, value= position index
    KeyedVector<MUINT32, TemplateLocStat> mFrameIDToLocMap_mvIn;
    KeyedVector<MUINT32, TemplateLocStat> mFrameIDToLocMap_mvOut;
    KeyedVector<MUINT32, TemplateLocStat> mFrameIDToLocMap_mvCrop;

public:
    TemplateStatistics(): miActiveFrameID(-1), mvInTotalSize(0), mvOutTotalSize(0),
                          mvCropTotalSize(0){}

    MVOID inherit(TemplateStatistics& stat);
    MVOID setActiveFrame(MUINT32 frameID);
    MVOID addCrop(eCropGroup groupID);
    MVOID addMvIn(MUINT32 portIdx);
    MVOID addMvOut(MUINT32 portIdx);

    MUINT32 mvInTotalSize;
    MUINT32 mvOutTotalSize;
    MUINT32 mvCropTotalSize;

    MINT32 miActiveFrameID;
};

class QParamTemplateGenerator;

class QParamTemplateGenerator
{
private:
    QParamTemplateGenerator();
    MBOOL checkValid();
public:
    QParamTemplateGenerator(MUINT32 frameID, MUINT32 iSensorIdx, ENormalStreamTag streamTag, TemplateStatistics* pLearningStat=NULL);
    QParamTemplateGenerator& addCrop(eCropGroup groupID, MPoint startLoc, MSize cropSize, MSize resizeDst);
    QParamTemplateGenerator& addInput(NSCam::NSIoPipe::PortID portID);
    QParamTemplateGenerator& addOutput(NSCam::NSIoPipe::PortID portID, MINT32 transform=0);
    QParamTemplateGenerator& addModuleInfo(MUINT32 moduleTag, MVOID* moduleStruct);
    MBOOL generate(QParams& rOutputParams, TemplateStatistics& rStats);
public:
    MUINT32 miFrameID;
    MINT32 mStreamTag;
    MINT32 miSensorIdx;
    android::Vector<Input>      mvIn;
    android::Vector<Output>     mvOut;
    android::Vector<MCrpRsInfo> mvCropRsInfo;
    android::Vector<ModuleInfo> mvModuleData;
    TemplateStatistics mStats;
};

class QParamTemplateFiller
{

public:
    QParamTemplateFiller(QParams& target, const TemplateStatistics& tempStats);
    QParamTemplateFiller& insertTuningBuf(MUINT frameID, MVOID* pTuningBuf);
    QParamTemplateFiller& insertInputBuf(MUINT frameID, NSCam::NSIoPipe::PortID portID, IImageBuffer* pImggBuf);
    QParamTemplateFiller& insertInputBuf(MUINT frameID, NSCam::NSIoPipe::PortID portID, sp<EffectFrameInfo> pFrameInfo);
    QParamTemplateFiller& insertOutputBuf(MUINT frameID, NSCam::NSIoPipe::PortID portID, IImageBuffer* pImggBuf);
    QParamTemplateFiller& insertOutputBuf(MUINT frameID, NSCam::NSIoPipe::PortID portID, sp<EffectFrameInfo> pFrameInfo);
    QParamTemplateFiller& setCrop(MUINT frameID, eCropGroup groupID, MPoint startLoc, MSize cropSize, MSize resizeDst);
    QParamTemplateFiller& delOutputPort(MUINT frameID, NSCam::NSIoPipe::PortID portID, eCropGroup cropGID);
    /**
     * Validate the template filler status
     *
     * @return
     *  -  true if successful; otherwise false.
     **/
    MBOOL validate();
private:

    // the location to be deleted vector
    Vector<MUINT> mDelOutPortLocVec;
    Vector<MUINT>  mDelCropLocVec;
public:
    QParams& mTargetQParam;
    const TemplateStatistics& mTemplStats;
    MBOOL mbSuccess;

};

/**
 * @brief Map buffer id to its queued version used in QUEUED flow type
 * @param [in] bid buffer id to be retrieved its queued one
 * @return
 * - queued buffer id of input one
 */
DepthMapBufferID mapQueuedBID(DepthMapBufferID bid);
 /**
 * @brief check the status is required for queued depth flow
 * @param [in] pRequest Effect Request
 * @return
 * - MTRUE indicated yes
 * - MFALSE indicated no
 */
MBOOL isQueueDepthFlow(DepthNodeOpState eState, DepthFlowType eFlowType);
}
}
}
#endif
