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
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifndef K2_PLATFORM
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include "mtk_flp_sys.h"
#include "mtk_flp.h"
#include "mtk_flp_dc.h"
#else
#include "mtk_flp_connsys_sys.h"
#include "mtk_gps_bora_flp.h"
#endif

#ifdef DEBUG_LOG

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FLP_DC_WIFI"

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

#define WIFI_PEER_RECORD    10

//*****************************************
//  Structures and enums
//*****************************************
/* Message from FLP HAL to/from WIFI HAL */
enum{
    /* from FLP to WIFI */
    CMD_FLP_START_WIFI_REQ = 0x00, //no payload
    CMD_FLP_STOP_WIFI_REQ,         //no payload
    CMD_FLP_WIFI_SET_MODE_REQ,     //payload, flp set wifi to operate in certain mode

    /* from WIFI to FLP */
    CMD_FLP_START_WIFI_RES = 0x10, //no payload,
    CMD_FLP_STOP_WIFI_RES,         //no payload
    CMD_FLP_WIFI_SET_MODE_RES,     //payload, response from wifi to flp
    CMD_WIFI_FLP_CHANGE_MODE_NTF,  //payload, wifi inform flp the change of wifi op mode
    CMD_WIFI_ON_NTF_TO_FLP,        //no payload
    CMD_WIFI_REPORT_LOC,           //payload, refer MTK_FLP_LOCATION_T
    CMD_WIFI_REPORT_PEER_INFO,     //payload, refer MTK_FLP_WIFI_PEER_T
};

/* WIFI operation mode*/
enum{
    WIFI_NORMAL_MODE = 0x00,
    WIFI_FREE_RUNNING_MODE,
    WIFI_PERIODIC_10SEC,
    WIFI_PERIODIC_20SEC,
};

/* FLP WIFI communication success/failed */
enum{
    FLP_SUCCESS = 0x00,
    FLP_NOT_READY,
    FLP_BUSY,
};

typedef struct {
    INT8 ssid[32];
    UINT8 mac[6];
    INT8 rssi;
    double longtitude;
    double latitude;
    double altitude;
    UINT32  range; //distance in cm
}MTK_FLP_WIFI_PEER_T;

typedef struct {
    INT8 ssid[32];
    UINT8 mac[6];
    double longtitude;
    double latitude;
}MTK_FLP_WIFI_PEER_RECORD_T;

typedef enum MTK_FLP_DC_WIFI_STATE
{
    DC_WIFI_IDLE = 0,
    DC_WIFI_RTT_ENABLED,

}MTK_FLP_DC_WIFI_STATE;


#ifndef K2_PLATFORM //un-offload
void mtk_flp_dc_wifi_main(void);

int mtk_flp_dc_wifi_process(MTK_FLP_MSG_T *prmsg);

//*****************************************
// Report thread function ball backs
//*****************************************
#define DC_WIFI_ENABLE 0
#if(DC_WIFI_ENABLE==1)
DcReportThread    Report_thrd_WIFI =  (DcReportThread)mtk_flp_dc_wifi_main;      //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_WIFI =  (DcProcessFunction)mtk_flp_dc_wifi_process;  //need to set to NULL for not activating this location source.
#else
DcReportThread    Report_thrd_WIFI = NULL;      //need to set to NULL for not activating this location source.
DcProcessFunction    Process_Func_WIFI = NULL;  //need to set to NULL for not activating this location source.
#endif
#define DC_WIFI_UDP_SVR_PATH    "/data/misc/wifi/sockets/wlan0"
#define DC_WIFI_UDP_CLI_PATH    "/data/misc/wifi/sockets/flpclient"
#define DC_WIFI_UDP_MNT_PATH    "/data/misc/wifi/sockets/flpclient_monitor"

static int flp_wifi_socket_fd = -1;
static int flp_wifi_monitor_fd = -1;
static struct sockaddr_un sock_wifi_svr_addr;
static struct sockaddr_un sock_wifi_cli_addr;
static struct sockaddr_un sock_wifi_mnt_addr;
static int  dc_wifi_state = DC_WIFI_IDLE;
static int  DebugPeer = 0;

#define WIFI_UDP_LEN    4096
static char wifisockbuf[WIFI_UDP_LEN];


#define UPDATE_PEER_PERIOD_S    30
#define UPDATE_LOC_PERIOD_S     10

typedef uint32_t fixed32;
typedef uint16_t fixed16;

#define FIXED_BITVALUE(fbitlen) (1.0 / (double)(1<<(fbitlen)))


/// converting fixed with Q of qsize total bits and fsize bits of fraction part
double fixed_to_double(UINT64 val, int qsize, int fsize) {
    UINT64  ival, fval;

    FLPD("val:%lx q:%d f:%d", val, qsize, fsize);

    if(val >> (qsize-1)) {    //check if negative value
        ival = val>>fsize;
        fval =  val & ( (1<<fsize)-1);

        //FLPD("ival:%X%X", (UINT32)ival>>32,(UINT32)ival&0xffffffff);
        //FLPD("fval:%X%X", (UINT32)fval>>32,(UINT32)fval&0xffffffff);
        //FLPD("val:%X%X", (UINT32)val>>32,(UINT32)val&0xffffffff);

        ival = ~ival;
        ival+=1;
        ival = ival << (64-fsize);  //L-shift redundant part.
        ival = ival >> (64-qsize);  //R-shift to it's normal position
        val = ival+fval;
        //FLPD("ival:%X%X", (UINT32)ival>>32,(UINT32)ival&0xffffffff);
        //FLPD("fval:%X%X", (UINT32)fval>>32,(UINT32)fval&0xffffffff);
        //FLPD("val:%X%X", (UINT32)val>>32,(UINT32)val&0xffffffff);
        return -FIXED_BITVALUE(fsize) * val ;
    } else {
        return FIXED_BITVALUE(fsize) * val ;
    }
}

