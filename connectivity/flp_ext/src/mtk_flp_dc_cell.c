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
 *  mtk_flp_dc.c
 *
 * Project:
 * --------
 *
 * Description:
 * ------------
 * Data Center Process functions
 *
 * Author:
 * -------
 *  Demarco Chou, demarco.chou@mediatek.com, 2014-03-24
 *
 *******************************************************************************/
/*****************************************************************************
 * Include
 *****************************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

//fused_location.h located in: alps\hardware\libhardware\include\hardware
#ifndef K2_PLATFORM
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#include "sys/epoll.h"
#else
#include "mtk_flp_connsys_sys.h"
#include "mtk_gps_bora_flp.h"
#endif
#include "mtk_flp_dc.h"
#include "slp_api.h"
#include <unistd.h>
//#include <android/log.h>
//#include <utils/Log.h> // For Debug

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FLP_DC_CELL"

#ifndef K2_PLATFORM
#define FLPE(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define FLPD(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define FLPW(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_WARN, LOG_TAG, __VA_ARGS__)


#else
#define FLPD(...)
#define FLPW(...)
#define FLPE(...)
#endif
#endif

//*****************************************
// Report thread function ball backs
//*****************************************
#ifdef K2_PLATFORM
void mtk_flp_dc_cell_main(MTK_FLP_MSG_T *prmsg);
#else
void mtk_flp_dc_cell_main(void);
#endif
int mtk_flp_dc_cell_process(MTK_FLP_MSG_T *prmsg);

DcReportThread    Report_thrd_CELL = (DcReportThread)mtk_flp_dc_cell_main;      //need to set to NULL for not activating this location source.

DcProcessFunction    Process_Func_CELL = (DcProcessFunction)mtk_flp_dc_cell_process;  //need to set to NULL for not activating this location source.

typedef enum
{
    MTK_MDM_GSM = 0x0,
    MTK_MDM_CDMA,
    MTK_MDM_WCDMA,
    MTK_MDM_LTE,
    MTK_MDM_TYPE_END
} MTK_MDM_SOURCE_TYPE_E;

typedef enum
{
    MTK_MDM_RAW_TO_SLP = 0x00,
    MTK_MDM_RAW_TO_QUERY,
    MTK_MDM_QUERY_TO_REACQ,
    MTK_MDM_REACQ_TO_QUERY,
    MTK_MDM_REACQ_TO_SLP,
    MTK_MDM_CONVERT_TYPE_END
}MDM_CNV_TYPE;

typedef struct
{
    UINT32 length;
} MTK_MDM_HEADER_T;

typedef struct
{
    UINT16 MDM_TYPE;
    UINT16 MCC;
    UINT16 NET;
    UINT16 AREA;
    UINT16 OTHER;
    UINT16 SIGNAL_STRENGTH;
    UINT32 CELL_ID;
    UINT32 isCamping;
} MTK_MDM_DATA_T;

typedef struct
{
    UINT32 CELL_ID;
    float lat; //in degree
    float lng; //in degree
    int alt; //meter
    int acc; //meter
    UINT16 SIGNAL_STRENGTH;
    UINT16 is_camping;
}MTK_MDM_CNN_QUERY_TABLE;

typedef struct
{
    MTK_MDM_DATA_T cell_info;
    int session;
    int index;
    sclp_result_enum result;
    UINT16 is_camping;
}MTK_MDM_AP_QUERY_TABLE;

/* Protocol with CNN DC_Cell*/
enum
{
    CMD_MODEM_CELL_LOC_QUERY = 0,
    CMD_MODEM_CELL_LOC_RESPONSE,
};

#define CELL_TIMER 15000 //15sec query one set of cell
#define MAX_EPOLL_EVENT 50
#define DB_SIZE 50
#define REACQ_SIZE 10
UINT32 cell_on = 0;
int slp_session = 0;

#ifndef K2_PLATFORM  //flp run in AP
MTK_MDM_AP_QUERY_TABLE ap_slp_query_table[DB_SIZE];
sclp_cell_reacquire_info_list mdm_reacquire_list = {0};
int query_cnt = 0;
int reacquire_cnt = 0;
int network_status = -1, network_type = -1;

extern INT32 mtk_flp_dc_offload_process(MTK_FLP_MSG_T *prmsg);

void flp_clean_query_table(int start_index) {
    if(start_index >= DB_SIZE) {
        FLPD("start_indx: %d, return\n",start_index );
        return;
    }

    if(start_index == 0) {
        memset(&ap_slp_query_table[start_index], 0 , sizeof(MTK_MDM_AP_QUERY_TABLE));
    } else if(start_index < (DB_SIZE-1)) {
        memmove(&ap_slp_query_table[start_index], &ap_slp_query_table[start_index+1],(query_cnt-start_index)*sizeof(MTK_MDM_AP_QUERY_TABLE));
    }
    if(query_cnt > 0) {
        query_cnt--;
    }
    FLPD("current query_cnt %d",query_cnt);
}

