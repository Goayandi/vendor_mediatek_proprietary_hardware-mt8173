/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
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


#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <private/android_filesystem_config.h>


#define  LOG_TAG  "FLP_JNI"
//#include <cutils/xlog.h>
#include <cutils/sockets.h>
#include <cutils/properties.h>

//#include <utils/Log.h> // For Debug

//#include <android/log.h>
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#define MTK_64_PLATFORM

typedef enum
{
    INIT_GEOFENCE,
    ADD_GEOFENCE_AREA,
    PAUSE_GEOFENCE,
    RESUME_GEOFENCE,
    REMOVE_GEOFENCE,
    MODIFY_GEOFENCE,
    RECOVER_GEOFENCE,
    //SET_GEOFENCE_LOC_CB,
    SET_GEOFENCE_PASSTHRU_CB,
    SET_GEOFENCE_CLEAR_CB,
}MTK_COMMAND_T;


#ifdef DEBUG_LOG
#define FLPE(...)       mtk_flp_sys_dbg(MTK_FLP_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define FLPD(...)       mtk_flp_sys_dbg(MTK_FLP_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define FLPW(...)       mtk_flp_sys_dbg(MTK_FLP_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define TRC()

#else
    #define TRC()
    #define FLPE(f, ...)
    #define FLPW(f, ...)
    #define FLPD(...)    ((void)0)
#endif
#define SEM 0
#define NEED_IPC_WITH_CODEC 1


#if (NEED_IPC_WITH_CODEC)
#include <private/android_filesystem_config.h>
#endif

#define SOCKET_RETRY_CNT        10
#define SOCKET_RETRY_INTERVAL   100000      //100ms

/**********************************************************
 *  Global vars                                           *
 **********************************************************/
static FlpCallbacks mtkFlpCallbacks;
static FlpDiagnosticCallbacks mtkFlpDiagCallbacks;
static int  g_ThreadExitJniSocket=FALSE;
static pthread_t    hal_jni_thread_id;
int  g_server_socket_fd;

static pid_t g_flp_child_pid = (-1);
sem_t kill_process_sem;

#define FLP_OPTION_MAX  256
static FlpBatchOptions  *gFlpOptions[FLP_OPTION_MAX];
static FlpBatchOptions  gFusedOptions;
static int FlpCheckTimeoutSec = 0;
static int GeoCheckTimeoutSec = 0;
static int FlpCheckCnnAvail = 0;
static int gFlpFirstBatch = 1;

#define UNICODE_BUF_LEN 2048
char gUnicodebuf[UNICODE_BUF_LEN];

extern const FlpGeofencingInterface mtkFlpGeofenceInterface;
extern void mtk_flp_hal_geofence_recover_geofence();
extern void mtk_flp_hal_geofence_clear_geofence();
extern int set_buff_transition_fence(const int32_t fence_id, const int transition);
extern int mtk_geo_is_start();

MTK_FLP_GEO_LOC_CB      cbGeoLocCallback = NULL;
MTK_FLP_GEO_PASSTHRU_CB cbGeoPassthruCallback = NULL;
MTK_FLP_GEO_CLEAR_CB    cbGeoClearCallback = NULL;

#define MTK_FLP_BATCH_SIZE  30
static FlpLocation  FlpLocBuffer[MTK_FLP_BATCH_SIZE];
static int          FlpLocNum=0;
static int          FlpLocBegin=0;
static int          FlpLocEnd=0;
FlpUtcTime          gReportedTimestamp=0;
static FlpLocation  FlpLocRepBuf[MTK_FLP_BATCH_SIZE];
static int          FlpPseudoRunning=0;

static int FlpCapability = 0;
static int GeoCapability = 0;
static int FlpLastStatus = 2;

MTK_FLP_RECOVERY_INFO_T gSysInfo = {FLP_AP_MODE, 0};

typedef enum
{
    GEOFENCE_ADD_CALLBACK,
    GEOFENCE_REMOVE_CALLBACK,
    GEOFENCE_PAUSE_CALLBACK,
    GEOFENCE_RESUME_CALLBACK
}GEOFENCE_CALLBACK_T;

typedef struct mtk_geofence_callback
{
    int32_t cb_id;
    int32_t geofence_id;
    int32_t result;
}MTK_GEOFENCE_CALLBACK_T;

extern FlpGeofenceCallbacks *mtk_geofence_callbacks;


//static void mtk_flp_query_options(char *data, int len);
//static void mtk_flp_loopback_cmd(char *data, int len);
static FlpBatchOptions* mtk_flp_get_options(int id);
static void mtk_flp_dump_options(FlpBatchOptions* options);
static void mtk_flp_set_options(int id, FlpBatchOptions* options);
static void mtk_flp_update_options();
static void mtk_flp_pdr_diag_proc(char *diag_buff);
void mtk_flp_set_flp_check_timer(int timeout_ms);
void mtk_flp_set_geo_check_timer(int timeout_ms);
void mtk_flp_update_check_timer();
void mtk_flp_check_cnn_timer_expire();


static void FlpLocRingInit(void) {
    FlpLocNum = 0;
    FlpLocBegin = 0;
    FlpLocEnd = 0;
}

static int FlpLocRingIsFull(void) {
    return (FlpLocNum==MTK_FLP_BATCH_SIZE);
}

static int FlpLocRingIsEmpty(void) {
    return (FlpLocNum==0);
}

//Add one location to the Ring buffer.  Remove oldest when full.
static void FlpLocRingAdd(FlpLocation *p) {
    if(p == NULL) {
        FLPD("Addning NULL");
        return;
    }
    mtk_flp_sys_dbg_dump_loc(p);
    memcpy(&FlpLocBuffer[FlpLocBegin], p, sizeof(FlpLocation));

    FLPD("LocRing Add to %d(%x)", FlpLocBegin, &FlpLocBuffer[FlpLocBegin]);

    FlpLocBegin++;
    if(FlpLocBegin == MTK_FLP_BATCH_SIZE) {
        FlpLocBegin = 0;
    }

    if( FlpLocNum < MTK_FLP_BATCH_SIZE) {       //if already not full yet.
        FlpLocNum++;
    } else {                                       //if full. drop oldest
        FlpLocEnd++;
        if(FlpLocEnd == MTK_FLP_BATCH_SIZE) {
            FlpLocEnd = 0;
        }
    }
}

//Remove a location from tail.
static int FlpLocRingRemove(FlpLocation **p) {
    if(FlpLocRingIsEmpty()) {
        FLPD("Ring empty!!!");
        return MTK_FLP_ERROR;
    }
    //memcpy(p, &FlpLocBuffer[FlpLocEnd], sizeof(FlpLocation) );
    *p = &FlpLocBuffer[FlpLocEnd];

    FlpLocNum--;
    FlpLocEnd++;
    if(FlpLocEnd == MTK_FLP_BATCH_SIZE) {
        FlpLocEnd = 0;
    }

    return MTK_FLP_SUCCESS;
}

//return the number of locations.
static int FlpLocRingRemoveN(FlpLocation **p, int N) {
    int idx=0;
    if(N<=0) {
        FLPD("Wrong N");
        return 0;
    }

    while(!FlpLocRingIsEmpty()) {
        FlpLocRingRemove(&p[idx]);
        idx++;
        if(idx == N) {
            break;
        }
    }
    return idx;
}

