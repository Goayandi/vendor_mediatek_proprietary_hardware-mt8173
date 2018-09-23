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
#include <cutils/properties.h>
#include <termios.h>
#include <signal.h>
#include <ctype.h>

#include <ril_callbacks.h>

#include "ril_nw.h"
#include "hardware/ccci_intf.h"

#ifdef MTK_RIL_MD1
#define LOG_TAG "RIL"
#else
#define LOG_TAG "RILMD2"
#endif

#include <utils/Log.h>

#define OEM_CHANNEL_CTX getRILChannelCtxFromToken(t)

#ifdef MTK_GEMINI
extern int isModemResetStarted;
#endif

extern int s_md_off;
extern int s_main_loop;

extern char s_logicalModemId[SIM_COUNT][MAX_UUID_LENGTH];

extern char* s_imei[RIL_SOCKET_NUM];
extern char* s_imeisv[RIL_SOCKET_NUM];
extern char* s_basebandVersion[RIL_SOCKET_NUM];
extern char* s_projectFlavor[RIL_SOCKET_NUM];
extern char* s_calData;
extern int handleOemUnsolicited(const char *s, const char *sms_pdu, RILChannelCtx* p_channel);

void requestSetMute(void * data, size_t datalen, RIL_Token t)
{
    char * cmd;
    ATResponse *p_response = NULL;
    int err;

    asprintf(&cmd, "AT+CMUT=%d", ((int *)data)[0]);
    err = at_send_command(cmd, &p_response, OEM_CHANNEL_CTX);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

void requestGetMute(void * data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err;
    int response;
    char *line;

    err = at_send_command_singleline("AT+CMUT?", "+CMUT:", &p_response, OEM_CHANNEL_CTX);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));

    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

RIL_Errno resetRadio() {
    int err;
    ATResponse *p_response = NULL;
    RILChannelCtx* p_channel = NULL;
    int i =0;
    int cur3GSim = queryMainProtocol();
    p_channel = getRILChannelCtx(RIL_SIM,getMainProtocolRid());

    RLOGI("start to reset radio");

    // only do power off when it is on
    if (s_md_off != 1)
    {
        //power off modem
        if (isCCCIPowerOffModem()) {
            s_md_off = 1;
        }

        /* Reset the modem, we will do the following steps
         * 1. AT+EPOF,
         *      do network detach and power-off the SIM
         *      By this way, we can protect the SIM card
         * 2. AT+EPON
         *      do the normal procedure of boot up
         * 3. stop muxd
         *      because we will re-construct the MUX channels
         * 4. The responsibility of Telephony Framework
         *    i. stop rild
         *    ii. start muxd to re-construct the MUX channels and start rild
         *    iii. make sure that it is OK if there is any request in the request queue
         */
    #ifdef MTK_GEMINI
        isModemResetStarted = 1;
        err = at_send_command("AT+EPOF", &p_response, p_channel);
    #else
        err = at_send_command("AT+EPOF", &p_response, p_channel);
    #endif

        if (err != 0 || p_response->success == 0) {
            RLOGW("There is something wrong with the exectution of AT+EPOF");
        }
        at_response_free(p_response);

        if (isCCCIPowerOffModem()) {
            RLOGD("Flight mode power off modem, trigger CCCI power on modem");
            triggerCCCIIoctl(CCCI_IOC_ENTER_DEEP_FLIGHT);
        }
    }
    RLOGD("update 3G Sim property : %d", cur3GSim);
    setSimSwitchProp(cur3GSim);

    //power on modem
    if (isCCCIPowerOffModem()) {
        RLOGD("Flight mode power on modem, trigger CCCI power on modem");
        property_set("gsm.ril.eboot", "1");
        triggerCCCIIoctl(CCCI_IOC_LEAVE_DEEP_FLIGHT);
    #ifdef MTK_GEMINI
        isModemResetStarted = 0;
    #endif
    } else {
    #ifdef MTK_GEMINI
        err = at_send_command("AT+EPON", &p_response, p_channel);
        isModemResetStarted = 0;
    #else
        err = at_send_command("AT+EPON", &p_response, p_channel);
    #endif
        if (err != 0 || p_response->success == 0) {
            LOGW("There is something wrong with the exectution of AT+EPON");
        }
        at_response_free(p_response);
    }
    s_main_loop = 0;
    return RIL_E_SUCCESS;
}

void requestResetRadio(void * data, size_t datalen, RIL_Token t)
{
    RIL_Errno err = resetRadio();
    RIL_onRequestComplete(t, err, NULL, 0);
}

// TODO: requestOemHookRaw
void requestOemHookRaw(void * data, size_t datalen, RIL_Token t)
{
    /* atci start */
    ATResponse * p_response = NULL;
    ATLine* p_cur = NULL;
    const char* buffer = (char*)data;
    char** line;
    int i;
    int strLength = 0;
    int err = -1;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;

    RLOGD("data = %s, length = %d", buffer, datalen);

    err = at_send_command_raw(buffer, &p_response, OEM_CHANNEL_CTX);

    if (err < 0) {
        RLOGE("OEM_HOOK_RAW fail");
        goto error;
    }

    RLOGD("p_response->success = %d", p_response->success);
    RLOGD("p_response->finalResponse = %s", p_response->finalResponse);

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
        p_cur = p_cur->p_next) {
        RLOGD("p_response->p_intermediates = %s", p_cur->line);
        strLength++;
    }
    strLength++;
    RLOGD("strLength = %d", strLength);

    line = (char **) alloca(strLength * sizeof(char *));
    if (strLength == 1) {
        line[0] =  p_response->finalResponse;
        RLOGD("line[0] = %s", line[0]);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, line, strLength * sizeof(char *));
    } else {
        for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
            p_cur = p_cur->p_next, i++)
        {
            line[i] = p_cur->line;
            RLOGD("line[%d] = %s", i, line[i]);
        }
        line[i] = p_response->finalResponse;
        RLOGD("line[%d] = %s", i, line[i]);

        RIL_onRequestComplete(t, RIL_E_SUCCESS, line, strLength * sizeof(char *));
    }

     at_response_free(p_response);
     return;

error:
    line = (char **) alloca(sizeof(char *));
    line[0] = "ERROR";
    RLOGD("line[0] = %s", line[0]);
    RIL_onRequestComplete(t, ret, line, sizeof(char *));

    at_response_free(p_response);
    /* atci end */
}