int flp_cell_table_convert(MDM_CNV_TYPE mode, sclp_cell_info *slp_info, sclp_reacquire_cell_info *query_info, MTK_MDM_DATA_T *mdm_raw, int query_idx) {
    int ret = MTK_FLP_SUCCESS;
    UNUSED(query_info);
    if(mode == MTK_MDM_RAW_TO_SLP) {
        if((mdm_raw == NULL) || (slp_info == NULL)) {
            FLPD("convert %d fail, data NA\n", mode);
            ret = MTK_FLP_ERROR;
            return ret;
        }
        if(mdm_raw->MDM_TYPE == MTK_MDM_GSM) {
            slp_info->cell_type = SCLP_CELL_GSM;
            slp_info->cell.gsm_cell.mcc = (short)mdm_raw->MCC;
            slp_info->cell.gsm_cell.mnc = (short)mdm_raw->NET;
            slp_info->cell.gsm_cell.lac= (unsigned short)mdm_raw->AREA;
            slp_info->cell.gsm_cell.cid= (unsigned short)mdm_raw->CELL_ID;
            FLPD("gsm %d,%d,%d,%d\t", slp_info->cell.gsm_cell.mcc,
                slp_info->cell.gsm_cell.mnc, slp_info->cell.gsm_cell.lac,slp_info->cell.gsm_cell.cid);
        } else if(mdm_raw->MDM_TYPE == MTK_MDM_CDMA) {
            slp_info->cell_type = SCLP_CELL_C2K;
            slp_info->cell.cdma_cell.sid = (unsigned short)mdm_raw->NET;
            slp_info->cell.cdma_cell.nid = (unsigned short)mdm_raw->AREA;
            slp_info->cell.cdma_cell.bid = (unsigned short)mdm_raw->CELL_ID;
            FLPD("cdma %d,%d,%d\t", slp_info->cell.cdma_cell.sid,slp_info->cell.cdma_cell.nid,
                slp_info->cell.cdma_cell.bid);
        } else if(mdm_raw->MDM_TYPE == MTK_MDM_WCDMA) {
            slp_info->cell_type = SCLP_CELL_WCDMA;
            slp_info->cell.wcdma_cell.mcc = (short)mdm_raw->MCC;
            slp_info->cell.wcdma_cell.mnc = (short)mdm_raw->NET;
            slp_info->cell.wcdma_cell.lac = (unsigned short)mdm_raw->AREA;
            slp_info->cell.wcdma_cell.uc = (int)mdm_raw->CELL_ID;
            FLPD("wcdma %d,%d,%d,%ld\t", slp_info->cell.wcdma_cell.mcc,
                slp_info->cell.wcdma_cell.mnc,slp_info->cell.wcdma_cell.lac,
                slp_info->cell.wcdma_cell.uc);
        } else if( mdm_raw->MDM_TYPE == MTK_MDM_LTE) {
            slp_info->cell_type = SCLP_CELL_LTE;
            slp_info->cell.lte_cell.mcc = (short)mdm_raw->MCC;
            slp_info->cell.lte_cell.mnc = (short)mdm_raw->NET;
            slp_info->cell.lte_cell.tac = (unsigned short)mdm_raw->AREA;
            slp_info->cell.lte_cell.ci = (int)mdm_raw->CELL_ID;
            slp_info->cell.lte_cell.pci = (short)mdm_raw->OTHER;
            FLPD("lte %d,%d,%d,%ld,%d\t", slp_info->cell.lte_cell.mcc,
                slp_info->cell.lte_cell.mnc, slp_info->cell.lte_cell.tac,
                slp_info->cell.lte_cell.ci, slp_info->cell.lte_cell.pci);
        }
    } else if(mode == MTK_MDM_RAW_TO_QUERY) {
        if(mdm_raw == NULL) {
            FLPD("convert %d fail, data NA\n", mode);
            ret = MTK_FLP_ERROR;
            return ret;
        }
        ap_slp_query_table[query_cnt].cell_info.MDM_TYPE =  mdm_raw->MDM_TYPE;
        ap_slp_query_table[query_cnt].cell_info.MCC = mdm_raw->MCC;
        ap_slp_query_table[query_cnt].cell_info.NET = mdm_raw->NET;
        ap_slp_query_table[query_cnt].cell_info.AREA = mdm_raw->AREA;
        ap_slp_query_table[query_cnt].cell_info.CELL_ID = mdm_raw->CELL_ID;
        ap_slp_query_table[query_cnt].cell_info.SIGNAL_STRENGTH = mdm_raw->SIGNAL_STRENGTH;
        ap_slp_query_table[query_cnt].session = slp_session;
        ap_slp_query_table[query_cnt].index = query_idx;
        ap_slp_query_table[query_cnt].result = 2;
        ap_slp_query_table[query_cnt].is_camping = mdm_raw->isCamping;
        FLPD("MTK_MDM_RAW_TO_QUERY %d: %d, %d, %d, %d, %d, %d",query_cnt, ap_slp_query_table[query_cnt].cell_info.MDM_TYPE,
            ap_slp_query_table[query_cnt].cell_info.SIGNAL_STRENGTH, ap_slp_query_table[query_cnt].session,
            ap_slp_query_table[query_cnt].index,ap_slp_query_table[query_cnt].cell_info.CELL_ID,
            ap_slp_query_table[query_cnt].is_camping);
        query_cnt++;
    } else if(mode == MTK_MDM_QUERY_TO_REACQ) {
        if(ap_slp_query_table[query_idx].cell_info.MDM_TYPE == MTK_MDM_GSM) {
            mdm_reacquire_list.cells[reacquire_cnt].cell_type = SCLP_CELL_GSM;
            mdm_reacquire_list.cells[reacquire_cnt].signal_strength = (unsigned short)ap_slp_query_table[query_idx].cell_info.SIGNAL_STRENGTH;
            mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.mcc = (short)ap_slp_query_table[query_idx].cell_info.MCC;
            mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.mnc = (short)ap_slp_query_table[query_idx].cell_info.NET;
            mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.lac = (unsigned short)ap_slp_query_table[query_idx].cell_info.AREA;
            mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.cid = (unsigned short)ap_slp_query_table[query_idx].cell_info.CELL_ID;
            mdm_reacquire_list.cells[reacquire_cnt].iscamping = 0;//ap_slp_query_table[query_idx].is_camping;
            FLPD("reacquire gsm %d: %d,%d,%d,%d\t", reacquire_cnt,
                mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.mcc,
                mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.mnc,
                mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.lac,
                mdm_reacquire_list.cells[reacquire_cnt].cell.gsm_cell.cid);
            reacquire_cnt++;
            //remove and modify old session info
            flp_clean_query_table(query_idx);
        } else if(ap_slp_query_table[query_idx].cell_info.MDM_TYPE == MTK_MDM_CDMA) {
            mdm_reacquire_list.cells[reacquire_cnt].cell_type = SCLP_CELL_C2K;
            mdm_reacquire_list.cells[reacquire_cnt].signal_strength = (unsigned short)ap_slp_query_table[query_idx].cell_info.SIGNAL_STRENGTH;
            mdm_reacquire_list.cells[reacquire_cnt].cell.cdma_cell.sid = (unsigned short)ap_slp_query_table[query_idx].cell_info.NET;
            mdm_reacquire_list.cells[reacquire_cnt].cell.cdma_cell.nid = (unsigned short)ap_slp_query_table[query_idx].cell_info.AREA;
            mdm_reacquire_list.cells[reacquire_cnt].cell.cdma_cell.bid = (unsigned short)ap_slp_query_table[query_idx].cell_info.CELL_ID;
            mdm_reacquire_list.cells[reacquire_cnt].iscamping = 0;// ap_slp_query_table[query_idx].is_camping;
            FLPD("reacquire cdma %d: %d,%d,%d\t", reacquire_cnt,
                mdm_reacquire_list.cells[reacquire_cnt].cell.cdma_cell.sid,
                mdm_reacquire_list.cells[reacquire_cnt].cell.cdma_cell.nid,
                mdm_reacquire_list.cells[reacquire_cnt].cell.cdma_cell.bid);
            reacquire_cnt++;
            //remove and modify old session info
            flp_clean_query_table(query_idx);
        } else if( ap_slp_query_table[query_idx].cell_info.MDM_TYPE == MTK_MDM_WCDMA) {
            mdm_reacquire_list.cells[reacquire_cnt].cell_type = SCLP_CELL_WCDMA;
            mdm_reacquire_list.cells[reacquire_cnt].signal_strength = (unsigned short)ap_slp_query_table[query_idx].cell_info.SIGNAL_STRENGTH;
            mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.mcc = (short)ap_slp_query_table[query_idx].cell_info.MCC;
            mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.mnc = (short)ap_slp_query_table[query_idx].cell_info.NET;
            mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.lac = (unsigned short)ap_slp_query_table[query_idx].cell_info.AREA;
            mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.uc = (int)ap_slp_query_table[query_idx].cell_info.CELL_ID;
            mdm_reacquire_list.cells[reacquire_cnt].iscamping = 0;//ap_slp_query_table[query_idx].is_camping;
            FLPD("reacquire wcdma %d: %d,%d,%d,%ld\t", reacquire_cnt,
                mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.mcc,
                mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.mnc,
                mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.lac,
                mdm_reacquire_list.cells[reacquire_cnt].cell.wcdma_cell.uc);
            reacquire_cnt++;
            //remove and modify old session info
            flp_clean_query_table(query_idx);
        } else if( ap_slp_query_table[query_idx].cell_info.MDM_TYPE == MTK_MDM_LTE) {
            mdm_reacquire_list.cells[reacquire_cnt].cell_type = SCLP_CELL_LTE;
            mdm_reacquire_list.cells[reacquire_cnt].signal_strength = (unsigned short)ap_slp_query_table[query_idx].cell_info.SIGNAL_STRENGTH;
            mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.mcc = (short)ap_slp_query_table[query_idx].cell_info.MCC;
            mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.mnc = (short)ap_slp_query_table[query_idx].cell_info.NET;
            mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.tac = (unsigned short)ap_slp_query_table[query_idx].cell_info.AREA;
            mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.ci = (int)ap_slp_query_table[query_idx].cell_info.CELL_ID;
            mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.pci = (short)ap_slp_query_table[query_idx].cell_info.OTHER;
            mdm_reacquire_list.cells[reacquire_cnt].iscamping =0;// ap_slp_query_table[query_idx].is_camping;
            FLPD("reacquire lte %d: %d,%d,%d,%ld,%d\t", reacquire_cnt,
                mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.mcc,
                mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.mnc,
                mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.tac,
                mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.ci,
                mdm_reacquire_list.cells[reacquire_cnt].cell.lte_cell.pci);
            reacquire_cnt++;
            //remove and modify old session info
            flp_clean_query_table(query_idx);
        }
    } else if(mode == MTK_MDM_REACQ_TO_QUERY) {
        FLPD("enter MTK_MDM_REACQ_TO_QUERY mode, %d, %d\n",query_cnt,query_idx);
        ap_slp_query_table[query_cnt].cell_info.MDM_TYPE =  mdm_reacquire_list.cells[query_idx].cell_type;
        ap_slp_query_table[query_cnt].cell_info.SIGNAL_STRENGTH = mdm_reacquire_list.cells[query_idx].signal_strength;
        ap_slp_query_table[query_cnt].session = slp_session;
        ap_slp_query_table[query_cnt].index = query_idx;
        ap_slp_query_table[query_cnt].result = 2;
        ap_slp_query_table[query_cnt].is_camping = mdm_reacquire_list.cells[query_idx].iscamping;
        FLPD("MTK_MDM_REACQ_TO_QUERY: %d, %d, %d, %d, %d, %d",ap_slp_query_table[query_cnt].cell_info.MDM_TYPE,
        ap_slp_query_table[query_cnt].cell_info.SIGNAL_STRENGTH, ap_slp_query_table[query_cnt].session,
        ap_slp_query_table[query_cnt].index,ap_slp_query_table[query_cnt].result,
        ap_slp_query_table[query_cnt].is_camping);

        if(ap_slp_query_table[query_cnt].cell_info.MDM_TYPE == SCLP_CELL_GSM) {
            ap_slp_query_table[query_cnt].cell_info.MCC = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.mcc;
            ap_slp_query_table[query_cnt].cell_info.NET = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.mnc;
            ap_slp_query_table[query_cnt].cell_info.AREA = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.lac;
            ap_slp_query_table[query_cnt].cell_info.CELL_ID = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.cid;
            FLPD("reacq2query type:%d, %d %d %d %d, session:%d, indx: %d\n",SCLP_CELL_GSM, ap_slp_query_table[query_cnt].cell_info.MCC,
                ap_slp_query_table[query_cnt].cell_info.NET,ap_slp_query_table[query_cnt].cell_info.AREA,
                ap_slp_query_table[query_cnt].cell_info.CELL_ID, ap_slp_query_table[query_cnt].session, ap_slp_query_table[query_cnt].index);
            query_cnt++;
        } else if(ap_slp_query_table[query_cnt].cell_info.MDM_TYPE == SCLP_CELL_WCDMA) {
            ap_slp_query_table[query_cnt].cell_info.MCC = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.mcc;
            ap_slp_query_table[query_cnt].cell_info.NET = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.mnc;
            ap_slp_query_table[query_cnt].cell_info.AREA = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.lac;
            ap_slp_query_table[query_cnt].cell_info.CELL_ID = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.uc;
            FLPD("reacq2query type:%d, %d %d %d %d, session:%d, indx: %d\n",SCLP_CELL_WCDMA, ap_slp_query_table[query_cnt].cell_info.MCC,
                ap_slp_query_table[query_cnt].cell_info.NET,ap_slp_query_table[query_cnt].cell_info.AREA,
                ap_slp_query_table[query_cnt].cell_info.CELL_ID, ap_slp_query_table[query_cnt].session, ap_slp_query_table[query_cnt].index);
            query_cnt++;
        } else if(ap_slp_query_table[query_cnt].cell_info.MDM_TYPE == SCLP_CELL_LTE) {
            ap_slp_query_table[query_cnt].cell_info.MCC =  mdm_reacquire_list.cells[query_idx].cell.lte_cell.mcc;
            ap_slp_query_table[query_cnt].cell_info.NET = mdm_reacquire_list.cells[query_idx].cell.lte_cell.mnc;
            ap_slp_query_table[query_cnt].cell_info.AREA = mdm_reacquire_list.cells[query_idx].cell.lte_cell.tac;
            ap_slp_query_table[query_cnt].cell_info.CELL_ID = mdm_reacquire_list.cells[query_idx].cell.lte_cell.ci;
            FLPD("reacq2query type:%d, %d %d %d %d, session:%d, indx: %d\n",SCLP_CELL_LTE, ap_slp_query_table[query_cnt].cell_info.MCC,
                ap_slp_query_table[query_cnt].cell_info.NET,ap_slp_query_table[query_cnt].cell_info.AREA,
                ap_slp_query_table[query_cnt].cell_info.CELL_ID, ap_slp_query_table[query_cnt].session, ap_slp_query_table[query_cnt].index);
            query_cnt++;
        } else if(ap_slp_query_table[query_cnt].cell_info.MDM_TYPE == SCLP_CELL_C2K) {
            ap_slp_query_table[query_cnt].cell_info.MCC = 0;
            ap_slp_query_table[query_cnt].cell_info.NET = mdm_reacquire_list.cells[query_idx].cell.cdma_cell.sid;
            ap_slp_query_table[query_cnt].cell_info.AREA = mdm_reacquire_list.cells[query_idx].cell.cdma_cell.nid;
            ap_slp_query_table[query_cnt].cell_info.CELL_ID = mdm_reacquire_list.cells[query_idx].cell.cdma_cell.bid;
            FLPD("reacq2query type:%d, %d %d %d %d, session:%d, indx: %d\n",SCLP_CELL_C2K, ap_slp_query_table[query_cnt].cell_info.MCC,
                ap_slp_query_table[query_cnt].cell_info.NET,ap_slp_query_table[query_cnt].cell_info.AREA,
                ap_slp_query_table[query_cnt].cell_info.CELL_ID, ap_slp_query_table[query_cnt].session, ap_slp_query_table[query_cnt].index);
            query_cnt++;
        }
    } else if(mode == MTK_MDM_REACQ_TO_SLP) {
        if((slp_info == NULL)) {
            FLPD("convert %d fail, data NA\n", mode);
            ret = MTK_FLP_ERROR;
            return ret;
        }
        if(mdm_reacquire_list.cells[query_idx].cell_type == SCLP_CELL_GSM) {
            slp_info->cell.gsm_cell.mcc = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.mcc;
            slp_info->cell.gsm_cell.mnc = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.mnc;
            slp_info->cell.gsm_cell.cid = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.cid;
            slp_info->cell.gsm_cell.lac = mdm_reacquire_list.cells[query_idx].cell.gsm_cell.lac;
            FLPD("reacq gsm %d,%d,%d,%d\t", slp_info->cell.gsm_cell.mcc,
                slp_info->cell.gsm_cell.mnc, slp_info->cell.gsm_cell.lac,slp_info->cell.gsm_cell.cid);
        } else if(mdm_reacquire_list.cells[query_idx].cell_type == SCLP_CELL_WCDMA) {
            slp_info->cell.wcdma_cell.mnc = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.mnc;
            slp_info->cell.wcdma_cell.mcc = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.mcc;
            slp_info->cell.wcdma_cell.lac = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.lac;
            slp_info->cell.wcdma_cell.uc = mdm_reacquire_list.cells[query_idx].cell.wcdma_cell.uc;
            FLPD("reacq wcdma %d,%d,%d,%ld\t", slp_info->cell.wcdma_cell.mcc,
                slp_info->cell.wcdma_cell.mnc,slp_info->cell.wcdma_cell.lac,
                slp_info->cell.wcdma_cell.uc);
        } else if(mdm_reacquire_list.cells[query_idx].cell_type == SCLP_CELL_C2K) {
            slp_info->cell.cdma_cell.bid = mdm_reacquire_list.cells[query_idx].cell.cdma_cell.bid;
            slp_info->cell.cdma_cell.nid= mdm_reacquire_list.cells[query_idx].cell.cdma_cell.nid;
            slp_info->cell.cdma_cell.sid= mdm_reacquire_list.cells[query_idx].cell.cdma_cell.sid;
            FLPD("reacq cdma %d,%d,%d\t", slp_info->cell.cdma_cell.sid,slp_info->cell.cdma_cell.nid,
                slp_info->cell.cdma_cell.bid);
        } else if(mdm_reacquire_list.cells[query_idx].cell_type == SCLP_CELL_LTE) {
            slp_info->cell.lte_cell.mcc = mdm_reacquire_list.cells[query_idx].cell.lte_cell.mcc;
            slp_info->cell.lte_cell.mnc = mdm_reacquire_list.cells[query_idx].cell.lte_cell.mnc;
            slp_info->cell.lte_cell.ci = mdm_reacquire_list.cells[query_idx].cell.lte_cell.ci;
            slp_info->cell.lte_cell.pci = mdm_reacquire_list.cells[query_idx].cell.lte_cell.pci;
            slp_info->cell.lte_cell.tac = mdm_reacquire_list.cells[query_idx].cell.lte_cell.tac;
            FLPD("reacq lte %d,%d,%d,%ld,%d\t", slp_info->cell.lte_cell.mcc,
                slp_info->cell.lte_cell.mnc, slp_info->cell.lte_cell.tac,
                slp_info->cell.lte_cell.ci, slp_info->cell.lte_cell.pci);
        }
    }
    return ret;
}