#if 0
double fixed32_to_double( fixed32 val, int fbitlen )
{
    if (val & 0x80000000){
        fixed32 ival = val>>fbitlen;
        fixed32 fval =  val<<(32-fbitlen);
        fval = fval >> (32-fbitlen);

        FLPD("ival:%16x fval:%016X val:%016X", ival, fval, val);

        ival = ~ival;
        ival+=1;
        ival = ival<<fbitlen;

        FLPD("ival:%08x fval:%08X val:%08X", ival, fval, val);

        val = ival+fval;

        FLPD("ival:%08x fval:%08X val:%08X", ival, fval, val);


        return -FIXED_BITVALUE(fbitlen) * val ;
    }
    else
        return FIXED_BITVALUE(fbitlen) * val ;
}

double fixed16_to_double( fixed16 val, int fbitlen )
{
    if ((~val) < val){
        fixed16 ival = val>>fbitlen;
        fixed16 fval =  val<<(16-fbitlen);
        fval = fval >> (16-fbitlen);

        FLPD("ival:%04x fval%04X val:%04X", ival, fval, val);

        ival = ~ival;
        ival+=1;
        ival = ival<<fbitlen;

        FLPD("ival:%04x fval%04X  val:%04X", ival, fval, val);

        val = ival+fval;

        FLPD("ival:%04x fval%04X  val:%04X", ival, fval, val);

        return -FIXED_BITVALUE(fbitlen) * val ;
    }
    else
        return FIXED_BITVALUE(fbitlen) * val ;
}
#endif

#define WIFI_INTERFACE_CMD  "INTERFACES"
#define WIFI_ATTACH_CMD     "ATTACH"
#define WIFI_DETACH_CMD     "DETACH"


