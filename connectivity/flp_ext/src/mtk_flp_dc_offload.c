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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/stat.h>

//fused_location.h located in: alps\hardware\libhardware\include\hardware
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#include "mtk_flp_dc.h"
#include <math.h>

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FLP_DC_OL"

#define FLPE(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define FLPD(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define FLPW(...)   mtk_flp_sys_dbg(MTK_FLP_LOG_WARN, LOG_TAG, __VA_ARGS__)

#else
#define FLPD(...)
#define FLPW(...)
#define FLPE(...)
#endif

#define MTKFLP_OFL_SOCK_SVR_PATH   "/data/gps_mnl/flp2mnld"
#define MTKFLP_OFL_SOCK_CLI_PATH   "/data/gps_mnl/mnld2flp"

#define SOCKET_OFL_RETRY_CNT       10
#define SOCKET_OFL_RETRY_INTERVAL  100000
int flp_ofl_server_fd = -1;
int flp_ofl_client_fd = -1;
static struct sockaddr_un ofl_client_sockaddr;
static struct sockaddr_un ofl_server_sockaddr;

#define FLP_OFL_SOCK_BUF_LEN    65535
char ofl_sockbuf[FLP_OFL_SOCK_BUF_LEN];

enum{
    FLP_MNL_CMD_UNKNOWN  = -1,
    FLP_MNL_CMD_INIT     = 0x00,
    FLP_MNL_CMD_DEINIT   = 0x01,
    FLP_MNL_CMD_LOOPBACK = 0x02,
    FLP_MNL_CMD_DATA_SEND= 0x03,
    FLP_MNL_CMD_DATA_RECV= 0x04,
    FLP_MNL_CMD_NETWORK_STATUS= 0x05,
    FLP_MNL_CMD_REBOOT_DONE= 0x06,
    FLP_MNL_CMD_GPS_OPEN_DONE = 0x07,
    FLP_MNL_CMD_GPS_STOP_DONE = 0x08,
    FLP_MNL_CMD_REPORT_LOC = 0x09,
};

enum{
    CMD_FLP_REPORT_GNSS_LOC = 58,
};

#define OFFLOAD_PAYLOAD_LEN 600
typedef struct
{
    UINT32 type;
    UINT32 length;
    UINT8  data[OFFLOAD_PAYLOAD_LEN];
} MTK_FLP_OFFLOAD_MSG_T;

void mtk_flp_dc_offload_main(void);
INT32 mtk_flp_dc_offload_process(MTK_FLP_MSG_T *prmsg);
extern int mtk_flp_dc_cell_process(MTK_FLP_MSG_T *prmsg);
extern void mtk_flp_dc_gnss_main(MTK_FLP_MSG_T *prmsg);
extern INT32 mtk_flp_hal_get_sys_mode();


volatile int mnld_ret = -1;

#if 1
#define OFFLOAD_SYS 1
#endif
#if (OFFLOAD_SYS==1)
DcReportThread    Report_thrd_OFFLOAD = (DcReportThread)mtk_flp_dc_offload_main;        //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_OFFLOAD = (DcProcessFunction)mtk_flp_dc_offload_process; //need to set to NULL for not activating this location source.
#else
DcReportThread    Report_thrd_OFFLOAD = NULL;        //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_OFFLOAD = NULL; //need to set to NULL for not activating this location source.
#endif