//get the value of last N location without removing it.
static int FlpLocRingPeekLastN(FlpLocation **p, int N) {
    int b,e,num,cur_num=0;

    b = FlpLocBegin;
    e = FlpLocEnd;
    num = FlpLocNum;

    while(cur_num < N && num > 0) {
        b--;
        if(b < 0) {
            b = MTK_FLP_BATCH_SIZE-1;
        }
        mtk_flp_sys_dbg_dump_loc(&FlpLocBuffer[b]);

        //memcpy( p[cur_num], &FlpLocBuffer[b], sizeof(FlpLocation));
        p[cur_num] = &FlpLocBuffer[b];  //copy pointer only. not content.
        FLPD("LocRing Peek to %d(%x)", b, &FlpLocBuffer[b]);
        num--;
        cur_num++;
    }
    return cur_num;
}


/*******************************************
  unicode needed to have len*2 bytes.
********************************************/
int Ascii2Unicode(char* ascii, char * unicode, int len) {
    int i;
    for(i = 0; i < len; i++) {
        unicode[i*2]=ascii[i];
        unicode[i*2+1]=0;
    }
    return len;
}


/*******************************************
  ONLY support ASCII chars!!
********************************************/
int Unicode2Ascii(char* unicode, char * ascii, int len) {
    int i;
    for(i = 0; i < len; i++) {
        ascii[i] = unicode[i*2];
    }
    return len;
}

INT32 mtk_flp_geo_set_loc_cb( MTK_FLP_GEO_LOC_CB *cb) {
    cbGeoLocCallback = *cb;
    return 0;
}

INT32 mtk_flp_geo_set_passthru_cb(MTK_FLP_GEO_PASSTHRU_CB *cb) {
    cbGeoPassthruCallback = *cb;
    return 0;
}

INT32 mtk_flp_geo_set_clear_cb( MTK_FLP_GEO_CLEAR_CB *cb) {
    cbGeoClearCallback = *cb;
    return 0;
}

INT32 mtk_flp_geo_pass_thru(void *ptr, int size) {
    MTK_FLP_MSG_T *flp_msg;

    FLPD("Geofence pass thru");
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+ size);
    flp_msg->type = MTK_FLP_MSG_DC_GEO_PASSTHRU_SEND_NTF;
    flp_msg->length = size;
    if(size) {
        memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)ptr)+sizeof(MTK_FLP_MSG_T), size);
    }
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
    return 0;
}



#define KILL_ZOMBIE_PROCESS

#ifdef KILL_ZOMBIE_PROCESS
#include <signal.h>
#include <sys/wait.h>

void clean_up_child_process (int signal_number) {
   int status;

   FLPD("clean_up_child_process...(sig_num: %d)\n", signal_number);

   /* Clean up the child process. */
   wait (&status);

#ifdef KILL_ZOMBIE_PROCESS
   FLPD("post sem for kill_process_sem\n");
   sem_post(&kill_process_sem);
#endif

   FLPD("clean_up_child_process status: %d\n", status);
}
#endif
static int android_flpdaemon_deinit(void) {
   int ret = 0;

   FLPD("android_flpdaemon_deinit...\n");

   ret = kill(g_flp_child_pid, SIGTERM);
   if (ret < 0) { // error case
      FLPE("kill flp(pid:%d) fail: %d, (%s)\n", g_flp_child_pid, errno, strerror(errno));
      return FALSE;
   }
   else if (ret > 0) { // unreasonable return value
      FLPE("kill flp(pid:%d) fail, unreasonalbe ret: %d\n", g_flp_child_pid, ret);
      return FALSE;
   }
   else if (ret == 0) { // On success, zero is returned
      FLPD("kill flp(pid:%d) success\n", g_flp_child_pid);
      g_flp_child_pid = -1;
   }
   FLPD("android_flpdaemon_deinit ok\n");

   return TRUE;
}

#if 1
/**********************************************************
 *  Dummy thread for testing                              *
 **********************************************************/
static int          dummy_thread_running = 0;
static int          dummy_thread_run_once = 0;
static int          dummy_thread_exit = 0;
static pthread_t    dummy_thread_id;
#define             dummy_loc_size  5
static FlpLocation  dummy_loc_arr[dummy_loc_size];
static FlpLocation* dummy_loc[dummy_loc_size];
static char         dummy_inject_in_data_len=0;
static char         dummy_inject_in_data[1024];
static char         dummy_inject_out_data_len=0;
static char         dummy_inject_out_data[1024];
static int          dummy_inject_stat=0;    //0: idle, 1:enabled, 2: running, 3:loopback, 4:Query

static void mtk_flp_dump_locations(int n, FlpLocation** locs) {
    int i;
    FLPD("Duump %d Locations:", n);
    for(i = 0; i < n; i++){
        mtk_flp_sys_dbg_dump_loc(locs[i]);
#if 0
        FLPD("Loc[%d] s:%d, f:%x, acc:%f, alt:%f, bea:%f, lat:%f, lng:%f, spd:%f, tm:%lld",
        i, options[i]->size, options[i]->flags, options[i]->accuracy, options[i]->altitude,
        options[i]->bearing, options[i]->latitude, options[i]->longitude, options[i]->speed,
        options[i]->timestamp);
#endif
    }

}

void dummy_report_location() {
    if(mtkFlpCallbacks.location_cb) {
        mtk_flp_dump_locations(1, &dummy_loc[0]);
        mtkFlpCallbacks.location_cb(1, &dummy_loc[0]);
    } else {
        FLPD("dummy_report_location: CB not set!!!");
    }
}

void dummy_report_diag() {
    char unicodebuf[2048];

    if(dummy_inject_stat == 0) {
        return;
    } else if(dummy_inject_stat == 1) {
        sprintf(dummy_inject_out_data, "ENA_RSP=OK\n");
    } else if(dummy_inject_stat == 2) {
        sprintf(dummy_inject_out_data, "FLP_NTF=Lng::0.324 LAT::12.333 ALT:0 \n");
    } else if(dummy_inject_stat == 3) {
        memset(dummy_inject_out_data, 0, sizeof(dummy_inject_out_data) );
        memcpy(dummy_inject_out_data, dummy_inject_in_data, dummy_inject_in_data_len);
        dummy_inject_out_data[4]='R';
        dummy_inject_out_data[5]='S';
        dummy_inject_out_data[6]='P';
        dummy_inject_out_data_len = dummy_inject_in_data_len;
        dummy_inject_stat = 2;
    } else {
        dummy_inject_stat = 0;
        FLPD("Error dummy_inject_stat\n");
        return;
    }

    dummy_inject_out_data_len = strlen(dummy_inject_out_data);
    if(mtkFlpDiagCallbacks.data_cb ) {
        FLPD("DIAG:%s\n", dummy_inject_out_data);
        Ascii2Unicode(dummy_inject_out_data, unicodebuf, dummy_inject_out_data_len);
        mtkFlpDiagCallbacks.data_cb( unicodebuf, dummy_inject_out_data_len);
    }
}

