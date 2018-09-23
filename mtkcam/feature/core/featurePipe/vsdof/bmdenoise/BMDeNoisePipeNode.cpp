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


// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/feature/stereo/hal/stereo_common.h>
#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>

// Module header file


// Local header file
#include "BMDeNoisePipeNode.h"

using namespace NSCam::NSCamFeature::NSFeaturePipe;
/*******************************************************************************
* Global Define
********************************************************************************/


/*******************************************************************************
* External Function
********************************************************************************/



/*******************************************************************************
* Enum Define
********************************************************************************/




/*******************************************************************************
* Structure Define
********************************************************************************/






//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
BMDeNoisePipeNode::
BMDeNoisePipeNode(const char *name, Graph_T *graph)
  : CamThreadNode(name)
{
    miDumpBufSize = ::property_get_int32("bmdenoise.pipe.dump.size", 0);
    miDumpStartIdx = ::property_get_int32("bmdenoise.pipe.dump.start", 0);
    miTuningDump = ::property_get_int32("bmdenoise.tuning.dump", 0);
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDeNoisePipeDataHandler Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
const char*
BMDeNoisePipeDataHandler::
ID2Name(DataID id)
{
    return "UNKNOWN";
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BMDeNoisePipeNode Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void
BMDeNoisePipeNode::
enableDumpImage(MBOOL flag)
{
    mbDumpImageBuffer = flag;
}
MBOOL
BMDeNoisePipeNode::
onInit()
{
    mbDebugLog = StereoSettingProvider::isLogEnabled(PERPERTY_BMDENOISE_NODE_LOG);
    mbDumpImageBuffer = getPropValue();
    mbProfileLog = StereoSettingProvider::isProfileLogEnabled();

    if(StereoSettingProvider::getStereoSensorIndex(mSensorIdx_Main1, mSensorIdx_Main2) != MTRUE){
    	MY_LOGE("Can't get sensor id from StereoSettingProvider!");
    	return MFALSE;
    }

    return MTRUE;
}

MBOOL
BMDeNoisePipeNode::
onDump(DataID id, ImgInfoMapPtr &data, const char* fileName, const char* postfix)
{
    VSDOF_LOGD("onDump: dataID:%d ", id);
    sp<EffectRequest> pEffReq = data->getRequestPtr();
    MUINT iReqIdx = pEffReq->getRequestNo();

    // make path for alg
    char filepathAlg[1024];
    snprintf(filepathAlg, 1024, "/sdcard/bmdenoise/CModelData");
    VSDOF_LOGD("makePath: %s", filepathAlg);
    makePath(filepathAlg, 0660);

    // make path for featurePipe
    char filepath[1024];
    snprintf(filepath, 1024, "/sdcard/bmdenoise/%d/%s", iReqIdx, getName());

    // make path
    VSDOF_LOGD("makePath: %s", filepath);
    makePath(filepath, 0660);

    // check dump index
    if(iReqIdx < miDumpStartIdx || iReqIdx >= miDumpStartIdx + miDumpBufSize)
        return MTRUE;

    const SmartImageBufferSet ImgBufSet = data->getImageBufferSet();
    VSDOF_LOGD("onDump: ImgBufSet.size:%d ", ImgBufSet.size());

    char writepath[1024];
    for(size_t i=0;i<ImgBufSet.size();++i)
    {
        const BMDeNoiseBufferID& BID = ImgBufSet.keyAt(i);
        const SmartImageBuffer& smBuf = ImgBufSet.valueAt(i);
        const char* writeFileName = (fileName != NULL) ? fileName : onDumpBIDToName(BID);
        const char* postfixName = (postfix != NULL) ? postfix : "";

        VSDOF_LOGD("i=%d BID: %d writeFileName:%s", i, BID, writeFileName);

        if(strchr(postfixName, '.') != NULL)
            snprintf(writepath, 1024, "%s/%s_%dx%d%s", filepath, writeFileName,
                smBuf->mImageBuffer->getImgSize().w, smBuf->mImageBuffer->getImgSize().h, postfixName);
        else
            snprintf(writepath, 1024, "%s/%s_%dx%d%s.raw", filepath, writeFileName,
                smBuf->mImageBuffer->getImgSize().w, smBuf->mImageBuffer->getImgSize().h, postfixName);

        VSDOF_LOGD("saveToFile: %s", writepath);
        smBuf->mImageBuffer->saveToFile(writepath);
    }

    VSDOF_LOGD("onDump: ImgBufSet.size:%d final", ImgBufSet.size());

    const GraphicImageBufferSet graImgBufSet = data->getGraphicBufferSet();
    for(size_t i=0;i<graImgBufSet.size();++i)
    {
        const BMDeNoiseBufferID& BID = graImgBufSet.keyAt(i);
        const SmartGraphicBuffer& smGraBuf = graImgBufSet.valueAt(i);
        const char* writeFileName = (fileName != NULL) ? fileName : onDumpBIDToName(BID);
        const char* postfixName = (postfix != NULL) ? postfix : "";

        if(strchr(postfixName, '.') != NULL)
            snprintf(writepath, 1024, "%s/%s_%dx%d-graphic-%s", filepath, writeFileName,
                smGraBuf->mImageBuffer->getImgSize().w, smGraBuf->mImageBuffer->getImgSize().h, postfixName);
        else
            snprintf(writepath, 1024, "%s/%s_%dx%d%s-graphic.raw", filepath, writeFileName,
                smGraBuf->mImageBuffer->getImgSize().w, smGraBuf->mImageBuffer->getImgSize().h, postfixName);
        VSDOF_LOGD("saveToFile: %s", writepath);
        smGraBuf->mImageBuffer->saveToFile(writepath);
    }
    return MTRUE;
}

MBOOL
BMDeNoisePipeNode::
handleDump(DataID id, ImgInfoMapPtr& data, const char* fileName, const char* postfix)
{
    return this->onDump(id, data, fileName, postfix);
}

MBOOL
BMDeNoisePipeNode::
handleDataAndDump(DataID id, ImgInfoMapPtr& data)
{
    MBOOL bRet = this->onDump(id, data);
    bRet = bRet & this->handleData(id, data->getRequestPtr());
    return bRet;
}

MBOOL
BMDeNoisePipeNode::
handleData(DataID id, EffectRequestPtr pReq)
{
    VSDOF_LOGD("handleData: %d", pReq->getRequestNo());
    return CamThreadNode<BMDeNoisePipeDataHandler>::handleData(id, pReq);
}