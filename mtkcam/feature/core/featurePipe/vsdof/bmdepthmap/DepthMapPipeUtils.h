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
/**
 * @file DepthMapPipeUtils.h
 * @brief Utility functions and classes inside the DepthMapPipe
*/

#ifndef _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_UTILS_H_
#define _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_UTILS_H_


// Standard C header file

// Android system/core header file
#include <utils/Vector.h>
#include <utils/KeyedVector.h>
// mtkcam custom header file

// mtkcam global header file
#include <drv/isp_reg.h>
#include <mtkcam/drv/iopipe/Port.h>
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/def/common.h>
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>
// Module header file

// Local header file
#include "DepthMapEffectRequest.h"
// Log header file
#define PIPE_MODULE_TAG "DepthMapPipe"
#define PIPE_CLASS_TAG "Utils"
#include <featurePipe/core/include/PipeLog.h>

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using namespace NSCam::NSIoPipe::NSPostProc;
using namespace android;
using NSCam::NSIoPipe::PortID;

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


/*******************************************************************************
* Structure Definition
********************************************************************************/

/**
 * @struct eis_region
 * @brief Define the EIS output crop region
 **/
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
 * @struct SimpleOptionMask
 * @brief provide the simple mask enable/disable check
 *        requirement: Template type need the ability to cast to int
 *

struct SimpleOptionMask
{
public:
    ScenarioMask(): miMask(0) {}
    ScenarioMask(T sce) {miMask = 1<<(int)sce;}
    ScenarioMask& enable(T scen);
    ScenarioMask& disable(T scen);
    MBOOL check(T sce);
private:
    MUINT32 miMask;
};
*/


/*******************************************************************************
* Function Definition
********************************************************************************/

/**
 * @brief Try to get metadata value
 * @param [in] pMetadata IMetadata instance
 * @param [in] tag the metadata tag to retrieve
 * @param [out] rVal the metadata value to be stored.
 * @return
 * - MTRUE indicates success.
 * - MFALSE indicates failure.
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
 * @brief Try to set metadata value
 * @param [in] pMetadata IMetadata instance
 * @param [in] tag the metadata tag to configure
 * @param [in] rVal the metadata value to set
 * @return
 * - MTRUE indicates success.
 * - MFALSE indicates failure.
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

/**
 * @brief update the metadata entry
 * @param [in] pMetadata IMetadata instance
 * @param [in] tag the metadata tag to update
 * @param [in] rVal the metadata entry value
 * @return
 * - MTRUE indicates success.
 * - MFALSE indicates failure.
 */
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

/**
 * @brief Query the eis crop refion from the input HAL metadata
 */
MBOOL queryEisRegion(IMetadata* const inHal, eis_region& region);
/**
 * @brief Check EIS is on or not.
 */
MBOOL isEISOn(IMetadata* const inApp);


/*******************************************************************************
* Struct Definition
********************************************************************************/
/**
 * @struct TemplateLocStat
 * @brief Store the location information during the template generator
 **/
struct TemplateLocStat
{
    // key = portID or groupID, value= position index
    KeyedVector<MUINT32, MUINT32> mKeyToLocMap;
};


/**
 * @class TemplateStatistics
 * @brief Store the location information of mvIn, mvOut, mvCropRsInfo for each frame
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
    MVOID debug();


public:
    MUINT32 mvInTotalSize;
    MUINT32 mvOutTotalSize;
    MUINT32 mvCropTotalSize;

    MINT32 miActiveFrameID;
};

/**
 * @struct QParamTemplate
 * @brief QParams template
 **/

struct QParamTemplate
{
    QParams mTemplate;
    TemplateStatistics mStats;
};

/**
 * @brief Container class for the callback operations
 */
class DepthMapPipeNode;

struct EnqueCookieContainer
{
public:
    EnqueCookieContainer(sp<DepthMapEffectRequest> req, DepthMapPipeNode* pNode)
    : mRequest(req), mpNode(pNode) {}
public:
    sp<DepthMapEffectRequest> mRequest;
    DepthMapPipeNode* mpNode;
};
/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * @class QParamTemplateGenerator
 * @brief Store the location information of mvIn, mvOut, mvCropRsInfo for each frame
 **/
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
    MBOOL generate(QParamTemplate& rQParamTmpl);
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

/**
 * @class QParamTemplateFiller
 * @brief Fill the corresponding input/output/tuning buffer and configure crop information
 **/
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

enum tuning_tag
{
    tuning_tag_G2G = 0,
    tuning_tag_G2C,
    tuning_tag_GGM,
    tuning_tag_UDM,
};
void SetDefaultTuning(dip_x_reg_t* pIspReg, MUINT32* tuningBuf, tuning_tag tag, int enFgMode);

/**
 * @brief QParams debug function
 */
MVOID debugQParams(const QParams& rInputQParam);

}
}
}
#endif
