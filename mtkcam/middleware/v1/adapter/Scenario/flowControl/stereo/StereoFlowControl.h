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

#ifndef _MTK_HARDWARE_MTKCAM_ADAPTER_STEREO_STEREOFLOWCONTROL_H_
#define _MTK_HARDWARE_MTKCAM_ADAPTER_STEREO_STEREOFLOWCONTROL_H_
//
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <chrono>
#include <sys/prctl.h>
//
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBufferProvider.h>
#include <mtkcam/middleware/v1/LegacyPipeline/ILegacyPipeline.h>
#include <mtkcam/middleware/v1/Scenario/IFlowControl.h>
#include <mtkcam/middleware/v1/LegacyPipeline/request/IRequestController.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProvider.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoBufferPool.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoSelector.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/buffer/StereoBufferSynchronizer.h>
#include <mtkcam/middleware/v1/LegacyPipeline/NodeId.h>
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>

using namespace android;
#include <mtkcam/middleware/v1/IParamsManagerV3.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineUtils.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineBuilder.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/StereoLegacyPipelineBuilder.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>

#include <mtkcam/middleware/v1/LegacyPipeline/stereo/BayerAndBayer/vsdof/StereoPipelineData.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/3rdParty/StereoPipelineData.h>
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/BayerAndMono/denoise/StereoPipelineData.h>
#include <mtkcam/feature/stereo/hal/bmdn_hal.h>
#endif
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/ImageStreamManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/MetaStreamManager.h>

#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>

#include <mtkcam/middleware/v1/IShot.h>
#include <Scenario/Shot/StereoShot/StereoShot.h>
#include <mtkcam/middleware/v1/camshot/BufferCallbackHandler.h>

#include <isp_tuning.h>

#include <camera_custom_stereo.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <mtkcam/utils/hw/ITemperatureMonitor.h>

#include "../inc/FlowControlBase.h"
/******************************************************************************
 *
 ******************************************************************************/
