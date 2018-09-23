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
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
//#include <sys/timeb.h>
//#include <utils/Log.h> // For Debug
#include <sys/stat.h>
//fused_location.h located in: alps\hardware\libhardware\include\hardware
#include "mtk_flp_dc.h"
//#include <android/log.h>
//#include <utils/Log.h> // For Debug
#include <math.h>

#ifndef K2_PLATFORM
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#else
#include "mtk_flp_connsys_sys.h"
#include "mtk_gps_bora_flp.h"
#endif

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FLP_DC_PS"

#ifndef K2_PLATFORM
#define FLPE(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define FLPD(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define FLPW(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define FLPD(...)
#define FLPW(...)
#define FLPE(...)
#endif

#else
#define FLPD(...)
#define FLPW(...)
#define FLPE(...)
#endif

//*****************************************
// Report thread function ball backs
//*****************************************
#define PSEUDO_TIMER 1000 //1sec
#ifndef K2_PLATFORM
extern VOID mtk_flp_host_timer_expire(MTK_FLP_TIMER_E id);
#endif

void mtk_flp_dc_pseudo_main(void);
int mtk_flp_dc_pseudo_process(MTK_FLP_MSG_T *prmsg);

#ifndef K2_PLATFORM
#define PSEUDO_LOC 1
#else
#define PSEUDO_LOC 0
#endif
#if (PSEUDO_LOC==1)
DcReportThread    Report_thrd_PSEUDO = NULL; //(DcReportThread)mtk_flp_dc_pseudo_main;        //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_PSEUDO = (DcProcessFunction)mtk_flp_dc_pseudo_process; //need to set to NULL for not activating this location source.
#else
DcReportThread    Report_thrd_PSEUDO = NULL;        //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_PSEUDO = NULL; //need to set to NULL for not activating this location source.
#endif


//*****************************************************************************
//  Pseudo function for generating a CIRCLE path.
//*****************************************************************************
#define OrigX   (2.112162*180/M_PI)
#define OrigY   (0.433563*180/M_PI)
#define PseudoR 0.0001
static INT32    curDegree=0;
static double   curX=0;
static double   curY=0;
static INT32    PseudoRun=0;
static INT32    PseudoDiagRun=0;
static INT32    AP_Reported=0;

static void GetPseudoLocation(MTK_FLP_LOCATION_T *loc) {
#ifdef K2_PLATFORM
    FLP_TRC("Generate Pseudo Location\n");
#else
    FLPD("Generate Pseudo Location\n");
#endif

    if(loc == NULL)
        return;

    curX = OrigX + PseudoR * cos(curDegree * M_PI /180);
    curY = OrigY + PseudoR * sin(curDegree * M_PI /180);

    loc->altitude = 0;
    loc->longitude = curX;
    loc->latitude = curY;
    loc->accuracy = 1;
    loc->bearing = 0;
    loc->flags = FLP_LOCATION_HAS_LAT_LONG|FLP_LOCATION_HAS_ALTITUDE|FLP_LOCATION_HAS_SPEED|FLP_LOCATION_HAS_BEARING|FLP_LOCATION_HAS_ACCURACY;
    loc->size = sizeof(MTK_FLP_LOCATION_T);
    loc->sources_used =  FLP_TECH_MASK_PSEUDO;
    loc->speed = 0;
#ifdef K2_PLATFORM
    loc->timestamp = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick()/TTICK_CLK); ///TODO:Change to proper utc time when function is available
#else
    loc->timestamp = mtk_flp_get_timestamp(NULL);
#endif

    curDegree +=6;          //step 6 degree each time.
    if( curDegree >=360)
        curDegree=0;

#ifdef K2_PLATFORM
    FLP_TRC(" CurX:%f CurY:%f", curX, curY);
#else
    FLPD(" CurX:%f CurY:%f", curX, curY);
#endif

}

void mtk_flp_pseudo_expire() {
    //FLP_TRC("pseudo timer handler");
    mtk_flp_dc_pseudo_main();
}

extern INT32 mtk_flp_lowpower_query_mode ();

#ifndef K2_PLATFORM
static void PseudoDiagAPReport(void) {
    char buf[1024];
    MTK_FLP_MSG_T   *flp_msg;

    sprintf(buf, "WIFI_AP_NTF=SSID::AABBCCDD\tLNG::%f\tLAT::%f\tALT::0\nSSID::AABBCCDE\tLNG::%f\tLAT::%f\tALT::0\n",
                OrigX - PseudoR, OrigY-PseudoR, OrigX + PseudoR, OrigY+PseudoR);
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(buf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(buf);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), buf, strlen(buf) );
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

    sprintf(buf, "BT_AP_NTF=MAC::01-02-03-04-05-06\tLNG::%f\tLAT::%f\tALT::0\tDIST:3\tRSSI:33\nMAC::01-02-03-04-05-16\tLNG::%f\tLAT::%f\tALT::0\tDIST:5\tRSSI:55\n",
                OrigX + PseudoR, OrigY-PseudoR, OrigX - PseudoR, OrigY+PseudoR);
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(buf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(buf);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), buf, strlen(buf) );
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

}

