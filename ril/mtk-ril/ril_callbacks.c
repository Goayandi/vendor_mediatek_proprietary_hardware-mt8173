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

#include <dirent.h>
#include <stdlib.h>
#include <telephony/mtk_ril.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include "atchannels.h"
#include "at_tok.h"
#include "misc.h"
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <termios.h>

#include <ril_callbacks.h>

#ifdef MTK_RIL_MD1
#define LOG_TAG "RIL"
#else
#define LOG_TAG "RILMD2"
#endif

#include <utils/Log.h>

#include <cutils/properties.h>

//#include <hardware/ril/librilutils/proto/sap-api.pb.h>

#define PROPERTY_RIL_SIM_READY              "ril.ready.sim"

//[New R8 modem FD]
#define PROPERTY_FD_SCREEN_ON_TIMER     "persist.radio.fd.counter"
#define PROPERTY_FD_SCREEN_ON_R8_TIMER  "persist.radio.fd.r8.counter"
#define PROPERTY_FD_SCREEN_OFF_TIMER    "persist.radio.fd.off.counter"
#define PROPERTY_FD_SCREEN_OFF_R8_TIMER    "persist.radio.fd.off.r8.counter"
#define PROPERTY_RIL_FD_MODE    "ril.fd.mode"
/* FD related timer: units: 1 sec */
#define DEFAULT_FD_SCREEN_ON_TIMER "15"
#define DEFAULT_FD_SCREEN_ON_R8_TIMER "15"
#define DEFAULT_FD_SCREEN_OFF_TIMER "5"
#define DEFAULT_FD_SCREEN_OFF_R8_TIMER "5"

static void onRequest(int request, void *data, size_t datalen, RIL_Token t, RIL_SOCKET_ID socket_id);

#if defined(ANDROID_MULTI_SIM)
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t,
        RIL_SOCKET_ID socket_id);
#else
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t);
#endif

static RIL_RadioState currentState(RIL_SOCKET_ID rid);
static int onSupports(int requestCode);
static void onCancel(RIL_Token t);
static const char *getVersion();

//void sendBtSapResponseComplete(RIL_Token t, RIL_Errno ret, MsgId msgId, void *data);

extern const char *requestToString(int request);
extern void initRILChannels(void);
extern void initialCidTable();
extern void getLogicModemId();

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

#ifdef  RIL_SHLIB
const struct RIL_Env *s_rilenv;
#endif  /* RIL_SHLIB */

