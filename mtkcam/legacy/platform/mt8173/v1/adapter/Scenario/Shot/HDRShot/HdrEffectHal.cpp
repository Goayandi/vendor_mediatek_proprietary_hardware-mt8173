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
#define LOG_TAG "HdrEffectHal"

#include "HdrEffectHal.h"

/******************************************************************************
 *
 ******************************************************************************/
#define FUNCTION_LOG_START      CAM_LOGD("[%s] - E.", __FUNCTION__)
#define FUNCTION_LOG_END        CAM_LOGD("[%s] - X.", __FUNCTION__)


using namespace NSCam;
using namespace android;

bool
HdrEffectHal::
allParameterConfigured() 
{
    FUNCTION_LOG_START;
    //@todo implement this    
    FUNCTION_LOG_END;
    return true;
}


status_t
HdrEffectHal::
initImpl() 
{
    FUNCTION_LOG_START;
    mu4FrameNum = 0;
    //mpListener = NULL;

    const size_t SIZE = 256;
    char parameters[SIZE] = "picture-width=4096;picture-height=2304;picture-format=jpeg;jpeg-thumbnail-width=320;jpeg-thumbnail-height=240;jpeg-thumbnail-quality=100;jpeg-quality=100;rotation=0;zoom=100";
    android::String8 str_params(parameters);
    mHdrEffectHalParam.unflatten(str_params);

    mHdrEffectHalParam.dump();
    android::String8 EFFECTHAL_KEY_PICTURE_WIDTH("picture-width");
    android::String8 EFFECTHAL_KEY_PICTURE_HEIGHT("picture-height");
    // Supported image formats for captured pictures.
    // Example value: "jpeg,rgb565". Read only.
    android::String8 EFFECTHAL_KEY_PICTURE_FORMAT("picture-format");
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_WIDTH("jpeg-thumbnail-width");
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_HEIGHT("jpeg-thumbnail-height");
    // Thumbnail quality of captured picture. The range is 1 to 100, with 100 being
    // the best.
    // Example value: "90". Read/write.
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_QUALITY("jpeg-thumbnail-quality");
    // Jpeg quality of captured picture. The range is 1 to 100, with 100 being
    // the best.
    // Example value: "90". Read/write.
    android::String8 EFFECTHAL_KEY_JPEG_QUALITY("jpeg-quality");
    android::String8 EFFECTHAL_KEY_ROTATION("rotation");
    // Current zoom value, with 100 being zoom x1
    android::String8 EFFECTHAL_KEY_ZOOM("zoom");
    // Sensor full capture width
    android::String8 EFFECTHAL_KEY_FULL_CAPTURE_WIDTH("full-capture-width");
    // Sensor full capture height
    android::String8 EFFECTHAL_KEY_FULL_CAPTURE_HEIGHT("full-capture-height");
    // Sensor type, raw is 0
    android::String8 EFFECTHAL_KEY_SENSOR_TYPE("sensor-type");
    android::String8 EFFECTHAL_KEY_SENSOR_CAPTURE_WIDTH("sensor_capture_width");   
    android::String8 EFFECTHAL_KEY_SENSOR_CAPTURE_HEIGHT("sensor_capture_height"); 

    mSupportedParameters.append(EFFECTHAL_KEY_PICTURE_WIDTH);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_PICTURE_HEIGHT);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_PICTURE_FORMAT);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_JPEG_THUMBNAIL_WIDTH);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_JPEG_THUMBNAIL_HEIGHT);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_JPEG_THUMBNAIL_QUALITY);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_JPEG_QUALITY);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_ROTATION);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_ZOOM);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_FULL_CAPTURE_WIDTH);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_FULL_CAPTURE_HEIGHT);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_SENSOR_TYPE);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_SENSOR_CAPTURE_WIDTH);
    mSupportedParameters.append(",");
    mSupportedParameters.append(EFFECTHAL_KEY_SENSOR_CAPTURE_HEIGHT);
    mSupportedParameters.append(",");
    
    CAM_LOGD("[init] mSupportedParameters:%s\n", mSupportedParameters.string());

    mpHdrProc = IHdrProc::createInstance();
    if (mpHdrProc == NULL)
    {
        printf("failed to create HdrProc instance");
        return UNKNOWN_ERROR;
    }
    mpHdrProc->init();

    FUNCTION_LOG_END;
    return OK;
}


