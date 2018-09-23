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

#define LOG_TAG "MtkCam/MetadataProvider.constructStatic"
//
#include "MyUtils.h"
//
#include <dlfcn.h>
//
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/aaa/IDngInfo.h>
#include <mtkcam/utils/metadata/IMetadataConverter.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>


/******************************************************************************
 *
 ******************************************************************************/

//#define MY_LOGI(fmt, arg...)    do {XLOGI(LOG_TAG fmt, ##arg);printf(LOG_TAG fmt "\n", ##arg);} while (0)
//#define MY_LOGD(fmt, arg...)    do {XLOGD(LOG_TAG fmt, ##arg);printf(LOG_TAG fmt "\n", ##arg);} while (0)
//#define MY_LOGE(fmt, arg...)    do {XLOGE("error" LOG_TAG fmt, ##arg);printf(LOG_TAG fmt "\n", ##arg);} while (0)
#define FUNCTION_LOG_START      MY_LOGD("[%s] - E.", __FUNCTION__)
#define FUNCTION_LOG_END        do {if(!ret) MY_LOGE("[%s] fail", __FUNCTION__); MY_LOGD("[%s] - X. ret=%d", __FUNCTION__, ret);} while(0)
#define FUNCTION_LOG_END_MUM    MY_LOGD("[%s] - X.", __FUNCTION__)

#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s] " fmt, __FUNCTION__, ##arg)

#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)

//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)



/******************************************************************************
 *
 ******************************************************************************/

