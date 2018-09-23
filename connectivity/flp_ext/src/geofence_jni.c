//#include <cutils/xlog.h>
//#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
//#include <sys/epoll.h>
#include <math.h>
//#include <hardware/gps.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
//#include <sys/epoll.h>
#include <string.h>
#include <signal.h>
#include <time.h>
//#include <sys/un.h>
#include <sys/stat.h>
//#include <sys/ioctl.h>
//#include <sys/socket.h>
#include <stdio.h>
//#include "ptype.h"
//#include "Geofence.h"
//#include "mtk_flp.h"
#include "mtk_flp_sys.h"

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "GEO_JNI"

#define FLPE(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define FLPD(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define FLPW(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_WARN, LOG_TAG, __VA_ARGS__)

#else
#define FLPD(...)
#define FLPW(...)
#define FLPE(...)
#endif

typedef enum
{
    inside,
    outside,
    uncertain
}SET_STATE;


typedef enum
{
    INIT_GEOFENCE,
    ADD_GEOFENCE_AREA,
    PAUSE_GEOFENCE,
    RESUME_GEOFENCE,
    REMOVE_GEOFENCE,
    MODIFY_GEOFENCE,
    RECOVER_GEOFENCE,
    CLEAR_GEOFENCE,
    //SET_GEOFENCE_LOC_CB,
    SET_GEOFENCE_PASSTHRU_CB,
    SET_GEOFENCE_CLEAR_CB,
}MTK_COMMAND_T;

typedef struct mtk_geofence_area
{
    int32_t geofence_id;
    double latitude;
    double longitude;
    double radius;
    int last_transition; /*current state, most cases is GPS_GEOFENCE_UNCERTAIN*/
    int monitor_transition; /*bitwise or of  entered/exited/uncertain*/
    int notification_period;/*timer  interval, period of report transition status*/
    int unknown_timer;/*continue positioning time limitied while positioning*/
    int alive;/*geofence status, 1 alive, 0 sleep*/
    uint32_t source_to_use; /* source to use for geofence */
    SET_STATE latest_state;/*latest status: outside, inside, uncertain*/
}MTK_GEOFENCE_PROPERTY_T;

typedef struct mtk_modify_geofence
{
    int32_t geofence_id;
    uint32_t source_to_use; /* source to use for geofence */
    int last_transition; /*current state, most cases is GPS_GEOFENCE_UNCERTAIN*/
    int monitor_transition; /*bitwise or of  entered/exited/uncertain*/
    int notification_period;/*timer  interval, period of report transition status*/
    int unknown_timer;/*continue positioning time limitied while positioning*/
}MTK_MODIFY_GEOFENCE;


#define FLP_SEND_CONNSYS_MAX_FENCE_AT_ONCE  4
#define MAX_GOEFENCE 100
static MTK_GEOFENCE_PROPERTY_T geofence_property[MAX_GOEFENCE];
static int gTotalFence = 0; /*current fence number, must < MAX_GEOFENCE*/

static void mtk_flp_hal_geofence_init(FlpGeofenceCallbacks* callbacks );
static void mtk_flp_hal_geofence_add_geofences(int32_t number_of_geofences, Geofence** geofences);
static void mtk_flp_hal_geofence_pause_geofence(int32_t geofence_id);
static void mtk_flp_hal_geofence_resume_geofence(int32_t geofence_id, int monitor_transitions);
static void mtk_flp_hal_geofence_modify_geofence_option(int32_t geofence_id, GeofenceOptions* options);
static void mtk_flp_hal_geofence_remove_geofences(int32_t number_of_geofences, int32_t* geofence_id);

/* For recover mechanism */
static int check_buff_full_of_fence();
static int check_buff_fence_exist(const int32_t fence_id);
static int check_valid_transition(const int transition);
static int check_fence_vadility(const MTK_GEOFENCE_PROPERTY_T fence);
static int get_buff_avalibale_item();
static int set_buff_fence_infor_by_item(const int item, const MTK_MODIFY_GEOFENCE fence_property);
static void set_buff_init_fence();
static int set_buff_add_fence(const MTK_GEOFENCE_PROPERTY_T fence);
static int set_buff_pause_fence(const int item);
static int set_buff_resume_fence(const int item, const int transition);
static int set_buff_remove_fence(const int item);
static int set_buff_modify_geofence(const MTK_MODIFY_GEOFENCE fence);
static int get_buff_unknown_time();
int set_buff_transition_fence(const int32_t fence_id, const int transition);

