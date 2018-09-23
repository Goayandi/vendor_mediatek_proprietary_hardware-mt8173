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

#include "IHdrProc.h"
#include "MyHdr.h"
#include "Hdr.h"

namespace NS3A {
    class CaptureParam_T;
};


/******************************************************************************
 *
 ******************************************************************************/
using namespace NSCam;

class HDRProc : public IHdrProc
{
public:     ////                    Instantiation.

    /**
     * @brief Destroy the instance
     *
     * @details
     *
     * @note
     *
     * @return
     * 
     */
    virtual MVOID                   destroyInstance();

    /**
     * @brief Init the instance
     *
     * @details
     *
     * @note
     *
     * @return
     * 
     */
    virtual MBOOL                   init();

    /**
     * @brief Uninit the instance
     *
     * @details
     *
     * @note
     *
     * @return
     * 
     */
    virtual MBOOL                   uninit();

                                    HDRProc() {}
                                    
    virtual MBOOL                   setParam(MUINT32 paramId, MUINT32 iArg1, MUINT32 iArg2 );
    virtual MBOOL                   getParam(MUINT32 paramId, MUINT32 & rArg1, MUINT32 & rArg2 );
    virtual MBOOL                   setShotParam(MUINT32 w, MUINT32 h, MUINT32 transform, MUINT32 zoomratio);
    virtual MBOOL                   setJpegParam(MUINT32 main_w, MUINT32 main_h, MUINT32 th_w, MUINT32 th_h,MUINT32 quality);
    virtual MBOOL                   prepare();
    virtual MBOOL                   addInputFrame(MINT32 frame_index, const android::sp<IImageBuffer> imgbuf);
    virtual MBOOL                   addOutputFrame(MINT32 jpg_yuv_img, android::sp<IImageBuffer> imgbuf);
    virtual MBOOL                   start();
    virtual MBOOL                   release();
    virtual MBOOL                   GetBuf(IImageBuffer** src0, IImageBuffer** small0, IImageBuffer** src1, IImageBuffer** small1 );
    virtual MBOOL                   getHDRCapInfo(MUINT32& u4FrameNum, std::vector<MUINT32>& vu4Eposuretime, std::vector<MUINT32>& vu4RealISO, std::vector<MUINT32>& vu4FlareOffset);


private:
    virtual                         ~HDRProc() {}

public:

private:

    NSCamShot::ShotParam mShotParam;
    NSCamShot::JpegParam mJpegParam;

/*
    //
    MUINT32 nrtype = queryCapNRType( getCaptureIso(), isMfbShot);
    EImageFormat ePostViewFmt = 
        static_cast<EImageFormat>(mShotParam.miPostviewDisplayFormat);
           ); 
*/
};


/******************************************************************************
 *
 ******************************************************************************/