status_t
HdrEffectHal::
uninitImpl() 
{
    FUNCTION_LOG_START;
    mpHdrProc->uninit();
    FUNCTION_LOG_END;
    return OK;
}


//non-blocking
status_t           
HdrEffectHal::
prepareImpl() 
{
    FUNCTION_LOG_START;
    //@todo implement this
    EffectResult result;
    android::String8 EFFECTHAL_KEY_PICTURE_WIDTH("picture-width");
    android::String8 EFFECTHAL_KEY_PICTURE_HEIGHT("picture-height");
    // Supported image formats for captured pictures.
    // Example value: "jpeg,rgb565". Read only.
    android::String8 EFFECTHAL_KEY_PICTURE_FORMAT("picture-format");
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_WIDTH("jpeg-thumbnail-width");
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_HEIGHT("jpeg-thumbnail-height");
    // Thumbnail quality of captured picture. The range is 1 to 100, with 100 being
    // the best.
    // Example value: "90". Read/write.
    android::String8 EFFECTHAL_KEY_JPEG_THUMBNAIL_QUALITY("jpeg-thumbnail-quality");
    // Jpeg quality of captured picture. The range is 1 to 100, with 100 being
    // the best.
    // Example value: "90". Read/write.
    android::String8 EFFECTHAL_KEY_JPEG_QUALITY("jpeg-quality");
    android::String8 EFFECTHAL_KEY_ROTATION("rotation");
    // Current zoom value, with 100 being zoom x1
    android::String8 EFFECTHAL_KEY_ZOOM("zoom");   
    android::String8 EFFECTHAL_KEY_SENSOR_CAPTURE_WIDTH("sensor_capture_width");   
    android::String8 EFFECTHAL_KEY_SENSOR_CAPTURE_HEIGHT("sensor_capture_height"); 
    
    mpHdrProc->setJpegParam(mHdrEffectHalParam.getInt (EFFECTHAL_KEY_PICTURE_WIDTH.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_PICTURE_HEIGHT.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_JPEG_THUMBNAIL_WIDTH.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_JPEG_THUMBNAIL_HEIGHT.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_JPEG_QUALITY.string()));
    
    mpHdrProc->setShotParam(mHdrEffectHalParam.getInt (EFFECTHAL_KEY_PICTURE_WIDTH.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_PICTURE_HEIGHT.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_ROTATION.string())
                            ,mHdrEffectHalParam.getInt(EFFECTHAL_KEY_ZOOM.string()));

    
    mpHdrProc->setParam(IHdrProc::HDRProcParam_Set_transform
                        ,mHdrEffectHalParam.getInt (EFFECTHAL_KEY_ROTATION.string())
                        ,0);
        
    mpHdrProc->setParam(IHdrProc::HDRProcParam_Set_sensor_size
                        ,mHdrEffectHalParam.getInt (EFFECTHAL_KEY_SENSOR_CAPTURE_WIDTH.string())
                        ,mHdrEffectHalParam.getInt (EFFECTHAL_KEY_SENSOR_CAPTURE_HEIGHT.string()));
                        
    mpHdrProc->prepare();
    
    result.set("onPrepared", 1);
    prepareDone(result, OK);
    FUNCTION_LOG_END;
    return OK;
}


status_t   
HdrEffectHal::
releaseImpl() 
{
    FUNCTION_LOG_START;
    mpHdrProc->release();
    FUNCTION_LOG_END;
    return OK;
}

status_t
HdrEffectHal::
getNameVersionImpl(EffectHalVersion &nameVersion) const
{
    FUNCTION_LOG_START;
    //@todo implement this
    nameVersion.effectName = "Hdr";
    nameVersion.major = 1;
    nameVersion.minor = 0;
    FUNCTION_LOG_END;
    return OK;
}

#if 1
status_t           
HdrEffectHal::
getCaptureRequirementImpl(EffectParameter *inputParam, Vector<EffectCaptureRequirement> &requirements) const 

