/*****************************************************************************
 * Include
 *****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>
#ifndef K2_PLATFORM
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#include "mtk_flp_dc.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <private/android_filesystem_config.h>
#else
#include "mtk_flp_connsys_sys_type.h"
#include "mtk_flp_connsys_sys.h"
#include "mtk_gps_bora_flp.h"
#endif

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FLP_DC_SENS"

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


/*****************************************************************************
 * Define
 *****************************************************************************/
#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

#define NELEMS(x)    (int)(sizeof(x) / sizeof(x[0]))
#define MAX_UINT32 0xfffffffe //max value of ttick, for ttick rollover calculation
#define MAX_FIRST_TTFF_TIME  60 //after 60sec, will trigger pdr loc regardless of lack of first fix

typedef struct
{
    INT32 x;
    INT32 y;
    INT32 z;
}MTK_FLP_PDR_INPUT;

typedef struct PDR_GPS
{
    /* gps report location in lla under () frame*/
    double location[3];

    /* velocity values are in meter per second (m/s) under (N,E,D) frame*/
    float velocity[3];

    /* velocity one sigma error in meter per second under (N,E,D) frame*/
    float  velocitySigma[3];

    /*  horizontal dilution of precision value in unitless */
    float HDOP;

    /*  horizontal accuracy of location in meter */
    float HACC;

} PDR_GPS, *LPPDR_GPS;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
extern int mtk_flp_lowpower_get_latest_loc(MTK_FLP_LOCATION_T *location);
extern INT32 mtk_flp_lowpower_query_mode ();
extern UINT32 mtk_flp_lowpower_query_source ();
extern int mtk_flp_lowpower_query_start_elapsed_time(void);
int mtk_flp_dc_sensor_process(MTK_FLP_MSG_T *prmsg);

#ifndef K2_PLATFORM
void mtk_flp_dc_sensor_main(void);
extern UINT32 mtk_flp_sys_get_time_tick();
#else
void mtk_flp_dc_sensor_main(MTK_FLP_MSG_T *prmsg);
#endif

//*****************************************
// Report thread function call backs
//*****************************************
#ifndef K2_PLATFORM
#define TURN_ON_DC_SENSOR    1

#if (TURN_ON_DC_SENSOR == 1 )
DcReportThread    Report_thrd_SENSOR = (DcReportThread)mtk_flp_dc_sensor_main;
DcProcessFunction    Process_Func_SENSOR = (DcProcessFunction)mtk_flp_dc_sensor_process;
#else
DcReportThread    Report_thrd_SENSOR = NULL;
DcProcessFunction    Process_Func_SENSOR = NULL;
#endif
#endif


/*****************************************************************************
 * Global Variables
 *****************************************************************************/
static double flp_state[3]={30.663968, 104.065630, 0.0}; //(lat, lon, theta)
static double flp_state_1[3]={30.663968, 104.065630, 0.0};
static double flp_variance_1[9]={9000000.0,0,0,0,9000000.0,0,0,0,9000000.0};
static int has_updated_by_radio = 1;
static int pdr_acc_step_cnt = 0;
static int pdr_acc_step_len = 0;
MTK_FLP_UTC_TIME pdr_last_ttick = 0;

#ifndef K2_PLATFORM
#define MTKFLP_COMM_SENSOR_SOCK_CLI_PATH   "/data/mpe_flp/mpe2flpsensor"
#define MTKFLP_COMM_SENSOR_SOCK_SVR_PATH   "/data/mpe_flp/flpsensor2mpe"

int flp_sensor_server_fd = -1;
int flp_sensor_client_fd = -1;
static struct sockaddr_un sensor_client_sockaddr;
static struct sockaddr_un sensor_server_sockaddr;

#define SOCK_BUF_LEN    65535
char sockbuf[SOCK_BUF_LEN];
#endif
/*****************************************************************************
 * Function
 *****************************************************************************/
float Array3x3_Invert_double(double *matrixA , double *matrixA_1) {
    // Calculate the determinate value of a 3x3 matrix
    double det , mul1 , mul2;
    int i , j;
    int pos = 0;

    for(det=0 , i=0 ; i<3 ; det+=(mul1-mul2) , i++) {
        for (mul1=mul2=1 , j=0 ; j<3 ; j ++) {
            pos = ((i+j)%3)*3 + j;
            mul1 *= matrixA[pos];
            pos = ((i+j)%3)*3 + 2-j;
            mul2 *= matrixA[pos];
        }
    }

    if (det == 0)
        return -1;

    // calculate each element of the inverse matrix
    for(i=0 ; i<3 ; i++) {
        for (j=0 ; j<3 ; j++) {
            matrixA_1[j*3+i] = ( matrixA[((i+1)%3)*3 + (j+1)%3] * matrixA[((i+2)%3)*3 + (j+2)%3] -
            matrixA[((i+2)%3)*3 + (j+1)%3] * matrixA[((i+1)%3)*3 + (j+2)%3] ) / det;
        }
    }
    return det;
}

