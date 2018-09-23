#include "AudioUtility.h"

#include "AudioLock.h"
#include <sys/auxv.h>

#ifdef MTK_PERFSERVICE_SUPPORT
#include "perfservice_types.h"
#endif
#include <dlfcn.h>

namespace android
{

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#if defined(__LP64__)
const char* AUDIO_COMPENSATION_FILTER_LIB_VENDOR_PATH = "/vendor/lib64/libaudiocompensationfilter.so";
const char* AUDIO_COMPENSATION_FILTER_LIB_PATH = "/system/lib64/libaudiocompensationfilter.so";
const char* AUDIO_COMPONENT_ENGINE_LIB_VENDOR_PATH = "/vendor/lib64/libaudiocomponentengine.so";
const char* AUDIO_COMPONENT_ENGINE_LIB_PATH = "/system/lib64/libaudiocomponentengine.so";

const char* AUDIO_PERFSERVICE_LIB_PATH = "/system/lib64/libperfservicenative.so";
#else
const char* AUDIO_COMPENSATION_FILTER_LIB_VENDOR_PATH = "/vendor/lib/libaudiocompensationfilter.so";
const char* AUDIO_COMPENSATION_FILTER_LIB_PATH = "/system/lib/libaudiocompensationfilter.so";
const char* AUDIO_COMPONENT_ENGINE_LIB_VENDOR_PATH = "/vendor/lib/libaudiocomponentengine.so";
const char* AUDIO_COMPONENT_ENGINE_LIB_PATH = "/system/lib/libaudiocomponentengine.so";

const char* AUDIO_PERFSERVICE_LIB_PATH = "/system/lib/libperfservicenative.so";
#endif
static void *g_AudioComponentEngineHandle = NULL;
static create_AudioBitConverter *g_CreateMtkAudioBitConverter = NULL;
static create_AudioSrc *g_CreateMtkAudioSrc = NULL;
static create_AudioLoud *g_CreateMtkAudioLoud = NULL;
static destroy_AudioBitConverter *g_DestroyMtkAudioBitConverter = NULL;
static destroy_AudioSrc *g_DestroyMtkAudioSrc = NULL;
static destroy_AudioLoud *g_DestroyMtkAudioLoud = NULL;
static void *g_AudioCompensationFilterHandle = NULL;
static accessAudioCompFltCustParam *g_setAudioCompFltCustParamFrom = NULL;
static accessAudioCompFltCustParam *g_getAudioCompFltCustParamFrom = NULL;

const static char* ProcessBitsString[] ={"v7l","v8l","aarch64"};

#ifdef MTK_BASIC_PACKAGE
#define DUMP_PATH "/data/misc/audioserver/"
#else
#define DUMP_PATH "/sdcard/mtklog/audio_dump/"
#endif

const char *audio_dump_path = DUMP_PATH;
const char *streamout_ori = DUMP_PATH"streamoutori_dump.pcm";
const char *streamout_ori_propty = "streamout_ori.pcm.dump";
const char *streamout_dcr = DUMP_PATH"streamoutdcr_dump.pcm";
const char *streamout_dcr_propty = "streamout_dcr.pcm.dump";

const char *streamout_s2m = DUMP_PATH"streamouts2m_dump.pcm";
const char *streamout_s2m_propty = "streamout_s2m.pcm.dump";
const char *streamout_acf = DUMP_PATH"streamoutacf_dump.pcm";
const char *streamout_acf_propty = "streamout_acf.pcm.dump";
const char *streamout_hcf = DUMP_PATH"streamouthcf_dump.pcm";
const char *streamout_hcf_propty = "streamout_hcf.pcm.dump";

const char *streamout = DUMP_PATH"streamout.pcm";
const char *streamoutfinal = DUMP_PATH"streamoutfinal.pcm";
const char *streamout_propty = "streamout.pcm.dump";
const char *aud_dumpftrace_dbg_propty = "dumpftrace_dbg";
const char *streaminIVCPMIn = DUMP_PATH"StreamIVPCMIn_Dump.pcm";
const char *streaminIVIn = DUMP_PATH"StreamIVIn_Dump.pcm";
const char *streamout_vibsignal = DUMP_PATH"streamoutvib.pcm";
const char *streamout_notch = DUMP_PATH"streamoutnotch.pcm";

const char *streaminmanager = DUMP_PATH"StreamInManager_Dump.pcm";
const char *streamin = DUMP_PATH"StreamIn_Dump.pcm";
const char *streaminOri = DUMP_PATH"StreamInOri_Dump.pcm";
const char *streaminI2S = DUMP_PATH"StreamInI2S_Dump.pcm";
const char *streaminDAIBT = DUMP_PATH"StreamInDAIBT_Dump.pcm";
const char *streaminSpk = DUMP_PATH"streamin_spk.pcm";
const char *streaminSpk_propty = "streamin.spkpcm.dump";
const char *capture_data_provider = DUMP_PATH"CaptureDataProvider";

const char *streamin_propty = "streamin.pcm.dump";
const char *streamin_epl_propty = "streamin.epl.dump";

const char *allow_low_latency_propty = "streamout.lowlatency.allow";
const char *streamhfp_propty = "streamhfp.pcm.dump";
const char *allow_offload_propty = "streamout.offload.allow";
const char *streaminlog_propty = "streamin.log";


unsigned long long TimeDifference(struct timespec time1, struct timespec time2)
{
    unsigned long long diffns = 0;
    struct timespec tstemp1 = time1;
    struct timespec tstemp2 = time2;

    //    ALOGD("TimeStampDiff time1 sec= %ld, nsec=%ld, time2 sec=%ld, nsec=%ld" ,tstemp1.tv_sec, tstemp1.tv_nsec, tstemp2.tv_sec, tstemp2.tv_nsec);

    if (tstemp1.tv_sec > tstemp2.tv_sec)
    {
        if (tstemp1.tv_nsec >= tstemp2.tv_nsec)
        {
            diffns = ((tstemp1.tv_sec - tstemp2.tv_sec) * (unsigned long long)1000000000) + tstemp1.tv_nsec - tstemp2.tv_nsec;
        }
        else
        {
            diffns = ((tstemp1.tv_sec - tstemp2.tv_sec - 1) * (unsigned long long)1000000000) + tstemp1.tv_nsec + 1000000000 - tstemp2.tv_nsec;
        }
    }
    else if (tstemp1.tv_sec == tstemp2.tv_sec)
    {
        if (tstemp1.tv_nsec >= tstemp2.tv_nsec)
        {
            diffns = tstemp1.tv_nsec - tstemp2.tv_nsec;
        }
        else
        {
            diffns = tstemp2.tv_nsec - tstemp1.tv_nsec;
        }
    }
    else
    {
        if (tstemp2.tv_nsec >= tstemp1.tv_nsec)
        {
            diffns = ((tstemp2.tv_sec - tstemp1.tv_sec) * (unsigned long long)1000000000) + tstemp2.tv_nsec - tstemp1.tv_nsec;
        }
        else
        {
            diffns = ((tstemp2.tv_sec - tstemp1.tv_sec - 1) * (unsigned long long)1000000000) + tstemp2.tv_nsec + 1000000000 - tstemp1.tv_nsec;
        }
    }
    //    ALOGD("TimeDifference time1 sec= %ld, nsec=%ld, time2 sec=%ld, nsec=%ld, diffns=%lld" ,tstemp1.tv_sec, tstemp1.tv_nsec, tstemp2.tv_sec,tstemp2.tv_nsec,diffns);
    return diffns;
}


//---------- implementation of ringbuffer--------------


// function for get how many data is available

/**
* function for get how many data is available
* @return how many data exist
*/

int RingBuf_getDataCount(const RingBuf *RingBuf1)
{
    /*
    ALOGD("RingBuf1->pBase = 0x%x RingBuf1->pWrite = 0x%x  RingBuf1->bufLen = %d  RingBuf1->pRead = 0x%x",
        RingBuf1->pBufBase,RingBuf1->pWrite, RingBuf1->bufLen,RingBuf1->pRead);
        */
    int count = RingBuf1->pWrite - RingBuf1->pRead;
    if (count < 0) { count += RingBuf1->bufLen; }
    return count;
}

/**
*  function for get how free space available
* @return how free sapce
*/

int RingBuf_getFreeSpace(const RingBuf *RingBuf1)
{
    /*
    ALOGD("RingBuf1->pBase = 0x%x RingBuf1->pWrite = 0x%x  RingBuf1->bufLen = %d  RingBuf1->pRead = 0x%x",
        RingBuf1->pBufBase,RingBuf1->pWrite, RingBuf1->bufLen,RingBuf1->pRead);*/
    int count = 0;

    if (RingBuf1->pRead > RingBuf1->pWrite)
    {
        count = RingBuf1->pRead - RingBuf1->pWrite - 8;
    }
    else // RingBuf1->pRead <= RingBuf1->pWrite
    {
        count = RingBuf1->pRead - RingBuf1->pWrite + RingBuf1->bufLen - 8;
    }

    return (count > 0) ? count : 0;
}

/**
* copy count number bytes from ring buffer to buf
* @param buf buffer copy from
* @param RingBuf1 buffer copy to
* @param count number of bytes need to copy
*/

void RingBuf_copyToLinear(char *buf, RingBuf *RingBuf1, int count)
{
    /*
    ALOGD("RingBuf1->pBase = 0x%x RingBuf1->pWrite = 0x%x  RingBuf1->bufLen = %d  RingBuf1->pRead = 0x%x",
        RingBuf1->pBufBase,RingBuf1->pWrite, RingBuf1->bufLen,RingBuf1->pRead);*/
    if (RingBuf1->pRead <= RingBuf1->pWrite)
    {
        memcpy(buf, RingBuf1->pRead, count);
        RingBuf1->pRead += count;
    }
    else
    {
        char *end = RingBuf1->pBufBase + RingBuf1->bufLen;
        int r2e = end - RingBuf1->pRead;
        if (count <= r2e)
        {
            //ALOGD("2 RingBuf_copyToLinear r2e= %d count = %d",r2e,count);
            memcpy(buf, RingBuf1->pRead, count);
            RingBuf1->pRead += count;
            if (RingBuf1->pRead == end)
            {
                RingBuf1->pRead = RingBuf1->pBufBase;
            }
        }
        else
        {
            //ALOGD("3 RingBuf_copyToLinear r2e= %d count = %d",r2e,count);
            memcpy(buf, RingBuf1->pRead, r2e);
            memcpy(buf + r2e, RingBuf1->pBufBase, count - r2e);
            RingBuf1->pRead = RingBuf1->pBufBase + count - r2e;
        }
    }
}

/**
* copy count number bytes from buf to RingBuf1
* @param RingBuf1 ring buffer copy from
* @param buf copy to
* @param count number of bytes need to copy
*/
void RingBuf_copyFromLinear(RingBuf *RingBuf1, const char *buf, int count)
{
    int spaceIHave;
    char *end = RingBuf1->pBufBase + RingBuf1->bufLen;

    // count buffer data I have
    spaceIHave = RingBuf1->bufLen - RingBuf_getDataCount(RingBuf1) - 8;
    //spaceIHave = RingBuf_getDataCount(RingBuf1);

    // if not enough, assert
    ASSERT(spaceIHave >= count);

    if (RingBuf1->pRead <= RingBuf1->pWrite)
    {
        int w2e = end - RingBuf1->pWrite;
        if (count <= w2e)
        {
            memcpy(RingBuf1->pWrite, buf, count);
            RingBuf1->pWrite += count;
            if (RingBuf1->pWrite == end)
            {
                RingBuf1->pWrite = RingBuf1->pBufBase;
            }
        }
        else
        {
            memcpy(RingBuf1->pWrite, buf, w2e);
            memcpy(RingBuf1->pBufBase, buf + w2e, count - w2e);
            RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
        }
    }
    else
    {
        memcpy(RingBuf1->pWrite, buf, count);
        RingBuf1->pWrite += count;
    }

}

/**
* fill count number zero bytes to RingBuf1
* @param RingBuf1 ring buffer fill from
* @param count number of bytes need to copy
*/
void RingBuf_fillZero(RingBuf *RingBuf1, int count)
{
    int spaceIHave;
    char *end = RingBuf1->pBufBase + RingBuf1->bufLen;

    // count buffer data I have
    spaceIHave = RingBuf1->bufLen - RingBuf_getDataCount(RingBuf1) - 8;
    //spaceIHave = RingBuf_getDataCount(RingBuf1);

    // if not enough, assert
    ASSERT(spaceIHave >= count);

    if (RingBuf1->pRead <= RingBuf1->pWrite)
    {
        int w2e = end - RingBuf1->pWrite;
        if (count <= w2e)
        {
            memset(RingBuf1->pWrite, 0, sizeof(char)*count);
            RingBuf1->pWrite += count;
            if (RingBuf1->pWrite == end)
            {
                RingBuf1->pWrite = RingBuf1->pBufBase;
            }
        }
        else
        {
            memset(RingBuf1->pWrite, 0, sizeof(char)*w2e);
            memset(RingBuf1->pBufBase, 0, sizeof(char) * (count - w2e));
            RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
        }
    }
    else
    {
        memset(RingBuf1->pWrite, 0, sizeof(char)*count);
        RingBuf1->pWrite += count;
    }

}


/**
* copy ring buffer from RingBufs(source) to RingBuft(target)
* @param RingBuft ring buffer copy to
* @param RingBufs copy from copy from
*/

void RingBuf_copyEmpty(RingBuf *RingBuft, RingBuf *RingBufs)
{
    if (RingBufs->pRead <= RingBufs->pWrite)
    {
        RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, RingBufs->pWrite - RingBufs->pRead);
        //RingBufs->pRead = RingBufs->pWrite;
        // no need to update source read pointer, because it is read to empty
    }
    else
    {
        char *end = RingBufs->pBufBase + RingBufs->bufLen;
        RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, end - RingBufs->pRead);
        RingBuf_copyFromLinear(RingBuft, RingBufs->pBufBase, RingBufs->pWrite - RingBufs->pBufBase);
    }
}