IHdrProc*
IHdrProc::createInstance()
{
    return new HDRProc();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
HDRProc::destroyInstance()
{
    delete this;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HDRProc::init()
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    
    HDRProcInit();
    
    FUNCTION_LOG_END
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HDRProc::uninit()
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    
    HDRProcUnInit();
    
    FUNCTION_LOG_END
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::setParam(MUINT32 paramId, MUINT32 iArg1, MUINT32 iArg2)
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;

    if(paramId <= HDRProcParam_Begin || paramId >= HDRProcParam_Num){
        CAM_LOGE("[HDRProc::setParam] invalid paramId:%d \n",paramId);
        return MFALSE;
    }
    
    HDRProcSetParam(paramId,iArg1,iArg2);
    
    FUNCTION_LOG_END
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::getParam(MUINT32 paramId, MUINT32 & rArg1, MUINT32 & rArg2)
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;

    if(paramId <= HDRProcParam_Begin || paramId >= HDRProcParam_Num){
        CAM_LOGE("[HDRProc::getParam] invalid paramId:%d \n",paramId);
        return MFALSE;
    }
    
    HDRProcGetParam(paramId,rArg1,rArg2);
    
    FUNCTION_LOG_END
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::setShotParam(MUINT32 w, MUINT32 h, MUINT32 transform, MUINT32 zoomratio)
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;

    mShotParam.u4PictureWidth = w;
    mShotParam.u4PictureHeight = h;
    mShotParam.u4PictureTransform = transform;
    mShotParam.u4ZoomRatio = zoomratio; 
    
    CAM_LOGD("[HDRProc::setShotParam] wxh(%dx%d),transform:%d,zoomratio:%d\n", mShotParam.u4PictureWidth, mShotParam.u4PictureHeight, mShotParam.u4PictureTransform, mShotParam.u4ZoomRatio);
    HDRProcSetShotParam((void *)&mShotParam);
    
    FUNCTION_LOG_END
    return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::setJpegParam(MUINT32 main_w, MUINT32 main_h, MUINT32 th_w, MUINT32 th_h,MUINT32 quality)
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    
    mJpegParam.u4JpsWidth = main_w;    
    mJpegParam.u4JpsHeight = main_h; 
    mJpegParam.u4ThumbWidth = th_w;
    mJpegParam.u4ThumbHeight = th_h;  
    mJpegParam.u4Quality = quality;

    CAM_LOGD("[HDRProc::setJpegParam] main wxh(%dx%d), tb wxh(%dx%d),Quality:%d\n", mJpegParam.u4JpsWidth, mJpegParam.u4JpsHeight, mJpegParam.u4ThumbWidth, mJpegParam.u4ThumbHeight, mJpegParam.u4Quality);
    HDRProcSetJpegParam((void *)&mJpegParam);
    
    FUNCTION_LOG_END
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::prepare()
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    HDRProcPrepare();
    FUNCTION_LOG_END
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::addInputFrame(MINT32 frame_index, const android::sp<IImageBuffer> imgbuf)
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    HDRProcAddInputFrame(frame_index, imgbuf);
    FUNCTION_LOG_END
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/

MBOOL                   
HDRProc::addOutputFrame(MINT32 jpg_yuv_img, android::sp<IImageBuffer> imgbuf)
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    HDRProcAddOutFrame(jpg_yuv_img,imgbuf);
    FUNCTION_LOG_END
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/

MBOOL                   
HDRProc::start()
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    HDRProcStart();
    FUNCTION_LOG_END
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::release()
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    HDRProcRelease();
    FUNCTION_LOG_END
    return ret;

}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::GetBuf(IImageBuffer** src0, IImageBuffer** small0, IImageBuffer** src1, IImageBuffer** small1 )
{
	MBOOL ret = MTRUE;
	FUNCTION_LOG_START;
    HDRProcGetBuf( src0,  small0,  src1,  small1 );
    FUNCTION_LOG_END
    return ret;

}


/******************************************************************************
 *
 ******************************************************************************/
MBOOL                   
HDRProc::getHDRCapInfo(MUINT32& u4FrameNum, std::vector<MUINT32>& vu4Eposuretime, std::vector<MUINT32>& vu4RealISO, std::vector<MUINT32>& vu4FlareOffset){
    MBOOL ret = true;
    std::vector<CaptureParam_T>     rCap3AParam;
    
    HDRProcGetCapInfo(0,rCap3AParam,u4FrameNum);    // TODO, should use sensor id instead of 0s
    CAM_LOGD("[HDRProc::getHDRCapInfo] hdrFrameNum:%d\n", u4FrameNum);
    CAM_LOGD("[HDRProc::getHDRCapInfo] vCap3AParam.size():(%d)", rCap3AParam.size());
    for (std::vector<NS3A::CaptureParam_T>::iterator it = rCap3AParam.begin();it != rCap3AParam.end();it++)
    {
        CAM_LOGD("[HDRProc::getHDRCapInfo] it->u4Eposuretime:(%d)", it->u4Eposuretime);
        CAM_LOGD("[HDRProc::getHDRCapInfo] it->u4AfeGain:(%d)", it->u4AfeGain);
        CAM_LOGD("[HDRProc::getHDRCapInfo] it->u4IspGain:(%d)", it->u4IspGain);
        CAM_LOGD("[HDRProc::getHDRCapInfo] it->u4FlareOffset:(%d)", it->u4FlareOffset);
        CAM_LOGD("[HDRProc::getHDRCapInfo] it->u4RealISO:(%d)", it->u4RealISO);
        vu4Eposuretime.push_back(it->u4Eposuretime);
        vu4RealISO.push_back(it->u4RealISO);
        vu4FlareOffset.push_back(it->u4FlareOffset);        
    }    
    
    return ret;
}