void array_multiply_double(double *matrixA,double *matrixB,double *matrixC,int sizeAR,int sizeAC,int sizeBR,int sizeBC) {
    int i,j;
    double a,b;

    if( (sizeAR >10) || (sizeBC > 10)) {
        return;
    }

    for(i = 0; i < (sizeAR * sizeBC); i++) {
        *(matrixC + i) = 0;
        for(j = 0;j < sizeBR; j++) {
            a = *(matrixA + (i / sizeBC) * sizeAC + j);
            b = *(matrixB + (i % sizeBC) + j * sizeBC);
            *(matrixC + i) += a * b;
        }
    }
}

int pdr_translation_transformation(int dn, int de, LPPDR_GPS pgps, double *ste_1, double *var_1, double *ste_p, double *var_p) {
    double state_p[3]={0.0f};
    double variance_p[9]={0.0f};
    double state_1_local[3]={0.0f};
    double variance_1_local[9]={0.0f};
    double latitude_rad=pgps->location[0]*M_PI/180.0;
    double sin_lat = sin(latitude_rad);
    double cos_lat = cos(latitude_rad);
    double temp_a=(1 - 6.69437999014/1000*sin_lat*sin_lat);
    double v=6378137/(temp_a);
    double v_cos = v*cos_lat;
    double r=v*(1-6.69437999014/1000)/(temp_a);
    double dlat= ((double)dn)/100.0/r;
    double dlon= ((double)de)/100.0/v_cos;
    //FLP_TRC("dlat: %lf, dlon:%lf \n",dlat*1000000000, dlon*1000000000);

    memcpy(state_p, ste_p, sizeof(state_p));
    memcpy(variance_p, var_p, sizeof(variance_p));
    memcpy(state_1_local, ste_1, sizeof(state_1_local));
    memcpy(variance_1_local, var_1, sizeof(variance_1_local));

    dlat = dlat*180.0/M_PI;
    dlon = dlon*180.0/M_PI; //radian to degree
    //FLP_TRC("dlat1: %lf, dlon1:%lf \n",dlat*1000000000, dlon*1000000000);

    state_p[0]= flp_state_1[0]+dlat;
    state_p[1]= flp_state_1[1]+dlon;
    memcpy(variance_p, variance_1_local, sizeof(variance_p));
    variance_p[0] = variance_1_local[0]+(double)(6e-9);// degree^2
    variance_p[4] = variance_1_local[4]+(double)(6e-9);// degree^2
    variance_p[8] = variance_1_local[8]+(double)(3e-4); //radian^2

    //FLP_TRC("pdr_trans: dn %d, de %d, dlat %lf, dlon %lf,state_p: %lf, %lf\n",
    //    dn, de, dlat*1000000000.0, dlon*1000000000.0, state_p[0], state_p[1] );

    memcpy(ste_p, state_p, sizeof(state_p));
    memcpy(var_p, variance_p, sizeof(variance_p));

    return 0;
}

static void radio_location_update(LPPDR_GPS pgps, double *ste_p, double *var_p) {
    double state_p[3]={0.0f};
    double variance_p[9]={0.0f};
    double phi[9]={0.0f};
    double inv_phi[9]={0.0f};
    double vel_2norm = pgps->velocity[0]*pgps->velocity[0] + pgps->velocity[1]*pgps->velocity[1];
    double Q_bearing=0.0;
    double Zt_Zp[3]={0.0f};
    double K[9]={0.0f};
    double KE[3]={0.0f};
    double variance[9]={0.0f};
    double I_K[9]={0.0f};
    int i;

    memcpy(state_p, ste_p, sizeof(state_p));
    memcpy(variance_p, var_p, sizeof(variance_p));
    //vel_2norm == 0 ? vel_2norm += (double)0.0000001 : vel_2norm += 0;

    if(vel_2norm == 0) {
        vel_2norm += (double)0.0000001;
    } else {
        vel_2norm += 0;
    }

    Q_bearing = (double)((pgps->HDOP)*(pgps->HDOP)*(pgps->velocitySigma[0])*(pgps->velocitySigma[0]))/(vel_2norm);
    memcpy(phi, variance_p, sizeof(phi));
    phi[0] += (double)((pgps->HACC)*(pgps->HACC)*0.00001*0.00001);// sigma_x_lat in degree^2
    phi[4] += (double)((pgps->HACC)*(pgps->HACC)*0.00001*0.00001);//sigma_y_lon in degree^2
    phi[8] += Q_bearing;
    Array3x3_Invert_double(phi, inv_phi);

    Zt_Zp[0] = pgps->location[0] - state_p[0];
    Zt_Zp[1] = pgps->location[1] - state_p[1];
    Zt_Zp[2] = atan2(pgps->velocity[1],pgps->velocity[0]) - state_p[2]; // be care of the transition point

    if (Zt_Zp[2] > M_PI) {
        Zt_Zp[2] -= (double)(2*M_PI);
    } else if (Zt_Zp[2] < -M_PI) {
        Zt_Zp[2] += (double)(2*M_PI);
    }

    array_multiply_double(variance_p, inv_phi, K, 3, 3, 3, 3);
    array_multiply_double(K, Zt_Zp, KE, 3, 3, 3, 1);

    for (i = 0; i < NELEMS(KE); i++) {
        state_p[i] += KE[i];
    }

    memcpy(I_K, K, sizeof(I_K));
    I_K[0]=1-I_K[0];
    I_K[1]=0-I_K[1];
    I_K[2]=0-I_K[2];
    I_K[3]=0-I_K[3];
    I_K[4]=1-I_K[4];
    I_K[5]=0-I_K[5];
    I_K[6]=0-I_K[6];
    I_K[7]=0-I_K[7];
    I_K[8]=1-I_K[8];

    array_multiply_double(I_K, variance_p, variance, 3, 3, 3, 3);
    memcpy(variance_p, variance, sizeof(variance_p));
    memcpy(ste_p, &state_p, sizeof(state_p));
    memcpy(var_p, &variance_p, sizeof(variance_p));
}