void* dummy_thread_body(void* arg) {
    TRC();
    UNUSED(arg);
    //Set thead event so that current thread can call other callbacks.
    /*
    if(mtkFlpCallbacks.set_thread_event_cb){
        mtkFlpCallbacks.set_thread_event_cb(ASSOCIATE_JVM);
    }else{
        FLPE("set_thread_event_cb not set!!");
    }
*/
    while(dummy_thread_exit == 0) {
        do {
            usleep(1000000);
        } while(dummy_thread_running==0 && dummy_thread_run_once==0 && dummy_inject_stat==0);

        FLPD("RunOnce:%d Running:%d\n", dummy_thread_run_once, dummy_thread_running);

        /***** thread body *******/
        dummy_report_location();
        dummy_report_diag();
        if(dummy_thread_run_once != 0) {
            dummy_thread_run_once = 0;
        }
    }
    return NULL;
}

void Init_dummy_thread(void) {
    int ret = 0, i;
    dummy_thread_exit = 0;
    dummy_thread_running = 0;

    TRC();
    for(i = 0; i < dummy_loc_size; i++) {
        dummy_loc_arr[i].size = sizeof(FlpLocation);
        dummy_loc_arr[i].flags = 0xf;
        dummy_loc_arr[i].accuracy = 0.0;
        dummy_loc_arr[i].altitude = 0.0;
        dummy_loc_arr[i].bearing = 0.0;
        dummy_loc_arr[i].latitude = 0.0;
        dummy_loc_arr[i].longitude = 0.0;
        dummy_loc_arr[i].speed = 0.0;
        dummy_loc_arr[i].timestamp = (FlpUtcTime)time(NULL);
        dummy_loc[i] = &dummy_loc_arr[i];
    }
    ret = pthread_create(&dummy_thread_id, NULL, dummy_thread_body, NULL);
}
#endif


static void mtk_flp_dump_hex(char *head, char *inbuf, int len) {
    char buf[4096];
    int i;
    FLPD("mtk_flp_dump_hex len = %d, 3xlen =%d",len, len*3);
    for(i = 0; i < len; i++) {
        sprintf(&buf[i*3], "%02X ", inbuf[i]);
    }
    buf[len*3] = '\0';
    FLPD("%s: %s",head, buf);
    FLPD("%s",inbuf);
}

static void mtk_flp_request_loc(int req_num) {
    INT32 loc_num = 0;
    FlpLocation *locs[MTK_FLP_BATCH_SIZE];

    loc_num = FlpLocRingPeekLastN(locs, req_num);
    //loc_num = FlpLocRingRemoveN(locs, MTK_FLP_BATCH_SIZE);
    FLPD("request loc, loc_num:%d", loc_num);
    mtk_flp_dump_locations(loc_num, locs);
    mtkFlpCallbacks.location_cb(loc_num, locs );
}

static void mtk_flp_flush_loc() {
    INT32 loc_num = 0;
    FlpLocation *locs[MTK_FLP_BATCH_SIZE];

    loc_num = FlpLocRingPeekLastN(locs, MTK_FLP_BATCH_SIZE);
    FLPD("flush loc, loc_num:%d", loc_num);
    //mtk_flp_dump_locations(loc_num, locs);
    mtkFlpCallbacks.location_cb(loc_num, locs );
    loc_num = FlpLocRingRemoveN(locs, MTK_FLP_BATCH_SIZE);
}

static void mtk_flp_report_status(int status) {
    FLPD("report status: %d", status);
    if(FlpLastStatus != status) {
        FLPD("status change from: %d to %d", FlpLastStatus, status);
        mtkFlpCallbacks.flp_status_cb(status);
        FlpLastStatus = status;
    }
}

static void mtk_flp_report_loc(MTK_FLP_MSG_T *prmsg) {
    INT32 loc_num = 0;
    FlpLocation* ptr = NULL;
    int i;
    FlpLocation *locs[MTK_FLP_BATCH_SIZE];
    FLPD("mtk_flp_report_loc");
    //add locations to ring buffer every time location is reported
    loc_num = prmsg->length/sizeof(MTK_FLP_LOCATION_T);
    if(gSysInfo.sys_mode == FLP_OFFLOAD_MODE) {
        loc_num = prmsg->length/sizeof(MTK_FLP_LOCATION_T);
        for(i = 0; i < loc_num; i++) {
            ptr = (FlpLocation* )((UINT8*)prmsg + sizeof(MTK_FLP_MSG_T) + i*sizeof(MTK_FLP_LOCATION_T));
            FLPD("report loc %d, ptr:%x", i, ptr);
            locs[i] = ptr;
        }
        mtkFlpCallbacks.location_cb(loc_num, locs);
    } else {
        for(i = 0; i < loc_num; i++) {
            ptr = (FlpLocation* )((UINT8*)prmsg + sizeof(MTK_FLP_MSG_T) + i*sizeof(MTK_FLP_LOCATION_T));
            FLPD("report loc 1_%d, ptr:%x", i, ptr);
            FlpLocRingAdd(ptr);
        }

        FLPD("report loc, flag:%x", gFusedOptions.flags);
        if(gFusedOptions.flags & FLP_BATCH_CALLBACK_ON_LOCATION_FIX) {
            FLPD("FLP_BATCH_CALLBACK_ON_LOCATION_FIX");
            FlpLocRingPeekLastN(locs, 1);
            mtkFlpCallbacks.location_cb(1, locs);
        } else if(gFusedOptions.flags & FLP_BATCH_WAKEUP_ON_FIFO_FULL) {
            if(!FlpLocRingIsFull()) {
                FLPD("Location Ring not FULL");
                return;
            }
            loc_num = FlpLocRingPeekLastN(locs, MTK_FLP_BATCH_SIZE);
            //mtk_flp_dump_locations(loc_num, locs);
            FLPD("FLP_BATCH_WAKEUP_ON_FIFO_FULL: %d", loc_num);
            mtkFlpCallbacks.location_cb(loc_num, locs );
            loc_num = FlpLocRingRemoveN( locs, MTK_FLP_BATCH_SIZE);
        }
        //FLPD("Num of loc:%d  prmsg->len=%d sizeof(FLP_Loc)=%d", loc_num, prmsg->length, sizeof(MTK_FLP_LOCATION_T));
    }

}

static void mtk_flp_geofence_callbacks_proc(MTK_FLP_MSG_T *prmsg) {
    MTK_GEOFENCE_CALLBACK_T cbs;

    if(mtk_geofence_callbacks == NULL) {
        FLPD("mtk_flp_geofence_callbacks_proc: NO geofence callbacks assigned");
        return;
    }

    memcpy(&cbs,(char*)prmsg+sizeof(MTK_FLP_MSG_T),sizeof(MTK_GEOFENCE_CALLBACK_T));
    FLPD("geo_cb_proc: id=%d, result=%d",cbs.cb_id,cbs.result);

    switch(cbs.cb_id) {
    case GEOFENCE_ADD_CALLBACK:
        mtk_geofence_callbacks->geofence_add_callback(cbs.geofence_id, cbs.result);
        break;
    case GEOFENCE_REMOVE_CALLBACK:
        mtk_geofence_callbacks->geofence_remove_callback(cbs.geofence_id, cbs.result);
        break;
    case GEOFENCE_PAUSE_CALLBACK:
        mtk_geofence_callbacks->geofence_pause_callback(cbs.geofence_id, cbs.result);
        break;
    case GEOFENCE_RESUME_CALLBACK:
        mtk_geofence_callbacks->geofence_resume_callback(cbs.geofence_id, cbs.result);
        break;
    default:
        FLPD("mtk_flp_geofence_callbacks_proc: Unknown callback id:%d", cbs.cb_id);
        break;
    }

}