static void PseudoDiagReport(void) {
    char buf[1024];
    MTK_FLP_MSG_T   *flp_msg;

    sprintf(buf, "FLP_NTF=Lng::%.7f LAT::%.7f ALT:0 \n", curX, curY);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(buf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(buf);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), buf, strlen(buf));
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
    if(AP_Reported == 0) {
        AP_Reported = 1;
        PseudoDiagAPReport();
    }

}

/************************************************************************/
//  Receiver location/meassurment from GNSS HAL and report back to DC
/************************************************************************/
void mtk_flp_dc_pseudo_main(void) {
    MTK_FLP_MSG_T   *flp_msg= NULL;
    MTK_FLP_LOCATION_T  loc;
    char localbuf[512];

    if(PseudoRun == 1) {
        GetPseudoLocation(&loc);
        if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY)) {
            #if 0
            sprintf(localbuf,"FLP_NTF=LNG:%f LAT:%f ALT:%f", loc.longitude, loc.latitude, loc.altitude);
            flp_msg = mtk_flp_sys_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
            flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
            flp_msg->length = strlen(localbuf);
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), localbuf,strlen(localbuf) );
            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
            #endif
            /* fake gnss data */
            loc.sources_used = FLP_TECH_MASK_GNSS;
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_LOCATION_T));
            FLPD("[%s:%d]Malloc msg:%x Location struct:%x",__FILE__,__LINE__,flp_msg , (UINT8*)flp_msg+sizeof(MTK_FLP_MSG_T));
            flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
            flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
            FLPD("flp_msg[%x]", (unsigned int)flp_msg);
            memcpy(((char*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T));
            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
        }
    }
    #if 0
    int i;
    MTK_FLP_MSG_T   *flp_msg;
    MTK_FLP_LOCATION_T  loc;


    FLPD("mtk_flp_dc_offload_main running.....\n");
    while(1)
    {
        do{
            usleep(1000000);
           // FLPD("PseudoRun = %d\n",PseudoRun);
        }while( PseudoRun==0 );


    GetPseudoLocation(&loc);
    //mtk_flp_dump_locations(&loc);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_LOCATION_T));
    FLPD("[%s:%d]Malloc msg:%x Location struct:%x",__FILE__,__LINE__,flp_msg , (UINT8*)flp_msg+sizeof(MTK_FLP_MSG_T));
    flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
    flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
    FLPD("flp_msg[%x]", (unsigned int)flp_msg);
    memcpy(  ((char*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T) );

    //mtk_flp_dump_locations( ((char*)flp_msg)+sizeof(MTK_FLP_MSG_T) );
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

    FLPD("PseudoDiagRun:%d", PseudoDiagRun);
    //if( PseudoDiagRun ){
    //    PseudoDiagReport();
    //}
    }
    #endif
}

#else  //#ifdef K2_PLATFORM
static void PseudoDiagAPReport(void) {
    char buf[1024];
    MTK_FLP_MSG_T   *flp_msg;

    sprintf(buf, "WIFI_AP_NTF=SSID::AABBCCDD\tLNG::%f\tLAT::%f\tALT::0\nSSID::AABBCCDE\tLNG::%f\tLAT::%f\tALT::0\n",\
                (OrigX - PseudoR), (OrigY-PseudoR), (OrigX + PseudoR), (OrigY+PseudoR));
    FLP_TRC("WIFI_AP_NTF=SSID::AABBCCDD\tLNG::%f\tLAT::%f\tALT::0\nSSID::AABBCCDE\tLNG::%f\tLAT::%f\tALT::0\n",\
                (OrigX - PseudoR), (OrigY-PseudoR), (OrigX + PseudoR), (OrigY+PseudoR));
    flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(buf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(buf);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), buf, strlen(buf));
    mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

    sprintf(buf, "BT_AP_NTF=MAC::01-02-03-04-05-06\tLNG::%f\tLAT::%f\tALT::0\tDIST:3\tRSSI:33\nMAC::01-02-03-04-05-16\tLNG::%f\tLAT::%f\tALT::0\tDIST:5\tRSSI:55\n", \
                (OrigX + PseudoR), (OrigY-PseudoR), (OrigX - PseudoR), (OrigY+PseudoR));
    FLP_TRC("BT_AP_NTF=MAC::01-02-03-04-05-06\tLNG::%f\tLAT::%f\tALT::0\tDIST:3\tRSSI:33\nMAC::01-02-03-04-05-16\tLNG::%f\tLAT::%f\tALT::0\tDIST:5\tRSSI:55\n", \
                (OrigX + PseudoR), (OrigY-PseudoR), (OrigX - PseudoR), (OrigY+PseudoR));
    flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(buf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(buf);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), buf, strlen(buf));
    mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

}