int flp_rearrange_cell(MTK_MDM_HEADER_T *mdm_header, sclp_cell_info_list *cell_list) {
    int i, k, update_table, cell_cnt = 0;
    int len = mdm_header->length;
    MTK_MDM_DATA_T mdm_data[SCLP_MAX_CELL_SIZE];

    FLPD("flp_rearrange_cell len = %d\n",len);
    if((len <= 0) || (len>SCLP_MAX_CELL_SIZE)) {
        FLPD("flp cell wrong size");
        return -1;
    }

    //fill cell_list struct
    cell_list->caller = SCLP_CALLER_FLPD;
    cell_list->session_id = slp_session;

    FLPD("slp session %d\n", slp_session);
    memcpy(&mdm_data, ((UINT8*)mdm_header) + sizeof(MTK_MDM_HEADER_T),len*sizeof(MTK_MDM_DATA_T));

    for(i=0; i<len; i++) {
        update_table = 1;
        //Check if cell has been recorder previously
        for (k=0; k<query_cnt; k++) {
            if((ap_slp_query_table[k].cell_info.MDM_TYPE == mdm_data[i].MDM_TYPE) &&
                (ap_slp_query_table[k].cell_info.CELL_ID == mdm_data[i].CELL_ID)) {
                if(ap_slp_query_table[k].cell_info.SIGNAL_STRENGTH == mdm_data[i].SIGNAL_STRENGTH) {
                    FLPD("discard query of cell id %d, rec cell id %d \n", mdm_data[i].CELL_ID, ap_slp_query_table[k].cell_info.CELL_ID );
                } else {
                    FLPD("update slp table, %d, signal from %d to %d \n", k, ap_slp_query_table[k].cell_info.SIGNAL_STRENGTH, mdm_data[i].SIGNAL_STRENGTH );
                    ap_slp_query_table[k].cell_info.SIGNAL_STRENGTH = mdm_data[i].SIGNAL_STRENGTH;
                }
                k = query_cnt;
                update_table = 0;
            }
        }

        if(update_table) {
            flp_cell_table_convert(MTK_MDM_RAW_TO_SLP, &cell_list->cells[cell_cnt], 0, &mdm_data[i], 0);
            cell_cnt += 1;

            if(query_cnt >= DB_SIZE) {
                for(k=0;k<query_cnt;k++) {
                    FLPD("dump cell table %d, %d, %d \n", k, ap_slp_query_table[k].cell_info.CELL_ID,ap_slp_query_table[k].cell_info.MDM_TYPE );
                }
                memmove(&ap_slp_query_table[0],&ap_slp_query_table[1], (DB_SIZE-1)*sizeof(MTK_MDM_AP_QUERY_TABLE));
                query_cnt = DB_SIZE-1;
            }
            flp_cell_table_convert(MTK_MDM_RAW_TO_QUERY, 0, 0, &mdm_data[i], i);
        }
    }

    cell_list->num = cell_cnt;
    slp_session++;

    FLPD("flp_rearrange_cell done, cell_cnt = %d \n", cell_cnt);
    return 0;
}

