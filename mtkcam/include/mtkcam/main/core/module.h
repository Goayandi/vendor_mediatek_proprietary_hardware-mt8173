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

#ifndef _MTK_HARDWARE_MTKCAM_INCLUDE_MTKCAM_MAIN_CORE_MODULE_H_
#define _MTK_HARDWARE_MTKCAM_INCLUDE_MTKCAM_MAIN_CORE_MODULE_H_
//
/******************************************************************************
 *
 ******************************************************************************/
#include <sys/cdefs.h>

__BEGIN_DECLS

/******************************************************************************
 *
 ******************************************************************************/
enum
{
    /*****
     * drv
     */
    MTKCAM_MODULE_ID_DRV_HAL_SensorList,    /*!< include/mtkcam/drv/IHalSensor.h */
    MTKCAM_MODULE_ID_DRV_HW_SYNC_DRV,       /*!< include/mtkcam/drv/IHwSyncDrv.h */

    /*****
     * aaa
     */
    MTKCAM_MODULE_ID_AAA_HAL_3A,            /*!< include/mtkcam/aaa/IHal3A.h */
    MTKCAM_MODULE_ID_AAA_HAL_FLASH,         /*!< include/mtkcam/aaa/IHalFlash.h */
    MTKCAM_MODULE_ID_AAA_ISP_MGR_IF,        /*!< include/mtkcam/aaa/IspMgrIf.h */
    MTKCAM_MODULE_ID_AAA_SYNC_3A_MGR,       /*!< include/mtkcam/aaa/ISync3A.h */
    MTKCAM_MODULE_ID_AAA_SW_NR,             /*!< include/mtkcam/aaa/ICaptureNR.h */
    MTKCAM_MODULE_ID_AAA_DNG_INFO,          /*!< include/mtkcam/aaa/IDngInfo.h */
    MTKCAM_MODULE_ID_AAA_NVBUF_UTIL,        /*!< include/mtkcam/aaa/INvBufUtil.h */
    MTKCAM_MODULE_ID_AAA_LSC_TABLE,         /*!< include/mtkcam/aaa/ILscTable.h */
    MTKCAM_MODULE_ID_AAA_LCS_HAL,           /*!< include/mtkcam/aaa/lcs/lcs_hal.h */

    /*****
     * feature
     */
    MTKCAM_MODULE_ID_HDR_PROC2,             /*!< include/mtkcam/feature/hdr/IHDRProc2.h */
    MTKCAM_MODULE_ID_HDR_PLATFORM,          /*!< include/mtkcam/feature/hdr/Platform.h */

    //--------------------------------------------------------------------------
    MTKCAM_MODULE_ID_COUNT
};


/*****************************************************************************
 * @brief Get the mtkcam module factory.
 *
 * @details Given a mtkcam module ID, return its corresponding module factory.
 * The real type of module factory depends on the given module ID, and users
 * must cast the output to the real type of module factory.
 *
 * @note
 *
 * @param[in] moduleId: mtkcam module ID.
 *
 * @return a module factory corresponding to the given module ID.
 *
 ******************************************************************************/
void*   getMtkCamModuleFactory(unsigned int moduleId);


#define MAKE_MTKCAM_MODULE(_module_id_, _module_factory_type_, ...) \
    (( (_module_factory_type_) getMtkCamModuleFactory(_module_id_) )(__VA_ARGS__))



/******************************************************************************
 *
 ******************************************************************************/
/**
 * mtkcam module versioning control
 */

/**
 * The most significant bits (bit 24 ~ 31)
 * store the information of major version number.
 *
 * The least significant bits (bit 16 ~ 23)
 * store the information of minor version number.
 *
 * bit 0 ~ 15 are reserved for future use
 */
#define MTKCAM_MAKE_API_VERSION(major,minor) \
    ((((major) & 0xff) << 24) | (((minor) & 0xff) << 16))

#define MTKCAM_GET_MAJOR_API_VERSION(version) \
    (((version) >> 24) & 0xff)

#define MTKCAM_GET_MINOR_API_VERSION(version) \
    (((version) >> 16) & 0xff)



/******************************************************************************
 *
 ******************************************************************************/
__END_DECLS
#endif  //_MTK_HARDWARE_MTKCAM_INCLUDE_MTKCAM_MAIN_CORE_MODULE_H_