extern void mtk_flp_set_geo_check_timer(int timeout_ms);
extern void mtk_flp_update_check_timer();
extern int mtk_flp_is_start();
extern MTK_FLP_RECOVERY_INFO_T gSysInfo;

FlpGeofenceCallbacks *mtk_geofence_callbacks = NULL; /*keep callback function addresses*/
extern int g_server_socket_fd;

const FlpGeofencingInterface mtkFlpGeofenceInterface = {
    sizeof(FlpGeofencingInterface),
    mtk_flp_hal_geofence_init,
    mtk_flp_hal_geofence_add_geofences,
    mtk_flp_hal_geofence_pause_geofence,
    mtk_flp_hal_geofence_resume_geofence,
    mtk_flp_hal_geofence_modify_geofence_option,
    mtk_flp_hal_geofence_remove_geofences,
};

/*********************************************************/
/* FLP Geofence Interface implementation                 */
/*********************************************************/

void mtk_flp_hal_geofence_init(FlpGeofenceCallbacks* callbacks) {
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T geo_msg;

    FLPD("init geofence, cb function : %x",callbacks);
    mtk_geofence_callbacks = callbacks;

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T);
    geo_msg.type = INIT_GEOFENCE;
    geo_msg.length = sizeof(MTK_FLP_MSG_T);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T) );

    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);
    set_buff_init_fence();
    return;
}


/*buffer format is like below: let's add fence batches
--------------------------------------------------------------
-command header---fence 1---fence 2 --- fence number_of_geofence--
---------------------------------------------------------------
*/
void mtk_flp_hal_geofence_add_geofences(int32_t number_of_geofences, Geofence** geofences) {
    int i;
    int ret;
    int32_t partial_size;
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T geo_msg;
    MTK_GEOFENCE_PROPERTY_T dbg_fence;

    if((gTotalFence == 0) && !mtk_flp_is_start()) {
        gSysInfo.sys_mode = mtk_flp_load_sys_mode();
        flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(gSysInfo.sys_mode));
        flp_msg->type = MTK_FLP_MSG_SYS_REPORT_MODE_NTF;
        flp_msg->length = sizeof(gSysInfo.sys_mode);
        FLPD("mtk flp report system mode = %d", gSysInfo.sys_mode);
        memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&gSysInfo.sys_mode), sizeof(gSysInfo.sys_mode));
        mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
        mtk_flp_sys_msg_free(flp_msg);
    }
    //Send the geofence data one by one
    partial_size = 1;
    for(i = 0; i < number_of_geofences; i++) {
       flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_MSG_T) + sizeof(int32_t) + sizeof(MTK_GEOFENCE_PROPERTY_T));
       flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
       flp_msg->length =  sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t) + sizeof(MTK_GEOFENCE_PROPERTY_T);
       geo_msg.type = ADD_GEOFENCE_AREA;
       geo_msg.length =  sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t) + sizeof(MTK_GEOFENCE_PROPERTY_T);
       memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
       memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &partial_size, sizeof(int32_t));
       memset(&dbg_fence,0,sizeof(MTK_GEOFENCE_PROPERTY_T));
       dbg_fence.geofence_id = geofences[i]->geofence_id;
       dbg_fence.latitude= geofences[i]->data->geofence.circle.latitude;
       dbg_fence.longitude = geofences[i]->data->geofence.circle.longitude;
       dbg_fence.radius = geofences[i]->data->geofence.circle.radius_m;
       dbg_fence.last_transition = geofences[i]->options->last_transition;
       dbg_fence.monitor_transition= geofences[i]->options->monitor_transitions;
       dbg_fence.notification_period= geofences[i]->options->notification_responsivenes_ms;
       dbg_fence.unknown_timer= geofences[i]->options->unknown_timer_ms;
       dbg_fence.alive = 1;
       dbg_fence.source_to_use = geofences[i]->options->sources_to_use;
       dbg_fence.latest_state = geofences[i]->options->last_transition >> 1;
       memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)),&dbg_fence,sizeof(MTK_GEOFENCE_PROPERTY_T));
       FLPD("add fence,id=%ld,type=%d,%d,%d,%lf,%d,%d",dbg_fence.geofence_id,dbg_fence.last_transition,dbg_fence.monitor_transition,
        dbg_fence.notification_period,dbg_fence.latitude,dbg_fence.unknown_timer,dbg_fence.source_to_use);
       mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
       mtk_flp_sys_msg_free(flp_msg);

       /* For geofence recover mechanism */
       ret = check_fence_vadility(dbg_fence);
       if(ret == 0) {
           set_buff_add_fence(dbg_fence);
           mtk_flp_set_geo_check_timer(get_buff_unknown_time());
           mtk_flp_update_check_timer();
       }
    }
    return;
}