const struct RIL_Env *s_rilsapenv;
static const RIL_RadioFunctions s_sapcallbacks = {
    RIL_VERSION,
    onSapRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

static RIL_RadioState sState = RADIO_STATE_UNAVAILABLE;

#ifdef  MTK_GEMINI
static RIL_RadioState sState2 = RADIO_STATE_UNAVAILABLE;
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
static RIL_RadioState sState3 = RADIO_STATE_UNAVAILABLE;
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
static RIL_RadioState sState4 = RADIO_STATE_UNAVAILABLE;
#endif
#endif  /* MTK_GEMINI */

static pthread_mutex_t s_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_state_cond = PTHREAD_COND_INITIALIZER;

static int s_port = -1;
static const char *s_device_path = NULL;
static int s_device_socket = 0;

static int s_device_range_begin = -1;
static int s_device_range_end = -1;

/* trigger change to this with s_state_cond */
static int s_closed = 0;

#ifdef  MTK_RIL
static const RIL_SOCKET_ID s_pollSimId = RIL_SOCKET_1;
#ifdef  MTK_GEMINI
static const RIL_SOCKET_ID s_pollSimId2 = RIL_SOCKET_2;
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
static const RIL_SOCKET_ID s_pollSimId3 = RIL_SOCKET_3;
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
static const RIL_SOCKET_ID s_pollSimId4 = RIL_SOCKET_4;
#endif

#endif  /* MTK_GEMINI */
#endif  /* MTK_RIL */

//BEGIN mtk03923 [ALPS00061979 - MS Lost network after sending many DTMF]
extern int inCallNumber;
//END   mtk03923 [ALPS00061979 - MS Lost network after sending many DTMF]
extern int setPlmnListFormat(RIL_SOCKET_ID rid, int format);

static const struct timeval TIMEVAL_0 = { 0, 0 };

/* Commands pending in AT channel */
static pthread_mutex_t s_pendinglist_mutex = PTHREAD_MUTEX_INITIALIZER;
static int pendinglist[RIL_SUPPORT_CHANNELS] = { 0 };

static int s_isSimSwitched = 0;
static int s_telephonyMode = -1;
extern int bPSBEARERSupport;

extern int s_md_off;
extern int s_main_loop;

extern int bCREGType3Support;
char s_logicalModemId[SIM_COUNT][MAX_UUID_LENGTH];

int s_isUserLoad = 0;

/*
 * If found the request in pendinglist, return list index else retrun RIL_SUPPORT_CHANNELS
 */
static int findPendingRequest(int request)
{
    int i;
    for (i = 0; i < RIL_SUPPORT_CHANNELS; i++) {
        if (*(pendinglist + i) == request)
            return i;
    }

    return i;
}

static void setRequest(int request)
{
    pthread_mutex_lock(&s_pendinglist_mutex);
    /* find an empty slot */
    pendinglist[findPendingRequest(0)] = request;
    assert(i < RIL_SUPPORT_CHANNELS);
    pthread_mutex_unlock(&s_pendinglist_mutex);
}

static void resetRequest(int request)
{
    pthread_mutex_lock(&s_pendinglist_mutex);
    pendinglist[findPendingRequest(request)] = 0;
    assert(i < RIL_SUPPORT_CHANNELS);
    pthread_mutex_unlock(&s_pendinglist_mutex);
}

extern int RILcheckPendingRequest(int request)
{
    return (RIL_SUPPORT_CHANNELS == findPendingRequest(request)) ? 0 : 1;
}

/** do post-AT+CFUN=1 initialization */
static void onRadioPowerOn(RIL_SOCKET_ID rid)
{
    const RIL_SOCKET_ID *p_rilId = &s_pollSimId;

#ifdef MTK_GEMINI
    if (RIL_SOCKET_2 == rid)
        p_rilId = &s_pollSimId2;
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
    if (RIL_SOCKET_3 == rid)
        p_rilId = &s_pollSimId3;
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
    if (RIL_SOCKET_4 == rid)
        p_rilId = &s_pollSimId4;
#endif
#endif

    pollSIMState((void *)p_rilId);
}

/** do post- SIM ready initialization */
static void onSIMReady(RIL_SOCKET_ID rid)
{
}

/*** Callback methods from the RIL library to us ***/

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void onRequest(int request, void *data, size_t datalen, RIL_Token t, RIL_SOCKET_ID socket_id)
{
    RIL_RadioState radioState = sState;

    RLOGD("onRequest: %s, datalen = %d", requestToString(request), datalen);

#ifdef MTK_GEMINI
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
    if (RIL_CHANNEL_SET4_OFFSET <= RIL_queryMyChannelId(t))
        radioState = sState4;
    else
#endif
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
        if (RIL_CHANNEL_SET3_OFFSET <= RIL_queryMyChannelId(t))
            radioState = sState3;
        else
#endif
            if (RIL_CHANNEL_OFFSET <= RIL_queryMyChannelId(t))
                radioState = sState2;
#endif
    RLOGD("radioState:%d", radioState);

    if (s_md_off &&
        request != RIL_REQUEST_RADIO_POWER &&
        request != RIL_REQUEST_GET_RADIO_CAPABILITY &&
        request != RIL_REQUEST_GET_IMEI &&
        request != RIL_REQUEST_GET_IMEISV &&
        request != RIL_REQUEST_BASEBAND_VERSION &&
        request != RIL_REQUEST_GET_SIM_STATUS &&
        request != RIL_REQUEST_RESET_RADIO &&
        request != RIL_REQUEST_SET_RADIO_CAPABILITY
       ) {
        RLOGD("MD off and ignore %s", requestToString(request));
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    /* Ignore all requests except RIL_REQUEST_GET_SIM_STATUS,RIL_REQUEST_DETECT_SIM_MISSING when RADIO_STATE_UNAVAILABLE. */
    // [ALPS00958313] Pass RADIO_POWERON when radio not available that is caused by muxd shutdown device before ccci ioctl.
    if (radioState == RADIO_STATE_UNAVAILABLE &&
        request != RIL_REQUEST_GET_SIM_STATUS &&
        request != RIL_REQUEST_OEM_HOOK_RAW && //This is for ATCI
        request != RIL_REQUEST_OEM_HOOK_STRINGS &&
        request != RIL_REQUEST_GET_RADIO_CAPABILITY &&
        request != RIL_REQUEST_SET_RADIO_CAPABILITY &&
        request != RIL_REQUEST_RESET_RADIO
       ) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    /* Ignore all non-power requests when RADIO_STATE_OFF
     * (except RIL_REQUEST_GET_SIM_STATUS, RIL_REQUEST_DETECT_SIM_MISSING)
     */
    if (radioState == RADIO_STATE_OFF &&
        !(request == RIL_REQUEST_RADIO_POWER ||
          request == RIL_REQUEST_GET_SIM_STATUS ||
          request == RIL_REQUEST_GET_IMEI ||
          request == RIL_REQUEST_GET_IMEISV ||
          request == RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE ||
          request == RIL_REQUEST_BASEBAND_VERSION ||
          request == RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE ||
          request == RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE ||
          request == RIL_REQUEST_SET_LOCATION_UPDATES ||
          request == RIL_REQUEST_WRITE_SMS_TO_SIM ||
          request == RIL_REQUEST_DELETE_SMS_ON_SIM ||
          request == RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG ||
          request == RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG ||
          request == RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION ||
          request == RIL_REQUEST_GET_SMSC_ADDRESS ||
          request == RIL_REQUEST_SET_SMSC_ADDRESS ||
          request == RIL_REQUEST_REPORT_SMS_MEMORY_STATUS ||
          request == RIL_REQUEST_SCREEN_STATE ||
          request == RIL_REQUEST_RESET_RADIO ||
          request == RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE ||
          request == RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND ||
          request == RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM ||
          request == RIL_REQUEST_SET_TTY_MODE ||
          request == RIL_REQUEST_OEM_HOOK_RAW ||
          request == RIL_REQUEST_OEM_HOOK_STRINGS ||
          request == RIL_REQUEST_SIM_IO ||
          request == RIL_REQUEST_GET_RADIO_CAPABILITY ||
          request == RIL_REQUEST_SET_RADIO_CAPABILITY ||
          request == RIL_REQUEST_ENTER_SIM_PIN ||
          request == RIL_REQUEST_ENTER_SIM_PUK ||
          request == RIL_REQUEST_ENTER_SIM_PIN2 ||
          request == RIL_REQUEST_ENTER_SIM_PUK2 ||
          request == RIL_REQUEST_CHANGE_SIM_PIN ||
          request == RIL_REQUEST_CHANGE_SIM_PIN2 ||
          request == RIL_REQUEST_QUERY_FACILITY_LOCK ||
          request == RIL_REQUEST_SET_FACILITY_LOCK ||
          request == RIL_REQUEST_ALLOW_DATA ||
          request == RIL_REQUEST_GET_IMSI ||
          request == RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE
         )) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    //BEGIN mtk03923 [ALPS00061979 - MS Lost network after sending many DTMF]
    if (inCallNumber == 0 &&
        (request == RIL_REQUEST_DTMF ||
         request == RIL_REQUEST_DTMF_START ||
         request == RIL_REQUEST_DTMF_STOP)) {
        RIL_onRequestComplete(t, RIL_E_CANCELLED, NULL, 0); // RIL_E_GENERIC_FAILURE
        return;
    }
    //END   mtk03923 [ALPS00061979 - MS Lost network after sending many DTMF]

    /* set pending RIL request */
    setRequest(request);

    if (!(rilSimMain(request, data, datalen, t) ||
          rilNwMain(request, data, datalen, t) ||
          rilCcMain(request, data, datalen, t) ||
          rilSsMain(request, data, datalen, t) ||
          rilSmsMain(request, data, datalen, t) ||
          rilStkMain(request, data, datalen, t) ||
          rilOemMain(request, data, datalen, t) ||
          rilDataMain(request, data, datalen, t)))
        RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);

    /* Reset pending RIL request */
    resetRequest(request);
}

#if defined(ANDROID_MULTI_SIM)
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t,
        RIL_SOCKET_ID socket_id) {
#else
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t) {
#endif
    RLOGD("onSapRequest: %d", request);
#if 0
    RIL_SOCKET_ID socketId = RIL_SOCKET_1;
    #if defined(ANDROID_MULTI_SIM)
    socketId = socket_id;
    #endif

    if (request < MsgId_RIL_SIM_SAP_CONNECT /* MsgId_UNKNOWN_REQ */ ||
            request > MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL) {
        RLOGD("invalid request");
        RIL_SIM_SAP_ERROR_RSP rsp;
        rsp.dummy_field = 1;
        sendBtSapResponseComplete(t, Error_RIL_E_REQUEST_NOT_SUPPORTED, MsgId_RIL_SIM_SAP_ERROR_RESP
                , &rsp);
        return;
    }

    if (s_md_off) {
        RLOGD("MD off and reply failure to Sap message");
        RIL_SIM_SAP_CONNECT_RSP rsp;
        rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_CONNECT_FAILURE;
        rsp.has_max_message_size = false;
        rsp.max_message_size = 0;
        sendBtSapResponseComplete(t, Error_RIL_E_RADIO_NOT_AVAILABLE, MsgId_RIL_SIM_SAP_CONNECT,
                &rsp);
        return;
    }

    /* set pending RIL request */
    setRequest(request);

    if (!(rilBtSapMain(request, data, datalen, t, socketId) ||
            rilStkBtSapMain(request, data, datalen, t, socketId))) {
        RIL_SIM_SAP_ERROR_RSP rsp;
        rsp.dummy_field = 1;
        sendBtSapResponseComplete(t, Error_RIL_E_REQUEST_NOT_SUPPORTED, MsgId_RIL_SIM_SAP_ERROR_RESP
                , &rsp);
    }

    /* Reset pending RIL request */
    resetRequest(request);
#endif
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
static RIL_RadioState currentState(RIL_SOCKET_ID rid)
{
    return getRadioState(rid);
}

/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */
static int onSupports(int requestCode)
{
    //@@@ todo
    return 1;
}

static void onCancel(RIL_Token t)
{
    //@@@todo
}

static const char *getVersion(void)
{
#ifdef MTK_GEMINI
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
    return "mtk gemini+ 4 SIM ril 1.0";
#elif (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
    return "mtk gemini+ 3 SIM ril 1.0";
#else
    return "mtk gemini ril 1.0";
#endif
#else
    return "mtk ril w10.20";
#endif  /* MTK_GEMINI */
}


static int isReadMccMncForBootAnimation()
{
    char prop[PROPERTY_VALUE_MAX] = {0};
    property_get("ril.read.imsi", prop, "");
    return (!strcmp(prop, "1")) ? 1 : 0;
}

static void resetSystemProperties(RIL_SOCKET_ID rid)
{
    RLOGI("[RIL_CALLBACK] resetSystemProperties");
    resetSIMProperties(rid);
}

/**
 * Initialize everything that can be configured while we're still in
 * AT+CFUN=0
 */
#ifdef  MTK_RIL
static void initializeCallback(void *param)
{
    ATResponse *p_response = NULL;
    int err;
    RIL_SOCKET_ID rid = *((RIL_SOCKET_ID *)param);
    int nRadioState = -1;
    //[New R8 modem FD] Enlarge array size of property_value
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    int current_share_modem = 0;

    setRadioState(RADIO_STATE_OFF, rid);

    err = at_handshake(getDefaultChannelCtx(rid));

    RLOGI("AT handshake: %d", err);

    resetSystemProperties(rid);

    /* note: we don't check errors here. Everything important will
     * be handled in onATTimeout and onATReaderClosed */

    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    at_send_command("ATE0Q0V1", NULL, getDefaultChannelCtx(rid));

    /*  No auto-answer */
    at_send_command("ATS0=0", NULL, getDefaultChannelCtx(rid));

    /*  Extended errors */
    at_send_command("AT+CMEE=1", NULL, getDefaultChannelCtx(rid));

    /*  Disable CFU query */
    err = at_send_command("AT+ESSP=1", &p_response, getDefaultChannelCtx(rid));

    /*  Notify project type , 1 : Basic , 2 : BSP , 3 : Turnkey */
    at_send_command("AT+EPROJECT=1", NULL, getDefaultChannelCtx(rid));

    /* check if modem support +CREG=3 */
    err = at_send_command("AT+CREG=3", &p_response, getDefaultChannelCtx(rid));
    if (err < 0 || p_response->success == 0) {
        bCREGType3Support = 0;

        /*  check if modem support +CREG=2 */
        err = at_send_command("AT+CREG=2", &p_response, getDefaultChannelCtx(rid));

        /* some handsets -- in tethered mode -- don't support CREG=2 */
        if (err < 0 || p_response->success == 0)
            at_send_command("AT+CREG=1", NULL, getDefaultChannelCtx(rid));
    }
    at_response_free(p_response);

#ifdef MTK_FEMTO_CELL_SUPPORT
    err = at_send_command("AT+ECSG=4,1", NULL, getDefaultChannelCtx(rid));
#endif

    /*  GPRS registration events */
    at_send_command("AT+CGREG=1", NULL, getDefaultChannelCtx(rid));
    at_send_command("AT+CEREG=1", NULL, getDefaultChannelCtx(rid));
    err = at_send_command("AT+PSBEARER=1", NULL, getDefaultChannelCtx(rid));
    if (err < 0)
        bPSBEARERSupport = 0;

    /*  Call Waiting notifications */
    at_send_command("AT+CCWA=1", NULL, getDefaultChannelCtx(rid));

    /*  mtk00924: enable Call Progress notifications */
    at_send_command("AT+ECPI=4294967295", NULL, getDefaultChannelCtx(rid));

    /*  Alternating voice/data off */
    /*
     * at_send_command("AT+CMOD=0", NULL, getDefaultChannelCtx(rid));
     */

    /*  Not muted */
    /*
     * at_send_command("AT+CMUT=0", NULL, getDefaultChannelCtx(rid));
     */

    /*  +CSSU unsolicited supp service notifications */
    at_send_command("AT+CSSN=1,1", NULL, getDefaultChannelCtx(rid));

    /*  connected line identification on */
    at_send_command("AT+COLP=1", NULL, getDefaultChannelCtx(rid));

    /*  HEX character set */
    at_send_command("AT+CSCS=\"UCS2\"", NULL, getDefaultChannelCtx(rid));

    /*  USSD unsolicited */
    at_send_command("AT+CUSD=1", NULL, getDefaultChannelCtx(rid));

    /*  Enable +CGEV GPRS event notifications, but don't buffer */
    at_send_command("AT+CGEREP=1,0", NULL, getDefaultChannelCtx(rid));

    /*  SMS PDU mode */
    at_send_command("AT+CMGF=0", NULL, getDefaultChannelCtx(rid));

    /* Enable getting NITZ, include TZ and Operator Name*/
    /* To Receive +CIEV: 9 and +CIEV: 10*/
    at_send_command("AT+CTZR=1", NULL, getDefaultChannelCtx(rid));

    /*  Enable getting CFU info +ECFU and speech info +ESPEECH*/
    int einfo_value;
#ifdef MTK_UMTS_TDD128_MODE //Add for TDD rb release[bit 9]
    einfo_value = 306;
#else
    einfo_value = 50;
#endif
    /*  Enable getting CFU info +ECFU and speech info +ESPEECH and modem warning +EWARNING(0x100) */
    char modemWarningProperty[PROPERTY_VALUE_MAX];
    char einfoStr[32];
    property_get("persist.radio.modem.warning", modemWarningProperty, 0);
    if (strcmp(modemWarningProperty, "1") == 0) {
        /* Enable "+EWARNING" */
        einfo_value |= 256;
    }
    sprintf(einfoStr, "AT+EINFO=%d", einfo_value);
    at_send_command(einfoStr, NULL, getDefaultChannelCtx(rid));


    /*  Enable get ECSQ URC */
    /* ALPS00465815, fix power consumption issue, START */
    if (isDualTalkMode() && getExternalModemSlot() == rid) {
        at_send_command("AT+ECSQ=0", NULL, getDefaultChannelCtx(rid));
    } else {
        at_send_command("AT+ECSQ=1", NULL, getDefaultChannelCtx(rid));
    }
    /* ALPS00465815, fix power consumption issue, END */

    /*  Enable get +CIEV:7 URC to receive SMS SIM Storage Status*/
    at_send_command("AT+CMER=1,0,0,2,0", NULL, getDefaultChannelCtx(rid));

    // Disable 3G VT by default
    at_send_command("AT+ECCP=1", NULL, getDefaultChannelCtx(rid));

#ifndef MTK_FD_SUPPORT
    RLOGD("Not Support Fast Dormancy");
#else
    RLOGD("Check RIL FD Mode on rid=%d", rid);
#endif

#ifdef MTK_WORLD_PHONE
    at_send_command("AT+ECOPS=1", NULL, getDefaultChannelCtx(rid));
    RLOGD("AT+ECOPS=1 sent");
    at_send_command("AT+EMSR=0,1", NULL, getDefaultChannelCtx(rid));
    RLOGD("AT+EMSR=0,1 sent");
#endif

    if (isInternationalRoamingEnabled()) {
        /* ALPS00501602, fix power issue by disable ECOPS URC START */
        if (!isDualTalkMode() && rid == RIL_SOCKET_1) {
            at_send_command("AT+ECOPS=1", NULL, getDefaultChannelCtx(rid));
            at_send_command("AT+EMSR=0,1", NULL, getDefaultChannelCtx(rid));
        }
        /* ALPS00501602, fix power issue by disable ECOPS URC END */

        /* ALPS00523054 START */
        if (isDualTalkMode() && getExternalModemSlot() == rid) {
            RLOGI("Turn off URC of the unused modem SIM protocol, rid=%d", rid);
            at_send_command("AT+ECSQ=0", NULL, getDefaultChannelCtx(rid));
            at_send_command("AT+CREG=1", NULL, getDefaultChannelCtx(rid));
            at_send_command("AT+CGREG=1", NULL, getDefaultChannelCtx(rid));
            at_send_command("AT+PSBEARER=0", NULL, getDefaultChannelCtx(rid));
        }
        /* ALPS00523054 END */
    }

    //[New R8 modem FD]
#ifdef MTK_FD_SUPPORT
    /* MTK_FD_SUPPORT is turned on single sim 3G Project, WG/G Gemini Project => For EVDO_DT_SUPPORT: Turn off MTK_FD_DORMANCY */
    if (rid == RIL_SOCKET_1) {
        RLOGD("Support Fast Dormancy Feature");
        /* Fast Dormancy is only available on 3G Protocol Set */
        /* [Step#01] Query if the new FD mechanism is supported by modem or not */
        err = at_send_command_singleline("AT+EFD=?", "+EFD:", &p_response, getDefaultChannelCtx(rid));
        RLOGD("Result of AT+EFD => %s", p_response->finalResponse);

        memset(property_value, 0, sizeof(property_value));
        property_get(PROPERTY_FD_SCREEN_OFF_R8_TIMER, property_value, DEFAULT_FD_SCREEN_OFF_R8_TIMER);
        RLOGD("Test FD value=%s,%d", property_value, (int)(atof(property_value) * 10));

        //[New R8 modem FD for test purpose]
        if ((err == 0 && p_response->success == 1) && (strncmp(p_response->finalResponse, "OK", 2) == 0)) {
            int errcode = -1;
            /* TEL FW can query this variable to know if AP side is necessary to execute FD or not */
            errcode = property_set(PROPERTY_RIL_FD_MODE, "1");
            memset(property_value, 0, sizeof(property_value));
            property_get(PROPERTY_RIL_FD_MODE, property_value, "0");
            RLOGD("ril.fd.mode=%s, errcode=%d", property_value, errcode);

            RLOGD("Try to get FD related timers from system");
#if 1
            /* [Step#02] Set default FD related timers for mode: format => AT+EFD=2, timer_id, timer_value (unit:0.1sec)  */
            char *timer_value;
            memset(property_value, 0, sizeof(property_value));
            property_get(PROPERTY_FD_SCREEN_OFF_TIMER, property_value, DEFAULT_FD_SCREEN_OFF_TIMER);
            RLOGD("Screen Off FD Timer=%s", property_value);
            /* timerId=0: Screen Off + Legancy FD */
            asprintf(&timer_value, "AT+EFD=2,0,%d", (int)(atof(property_value) * 10));
            at_send_command(timer_value, NULL, getDefaultChannelCtx(rid));
            free(timer_value);

            /* timerId=2: Screen Off + R8 FD */
            memset(property_value, 0, sizeof(property_value));
            property_get(PROPERTY_FD_SCREEN_OFF_R8_TIMER, property_value, DEFAULT_FD_SCREEN_OFF_R8_TIMER);
            asprintf(&timer_value, "AT+EFD=2,2,%d", (int)(atof(property_value) * 10));
            at_send_command(timer_value, NULL, getDefaultChannelCtx(rid));
            free(timer_value);
            memset(property_value, 0, sizeof(property_value));
            property_get(PROPERTY_FD_SCREEN_ON_TIMER, property_value, DEFAULT_FD_SCREEN_ON_TIMER);
            RLOGD("Screen On FD Timer=%s", property_value);
            /* timerId=1: Screen On + Legancy FD */
            asprintf(&timer_value, "AT+EFD=2,1,%d", (int)(atof(property_value) * 10));
            at_send_command(timer_value, NULL, getDefaultChannelCtx(rid));
            free(timer_value);

            /* timerId=3: Screen On + R8 FD */
            memset(property_value, 0, sizeof(property_value));
            property_get(PROPERTY_FD_SCREEN_ON_R8_TIMER, property_value, DEFAULT_FD_SCREEN_ON_R8_TIMER);
            asprintf(&timer_value, "AT+EFD=2,3,%d", (int)(atof(property_value) * 10));
            at_send_command(timer_value, NULL, getDefaultChannelCtx(rid));
            free(timer_value);
            memset(property_value, 0, sizeof(property_value));

            /* [Step#03] Enable FD Mechanism MD: after finishing to set FD related default timer to modem */
            at_send_command("AT+EFD=1", NULL, getDefaultChannelCtx(rid));
#endif
        }
    }
#endif

    at_send_command("AT+EAIC=2", NULL, getDefaultChannelCtx(rid));

    at_send_command("AT+CLIP=1", NULL, getDefaultChannelCtx(rid));

    at_send_command("AT+CNAP=1", NULL, getDefaultChannelCtx(rid));

    // set data/call prefer
    // 0 : call prefer
    // 1 : data prefer
    char gprsPrefer[PROPERTY_VALUE_MAX] = { 0 };
    property_get("persist.radio.gprs.prefer", gprsPrefer, "0");
    if ((atoi(gprsPrefer) == 0)) {
        // call prefer
        at_send_command("AT+EGTP=1", NULL, getDefaultChannelCtx(rid));
        at_send_command("AT+EMPPCH=1", NULL, getDefaultChannelCtx(rid));
    } else {
        // data prefer
        at_send_command("AT+EGTP=0", NULL, getDefaultChannelCtx(rid));
        at_send_command("AT+EMPPCH=0", NULL, getDefaultChannelCtx(rid));
    }

    /* ALPS00574862 Remove redundant +COPS=3,2;+COPS? multiple cmd in REQUEST_OPERATOR */
    at_send_command("AT+COPS=3,2", NULL, getDefaultChannelCtx(rid));

    // ALPS00353868 START
    err = at_send_command("AT+COPS=3,3",  &p_response, getDefaultChannelCtx(rid));
    RLOGI("AT+COPS=3,3 got err= %d,success=%d", err, p_response->success);

    if (err >= 0 && p_response->success != 0) {
        setPlmnListFormat(rid, 1);
    }
    at_response_free(p_response);
    // ALPS00353868 END

    //BEGIN mtk03923 [20110713][Enable +ESIMS URC]
    at_send_command("AT+ESIMS=1", NULL, getDefaultChannelCtx(rid));
    //END   mtk03923 [20110713][Enable +ESIMS URC]

    //ALPS01228632
#ifdef MTK_SIM_RECOVERY
    at_send_command("AT+ESIMREC=1", NULL, getDefaultChannelCtx(rid));
#else
    at_send_command("AT+ESIMREC=0", NULL, getDefaultChannelCtx(rid));
#endif

    mtk_initializeCallback(param);

    if (isDualTalkMode()) {
        current_share_modem = 1;
    } else {
        #if (SIM_COUNT >= 2)
        current_share_modem = 2;
        #else
        current_share_modem = 1;
        #endif
    }

    nRadioState = queryRadioState(rid);
#if 1
#ifdef  MTK_GEMINI
    if (0 == nRadioState)
        requestSimReset(rid);
    else    /* MTK_GEMINI */
        requestSimInsertStatus(rid);

    RLOGD("start rild bootup flow [%d, %d, %d, %d]", isDualTalkMode(), rid, RIL_is3GSwitched(), current_share_modem);
    if (isInternationalRoamingEnabled()
        || getTelephonyMode() == 100
        || getTelephonyMode() == 101) {
        if (isDualTalkMode()) {
            /* 104 and 105 should be add handler here */
            if (rid == RIL_SOCKET_2) {
                flightModeBoot();

                // Regional Phone: boot animation START
                RLOGD("Regional Phone: boot animation START (DT) [%d,%d,%d]", current_share_modem, rid, isReadMccMncForBootAnimation());
                if (isReadMccMncForBootAnimation()) {
#ifdef MTK_RIL_MD2
                    requestMccMncForBootAnimation(RIL_SOCKET_2);
#else
                    requestMccMncForBootAnimation(RIL_SOCKET_1);
#endif
                }
                // Regional Phone: boot animation END

                bootupGetIccid(rid);
                bootupGetImei(rid);
                bootupGetImeisv(rid);
                bootupGetBasebandVersion(rid);
                bootupGetCalData(rid);
                RLOGD("case 1 get SIM inserted status [%d]", sim_inserted_status);
            }
        } else {
            if (rid == RIL_SOCKET_2) {
                flightModeBoot();

                // Regional Phone: boot animation START
                RLOGD("Regional Phone: boot animation START (Single) [%d,%d,%d]", current_share_modem, rid, isReadMccMncForBootAnimation());
                if (isReadMccMncForBootAnimation()) {
                    requestMccMncForBootAnimation(RIL_SOCKET_1);
                    requestMccMncForBootAnimation(RIL_SOCKET_2);
                }
                // Regional Phone: boot animation END

                bootupGetIccid(RIL_SOCKET_1);
                bootupGetIccid(RIL_SOCKET_2);
                bootupGetImei(RIL_SOCKET_1);
                bootupGetImei(RIL_SOCKET_2);
                bootupGetImeisv(RIL_SOCKET_1);
                bootupGetImeisv(RIL_SOCKET_2);
                bootupGetBasebandVersion(RIL_SOCKET_1);
                bootupGetBasebandVersion(RIL_SOCKET_2);
                bootupGetCalData(RIL_SOCKET_1);
                RLOGD("case 2 get SIM inserted status [%d]", sim_inserted_status);
            }
        }
    } else {
        if ((!isDualTalkMode() && rid == RIL_SOCKET_2) ||
            (isDualTalkMode() && ((rid == RIL_SOCKET_2 && RIL_is3GSwitched()) || (rid == RIL_SOCKET_1 && !RIL_is3GSwitched())))
           ) {
            flightModeBoot();
            if (isDualTalkMode()) {

                // Regional Phone: boot animation START
                RLOGD("Regional Phone: boot animation START (DT) [%d,%d,%d]", current_share_modem, rid, isReadMccMncForBootAnimation());
                if (isReadMccMncForBootAnimation()) {
#ifdef MTK_RIL_MD2
                    requestMccMncForBootAnimation(RIL_SOCKET_2);
#else
                    requestMccMncForBootAnimation(RIL_SOCKET_1);
#endif
                }
                // Regional Phone: boot animation END

                bootupGetIccid(rid); //query ICCID after AT+ESIMS
                bootupGetImei(rid);
                bootupGetImeisv(rid);
                bootupGetBasebandVersion(rid);
                if ((rid == RIL_SOCKET_2 && RIL_is3GSwitched()) || (rid == RIL_SOCKET_1 && !RIL_is3GSwitched())) {
                    bootupGetCalData(rid);
                    RLOGD("case 3 bootupGetCalData");
                }
            } else {
                // Regional Phone: boot animation START
                RLOGD("Regional Phone: boot animation START (Single) [%d,%d,%d]", current_share_modem, rid, isReadMccMncForBootAnimation());
                if (isReadMccMncForBootAnimation()) {
                    requestMccMncForBootAnimation(RIL_SOCKET_1);
                    requestMccMncForBootAnimation(RIL_SOCKET_2);
                }
                // Regional Phone: boot animation END

                bootupGetIccid(RIL_SOCKET_1);
                bootupGetIccid(RIL_SOCKET_2);
                bootupGetImei(RIL_SOCKET_1);
                bootupGetImei(RIL_SOCKET_2);
                bootupGetImeisv(RIL_SOCKET_1);
                bootupGetImeisv(RIL_SOCKET_2);
                bootupGetBasebandVersion(RIL_SOCKET_1);
                bootupGetBasebandVersion(RIL_SOCKET_2);
                bootupGetCalData(RIL_SOCKET_2);
            }
            RLOGD("case 3 get SIM inserted status [%d]", sim_inserted_status);
        } else if (current_share_modem == 1 && rid == RIL_SOCKET_1) {
            flightModeBoot();

            // Regional Phone: boot animation START
            RLOGD("Regional Phone: boot animation START (Single) [%d,%d,%d]", current_share_modem, rid, isReadMccMncForBootAnimation());
            if (isReadMccMncForBootAnimation()) {
                requestMccMncForBootAnimation(rid);
            }
            // Regional Phone: boot animation END

            bootupGetIccid(rid);
            bootupGetImei(rid);
            bootupGetImeisv(rid);
            bootupGetBasebandVersion(rid);
            bootupGetCalData(rid);
            RLOGD("get SIM inserted status (Single) [%d]", sim_inserted_status);
        }  else if (rid == RIL_SOCKET_2 + (SIM_COUNT - 2)) {
            int i;
            flightModeBoot();
            for(i = 0; i < SIM_COUNT; i++) {
                // Regional Phone: boot animation START
                RLOGD("Regional Phone: boot animation START [%d,%d,%d]", current_share_modem, rid, isReadMccMncForBootAnimation());
                if (isReadMccMncForBootAnimation()) {
                    // assume: current_share_modem == 2
                    requestMccMncForBootAnimation(RIL_SOCKET_1 + i);
                }
                // Regional Phone: boot animation END

                bootupGetIccid(RIL_SOCKET_1 + i);
                bootupGetImei(RIL_SOCKET_1 + i);
                bootupGetImeisv(RIL_SOCKET_1 + i);
                bootupGetBasebandVersion(RIL_SOCKET_1 + i);
            }
            bootupGetCalData(RIL_SOCKET_1);

            RLOGD("rid=%d,report sim_inserted_status [%d]", rid, sim_inserted_status);
        }
    }
#else
    if (0 == nRadioState)
        requestSimReset(rid);
    else    /* MTK_GEMINI */
        requestSimInsertStatus(rid);

    flightModeBoot();

    if (isReadMccMncForBootAnimation()) {
        requestMccMncForBootAnimation(RIL_SOCKET_1);
    }

    bootupGetIccid(rid);
    bootupGetImei(rid);
    bootupGetImeisv(rid);
    bootupGetBasebandVersion(rid);
    bootupGetCalData(rid);
#endif  /* MTK_GEMINI */
#else
    if (0 == nRadioState)
        requestSimReset(rid);
    else    /* MTK_GEMINI */
        requestSimInsertStatus(rid);

#endif
    requestGetPacketSwitchBearer(rid);

    updateNitzOperInfo(rid);

    // GCG switcher feature
    requestGetGcfMode(rid);
    // GCG switcher feature

    // This is used for wifi-onlg version load
    // Since RIL is not connected to RILD in wifi-only version
    // we query it and stored into a system property
    // note: since this patch has no impact to nomal load, do this in normal initial procedure
    requestSN(rid);

    initCFUQueryType();

    /* assume radio is off on error */
    if (1 == nRadioState)
        setRadioState(RADIO_STATE_SIM_NOT_READY, rid);
}
#else   /* MTK_RIL */
static void initializeCallback(void *param)
{
    ATResponse *p_response = NULL;
    int err;

    setRadioState(RADIO_STATE_OFF);

    at_handshake();

    /* note: we don't check errors here. Everything important will
     * be handled in onATTimeout and onATReaderClosed */

    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    at_send_command("ATE0Q0V1", NULL);

    /*  No auto-answer */
    at_send_command("ATS0=0", NULL);

    /*  Extended errors */
    at_send_command("AT+CMEE=1", NULL);

    /*  Network registration events */
    err = at_send_command("AT+CREG=2", &p_response);

    /* some handsets -- in tethered mode -- don't support CREG=2 */
    if (err < 0 || p_response->success == 0)
        at_send_command("AT+CREG=1", NULL);

    at_response_free(p_response);

    /*  GPRS registration events */
    at_send_command("AT+CGREG=1", NULL);

    /*  Call Waiting notifications */
    at_send_command("AT+CCWA=1", NULL);

    /*  Alternating voice/data off */
    at_send_command("AT+CMOD=0", NULL);

    /*  Not muted */
    at_send_command("AT+CMUT=0", NULL);

    /*  +CSSU unsolicited supp service notifications */
    at_send_command("AT+CSSN=0,1", NULL);

    /*  no connected line identification */
    at_send_command("AT+COLP=0", NULL);

    /*  HEX character set */
    at_send_command("AT+CSCS=\"HEX\"", NULL);

    /*  USSD unsolicited */
    at_send_command("AT+CUSD=1", NULL);

    /*  Enable +CGEV GPRS event notifications, but don't buffer */
    at_send_command("AT+CGEREP=1,0", NULL);

    /*  SMS PDU mode */
    at_send_command("AT+CMGF=0", NULL);

#ifdef  USE_TI_COMMANDS
    at_send_command("AT%CPI=3", NULL);

    /*  TI specific -- notifications when SMS is ready (currently ignored) */
    at_send_command("AT%CSTAT=1", NULL);
#endif  /* USE_TI_COMMANDS */

    /* assume radio is off on error */
    if (isRadioOn() > 0)
        setRadioState(RADIO_STATE_SIM_NOT_READY);
}
#endif  /* MTK_RIL */

static void waitForClose()
{
    pthread_mutex_lock(&s_state_mutex);

    while (s_closed == 0)
        pthread_cond_wait(&s_state_cond, &s_state_mutex);

    pthread_mutex_unlock(&s_state_mutex);
}


/**
 * Called by atchannel when an unsolicited line appears
 * This is called on atchannel's reader thread. AT commands may
 * not be issued here
 */
static void onUnsolicited(const char *s, const char *sms_pdu, void *pChannel)
{
    char *line = NULL;
    int err;
    RIL_RadioState radioState = sState;
    RILChannelCtx *p_channel = (RILChannelCtx *)pChannel;

#ifdef  MTK_GEMINI
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
    if (RIL_SOCKET_4 == getRILIdByChannelCtx(p_channel))
        radioState = sState4;
    else
#endif
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
        if (RIL_SOCKET_3 == getRILIdByChannelCtx(p_channel))
            radioState = sState3;
        else
#endif
            if (RIL_SOCKET_2 == getRILIdByChannelCtx(p_channel))
                radioState = sState2;
#endif

    /* Ignore unsolicited responses until we're initialized.
     * This is OK because the RIL library will poll for initial state
     */
    if (radioState == RADIO_STATE_UNAVAILABLE)
        return;

    if (!(rilNwUnsolicited(s, sms_pdu, p_channel) ||
          rilCcUnsolicited(s, sms_pdu, p_channel) ||
          rilSsUnsolicited(s, sms_pdu, p_channel) ||
          rilSmsUnsolicited(s, sms_pdu, p_channel) ||
          rilStkUnsolicited(s, sms_pdu, p_channel) ||
          rilOemUnsolicited(s, sms_pdu, p_channel) ||
          rilDataUnsolicited(s, sms_pdu, p_channel) ||
          rilSimUnsolicited(s, sms_pdu, p_channel)))
        RLOGE("Unhandled unsolicited result code: %s\n", s);
}

#ifdef  MTK_RIL
/* Called on command or reader thread */
static void onATReaderClosed(RILChannelCtx *p_channel)
{
    RLOGI("AT channel closed\n");
    at_close(p_channel);
    assert(0);
    s_closed = 1;

    setRadioState(RADIO_STATE_UNAVAILABLE, getRILIdByChannelCtx(p_channel));
}

/* Called on command thread */
static void onATTimeout(RILChannelCtx *p_channel)
{
    RLOGI("AT channel timeout; closing\n");
    at_close(p_channel);
    assert(0);
    s_closed = 1;

    /* FIXME cause a radio reset here */

    setRadioState(RADIO_STATE_UNAVAILABLE, getRILIdByChannelCtx(p_channel));
}
#else   /* MTK_RIL */
/* Called on command or reader thread */
static void onATReaderClosed()
{
    RLOGI("AT channel closed\n");
    at_close();
    s_closed = 1;

    setRadioState(RADIO_STATE_UNAVAILABLE);
}

/* Called on command thread */
static void onATTimeout()
{
    RLOGI("AT channel timeout; closing\n");
    at_close();

    s_closed = 1;

    /* FIXME cause a radio reset here */

    setRadioState(RADIO_STATE_UNAVAILABLE);
}
#endif  /* MTK_RIL */

static void usage(char *s)
{
#ifdef  RIL_SHLIB
    fprintf(stderr, "reference-ril requires: -p <tcp port> or -d /dev/tty_device\n");
#else   /* RIL_SHLIB */
    fprintf(stderr, "usage: %s [-p <tcp port>] [-d /dev/tty_device]\n", s);
    exit(-1);
#endif  /* RIL_SHLIB */
}


#ifdef  MTK_RIL
/* These nodes are created by gsm0710muxd */
#define RIL_SUPPORT_CHANNELS_MAX_NAME_LEN 32
char s_mux_path[RIL_SUPPORT_CHANNELS][RIL_SUPPORT_CHANNELS_MAX_NAME_LEN] = {
#ifdef MTK_RIL_MD2
    "/dev/radio/pttynoti-md2",
    "/dev/radio/pttycmd1-md2",
    "/dev/radio/pttycmd2-md2",
    "/dev/radio/pttycmd3-md2",
    "/dev/radio/pttycmd4-md2",
    "/dev/radio/atci1-md2",
#ifdef  MTK_GEMINI
    "/dev/radio/ptty2noti-md2",
    "/dev/radio/ptty2cmd1-md2",
    "/dev/radio/ptty2cmd2-md2",
    "/dev/radio/ptty2cmd3-md2",
    "/dev/radio/ptty2cmd4-md2",
    "/dev/radio/atci2-md2",
#endif  /* MTK_GEMINI */

#else

    "/dev/radio/pttynoti",
    "/dev/radio/pttycmd1",
    "/dev/radio/pttycmd2",
    "/dev/radio/pttycmd3",
    "/dev/radio/pttycmd4",
    "/dev/radio/atci1",
#ifdef  MTK_GEMINI
    "/dev/radio/ptty2noti",
    "/dev/radio/ptty2cmd1",
    "/dev/radio/ptty2cmd2",
    "/dev/radio/ptty2cmd3",
    "/dev/radio/ptty2cmd4",
    "/dev/radio/atci2",

#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
    "/dev/radio/ptty3noti",
    "/dev/radio/ptty3cmd1",
    "/dev/radio/ptty3cmd2",
    "/dev/radio/ptty3cmd3",
    "/dev/radio/ptty3cmd4",
    "/dev/radio/atci3",
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
    "/dev/radio/ptty4noti",
    "/dev/radio/ptty4cmd1",
    "/dev/radio/ptty4cmd2",
    "/dev/radio/ptty4cmd3",
    "/dev/radio/ptty4cmd4",
    "/dev/radio/atci4",
#endif
#endif  /* MTK_GEMINI */


#endif
};

static void switchMuxPath() {
#ifdef  MTK_GEMINI
#ifndef MTK_RIL_MD2
    char prop_value[PROPERTY_VALUE_MAX] = { 0 };
    char s_mux_path_tmp[RIL_CHANNEL_OFFSET][RIL_SUPPORT_CHANNELS_MAX_NAME_LEN] = {0};
    int targetSim = 0;
    int i;

    property_get(PROPERTY_3G_SIM, prop_value, "1");

    targetSim = atoi(prop_value);
    RLOGD("targetSim : %d", targetSim);
    if (targetSim*RIL_CHANNEL_OFFSET > RIL_SUPPORT_CHANNELS) {
        RLOGD("!!!! targetSim*RIL_CHANNEL_OFFSET > RIL_SUPPORT_CHANNELS");
    } else if (targetSim != 1){
        // exchange mux channel for SIM switch

        for(i=0;i<RIL_CHANNEL_OFFSET; i++) {
            strcpy(s_mux_path_tmp[i], s_mux_path[(targetSim-1)*RIL_CHANNEL_OFFSET+i]);
            strcpy(s_mux_path[(targetSim-1)*RIL_CHANNEL_OFFSET+i], s_mux_path[i]);
            strcpy(s_mux_path[i], s_mux_path_tmp[i]);
        }

    }
    for(i=0;i<RIL_SUPPORT_CHANNELS; i++) {
        RLOGD("s_mux_path[%d] = %s", i, s_mux_path[i]);
    }

#endif // MTK_RIL_MD2
#endif //#ifdef  MTK_GEMINI
}

static void doSWSimSwitch(int target3GSim)
{
    set3GCapability(NULL, target3GSim);
    //requestResetRadio(NULL, 0, NULL);
    property_set("ril.mux.report.case", "6");
    property_set("ctl.start", "muxreport-daemon");
    s_main_loop = 0;
}

static int performSWSimSwitchIfNecessary(int sim3G)
{
    int continueInit = 1;
    int telephonyMode = getTelephonyMode();
    int firstModem = getFirstModem();
    RLOGI("current telephony mode and 3G SIM: [mode:%d, 3G SIM:%d, FirstMD:%d]", telephonyMode, sim3G, firstModem);
    switch (telephonyMode) {
        case TELEPHONY_MODE_5_WGNTG_DUALTALK:
        case TELEPHONY_MODE_6_TGNG_DUALTALK:
        case TELEPHONY_MODE_7_WGNG_DUALTALK:
        case TELEPHONY_MODE_8_GNG_DUALTALK:
#ifdef MTK_RIL_MD2
            if (sim3G == CAPABILITY_3G_SIM1) {
                RLOGI("MD2 need to do default SW switch [%d, %d]", firstModem, telephonyMode);
                doSWSimSwitch(CAPABILITY_3G_SIM2);
                continueInit = 0;
            } else {
                s_isSimSwitched = 1;
            }
#else
            s_isSimSwitched = 0;
#endif
            break;
        case TELEPHONY_MODE_100_TDNC_DUALTALK:
#ifdef MTK_RIL_MD2
            if (!isInternationalRoamingEnabled()) {
                s_isSimSwitched = 1;
            } else if (sim3G == CAPABILITY_3G_SIM1 && !isInternationalRoamingEnabled()) {
                RLOGI("MD2 need to do default SW switch [%d, %d]", firstModem, telephonyMode);
                doSWSimSwitch(CAPABILITY_3G_SIM2);
                continueInit = 0;
            } else {
                s_isSimSwitched = 1;
            }
#else
            s_isSimSwitched = 0;
#endif
            break;
        case TELEPHONY_MODE_101_FDNC_DUALTALK:
            s_isSimSwitched = 0;
            break;
        default:
            if (sim3G >= CAPABILITY_3G_SIM2)
                s_isSimSwitched = 1;
            else
                s_isSimSwitched = 0;
            break;
    }
    return continueInit;
}

static void sendRadioCapabilityDoneIfNeeded () {
    int sim3G = queryMainProtocol();
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    int sessionId = 0;

    property_get(PROPERTY_SET_RC_SESSION_ID[0], property_value, "0");
    sessionId = atoi(property_value);
    RLOGI("sendRadioCapabilityDoneIfNeeded sessionId:%d", sessionId);
    if (sessionId != 0) {
        RIL_RadioCapability* rc = (RIL_RadioCapability*) malloc(sizeof(RIL_RadioCapability));
        memset(rc, 0, sizeof(RIL_RadioCapability));
        rc->version = RIL_RADIO_CAPABILITY_VERSION;
        rc->session = sessionId;
        rc->phase = RC_PHASE_UNSOL_RSP;
        rc->rat = RAF_GSM;
        rc->status = RC_STATUS_SUCCESS;

        RLOGI("sim switch done, send URC, sim3G %d", sim3G);

        switch(sim3G) {
        #if (SIM_COUNT >= 2)
            case CAPABILITY_3G_SIM2:
                strcpy(rc->logicalModemUuid, s_logicalModemId[1]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_1);
        #if (SIM_COUNT >= 3)
                strcpy(rc->logicalModemUuid, s_logicalModemId[2]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_3);
        #endif
        #if (SIM_COUNT >= 4)
                strcpy(rc->logicalModemUuid, s_logicalModemId[3]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_4);
        #endif
                if (isLteSupport()) {
                    rc->rat = RAF_GSM | RAF_UMTS | RAF_LTE;
                } else {
                    rc->rat = RAF_GSM | RAF_UMTS;
                }
                strcpy(rc->logicalModemUuid, s_logicalModemId[0]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_2);
                break;
        #if (SIM_COUNT >= 3)
            case CAPABILITY_3G_SIM3:
                strcpy(rc->logicalModemUuid, s_logicalModemId[2]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_1);
                strcpy(rc->logicalModemUuid, s_logicalModemId[1]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_2);
        #if (SIM_COUNT >= 4)
                strcpy(rc->logicalModemUuid, s_logicalModemId[3]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_4);
        #endif
                if (isLteSupport()) {
                    rc->rat = RAF_GSM | RAF_UMTS | RAF_LTE;
                } else {
                    rc->rat = RAF_GSM | RAF_UMTS;
                }
                strcpy(rc->logicalModemUuid, s_logicalModemId[0]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_3);
                break;
        #endif
        #if (SIM_COUNT >= 4)
            case CAPABILITY_3G_SIM4:
                strcpy(rc->logicalModemUuid, s_logicalModemId[3]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_1);
                strcpy(rc->logicalModemUuid, s_logicalModemId[1]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_2);
                strcpy(rc->logicalModemUuid, s_logicalModemId[2]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_3);
                if (isLteSupport()) {
                    rc->rat = RAF_GSM | RAF_UMTS | RAF_LTE;
                } else {
                    rc->rat = RAF_GSM | RAF_UMTS;
                }
                strcpy(rc->logicalModemUuid, s_logicalModemId[0]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_4);
                break;
        #endif
        #endif // #if (SIM_COUNT >= 2)
            default:
            case CAPABILITY_3G_SIM1:
        #if (SIM_COUNT >= 2)
                strcpy(rc->logicalModemUuid, s_logicalModemId[1]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_2);
        #if (SIM_COUNT >= 3)
                strcpy(rc->logicalModemUuid, s_logicalModemId[2]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_3);
        #endif
        #if (SIM_COUNT >= 4)
                strcpy(rc->logicalModemUuid, s_logicalModemId[3]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_4);
        #endif
        #endif
                if (isLteSupport()) {
                    rc->rat = RAF_GSM | RAF_UMTS | RAF_LTE;
                } else {
                    rc->rat = RAF_GSM | RAF_UMTS;
                }
                strcpy(rc->logicalModemUuid, s_logicalModemId[0]);
                RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, rc, sizeof(RIL_RadioCapability), RIL_SOCKET_1);
                break;
        }
    }
}