/**
* copy ring buffer from RingBufs(source) to RingBuft(target) with count
* @param RingBuft ring buffer copy to
* @param RingBufs copy from copy from
*/
int RingBuf_copyFromRingBuf(RingBuf *RingBuft, RingBuf *RingBufs, int count)
{
    int cntInRingBufs = RingBuf_getDataCount(RingBufs);
    int freeSpaceInRingBuft = RingBuf_getFreeSpace(RingBuft);
    ASSERT(count <= cntInRingBufs && count <= freeSpaceInRingBuft);

    if (RingBufs->pRead <= RingBufs->pWrite)
    {
        RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, count);
        RingBufs->pRead += count;
        // no need to update source read pointer, because it is read to empty
    }
    else
    {
        char *end = RingBufs->pBufBase + RingBufs->bufLen;
        int cnt2e = end - RingBufs->pRead;
        if (cnt2e >= count)
        {
            RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, count);
            RingBufs->pRead += count;
            if (RingBufs->pRead == end)
            {
                RingBufs->pRead = RingBufs->pBufBase;
            }
        }
        else
        {
            RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, cnt2e);
            RingBuf_copyFromLinear(RingBuft, RingBufs->pBufBase, count - cnt2e);
            RingBufs->pRead = RingBufs->pBufBase + count - cnt2e;
        }
    }
    return count;
}