static int mtk_flp_dc_wifi_connect() {
    char cmd[16], rsp[1024];
    int rsplen=0;
    socklen_t  addrlen;

    ///// Connect to control FD ///////////////
    if((flp_wifi_socket_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1) {
        FLPD("wifi open udp sock failed\r\n");
        return -1;
    }
    memset(&sock_wifi_cli_addr, 0, sizeof(sock_wifi_cli_addr));
    sock_wifi_cli_addr.sun_family = AF_LOCAL;
    strcpy(sock_wifi_cli_addr.sun_path, DC_WIFI_UDP_CLI_PATH);

    unlink(DC_WIFI_UDP_CLI_PATH);
    if(bind(flp_wifi_socket_fd, (const struct sockaddr *) &sock_wifi_cli_addr, sizeof(sock_wifi_cli_addr)) < 0) {
        close(flp_wifi_socket_fd);
        FLPD("server: bind");
        return -1;
    }
    int res = chmod(DC_WIFI_UDP_CLI_PATH, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP| S_IROTH |S_IWOTH |S_IXOTH);
    if(res < 0) {
        FLPD("chmod error:%d", res);
        return -1;
    }

    memset(&sock_wifi_svr_addr, 0, sizeof(sock_wifi_svr_addr));
    sock_wifi_svr_addr.sun_family = AF_LOCAL;
    strcpy(sock_wifi_svr_addr.sun_path, DC_WIFI_UDP_SVR_PATH);
    if(connect(flp_wifi_socket_fd, (struct sockaddr*)&sock_wifi_svr_addr, sizeof(sock_wifi_svr_addr)) < 0) {
        FLPD("Connect error to wifi supplicant server");
        return -1;
    }
    FLPD("Control connection ok");


    /////  Connect to monitor FD /////////////
    if((flp_wifi_monitor_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1) {
        FLPD("wifi open udp sock failed\r\n");
        return -1;
    }
    memset(&sock_wifi_mnt_addr, 0, sizeof(sock_wifi_mnt_addr));
    sock_wifi_mnt_addr.sun_family = AF_LOCAL;
    strcpy(sock_wifi_mnt_addr.sun_path, DC_WIFI_UDP_MNT_PATH);

    unlink(DC_WIFI_UDP_MNT_PATH);
    if(bind(flp_wifi_monitor_fd, (const struct sockaddr *) &sock_wifi_mnt_addr, sizeof(sock_wifi_mnt_addr)) < 0) {
        close(flp_wifi_monitor_fd);
        FLPD("server: bind");
        return -1;
    }
    res = chmod(DC_WIFI_UDP_MNT_PATH, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP| S_IROTH |S_IWOTH |S_IXOTH);
    if(res < 0) {
        FLPD("chmod error:%d", res);
        return -1;
    }

    memset(&sock_wifi_svr_addr, 0, sizeof(sock_wifi_svr_addr));
    sock_wifi_svr_addr.sun_family = AF_LOCAL;
    strcpy(sock_wifi_svr_addr.sun_path, DC_WIFI_UDP_SVR_PATH);

    if(connect(flp_wifi_monitor_fd, (struct sockaddr*)&sock_wifi_svr_addr, sizeof(sock_wifi_svr_addr)) < 0) {
        FLPD("Connect error to wifi supplicant server");
        return -1;
    }

    ///TODO: need to handle detach later.
    strcpy( cmd, WIFI_ATTACH_CMD);///Send attach command
    if(send(flp_wifi_monitor_fd, cmd, strlen(cmd), 0) < 0) {
        FLPD("Send error");
    }
    rsplen = recv(flp_wifi_monitor_fd, rsp, 1024, 0);
    if(rsplen > 0) {
        FLPD("Attach Recv:%s", rsp);
    } else {
        FLPD("Attach Recv err:%s", strerror(errno) );
    }
    FLPD("Monitor connection OK");
    return 0;
}

static int mtk_flp_dc_wifi_recv_msg(char **buf, int *len) {
    int recvlen = 0;
    socklen_t addrlen = sizeof(sock_wifi_svr_addr);
    fd_set readfds;
    int max_sd = 0, ret=0;


    max_sd = (flp_wifi_socket_fd>flp_wifi_monitor_fd?flp_wifi_socket_fd:flp_wifi_monitor_fd);

    FD_ZERO(&readfds);
    FD_SET(flp_wifi_socket_fd, &readfds);
    FD_SET(flp_wifi_monitor_fd, &readfds);

    ret = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

    if(ret<0) {
        FLPD("wifi select err: %s", strerror(errno) );
    }
    *buf = NULL;
    if(FD_ISSET(flp_wifi_socket_fd, &readfds)) {
        recvlen = recv(flp_wifi_socket_fd, wifisockbuf, WIFI_UDP_LEN, 0);
        if(recvlen < 0) {
            FLPD("wifi select err: %s", strerror(errno) );
            return -1;
        }
        FD_CLR(flp_wifi_socket_fd, &readfds);
    } else if(FD_ISSET(flp_wifi_monitor_fd, &readfds)) {
        recvlen = recv(flp_wifi_monitor_fd, wifisockbuf, WIFI_UDP_LEN, 0);
        if(recvlen < 0) {
            FLPD("wifi select err: %s", strerror(errno) );
            return -1;
        }
        FD_CLR(flp_wifi_monitor_fd, &readfds);
    }
    wifisockbuf[recvlen] = '\0';        //force null-terminate
    *buf = wifisockbuf;
    *len = recvlen;
    return recvlen;
}


unsigned int toInt(char c) {
    if (c >= '0' && c <= '9') return      c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return -1;
}

static void strtohex(char *buf, int len, UINT64* val) {
    int i;

    *val = 0;
    for(i = 0; i < len; i++) {
        *val = ( (*val)<<4) + toInt(buf[i]);
    }
}

static int mtk_flp_dc_wifi_send_msg(char *buf, int len) {
    if(flp_wifi_socket_fd <=0) {
        FLPD("WIFI Send: UDP socket not created");
        return -1;
    }

    if(send(flp_wifi_socket_fd, buf, len, 0) < 0) {
        FLPD("WIFI send command to DC fail:%s\r\n", strerror(errno));
    }
    return 0;
}

static int mtk_flp_dc_wifi_parse_location(char *buf, int len) {
    char lng[10], lat[10], alt[10],acc[10];
    MTK_FLP_MSG_T       *flp_msg;
    FlpLocation  *loc;
    double              Lng,Lat,Alt,Acc;
    UINT64              binVal=0;
    char    localbuf[2048];

    UNUSED(len);
    sscanf(buf, "longitude=0x%8s\tlatitude=0x%8s\taltitude=0x%8s\taccuracy=0x%4s", lng, lat, alt, acc);

    FLPD("lng:%s lat:%s alt:%s acc:%s", lng, lat,alt,acc);

    if(strlen(lng)!=8 || strlen(lat)!=8 || strlen(alt)!=8  || strlen(acc)!=4) {
        FLPD("Error decoding location string:%s %s %s %s", lng, lat,alt,acc);
        return MTK_FLP_ERROR;
    }

    strtohex(lng, 8, &binVal);
    Lng = fixed_to_double(binVal, 32, 23);

    strtohex(lat, 8, &binVal);
    Lat = fixed_to_double(binVal, 32, 23);

    strtohex(alt, 8, &binVal);
    Alt = fixed_to_double(binVal, 30, 22);

    strtohex(acc, 4, &binVal);
    Acc = fixed_to_double(binVal, 16, 8);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) + sizeof(MTK_FLP_LOCATION_T));
    FLPD("[%s:%d]Malloc msg:%x Location struct:%x",__FILE__,__LINE__,flp_msg , (UINT8*)flp_msg+sizeof(MTK_FLP_MSG_T));
    flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
    flp_msg->length = sizeof(MTK_FLP_LOCATION_T);

    loc= (FlpLocation *)((UINT8*)flp_msg+sizeof(MTK_FLP_MSG_T));
    loc->altitude = Alt;
    loc->longitude = Lng;
    loc->latitude = Lat;
    loc->accuracy = Acc;
    loc->bearing = 0;
    loc->flags = FLP_LOCATION_HAS_LAT_LONG|FLP_LOCATION_HAS_ALTITUDE|FLP_LOCATION_HAS_SPEED|FLP_LOCATION_HAS_BEARING|FLP_LOCATION_HAS_ACCURACY;
    loc->size = sizeof(MTK_FLP_LOCATION_T);
    loc->sources_used = FLP_TECH_MASK_WIFI;
    loc->speed = 0;
    loc->timestamp = mtk_flp_get_timestamp(NULL);
    mtk_flp_sys_dbg_dump_loc(loc);
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);


    //send Location Diag notification
    sprintf(localbuf, "RTT_NTF=LNG::%f LAT::%f ALT::%f\n", loc->longitude, loc->latitude, loc->altitude);
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(localbuf);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf) );
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
    return MTK_FLP_SUCCESS;
}