int isUserLoad() {
    int isUserLoad = 0;
    char property_value_emulation[PROPERTY_VALUE_MAX] = { 0 };
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("ril.emulation.userload", property_value_emulation, "0");
    if(strcmp("1", property_value_emulation) == 0) {
        return 1;
    }
    property_get("ro.build.type", property_value, "");
    isUserLoad = (strcmp("user", property_value) == 0);
    return isUserLoad;
}

static void *mainLoop(void *param)
{
    int ret;
    int i;
    RILChannelCtx *p_channel;
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    int curr_share_modem = 0;

    s_isUserLoad = isUserLoad();
    AT_DUMP("== ", "entering mainLoop()", -1);
    at_set_on_reader_closed(onATReaderClosed);
    at_set_on_timeout(onATTimeout);
    initRILChannels();
    getLogicModemId();
    switchMuxPath();
    while (s_main_loop) {
        for (i = 0; i < RIL_SUPPORT_CHANNELS; i++) {
            p_channel = getChannelCtxbyId(i);

            while (p_channel->fd < 0) {
                do
                    p_channel->fd = open(s_mux_path[i], O_RDWR);
                while (p_channel->fd < 0 && errno == EINTR);

                if (p_channel->fd < 0) {
                    perror("opening AT interface. retrying...");
                    RLOGE("could not connect to %s: %s",
                          s_mux_path[i], strerror(errno));
                    sleep(10);
                    /* never returns */
                } else {
                    struct termios ios;
                    tcgetattr(p_channel->fd, &ios);
                    ios.c_lflag = 0; /* disable ECHO, ICANON, etc... */
                    ios.c_iflag = 0;
                    tcsetattr(p_channel->fd, TCSANOW, &ios);
                }
            }


            s_closed = 0;
            ret = at_open(p_channel->fd, onUnsolicited, p_channel);


            if (ret < 0) {
                RLOGE("AT error %d on at_open\n", ret);
                return 0;
            }
        }

        int sim3G = queryMainProtocol();
        if (sim3G != RIL_get3GSIM()) {
            RLOGE("sim switch property is different with modem ES3G?, sync it and restart");
            resetRadio();
            return 0;
        }
        sendRadioCapabilityDoneIfNeeded();

        // update sim switch status when booting
        setSimSwitchProp(sim3G);

        if (performSWSimSwitchIfNecessary(sim3G)) {
            initialCidTable();
            if (isDualTalkMode()) {
                RLOGI("DualTalk mode and default switch status: %d", s_isSimSwitched);
            } else {
                RLOGI("Is SIM Switch now: [%d], 3G SIM mapped for framework: [SIM%d]", s_isSimSwitched, sim3G);
                char* value = NULL;
                asprintf(&value, "%d", sim3G);
                property_set(PROPERTY_3G_SIM, value);
                free(value);
            }

            RLOGI("Start initialized callback");
            RIL_requestTimedCallback(initializeCallback, (void *)&s_pollSimId, &TIMEVAL_0);

            //BEGIN mtk03923 [20110711][Ingore SIM2 when SHARED_MODEM=single]
#ifdef  MTK_GEMINI
            if (isInternationalRoamingEnabled()
                || getTelephonyMode() == 100
                || getTelephonyMode() == 101) {
                curr_share_modem = 2;
            } else {
                if (isDualTalkMode()) {
                    curr_share_modem = 1;
                } else {
                    // SPROUT workarount
                    curr_share_modem = 2;
                }
            }
            RLOGI("Start initialized callback 2 %d", curr_share_modem);
            switch (curr_share_modem) {
                case 2:
                    RIL_requestTimedCallback(initializeCallback, (void *)&s_pollSimId2, &TIMEVAL_0);
                    break;
                default:
                    break;
            }
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
            RLOGI("initializeCallback ril3");
            RIL_requestTimedCallback(initializeCallback, (void *)&s_pollSimId3, &TIMEVAL_0);
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
            RLOGI("initializeCallback ril4");
            RIL_requestTimedCallback(initializeCallback, (void *)&s_pollSimId4, &TIMEVAL_0);
#endif
#endif  /* MTK_GEMINI */
            // Give initializeCallback a chance to dispatched, since
            // we don't presently have a cancellation mechanism
            sleep(1);
            //END   mtk03923 [20110711][Ingore SIM2 when SHARED_MODEM=single]
        }

        waitForClose();
        RLOGI("Re-opening after close");
    }
    RLOGI("Main loop exit");
    return 0;
}
#else   /* MTK_RIL */
static void *mainLoop(void *param)
{
    int fd;
    int ret;
    char path[50];
    int ttys_index;

    AT_DUMP("== ", "entering mainLoop()", -1);
    at_set_on_reader_closed(onATReaderClosed);
    at_set_on_timeout(onATTimeout);

    for (;; ) {
        fd = -1;
        while (fd < 0) {
            if (s_port > 0) {
                fd = socket_loopback_client(s_port, SOCK_STREAM);
            } else if (s_device_socket) {
                if (!strcmp(s_device_path, "/dev/socket/qemud")) {
                    /* Qemu-specific control socket */
                    fd = socket_local_client("qemud", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
                    if (fd >= 0) {
                        char answer[2];

                        if (write(fd, "gsm", 3) != 3 ||
                            read(fd, answer, 2) != 2 ||
                            memcmp(answer, "OK", 2) != 0) {
                            close(fd);
                            fd = -1;
                        }
                    }
                } else {
                    fd = socket_local_client(s_device_path, ANDROID_SOCKET_NAMESPACE_FILESYSTEM, SOCK_STREAM);
                }
            } else if (s_device_path != NULL) {
                fd = open(s_device_path, O_RDWR);
                if (fd >= 0 && !memcmp(s_device_path, "/dev/ttyS", 9)) {
                    /* disable echo on serial ports */
                    struct termios ios;
                    tcgetattr(fd, &ios);
                    ios.c_lflag = 0; /* disable ECHO, ICANON, etc... */
                    ios.c_iflag = 0;
                    tcsetattr(fd, TCSANOW, &ios);
                }
            }
#if 0
            else if (s_device_range_begin >= 0 && s_device_range_end >= 0) {
                RLOGD("Open ttyS....");
                ttys_index = s_device_range_begin;
                while (ttys_index <= s_device_range_end) {
                    sprintf(path, "/dev/ttyS%d", ttys_index);
                    fd = open(path, O_RDWR);
                    if (fd >= 0) {
                        /* disable echo on serial ports */
                        struct termios ios;
                        tcgetattr(fd, &ios);
                        ios.c_lflag = 0; /* disable ECHO, ICANON, etc... */
                        ios.c_iflag = 0;
                        tcsetattr(fd, TCSANOW, &ios);
                    } else {
                        RLOGE("Can't open the device /dev/ttyS%d: %s", ttys_index, strerror(errno));
                    }
                    ttys_index++;
                }
            }
#endif

            if (fd < 0) {
                perror("opening AT interface. retrying...");
                sleep(10);
                /* never returns */
            }
        }

        RLOGD("FD: %d", fd);

        s_closed = 0;
        ret = at_open(fd, onUnsolicited);

        if (ret < 0) {
            RLOGE("AT error %d on at_open\n", ret);
            return 0;
        }

        RIL_requestTimedCallback(initializeCallback, NULL, &TIMEVAL_0);

        // Give initializeCallback a chance to dispatched, since we don't presently have a cancellation mechanism
        sleep(1);

        waitForClose();
        RLOGI("Re-opening after close");
    }
}
#endif  /* MTK_RIL */

#ifdef  MTK_RIL
RIL_RadioState getRadioState(RIL_SOCKET_ID rid)
{
    RIL_RadioState radioState = sState;

#ifdef  MTK_GEMINI
    if (RIL_SOCKET_2 == rid)
        radioState = sState2;
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
    if (RIL_SOCKET_3 == rid)
        radioState = sState3;
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4SIM*/
    if (RIL_SOCKET_4 == rid)
        radioState = sState4;
#endif
#endif

    RLOGI("getRadioState(): radioState=%d\n", radioState);

    return radioState;
}
#else   /* MTK_RIL */
RIL_RadioState getRadioState(void)
{
    return sState;
}
#endif  /* MTK_RIL */


#ifdef  MTK_RIL
void setRadioState(RIL_RadioState newState, RIL_SOCKET_ID rid)
#else
void setRadioState(RIL_RadioState newState)
#endif  /* MTK_RIL */
{
    RIL_RadioState oldState;
    RIL_RadioState *pState = NULL;

    pthread_mutex_lock(&s_state_mutex);

    oldState = sState;
    pState = &sState;
#ifdef  MTK_GEMINI
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
    if (RIL_SOCKET_3 == rid) {
        oldState = sState3;
        pState = &sState3;
    }
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4SIM*/
    if (RIL_SOCKET_4 == rid) {
        oldState = sState4;
        pState = &sState4;
    }
#endif
    if (RIL_SOCKET_2 == rid) {
        oldState = sState2;
        pState = &sState2;
    }
#endif  /* MTK_GEMINI */

    if (s_closed > 0) {
        // If we're closed, the only reasonable state is
        // RADIO_STATE_UNAVAILABLE
        // This is here because things on the main thread
        // may attempt to change the radio state after the closed
        // event happened in another thread
        assert(0);
        newState = RADIO_STATE_UNAVAILABLE;
    }

    if (*pState != newState || s_closed > 0) {
        *pState = newState;
        assert(0);
        pthread_cond_broadcast(&s_state_cond);
    }

    pthread_mutex_unlock(&s_state_mutex);

    RLOGI("setRadioState(%d): newState=%d, oldState=%d, *pState=%d\n", rid, newState, oldState, *pState);

    /* do these outside of the mutex */
    if (*pState != oldState || oldState == RADIO_STATE_SIM_LOCKED_OR_ABSENT) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0, rid);

        /* FIXME onSimReady() and onRadioPowerOn() cannot be called
         * from the AT reader thread
         * Currently, this doesn't happen, but if that changes then these
         * will need to be dispatched on the request thread
         */
        /*
        if (*pState == RADIO_STATE_SIM_READY)
            onSIMReady(rid);
        else if (*pState == RADIO_STATE_SIM_NOT_READY)
            onRadioPowerOn(rid);
        */
        /* Android KK. But in order to send Telephony framework SIM_NOT_READY. It should call onRadioPowerOn even *pState is RADIO_STATE_OFF. */
        if (*pState == RADIO_STATE_ON || *pState == RADIO_STATE_OFF) {
            onRadioPowerOn(rid);
        }

        /* the following property is used for auto test */
        if (*pState == RADIO_STATE_SIM_READY)
            property_set((const char *)PROPERTY_RIL_SIM_READY, (const char *)"true");
        else
            property_set((const char *)PROPERTY_RIL_SIM_READY, (const char *)"false");
    }
}


