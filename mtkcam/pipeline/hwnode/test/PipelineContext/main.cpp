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
#define LOG_TAG "PipielineContextTest"
//
#include <mtkcam/utils/std/Log.h>
//
#include <stdlib.h>
#include <utils/Errors.h>
#include <utils/List.h>
#include <utils/RefBase.h>
//
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#include <mtkcam/pipeline/pipeline/IPipelineDAG.h>
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>

#include <mtkcam/pipeline/pipeline/PipelineContext.h>
//
#include <mtkcam/pipeline/utils/streambuf/StreamBufferPool.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
//
#include <mtkcam/utils/imgbuf/IIonImageBufferHeap.h>
//
#include <mtkcam/drv/IHalSensor.h>
#ifdef USING_MTK_LDVT /*[EP_TEMP]*/ //[FIXME] TempTestOnly - USING_FAKE_SENSOR
#include <drv/src/isp/mt6797/iopipe/CamIO/FakeSensor.h>
#endif
//
#include <mtkcam/pipeline/hwnode/NodeId.h>
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/hwnode/P2Node.h>
#include <mtkcam/pipeline/hwnode/JpegNode.h>
//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
//
#include <mtkcam/utils/metastore/ITemplateRequest.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <hardware/camera3.h> // for template
//
#include <mtkcam/drv/iopipe/CamIO/INormalPipe.h>

#include <mtkcam/utils/metadata/IMetadataTagSet.h>
#include <mtkcam/utils/metadata/IMetadataConverter.h>

using namespace NSCam;
using namespace v3;
using namespace NSCam::v3::Utils;
using namespace android;
using namespace NSCam::v3::NSPipelineContext;

// [FIXME], temporarily assign fake value, need to merge flag
#define MTK_P2NODE_UT_PLUGIN_FLAG       0x1fffffff
#define MTK_P2NODE_UT_PLUGIN_FLAG_RAW   0x1fffffff
#define MTK_P2NODE_UT_PLUGIN_FLAG_YUV   0x1fffffff

/******************************************************************************
 *
 ******************************************************************************/
#if 0
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#else
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);\
                                    printf("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);\
                                    printf("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);\
                                    printf("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);\
                                    printf("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);\
                                    printf("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg);
#endif
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
//
#define TEST(cond, result)          do { if ( (cond) == (result) ) { printf("Pass\n"); } else { printf("Failed\n"); } }while(0)
#define FUNCTION_IN     MY_LOGD_IF(1, "+");

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif
#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

typedef enum
{
  eTEST_CASE_P1,
  eTEST_CASE_P1_P2,
  eTEST_CASE_P1_P2_CROP,
  eTEST_CASE_P1_P2_MNR,
  eTEST_CASE_P1_P2_SWNR,
  eTEST_CASE_P1_P2_RAW,
  eTEST_CASE_P1_P2_RAW_YUV,
  eTEST_CASE_P1_P2_RAW_MNR,
  eTEST_CASE_P1_P2_RAW_SWNR,
  eTEST_CASE_P1_P2_JPEG,
  eTEST_CASE_P1_P2_JPEG_WITH_RAW_YUV,
  eTEST_CASE_MAX,
} eTestCase ;

static
sp<ImageStreamInfo> createRawImageStreamInfo(
        char const*         streamName,
        StreamId_T          streamId,
        MUINT32             streamType,
        size_t              maxBufNum,
        size_t              minInitBufNum,
        MUINT               usageForAllocator,
        MINT                imgFormat,
        MSize const&        imgSize,
        size_t const        stride
        );

static
sp<ImageStreamInfo> createImageStreamInfo(
        char const*         streamName,
        StreamId_T          streamId,
        MUINT32             streamType,
        size_t              maxBufNum,
        size_t              minInitBufNum,
        MUINT               usageForAllocator,
        MINT                imgFormat,
        MSize const&        imgSize,
        MUINT32             transform = 0
        );
/******************************************************************************
 *
 ******************************************************************************/

namespace {

    enum STREAM_ID{
        STREAM_ID_RAW1 = 1,     // resized raw
        STREAM_ID_RAW2,         // full raw
        STREAM_ID_YUV1,         // P2 out, display
        STREAM_ID_YUV_MAIN,     // P2 out, YUV main
        STREAM_ID_YUV_THUMBNAIL,// P2 out, YUV thumbnail
        STREAM_ID_JPEG,         // Jpeg Out
        //
        STREAM_ID_METADATA_CONTROL_APP,
        STREAM_ID_METADATA_CONTROL_HAL,
        STREAM_ID_METADATA_RESULT_P1_APP,
        STREAM_ID_METADATA_RESULT_P1_HAL,
        STREAM_ID_METADATA_RESULT_P2_APP,
        STREAM_ID_METADATA_RESULT_P2_HAL,
        STREAM_ID_METADATA_RESULT_JPEG_APP,
        //STREAM_ID_APPMETADATA2,
        //STREAM_ID_HALMETADATA1
    };

    enum NODE_ID{
        NODE_ID_NODE1 = 1,
        NODE_ID_NODE2,
        NODE_ID_FAKE
    };
    //
    IHalSensor* mpSensorHalObj = NULL;
    //
    static MUINT32 gSensorId = 0;
    static MUINT32 requestTemplate = CAMERA3_TEMPLATE_PREVIEW;
    //static bool test_full = true;
    //static bool test_resize = true;

    P1Node::SensorParams        gSensorParam;
    P1Node::ConfigParams        gP1ConfigParam;
    P2Node::ConfigParams        gP2ConfigParam;
    //
    MSize                       gRrzoSize;
    const MINT                  gRrzoFormat = eImgFmt_FG_BAYER10;
    size_t                      gRrzoStride;
    //
    MSize                       gImgoSize;
    const MINT                  gImgoFormat = eImgFmt_BAYER10;
    size_t                      gImgoStride;
    //
    android::sp<PipelineContext> gContext;
    //
    // StreamInfos
    sp<IMetaStreamInfo>         gControlMeta_App;
    sp<IMetaStreamInfo>         gControlMeta_Hal;
    sp<IMetaStreamInfo>         gResultMeta_P1_App;
    sp<IMetaStreamInfo>         gResultMeta_P1_Hal;
    sp<IMetaStreamInfo>         gResultMeta_P2_App;
    sp<IMetaStreamInfo>         gResultMeta_P2_Hal;
    sp<IMetaStreamInfo>         gResultMeta_Jpeg_App;
    //
    sp<IImageStreamInfo>        gImage_RrzoRaw;
    sp<IImageStreamInfo>        gImage_ImgoRaw;
    sp<IImageStreamInfo>        gImage_Yuv;
    sp<IImageStreamInfo>        gImage_YuvJpeg;
    sp<IImageStreamInfo>        gImage_YuvThumbnail;
    sp<IImageStreamInfo>        gImage_Jpeg;


    // requestBuilder
    sp<RequestBuilder>          gRequestBuilderCap;
    sp<RequestBuilder>          gRequestBuilderP1;
    sp<RequestBuilder>          gRequestBuilderPrv;

}; // namespace

unsigned int testCase = 0;    // set for number of test case
int testRequestCnt    = 1;    // set for request count
int testDumpImage     = 0;    // dump raw,yuv,jpeg to files (raw or yuv: /sdcard/camera_dump/, jpeg: /data/)
int testIntervalMs    = 0;    // set for the time interval between requests in MS
int testZoom10X       = 10;   // set for zoom in 10X expression (ex: 2-multiple is 20)
int testCropAlign     = 0;    // set for crop alignment (0:center 1:LT 2:RT 3:LB 4:RB)

/******************************************************************************
 *
 ******************************************************************************/
static void help(char *pProg)
{
    printf("Pipeline test Usage:\n");
    printf("\t\t%s [testCase] [RequestCnt] [dumpImage] [IntervalMs]\n\n", pProg);
    printf("\t\t testCase:(MUST)\n");
    printf("\t\t\t number of test case\n");
    printf("\t\t\t 0: P1\n");
    printf("\t\t\t 1: P1+P2\n");
    printf("\t\t\t 2: P1+P2, crop different region\n");
    printf("\t\t\t 3: P1+P2(+MNR)\n");
    printf("\t\t\t 4: P1+P2(+SWNR)\n");
    printf("\t\t\t 5: P1+P2(+RawPlugIn)\n");
    printf("\t\t\t 6: P1+P2(+RawPlugIn+YuvPlugIn)\n");
    printf("\t\t\t 7: P1+P2(+RawPlugIn+MNR)\n");
    printf("\t\t\t 8: P1+P2(+RawPlugIn+SWNR)\n");
    printf("\t\t\t 9: P1+P2+Jpeg, (n -1) preview request + (1) Capture request\n");
    printf("\t\t\t 10: case 9 with (RawPlugIn+YuvPlugIn)\n");
    printf("\t\t RequestCnt:\n");
    printf("\t\t\t request count\n");
    printf("\t\t dumpImage:\n");
    printf("\t\t\t dump raw,yuv,jpeg to files (raw or yuv: /sdcard/camera_dump/, jpeg: /data/)\n");
    printf("\t\t IntervalMs:\n");
    printf("\t\t\t the time interval between requests in MS\n");
//    printf("\t\t Zoom10X:\n");
//    printf("\t\t\t zoom in 10X expression (ex: 2-multiple is 20)\n");
//    printf("\t\t CropAlign:\n");
//    printf("\t\t\t crop alignment (0:center 1:LT 2:RT 3:LB 4:RB)\n");
    printf("\n\n Default is :%s %d %d %d %d\n",
                             pProg, testCase, testRequestCnt,
                             testDumpImage, testIntervalMs);
}
/******************************************************************************
 *
 ******************************************************************************/
void clear_global_var()
{
    if ( gContext != 0 ){
        MY_LOGD("flush ...\n");
        gContext->flush();
        MY_LOGD("waitUntilDrained ...\n");
        gContext->waitUntilDrained();
        MY_LOGD("set Context to Null ...\n");
        gContext = NULL;
    }
}

template <typename T>
inline MBOOL
tryGetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T & rVal
)
{
    if( pMetadata == NULL ) {
        MY_LOGE("pMetadata == NULL");
        return MFALSE;
    }

    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if( !entry.isEmpty() ) {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }
    return MFALSE;
}