{   
    FUNCTION_LOG_START;
    MUINT32 HDRFrameNum = 0;
    EffectCaptureRequirement result;
    std :: vector < MUINT32 > vu4Eposuretime;
    std :: vector < MUINT32 > vu4RealISO;
    std :: vector < MUINT32 > vu4FlareOffset;
    char picture_size_str[32];

    android::String8 EFFECTHAL_KEY_HDR_FRAME_INDEX("hdr-frame-index");  
    android::String8 EFFECTHAL_KEY_EXPOSURE_TIME("exposure-time");  
    android::String8 EFFECTHAL_KEY_ISO("iso");  
    android::String8 EFFECTHAL_KEY_FLARE_OFFSET("flare-offset");
    
    android::String8 EFFECTHAL_KEY_SURFACE_ID("surface-id");      
    android::String8 EFFECTHAL_KEY_SURFACE_FORMAT("surface-format");
    android::String8 EFFECTHAL_KEY_SURFACE_WIDTH("surface-width");  
    android::String8 EFFECTHAL_KEY_SURFACE_HEIGHT("surface-height"); 
    
    android::String8 EFFECTHAL_KEY_PICTURE_FORMAT("picture-format"); 
    android::String8 EFFECTHAL_KEY_PICTURE_SIZE("picture-size"); 


    #if 0
    // TODO, if parameter is null, return buffer count and size
    if(NULL == parameter){
        CAM_LOGE("NULL == parameter \n");
        return BAD_VALUE;
    }
    #endif
    vu4Eposuretime.clear();
    vu4RealISO.clear();
    vu4FlareOffset.clear();
    mpHdrProc->getHDRCapInfo(HDRFrameNum, vu4Eposuretime,vu4RealISO,vu4FlareOffset);

    MUINT32 uSrcMainFormat = 0;
    MUINT32 uSrcMainWidth = 0;
    MUINT32 uSrcMainHeight = 0;
    
    MUINT32 uSrcSmallFormat = 0;
    MUINT32 uSrcSmallWidth = 0;
    MUINT32 uSrcSmallHeight = 0;
    MUINT32 empty = 0;
    
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_main_format,uSrcMainFormat,empty);
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_main_size,uSrcMainWidth,uSrcMainHeight);
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_small_format,uSrcSmallFormat,empty);
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_small_size,uSrcSmallWidth,uSrcSmallHeight);

        /*
         mu4FrameNum = HDRFrameNum; // TODO, unknwon build error
        error: assignment of member 'NSCam::HdrEffectHal::mu4FrameNum' in read-only object
         mu4FrameNum = HDRFrameNum;
      */
    for(MUINT32 i=0; i< (HDRFrameNum<<1); i++) {    // one frame Num need main and small two surface
        result.set(EFFECTHAL_KEY_SURFACE_ID.string(), i);
        result.set(EFFECTHAL_KEY_EXPOSURE_TIME.string(), vu4Eposuretime[i>>1]);
        result.set(EFFECTHAL_KEY_ISO.string(), vu4RealISO[i>>1]);
        result.set(EFFECTHAL_KEY_FLARE_OFFSET.string(), vu4FlareOffset[i>>1]);
        if((i%2) == 0){
            result.set(EFFECTHAL_KEY_SURFACE_FORMAT.string(), uSrcMainFormat);
            result.set(EFFECTHAL_KEY_SURFACE_WIDTH.string(), uSrcMainWidth);
            result.set(EFFECTHAL_KEY_SURFACE_HEIGHT.string(), uSrcMainHeight);
            result.set(BasicParameters::KEY_PICTURE_FORMAT, uSrcMainFormat);
            sprintf(picture_size_str, "%dx%d", uSrcMainWidth, uSrcMainHeight);
            result.set(EFFECTHAL_KEY_PICTURE_SIZE.string(), picture_size_str);
        } else {
            result.set(EFFECTHAL_KEY_SURFACE_FORMAT.string(), uSrcSmallFormat);
            result.set(EFFECTHAL_KEY_SURFACE_WIDTH.string(), uSrcSmallWidth);
            result.set(EFFECTHAL_KEY_SURFACE_HEIGHT.string(), uSrcSmallHeight); 
            result.set(BasicParameters::KEY_PICTURE_FORMAT, uSrcSmallFormat); 
            sprintf(picture_size_str, "%dx%d", uSrcSmallWidth, uSrcSmallHeight);
            result.set(EFFECTHAL_KEY_PICTURE_SIZE.string(), picture_size_str);
        }
        result.dump();
        requirements.push_back(result);
    }
    
    FUNCTION_LOG_END;
    return OK;
}

