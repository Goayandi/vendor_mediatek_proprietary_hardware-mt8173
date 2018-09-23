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
 * @file DeptMapPipeNode.h
 * @brief Base class of the DepthMapPipe's feature pipe node
 */
#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_NODE_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_NODE_H_

// Standard C header file

// Android system/core header file
#include <utils/Vector.h>
#include <utils/String8.h>
#include <utils/Condition.h>

// mtkcam custom header file

// mtkcam global header file

// Module header file
#include <featurePipe/core/include/CamThreadNode.h>

// Local header file
#include "DepthMapPipe_Common.h"
#include "DepthMapEffectRequest.h"
#include "./flowOption/DepthMapFlowOption.h"
#include "./bufferConfig/BaseBufferConfig.h"

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {


/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * @class DepthMapDataHandler
 * @brief Data hanlder for depthmap pipe node
 */
class DepthMapDataHandler
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    typedef DepthMapDataID DataID;
    DepthMapDataHandler();
    virtual ~DepthMapDataHandler();
public:
    virtual MBOOL onData(DataID id, DepthMapRequestPtr &data) { return MFALSE; }
    // dump buffer
    virtual MBOOL onDump(
                DataID id,
                DepthMapRequestPtr &request,
                const char* fileName=NULL,
                const char* postfix=NULL) { return MFALSE; }
    // ID to name
    static const char* ID2Name(DataID id);
};

/**
 * @class DepthMapPipeNode
 * @brief nodes inside the depthmap pipe
 */
class DepthMapPipeNode : public DepthMapDataHandler, public CamThreadNode<DepthMapDataHandler>
{
public:
    typedef CamGraph<DepthMapPipeNode> Graph_T;
    typedef DepthMapDataHandler Handler_T;

public:
    DepthMapPipeNode(
                const char *name,
                DepthMapPipeNodeID nodeID,
                sp<DepthMapFlowOption> pFlowOpt,
                const DepthMapPipeSetting& setting);

    virtual ~DepthMapPipeNode();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CamThreadNode Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MBOOL onInit()           { return MTRUE; }
    virtual MBOOL onUninit()         { return MTRUE; }
    virtual MBOOL onThreadStart()    { return MTRUE; }
    virtual MBOOL onThreadStop()     { return MTRUE; }
    virtual MBOOL onThreadLoop() = 0;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapDataHandler Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    struct DumpConfig
    {
        DumpConfig() {}
        DumpConfig(char* fname, char* post, MBOOL stride)
        : fileName(fname), postfix(post), bStridePostfix(stride) {}

        char* fileName=NULL;
        char* postfix=NULL;
        MBOOL bStridePostfix = MFALSE;
    };
    // dump buffer
    virtual MBOOL onDump(
                DataID id,
                DepthMapRequestPtr &request,
                DumpConfig* config=NULL);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipeNode Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief handle data for DepthMapNode
     * @param [in] id Data id
     * @param [in] pReq request to pass
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL handleData(DataID id, DepthMapRequestPtr pReq);

    /**
     * @brief perform dump operation
     * @param [in] id Data id
     * @param [in] request request to dump
     * @param [in] config dump config
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL handleDump(
                DataID id,
                DepthMapRequestPtr &request,
                DumpConfig* config=NULL);

    /**
     * @brief perform handleData and dumpping buffer
     */
    MBOOL handleDataAndDump(DataID id, DepthMapRequestPtr &request);

    /**
     * @brief get node id of this node
     */
    DepthMapPipeNodeID getNodeId() {return mNodeId;}
    // used for UT
    template <typename Type3>
    MVOID handleUTData(DataID id, Type3& buffer)
    {
      MY_LOGD("handleUTData aa");
      this->onData(id, buffer);
    }
protected:
    // By node define the
    virtual const char* onDumpBIDToName(DepthMapBufferID BID) {return "non-defined.blob";}
    // check the buffer of DataID will dump or not
    virtual MBOOL checkToDump(DataID id);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipeNode Public Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    // time stamp
    static MUINT32 miTimestamp;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipeNode Protected Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    static DataIDToBIDMap mDataIDToBIDMap;
    // Dump buffer index
    MUINT miDumpBufSize;
    MUINT miDumpStartIdx;
    // flow option instance
    sp<DepthMapFlowOption> mpFlowOption;
    // node id
    DepthMapPipeNodeID mNodeId;
private:
};

}; // namespace NSFeaturePipe_DepthMap_BM
}; // namespace NSCamFeature
}; // namespace NSCam

#endif // _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_NODE_H_