template <typename T>
inline MBOOL
trySetMetadata(
    IMetadata* pMetadata,
    MUINT32 const tag,
    T const& val
)
{
    if( pMetadata == NULL ) {
        MY_LOGE("pMetadata == NULL");
        return MFALSE;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
    return MTRUE;
}
// crop test
#ifdef P1_REQ_CROP_TAG
#undef P1_REQ_CROP_TAG
#endif
#define P1_REQ_CROP_TAG (MTK_P1NODE_SCALAR_CROP_REGION) // [FIXME] sync correct tag

static MSize checkCropSize(NSCam::NSIoPipe::PortID portId,
    EImageFormat fmt, MSize size)
{
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo qry;
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryIn input;
    MSize nSize = size;
    input.width = size.w;

    NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
        portId.index,
        NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX,
        fmt, input, qry);

    nSize.w = qry.x_pix;

    return nSize;
}

static MPoint checkCropStart(NSCam::NSIoPipe::PortID portId,
    MSize in, MSize crop, MINT32 fmt, MUINT32 option)
{
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo qry;
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryIn input;
    MINT32 width = 0;
    MINT32 height = 0;

    switch (option) {

        case 1: // L-T
            width = 0;
            height = 0;
            break;

        case 2: // R-T
            width = (in.w-crop.w);
            height = 0;
            break;

        case 3: // L-B
            width = 0;
            height = (in.h-crop.h);
            break;

        case 4: // R-B
            width = (in.w-crop.w);
            height = (in.h-crop.h);
            break;

        case 0: // Center
        default:
            width = (in.w-crop.w)/2;
            height = (in.h-crop.h)/2;
            break;
    }
    if (width < 0) {
        MY_LOGE("Wrong crop width %d of %d", crop.w, in.w);
        width = 0;
    }
    if (height < 0) {
        MY_LOGE("Wrong crop height %d of %d", crop.h, in.h);
        height = 0;
    }
    input.width = width;
    NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
        portId.index,
        NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_START_X,
        (EImageFormat)fmt, input, qry);

    return MPoint(qry.crop_x, height);
}


/******************************************************************************
 *
 ******************************************************************************/
void setupMetaStreamInfo()
{
    gControlMeta_App =
        new MetaStreamInfo(
                "App:Meta:Control",
                STREAM_ID_METADATA_CONTROL_APP,
                eSTREAMTYPE_META_IN,
                0
                );
    gControlMeta_Hal =
        new MetaStreamInfo(
                "Hal:Meta:Control",
                STREAM_ID_METADATA_CONTROL_HAL,
                eSTREAMTYPE_META_IN,
                0
                );
    gResultMeta_P1_App =
        new MetaStreamInfo(
                "App:Meta:ResultP1",
                STREAM_ID_METADATA_RESULT_P1_APP,
                eSTREAMTYPE_META_OUT,
                0
                );
    gResultMeta_P1_Hal =
        new MetaStreamInfo(
                "Hal:Meta:ResultP1",
                STREAM_ID_METADATA_RESULT_P1_HAL,
                eSTREAMTYPE_META_INOUT,
                0
                );
    gResultMeta_P2_App =
        new MetaStreamInfo(
                "App:Meta:ResultP2",
                STREAM_ID_METADATA_RESULT_P2_APP,
                eSTREAMTYPE_META_OUT,
                0
                );
    gResultMeta_P2_Hal =
        new MetaStreamInfo(
                "Hal:Meta:ResultP2",
                STREAM_ID_METADATA_RESULT_P2_HAL,
                eSTREAMTYPE_META_INOUT,
                0
                );
    gResultMeta_Jpeg_App =
        new MetaStreamInfo(
                "App:Meta:Jpeg",
                STREAM_ID_METADATA_RESULT_JPEG_APP,
                eSTREAMTYPE_META_OUT,
                0
                );
}