/**
* write bytes size of count woith value
* @param RingBuf1 ring buffer copy to
* @value value put into buffer
* @count bytes ned to put.
*/
void RingBuf_writeDataValue(RingBuf *RingBuf1, const int value, const int count)
{
    int spaceIHave;

    // count buffer data I have
    spaceIHave = RingBuf1->bufLen - RingBuf_getDataCount(RingBuf1) - 8;

    // if not enough, assert
    ASSERT(spaceIHave >= count);

    if (RingBuf1->pRead <= RingBuf1->pWrite)
    {
        char *end = RingBuf1->pBufBase + RingBuf1->bufLen;
        int w2e = end - RingBuf1->pWrite;
        if (count <= w2e)
        {
            memset(RingBuf1->pWrite, value, count);
            RingBuf1->pWrite += count;
        }
        else
        {
            memset(RingBuf1->pWrite, value, w2e);
            memset(RingBuf1->pBufBase, value, count - w2e);
            RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
        }
    }
    else
    {
        memset(RingBuf1->pWrite, value, count);
        RingBuf1->pWrite += count;
    }

}


//---------end of ringbuffer implemenation------------------------------------------------------


//--------pc dump operation

struct BufferDump
{
    void *pBufBase;
    int ssize_t;
};

#if defined(PC_EMULATION)
HANDLE hPCMDumpThread = NULL;
HANDLE PCMDataNotifyEvent = NULL;
#else
pthread_t hPCMDumpThread = NULL;
pthread_cond_t  PCMDataNotifyEvent;
pthread_mutex_t PCMDataNotifyMutex;
#endif

AudioLock mPCMDumpMutex; // use for PCM buffer dump
KeyedVector<FILE *, Vector<BufferDump *> *> mDumpFileHandleVector; // vector to save current recording client
uint32_t mSleepTime = 2;

void *PCMDumpThread(void *arg);

int AudiocheckAndCreateDirectory(const char *pC)
{
    char tmp[PATH_MAX];
    int i = 0;
    while (*pC)
    {
        tmp[i] = *pC;
        if (*pC == '/' && i)
        {
            tmp[i] = '\0';
            if (access(tmp, F_OK) != 0)
            {
                if (mkdir(tmp, 0770) == -1)
                {
                    ALOGE("AudioDumpPCM: mkdir error! %s\n", (char *)strerror(errno));
                    return -1;
                }
            }
            tmp[i] = '/';
        }
        i++;
        pC++;
    }
    return 0;
}

bool bDumpStreamOutFlg = false;
bool bDumpStreamInFlg = false;

FILE *AudioOpendumpPCMFile(const char *filepath, const char *propty)
{
    char value[PROPERTY_VALUE_MAX];
    int ret;
    property_get(propty, value, "0");
    int bflag = atoi(value);

    if (!bflag)
    {
        if(!strcmp(propty,streamout_propty) && bDumpStreamOutFlg)
            bflag = 1;
        else if(!strcmp(propty,streamin_propty) && bDumpStreamInFlg)
            bflag = 1;
    }

    if (bflag)
    {
        ret = AudiocheckAndCreateDirectory(filepath);
        if (ret < 0)
        {
            ALOGE("AudioOpendumpPCMFile dumpPCMData checkAndCreateDirectory() fail!!!");
        }
        else
        {
            FILE *fp = fopen(filepath, "wb");
            if (fp != NULL)
            {

                mPCMDumpMutex.lock();

                Vector<BufferDump *> *pBD = new Vector<BufferDump *>;
                //ALOGD("AudioOpendumpPCMFile file=%p, pBD=%p",fp, pBD);
                mDumpFileHandleVector.add(fp, pBD);
                /*for (size_t i = 0; i < mDumpFileHandleVector.size() ; i++)
                {
                    ALOGD("AudioOpendumpPCMFile i=%d, handle=%p, %p",i,mDumpFileHandleVector.keyAt(i),mDumpFileHandleVector.valueAt(i));
                }*/

                //ALOGD("AudioOpendumpPCMFile hPCMDumpThread=%p",hPCMDumpThread);
                if (hPCMDumpThread == NULL)
                {
#if defined(PC_EMULATION)
                    PCMDataNotifyEvent = CreateEvent(NULL, TRUE, FALSE, "PCMDataNotifyEvent");
                    hPCMDumpThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PCMDumpThread, NULL, 0, 0);
                    if (hPCMDumpThread == 0)
                    {
                        ALOGE("hPCMDumpThread create fail!!!");
                    }
                    else
                    {
                        ALOGD("hPCMDumpThread=%lu created", hPCMDumpThread);
                    }
#else
                    //create PCM data dump thread here
                    int ret;
                    ret = pthread_create(&hPCMDumpThread, NULL, PCMDumpThread, NULL);
                    if (ret != 0)
                    {
                        ALOGE("hPCMDumpThread create fail!!!");
                    }
                    else
                    {
                        ALOGD("hPCMDumpThread=%lu created", hPCMDumpThread);
                    }
                    ret = pthread_cond_init(&PCMDataNotifyEvent, NULL);
                    if (ret != 0)
                    {
                        ALOGE("PCMDataNotifyEvent create fail!!!");
                    }

                    ret = pthread_mutex_init(&PCMDataNotifyMutex, NULL);
                    if (ret != 0)
                    {
                        ALOGE("PCMDataNotifyMutex create fail!!!");
                    }
#endif
                }
                mPCMDumpMutex.unlock();
                return fp;
            }
            else
            {
                ALOGE("AudioFlinger AudioOpendumpPCMFile %s fail", propty);
            }
        }
    }
    return NULL;
}

