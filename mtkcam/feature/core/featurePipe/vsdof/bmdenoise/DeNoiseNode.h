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
#ifndef _MTK_CAMERA_VSDOF_BMDENOISE_FEATURE_PIPE_DENOISE_NODE_H_
#define _MTK_CAMERA_VSDOF_BMDENOISE_FEATURE_PIPE_DENOISE_NODE_H_

#include "BMDeNoisePipe_Common.h"
#include "BMDeNoisePipeNode.h"
#include <mtkcam/feature/stereo/hal/bmdn_hal.h>
#include <mtkcam/aaa/ILscTable.h>

class DpBlitStream;

using namespace NSCam::NSIoPipe::NSPostProc;
using namespace NS3Av3;
using namespace NS_BMDN_HAL;

namespace NSCam{
namespace NSCamFeature{
namespace NSFeaturePipe{

class DeNoiseNode : public BMDeNoisePipeNode
{
    public:
        DeNoiseNode() = delete;
        DeNoiseNode(const char *name, Graph_T *graph, MINT32 openId);
        virtual ~DeNoiseNode();
        /**
         * Receive EffectRequestPtr from previous node.
         * @param id The id of receiverd data.
         * @param request EffectRequestPtr contains image buffer and some information.
         */
        virtual MBOOL onData(DataID id, EffectRequestPtr &request);
        virtual MBOOL onData(DataID id, ImgInfoMapPtr& data);
    protected:
        virtual MBOOL onInit();
        virtual MBOOL onUninit();
        virtual MBOOL onThreadStart();
        virtual MBOOL onThreadStop();
        virtual MBOOL onThreadLoop();
        virtual const char* onDumpBIDToName(BMDeNoiseBufferID BID);
    private:
        struct EnquedBufPool : public Timer
        {
        public:
            EffectRequestPtr mRequest;
            DeNoiseNode* mpNode;
            Vector<SmartTuningBuffer> mEnqueTuningBuffer;
            KeyedVector<BMDeNoiseBufferID, SmartImageBuffer> mEnquedSmartImgBufMap;
            KeyedVector<BMDeNoiseBufferID, sp<IImageBuffer> > mEnquedIImgBufMap;

            EnquedBufPool(EffectRequestPtr req, DeNoiseNode* deNoiseNode)
            : mRequest(req), mEnquedSmartImgBufMap(), mpNode(deNoiseNode)
            {
            }

            MVOID addBuffData(BMDeNoiseBufferID bufID, SmartImageBuffer pSmBuf)
            { mEnquedSmartImgBufMap.add(bufID, pSmBuf); }

            MVOID addTuningData(SmartTuningBuffer pSmBuf)
            { mEnqueTuningBuffer.add(pSmBuf); }

            MVOID addIImageBuffData(BMDeNoiseBufferID bufID, sp<IImageBuffer> pBuf)
            { mEnquedIImgBufMap.add(bufID, pBuf); }
        };
        class BufferSizeConfig
        {
        public:
            BufferSizeConfig();
            virtual ~BufferSizeConfig();
            MVOID debug() const;

        public:
            MSize BMDENOISE_BMDN_RESULT_SIZE;
            MSize BMDENOISE_BMDN_RESULT_ROT_BACK_SIZE;
        };

        class BufferPoolSet
        {
        public:
            BufferPoolSet();
            virtual ~BufferPoolSet();

            MBOOL init(const BufferSizeConfig& config, const MUINT32 tuningsize);
            MBOOL uninit();
        public:
            // buffer pools
            android::sp<ImageBufferPool> mpBMDeNoiseResult_bufPool;
            android::sp<ImageBufferPool> mpBMDeNoiseResultRotBack_bufPool;
            android::sp<TuningBufferPool> mpTuningBuffer_bufPool;
        private:
        };

        struct ISPTuningConfig
        {
            FrameInfoPtr& pInAppMetaFrame;
            FrameInfoPtr& pInHalMetaFrame;
            IHal3A* p3AHAL;
            MBOOL bInputResizeRaw;
        };
    private:
        EnquedBufPool* doBMDeNoise(EffectRequestPtr request_warping);
        MBOOL doPostProcess(EffectRequestPtr request_warping, EnquedBufPool* BMDNResultData);
        MBOOL doMDP(IImageBuffer* src, IImageBuffer* dst);
        MBOOL doNonDeNoiseCapture(EffectRequestPtr request);
        MVOID doDataDump(IImageBuffer* pBuf, BMDeNoiseBufferID BID, MUINT32 iReqIdx);
        MVOID doInputDataDump(EffectRequestPtr request);
        MVOID doOutputDataDump(EffectRequestPtr request, EnquedBufPool* pEnqueBufferPool);

        // P2 callbacks
        static MVOID onP2SuccessCallback(QParams& rParams);
        MVOID handleP2Done(QParams& rParams, EnquedBufPool* pEnqueData);
        static MVOID onP2FailedCallback(QParams& rParams);

        MBOOL prepareModuleSettings();
        MVOID cleanUp();
        IMetadata* getMetadataFromFrameInfoPtr(sp<EffectFrameInfo> pFrameInfo);
        TuningParam applyISPTuning(SmartTuningBuffer& targetTuningBuf, const ISPTuningConfig& ispConfig, MBOOL useDefault = MFALSE);

        MINT32 DegreeToeTransform(MINT32 degree);
        MINT32 eTransformToDegree(MINT32 eTrans);
        MINT32 getJpegRotation(IMetadata* pMeta);

        // get params for BMDN_HAL
        MBOOL getIsRotateAndBayerOrder(BMDN_HAL_IN_DATA &rInData);
        MBOOL getDynamicShadingRA(BMDN_HAL_IN_DATA &rInData);
        MBOOL getPerformanceQualityOption(BMDN_HAL_IN_DATA &rInData);
        MBOOL getSensorGain(IMetadata* pMeta, BMDN_HAL_IN_DATA &rInData, MBOOL isBayer);
        MBOOL getISODependentParams(IMetadata* pMeta, BMDN_HAL_IN_DATA &rInData);
        MBOOL getAffineMatrix(float* warpingMatrix, BMDN_HAL_IN_DATA &rInData);
        MBOOL getShadingGain(TuningParam& tuningParam, ILscTable& lscTbl, MBOOL isBayer);

    private:
        WaitQueue<EffectRequestPtr>                     mRequests;

        DpBlitStream*                                   mpDpStream = nullptr;
        INormalStream*                                  mpINormalStream = nullptr;
        IHal3A*                                         mp3AHal_Main1 = nullptr;
        IHal3A*                                         mp3AHal_Main2 = nullptr;

        MINT32                                          miOpenId = -1;
        MINT32                                          mModuleTrans = -1;
        MINT32                                          mBayerOrder_main1 = -1;

        BufferPoolSet mBufPoolSet;
        BufferSizeConfig mBufConfig;

        StereoSizeProvider*                             mSizePrvider = StereoSizeProvider::getInstance();
        MINT32                                          miDumpShadingGain = 0;

        BMDeNoiseQualityPerformanceParam                mDebugPerformanceQualityOption;
};
};
};
};
#endif // _MTK_CAMERA_VSDOF_BMDENOISE_FEATURE_PIPE_DENOISE_NODE_H_