#else

status_t           
HdrEffectHal::
getCaptureRequirementImpl(EffectCaptureRequirement  & requirement) const 
{   
    FUNCTION_LOG_START;
    MUINT32 uSrcMainFormat = 0;
    MUINT32 uSrcMainWidth = 0;
    MUINT32 uSrcMainHeight = 0;

    MUINT32 uSrcSmallFormat = 0;
    MUINT32 uSrcSmallWidth = 0;
    MUINT32 uSrcSmallHeight = 0;
    MUINT32 empty = 0;
    
    CAM_LOGE("Should not use this function");
    
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_main_format,uSrcMainFormat,empty);
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_main_size,uSrcMainWidth,uSrcMainHeight);
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_small_format,uSrcSmallFormat,empty);
    mpHdrProc->getParam(IHdrProc::HDRProcParam_Get_src_small_size,uSrcSmallWidth,uSrcSmallHeight);
    #if 1
    android::String8 EFFECTHAL_KEY_MAIN_SURFACE_FORMAT("main-surface-format");
    android::String8 EFFECTHAL_KEY_MAIN_SURFACE_WIDTH("main-surface-width");  
    android::String8 EFFECTHAL_KEY_MAIN_SURFACE_HEIGHT("main-surface-height"); 
    android::String8 EFFECTHAL_KEY_SMALL_SURFACE_FORMAT("small-surface-format");
    android::String8 EFFECTHAL_KEY_SMALL_SURFACE_WIDTH("small-surface-width");  
    android::String8 EFFECTHAL_KEY_SMALL_SURFACE_HEIGHT("small-surface-height"); 
    requirement.set(EFFECTHAL_KEY_MAIN_SURFACE_FORMAT.string(), uSrcMainFormat);
    requirement.set(EFFECTHAL_KEY_MAIN_SURFACE_WIDTH.string(), uSrcMainWidth);
    requirement.set(EFFECTHAL_KEY_MAIN_SURFACE_HEIGHT.string(), uSrcMainHeight);
    requirement.set(EFFECTHAL_KEY_SMALL_SURFACE_FORMAT.string(), uSrcSmallFormat);
    requirement.set(EFFECTHAL_KEY_SMALL_SURFACE_WIDTH.string(), uSrcSmallWidth);
    requirement.set(EFFECTHAL_KEY_SMALL_SURFACE_HEIGHT.string(), uSrcSmallHeight);
    #else
    android::String8 EFFECTHAL_KEY_SURFACE_ID("surface-id");  
    android::String8 EFFECTHAL_KEY_SURFACE_WIDTH("surface-width");  
    android::String8 EFFECTHAL_KEY_SURFACE_HEIGHT("surface-height"); 
    android::String8 EFFECTHAL_KEY_SURFACE_FORMAT("surface-format");
    requirement.set(EFFECTHAL_KEY_SURFACE_ID.string(), 0);
    requirement.set(EFFECTHAL_KEY_SURFACE_FORMAT.string(), uSrcMainFormat);
    requirement.set(EFFECTHAL_KEY_SURFACE_WIDTH.string(), uSrcMainWidth);
    requirement.set(EFFECTHAL_KEY_SURFACE_HEIGHT.string(), uSrcMainHeight);
    requirement.set(EFFECTHAL_KEY_SURFACE_ID.string(), 1);
    requirement.set(EFFECTHAL_KEY_SURFACE_FORMAT.string(), uSrcSmallFormat);
    requirement.set(EFFECTHAL_KEY_SURFACE_WIDTH.string(), uSrcSmallWidth);
    requirement.set(EFFECTHAL_KEY_SURFACE_HEIGHT.string(), uSrcSmallHeight);
    #endif
    requirement.dump();


    FUNCTION_LOG_END;
    return OK;
}
#endif

