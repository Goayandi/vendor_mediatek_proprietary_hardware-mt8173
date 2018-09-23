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

 // Standard C header file
#include <time.h>
#include <string.h>
// Android system/core header file

// mtkcam custom header file

// mtkcam global header file

// Module header file
#include <featurePipe/core/include/DebugUtil.h>
// Local header file
#include "DepthMapPipeNode.h"
#include "./bufferPoolMgr/BaseBufferHandler.h"
#include "./bufferConfig/BaseBufferConfig.h"
/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

/*******************************************************************************
* DepthMapDataHandler Definition
********************************************************************************/
DataIDToBIDMap DepthMapPipeNode::mDataIDToBIDMap = getDataIDToBIDMap();
MUINT32 DepthMapPipeNode::miTimestamp = 0;

const char* DepthMapDataHandler::ID2Name(DataID id)
{
#define MAKE_NAME_CASE(name) \
  case name: return #name;

  switch(id)
  {
    MAKE_NAME_CASE(ID_INVALID);
    MAKE_NAME_CASE(ROOT_ENQUE);

    MAKE_NAME_CASE(P2AFM_TO_N3D_FEFM_CCin);
    MAKE_NAME_CASE(P2AFM_TO_WMF_MY_SL);
    MAKE_NAME_CASE(P2AFM_TO_FD_IMG);

    MAKE_NAME_CASE(N3D_TO_DPE_MVSV_MASK);
    MAKE_NAME_CASE(N3D_TO_OCC_LDC);
    MAKE_NAME_CASE(N3D_TO_FD_EXTDATA_MASK);
    MAKE_NAME_CASE(DPE_TO_OCC_MVSV_DMP_CFM);
    MAKE_NAME_CASE(DPE_OUT_DISPARITY);
    MAKE_NAME_CASE(OCC_TO_WMF_DMH_MY_S);

    // DepthMap output
    MAKE_NAME_CASE(P2AFM_OUT_MV_F);
    MAKE_NAME_CASE(P2AFM_OUT_FD);
    MAKE_NAME_CASE(P2AFM_OUT_MV_F_CAP);
    MAKE_NAME_CASE(WMF_OUT_DMW_MY_S);
    MAKE_NAME_CASE(WMF_OUT_DEPTHMAP);
    MAKE_NAME_CASE(FD_OUT_EXTRADATA);

    MAKE_NAME_CASE(DEPTHMAP_META_OUT);
    MAKE_NAME_CASE(N3D_OUT_JPS_WARPING_MATRIX);
    #ifdef GTEST
    MAKE_NAME_CASE(UT_OUT_FE);
    #endif
    MAKE_NAME_CASE(TO_DUMP_BUFFERS);
    MAKE_NAME_CASE(TO_DUMP_RAWS);
    MAKE_NAME_CASE(ERROR_OCCUR_NOTIFY);

  };
  MY_LOGW(" unknown id:%d", id);
  return "UNKNOWN";
#undef MAKE_NAME_CASE
}

DepthMapDataHandler::
DepthMapDataHandler()
{}

DepthMapDataHandler::
~DepthMapDataHandler()
{
}

/*******************************************************************************
* DepthMapPipeNode Definition
********************************************************************************/
DepthMapPipeNode::
DepthMapPipeNode(
    const char *name,
    DepthMapPipeNodeID nodeID,
    sp<DepthMapFlowOption> pFlowOpt,
    const DepthMapPipeSetting& setting
)
: CamThreadNode(name)
, mpFlowOption(pFlowOpt)
, mNodeId(nodeID)
{
    // dump buffer size : default size 5
    char cLogLevel[PROPERTY_VALUE_MAX];
    ::property_get("depthmap.pipe.dump.size", cLogLevel, "0");
    miDumpBufSize = atoi(cLogLevel);
    // start frame index of dump buffer  : default 0
    ::property_get("depthmap.pipe.dump.start", cLogLevel, "0");
    miDumpStartIdx = atoi(cLogLevel);

    MY_LOGD("mbDebugLog=%d mbProfileLog=%d",
            DepthPipeLoggingSetup::mbDebugLog, DepthPipeLoggingSetup::mbProfileLog);
}

DepthMapPipeNode::~DepthMapPipeNode()
{
}