static int mtk_flp_dc_offload_connect_socket(void) {

    /* Create socket with GPS HAL */
    if((flp_ofl_client_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1) {
        FLPD("open client sock failed\r\n");
        return -1;
    }

    unlink(MTKFLP_OFL_SOCK_CLI_PATH);
    memset(&ofl_client_sockaddr, 0, sizeof(ofl_client_sockaddr));
    ofl_client_sockaddr.sun_family = AF_LOCAL;
    strcpy(ofl_client_sockaddr.sun_path, MTKFLP_OFL_SOCK_CLI_PATH);
    if(bind(flp_ofl_client_fd, (struct sockaddr *)&ofl_client_sockaddr, sizeof(ofl_client_sockaddr)) < 0) {
        FLPD("Bind client error: %s\n", strerror(errno));
        return -1;
    }

    //int res = chmod(MTKFLP_OFL_SOCK_CLI_PATH, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP);
    int res = chmod(MTKFLP_OFL_SOCK_CLI_PATH, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP| S_IROTH |S_IWOTH |S_IXOTH);

    FLPD("chmod res = %d\n", res);  //770<--mode

    if((flp_ofl_server_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        FLPD("open server sock failed\r\n");
        return -1;
    }
    memset(&ofl_server_sockaddr, 0, sizeof(ofl_server_sockaddr));
    ofl_server_sockaddr.sun_family = AF_LOCAL;
    strcpy(ofl_server_sockaddr.sun_path, MTKFLP_OFL_SOCK_SVR_PATH);
    return 0;
}

static int mtk_flp_dc_offload_send_msg(MTK_FLP_MSG_T *msg, int len) {
    MTK_FLP_MSG_T   *flp_msg;
    INT32 init_flag;
    FLPD("Sending %d to OFFLOAD HAL", len);
    if(flp_ofl_server_fd <= 0) {
        FLPD("DC Send to MNL: UDP socket not connectedd");
        return -1;
    }
    //FLPD("GNSS Svr fd: %d",flp_gnss_server_fd);
    //FLPD("GNSS Svr.sun_path,%s",gnss_server_sockaddr.sun_path);
    //FLPD("GNSS Svr.sun_family,%d",gnss_server_sockaddr.sun_family);

    /* Send cmdline to HAL*/
    if(sendto(flp_ofl_server_fd, msg, len, 0, (struct sockaddr*)&ofl_server_sockaddr, sizeof(ofl_server_sockaddr)) < 0) {
        FLPD("DC send to GSNN  fail:%s", strerror(errno));
    }

    if(msg->type == FLP_MNL_CMD_INIT) {
        init_flag = 1;
        flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(INT32));
        flp_msg->type = MTK_FLP_MSG_SYS_REPORT_MNL_INIT_CMD;
        flp_msg->length = sizeof(INT32);
        memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&init_flag), sizeof(INT32));
        mtk_flp_sys_udp_send_msg(MTK_FLP2HAL, flp_msg);
        mtk_flp_sys_msg_free(flp_msg);
        mtk_flp_sys_sleep(1000);
    } else if(msg->type == FLP_MNL_CMD_DEINIT) {
        init_flag = 0;
        flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(INT32));
        flp_msg->type = MTK_FLP_MSG_SYS_REPORT_MNL_INIT_CMD;
        flp_msg->length = sizeof(INT32);
        memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&init_flag), sizeof(INT32));
        mtk_flp_sys_udp_send_msg(MTK_FLP2HAL, flp_msg);
        mtk_flp_sys_msg_free(flp_msg);
    }
    return 0;
}

static int mtk_flp_dc_offload_recv_msg(MTK_FLP_OFFLOAD_MSG_T **msg) {
    int recvlen = 0;
    socklen_t addrlen = sizeof(ofl_client_sockaddr);

    *msg = NULL;
    FLPD("Receiving from MNL");

    recvlen = recvfrom(flp_ofl_client_fd, &ofl_sockbuf[0], sizeof(MTK_FLP_OFFLOAD_MSG_T), 0, (struct sockaddr*)&ofl_client_sockaddr, &addrlen);

    if(recvlen >= (int)sizeof(MTK_FLP_OFFLOAD_MSG_T)) {//normal message
        //FLPD("%d bytes received", recvlen);
        *msg = (MTK_FLP_OFFLOAD_MSG_T *)ofl_sockbuf; //1 //NOTE: Does not allocate new message here.
        return recvlen;
    } else {
        FLPD("Error recveiving GNSS UDP data: %s", strerror(errno));
        return -1;
    }
}

static int mtk_flp_dc_offload_loopback_recv_msg(MTK_FLP_OFFLOAD_MSG_T **msg) {
    int recvlen = 0;
    socklen_t addrlen = sizeof(ofl_client_sockaddr);

    *msg = NULL;
    FLPD("Receiving from MNL");

    recvlen = recvfrom(flp_ofl_client_fd, &ofl_sockbuf[0], sizeof(MTK_FLP_OFFLOAD_MSG_T), 0, (struct sockaddr*)&ofl_client_sockaddr, &addrlen);

    if(recvlen >= (int)sizeof(MTK_FLP_OFFLOAD_MSG_T)) {//normal message
        //FLPD("%d bytes received", recvlen);
       *msg = (MTK_FLP_OFFLOAD_MSG_T *)ofl_sockbuf; //1 //NOTE: Does not allocate new message here.
        return recvlen;
    } else {
        FLPD("Error recveiving GNSS UDP data: %s", strerror(errno));
        return -1;
    }
}

