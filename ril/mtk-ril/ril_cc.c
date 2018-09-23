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

/* MTK proprietary start */
#define CC_CHANNEL_CTX getRILChannelCtxFromToken(t)

char *setupCpiData[9] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
int callWaiting = 0;
int isAlertSet = 0;
int hasReceivedRing = 0;
int inCallNumber = 0;
// BEGIN mtk03923 [20120210][ALPS00114093]
int inDTMF = 0;
// END mtk03923 [20120210][ALPS00114093]
bool isRecvECPI0 = false;
int ringCallID = 0;
RIL_SOCKET_ID ridRecvECPI0 = RIL_SOCKET_NUM;

// [ALPS00242104]Invalid number show but cannot call drop when dial VT call in 2G network
// mtk04070, 2012.02.24
int bUseLocalCallFailCause = 0;
int dialLastError = 0;
/* MTK proprietary end */

static int clccStateToRILState(int state, RIL_CallState *p_state) {
    switch (state) {
        case 0: *p_state = RIL_CALL_ACTIVE;   return 0;
        case 1: *p_state = RIL_CALL_HOLDING;  return 0;
        case 2: *p_state = RIL_CALL_DIALING;  return 0;
        case 3: *p_state = RIL_CALL_ALERTING; return 0;
        case 4: *p_state = RIL_CALL_INCOMING; return 0;
        case 5: *p_state = RIL_CALL_WAITING;  return 0;
        default: return -1;
    }
}

/**
 * Note: directly modified line and has *p_call point directly into
 * modified line
 */
static int callFromCLCCLine(char *line, RIL_Call *p_call) {
    // +CLCC: 1,0,2,0,0,\"+18005551212\",145
    // index,isMT,state,mode,isMpty(,number,TOA)?

    int err;
    int state;
    int mode;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->index));
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &(p_call->isMT));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto error;

    err = clccStateToRILState(state, &(p_call->state));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &mode);
    if (err < 0) goto error;

    p_call->isVoice = (mode == 0);

    err = at_tok_nextbool(&line, &(p_call->isMpty));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(p_call->number));

        /* tolerate null here */
        if (err < 0) return 0;

        // Some lame implementations return strings
        // like "NOT AVAILABLE" in the CLCC line
        if ((p_call->number != NULL) &&
            (strspn(p_call->number, "+*#0123456789") != strlen(p_call->number))) {
            p_call->number = NULL;
        }

        err = at_tok_nextint(&line, &p_call->toa);
        if (err < 0) goto error;
    }

    p_call->uusInfo = NULL;

    return 0;

error:
    LOGE("invalid CLCC line\n");
    return -1;
}