void requestOemHookStrings(void * data, size_t datalen, RIL_Token t)
{
    int i;
    const char ** cur;
    ATResponse *    p_response = NULL;
    int             err = -1;
    ATLine*         p_cur = NULL;
    char**          line;
    int             strLength = datalen / sizeof(char *);
    RIL_Errno       ret = RIL_E_GENERIC_FAILURE;
    RIL_SOCKET_ID rid = getRILIdByChannelCtx(getRILChannelCtxFromToken(t));

    RLOGD("got OEM_HOOK_STRINGS: 0x%8p %lu", data, (long)datalen);

    for (i = strLength, cur = (const char **)data ;
         i > 0 ; cur++, i --) {
            RLOGD("> '%s'", *cur);
    }


    if (strLength != 2) {
        /* Non proietary. Loopback! */

        RIL_onRequestComplete(t, RIL_E_SUCCESS, data, datalen);

        return;

    }

    /* For AT command access */
    cur = (const char **)data;

    if (NULL != cur[1] && strlen(cur[1]) != 0) {

        if ((strncmp(cur[1],"+CIMI",5) == 0) ||(strncmp(cur[1],"+CGSN",5) == 0)) {

            err = at_send_command_numeric(cur[0], &p_response, OEM_CHANNEL_CTX);

        } else {

        err = at_send_command_multiline(cur[0],cur[1], &p_response, OEM_CHANNEL_CTX);

        }

    } else {

        err = at_send_command(cur[0],&p_response,OEM_CHANNEL_CTX);

    }

    if (strncmp(cur[0],"AT+EPIN2",8) == 0) {
        SimPinCount retryCounts;
        LOGW("OEM_HOOK_STRINGS: PIN operation detect");
        getPINretryCount(&retryCounts, t, rid);
    }

    if (err < 0 || NULL == p_response) {
            RLOGE("OEM_HOOK_STRINGS fail");
            goto error;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
            break;
        case CME_INCORRECT_PASSWORD:
            ret = RIL_E_PASSWORD_INCORRECT;
            break;
        case CME_SIM_PIN_REQUIRED:
        case CME_SIM_PUK_REQUIRED:
            ret = RIL_E_PASSWORD_INCORRECT;
            break;
        case CME_SIM_PIN2_REQUIRED:
            ret = RIL_E_SIM_PIN2;
            break;
        case CME_SIM_PUK2_REQUIRED:
            ret = RIL_E_SIM_PUK2;
            break;
        case CME_DIAL_STRING_TOO_LONG:
            ret = RIL_E_GENERIC_FAILURE;
            break;
        case CME_TEXT_STRING_TOO_LONG:
            ret = RIL_E_GENERIC_FAILURE;
            break;
        case CME_MEMORY_FULL:
            ret = RIL_E_GENERIC_FAILURE;
            break;
        case CME_BT_SAP_UNDEFINED:
            ret = RIL_E_GENERIC_FAILURE;
            break;
        case CME_BT_SAP_NOT_ACCESSIBLE:
            ret = RIL_E_GENERIC_FAILURE;
            break;
        case CME_BT_SAP_CARD_REMOVED:
            ret = RIL_E_GENERIC_FAILURE;
            break;
        default:
            ret = RIL_E_GENERIC_FAILURE;
            break;
    }

    if (ret != RIL_E_SUCCESS) {
        goto error;
    }

    if (strncmp(cur[0],"AT+ESSP",7) == 0) {
        RLOGI("%s , %s !",cur[0], cur[1]);
        if(strcmp(cur[1], "") == 0) {
            updateCFUQueryType(cur[0]);
        }
    }

    /* Count response length */
    strLength = 0;

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
        p_cur = p_cur->p_next)
        strLength++;

    if (strLength == 0) {

        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

    } else {

        RLOGI("%d of %s received!",strLength, cur[1]);

        line = (char **) alloca(strLength * sizeof(char *));

        for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
            p_cur = p_cur->p_next, i++)
        {
            line[i] = p_cur->line;
        }

        RIL_onRequestComplete(t, RIL_E_SUCCESS, line, strLength * sizeof(char *));
    }

    at_response_free(p_response);

    return;

error:
    RIL_onRequestComplete(t, ret, NULL, 0);

    at_response_free(p_response);

}

void updateCFUQueryType(const char *cmd)
{
    int fd;
    int n;
    struct env_ioctl en_ctl;
    char *name = NULL;
    char *value = NULL;

    do {
        value = calloc(1, BUF_MAX_LEN);
        if(value == NULL) {
            RLOGE("malloc value fail");
            break;
        }
        value[0] = *(cmd+8);

        property_set(SETTING_QUERY_CFU_TYPE, value);

        name = calloc(1, BUF_MAX_LEN);
        if(name == NULL) {
            RLOGE("malloc name fail");
            free(value);
            break;
        }

        memset(&en_ctl,0x00,sizeof(struct env_ioctl));

        fd= open("/proc/lk_env",O_RDWR);

        if(fd<= 0) {
            RLOGE("ERROR open fail %d\n", fd);
            free(name);
            free(value);
            break;
        }

        strncpy(name,SETTING_QUERY_CFU_TYPE, strlen(SETTING_QUERY_CFU_TYPE));

        en_ctl.name = name;
        en_ctl.value = value;
        en_ctl.name_len = strlen(name)+1;

        en_ctl.value_len = strlen(value)+1;
        LOGD("write %s = %s\n",name,value);
        n=ioctl(fd,ENV_WRITE,&en_ctl);
        if(n<0) {
            printf("ERROR write fail %d\n",n);
        }
        free(name);
        free(value);
        close(fd);
    }while(0);
}

void initCFUQueryType()
{
    int fd;
    int n;
    struct env_ioctl en_ctl;
    char *name = NULL;
    char *value = NULL;

    do {
        memset(&en_ctl,0x00,sizeof(struct env_ioctl));

        fd= open("/proc/lk_env",O_RDONLY);

        if(fd<= 0) {
            RLOGE("ERROR open fail %d\n", fd);
            break;
        }

        name = calloc(1, BUF_MAX_LEN);
        if(name == NULL) {
            RLOGE("malloc name fail");
            close(fd);
            break;
        }
        value = calloc(1, BUF_MAX_LEN);
        if(value == NULL) {
            RLOGE("malloc value fail");
            free(name);
            close(fd);
            break;
        }
        memcpy(name,SETTING_QUERY_CFU_TYPE, strlen(SETTING_QUERY_CFU_TYPE));

        en_ctl.name = name;
        en_ctl.value = value;
        en_ctl.name_len = strlen(name)+1;
        en_ctl.value_len = BUF_MAX_LEN;

        RLOGD("read %s \n",name);

        n=ioctl(fd,ENV_READ,&en_ctl);
        if(n<0){
            RLOGE("ERROR read fail %d\n",n);
        }
        else {
            property_set(name, value);
        }

        free(name);
        free(value);
        close(fd);
    }while(0);
}