#if 0
static int mtk_flp_dc_offload_test_loopback(void)
{
    int i=0;
    MTK_FLP_OFFLOAD_MSG_T *ptr=NULL;


    ptr = (MTK_FLP_OFFLOAD_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_OFFLOAD_MSG_T));
    ptr->type = FLP_MNL_CMD_LOOPBACK;
    ptr->length = OFFLOAD_PAYLOAD_LEN;

    for(i=0;i<OFFLOAD_PAYLOAD_LEN;i++){
            ptr->data[i]=i;
    }
    mtk_flp_dc_offload_send_msg(ptr, sizeof(MTK_FLP_OFFLOAD_MSG_T));
    return 0;
}

#else

static int mtk_flp_dc_offload_test_loopback(int mode) {
    int i=0;
    MTK_FLP_OFFLOAD_MSG_T *ptr=NULL;

    ptr = (MTK_FLP_OFFLOAD_MSG_T *)mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_OFFLOAD_MSG_T));
    ptr->type = (UINT32)mode;
    ptr->length = OFFLOAD_PAYLOAD_LEN;

    for(i = 0; i < OFFLOAD_PAYLOAD_LEN; i++) {
            ptr->data[i]=i;
    }
    mtk_flp_dc_offload_send_msg((MTK_FLP_MSG_T *)ptr, sizeof(MTK_FLP_OFFLOAD_MSG_T));
    return 0;
}

#endif

static void mtk_flp_dc_offload_init() {
    if(mtk_flp_dc_offload_connect_socket()) {
        return;
    }
}

static void mtk_flp_dc_loc_rearrange(MTK_FLP_LOCATION_T *loc_in, MTK_FLP_LOCATION_T *loc_out) {
    loc_out->size = loc_in->size;
    loc_out->sources_used = loc_in->sources_used;
    loc_out->flags = loc_in->flags;
    loc_out->latitude = loc_in->latitude;
    loc_out->longitude = loc_in->longitude;
    loc_out->accuracy = loc_in->accuracy;
    loc_out->altitude = loc_in->altitude;
    loc_out->speed = loc_in->speed;
    loc_out->bearing = loc_in->bearing;
    loc_out->timestamp = loc_in->timestamp;

    FLPD("Loc from CNN:LNG:%f LAT:%f ALT:%f ACC:%f SPD:%f BEARING:%f, FLAGS:%04X SOURCES:%08X Timestamp:%lld",
    loc_out->longitude, loc_out->latitude, loc_out->altitude, loc_out->accuracy,
    loc_out->speed, loc_out->bearing, loc_out->flags, loc_out->sources_used, loc_out->timestamp);
    FLPD("Loc processed:LNG:%f LAT:%f ALT:%f ACC:%f SPD:%f BEARING:%f, FLAGS:%04X SOURCES:%08X Timestamp:%lld",
    loc_in->longitude, loc_in->latitude, loc_in->altitude, loc_in->accuracy,
    loc_in->speed, loc_in->bearing, loc_in->flags, loc_in->sources_used, loc_in->timestamp);

}