extern void requestGetCurrentCalls(void *data, size_t datalen, RIL_Token t) {
    int err;
    ATResponse *p_response;
    ATLine *p_cur;
    int countCalls;
    int countValidCalls;
    RIL_Call *p_calls;
    RIL_Call **pp_calls;
    int i;
    int needRepoll = 0;

    err = at_send_command_multiline("AT+CLCC", "+CLCC:", &p_response, CC_CHANNEL_CTX);

    if (err != 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        at_response_free(p_response);
        return;
    }

    /* count the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next)
        countCalls++;

    /* yes, there's an array of pointers and then an array of structures */

    pp_calls = (RIL_Call **)alloca(countCalls * sizeof(RIL_Call *));
    p_calls = (RIL_Call *)alloca(countCalls * sizeof(RIL_Call));
    memset(p_calls, 0, countCalls * sizeof(RIL_Call));

    /* init the pointer array */
    for (i = 0; i < countCalls; i++)
        pp_calls[i] = &(p_calls[i]);

    for (countValidCalls = 0, p_cur = p_response->p_intermediates
         ; p_cur != NULL
         ; p_cur = p_cur->p_next
         ) {
        err = callFromCLCCLine(p_cur->line, p_calls + countValidCalls);

        if (err != 0)
            continue;

        countValidCalls++;
    }


    RIL_onRequestComplete(t, RIL_E_SUCCESS, pp_calls, countValidCalls * sizeof(RIL_Call *));

    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

void requestDial(void *data, size_t datalen, RIL_Token t, int isEmergency) {
    RIL_Dial *p_dial;
    char *cmd;
    const char *clir;
    int ret;
    ATResponse *p_response = NULL;

    /*mtk00924: ATDxxxI can not used for FDN check, therefore, change to #31# or *31#*/

    p_dial = (RIL_Dial *)data;

    switch (p_dial->clir) {
    case 1: /*invocation*/
        clir = "#31#";
        break;
    case 2: /*suppression*/
        clir = "*31#";
        break;
    case 0:
    default: /*subscription default*/
        clir = "";
        break;
    }

    if (isEmergency) {
        /* If an incoming call exists(+CRING is not received yet), hang it up before dial ECC */
        if ((setupCpiData[0] != NULL) && isRecvECPI0) {
            LOGD("To hang up incoming call(+CRING is not received yet) before dialing ECC!");

            asprintf(&cmd, "AT+CHLD=1%s", setupCpiData[0]);

            isRecvECPI0 = false;
            int i;
            for (i = 0; i < 9; i++) {
                free(setupCpiData[i]);
                setupCpiData[i] = NULL;
            }

            at_send_command(cmd, NULL, CC_CHANNEL_CTX);
            free(cmd);

            sleep(1);
        }

        asprintf(&cmd, "ATDE%s%s;", clir, p_dial->address);
    } else {
        // BEGIN mtk03923 [20111004][ALPS00077405]
        // CC operation will fail when dialing number exceed 40 character due to modem capability limitation.
        if (strlen(p_dial->address) > 40) {
            LOGE("strlen(%s)=%d exceeds 40 character\n", p_dial->address, strlen(p_dial->address));

            RIL_onRequestComplete(t, RIL_E_CANCELLED, NULL, 0);
            at_response_free(p_response);

            // [ALPS00251057] It didn't pop FDN dialog when dial an invalid number
            // But this is not related to FDN issue, it returned to AP since number is too long.
            // mtk04070, 2012.03.12
            bUseLocalCallFailCause = 1;
            dialLastError = 28; /* Refer to CallFailCause.java - INVALID_NUMBER_FORMAT */

            return;
        }
        // END mtk03923 [20111004][ALPS00077405]

        asprintf(&cmd, "ATD%s%s;", clir, p_dial->address);
    }
    ret = at_send_command(cmd, &p_response, CC_CHANNEL_CTX);

    // [ALPS00242104]Invalid number show but cannot call drop when dial VT call in 2G network
    // mtk04070, 2012.02.24
    bUseLocalCallFailCause = 0;
    dialLastError = 0;

    free(cmd);

    if (ret < 0 || p_response->success == 0)
        goto error;
    /* success or failure is ignored by the upper layer here.
     * it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

void requestHangup(void *data, size_t datalen, RIL_Token t) {
    int *p_line;
    char *cmd;

    p_line = (int *)data;

    // 3GPP 22.030 6.5.5
    // "Releases a specific active call X"
    asprintf(&cmd, "AT+CHLD=1%d", p_line[0]);
    at_send_command(cmd, NULL, CC_CHANNEL_CTX);

    free(cmd);

    /* success or failure is ignored by the upper layer here.
     * it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestHangupWaitingOrBackground(void *data, size_t datalen, RIL_Token t) {
    // 3GPP 22.030 6.5.5
    // "Releases all held calls or sets User Determined User Busy
    //  (UDUB) for a waiting call."
    at_send_command("AT+CHLD=0", NULL, CC_CHANNEL_CTX);

    /* success or failure is ignored by the upper layer here.
     * it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestHangupForegroundResumeBackground(void *data, size_t datalen, RIL_Token t) {
    // 3GPP 22.030 6.5.5
    // "Releases all active calls (if any exist) and accepts
    //  the other (held or waiting) call."
    // at_send_command("AT+CHLD=1", NULL, CC_CHANNEL_CTX);
    at_send_command_multiline("AT+CHLD=1", "NO CARRIER", NULL, CC_CHANNEL_CTX);

    /* success or failure is ignored by the upper layer here.
     * it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestSwitchWaitingOrHoldingAndActive(void *data, size_t datalen, RIL_Token t) {
    int ret;
    ATResponse *p_response = NULL;

        // BEGIN mtk03923 [20120210][ALPS00114093]
        if (inDTMF) {
        RIL_onRequestComplete(t, RIL_E_CANCELLED, NULL, 0);     // RIL_E_GENERIC_FAILURE
            return;
        }
        // END mtk03923 [20120210][ALPS00114093]


    ret = at_send_command("AT+CHLD=2", &p_response, CC_CHANNEL_CTX);

    if (ret < 0 || p_response->success == 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

void requestAnswer(void *data, size_t datalen, RIL_Token t) {
    at_send_command("ATA", NULL, CC_CHANNEL_CTX);

    /* success or failure is ignored by the upper layer here.
     * it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestConference(void *data, size_t datalen, RIL_Token t) {
    int ret;
    ATResponse *p_response = NULL;

        // BEGIN mtk03923 [20120210][ALPS00114093]
        if (inDTMF) {
        RIL_onRequestComplete(t, RIL_E_CANCELLED, NULL, 0);     // RIL_E_GENERIC_FAILURE
            return;
        }
        // END mtk03923 [20120210][ALPS00114093]

    ret = at_send_command("AT+CHLD=3", &p_response, CC_CHANNEL_CTX);

    if (ret < 0 || p_response->success == 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

void requestUdub(void *data, size_t datalen, RIL_Token t) {
    /* user determined user busy */
    /* sometimes used: ATH */
    at_send_command("ATH", NULL, CC_CHANNEL_CTX);

    /* success or failure is ignored by the upper layer here.
     * it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestSeparateConnection(void *data, size_t datalen, RIL_Token t) {
    char cmd[12];
    int party = ((int *)data)[0];
    int ret;
    ATResponse *p_response = NULL;

        // BEGIN mtk03923 [20120210][ALPS00114093]
        if (inDTMF) {
        RIL_onRequestComplete(t, RIL_E_CANCELLED, NULL, 0);     // RIL_E_GENERIC_FAILURE
            return;
        }
        // END mtk03923 [20120210][ALPS00114093]


    // Make sure that party is in a valid range.
    // (Note: The Telephony middle layer imposes a range of 1 to 7.
    // It's sufficient for us to just make sure it's single digit.)
    if (party > 0 && party < 10) {
        sprintf(cmd, "AT+CHLD=2%d", party);
        ret = at_send_command(cmd, &p_response, CC_CHANNEL_CTX);

        if (ret < 0 || p_response->success == 0) {
            at_response_free(p_response);
            goto error;
        }

        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void requestExplicitCallTransfer(void *data, size_t datalen, RIL_Token t) {
    /* MTK proprietary start */
    int ret;
    ATResponse *p_response = NULL;

        // BEGIN mtk03923 [20120210][ALPS00114093]
        if (inDTMF) {
        RIL_onRequestComplete(t, RIL_E_CANCELLED, NULL, 0);     // RIL_E_GENERIC_FAILURE
            return;
        }
        // END  mtk03923 [20120210][ALPS00114093]


    ret = at_send_command("AT+CHLD=4", &p_response, CC_CHANNEL_CTX);

    if (ret < 0 || p_response->success == 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    /* MTK proprietary end */
}

void requestLastCallFailCause(void *data, size_t datalen, RIL_Token t) {
    /* MTK proprietary start */
    RIL_LastCallFailCauseInfo callFailCause;
    char *line;
    int ret;
    ATResponse *p_response = NULL;

    memset(&callFailCause, 0, sizeof(RIL_LastCallFailCauseInfo));

    // [ALPS00242104]Invalid number show but cannot call drop when dial VT call in 2G network
    // mtk04070, 2012.02.24
    if (bUseLocalCallFailCause == 1) {
       callFailCause.cause_code = dialLastError;
       LOGD("Use local call fail cause = %d", callFailCause.cause_code);
    }
    else {
        ret = at_send_command_singleline("AT+CEER", "+CEER:", &p_response, CC_CHANNEL_CTX);

       if (ret < 0 || p_response->success == 0)
           goto error;

       line = p_response->p_intermediates->line;

       ret = at_tok_start(&line);

       if (ret < 0)
           goto error;

       ret = at_tok_nextint(&line, &(callFailCause.cause_code));
       if (ret < 0)
           goto error;

       ret = at_tok_nextstr(&line, &(callFailCause.vendor_cause));
       if (ret < 0)
           goto error;

       LOGD("MD fail cause_code = %d, vendor_cause = %s",
               callFailCause.cause_code, callFailCause.vendor_cause);
    }

    /*if there are more causes need to be translated in the future,
     * discussing with APP owner to implement this in upper layer.
     * For the hard coded value, please refer to modem code.*/

    if (callFailCause.cause_code == 10)
        callFailCause.cause_code = CALL_FAIL_CALL_BARRED;
    else if (callFailCause.cause_code == 2600)
        callFailCause.cause_code = CALL_FAIL_FDN_BLOCKED;
    else if (callFailCause.cause_code == 2052)
        callFailCause.cause_code = CALL_FAIL_IMSI_UNKNOWN_IN_VLR;
    else if (callFailCause.cause_code == 2053)
        callFailCause.cause_code = CALL_FAIL_IMEI_NOT_ACCEPTED;
    else if ((callFailCause.cause_code > 127 && callFailCause.cause_code != 2165
            && callFailCause.cause_code != 380) || callFailCause.cause_code <= 0)
        callFailCause.cause_code = CALL_FAIL_ERROR_UNSPECIFIED;

    LOGD("RIL fail cause_code = %d, vendor_cause = %s",
            callFailCause.cause_code, callFailCause.vendor_cause);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &callFailCause, sizeof(RIL_LastCallFailCauseInfo));
    if (NULL != p_response) {
        at_response_free(p_response);
    }
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    if (NULL != p_response) {
        at_response_free(p_response);
    }
    /* MTK proprietary end */
}

void requestDtmf(void *data, size_t datalen, RIL_Token t) {
    char c = ((char *)data)[0];
    char *cmd;

    asprintf(&cmd, "AT+VTS=%c", (int)c);
    at_send_command(cmd, NULL, CC_CHANNEL_CTX);

    free(cmd);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

extern int rilCcMain(int request, void *data, size_t datalen, RIL_Token t) {
    switch (request) {
    case RIL_REQUEST_GET_CURRENT_CALLS:
        requestGetCurrentCalls(data, datalen, t);
        break;
    case RIL_REQUEST_DIAL:
        requestDial(data, datalen, t, 0);
        break;
    case RIL_REQUEST_HANGUP:
        requestHangup(data, datalen, t);
        break;
    case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
        requestHangupWaitingOrBackground(data, datalen, t);
        break;
    case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
        requestHangupForegroundResumeBackground(data, datalen, t);
        break;
    case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
        requestSwitchWaitingOrHoldingAndActive(data, datalen, t);
        break;
    case RIL_REQUEST_ANSWER:
        requestAnswer(data, datalen, t);
        break;
    case RIL_REQUEST_CONFERENCE:
        requestConference(data, datalen, t);
        break;
    case RIL_REQUEST_UDUB:
        requestUdub(data, datalen, t);
        break;
    case RIL_REQUEST_SEPARATE_CONNECTION:
        requestSeparateConnection(data, datalen, t);
        break;
    case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:
        requestExplicitCallTransfer(data, datalen, t);
        break;
    case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
        requestLastCallFailCause(data, datalen, t);
        break;
    case RIL_REQUEST_DTMF:
        requestDtmf(data, datalen, t);
        break;
    case RIL_REQUEST_DTMF_START:
        requestDtmfStart(data, datalen, t);
        break;
    case RIL_REQUEST_DTMF_STOP:
        requestDtmfStop(data, datalen, t);
        break;
    case RIL_REQUEST_SET_TTY_MODE:
        requestSetTTYMode(data, datalen, t);
        break;
    default:
        return 0; /* no matched request */
        break;
    }

    return 1; /* request found and handled */
}

extern int rilCcUnsolicited(const char *s, const char *sms_pdu, RILChannelCtx *p_channel) {
    RIL_SOCKET_ID rid = getRILIdByChannelCtx(p_channel);

    /* MTK proprietary start */
    if (strStartsWith(s, "RING") || strStartsWith(s, "+CRING")) {
        LOGD("receiving RING!!!!!!");

        if (!isRecvECPI0) {
            LOGD("we havn't receive ECPI0, skip this RING!");
            return 1;
        }
        if (!hasReceivedRing) {
            LOGD("receiving first RING!!!!!!");
            hasReceivedRing = 1;
        }

        if (setupCpiData[0] != NULL) {
            LOGD("sending STATE CHANGE!!!!!!");
            RIL_onUnsolicitedResponse(
                    RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                    NULL, 0, rid);

            int i;
            for (i = 0; i < 9; i++) {
                free(setupCpiData[i]);
                setupCpiData[i] = NULL;
            }
            sleep(1);
        }

        RIL_onUnsolicitedResponse(RIL_UNSOL_CALL_RING, NULL, 0, rid);
        return 1;
    } else if (strStartsWith(s, "+ECPI")) {
        onCallProgressInfoCallStateChange((char *)s, rid);
        return 1;
    } else if (strStartsWith(s, "+ESPEECH")) {
        onSpeechInfo((char *)s, rid);
        return 1;
    } else if (strStartsWith(s, "+EAIC")) {
        onIncomingCallIndication((char *)s, rid);
        return 1;
    } else if (strStartsWith(s,"+CSSI:")) {
        /* +CSSI is MO */
        onSuppSvcNotification((char *)s, 0, rid);
        return 1;
    } else if (strStartsWith(s,"+CSSU:")) {
        /* +CSSU is MT */
        onSuppSvcNotification((char *)s, 1, rid);
        return 1;
    }

    return 0;
    /* MTK proprietary end */
}
