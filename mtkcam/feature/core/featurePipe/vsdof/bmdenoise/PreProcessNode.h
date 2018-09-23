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
#ifndef _MTK_CAMERA_VSDOF_BMDENOISE_FEATURE_PIPE_PRE_PROCESS_NODE_H_
#define _MTK_CAMERA_VSDOF_BMDENOISE_FEATURE_PIPE_PRE_PROCESS_NODE_H_

#include "BMDeNoisePipe_Common.h"
#include "BMDeNoisePipeNode.h"

class DpBlitStream;
//
using namespace NSCam::NSIoPipe::NSPostProc;
using namespace NS3Av3;
//
namespace NSCam{
namespace NSCamFeature{
namespace NSFeaturePipe{

class PreProcessNode : public BMDeNoisePipeNode
{
    public:
        PreProcessNode() = delete;
        PreProcessNode(const char *name, Graph_T *graph, MINT32 openId);
        virtual ~PreProcessNode();
        /**
         * Receive EffectRequestPtr from previous node.
         * @param id The id of receiverd data.
         * @param request EffectRequestPtr contains image buffer and some information.
         */
        virtual MBOOL onData(DataID id, EffectRequestPtr &request);
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
            PreProcessNode* mpNode;
            Vector<SmartTuningBuffer> mEnqueTuningBuffer;
            KeyedVector<BMDeNoiseBufferID, SmartImageBuffer> mEnquedSmartImgBufMap;

            EnquedBufPool(EffectRequestPtr req, PreProcessNode* pPreProcessNode)
            : mRequest(req), mEnquedSmartImgBufMap(), mpNode(pPreProcessNode)
            {
            }

            MVOID addBuffData(BMDeNoiseBufferID bufID, SmartImageBuffer pSmBuf)
            { mEnquedSmartImgBufMap.add(bufID, pSmBuf); }

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
            MSize BMPREPROCESS_MAIN1_W_SIZE;
            MSize BMPREPROCESS_MAIN2_W_SIZE;
            MSize BMPREPROCESS_MAIN1_MFBO_SIZE;
            MSize BMPREPROCESS_MAIN2_MFBO_SIZE;

            MSize BMPREPROCESS_MAIN1_MFBO_FINAL_SIZE;
            MSize BMPREPROCESS_MAIN2_MFBO_FINAL_SIZE;
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
            android::sp<ImageBufferPool> mpMain1_w_bufPool;
            android::sp<ImageBufferPool> mpMain2_w_bufPool;
            android::sp<ImageBufferPool> mpMain1_mfbo_bufPool;
            android::sp<ImageBufferPool> mpMain2_mfbo_bufPool;

            android::sp<ImageBufferPool> mpMain1_mfbo_final_bufPool;
            android::sp<ImageBufferPool> mpMain2_mfbo_final_bufPool;

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

        // P2 callbacks
        static MVOID onP2SuccessCallback(QParams& rParams);
        MVOID handleP2Done(QParams& rParams, EnquedBufPool* pEnqueData);
        static MVOID onP2FailedCallback(QParams& rParams);

        MBOOL prepareModuleSettings();
        MVOID cleanUp();
        MBOOL doPreProcessRequest(EffectRequestPtr request);
        IMetadata* getMetadataFromFrameInfoPtr(sp<EffectFrameInfo> pFrameInfo);
        TuningParam applyISPTuning(SmartTuningBuffer& targetTuningBuf, const ISPTuningConfig& ispConfig, MBOOL useDefault = MFALSE);
        MINT32 eTransformToDegree(MINT32 eTrans);
    private:
        MINT32                                          miOpenId = -1;

        WaitQueue<EffectRequestPtr>                     mRequests;

        DpBlitStream*                                   mpDpStream = nullptr;
        INormalStream*                                  mpINormalStream = nullptr;
        IHal3A*                                         mp3AHal_Main1 = nullptr;
        IHal3A*                                         mp3AHal_Main2 = nullptr;

        MINT32                                          mModuleTrans = -1;

        BufferPoolSet mBufPoolSet;
        BufferSizeConfig mBufConfig;

        StereoSizeProvider*                             mSizePrvider = nullptr;
};
};
};
};
#endif // _MTK_CAMERA_VSDOF_BMDENOISE_FEATURE_PIPE_PRE_PROCESS_NODE_H_