#ifdef MTK_64_PLATFORM
void mtk_flp_hal_loc_rearrange(UINT8 *loc_in, FlpLocation *loc_out) {
    UINT8 ratio = 2; //32 to 64-bits
    UINT8 padding_diff = 4;
    UINT32 sizeof_loc_in = sizeof(FlpLocation) - sizeof(size_t)/ratio - padding_diff;

    memset(loc_out, 0, sizeof(FlpLocation));
    loc_out->size = sizeof(FlpLocation);
    memcpy(&loc_out->flags, loc_in + sizeof(size_t)/ratio, sizeof(uint16_t));
    memcpy(&loc_out->latitude, loc_in + sizeof(size_t), sizeof_loc_in - sizeof(size_t));
}
#endif

static void mtk_flp_geofence_transition_callbacks_proc(MTK_FLP_MSG_T *prmsg) {
    uint32_t sources_used;
    int32_t geofence_id, transition;
    FlpLocation location;
    FlpUtcTime timestamp;
    #ifdef MTK_64_PLATFORM
    UINT8 ratio = 2; //32 to 64-bits
    UINT8 padding_diff = 4;
    UINT32 sizeof_loc_in = sizeof(FlpLocation) - sizeof(size_t)/ratio - padding_diff;
    UINT8 loc_in[128] = {0};
    #else
    uint32_t sizeof_loc_in = sizeof(FlpLocation);
    #endif

    if(mtk_geofence_callbacks == NULL) {
        FLPD("mtk_flp_geofence_transition_callbacks_proc: NO geofence callbacks assigned");
        return;
    }
    memcpy(&geofence_id, (((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)), sizeof(int32_t));
    #ifdef MTK_64_PLATFORM
    memcpy(loc_in, (((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)), sizeof_loc_in);
    mtk_flp_hal_loc_rearrange(loc_in, &location);
    #else
    memcpy(&location, (((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)), sizeof_loc_in);
    #endif
    memcpy(&transition, (((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+sizeof_loc_in), sizeof(int32_t));
    memcpy(&timestamp, (((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+sizeof_loc_in+sizeof(int32_t)), sizeof(FlpUtcTime));
    memcpy(&sources_used, (((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+sizeof_loc_in+sizeof(int32_t)+sizeof(FlpUtcTime)), sizeof(uint32_t));
    FLPD("geo transition: id=%ld, transition=%ld, latlon=%lf,%lf",geofence_id,transition,location.latitude,location.longitude);

    mtk_geofence_callbacks->geofence_transition_callback(geofence_id, &location, transition, timestamp, sources_used);
    set_buff_transition_fence(geofence_id, transition);
}

static void mtk_flp_pdr_diag_proc(char *diag_buff) {
    unsigned int i, sum = 0;
    char temp_buff[1000];
    time_t now = time(0);
    sprintf(temp_buff, "TPDR DATA 1 0 %lld %s", (long long)now, diag_buff+5);
    for(i = 0; i < strlen(temp_buff); i++) {
        sum += temp_buff[i];
    }
    sprintf(diag_buff, "%s %d", temp_buff, sum);
    FLPD("%s", diag_buff);
}

INT32 mtk_flp_hal_jni_proc (MTK_FLP_MSG_T *prmsg) {
    UINT8* pData = NULL;
    MTK_FLP_MSG_T* pRspMsg = NULL;
    INT32 ret = MTK_FLP_SUCCESS;
    char    *rsp=0;
    char    rspbuf[32];
    INT32   loc_num = 0;
    FlpLocation* ptr = NULL;
    char diag_buff[1000];
    int geo_res;
    #ifdef MTK_64_PLATFORM
    UINT8 loc_in[128] = {0};
    FlpLocation loc_out;
    #else
    FlpLocation* loc_in = NULL;
    #endif
    INT32   *param=NULL;
    MTK_FLP_MSG_T *flp_msg = NULL;
    MTK_FLP_MSG_T geo_msg;
    int data_size;

    if(prmsg == NULL) {
        FLPD("mtk_flp_hal_jni_proc, recv prmsg is null pointer\r\n");
        return MTK_FLP_ERROR;
    }
    if(prmsg->length>0) {
        pData = ((UINT8*)prmsg + sizeof(MTK_FLP_MSG_T));
    }
    if(gSysInfo.sys_mode == FLP_OFFLOAD_MODE) {
        FlpCheckCnnAvail = 1;
    }
    FLPD("mtk_flp_hal_jni_proc, recv prmsg, type %d, len %d\r\n", prmsg->type, prmsg->length);
    switch (prmsg->type) {
    case MTK_FLP_MSG_SYS_FLPD_REBOOT_DONE_NTF:
        FLPD("FLP HAL RECOVER: recv FLPD_REBOOT_DONE_NTF");
        mtk_flp_sys_sleep(1000);
        // send recovery cmd
        flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_RECOVERY_INFO_T));
        flp_msg->type = MTK_FLP_MSG_SYS_RECOVERY_CMD;
        flp_msg->length = sizeof(MTK_FLP_RECOVERY_INFO_T);
        FLPD("FLP HAL RECOVER: Re-send system mode = %d", gSysInfo.sys_mode);
        memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&gSysInfo), sizeof(MTK_FLP_RECOVERY_INFO_T));
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
        mtk_flp_sys_msg_free(flp_msg);
        if(gSysInfo.sys_mode == FLP_AP_MODE) {
            if(!gFlpFirstBatch) {
                FLPD("FLP HAL RECOVER: send start cmd - AP mode");
                flp_msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
                flp_msg->type = MTK_FLP_MSG_HAL_START_CMD;
                flp_msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
                memcpy(((INT8*)flp_msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
                mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
            } else {
                FLPD("FLP HAL RECOVER: No need send start cmd - AP mode");
            }
            mtk_flp_hal_geofence_recover_geofence();
        } else { //FLP_OFFLOAD_MODE
            if(!gFlpFirstBatch) {
                FLPD("FLP HAL RECOVER: send set option cmd - OFL mode");
                flp_msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
                flp_msg->type = MTK_FLP_MSG_HAL_SET_OPTION_CMD;
                flp_msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
                memcpy(((INT8*)flp_msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
                mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
            } else {
                FLPD("FLP HAL RECOVER: send stop cmd - OFL mode");
                flp_msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T));
                flp_msg->type = MTK_FLP_MSG_HAL_STOP_CMD;
                flp_msg->length = 0;
                mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
            }
            FLPD("FLP HAL RECOVER: send clear fence cmd - OFL mode");
            mtk_flp_hal_geofence_clear_geofence();
            mtk_flp_hal_geofence_recover_geofence();
        }
        break;
    case MTK_FLP_MSG_SYS_MNLD_REBOOT_DONE_NTF:
        FLPD("FLP HAL RECOVER: recv MMLD_REBOOT_DONE_NTF");
        if(gSysInfo.sys_mode == FLP_OFFLOAD_MODE) {
            if(!gFlpFirstBatch) {
                FLPD("FLP HAL RECOVER: send set option cmd - OFL mode");
                flp_msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
                flp_msg->type = MTK_FLP_MSG_HAL_SET_OPTION_CMD;
                flp_msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
                memcpy(((INT8*)flp_msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
                mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
            } else {
                FLPD("FLP HAL RECOVER: send stop cmd - OFL mode");
                flp_msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T));
                flp_msg->type = MTK_FLP_MSG_HAL_STOP_CMD;
                flp_msg->length = 0;
                mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
            }
            FLPD("FLP HAL RECOVER: send clear fence cmd - OFL mode");
            mtk_flp_hal_geofence_clear_geofence();
            mtk_flp_hal_geofence_recover_geofence();
        }
        break;
    case MTK_FLP_MSG_SYS_REPORT_MNL_INIT_CMD:
        memcpy(&gSysInfo.init_mnld_flag, (INT8*)prmsg + sizeof(MTK_FLP_MSG_T), prmsg->length);
        FLPD("MTK_FLP_MSG_SYS_REPORT_MNL_INIT_CMD received. flag = %d", gSysInfo.init_mnld_flag);
        break;
    case MTK_FLP_MSG_HAL_INIT_RSP:
        FLPD("MTK_FLP_MSG_HAL_INIT_RSP received");
        break;
    case MTK_FLP_MSG_HAL_DEINIT_RSP:
        FLPD("MTK_FLP_MSG_HAL_DEINIT_RSP received");
        break;
    case MTK_FLP_MSG_HAL_REPORT_LOC_NTF:
        #ifndef MTK_64_PLATFORM
        if( prmsg->length % sizeof(MTK_FLP_LOCATION_T) != 0) {
            FLPD("REPORT_LOC_NTF: Data length ERROR!!!!!!");
            return MTK_FLP_ERROR;
        }
        loc_in = (FlpLocation*)((INT8*)prmsg + sizeof(MTK_FLP_MSG_T));
        FLPD("REPORT_LOC_NTF TEST: loc accuracy = %f",loc_in->accuracy);
        if(loc_in->accuracy < 0.000001) {
           FLPD("LOC NTF invalid");
           return MTK_FLP_ERROR;
        }
        mtk_flp_report_loc(prmsg);
        #else
        memcpy(loc_in, (INT8*)prmsg + sizeof(MTK_FLP_MSG_T), prmsg->length);
        mtk_flp_hal_loc_rearrange(loc_in, &loc_out);
        MTK_FLP_MSG_T *prmsg_loc = mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_LOCATION_T));
        prmsg_loc->type = MTK_FLP_MSG_HAL_REPORT_LOC_NTF;
        prmsg_loc->length = sizeof(MTK_FLP_LOCATION_T);
        memcpy(((INT8*)prmsg_loc) + sizeof(MTK_FLP_MSG_T), &loc_out, sizeof(MTK_FLP_LOCATION_T));
        FLPD("REPORT_LOC_NTF TEST: loc accuracy = %f",loc_out.accuracy);
        if(loc_out.accuracy < 0.000001) {
           FLPD("LOC NTF invalid");
           return MTK_FLP_ERROR;
        }
        mtk_flp_report_loc(prmsg_loc);
        mtk_flp_sys_mem_free(prmsg_loc);
        #endif
        break;
    case MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF:
        memset(diag_buff, 0, sizeof(diag_buff));
        memcpy(diag_buff, (char*)prmsg+sizeof(MTK_FLP_MSG_T), prmsg->length);
        if(strstr(diag_buff, "TPDR")) {
            mtk_flp_pdr_diag_proc(diag_buff);
            Ascii2Unicode(diag_buff, gUnicodebuf, strlen(diag_buff));
            mtkFlpDiagCallbacks.data_cb(gUnicodebuf, strlen(diag_buff));
        } else {
            Ascii2Unicode((char*)prmsg+sizeof(MTK_FLP_MSG_T), gUnicodebuf, prmsg->length);
            mtkFlpDiagCallbacks.data_cb(gUnicodebuf, prmsg->length);
        }
        break;
    case MTK_FLP_MSG_DC_GEO_PASSTHRU_RECV_NTF:
        if(gSysInfo.sys_mode == FLP_OFFLOAD_MODE) {
            //Replace the cb function call to mtk_geofence_clear_fences by using msg: JNI->HAL->dc_offload->connsys_main->geofence.c(mcu)
            FLPD("send cbGeoPassthruCallback");
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ prmsg->length);
            flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
            flp_msg->length = sizeof(MTK_FLP_MSG_T)+prmsg->length;
            geo_msg.type = SET_GEOFENCE_PASSTHRU_CB;
            geo_msg.length = prmsg->length;
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T) );
            memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), ((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), prmsg->length  );
            mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
            mtk_flp_sys_msg_free(flp_msg);
        } else {
            if( cbGeoPassthruCallback != NULL) {
                cbGeoPassthruCallback(((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), prmsg->length );
            }
        }
        break;
    case MTK_FLP_MSG_HAL_GEOFENCE_CMD:
        if(gSysInfo.sys_mode == FLP_OFFLOAD_MODE) {
            //Replace the cb function call to mtk_geofence_clear_fences by using msg: JNI->HAL->dc_offload->connsys_main->geofence.c(mcu)
            FLPD("send cbGeoLocCallback");
            param = (INT32*)((INT8*)prmsg+sizeof(MTK_FLP_MSG_T));
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(int));
            flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
            flp_msg->length = sizeof(MTK_FLP_MSG_T)+ sizeof(param);
            geo_msg.type = SET_GEOFENCE_CLEAR_CB;
            geo_msg.length = sizeof(int);
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T) );
            memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), param, sizeof(int) );
            mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
            mtk_flp_sys_msg_free(flp_msg);
        } else {
            if(cbGeoClearCallback != NULL) {
                param = (INT32*)((INT8*)prmsg+sizeof(MTK_FLP_MSG_T));
                rsp = cbGeoClearCallback(*(param));
                if(rsp != NULL) {
                    sprintf(rspbuf, "GFC_RSP=0");
                } else {
                    sprintf(rspbuf, "GFC_RSP=1");
                }
                Ascii2Unicode(rspbuf, gUnicodebuf, strlen(rspbuf) );
                mtkFlpDiagCallbacks.data_cb(gUnicodebuf, strlen(rspbuf) );
            }
        }
        break;
    case MTK_FLP_MSG_HAL_GEOFENCE_RSP:
        //Replace the cb function call from mtk_geofence_clear_fences by using msg: geofence.c(mcu)->dc_offload->hal->jni->FWK
        FLPD("resp cbGeoLocCallback");
        memcpy(&geo_res,pData, sizeof(int));
        FLPD("geo_res = %d",geo_res);
        if(geo_res == 0) {
            sprintf(rspbuf, "GFC_RSP=0");
        } else {
            sprintf(rspbuf, "GFC_RSP=1");
        }
        Ascii2Unicode(rspbuf, gUnicodebuf, strlen(rspbuf));
        mtkFlpDiagCallbacks.data_cb(gUnicodebuf, strlen(rspbuf));
        break;
    case MTK_FLP_MSG_OFL_GEOFENCE_CALLBACK_NTF:
        mtk_flp_geofence_transition_callbacks_proc(prmsg);
        break;
    case MTK_FLP_MSG_HAL_REQUEST_LOC_NTF:
        param = (INT32*)((INT8*)prmsg+sizeof(MTK_FLP_MSG_T));
        mtk_flp_request_loc( *param);
        break;
    case MTK_FLP_MSG_HAL_FLUSH_LOC_NTF:
        mtk_flp_flush_loc();
        break;
    case MTK_FLP_MSG_HAL_GEOFENCE_CALLBACK_NTF:
        mtk_flp_geofence_callbacks_proc(prmsg);
        break;
    case MTK_FLP_MSG_HAL_REPORT_STATUS_NTF:
        param = (INT32*)((INT8*)prmsg+sizeof(MTK_FLP_MSG_T));
        mtk_flp_report_status(*param);
    default:
        FLPD("Uknown  received type:%d", prmsg->type);
        break;
    }
    mtk_flp_sys_mem_free(prmsg);
    return 0;
}