static MTK_FLP_WIFI_PEER_T  gPeerArray[32];

static int mtk_flp_dc_wifi_parse_peer(char *buf, MTK_FLP_WIFI_PEER_T *peer) {
    char localbuf[1024];
    char *ptr;
    char lng[10], lat[10], alt[100];
    char rssi;
    MTK_FLP_MSG_T       *flp_msg;
    MTK_FLP_LOCATION_T  *loc;
    double              Lng,Lat,Alt,Acc;
    UINT64              binVal=0;

    memcpy(localbuf, buf, strlen(buf));

    FLPD("PEER2:%s", localbuf);

    sscanf(localbuf, "ssid=%s\tmacaddr=%hhu:%hhu:%hhu:%hhu:%hhu:%hhu\trssi=%hhu\tlongitude=0x%8s\tlatitude=0x%8s\taltitude=0x%8s",
                    peer->ssid, &(peer->mac[0]), &(peer->mac[1]), &(peer->mac[2]), &(peer->mac[3]), &(peer->mac[4]), &(peer->mac[5]),
                    &rssi, lng, lat, alt);

    //FLPD("Peer: ssid:%s mac:%x-%x-%x-%x-%x-%x lng:%s lat:%s alt:%s rssi:%x",peer->ssid, (peer->mac[0]), (peer->mac[1]), (peer->mac[2]), (peer->mac[3]), (peer->mac[4]), (peer->mac[5]),    lng, lat,alt,rssi);

    if(strlen(lng)!=8 || strlen(lat)!=8 || strlen(alt)!=8) {
        FLPD("Error decoding location string:%s %s %s", lng, lat,alt);
        return MTK_FLP_ERROR;
    }

    strtohex(lng, 8, &binVal);
    Lng = fixed_to_double(binVal, 32, 23);

    strtohex(lat, 8, &binVal);
    Lat = fixed_to_double(binVal, 32, 23);

    strtohex(lng, 8, &binVal);
    Alt = fixed_to_double(binVal, 30, 22);

    peer->longtitude = Lng;
    peer->latitude = Lat;
    peer->altitude = Alt;
    FLPD("SSID:%s RSSI%x MAC:%X-%X-%X-%X-%X-%X LNG:%f LAT:%f ALT:%f",
            peer->ssid, peer->rssi,
            peer->mac[0], (peer->mac[1]), (peer->mac[2]), (peer->mac[3]), (peer->mac[4]), (peer->mac[5]),
            Lng, Lat, Alt);

#if 0   //need to process all AP info together before sending out
    memset(localbuf, 0, sizeof(localbuf));
    ptr = localbuf;
    sprintf(ptr, "WIFI_AP_NTF=");
    ptr += strlen(ptr);

    sprintf(ptr, "SSID::%s\tMAC:%X-%X-%X-%X-%X-%X\tLNG::%f\tLAT::%f\tALT::%f\tDIST:%d\tRSSI:%d\n"
                ,btinfo->db[i].BeaAddr[0], btinfo->db[i].BeaAddr[1], btinfo->db[i].BeaAddr[2]
                ,btinfo->db[i].BeaAddr[3], btinfo->db[i].BeaAddr[4], btinfo->db[i].BeaAddr[5]
                ,btinfo->db[i].BeaLong, btinfo->db[i].BeaLat, btinfo->db[i].BeaHeight, btinfo->db[i].BeaDist, btinfo->db[i].BeaRssi);
        ptr += strlen(ptr);

    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
    flp_msg->type = MTK_FLP_MSG_DC_DIAG_REPORT_DATA_NTF;
    flp_msg->length = strlen(localbuf);
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf) );
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
#endif
    return MTK_FLP_SUCCESS;
}


static int mtk_flp_dc_wifi_parse_peers(char *buf, int len) {
    char    *ptr,*trm;
    int     idx=0;
    UNUSED(len);
    ptr = strtok(buf, "\n");
    while(ptr != NULL) {
        mtk_flp_dc_wifi_parse_peer(ptr, &gPeerArray[idx]);
        ptr = strtok(NULL, "\n");
        idx++;
    }
    return MTK_FLP_SUCCESS;
}

static void mtk_flp_dc_wifi_process_diag(MTK_FLP_MSG_T *prmsg) {
    char *ptr, *optr;
    MTK_FLP_MSG_T*  flp_msg;

    if(prmsg == NULL)
        return;

    ptr= (char*)prmsg+sizeof(MTK_FLP_MSG_T);

    if(strncmp(ptr, "ENA_CMD", 7) == 0) {
        if( ptr[8] == '1') {
            FLPD("Start WIFI Peer Debug!!!");
            DebugPeer = 1;
        } else {
            FLPD("Stop WIFI Peer Debug!!!");
            DebugPeer = 0;
        }
    }
}

/************************************************************************/
//  Process msgs from DC and send to GNSS HAL
/************************************************************************/

