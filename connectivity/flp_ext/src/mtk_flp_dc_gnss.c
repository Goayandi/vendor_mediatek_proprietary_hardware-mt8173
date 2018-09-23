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
#include "mtk_flp_dc.h"

#ifndef K2_PLATFORM
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#else
#include "mtk_flp_connsys_sys.h"
#include "mtk_gps_bora_flp.h"
#endif


//#include <android/log.h>

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FLP_DC_GNSS"

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
//  Structures and enums
//*****************************************
/* Message from FLP HAL to GNSS HAL */
enum{
    /* from FLP to GNSS */
    CMD_FLP_START_GNSS_REQ = 0,    //no payload
    CMD_FLP_STOP_GNSS_REQ,         //no payload
    CMD_FLP_INJECT_REF_LOC,        //FlpLocation, refer to fused_location.h
    CMD_FLP_REF_LOC_REQ,           //flags
    CMD_FLP_SET_ID_REQ,            //flags
    CMD_FLP_SET_GEOFENCE_PROP,     //buff_get_binary
    CMD_FLP_START_ALWAYS_LOCATE_REQ, //no payload
    CMD_FLP_STOP_ALWAYS_LOCATE_REQ,  //no payload

    CMD_FLP_SCREEN_ON_NTF,           //no payload
    CMD_FLP_SCREEN_OFF_NTF,          //no payload

    /* from GNSS to FLP */
    CMD_FLP_START_GNSS_DONE = 50,  //no payload
    CMD_FLP_START_GNSS_FAILED,     //no payload
    CMD_FLP_STOP_GNSS_DONE,        //no payload
    CMD_FLP_STOP_GNSS_FAILED,      //no payload
    CMD_FLP_REPORT_REF_LOC,        //AGpsRefLocationMac, refer to gps.h
    CMD_FLP_REPORT_SET_ID,         //AGpsRefLocationCellID, refer to gps.h
    CMD_FLP_INJECT_REF_LOC_DONE,   //no payload
    CMD_FLP_INJECT_REF_LOC_FAILED, //no payload
    CMD_FLP_REPORT_GNSS_LOC,       //GpsLocation, GpsSvStatus
    CMD_FLP_GET_GEOFENCE_PROP,     //buff_get_binary
    CMD_FLP_START_ALWAYS_LOCATE_DONE,    //no payload
    CMD_FLP_START_ALWAYS_LOCATE_FAILED,  //no payload
    CMD_FLP_STOP_ALWAYS_LOCATE_DONE,     //no payload
    CMD_FLP_STOP_ALWAYS_LOCATE_FAILED,   //no payload
};

/** Represents SV information. */
typedef struct {
    /** set to sizeof(GpsSvInfo) */
    size_t          size;
    /** Pseudo-random number for the SV. */
    int     prn;
    /** Signal to noise ratio. */
    float   snr;
    /** Elevation of SV in degrees. */
    float   elevation;
    /** Azimuth of SV in degrees. */
    float   azimuth;
} GpsSvInfo;

#define GPS_MAX_SVS 256
typedef struct {
    /** set to sizeof(GpsSvStatus) */
    size_t          size;

    /** Number of SVs currently visible. */
    int         num_svs;

    /** Contains an array of SV information. */
    GpsSvInfo   sv_list[GPS_MAX_SVS];

    /** Represents a bit mask indicating which SVs
     * have ephemeris data.
     */
    uint32_t    ephemeris_mask[8];

    /** Represents a bit mask indicating which SVs
     * have almanac data.
     */
    uint32_t    almanac_mask[8];

    /**
     * Represents a bit mask indicating which SVs
     * were used for computing the most recent position fix.
     */
    uint32_t    used_in_fix_mask[8];
} MTK_FLP_GPS_SV_STATUS_T;

/** Represents a location. */
typedef struct {
    /** set to sizeof(GpsLocation) */
    size_t          size;
    /** Contains GpsLocationFlags bits. */
    uint16_t        flags;
    /** Represents latitude in degrees. */
    double          latitude;
    /** Represents longitude in degrees. */
    double          longitude;
    /** Represents altitude in meters above the WGS 84 reference
     * ellipsoid. */
    double          altitude;
    /** Represents speed in meters per second. */
    float           speed;
    /** Represents heading in degrees. */
    float           bearing;
    /** Represents expected accuracy in meters. */
    float           accuracy;
    /** Timestamp for the location fix. */
    MTK_FLP_UTC_TIME      timestamp;
} MtkGpsLocation;