void mtk_flp_hal_geofence_pause_geofence(int32_t geofence_id) {
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T geo_msg;
    INT8 *msg_dbg;
    int ret;

    FLPD("mtk_flp_geofence_pause_geofence, id: %d fence",geofence_id);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t);
    geo_msg.type = PAUSE_GEOFENCE;
    geo_msg.length = sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
    memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &geofence_id, sizeof(int32_t));
    msg_dbg = (INT8*)flp_msg;
    FLPD("geo dbg 1: %d %d %d %d %d %d %d %d",*msg_dbg, *(msg_dbg+1),*(msg_dbg+2),*(msg_dbg+3),*(msg_dbg+4),*(msg_dbg+5),*(msg_dbg+6),*(msg_dbg+7));
    FLPD("geo dbg 2: %d %d %d %d %d %d %d %d",*(msg_dbg+8), *(msg_dbg+9),*(msg_dbg+10),*(msg_dbg+11),*(msg_dbg+12),*(msg_dbg+13),*(msg_dbg+14),*(msg_dbg+15));
    FLPD("geo dbg 3: %d %d %d %d ",*(msg_dbg+16), *(msg_dbg+17),*(msg_dbg+18),*(msg_dbg+19));
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);

    /* For geofence recover mechanism */
    ret = check_buff_fence_exist(geofence_id);
    if(ret != -1) {
        set_buff_pause_fence(ret);
        mtk_flp_set_geo_check_timer(get_buff_unknown_time());
        mtk_flp_update_check_timer();
    }
    return;
}

void mtk_flp_hal_geofence_resume_geofence(int32_t geofence_id, int monitor_transitions) {
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T geo_msg;
    INT8 *msg_dbg;
    int ret;

    FLPD("mtk_flp_geofence_resume_geofence, id: %d fence",geofence_id);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T) + sizeof(int32_t) + sizeof(int));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t) + sizeof(int);
    geo_msg.type = RESUME_GEOFENCE;
    geo_msg.length =  sizeof(MTK_FLP_MSG_T) + sizeof(int32_t) + sizeof(int);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
    memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &geofence_id, sizeof(int32_t));
    memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)), &monitor_transitions, sizeof(int));

    msg_dbg = (INT8*)flp_msg;
    FLPD("geo dbg 1: %d %d %d %d %d %d %d %d",*msg_dbg, *(msg_dbg+1),*(msg_dbg+2),*(msg_dbg+3),*(msg_dbg+4),*(msg_dbg+5),*(msg_dbg+6),*(msg_dbg+7));
    FLPD("geo dbg 2: %d %d %d %d %d %d %d %d",*(msg_dbg+8), *(msg_dbg+9),*(msg_dbg+10),*(msg_dbg+11),*(msg_dbg+12),*(msg_dbg+13),*(msg_dbg+14),*(msg_dbg+15));
    FLPD("geo dbg 3: %d %d %d %d %d %d ",*(msg_dbg+16), *(msg_dbg+17),*(msg_dbg+18),*(msg_dbg+19),*(msg_dbg+20),*(msg_dbg+21));
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);

    /* For geofence recover mechanism */
    ret = check_buff_fence_exist(geofence_id);
    if(ret != -1) {
        set_buff_resume_fence(ret, monitor_transitions);
        mtk_flp_set_geo_check_timer(get_buff_unknown_time());
        mtk_flp_update_check_timer();
    }

    return;
}