void flp_reacquire_cell() {
    int j;
    sclp_cell_info_list cell_list ={0};

    FLPD("reacquire cell");

    for(j = 0; j < reacquire_cnt; j++) {
        //copy reacquire list to slp session
        FLPD("flp_reacquire_cell %d",j);
        flp_cell_table_convert(MTK_MDM_REACQ_TO_SLP, &cell_list.cells[j],0,0,j);
        if(query_cnt >= DB_SIZE) {
            memmove(&ap_slp_query_table[0], &ap_slp_query_table[1],(DB_SIZE-1)*sizeof(MTK_MDM_AP_QUERY_TABLE));
            query_cnt = DB_SIZE-1;
        }
        //copy reacquire list to query list
        flp_cell_table_convert(MTK_MDM_REACQ_TO_QUERY, 0, 0, 0, j);
        if(j == (reacquire_cnt-1)) {
            //set last query as camping cell
            mdm_reacquire_list.cells[j].iscamping = 1;
        }
    }

    cell_list.caller = SCLP_CALLER_FLPD;
    cell_list.session_id = slp_session;
    cell_list.num = reacquire_cnt;

    slp_start(&cell_list);
    reacquire_cnt = 0;
    slp_session++;
}

int flp_slp_result_to_cnn(sclp_cell_location_list* result) {
    int i,j,k;
    int start_cnt = 0, session_found = 0;
    MTK_MDM_CNN_QUERY_TABLE mdm_res ={0};
    sclp_cell_info tmp_cell[SCLP_MAX_CELL_SIZE];
    MTK_FLP_MSG_T *ptr=NULL, *flp_msg=NULL ;

    if(result->num >0) {
        for(i = 0; i < result->num; i++) {
            session_found = 0;
            for(j = 0; j < query_cnt; j++) {
                if((ap_slp_query_table[j].session == result->session_id) &&
                    (ap_slp_query_table[j].index == result->locations[i].index))
                {
                    start_cnt = j;
                    j= query_cnt;
                    session_found = 1;
                }
            }
            if((start_cnt+i) >= DB_SIZE) {
                start_cnt = -1*i;
            }
            if(result->locations[i].result == SCLP_RESULT_SUCCESS) {
                if(session_found) {
                    mdm_res.CELL_ID = ap_slp_query_table[start_cnt+i].cell_info.CELL_ID;
                    mdm_res.lat = result->locations[i].lat;
                    mdm_res.lng = result->locations[i].lng;
                    mdm_res.alt = result->locations[i].alt;
                    mdm_res.acc = result->locations[i].acc;
                    mdm_res.SIGNAL_STRENGTH = ap_slp_query_table[start_cnt+i].cell_info.SIGNAL_STRENGTH;
                    mdm_res.is_camping = ap_slp_query_table[start_cnt+i].is_camping;

                    ptr = mtk_flp_sys_mem_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(MTK_MDM_CNN_QUERY_TABLE));
                    ptr->length = sizeof(MTK_MDM_CNN_QUERY_TABLE);
                    ptr->type = MTK_FLP_MDS_DC_CELL_DATA_FROM_AP;
                    memcpy(((INT8*)ptr)+sizeof(MTK_FLP_MSG_T), &mdm_res, sizeof(MTK_MDM_CNN_QUERY_TABLE));
                    mtk_flp_dc_offload_process(ptr); //send HAL(AP)->Kernel(connsys) msg here
                    mtk_flp_sys_msg_free(ptr);
                }
            } else if((result->locations[i].result != SCLP_RESULT_ERROR_INVALID_PARAM) ||
                (result->locations[i].result != SCLP_RESULT_ERROR_NO_POSITION)) {
                if(reacquire_cnt >= SCLP_MAX_CELL_SIZE ) {
                    memset( tmp_cell, 0 ,sizeof(tmp_cell));
                    memmove(&mdm_reacquire_list.cells[0], &mdm_reacquire_list.cells[1], (SCLP_MAX_CELL_SIZE-1)*sizeof(sclp_cell_location));
                    reacquire_cnt = SCLP_MAX_CELL_SIZE-1;
                }
                flp_cell_table_convert(MTK_MDM_QUERY_TO_REACQ, 0, 0, 0, (start_cnt+i));
            }
        }
    } else {
       return -1;
    }

    return 0;
}