typedef struct
{
    MtkGpsLocation              loc;
    MTK_FLP_GPS_SV_STATUS_T     svstatus;
}MTK_FLP_GNSS_PAYLOAD_T;

typedef struct
{
    UINT32 type;
    UINT32 length;
    union{
        UINT8                   buf[512];
        MTK_FLP_GNSS_PAYLOAD_T  payload;
    }u;
} MTK_FLP_GNSS_MSG_T;

#define RMS295_CONVERSION 1.7  //conversion of 1sigma accuracy to 95% accuracy http://en.wikipedia.org/wiki/Circular_error_probable

//*****************************************
// Function Declaration
//*****************************************
extern INT32 mtk_flp_lowpower_query_mode ();
void mtk_flp_dc_gnss_main(MTK_FLP_MSG_T *prmsg);
int mtk_flp_dc_gnss_process(MTK_FLP_MSG_T *prmsg);
static int mtk_flp_dc_gnss_gpsloc2flploc(MtkGpsLocation* gpsloc, MTK_FLP_LOCATION_T *flploc);

#ifndef K2_PLATFORM
extern INT32 mtk_flp_dc_offload_process(MTK_FLP_MSG_T *prmsg);
#endif

//*****************************************
// Report thread function ball backs
//*****************************************
#ifndef K2_PLATFORM
#define TURN_ON_DC_GNSS    1
#else
#define TURN_ON_DC_GNSS    0
#endif

#if (TURN_ON_DC_GNSS==1 )
DcReportThread    Report_thrd_GNSS = NULL;  //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_GNSS = (DcProcessFunction)mtk_flp_dc_gnss_process;  //need to set to NULL for not activating this location source.
#else
DcReportThread    Report_thrd_GNSS = NULL;                      //(DcReportThread)mtk_flp_dc_gnss_main;          //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_GNSS = NULL;                  //(DcProcessFunction)mtk_flp_dc_gnss_process;   //need to set to NULL for not activating this location source.
#endif


//****************************************
//  buff= [type][length][----payload-----]
//      type = MTK_FLP_GNSS_MSG_TYPE_E
//      length = legnth of payload.  Ex. for CMD_FLP_REPORT_GNSS_LOC it is sizeof(GnssMessageStruct)
//*****************************************

#ifndef K2_PLATFORM
void mtk_flp_dc_gnss_main(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_MSG_T   *flp_msg = NULL , *flp_msg1 = NULL;
    MTK_FLP_GNSS_MSG_T *msg;
    MTK_FLP_LOCATION_T  loc;
    MtkGpsLocation  *test_loc;
    char localbuf[512];

    FLPD("DC GNSS Main starts....");

    if((prmsg == NULL)) {
        FLPD("mtk_flp_dc_gnss_main, recv prmsg is null pointer\r\n");
        return;
    }

    msg = (MTK_FLP_GNSS_MSG_T *)prmsg;

    switch( msg->type ) {
        case CMD_FLP_START_GNSS_DONE:
        case CMD_FLP_START_GNSS_FAILED:
        case CMD_FLP_STOP_GNSS_DONE:
        case CMD_FLP_STOP_GNSS_FAILED:
        case CMD_FLP_INJECT_REF_LOC_DONE:
        case CMD_FLP_INJECT_REF_LOC_FAILED:
            FLPD("GNSS Result:%d", msg->type);
            break;
        case CMD_FLP_REPORT_GNSS_LOC:
            FLPD("GNSS Report loc len:%d",sizeof(MTK_FLP_LOCATION_T));
            FLPD("GNSS size: %d %d %d %d\n\r", sizeof(MTK_FLP_GNSS_MSG_T), sizeof(MTK_FLP_GNSS_PAYLOAD_T), sizeof(MtkGpsLocation), sizeof(MTK_FLP_GPS_SV_STATUS_T));
            test_loc = (MtkGpsLocation *)(((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T));
            FLPD("GNSS LOC DBG: %f,%lf,%lf,%d",test_loc->accuracy,test_loc->latitude,test_loc->longitude,test_loc->size);

            if(mtk_flp_dc_gnss_gpsloc2flploc((MtkGpsLocation *)(((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)), &loc) != MTK_FLP_SUCCESS) {
                FLPD("Incorrect data received");
                break;
            }
            if(loc.flags == 0) {
                FLPD("Location no fix");
                break;
            }
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T) );
            flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
            flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T) );
            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

            if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                //send DIAG location
                sprintf(localbuf,"GNSS_NTF=LNG:%f LAT:%f ALT:%f", loc.longitude, loc.latitude, loc.altitude);
                FLPD("GNSS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",loc.longitude, loc.latitude, loc.altitude, loc.size,loc.sources_used);
                flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                flp_msg1->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
                flp_msg1->length = strlen(localbuf);
                memcpy( ((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf));
                mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg1);
            }
            break;
        case CMD_FLP_GET_GEOFENCE_PROP:
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + msg->length);
            flp_msg->type = MTK_FLP_MSG_DC_GEO_PASSTHRU_RECV_NTF;
            flp_msg->length = msg->length;
            memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)flp_msg) + sizeof(MTK_FLP_GNSS_MSG_T), msg->length);
            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
            break;
        default:
            FLPD("Unknown GNSS message type: %x", msg->type);
            break;

        }

}