void mtk_flp_hal_jni_thread(void) {

    UINT32 ret = MTK_FLP_SUCCESS, retry_cnt=0;
    MTK_FLP_MSG_T *flp_msg;

    FLPD("mtk_flp_hal_jni_thread, Create\n");

    if(mtkFlpCallbacks.set_thread_event_cb) {
        FLPE("mtkFlpCallbacks.set_thread_event_cb");
        mtkFlpCallbacks.set_thread_event_cb(ASSOCIATE_JVM);
    } else {
        FLPE("set_thread_event_cb not set!!");
    }

    if(mtkFlpCallbacks.flp_capabilities_cb) {
        FLPE("mtkFlpCallbacks.flp_capabilities_cb");
        FlpCapability = (int)(CAPABILITY_GNSS | CAPABILITY_WIFI | CAPABILITY_CELL);
        mtkFlpCallbacks.flp_capabilities_cb(FlpCapability);
    } else {
        FLPE("mtkFlpCallbacks.flp_capabilities_cb not set!!");
    }
    /*
    if(mtk_geofence_callbacks->flp_capabilities_cb) {
        FLPE("mtk_geofence_callbacks.flp_capabilities_cb");
        GeoCapability = (int)(CAPABILITY_GNSS | CAPABILITY_WIFI | CAPABILITY_CELL);
        mtk_geofence_callbacks->flp_capabilities_cb(GeoCapability);
    } else {
        FLPE("mtk_geofence_callbacks.flp_capabilities_cb not set!!");
    }
    */

    g_server_socket_fd = mtk_flp_sys_udp_socket_bind(MTK_FLP2HAL);
    mtk_flp_sys_timer_create(FLP_TIMER_ID_CHECKCNN);

    //Send SYS_MODE to HAL
    gSysInfo.sys_mode = mtk_flp_load_sys_mode();
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(gSysInfo.sys_mode));
    flp_msg->type = MTK_FLP_MSG_SYS_REPORT_MODE_NTF;
    flp_msg->length = sizeof(gSysInfo.sys_mode);
    FLPD("mtk flp report system mode = %d", gSysInfo.sys_mode);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&gSysInfo.sys_mode), sizeof(gSysInfo.sys_mode));
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);

    //Send SYS_INIT to HAL
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) );
    flp_msg->type = MTK_FLP_MSG_HAL_INIT_CMD;
    flp_msg->length = 0;
    FLPD("mtk_flp_hal_jni_thread, send init to HAL thread\n");
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);
    while(!g_ThreadExitJniSocket) {
        // - recv msg from socket interface
        ret = mtk_flp_sys_udp_recv_msg(g_server_socket_fd, &flp_msg);

        if (ret == MTK_FLP_SUCCESS && (!g_ThreadExitJniSocket)) {
            FLPD("mtk_flp_hal_jni_thread, read msg ok (type,%d,len:%d)\n", flp_msg->type, flp_msg->length);

            // Process received message
            mtk_flp_hal_jni_proc(flp_msg);
        } else {
            FLPD("mtk_flp_hal_jni_thread, read msg fail,exit socket thread\n");

            //read msg fail...
            g_ThreadExitJniSocket = TRUE;
        }
    }

    //----------------------------------------------------------------
    // Close socket
    //----------------------------------------------------------------
    FLPD("Closing server_fd,%d\r\n",g_server_socket_fd);
    close(g_server_socket_fd);


    FLPD("mtk_flp_hal_jni_thread, exit\n");

    g_ThreadExitJniSocket = TRUE;
    pthread_exit(NULL);

    return;
}