void AudioCloseDumpPCMFile(FILE  *file)
{
    if (file != NULL)
    {
        mPCMDumpMutex.lock();
        //ALOGD("AudioCloseDumpPCMFile file=%p, HandleCount=%d",file,mDumpFileHandleVector.size());
        if (mDumpFileHandleVector.size())
        {
            for (size_t i = 0; i < mDumpFileHandleVector.size() ; i++)
            {
                //ALOGD("AudioCloseDumpPCMFile i=%d, handle=%p",i,mDumpFileHandleVector.keyAt(i));
                if (file == mDumpFileHandleVector.keyAt(i))
                {
                    FILE *key = mDumpFileHandleVector.keyAt(i);
                    while ((* mDumpFileHandleVector.valueAt(i)).size() != 0)
                    {
                        free((* mDumpFileHandleVector.valueAt(i))[0]->pBufBase);
                        delete(* mDumpFileHandleVector.valueAt(i))[0];
                        (* mDumpFileHandleVector.valueAt(i)).removeAt(0);
                    }
                    delete mDumpFileHandleVector.valueAt(i);
                    mDumpFileHandleVector.removeItem(key);
                }
            }
        }

        mPCMDumpMutex.unlock();
        fclose(file);
        file = NULL;
    }
    else
    {
        ALOGE("AudioCloseDumpPCMFile file== NULL");
    }
}

void AudioDumpPCMData(void *buffer , uint32_t bytes, FILE  *file)
{
    if (hPCMDumpThread != NULL)
    {
        mPCMDumpMutex.lock();
        if (mDumpFileHandleVector.size())
        {
            for (size_t i = 0; i < mDumpFileHandleVector.size() ; i++)
            {
                if (file == mDumpFileHandleVector.keyAt(i))
                {
                    FILE *key = mDumpFileHandleVector.keyAt(i);
                    //ALOGD("AudioDumpPCMData find!! i=%d, key=%p, value=%p",i,mDumpFileHandleVector.keyAt(i),mDumpFileHandleVector.valueAt(i));
                    BufferDump *newInBuffer = new BufferDump;
                    newInBuffer->pBufBase = (short *) malloc(bytes);
                    memcpy(newInBuffer->pBufBase, buffer, bytes);
                    newInBuffer->ssize_t = bytes;
                    (* mDumpFileHandleVector.valueAt(i)).add(newInBuffer);

                    if (mSleepTime == -1) //need to send event
                    {
#if defined(PC_EMULATION)
                        SetEvent(PCMDataNotifyEvent);
#else
                        pthread_mutex_lock(&PCMDataNotifyMutex);
                        pthread_cond_signal(&PCMDataNotifyEvent);
                        pthread_mutex_unlock(&PCMDataNotifyMutex);
#endif
                    }
                }
            }
        }
        mPCMDumpMutex.unlock();
    }
    else    //if no dump thread, just write the data
    {
        fwrite((void *)buffer, sizeof(char), bytes, file);
    }

}

void *PCMDumpThread(void *arg)
{
    ALOGD("PCMDumpThread");
    bool bHasdata = false;
    int iNoDataCount = 0;
    while (1)
    {
        mPCMDumpMutex.lock();
        bHasdata = false;
        //ALOGV( "PCMDumpThread mDumpFileHandleVector.size()=%d",mDumpFileHandleVector.size());
        for (size_t i = 0; i < mDumpFileHandleVector.size() ; i++)
        {

            if ((* mDumpFileHandleVector.valueAt(i)).size() > 0)
            {
                bHasdata = true;
                fwrite((* mDumpFileHandleVector.valueAt(i))[0]->pBufBase, (* mDumpFileHandleVector.valueAt(i))[0]->ssize_t, 1, mDumpFileHandleVector.keyAt(i));
                free((* mDumpFileHandleVector.valueAt(i))[0]->pBufBase);
                delete(* mDumpFileHandleVector.valueAt(i))[0];
                (* mDumpFileHandleVector.valueAt(i)).removeAt(0);
            }
        }
        mPCMDumpMutex.unlock();
        if (!bHasdata)
        {
            iNoDataCount++;
            if (iNoDataCount >= 1000)
            {
                mSleepTime = -1;
                ALOGD("PCMDumpThread, wait for new data dump\n");
#if defined(PC_EMULATION)
                WaitForSingleObject(PCMDataNotifyEvent, INFINITE);
                ResetEvent(PCMDataNotifyEvent);
#else
                pthread_mutex_lock(&PCMDataNotifyMutex);
                pthread_cond_wait(&PCMDataNotifyEvent, &PCMDataNotifyMutex);
                pthread_mutex_unlock(&PCMDataNotifyMutex);
                ALOGD("PCMDumpThread, PCM data dump again\n");
#endif
            }
            else
            {
                mSleepTime = 10;
                usleep(mSleepTime * 1000);
            }
        }
        else
        {
            iNoDataCount = 0;
            mSleepTime = 2;
            usleep(mSleepTime * 1000);
        }
        /*
                if(mDumpFileHandleVector.size()==0)
                {
                    ALOGD( "PCMDumpThread exit, no dump handle real");
                    hPCMDumpThread = NULL;
                    pthread_exit(NULL);
                    return 0;
                }*/

    }

    ALOGD("PCMDumpThread exit hPCMDumpThread=%lu", hPCMDumpThread);
    hPCMDumpThread = NULL;
    pthread_exit(NULL);
    return 0;
}

#define CVSD_LOOPBACK_BUFFER_SIZE (180*1000)//BTSCO_CVSD_RX_FRAME*SCO_RX_PCM8K_BUF_SIZE * 10
static uint8_t cvsd_temp_buffer[CVSD_LOOPBACK_BUFFER_SIZE]; //temp buf only for dump to file
static uint32_t cvsd_temp_w = 0;
static uint32_t cvsd_temp_r = 0;
const static uint32_t cvsd_temp_size = CVSD_LOOPBACK_BUFFER_SIZE;

void CVSDLoopbackGetWriteBuffer(uint8_t **buffer, uint32_t *buf_len)  // in bytes
{
    int32_t count;

    if (cvsd_temp_r > cvsd_temp_w)
    {
        count = cvsd_temp_r - cvsd_temp_w - 8;
    }
    else
    {
        count = cvsd_temp_size + cvsd_temp_r - cvsd_temp_w - 8;
    }

    *buffer = (uint8_t *)&cvsd_temp_buffer[cvsd_temp_w];
    *buf_len = (count > 0) ? count : 0;
    ALOGD("BT_SW_CVSD CODEC LOOPBACK record thread: CVSDLoopbackGetWriteBuffer: buf_len: %d", *buf_len);
}