status_t
MetadataProvider::
impConstructStaticMetadata_by_SymbolName(
    String8 const&      s8Symbol,
    IMetadata &metadata
)
{
typedef status_t (*PFN_T)(
        IMetadata &         metadata,
        Info const&         info
    );
    //
    PFN_T pfn = (PFN_T)::dlsym(RTLD_DEFAULT, s8Symbol.string());
    if  ( ! pfn ) {
        MY_LOGW("%s not found", s8Symbol.string());
        return  NAME_NOT_FOUND;
    }
    //
    status_t const status = pfn(metadata, mInfo);
    MY_LOGW_IF(OK != status, "%s returns status[%s(%d)]", s8Symbol.string(), ::strerror(-status), -status);
    //
    return  status;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
MetadataProvider::
impConstructStaticMetadata(
    IMetadata &metadata
)
{
    for (int i = 0; NULL != kStaticMetadataTypeNames[i]; i++)
    {
        char const*const pTypeName = kStaticMetadataTypeNames[i];

        status_t status = OK;
        //
        String8 const s8Symbol_Sensor = String8::format("%s_DEVICE_%s_%s", PREFIX_FUNCTION_STATIC_METADATA, pTypeName, mInfo.getSensorDrvName());
        status = impConstructStaticMetadata_by_SymbolName(s8Symbol_Sensor, metadata);
        if  ( OK == status ) {
            continue;
        }
        //
        String8 const s8Symbol_Common = String8::format("%s_DEVICE_%s_%s", PREFIX_FUNCTION_STATIC_METADATA, pTypeName, "COMMON");
        status = impConstructStaticMetadata_by_SymbolName(s8Symbol_Common, metadata);
        if  ( OK == status ) {
            continue;
        }
        //
        MY_LOGE_IF(0, "Fail for both %s & %s", s8Symbol_Sensor.string(), s8Symbol_Common.string());
    }
    //
    for (int i = 0; NULL != kStaticMetadataTypeNames[i]; i++)
    {
        char const*const pTypeName = kStaticMetadataTypeNames[i];

        status_t status = OK;
        //
        String8 const s8Symbol_Sensor = String8::format("%s_PROJECT_%s_%s", PREFIX_FUNCTION_STATIC_METADATA, pTypeName, mInfo.getSensorDrvName());
        status = impConstructStaticMetadata_by_SymbolName(s8Symbol_Sensor, metadata);
        if  ( OK == status ) {
            continue;
        }
        //
        String8 const s8Symbol_Common = String8::format("%s_PROJECT_%s_%s", PREFIX_FUNCTION_STATIC_METADATA, pTypeName, "COMMON");
        status = impConstructStaticMetadata_by_SymbolName(s8Symbol_Common, metadata);
        if  ( OK == status ) {
            continue;
        }
        //
        MY_LOGE_IF(0, "Fail for both %s & %s", s8Symbol_Sensor.string(), s8Symbol_Common.string());
    }
    //
    return  OK;
}


/******************************************************************************
 *
 ******************************************************************************/
status_t
MetadataProvider::
constructStaticMetadata(sp<IMetadataConverter> pConverter, camera_metadata*& rpDstMetadata, IMetadata& mtkMetadata)
{
    MY_LOGD("construct static metadata\n");

    status_t status = OK;

    //-----(1)-----//
    //get static informtation from customization (with camera_metadata format)
    //calculate its entry count and data count
    if  ( OK != (status = impConstructStaticMetadata(mtkMetadata)) ) {
        MY_LOGE("Unable evaluate the size for camera static info - status[%s(%d)]\n", ::strerror(-status), -status);
        return  status;
    }
    MY_LOGD("Allocating %d entries from customization", mtkMetadata.count());


    //-----(2.1)------//
    //get static informtation from sensor hal moduls (with IMetadata format)
    IMetadata sensorMetadata = MAKE_HalSensorList()->queryStaticInfo(mInfo.getDeviceId());
    MY_LOGD("Allocating %d entries from sensor HAL", sensorMetadata.count());

    //
#if 1
    IMetadata rDngMeta = MAKE_DngInfo(LOG_TAG, mInfo.getDeviceId())->getStaticMetadata();
#else
    IMetadata rDngMeta;
#endif
    MY_LOGD("Allocating %d entries from Dng Info", rDngMeta.count());

    //--- (2.1.1) --- //
    //merge.
    sensorMetadata += rDngMeta;
    for (size_t i = 0; i < sensorMetadata.count(); i++)
    {
        IMetadata::Tag_t mTag = sensorMetadata.entryAt(i).tag();
        mtkMetadata.update(mTag, sensorMetadata.entryAt(i));
    }
    MY_LOGD("Allocating %d entries from customization + sensor HAL + Dng Info", mtkMetadata.count());

#if 0
    //-----(2.2)------//
    //get static informtation from other hal moduls (with IMetadata format)
    IMetadata halmetadata = MAKE_HalSensorList()->queryStaticInfo(mInfo.getDeviceId());

    //calculate its entry count and data count
    entryCount = 0;
    dataCount = 0;


    status = AndroidMetadata::getIMetadata_dataCount(halmetadata, entryCount, dataCount);
    if (status != OK)
    {
        MY_LOGE("get Imetadata count error - status[%s(%d)", ::strerror(-status), -status);
        return status;
    }

    MY_LOGD(
        "Allocating %d entries, %d extra bytes from HAL modules",
        entryCount, dataCount
    );

    addOrSizeInfo.mEntryCount += entryCount;
    addOrSizeInfo.mDataCount += dataCount;

#endif

    //overwrite
    updateData(mtkMetadata);
    //
    #if (PLATFORM_SDK_VERSION >= 21)
    pConverter->convert(mtkMetadata, rpDstMetadata);
    //
    ::sort_camera_metadata(rpDstMetadata);
    #endif

    return  status;
}


/******************************************************************************
 *
 ******************************************************************************/
template<class T>
struct converter {
    converter( T const& tag, T const& srcFormat, T const& dstFormat, IMetadata& data) {
        IMetadata::IEntry entry = data.entryFor(tag);
        copy( srcFormat, dstFormat, entry);
        data.update(tag, entry);
    }

    void copy( T const& srcFormat, T const& dstFormat, IMetadata::IEntry& entry) {
        T input = MTK_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT;
        for(size_t i = 0; i < entry.count(); i+=4) {
            if (entry.itemAt(i, Type2Type<T>())!= srcFormat
                || entry.itemAt(i+3, Type2Type< T >()) == input) {
                continue;
            }
            entry.push_back(dstFormat, Type2Type< T >());
            entry.push_back(entry.itemAt(i+1, Type2Type< T >()), Type2Type< T >());
            entry.push_back(entry.itemAt(i+2, Type2Type< T >()), Type2Type< T >());
            entry.push_back(entry.itemAt(i+3, Type2Type< T >()), Type2Type< T >());
        }
    };
};

void
MetadataProvider::
updateData(IMetadata &rMetadata)
{
    {
        MINT32 maxJpegsize = 0;
        IMetadata::IEntry blobEntry = rMetadata.entryFor(MTK_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
        for(size_t i = 0; i < blobEntry.count(); i+=4) {
            if (blobEntry.itemAt(i, Type2Type<MINT32>())!= HAL_PIXEL_FORMAT_BLOB) {
                continue;
            }
            //avaiblable blob size list should order in descedning.
            MSize maxBlob = MSize(blobEntry.itemAt(i+1, Type2Type<MINT32>()),
                            blobEntry.itemAt(i+2, Type2Type<MINT32>()));
            MINT32 jpegsize = maxBlob.size()*1.2; //*2*0.6
            if (jpegsize > maxJpegsize) {
                maxJpegsize = jpegsize;
            }
            IMetadata::IEntry entry(MTK_JPEG_MAX_SIZE);
            entry.push_back(maxJpegsize, Type2Type< MINT32 >());
            rMetadata.update(MTK_JPEG_MAX_SIZE, entry);
         }
    }

    // update implementation defined
    {
        converter<MINT32>(
            MTK_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
            HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
            rMetadata
        );
        //
        converter<MINT64>(
            MTK_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
            HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
            rMetadata
        );
        //
        converter<MINT64>(
            MTK_SCALER_AVAILABLE_STALL_DURATIONS,
            HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
            rMetadata
        );
    }

    // update yv12
    {
        converter<MINT32>(
            MTK_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
            HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_YV12,
            rMetadata
        );
        //
        converter<MINT64>(
            MTK_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
            HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_YV12,
            rMetadata
        );
        //
        converter<MINT64>(
            MTK_SCALER_AVAILABLE_STALL_DURATIONS,
            HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_YV12,
            rMetadata
        );
    }

    // update HDR Request Common Type
    {
        IMetadata::IEntry availReqEntry = rMetadata.entryFor(MTK_REQUEST_AVAILABLE_REQUEST_KEYS);
        availReqEntry.push_back(MTK_HDR_FEATURE_HDR_MODE , Type2Type< MINT32 >());
        rMetadata.update(availReqEntry.tag(), availReqEntry);

        IMetadata::IEntry availResultEntry = rMetadata.entryFor(MTK_REQUEST_AVAILABLE_RESULT_KEYS);
        availResultEntry.push_back(MTK_HDR_FEATURE_VHDR_MODE , Type2Type< MINT32 >());
        rMetadata.update(availResultEntry.tag(), availResultEntry);

        IMetadata::IEntry availCharactsEntry = rMetadata.entryFor(MTK_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS);
        availCharactsEntry.push_back(MTK_HDR_FEATURE_AVAILABLE_VHDR_MODES , Type2Type< MINT32 >());
        availCharactsEntry.push_back(MTK_HDR_FEATURE_AVAILABLE_HDR_MODES , Type2Type< MINT32 >());
        rMetadata.update(availCharactsEntry.tag(), availCharactsEntry);
    }

    // update Available vHDR Mode & HDR Modes
    {
        // Available vHDR mode
        IMetadata::IEntry availVhdrEntry = rMetadata.entryFor(MTK_HDR_FEATURE_AVAILABLE_VHDR_MODES);
        if(availVhdrEntry.isEmpty()){
            IMetadata::IEntry entry(MTK_HDR_FEATURE_AVAILABLE_VHDR_MODES);
            entry.push_back(MTK_HDR_FEATURE_VHDR_MODE_OFF, Type2Type< MINT32 >());
            rMetadata.update(entry.tag(), entry);
            availVhdrEntry = entry;
        }

        // Available HDR mode
        IMetadata::IEntry availHdrEntry(MTK_HDR_FEATURE_AVAILABLE_HDR_MODES);
        MBOOL isHDRSensor = (availVhdrEntry.count() > 1);

        availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_OFF, Type2Type< MINT32 >());
#if (MTKCAM_HAVE_HDR_SUPPORT == 1)
        availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_ON, Type2Type< MINT32 >());

        // is VIDEO_ON availale
#if (MTKCAM_HAVE_VHDR_SUPPORT == 1)
        if(isHDRSensor){
            availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_VIDEO_ON, Type2Type< MINT32 >());
        }
#endif // (MTKCAM_HAVE_VHDR_SUPPORTED endif)

        // is AUTO_ON & AUTO_VIDEO_ON available
#if (MTKCAM_HDR_DETECTION_MODE == 1) // support HDR sensor
        if(isHDRSensor){
            availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_AUTO, Type2Type< MINT32 >());
            availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_VIDEO_AUTO, Type2Type< MINT32 >());
        }
#elif(MTKCAM_HDR_DETECTION_MODE == 2) // support generic sensor
        if(!isHDRSensor){
            availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_AUTO, Type2Type< MINT32 >());
        }
#elif(MTKCAM_HDR_DETECTION_MODE == 3) // support all sensor
        availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_AUTO, Type2Type< MINT32 >());
        if(isHDRSensor){
            availHdrEntry.push_back(MTK_HDR_FEATURE_HDR_MODE_VIDEO_AUTO, Type2Type< MINT32 >());
        }
#endif // (MTKCAM_HDR_DETECTION_MODE endif)

#endif// MTKCAM_HAVE_HDR_SUPPORT endif

        rMetadata.update(availHdrEntry.tag(), availHdrEntry);
    }

}