/*********************************************************/
/* FLP Location Diagnostic Interface implementation                 */
/*********************************************************/
void mtk_flp_diag_init(FlpDiagnosticCallbacks* callbacks) {
    TRC();
    mtkFlpDiagCallbacks = *callbacks;
}
#define LPBCMD "LPB_CMD:"
#define QRYCMD "QRY_CMD"
#define GFCCMD "GFC_CMD"
#define GFCRSP "GFC_RSP"

int mtk_flp_diag_inject_data(char* data, int length)
{
    MTK_FLP_MSG_T   *msg;
    FlpBatchOptions *op = NULL;
    int i, id, param;
    char buf[256], asciibuf[1024], retbuf[1024];
    TRC();

    if(data == NULL || length <= 0) {
        return MTK_FLP_ERROR;
    }

    FLPD("InjectedData(%d): %s\n", length, data);
    for(i=0; i<length; i++) {
        sprintf(&buf[i*2],"%02x",data[i]);
    }
    buf[length*2]='\0';
    FLPD("InjectedData2(%d): %s\n", length, buf);

    Unicode2Ascii(data, asciibuf, length);

    //FLPD("Checking diag commands:");
    //mtk_flp_query_options(asciibuf, length);    //check if it is QRY_CMD, if yes, reply with correct options
    //mtk_flp_loopback_cmd(asciibuf, length);     //check if it is LPB_CMD, if yes, reply with loop-back data

    if(strncmp(asciibuf, LPBCMD, strlen(LPBCMD)) == 0) {
        memset(retbuf, 0, sizeof(retbuf));
        sprintf(retbuf, "LPB_RSP:%s", asciibuf+strlen(LPBCMD));
        FLPD("LPB Resp:", retbuf);
        //mtk_flp_dump_hex("After", gUnicodebuf, prmsg->length*2);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + strlen(retbuf));
        msg->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
        msg->length = strlen(retbuf);
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),retbuf,msg->length);
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);       //send to HAL, will return back immediatedly.
        mtk_flp_sys_msg_free(msg);
    } else if(strncmp(asciibuf, QRYCMD, strlen(QRYCMD)) == 0) {
        FLPD("QRYCMD received");
        sscanf(asciibuf, "QRY_CMD=%d", &id);
        if((op=mtk_flp_get_options(id)) != NULL) {
            sprintf(retbuf, "QRY_RSP=%f,%d,%d,%lld,%f\n", op->max_power_allocation_mW,
                    op->sources_to_use, op->flags, (long long int)op->period_ns, op->smallest_displacement_meters);
        } else {
            sprintf(retbuf, "QRY_RSP=0.0,0,0,0\n");
        }
        FLPD("QRY Resp:", retbuf);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + strlen(retbuf));
        msg->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
        msg->length = strlen(retbuf);
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),retbuf,msg->length);
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);       //send to HAL, will return back immediatedly.
        mtk_flp_sys_msg_free(msg);
    } else if(strncmp(asciibuf, GFCCMD, strlen(GFCCMD)) == 0) {
        FLPD("GFCCMD received");
        sscanf(asciibuf, "GFC_CMD=%d", &param);
        FLPD("GFC_CMD:%d", param);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(INT32));
        msg->type = MTK_FLP_MSG_HAL_GEOFENCE_CMD;
        msg->length = sizeof(INT32);
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)), &param, sizeof(INT32));
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);       //send to HAL, will return back immediatedly.
        mtk_flp_sys_msg_free(msg);
    } else {
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + length);
        msg->type = MTK_FLP_MSG_HAL_DIAG_INJECT_DATA_NTF;
        msg->length = length;
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),asciibuf,length);
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
        mtk_flp_sys_msg_free(msg);
    }
    return MTK_FLP_SUCCESS;
}

