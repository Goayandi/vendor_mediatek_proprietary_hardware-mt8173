/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2012
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

/*******************************************************************************
 * Filename:
 * ---------
 *  mtk_nfc_log.c
 *
 * Project:
 * --------
 *
 * Description:
 * ------------
 *
 * Author:
 * -------
 *  Leo kuo, ext 32813, leo.kuo@mediatek.com, 2015-11-20
 *
 *******************************************************************************/
/*****************************************************************************
 * Include
 *****************************************************************************/
#include "mtk_nfc_sys_type.h"
#include "mtk_nfc_sys.h"

#include "mtk_nfc_log.h"

#include <stdio.h>
#include <stdlib.h>


/*****************************************************************************
 * Define
 *****************************************************************************/


/*****************************************************************************
 * Data Structure
 *****************************************************************************/

/*****************************************************************************
 * Extern Area
 *****************************************************************************/


/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
char *gStrDbgMod[DBG_MOD_END] = {"NON","SYS","HAL","NCI","LLC","BIN","DAL","LIB","P2P","BOOT","RW","AOSP","AOSP-MW","AOSP-LIB","AOSP-HAL"};
char *gStrDbgTyp[DBG_TYP_END] = {"NON","ERR","WRN","TRACE","INF"};

uint32_t gu4EnDbgModule = 0x00000FFF;
#if 0
uint8_t gu1EnDbgLevel[32] = { 3,3,3,3,3,3,3,3,
                              3,3,3,0,0,0,0,0,
                              0,0,0,0,0,0,0,0,
                              0,0,0,0,0,0,0,0,
                            };
#endif

UINT8 gDgbLevelPorting=4;

/*****************************************************************************
 * Function
 *****************************************************************************/
UINT8 mtk_nfc_read_log_cfg_porting()
{
    int result = 0; //not support
    FILE * fp;
    CHAR buf[128];
    CHAR name[30];
    CHAR value[30];
    CHAR *ptr;

    fp = fopen("/system/etc/nfc.cfg","r");
    if (fp == NULL) {
        mtk_nfc_sys_dbg_string("Can't open nfcse.cfg file. \r\n");
        return result;
    }

    while(fgets(buf, 128, (void *)fp) != NULL) {
        //NFCD("%s", buf);

        // bypass comment line
        if (buf[0] == '#') {
            continue;
        }

        // parsing - name
        ptr = strtok(buf, " :#\t\xd\xa");
        if (ptr == NULL) {
            continue;
        }
        strncpy(name, ptr, 30);

        // parsing - value
        ptr = strtok(NULL, " #\t\xd\xa");
        if (ptr == NULL) {
            continue;
        }
        strncpy(value, ptr, 30);
        // parse  value
        if(strncmp(name, "NFC_TRACE_LEVEL", 30) == 0) {
            result = atoi(value);
            break;
        }
        else
        {
            result = DBG_TYP_END;
        }
    }

    fclose(fp);

    if(result == DBG_TYP_END)
    {
        mtk_nfc_sys_dbg_string("Don't find NFC_TRACE_LEVEL and set level :3 (DBG_TYP_TRACE) by default");
        result = DBG_TYP_TRACE;
    }
    return result;


}




/**
    NFC_TRACE_LEVEL_INFO            4    *messages (general)
    NFC_TRACE_LEVEL_TRACE          3    *Warning condition trace messages
    NFC_TRACE_LEVEL_WARNING     2    *Warning condition trace messages
    NFC_TRACE_LEVEL_ERROR         1    *condition trace messages
    NFC_TRACE_LEVEL_NONE           0    *no trace messages
**/

void mtk_nfc_set_log_level_porting(UINT8 level)
{
    //mtk_nfc_sys_dbg_string("mtk_nfc_set_log_level ");
    gDgbLevelPorting=level;
}

void NFC_MPRINT_LEVEL_PORTING(char* module ,mtkNfc_eDbgTyp_t eDbgTyp,char *str, ...)
{
#define LOCAL_DBG_BUF (256*4)


    // debug level check
    if ( ( eDbgTyp > gDgbLevelPorting) ||
         ( eDbgTyp == DBG_TYP_NON ) )
    {
        return;
    }


    char trace[LOCAL_DBG_BUF];
    char szBuf[LOCAL_DBG_BUF];
    va_list  args;

    va_start(args, str);
    vsnprintf(szBuf, LOCAL_DBG_BUF-strlen(module), str, args);
    va_end(args);

    sprintf(trace, "%s%s", module ,szBuf);

    mtk_nfc_sys_dbg_string(trace);

}


void NFC_PRINT_BUFFER_LEVEL_PORTING(char *msg, mtkNfc_eDbgTyp_t eDbgTyp, uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    char trace[MAX_TRACE_BUFFER];
#ifdef REDUCE_DEBUG_MSG_FOR_ANDROID
    uint16_t j, title_len = 0;
    char szBuf_tmp[8];
#endif


        // debug level check
    if ( ( eDbgTyp > gDgbLevelPorting) ||
         ( eDbgTyp == DBG_TYP_NON ) )
    {
        return;
    }
    #ifdef DEBUG_WITH_TIME_TAG
    char time_tag[32];
    MTK_TIME_T tSysTime;
    mtk_nfc_sys_time_read(&tSysTime);
    sprintf(time_tag, "[%02d:%02d:%02d.%03d] ", tSysTime.hour, tSysTime.min, tSysTime.sec, tSysTime.msec);
    mtk_nfc_sys_dbg_string(const CH * pString)(time_tag);
    #endif

    snprintf(trace, MAX_TRACE_BUFFER, "%s : ", msg);
#ifdef REDUCE_DEBUG_MSG_FOR_ANDROID
    title_len = (uint16_t)strlen(trace);
    for(i = 0; i < len; i++)
    {
        // append output byte
        sprintf(szBuf_tmp, "%02x,", buf[i]);
        strcat(trace, szBuf_tmp);

        // break line
        if (((i+1) % MAX_TRACE_BYTE_PER_LINE) == 0)
        {
            strcat(trace, "\r\n");
            mtk_nfc_sys_dbg_string(trace);
            memset(trace, 0, MAX_TRACE_BUFFER);

            // alignment
            for(j = 0; j < title_len; j++)
            {
                strcat(trace, " ");
            }
        }
    }
    strcat(trace, "\r\n");
    mtk_nfc_sys_dbg_string(trace);
#else
    mtk_nfc_sys_dbg_string(trace);
    mtk_nfc_sys_dbg_trace(buf,len);
    mtk_nfc_sys_dbg_string("\r\n");
#endif
}