void mtk_flp_hal_geofence_modify_geofence_option(int32_t geofence_id, GeofenceOptions* options) {
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T geo_msg;
    MTK_MODIFY_GEOFENCE modify_fence;
    int ret;

    FLPD("mtk_flp_geofence_modify_geofence_option, id: %d fence",geofence_id);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_MODIFY_GEOFENCE));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_MODIFY_GEOFENCE);
    geo_msg.type = MODIFY_GEOFENCE;
    geo_msg.length = sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_MODIFY_GEOFENCE);
    modify_fence.geofence_id = geofence_id;
    modify_fence.last_transition = options->last_transition;
    modify_fence.monitor_transition = options->monitor_transitions;
    modify_fence.notification_period = options->notification_responsivenes_ms;
    modify_fence.unknown_timer = options->unknown_timer_ms;
    modify_fence.source_to_use = options->sources_to_use;
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
    memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &modify_fence, sizeof(MTK_MODIFY_GEOFENCE));

    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);

    /* For geofence recover mechanism */
    ret = check_valid_transition(modify_fence.monitor_transition);
    if(ret != -1) {
        set_buff_modify_geofence(modify_fence);
        mtk_flp_set_geo_check_timer(get_buff_unknown_time());
        mtk_flp_update_check_timer();
    }
    return;
}

void mtk_flp_hal_geofence_remove_geofences(int32_t number_of_geofences, int32_t* geofence_id) {
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T geo_msg;
    int i;
    int32_t local_id;
    int ret;

    FLPD("mtk_flp_geofence_remove_geofences, no of fence=%d",number_of_geofences);
#if 1
    INT8 *msg_dbg;
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t)+ number_of_geofences*sizeof(int32_t));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+ number_of_geofences*sizeof(int32_t);
    geo_msg.type = REMOVE_GEOFENCE;
    geo_msg.length = sizeof(MTK_FLP_MSG_T) + sizeof(int32_t)+ number_of_geofences*sizeof(int32_t);
    memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
    memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &number_of_geofences, sizeof(int32_t));

    for(i = 0; i < number_of_geofences; i++) {
        local_id = *(geofence_id + i);
        memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+i*sizeof(int32_t)), &local_id, sizeof(int32_t) );
        FLPD("fence %d, id %d",number_of_geofences,local_id );
    }
#else
    INT8 *msg_dbg;
    double a = 0.0;

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t)+ number_of_geofences*sizeof(int32_t)+sizeof(double));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+ number_of_geofences*sizeof(int32_t)+sizeof(int32_t)+sizeof(double);
    geo_msg.type = REMOVE_GEOFENCE;
    geo_msg.length = sizeof(int32_t)+ number_of_geofences*sizeof(int32_t)+sizeof(int32_t)+sizeof(double);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T) );
    memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &number_of_geofences, sizeof(int32_t) );

    for(i=0;i<number_of_geofences;i++){
        local_id = *(geofence_id + i);
        memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+i*sizeof(int32_t)), &local_id, sizeof(int32_t) );
        FLPD("fence %d, id %d",number_of_geofences,local_id );
    }
    memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t)+number_of_geofences*sizeof(int32_t)), &a, sizeof(double) );

 /*
    INT8 *msg_dbg;
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t)+ number_of_geofences*sizeof(int32_t));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+ number_of_geofences*sizeof(int32_t);
    geo_msg.type = REMOVE_GEOFENCE;
    geo_msg.length = sizeof(int32_t)+ number_of_geofences*sizeof(int32_t);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T) );
    memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &number_of_geofences, sizeof(int32_t) );

    for(i=0;i<number_of_geofences;i++){
        local_id = *(geofence_id + i);
        memcpy( (((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)+i*sizeof(int32_t)), &local_id, sizeof(int32_t) );
        FLPD("fence %d, id %d",number_of_geofences,local_id );
    }
*/
#endif
    msg_dbg = (INT8*)flp_msg;
    /*
    FLPD("geo dbg 1: %d %d %d %d %d %d %d %d",*msg_dbg, *(msg_dbg+1),*(msg_dbg+2),*(msg_dbg+3),*(msg_dbg+4),*(msg_dbg+5),*(msg_dbg+6),*(msg_dbg+7));
    FLPD("geo dbg 2: %d %d %d %d %d %d %d %d",*(msg_dbg+8), *(msg_dbg+9),*(msg_dbg+10),*(msg_dbg+11),*(msg_dbg+12),*(msg_dbg+13),*(msg_dbg+14),*(msg_dbg+15));
    FLPD("geo dbg 3: %d %d %d %d %d %d %d %d",*(msg_dbg+16), *(msg_dbg+17),*(msg_dbg+18),*(msg_dbg+19),*(msg_dbg+20),*(msg_dbg+21),*(msg_dbg+22),*(msg_dbg+23));
    */
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);

    /* For geofence recover mechanism */
    ret = check_buff_fence_exist(*geofence_id);
    if(ret != -1) {
        set_buff_remove_fence(ret);
        mtk_flp_set_geo_check_timer(get_buff_unknown_time());
        mtk_flp_update_check_timer();
    }
    return;
}