#ifdef  RIL_SHLIB
pthread_t s_tid_mainloop;

/* 0: vendor RIL API wihtout socket id
   1: vendor RIL API with socket id */
const int RIL_MultiSocketInRild(void)
{
    return 1;
}

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;
    int index;
    char *tmp;
    pthread_attr_t attr;
    char path[50];
    int ttys_index, i;

    s_rilenv = env;

    while (-1 != (opt = getopt(argc, argv, "p:d:s:m:"))) {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                    return NULL;
                }
                RLOGI("Opening loopback port %d\n", s_port);
                break;

            case 'd':
                s_device_path = optarg;
                RLOGI("Opening tty device %s\n", s_device_path);
                break;

            case 's':
                s_device_path = optarg;
                s_device_socket = 1;
                RLOGI("Opening socket %s\n", s_device_path);
                RLOGD("Got device!");
                break;
            case 'c':
                RLOGD("skip the argument");
                break;

#ifdef  MTK_RIL
            case 'm':
                RLOGD("Input range: %s %s %s", optarg, argv[1], argv[2]);

                tmp = strtok(optarg, " ");
                s_device_range_begin = atoi(tmp);
                tmp = strtok(NULL, " ");
                s_device_range_end = atoi(tmp);

                if ((s_device_range_end - s_device_range_begin + 1) != RIL_SUPPORT_CHANNELS) {
                    RLOGE("We can't accept the input configuration for muliple channel since we need %d COM ports", RIL_SUPPORT_CHANNELS);
                    return NULL;
                }

                RLOGD("Open the ttyS%d to ttyS%d", s_device_range_begin, s_device_range_end);

                RLOGD("Link ttyS....");
                ttys_index = s_device_range_begin;
                i = 0;
                switchMuxPath();
                while (ttys_index <= s_device_range_end) {
                    sprintf(path, "/dev/ttyS%d", ttys_index);
                    RLOGD("Unlock %s on Link %s", path, s_mux_path[i]);
                    /*if(chmod(path, 0666) < 0 )
                     * {
                     *  RLOGD("chomod: system-error: '%s' (code: %d)", strerror(errno), errno);
                     *  return NULL;
                     * }*/
                    if (symlink(path, s_mux_path[i]) < 0) {
                        RLOGD("symlink: system-error: '%s' (code: %d)", strerror(errno), errno);
                        return NULL;
                    }
                    ttys_index++;
                    i++;
                }
                break;
