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

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_NODE_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_NODE_H_

#include "DepthMapPipe_Common.h"

#include <utils/Vector.h>
#include <utils/String8.h>
#include <utils/Condition.h>
#include <featurePipe/core/include/CamThreadNode.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum DepthMapDataID {
    ID_INVALID,
    ROOT_ENQUE,

    P2AFM_TO_N3D_FEFM_CCin,
    P2AFM_TO_GF_MY_SL,
    P2AFM_TO_FD_IMG,
    N3D_TO_DPE_MVSV_MASK,
    N3D_TO_OCC_LDC,
    N3D_TO_FD_EXTDATA_MASK,
    DPE_TO_OCC_MVSV_DMP_CFM,
    OCC_TO_WMF_DMH_MY_S,
    WMF_TO_GF_DMW_MY_S, //10
    P2AFM_TO_GF_DMW_MYS,

    // DepthMap output
    P2AFM_OUT_MV_F,
    P2AFM_OUT_FD,
    P2AFM_OUT_MV_F_CAP,
    GF_OUT_DMBG,
    GF_OUT_DEPTHMAP,
    FD_OUT_EXTRADATA,
    N3D_OUT_JPS,
    DEPTHMAP_META_OUT,

    // notify the QUEUED flow type finish
    QUEUED_FLOW_DONE,
    // Notify error occur underlying the nodes
    ERROR_OCCUR_NOTIFY,
    // use to dump p1 raw buffers
    TO_DUMP_RAWS,
    // use to dump buffers
    TO_DUMP_BUFFERS,
    #ifdef GTEST
    // For UT
    UT_OUT_FE
    #endif
};



class DepthMapDataHandler
{
public:
    typedef DepthMapDataID DataID;
public:
    virtual ~DepthMapDataHandler();
    virtual MBOOL onData(DataID id, EffectRequestPtr &data) { return MFALSE; }
    virtual MBOOL onData(DataID id, FrameInfoPtr &data)     { return MFALSE; }
    virtual MBOOL onData(DataID id, SmartImageBuffer &data) { return MFALSE; }
    virtual MBOOL onData(DataID id, ImgInfoMapPtr &data) { return MFALSE; }
    virtual MBOOL onData(DataID id, ExtraDataInfoPtr &data) { return MFALSE; }
    // dump buffer
    virtual MBOOL onDump(
                    DataID id,
                    FrameInfoPtr &data,
                    DepthNodeOpState eState,
                    const char* fileName=NULL,
                    const char* postfix=NULL)  { return MFALSE; }

    virtual MBOOL onDump(
                    DataID id,
                    ImgInfoMapPtr &data,
                    const char* fileName=NULL,
                    const char* postfix=NULL) { return MFALSE; }

    static const char* ID2Name(DataID id);

};

class DepthMapPipeNode : public DepthMapDataHandler, public CamThreadNode<DepthMapDataHandler>
{
public:
    typedef CamGraph<DepthMapPipeNode> Graph_T;
    typedef DepthMapDataHandler Handler_T;

public:
    DepthMapPipeNode(const char *name, DepthFlowType flowType);
    virtual ~DepthMapPipeNode();

public:
    // dump buffer
    virtual MBOOL onDump(
                    DataID id,
                    FrameInfoPtr &data,
                    DepthNodeOpState eState,
                    const char* fileName=NULL,
                    const char* postfix=NULL);

    virtual MBOOL onDump(
                    DataID id,
                    ImgInfoMapPtr &data,
                    const char* fileName=NULL,
                    const char* postfix=NULL);

    // perform dump operation
    MBOOL handleDump(DataID id, ImgInfoMapPtr& data, const char* fileName=NULL, const char* postfix=NULL);
    MBOOL handleDump(DataID id, FrameInfoPtr& data, DepthNodeOpState eState, const char* fileName=NULL, const char* postfix=NULL);
    MBOOL handleDataAndDump(DataID id, ImgInfoMapPtr& data);
    MBOOL handleDataAndDump(DataID id, FrameInfoPtr data, DepthNodeOpState eState);

public:
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
protected:
    virtual MBOOL onInit()           { return MTRUE; }
    virtual MBOOL onUninit()         { return MTRUE; }
    virtual MBOOL onThreadStart()    { return MTRUE; }
    virtual MBOOL onThreadStop()     { return MTRUE; }
    virtual MBOOL onThreadLoop() = 0;
    // check the buffer of DataID will dump or not
    virtual MBOOL checkToDump(DataID id);
    // map the input meta to queued one if needed
    DepthMapBufferID mapQueuedBufferID(
                                EffectRequestPtr pRequest,
                                DepthMapBufferID bid);
    // check the request is queued depth flow type.
    MBOOL isQueuedDepthRequest(EffectRequestPtr pRequest);
protected:
    // flow type
    DepthFlowType mFlowType;
    // Logging flag
    MBOOL mbProfileLog;
    MBOOL mbDebugLog;
    MUINT miDumpBufSize;
    MUINT miDumpStartIdx;
private:
};

}; // namespace NSFeaturePipe
}; // namespace NSCamFeature
}; // namespace NSCam

#endif // _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_NODE_H_