void CVSDLoopbackGetReadBuffer(uint8_t **buffer, uint32_t *buf_len)  // in bytes
{
    int32_t count;

    if (cvsd_temp_w >= cvsd_temp_r)
    {
        count = cvsd_temp_w - cvsd_temp_r;
    }
    else
    {
        count = cvsd_temp_size - cvsd_temp_r;
    }

    *buffer = (uint8_t *)&cvsd_temp_buffer[cvsd_temp_r];
    *buf_len = count;
    ALOGD("BT_SW_CVSD CODEC LOOPBACK record thread: CVSDLoopbackGetReadBuffer: buf_len: %d", count);
}

void CVSDLoopbackReadDataDone(uint32_t len) // in bytes
{
    cvsd_temp_r += len;
    if (cvsd_temp_r == cvsd_temp_size)
    {
        cvsd_temp_r = 0;
    }
    ALOGD("BT_SW_CVSD CODEC LOOPBACK record thread: CVSDLoopbackReadDataDone: len: %d", len);
}

void CVSDLoopbackWriteDataDone(uint32_t len) // in bytes
{
    cvsd_temp_w += len;
    if (cvsd_temp_w == cvsd_temp_size)
    {
        cvsd_temp_w = 0;
    }
    ALOGD("BT_SW_CVSD CODEC LOOPBACK record thread: CVSDLoopbackWriteDataDone: len: %d", len);
}

void CVSDLoopbackResetBuffer(void) // in bytes
{
    memset(cvsd_temp_buffer, 0, CVSD_LOOPBACK_BUFFER_SIZE);
    cvsd_temp_w = 180 * 100; //if 0, deadlock
    cvsd_temp_r = 0;
    ALOGD("BT_SW_CVSD CODEC LOOPBACK record thread: CVSDLoopbackResetBuffer");
}

int32_t CVSDLoopbackGetFreeSpace(void)
{
    int32_t count;

    if (cvsd_temp_r > cvsd_temp_w)
    {
        count = cvsd_temp_r - cvsd_temp_w - 8;
    }
    else
    {
        count = cvsd_temp_size + cvsd_temp_r - cvsd_temp_w - 8;
    }

    return (count > 0) ? count : 0; // free size in byte
}

int32_t CVSDLoopbackGetDataCount(void)
{
    return (cvsd_temp_size - CVSDLoopbackGetFreeSpace() - 8);
}

const char PROPERTY_KEY_DMIC_ON[PROPERTY_KEY_MAX]  = "persist.af.feature.dmic";
const char PROPERTY_KEY_2IN1SPK_ON[PROPERTY_KEY_MAX] = "persist.af.feature.2in1spk";
const char PROPERTY_KEY_VIBSPK_ON[PROPERTY_KEY_MAX] = "persist.af.feature.vibspk";


bool IsAudioSupportFeature(int dFeatureOption)
{
    bool bSupportFlg = false;
    char stForFeatureUsage[PROPERTY_VALUE_MAX];

    switch (dFeatureOption)
    {
        case AUDIO_SUPPORT_DMIC:
        {
#ifdef MTK_DIGITAL_MIC_SUPPORT
            property_get(PROPERTY_KEY_DMIC_ON, stForFeatureUsage, "1"); //"1": default on
#else
            property_get(PROPERTY_KEY_DMIC_ON, stForFeatureUsage, "0"); //"0": default off
#endif
            bSupportFlg = (stForFeatureUsage[0] == '0') ? false : true;
            //ALOGD("IsAudioSupportFeature AUDIO_SUPPORT_DMIC [%d]\n",bSupportFlg);

            break;
        }
        case AUDIO_SUPPORT_2IN1_SPEAKER:
        {
#ifdef USING_2IN1_SPEAKER
            property_get(PROPERTY_KEY_2IN1SPK_ON, stForFeatureUsage, "1"); //"1": default on
#else
            property_get(PROPERTY_KEY_2IN1SPK_ON, stForFeatureUsage, "0"); //"0": default off
#endif
            bSupportFlg = (stForFeatureUsage[0] == '0') ? false : true;
            //ALOGD("IsAudioSupportFeature AUDIO_SUPPORT_2IN1_SPEAKER [%d]\n",bSupportFlg);

            break;
        }
        case AUDIO_SUPPORT_VIBRATION_SPEAKER:
        {
#ifdef MTK_VIBSPK_SUPPORT
            property_get(PROPERTY_KEY_VIBSPK_ON, stForFeatureUsage, "1"); //"1": default on
#else
            property_get(PROPERTY_KEY_VIBSPK_ON, stForFeatureUsage, "0"); //"0": default off
#endif
            bSupportFlg = (stForFeatureUsage[0] == '0') ? false : true;
            //ALOGD("IsAudioSupportFeature AUDIO_SUPPORT_VIBRATION_SPEAKER [%d]\n",bSupportFlg);

            break;
        }
        case AUDIO_SUPPORT_EXTERNAL_BUILTIN_MIC:
        {
#ifdef MTK_EXTERNAL_BUILTIN_MIC_SUPPORT
            bSupportFlg = true;
#else
            bSupportFlg = false;
#endif
            break;
        }
        case AUDIO_SUPPORT_EXTERNAL_ECHO_REFERENCE:
        {
#ifdef MTK_EXTERNAL_SPEAKER_DAC_SUPPORT
            bSupportFlg = true;
#else
            bSupportFlg = false;
#endif
            break;
        }
        default:
            break;
    }

    return bSupportFlg;
}


timespec GetSystemTime(bool print)
{
    struct timespec systemtime;
    int rc;
    rc = clock_gettime(CLOCK_MONOTONIC, &systemtime);
    if (rc != 0)
    {
        systemtime.tv_sec  = 0;
        systemtime.tv_nsec = 0;
        ALOGD("%s() clock_gettime error", __FUNCTION__);
    }
    if (print == true)
    {
        ALOGD("%s(), sec %ld nsec %ld", __FUNCTION__, systemtime.tv_sec, systemtime.tv_nsec);
    }

    return systemtime;
}

static const char *g_phone_mic_propty = "persist.rm.debug.phonemic";
static const char *g_headset_mic_propty = "persist.rm.debug.headsetmic";