MBOOL 
HdrEffectHal::
isParameterValid(const char *param)
{
    FUNCTION_LOG_START;
    bool ret = MFALSE;
    char *pos;
    char supported[255];

    if (NULL == param) {
        CAM_LOGE("Invalid parameter string");
        goto exit;
    }

    strncpy(supported, mSupportedParameters.string(), mSupportedParameters.length());

    pos = strtok(supported, ",");
    while (pos != NULL) {
        if (!strcmp(pos, param)) {
            ret = MTRUE;
            break;
        }
        pos = strtok(NULL, ",");
    }

exit:
    ;

    FUNCTION_LOG_END;
    return ret;
}


status_t           
HdrEffectHal::
setParameterImpl(android::String8 &key, android::String8 &object) 
{
    FUNCTION_LOG_START;
        
    if(!isParameterValid(key.string())){
        CAM_LOGE("[setParameter] unsupported parameter %s\n", key.string());
        return BAD_VALUE;
    } else {
        CAM_LOGD("[setParameter] valid parameter %s\n", key.string());
        mHdrEffectHalParam.set(key.string(),object.string());
    }

    FUNCTION_LOG_END;
    return OK;
}

status_t           
HdrEffectHal::
setParametersImpl(android::sp<EffectParameter> parameter)
{
    FUNCTION_LOG_START;
    //@todo implement this
    FUNCTION_LOG_END;
    return OK;
}


status_t           
HdrEffectHal::
startImpl(uint64_t *uid) 
{
    FUNCTION_LOG_START;
    EffectResult result;
    EffectParameter parameter;
    mpHdrProc->start();
    result.set("onCompleted", 1);
    startDone(result, parameter, OK);   //@todo call this after completed
    FUNCTION_LOG_END;
    return OK;
}


status_t
HdrEffectHal::
abortImpl(EffectResult &result, EffectParameter const *parameter)
{
    FUNCTION_LOG_START;
    status_t ret = OK;

    
    //@todo implement this
    result.set("onAborted", 1);

    FUNCTION_LOG_END;
    return ret;
}


//non-blocking
status_t           
HdrEffectHal::
addInputFrameImpl(const sp<IImageBuffer> frame, const sp<EffectParameter> parameter)
{
    FUNCTION_LOG_START;

    //@todo implement this
    //@todo use a map to maintain EffectParameter/IImageBuffer
    //@todo check the live cycle of parameter

    // EFFECTHAL_KEY_HDR_FRAME_INDEX: 0 , 1 , 2 by sequence

    android::String8 EFFECTHAL_KEY_SURFACE_ID("surface-id");  

    if(parameter!=NULL){
        //parameter->dump();
        /*
                 mu4FrameNum = HDRFrameNum; // TODO, unknwon build error
                error: assignment of member 'NSCam::HdrEffectHal::mu4FrameNum' in read-only object
                 mu4FrameNum = HDRFrameNum;
            */
        if(parameter->getInt(EFFECTHAL_KEY_SURFACE_ID.string()) > 6 /*mu4FrameNum<<1*/){
            return BAD_VALUE;
        }
    } else {
        CAM_LOGE("[addInputFrame] parameter is NULL\n");
        return BAD_VALUE;
    }

    if(frame == NULL){
        CAM_LOGE("[addInputFrame] frame is NULL\n");
        return BAD_VALUE;
    }

    mpHdrProc->addInputFrame(parameter->getInt(EFFECTHAL_KEY_SURFACE_ID.string()),frame);       

    EffectResult result;
    result.set("addInputFrameImpl", "1");
    addInputFrameDone(result, parameter, OK);

    FUNCTION_LOG_END;
    return OK;
}


//non-blocking
status_t           
HdrEffectHal::
addOutputFrameImpl(const sp<IImageBuffer> frame, const sp<EffectParameter> parameter)
{
    FUNCTION_LOG_START;
    status_t ret = OK;
    
    //@todo implement this

    // jpg_yuv_img: 0 is jpg , 1 is yuv, 2 is both
    mpHdrProc->addOutputFrame(0,frame);
    EffectResult result;
    result.set("addOutputFrameImpl", "1");
    // TODO : if David add null point check for parameter, enable addOutputFrameDone
    //addOutputFrameDone(result, parameter, OK);
    FUNCTION_LOG_END;
    return ret;
}