/************************************************************************/
//  Process msgs from DC and send to GNSS HAL
/************************************************************************/
int mtk_flp_dc_gnss_process(MTK_FLP_MSG_T *prmsg) {
    //MTK_FLP_GNSS_MSG_T  msg;
    MTK_FLP_MSG_T *ptr;
    MTK_FLP_BATCH_OPTION_T *option;
    UINT8 u8APScrStatus, u8AlwaysLocate_mode;

    option = (MTK_FLP_BATCH_OPTION_T *)((UINT8*)prmsg+sizeof(MTK_FLP_MSG_T));


    FLPD("GNSS Proc msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch(prmsg->type) {
    case MTK_FLP_MSG_DC_START_CMD:
        if(option->sources_to_use & FLP_TECH_MASK_GNSS) {
            FLPD("START GNSS Batching");
            mtk_flp_dc_offload_process(prmsg);
        }
        break;
    case MTK_FLP_MSG_DC_STOP_CMD:
        FLPD("Stop GNSS Batching");
        mtk_flp_dc_offload_process(prmsg);
        break;
    case MTK_FLP_MSG_DC_DIAG_INJECT_DATA_NTF:
        FLPD("GNSS DIAG INJECT");
        break;
    case MTK_FLP_MSG_DC_GEO_PASSTHRU_SEND_NTF:
        ptr = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+prmsg->length);
        ptr->type = CMD_FLP_SET_GEOFENCE_PROP;
        ptr->length = prmsg->length;
        memcpy( (INT8*)ptr+sizeof(MTK_FLP_GNSS_MSG_T), ((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), prmsg->length);
        mtk_flp_dc_offload_process(ptr);
        mtk_flp_sys_mem_free(ptr);
        break;
    case MTK_FLP_MSG_CONN_SCREEN_STATUS:
        memcpy(&u8APScrStatus,(UINT8*)prmsg+sizeof(MTK_FLP_MSG_T),sizeof(UINT8));
        FLPD("ap scrn status: %d",u8APScrStatus);
        ptr = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T));
        if(u8APScrStatus == EARLYSUSPEND_ON) {
            ptr->type = CMD_FLP_SCREEN_ON_NTF;
        } else if(u8APScrStatus == EARLYSUSPEND_MEM) {
            ptr->type = CMD_FLP_SCREEN_OFF_NTF;
        }
        ptr->length = 0;
        mtk_flp_dc_offload_process(ptr);
        mtk_flp_sys_mem_free(ptr);
        break;
    case MTK_FLP_MSG_DC_RAW_SAP_NTF:
        ptr = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T));
        memcpy(&u8AlwaysLocate_mode,(UINT8*)prmsg+sizeof(MTK_FLP_MSG_T),sizeof(UINT8));
        FLPD("DC GNSS alwayslocate=%d",u8AlwaysLocate_mode);
        if(u8AlwaysLocate_mode == 1) {
            ptr->type = CMD_FLP_START_ALWAYS_LOCATE_REQ;
        } else if(u8AlwaysLocate_mode == 0) {
            ptr->type = CMD_FLP_STOP_ALWAYS_LOCATE_REQ;
        }
        ptr->length = 0;
        mtk_flp_dc_offload_process(ptr);
        mtk_flp_sys_mem_free(ptr);
        break;
    default:
        FLPD("Unknown GNSS message to send");
        break;
    }

    return 0;
}

#else //ifdef K2_PLATFORM for offload function