//
namespace NSCam {
namespace v1 {
namespace NSLegacyPipeline {


class StereoFlowControl
    : public FlowControlBase
{

public:
    static const int STEREO_FLOW_PREVIEW_REQUSET_NUM_START  = 0;
    static const int STEREO_FLOW_PREVIEW_REQUSET_NUM_END    = 1000;
    static const int STEREO_FLOW_CAPTURE_REQUSET_NUM_START  = 3000;
    static const int STEREO_FLOW_CAPTURE_REQUSET_NUM_END    = 4000;

    enum
    {
        CONTROL_DNG_FLAG,
    };

    enum SeneorModuleType
    {
        UNSUPPORTED_SENSOR_MODULE = 1<<6,
        BAYER_AND_BAYER = 1<<7,
        BAYER_AND_MONO = 1<<8,
    };
    static const int STEREO_BB_PRV_CAP_REC = E_STEREO_FEATURE_CAPTURE | E_STEREO_FEATURE_VSDOF | BAYER_AND_BAYER;
    static const int STEREO_BM_PRV_CAP_REC = E_STEREO_FEATURE_CAPTURE | E_STEREO_FEATURE_VSDOF | BAYER_AND_MONO;
    static const int STEREO_BM_DENOISE = E_STEREO_FEATURE_DENOISE | BAYER_AND_MONO;
    static const int STEREO_3RDPARTY = E_STEREO_FEATURE_THIRD_PARTY;

    enum ScenarioMode
    {
        NONE,
        PREVIEW,
        RECORD,
        CAPTURE,
    };

private:

    struct CommmandType_T{
        enum {
            CONTROL_STOP_P2_PRV,
            CONTROL_START_P2_PRV,
        };
    };

    struct CaptureBufferData
    {
        sp<StereoSelector> mpSelector_full = nullptr;
        sp<StereoSelector> mpSelector_resized = nullptr;
        sp<StereoSelector> mpSelector_full_main2 = nullptr;
        sp<StereoSelector> mpSelector_resized_main2 = nullptr;

        sp<IImageBuffer> mpIMGOBuf_Main1 = nullptr;
        sp<IImageBuffer> mpRRZOBuf_Main1 = nullptr;
        sp<IImageBuffer> mpIMGOBuf_Main2 = nullptr;
        sp<IImageBuffer> mpRRZOBuf_Main2 = nullptr;

        MINT64 timestamp_P1 = 0;
    };

    struct ContextBuilderContent
    {
        const MetaStreamManager::metadata_info_setting* metaTable;
        const MINT32* nodeConfigData;
        ImageStreamManager::image_stream_info_pool_setting* imageTable;
    };

    struct StereoModeContextBuilderContent
    {
        // p1
        ContextBuilderContent mP1Main1Content;
        ContextBuilderContent mP1Main2Content;
        // p2
        ContextBuilderContent mFullContent;
        ContextBuilderContent mPrvContent;
        ContextBuilderContent mCapContent;
        ContextBuilderContent mDngCapContent;
    };
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
                                                StereoFlowControl(
                                                    char const*                 pcszName,
                                                    MINT32 const                i4OpenId,
                                                    sp<IParamsManagerV3>          pParamsManagerV3,
                                                    sp<ImgBufProvidersManager>  pImgBufProvidersManager,
                                                    sp<INotifyCallback>         pCamMsgCbInfo
                                                );

    virtual                                     ~StereoFlowControl() {};

    virtual status_t                            takePicture(StereoShotParam shotParam);

    virtual MBOOL                               setCallbacks(sp<StereoShot> pShot);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IFlowControl Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    virtual char const*                         getName()   const;

    virtual int32_t                             getOpenId() const;

public:  //// Adapter

    virtual status_t                            startPreview();

    virtual status_t                            stopPreview();

    virtual status_t                            resumePreview();

    virtual status_t                            startRecording();

    virtual status_t                            stopRecording();

    virtual status_t                            autoFocus();

    virtual status_t                            cancelAutoFocus();

    virtual status_t                            precapture(int& flashRequired);

    virtual status_t                            takePicture();

    virtual status_t                            setParameters();

    virtual status_t                            sendCommand(
                                                    int32_t cmd,
                                                    int32_t arg1,
                                                    int32_t arg2
                                                );

public:

    virtual status_t                            dump(
                                                    int fd,
                                                    Vector<String8>const& args
                                                );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IRequestUpdater Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual MERROR                              updateRequestSetting(
                                                    IMetadata* appSetting,
                                                    IMetadata* halSetting
                                                );

    virtual MERROR                              updateParameters(
                                                    IMetadata* setting
                                                );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IFeatureFlowControl Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual MERROR                              submitRequest(
                                                    Vector< SettingSet >          vSettings,
                                                    BufferList                    vDstStreams,
                                                    Vector< MINT32 >&             vRequestNo
                                                );

    virtual MERROR                              submitRequest(
                                                    Vector< SettingSet >          vSettings,
                                                    Vector< BufferList >          vDstStreams,
                                                    Vector< MINT32 >&             vRequestNo
                                                );

    virtual MUINT                               getSensorMode(){ return mSensorParam.mode; }

    virtual MERROR                              pausePreview( MBOOL stopPipeline );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  RefBase Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MVOID                               onLastStrongRef( const void* /*id*/);
private:
    sp<ImageStreamInfo>
                                    createImageStreamInfo(
                                        char const*         streamName,
                                        StreamId_T          streamId,
                                        MUINT32             streamType,
                                        size_t              maxBufNum,
                                        size_t              minInitBufNum,
                                        MUINT               usageForAllocator,
                                        MINT                imgFormat,
                                        MSize const&        imgSize,
                                        MUINT32             transform
                                    );
    MINT32                          getSensorModuleType();
    MINT32                          getStereoMode();
    status_t                        buildStereoPipeline(
                                        MINT32 stereoMode,
                                        MINT32 sensorModuleType,
                                        MINT32 pipelineMode);
    status_t                        get_BB_Prv_Cap_Content(
                                        StereoModeContextBuilderContent &content);
    status_t                        get_BB_Rec_Content(
                                        StereoModeContextBuilderContent &content);
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    status_t                        get_BM_Denoise_Content(
                                        StereoModeContextBuilderContent &content);
#endif
    status_t                        get_3rdParty_Content(
                                        StereoModeContextBuilderContent &content);
    status_t                        queryPass1ActiveArrayCrop();
    status_t                        createStereoRequestController();
    status_t                        setAndStartStereoSynchronizer();
    status_t                        startStereoPipeline(
                                        MINT32 request_num_start,
                                        MINT32 request_num_end);
    status_t                        buildStereoP1Pipeline(
                                        ContextBuilderContent p1Main1Content,
                                        ContextBuilderContent p1Main2Content,
                                        MBOOL bRecording,
                                        MBOOL bMain2MONO);
    status_t                        buildStereoP1Pipeline_Main1();
    status_t                        buildStereoP1Pipeline_Main2();
    status_t                        buildStereoP2Pipeline();
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.(nested calsses)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    class StereoRequestUpdater
        : public virtual android::RefBase
        , public IRequestUpdater
    {
        public:
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            //  IRequestUpdater Interface.
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            virtual MERROR                          updateRequestSetting(
                                                        IMetadata* appSetting,
                                                        IMetadata* halSetting
                                                    );

            virtual MERROR                          updateParameters(
                                                        IMetadata* setting
                                                    );
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            //  StereoRequestUpdater Interface.
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            struct StreamType{
                enum {
                    RESIZED,
                    FULL,
                    RESIZED_MAIN2,
                    FULL_MAIN2
                };
            };
                                                    StereoRequestUpdater()=delete;

                                                    StereoRequestUpdater(
                                                        sp<StereoFlowControl>  pStereoFlowControl,
                                                        sp<StereoBufferSynchronizer> pStereoBufferSynchronizer,
                                                        sp<IParamsManagerV3> pParamsManagerV3
                                                    );

            virtual MVOID                           onLastStrongRef( const void* /*id*/);

            virtual MERROR                          addSelector(sp<StereoSelector> pSelector, MINT32 streamType);

            virtual MERROR                          addPool(sp<StereoBufferPool> pPool, MINT32 streamType);

            virtual MERROR                          setSensorParams(StereoPipelineSensorParam main1, StereoPipelineSensorParam main2);

            virtual MVOID                           setPreviewStreamId(MINT32 streamId);
            virtual MVOID                           setFdStreamId(MINT32 streamId);

            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            //  StereoRequestUpdater data members
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        protected:
            sp<StereoSelector>                                 mSelector_resized            = NULL;
            sp<StereoSelector>                                 mSelector_full               = NULL;
            sp<StereoSelector>                                 mSelector_resized_main2      = NULL;
            sp<StereoSelector>                                 mSelector_full_main2         = NULL;

            sp<StereoBufferPool>                               mPool_resized                = NULL;
            sp<StereoBufferPool>                               mPool_full                   = NULL;
            sp<StereoBufferPool>                               mPool_resized_main2          = NULL;
            sp<StereoBufferPool>                               mPool_full_main2             = NULL;

            StereoPipelineSensorParam                          mSensorParam                 = {0, MSize(0,0), 0, 0};
            StereoPipelineSensorParam                          mSensorParam_main2           = {0, MSize(0,0), 0, 0};

            sp<StereoFlowControl>                              mpStereoFlowControl          = NULL;
            sp<StereoBufferSynchronizer>                       mpStereoBufferSynchronizer   = NULL;
            sp<IParamsManagerV3>                               mpParamsMngrV3               = NULL;

            int                                                mReqNo                       = 0;
            int                                                mLogLevel                    = 0;

            /*
            * To update special params for Stereo Cam
            */
            IMetadata                                          mAppSetting;

            mutable RWLock                                     mLock;
            MINT32                                             mPrvStreamId = 0;
            MINT32                                             mFdStreamId = 0;
    };

    class StereoMain2RequestUpdater
        :public StereoRequestUpdater
    {
        public:
                                                    StereoMain2RequestUpdater(
                                                        sp<StereoFlowControl>  pStereoFlowControl,
                                                        sp<StereoBufferSynchronizer> pStereoBufferSynchronizer,
                                                        sp<IParamsManagerV3> pParamsManagerV3
                                                    );

            virtual MERROR                          updateRequestSetting(
                                                        IMetadata* appSetting,
                                                        IMetadata* halSetting
                                                    );

            virtual MERROR                          updateParameters(IMetadata* setting);

    };
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    MERROR                                      setCamClient(
                                                    char* const name,
                                                    StreamId streamId,
                                                    Vector<PipelineImageParam>& vImageParam,
                                                    Vector<MUINT32> clientMode
                                                );

    MERROR                                      checkNotifyCallback(
                                                    IMetadata* meta
                                                );

    int32_t                                     getOpenId_Main2() const;

    int32_t                                     getOpenId_P2Prv() const;

    MINT32                                      getPipelineMode() const {return mPipelineMode;};

    MRect                                       getActiveArrayCrop() const {return mActiveArrayCrop;};

    MRect                                       getActiveArrayCrop_Main2() const {return mActiveArrayCrop_Main2;};

    MRect                                       getSensorDomainCrop() const {return mSensorDomainCrop;};

    MRect                                       getSensorDomainCrop_Main2() const {return mSensorDomainCrop_Main2;};

    sp<IParamsManager>                          getParamsMgr() const{return mpParamsManagerV3->getParamsMgr();};

    sp<IParamsManagerV3>                        getParamsMgrV3() const{return mpParamsManagerV3;};

    MUINT8                                      getIspProfile() const{return mIspProfile;};

    MBOOL                                       waitP2PrvReady();
private:
    sp<ILegacyPipeline>                         constructP1Pipeline_Main1(
                                                    ContextBuilderContent content
                                                );

    sp<ILegacyPipeline>                         constructP1Pipeline_Main2(
                                                    ContextBuilderContent content
                                                );

    sp<NodeConfigDataManager>                   buildNodeConfigDataManager(
                                                    ContextBuilderContent content);

    sp<ILegacyPipeline>                         constructP2Pipeline_PrvCap_BB(
                                                    StereoModeContextBuilderContent content,
                                                    sp<ImageStreamManager>& spImageStreamManager
                                                );

    sp<ILegacyPipeline>                         constructP2Pipeline_Rec_BB(
                                                    StereoModeContextBuilderContent content
                                                );

#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    sp<ILegacyPipeline>                         constructP2Pipeline_PrvCap_BM(
                                                    StereoModeContextBuilderContent content,
                                                    sp<ImageStreamManager>& spImageStreamManager
                                                );
#endif
    sp<ILegacyPipeline>                         constructP2Pipeline_PrvCap_3rdParty(
                                                    StereoModeContextBuilderContent content,
                                                    sp<ImageStreamManager>& spImageStreamManager
                                                );
    MINT                                        getImageStreamIndex(
                                                    ImageStreamManager::image_stream_info_pool_setting* pTable,
                                                    NSCam::v3::StreamId_T id
                                                );
    MERROR                                      updateRrzoInfo(
                                                    MBOOL bMain1,
                                                    ImageStreamManager::image_stream_info_pool_setting* pImgTable,
                                                    MUINT32 mode
                                                );
    MERROR                                      updateImgoInfo(
                                                    MBOOL bMain1,
                                                    ImageStreamManager::image_stream_info_pool_setting* pImgTable,
                                                    MUINT32 mode
                                                );
    MUINT32                                     prepareSensor(MUINT sensorId, StereoPipelineSensorParam& sensorParam);