int mpe_update_flp_lla(MTK_FLP_LOCATION_T *loc_in, int dx, int dy, MTK_FLP_LOCATION_T *fused_loc) {
    double flp_state_p[3]={0.0f};
    double flp_variance_p[9]={0.0f};
    PDR_GPS pradio_in;

    int dn=dy;
    int de=dx;
    memset(&pradio_in, 0, sizeof(PDR_GPS));
    if(dn!=0 || de!=0) {
        pradio_in.location[0] = loc_in->latitude;
        pradio_in.location[1] = loc_in->longitude;
        pradio_in.location[2] = 0;

        pdr_translation_transformation(dn,de, &pradio_in, flp_state_1, flp_variance_1, flp_state_p, flp_variance_p);
        //FLP_TRC("flp_variance: %lf, %lf\n",flp_variance_1[0],flp_variance_p[0]);
    } else {
        return MTK_FLP_ERROR;
    }

    //pdr_gps format convert
    if(loc_in != NULL) {
        //FLP_TRC("loc_in: %f, %f",loc_in->latitude,loc_in->longitude );
        if((loc_in->accuracy != 0)) {
            if((loc_in->sources_used == FLP_TECH_MASK_GNSS) || (loc_in->sources_used == FLP_TECH_MASK_WIFI) || (loc_in->sources_used == FLP_TECH_MASK_CELL)) {
                pradio_in.HACC = (loc_in->accuracy)*(loc_in->accuracy);
                pradio_in.location[0] = loc_in->latitude;
                pradio_in.location[1] = loc_in->longitude;
                pradio_in.location[2] = 0;
                radio_location_update(&pradio_in, flp_state_p, flp_variance_p);
            }
        }

        memcpy(flp_state, flp_state_p, sizeof(flp_state));
        memcpy(flp_state_1, flp_state_p, sizeof(flp_state_1));
        memcpy(flp_variance_1, flp_variance_p, sizeof(flp_variance_1));
        //FLP_TRC("pdr_var_propag: variance_1 %lf, HACC out %lf, gps ACC in %f\n",flp_variance_1[0], sqrt(flp_variance_1[0])*100000.0, pradio_in.HACC);
    } else {
        return MTK_FLP_ERROR;
    }

    if(fused_loc != NULL) {
        //rearrange to FLP format
        fused_loc->latitude = flp_state[0];
        fused_loc->longitude = flp_state[1];
        fused_loc->altitude= loc_in->altitude;
        fused_loc->size = sizeof(MTK_FLP_LOCATION_T);
        fused_loc->accuracy = sqrt(flp_variance_1[0])*100000.0; //degree^2 to m
        fused_loc->bearing = 0;
        fused_loc->flags = (UINT16)(FLP_LOCATION_HAS_LAT_LONG | FLP_LOCATION_HAS_ACCURACY | FLP_LOCATION_HAS_ALTITUDE);
        fused_loc->sources_used = FLP_TECH_MASK_SENSORS;
        fused_loc->speed = 0;
        #ifdef K2_PLATFORM
        fused_loc->timestamp = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick()/TTICK_CLK);
        #else
        fused_loc->timestamp = mtk_flp_get_timestamp(NULL);
        #endif
    } else {
        return MTK_FLP_ERROR;
    }
    return MTK_FLP_SUCCESS;
}