void updateSignalStrength(RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err;
    //MTK-START [ALPS00506562][ALPS00516994]
    //int response[12]={0};
    int response[17]={0};
    //MTK-START [ALPS00506562][ALPS00516994]

    char *line;

    memset(response, 0, sizeof(response));

    err = at_send_command_singleline("AT+ECSQ", "+ECSQ:", &p_response, OEM_CHANNEL_CTX);

    if (err < 0 || p_response->success == 0 ||
            p_response->p_intermediates  == NULL) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    //err = getSingnalStrength(line, &response[0], &response[1], &response[2], &response[3], &response[4]);
    err = getSingnalStrength(line, response);

    if (err < 0) goto error;

    if (99 == response[0])
        LOGD("Ignore rssi 99(unknown)");
    else
        RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
                response,
                sizeof(response),
                getRILIdByChannelCtx(getRILChannelCtxFromToken(t)));

    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    RLOGE("updateSignalStrength ERROR: %d", err);
}

void requestScreenState(void * data, size_t datalen, RIL_Token t)
{
    /************************************
    * Disable the URC: ECSQ,CREG,CGREG,CTZV
    * For the URC +CREG and +CGREG
    * we will buffer the URCs when the screen is off
    * and issues URCs when the screen is on
    * So we can reflect the ture status when screen is on
    *************************************/

    int on_off, err;
    ATResponse *p_response = NULL;

    on_off = ((int*)data)[0];

    if (on_off)
    {
        // screen is on

         /* Disable Network registration events of the changes in LAC or CID */
        err = at_send_command("AT+CREG=3", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0){
            err = at_send_command("AT+CREG=2", &p_response, OEM_CHANNEL_CTX);
            if (err != 0 || p_response->success == 0)
                LOGW("There is something wrong with the exectution of AT+CREG=2");
        }
        at_response_free(p_response);

        err = at_send_command("AT+CGREG=2", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+CGREG=2");
        at_response_free(p_response);

        /* Enable get ECSQ URC */
        at_send_command("AT+ECSQ=1", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+ECSQ=2");
        at_response_free(p_response);
        updateSignalStrength(t);

        /* Enable PSBEARER URC */
        err = at_send_command("AT+PSBEARER=1", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+PSBEARER=1");
        at_response_free(p_response);

        err = at_send_command("AT+CEREG=2", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+CEREG=2");
        at_response_free(p_response);

        /* Enable ECSG URC */
    #ifdef MTK_FEMTO_CELL_SUPPORT
        at_send_command("AT+ECSG=4,1", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+ECSG=4,1");
        at_response_free(p_response);
    #endif
    }
    else
    {
        // screen is off

        /* Disable Network registration events of the changes in LAC or CID */
        err = at_send_command("AT+CREG=1", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+CREG=1");
        at_response_free(p_response);

        err = at_send_command("AT+CGREG=1", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+CGREG=1");
        at_response_free(p_response);

        /* Disable get ECSQ URC */
        at_send_command("AT+ECSQ=0", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+ECSQ=0");
        at_response_free(p_response);

        /* Disable PSBEARER URC */
        err = at_send_command("AT+PSBEARER=0", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+PSBEARER=0");
        at_response_free(p_response);

        err = at_send_command("AT+CEREG=1", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+CEREG=1");
        at_response_free(p_response);

    #ifdef MTK_FEMTO_CELL_SUPPORT
        /* Disable ECSG URC */
        at_send_command("AT+ECSG=4,0", &p_response, OEM_CHANNEL_CTX);
        if (err != 0 || p_response->success == 0)
            LOGW("There is something wrong with the exectution of AT+ECSG=4,0");
        at_response_free(p_response);
    #endif
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

extern int queryMainProtocol()
{
    ATResponse *p_response = NULL;
    int err;
    int response;
    char *line;

#ifndef MTK_GEMINI
        /* Only Gemini modem support AT+ES3G */
    return CAPABILITY_3G_SIM1;
#else
    err = at_send_command_singleline("AT+ES3G?", "+ES3G:", &p_response, getRILChannelCtx(RIL_SIM,getMainProtocolRid()));

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response);
    if (err < 0) goto error;

    /* Gemini+ , +ES3G response 1: SIM1 , 2: SIM2 , 4:SIM3 ,8: SIM4. For SIM3 and SIM4 we convert to 3 and 4 */
    if(response == 4){
        response = CAPABILITY_3G_SIM3;
    }else if(response == 8){
        response = CAPABILITY_3G_SIM4;
    }

    at_response_free(p_response);

    return response;

error:
    at_response_free(p_response);
    return CAPABILITY_3G_SIM1;
#endif // MTK_GEMINI
}

// TBD, remove
extern void set3GCapability(RIL_Token t, int setting)
{
    char * cmd;
    ATResponse *p_response = NULL;
    int err = 0;
    asprintf(&cmd, "AT+ES3G=%d, %d", setting, NETWORK_MODE_WCDMA_PREF);
    if (t) {
        err = at_send_command(cmd, &p_response, OEM_CHANNEL_CTX);
    } else {
        RLOGI("The ril token is null, use URC instead");
        err = at_send_command(cmd, &p_response, getChannelCtxbyProxy(getMainProtocolRid()));
    }

    free(cmd);
    if (err < 0 || p_response->success == 0) {
        RLOGI("Set 3G capability to [%d] failed", setting);
    } else {
        RLOGI("Set 3G capability to [%d] successfully", setting);
    }
    at_response_free(p_response);
}

void initRadioCapabilityResponse(RIL_RadioCapability* rc, RIL_RadioCapability* copyFromRC) {
    memset(rc, 0, sizeof(RIL_RadioCapability));
    rc->version = RIL_RADIO_CAPABILITY_VERSION;
    rc->session = copyFromRC->session;
    rc->phase = copyFromRC->phase;
    rc->rat = copyFromRC->rat;
    strcpy(rc->logicalModemUuid, copyFromRC->logicalModemUuid);
    rc->status = copyFromRC->status;
}

extern void requestGetRadioCapability(void * data, size_t datalen, RIL_Token t)
{
    RIL_SOCKET_ID rid = getRILIdByChannelCtx(getRILChannelCtxFromToken(t));
    int sim3G = RIL_get3GSIM();
    RLOGI("3G Sim : %d, current RID : %d", sim3G, rid);

    RIL_RadioCapability* rc = (RIL_RadioCapability*) calloc(1, sizeof(RIL_RadioCapability));
    rc->version = RIL_RADIO_CAPABILITY_VERSION;
    rc->session = 0;
    rc->phase = RC_PHASE_CONFIGURED;
    rc->rat = RAF_GSM;
    rc->status = RC_STATUS_NONE;
    RLOGI("requestGetRadioCapability : %d, %d, %d, %d, %s, %d, rild:%d",
            rc->version, rc->session, rc->phase, rc->rat, rc->logicalModemUuid, rc->status, rid);

#ifndef MTK_GEMINI;
    /* Only Gemini modem support AT+ES3G */
    if (isLteSupport()) {
        rc->rat = RAF_GSM | RAF_UMTS | RAF_LTE;
    } else {
        rc->rat = RAF_GSM | RAF_UMTS;
    }
    strcpy(rc->logicalModemUuid, s_logicalModemId[rid]);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, rc, sizeof(RIL_RadioCapability));
#else
    if (rid == (sim3G-1)) {
        if (isLteSupport()) {
            rc->rat = RAF_GSM | RAF_UMTS | RAF_LTE;
        } else {
            rc->rat = RAF_GSM | RAF_UMTS;
        }
        strcpy(rc->logicalModemUuid, s_logicalModemId[0]);
    } else {
        rc->rat = RAF_GSM;
        if (rid == 0) {
            strcpy(rc->logicalModemUuid, s_logicalModemId[(sim3G-1)]);
        } else {
            strcpy(rc->logicalModemUuid, s_logicalModemId[rid]);
        }
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, rc, sizeof(RIL_RadioCapability));
#endif // MTK_GEMINI
}

extern ApplyRadioCapabilityResult applyRadioCapability(RIL_RadioCapability* rc, RIL_Token t)
{
    char * cmd;
    ATResponse *p_response = NULL;
    int err;
    RIL_SOCKET_ID rid = getRILIdByChannelCtx(getRILChannelCtxFromToken(t));
    int old3GSim = queryMainProtocol();
    RLOGI("applyRadioCapability : %d, %d, %d", rc->rat, old3GSim, rid);

    if (rc->rat == RAF_GSM) {
        // only send ES3G in 2G->3G rid
        RLOGI("no need to do sim switch");
        return ApplyRC_NONEED;
    }

    /* Gemini+ , +ES3G response 1: SIM1 , 2: SIM2 , 4:SIM3 ,8: SIM4.  */
    int modem_setting_value = CAPABILITY_3G_SIM1 << (rid);
    if (isLteSupport()) {
        asprintf(&cmd, "AT+ES3G=%d, %d", modem_setting_value, NETWORK_MODE_GSM_UMTS_LTE);
    } else {
        asprintf(&cmd, "AT+ES3G=%d, %d", modem_setting_value, NETWORK_MODE_WCDMA_PREF);
    }

    err = at_send_command(cmd, &p_response, getRILChannelCtx(RIL_SIM,getMainProtocolRid()));
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        at_response_free(p_response);
        return APPLYRC_FAIL;
    }
    at_response_free(p_response);
    return ApplyRC_SUCCESS;
}

extern void requestSetRadioCapability(void * data, size_t datalen, RIL_Token t)
{
    char * cmd;
    ATResponse *p_response = NULL;
    ApplyRadioCapabilityResult applyRcResult = ApplyRC_NONEED;
    RIL_SOCKET_ID rid = getRILIdByChannelCtx(getRILChannelCtxFromToken(t));
    char sRcSessionId[32] = {0};
    RIL_RadioCapability rc;

    memcpy(&rc, data, sizeof(RIL_RadioCapability));
    RLOGI("requestSetRadioCapability : %d, %d, %d, %d, %s, %d, rild:%d",
        rc.version, rc.session, rc.phase, rc.rat, rc.logicalModemUuid, rc.status, rid);

    memset(sRcSessionId, 0, sizeof(sRcSessionId));
    sprintf(sRcSessionId,"%d",rc.session);


    RIL_RadioCapability* responseRc = (RIL_RadioCapability*) malloc(sizeof(RIL_RadioCapability));
    initRadioCapabilityResponse(responseRc, &rc);
    int sim3G = RIL_get3GSIM();
#ifndef MTK_GEMINI;
    strcpy(responseRc->logicalModemUuid, s_logicalModemId[rid]);
#else
    if (rid == (sim3G-1)) {
        strcpy(responseRc->logicalModemUuid, s_logicalModemId[0]);
    } else {
        if (rid == 0) {
            strcpy(responseRc->logicalModemUuid, s_logicalModemId[(sim3G-1)]);
        } else {
            strcpy(responseRc->logicalModemUuid, s_logicalModemId[rid]);
        }
    }
#endif // MTK_GEMINI
    switch (rc.phase) {
        case RC_PHASE_START:
            RLOGI("requestSetRadioCapability RC_PHASE_START");
            //property_set(PROPERTY_SET_RC_SESSION_ID[0], sRcSessionId);
            // keep session id with system property
            // after modem reset, send session id back to framework with urc
            property_set(PROPERTY_SET_RC_SESSION_ID[0], sRcSessionId);
            responseRc->status = RC_STATUS_SUCCESS;
            RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc, sizeof(RIL_RadioCapability));
            break;
        case RC_PHASE_FINISH:
            RLOGI("requestSetRadioCapability RC_PHASE_FINISH");

            // transaction of sim switch is done, reset system property
            property_set(PROPERTY_SET_RC_SESSION_ID[0], "0");
            responseRc->phase = RC_PHASE_CONFIGURED;
            responseRc->status = RC_STATUS_SUCCESS;
            RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc, sizeof(RIL_RadioCapability));
            break;
        case RC_PHASE_APPLY:
            responseRc->status = RC_STATUS_SUCCESS;
            applyRcResult = applyRadioCapability(responseRc, t);

            // send request back to socket before reset radio,
            // or the response may be lost due to modem reset

            RLOGI("requestSetRadioCapability applyRcResult:%d, s_md_off:%d",applyRcResult, s_md_off);
            switch (applyRcResult) {
                case ApplyRC_SUCCESS:
                case ApplyRC_NONEED:
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc, sizeof(RIL_RadioCapability));
                    break;
                case APPLYRC_FAIL:
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, responseRc, sizeof(RIL_RadioCapability));
                    break;
            }

            if (applyRcResult == ApplyRC_SUCCESS) {
                RLOGI("requestSetRadioCapability resetRadio");
                resetRadio();
            }
            break;
        default:
            RLOGI("requestSetRadioCapability default");
            responseRc->status = RC_STATUS_FAIL;
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
    }
    free(responseRc);
}

extern void flightModeBoot()
{
    ATResponse *p_response = NULL;
    RLOGI("Start flight modem boot up");
#ifdef MTK_GEMINI
    int err = at_send_command("AT+EFUN=0", &p_response, getRILChannelCtx(RIL_SIM,getMainProtocolRid()));
#else
    int err = at_send_command("AT+CFUN=4", &p_response, getRILChannelCtx(RIL_SIM,getMainProtocolRid()));
#endif
    if (err != 0 || p_response->success == 0)
        LOGW("Start flight modem boot up failed");
    at_response_free(p_response);

    int telephonyMode = getTelephonyMode();
    switch (telephonyMode) {
        case TELEPHONY_MODE_6_TGNG_DUALTALK:
#ifdef MTK_RIL_MD1
            err = at_send_command("AT+ERAT=0", &p_response, getChannelCtxbyProxy(RIL_SOCKET_1));
            if (err != 0 || p_response->success == 0)
                LOGW("Set default RAT mode failed");
            at_response_free(p_response);
#endif
            break;
        case TELEPHONY_MODE_7_WGNG_DUALTALK:
#ifdef MTK_RIL_MD2
            err = at_send_command("AT+ERAT=0", &p_response, getChannelCtxbyProxy(RIL_SOCKET_1));
            if (err != 0 || p_response->success == 0)
                LOGW("Set default RAT mode failed");
            at_response_free(p_response);
#endif
            break;
        case TELEPHONY_MODE_8_GNG_DUALTALK:
            err = at_send_command("AT+ERAT=0", &p_response, getChannelCtxbyProxy(RIL_SOCKET_1));
            if (err != 0 || p_response->success == 0)
                LOGW("Set default RAT mode failed");
            at_response_free(p_response);
            break;
    }
}

extern int getMappingSIMByCurrentMode(RIL_SOCKET_ID rid) {
    if (isDualTalkMode()) {
        int firstMD = getFirstModem();
        if (firstMD == FIRST_MODEM_NOT_SPECIFIED) {
            //no first MD concept, use external MD mechanism
            int telephonyMode = getTelephonyMode();
            switch (telephonyMode) {
#ifdef MTK_GEMINI
                case TELEPHONY_MODE_100_TDNC_DUALTALK:
                case TELEPHONY_MODE_101_FDNC_DUALTALK:
                    if (rid == RIL_SOCKET_2) {
                        //for international roaming, the socket2 is connected with framework
                        if (getExternalModemSlot() == GEMINI_SIM_1)
                            return GEMINI_SIM_2;
                        else
                            return GEMINI_SIM_1;
                    } else {
                        LOGW("Get mapping SIM but no match case[a]");
                    }
                    break;
#endif
                default:
                    //TELEPHONY_MODE_0_NONE, TELEPHONY_MODE_102_WNC_DUALTALK, TELEPHONY_MODE_103_TNC_DUALTALK
                    if (rid == RIL_SOCKET_1) {
                        if (getExternalModemSlot() == GEMINI_SIM_1)
                            return GEMINI_SIM_2;
                        else
                            return GEMINI_SIM_1;
                    } else {
                        LOGW("Get mapping SIM but no match case[b]");
                    }
                    break;
            }
        } else if (firstMD == FIRST_MODEM_MD1) {
            #ifdef MTK_RIL_MD1
                return GEMINI_SIM_1;
            #else
                return GEMINI_SIM_2;
            #endif
        } else {
            #ifdef MTK_RIL_MD1
                return GEMINI_SIM_2;
            #else
                return GEMINI_SIM_1;
            #endif
        }
    } else {
        return GEMINI_SIM_1+rid;
    }
    return -1;
}

extern void upadteSystemPropertyByCurrentMode(int rid, char* key1, char* key2, char* value) {
    if (isDualTalkMode()) {
        int firstMD = getFirstModem();

        if (firstMD == FIRST_MODEM_NOT_SPECIFIED) {
            //no first MD concept, use external MD mechanism
            int telephonyMode = getTelephonyMode();
            switch (telephonyMode) {
#ifdef MTK_GEMINI
                case TELEPHONY_MODE_100_TDNC_DUALTALK:
                case TELEPHONY_MODE_101_FDNC_DUALTALK:
                    if (rid == RIL_SOCKET_2) {
                        //for international roaming, the socket2 is connected with framework
                        if (getExternalModemSlot() == GEMINI_SIM_1) {
                            RLOGI("Update property SIM2 (exMD) [%s, %s]", key2, value != NULL ? value : "");
                            property_set(key2, value);
                        } else {
                            RLOGI("Update property SIM1 (exMD)[%s, %s]", key1, value != NULL ? value : "");
                            property_set(key1, value);
                        }
                    } else {
                        LOGW("Update property but no match case[a]");
                    }
                    break;
#endif
                default:
                    //TELEPHONY_MODE_0_NONE, TELEPHONY_MODE_102_WNC_DUALTALK, TELEPHONY_MODE_103_TNC_DUALTALK
                    if (rid == RIL_SOCKET_1) {
                        if (getExternalModemSlot() == GEMINI_SIM_1) {
                            RLOGI("Update property SIM2 (exMD) [%s, %s]", key2, value != NULL ? value : "");
                            property_set(key2, value);
                        } else {
                            RLOGI("Update property SIM1 (exMD)[%s, %s]", key1, value != NULL ? value : "");
                            property_set(key1, value);
                        }
                    } else {
                        LOGW("Update property but no match case[b]");
                    }
                    break;
            }
        } else if (firstMD == FIRST_MODEM_MD1) {
            #ifdef MTK_RIL_MD1
                RLOGI("Update property SIM1 (dt)[%s, %s]", key1, value != NULL ? value : "");
                property_set(key1, value);
            #else
                RLOGI("Update property SIM2 (dt) [%s, %s]", key2, value != NULL ? value : "");
                property_set(key2, value);
            #endif
        } else {
            #ifdef MTK_RIL_MD1
                RLOGI("Update property SIM2 (dt switched) [%s, %s]", key2, value != NULL ? value : "");
                property_set(key2, value);
            #else
                RLOGI("Update property SIM1 (dt switched) [%s, %s]", key1, value != NULL ? value : "");
                property_set(key1, value);
            #endif
        }
    } else {
        if (rid == RIL_SOCKET_1) {
            RLOGI("Update property SIM1 [%s, %s]", key1, value != NULL ? value : "");
            property_set(key1, value);
        } else {
            RLOGI("Update property SIM2 [%s, %s]", key2, value != NULL ? value : "");
            property_set(key2, value);
        }
    }
}

extern void upadteSystemPropertyByCurrentModeGemini(int rid, char* key1, char* key2, char* key3, char* key4, char* value) {
    int pivot = 1;
    int pivotSim;;

    if (isDualTalkMode()) {
        int firstMD = getFirstModem();

        if (firstMD == FIRST_MODEM_NOT_SPECIFIED) {
            //no first MD concept, use external MD mechanism
            int telephonyMode = getTelephonyMode();
            switch (telephonyMode) {
#ifdef MTK_GEMINI
                case TELEPHONY_MODE_100_TDNC_DUALTALK:
                case TELEPHONY_MODE_101_FDNC_DUALTALK:
                    if (rid == RIL_SOCKET_2) {
                        //for international roaming, the socket2 is connected with framework
                        if (getExternalModemSlot() == GEMINI_SIM_1) {
                            RLOGI("Update property SIM2 (exMD) [%s, %s]", key2, value != NULL ? value : "");
                            property_set(key2, value);
                        } else {
                            RLOGI("Update property SIM1 (exMD)[%s, %s]", key1, value != NULL ? value : "");
                            property_set(key1, value);
                        }
                    } else {
                        LOGW("Update property but no match case[a]");
                    }
                    break;
#endif
                default:
                    //TELEPHONY_MODE_0_NONE, TELEPHONY_MODE_102_WNC_DUALTALK, TELEPHONY_MODE_103_TNC_DUALTALK
                    if (rid == RIL_SOCKET_1) {
                        if (getExternalModemSlot() == GEMINI_SIM_1) {
                            RLOGI("Update property SIM2 (exMD) [%s, %s]", key2, value != NULL ? value : "");
                            property_set(key2, value);
                        } else {
                            RLOGI("Update property SIM1 (exMD)[%s, %s]", key1, value != NULL ? value : "");
                            property_set(key1, value);
                        }
                    } else {
                        LOGW("Update property but no match case[b]");
                    }
                    break;
            }
        } else if (firstMD == FIRST_MODEM_MD1) {
            #ifdef MTK_RIL_MD1
                RLOGI("Update property SIM1 (dt)[%s, %s]", key1, value != NULL ? value : "");
                property_set(key1, value);
            #else
                RLOGI("Update property SIM2 (dt) [%s, %s]", key2, value != NULL ? value : "");
                property_set(key2, value);
            #endif
        } else {
            #ifdef MTK_RIL_MD1
                RLOGI("Update property SIM2 (dt switched) [%s, %s]", key2, value != NULL ? value : "");
                property_set(key2, value);
            #else
                RLOGI("Update property SIM1 (dt switched) [%s, %s]", key1, value != NULL ? value : "");
                property_set(key1, value);
            #endif
        }
    } else {
        pivotSim = pivot << rid;
        RLOGI("Update property SIM%d [%s]", pivotSim, value != NULL ? value : "");
        switch(pivotSim) {
            case 1:
                property_set(key1, value);
                break;
            case 2:
                property_set(key2, value);
                break;
            case 4:
                property_set(key3, value);
                break;
            case 8:
                property_set(key4, value);
                break;
            default:
                RLOGE("Update property SIM%d it is unexpected", pivotSim);
                break;
        }
    }
}

extern void bootupGetIccid(RIL_SOCKET_ID rid) {
    int result = 0;
    ATResponse *p_response = NULL;
    int err = at_send_command_singleline("AT+ICCID?", "+ICCID:", &p_response, getDefaultChannelCtx(rid));

    if (err >= 0 && p_response != NULL) {
        if (at_get_cme_error(p_response) == CME_SUCCESS) {
            char *line;
            char *iccId;
            line = p_response->p_intermediates->line;
            err = at_tok_start (&line);
            if (err >= 0) {
                err = at_tok_nextstr(&line, &iccId);
                if (err >= 0) {
                    RLOGD("bootupGetIccid[%d] iccid is %s", rid, iccId);
                    upadteSystemPropertyByCurrentModeGemini(rid, PROPERTY_ICCID_SIM[0], PROPERTY_ICCID_SIM[1], PROPERTY_ICCID_SIM[2], PROPERTY_ICCID_SIM[3], iccId);
                    result = 1;
                } else
                    RLOGD("bootupGetIccid[%d]: get iccid error 2", rid);
            } else {
                RLOGD("bootupGetIccid[%d]: get iccid error 1", rid);
            }
        } else {
            RLOGD("bootupGetIccid[%d]: Error or no SIM inserted!", rid);
        }
    } else {
        RLOGE("bootupGetIccid[%d] Fail", rid);
    }
    at_response_free(p_response);

    if (!result) {
        RLOGE("bootupGetIccid[%d] fail and write default string", rid);
        upadteSystemPropertyByCurrentModeGemini(rid, PROPERTY_ICCID_SIM[0], PROPERTY_ICCID_SIM[1], PROPERTY_ICCID_SIM[2], PROPERTY_ICCID_SIM[3], "N/A");
    }
}

extern void bootupGetImei(RIL_SOCKET_ID rid) {
    RLOGE("bootupGetImei[%d]", rid);
    ATResponse *p_response = NULL;
    int err = at_send_command_numeric("AT+CGSN", &p_response, getDefaultChannelCtx(rid));

    if (err >= 0 && p_response->success != 0) {
        err = asprintf(&s_imei[rid], "%s", p_response->p_intermediates->line);
        if(err < 0)
            RLOGE("bootupGetImei[%d] set fail", rid);
    } else {
        RLOGE("bootupGetImei[%d] Fail", rid);
    }
    at_response_free(p_response);
}

extern void bootupGetImeisv(RIL_SOCKET_ID rid) {
    RLOGE("bootupGetImeisv[%d]", rid);
    ATResponse *p_response = NULL;
    int err = at_send_command_singleline("AT+EGMR=0,9", "+EGMR:",&p_response, getDefaultChannelCtx(rid));

    if (err >= 0 && p_response->success != 0) {
        char* sv = NULL;
        char* line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if(err >= 0) {
            err = at_tok_nextstr(&line, &sv);
            if(err >= 0) {
                err = asprintf(&s_imeisv[rid], "%s", sv);
                if(err < 0)
                    RLOGE("bootupGetImeisv[%d] set fail", rid);
            } else {
                RLOGE("bootupGetImeisv[%d] get token fail", rid);
            }
        } else {
            RLOGE("bootupGetImeisv[%d] AT CMD fail", rid);
        }
    } else {
        RLOGE("bootupGetImeisv[%d] Fail", rid);
    }
    at_response_free(p_response);
}

void bootupGetProjectFlavor(RIL_SOCKET_ID rid){
    RLOGE("bootupGetProjectAndMdInfo[%d]", rid);
    ATResponse *p_response = NULL;
    ATResponse *p_response2 = NULL;
    char* line = NULL;
    char* line2 = NULL;
    char* projectName= NULL;
    char* flavor= NULL;
    int err;

    // Add for MD-team query project/flavor properties : project name(flavor)
    err = at_send_command_singleline("AT+EGMR=0,4", "+EGMR:",&p_response, getDefaultChannelCtx(rid));
    if (err >= 0 && p_response->success != 0) {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if(err >= 0) {
            err = at_tok_nextstr(&line, &projectName);
            if(err < 0)
                RLOGE("bootupGetProjectName[%d] get token fail", rid);
        } else {
            RLOGE("bootupGetProjectName[%d] AT CMD fail", rid);
        }
    } else {
        RLOGE("bootupGetProjectName[%d] Fail", rid);
    }

    //query project property (flavor)
    err = at_send_command_singleline("AT+EGMR=0,13", "+EGMR:",&p_response2, getDefaultChannelCtx(rid));
    if (err >= 0 && p_response2->success != 0) {
        line2 = p_response2->p_intermediates->line;
        err = at_tok_start(&line2);
        if(err >= 0) {
            err = at_tok_nextstr(&line2, &flavor);
            if(err < 0)
                RLOGE("bootupGetFlavor[%d] get token fail", rid);
        } else {
            RLOGE("bootupGetFlavor[%d] AT CMD fail", rid);
        }
    } else {
        RLOGE("bootupGetFlavor[%d] Fail", rid);
    }

    //combine string: projectName(flavor)
    err = asprintf(&s_projectFlavor[rid], "%s(%s)",projectName ,flavor);
    if(err < 0) RLOGE("bootupGetProject[%d] set fail", rid);

    if (getMappingSIMByCurrentMode(rid) == GEMINI_SIM_2){
        err = property_set("gsm.project.baseband.2",s_projectFlavor[rid]);
        if(err < 0) RLOGE("SystemProperty: PROPERTY_PROJECT_2 set fail");
    }else{
        err = property_set("gsm.project.baseband" ,s_projectFlavor[rid]);
        if(err < 0) RLOGE("SystemProperty: PROPERTY_PROJECT set fail");
    }

    at_response_free(p_response);
    at_response_free(p_response2);

}

extern void bootupGetBasebandVersion(RIL_SOCKET_ID rid) {
    RLOGE("bootupGetBasebandVersion[%d]", rid);
    ATResponse *p_response = NULL;
    int err, i, len;
    char *line, *ver, null;

    ver = &null;
    ver[0] = '\0';

    //Add for MD-team query project/flavor properties : project name(flavor)
    bootupGetProjectFlavor(rid);

    err = at_send_command_multiline("AT+CGMR", "+CGMR:",&p_response, getDefaultChannelCtx(rid));

    if (err < 0 || p_response->success == 0)
    {
        goto error;
    }
    else if (p_response->p_intermediates != NULL)
    {
        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if(err < 0) goto error;

        //remove the white space from the end
        len = strlen(line);
        while( len > 0 && isspace(line[len-1]) )
            len --;
        line[len] = '\0';

        //remove the white space from the beginning
        while( (*line) != '\0' &&  isspace(*line) )
            line++;

        ver = line;
    }
    else
    {
        // ALPS00295957 : To handle AT+CGMR without +CGMR prefix response
        at_response_free(p_response);
        p_response = NULL;

        RLOGE("Retry AT+CGMR without expecting +CGMR prefix");

        err = at_send_command_raw("AT+CGMR", &p_response, getDefaultChannelCtx(rid));

        if (err < 0) {
            RLOGE("Retry AT+CGMR ,fail");
            goto error;
        }

        if(p_response->p_intermediates != NULL)
        {
            line = p_response->p_intermediates->line;

            RLOGD("retry CGMR response = %s", line);

            //remove the white space from the end
            len = strlen(line);
            while( len > 0 && isspace(line[len-1]) )
                len --;
            line[len] = '\0';

            //remove the white space from the beginning
            while( (*line) != '\0' &&  isspace(*line) )
                line++;

            ver = line;
        }
    }
    asprintf(&s_basebandVersion[rid], "%s", ver);
    at_response_free(p_response);
    return;
error:
    at_response_free(p_response);
}

/* ALPS00582073 START */
extern void bootupGetCalData(RIL_SOCKET_ID rid)
{
    ATResponse *p_response = NULL;
    int err;

    err = at_send_command_singleline("AT+ECAL?", "+ECAL:", &p_response, getDefaultChannelCtx(rid));

    if (err < 0 || p_response->success == 0){
        RLOGE("bootupGetCalData fail,err=%d", err);
    }
    else{
        err = asprintf(&s_calData, "%s", p_response->p_intermediates->line);
        if(err < 0)
            RLOGE("bootupGetCalData set fail,err=%d", err);
        else
            RLOGD("bootupGetCalData s_calData =%s", s_calData);
    }
    at_response_free(p_response);
}
/* ALPS00582073 END */

extern void requestStoreModem(void *data, size_t datalen, RIL_Token t)
{
    int modemType = ((int *)data)[0];
    triggerCCCIIoctlEx(CCCI_IOC_STORE_MD_TYPE, &modemType);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    RLOGD("requestStoreModem complete");
    return;
}

extern void requestQueryModem(void *data, size_t datalen, RIL_Token t)
{
    int response = 0;
    triggerCCCIIoctlEx(CCCI_IOC_GET_MD_TYPE, &response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    RLOGD("requestQueryModem complete, response=%d", response);
    return;
}

void requestGetActivityInfo(void *data, size_t datalen, RIL_Token t){
    ATResponse *p_response = NULL;
    int err;
    char *line;
    RIL_ActivityStatsInfo activityStatsInfo; // RIL_NUM_TX_POWER_LEVELS 5
    int num_tx_levels = 0;

    memset(&activityStatsInfo, 0, sizeof(activityStatsInfo));

    // set sleep/idle mode time
    // TODO: need impl. after MD confirm solution.

    // set Tx/Rx power level
    err = at_send_command_singleline("AT+ERFTX=9", "+ERFTX:", &p_response, OEM_CHANNEL_CTX);
    if (err < 0 || p_response->success == 0) goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;
    err = at_tok_nextint(&line, &num_tx_levels);
    if (err < 0) goto error;
    if (num_tx_levels > RIL_NUM_TX_POWER_LEVELS) {
        LOGE("requestGetActivityInfo TX level invalid (%d)", num_tx_levels);
        goto error;
    }
    for (int i = 0; i < num_tx_levels; i++) {
        err = at_tok_nextint(&line, &(activityStatsInfo.tx_mode_time_ms[i]));
        if (err < 0) goto error;
    }
    err = at_tok_nextint(&line, &(activityStatsInfo.rx_mode_time_ms));
    if (err < 0) goto error;
    err = at_tok_nextint(&line, &(activityStatsInfo.sleep_mode_time_ms));
    if (err < 0) goto error;
    err = at_tok_nextint(&line, &(activityStatsInfo.idle_mode_time_ms));
    if (err < 0) goto error;

    LOGD("requestGetActivityInfo Tx/Rx (%d, %d, %d, %d, %d, %d, %d, %d)", num_tx_levels,
            activityStatsInfo.tx_mode_time_ms[0], activityStatsInfo.tx_mode_time_ms[1],
            activityStatsInfo.tx_mode_time_ms[2], activityStatsInfo.tx_mode_time_ms[3],
            activityStatsInfo.tx_mode_time_ms[4], activityStatsInfo.rx_mode_time_ms,
            activityStatsInfo.sleep_mode_time_ms, activityStatsInfo.idle_mode_time_ms);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &activityStatsInfo, sizeof(activityStatsInfo));

    at_response_free(p_response);
    return;

error:
    LOGE("requestGetActivityInfo error");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

extern int triggerCCCIIoctlEx(int request, int *param)
{
    int ret_ioctl_val = -1;
    int ccci_sys_fd = -1;
    char dev_node[32] = {0};
    int enableMd1 = 0, enableMd2 = 0, enableMd5 = 0;
    char prop_value[PROPERTY_VALUE_MAX] = { 0 };

    property_get("ro.boot.opt_md1_support", prop_value, "0");
    enableMd1 = atoi(prop_value);
    property_get("ro.boot.opt_md2_support", prop_value, "0");
    enableMd2 = atoi(prop_value);
    property_get("ro.boot.opt_md5_support", prop_value, "0");
    enableMd5 = atoi(prop_value);

#if defined(PURE_AP_USE_EXTERNAL_MODEM)
    LOGD("Open CCCI MD1 ioctl port[%s]",CCCI_MD1_POWER_IOCTL_PORT);
    ccci_sys_fd = open(CCCI_MD1_POWER_IOCTL_PORT, O_RDWR);
#else
#ifdef MTK_RIL_MD2
    if (enableMd5) {
        snprintf(dev_node, 32, "%s", ccci_get_node_name(USR_RILD_IOCTL, MD_SYS5));
        LOGD("MD2/SYS5 IOCTL [%s, %d]", dev_node, request);
    } else {
        snprintf(dev_node, 32, "%s", ccci_get_node_name(USR_RILD_IOCTL, MD_SYS2));
        LOGD("MD2 IOCTL [%s, %d]", dev_node, request);
    }
#else /* MTK_RIL_MD2 */
    if (enableMd1) {
        snprintf(dev_node, 32, "%s", ccci_get_node_name(USR_RILD_IOCTL, MD_SYS1));
        LOGD("MD1/SYS1 IOCTL [%s, %d]", dev_node, request);
    } else if(enableMd2) {
        snprintf(dev_node, 32, "%s", ccci_get_node_name(USR_MUXR_IOCTL, MD_SYS2));
        LOGD("MD2/SYS2 IOCTL [%s, %d]", dev_node, request);
    } else {
        snprintf(dev_node, 32, "%s", ccci_get_node_name(USR_RILD_IOCTL, MD_SYS5));
        LOGD("MD1/SYS5 IOCTL [%s, %d]", dev_node, request);
    }
#endif
    ccci_sys_fd = open(dev_node, O_RDWR | O_NONBLOCK);
#endif

    if (ccci_sys_fd < 0) {
        RLOGD("Open CCCI ioctl port failed [%d]", ccci_sys_fd);
        return -1;
    }

#if defined(PURE_AP_USE_EXTERNAL_MODEM)
    if(request == CCCI_IOC_ENTER_DEEP_FLIGHT) {
        int pid = findPid("gsm0710muxd");
        RLOGD("MUXD pid=%d",pid);
        if(pid != -1) kill(pid,SIGUSR2);
        RLOGD("send SIGUSR2 to MUXD done");
        sleepMsec(100);    // make sure MUXD have enough time to close channel and FD
    }
#endif

    ret_ioctl_val = ioctl(ccci_sys_fd, request, param);
    LOGD("CCCI ioctl result: ret_val=%d, request=%d, param=%d", ret_ioctl_val, request, *param);

    close(ccci_sys_fd);
    return ret_ioctl_val;
}

extern int triggerCCCIIoctl(int request)
{
    int param = -1;
    int ret_ioctl_val;

    ret_ioctl_val = triggerCCCIIoctlEx(request, &param);

    return ret_ioctl_val;
}

extern int rilOemMain(int request, void *data, size_t datalen, RIL_Token t)
{
    switch (request)
    {
        case RIL_REQUEST_OEM_HOOK_RAW:
            requestOemHookRaw(data, datalen, t);
            // echo back data
            //RIL_onRequestComplete(t, RIL_E_SUCCESS, data, datalen);
            break;
        case RIL_REQUEST_OEM_HOOK_STRINGS:
            requestOemHookStrings(data,datalen,t);
            break;
        case RIL_REQUEST_SCREEN_STATE:
            requestScreenState(data, datalen, t);
            break;
        case RIL_REQUEST_SET_MUTE:
            requestSetMute(data,datalen,t);
            break;
        case RIL_REQUEST_GET_MUTE:
            requestGetMute(data, datalen, t);
            break;
        case RIL_REQUEST_RESET_RADIO:
            requestResetRadio(data, datalen, t);
            break;
        case RIL_REQUEST_GET_RADIO_CAPABILITY:
            requestGetRadioCapability(data, datalen, t);
            break;
        case RIL_REQUEST_SET_RADIO_CAPABILITY:
            requestSetRadioCapability(data, datalen, t);
            break;
        case RIL_REQUEST_GET_ACTIVITY_INFO:
            requestGetActivityInfo(data, datalen, t);
            break;
        default:
            return 0;  /* no matched request */
            break;
    }

    return 1; /* request found and handled */
}

extern int rilOemUnsolicited(const char *s, const char *sms_pdu, RILChannelCtx* p_channel)
{
    return handleOemUnsolicited(s, sms_pdu, p_channel);
}