/*
enable                -->    DRIVER rtt_enable
                         <--    RTT_EVENT_ENABLE_DONE

Discover peer       -->    DRIVER rtt_discover_peer -pPeroid -rReport
                         <--    RTT_EVENT_PEER_INFO
                        -->    DRIVER rtt_get_peer_info
                        <--     ssid=AP1 macaddr=00:01:01:01:01:01  longitude= 0x4dcc1fc8 latitude= 0x4dcc1fc8 altitude=0x00000000

Update Loc          -->     DRIVER rtt_update_location <-pPERIOD>
                        <--     RTT_EVENT_LOCATION_INFO
                        -->     DRIVER rtt_get_location_info
                        <--     longitude= 0x04dcc1fc8 latitude= 0x04dcc1fc8 altitude=0x00000000 accuracy=0x0000

disbable:             -->    DRIVER rtt_disable
                        <--     RTT_EVENT_DISABLE_DONE

*/
static int mtk_flp_dc_wifi_proc_msg(char *buf, int len) {
    char cmd[128];
    int cmdlen = 0;

    FLPD("Recv:%s", buf);

    if(strncmp(buf, "RTT_EVENT_ENABLE_DONE", strlen("RTT_EVENT_ENABLE_DONE")) == 0) {
        //nothing to be done for now.
        dc_wifi_state = DC_WIFI_RTT_ENABLED;
        sprintf(cmd , "DRIVER rtt_discover_peer -p%d -r%d", UPDATE_PEER_PERIOD_S, 0);
        cmdlen = strlen(cmd);
        mtk_flp_dc_wifi_send_msg(cmd, cmdlen);
        sprintf(cmd , "DRIVER rtt_update_location -p%d", UPDATE_LOC_PERIOD_S);
        cmdlen = strlen(cmd);
        mtk_flp_dc_wifi_send_msg(cmd, cmdlen);
    } else if(strncmp(buf, "RTT_EVENT_PEER_INFO", strlen("RTT_EVENT_PEER_INFO")) == 0) {
        sprintf(cmd , "DRIVER rtt_get_peer_info");
        cmdlen = strlen(cmd);
        mtk_flp_dc_wifi_send_msg(cmd, cmdlen);
    } else if(strncmp(buf, "RTT_EVENT_LOCATION_INFO", strlen("RTT_EVENT_LOCATION_INFO")) == 0) {
        sprintf(cmd , "DRIVER rtt_get_location_info");
        cmdlen = strlen(cmd);
        mtk_flp_dc_wifi_send_msg(cmd, cmdlen);
    } else if(strncmp(buf, "longitude", strlen("longitude")) == 0) {
        mtk_flp_dc_wifi_parse_location(buf, len);
    } else if(strncmp(buf, "ssid", strlen("ssid")) == 0) {
        mtk_flp_dc_wifi_parse_peers(buf, len);
    } else if(strncmp(buf, "RTT_EVENT_DISABLE_DONE", strlen("RTT_EVENT_DISABLE_DONE")) == 0) {
        dc_wifi_state = DC_WIFI_IDLE;
    }
    return 0;
}

/************************************************************************/
//  Receiver location/meassurment from GNSS HAL and report back to DC
/************************************************************************/
void mtk_flp_dc_wifi_main(void) {
    int i;
    int recvlen=0;
    char *recvbuf=NULL;
    double d = 0;


    FLPD("DC WIFI Adaption main begins...");
    d = fixed_to_double( (UINT64)0x04dcc1fc8, (int)34, (int)25);
    FLPD("test32 1  +38.89868 :%f",   d);
    d = fixed_to_double( (UINT64)0xf65ecf031, (int)34, (int)25);
    FLPD("test32 2  -77.03723 :%f",   d);
    d = fixed_to_double( (UINT64)0xfd80, (int)16, (int)8);
    FLPD("test16 3  -3.5     :%f",   d);
    d = fixed_to_double( (UINT64)0x7f000000, (int)31, (int)25);
    FLPD("test32 4  -1.5 :%f",   d);

    mtk_flp_dc_wifi_connect();

    while(1) {
        if( mtk_flp_dc_wifi_recv_msg(&recvbuf, &recvlen) > 0) {
            mtk_flp_dc_wifi_proc_msg(recvbuf, recvlen);
        } else {
            FLPD("Error receiving WIFI message, break out");
            break;
        }
    }
}

int mtk_flp_dc_wifi_process(MTK_FLP_MSG_T *prmsg) {
    char sendbuf[1024];
    MTK_FLP_MSG_T   *flp_msg;
    int i;
    int recvlen=0;
    char *recvbuf=NULL;
    MTK_FLP_BATCH_OPTION_T *option;

    option = (MTK_FLP_BATCH_OPTION_T *)((UINT8*)prmsg+sizeof(MTK_FLP_MSG_T));

    switch(prmsg->type) {
    case MTK_FLP_MSG_DC_START_CMD:
        if(option->sources_to_use & FLP_TECH_MASK_WIFI) {
            FLPD("START WIFI Batching");
            sprintf(sendbuf, "DRIVER rtt_enable");
            mtk_flp_dc_wifi_send_msg(sendbuf, strlen(sendbuf));
        }
        break;
    case MTK_FLP_MSG_DC_STOP_CMD:
        //if( option->sources_to_use & FLP_TECH_MASK_WIFI)
        {
            FLPD("STOP WIFI Batching");
            sprintf(sendbuf, "DRIVER rtt_disable");
            mtk_flp_dc_wifi_send_msg(sendbuf, strlen(sendbuf));
        }
        break;
    case MTK_FLP_MSG_DC_WIFI_MANUAL_GET_LOC_NTF:    //support polling for location info.
        FLPD("POLLING rtt_get_location_info");
        sprintf(sendbuf , "DRIVER rtt_get_location_info");
        mtk_flp_dc_wifi_send_msg(sendbuf, strlen(sendbuf));
        break;
    default:
        FLPD("Unknown WIFI message type:%x", prmsg->type);
        break;
    }
#if 0
    flp_msg = mtk_flp_sys_msg_alloc( sizeof(MTK_FLP_MSG_T) +prmsg->length);
    flp_msg->type = MSG_DC2KER(prmsg->type);
    flp_msg->length = prmsg->length;
    memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), ((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T), prmsg->length);
    mtk_flp_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);