int mtk_flp_sensor_report_pdr(MTK_FLP_PDR_INPUT *pdr_in, float pdr_accuracy) {
    MTK_FLP_MSG_T *flp_msg1=NULL;
    MTK_FLP_UTC_TIME current_ttick;
    float angle = 0;
    int pdr_speed_x = 0, pdr_speed_y = 0, pdr_bearing = 0, pdr_height = 0;
    int ret = MTK_FLP_ERROR;
    char localbuf[512];

    //arrange TPDR output to AP
    //Info including: stepcount, dx/dy w.r.t first fix, step length, x/y speed, bearing
    if(pdr_in != NULL) {
        pdr_acc_step_cnt += 1;
        pdr_acc_step_len += (int)(sqrtf((float)((pdr_in->x)*(pdr_in->x) + (pdr_in->y)*(pdr_in->y))));
        angle = atan((float)(pdr_in->y)/(float)(pdr_in->x));

        if(pdr_in->x >=0) { ////section 1,2
            if(pdr_in->y >= 0) { //section 1
                pdr_bearing = (abs)((int)(angle*180.0/M_PI));
            } else { //section 2
                pdr_bearing = 90 + (abs)((int)(angle*180.0/M_PI));
            }
        } else { //section3,4
            if(pdr_in->y >= 0) { //section 4
                pdr_bearing = 360 - (abs)((int)(angle*180.0/M_PI));
            } else { //section 3
                pdr_bearing = 270 - (abs)((int)(angle*180.0/M_PI));
            }
        }
        //FLP_TRC("TPDR angle: %f,%f, bearing: %d, acc_len: %d, acc_cnt:%d", ((float)(pdr_in->y)/(float)(pdr_in->x)),angle,pdr_bearing,pdr_acc_step_len, pdr_acc_step_cnt);

        if(pdr_last_ttick == 0) { //first output
#ifndef K2_PLATFORM
            pdr_last_ttick = (MTK_FLP_UTC_TIME)mtk_flp_sys_get_time_tick();
#else
            pdr_last_ttick = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick());
#endif
            pdr_speed_x = pdr_in->x;
            pdr_speed_y = pdr_in->y;
            //FLP_TRC("curr_ttick:%lld",pdr_last_ttick);
        } else {
#ifndef K2_PLATFORM
            current_ttick = (MTK_FLP_UTC_TIME)mtk_flp_sys_get_time_tick();
#else
            current_ttick = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick());
#endif
            if(current_ttick == pdr_last_ttick) {
                return ret;
            }

            if(current_ttick < pdr_last_ttick) { //rollover detection
#ifndef K2_PLATFORM
                current_ttick = (MTK_FLP_UTC_TIME)((MAX_UINT32 - pdr_last_ttick) +  mtk_flp_sys_get_time_tick());
                //FLP_TRC("curr_ttick:%lld, last_ttick:%lld",current_ttick,pdr_last_ttick);
                pdr_speed_x = ((float)pdr_in->x)/((float)(current_ttick-pdr_last_ttick));
                pdr_speed_y = ((float)pdr_in->y)/((float)(current_ttick-pdr_last_ttick));
#else
                current_ttick = (MTK_FLP_UTC_TIME)((MAX_UINT32 - pdr_last_ttick) +  mtk_flp_mcu_get_time_tick());
                //FLP_TRC("curr_ttick:%lld, last_ttick:%lld",current_ttick,pdr_last_ttick);
                pdr_speed_x = ((float)pdr_in->x)/((float)(current_ttick-pdr_last_ttick)/(float)TTICK_CLK);
                pdr_speed_y = ((float)pdr_in->y)/((float)(current_ttick-pdr_last_ttick)/(float)TTICK_CLK);
#endif
                //FLP_TRC("ttick rollover detected, %lld",current_ttick);
            }
#ifndef K2_PLATFORM
            pdr_last_ttick = (MTK_FLP_UTC_TIME)(mtk_flp_sys_get_time_tick());
#else
            pdr_last_ttick = (MTK_FLP_UTC_TIME)(mtk_flp_mcu_get_time_tick());
#endif
        }
        //FLP_TRC("TPDR %d %d %d %d %d %d %d %d %d",pdr_acc_step_cnt, pdr_in->x, pdr_in->y,
        //pdr_speed_x, pdr_speed_y, pdr_acc_step_len, pdr_height,pdr_bearing,(int)pdr_accuracy );

        //rearrane TPDR output
        if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
            sprintf(localbuf,"TPDR %d %d %d %d %d %d %d %d %d",pdr_acc_step_cnt, pdr_in->x, pdr_in->y,
            pdr_speed_x, pdr_speed_y, pdr_acc_step_len, pdr_height,pdr_bearing,(int)pdr_accuracy );
#ifndef K2_PLATFORM
            flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
            flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
            flp_msg1->length = strlen(localbuf);
            memcpy(((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf));
            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg1);
#else
            flp_msg1 = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
            flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
            flp_msg1->length = strlen(localbuf);
            memcpy(((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf));
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);
#endif
        }
        ret = MTK_FLP_SUCCESS;
    }
    return ret;
}