static void SLP_reboot() {
    FLPD("slp_reboot() recv");
}

static void SLP_location_result(sclp_cell_location_list* result) {
    int i;
    FLPD("slp_location_result() recv");
    //dump_sclp_cell_location_list(result);

    if(result->num < 1) {
        FLPD("slp loc result err, %d", result->num);
        return;
    }

    for(i = 0; i < result->num; i++) {
        FLPD("slp session: %d,lat %f,lon %f,acc %d, result %d ", result->session_id, result->locations[i].lat, result->locations[i].lng, result->locations[i].acc, result->locations[i].result);
    }
    flp_slp_result_to_cnn(result);

}

int epoll_add_fd(int epfd, int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        FLPD("epoll_add_fd() epoll_ctl() failed reason=[%s]%d epfd=%d fd=%d",
            strerror(errno), errno, epfd, fd);
        return -1;
    } else {
        FLPD("epoll_add_fd success");
    }
    return 0;
}

/************************************************************************/
//  Receiver location/meassurment from GNSS HAL and report back to DC
/************************************************************************/
void mtk_flp_dc_cell_main(void) {
    struct epoll_event events[MAX_EPOLL_EVENT];
    int epfd, fd = -1, i, n, type;
    //sclp_callbacks g_sclp_callbacks;
    sclp_cell_location_list loc_list;

    usleep(10000000);
    usleep(10000000);
    FLPD("DC Cell Main starts 2....");

    while( (fd == -1) || (fd == 0)) {
         fd = slp_bind_callback(SCLP_CALLER_FLPD);
    }

    epfd = epoll_create(MAX_EPOLL_EVENT);
    epoll_add_fd(epfd, fd);
    FLPD("slp_bind_callback, fd=%d, %d",fd, epfd);

    memset(&ap_slp_query_table, 0, DB_SIZE*sizeof(MTK_MDM_AP_QUERY_TABLE));
    memset(&mdm_reacquire_list, 0, sizeof(sclp_cell_info_list));

   // g_sclp_callbacks.slp_reboot = SLP_reboot;
    //g_sclp_callbacks.slp_location_result = SLP_location_result;

    while(1) {
        n = epoll_wait(epfd, events, MAX_EPOLL_EVENT , -1);

        if(n == -1) {
            if(errno == EINTR) {
                continue;
            } else {
                FLPD("sclp_main_thread() epoll_wait failure reason=[%s]%d",
                strerror(errno), errno);
                return;
            }
        }
        for(i = 0; i < n; i++) {
            FLPD("enter slp cb");
            if((events[i].data.fd == fd) && (events[i].events & EPOLLIN)) {
                FLPD("slp_callback_hdlr, fd %d", fd);
                memset(&loc_list, 0 ,sizeof(sclp_cell_location_list));
                slp_callback_hdlr(fd, &type, &loc_list);
                if(type == SCLP_CALLBACK_REBOOT) {
                    SLP_reboot();
                } else if(type == SCLP_CALLBACK_LOCATION_RESULT) {
                    SLP_location_result(&loc_list);
                }
            }
        }
    }
}

