/*
*  Copyright (C) 2014 MediaTek Inc.
*
*  Modification based on code covered by the below mentioned copyright
*  and/or permission notice(s).
*/

/* //$(MTK_PATH_SOURCE)/hardware/ril/mtk-ril/atchannels.c
**
** Copyright 2014, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include <telephony/mtk_ril.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <termios.h>
#include <cutils/properties.h>

#define LOG_NDEBUG 0

#ifdef MTK_RIL_MD1
#define LOG_TAG "AT"
#else
#define LOG_TAG "ATMD2"
#endif
#include <utils/Log.h>


#include "misc.h"
#include "atchannels.h"
#include "at_tok.h"

#ifdef HAVE_AEE_FEATURE
#include "aee.h"
#endif

#ifdef MTK_RIL
static const char *s_atcmd_without_log[] = {
    //"AT+CGLA",
    //"+CGLA",
};

#ifdef MTK_RIL_MD2
static const char* PROPERTY_MODEM_EE = "ril.mux.ee.md2";
#else
static const char* PROPERTY_MODEM_EE = "ril.mux.ee.md1";
#endif

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/// M: Load timeout of special AT command
static ATTimeout s_at_timeout[] = {
#include "at_commands_timeout.h"
};

#if AT_DEBUG
void AT_DUMP(const char *prefix, const char *buff, int len)
{
    if (len < 0)
        len = strlen(buff);
    RLOGD("%.*s", len, buff);
}
#endif  /* AT_DEBUG */

/*
 * for current pending command
 * these are protected by s_commandmutex
 */

static void (*s_onTimeout)(RILChannelCtx *p_channel) = NULL;
static void (*s_onReaderClosed)(RILChannelCtx *p_channel) = NULL;

static void onReaderClosed();
static int writeCtrlZ(const char *s, RILChannelCtx *p_channel);
static int writeline(const char *s, RILChannelCtx *p_channel);

static RILChannelCtx s_RILChannel[RIL_SUPPORT_CHANNELS];

const char *channelIdToString(RILChannelId id);
extern int s_isUserLoad;

extern void RIL_onRequestAck(RIL_Token t);

extern void initRILChannels(void)
{
    RILChannelCtx *channel;

    int i;
    for (i = 0; i < RIL_SUPPORT_CHANNELS; i++) {
        channel = &s_RILChannel[i];
        memset(channel, 0, sizeof(RILChannelCtx));

        channel->fd = -1; /* fd of the AT channel */
        channel->ATBufferCur = channel->ATBuffer;
        channel->myName = channelIdToString(i);
        channel->id = i;
    }
}

#define NS_PER_S 1000000000
static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *)NULL);

    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L) * 1000L;
    /* assuming tv.tv_usec < 10^6 */
    if (p_ts->tv_nsec >= NS_PER_S) {
        p_ts->tv_sec++;
        p_ts->tv_nsec -= NS_PER_S;
    }
}

void sleepMsec(long long msec)
{
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do
        err = nanosleep(&ts, &ts);
    while (err < 0 && errno == EINTR);
}

/** add an intermediate response to sp_response*/
static void addIntermediate(const char *line, RILChannelCtx *p_channel)
{
    ATResponse *p_response = p_channel->p_response;

    ATLine *p_new;
    p_new = (ATLine *) calloc(1, sizeof(ATLine));
    p_new->line = strdup(line);

    /* note: this adds to the head of the list, so the list
     * will be in reverse order of lines received. the order is flipped
     * again before passing on to the command issuer */
    p_new->p_next = p_response->p_intermediates;
    p_response->p_intermediates = p_new;
}

/**
 * returns 1 if line is a final response indicating error
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char *s_finalResponsesError[] = {
    "ERROR",
    "+CMS ERROR:",
    "+CME ERROR:",
    "NO CARRIER",   /* sometimes! */
    "NO ANSWER",
    "NO DIALTONE",
};