volatile UINT32 AR_counter;
/************************************************************************/
//  Receiver location/meassurment from GNSS HAL and report back to DC
/************************************************************************/
void mtk_flp_dc_gnss_main(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_MSG_T   *flp_msg = NULL , *flp_msg1 = NULL;
    MTK_FLP_GNSS_MSG_T *msg;
    MTK_FLP_LOCATION_T  loc;
    MtkGpsLocation  *test_loc;
    char localbuf[512];

    FLP_TRC("DC GNSS Main starts....");

    if((prmsg == NULL) || (prmsg->length < 0)) {
        FLP_TRC("mtk_flp_dc_gnss_main, recv prmsg is null pointer\r\n");
        return;
    }

    msg = (MTK_FLP_GNSS_MSG_T *)prmsg;

    switch(msg->type) {
        case CMD_FLP_START_GNSS_DONE:
        case CMD_FLP_START_GNSS_FAILED:
        case CMD_FLP_STOP_GNSS_DONE:
        case CMD_FLP_STOP_GNSS_FAILED:
        case CMD_FLP_INJECT_REF_LOC_DONE:
        case CMD_FLP_INJECT_REF_LOC_FAILED:
            FLP_TRC("GNSS Result:%d", msg->type);
            break;
        case CMD_FLP_REPORT_GNSS_LOC:
            FLP_TRC("GNSS Report loc len:%d",sizeof(MTK_FLP_LOCATION_T));
            FLP_TRC("GNSS size: %d %d %d %d\n\r", sizeof(MTK_FLP_GNSS_MSG_T), sizeof(MTK_FLP_GNSS_PAYLOAD_T), sizeof(MtkGpsLocation), sizeof(MTK_FLP_GPS_SV_STATUS_T));
            test_loc = (MtkGpsLocation *)(((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T));
            FLP_TRC("GNSS LOC DBG: %f,%lf,%lf,%d",test_loc->accuracy,test_loc->latitude,test_loc->longitude,test_loc->size);

            if(mtk_flp_dc_gnss_gpsloc2flploc( (MtkGpsLocation *)(((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)), &loc) != MTK_FLP_SUCCESS) {
                FLP_TRC("Incorrect data received");
                break;
            }
            if(loc.flags == 0) {
                FLP_TRC("Location no fix");
                break;
            }
            flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T));
            flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
            flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc, sizeof(MTK_FLP_LOCATION_T));
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

            if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                //send DIAG location
                sprintf(localbuf,"GNSS_NTF=LNG:%f LAT:%f ALT:%f", loc.longitude, loc.latitude, loc.altitude);
                FLP_TRC("GNSS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",loc.longitude, loc.latitude, loc.altitude, loc.size,loc.sources_used);
                flp_msg1 = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
                flp_msg1->length = strlen(localbuf);
                memcpy( ((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf));
                mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);
            }
            break;
        case CMD_FLP_GET_GEOFENCE_PROP:
            flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) + msg->length);
            flp_msg->type = MTK_FLP_MSG_DC_GEO_PASSTHRU_RECV_NTF;
            flp_msg->length = msg->length;
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)flp_msg)+sizeof(MTK_FLP_GNSS_MSG_T), msg->length);
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
            break;
        default:
            FLP_TRC("Unknown GNSS message type: %x", msg->type);
            break;
        }
}