/************************************************************************/
//  Process msgs from DC and send to GNSS HAL
/************************************************************************/
int mtk_flp_dc_cell_process(MTK_FLP_MSG_T *prmsg) {
    int i, ret;
    MTK_MDM_HEADER_T *mdm_header = NULL;
    sclp_cell_info_list cell_list;

    if((prmsg == NULL)) {
        FLPD("mtk_flp_dc_cell_main, recv prmsg is null pointer\r\n");
        return -1;
    }
    FLPD("Cell Proc msg(%p) type:0x%02x len:%d", prmsg, prmsg->type,prmsg->length);

    switch(prmsg->type) {
        case MTK_FLP_MDS_DC_CELL_DATA_TO_AP:
            mdm_header =  mtk_flp_sys_mem_alloc(prmsg->length);
            memcpy( mdm_header,(INT8*)prmsg+sizeof(MTK_FLP_MSG_T),prmsg->length);
            FLPD("copy mdm_header, mdm_header len = %d \n",mdm_header->length);

            if(mdm_header != NULL) {
                //rearrange cell info as request by slp function
                ret = flp_rearrange_cell(mdm_header, &cell_list);

                if(!ret) {
                    FLPD("cell_list dbg: %d %d", cell_list.caller, cell_list.num);

                    //receive cell id from cnn, call slp function to query cell latlon
                    slp_start(&cell_list);

                    mtk_flp_sys_mem_free(mdm_header);
                    FLPD("mtk_flp_dc_cell_process success");
                } else {
                    mtk_flp_sys_mem_free(mdm_header);
                    FLPD("convert cell error");
                }
            }
            break;
        case MTK_FLP_MSG_MNLD_NETWORK_STATUS_NTF:
            FLPD("Received network status, message type: %x, len = %d", prmsg->type,(prmsg->length));
            if(prmsg->length >= (2*sizeof(int))) {
                memcpy(&network_status, ((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), sizeof(int));
                memcpy(&network_type, ((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)+sizeof(int), sizeof(int));
                FLPD("network status: %d, network type: %d \n", network_status, network_type);
                if((network_status) && (reacquire_cnt > 0)) {
                    flp_reacquire_cell();
                }
            }
            break;
        default:
            FLPD("unidentified msg: %0x%02x\n",prmsg->type);
            break;
    }
    return 0;
}
#else //flp run in cnn
MTK_FLP_LOCATION_T cell_latest_loc = {0};
MTK_MDM_CNN_QUERY_TABLE cell_database[DB_SIZE];
int db_cnt = 0;
int flp_cell_loc_available = 0;
int flp_cell_start = 0;

void flp_dc_simulate_cell(MTK_MDM_DATA_T *mdm_data, MTK_MDM_SOURCE_TYPE_E type) {
    int i;
    //Golden data GSM: [mcc,net,area,cellid]
    //Golden data CDMA: [net,area,cellid]
    //Golden data WCDMA: [mcc,net,area,cellid]
    //Golden data LTE: [mcc,net,area,cellid]
    UINT32 gsm_cell[10][4] ={ {262,2,435,37041},{262,3,32229,5259},{262,7,51042,10054},{262,1,18511,23469},{645,1,160,9079},{302,720,14100,33951},{262,2,852,3731},{502,19,13004,19033},{214,7,716,5192},{270,1,1031,15312} };
    UINT32 cdma_cell[10][3] ={ {410,6035,14361},{410,33108,54112},{410,408,30043},{260,8901,34132},{26,43786,47504},{890,307,20713},{590,3,50280},{260,10611,21462},{410,35534,24053},{410,53021,40306} };
    UINT32 wcdma_cell[10][4] ={ {262,2,801,86355},{262,3,1107,329299},{262,7,21934,15378149},{716,6,7308,284140},{722,7,4437,237713},{232,3,58400,2107269},{250,2,5407,6630539},{219,10,5120,154455831},{214,7,3860,22165221},{204,16,705,47267223} };
    UINT32 lte_cell[10][5] ={ {216,30,12002,168451,434},{216,30,480,206339,17},{216,30,12000,130818,373},{216,1,5121,86787,399},{216,70,3301,1287682,223},{204,16,295,3184651,0},{216,1,5211,442881,109},{216,30,12002,230146,259},{216,1,5811,33283,17},{216,30,910,151297,258} };
    //UINT8 *msg_dbg = NULL;

    if((type == MTK_MDM_GSM) || (type == MTK_MDM_TYPE_END )) {
        for(i = 0; i < SCLP_MAX_CELL_SIZE; i++) {
            mdm_data[i].MDM_TYPE = (UINT16)MTK_MDM_GSM;
            mdm_data[i].MCC = (UINT16)(gsm_cell[i][0]);
            mdm_data[i].NET = (UINT16)(gsm_cell[i][1]);
            mdm_data[i].AREA= (UINT16)(gsm_cell[i][2]);
            mdm_data[i].CELL_ID= (UINT32)(gsm_cell[i][3]);

            FLP_TRC("gsm 1: %d,%d,%d,%d,%d ", mdm_data[i].MDM_TYPE,mdm_data[i].MCC,mdm_data[i].NET,mdm_data[i].AREA, mdm_data[i].CELL_ID);
        }
    } else if(type == MTK_MDM_CDMA) {
        for(i = 0; i < SCLP_MAX_CELL_SIZE; i++) {
            mdm_data[i].MDM_TYPE = (UINT16)MTK_MDM_CDMA;
            mdm_data[i].NET = (UINT16)(cdma_cell[i][0]);
            mdm_data[i].AREA= (UINT16)(cdma_cell[i][1]);
            mdm_data[i].CELL_ID= (UINT32)(cdma_cell[i][2]);

            FLP_TRC("cdma 1: %d,%d,%d,%d ", mdm_data[i].MDM_TYPE,mdm_data[i].NET,mdm_data[i].AREA, mdm_data[i].CELL_ID);
        }
    } else if(type == MTK_MDM_WCDMA) {
        for(i = 0; i < SCLP_MAX_CELL_SIZE; i++) {
            mdm_data[i].MDM_TYPE = (UINT16)MTK_MDM_WCDMA;
            mdm_data[i].MCC = (UINT16)(wcdma_cell[i][0]);
            mdm_data[i].NET = (UINT16)(wcdma_cell[i][1]);
            mdm_data[i].AREA= (UINT16)(wcdma_cell[i][2]);
            mdm_data[i].CELL_ID= (UINT32)(wcdma_cell[i][3]);

            FLP_TRC("wcdma 1: %d,%d,%d,%d,%d ", mdm_data[i].MDM_TYPE,mdm_data[i].MCC,mdm_data[i].NET,mdm_data[i].AREA, mdm_data[i].CELL_ID);
        }
    } else if(type == MTK_MDM_LTE) {
        for(i = 0; i < SCLP_MAX_CELL_SIZE; i++) {
            mdm_data[i].MDM_TYPE = (UINT16)MTK_MDM_LTE;
            mdm_data[i].MCC = (UINT16)(lte_cell[i][0]);
            mdm_data[i].NET = (UINT16)(lte_cell[i][1]);
            mdm_data[i].AREA= (UINT16)(lte_cell[i][2]);
            mdm_data[i].CELL_ID= (UINT32)(lte_cell[i][3]);

            FLP_TRC("lte 1: %d,%d,%d,%d,%d ", mdm_data[i].MDM_TYPE,mdm_data[i].MCC,mdm_data[i].NET,mdm_data[i].AREA, mdm_data[i].CELL_ID);
        }
    }
}

void flp_cell_parsing_location (MTK_MDM_CNN_QUERY_TABLE cell_in, MTK_FLP_LOCATION_T *loc_out) {
   if(loc_out != NULL) {
        loc_out->accuracy = cell_in.acc;
        loc_out->altitude = cell_in.alt;
        loc_out->bearing = 0.0;
        loc_out->flags = FLP_LOCATION_HAS_LAT_LONG|FLP_LOCATION_HAS_ALTITUDE|FLP_LOCATION_HAS_ACCURACY;
        loc_out->latitude = cell_in.lat;
        loc_out->longitude = cell_in.lng;
        loc_out->size = sizeof(MTK_FLP_LOCATION_T);
        loc_out->sources_used = FLP_TECH_MASK_CELL;
        loc_out->speed = 0.0;
        loc_out->timestamp = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick()/TTICK_CLK);
        FLP_TRC("cell info: %lf, %lf\n",loc_out->latitude,loc_out->longitude);
    }
}

/************************************************************************/
//  Receiver location/meassurment from GNSS HAL and report back to DC
/************************************************************************/
//#define CELL_TEST_SLP

#ifdef CELL_TEST_SLP
extern INT32 mtk_flp_offload_check_msg(void);
#endif

void mtk_flp_dc_cell_main(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_MSG_T *ptr = NULL;
    MTK_MDM_HEADER_T mdm_header= {0};
    MTK_MDM_HEADER_T rearrange_query_header = {0};
    #ifdef CELL_TEST_SLP
    MTK_MDM_DATA_T mdm_data[SCLP_MAX_CELL_SIZE];
    #else
    MTK_MDM_DATA_T mdm_data = {0};
    MTK_MDM_DATA_T *mdm_query_data = NULL;
    MTK_FLP_LOCATION_T loc = {0};
    MTK_FLP_MSG_T *flp_msg = NULL;
    int i, j;
    UINT32 total_query_size = 0;
    #endif

    FLP_TRC("DC cell Main starts....");

    if((prmsg == NULL) || (prmsg->length < 0)) {
        FLP_TRC("mtk_flp_dc_gnss_main, recv prmsg is null pointer\r\n");
        return;
    }

    switch( prmsg->type) {
       case CMD_FLP_START_CELL_RES:
            FLP_TRC("start mdm success\n");
        break;
       case CMD_FLP_STOP_CELL_RES:
            FLP_TRC("stop mdm success\n");
        break;
       case CMD_SCP_SEND_CELL_ID:
            FLP_TRC("recv cell id from scp");

            #ifdef CELL_TEST_SLP
            //generate fake cell id data
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_MDM_HEADER_T) + SCLP_MAX_CELL_SIZE*sizeof(MTK_MDM_DATA_T)) ;
            ptr->type = prmsg->type;
            ptr->length = sizeof(MTK_MDM_HEADER_T) + SCLP_MAX_CELL_SIZE*sizeof(MTK_MDM_DATA_T);
            FLP_TRC("cell len = %d", ptr->length );
            mdm_header.length = SCLP_MAX_CELL_SIZE;
            memset( &mdm_data, 0, SCLP_MAX_CELL_SIZE*sizeof(MTK_MDM_DATA_T) );

            flp_dc_simulate_cell( (MTK_MDM_DATA_T *)mdm_data, (MTK_MDM_SOURCE_TYPE_E)(cell_on) );
            memcpy( (INT8*)ptr+sizeof(MTK_FLP_MSG_T),&mdm_header, sizeof(MTK_MDM_HEADER_T));
            memcpy( (INT8*)ptr+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_MDM_HEADER_T),&mdm_data, SCLP_MAX_CELL_SIZE*sizeof(MTK_MDM_DATA_T));
            mtk_flploc_mcu_sys_msg_send(ptr,MTK_FLP_TASKID_GEOFENCE);

            cell_on++;

            if(cell_on == 5) {
                mtk_flp_mcu_timer_stop(FLP_TIMER_ID_RESERVED);
            }
            mtk_flp_offload_check_msg();

            #else
            memcpy( &mdm_header, (INT8*)prmsg+sizeof(MTK_FLP_MSG_T),sizeof(MTK_MDM_HEADER_T));
            FLP_TRC("mdm header len:%d\n", mdm_header.length);

            // look up table
            if(mdm_header.length <= 0) {
                return;
            }

            if(db_cnt <= 0) { //first query, no db yet
                ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) + (prmsg->length));
                ptr->type = MTK_FLP_MDS_DC_CELL_DATA_TO_AP;
                ptr->length = prmsg->length;
                memcpy((INT8*)ptr+sizeof(MTK_FLP_MSG_T), (INT8*)prmsg+sizeof(MTK_FLP_MSG_T), prmsg->length);
                mtk_flploc_mcu_sys_msg_send(ptr,MTK_FLP_TASKID_GEOFENCE);
            } else {
                mdm_query_data = mtk_flp_mcu_mem_alloc( (mdm_header.length)*sizeof(MTK_MDM_DATA_T));
                for(j = 0; j < mdm_header.length; j++) {
                    memset( &mdm_data, 0, sizeof(MTK_MDM_DATA_T));
                    memcpy( &mdm_data, (INT8*)prmsg+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_MDM_HEADER_T)+ (j*sizeof(MTK_MDM_DATA_T)), sizeof(MTK_MDM_DATA_T));
                    for(i = 0;i < db_cnt; i++) {
                        if((cell_database[i].CELL_ID == mdm_data.CELL_ID)) {
                            if(mdm_data.isCamping == 1) {
                                FLP_TRC("Hit cell table: %d, camping %d, strength %d, lat %f, lng %f, alt %d, acc %d \n", cell_database[i].CELL_ID,
                                cell_database[i].is_camping, cell_database[i].SIGNAL_STRENGTH, cell_database[i].lat, cell_database[i].lng,
                                cell_database[i].alt, cell_database[i].acc);

                                //Report cell location to dc
                                flp_cell_parsing_location(cell_database[i], &loc);

                                //update latest cell location
                                memcpy( &cell_latest_loc, &loc, sizeof(MTK_FLP_LOCATION_T) );
                                flp_cell_loc_available = 1;

                                flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T));
                                flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                                flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                                memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T));
                                mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
                            }
                            break;
                        }
                        if(i == (db_cnt-1)) {
                            memcpy(&mdm_query_data[total_query_size], &mdm_data, sizeof(MTK_MDM_DATA_T));
                            total_query_size++;
                        }
                    }
                }
                if(total_query_size > 0) {
                    // new query
                    ptr = mtk_flp_mcu_mem_alloc(sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_MDM_HEADER_T) + (total_query_size*sizeof(MTK_MDM_DATA_T)));
                    ptr->type = MTK_FLP_MDS_DC_CELL_DATA_TO_AP;
                    ptr->length = sizeof(MTK_MDM_HEADER_T) + (total_query_size*sizeof(MTK_MDM_DATA_T));
                    rearrange_query_header.length = total_query_size;
                    memcpy((INT8*)ptr+sizeof(MTK_FLP_MSG_T), &rearrange_query_header, sizeof(MTK_MDM_HEADER_T));
                    memcpy((INT8*)ptr+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_MDM_HEADER_T), mdm_query_data, total_query_size*sizeof(MTK_MDM_DATA_T));
                    mtk_flploc_mcu_sys_msg_send(ptr,MTK_FLP_TASKID_GEOFENCE);
                }
                mtk_flp_mcu_mem_free(mdm_query_data);
            }
            #endif
            break;
        default:
            FLP_TRC("Unknown cell message type: %x", prmsg->type);
            break;

    }

}