uint32_t GetMicDeviceMode(uint32_t mic_category) //0: phonemic, 1: headsetmic
{
    char value[PROPERTY_VALUE_MAX];
    int ret, bflag;
    uint32_t mPhoneMicMode, mHeadsetMicMode;

    if (mic_category == 0)
    {
#ifdef PHONE_MIC_MODE //defined in audio_custom_exp.h
        mPhoneMicMode = PHONE_MIC_MODE;
        ALOGD("PHONE_MIC_MODE defined!, mPhoneMicMode = %d", mPhoneMicMode);
#else
        mPhoneMicMode = AUDIO_MIC_MODE_DCC;
#endif
        // control by setprop
        property_get(g_phone_mic_propty, value, "0");
        bflag = atoi(value);
        if (bflag != 0)
        {
            mPhoneMicMode = bflag;
            ALOGD("mPhoneMicMode getprop, mPhoneMicMode = %d", mPhoneMicMode);
        }
        return mPhoneMicMode;
    }
    else if (mic_category == 1)
    {
#ifdef HEADSET_MIC_MODE //defined in audio_custom_exp.h
        mHeadsetMicMode = HEADSET_MIC_MODE;
        ALOGD("HEADSET_MIC_MODE defined!, mHeadsetMicMode = %d", mHeadsetMicMode);
#else
        mHeadsetMicMode = AUDIO_MIC_MODE_DCC;
#endif
        // control by setprop
        property_get(g_headset_mic_propty, value, "0");
        bflag = atoi(value);
        if (bflag != 0)
        {
            mHeadsetMicMode = bflag;
            ALOGD("mHeadsetMicMode getprop, mHeadsetMicMode = %d", mHeadsetMicMode);
        }
        return mHeadsetMicMode;
    }
    else
    {
        ALOGE("%s() wrong mic_category!!!", __FUNCTION__);
        return 0;
    }
}

unsigned int FormatTransfer(	int SourceFormat, int TargetFormat, void *Buffer, unsigned int mReadBufferSize)
{
    unsigned mReformatSize =0;
    int *srcbuffer = (int*)Buffer;
    short *dstbuffer = (short*)Buffer;
    if(SourceFormat == PCM_FORMAT_S32_LE && TargetFormat == PCM_FORMAT_S16_LE)
    {
    	short temp = 0;
    	while(mReadBufferSize)
    	{
    		temp = (short)((*srcbuffer)>>8);
    		*dstbuffer = temp;
    		srcbuffer++;
    		dstbuffer++;
    		mReadBufferSize-= sizeof(int);
    		mReformatSize+= sizeof(short);
    	}
    }
    else
    {
    	mReformatSize = mReadBufferSize;
    }
    return mReformatSize;
}

#define FACTORY_BOOT 4
#define ATE_FACTORY_BOOT 6
#define BOOTMODE_PATH "/sys/class/BOOT/BOOT/boot/boot_mode"

int readSys_int(char const * path)
{
    int fd;

    if (path == NULL)
        return -1;

    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buffer[20];
        int amt = read(fd, buffer, sizeof(int));
        close(fd);
        return amt == -1 ? -errno : atoi(buffer);
    }
    ALOGE("write_int failed to open %s\n", path);
    return -errno;
}

int InFactoryMode()
{
    int bootMode;
    int ret = false;

    bootMode = readSys_int(BOOTMODE_PATH);
    ALOGD("bootMode = %d",bootMode);
    if(FACTORY_BOOT== bootMode)
    {
        ALOGD("Factory mode boot!\n");
        ret = true;
    }
    else if(ATE_FACTORY_BOOT== bootMode)
    {
        ALOGD("ATE Factory mode boot!\n");
        ret = true;
    }
    else
    {
        ret = false;
        ALOGD("Unsupported factory mode!\n");
    }
    return ret;
}

int In64bitsProcess()
{
    char *platform = (char *)getauxval(AT_PLATFORM);
    //ALOGD("%s platform = %s",__FUNCTION__,platform);
    if(strcmp(platform,"aarch64") == 0)
    {
        return true;
    }
    return false;
}

#ifdef MTK_PERFSERVICE_SUPPORT


static void *gPerfServiceLib = NULL;

typedef int  (*PerfServiceNative_userRegBigLittle_t)(
    int scn_core_big, int scn_freq_big, int scn_core_little, int scn_freq_little);

typedef void (*PerfServiceNative_userRegScnConfig_t)(
    int handle, int cmd, int param_1, int param_2, int param_3, int param_4);

typedef void (*PerfServiceNative_userUnregScn_t)(int handle);

typedef void (*PerfServiceNative_userEnable_t)(int handle);

typedef void (*PerfServiceNative_userDisable_t)(int handle);

static PerfServiceNative_userRegBigLittle_t Audio_PerfServiceNative_userRegBigLittle = NULL;
static PerfServiceNative_userRegScnConfig_t Audio_PerfServiceNative_userRegScnConfig = NULL;
static PerfServiceNative_userUnregScn_t     Audio_PerfServiceNative_userUnregScn = NULL;
static PerfServiceNative_userEnable_t       Audio_PerfServiceNative_userEnable = NULL;
static PerfServiceNative_userDisable_t      Audio_PerfServiceNative_userDisable = NULL;

void initPerfServiceScn()
{
    static bool initFlag;

    ALOGD("%s+", __FUNCTION__);

    if (!initFlag)
    {
        initFlag = true;
        if (access(AUDIO_PERFSERVICE_LIB_PATH, R_OK) == 0) {
            gPerfServiceLib = dlopen(AUDIO_PERFSERVICE_LIB_PATH, RTLD_NOW);
        } else {
            ALOGE("%s filepath %s doesn't exist", __FUNCTION__, AUDIO_PERFSERVICE_LIB_PATH);
            return;
        }

        if (gPerfServiceLib == NULL)
        {
            ALOGE("%s gPerfServiceLib is NULL", __FUNCTION__);
        }
        else
        {
            Audio_PerfServiceNative_userRegBigLittle =
                (PerfServiceNative_userRegBigLittle_t)dlsym(gPerfServiceLib, "PerfServiceNative_userRegBigLittle");
            if (Audio_PerfServiceNative_userRegBigLittle == NULL)
            {
                ALOGE("%s Audio_PerfServiceNative_userRegBigLittle err", __FUNCTION__);
            }

            Audio_PerfServiceNative_userRegScnConfig =
                (PerfServiceNative_userRegScnConfig_t)dlsym(gPerfServiceLib, "PerfServiceNative_userRegScnConfig");
            if (Audio_PerfServiceNative_userRegScnConfig == NULL)
            {
                ALOGE("%s Audio_PerfServiceNative_userRegScnConfig err", __FUNCTION__);
            }

            Audio_PerfServiceNative_userUnregScn =
                (PerfServiceNative_userUnregScn_t)dlsym(gPerfServiceLib, "PerfServiceNative_userUnregScn");
            if (Audio_PerfServiceNative_userUnregScn == NULL)
            {
                ALOGE("%s Audio_PerfServiceNative_userUnregScn err", __FUNCTION__);
            }

            Audio_PerfServiceNative_userEnable =
                (PerfServiceNative_userEnable_t)dlsym(gPerfServiceLib, "PerfServiceNative_userEnable");
            if (Audio_PerfServiceNative_userEnable == NULL)
            {
                ALOGE("%s Audio_PerfServiceNative_userEnable err", __FUNCTION__);
            }

            Audio_PerfServiceNative_userDisable =
                (PerfServiceNative_userDisable_t)dlsym(gPerfServiceLib, "PerfServiceNative_userDisable");
            if (Audio_PerfServiceNative_userDisable == NULL)
            {
                ALOGE("%s Audio_PerfServiceNative_userDisable err", __FUNCTION__);
            }
        }
    }
}