static void PseudoDiagReport(void) {
    char buf[1024];
    MTK_FLP_MSG_T   *flp_msg;

    sprintf(buf, "FLP_NTF=Lng::%.7f LAT::%.7f ALT:0 \n", curX, curY);
    FLP_TRC("FLP_NTF=Lng::%.7f LAT::%.7f ALT:0 \n", curX, curY);
    flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(buf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(buf);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), buf, strlen(buf));
    mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
    if(AP_Reported==0){
        AP_Reported = 1;
        PseudoDiagAPReport();
    }
}


/************************************************************************/
//  Receiver location/meassurment from GNSS HAL and report back to DC
/************************************************************************/
extern INT32 mtk_flp_offload_check_msg(void);
void mtk_flp_dc_pseudo_main(void) {
    MTK_FLP_MSG_T   *flp_msg= NULL, *flp_msg1=NULL;
    MTK_FLP_LOCATION_T  loc;
    char localbuf[512],localbuf1[512];

    FLP_TRC("mtk_flp_dc_offload_main running.....\n");

    if(PseudoRun == 1) {
        GetPseudoLocation(&loc);

        if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY)) {
            sprintf(localbuf,"FLP_NTF=LNG:%f LAT:%f ALT:%f \n", loc.longitude, loc.latitude, loc.altitude);
            snprintf(localbuf1,(sizeof(localbuf)+1),"FLP_NTF=LNG:%f LAT:%f ALT:%f \n", loc.longitude, loc.latitude, loc.altitude);
            flp_msg1 = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf1));
            flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
            flp_msg1->length = strlen(localbuf1);
            memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), localbuf1,strlen(localbuf1));
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);

            mtk_flp_offload_check_msg();
        }
        #if 0
        flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_LOCATION_T));
        flp_msg->type = MTK_FLP_MSG_HAL_REPORT_LOC_NTF;
        flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
        memcpy(  ((char*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T) );
        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg);
        FLP_TRC("pseudo loc: %lf, %lf, %f", loc.latitude, loc.longitude, loc.accuracy);
        FLP_TRC("FLP_NTF=LNG:%f LAT:%f ALT:%f ", loc.longitude, loc.latitude, loc.altitude);
        mtk_flp_offload_check_msg();
        #endif
    }
}

#endif

void mtk_flp_dc_pseudo_process_diag(MTK_FLP_MSG_T *prmsg) {
    char *ptr;

    if(prmsg == NULL)
        return;

    ptr= (char*)prmsg+sizeof(MTK_FLP_MSG_T);
    #ifdef K2_PLATFORM
    FLP_TRC("DIAG:%s", ptr);
    #else
    FLPD("DIAG:%s", ptr);
    #endif
    if(strncmp(ptr, "ENA_SUDO_LOC", 12) == 0) {
        if(ptr[13] == '1') {
            #ifdef K2_PLATFORM
            FLP_TRC("Enable Pseudo Location!!!");
            mtk_flp_mcu_timer_start(FLP_TIMER_ID_PSEUDO,PSEUDO_TIMER);
            //mtk_flp_mcu_pseudo_timer_start(PSEUDO_TIMER);
            #else
            FLPD("Enable Pseudo Location!!!");
            mtk_flp_sys_timer_start(FLP_TIMER_ID_PSEUDO,PSEUDO_TIMER, (ppCallBck_t)mtk_flp_host_timer_expire, NULL);
            #endif
            PseudoRun = 1;
            PseudoDiagRun = 1;
        } else {
            #ifdef K2_PLATFORM
            FLP_TRC("Disable Pseudo Location!!!");
            mtk_flp_mcu_timer_stop(FLP_TIMER_ID_PSEUDO);
            //mtk_flp_mcu_pseudo_timer_stop();
            #else
            FLPD("Disable Pseudo Location!!!");
            mtk_flp_sys_timer_stop(FLP_TIMER_ID_PSEUDO);
            #endif
            PseudoRun = 0;
            PseudoDiagRun = 0;
            AP_Reported = 0;
        }
    }

}

/************************************************************************/
//  Process msgs from DC and send to GNSS HAL
/************************************************************************/
int mtk_flp_dc_pseudo_process(MTK_FLP_MSG_T *prmsg) {
    //MTK_FLP_MSG_T   *flp_msg;
    //MTK_FLP_BATCH_OPTION_T *option;
    #ifdef K2_PLATFORM
    FLP_TRC("OFFLOAD Proc msg(%x) type:0x%02x", (unsigned int)prmsg, prmsg->type);
    #else
    FLPD("OFFLOAD Proc msg(%x) type:0x%02x", (unsigned int)prmsg, prmsg->type);
    #endif
    //option = (UINT8*)flp_msg+sizeof(MTK_FLP_MSG_T);

    switch(prmsg->type) {
    case MTK_FLP_MSG_DC_DIAG_INJECT_DATA_NTF:
        mtk_flp_dc_pseudo_process_diag(prmsg);
        break;
    default:
        break;
    }
    return 0;
}
