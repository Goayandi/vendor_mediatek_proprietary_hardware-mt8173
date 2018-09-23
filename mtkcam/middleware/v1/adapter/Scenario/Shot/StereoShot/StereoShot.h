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

#ifndef _MTK_CAMERA_CAMADAPTER_SCENARIO_SHOT_STEREOSHOT_H_
#define _MTK_CAMERA_CAMADAPTER_SCENARIO_SHOT_STEREOSHOT_H_

#include <mtkcam/def/BuiltinTypes.h>
#include <mtkcam/def/Errors.h>

#include <mtkcam/middleware/v1/IShot.h>
#include <../inc/ImpShot.h>
#include <mtkcam/middleware/v1/LegacyPipeline/processor/ResultProcessor.h>

#include <mtkcam/middleware/v1/camshot/_params.h>
#include <mtkcam/middleware/v1/camshot/BufferCallbackHandler.h>

#include <mtkcam/utils/metadata/IMetadata.h>

#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProvider.h>
// manager
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/ImageStreamManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/NodeConfigDataManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/MetaStreamManager.h>
//
#include <mtkcam/middleware/v1/Scenario/IFlowControl.h>
//
#include <string>
#include <list>
namespace NSCam {
namespace v1 {
    class ISelector;
    class BufferCallbackHandler;
    class StreamBufferProviderFactory;
namespace NSLegacyPipeline {
    class ILegacyPipeline;
    class IConsumerPool;
};
};
};
//
namespace android {
namespace NSShot {

// MET tags
#define TAKE_PICTURE "takePicture"
/******************************************************************************
 *
 ******************************************************************************/
class StereoShot
    : public ImpShot,
      public NSCam::v1::ResultProcessor::IListener,
      public NSCam::v1::IImageCallback
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Instantiation.
    StereoShot() = delete;
    StereoShot(
                char const*const pszShotName,
                uint32_t const u4ShotMode,
                int32_t const i4OpenId,
                sp<NSCam::v1::NSLegacyPipeline::IFlowControl> spIFlowControl
                );
    virtual ~StereoShot();
    void                    onDestroy() override;
    void                    setDstStream(MINT32 reqId, std::list<StreamId_T> dstStreams);
    void                    setDngFlag(MBOOL flag)
                            {
                                mbSupportDng = flag;
                            }
    void                    addAppMetaData(MUINT32 requestNo, IMetadata metadata)
                            {
                                mvAppMetadataSet.add(requestNo, metadata);
                            }
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  ResultProcessor::IListener interface
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    void                    onResultReceived(
                                        MUINT32    const requestNo,
                                        StreamId_T const streamId,
                                        MBOOL      const errorResult,
                                        IMetadata  const result) override;
    void                    onFrameEnd(
                                        MUINT32         const requestNo) override {};
    String8                 getUserName() override;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IImageCallback interface
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    MERROR                  onResultReceived(
                                        MUINT32    const requestNo,
                                        StreamId_T const streamId,
                                        MBOOL      const errorBuffer,
                                        android::sp<IImageBuffer>& pBuffer) override;
private:
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    bool                    sendCommand(
                                        uint32_t const  cmd,
                                        MUINTPTR const  arg1,
                                        uint32_t const  arg2,
                                        uint32_t const  arg3 = 0
                                    ) override;
    bool                    onCmd_reset();
    bool                    onCmd_capture();
    void                    onCmd_cancel();

    bool                    ZsdCapture();
private:
    //
    mutable Mutex                                       mCaptureLock;
    // shot parameters
    NSCamShot::SensorParam                              mSensorParam;

    MBOOL                                               mAppDone = MFALSE;
    MBOOL                                               mHalDone = MFALSE;
    //
    Mutex                                               mMetadataLock;
    //
    //
    MBOOL                                               mbCBShutter = MTRUE;
    //
    IMetadata                                           mSelectorAppMetadata_main1;
    //
    MINT8                                               miImgCount = 0;
    Mutex                                               mImgResultLock;
	//
	MINT32                                              miBokehLevel = 0;
    //
    MBOOL                                               mbEnableDumpCaptureData = MFALSE;
    MBOOL                                               mbEnableFastShot2Shot   = MFALSE;
    MBOOL                                               mbBMDenoiseTuningDump   = MFALSE;

    std::string                                         msFilename = "";
    std::string                                         msCMD = "";
    std::list<MUINT32>                                  mvCBList;
    MBOOL                                               mbSupportDng = MFALSE;
    MBOOL                                               mbSupport3rdParty = MFALSE;
    DefaultKeyedVector<MUINT32, IMetadata>              mvAppMetadataSet;
    DefaultKeyedVector<MUINT32, std::list<StreamId_T>>  mvReqCBList;
    sp<NSCam::v1::NSLegacyPipeline::IFlowControl>       mpIFlowControl = nullptr;
};
}; // namespace NSShot
}; // namespace android
#endif  //  _MTK_CAMERA_CAMADAPTER_SCENARIO_SHOT_STEREOSHOT_H_