/************************************************************************/
//  Process msgs from DC and send to GNSS HAL
/************************************************************************/
int mtk_flp_dc_gnss_process(MTK_FLP_MSG_T *prmsg) {
    //MTK_FLP_GNSS_MSG_T  msg;
    MTK_FLP_MSG_T *ptr;
    MTK_FLP_BATCH_OPTION_T *option;
    UINT8 u8APScrStatus, u8AlwaysLocate_mode;

    option = (MTK_FLP_BATCH_OPTION_T *)((UINT8*)prmsg+sizeof(MTK_FLP_MSG_T));


    FLP_TRC("GNSS Proc msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch(prmsg->type) {
    case MTK_FLP_MSG_DC_START_CMD:
        //FLP_TRC("Option:%x", option);
        if(option->sources_to_use & FLP_TECH_MASK_GNSS) {
            FLP_TRC("START GNSS Batching");
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T)+prmsg->length);
            ptr->type = CMD_FLP_START_GNSS_REQ;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_GNSS,ptr);
        }
        break;
    case MTK_FLP_MSG_DC_STOP_CMD:
        {
            FLP_TRC("Stop GNSS Batching");
            ptr = mtk_flp_mcu_mem_alloc(sizeof(MTK_FLP_MSG_T)+prmsg->length);
            ptr->type = CMD_FLP_STOP_GNSS_REQ;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_GNSS, ptr);
        }
        break;
    case MTK_FLP_MSG_DC_DIAG_INJECT_DATA_NTF:
        FLP_TRC("GNSS DIAG INJECT");
        break;
    case MTK_FLP_MSG_DC_GEO_PASSTHRU_SEND_NTF:
        ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T)+prmsg->length);
        ptr->type = CMD_FLP_SET_GEOFENCE_PROP;
        ptr->length = prmsg->length;
        memcpy( (INT8*)ptr+sizeof(MTK_FLP_GNSS_MSG_T), ((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), prmsg->length);
        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL,ptr);
        break;
    case MTK_FLP_MSG_CONN_SCREEN_STATUS:
        memcpy(&u8APScrStatus,(UINT8*)prmsg+sizeof(MTK_FLP_MSG_T),sizeof(UINT8));
        FLP_TRC("ap scrn status: %d",u8APScrStatus);
        ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
        if(u8APScrStatus == EARLYSUSPEND_ON) {
            ptr->type = CMD_FLP_SCREEN_ON_NTF;
        } else if(u8APScrStatus == EARLYSUSPEND_MEM) {
            ptr->type = CMD_FLP_SCREEN_OFF_NTF;
        }
        ptr->length = 0;
        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_GNSS,ptr);
        break;
    case MTK_FLP_MSG_DC_RAW_SAP_NTF:
        ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
        memcpy(&u8AlwaysLocate_mode,(UINT8*)prmsg+sizeof(MTK_FLP_MSG_T),sizeof(UINT8));
        FLP_TRC("DC GNSS alwayslocate=%d",u8AlwaysLocate_mode);
        if(u8AlwaysLocate_mode == 1) {
            ptr->type = CMD_FLP_START_ALWAYS_LOCATE_REQ;
        } else if(u8AlwaysLocate_mode == 0) {
            ptr->type = CMD_FLP_STOP_ALWAYS_LOCATE_REQ;
        }
        ptr->length = 0;
        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_GNSS,ptr);
        break;
    default:
        FLP_TRC("Unknown GNSS message to send");
        break;
    }
    return 0;
}

#endif //#ifndef K2_PLATFORM

static int mtk_flp_dc_gnss_gpsloc2flploc(MtkGpsLocation* gpsloc, MTK_FLP_LOCATION_T *flploc) {
    if(gpsloc == NULL || flploc == NULL) {
        return MTK_FLP_ERROR;
    }

    memcpy(flploc, gpsloc, sizeof(MtkGpsLocation));
    flploc->size = sizeof(MTK_FLP_LOCATION_T);
    flploc->sources_used = FLP_TECH_MASK_GNSS;
    flploc->accuracy = RMS295_CONVERSION*(gpsloc->accuracy); //convert accuracy from 67% to 95%
    flploc->altitude = gpsloc->altitude;
    flploc->bearing = gpsloc->bearing;
    flploc->flags =gpsloc->flags;
    flploc->latitude = gpsloc->latitude;
    flploc->longitude = gpsloc->longitude;
    flploc->speed = gpsloc->speed;

#ifdef K2_PLATFORM
    flploc->timestamp = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick()/TTICK_CLK);
    FLP_TRC("[DC_GNSS]:dc2gnss: acc:%.4f,alt:%.4f,bearing:%.4f,flag:%d,lat:%.4f,lon:%.4f,spd:%.4f",
        flploc->accuracy,flploc->altitude,flploc->bearing,flploc->flags,flploc->latitude,
        flploc->longitude,flploc->speed);
#else
    flploc->timestamp = mtk_flp_get_timestamp(NULL);
    FLPD("[DC_GNSS]:dc2gnss: acc:%.4f,alt:%.4f,bearing:%.4f,flag:%d,lat:%.4f,lon:%.4f,spd:%.4f",
        flploc->accuracy,flploc->altitude,flploc->bearing,flploc->flags,flploc->latitude,
        flploc->longitude,flploc->speed);
#endif
    if(flploc->accuracy < 0.001) {
      return MTK_FLP_ERROR; //return false if no fix from gps yet
    }

    return MTK_FLP_SUCCESS;
}