static const FlpDiagnosticInterface mtkFlpDiagnosticInterface = {
    sizeof(FlpDiagnosticInterface),
    mtk_flp_diag_init,
    mtk_flp_diag_inject_data,
};


/*********************************************************/
/* FLP Device Context Interface implementation                 */
/*********************************************************/
int  mtk_flp_dev_inject_device_context(uint32_t enabledMask) {
    UNUSED(enabledMask);
    return 0;
}

static const FlpDeviceContextInterface mtkFlpDeviceContextInterface = {
    sizeof(FlpDeviceContextInterface),
    mtk_flp_dev_inject_device_context,
};

typedef void*(*threadptr)(void*);
/*********************************************************/
/* FLP Location Interface implementation                 */
/*********************************************************/
int mtk_flp_init(FlpCallbacks* callbacks) {
    int ret,i;
    TRC();
    mtkFlpCallbacks = *callbacks;

    for(i = 0; i < FLP_OPTION_MAX; i++) {
        gFlpOptions[i] = malloc( sizeof(FlpBatchOptions));
        memset(gFlpOptions[i], 0, sizeof(FlpBatchOptions));
    }

    //init flp daemon
    //Remove android_flpdaemon_init on 20150625, daemon will be triggered by system service, remove fork child process
    //android_flpdaemon_init();

    //Init client socket thread
    ret = pthread_create(&hal_jni_thread_id, NULL, (threadptr)mtk_flp_hal_jni_thread, NULL);
    if(ret < 0) {
        FLPD("Create client thread error\n");
        return MTK_FLP_ERROR;
    }
    return 0;
}

int mtk_flp_get_batch_size() {
    int ret;
    TRC();
    //testing code
    return MTK_FLP_BATCH_SIZE;
}

static void mtk_flp_dump_options(FlpBatchOptions* options) {
    FLPD("BatchOptions(%x)=====>", options);
    FLPD("flgs: %x", options->flags);
    FLPD("max_power_allocation_mW: %f", options->max_power_allocation_mW);
    FLPD("period_ns:%lld", options->period_ns);
    FLPD("sources_to_use:%x", options->sources_to_use);
    FLPD("smallest_displacement:%f", options->smallest_displacement_meters);
}

static void mtk_flp_set_options(int id, FlpBatchOptions* options) {
    if( options == NULL) {
        FLPD("WRONG USAGE of mtk_flp_set_options");
        return;
    }
    FLPD("mtk_flp_set_options id = %d",id);
    memcpy( gFlpOptions[id], options, sizeof(FlpBatchOptions));
    mtk_flp_dump_options(gFlpOptions[id]);
}

static void mtk_flp_update_options() {
    int i;
    // init gFusedOptions
    memset(&gFusedOptions, 0, sizeof(FlpBatchOptions));
    gFusedOptions.period_ns = 1000000000000;
    // update gFusedOptions
    for(i = 0; i < FLP_OPTION_MAX; i++) {
        if(gFlpOptions[i]->sources_to_use == 0) {
            continue;
        }
        gFusedOptions.sources_to_use |= gFlpOptions[i]->sources_to_use;
        if(gFusedOptions.period_ns > gFlpOptions[i]->period_ns) {
            gFusedOptions.period_ns = gFlpOptions[i]->period_ns;
        }
        if(gFusedOptions.flags < gFlpOptions[i]->flags) {
            gFusedOptions.flags = gFlpOptions[i]->flags;
        }
    }
    if(gFusedOptions.period_ns == 1000000000000) {
        gFusedOptions.period_ns = 0;
    }
    // update check timeout sec
    switch(gFusedOptions.flags) {
        case FLP_BATCH_CALLBACK_ON_LOCATION_FIX:
            mtk_flp_set_flp_check_timer(gFusedOptions.period_ns/1000000);
            break;
        case FLP_BATCH_WAKEUP_ON_FIFO_FULL:
            mtk_flp_set_flp_check_timer(gFusedOptions.period_ns/1000000 * MTK_FLP_BATCH_SIZE);
            break;
        case FLP_BATCH_NONE:
            mtk_flp_set_flp_check_timer(0);
            break;
    }
    mtk_flp_update_check_timer();
    FLPD("mtk_flp_update_options");
    mtk_flp_dump_options(&gFusedOptions);
}

static FlpBatchOptions* mtk_flp_get_options(int id) {
    return gFlpOptions[id];
}

#if 0
static void mtk_flp_query_options(char *data, int len)
{
    char retbuf[1024];
    FlpBatchOptions *op = NULL;
    int id;

    if( strncmp(data, "QRY_CMD", strlen("QRY_CMD") ) == NULL ){
        sscanf(data, "QRY_CMD=%d", &id);
        if( (op=mtk_flp_get_options(id)) != NULL){
            sprintf(retbuf, "QRY_RSP=%f,%d,%d,%d\n", op->max_power_allocation_mW, op->sources_to_use, op->flags, op->period_ns);
        }
        else{
            sprintf(retbuf, "QRY_RSP=0.0,0,0,0\n");
        }

        Ascii2Unicode(retbuf, gUnicodebuf, strlen(retbuf));
        //mtk_flp_dump_hex("After", gUnicodebuf, prmsg->length*2);
        mtkFlpDiagCallbacks.data_cb(gUnicodebuf, strlen(retbuf));
    }

}


static void mtk_flp_loopback_cmd(char *data, int len)
{
    char retbuf[1024];
    FlpBatchOptions *op = NULL;

    if( strncmp(data, LPBCMD, strlen(LPBCMD) ) == NULL ){
        sprintf(retbuf, "LPB_RSP:%s", data+strlen(LPBCMD) );
        Ascii2Unicode(retbuf, gUnicodebuf, strlen(retbuf));
        //mtk_flp_dump_hex("After", gUnicodebuf, prmsg->length*2);
        mtkFlpDiagCallbacks.data_cb(gUnicodebuf, strlen(retbuf));
    }
}

#endif

int mtk_flp_start_batching(int id, FlpBatchOptions* options) {
    MTK_FLP_MSG_T *msg;
    TRC();

    if(options == NULL) {
        return MTK_FLP_ERROR;
    }
    mtk_flp_set_options(id, options);
    mtk_flp_update_options();
    if(gFlpFirstBatch == 1) {
        if(!mtk_geo_is_start()) {
            gSysInfo.sys_mode = mtk_flp_load_sys_mode();
            msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(gSysInfo.sys_mode));
            msg->type = MTK_FLP_MSG_SYS_REPORT_MODE_NTF;
            msg->length = sizeof(gSysInfo.sys_mode);
            FLPD("mtk flp report system mode = %d", gSysInfo.sys_mode);
            memcpy(((INT8*)msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&gSysInfo.sys_mode), sizeof(gSysInfo.sys_mode));
            mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
            mtk_flp_sys_msg_free(msg);
        }

        FLPD("first instance, add id = %d", id);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
        msg->type = MTK_FLP_MSG_HAL_START_CMD;
        msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
        mtk_flp_sys_msg_free(msg);
        gFlpFirstBatch = 0;
    } else {
        FLPD("update instance, add id = %d", id);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
        msg->type = MTK_FLP_MSG_HAL_SET_OPTION_CMD;
        msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
        mtk_flp_sys_msg_free(msg);
    }
    return MTK_FLP_SUCCESS;
}

