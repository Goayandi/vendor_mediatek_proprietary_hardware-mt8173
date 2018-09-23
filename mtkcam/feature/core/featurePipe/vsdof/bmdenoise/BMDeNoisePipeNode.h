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

/**
 * @file BMDeNoisePipeNode.h
 * @brief This is the definition of Bayer+White de-noise pipe nodes
*/

#ifndef _MTK_CAMERA_FEATURE_PIPE_BMDeNoise_PIPE_NODE_H_
#define _MTK_CAMERA_FEATURE_PIPE_BMDeNoise_PIPE_NODE_H_

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file
#include <core/include/CamThreadNode.h>
#include <core/include/ImageBufferPool.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>

// Module header file
#include <featurePipe/core/include/Timer.h>

// Local header file
#include "BMDeNoisePipe_Common.h"


#define UNUSED(expr) do { (void)(expr); } while (0)
//
#define FUNC_START  MY_LOGD("+")
#define FUNC_END    MY_LOGD("-")
/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

typedef android::sp<EffectRequest> EffectRequestPtr;
typedef android::sp<EffectFrameInfo> FrameInfoPtr;
/*******************************************************************************
* Enum Define
********************************************************************************/
enum BMDeNoiseDataId {
    ID_INVALID,
    ROOT_ENQUE,

    RREPROCESS_TO_DENOISE_RAW,
    RREPROCESS_TO_DENOISE_WARPING,

    DENOISE_RAW_IMAGE_OUT,
    DENOISE_RESULT_OUT,

    ERROR_OCCUR_NOTIFY
};



/*******************************************************************************
* Structure Define
********************************************************************************/




/*******************************************************************************
* Class Define
********************************************************************************/
/**
 * @class BMDeNoisePipeDataHandler
 * @brief BMDeNoisePipeDataHandler
 */
class BMDeNoisePipeDataHandler
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
typedef BMDeNoiseDataId DataID;

// Constructor
// Copy constructor
// Create instance
// Destr instance

protected:
// Destructor
virtual ~BMDeNoisePipeDataHandler(){};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief onData
     * @param [in] id: dataID
     * @param [in] data: EffectRequestPtr
     * @return
     *-true indicates success, otherwise indicates fail
     */
    virtual MBOOL onData(DataID id, EffectRequestPtr &data)
    {
        UNUSED(id);
        UNUSED(data);
        return MFALSE;
    }
    /**
     * @brief onData
     * @param [in] id: dataID
     * @param [in] data: FrameInfoPtr
     * @return
     *-true indicates success, otherwise indicates fail
     */
    virtual MBOOL onData(DataID id, FrameInfoPtr &data)
    {
        UNUSED(id);
        UNUSED(data);
        return MFALSE;
    }
    /**
     * @brief onData
     * @param [in] id: dataID
     * @param [in] data: SmartImageBuffer
     * @return
     *-true indicates success, otherwise indicates fail
     */
    virtual MBOOL onData(DataID id, SmartImageBuffer &data)
    {
        UNUSED(id);
        UNUSED(data);
        return MFALSE;
    }
    /**
     * @brief onData
     * @param [in] id: dataID
     * @param [in] data: SmartImageBuffer
     * @return
     *-true indicates success, otherwise indicates fail
     */
    virtual MBOOL onData(DataID id, ImgInfoMapPtr &data)
    {
        UNUSED(id);
        UNUSED(data);
        return MFALSE;
    }
    /**
     * @brief ID2Name
     * @param [in] id: dataID
     * @return
     *-the string name of the DataID
     */
    static const char* ID2Name(DataID id);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Protected Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Private Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
};




/*******************************************************************************
* Class Define
********************************************************************************/
/**
 * @class BMDeNoisePipeNode
 * @brief BMDeNoisePipeNode
 */
class BMDeNoisePipeNode:
            public BMDeNoisePipeDataHandler,
            public CamThreadNode<BMDeNoisePipeDataHandler>
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
typedef CamGraph<BMDeNoisePipeNode> Graph_T;
typedef BMDeNoisePipeDataHandler Handler_T;
// Constructor
BMDeNoisePipeNode(
            const char *name,
            Graph_T *graph);
// Copy constructor
// Create instance
// Destr instance

protected:
// Destructor
virtual ~BMDeNoisePipeNode(){};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    /**
     * @brief enableDumpImage
     * @param [in] flag: indicate whether to dump or not
     * @return
     *-void
     */
    void enableDumpImage(MBOOL flag);

    virtual MBOOL onDump(DataID id, ImgInfoMapPtr &data, const char* fileName=NULL, const char* postfix=NULL);
    MBOOL handleDump(DataID id, ImgInfoMapPtr& data, const char* fileName=NULL, const char* postfix=NULL);
    MBOOL handleDataAndDump(DataID id, ImgInfoMapPtr& data);
    MBOOL handleData(DataID id, EffectRequestPtr pReq);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    /**
     * @brief onInit
     * @return
     *-true indicates success, otherwise indicates fail
     */
    MBOOL onInit()          override;
    /**
     * @brief onUninit
     * @return
     *-true indicates success, otherwise indicates fail
     */
    MBOOL onUninit()        override    { return MTRUE; }
    /**
     * @brief onThreadStart
     * @return
     *-true indicates success, otherwise indicates fail
     */
    MBOOL onThreadStart()   override    { return MTRUE; }
    /**
     * @brief onThreadStop
     * @return
     *-true indicates success, otherwise indicates fail
     */
    MBOOL onThreadStop()    override    { return MTRUE; }
    /**
     * @brief onThreadLoop
     * @return
     *-true indicates success, otherwise indicates fail
     */
    virtual MBOOL onThreadLoop() = 0;
    /**
     * @brief onDumpBIDToName
     * @return the name string
     */
    virtual const char* onDumpBIDToName(BMDeNoiseBufferID BID) {return "non-defined.blob";}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Protected Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:

    MBOOL                   mbDumpImageBuffer = MFALSE;
    MBOOL                   mbProfileLog = MFALSE;
    MBOOL                   mbDebugLog = MFALSE;

    MINT32                  mSensorIdx_Main1 = -1;
    MINT32                  mSensorIdx_Main2 = -1;

    MUINT                   miDumpBufSize = 0;
    MUINT                   miDumpStartIdx= 0;
    MUINT                   miTuningDump = 0;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Private Data Members
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
};

/*******************************************************************************
* Namespace end.
********************************************************************************/
};
};
};


#endif