    MERROR                                      enterMMDVFSScenario();

    MERROR                                      exitMMDVFSScenario();

    MERROR                                      enterPerformanceScenario(ScenarioMode newScenario);

    MERROR                                      exitPerformanceScenario();

    MERROR                                      setBufferPoolsReturnMode(MINT32 returnMode);

    virtual MINT32                              getScenario() const;
    virtual android::status_t                   changeToPreviewStatus();

    MERROR                                      getSelector(
                                                    MINT32 sensorId,
                                                    MINT32 streamId,
                                                    sp<StereoSelector>& pStreamBufferProvider);

    MERROR                                      prepareMetadataAndImageBuffer(
                                                    IMetadata* appSetting,
                                                    IMetadata* halSetting,
                                                    CaptureBufferData* bufferData
                                                    );
    MERROR                                      buildCaptureStream(BufferList& vDstStreams, MBOOL isDng);
#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    MERROR                                      buildCaptureStream_DeNoise(BufferList& vDstStreams, MBOOL isDng);
#endif
    MERROR                                      buildCaptureStream_3rdParty(BufferList& vDstStreams, MBOOL isDng);


    MERROR                                      setCaptureBufferProvoider(JpegParam jpegParam, ShotParam shotParam, MBOOL isDng);
    MERROR                                      setCaptureBufferProvoider_DeNoise(
                                                    JpegParam jpegParam,
                                                    ShotParam shotParam,
                                                    MBOOL isDng,
                                                    MBOOL needReAllocate,
                                                    MINT32 bufferCount);
    MERROR                                      setCaptureBufferProvoider_3rdParty(JpegParam jpegParam, ShotParam shotParam, MBOOL isDng);