MBOOL
DepthMapPipeNode::
onDump(
    DataID id,
    DepthMapRequestPtr &pRequest,
    DumpConfig* config
)
{
    MY_LOGD("onDump reqID=%d dataid=%d", pRequest->getRequestNo(), id);
    if(!checkToDump(id))
    {
        MY_LOGD("onDump reqID=%d dataid=%d, checkDump failed!", pRequest->getRequestNo(), id);
        return MFALSE;
    }

    char* fileName = (config != NULL) ? config->fileName : NULL;
    char* postfix = (config != NULL) ? config->postfix : NULL;
    MBOOL bStridePostfix = (config != NULL) ? config->bStridePostfix : MFALSE;

    MUINT iReqIdx = pRequest->getRequestNo();
    // check dump index
    if(iReqIdx < miDumpStartIdx || iReqIdx >= miDumpStartIdx + miDumpBufSize)
        return MTRUE;

    if(mDataIDToBIDMap.indexOfKey(id)<0)
    {
        MY_LOGD("onDump reqID=%d dataid=%d, DataID non-exist!", pRequest->getRequestNo(), id);
        return MFALSE;
    }

    const char *dir = (StereoSettingProvider::stereoProfile() == STEREO_SENSOR_PROFILE_FRONT_FRONT) ?
                        "Front" : "Rear";

    // generate file path
    char filepath[1024];
    if(pRequest->getRequestAttr().opState == eSTATE_NORMAL)
        snprintf(filepath, 1024, "/sdcard/vsdof/pv_vr/%s/%d/%s", dir, iReqIdx, getName());
    else
        snprintf(filepath, 1024, "/sdcard/vsdof/capture/%s/%d/%s", dir, iReqIdx, getName());

    // make path
    VSDOF_LOGD("makePath: %s", filepath);
    makePath(filepath, 0660);

    // get the buffer id array for dumping
    const Vector<DepthMapBufferID>& vDumpBufferID = mDataIDToBIDMap.valueFor(id);
     sp<BaseBufferHandler> pBufferHandler = pRequest->getBufferHandler();
    char writepath[1024];
    char strideStr[100];

    VSDOF_LOGD("dataID:%d buffer id size=%d", id, vDumpBufferID.size());
    for(size_t i=0;i<vDumpBufferID.size();++i)
    {
        const DepthMapBufferID& oriBID = vDumpBufferID.itemAt(i);
        DepthMapBufferID BID = mpFlowOption->reMapBufferID(pRequest->getRequestAttr(), oriBID);

        VSDOF_LOGD("Dump -- index%d, buffer id=%d", i, BID);
        IImageBuffer* pImgBuf;
        MBOOL bRet = pBufferHandler->getEnqueBuffer(this->getNodeId(), BID, pImgBuf);
        if(!bRet)
        {
            VSDOF_LOGD("Failed to get enqued buffer, id: %d", BID);
            continue;
        }
        // stride string
        if(bStridePostfix)
            snprintf(strideStr, 100, "_%d", pImgBuf->getBufStridesInBytes(0));
        else
            snprintf(strideStr, 100, "");

        const char* writeFileName = (fileName != NULL) ? fileName : onDumpBIDToName(BID);
        const char* postfixName = (postfix != NULL) ? postfix : "";

        if(strchr(postfixName, '.') != NULL)
            snprintf(writepath, 1024, "%s/%s_%dx%d%s%s", filepath, writeFileName,
                pImgBuf->getImgSize().w, pImgBuf->getImgSize().h, strideStr, postfixName);
        else
        {
            snprintf(writepath, 1024, "%s/%s_%dx%d%s%s.yuv", filepath, writeFileName,
                pImgBuf->getImgSize().w, pImgBuf->getImgSize().h, strideStr, postfixName);
        }


        VSDOF_LOGD("saveToFile: %s", writepath);
        pImgBuf->saveToFile(writepath);
    }

    return MTRUE;
}

MBOOL
DepthMapPipeNode::
handleData(DataID id, DepthMapRequestPtr pReq)
{
    MBOOL bConnect = mpFlowOption->checkConnected(id);
    if(bConnect)
    {
        CamThreadNode<DepthMapDataHandler>::handleData(id, pReq);
        return MTRUE;
    }
    return MFALSE;
}


MBOOL
DepthMapPipeNode::
handleDump(
    DataID id,
    DepthMapRequestPtr &request,
    DumpConfig* config
)
{
    return this->onDump(id, request, config);
}

MBOOL
DepthMapPipeNode::
handleDataAndDump(DataID id, DepthMapRequestPtr &request)
{
    // dump first and then handle data
    MBOOL bRet = this->onDump(id, request);
    bRet &= this->handleData(id, request);
    return bRet;
}

MBOOL DepthMapPipeNode::checkToDump(DataID id)
{
    MINT32 iNodeDump =  getPropValue();

    if(getPropValue() == 0)
    {
        VSDOF_LOGD("node check failed!node: %s dataID: %d", getName(), id);
        return MFALSE;
    }

    if(getPropValue(id) == 0)
    {
        VSDOF_LOGD("dataID check failed!dataID: %d", id);
        return MFALSE;
    }

    return MTRUE;
}

}; // namespace NSFeaturePipe_DepthMap_BM
}; // namespace NSCamFeature
}; // namespace NSCam