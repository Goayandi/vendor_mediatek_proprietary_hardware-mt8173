/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/********************************************************************************************
 *     LEGAL DISCLAIMER
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION,
TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/

#include <stdio.h>
#include <iostream>
#include "gtest/gtest.h"

#include "AppAutorama.h"
#include "autorama_hal_base.h"

#define TEST_IMG_WIDTH        2560
#define TEST_IMG_HEIGHT       1440
#define TEST_IMG_NUM            3
#define TEST_IMG_EV             80
#define TEST_IMG_SIZE           (TEST_IMG_WIDTH*TEST_IMG_HEIGHT*3)/2

unsigned char test_img_00_yuv[TEST_IMG_SIZE];
unsigned int test_img_00_yuv_len=TEST_IMG_SIZE;
unsigned char test_img_01_yuv[TEST_IMG_SIZE];
unsigned int test_img_01_yuv_len=TEST_IMG_SIZE;
unsigned char test_img_02_yuv[TEST_IMG_SIZE];
unsigned int test_img_02_yuv_len=TEST_IMG_SIZE;

void *test_img[]=
{
    (void*)test_img_00_yuv,
    (void*)test_img_01_yuv,
    (void*)test_img_02_yuv,
};


TEST(AutoRamaUT, TestAlgorithmObject)
{
    MRESULT  ret_code;

    MTKAutorama*            obj;
    MTKAutoramaEnvInfo      info;
    MTKAutoramaGetEnvInfo   get_info;
    MTKAutoramaResultInfo   result_info;
    MTKAutoramaProcInfo     proc_info;
    MUINT8*                 work_buffer;
    MUINT32                 work_buffer_size;

    // Create algorithm object
    printf("call MTKAutorama::createInstance\n");
    obj = MTKAutorama::createInstance(DRV_AUTORAMA_OBJ_SW);
    EXPECT_TRUE(obj != NULL);

    // Create working buffer
    get_info.ImgWidth          = TEST_IMG_WIDTH;
    get_info.ImgHeight         = TEST_IMG_HEIGHT;
    get_info.MaxSnapshotNumber = TEST_IMG_NUM;
    ret_code = obj->AutoramaFeatureCtrl(MTKAUTORAMA_FEATURE_GET_ENV_INFO, 0, &get_info);
    EXPECT_TRUE(ret_code==S_AUTORAMA_OK);

    work_buffer_size = get_info.WorkingBuffSize;
    work_buffer = (MUINT8*)malloc(work_buffer_size);

    printf("call MTKAUTORAMA_FEATURE_GET_ENV_INFO,working buffer size = %d\n",work_buffer_size);

    EXPECT_TRUE(work_buffer != NULL);

    // Set Pano driver environment parameters
    info.SrcImgWidth = TEST_IMG_WIDTH;
    info.SrcImgHeight = TEST_IMG_HEIGHT;
    info.MaxPanoImgWidth = AUTORAMA_MAX_WIDTH;
    info.WorkingBufAddr = work_buffer;
    info.WorkingBufSize = work_buffer_size;
    info.MaxSnapshotNumber = TEST_IMG_NUM;
    info.FixAE = 0;
    info.FocalLength = 0;
    info.GPUWarp = 0;
    info.SrcImgFormat = MTKAUTORAMA_IMAGE_NV21;
    ret_code = obj->AutoramaInit(&info, 0);
    printf("call AutoramaInit\n");
    EXPECT_TRUE(ret_code >=0);

    // add image
    for(int i=0;i<TEST_IMG_NUM;i++)
    {
        printf("add image %d\n",i);
        proc_info.AutoramaCtrlEnum = MTKAUTORAMA_CTRL_ADD_IMAGE;
        proc_info.SrcImgAddr = (void*) test_img[i];
        proc_info.EV = TEST_IMG_EV;
        proc_info.StitchDirection=MTKAUTORAMA_DIR_UP;
        ret_code = obj->AutoramaFeatureCtrl(MTKAUTORAMA_FEATURE_SET_PROC_INFO, &proc_info, 0);
        EXPECT_TRUE(ret_code == S_AUTORAMA_OK);

        ret_code = obj->AutoramaMain();
        EXPECT_TRUE(ret_code == S_AUTORAMA_OK);
    }


    // do stitch
    printf("stitch image\n");
    proc_info.AutoramaCtrlEnum = MTKAUTORAMA_CTRL_STITCH;
    ret_code = obj->AutoramaFeatureCtrl(MTKAUTORAMA_FEATURE_SET_PROC_INFO, &proc_info, 0);
    EXPECT_TRUE(ret_code == S_AUTORAMA_OK);

    ret_code = obj->AutoramaMain();
    EXPECT_TRUE(ret_code == S_AUTORAMA_OK);


    // get result
    ret_code = obj->AutoramaFeatureCtrl(MTKAUTORAMA_FEATURE_GET_RESULT, 0, &result_info);
    EXPECT_TRUE(ret_code == S_AUTORAMA_OK);

    printf("ImgWidth %d ImgHeight %d ImgBufferAddr 0x%llx\n",
            result_info.ImgWidth, result_info.ImgHeight,result_info.ImgBufferAddr);
}