/************************************************************************/
//  Process msgs from DC and send to GNSS HAL
/************************************************************************/
int mtk_flp_dc_cell_process(MTK_FLP_MSG_T *prmsg) {
#ifndef CELL_TEST_SLP
    MTK_FLP_MSG_T *ptr;
#endif
    MTK_FLP_BATCH_OPTION_T *option;
    MTK_MDM_CNN_QUERY_TABLE tmp_mdm_info[SCLP_MAX_CELL_SIZE] = {{0}};
    MTK_MDM_CNN_QUERY_TABLE camping_cell = {0};
    MTK_FLP_LOCATION_T loc = {0};
    MTK_FLP_MSG_T *flp_msg = NULL;
    int mdm_info_size, i;

    option = (MTK_FLP_BATCH_OPTION_T *)( (UINT8*)prmsg+sizeof(MTK_FLP_MSG_T) );
    FLP_TRC("Cell Proc msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch(prmsg->type) {
        case MTK_FLP_MSG_DC_START_CMD:
        {
            if( option->sources_to_use & FLP_TECH_MASK_CELL) {
                FLP_TRC("START Zero power geofence");
                flp_cell_start = 1;
#ifdef CELL_TEST_SLP
                //Just for experiment, remove for real case
                mtk_flp_mcu_timer_start(FLP_TIMER_ID_RESERVED,CELL_TIMER);
#else
                if(flp_cell_loc_available) {
                    flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T) );
                    flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                    flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &cell_latest_loc, sizeof(MTK_FLP_LOCATION_T) );
                    mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
                }
                ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
                ptr->type = CMD_FLP_START_CELL;
                ptr->length = 0;
                mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_SCP,ptr);