#endif  /* MTK_RIL */

            default:
                usage(argv[0]);
                RLOGD("1- return null, %c", opt);
                //return NULL;
        }
    }

    if (s_port < 0 && s_device_path == NULL &&
        (s_device_range_begin < 0 || s_device_range_end < 0)) {
        usage(argv[0]);
        return NULL;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_mainloop, &attr, mainLoop, NULL);

    return &s_callbacks;
}

const RIL_RadioFunctions *RIL_SAP_Init(const struct RIL_Env *env, int argc,
        char **argv) {
    s_rilsapenv = env;
    return &s_sapcallbacks;
}
#else   /* RIL_SHLIB */
int main(int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;

    while (-1 != (opt = getopt(argc, argv, "p:d:"))) {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0)
                    usage(argv[0]);
                RLOGI("Opening loopback port %d\n", s_port);
                break;

            case 'd':
                s_device_path = optarg;
                RLOGI("Opening tty device %s\n", s_device_path);
                break;

            case 's':
                s_device_path = optarg;
                s_device_socket = 1;
                RLOGI("Opening socket %s\n", s_device_path);
                break;

            default:
                usage(argv[0]);
        }
    }

    if (s_port < 0 && s_device_path == NULL)
        usage(argv[0]);

    RIL_register(&s_callbacks);

    mainLoop(NULL);

    return 0;
}
#endif  /* RIL_SHLIB */

