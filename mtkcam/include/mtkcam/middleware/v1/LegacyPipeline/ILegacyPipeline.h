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

#ifndef _MTK_HARDWARE_INCLUDE_MTKCAM_V1_LEGACYPIPELINE_ILEGACYPIPELINE_H_
#define _MTK_HARDWARE_INCLUDE_MTKCAM_V1_LEGACYPIPELINE_ILEGACYPIPELINE_H_
//
#include <utils/RefBase.h>
#include <utils/Vector.h>

#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/pipeline/PipelineContext.h>
#include "processor/ResultProcessor.h"
#include "buffer/StreamBufferProvider.h"

typedef NSCam::v3::StreamId_T StreamId;
using namespace NSCam;
using namespace NSCam::v3::Utils;
using namespace NSCam::v3::NSPipelineContext;

using namespace android;

/******************************************************************************
 *
 ******************************************************************************/
//
namespace NSCam {
namespace v1 {
namespace NSLegacyPipeline {

struct SettingSet
{
    IMetadata appSetting;
    IMetadata halSetting;
};

struct BufferSet
{
    StreamId             streamId;
    // true : get buffer when contructing pipeline frame
    MBOOL                criticalBuffer;
};

struct BufferList
    : Vector< BufferSet >
{
};

class ILegacyPipeline
    : public virtual RefBase
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Parameters.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    struct ResultSet
        : KeyedVector<StreamId_T, IMetadata>
    {
    };
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////

    virtual MERROR                      flush()=0;

    virtual MERROR                      waitUntilDrained()=0;

    virtual MERROR                      waitUntilDrainedAndFlush()=0;

    virtual wp<ResultProcessor>         getResultProcessor() = 0;

    virtual wp<TimestampProcessor>      getTimestampProcessor() = 0;

    virtual MERROR                      submitSetting(
                                            MINT32    const requestNo,
                                            IMetadata& appMeta,
                                            IMetadata& halMeta,
                                            ResultSet* pResultSet = nullptr
                                        ) = 0;

    virtual MERROR                      submitRequest(
                                            MINT32    const   requestNo,
                                            IMetadata&        appMeta,
                                            IMetadata&        halMeta,
                                            Vector<BufferSet> vDstStreams
                                        ) = 0;

public:     ////
    virtual sp<IImageStreamInfo>        queryImageStreamInfo(StreamId const streamId) = 0;

    virtual sp<IMetaStreamInfo>         queryMetaStreamInfo(StreamId const streamId) = 0;

    virtual sp<StreamBufferProvider>    queryProvider(StreamId const streamId) = 0;

    virtual MVOID                       setRequestBuilder(
                                            sp<RequestBuilder> const pRequestBuilder) = 0;

    virtual MVOID                       getMetaIOMapInfo(
                                            KeyedVector<NSCam::v3::Pipeline_NodeId_T,
                                            NSCam::v3::NSPipelineContext::IOMapSet >& vInfo) = 0;
};

class IFeatureFlowControl
    : public virtual RefBase
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     //// request

    /**
     * Submit user defined request. Multiple settings have same output.
     *
     * @param[in] vSettings: [appSetting, halSetting] for pipeline frame.
     *
     * @param[in] vDstStreams: all desired image output stream id.
     *
     * @param[in] vRequestNo: request number for user's setting.
     */
    virtual MERROR                  submitRequest(
                                        Vector< SettingSet >          vSettings,
                                        BufferList                    vDstStreams,
                                        Vector< MINT32 >&             vRequestNo
                                    ) = 0;

    /**
     * Submit user defined request. Each settings has its own output.
     *
     * @param[in] vSettings: [appSetting, halSetting] for pipeline frame.
     *
     * @param[in] vDstStreams: desired image output stream id for each setting.
     *
     * @param[in] vRequestNo: request number for user's setting.
     */
    virtual MERROR                  submitRequest(
                                        Vector< SettingSet >          vSettings,
                                        Vector< BufferList >          vDstStreams,
                                        Vector< MINT32 >&             vRequestNo
                                    ) = 0;

public:     //// preview
    /**
     * Pause preview thread to send request to pipeline.
     *
     * @param[in] stopPipeline:
     *            true : pipeline stop but not destroy
     *            false: pipeline still runnig but will not output preview buffer
     *
     * @return
     *  0 indicates success; non-zero indicates an error code.
     */
    virtual MERROR                  pausePreview( MBOOL stopPipeline ) = 0;

    /**
     * Resume preview thread sending request to pipeline.
     *
     * @return
     *  0 indicates success; non-zero indicates an error code.
     */
    virtual MERROR                  resumePreview() = 0;

    /**
     * Get sensor mode from flow control
     *
     * @param[out] sensorMode: current sensor mode
     */
    virtual MUINT                   getSensorMode() = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  //namespace NSLegacyPipeline
};  //namespace v1
};  //namespace NSCam
#endif  //_MTK_HARDWARE_INCLUDE_MTKCAM_V1_LEGACYPIPELINE_ILEGACYPIPELINE_H_
