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
 * MediaTek Inc. (C) 2016. All rights reserved.
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

#define LOG_TAG "MtkCam/DebugExifUtils"

#include <mtkcam/utils/exif/DebugExifUtils.h>

#include <vector>

#include <mtkcam/utils/metadata/IMetadata.h>

#include <debug_exif/dbg_id_param.h>
#include <debug_exif/cam/dbg_cam_param.h>

#include <mtkcam/utils/std/Log.h>

using namespace NSCam;

// ---------------------------------------------------------------------------

template <typename T>
static inline MVOID updateEntry(
        IMetadata* metadata, const MUINT32 tag, const T& val)
{
    if (metadata == NULL)
    {
        CAM_LOGW("pMetadata is NULL");
        return;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    metadata->update(tag, entry);
}

static bool setDebugExifMF(
        const MUINT32 tagKey, const MUINT32 tagData,
        const std::map<MUINT32, MUINT32>& debugInfoList,
        IMetadata* exifMetadata)
{
    // allocate memory of debug information
    IMetadata::Memory debugInfoSet;
    debugInfoSet.resize(sizeof(DEBUG_MF_INFO_S));
    std::fill(debugInfoSet.begin(), debugInfoSet.end(), 0);

    // add debug information
    {
        DEBUG_MF_INFO_S *debugInfo =
            reinterpret_cast<DEBUG_MF_INFO_S *>(debugInfoSet.editArray());
        debugInfo->Tag[0].u4FieldID    = (0x1000000 | MF_TAG_VERSION);
        debugInfo->Tag[0].u4FieldValue = MF_DEBUG_TAG_VERSION;

        for (const auto& item : debugInfoList)
        {
            const MUINT32 index = item.first;
            debugInfo->Tag[index].u4FieldID    = (0x1000000 | index);
            debugInfo->Tag[index].u4FieldValue = item.second;
        }
    }

    // update debug exif metadata
    updateEntry<MINT32>(exifMetadata, tagKey, DEBUG_CAM_MF_MID);
    updateEntry<IMetadata::Memory>(exifMetadata, tagData, debugInfoSet);

    return true;
}

// ---------------------------------------------------------------------------

IMetadata* DebugExifUtils::setDebugExif(
        const DebugExifType type,
        const MUINT32 tagKey, const MUINT32 tagData,
        const std::map<MUINT32, MUINT32>& debugInfoList,
        IMetadata* exifMetadata)
{
    if (exifMetadata == NULL)
    {
        CAM_LOGW("invalid metadata(%p)", exifMetadata);
        return nullptr;
    }

    bool ret = [=, &type, &debugInfoList](IMetadata* metadata) -> bool {
        switch (type)
        {
            case DebugExifType::DEBUG_EXIF_MF:
                return setDebugExifMF(tagKey, tagData, debugInfoList, metadata);
            default:
                CAM_LOGW("invalid debug exif type, do nothing");
                return false;
        }
    } (exifMetadata);

    return (ret == true) ? exifMetadata : nullptr;

    return nullptr;
}