/* For Geofence recovery mechanism*/

void mtk_flp_hal_geofence_recover_geofence() {
    int i;
    int partial_size = 1;
    MTK_FLP_MSG_T* flp_msg = NULL;
    MTK_FLP_MSG_T geo_msg;

    FLPD("FLP HAL RECOVER: recover total fence = %d", gTotalFence);
    for(i = 0; i < MAX_GOEFENCE; i++) {
        if(geofence_property[i].alive == 1) {
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_MSG_T) + sizeof(int32_t) + sizeof(MTK_GEOFENCE_PROPERTY_T));
            flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
            flp_msg->length =  sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t) + sizeof(MTK_GEOFENCE_PROPERTY_T);
            geo_msg.type = RECOVER_GEOFENCE;
            geo_msg.length =  sizeof(MTK_FLP_MSG_T)+ sizeof(int32_t) + sizeof(MTK_GEOFENCE_PROPERTY_T);
            memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
            memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)), &partial_size, sizeof(int32_t));
            memcpy((((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+sizeof(int32_t)),&geofence_property[i],sizeof(MTK_GEOFENCE_PROPERTY_T));
            FLPD("FLP HAL RECOVER: id=%ld,type=%d,%d,%d,%lf,%d,%d",geofence_property[i].geofence_id,geofence_property[i].last_transition,geofence_property[i].monitor_transition,
            geofence_property[i].notification_period,geofence_property[i].latitude,geofence_property[i].unknown_timer,geofence_property[i].source_to_use);
            mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
            mtk_flp_sys_msg_free(flp_msg);
        }
    }
    return;
}

void mtk_flp_hal_geofence_clear_geofence() {
    int i;
    MTK_FLP_MSG_T* flp_msg = NULL;
    MTK_FLP_MSG_T geo_msg;

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T));
    flp_msg->type = MTK_FLP_MSG_OFL_GEOFENCE_CMD;
    flp_msg->length = sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_MSG_T);
    geo_msg.type = CLEAR_GEOFENCE;
    geo_msg.length = sizeof(MTK_FLP_MSG_T);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &geo_msg, sizeof(MTK_FLP_MSG_T));
    mtk_flp_sys_udp_send_msg(MTK_HAL2FLP, flp_msg);
    mtk_flp_sys_msg_free(flp_msg);
    return;
}

int mtk_geo_is_start() {
    return (gTotalFence > 0);
}

static int check_buff_full_of_fence() {
    if(gTotalFence >= MAX_GOEFENCE) {
        return -1;
    } else {
        return 0;
    }
}

static int check_buff_fence_exist(const int32_t fence_id) {
    int i;

    for(i = 0; i < MAX_GOEFENCE; i++) {
        if(geofence_property[i].geofence_id == fence_id ) {
            return i;
        }
    }
    return -1;
}

static int check_valid_transition(const int transition) {
    switch(transition) {
        case FLP_GEOFENCE_TRANSITION_ENTERED:
        case FLP_GEOFENCE_TRANSITION_EXITED:
        case FLP_GEOFENCE_TRANSITION_ENTERED|FLP_GEOFENCE_TRANSITION_EXITED:
        case FLP_GEOFENCE_TRANSITION_ENTERED|FLP_GEOFENCE_TRANSITION_EXITED|FLP_GEOFENCE_TRANSITION_UNCERTAIN:
            //FLPD("valid transition\n");
            return 0;
        default:
            //FLPD("invalid transition %d\n", transition);
            return -1;
    }
}

static int check_fence_vadility(const MTK_GEOFENCE_PROPERTY_T fence) {
    int ret;
    int ret2;
    //FLPD("before add this new fence, let's check it's vability first\n");

    ret = check_buff_full_of_fence();
    if(ret < 0) {
        FLPD("too many fences\n");
        ret2 =  -1;
        return ret2;
    }
    ret = check_buff_fence_exist(fence.geofence_id);
    if(ret >= 0) {
        FLPD("fence has been added before\n");
        ret2 =  -2;
        return ret2;
    }
    ret = check_valid_transition(fence.monitor_transition);
    if(ret < 0) {
        FLPD("fence is invalid\n");
        ret2 =  -3;
        return ret2;
    }
    ret2 = 0;
    return ret2;
}