#ifndef K2_PLATFORM //AP version
static int mtk_flp_dc_sensor_connect_socket(void) {
    int res;
    /* Create socket with MPED */
    if((flp_sensor_client_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1) {
        FLPD("open client sock failed\r\n");
        return -1;
    }

    unlink(MTKFLP_COMM_SENSOR_SOCK_CLI_PATH);
    memset(&sensor_client_sockaddr, 0, sizeof(sensor_client_sockaddr));
    sensor_client_sockaddr.sun_family = AF_LOCAL;
    strcpy(sensor_client_sockaddr.sun_path, MTKFLP_COMM_SENSOR_SOCK_CLI_PATH);

    if (bind(flp_sensor_client_fd, (struct sockaddr *)&sensor_client_sockaddr, sizeof(sensor_client_sockaddr)) < 0) {
        FLPD("Bind client error: %s\n", strerror(errno));
        return -1;
    }

    res = chmod(MTKFLP_COMM_SENSOR_SOCK_CLI_PATH, 0660);
    chown(MTKFLP_COMM_SENSOR_SOCK_CLI_PATH, -1, AID_INET);
    FLPD("chmod res = %d\n", res);  //770<--mode

    if((flp_sensor_server_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        FLPD("open server sock failed\r\n");
        return -1;
    }
    memset(&sensor_server_sockaddr, 0, sizeof(sensor_server_sockaddr));
    sensor_server_sockaddr.sun_family = AF_LOCAL;
    strcpy(sensor_server_sockaddr.sun_path, MTKFLP_COMM_SENSOR_SOCK_SVR_PATH);
    return 0;
}

static int mtk_flp_dc_sensor_send_msg(MTK_FLP_MSG_T *msg, int len) {
    FLPD("Sending %d to MPED\n", len);
    if(flp_sensor_server_fd <=0) {
        FLPD("DC Send to MPED: UDP socket not connected\n");
        return -1;
    } if(msg == NULL) {
        FLPD("socket msg NULL\n");
        return -1;
    }

    if(sendto(flp_sensor_server_fd, msg, len, 0, (struct sockaddr*)&sensor_server_sockaddr, sizeof(sensor_server_sockaddr)) < 0) {
        FLPD("DC send to MPED fail:%s\n", strerror(errno));
    }
    return 0;
}

static int mtk_flp_dc_sensor_recv_msg(MTK_FLP_MSG_T **msg) {
    int recvlen = 0;
    socklen_t addrlen = sizeof(sensor_client_sockaddr);

    *msg = NULL;
    FLPD("Receiving from MPED\n");

    recvlen = recvfrom(flp_sensor_client_fd, &sockbuf[0], SOCK_BUF_LEN, 0, (struct sockaddr*)&sensor_client_sockaddr, &addrlen);

    if(recvlen >= (int)sizeof(MTK_FLP_MSG_T)) {
        FLPD("%d bytes received\n", recvlen);
        *msg = (MTK_FLP_MSG_T *)sockbuf;
        return recvlen;
    } else {
        FLPD("Error recveiving MPED UDP data: %s\n", strerror(errno));
        return -1;
    }
    return -1;
}

void mtk_flp_dc_sensor_main(void) {
    MTK_FLP_MSG_T *flp_msg = NULL, *flp_msg1 = NULL, *msg = NULL;
    MTK_FLP_LOCATION_T  loc_out={0}, latest_loc={0};
    MTK_FLP_PDR_INPUT pdr_in = {0};
    char localbuf[512];
    int flp_loc_available = -1, retlen = -1;

    if(mtk_flp_dc_sensor_connect_socket() == 0) {
        FLPD("mtk_flp_dc_sensor_main init OK\n");
    } else {
        FLPD("mtk_flp_dc_sensor_main init failed\n");
        return;
    }

    while(1) {
        retlen = mtk_flp_dc_sensor_recv_msg((MTK_FLP_MSG_T **)&msg);
        if(retlen <= 0) {
            continue;
        }
        switch(msg->type){
            case CMD_FLP_START_SENSOR_RES:
                FLPD("SENSOR start ok");
                break;
            case CMD_FLP_STOP_SENSOR_RES:
                FLPD("SENSOR stop ok");
                break;
            case CMD_SCP_SEND_ADR:
            {
                if(!(mtk_flp_lowpower_query_source() & FLP_TECH_MASK_SENSORS)) {
                    FLPD("sensor not yet started, send stop command\n");
                    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T));
                    flp_msg->type = CMD_FLP_STOP_SENSOR;
                    flp_msg->length = 0;
                    mtk_flp_dc_sensor_send_msg(flp_msg, (flp_msg->length)+sizeof(MTK_FLP_MSG_T));
                    mtk_flp_sys_msg_free(flp_msg);
                    return;
                }
                if(msg->length >= sizeof(MTK_FLP_PDR_INPUT)) {
                    memcpy( &pdr_in, ((INT8*)msg)+ sizeof(MTK_FLP_MSG_T), sizeof(MTK_FLP_PDR_INPUT));
                    FLPD("pdr_in: %d, %d, %d\n",pdr_in.x, pdr_in.y,pdr_in.z);
                }
                flp_loc_available = mtk_flp_lowpower_get_latest_loc(&latest_loc);

                if((flp_loc_available == MTK_FLP_SUCCESS) || !(mtk_flp_lowpower_query_source() & FLP_TECH_MASK_GNSS)) { //other source update loc before
                    if((flp_loc_available == MTK_FLP_ERROR) && !(mtk_flp_lowpower_query_source() & FLP_TECH_MASK_GNSS)) { //if no gnss, treat all other source as sensor only
                        flp_state[0]= 24.841358486;
                        flp_state[1]= 121.01797939;
                        flp_state[2]= 126;
                        memcpy(flp_state_1,flp_state, 3*sizeof(double));

                        //sensor only initial variance
                        memset(flp_variance_1, 0, 9*sizeof(double));
                        flp_variance_1[0] = 1e-10;
                        flp_variance_1[4] = 1e-10;
                        flp_variance_1[8] = 1e-10; //1m
                        memset(&latest_loc,0,sizeof(MTK_FLP_LOCATION_T));
                        has_updated_by_radio = 1;
                        FLPD("enable sensor alone start loc");
                    }

                    if((mtk_flp_lowpower_query_source() & FLP_TECH_MASK_GNSS) && (has_updated_by_radio) && (latest_loc.sources_used != FLP_TECH_MASK_SENSORS)) { //not sensor only
                        memset(flp_variance_1, 0, 9*sizeof(double));
                        flp_variance_1[0] = 9000000.0;
                        flp_variance_1[4] = 9000000.0;
                        flp_variance_1[8] = 9000000.0; //300000km
                        has_updated_by_radio = 0;
                        flp_state[0] = latest_loc.latitude;
                        flp_state[1] = latest_loc.longitude;
                        flp_state[2] = latest_loc.altitude;
                        memcpy(flp_state_1, flp_state, 3*sizeof(double));
                    }

                    if(mpe_update_flp_lla(&latest_loc, pdr_in.x, pdr_in.y, &loc_out) == MTK_FLP_SUCCESS) {
                        //send TPDR
                        mtk_flp_sensor_report_pdr(&pdr_in, loc_out.accuracy);

                        //send location
                        flp_msg = mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T));
                        flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                        flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                        memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc_out, sizeof(MTK_FLP_LOCATION_T));
                        mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

                        FLPD("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d ACC:%f \n",
                            loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used,loc_out.accuracy);

                        if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                            //send DIAG location
                            sprintf(localbuf,"SENS_NTF=LNG:%f LAT:%f ALT:%f", loc_out.longitude, loc_out.latitude, loc_out.altitude);
                            //FLP_TRC("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",
                            //    loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used);

                            flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                            flp_msg1->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
                            flp_msg1->length = strlen(localbuf);
                            memcpy(((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf));
                            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg1);
                        }
                    }
                } else if ((flp_loc_available == MTK_FLP_ERROR) && (mtk_flp_lowpower_query_source() != FLP_TECH_MASK_SENSORS)) {
                    //after 60sec if other source other than sensor did not provide location yet,
                    //sensor will start to output location which started with default location
                    if(mtk_flp_lowpower_query_start_elapsed_time() > MAX_FIRST_TTFF_TIME) {
                        flp_state[0]= 24.841358486;
                        flp_state[1]= 121.01797939;
                        flp_state[2]= 126;
                        memcpy(flp_state_1,flp_state, 3*sizeof(double));

                        //sensor only initial variance
                        memset(flp_variance_1, 0, 9*sizeof(double));
                        flp_variance_1[0] = 1e-10;
                        flp_variance_1[4] = 1e-10;
                        flp_variance_1[8] = 1e-10; //1m
                        memset(&latest_loc,0,sizeof(MTK_FLP_LOCATION_T));
                        has_updated_by_radio = 1;
                        FLPD("enable sensor alone start loc\n");

                        if(mpe_update_flp_lla(&latest_loc, pdr_in.x, pdr_in.y, &loc_out) == MTK_FLP_SUCCESS) {
                            //send TPDR
                            mtk_flp_sensor_report_pdr(&pdr_in, loc_out.accuracy);

                            //send location
                            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T));
                            flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                            flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc_out, sizeof(MTK_FLP_LOCATION_T));
                            mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

                            FLPD("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d ACC:%f \n",
                                loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used,loc_out.accuracy);

                            if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                                //send DIAG location
                                sprintf(localbuf,"SENS_NTF=LNG:%f LAT:%f ALT:%f", loc_out.longitude, loc_out.latitude, loc_out.altitude);
                                //FLP_TRC("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",
                                //    loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used);

                                flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                                flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
                                flp_msg1->length = strlen(localbuf);
                                memcpy( ((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf) );
                                mtk_flp_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);
                            }
                        }
                    }
                }
            }
                break;
            case CMD_SCP_SEND_REBOOT:
                if(mtk_flp_lowpower_query_source() & FLP_TECH_MASK_SENSORS) {
                    FLPD("START PDR");
                    memset(flp_variance_1, 0, 9*sizeof(double));
                    flp_variance_1[0] = 9000000.0;
                    flp_variance_1[4] = 9000000.0;
                    flp_variance_1[8] = 9000000.0;
                    pdr_acc_step_cnt = 0;
                    pdr_acc_step_len = 0;
                    pdr_last_ttick = 0;
                    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T));
                    flp_msg->type = CMD_FLP_START_SENSOR;
                    flp_msg->length = 0;
                    mtk_flp_dc_sensor_send_msg(flp_msg, (flp_msg->length)+sizeof(MTK_FLP_MSG_T));
                    mtk_flp_sys_msg_free(flp_msg);
                }
                break;
            default:
                FLPD("Unknown wifi message type: %x", msg->type);
                break;
        }
    }
}