int RIL_is3GSwitched()
{
#if 0
    int is3GSwitched = 0;

    char *sim = malloc(sizeof(char) * 2);
    memset(sim, 0, sizeof(char) * 2);

    property_get(PROPERTY_3G_SIM, sim, "1");

    if (strcmp(sim, CAPABILITY_3G_SIM2) == 0)
        is3GSwitched = 1;

    free(sim);

    return is3GSwitched;
#else
    return s_isSimSwitched;
#endif
}

/*********************************
    [Telephony Mode Definition]
        0: default, not MT6589
        1: W+G Gemini
        2: T+G Gemini
        3: FDD Single SIM
        4: TDD Single SIM
        5: W+T DualTalk
        6: T+G DualTalk
        7: W+G DualTalk
        8: G+G DualTalk
*********************************/
int getTelephonyMode() {
    //s_telephonyMode = MTK_TELEPHONY_MODE;
    s_telephonyMode = 0;
    if (s_telephonyMode < 0) {
        s_telephonyMode = 0;
    }
    return s_telephonyMode;
}

int RIL_get3GSIM()
{
    char tmp[PROPERTY_VALUE_MAX] = { 0 };
    int simId = 0;

    property_get(PROPERTY_3G_SIM, tmp, "1");
    simId = atoi(tmp);
    return simId;
}

