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

#ifndef _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_P2AFM_NODE_H
#define _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_P2AFM_NODE_H

#include "DepthMapPipeNode.h"
#include "DepthMapPipe_Common.h"
#include "DepthMapPipeUtils.h"
#include "DataStorage.h"


#include <mtkcam/aaa/IHal3A.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

#include <featurePipe/core/include/WaitQueue.h>
#include <featurePipe/core/include/GraphicBufferPool.h>
#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
#include <featurePipe/vsdof/util/TuningBufferPool.h>

#include <drv/isp_reg.h>

using android::KeyedVector;
using android::Vector;
using namespace NSCam::NSIoPipe::NSPostProc;
using namespace NS3Av3;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class P2AFMNode : public DepthMapPipeNode
{
public:
    P2AFMNode(
        const char* name,
        DepthFlowType flowType,
        MUINT32 iSensorIdx_Main1,
        MUINT32 iSensorIdx_Main2);

    virtual ~P2AFMNode();
public:
    MVOID setSensorIdx(MUINT32 main1_idx, MUINT32 main2_idx);
    virtual MBOOL onData(DataID id, EffectRequestPtr &request);
    // storage to record the depthmap buffers, used to query depth buffer
    MBOOL setDepthBufStorage(sp<DepthInfoStorage> pDepthStorage);
protected:
    virtual MBOOL onInit();
    virtual MBOOL onUninit();
    virtual MBOOL onThreadLoop();
    virtual MBOOL onThreadStart();
    virtual MBOOL onThreadStop();
    virtual const char* onDumpBIDToName(DepthMapBufferID BID);
private:
    struct EnquedBufPool: SimpleTimer
    {
    public:
        EffectRequestPtr mRequest;
        P2AFMNode* mpNode;
        Vector<SmartTuningBuffer> mEnqueTuningBuffer;
        KeyedVector<DepthMapBufferID, SmartImageBuffer> mEnquedSmartImgBufMap;
        KeyedVector<DepthMapBufferID, SmartGraphicBuffer> mEnquedSmartGraImgBufMap;

        EnquedBufPool(EffectRequestPtr req, P2AFMNode* pP2AFMNode)
        : mRequest(req), mEnquedSmartImgBufMap(), mpNode(pP2AFMNode)
        {
        }

        MVOID addBuffData(DepthMapBufferID bufID, SmartImageBuffer pSmBuf)
        { mEnquedSmartImgBufMap.add(bufID, pSmBuf); }

        MVOID addBuffData(DepthMapBufferID bufID, SmartGraphicBuffer pSmBuf)
        { mEnquedSmartGraImgBufMap.add(bufID, pSmBuf); }

        MVOID addTuningData(SmartTuningBuffer pSmBuf)
        { mEnqueTuningBuffer.add(pSmBuf); }
    };

    class BufferSizeConfig
    {
    public:
        BufferSizeConfig();
        virtual ~BufferSizeConfig();
        MVOID debug() const;

    public:
        MSize P2AFM_FD_IMG_SIZE;
        MSize P2AFM_FEAO_INPUT_SIZE;
        MSize P2AFM_FE1BO_INPUT_SIZE;
        MSize P2AFM_FE2BO_INPUT_SIZE;
        MSize P2AFM_FE1CO_INPUT_SIZE;
        MSize P2AFM_FE2CO_INPUT_SIZE;
        MSize P2AFM_RECT_IN_VR_SIZE_MAIN1;
        MSize P2AFM_RECT_IN_VR_SIZE_MAIN2;
        MSize P2AFM_RECT_IN_PV_SIZE_MAIN1;
        MSize P2AFM_RECT_IN_PV_SIZE_MAIN2;
        MSize P2AFM_RECT_IN_CAP_SIZE;
        MSize P2AFM_RECT_IN_CAP_IMG_SIZE;
        MSize P2AFM_MAIN_IMAGE_SIZE;
        MSize P2AFM_MAIN_IMAGE_CAP_SIZE;
        MSize P2AFM_CCIN_SIZE;

        StereoArea P2AFM_MAIN2_FEAO_AREA;
        StereoArea P2AFM_MAIN2_FEBO_AREA;
        StereoArea P2AFM_MAIN2_FECO_AREA;
    };

    class BufferPoolSet
    {
    public:
        BufferPoolSet();
        virtual ~BufferPoolSet();
    public:
        MBOOL init(
            const BufferSizeConfig& config,
            DepthFlowType flowType);
        MBOOL uninit();
    public:
        // buffer pools
        android::sp<ImageBufferPool> mpFEOB_BufPool;
        android::sp<ImageBufferPool> mpFEOC_BufPool;
        android::sp<ImageBufferPool> mpFMOB_BufPool;
        android::sp<ImageBufferPool> mpFMOC_BufPool;

        android::sp<ImageBufferPool> mpRectInVRBufPool;
        android::sp<ImageBufferPool> mpRectInVRBufPool_Main2;
        android::sp<ImageBufferPool> mpRectInPVBufPool;
        android::sp<ImageBufferPool> mpRectInPVBufPool_Main2;
        android::sp<GraphicBufferPool> mpRectInCapBufPool_Gra_Main2;
        android::sp<ImageBufferPool> mpCCInBufPool;
        android::sp<ImageBufferPool> mpFDBufPool;

        android::sp<ImageBufferPool> mpFEBOInBufPool_Main1;
        android::sp<ImageBufferPool> mpFEBOInBufPool_Main2;
        android::sp<ImageBufferPool> mpFECOInBufPool_Main1;
        android::sp<ImageBufferPool> mpFECOInBufPool_Main2;

        android::sp<TuningBufferPool> mpTuningBufferPool;

        #ifdef GTEST
        // For UT requirement
        android::sp<ImageBufferPool> mFEInput_Stage2_Main1;
        android::sp<ImageBufferPool> mFEInput_Stage2_Main2;
        android::sp<ImageBufferPool> mFEInput_Stage3_Main1;
        android::sp<ImageBufferPool> mFEInput_Stage3_Main2;
        #endif

    private:
        MVOID queryFEOBufferSize(MSize iBufSize, MUINT iBlockSize, MUINT32 &riFEOWidth, MUINT32 &riFEOHeight);
        MVOID queryFMOBufferSize(MUINT32 iFEOWidth, MUINT32 iFEOHeight, MUINT32 &riFMOWidth, MUINT32 &riFMOHeight);
    };

    struct ISPTuningConfig
    {
        FrameInfoPtr& pInAppMetaFrame;
        FrameInfoPtr& pInHalMetaFrame;
        IHal3A* p3AHAL;
        MBOOL bInputResizeRaw;
    };

    // P2 callbacks
    static MVOID onP2Callback(QParams& rParams);
    MVOID handleP2Done(QParams& rParams, EnquedBufPool* pEnqueData);
    static MVOID onP2FailedCallback(QParams& rParams);

    // utilities
    MVOID cleanUp();

    // Prepare the QParam template of each scenario
    MBOOL prepareBurstQParams();
    MBOOL prepareBurstQParams_NORMAL(MINT32 iModuleTrans);
    MBOOL prepareBurstQParams_CAP(MINT32 iModuleTrans);
    // Build the to-enque QParam in runtime
    MBOOL buildQParams_NORMAL(
                            EffectRequestPtr& rEffReqPtr,
                            QParams& rEnqueParam,
                            EnquedBufPool* pEnquePoolData,
                            const TemplateStatistics& tmplStats
                            );
    MBOOL buildQParams_CAP(
                            EffectRequestPtr& rEffReqPtr,
                            QParams& rEnqueParam,
                            EnquedBufPool* pEnquePoolData,
                            const TemplateStatistics& tmplStats);

    MVOID _buildFEFMFrame(QParamTemplateFiller& rQParamFiller, SmartImageBuffer feoBuf[], EnquedBufPool* pEnquePoolData);

    // mapping between portID and node data type
    NSCam::NSIoPipe::PortID mapToPortID(const MUINT32 nodeDataType);
    MUINT32 mapToNodeDataType(const NSCam::NSIoPipe::PortID portId);

    // isp tuning operations
    TuningParam applyISPTuning(
                            SmartTuningBuffer& targetTuningBuf,
                            const ISPTuningConfig& ispConfig);
    MVOID setupEmptyTuningWithFE(SmartTuningBuffer& targetTuningBuf, MUINT iBlockSize);
    MVOID setupEmptyTuningWithFM(SmartTuningBuffer& targetTuningBuf, MUINT iFrameID);
    /**
     * @brief Calaulate the crop region according to the screen ratio
     * @param [in] pFSRAW_frameInfo : input frameInfo
     * @param [out] rCropStartPt : start point of the crop region
     * @param [out] rCropSize : crop region size
     * @return operation result
     */
    MBOOL calCropForScreen(FrameInfoPtr& pFrameInfo, MPoint &rCropStartPt, MSize& rCropSize );
    // prepare template
    MVOID prepareTemplateParams();
    MVOID debugQParams(const QParams& rInputQParam);
    /**
     * @brief dump buffers after p2 operation done
     */
    MVOID dumpingP2Buffers(EnquedBufPool* pEnqueBufPool);
    /**
     * @brief flow type handling function after P2 done
     */
    MVOID onHandleFlowTypeP2Done(EnquedBufPool* pEnqueBufPool);
    /**
     * @brief query the EIS crop region with remapped dimension
     * @param [in] rEffReqPtr effect request
     * @param [in] pMeta_InHal InHalMeta
     * @param [in] szEISDomain EIS domain
     * @param [in] szEISDomain target domain to be remapped
     * @param [out] rOutRegion target domain to be remapped
     */
    MBOOL queryRemappedEISRegion(
                        EffectRequestPtr& rEffReqPtr,
                        IMetadata* pMeta_InHal,
                        MSize szEISDomain,
                        MSize szRemappedDomain,
                        eis_region& rOutRegion);

public:
    // request queue
    WaitQueue<EffectRequestPtr> mRequestQue;
    // P2 NormalStream
    INormalStream*  mpINormalStream = nullptr;
    // sensor index
    MUINT32 miSensorIdx_Main1;
    MUINT32 miSensorIdx_Main2;
    // 3A hal
    IHal3A* mp3AHal_Main1 = nullptr;
    IHal3A* mp3AHal_Main2 = nullptr;
    // Burst triggering QParams template
    QParams mBurstParamTmplate_Cap;
    TemplateStatistics mTemplStats_Cap;
    QParams mBurstParamTmplate_NORMAL;
    TemplateStatistics mTemplStats_NORMAL;
    // SRZ CROP/RESIZE template, frameID to SRZInfo map
    KeyedVector<MUINT, _SRZ_SIZE_INFO_*> mSrzSizeTemplateMap;
    // FE Tuning Buffer Map, key=stage, value= tuning buffer
    KeyedVector<MUINT, SmartTuningBuffer> mFETuningBufferMap;
    // FM Tuning Buffer Map, key=frame ID, value= tuning buffer
    KeyedVector<MUINT, SmartTuningBuffer> mFMTuningBufferMap;
    // BufferPoolSet
    BufferPoolSet mBufPoolSet;
    // Buffer size settings
    BufferSizeConfig mBufConfig;
private:
    int frameIdx_LSide[2];
    int frameIdx_RSide[2];
    // pointer to DepthMapPipe's buffer storage of depth
    sp<DepthInfoStorage> mpDepthStorage;
    // last depth buffer info
    DepthBufferInfo mLastQueueDepthInfo;
};

}; //NSFeaturePipe
}; //NSCamFeature
}; //NSCam


#endif //_MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_P2AFM_NODE_H