/************************************************************************/
//  Receiver location/measurement from offload kernel and report back to HAL
/************************************************************************/
void mtk_flp_dc_offload_main(void) {
    int i, retlen, network_status, network_type;
    MTK_FLP_OFFLOAD_MSG_T *msg;
    MTK_FLP_MSG_T   *flp_msg, *flp_msg1;
    MTK_FLP_LOCATION_T  loc_in, loc_out;
    char localbuf[512];
    UINT8* msg_dbg = NULL;
    INT32 sysMode;

    FLPD("DC Offload Main starts....");

    mtk_flp_dc_offload_init();

    while(1) {
        retlen = mtk_flp_dc_offload_recv_msg((MTK_FLP_OFFLOAD_MSG_T **)&msg);
        if(retlen <= 0) {
            continue;
        }
        FLPD("mtk_flp_dc_offload_recv_msg type = %d, len = %d \n",  msg->type, msg->length);
        if(msg->type == FLP_MNL_CMD_REBOOT_DONE) {
            FLPD("FLP_MNL_CMD_REBOOT_DONE");
            if(mnld_ret == 0) {
                //send init message to mnld first
                flp_msg = mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + sizeof(sysMode)); //send header only, no message body
                flp_msg->type = FLP_MNL_CMD_INIT;
                flp_msg->length = sizeof(sysMode);
                sysMode = mtk_flp_hal_get_sys_mode();
                memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&sysMode), sizeof(sysMode));
                FLPD("Send to mnld for re-init,message type:%x, len:%d, mode:%d", flp_msg->type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)), sysMode);
                mnld_ret = mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                mtk_flp_sys_msg_free(flp_msg);
                //offload mode send mnld reboot done to hal
                if(mtk_flp_hal_get_sys_mode() == FLP_OFFLOAD_MODE) {
                    flp_msg = mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T));
                    flp_msg->type = MTK_FLP_MSG_SYS_MNLD_REBOOT_DONE_NTF;
                    flp_msg->length = 0;
                    mtk_flp_sys_udp_send_msg(MTK_FLP2HAL, flp_msg);
                    mtk_flp_sys_msg_free(flp_msg);
                }
            }
        } else if (msg->type == FLP_MNL_CMD_REPORT_LOC) {
            flp_msg = mtk_flp_sys_msg_alloc(sizeof(MTK_FLP_MSG_T) + msg->length);
            flp_msg->type = CMD_FLP_REPORT_GNSS_LOC;
            flp_msg->length = msg->length;
            memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)msg)+sizeof(MTK_FLP_MSG_T), msg->length);
            mtk_flp_dc_gnss_main(flp_msg);
            mtk_flp_sys_msg_free(flp_msg);
        } else if(msg->type == FLP_MNL_CMD_GPS_OPEN_DONE) {
            FLPD("FLP_MNL_CMD_GPS_OPEN_DONE");
        } else if(msg->type == FLP_MNL_CMD_GPS_STOP_DONE) {
            FLPD("FLP_MNL_CMD_GPS_STOP_DONE");
        } else if(msg->type == FLP_MNL_CMD_DATA_RECV) {
            if(mnld_ret == -1) {
                FLPD("MNLD not init. Drop FLP_MNL_CMD_DATA_RECV");
                continue;
            }
            flp_msg = mtk_flp_sys_msg_alloc( (msg->length) );

            if(flp_msg == NULL) {
               return;
            }
            memcpy( ((INT8*)flp_msg), ((INT8*)msg)+ sizeof(MTK_FLP_MSG_T), (msg->length) );

            switch(flp_msg->type) {
            //case CMD_FLP_GET_GEOFENCE_PROP:
            case MTK_FLP_MSG_HAL_STOP_RSP:
                //send response to HAL
                FLPD("Received OFFLOAD message type: %x, len = %d", flp_msg->type,(flp_msg->length));
                if(flp_msg->type < MTK_FLP_MSG_END) {
                    mtk_flp_hal_proc(flp_msg);
                }

                //send stop message to mnld
                flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(sysMode));
                flp_msg1->type = FLP_MNL_CMD_DEINIT;
                flp_msg1->length = sizeof(sysMode);
                sysMode = mtk_flp_hal_get_sys_mode();
                FLPD("Send to mnld for deinit,message type:%x, len:%d, mode:%d", flp_msg1->type, flp_msg1->length, sysMode);
                mtk_flp_dc_offload_send_msg(flp_msg1, sizeof(MTK_FLP_MSG_T));
                mnld_ret = -1;
                mtk_flp_sys_msg_free(flp_msg);
                mtk_flp_sys_msg_free(flp_msg1);
                FLPD("mnld deinit success");
                break;
            case MTK_FLP_MDS_DC_CELL_DATA_TO_AP:
                FLPD("Received cell message type: %x, len = %d", flp_msg->type,(flp_msg->length));
                mtk_flp_dc_cell_process(flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
                break;
            case MTK_FLP_MSG_MNLD_NETWORK_STATUS_NTF:
                FLPD("Received network status, message type: %x, len = %d", flp_msg->type,(flp_msg->length));
                memcpy(&network_status, ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), sizeof(int));
                memcpy(&network_type, ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T)+sizeof(int), sizeof(int));
                FLPD("network status: %d, network type: %d \n", network_status, network_type);
                mtk_flp_dc_cell_process(flp_msg);
                mtk_flp_sys_msg_free(flp_msg);
                break;
            default:
                FLPD("Received OFFLOAD message type: %x, len = %d", flp_msg->type,(flp_msg->length));
                if(flp_msg->type < MTK_FLP_MSG_END) {
                    if(flp_msg->type == MTK_FLP_MSG_HAL_REPORT_LOC_NTF) {
                       FLPD("size check: %d %d \n", (flp_msg->length),sizeof(MTK_FLP_LOCATION_T) );
                       memcpy(&loc_in, ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), sizeof(MTK_FLP_LOCATION_T));
                       mtk_flp_dc_loc_rearrange(&loc_in,&loc_out);
                       flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_LOCATION_T));
                       flp_msg1->type = flp_msg->type;
                       flp_msg1->length = sizeof(MTK_FLP_LOCATION_T);
                       memcpy(((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), &loc_out,sizeof(MTK_FLP_LOCATION_T));
                       mtk_flp_hal_proc(flp_msg1);
                       mtk_flp_sys_msg_free(flp_msg1);
                    } else {
                        mtk_flp_hal_proc(flp_msg);
                    }
                }

                mtk_flp_sys_msg_free(flp_msg);
                break;
            }
        }
    }

}
#if 0
void mtk_flp_dc_offload_main(void)
{
    int i, retlen;
    MTK_FLP_OFFLOAD_MSG_T *msg;
    MTK_FLP_MSG_T   *flp_msg;
    MTK_FLP_LOCATION_T  loc;
    char localbuf[512];


    FLPD("DC Offload Main starts....");

    mtk_flp_dc_offload_init();

    while(1)
    {
        retlen = mtk_flp_dc_offload_recv_msg((MTK_FLP_OFFLOAD_MSG_T **)&msg);
        if( retlen <= 0){
            continue;
        }
        switch( msg->type ){
//        case CMD_FLP_GET_GEOFENCE_PROP:
        default:
            FLPD("Received OFFLOAD message type: %x", msg->type);
            break;

        }

    }

}