void setSimSwitchProp(int SimId)
{
    char* simSwitchValue = NULL;
    asprintf(&simSwitchValue, "%d", SimId);
    property_set(PROPERTY_3G_SIM, simSwitchValue);
    free(simSwitchValue);
}

int isEmulatorRunning()
{
    int isEmulatorRunning = 0;
    char *qemu = malloc(sizeof(char) * PROPERTY_VALUE_MAX);
    memset(qemu, 0, sizeof(char) * PROPERTY_VALUE_MAX);
    property_get("ro.kernel.qemu", qemu, "");
    if (strcmp(qemu, "1") == 0)
        isEmulatorRunning = 1;

    free(qemu);
    return isEmulatorRunning;
}

int isCCCIPowerOffModem()
{
    /* Return 1 if CCCI support power-off modem completely and deeply.
       If not support, we switch on/off modem by AT+EPON and AT+EPOF */
#ifdef MTK_MD_SHUT_DOWN_NT
    return 1;
#else
    return 0;
#endif
}

int getFirstModem()
{
    //here we could merge with external modem mechanism
    int result = FIRST_MODEM_NOT_SPECIFIED;
    int telephonyMode = getTelephonyMode();
    switch (telephonyMode) {
        case TELEPHONY_MODE_0_NONE:
        case TELEPHONY_MODE_100_TDNC_DUALTALK:
        case TELEPHONY_MODE_101_FDNC_DUALTALK:
        case TELEPHONY_MODE_102_WNC_DUALTALK:
        case TELEPHONY_MODE_103_TNC_DUALTALK:
            //do nothing since there is no first MD concept in these cases
            break;
        default: {
            //result = MTK_FIRST_MD;
            result = 1;

            if (result == FIRST_MODEM_NOT_SPECIFIED) {
#ifdef MTK_RIL_MD1
                result = FIRST_MODEM_MD1;
#else
                result = FIRST_MODEM_MD2;
#endif
            }
        }
    }
    return result;
}