static int get_buff_avalibale_item() {
    int i = 0;

    while(i < MAX_GOEFENCE && geofence_property[i].alive == 1) {
        //FLPD("index =%d\n",i);
        i++;
    }
        //FLPD("last index =%d\n",i);
    if(i == MAX_GOEFENCE) {
        //FLPD("i = max geofence\n");
        return -1;
    }
    return i;
}

static int set_buff_fence_infor_by_item(const int item, const MTK_MODIFY_GEOFENCE fence_property) {
    //  memcpy(&geofence_property[item], fence_property, sizeof(MTK_GEOFENCE_PROPERTY_T));  //copy from MTK_MODIFY_GEOFENCE, but use the sizeof MTK_GEOFENCE_PROPERTY_T
    geofence_property[item].geofence_id = fence_property.geofence_id;
    geofence_property[item].last_transition = fence_property.last_transition;
    geofence_property[item].monitor_transition = fence_property.monitor_transition;
    geofence_property[item].notification_period = fence_property.notification_period;
    geofence_property[item].unknown_timer = fence_property.unknown_timer;
    return 0;
}

static void set_buff_init_fence() {
    int i;
    memset(&geofence_property, 0, MAX_GOEFENCE*sizeof(MTK_GEOFENCE_PROPERTY_T));
    for(i = 0; i < MAX_GOEFENCE; i++) {
        geofence_property[i].geofence_id = -1;
    }
    return;
}

static int set_buff_add_fence(const MTK_GEOFENCE_PROPERTY_T fence) {
    int item;

    item = get_buff_avalibale_item();
    if(item < 0) {
        FLPD("out of range\n");
        return -1;
    }
    FLPD("new fenece will be added into item %d\n", item);

    memcpy(&geofence_property[item], &fence, sizeof(MTK_GEOFENCE_PROPERTY_T));
    geofence_property[item].alive = 1;
    gTotalFence++;
    return 0;
}

static int set_buff_pause_fence(const int item) {
    geofence_property[item].alive = 0;
    geofence_property[item].last_transition = FLP_GEOFENCE_TRANSITION_UNCERTAIN;
    geofence_property[item].latest_state = uncertain;
    return 0;
}

static int set_buff_resume_fence(const int item, const int transition) {
    geofence_property[item].monitor_transition = transition;
    geofence_property[item].alive = 1;
    return 0;
}

static int set_buff_remove_fence(const int item) {
    geofence_property[item].alive = 0;
    memset(&geofence_property[item],0,sizeof(MTK_GEOFENCE_PROPERTY_T));
    geofence_property[item].geofence_id = -1;
    gTotalFence--;
    return 0;
}

static int set_buff_modify_geofence(const MTK_MODIFY_GEOFENCE fence) {
    int ret;
    //FLPD("let's modify fence id =  %ld property\n", fence.geofence_id);
    ret = check_buff_fence_exist(fence.geofence_id);
    if(ret >= 0) {
        //FLPD("this fence is in array[%d]\n", ret);
    } else {
        //FLPD("invalid fence, not exist yet ret = %d\n", ret);
        return -1;
    }
    set_buff_fence_infor_by_item(ret, fence);
    return 0;

}

static int get_buff_unknown_time() {
    int i;
    int unknown_timer_ms = 1000000000;
    for(i = 0; i < MAX_GOEFENCE; i++) {
        if(geofence_property[i].alive == 1) {
            if(geofence_property[i].unknown_timer < unknown_timer_ms) {
                unknown_timer_ms = geofence_property[i].unknown_timer;
            }
        }
    }
    if(unknown_timer_ms == 1000000000) {
        unknown_timer_ms = 0;
    }
    return unknown_timer_ms;
}

int set_buff_transition_fence(const int32_t fence_id, const int transition) {
    int ret;
    int item;
    ret = check_valid_transition(transition);
    if(ret < 0) {
        FLPD("transition is invalid");
        return ret;
    }
    item = check_buff_fence_exist(fence_id);
    if(item >= 0) {
        geofence_property[item].last_transition = transition;
        geofence_property[item].latest_state = transition >> 1;
    }
    return 0;
}