void regPerfServiceScn(int *mPerfServiceHandle, const int scn_freq_little)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("audio.perfservice.bypass", value, "0");
    int bflag = atoi(value);
    if (bflag)
        return;

    initPerfServiceScn();

    if (Audio_PerfServiceNative_userRegBigLittle == NULL || Audio_PerfServiceNative_userRegScnConfig == NULL) {
        ALOGE("Audio_PerfServiceNative_userRegBigLittle == NULL || Audio_PerfServiceNative_userRegBigLittle == NULL");
        return;
    }

    int handler = Audio_PerfServiceNative_userRegBigLittle(0, 0, 0, scn_freq_little);
    ALOGD("%s+, handler %d, CPU speed %d", __FUNCTION__, handler, scn_freq_little);
    if (handler >= 0)
    {
        Audio_PerfServiceNative_userRegScnConfig(handler, CMD_SET_SCREEN_OFF_STATE, SCREEN_OFF_ENABLE, 0, 0, 0);
        ALOGD("%s-, perfService register !!", __FUNCTION__);
        *mPerfServiceHandle = handler;
    }
    else
    {
        ALOGD("%s-, perfService register fail !!", __FUNCTION__);
        *mPerfServiceHandle = -1;
    }
}

void unregPerfServiceScn(int *mPerfServiceHandle)
{
    //ALOGD("%s+, handler %d", __FUNCTION__, *mPerfServiceHandle);

    if (Audio_PerfServiceNative_userUnregScn == NULL) {
        ALOGE("Audio_PerfServiceNative_userUnregScn == NULL");
        return;
    }

    if (*mPerfServiceHandle >= 0)
    {
        Audio_PerfServiceNative_userUnregScn(*mPerfServiceHandle);
        *mPerfServiceHandle = -1;
    }

    //ALOGD("%s-, handler %d", __FUNCTION__, *mPerfServiceHandle);
}

void setPerfServiceScn(const int mPerfServiceHandle, const int scn_freq_little)
{
    //ALOGD("%s+, handler %d, scn_freq_little %d", __FUNCTION__, mPerfServiceHandle, scn_freq_little);

    if (Audio_PerfServiceNative_userRegScnConfig == NULL) {
        ALOGE("Audio_PerfServiceNative_userRegScnConfig == NULL");
        return;
    }

    if (mPerfServiceHandle >= 0)
    {
        Audio_PerfServiceNative_userRegScnConfig(mPerfServiceHandle, CMD_SET_CPU_FREQ_BIG_LITTLE_MIN , 0, scn_freq_little, 0, 0);
        Audio_PerfServiceNative_userRegScnConfig(mPerfServiceHandle, CMD_SET_CPU_FREQ_BIG_LITTLE_MAX , 0, scn_freq_little, 0, 0);
    }

    //ALOGD("%s-, handler %d", __FUNCTION__, mPerfServiceHandle);
}

void enablePerfServiceScn(const int mPerfServiceHandle)
{
    //ALOGD("%s+, handler %d", __FUNCTION__, mPerfServiceHandle);

    if (Audio_PerfServiceNative_userEnable == NULL) {
        ALOGE("Audio_PerfServiceNative_userEnable == NULL");
        return;
    }

    if (mPerfServiceHandle >= 0)
    {
        Audio_PerfServiceNative_userEnable(mPerfServiceHandle);
    }

    //ALOGD("%s-, handler %d", __FUNCTION__, mPerfServiceHandle);
}

void disablePerfServiceScn(const int mPerfServiceHandle)
{
    //ALOGD("%s+, handler %d", __FUNCTION__, mPerfServiceHandle);

    if (Audio_PerfServiceNative_userDisable == NULL) {
        ALOGE("Audio_PerfServiceNative_userDisable == NULL");
        return;
    }

    if (mPerfServiceHandle >= 0)
    {
        Audio_PerfServiceNative_userDisable(mPerfServiceHandle);
    }

    //ALOGD("%s-, handler %d", __FUNCTION__, mPerfServiceHandle);
}

#endif

inline void *openAudioRelatedLib(const char *filepath)
{
    if (filepath == NULL) {
        ALOGE("%s null input parameter", __FUNCTION__);
        return NULL;
    } else {
        if (access(filepath, R_OK) == 0) {
            return dlopen(filepath, RTLD_NOW);
        } else {
            ALOGE("%s filepath %s doesn't exist", __FUNCTION__, filepath);
            return NULL;
        }
    }
}
inline bool openAudioComponentEngine(void)
{
    if (g_AudioComponentEngineHandle == NULL) {
        g_CreateMtkAudioBitConverter = NULL;
        g_CreateMtkAudioSrc = NULL;
        g_CreateMtkAudioLoud = NULL;
        g_DestroyMtkAudioBitConverter = NULL;
        g_DestroyMtkAudioSrc = NULL;
        g_DestroyMtkAudioLoud = NULL;
        g_AudioComponentEngineHandle = openAudioRelatedLib(AUDIO_COMPONENT_ENGINE_LIB_VENDOR_PATH);
        if (g_AudioComponentEngineHandle == NULL) {
                g_AudioComponentEngineHandle  = openAudioRelatedLib(AUDIO_COMPONENT_ENGINE_LIB_PATH);
                return (g_AudioComponentEngineHandle == NULL) ? false : true;
        }
    }
    return true;
}

inline void closeAudioComponentEngine(void)
{
    if (g_AudioComponentEngineHandle != NULL) {
        dlclose(g_AudioComponentEngineHandle);
        g_AudioComponentEngineHandle = NULL;
        g_CreateMtkAudioBitConverter = NULL;
        g_CreateMtkAudioSrc = NULL;
        g_CreateMtkAudioLoud = NULL;
        g_DestroyMtkAudioBitConverter = NULL;
        g_DestroyMtkAudioSrc = NULL;
        g_DestroyMtkAudioLoud = NULL;
    }
}

MtkAudioBitConverterBase *newMtkAudioBitConverter(uint32_t sampling_rate, uint32_t channel_num, BCV_PCM_FORMAT format)
{
    if (!openAudioComponentEngine())
    {
        return NULL;
    }

    if (g_CreateMtkAudioBitConverter == NULL)
    {
        g_CreateMtkAudioBitConverter = (create_AudioBitConverter*)dlsym(g_AudioComponentEngineHandle, "createMtkAudioBitConverter");
        const char* dlsym_error1 = dlerror();
        if (g_CreateMtkAudioBitConverter == NULL) {
            ALOGE("Error -dlsym createMtkAudioBitConverter fail");
            closeAudioComponentEngine();
            return NULL;
        }
    }
    ALOGV("%p g_CreateMtkAudioBitConverter %p", g_AudioComponentEngineHandle, g_CreateMtkAudioBitConverter);
    return g_CreateMtkAudioBitConverter(sampling_rate, channel_num, format);
}