#endif

    return 0;

}

#else //offload
void mtk_flp_dc_wifi_main(MTK_FLP_MSG_T *prmsg);
int mtk_flp_dc_wifi_process(MTK_FLP_MSG_T *prmsg);
extern INT32 mtk_flp_lowpower_query_mode ();

static int dc_wifi_on = 0;
static int peer_rec_cnt = 0;
static int peer_rewrite_cnt = 0;
static int peer_rewrite_enable = 0;
MTK_FLP_WIFI_PEER_RECORD_T peer_rec[WIFI_PEER_RECORD]= {{0}};

//#define wifi_loopback

int mtk_flp_wifiloc2flploc(MTK_FLP_LOCATION_T *flploc, MTK_FLP_LOCATION_T *wifiloc) {
    int ret = -1;
    if((wifiloc != NULL)) {
        flploc->size = sizeof(MTK_FLP_LOCATION_T);
        flploc->sources_used = (UINT32)FLP_TECH_MASK_WIFI;
        flploc->flags = (UINT16)(FLP_LOCATION_HAS_LAT_LONG | FLP_LOCATION_HAS_ACCURACY);

        #ifdef wifi_loopback //revert inverted laltlon from wifi
        flploc->latitude = wifiloc->longitude;
        flploc->longitude = wifiloc->latitude;
        flploc->accuracy = 100.0;
        #else
        flploc->latitude = wifiloc->latitude;
        flploc->longitude = wifiloc->longitude;
        flploc->accuracy = wifiloc->accuracy;
        #endif

        flploc->altitude = wifiloc->altitude;
        flploc->speed = 0;
        flploc->bearing = 0;
        flploc->timestamp = wifiloc->timestamp;
        ret = 0;

        FLP_TRC("wifi Loc:LNG:%f LAT:%f ALT:%f ACC:%f SPD:%f BEARING:%f, FLAGS:%04X SOURCES:%08X Timestamp:%lld",
        flploc->longitude, flploc->latitude, flploc->altitude, flploc->accuracy,
        flploc->speed, flploc->bearing, flploc->flags, flploc->sources_used, flploc->timestamp);
    }
    return ret;
}

void mtk_flp_wifi_peer_record (MTK_FLP_WIFI_PEER_T *peer_loc_in) {
    int i;
    int repeat_peer = 0;

    //record peer loc in global
    if(peer_loc_in != NULL) {
        if((peer_loc_in->longtitude>180) || (peer_loc_in->longtitude < -180)
            || (peer_loc_in->latitude >90) || (peer_loc_in->latitude<-90)
            || ((peer_loc_in->latitude < 0.01) && (peer_loc_in->latitude < 0.01) && (peer_loc_in->altitude< 0.01))) {
            FLP_TRC("invalid peer loc");
            return;
        }
        if(peer_rec_cnt >0) {
            if(peer_rec_cnt >= WIFI_PEER_RECORD) {
                peer_rewrite_enable = 1;
                peer_rec_cnt = WIFI_PEER_RECORD;
            }

            for(i = 0; i < peer_rec_cnt; i++) {
                //compare mac, replace old mac
                //FLP_TRC("peer_cmp:%s, %s",peer_rec[i].mac,peer_loc_in->mac );
                if((peer_rec[i].mac[0] == peer_loc_in->mac[0]) && (peer_rec[i].mac[1] == peer_loc_in->mac[1])
                    && (peer_rec[i].mac[2] == peer_loc_in->mac[2]) && (peer_rec[i].mac[3] == peer_loc_in->mac[3])
                    && (peer_rec[i].mac[4] == peer_loc_in->mac[4]) && (peer_rec[i].mac[5] == peer_loc_in->mac[5])) {
                    memcpy( &peer_rec[i].mac, peer_loc_in->mac, 6*sizeof(UINT8) );
                    memcpy( &peer_rec[i].ssid, peer_loc_in->ssid, 32*sizeof(INT8));
                    peer_rec[i].latitude = peer_loc_in->latitude;
                    peer_rec[i].longtitude= peer_loc_in->longtitude;
                    repeat_peer = 1;
                    FLP_TRC("repeat_peer: %s",peer_rec[i].ssid);
                    break;
                }
            }
            if(!repeat_peer) {
                if(peer_rewrite_enable) {
                    if(peer_rewrite_cnt >= WIFI_PEER_RECORD) {
                        peer_rewrite_cnt = 0;
                    }
                    memcpy( &peer_rec[peer_rewrite_cnt].mac, peer_loc_in->mac, 6*sizeof(UINT8) );
                    memcpy( &peer_rec[peer_rewrite_cnt].ssid, peer_loc_in->ssid, 32*sizeof(INT8));
                    peer_rec[peer_rewrite_cnt].latitude = peer_loc_in->latitude;
                    peer_rec[peer_rewrite_cnt].longtitude= peer_loc_in->longtitude;

                    peer_rewrite_cnt++;
                    FLP_TRC("peer_rewrite_cnt %d",peer_rewrite_cnt);
                } else {
                    if(peer_rec_cnt < WIFI_PEER_RECORD) {
                        memcpy( &peer_rec[peer_rec_cnt].mac, peer_loc_in->mac, 6*sizeof(UINT8) );
                        memcpy( &peer_rec[peer_rec_cnt].ssid, peer_loc_in->ssid, 32*sizeof(INT8));
                        peer_rec[peer_rec_cnt].latitude = peer_loc_in->latitude;
                        peer_rec[peer_rec_cnt].longtitude= peer_loc_in->longtitude;
                        FLP_TRC("normal peer update: %s",peer_rec[peer_rec_cnt].ssid);
                    }
                }
                peer_rec_cnt++;
            }
        } else {
            if(peer_rec_cnt < WIFI_PEER_RECORD) {
                memcpy( &peer_rec[peer_rec_cnt].mac, peer_loc_in->mac, 6*sizeof(UINT8) );
                memcpy( &peer_rec[peer_rec_cnt].ssid, peer_loc_in->ssid, 32*sizeof(INT8));
                peer_rec[peer_rec_cnt].latitude = peer_loc_in->latitude;
                peer_rec[peer_rec_cnt].longtitude= peer_loc_in->longtitude;
                FLP_TRC("normal peer update 0: %s",peer_rec[peer_rec_cnt].ssid);
            }
            peer_rec_cnt++;
        }
    }
}