#endif
            }
        }
        break;
        case MTK_FLP_MSG_DC_STOP_CMD:
        {
            FLP_TRC("Stop Zero power geofence");
            flp_cell_start = 0;
#ifdef CELL_TEST_SLP
            //Just for experiment, remove for real case
            mtk_flp_mcu_timer_stop(FLP_TIMER_ID_RESERVED);
#else
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
            ptr->type = CMD_FLP_STOP_CELL;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_SCP,ptr);
#endif
        }
        break;
        case MTK_FLP_MDS_DC_CELL_DATA_FROM_AP:
        {
            FLP_TRC("cell lla from AP");

            //extract data from AP
            if(prmsg->length > 0) {
                memcpy(&tmp_mdm_info,((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), prmsg->length );
                memcpy(&camping_cell, &tmp_mdm_info, sizeof(MTK_MDM_CNN_QUERY_TABLE));
                mdm_info_size = (prmsg->length)/sizeof(MTK_MDM_CNN_QUERY_TABLE);
                FLP_TRC("mdm_info_size = %d", mdm_info_size);

                if((db_cnt+mdm_info_size) >= DB_SIZE) {
                    for(i = 0;i < db_cnt; i++) {
                        FLP_TRC("cell id b4: %d, camping %d, strength %d, lat %f, lng %f, alt %d, acc %d \n", cell_database[i].CELL_ID,
                        cell_database[i].is_camping, cell_database[i].SIGNAL_STRENGTH, cell_database[i].lat, cell_database[i].lng,
                        cell_database[i].alt, cell_database[i].acc);
                    }
                    if(db_cnt < DB_SIZE) {
                        memcpy(&cell_database[db_cnt],&tmp_mdm_info[0],(DB_SIZE-db_cnt)*sizeof(MTK_MDM_CNN_QUERY_TABLE) );
                    }
                    memcpy(&cell_database[0],&tmp_mdm_info[DB_SIZE-db_cnt],(mdm_info_size-DB_SIZE+db_cnt)*sizeof(MTK_MDM_CNN_QUERY_TABLE));
                    db_cnt = db_cnt + mdm_info_size-DB_SIZE;

                    for(i = 0; i < db_cnt; i++) {
                        FLP_TRC("cell id after: %d, camping %d, strength %d, lat %f, lng %f, alt %d, acc %d \n", cell_database[i].CELL_ID,
                        cell_database[i].is_camping, cell_database[i].SIGNAL_STRENGTH, cell_database[i].lat, cell_database[i].lng,
                        cell_database[i].alt, cell_database[i].acc);
                    }
                } else {
                    memcpy(&cell_database[db_cnt],&tmp_mdm_info, prmsg->length );
                    db_cnt += mdm_info_size;
                }

                //Report cell location to dc
                if((camping_cell.is_camping == 1) && (flp_cell_start == 1)) {
                    flp_cell_parsing_location(camping_cell, &loc);

                    //update latest cell location
                    memcpy(&cell_latest_loc, &loc, sizeof(MTK_FLP_LOCATION_T));
                    flp_cell_loc_available = 1;

                    flp_msg = mtk_flp_mcu_mem_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_LOCATION_T));
                    flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                    flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T));
                    mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
                }

#ifdef CELL_TEST_SLP
                mtk_flp_offload_check_msg();
#endif
            }
        }
        break;
        default:
            FLP_TRC("Unknown ZPG message to send");
        break;
    }

    return 0;
}

FLP_VOID mtk_flp_cell_expire() {
    MTK_FLP_MSG_T flp_msg;
    flp_msg.length = 0;
    flp_msg.type = CMD_SCP_SEND_CELL_ID;

    mtk_flp_dc_cell_main(&flp_msg);
}

#endif
