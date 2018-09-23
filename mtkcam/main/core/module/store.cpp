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

#define LOG_TAG "mtkcam-module"
//
#include <string.h>
#include "local.h"
//
/******************************************************************************
 *
 ******************************************************************************/
static
mtkcam_module_info*
get_module_info(unsigned int moduleId)
{
    struct ModuleStore
    {
        mtkcam_module_info  table[MTKCAM_MODULE_ID_COUNT];
        ModuleStore()
        {
            ::memset(&table, 0, sizeof(table));
            MY_LOGI("ctor");
        }
    };
    static ModuleStore store;
    return &store.table[moduleId];
}


/******************************************************************************
 *
 ******************************************************************************/
static
void
dump_module(
    mtkcam_module_info const* info,
    char const* prefix_msg = ""
)
{
    MY_LOGI(
        "[%s] module_id:%u module_factory:%p register_name:%s",
        prefix_msg,
        info->module_id,
        info->module_factory,
        (info->register_name ? info->register_name : "unknown")
    );
}


/******************************************************************************
 *
 ******************************************************************************/
void register_mtkcam_module(mtkcam_module_info const* info)
{
    if  ( MTKCAM_MODULE_ID_COUNT <= info->module_id ) {
        MY_LOGE("Bad module_id(%u) >= MTKCAM_MODULE_ID_COUNT(%u)", info->module_id, MTKCAM_MODULE_ID_COUNT);
        dump_module(info);
        return;
    }

    if  ( ! info->module_factory ) {
        MY_LOGW("Bad module_factory==NULL");
        dump_module(info);
        return;
    }

    if  ( get_module_info(info->module_id)->module_factory ) {
        MY_LOGE("Has registered before");
        dump_module(get_module_info(info->module_id), "old");
        dump_module(info, "new");
        return;
    }

    dump_module(info, "registered");
    *get_module_info(info->module_id) = *info;
}


/******************************************************************************
 *
 ******************************************************************************/
extern "C"
void* MtkCam_getModuleFactory(unsigned int moduleId)
{
    if  ( MTKCAM_MODULE_ID_COUNT <= moduleId ) {
        MY_LOGE("Bad moduleId(%u) >= MTKCAM_MODULE_ID_COUNT(%u)", moduleId, MTKCAM_MODULE_ID_COUNT);
        return NULL;
    }

    mtkcam_module_info const* info = get_module_info(moduleId);
    if  ( ! info->module_factory ) {
        MY_LOGW("[moduleId:%u] Bad module_factory==NULL", moduleId);
        dump_module(info);
        return NULL;
    }

    return info->module_factory;
}