    MERROR                                      allocProvider(sp<IImageStreamInfo> streamInfo, sp<NSCam::v1::BufferCallbackHandler> pCallbackHandler);

    MERROR                                      allocProvider(sp<IImageStreamInfo> streamInfo);

    MBOOL                                       sendTimestampToProvider(MUINT32 requestNo, StreamId_T id, MINT64 timestamp);

    MERROR                                      uninitPipelineAndRelatedResource();

    MERROR                                      updateParametersCommon(IMetadata* setting, sp<IParamsManagerV3> pParamsMgrV3, StereoPipelineSensorParam& sensorParam);

    MERROR                                      doSynchronizerBinding();

    MERROR                                      setRequsetTypeForAllPipelines(MINT32 type);

    MBOOL                                       needReAllocBufferProvider(StereoShotParam shotParam);

    MVOID                                       doBMDNHALInit();

    static MVOID*                               doParrallelInit(void* arg);
    static void*                                doThreadInit(void* arg);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Data Members.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    enum {
        PipelineMode_PREVIEW,
        PipelineMode_RECORDING,
        PipelineMode_ZSD
    };

    char*                                       mName                           = NULL;
    int                                         mLogLevel                       = 0;
    MINT32                                      mOpenId                         = -1;
    MINT32                                      mOpenId_main2                   = -1;
    MINT32                                      mOpenId_P2Prv                   = -1;
    MINT32                                      mPipelineMode                   = PipelineMode_PREVIEW;