static int isFinalResponseError(const char *line)
{
    size_t i;
    for (i = 0; i < NUM_ELEMS(s_finalResponsesError); i++) {
        if (strStartsWith(line, s_finalResponsesError[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * returns 1 if line is a final response indicating success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char *s_finalResponsesSuccess[] = {
    "OK",
    "CONNECT"   /* some stacks start up data on another channel */
};

static int isFinalResponseSuccess(const char *line)
{
    size_t i;
    for (i = 0; i < NUM_ELEMS(s_finalResponsesSuccess); i++) {
        if (strStartsWith(line, s_finalResponsesSuccess[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * returns 1 if line is a final response, either  error or success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static int isFinalResponse(const char *line) {
    return isFinalResponseSuccess(line) || isFinalResponseError(line);
}

/**
 * returns 1 if line is a request acknowledgment from MD
 */
static const char *s_reqAck[] = {
        "ACK"
};
static int isRequestAcked(const char *line) {
    size_t i;
    if ((RIL_VERSION < 13) || (line == NULL)) {
        return 0;
    }
    for (i = 0; i < NUM_ELEMS(s_reqAck); i++) {
        if (strStartsWith(line, s_reqAck[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * returns 1 if line is the first line in (what will be) a two-line
 * SMS unsolicited response
 */
static const char *s_smsUnsoliciteds[] = {
    "+CMT:",
    "+CDS:",
    "+CBM:"
};

static int isSMSUnsolicited(const char *line)
{
    size_t i;
    for (i = 0; i < NUM_ELEMS(s_smsUnsoliciteds); i++) {
        if (strStartsWith(line, s_smsUnsoliciteds[i])) {
            return 1;
        }
    }
    return 0;
}

/** assumes s_commandmutex is held */
static void handleFinalResponse(const char *line, RILChannelCtx *p_channel)
{
    ATResponse *p_response = p_channel->p_response;
    p_response->finalResponse = strdup(line);

    pthread_cond_signal(&p_channel->commandcond);
}

static void handleRequestAck(RILChannelCtx *p_channel)
{
    pthread_cond_signal(&p_channel->commandcond);
}

static void handleUnsolicited(const char *line, RILChannelCtx *p_channel)
{
    if (p_channel->unsolHandler != NULL)
        p_channel->unsolHandler(line, NULL, p_channel);
}

static void processLine(const char *line, RILChannelCtx *p_channel)
{
    ATResponse *p_response = p_channel->p_response;
    const char *smsPDU = p_channel->smsPDU;
    //Move out to the function
    //pthread_mutex_lock(&p_channel->commandmutex);
    int isIntermediateResult = 0;
    int isRequestAck = 0;

    if (p_response == NULL) {
        /* no command pending */
        handleUnsolicited(line, p_channel);
        return;
    } else if (isRequestAcked(line)) {
        isRequestAck = 1;
        RLOGD("receive ACK (%s)\n", line);
    } else {
        switch (p_channel->type) {
        case NO_RESULT:
            //handleUnsolicited(line,p_channel);
            break;
        case NUMERIC:
            if (p_response->p_intermediates == NULL
                && isdigit(line[0])
                ) {
                addIntermediate(line, p_channel);
                isIntermediateResult = 1;
            } else {
                /* either we already have an intermediate response or
                 *     the line doesn't begin with a digit */
                //handleUnsolicited(line,p_channel);
            }
            break;
        case SINGLELINE:
            if (p_response->p_intermediates == NULL
                && strStartsWith(line, p_channel->responsePrefix)
                ) {
                addIntermediate(line, p_channel);
                isIntermediateResult = 1;
            } else {
                /* we already have an intermediate response */
                //handleUnsolicited(line,p_channel);
            }
            break;
        case MULTILINE:
            if (strStartsWith(line, p_channel->responsePrefix)) {
                addIntermediate(line, p_channel);
                isIntermediateResult = 1;
            } else {
                //handleUnsolicited(line,p_channel);
            }
            break;
        /* atci start */
        case RAW:
            if (!isFinalResponseSuccess(line) && !isFinalResponseError(line)) {
                addIntermediate(line, p_channel);
                isIntermediateResult = 1;
            }
            break;
        /* atci end */
        default: /* this should never be reached */
            RLOGE("Unsupported AT command type %d\n", p_channel->type);
            //handleUnsolicited(line,p_channel);
            break;
        }
    }

    if (isRequestAck) {
        p_response->ack = 1;
        handleRequestAck(p_channel);
    } else if (isIntermediateResult) {
        /* No need to run the following code*/
    } else if (isFinalResponseSuccess(line)) {
        p_response->success = 1;
        handleFinalResponse(line, p_channel);
    } else if (isFinalResponseError(line)) {
        p_response->success = 0;
        handleFinalResponse(line, p_channel);
    } else if (smsPDU != NULL && 0 == strcmp(line, "> ")) {
        // See eg. TS 27.005 4.3
        // Commands like AT+CMGS have a "> " prompt
        writeCtrlZ(smsPDU, p_channel);
        smsPDU = NULL;
    } else {
        handleUnsolicited(line, p_channel);
    }
    // Move out to the function
    //pthread_mutex_unlock(&p_channel->commandmutex);
}

/**
 * Returns a pointer to the end of the next line
 * special-cases the "> " SMS prompt
 *
 * returns NULL if there is no complete line
 */
static char *findNextEOL(char *cur)
{
    if (cur[0] == '>' && cur[1] == ' ' && cur[2] == '\0')
        /* SMS prompt character...not \r terminated */
        return cur + 2;

    // Find next newline
    while (*cur != '\0' && *cur != '\r' && *cur != '\n') {
        cur++;
    }

    return *cur == '\0' ? NULL : cur;
}

/**
 * Reads a line from the AT channel, returns NULL on timeout.
 * Assumes it has exclusive read access to the FD
 *
 * This line is valid only until the next call to readline
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */
static const char *readline(RILChannelCtx *p_channel)
{
    ssize_t count;

    char *p_read = NULL;
    char *p_eol = NULL;
    char *ret;
    int i = 0;

    /* this is a little odd. I use *s_ATBufferCur == 0 to
     * mean "buffer consumed completely". If it points to a character, than
     * the buffer continues until a \0
     */
    if (*p_channel->ATBufferCur == '\0') {
        /* empty buffer */
        p_channel->ATBufferCur = p_channel->ATBuffer;
        *p_channel->ATBufferCur = '\0';
        p_read = p_channel->ATBuffer;
    } else { /* *s_ATBufferCur != '\0' */
        /* there's data in the buffer from the last read */

        // skip over leading newlines
        while (*p_channel->ATBufferCur == '\r' || *p_channel->ATBufferCur == '\n')
            p_channel->ATBufferCur++;

        p_eol = findNextEOL(p_channel->ATBufferCur);

        if (p_eol == NULL) {
            /* a partial line. move it up and prepare to read more */
            size_t len;

            len = strlen(p_channel->ATBufferCur);

            memmove(p_channel->ATBuffer, p_channel->ATBufferCur, len + 1);
            p_read = p_channel->ATBuffer + len;
            p_channel->ATBufferCur = p_channel->ATBuffer;
        }
        /* Otherwise, (p_eol !- NULL) there is a complete line  */
        /* that will be returned the while () loop below        */
    }

    while (p_eol == NULL) {
        if (0 == MAX_AT_RESPONSE - (p_read - p_channel->ATBuffer)) {
            RLOGE("ERROR: Input line exceeded buffer\n");
            /* ditch buffer and start over again */
            p_channel->ATBufferCur = p_channel->ATBuffer;
            *p_channel->ATBufferCur = '\0';
            p_read = p_channel->ATBuffer;
        }

        do
            count = read(p_channel->fd, p_read,
                     MAX_AT_RESPONSE - (p_read - p_channel->ATBuffer));
        while (count < 0 && errno == EINTR);

        if (count > 0) {
            p_channel->readCount += count;

            p_read[count] = '\0';


            // skip over leading newlines
            while (*p_channel->ATBufferCur == '\r' || *p_channel->ATBufferCur == '\n')
                p_channel->ATBufferCur++;

            p_eol = findNextEOL(p_channel->ATBufferCur);
            p_read += count;
        } else if (count <= 0) {
            /* read error encountered or EOF reached */
            if (count == 0)
                RLOGD("atchannel: EOF reached");
            else
                RLOGD("atchannel: read error %s", strerror(errno));
            return NULL;
        }
    }

    /* a full line in the buffer. Place a \0 over the \r and return */

    ret = p_channel->ATBufferCur;
    *p_eol = '\0';
    p_channel->ATBufferCur = p_eol + 1; /* this will always be <= p_read,    */
    /* and there will be a \0 at *p_read */

    if (s_isUserLoad) {
        for (i = 0; i < NUM_ELEMS(s_atcmd_without_log); i++) {
            if (strStartsWith(ret, s_atcmd_without_log[i])) {
                RLOGD("AT< %s=xxx\n", s_atcmd_without_log[i]);
                break;
            }
        }
        if (i == NUM_ELEMS(s_atcmd_without_log)) {
            RLOGD("AT< %s\n", ret);
        }
    } else {
        RLOGD("AT< %s\n", ret);
    }
    return ret;
}

static void onReaderClosed(RILChannelCtx *p_channel)
{
    if (s_onReaderClosed != NULL && p_channel->readerClosed == 0) {
        pthread_mutex_lock(&p_channel->commandmutex);

        p_channel->readerClosed = 1;

        pthread_cond_signal(&p_channel->commandcond);

        pthread_mutex_unlock(&p_channel->commandmutex);

        s_onReaderClosed(p_channel);
    }
}


static void *readerLoop(void *arg)
{
    RILChannelCtx *p_channel = (RILChannelCtx *)arg;
    const char *readerName = p_channel->myName;

    RLOGI("%s is up", readerName);


    for (;; ) {
        const char *line;

        line = readline(p_channel);

        RLOGD("%s:%s", readerName, line);

        if (line == NULL)
            break;

        if (isSMSUnsolicited(line)) {
            char *line1;
            const char *line2;
            RLOGD("SMS Urc Received!");
            // The scope of string returned by 'readline()' is valid only
            // till next call to 'readline()' hence making a copy of line
            // before calling readline again.
            line1 = strdup(line);
            line2 = readline(p_channel);

            if (line2 == NULL) {
                RLOGE("NULL line found in %s", readerName);
                free(line1);
                break;
            }

            if (p_channel->unsolHandler != NULL) {
                RLOGD("%s: line1:%s,line2:%s", readerName, line1, line2);
                p_channel->unsolHandler(line1, line2, p_channel);
            }
            free(line1);
        } else {
            pthread_mutex_lock(&p_channel->commandmutex);
            RLOGD("%s Enter processLine", readerName);
            processLine(line, p_channel);
            pthread_mutex_unlock(&p_channel->commandmutex);
        }
    }
    RLOGE("%s Closed", readerName);
    onReaderClosed(p_channel);

    return NULL;
}

/**
 * Sends string s to the radio with a \r appended.
 * Returns AT_ERROR_* on error, 0 on success
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */
static int writeline(const char *s, RILChannelCtx *p_channel)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;
    int i = 0;

    if (p_channel->fd < 0 || p_channel->readerClosed > 0)
        return AT_ERROR_CHANNEL_CLOSED;

    if (s_isUserLoad) {
        for (i = 0; i < NUM_ELEMS(s_atcmd_without_log); i++) {
            if (strStartsWith(s, s_atcmd_without_log[i])) {
                RLOGD("AT> %s=xxx\n", s_atcmd_without_log[i]);
                break;
            }
        }
        if (i == NUM_ELEMS(s_atcmd_without_log)) {
            RLOGD("AT> %s\n", s);
        }
    } else {
        RLOGD("AT> %s\n", s);
    }
    /* the main string */
    while (cur < len) {
        do
            written = write(p_channel->fd, s + cur, len - cur);
        while (written < 0 && errno == EINTR);

        if (written < 0)
            return AT_ERROR_GENERIC;

        cur += written;
    }

    /* the \r  */
    do {
        written = write(p_channel->fd, "\r", 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0)
        return AT_ERROR_GENERIC;

    return 0;
}
static int writeCtrlZ(const char *s, RILChannelCtx *p_channel)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    if (p_channel->fd < 0 || p_channel->readerClosed > 0)
        return AT_ERROR_CHANNEL_CLOSED;

    RLOGD("AT> %s^Z\n", s);

    AT_DUMP(">* ", s, strlen(s));

    /* the main string */
    while (cur < len) {
        do
            written = write(p_channel->fd, s + cur, len - cur);
        while (written < 0 && errno == EINTR);

        if (written < 0)
            return AT_ERROR_GENERIC;

        cur += written;
    }

    /* the ^Z  */
    do {
        written = write(p_channel->fd, "\032", 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0)
        return AT_ERROR_GENERIC;

    return 0;
}

static void clearPendingCommand(RILChannelCtx *p_channel)
{
    if (p_channel->p_response != NULL)
        at_response_free(p_channel->p_response);

    p_channel->p_response = NULL;
    p_channel->responsePrefix = NULL;
    p_channel->smsPDU = NULL;
}

/**
 * Starts AT handler on stream "fd'
 * returns 0 on success, -1 on error
 */
int at_open(int fd, ATUnsolHandler h, RILChannelCtx *p_channel)
{
    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    assert(p_channel->fd == fd);
    p_channel->unsolHandler = h;
    p_channel->readerClosed = 0;

    p_channel->responsePrefix = NULL;
    p_channel->smsPDU = NULL;
    p_channel->p_response = NULL;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&p_channel->tid_reader, &attr, readerLoop, (void *)p_channel);

    if (ret < 0) {
        perror("pthread_create");
        return -1;
    }


    return 0;
}

/* FIXME is it ok to call this from the reader and the command thread? */
void at_close(RILChannelCtx *p_channel)
{
    if (p_channel->fd >= 0)
        close(p_channel->fd);
    p_channel->fd = -1;

    pthread_mutex_lock(&p_channel->commandmutex);

    p_channel->readerClosed = 1;

    pthread_cond_signal(&p_channel->commandcond);

    pthread_mutex_unlock(&p_channel->commandmutex);

    /* the reader thread should eventually die */
}

static ATResponse *at_response_new()
{
    return (ATResponse *)calloc(1, sizeof(ATResponse));
}

void at_response_free(ATResponse *p_response)
{
    ATLine *p_line;

    if (p_response == NULL) return;

    p_line = p_response->p_intermediates;

    while (p_line != NULL) {
        ATLine *p_toFree;

        p_toFree = p_line;
        p_line = p_line->p_next;

        free(p_toFree->line);
        free(p_toFree);
    }

    if (p_response->finalResponse != NULL)
        free(p_response->finalResponse);
    free(p_response);
}

/**
 * The line reader places the intermediate responses in reverse order
 * here we flip them back
 */
static void reverseIntermediates(ATResponse *p_response)
{
    ATLine *pcur, *pnext;

    pcur = p_response->p_intermediates;
    p_response->p_intermediates = NULL;

    while (pcur != NULL) {
        pnext = pcur->p_next;
        pcur->p_next = p_response->p_intermediates;
        p_response->p_intermediates = pcur;
        pcur = pnext;
    }
}

/**
 * Internal send_command implementation
 * Doesn't lock or call the timeout callback
 *
 * timeoutMsec == 0 means infinite timeout
 */
static int at_send_command_full_nolock_ack(const char *command, ATCommandType type,
                       const char *responsePrefix, const char *smspdu,
                       long long timeoutMsec, ATResponse **pp_outResponse, RILChannelCtx *p_channel,
                       RIL_Token ackToken)
{
    int err = 0;

    struct timespec ts;

    if (p_channel->p_response != NULL) {
        err = AT_ERROR_COMMAND_PENDING;
        RLOGE("AT_ERROR_COMMAND_PENDING: %s", command);
        goto error;
    }

    err = writeline(command, p_channel);

    if (err < 0)
        goto error;

    p_channel->type = type;
    p_channel->responsePrefix = responsePrefix;
    p_channel->smsPDU = smspdu;
    p_channel->p_response = at_response_new();

    if (timeoutMsec != 0) {
        setTimespecRelative(&ts, timeoutMsec);
    }

    while (p_channel->p_response->finalResponse == NULL && p_channel->readerClosed == 0) {
        if (timeoutMsec != 0) {
            err = pthread_cond_timedwait(&p_channel->commandcond, &p_channel->commandmutex, &ts);
        } else {
            err = pthread_cond_wait(&p_channel->commandcond, &p_channel->commandmutex);
        }

        if (err == ETIMEDOUT) {
            err = AT_ERROR_TIMEOUT;
            goto error;
        }
        if ((p_channel->p_response->finalResponse == NULL && p_channel->readerClosed == 0)
                && (p_channel->p_response->ack == 1)) {
            if (ackToken != NULL) {
                RIL_onRequestAck(ackToken);
            }
            p_channel->p_response->ack = 0;
        }
    }

    if (p_channel->readerClosed > 0) {
        err = AT_ERROR_CHANNEL_CLOSED;
        goto error;
    }

    if (pp_outResponse == NULL) {
        at_response_free(p_channel->p_response);
    } else {
        /* line reader stores intermediate responses in reverse order */
        reverseIntermediates(p_channel->p_response);
        *pp_outResponse = p_channel->p_response;
    }

    p_channel->p_response = NULL;

    err = 0;

error:
    clearPendingCommand(p_channel);

    return err;
}
static int at_send_command_full_nolock(const char *command, ATCommandType type,
                       const char *responsePrefix, const char *smspdu,
                       long long timeoutMsec, ATResponse **pp_outResponse, RILChannelCtx *p_channel)
{
    return at_send_command_full_nolock_ack(command, type, responsePrefix, smspdu, timeoutMsec,
            pp_outResponse, p_channel, NULL);
}

/**
 * Internal send_command implementation
 *
 * timeoutMsec == 0 means infinite timeout
 */
static int at_send_command_full_ack(const char *command, ATCommandType type,
                const char *responsePrefix, const char *smspdu,
                long long timeoutMsec, ATResponse **pp_outResponse, RILChannelCtx *p_channel,
                RIL_Token ackToken)
{
    int err;

    if (0 != pthread_equal(p_channel->tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        RLOGD("Invalid Thread: send on %s, reader:%lu, self: %lu", p_channel->myName, p_channel->tid_reader, pthread_self());
        return AT_ERROR_INVALID_THREAD;
    }

    if (0 != p_channel->tid_myProxy) {
        /* This channel is occupied by some proxy */
        RLOGD("Occupied Thread: %s send on %s, pthread_self(): %lu, tid: %lu", command, p_channel->myName, pthread_self(), p_channel->tid_myProxy);
        assert(0);
        return AT_ERROR_INVALID_THREAD;
    }

    pthread_mutex_lock(&p_channel->commandmutex);

    /* Assign owner proxy */
    p_channel->tid_myProxy = pthread_self();

    RLOGD("AT send on %s, tid:%lu", p_channel->myName, p_channel->tid_myProxy);

    // 10 minutes detection mechanism for AT commands not replying
    timeoutMsec = getATCommandTimeout(command);
    err = at_send_command_full_nolock_ack(command, type, responsePrefix, smspdu, timeoutMsec,
            pp_outResponse, p_channel, ackToken);
    if ((isInternalLoad() == 1) || (s_isUserLoad != 1)) {
        char modemException[PROPERTY_VALUE_MAX] = { 0 };
        property_get(PROPERTY_MODEM_EE, modemException, "0");
        if (err == AT_ERROR_TIMEOUT) {
            char *pErrMsg = NULL;
            pErrMsg = calloc(1, 201);
            if (pErrMsg != NULL) {
                // parse the AT CMD
                char key[20] = {0};
                int cmdLen = strlen(command);
                int i = 0, start = 0, end = cmdLen - 1;
                for (i = 0; i < cmdLen; i++) {
                    if (command[i] == '+') {
                        start = i + 1;
                    }
                    if (command[i] == '=' || command[i] == '?') {
                        end = i - 1;
                        break;
                    }
                }
                strncpy(key, (command + start), MIN(19, (end - start + 1)));
                if (atoi(modemException) != 1) {
                    snprintf(pErrMsg, 200, "AT command pending too long, assert!!! AT cmd: %s, timer: %dms\nCRDISPATCH_KEY:ATTO=%s", key, timeoutMsec, key);
                    RLOGD("AT command pending too long, assert!!! on %s, tid:%lu, AT cmd: %s, AT command timeout: %dms",
                            p_channel->myName, p_channel->tid_myProxy, command, timeoutMsec);
                    #ifdef HAVE_AEE_FEATURE
                    aee_system_exception("mtkrild", NULL, DB_OPT_DEFAULT, pErrMsg);
                    exit(0);
                    #else
                    LOG_ALWAYS_FATAL(pErrMsg);
                    #endif
                } else {
                    snprintf(pErrMsg, 200, "Modem already exception, assert!!!  AT cmd: %s, timer: %dms\nCRDISPATCH_KEY:ATTO=%s", key, timeoutMsec, key);
                    RLOGD("Modem already exception, assert!!! on %s, tid:%lu, last AT cmd: %s, AT command timeout: %dms",
                            p_channel->myName, p_channel->tid_myProxy, command, timeoutMsec);
                    exit(0);
                }
                free(pErrMsg);
            } else {
                if (atoi(modemException) != 1) {
                    LOG_ALWAYS_FATAL("AT command pending too long, assert!!! on %s, tid:%lu, AT cmd: %s, AT command timeout: %dms",
                            p_channel->myName, p_channel->tid_myProxy, command, timeoutMsec);
                } else {
                    LOG_ALWAYS_FATAL("Modem already exception, assert!!! on %s, tid:%lu, last AT cmd: %s, AT command timeout: %dms",
                            p_channel->myName, p_channel->tid_myProxy, command, timeoutMsec);
                }
            }
        }
    } else {
        if (err == AT_ERROR_TIMEOUT) {
        #ifdef MTK_RIL_MD2
            if (isDualTalkMode() && (RIL_get3GSIM() != 1)) {
                // DSDA and SIM switch => reset MD
                property_set("ril.mux.report.case", "2");
            } else {
                // reset MD2
                property_set("ril.mux.report.case", "6");
            }
        #else
            if (isDualTalkMode() && (RIL_get3GSIM() != 1)) {
                // DSDA and SIM switch => reset MD2
                property_set("ril.mux.report.case", "6");
            } else {
                // reset MD
                property_set("ril.mux.report.case", "2");
            }
        #endif
            property_set("ril.muxreport", "1");
        }
    }

    RLOGD("response received on %s, tid:%lu", p_channel->myName, p_channel->tid_myProxy);

    /* Release the proxy */
    p_channel->tid_myProxy = 0;

    pthread_mutex_unlock(&p_channel->commandmutex);

    if (err == AT_ERROR_TIMEOUT && s_onTimeout != NULL)
        s_onTimeout(p_channel);

    return err;
}
static int at_send_command_full(const char *command, ATCommandType type,
                const char *responsePrefix, const char *smspdu,
                long long timeoutMsec, ATResponse **pp_outResponse, RILChannelCtx *p_channel)
{
    return at_send_command_full_ack(command, type, responsePrefix, smspdu, timeoutMsec,
            pp_outResponse, p_channel, NULL);
}

RILChannelCtx *getChannelCtxbyId(RILChannelId id)
{
    assert(id < RIL_SUPPORT_CHANNELS);
    return &s_RILChannel[id];
}

RIL_SOCKET_ID getRILIdByChannelCtx(RILChannelCtx *p_channel)
{
#ifndef MTK_GEMINI
    return RIL_SOCKET_1;
#else   /* MTK_GEMINI */
    if (p_channel->id < RIL_CHANNEL_OFFSET)
        return RIL_SOCKET_1;
    else
#if (SIM_COUNT >= 4)/* Gemini plus 4 SIM*/
         if (p_channel->id >= RIL_CHANNEL_SET4_OFFSET)
             return RIL_SOCKET_4;
#endif
#if (SIM_COUNT >= 3)/* Gemini plus 3 SIM*/
         if (p_channel->id >= RIL_CHANNEL_SET3_OFFSET)
             return RIL_SOCKET_3;
#endif
        return RIL_SOCKET_2;
#endif  /* MTK_GEMINI */
}

RILChannelCtx *getDefaultChannelCtx(RIL_SOCKET_ID rid)
{
    RILChannelCtx *channel = &s_RILChannel[RIL_URC];

#ifdef  MTK_GEMINI
    if (RIL_SOCKET_2 == rid)
        channel = &s_RILChannel[RIL_URC2];
#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
    if (RIL_SOCKET_3 == rid)
        channel = &s_RILChannel[RIL_URC3];
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
    if (RIL_SOCKET_4 == rid)
        channel = &s_RILChannel[RIL_URC4];
#endif
#endif  /* MTK_GEMINI */

    return channel;
}

RILChannelCtx *getRILChannelCtx(RILSubSystemId subsystem, RIL_SOCKET_ID rid)
{
    RILChannelCtx *channel;
    int channelOffset = rid*RIL_CHANNEL_OFFSET;
    RLOGD("getRILChannelCtx subsystem:%d, rid:%d channelOffset:%d", subsystem, rid, channelOffset);

    switch (subsystem) {
        case RIL_CC:
        case RIL_SS:
            channel = &s_RILChannel[channelOffset+RIL_CMD_2];
            break;
        case RIL_DATA:
            channel = &s_RILChannel[channelOffset+RIL_CMD_4];
            break;
        case RIL_NW:
        case RIL_OEM:
            channel = &s_RILChannel[channelOffset+RIL_CMD_1];
            break;
        case RIL_SIM:
        case RIL_STK:
        case RIL_SMS:
            channel = &s_RILChannel[channelOffset+RIL_CMD_3];
            break;
        case RIL_DEFAULT:
        default:
            /* RIL_URC as default */
            channel = &s_RILChannel[channelOffset+RIL_URC];
            break;
    }

    return channel;
}

RILChannelCtx *getRILChannelCtxFromToken(RIL_Token t)
{
    assert(RIL_queryMyChannelId(t) < RIL_SUPPORT_CHANNELS);
    return &(s_RILChannel[RIL_queryMyChannelId(t)]);
}

RILChannelCtx *getChannelCtxbyProxy(RIL_SOCKET_ID rid)
{
    int proxyId = RIL_queryMyProxyIdByThread();
    assert(proxyId < RIL_SUPPORT_CHANNELS);

    RLOGD("getChannelCtxbyProxy proxyId:%d, tid:%lu", proxyId, pthread_self());
    if (proxyId > -1 && proxyId < RIL_SUPPORT_CHANNELS)
#ifdef MTK_GEMINI
            if ((rid == RIL_SOCKET_1 && proxyId >= RIL_CHANNEL_OFFSET) ||
            (rid == RIL_SOCKET_2 && proxyId < RIL_CHANNEL_OFFSET))
                return getDefaultChannelCtx(rid);
#if (SIM_COUNT >= 4)/* Gemini plus 4 SIM*/
            else if (rid == RIL_SOCKET_4 && proxyId < RIL_CHANNEL_SET4_OFFSET)
                return getDefaultChannelCtx(rid);
#endif
#if (SIM_COUNT >= 3)/* Gemini plus 3 SIM*/
            else if (rid == RIL_SOCKET_3 && proxyId < RIL_CHANNEL_SET3_OFFSET)
                return getDefaultChannelCtx(rid);
#endif
        else
#endif /* MTK_GEMINI */
            return &s_RILChannel[proxyId];
    else
        // This is not a proxy thread (URC thread)
        return getDefaultChannelCtx(rid);
}

RIL_SOCKET_ID getMainProtocolRid() {
    char prop_value[PROPERTY_VALUE_MAX] = { 0 };
    int targetSim = 0;

    property_get(PROPERTY_3G_SIM, prop_value, "1");
    targetSim = atoi(prop_value);

    switch (targetSim) {
#ifdef MTK_GEMINI
        case 2:
            return RIL_SOCKET_2;
            break;
#if (SIM_COUNT >= 3)
        case 3:
            return RIL_SOCKET_3;
            break;
#endif
#if (SIM_COUNT >= 4)
        case 4:
            return RIL_SOCKET_4;
            break;
#endif
#endif // #ifdef MTK_GEMINI
        default:
        case 1:
            return RIL_SOCKET_1;
            break;
    }
}
/**
 * Issue a single normal AT command with no intermediate response expected
 *
 * "command" should not include \r
 * pp_outResponse can be NULL
 *
 * if non-NULL, the resulting ATResponse * must be eventually freed with at_response_free
 */
int at_send_command(const char *command, ATResponse **pp_outResponse, RILChannelCtx *p_channel) {
    return at_send_command_ack(command, pp_outResponse, p_channel, NULL);
}

int at_send_command_ack(const char *command, ATResponse **pp_outResponse, RILChannelCtx *p_channel,
        RIL_Token ackToken) {
    int err;
    err = at_send_command_full_ack(command, NO_RESULT, NULL, NULL, 0, pp_outResponse, p_channel,
            ackToken);
    return err;
}


int at_send_command_singleline(const char *    command,
                   const char *    responsePrefix,
                   ATResponse **    pp_outResponse,
                   RILChannelCtx *    p_channel)
{
    return at_send_command_singleline_ack(command, responsePrefix, pp_outResponse, p_channel, NULL);
}
int at_send_command_singleline_ack(const char *    command,
                   const char *    responsePrefix,
                   ATResponse **    pp_outResponse,
                   RILChannelCtx *    p_channel,
                   RIL_Token ackToken)
{
    int err;
    err = at_send_command_full_ack(command, SINGLELINE, responsePrefix, NULL, 0, pp_outResponse,
            p_channel, ackToken);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
        ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_numeric(const char *    command,
                ATResponse **    pp_outResponse,
                RILChannelCtx *    p_channel)
{
    return at_send_command_numeric_ack(command,
                    pp_outResponse,
                    p_channel, NULL);
}
int at_send_command_numeric_ack(const char *    command,
                ATResponse **    pp_outResponse,
                RILChannelCtx *    p_channel,
                RIL_Token ackToken)
{
    int err;
    err = at_send_command_full_ack(command, NUMERIC, NULL, NULL, 0, pp_outResponse, p_channel,
            ackToken);

    if (err == 0 && pp_outResponse != NULL &&
        (*pp_outResponse)->success > 0 &&
        (*pp_outResponse)->p_intermediates == NULL)
    {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_sms(const char *    command,
                        const char *    pdu,
                        const char *    responsePrefix,
                        ATResponse **    pp_outResponse,
                        RILChannelCtx * p_channel)
{
    return at_send_command_sms_ack(command, pdu, responsePrefix, pp_outResponse, p_channel, NULL);
}
int at_send_command_sms_ack(const char *    command,
                        const char *    pdu,
                        const char *    responsePrefix,
                        ATResponse **    pp_outResponse,
                        RILChannelCtx * p_channel,
                        RIL_Token ackToken)
{
    int err;
    err = at_send_command_full_ack(command, SINGLELINE, responsePrefix, pdu, 0, pp_outResponse,
            p_channel, ackToken);

    if (err == 0 && pp_outResponse != NULL &&
        (*pp_outResponse)->success > 0 &&
        (*pp_outResponse)->p_intermediates == NULL) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_multiline(  const char * command,
                                const char * responsePrefix,
                                ATResponse ** pp_outResponse,
                                RILChannelCtx * p_channel)
{
    return at_send_command_multiline_ack(command, responsePrefix, pp_outResponse, p_channel, NULL);
}
int at_send_command_multiline_ack(  const char * command,
                                const char * responsePrefix,
                                ATResponse ** pp_outResponse,
                                RILChannelCtx * p_channel,
                                RIL_Token ackToken)
{
    int err;
    err = at_send_command_full_ack(command, MULTILINE, responsePrefix, NULL, 0, pp_outResponse,
            p_channel, ackToken);
    return err;
}

/* atci start */
int at_send_command_raw(const char *command, ATResponse **pp_outResponse, RILChannelCtx *p_channel)
{
    return at_send_command_raw_ack(command, pp_outResponse, p_channel, NULL);
}
int at_send_command_raw_ack(const char *command, ATResponse **pp_outResponse,
        RILChannelCtx *p_channel, RIL_Token ackToken)
{
    int err;
    err = at_send_command_full_ack(command, RAW, NULL, NULL, 0, pp_outResponse, p_channel,
            ackToken);
    return err;
}
/* atci end */

/** This callback is invoked on the command thread */
void at_set_on_timeout(void (*onTimeout)(RILChannelCtx *p_channel))
{
    s_onTimeout = onTimeout;
}

/**
 *  This callback is invoked on the reader thread (like ATUnsolHandler)
 *  when the input stream closes before you call at_close
 *  (not when you call at_close())
 *  You should still call at_close()
 */
void at_set_on_reader_closed(void (*onClose)(RILChannelCtx *p_channel))
{
    s_onReaderClosed = onClose;
}

/**
 * Periodically issue an AT command and wait for a response.
 * Used to ensure channel has start up and is active
 */
int at_handshake(RILChannelCtx *p_channel)
{
    int i;
    int err = 0;

    if (0 != pthread_equal(p_channel->tid_reader, pthread_self()))
        /* cannot be called from reader thread */
        return AT_ERROR_INVALID_THREAD;

    pthread_mutex_lock(&p_channel->commandmutex);

    for (i = 0; i < HANDSHAKE_RETRY_COUNT; i++) {
        /* some stacks start with verbose off */
        err = at_send_command_full_nolock("ATE0Q0V1", NO_RESULT, NULL, NULL, 0, NULL, p_channel);
        if (err == 0)
            break;
    }

    if (err == 0) {
        /* pause for a bit to let the input buffer drain any unmatched OK's
         * (they will appear as extraneous unsolicited responses) */
        sleepMsec(HANDSHAKE_TIMEOUT_MSEC);
    }

    pthread_mutex_unlock(&p_channel->commandmutex);

    return err;
}

/**
 * Returns error code from response
 * Assumes AT+CMEE=1 (numeric) mode
 */
AT_CME_Error at_get_cme_error(const ATResponse *p_response)
{
    int ret;
    int err;
    char *p_cur;

    if (p_response->success > 0)
        return CME_SUCCESS;

    if (p_response->finalResponse == NULL)
        return CME_ERROR_NON_CME;

    if (strStartsWith(p_response->finalResponse, "ERROR"))
        return CME_UNKNOWN;

    if (!strStartsWith(p_response->finalResponse, "+CME ERROR:"))
        return CME_ERROR_NON_CME;

    p_cur = p_response->finalResponse;
    err = at_tok_start(&p_cur);

    if (err < 0)
        return CME_ERROR_NON_CME;

    err = at_tok_nextint(&p_cur, &ret);

    if (err < 0)
        return CME_ERROR_NON_CME;

    return (AT_CME_Error)ret;
}

// M: For PPP channel
static RILChannelCtx g_pppDataChannel = {0};
static int g_channelIndex = 0;

RILChannelCtx *openPPPDataChannel(int isBlocking)
{
    RLOGI("openDataChannel");
    RILChannelCtx* p_channel = &g_pppDataChannel;
    if (p_channel->fd > 0)
        closePPPDataChannel();

    memset(p_channel, 0, sizeof(RILChannelCtx));
    p_channel->fd = -1;    /* fd of the AT channel */

    int retryCounter = 0;
    int err = 0;
    while (p_channel->fd < 0 && retryCounter < 5) {
        do {
            RLOGI("set property for usb permission");
            /* set this property than the permission of /dev/ttyACM0 will be set to 777 */
            property_set("gsm.usb.ttyusb", "1");
            if (isBlocking)
                p_channel->fd = open("/dev/ttyUSB0", O_RDWR);
            else
                p_channel->fd = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK);
        } while (p_channel->fd < 0 && errno == EINTR);

        if (p_channel->fd < 0) {
            perror ("opening AT interface. retrying...");
            RLOGE("could not connect to %s: %s", "/dev/ttyUSB0", strerror(errno));
            /* reduce polling time for usb connected */
            sleep(1);
            /* never returns */
        } else {
            struct termios ios;
            tcgetattr(p_channel->fd, &ios );
            ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */
            ios.c_iflag = 0;
            tcsetattr(p_channel->fd, TCSANOW, &ios );
        }
        ++retryCounter;
    }

    if (p_channel->fd < 0) {
        RLOGE("/dev/ttyUSB0 open failed");
        return NULL;
    } else {
        RLOGI("/dev/ttyUSB0 open success");
        p_channel->ATBufferCur = p_channel->ATBuffer;
        p_channel->myName = "PPP_CHANNEL";
        p_channel->id = ++g_channelIndex;
        p_channel->unsolHandler = 0;
        p_channel->readerClosed = 0;
        p_channel->responsePrefix = NULL;
        p_channel->smsPDU = NULL;
        p_channel->p_response = NULL;
    }
    return &g_pppDataChannel;
}

void closePPPDataChannel()
{
    RILChannelCtx* p_channel = &g_pppDataChannel;
    RLOGI("closeDataChannel [%d, %d]", g_channelIndex, p_channel->fd);
    if (p_channel->fd >= 0) {
        close(p_channel->fd);
    }
    p_channel->fd = -1;
}

int at_send_command_to_ppp_data_channel(const char *command, ATResponse **pp_outResponse, RILChannelCtx *p_channel) {
    const char* line = NULL;
    int ret = writeline(command, p_channel);
    if (ret == 0) {
        p_channel->p_response = at_response_new();
        do {
            line = readline(p_channel);
            if (line != NULL)
                RLOGI("readline: %s", line);
            else
                RLOGI("readline: EMPTY");
        } while (line != NULL && !(strcmp(line, "OK") == 0 || strcmp(line, "NO CARRIER") == 0 || strStartsWith(line, "CONNECT") == 1 || strstr(line, "ERROR")));

        if (line != NULL) {
            RLOGI("process line: %s", line);
            processLine(line, p_channel);
            if (pp_outResponse == NULL) {
                at_response_free(p_channel->p_response);
            } else {
                reverseIntermediates(p_channel->p_response);
                *pp_outResponse = p_channel->p_response;
            }
            return 0;
        }
    }
    return AT_ERROR_GENERIC;
}

void waitForTargetPPPStopped(RILChannelCtx *p_channel) {
    const char* line = NULL;
    int count = 0;
    p_channel->p_response = at_response_new();
    while (count < 60) {
        line = readline(p_channel);
        if (line != NULL && strcmp(line, "NO CARRIER") == 0) {
            RLOGI("readline: %s [%d]", line, count);
            break;
        } else {
            RLOGI("Still wait for NO CARRIER [%d]", count);
            ++count;
            sleep(1);
        }
    }
    at_response_free(p_channel->p_response);
}

void purge_data_channel(RILChannelCtx *p_channel) {
    if (p_channel > 0 && p_channel->fd > 0) {
        const char* line = NULL;
        char buffer[64] = {0};
        int count = 0;
        int readbyte = 0;
        while ((readbyte = read(p_channel->fd, buffer, 64)) > 0) {
            count += readbyte;
        }
        RLOGI("Total %d byte purged", count);
    } else {
        RLOGI("Channel not opened, not to purge");
    }
}
// End of PPP channel

const char *channelIdToString(RILChannelId id)
{
    switch (id) {
        case RIL_URC:       return "RIL_URC_READER";
        case RIL_CMD_1:     return "RIL_CMD_READER_1";
        case RIL_CMD_2:     return "RIL_CMD_READER_2";
        case RIL_CMD_3:     return "RIL_CMD_READER_3";
        case RIL_CMD_4:     return "RIL_CMD_READER_4";
        case RIL_ATCI:      return "RIL_ATCI_READER";
#ifdef  MTK_GEMINI
        case RIL_URC2:      return "RIL_URC2_READER";
        case RIL_CMD2_1:    return "RIL_CMD2_READER_1";
        case RIL_CMD2_2:    return "RIL_CMD2_READER_2";
        case RIL_CMD2_3:    return "RIL_CMD2_READER_3";
        case RIL_CMD2_4:    return "RIL_CMD2_READER_4";
        case RIL_ATCI2:     return "RIL_ATCI2_READER";

#if (SIM_COUNT >= 3) /* Gemini plus 3 SIM*/
        case RIL_URC3:        return "RIL_URC3_READER";
        case RIL_CMD3_1:    return "RIL_CMD3_READER_1";
        case RIL_CMD3_2:    return "RIL_CMD3_READER_2";
        case RIL_CMD3_3:    return "RIL_CMD3_READER_3";
        case RIL_CMD3_4:    return "RIL_CMD3_READER_4";
        case RIL_ATCI3:         return "RIL_ATCI3_READER";
#endif
#if (SIM_COUNT >= 4) /* Gemini plus 4 SIM*/
        case RIL_URC4:        return "RIL_URC4_READER";
        case RIL_CMD4_1:    return "RIL_CMD4_READER_1";
        case RIL_CMD4_2:    return "RIL_CMD4_READER_2";
        case RIL_CMD4_3:    return "RIL_CMD4_READER_3";
        case RIL_CMD4_4:    return "RIL_CMD4_READER_4";
        case RIL_ATCI4:         return "RIL_ATCI4_READER";
#endif
#endif  /* MTK_GEMINI */
        default:            return "<unknown proxy>";
    }
}

inline int isATCmdRspErr(int err, const ATResponse *p_response)
{
    //assert(p_response); //checking null here ???
    return (err < 0 || 0 == p_response->success) ? 1: 0;
}

int getATCommandTimeout(const char *command) {
    size_t i;
    for (i=0; i<NUM_ELEMS(s_at_timeout); i++) {
        if (strstr(command, s_at_timeout[i].command) != NULL) {
            return (s_at_timeout[i].timeout * 60 * 1000);
        }
    }
    return s_at_timeoutMsec;
}

int isInternalLoad() {
    #ifdef __PRODUCTION_RELEASE__
        return 0;
    #else
        char property_value[PROPERTY_VALUE_MAX] = { 0 };
        property_get("ril.emulation.production", property_value, "0");
        return (strcmp("1", property_value) != 0);
    #endif /* __PRODUCTION_RELEASE__ */
}

#endif  /* MTK_RIL */