int mtk_flp_dc_sensor_process(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_BATCH_OPTION_T *option;
    MTK_FLP_MSG_T *ptr;

    option = (MTK_FLP_BATCH_OPTION_T *)((UINT8*)prmsg+sizeof(MTK_FLP_MSG_T));
    FLPD("Sensor Proc msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch( prmsg->type )
    {
    case MTK_FLP_MSG_DC_START_CMD:
        if( option->sources_to_use & FLP_TECH_MASK_SENSORS) {
            FLPD("START PDR");
            memset(flp_variance_1, 0, 9*sizeof(double));
            flp_variance_1[0] = 9000000.0;
            flp_variance_1[4] = 9000000.0;
            flp_variance_1[8] = 9000000.0;
            pdr_acc_step_cnt = 0;
            pdr_acc_step_len = 0;
            pdr_last_ttick = 0;
            ptr = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T));
            ptr->type = CMD_FLP_START_SENSOR;
            ptr->length = 0;
            mtk_flp_dc_sensor_send_msg(ptr, (ptr->length)+sizeof(MTK_FLP_MSG_T));
            mtk_flp_sys_msg_free(ptr);
        }
        break;
    case MTK_FLP_MSG_DC_STOP_CMD:
        {
            FLPD("Stop sensor pdr");
            ptr = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T));
            ptr->type = CMD_FLP_STOP_SENSOR;
            ptr->length = 0;
            mtk_flp_dc_sensor_send_msg(ptr, (ptr->length)+sizeof(MTK_FLP_MSG_T));
            mtk_flp_sys_msg_free(ptr);
        }
        break;
    default:
        FLPD("Unknown pdr message to send");
        break;
    }
    return 0;
}