void setupImageStreamInfo()
{
    MSize const& jpegSize = gImgoSize;
    MSize const& thumbnailSize = MSize(120,80);
    {// Resized Raw
        MSize const& size = gRrzoSize;
        MINT const format = gRrzoFormat;
        size_t const stride = gRrzoStride;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE ;
        gImage_RrzoRaw = createRawImageStreamInfo(
                "Hal:Image:Resiedraw",
                STREAM_ID_RAW1,
                eSTREAMTYPE_IMAGE_INOUT,
                6, 4,
                usage, format, size, stride
                );
    }
    {// Full Raw
        MSize const& size = gImgoSize;
        MINT const format = gImgoFormat;
        size_t const stride = gImgoStride;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE ;
        gImage_ImgoRaw = createRawImageStreamInfo(
                "Hal:Image:Fullraw",
                STREAM_ID_RAW2,
                eSTREAMTYPE_IMAGE_INOUT,
                6, 4,
                usage, format, size, stride
                );
    }
    {// Display
        MSize const& size = MSize(640,480);
        MINT const format = eImgFmt_YUY2;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE ;//0;
        gImage_Yuv = createImageStreamInfo(
                "Hal:Image:yuv",
                STREAM_ID_YUV1,
                eSTREAMTYPE_IMAGE_INOUT,
                5, 1,
                usage, format, size
                );
    }
    {// JpegYuv
      /* Important!!, a problem will happen if maxnum is 1. (Buffer pool shouldn't be lack by design)
         A thread may be pending on mItemMapLock in pipelineframe's markUserStatus()
         while another thread is waiting this buffer in getImageBuffer() */
        MSize const& size = jpegSize;
        MINT const format = eImgFmt_YUY2;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE ;//0;
        gImage_YuvJpeg = createImageStreamInfo(
                "Hal:Image:YuvJpeg",
                STREAM_ID_YUV_MAIN,
                eSTREAMTYPE_IMAGE_INOUT,
                5, 1,
                usage, format, size
                );
    }
    {// JpegYuvThumbnail
        MSize const& size = thumbnailSize;
        MINT const format = eImgFmt_YUY2;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE ;//0;
        gImage_YuvThumbnail = createImageStreamInfo(
                "Hal:Image:YuvThumbnail",
                STREAM_ID_YUV_THUMBNAIL,
                eSTREAMTYPE_IMAGE_INOUT,
                1, 1,
                usage, format, size
                );
    }
    {// Jpeg
        MSize const& size = jpegSize;
        MINT const format = eImgFmt_BLOB;
        MUINT const usage = 0;//eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READWRITE ;//0;
        gImage_Jpeg = createImageStreamInfo(
                "Hal:Image:jpeg",
                STREAM_ID_JPEG,
                eSTREAMTYPE_IMAGE_INOUT,
                1, 1,
                usage, format, size
                );
    }
}
static void setupProperty()
{
    ::property_set("debug.camera.log", "2");
    if (testDumpImage)
    {
        ::property_set("debug.camera.dump.p2", "1");
        ::property_set("debug.camera.dump.mdp", "1");
    }
    else
    {
        ::property_set("debug.camera.dump.p2", "0");
        ::property_set("debug.camera.dump.mdp", "0");
    }
}
/******************************************************************************
 *
 ******************************************************************************/
void prepareSensor()
{
    #ifdef USING_MTK_LDVT /*[EP_TEMP]*/ //[FIXME] TempTestOnly - USING_FAKE_SENSOR
    IHalSensorList* const pHalSensorList = TS_FakeSensorList::getTestModel();
    MUINT num = pHalSensorList->searchSensors();
    //HalSensorList::buildSensorMetadata();
    #else
    IHalSensorList* const pHalSensorList = MAKE_HalSensorList();
    MUINT num = pHalSensorList->searchSensors();
    #endif
    MY_LOGD("searchSensors (%d)\n", num);

    mpSensorHalObj = pHalSensorList->createSensor("tester", gSensorId);
    if( ! mpSensorHalObj ) {
        MY_LOGE("create sensor failed");
        exit(1);
        return;
    }
    MUINT32    sensorArray[1] = {gSensorId};
    mpSensorHalObj->powerOn("tester", 1, &sensorArray[0]);

    MUINT32 sensorDev = pHalSensorList->querySensorDevIdx(gSensorId);
    SensorStaticInfo sensorStaticInfo;
    memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
    pHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
    //
    gSensorParam.mode = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;//SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
    gSensorParam.size = MSize(sensorStaticInfo.captureWidth, sensorStaticInfo.captureHeight);//(sensorStaticInfo.previewWidth, sensorStaticInfo.previewHeight);
    #ifdef USING_MTK_LDVT /*[EP_TEMP]*/ //[FIXME] TempTestOnly - USING_FAKE_SENSOR
    gSensorParam.fps = 1;
    #else
    gSensorParam.fps = sensorStaticInfo.captureFrameRate/10;//previewFrameRate/10;
    #endif
    gSensorParam.pixelMode = 0;
    //
    mpSensorHalObj->sendCommand(
            pHalSensorList->querySensorDevIdx(gSensorId),
            SENSOR_CMD_GET_SENSOR_PIXELMODE,
            (MUINTPTR)(&gSensorParam.mode),
            (MUINTPTR)(&gSensorParam.fps),
            (MUINTPTR)(&gSensorParam.pixelMode));
    //
    MY_LOGD("sensor params mode %d, size %dx%d, fps %d, pixelmode %d\n",
            gSensorParam.mode,
            gSensorParam.size.w, gSensorParam.size.h,
            gSensorParam.fps,
            gSensorParam.pixelMode);
    //exit(1);
}

/******************************************************************************
 *
 ******************************************************************************/
void closeSensor()
{
    MUINT32    sensorArray[1] = {gSensorId};
    mpSensorHalObj->powerOff("tester", 1, &sensorArray[0]);
}


/******************************************************************************
 *
 ******************************************************************************/
void prepareConfiguration()
{
#define ALIGN_2(x)     (((x) + 1) & (~1))
    //
    {
        MY_LOGD("pMetadataProvider ...\n");
        sp<IMetadataProvider> pMetadataProvider = IMetadataProvider::create(gSensorId);
        if (pMetadataProvider.get() != NULL) {
            MY_LOGD("pMetadataProvider (%p) +++\n", pMetadataProvider.get());
        }
        NSMetadataProviderManager::add(gSensorId, pMetadataProvider.get());
        if (pMetadataProvider.get() != NULL) {
            MY_LOGD("pMetadataProvider (%p) ---\n", pMetadataProvider.get());
        }
    }
    {
        ITemplateRequest* obj = NSTemplateRequestManager::valueFor(gSensorId);
        if(obj == NULL) {
            obj = ITemplateRequest::getInstance(gSensorId);
            NSTemplateRequestManager::add(gSensorId, obj);
        }
    }
    //
    MSize rrzoSize = MSize( ALIGN_2(gSensorParam.size.w / 2), ALIGN_2(gSensorParam.size.h / 2) );
    //
    #if 0
    NSImageio::NSIspio::ISP_QUERY_RST queryRst;
    //
    NSImageio::NSIspio::ISP_QuerySize(
            NSImageio::NSIspio::EPortIndex_RRZO,
            NSImageio::NSIspio::ISP_QUERY_X_PIX|
            NSImageio::NSIspio::ISP_QUERY_STRIDE_PIX|
            NSImageio::NSIspio::ISP_QUERY_STRIDE_BYTE,
            (EImageFormat)gRrzoFormat,
            rrzoSize.w,
            queryRst,
            gSensorParam.pixelMode  == 0 ?
            NSImageio::NSIspio::ISP_QUERY_1_PIX_MODE :
            NSImageio::NSIspio::ISP_QUERY_2_PIX_MODE
            );
    rrzoSize.w = queryRst.x_pix;
    gRrzoSize = MSize(rrzoSize.w, rrzoSize.h);
    gRrzoStride = queryRst.stride_byte;
    #else
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo qry_rrzo;
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryIn input_rrz;
    input_rrz.width = rrzoSize.w;

    NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
        NSCam::NSIoPipe::PORT_RRZO.index,
        NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX|NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE,
        (EImageFormat)gRrzoFormat, input_rrz, qry_rrzo);
    rrzoSize.w = qry_rrzo.x_pix;
    gRrzoSize = MSize(rrzoSize.w, rrzoSize.h);
    gRrzoStride = qry_rrzo.stride_byte;
    #endif
    MY_LOGD("rrzo size %dx%d, stride %d\n", gRrzoSize.w, gRrzoSize.h, (int)gRrzoStride);
    //
    //
    MSize imgoSize = MSize( gSensorParam.size.w, gSensorParam.size.h );
    //
    #if 0
    NSImageio::NSIspio::ISP_QUERY_RST queryRstF;
    //
    NSImageio::NSIspio::ISP_QuerySize(
            NSImageio::NSIspio::EPortIndex_IMGO,
            NSImageio::NSIspio::ISP_QUERY_X_PIX|
            NSImageio::NSIspio::ISP_QUERY_STRIDE_PIX|
            NSImageio::NSIspio::ISP_QUERY_STRIDE_BYTE,
            (EImageFormat)gImgoFormat,
            imgoSize.w,
            queryRstF,
            gSensorParam.pixelMode  == 0 ?
            NSImageio::NSIspio::ISP_QUERY_1_PIX_MODE :
            NSImageio::NSIspio::ISP_QUERY_2_PIX_MODE
            );
    imgoSize.w = queryRstF.x_pix;
    gImgoSize = MSize(imgoSize.w, imgoSize.h);
    gImgoStride = queryRstF.stride_byte;
    #else
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo qry_imgo;
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryIn input_imgo;
    input_imgo.width = imgoSize.w;

    NSCam::NSIoPipe::NSCamIOPipe::INormalPipe::query(
        NSCam::NSIoPipe::PORT_IMGO.index,
        NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX|NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE,
        (EImageFormat)gImgoFormat, input_imgo, qry_imgo);
    imgoSize.w = qry_imgo.x_pix;
    gImgoSize = MSize(imgoSize.w, imgoSize.h);
    gImgoStride = qry_imgo.stride_byte;
    #endif
    MY_LOGD("imgo size %dx%d, stride %d\n", gImgoSize.w, gImgoSize.h, (int)gImgoStride);
    //
    //exit(1);
}
/******************************************************************************
*
*******************************************************************************/
static
MVOID   add_stream_to_set( StreamSet& set, sp<IStreamInfo> pInfo ) {
    if( pInfo.get() ) set.add(pInfo->getStreamId());
}
/******************************************************************************
 *
 ******************************************************************************/
