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
* @file IspMgrIf.h
* @brief Use this interface to control some isp_mgr and do tuning adjust.
*/

#ifndef __IISP_MGR_IF_H__
#define __IISP_MGR_IF_H__

#include <utils/List.h>
#include <mtkcam/def/common.h>
#include <mtkcam/main/core/module.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/aaa/aaa_hal_common.h>
#include <mtkcam/drv/IHalSensor.h>

namespace NS3Av3
{
using namespace android;
using namespace NSCam;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
struct RMG_Config_Param
{
    MBOOL       iEnable;
    MUINT32     leFirst;
    MBOOL       zEnable;
    MUINT32     zPattern;
};

struct RMM_Config_Param
{
};

struct LCSO_Param
{
    MSize   size;
    MINT    format;
    size_t  stride;
    MUINT32 bitDepth;

    LCSO_Param()
        : size(MSize(0,0))
        , format(0)
        , stride(0)
        , bitDepth(0)
        {};
};

struct NR3D_Config_Param
{
    MBOOL       enable;
    MRect       onRegion; // region modified by GMV
    MSize       fullImgSize; // image full size for demo mode calculation

    NR3D_Config_Param()
    : enable(MFALSE)
    , onRegion(MRect(0,0))
    , fullImgSize(MSize(0,0))
    {};
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**
 * @brief Interface of IspMgr Interface
 */
class IspMgrIf {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  //          Ctor/Dtor.
                        IspMgrIf();
                virtual ~IspMgrIf();

private: // disable copy constructor and copy assignment operator
                        IspMgrIf(const IspMgrIf&);
    IspMgrIf&           operator=(const IspMgrIf&);

private:

    IHalSensorList *const m_pHalSensorList;
    ESensorDev_T m_eSensorDev;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interfaces.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    //
    /**
     * @brief get instance of IspMgrIf
     */
    static IspMgrIf&    getInstance();
    /**
    * @brief enable/disable PDC
    */
    virtual MVOID       setPDCEnable(ESensorDev_T eSensorDev, MBOOL enable);
    /**
    * @brief enable/disable PDCout
    */
    virtual MVOID       setPDCoutEnable(ESensorDev_T eSensorDev, MBOOL enable);
    /**
     * @brief enable/disable RMG
     */
    virtual MVOID       setRMGEnable(ESensorDev_T eSensorDev, MBOOL enable);
    /**
     * @brief enable/disable RMM
     */
    virtual MVOID       setRMMEnable(ESensorDev_T eSensorDev, MBOOL enable);
    /**
     * @brief enable/disable RMG debug
     */
    virtual MVOID       setRMGDebug(ESensorDev_T eSensorDev, MINT32 debugDump);
    /**
     * @brief enable/disable RMM debug
     */
    virtual MVOID       setRMMDebug(ESensorDev_T eSensorDev, MINT32 debugDump);
    /**
     * @brief config RMG,RMG2 initialize Parameter
     */
    virtual MVOID       configRMG_RMG2(ESensorDev_T eSensorDev, RMG_Config_Param& param);
    /**
     * @brief config RMM.RMM2 initialize Parameter
     */
    virtual MVOID       configRMM_RMM2(ESensorDev_T eSensorDev, RMM_Config_Param& param);

    /**
     * @brief query LCSO parameters, such as size, format, stride...
     */
    virtual MVOID       queryLCSOParams(LCSO_Param& param);

#if 0
    virtual MBOOL       queryLCSTopControl(MUINT i4SensorOpenIndex);
#endif

     /**
     * @brief set NR3D parameter and modify data in pTuning buffer
     */
    virtual MVOID       postProcessNR3D(ESensorDev_T eSensorDev, NR3D_Config_Param& param,
                                                void* pTuning);
};

}; // namespace NS3Av3


/**
 * @brief The definition of the maker of IspMgrIf instance.
 */
typedef NS3Av3::IspMgrIf& (*IspMgrIf_FACTORY_T)();
#define MAKE_IspMgrIf(...) \
    MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_ISP_MGR_IF, IspMgrIf_FACTORY_T, __VA_ARGS__)


#endif