#else //offload
void mtk_flp_dc_sensor_main(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_MSG_T   *flp_msg=NULL, *flp_msg1 =NULL;
    MTK_FLP_LOCATION_T  loc_out={0}, latest_loc={0};
    MTK_FLP_PDR_INPUT pdr_in = {0};
    char localbuf[512];
    int flp_loc_available = -1;

    if((prmsg == NULL) || (prmsg->length < 0)) {
        FLP_TRC("mtk_flp_dc_sensor_main, recv prmsg is null pointer\r\n");
        return;
    }

    FLP_TRC("sensor main msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch( prmsg->type ) {
        case CMD_FLP_START_SENSOR_RES:
            FLP_TRC("SENSOR start ok");
            break;
        case CMD_FLP_STOP_SENSOR_RES:
            FLP_TRC("SENSOR stop ok");
            break;
        case CMD_SCP_SEND_ADR:
        {
            if(prmsg->length >= sizeof(MTK_FLP_PDR_INPUT)) {
                memcpy( &pdr_in, ((INT8*)prmsg)+ sizeof(MTK_FLP_MSG_T), sizeof(MTK_FLP_PDR_INPUT));
                FLP_TRC("pdr_in: %d, %d, %d\n",pdr_in.x, pdr_in.y,pdr_in.z);
            }
            flp_loc_available = mtk_flp_lowpower_get_latest_loc(&latest_loc);

            if((flp_loc_available == MTK_FLP_SUCCESS) || (mtk_flp_lowpower_query_source() == FLP_TECH_MASK_SENSORS)) { //other source update loc before
                if((flp_loc_available == MTK_FLP_ERROR) && (mtk_flp_lowpower_query_source() == FLP_TECH_MASK_SENSORS)) { //sensor only, never fix before
                    flp_state[0]= 24.841358486;
                    flp_state[1]= 121.01797939;
                    flp_state[2]= 126;
                    memcpy(flp_state_1,flp_state, 3*sizeof(double));

                    //sensor only initial variance
                    memset(flp_variance_1, 0, 9*sizeof(double));
                    flp_variance_1[0] = 1e-10;
                    flp_variance_1[4] = 1e-10;
                    flp_variance_1[8] = 1e-10; //1m
                    memset(&latest_loc,0,sizeof(MTK_FLP_LOCATION_T));
                    has_updated_by_radio = 1;
                    FLP_TRC("enable sensor alone start loc");
                }

                if((mtk_flp_lowpower_query_source() != FLP_TECH_MASK_SENSORS) && (has_updated_by_radio) && (latest_loc.sources_used != FLP_TECH_MASK_SENSORS)) { //not sensor only
                    memset(flp_variance_1, 0, 9*sizeof(double));
                    flp_variance_1[0] = 9000000.0;
                    flp_variance_1[4] = 9000000.0;
                    flp_variance_1[8] = 9000000.0; //300000km
                    has_updated_by_radio = 0;
                    flp_state[0] = latest_loc.latitude;
                    flp_state[1] = latest_loc.longitude;
                    flp_state[2] = latest_loc.altitude;
                    memcpy(flp_state_1, flp_state, 3*sizeof(double));
                }

                if(mpe_update_flp_lla(&latest_loc, pdr_in.x, pdr_in.y, &loc_out) == MTK_FLP_SUCCESS) {
                    //send TPDR
                    mtk_flp_sensor_report_pdr(&pdr_in, loc_out.accuracy);

                    //send location
                    flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T));
                    flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                    flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc_out, sizeof(MTK_FLP_LOCATION_T));
                    mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

                    FLP_TRC("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d ACC:%f \n",
                        loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used,loc_out.accuracy);

                    if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                        //send DIAG location
                        sprintf(localbuf,"SENS_NTF=LNG:%f LAT:%f ALT:%f", loc_out.longitude, loc_out.latitude, loc_out.altitude);
                        //FLP_TRC("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",
                        //    loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used);

                        flp_msg1 = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                        flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
                        flp_msg1->length = strlen(localbuf);
                        memcpy( ((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf) );
                        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);
                    }
                }
            } else if ((flp_loc_available == MTK_FLP_ERROR) && (mtk_flp_lowpower_query_source() != FLP_TECH_MASK_SENSORS)) {
                //after 60sec if other source other than sensor did not provide location yet,
                //sensor will start to output location which started with default location
                if(mtk_flp_lowpower_query_start_elapsed_time() > MAX_FIRST_TTFF_TIME) {
                    flp_state[0]= 24.841358486;
                    flp_state[1]= 121.01797939;
                    flp_state[2]= 126;
                    memcpy(flp_state_1,flp_state, 3*sizeof(double));

                    //sensor only initial variance
                    memset(flp_variance_1, 0, 9*sizeof(double));
                    flp_variance_1[0] = 1e-10;
                    flp_variance_1[4] = 1e-10;
                    flp_variance_1[8] = 1e-10; //1m
                    memset(&latest_loc,0,sizeof(MTK_FLP_LOCATION_T));
                    has_updated_by_radio = 1;
                    FLP_TRC("enable sensor alone start loc\n");

                    if(mpe_update_flp_lla(&latest_loc, pdr_in.x, pdr_in.y, &loc_out) == MTK_FLP_SUCCESS) {
                        //send TPDR
                        mtk_flp_sensor_report_pdr(&pdr_in, loc_out.accuracy);

                        //send location
                        flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T));
                        flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
                        flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
                        memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc_out, sizeof(MTK_FLP_LOCATION_T));
                        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

                        FLP_TRC("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d ACC:%f \n",
                            loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used,loc_out.accuracy);

                        if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                            //send DIAG location
                            sprintf(localbuf,"SENS_NTF=LNG:%f LAT:%f ALT:%f", loc_out.longitude, loc_out.latitude, loc_out.altitude);
                            //FLP_TRC("SENS_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",
                            //    loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used);

                            flp_msg1 = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                            flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
                            flp_msg1->length = strlen(localbuf);
                            memcpy( ((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf) );
                            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);
                        }
                    }
                }
            }
        }
            break;
        default:
            FLP_TRC("Unknown wifi message type: %x", prmsg->type);
            break;

    }
}

