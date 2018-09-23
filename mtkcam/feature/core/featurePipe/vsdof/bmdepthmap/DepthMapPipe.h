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
 * @file DepthMapPipe.h
 * @brief Main class of DepthMapPipe
 */

#ifndef _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_PIPE_H_
#define _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_PIPE_H_

// Standard C header file

// Android system/core header file
#include <utils/Vector.h>
#include <utils/KeyedVector.h>

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>

// Module header file
#include <featurePipe/core/include/CamPipe.h>
#include <flowOption/DepthMapFlowOption.h>
#include <bufferPoolMgr/BaseBufferHandler.h>
#include <bufferPoolMgr/BaseBufferPoolMgr.h>

// Local header file
#include "./nodes/FDNode.h"
#include "./nodes/OCCNode.h"
#include "./nodes/N3DNode.h"
#include "./nodes/DPENode.h"
#include "./nodes/WMFNode.h"
#include "./nodes/P2AFMNode.h"
#include "DepthMapPipeNode.h"


/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using android::sp;
/*******************************************************************************
* Class Definition
********************************************************************************/

/**
 * @class DepthMapPipe
 * @brief DepthMapNode feature pipe
 */
class DepthMapPipe : public CamPipe<DepthMapPipeNode>, public DepthMapPipeNode::Handler_T, public IDepthMapPipe
{

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

public:
    /**
     * @brief DepthMapPipe initialize
     * @param [in] setting DepthMapPipe config setting
     * @param [in] pipeOption DepthMapPipe option
     */
    DepthMapPipe(DepthMapPipeSetting setting, DepthMapPipeOption pipeOption);
    virtual ~DepthMapPipe();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IDepthMapPipe Public Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    MBOOL init();
    MBOOL uninit();
    MVOID flush();
    MBOOL enque(sp<IDepthMapEffectRequest>& request);

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
    /**
     * @brief OnData callback function
     * @param [in] id DepthMapPipe DataID
     * @param [in] data the real data instance of this onData callback
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    virtual MBOOL onData(DataID id, DepthMapRequestPtr &data);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// DepthMapPipe Private Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    /**
     * @brief handle function for ERROR_OCCUR_NOTIFY.
     * @param [in] request DepthMapPipe effect request
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL onErrorNotity(DepthMapRequestPtr &request);
    /**
     * @brief handle function for data complete.
     * @param [in] request DepthMapPipe effect request
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL onDataSuccess(DepthMapRequestPtr &request);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Private Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    // mutex for request map
    android::Mutex mReqMapLock;
    // EffectRequest collections
    KeyedVector<MUINT32, DepthMapRequestPtr> mvRequestPtrMap;
    // feature pipe node map
    DepthMapPipeNodeMap mNodeMap;
    // flow option
    sp<DepthMapFlowOption> mpFlowOption;
    // buffer pool mgr
    sp<BaseBufferPoolMgr> mpBuffPoolMgr;
    // setting/config
    DepthMapPipeSetting mSetting;
    DepthMapPipeOption mPipeOption;
};


}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif // _MTK_CAMERA_FEATURE_PIPE_DEPTH_MAP_PIPE_H_