    // Pipelines
    // This pipeline consists of DepthNode and BokehNode
    sp< ILegacyPipeline >                       mpPipeline_P2                   = NULL;
    // This pipeline consists of P1Node
    sp< ILegacyPipeline >                       mpPipeline_P1                   = NULL;
    // This pipeline consists of P1Node_Main2
    sp< ILegacyPipeline >                       mpPipeline_P1_Main2             = NULL;

    // RequestControllers. There are three requestController for each pipeline
    sp<IRequestController>                      mpRequestController_P2          = NULL;
    sp<IRequestController>                      mpRequestController_P1          = NULL;
    sp<IRequestController>                      mpRequestController_P1_Main2    = NULL;
    // This updater is used by mpPipeline, it will block request thread and try to get meta and buffer from consumers
    sp<StereoRequestUpdater>                    mpStereoRequestUpdater_P2       = NULL;
    sp<StereoMain2RequestUpdater>               mpStereoRequestUpdater_P1_Main2 = NULL;

    sp<IParamsManagerV3>                        mpParamsManagerV3               = NULL;
    sp<ImgBufProvidersManager>                  mpImgBufProvidersMgr            = NULL;
    StereoPipelineSensorParam                   mSensorParam                    = {0, MSize(0,0), 0, 0};
    StereoPipelineSensorParam                   mSensorParam_main2              = {0, MSize(0,0), 0, 0};
    sp<INotifyCallback>                         mpCamMsgCbInfo                  = NULL;