int isDualTalkMode()
{
    int isDtSupport = 0;
    char property_value[PROPERTY_VALUE_MAX] = { 0 };

    property_get("ro.mtk_dt_support", property_value, "0");
    isDtSupport = atoi(property_value);
    return isDtSupport;
}

static char *gettaskName(char *buf, int size, FILE *file)
{
    int cnt = 0;
    int eof = 0;
    int eol = 0;
    int c;

    if (size < 1) {
        return NULL;
    }

    while (cnt < (size - 1)) {
        c = getc(file);
        if (c == EOF) {
            eof = 1;
            break;
        }

        *(buf + cnt) = c;
        cnt++;

        if (c == '\n') {
            eol = 1;
            break;
        }
    }

    /* Null terminate what we've read */
    *(buf + cnt) = '\0';

    if (eof) {
        if (cnt) {
            return buf;
        } else {
            return NULL;
        }
    } else if (eol) {
        return buf;
    } else {
        /* The line is too long.  Read till a newline or EOF.
         * If EOF, return null, if newline, return an empty buffer.
         */
        while(1) {
            c = getc(file);
            if (c == EOF) {
                return NULL;
            } else if (c == '\n') {
                *buf = '\0';
                return buf;
            }
        }
    }
    return NULL;
}


int findPid(char* name)
{
    int pid = -1;

    if (name != NULL) {
        RLOGI("Find PID of %s <process name>", name);
    } else {
        return pid;
    }

    const char* directory = "/proc";
    size_t      taskNameSize = 256;
    char*       taskName = calloc(taskNameSize, sizeof(char));

    DIR* dir = opendir(directory);

    if (dir) {
        struct dirent* de = 0;

        while ((de = readdir(dir)) != 0) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            int res = sscanf(de->d_name, "%d", &pid);

            if (res == 1) {
                // we have a valid pid

                // open the cmdline file to determine what's the name of the process running
                char cmdline_file[256] = {0};
                sprintf(cmdline_file, "%s/%d/cmdline", directory, pid);

                FILE* cmdline = fopen(cmdline_file, "r");

                if (gettaskName(taskName, taskNameSize, cmdline) != NULL) {
                    // is it the process we care about?
                    if (strstr(taskName, name) != 0) {
                        RLOGI("%s process, with PID %d, has been detected.", name, pid);
                        // just let the OS free this process' memory!
                        fclose(cmdline);
                        free(taskName);
                        return pid;
                    }
                }

                fclose(cmdline);
            }
        }

        closedir(dir);
    }

    // just let the OS free this process' memory!
    free(taskName);

    return -1;
}

// ALPS00447663, EVDO power on
int getExternalModemSlot()
{
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("ril.external.md", property_value, "0");
    return atoi(property_value) - 1;
}

int isInternationalRoamingEnabled()
{
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("ril.evdo.irsupport", property_value, "0");
    return atoi(property_value);
}

int isEVDODTSupport()
{
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("ril.evdo.dtsupport", property_value, "0");
    return atoi(property_value);
}

int isEvdoOnDualtalkMode()
{
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("mediatek.evdo.mode.dualtalk", property_value, "1");
    int mode = atoi(property_value);
    RLOGI("evdoOnDualtalkMode mode: %d", mode);
    return mode;
}

int isSupportCommonSlot()
{
    /* Return 1 if support SIM Hot Swap with Common Slot Feature */
#ifdef MTK_SIM_HOT_SWAP_COMMON_SLOT
    return 1;
#else
    return 0;
#endif
}

int isLteSupport() {
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("ro.boot.opt_lte_support", property_value, "0");
    return atoi(property_value);
}

/// M: SAP start
#if 0
void sendBtSapResponseComplete(RIL_Token t, RIL_Errno ret, MsgId msgId, void *data) {

    const pb_field_t *fields = NULL;
    size_t encoded_size = 0;
    uint32_t written_size;
    size_t buffer_size = 0;
    pb_ostream_t ostream;
    bool success = false;
    ssize_t written_bytes;
    int i = 0;

    RLOGD("sendBtSapResponseComplete, start (%d)", msgId);

    switch (msgId) {
        case MsgId_RIL_SIM_SAP_CONNECT:
            fields = RIL_SIM_SAP_CONNECT_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_DISCONNECT:
            fields = RIL_SIM_SAP_DISCONNECT_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_APDU:
            fields = RIL_SIM_SAP_APDU_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_TRANSFER_ATR:
            fields = RIL_SIM_SAP_TRANSFER_ATR_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_POWER:
            fields = RIL_SIM_SAP_POWER_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_RESET_SIM:
            fields = RIL_SIM_SAP_RESET_SIM_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL:
            fields = RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_ERROR_RESP:
            fields = RIL_SIM_SAP_ERROR_RSP_fields;
            break;
        default:
            RLOGE("sendBtSapResponseComplete, MsgId is mistake!");
            return;
    }

    if ((success = pb_get_encoded_size(&encoded_size, fields, data)) &&
            encoded_size <= INT32_MAX) {
        //buffer_size = encoded_size + sizeof(uint32_t);
        buffer_size = encoded_size;
        uint8_t buffer[buffer_size];
        //written_size = htonl((uint32_t) encoded_size);
        ostream = pb_ostream_from_buffer(buffer, buffer_size);
        //pb_write(&ostream, (uint8_t *)&written_size, sizeof(written_size));
        success = pb_encode(&ostream, fields, data);

        if(success) {
            RLOGD("sendBtSapResponseComplete, Size: %d (0x%x) Size as written: 0x%x",
                encoded_size, encoded_size, written_size);
            // Send response
            RIL_SAP_onRequestComplete(t, ret, buffer, buffer_size);
        } else {
            RLOGE("sendBtSapResponseComplete, Encode failed!");
        }
    } else {
        RLOGE("Not sending response type %d: encoded_size: %u. encoded size result: %d",
        msgId, encoded_size, success);
    }
}
#endif
/// SAP end