void setupPipelineContext()
{
    gContext = PipelineContext::create("test");
    if( !gContext.get() ) {
        MY_LOGE("cannot create context\n");
        return;
    }
    //
    gContext->beginConfigure();
    //
    // 1. Streams ***************
    //
    // 1.a. check if stream exist
    // 1.b. setup streams
    {
        // Meta
        StreamBuilder(eStreamType_META_APP, gControlMeta_App)
            .build(gContext);
        StreamBuilder(eStreamType_META_HAL, gControlMeta_Hal)
            .build(gContext);
        StreamBuilder(eStreamType_META_APP, gResultMeta_P1_App)
            .build(gContext);
        StreamBuilder(eStreamType_META_HAL, gResultMeta_P1_Hal)
            .build(gContext);
        StreamBuilder(eStreamType_META_APP, gResultMeta_P2_App)
            .build(gContext);
        StreamBuilder(eStreamType_META_HAL, gResultMeta_P2_Hal)
            .build(gContext);
        StreamBuilder(eStreamType_META_APP, gResultMeta_Jpeg_App)
            .build(gContext);
        // Image
        StreamBuilder(eStreamType_IMG_HAL_POOL, gImage_RrzoRaw)
            .build(gContext);
        StreamBuilder(eStreamType_IMG_HAL_POOL, gImage_ImgoRaw)
            .build(gContext);
        StreamBuilder(eStreamType_IMG_HAL_POOL, gImage_Yuv)
            .build(gContext);
        StreamBuilder(eStreamType_IMG_HAL_POOL, gImage_YuvJpeg)
            .build(gContext);
        StreamBuilder(eStreamType_IMG_HAL_RUNTIME, gImage_YuvThumbnail)
            .build(gContext);
        StreamBuilder(eStreamType_IMG_HAL_POOL, gImage_Jpeg)  //FIXME: Need to provide provider or AppImage
            .build(gContext);
    }
    //
    // 2. Nodes   ***************
    //
    // 2.a. check if node exist
    // 2.b. setup nodes
    //
    {
        typedef P1Node                  NodeT;
        typedef NodeActor< NodeT >      MyNodeActorT;
        //
        MY_LOGD("Nodebuilder p1 +\n");
        NodeT::InitParams initParam;
        {
            initParam.openId   = gSensorId;
            initParam.nodeId   = eNODEID_P1Node;
            initParam.nodeName = "P1Node";
        }
        NodeT::ConfigParams cfgParam;
        {
            cfgParam.pInAppMeta        = gControlMeta_App;
            cfgParam.pInHalMeta        = gControlMeta_Hal;
            cfgParam.pOutAppMeta       = gResultMeta_P1_App;
            cfgParam.pOutHalMeta       = gResultMeta_P1_Hal;
            cfgParam.pOutImage_resizer = gImage_RrzoRaw;
            cfgParam.pvOutImage_full.push_back(gImage_ImgoRaw); //N/A
            cfgParam.sensorParams = gSensorParam;
            #if 1 // test the NULL pool parameter
            cfgParam.pStreamPool_resizer = NULL;
            cfgParam.pStreamPool_full    = NULL;
            #else
            cfgParam.pStreamPool_resizer = gContext->queryImageStreamPool(STREAM_ID_RAW1);
            cfgParam.pStreamPool_full    = gContext->queryImageStreamPool(STREAM_ID_RAW2);
            #endif
        }
        //
        sp<MyNodeActorT> pNode = new MyNodeActorT( NodeT::createInstance() );
        pNode->setInitParam(initParam);
        pNode->setConfigParam(cfgParam);
        //
        StreamSet vIn;
        add_stream_to_set(vIn, gControlMeta_App);
        add_stream_to_set(vIn, gControlMeta_Hal);
        //
        StreamSet vOut;
        add_stream_to_set(vOut, gImage_RrzoRaw);
        add_stream_to_set(vOut, gImage_ImgoRaw);
        add_stream_to_set(vOut, gResultMeta_P1_App);
        add_stream_to_set(vOut, gResultMeta_P1_Hal);
        //
        NodeBuilder aNodeBuilder(eNODEID_P1Node, pNode);

        aNodeBuilder.addStream(NodeBuilder::eDirection_IN , vIn);
        aNodeBuilder.addStream(NodeBuilder::eDirection_OUT, vOut);

        if (gImage_RrzoRaw != 0)
            aNodeBuilder.setImageStreamUsage(gImage_RrzoRaw->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
        if (gImage_ImgoRaw != 0)
            aNodeBuilder.setImageStreamUsage(gImage_ImgoRaw->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);

        MERROR ret = aNodeBuilder.build(gContext);

        MY_LOGD("Nodebuilder p1 -\n");

        if( ret != OK ) {
            MY_LOGE("build p1 node error\n");
            return;
        }
    }

    {
        typedef P2Node                  NodeT;
        typedef NodeActor< NodeT >      MyNodeActorT;
        //
        MY_LOGD("Nodebuilder p2 +\n");
        NodeT::InitParams initParam;
        {
            initParam.openId   = gSensorId;
            initParam.nodeId   = eNODEID_P2Node;
            initParam.nodeName = "P2Node";
        }
        NodeT::ConfigParams cfgParam;
        {
            cfgParam.pInAppMeta    = gControlMeta_App;
            cfgParam.pInAppRetMeta = gResultMeta_P1_App;
            cfgParam.pInHalMeta    = gResultMeta_P1_Hal;
            cfgParam.pOutAppMeta   = gResultMeta_P2_App;
            cfgParam.pOutHalMeta   = gResultMeta_P2_Hal;
            if (gImage_ImgoRaw != 0)
                cfgParam.pvInFullRaw.push_back(gImage_ImgoRaw);
            cfgParam.pInResizedRaw = gImage_RrzoRaw;
            if (gImage_Yuv != 0)
                cfgParam.vOutImage.push_back(gImage_Yuv);
            if (gImage_YuvJpeg != 0)
                cfgParam.vOutImage.push_back(gImage_YuvJpeg);
            if (gImage_YuvThumbnail != 0)
                cfgParam.vOutImage.push_back(gImage_YuvThumbnail);
//            cfgParam.pOutFDImage = gImage_YuvThumbnail;  // FIXME: why ? IMG2O can output not only FD but also Thumbnail to save a run.
        }
        //
        sp<MyNodeActorT> pNode = new MyNodeActorT( NodeT::createInstance() );
        pNode->setInitParam(initParam);
        pNode->setConfigParam(cfgParam);
        //
        NodeBuilder aNodeBuilder(eNODEID_P2Node, pNode);
        //
        StreamSet vIn;
        add_stream_to_set(vIn, gImage_ImgoRaw);
        add_stream_to_set(vIn, gImage_RrzoRaw);
        add_stream_to_set(vIn, gControlMeta_App);
        add_stream_to_set(vIn, gResultMeta_P1_App);
        add_stream_to_set(vIn, gResultMeta_P1_Hal);
        //
        StreamSet vOut;
        add_stream_to_set(vOut, gImage_Yuv);
        add_stream_to_set(vOut, gImage_YuvJpeg);
        add_stream_to_set(vOut, gImage_YuvThumbnail);
        add_stream_to_set(vOut, gResultMeta_P2_App);
        add_stream_to_set(vOut, gResultMeta_P2_Hal);

        aNodeBuilder.addStream(NodeBuilder::eDirection_IN , vIn);
        aNodeBuilder.addStream(NodeBuilder::eDirection_OUT, vOut);

        if (gImage_RrzoRaw != 0)
            aNodeBuilder.setImageStreamUsage(gImage_RrzoRaw->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
        if (gImage_ImgoRaw != 0)
            aNodeBuilder.setImageStreamUsage(gImage_ImgoRaw->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
        if (gImage_Yuv != 0)
            aNodeBuilder.setImageStreamUsage(gImage_Yuv    ->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
        if (gImage_YuvJpeg != 0)
            aNodeBuilder.setImageStreamUsage(gImage_YuvJpeg->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);
        if (gImage_YuvThumbnail != 0)
            aNodeBuilder.setImageStreamUsage(gImage_YuvThumbnail->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_WRITE);

        MERROR ret = aNodeBuilder.build(gContext);

        MY_LOGD("Nodebuilder p2 -\n");

        if( ret != OK ) {
            MY_LOGE("build p2 node error\n");
            return;
        }
    }

    {
        typedef JpegNode                NodeT;
        typedef NodeActor< NodeT >      MyNodeActorT;
        //
        MY_LOGD("Nodebuilder Jpeg +\n");
        NodeT::InitParams initParam;
        {
            initParam.openId   = gSensorId;
            initParam.nodeId   = eNODEID_JpegNode;
            initParam.nodeName = "JpegNode";
        }
        NodeT::ConfigParams cfgParam;
        {
            cfgParam.pInAppMeta       = gControlMeta_App;
            cfgParam.pInHalMeta       = gResultMeta_P2_Hal;
            cfgParam.pOutAppMeta      = gResultMeta_Jpeg_App;
            cfgParam.pInYuv_Main      = gImage_YuvJpeg;
            cfgParam.pInYuv_Thumbnail = gImage_YuvThumbnail;
            cfgParam.pOutJpeg         = gImage_Jpeg;
        }
        //
        sp<MyNodeActorT> pNode = new MyNodeActorT( NodeT::createInstance() );
        pNode->setInitParam(initParam);
        pNode->setConfigParam(cfgParam);
        //
        NodeBuilder aNodeBuilder(eNODEID_JpegNode, pNode);
        //
        StreamSet vIn;
        add_stream_to_set(vIn, gImage_YuvJpeg);
        add_stream_to_set(vIn, gImage_YuvThumbnail);
        add_stream_to_set(vIn, gControlMeta_App);
        add_stream_to_set(vIn, gResultMeta_P2_Hal);
        //
        StreamSet vOut;
        add_stream_to_set(vOut, gImage_Jpeg);
        add_stream_to_set(vOut, gResultMeta_Jpeg_App);

        aNodeBuilder.addStream(NodeBuilder::eDirection_IN , vIn);
        aNodeBuilder.addStream(NodeBuilder::eDirection_OUT, vOut);

        if (gImage_YuvJpeg != 0)
            aNodeBuilder.setImageStreamUsage(gImage_YuvJpeg->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
        if (gImage_YuvThumbnail != 0)
            aNodeBuilder.setImageStreamUsage(gImage_YuvThumbnail->getStreamId(), eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_HW_CAMERA_READ);
        if (gImage_Jpeg != 0)
            aNodeBuilder.setImageStreamUsage(gImage_Jpeg->getStreamId(), eBUFFER_USAGE_SW_MASK | eBUFFER_USAGE_HW_CAMERA_WRITE);

        MERROR ret = aNodeBuilder.build(gContext);

        MY_LOGD("Nodebuilder Jpeg -\n");

        if( ret != OK ) {
            MY_LOGE("build Jpeg node error\n");
            return;
        }
    }
    //
    // 3. Pipeline **************
    //
    {
        NodeEdgeSet edges;
        edges.addEdge(eNODEID_P1Node, eNODEID_P2Node);
        edges.addEdge(eNODEID_P2Node, eNODEID_JpegNode);

        MERROR ret = PipelineBuilder()
            .setRootNode(
                    NodeSet().add(eNODEID_P1Node)
                    )
            .setNodeEdges(
                    edges
                    )
            .build(gContext);
        if( ret != OK ) {
            MY_LOGE("build pipeline error\n");
            return;
        }
    }
    //
    gContext->endConfigure();
}
/******************************************************************************
 *
 ******************************************************************************/
void setupRequestBuilder()
{
    {// ONLY P1
        gRequestBuilderP1 = new RequestBuilder();

        gRequestBuilderP1->setIOMap(
                                eNODEID_P1Node,
                                IOMapSet().add(
                                  IOMap()
                                  .addOut(gImage_RrzoRaw->getStreamId())
                                  .addOut(gImage_ImgoRaw->getStreamId())
                                  ),
                                IOMapSet().add(
                                    IOMap()
                                    .addIn(gControlMeta_App->getStreamId())
                                    .addIn(gControlMeta_Hal->getStreamId())
                                    .addOut(gResultMeta_P1_App->getStreamId())
                                    .addOut(gResultMeta_P1_Hal->getStreamId())
                                  )
                              );
        gRequestBuilderP1->setRootNode(
                                NodeSet().add(eNODEID_P1Node)
                              );
    }

    {// Preview Request
        gRequestBuilderPrv = new RequestBuilder();

        gRequestBuilderPrv->setIOMap(
                                eNODEID_P1Node,
                                IOMapSet().add(
                                  IOMap()
                                  .addOut(gImage_RrzoRaw->getStreamId())
                                  .addOut(gImage_ImgoRaw->getStreamId())
                                  ),
                                IOMapSet().add(
                                    IOMap()
                                    .addIn(gControlMeta_App->getStreamId())
                                    .addIn(gControlMeta_Hal->getStreamId())
                                    .addOut(gResultMeta_P1_App->getStreamId())
                                    .addOut(gResultMeta_P1_Hal->getStreamId())
                                  )
                              );
#if 1 // test to use 2 IOMaps to produce Yuv Images
        IOMapSet imgIOMapSet, metaIOMapSet;
        imgIOMapSet.add(
                      IOMap()
                      .addIn(gImage_RrzoRaw->getStreamId())
                      .addOut(gImage_Yuv->getStreamId())
                    );
        imgIOMapSet.add(
                      IOMap()
                      .addIn(gImage_ImgoRaw->getStreamId())
                      .addOut(gImage_YuvJpeg->getStreamId())
                      .addOut(gImage_YuvThumbnail->getStreamId())
                    );
        metaIOMapSet.add(
                      IOMap()
                      .addIn(gControlMeta_App->getStreamId())
                      .addIn(gResultMeta_P1_Hal->getStreamId())
                      .addOut(gResultMeta_P2_App->getStreamId())
                      .addOut(gResultMeta_P2_Hal->getStreamId())
                    );
        gRequestBuilderPrv->setIOMap(
                                eNODEID_P2Node,
                                imgIOMapSet,
                                metaIOMapSet
                              );
#else
        gRequestBuilderPrv->setIOMap(
                                eNODEID_P2Node,
                                IOMapSet().add(
                                  IOMap()
                                  .addIn(gImage_RrzoRaw->getStreamId())
                                  .addOut(gImage_Yuv->getStreamId())
                                  ),
                                IOMapSet().add(
                                    IOMap()
                                    .addIn(gControlMeta_App->getStreamId())
                                    .addIn(gResultMeta_P1_Hal->getStreamId())
                                    .addOut(gResultMeta_P2_App->getStreamId())
                                    .addOut(gResultMeta_P2_Hal->getStreamId())
                                  )
                              );
#endif
        gRequestBuilderPrv->setRootNode(
                                NodeSet().add(eNODEID_P1Node)
                              );
        gRequestBuilderPrv->setNodeEdges(
                                NodeEdgeSet().addEdge(eNODEID_P1Node, eNODEID_P2Node)
                              );
    }

    {// Capture Request
        gRequestBuilderCap = new RequestBuilder();

        gRequestBuilderCap->setIOMap(
                                eNODEID_P1Node,
                                IOMapSet().add(
                                  IOMap()
                                  .addOut(gImage_RrzoRaw->getStreamId())
                                  .addOut(gImage_ImgoRaw->getStreamId())
                                  ),
                                IOMapSet().add(
                                    IOMap()
                                    .addIn(gControlMeta_App->getStreamId())
                                    .addIn(gControlMeta_Hal->getStreamId())
                                    .addOut(gResultMeta_P1_App->getStreamId())
                                    .addOut(gResultMeta_P1_Hal->getStreamId())
                                  )
                              );
        gRequestBuilderCap->setIOMap(
                                eNODEID_P2Node,
                                IOMapSet().add(
                                  IOMap()
                                  .addIn(gImage_ImgoRaw->getStreamId())
                                  .addOut(gImage_Yuv->getStreamId())
                                  .addOut(gImage_YuvJpeg->getStreamId())
                                  .addOut(gImage_YuvThumbnail->getStreamId())
                                  ),
                                IOMapSet().add(
                                    IOMap()
                                    .addIn(gControlMeta_App->getStreamId())
                                    .addIn(gResultMeta_P1_App->getStreamId())
                                    .addIn(gResultMeta_P1_Hal->getStreamId())
                                    .addOut(gResultMeta_P2_App->getStreamId())
                                    .addOut(gResultMeta_P2_Hal->getStreamId())
                                  )
                              );
        gRequestBuilderCap->setIOMap(
                                eNODEID_JpegNode,
                                IOMapSet().add(
                                  IOMap()
                                  .addIn(gImage_YuvJpeg->getStreamId())
                                  .addIn(gImage_YuvThumbnail->getStreamId())
                                  .addOut(gImage_Jpeg->getStreamId())
                                  ),
                                IOMapSet().add(
                                    IOMap()
                                    .addIn(gControlMeta_App->getStreamId())
                                    .addIn(gResultMeta_P2_Hal->getStreamId())
                                    .addOut(gResultMeta_Jpeg_App->getStreamId())
                                  )
                              );
        gRequestBuilderCap->setRootNode(
                                NodeSet().add(eNODEID_P1Node)
                              );

        NodeEdgeSet edges;
        edges.addEdge(eNODEID_P1Node, eNODEID_P2Node);
        edges.addEdge(eNODEID_P2Node, eNODEID_JpegNode);

        gRequestBuilderCap->setNodeEdges(edges);
    }
}
/******************************************************************************
 *
 ******************************************************************************/
IMetaStreamBuffer*
createMetaStreamBuffer(
    android::sp<IMetaStreamInfo> pStreamInfo,
    IMetadata const& rSettings,
    MBOOL const repeating
)
{
    HalMetaStreamBuffer*
    pStreamBuffer =
    HalMetaStreamBuffer::Allocator(pStreamInfo.get())(rSettings);
    //
    pStreamBuffer->setRepeating(repeating);
    //
    return pStreamBuffer;
}


/******************************************************************************
 *
 ******************************************************************************/
sp<IMetaStreamBuffer>
get_default_request()
{
    sp<IMetaStreamBuffer> pSBuffer;

    ITemplateRequest* obj = NSTemplateRequestManager::valueFor(gSensorId);
    if(obj == NULL) {
        obj = ITemplateRequest::getInstance(gSensorId);
        NSTemplateRequestManager::add(gSensorId, obj);
    }
    IMetadata meta = obj->getMtkData(requestTemplate);
    //
    pSBuffer = createMetaStreamBuffer(gControlMeta_App, meta, false);
    //
    return pSBuffer;
}

/******************************************************************************
 *
 ******************************************************************************/
void processRequest()
{
    static int cnt = 0;
    int current_cnt = cnt++;
    //
    MY_LOGD("request %d +\n", current_cnt);
    //
    sp<IPipelineFrame> pFrame;
    //
    sp<IMetaStreamBuffer> pAppMetaControlSB = get_default_request();
    sp<HalMetaStreamBuffer> pHalMetaControlSB =
        HalMetaStreamBuffer::Allocator(gControlMeta_Hal.get())();
    {
        // modify hal control metadata
        IMetadata* pMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
        trySetMetadata<MSize>(pMetadata, MTK_HAL_REQUEST_SENSOR_SIZE, gSensorParam.size);

        // turn off plugin
        trySetMetadata<MUINT8>(pMetadata, MTK_P2NODE_UT_PLUGIN_FLAG, 0);

        // crop test
        if ( 0 )
        {
            MPoint zoomStart = MPoint(0, 0);
            MSize zoomSize = gSensorParam.size;
            zoomSize.w = zoomSize.w * 10 / testZoom10X;
            zoomSize.h = zoomSize.h * 10 / testZoom10X;
            zoomSize = checkCropSize(NSCam::NSIoPipe::PORT_RRZO,
                        (EImageFormat)gRrzoFormat, zoomSize);
            zoomStart = checkCropStart(NSCam::NSIoPipe::PORT_RRZO,
                        gSensorParam.size, zoomSize,
                        (EImageFormat)gRrzoFormat, testCropAlign);
            MY_LOGD("sensor(%d,%d)@zoom(%d.%dX) = crop(%d,%d)(%dx%d)[%d]\n",
                gSensorParam.size.w, gSensorParam.size.h,
                testZoom10X/10, testZoom10X%10,
                zoomStart.x, zoomStart.y, zoomSize.w, zoomSize.h,
                testCropAlign);
            MRect rect = MRect(zoomStart, zoomSize);
            trySetMetadata<MRect>(pMetadata, P1_REQ_CROP_TAG, rect);
        }
        pHalMetaControlSB->unlock(LOG_TAG, pMetadata);
    }
    {
        sp<RequestBuilder> pRequestBuilder;
        switch(testCase)
        {
            case eTEST_CASE_P1:
                pRequestBuilder = gRequestBuilderP1;
                break;

            case eTEST_CASE_P1_P2:
                pRequestBuilder = gRequestBuilderPrv;
                break;

            case eTEST_CASE_P1_P2_CROP:
                pRequestBuilder = gRequestBuilderPrv;

                // crop test, crop 4 corners and center and zoom from 1/2x to 1x
                {
                    IMetadata* pAppMetadata = pAppMetaControlSB->tryWriteLock(LOG_TAG);
                    MRect cropRect_metadata;
                    if( tryGetMetadata<MRect>(pAppMetadata, MTK_SCALER_CROP_REGION, cropRect_metadata) )
                    {
                        int pos = current_cnt%5; // (0:center 1:LT 2:RT 3:LB 4:RB)
                        int zoomRatio = MIN(10 + (current_cnt/5), 20);
                        int w, h, xOffset = 0, yOffset = 0, offset = 4;
                        w = cropRect_metadata.width()  * zoomRatio / 20;
                        h = cropRect_metadata.height() * zoomRatio / 20;

                        switch(pos)
                        {
                          case 0: // center
                              xOffset = (cropRect_metadata.width()  - w - offset)/2;
                              yOffset = (cropRect_metadata.height() - h - offset)/2;
                              break;
                          case 1: // LT
                              break;
                          case 2: // RT
                              xOffset = cropRect_metadata.width()  - w - offset;
                              break;
                          case 3: // LB
                              yOffset = cropRect_metadata.height() - h - offset;
                              break;
                          case 4: // RB
                              xOffset = cropRect_metadata.width()  - w - offset;
                              yOffset = cropRect_metadata.height() - h - offset;
                              break;
                          default:
                              break;
                        }

                        xOffset = MAX(xOffset, 0);
                        yOffset = MAX(yOffset, 0);

                        MRect rect = MRect(MPoint(xOffset, yOffset)+=cropRect_metadata.leftTop(), MSize(w,h));
                        trySetMetadata<MRect>(pAppMetadata, MTK_SCALER_CROP_REGION, rect);
                        MY_LOGD("zoom(pos=%d,ratio=%d)size(%d,%d),offset(%d,%d)\n",
                                pos, zoomRatio,
                                w, h, xOffset, yOffset);
                    }
                    else
                    {
                        MY_LOGE("Can't get MTK_SCALER_CROP_REGION!!");
                    }

                    pAppMetaControlSB->unlock(LOG_TAG, pAppMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_MNR:
                pRequestBuilder = gRequestBuilderPrv;

                {
                    MINT32 nrMode = MTK_NR_MODE_MNR;
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MINT32>(pHalMetadata, MTK_NR_MODE, nrMode);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_SWNR:
                pRequestBuilder = gRequestBuilderPrv;

                {
                    MINT32 nrMode = MTK_NR_MODE_SWNR;
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MINT32>(pHalMetadata, MTK_NR_MODE, nrMode);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_RAW:
                pRequestBuilder = gRequestBuilderPrv;

                {
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MUINT8>(pHalMetadata, MTK_P2NODE_UT_PLUGIN_FLAG, MTK_P2NODE_UT_PLUGIN_FLAG_RAW);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_RAW_YUV:
                pRequestBuilder = gRequestBuilderPrv;

                {
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MUINT8>(pHalMetadata, MTK_P2NODE_UT_PLUGIN_FLAG, MTK_P2NODE_UT_PLUGIN_FLAG_RAW | MTK_P2NODE_UT_PLUGIN_FLAG_YUV);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_RAW_MNR:
                pRequestBuilder = gRequestBuilderPrv;

                {
                    MINT32 nrMode = MTK_NR_MODE_MNR;
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MUINT8>(pHalMetadata, MTK_P2NODE_UT_PLUGIN_FLAG, MTK_P2NODE_UT_PLUGIN_FLAG_RAW);
                    trySetMetadata<MINT32>(pHalMetadata, MTK_NR_MODE, nrMode);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_RAW_SWNR:
                pRequestBuilder = gRequestBuilderPrv;

                {
                    MINT32 nrMode = MTK_NR_MODE_SWNR;
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MUINT8>(pHalMetadata, MTK_P2NODE_UT_PLUGIN_FLAG, MTK_P2NODE_UT_PLUGIN_FLAG_RAW);
                    trySetMetadata<MINT32>(pHalMetadata, MTK_NR_MODE, nrMode);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            case eTEST_CASE_P1_P2_JPEG:
                pRequestBuilder = (current_cnt + 1 == testRequestCnt) ? gRequestBuilderCap : gRequestBuilderPrv;
                break;

            case eTEST_CASE_P1_P2_JPEG_WITH_RAW_YUV:
                pRequestBuilder = (current_cnt + 1 == testRequestCnt) ? gRequestBuilderCap : gRequestBuilderPrv;
                {
                    IMetadata* pHalMetadata = pHalMetaControlSB->tryWriteLock(LOG_TAG);
                    trySetMetadata<MUINT8>(pHalMetadata, MTK_P2NODE_UT_PLUGIN_FLAG, MTK_P2NODE_UT_PLUGIN_FLAG_RAW | MTK_P2NODE_UT_PLUGIN_FLAG_YUV);
                    pHalMetaControlSB->unlock(LOG_TAG, pHalMetadata);
                }
                break;

            default:
                break ;
        }

        if (pRequestBuilder.get())
        {
                pRequestBuilder->setMetaStreamBuffer(
                                      gControlMeta_App->getStreamId(),
                                      pAppMetaControlSB
                                  );
                pRequestBuilder->setMetaStreamBuffer(
                                      gControlMeta_Hal->getStreamId(),
                                      pHalMetaControlSB
                                  );
                pFrame = pRequestBuilder->build(current_cnt, gContext);
        }
        if( ! pFrame.get() )
        {
            MY_LOGE("build request failed\n");
        }
    }
    if (0)  // Dump All MetaData
    {
        IMetadataTagSet const &mtagInfo = IDefaultMetadataTagSet::singleton()->getTagSet();
        sp<IMetadataConverter> mMetaDataConverter = IMetadataConverter::createInstance(mtagInfo);

        MY_LOGD("\n\nDump AppMeta:\n");
        MY_LOGD("==========================================================\n");
        IMetadata* pMetadata = pAppMetaControlSB->tryReadLock(LOG_TAG);
        pMetadata->dump();
        MY_LOGD("==========================================================\n");
        mMetaDataConverter->dumpAll(*pMetadata);
        pAppMetaControlSB->unlock(LOG_TAG, pMetadata);
        MY_LOGD("==========================================================\n");
        MY_LOGD("\n\nDump HalMeta:\n");
        MY_LOGD("==========================================================\n");
        pMetadata = pHalMetaControlSB->tryReadLock(LOG_TAG);
        pMetadata->dump();
//        MY_LOGD("==========================================================\n");
//        mMetaDataConverter->dumpAll(*pMetadata);  // Not Support
        pHalMetaControlSB->unlock(LOG_TAG, pMetadata);
        MY_LOGD("==========================================================\n");
    }
    //
    if( pFrame.get() )
    {
        if( OK != gContext->queue(pFrame) )
        {
            MY_LOGE("queue pFrame failed\n");
        }
    }

    MY_LOGD("request %d -\n", current_cnt);
}
/******************************************************************************
 * dump jpeg file to /data/jpegtest.jpg
 ******************************************************************************/
static void saveJpegtoFile()
{
    MY_LOGD("[PipelineContext] dumpJpeg ...\n");

    StreamId_T const streamId = gImage_Jpeg->getStreamId();

    sp<HalImageStreamBuffer::Allocator::StreamBufferPoolT> pPool_HalImageJpeg;
    //acquireFromPool
    pPool_HalImageJpeg = gContext->queryImageStreamPool(streamId);
    sp<HalImageStreamBuffer> pStreamBuffer;
    MERROR err = pPool_HalImageJpeg->acquireFromPool(
                     "Tester", pStreamBuffer, ::s2ns(30)
                 );

    MY_LOGE_IF(OK!=err || pStreamBuffer==0, "pStreamBuffer==0");

    sp<IImageBufferHeap>   pImageBufferHeap = NULL;
    IImageBuffer*          pImageBuffer = NULL;
    if (pStreamBuffer == NULL) {
        MY_LOGE("pStreamBuffer == NULL\n");
    }
    pImageBufferHeap = pStreamBuffer->tryReadLock(LOG_TAG);

    if (pImageBufferHeap == NULL) {
        MY_LOGE("pImageBufferHeap == NULL\n");
        return;
    }

    pImageBuffer = pImageBufferHeap->createImageBuffer();

    if (pImageBuffer == NULL) {
        MY_LOGE("rpImageBuffer == NULL\n");
        return;
    }
    pImageBuffer->lockBuf(LOG_TAG, eBUFFER_USAGE_SW_MASK);
    MY_LOGD("@@@fist byte:%x, %dx%d\n", *(reinterpret_cast<MINT8*>(pImageBuffer->getBufVA(0))),pImageBuffer->getImgSize().w,pImageBuffer->getImgSize().h);

    pImageBuffer->saveToFile("/data/jpegtest.jpg");

    pImageBuffer->unlockBuf(LOG_TAG);

    pStreamBuffer->unlock(LOG_TAG, pImageBufferHeap.get());
}
/******************************************************************************
 *
 ******************************************************************************/
void finishPipelineContext()
{
    MY_LOGD("waitUntilDrained ...\n");
    gContext->waitUntilDrained();
    MY_LOGD("waitUntilDrained END\n");
}
/******************************************************************************
 *
 ******************************************************************************/
int main(int argc, char** argv)
{
    MY_LOGD("[PipelineContext] start to test\n");
    if (argc < 2)
    {
        help(argv[0]);
        return -1;
    }
    if (argc >= 2)
    {
        testCase = atoi(argv[1]);
        MY_LOGD("[PipelineContext] set testCase: %d\n", testCase);
        if ( testCase >= eTEST_CASE_MAX)
        {
          MY_LOGD("[PipelineContext] testCase is too large\n");
          return -1;
        }
    }
    if(argc >= 3)
    {
        if (argv[2] != NULL) {
            testRequestCnt = atoi(argv[2]);
        }
        MY_LOGD("[PipelineContext] set RequestCnt: %d\n", testRequestCnt);
    }
    if(argc >= 4)
    {
        testDumpImage = atoi(argv[3]);
        MY_LOGD("[PipelineContext] set DumpImage: %d\n", testDumpImage);
    }
    if(argc >= 5) {
        testIntervalMs = atoi(argv[4]);
        MY_LOGD("[PipelineContext] set IntervalMs : %d\n", testIntervalMs);
    }

//    if(argc >= 6) {
//        testZoom10X = atoi(argv[5]);
//        MY_LOGD("[PipelineContext] set Zoom10X : %d\n", testZoom10X);
//    }
//
//    if(argc >= 7) {
//        testCropAlign = atoi(argv[6]);
//        MY_LOGD("[PipelineContext] set CropAlign : %d\n", testCropAlign);
//    }

    MY_LOGD("[PipelineContext] : "
            "testCase(%d) RequestCnt(%d) "
            "DumpImage(%d) IntervalMs(%d) ",
            testCase, testRequestCnt,
            testDumpImage, testIntervalMs);

    MY_LOGD("[PipelineContext] turn on camera log ...\n");
    setupProperty();
    //sensor
    MY_LOGD("[PipelineContext] prepareSensor ...\n");
    prepareSensor();

    MY_LOGD("[PipelineContext] prepareConfiguration ...\n");
    prepareConfiguration();

    MY_LOGD("[PipelineContext] setupMetaStreamInfo ...\n");
    setupMetaStreamInfo();

    MY_LOGD("[PipelineContext] setupImageStreamInfo ...\n");
    setupImageStreamInfo();

    MY_LOGD("[PipelineContext] setupPipelineContext ...\n");
    setupPipelineContext(); // Configure Stage

    MY_LOGD("[PipelineContext] setupRequestBuilder ...\n");
    setupRequestBuilder(); // SetUp RequestBuilder

    int c = 0;
    while( c++ < testRequestCnt )
    {
        MY_LOGD("[PipelineContext] processRequest (%d) +++\n", (c-1));
        processRequest();
        MY_LOGD("[PipelineContext] processRequest (%d) ---\n", (c-1));
        if (testIntervalMs > 0) {
            usleep(testIntervalMs * 1000);
        }
    };
    MY_LOGD("[PipelineContext] finishPipelineContext ...\n");
    finishPipelineContext();

    if( testDumpImage
      && (testCase == eTEST_CASE_P1_P2_JPEG || testCase == eTEST_CASE_P1_P2_JPEG_WITH_RAW_YUV))
        saveJpegtoFile();

    MY_LOGD("[PipelineContext] closeSensor ...\n");
    closeSensor();
    clear_global_var();

    MY_LOGD("[PipelineContext] end of test\n");
    return 0;
}


/******************************************************************************
 *
 ******************************************************************************/
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
)
{
    IImageStreamInfo::BufPlanes_t bufPlanes;
#define addBufPlane(planes, height, stride)                                      \
        do{                                                                      \
            size_t _height = (size_t)(height);                                   \
            size_t _stride = (size_t)(stride);                                   \
            IImageStreamInfo::BufPlane bufPlane= { _height * _stride, _stride }; \
            planes.push_back(bufPlane);                                          \
        }while(0)
    switch( imgFormat ) {
        case eImgFmt_YV12:
            addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
            addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w >> 1);
            addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w >> 1);
            break;
        case eImgFmt_NV21:
            addBufPlane(bufPlanes , imgSize.h      , imgSize.w);
            addBufPlane(bufPlanes , imgSize.h >> 1 , imgSize.w);
            break;
        case eImgFmt_YUY2:
            addBufPlane(bufPlanes , imgSize.h      , imgSize.w << 1);
            break;
        case eImgFmt_BLOB:
            /*
            add 328448 for image size
            standard exif: 1280 bytes
            4 APPn for debug exif: 0xFF80*4 = 65408*4 bytes
            max thumbnail size: 64K bytes
            */
            addBufPlane(bufPlanes , 1              , (imgSize.w * imgSize.h * 12 / 10) + 328448); //328448 = 64K+1280+65408*4
            break;
        default:
            MY_LOGE("format not support yet 0x%x \n", imgFormat);
            break;
    }
#undef  addBufPlane

    sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
                streamName,
                streamId,
                streamType,
                maxBufNum, minInitBufNum,
                usageForAllocator, imgFormat, imgSize, bufPlanes, transform
                );

    if( pStreamInfo == NULL ) {
        MY_LOGE("create ImageStream failed, %s, %ld \n",
                streamName, streamId);
    }

    return pStreamInfo;
}


/******************************************************************************
 *
 ******************************************************************************/
sp<ImageStreamInfo>
createRawImageStreamInfo(
    char const*         streamName,
    StreamId_T          streamId,
    MUINT32             streamType,
    size_t              maxBufNum,
    size_t              minInitBufNum,
    MUINT               usageForAllocator,
    MINT                imgFormat,
    MSize const&        imgSize,
    size_t const        stride
)
{
    IImageStreamInfo::BufPlanes_t bufPlanes;
    //
#define addBufPlane(planes, height, stride)                                      \
        do{                                                                      \
            size_t _height = (size_t)(height);                                   \
            size_t _stride = (size_t)(stride);                                   \
            IImageStreamInfo::BufPlane bufPlane= { _height * _stride, _stride }; \
            planes.push_back(bufPlane);                                          \
        }while(0)
    switch( imgFormat ) {
        case eImgFmt_BAYER10:
        case eImgFmt_FG_BAYER10:
            addBufPlane(bufPlanes , imgSize.h, stride);
            break;
        default:
            MY_LOGE("format not support yet 0x%x \n", imgFormat);
            break;
    }
#undef  addBufPlane

    sp<ImageStreamInfo>
        pStreamInfo = new ImageStreamInfo(
                streamName,
                streamId,
                streamType,
                maxBufNum, minInitBufNum,
                usageForAllocator, imgFormat, imgSize, bufPlanes
                );

    if( pStreamInfo == NULL ) {
        MY_LOGE("create ImageStream failed, %s, %ld \n",
                streamName, streamId);
    }

    return pStreamInfo;
}