    sp<IResourceContainer>                      mpResourceContainier            = NULL;
    sp<IResourceContainer>                      mpResourceContainierMain2       = NULL;
    sp<IResourceContainer>                      mpResourceContainierP2Prv       = NULL;

    sp<StereoSelector>                          mpStereoSelector_RESIZER        = NULL;
    sp<StereoSelector>                          mpStereoSelector_RESIZER_MAIN2  = NULL;
    sp<StereoSelector>                          mpStereoSelector_OPAQUE         = NULL;
    sp<StereoSelector>                          mpStereoSelector_OPAQUE_MAIN2   = NULL;

    sp<StereoBufferSynchronizer>                mpStereoBufferSynchronizer      = NULL;

    sp<StereoBufferPool>                        mpStereoBufferPool_RESIZER      = NULL;
    sp<StereoBufferPool>                        mpStereoBufferPool_RESIZER_MAIN2= NULL;
    sp<StereoBufferPool>                        mpStereoBufferPool_OPAQUE       = NULL;
    sp<StereoBufferPool>                        mpStereoBufferPool_OPAQUE_MAIN2 = NULL;

    MBOOL                                       mbDVFSLevel                     = MFALSE;
    MBOOL                                       mbEnableP2Prv                   = MTRUE;
    ScenarioMode                                mCurrentScenarioMode            = ScenarioMode::NONE;

    MRect                                       mActiveArrayCrop                = MRect(0, 0);
    MRect                                       mActiveArrayCrop_Main2          = MRect(0, 0);

    MRect                                       mSensorDomainCrop               = MRect(0, 0);
    MRect                                       mSensorDomainCrop_Main2         = MRect(0, 0);

    sp<IScenarioControl>                        mpScenarioCtrl                  = NULL;

    mutable Mutex                               mP2PrvLock;
    mutable Condition                           mCondP2PrvLock;
    MBOOL                                       isStopPreview                   = MFALSE;
    mutable Mutex                               mP2StopPrvLock;
    sp<StereoShot>                              mpShot                          = nullptr;
    sp<NSCam::v1::BufferCallbackHandler>        mpCallbackHandler               = nullptr;
    MINT32 miCapCounter = 3000;
    mutable Mutex                               mCaptureLock;
    mutable Condition                           mCondCaptureLock;
    MBOOL                                       isDNGEnable                     = MFALSE;
    sp<ImageStreamManager>                      mpImageStreamManager            = nullptr;

    MINT32                                      mCurrentStereoWarning           = -1;

    // temp solution
    MBOOL                                       mbIgnoreStereoWarning           = MFALSE;

    MUINT8                                      mIspProfile                     = NSIspTuning::EIspProfile_N3D_Preview;

    MINT32                                      mCurrentSensorModuleType        = -1;
    MINT32                                      mCurrentStereoMode              = -1;
    sp<ITemperatureMonitor>                     mpTemperatureMonitor            = nullptr;

    StereoShotParam                             mCurrentStereoShotParam;
    DefaultKeyedVector<MUINT32, std::chrono::time_point<std::chrono::system_clock> > mvCaptureRequests;

#if (MTKCAM_STEREO_DENOISE_SUPPORT == '1')
    pthread_t                                   mThreadHandle;
    NS_BMDN_HAL::BMDN_HAL*                      mpBMDN_HAL                      = nullptr;
#endif
    MBOOL                                       mbUsingThreadInit               = MFALSE;
    pthread_t                                   mPipelineInitThreadHandle;
    StereoModeContextBuilderContent             mCurrentPipelineContent;
};

/******************************************************************************
*
******************************************************************************/
};  //namespace NSPipelineContext
};  //namespace v1
};  //namespace NSCam
#endif  //_MTK_HARDWARE_MTKCAM_ADAPTER_STEREO_STEREOFLOWCONTROL_H_