void mtk_flp_wifi_print_peer() {
    char localbuf[512] = {0};
    char *ptr = NULL;
    MTK_FLP_MSG_T *flp_msg = NULL;
    int i;

    memset(localbuf, 0, sizeof(localbuf));
    ptr = localbuf;
    sprintf(ptr, "WIFI_AP_NTF=");
    ptr += strlen(ptr);

    for(i = 0; i < peer_rec_cnt; i++) {
        sprintf(ptr, "SSID::%s\tLAT::%f\tLNG::%f\n", peer_rec[i].ssid,peer_rec[i].latitude, peer_rec[i].longtitude );
        FLP_TRC("SSID::%s\tLAT::%f\tLNG::%f\n", peer_rec[i].ssid,peer_rec[i].latitude, peer_rec[i].longtitude );
        ptr += strlen(ptr);
    }
    FLP_TRC("wifi peer strlen = %d",strlen(localbuf));

    if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
        flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
        flp_msg->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
        flp_msg->length = strlen(localbuf);
        memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf) );
        mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg);
    }
}

void mtk_flp_dc_wifi_main(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_MSG_T *flp_msg = NULL, *flp_msg1 = NULL;
    MTK_FLP_LOCATION_T  *loc_in = NULL;
    MTK_FLP_LOCATION_T loc_out = {0};
    MTK_FLP_LOCATION_T loc_dbg = {0};
    MTK_FLP_WIFI_PEER_T peer_loc;
    UINT32 peer_read_len = 0, temp_read_len = 0;
    INT16 wifi_mode = -1;
    int ret = -1;
    char localbuf[512] = {0};
    #ifdef wifi_loopback
    double tmp_loc = 0;
    #endif

    if((prmsg == NULL) || (prmsg->length < 0)) {
        FLP_TRC("mtk_flp_dc_wifi_main, recv prmsg is null pointer\r\n");
        return;
    }

    FLP_TRC("wifi main msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);

    switch(prmsg->type) {
        case CMD_FLP_START_WIFI_RES:
            FLP_TRC("wifi start ok");
            break;
        case CMD_FLP_STOP_WIFI_RES:
            FLP_TRC("wifi stop ok");
            break;
        case CMD_FLP_WIFI_SET_MODE_RES:
            memcpy( &wifi_mode, ((INT8*)prmsg)+ sizeof(MTK_FLP_MSG_T), sizeof(INT16));
            FLP_TRC("flp req wifi update mode %d", wifi_mode);
            break;
        case CMD_WIFI_FLP_CHANGE_MODE_NTF:
            memcpy( &wifi_mode, ((INT8*)prmsg)+ sizeof(MTK_FLP_MSG_T), sizeof(INT16));
            FLP_TRC("wifi change mode to %d", wifi_mode);
            break;
        case CMD_WIFI_ON_NTF_TO_FLP:
            if(dc_wifi_on) {
                flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
                flp_msg->type = CMD_FLP_START_WIFI_REQ;
                flp_msg->length = 0;
                mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_WIFI,flp_msg);
                FLP_TRC("Start Wifi Batching, flp on first");
            }
            break;
        case CMD_WIFI_REPORT_LOC:
            loc_in = (MTK_FLP_LOCATION_T *)(((INT8*)prmsg)+sizeof(MTK_FLP_MSG_T));
            ret = mtk_flp_wifiloc2flploc(&loc_out,loc_in);
            FLP_TRC("CHECK wifi LOC SIZE = %d \n", sizeof(MTK_FLP_LOCATION_T));

            FLP_TRC("RTT Location:LNG:%f LAT:%f ALT:%f ACC:%f SPD:%f BEARING:%f, FLAGS:%04X SOURCES:%08X Timestamp:%lld",
            loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.accuracy,
            loc_out.speed, loc_out.bearing, loc_out.flags, loc_out.sources_used, loc_out.timestamp);

            flp_msg = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +sizeof(MTK_FLP_LOCATION_T) );
            flp_msg->type = MTK_FLP_MSG_DC_REPORT_LOC_NTF;
            flp_msg->length = sizeof(MTK_FLP_LOCATION_T);
            memcpy( ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), &loc_out, sizeof(MTK_FLP_LOCATION_T));
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_DC, flp_msg);

            //debug print
            memcpy( &loc_dbg, ((INT8*)flp_msg)+sizeof(MTK_FLP_MSG_T), sizeof(MTK_FLP_LOCATION_T));
            FLP_TRC("RTT Location DBG:LNG:%f LAT:%f ALT:%f ACC:%f SPD:%f BEARING:%f, FLAGS:%04X SOURCES:%08X Timestamp:%lld",
            loc_dbg.longitude, loc_dbg.latitude, loc_dbg.altitude, loc_dbg.accuracy,
            loc_dbg.speed, loc_dbg.bearing, loc_dbg.flags, loc_dbg.sources_used, loc_dbg.timestamp);

            //send DIAG location
            if((mtk_flp_lowpower_query_mode() != FLP_BATCH_GEOFENCE_ONLY) && (mtk_flp_lowpower_query_mode() != FLP_BATCH_NONE)) {
                sprintf(localbuf,"RTT_NTF=LNG:%f LAT:%f ALT:%f", loc_out.longitude, loc_out.latitude, loc_out.altitude);
                FLP_TRC("RTT_NTF=LNG::%f LAT::%f ALT::%f SIZE::%d SRC::%d\n",loc_out.longitude, loc_out.latitude, loc_out.altitude, loc_out.size,loc_out.sources_used);
                flp_msg1 = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T) +strlen(localbuf));
                flp_msg1->type = MTK_FLP_MSG_HAL_DIAG_REPORT_DATA_NTF;
                flp_msg1->length = strlen(localbuf);
                memcpy(((INT8*)flp_msg1)+sizeof(MTK_FLP_MSG_T), localbuf, strlen(localbuf));
                mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_HAL, flp_msg1);
            }
            break;
        case CMD_WIFI_REPORT_PEER_INFO:
            if(prmsg->length >= sizeof(MTK_FLP_WIFI_PEER_T)) {
                peer_read_len = (prmsg->length)/sizeof(MTK_FLP_WIFI_PEER_T);
                FLP_TRC("peer_read_len = %d", peer_read_len);

                if(peer_read_len*sizeof(MTK_FLP_WIFI_PEER_T) != prmsg->length) {
                    FLP_TRC(" wrong size of MTK_FLP_WIFI_PEER_T, input: %d, output: %d", prmsg->length, peer_read_len*sizeof(MTK_FLP_WIFI_PEER_T));
                } else {
                    while(temp_read_len < peer_read_len) {
                        memset(&peer_loc, 0, sizeof(MTK_FLP_WIFI_PEER_T));
                        memcpy(&peer_loc, ((INT8*)prmsg)+ sizeof(MTK_FLP_MSG_T)+temp_read_len*sizeof(MTK_FLP_WIFI_PEER_T), sizeof(MTK_FLP_WIFI_PEER_T));
#ifdef wifi_loopback //wifi input latlon was incorrect,need revert
                        tmp_loc = peer_loc.longtitude;
                        peer_loc.longtitude = peer_loc.latitude;
                        peer_loc.latitude = tmp_loc;
                        FLP_TRC("wifi peer revert lla: lat %f, lon %f", peer_loc.latitude , peer_loc.longtitude);
#endif
                        peer_loc.range = peer_loc.range /100;
                        FLP_TRC("SSID:%s RSSI%x MAC:%X-%X-%X-%X-%X-%X LNG:%f LAT:%f ALT:%f RANGE:%d",
                        peer_loc.ssid, peer_loc.rssi,
                        peer_loc.mac[0], (peer_loc.mac[1]), (peer_loc.mac[2]), (peer_loc.mac[3]), (peer_loc.mac[4]), (peer_loc.mac[5]),
                        peer_loc.longtitude, peer_loc.latitude, peer_loc.altitude, peer_loc.range);
                        mtk_flp_wifi_peer_record(&peer_loc);
                        temp_read_len++;
                    }
                    //Diag msg to AP
                    mtk_flp_wifi_print_peer();
                }
            } else {
                FLP_TRC("size peer loc incorrect : %d", prmsg->length);
            }
            break;
        default:
            FLP_TRC("Unknown wifi message type: %x", prmsg->type);
            break;

    }
}