#endif


/************************************************************************/
//  Process msgs from HAL and send to offload kernel
/************************************************************************/
INT32 mtk_flp_dc_offload_process(MTK_FLP_MSG_T *prmsg) {
    UINT8* pData = NULL;
    INT8* msg_dbg = NULL;
    double add_bit = 0.0;
    MTK_FLP_MSG_T* flp_msg=NULL;
    MTK_FLP_MSG_T* flp_msg1=NULL;
    MTK_FLP_MSG_T flp_init_msg;
    INT32 sysMode;
    MTK_FLP_RECOVERY_INFO_T sysInfo;

    if(prmsg == NULL) {
        FLPD("mtk_flp_dc_offload_process, recv prmsg is null pointer\n");
        return MTK_FLP_ERROR;
    }

    if(prmsg->length>0) {
        pData = ((UINT8*)prmsg + sizeof(MTK_FLP_MSG_T));
    }

    FLPD("mtk_flp_dc_offload_process, recv prmsg, type: 0x%02x, len:%d\r\n", prmsg->type, prmsg->length);

    switch(prmsg->type) {
        case MTK_FLP_MSG_HAL_INIT_CMD: //offload-based
        case MTK_FLP_MSG_DC_INIT_CMD: // host-based
            //mtk_flp_dc_offload_test_loopback(FLP_MNL_CMD_LOOPBACK);
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)); //send header only, no message body
            flp_msg->type = FLP_MNL_CMD_REBOOT_DONE;
            flp_msg->length = 0; //prmsg->length;
            FLPD("Send to mnld for reboot done, message type:%x, len:%d", flp_msg->type, flp_msg->length);
            mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
            mtk_flp_sys_msg_free(flp_msg);
            break;
        case MTK_FLP_MSG_SYS_RECOVERY_CMD:
            memcpy(&sysInfo, (INT32 *)((UINT8*)prmsg+sizeof(MTK_FLP_MSG_T)), sizeof(MTK_FLP_RECOVERY_INFO_T));
            if(mtk_flp_hal_get_sys_mode() == FLP_OFFLOAD_MODE) {
                if(sysInfo.init_mnld_flag) {
                    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(sysMode)); //send header only, no message body
                    flp_msg->type = FLP_MNL_CMD_INIT;
                    flp_msg->length = sizeof(sysMode);
                    sysMode = mtk_flp_hal_get_sys_mode();
                    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&sysMode), sizeof(sysMode));
                    FLPD("Send to mnld for initialization,message type:%x, len:%d, mode:%d", flp_msg->type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)), sysMode);
                    mnld_ret = mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                    mtk_flp_sys_msg_free(flp_msg);
                    while(mnld_ret == -1) {
                       //wait until mnld finish initialization
                    };
                }
            } else {
                flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)); //send header only, no message body
                flp_msg->type = FLP_MNL_CMD_REBOOT_DONE;
                flp_msg->length = 0; //prmsg->length;
                FLPD("Send to mnld for reboot done, message type:%x, len:%d", flp_msg->type, flp_msg->length);
                mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                mtk_flp_sys_msg_free(flp_msg);
            }
            break;
        case MTK_FLP_MSG_HAL_START_CMD:
        case MTK_FLP_MSG_HAL_DIAG_INJECT_DATA_NTF:
        case MTK_FLP_MSG_OFL_GEOFENCE_CMD:
            if(mnld_ret != 0) {
                //send init message to mnld first
                flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(sysMode)); //send header only, no message body
                flp_msg->type = FLP_MNL_CMD_INIT;
                flp_msg->length = sizeof(sysMode);
                sysMode = mtk_flp_hal_get_sys_mode();
                memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&sysMode), sizeof(sysMode));
                FLPD("Send to mnld for initialization,message type:%x, len:%d, mode:%d", flp_msg->type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)), sysMode);
                mnld_ret = mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                mtk_flp_sys_msg_free(flp_msg);
                while(mnld_ret == -1) {
                   //wait until mnld finish initialization
                };

                // send init message to ofl flp
                if(prmsg->type != MTK_FLP_MSG_OFL_GEOFENCE_CMD) {
                    //send init flp message to ofl flp
                    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)); //MTK_FLP_MSG_HAL_INIT_CMD carry no buffer
                    flp_msg->type = FLP_MNL_CMD_DATA_SEND;
                    flp_msg->length = sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length]
                    flp_init_msg.type = MTK_FLP_MSG_HAL_INIT_CMD;
                    flp_init_msg.length = 0;
                    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&flp_init_msg), sizeof(MTK_FLP_MSG_T));

                    FLPD("Send to OFL FLP,message type:%x, len:%d", flp_init_msg.type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                    mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                    mtk_flp_sys_msg_free(flp_msg);
                }
            }

            //send flp message to ofl flp
            flp_msg1 = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)+prmsg->length);
            flp_msg1->type = FLP_MNL_CMD_DATA_SEND;
            flp_msg1->length = prmsg->length + sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length+buff for flp recognition]

            if(flp_msg1->length > 0) {
               memcpy( ((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), ((INT8*)prmsg), (sizeof(MTK_FLP_MSG_T)+ prmsg->length));
            }
            FLPD("Send to OFL FLP,message type:%x, len:%d", prmsg->type, (flp_msg1->length + sizeof(MTK_FLP_MSG_T)));
            mtk_flp_dc_offload_send_msg(flp_msg1, (flp_msg1->length + sizeof(MTK_FLP_MSG_T)));
            mtk_flp_sys_msg_free(flp_msg1);
            break;
        case MTK_FLP_MSG_HAL_STOP_CMD:
               //send msg stop mnl first
               flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)); //MTK_FLP_MSG_HAL_INIT_CMD carry no buffer
               flp_msg->type = FLP_MNL_CMD_DATA_SEND;
               flp_msg->length = sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length]
               flp_init_msg.type = MTK_FLP_MSG_HAL_STOP_CMD;
               flp_init_msg.length = 0;
               memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&flp_init_msg), sizeof(MTK_FLP_MSG_T));
               FLPD("Send to OFL FLP,message type:%x, len:%d", flp_init_msg.type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
               mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
               mtk_flp_sys_msg_free(flp_msg);
            break;

        case MTK_FLP_MSG_DC_START_CMD: // host-based start gnss
            if(mnld_ret != 0) {
                //send init message to mnld first
                flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(sysMode)); //send header only, no message body
                flp_msg->type = FLP_MNL_CMD_INIT;
                flp_msg->length = sizeof(sysMode);
                sysMode = mtk_flp_hal_get_sys_mode();
                memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&sysMode), sizeof(sysMode));
                FLPD("Send to mnld for initialization,message type:%x, len:%d, mode:%d", flp_msg->type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)), sysMode);
                mnld_ret = mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                mtk_flp_sys_msg_free(flp_msg);
                while(mnld_ret == -1) {
                   //wait until mnld finish initialization
                };
            }
            /*
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)); //MTK_FLP_MSG_HAL_INIT_CMD carry no buffer
            flp_msg->type = FLP_MNL_CMD_DATA_SEND;
            flp_msg->length = sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length]
            flp_init_msg.type = CMD_FLP_START_GNSS_REQ;
            flp_init_msg.length = 0;
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&flp_init_msg), sizeof(MTK_FLP_MSG_T));
            FLPD("Send to mnld for start gnss, message type:%x, len:%d", flp_init_msg.type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
            mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
            */
            break;
        case MTK_FLP_MSG_DC_STOP_CMD:
            /*
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+sizeof(MTK_FLP_MSG_T)); //MTK_FLP_MSG_HAL_INIT_CMD carry no buffer
            flp_msg->type = FLP_MNL_CMD_DATA_SEND;
            flp_msg->length = sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length]
            flp_init_msg.type = CMD_FLP_STOP_GNSS_REQ;
            flp_init_msg.length = 0;
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)&flp_init_msg), sizeof(MTK_FLP_MSG_T));
            FLPD("Send to mnld for stop gnss, message type:%x, len:%d", flp_init_msg.type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
            mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
            */
           //send stop message to mnld
            flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(sysMode)); //send header only, no message body
            flp_msg->type = FLP_MNL_CMD_DEINIT;
            flp_msg->length = sizeof(sysMode);
            sysMode = mtk_flp_hal_get_sys_mode();
            FLPD("Send to mnld for deinit,message type:%x, len:%d, mode:%d", flp_msg->type, flp_msg->length, sysMode);
            mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
            mtk_flp_sys_msg_free(flp_msg);
            mnld_ret = -1;
            break;
        case MTK_FLP_MSG_CONN_SCREEN_STATUS:
            FLPD("screen dbg,mnld_ret=%d",mnld_ret);
            if(mnld_ret == 0) {
                flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_MSG_T)+prmsg->length);
                flp_msg->type = FLP_MNL_CMD_DATA_SEND;  //for loopback test between mnld & FLP AP  use FLP_MNL_CMD_LOOPBACK
                flp_msg->length = (prmsg->length) + sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length+buff for flp recognition]
                if(flp_msg->length > 0) {
                   memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)prmsg), (sizeof(MTK_FLP_MSG_T)+ (prmsg->length)));
                }
                FLPD("Send to OFL FLP,message type:%x, len:%d", prmsg->type, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                mtk_flp_sys_msg_free(flp_msg);
            } else {
                FLPD("free screen dbg");
                //mtk_flp_sys_msg_free(prmsg);
            }
            break;
        default:
            if(mtk_flp_hal_get_sys_mode() == FLP_OFFLOAD_MODE) {
                if(mnld_ret == 0) {
                    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T)+ sizeof(MTK_FLP_MSG_T)+prmsg->length);
                    flp_msg->type = FLP_MNL_CMD_DATA_SEND;  //for loopback test between mnld & FLP AP  use FLP_MNL_CMD_LOOPBACK
                    flp_msg->length = (prmsg->length) + sizeof(MTK_FLP_MSG_T); // data sent structure: [type, length for AP_conn recognition] + [type,length+buff for flp recognition]
                    if( flp_msg->length > 0) {
                       memcpy(((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)prmsg), (sizeof(MTK_FLP_MSG_T)+ (prmsg->length)));
                    }
                    mtk_flp_dc_offload_send_msg(flp_msg, (flp_msg->length + sizeof(MTK_FLP_MSG_T)));
                    mtk_flp_sys_msg_free(flp_msg);
                } else {
                    FLPD("mnld_ret=%d", mnld_ret);
                }
            }
            break;
    }

    return 0;

}