MtkAudioSrcBase *newMtkAudioSrc(uint32_t input_SR, uint32_t input_channel_num, uint32_t output_SR, uint32_t output_channel_num, SRC_PCM_FORMAT format)
{
    if (!openAudioComponentEngine())
    {
        return NULL;
    }

    if (g_CreateMtkAudioSrc == NULL)
    {
        g_CreateMtkAudioSrc = (create_AudioSrc*)dlsym(g_AudioComponentEngineHandle, "createMtkAudioSrc");
        const char* dlsym_error1 = dlerror();
        if (g_CreateMtkAudioSrc == NULL) {
            ALOGE("Error -dlsym createMtkAudioSrc fail");
            closeAudioComponentEngine();
            return NULL;
        }
    }
    ALOGV("%p g_CreateMtkAudioSrc %p", g_AudioComponentEngineHandle, g_CreateMtkAudioSrc);
    return g_CreateMtkAudioSrc(input_SR, input_channel_num, output_SR, output_channel_num, format);
}

MtkAudioLoudBase *newMtkAudioLoud(uint32_t eFLTtype)
{
    if (!openAudioComponentEngine())
    {
        return NULL;
    }

    if (g_CreateMtkAudioLoud == NULL)
    {
        g_CreateMtkAudioLoud = (create_AudioLoud*)dlsym(g_AudioComponentEngineHandle, "createMtkAudioLoud");
        const char* dlsym_error1 = dlerror();
        if (g_CreateMtkAudioLoud == NULL) {
            ALOGE("Error -dlsym createMtkAudioLoud fail");
            closeAudioComponentEngine();
            return NULL;
        }
    }
    ALOGV("%p g_CreateMtkAudioLoud %p", g_AudioComponentEngineHandle, g_CreateMtkAudioLoud);
    return g_CreateMtkAudioLoud(eFLTtype);
}

void deleteMtkAudioBitConverter(MtkAudioBitConverterBase *pObject)
{
    if (!openAudioComponentEngine())
    {
        return;
    }

    if (g_DestroyMtkAudioBitConverter == NULL)
    {
        g_DestroyMtkAudioBitConverter = (destroy_AudioBitConverter*)dlsym(g_AudioComponentEngineHandle, "destroyMtkAudioBitConverter");
        const char* dlsym_error1 = dlerror();
        if (g_DestroyMtkAudioBitConverter == NULL) {
            ALOGE("Error -dlsym destroyMtkAudioBitConverter fail");
            closeAudioComponentEngine();
            return;
        }
    }
    ALOGV("%p g_DestroyMtkAudioBitConverter %p", g_AudioComponentEngineHandle, g_DestroyMtkAudioBitConverter);
    g_DestroyMtkAudioBitConverter(pObject);
    return;
}

void deleteMtkAudioSrc(MtkAudioSrcBase *pObject)
{
    if (!openAudioComponentEngine())
    {
        return;
    }

    if (g_DestroyMtkAudioSrc == NULL)
    {
        g_DestroyMtkAudioSrc = (destroy_AudioSrc*)dlsym(g_AudioComponentEngineHandle, "destroyMtkAudioSrc");
        const char* dlsym_error1 = dlerror();
        if (g_DestroyMtkAudioSrc == NULL) {
            ALOGE("Error -dlsym destroyMtkAudioSrc fail");
            closeAudioComponentEngine();
            return;
        }
    }
    ALOGV("%p g_DestroyMtkAudioSrc %p", g_AudioComponentEngineHandle, g_DestroyMtkAudioSrc);
    g_DestroyMtkAudioSrc(pObject);
}

void deleteMtkAudioLoud(MtkAudioLoudBase *pObject)
{
    if (!openAudioComponentEngine())
    {
        return;
    }

    if (g_DestroyMtkAudioLoud == NULL)
    {
        g_DestroyMtkAudioLoud = (destroy_AudioLoud*)dlsym(g_AudioComponentEngineHandle, "destroyMtkAudioLoud");
        const char* dlsym_error1 = dlerror();
        if (g_DestroyMtkAudioLoud == NULL) {
            ALOGE("Error -dlsym destroyMtkAudioLoud fail");
            closeAudioComponentEngine();
            return;
        }
    }
    ALOGV("%p g_DestroyMtkAudioLoud %p", g_AudioComponentEngineHandle, g_DestroyMtkAudioLoud);
    g_DestroyMtkAudioLoud(pObject);
    return;
}

inline bool openAudioCompensationFilter(void)
{
    if (g_AudioCompensationFilterHandle == NULL) {
        g_setAudioCompFltCustParamFrom = NULL;
        g_getAudioCompFltCustParamFrom = NULL;
        g_AudioCompensationFilterHandle = openAudioRelatedLib(AUDIO_COMPENSATION_FILTER_LIB_VENDOR_PATH);
        if (g_AudioCompensationFilterHandle == NULL) {
                g_AudioCompensationFilterHandle  = openAudioRelatedLib(AUDIO_COMPENSATION_FILTER_LIB_PATH);
                return (g_AudioCompensationFilterHandle == NULL) ? false : true;
        }
    }
    return true;
}

inline void closeAudioCompensationFilter(void)
{
    if (g_AudioCompensationFilterHandle != NULL) {
        dlclose(g_AudioCompensationFilterHandle);
        g_AudioCompensationFilterHandle = NULL;
        g_setAudioCompFltCustParamFrom = NULL;
        g_getAudioCompFltCustParamFrom = NULL;
    }
}

int setAudioCompFltCustParam(AudioCompFltType_t eFLTtype, AUDIO_ACF_CUSTOM_PARAM_STRUCT *audioParam)
{
    if (!openAudioCompensationFilter()) {
        return 0;
    } else {
        if (g_setAudioCompFltCustParamFrom == NULL) {
            g_setAudioCompFltCustParamFrom = (accessAudioCompFltCustParam*)dlsym(g_AudioCompensationFilterHandle, "setAudioCompFltCustParamToStorage");
            const char* dlsym_error1 = dlerror();
            if (g_setAudioCompFltCustParamFrom == NULL) {
                closeAudioCompensationFilter();
                ALOGE("Error -dlsym setAudioCompFltCustParam fail");
                return 0;
            }
        }
    }
    return g_setAudioCompFltCustParamFrom(eFLTtype, audioParam);
}

int getAudioCompFltCustParam(AudioCompFltType_t eFLTtype, AUDIO_ACF_CUSTOM_PARAM_STRUCT *audioParam)
{
    if (!openAudioCompensationFilter()) {
        return 0;
    } else {
        if (g_getAudioCompFltCustParamFrom == NULL) {
            g_getAudioCompFltCustParamFrom = (accessAudioCompFltCustParam*)dlsym(g_AudioCompensationFilterHandle, "getAudioCompFltCustParamFromStorage");
            const char* dlsym_error1 = dlerror();
            if (g_getAudioCompFltCustParamFrom == NULL) {
                closeAudioCompensationFilter();
                ALOGE("Error -dlsym getAudioCompFltCustParam fail");
                return 0;
            }
        }
    }
    return g_getAudioCompFltCustParamFrom(eFLTtype, audioParam);
}

}