int mtk_flp_dc_sensor_process(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_BATCH_OPTION_T *option;
    MTK_FLP_MSG_T *ptr;

    option = (MTK_FLP_BATCH_OPTION_T *)((UINT8*)prmsg+sizeof(MTK_FLP_MSG_T));
    FLP_TRC("Sensor Proc msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch(prmsg->type) {
    case MTK_FLP_MSG_DC_START_CMD:
        if(option->sources_to_use & FLP_TECH_MASK_SENSORS) {
            FLP_TRC("START PDR");
            memset(flp_variance_1, 0, 9*sizeof(double));
            flp_variance_1[0] = 9000000.0;
            flp_variance_1[4] = 9000000.0;
            flp_variance_1[8] = 9000000.0;
            pdr_acc_step_cnt = 0;
            pdr_acc_step_len = 0;
            pdr_last_ttick = 0;
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
            ptr->type = CMD_FLP_START_SENSOR;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_SCP,ptr);
        }
        break;
    case MTK_FLP_MSG_DC_STOP_CMD:
        {
            FLP_TRC("Stop sensor pdr");
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
            ptr->type = CMD_FLP_STOP_SENSOR;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_SCP,ptr);
        }
        break;
    default:
        FLP_TRC("Unknown pdr message to send");
        break;
    }
    return 0;
}

#endif //endof offload

