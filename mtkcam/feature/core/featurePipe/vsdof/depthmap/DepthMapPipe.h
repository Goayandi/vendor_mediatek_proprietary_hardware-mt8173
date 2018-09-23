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

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_PIPE_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_PIPE_H_

#include "FDNode.h"
#include "OCCNode.h"
#include "N3DNode.h"
#include "DPENode.h"
#include "WMFNode.h"
#include "P2AFMNode.h"
#include "GFNode.h"
#include "DepthMapPipeNode.h"
#include "DataStorage.h"

#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <featurePipe/core/include/CamPipe.h>
#include <mtkcam/feature/stereo/pipe/IDepthMapPipe.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class DepthMapPipe : public CamPipe<DepthMapPipeNode>, public DepthMapPipeNode::Handler_T, public IDepthMapPipe
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    DepthMapPipe(
            DepthFlowType flowType,
            MINT32 iSensorIdx_Main1,
            MINT32 iSensorIdx_Main2);
    virtual ~DepthMapPipe();
    // enque function
    MBOOL enque(EffectRequestPtr& request);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IDepthMapPipe Public Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    MBOOL init();
    MBOOL uninit();
    MVOID flush();
    MVOID setFlushOnStop(MBOOL flushOnStop) { CamPipe<DepthMapPipeNode>::setFlushOnStop(flushOnStop); }
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CamPipe Protected Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    virtual MBOOL onInit();
    virtual MVOID onUninit();
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipe Protected Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    virtual MBOOL onData(DataID id, EffectRequestPtr &data);
    virtual MBOOL onData(DataID id, FrameInfoPtr &data);
    virtual MBOOL onData(DataID id, SmartImageBuffer &data);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipe Private Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    /**
     * @brief callback when eDEPTH_FLOW_TYPE_QUEUED_DEPTH finished
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL onQueuedDepthFlowDone(EffectRequestPtr pRequest);
    /**
     * @brief callback when error notify
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL onErrorNotify(EffectRequestPtr pRequest);
    /**
     * @brief flow type handle function before enque
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL handleFlowTypeOnEnque(EffectRequestPtr request);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipe Private Member
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    // mutex for request map
    android::Mutex mReqMapLock;
    // EffectRequest collections
    KeyedVector<MUINT32, EffectRequestPtr> mvRequestPtrMap;
    // flow type
    DepthFlowType mFlowType;
    // feature pipe nodes
    P2AFMNode mP2AFMNode;
    N3DNode mN3DNode;
    DPENode mDPENode;
    OCCNode mOCCNode;
    WMFNode mWMFNode;
    StereoFDNode mFDNode;
    GFNode mGFNode;
    Vector<DepthMapPipeNode*> mNodes;
    // buffer storage
    sp<DepthInfoStorage> mpDepthStorage;
    // Logging flag
    MBOOL mbProfileLog;
    MBOOL mbDebugLog;
};


}; // NSFeaturePipe
}; // NSCamFeature
}; // NSCam

#endif // _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_PIPE_H_