int mtk_flp_dc_wifi_process(MTK_FLP_MSG_T *prmsg) {
    MTK_FLP_MSG_T *ptr;
    MTK_FLP_BATCH_OPTION_T *option;

    option = (MTK_FLP_BATCH_OPTION_T *)( (UINT8*)prmsg+sizeof(MTK_FLP_MSG_T));

    FLP_TRC("wifi Proc msg(%x) type:0x%02x len:%d", (unsigned int)prmsg, prmsg->type,prmsg->length);
    /*
    switch(prmsg->type) {
    case MTK_FLP_MSG_DC_START_CMD:
        if(option->sources_to_use & FLP_TECH_MASK_WIFI) {
            FLP_TRC("Wifi Batching Enabled");
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T));
            ptr->type = CMD_FLP_START_WIFI_REQ;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_WIFI,ptr);
            FLP_TRC("Start Wifi Batching, wifi on first");
            dc_wifi_on = 1;
        }
        break;
    case MTK_FLP_MSG_DC_STOP_CMD:
        {
            FLP_TRC("Stop wifi Batching");
            ptr = mtk_flp_mcu_mem_alloc( sizeof(MTK_FLP_MSG_T)+prmsg->length);
            ptr->type = CMD_FLP_STOP_WIFI_REQ;
            ptr->length = 0;
            mtk_flp_mcu_sys_msg_send(MTK_FLP_TASKID_WIFI, ptr);
        }
        break;
    default:
        FLP_TRC("Unknown GNSS message to send type:0x%02x", prmsg->type);
        break;
    }
    */
    return 0;
}

#endif