int mtk_flp_update_batching_options(int id, FlpBatchOptions* options) {
    TRC();
    MTK_FLP_MSG_T   *msg;
    if(options == NULL) {
        return MTK_FLP_ERROR;
    }
    mtk_flp_set_options(id, options);
    mtk_flp_update_options();

    FLPD("update instance, update id = %d", id);
    msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
    msg->type = MTK_FLP_MSG_HAL_SET_OPTION_CMD;
    msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
    memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
    mtk_flp_sys_msg_free(msg);
    return MTK_FLP_SUCCESS;
}

int mtk_flp_stop_batching(int id) {
    TRC();
    int i;
    UINT32  sources=0;
    MTK_FLP_MSG_T   *msg;

    if( id >= FLP_OPTION_MAX || id <0) {
        FLPD("Error batching id:%d", id);
        return 0;
    }
    memset(gFlpOptions[id], 0, sizeof(FlpBatchOptions));
    mtk_flp_update_options();

    if(gFusedOptions.sources_to_use == 0) {
        FLPD("stop all instance, last id = %d", id);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T));
        msg->type = MTK_FLP_MSG_HAL_STOP_CMD;
        msg->length = 0;
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
        mtk_flp_sys_msg_free(msg);
        gFlpFirstBatch = 1;
    } else {
        FLPD("update instance, stop id = %d", id);
        msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(FlpBatchOptions));
        msg->type = MTK_FLP_MSG_HAL_SET_OPTION_CMD;
        msg->length = sizeof(MTK_FLP_BATCH_OPTION_T);
        memcpy(((INT8*)msg + sizeof(MTK_FLP_MSG_T)),&gFusedOptions,sizeof(FlpBatchOptions));
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
        mtk_flp_sys_msg_free(msg);
    }
    return MTK_FLP_SUCCESS;
}

void mtk_flp_cleanup() {
    TRC();
}

void mtk_flp_get_batched_location(int last_n_locations) {
    MTK_FLP_MSG_T   *msg;
    TRC();
    msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T)+sizeof(INT32));
    msg->type = MTK_FLP_MSG_HAL_REQUEST_LOC_NTF;
    msg->length = sizeof(INT32);
    memcpy( (UINT8*)msg + sizeof(MTK_FLP_MSG_T),&last_n_locations,sizeof(INT32));
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
    mtk_flp_sys_msg_free(msg);
}

int  mtk_flp_inject_location(FlpLocation* location) {
    TRC();
    MTK_FLP_MSG_T   *msg;

    if(location == NULL) {
        return MTK_FLP_ERROR;
    }
    msg = (MTK_FLP_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_LOCATION_T));
    FLPD("[%s:%d]Malloc msg:%x Location struct:%x",__FILE__,__LINE__,msg , (UINT8*)msg+sizeof(MTK_FLP_MSG_T));
    msg->type = MTK_FLP_MSG_HAL_INJECT_LOC_CMD;
    msg->length = sizeof(MTK_FLP_LOCATION_T);
    memcpy(((UINT8*)msg + sizeof(MTK_FLP_MSG_T)),location,sizeof(FlpLocation));
//    return mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
    if(mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg) < 0) {
        return MTK_FLP_ERROR;
    }
    mtk_flp_sys_msg_free(msg);
    return FLP_RESULT_SUCCESS;
}


const void* mtk_flp_get_extension(const char* name) {
    TRC();

    if(!strcmp(name, FLP_DIAGNOSTIC_INTERFACE)) {
        return &mtkFlpDiagnosticInterface;
    } else if(!strcmp(name, FLP_GEOFENCING_INTERFACE)) {
        FLPD("mtk_flp_get_extension: %s",name);
        FLPD("interface add: %x",&mtkFlpGeofenceInterface);
        return &mtkFlpGeofenceInterface;
    } else if(!strcmp(name, FLP_DEVICE_CONTEXT_INTERFACE)) {
        return &mtkFlpDeviceContextInterface;
    }
    return 0;
}

void mtk_flp_flush_batched_locations() {
    MTK_FLP_MSG_T *msg;
    TRC();

    msg = (MTK_FLP_MSG_T *) mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T));
    msg->type = MTK_FLP_MSG_HAL_FLUSH_LOC_NTF;
    msg->length = 0;
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
    mtk_flp_sys_msg_free(msg);
}

/*********************************************************/
/* FLP offload timeout check */
/*********************************************************/

void mtk_flp_set_flp_check_timer(int timeout_ms) {
    FlpCheckTimeoutSec = timeout_ms;
}

void mtk_flp_set_geo_check_timer(int timeout_ms) {
    GeoCheckTimeoutSec = timeout_ms;
}

void mtk_flp_update_check_timer() {
    int timeout_ms = 0;
    if(FlpCheckTimeoutSec == 0) {
        timeout_ms = GeoCheckTimeoutSec;
    } else if(GeoCheckTimeoutSec == 0) {
        timeout_ms = FlpCheckTimeoutSec;
    } else {
        timeout_ms = FlpCheckTimeoutSec > GeoCheckTimeoutSec? GeoCheckTimeoutSec: FlpCheckTimeoutSec;
    }
    FLPD("check_cnn_timer flp:%d, geo:%d, mix:%d", FlpCheckTimeoutSec, GeoCheckTimeoutSec, timeout_ms);
    if(timeout_ms == 0) {
        mtk_flp_sys_timer_stop(FLP_TIMER_ID_CHECKCNN);
    } else {
        mtk_flp_sys_timer_stop(FLP_TIMER_ID_CHECKCNN);
        mtk_flp_sys_timer_start(FLP_TIMER_ID_CHECKCNN, timeout_ms, mtk_flp_check_cnn_timer_expire, NULL);
    }
}

void mtk_flp_check_cnn_timer_expire() {
    MTK_FLP_MSG_T *msg;
    if(gSysInfo.sys_mode == FLP_OFFLOAD_MODE) {
        FLPD("check_cnn_timer_expire, avail=%d", FlpCheckCnnAvail);
        if(FlpCheckCnnAvail == 0) {
            msg = (MTK_FLP_MSG_T *) mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T));
            msg->type = MTK_FLP_MSG_SYS_CHECK_CNN_NTF;
            msg->length = 0;
            mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, msg);
            mtk_flp_sys_msg_free(msg);
        }
        FlpCheckCnnAvail = 0;
    }
}

int mtk_flp_is_start() {
    return (!gFlpFirstBatch);
}

static const FlpLocationInterface  mtkFlpInterface = {
    sizeof(FlpLocationInterface),
    mtk_flp_init,
    mtk_flp_get_batch_size,
    mtk_flp_start_batching,
    mtk_flp_update_batching_options,
    mtk_flp_stop_batching,
    mtk_flp_cleanup,
    mtk_flp_get_batched_location,
    mtk_flp_inject_location,
    mtk_flp_get_extension,
    mtk_flp_flush_batched_locations,
};

const FlpLocationInterface* mtk_flp_get_flp_interface(struct flp_device_t* dev) {
    TRC();
    UNUSED(dev);
    return &mtkFlpInterface;
}
