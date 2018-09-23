#define LOG_TAG "AudioALSACaptureDataProviderNormal"

#include "AudioALSACaptureDataProviderNormal.h"

#include <pthread.h>
#include <sys/prctl.h>

#include "AudioALSADriverUtility.h"
#include "AudioType.h"
#include "AudioSpeechEnhanceInfo.h"

#if !defined(MTK_BASIC_PACKAGE)
#include <audio_utils/pulse.h>
#endif


#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)

namespace android
{


/*==============================================================================
 *                     Constant
 *============================================================================*/


static uint32_t kReadBufferSize = 0;
static const uint32_t kDCRReadBufferSize = 0x2EE00; //48K\stereo\1s data , calculate 1time/sec

//static FILE *pDCCalFile = NULL;
static bool btempDebug = false;

/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderNormal *AudioALSACaptureDataProviderNormal::mAudioALSACaptureDataProviderNormal = NULL;
AudioALSACaptureDataProviderNormal *AudioALSACaptureDataProviderNormal::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderNormal == NULL)
    {
        mAudioALSACaptureDataProviderNormal = new AudioALSACaptureDataProviderNormal();
    }
    ASSERT(mAudioALSACaptureDataProviderNormal != NULL);
    return mAudioALSACaptureDataProviderNormal;
}

AudioALSACaptureDataProviderNormal::AudioALSACaptureDataProviderNormal():
    hReadThread(0),
    hDCCalThread(0),
    mCaptureDropSize(0),
    mDCCalEnable(false),
    mDCCalBufferFull(false),
    mDCCalDumpFile(NULL)
{
    ALOGD("%s()", __FUNCTION__);
    memset(&mNewtime, 0, sizeof(mNewtime));
    memset(&mOldtime, 0, sizeof(mOldtime));
    memset(timerec, 0, sizeof(timerec));
    memset((void *)&mDCCalBuffer, 0, sizeof(mDCCalBuffer));
}

AudioALSACaptureDataProviderNormal::~AudioALSACaptureDataProviderNormal()
{
    ALOGD("%s()", __FUNCTION__);
}


status_t AudioALSACaptureDataProviderNormal::open()
{
    ALOGD("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);
    AudioAutoTimeoutLock _l2(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    ASSERT(mEnable == false);

    int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmUl1Capture);
    int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmUl1Capture);
    ALOGD("%s cardindex = %d  pcmindex = %d", __FUNCTION__, cardindex, pcmindex);

    struct pcm_params *params;
    params = pcm_params_get(cardindex, pcmindex,  PCM_IN);
    if (params == NULL)
    {
        ALOGD("Device does not exist.\n");
    }
    unsigned int buffersizemax = pcm_params_get_max(params, PCM_PARAM_BUFFER_BYTES);
    ALOGD("buffersizemax = %d", buffersizemax);
    pcm_params_free(params);

    bool bHifiRecord = AudioSpeechEnhanceInfo::getInstance()->GetHifiRecord();

    ALOGD("bHifiRecord = %d", bHifiRecord);
    //debug++
    btempDebug = AudioSpeechEnhanceInfo::getInstance()->GetDebugStatus();
    ALOGD("btempDebug = %d", btempDebug);
    //debug--
    mConfig.channels = 2;
    mConfig.period_count = 4;
    mConfig.rate = 48000;
    uint32_t latency = getLatencyTime();

    if (bHifiRecord == true)
    {
       mConfig.rate = 96000;
    }

#ifdef MTK_DMIC_SR_LIMIT
    mConfig.rate = 32000;
#endif

    if (latency == UPLINK_LOW_LATENCY_MS)
    {
       mConfig.period_count = 8; // 2*(20ms/5ms);
    }

#ifdef RECORD_INPUT_24BITS
     mConfig.format = PCM_FORMAT_S32_LE;
     mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_8_24_BIT;
#else
     mConfig.format = PCM_FORMAT_S16_LE;
     mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
#endif

    kReadBufferSize = (((uint32_t)(mConfig.rate / 1000 * latency * mConfig.channels * (pcm_format_to_bits(mConfig.format) / 8))) & 0xFFFFFFC0); // (UL)48K\5ms data\stereo\4byte\(Align64byte)

#ifdef UPLINK_LOW_LATENCY
     mConfig.period_size = kReadBufferSize / mConfig.channels / (pcm_format_to_bits(mConfig.format) / 8);  //period size will impact the interrupt interval
#else
     mConfig.period_size = (buffersizemax / mConfig.channels / (pcm_format_to_bits(mConfig.format) / 8) / mConfig.period_count;
#endif


    ALOGD("%s(), format = %d, channels=%d, rate=%d, period_size=%d, period_count=%d,latency=%d kReadBufferSize=%d", __FUNCTION__,
    mConfig.format, mConfig.channels, mConfig.rate,mConfig.period_size,mConfig.period_count,latency,kReadBufferSize);
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;

    mCaptureDataProviderType = CAPTURE_PROVIDER_NORMAL;
    mCaptureDropSize = 0;

#ifdef MTK_VOW_DCCALI_SUPPORT
    //DC cal
    memset((void *)&mDCCalBuffer, 0, sizeof(mDCCalBuffer));
    mDCCalEnable = false;
    mDCCalBufferFull = false;
    mDCCalDumpFile = NULL;
#endif

    // config attribute (will used in client SRC/Enh/... later) // TODO(Harvey): query this
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = mConfig.rate;  //48000;
#ifdef MTK_DMIC_SR_LIMIT
    mStreamAttributeSource.sample_rate = mConfig.rate;  //32000;
#endif

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

#ifndef UPLINK_LOW_LATENCY  //no need to drop data
#ifdef RECORD_INPUT_24BITS // 24bit record
    mCaptureDropSize = ((mStreamAttributeSource.sample_rate * CAPTURE_DROP_MS << 3) / 1000);    //32bit, drop data which get from kernel
#else
    mCaptureDropSize = ((mStreamAttributeSource.sample_rate * CAPTURE_DROP_MS << 2) / 1000);    //16bit
#endif
#endif

    ALOGD("%s(), mCaptureDropSize=%d, CAPTURE_DROP_MS=%d", __FUNCTION__, mCaptureDropSize, CAPTURE_DROP_MS);
    ALOGD("%s(), period_count=%d, period_size=%d, samplerate = %d", __FUNCTION__, mConfig.period_count, mConfig.period_size, mConfig.rate);


    OpenPCMDump(LOG_TAG);

    // enable pcm
    ASSERT(mPcm == NULL);
    int pcmIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmUl1Capture);
    int cardIdx = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmUl1Capture);
    mPcm = pcm_open(cardIdx, pcmIdx, PCM_IN | PCM_MONOTONIC, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
    ALOGV("%s(), mPcm = %p", __FUNCTION__, mPcm);

    pcm_start(mPcm);

    // create reading thread
    mEnable = true;
    int ret = pthread_create(&hReadThread, NULL, AudioALSACaptureDataProviderNormal::readThread, (void *)this);
    if (ret != 0)
    {
        ALOGE("%s() create thread fail!!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

#ifdef MTK_VOW_DCCALI_SUPPORT
    //create DC cal thread
    ret = pthread_create(&hDCCalThread, NULL, AudioALSACaptureDataProviderNormal::DCCalThread, (void *)this);
    if (ret != 0)
    {
        ALOGE("%s() create DCCal thread fail!!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    ret = pthread_mutex_init(&mDCCal_Mutex, NULL);
    if (ret != 0)
    {
        ALOGE("%s, Failed to initialize mDCCal_Mutex!", __FUNCTION__);
    }

    ret = pthread_cond_init(&mDCCal_Cond, NULL);
    if (ret != 0)
    {
        ALOGE("%s, Failed to initialize mDCCal_Cond!", __FUNCTION__);
    }

    mDCCalEnable = true;

    mDCCalBuffer.pBufBase = new char[kDCRReadBufferSize];
    mDCCalBuffer.bufLen   = kDCRReadBufferSize;
    mDCCalBuffer.pRead    = mDCCalBuffer.pBufBase;
    mDCCalBuffer.pWrite   = mDCCalBuffer.pBufBase;
    ASSERT(mDCCalBuffer.pBufBase != NULL);

    AudioAutoTimeoutLock _lDC(mDCCalEnableLock);
    mDCCalEnable = true;

    OpenDCCalDump();
#if 0
    pDCCalFile = fopen("/sdcard/mtklog/DCCalFile.pcm", "wb");
    if (pDCCalFile == NULL)
    {
        ALOGW("%s(), create pDCCalFile fail ", __FUNCTION__);
    }
#endif
    //create DC cal ---
#endif

    return NO_ERROR;
}

uint32_t AudioALSACaptureDataProviderNormal::getLatencyTime()
{
    mlatency = UPLINK_NORMAL_LATENCY_MS ; //20ms
#ifdef UPLINK_LOW_LATENCY
    if (HasLowLatencyCapture())
    {
      mlatency = UPLINK_LOW_LATENCY_MS; //5ms
    }
#endif
    return mlatency;
}

status_t AudioALSACaptureDataProviderNormal::close()
{
    ALOGD("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class detach

    mEnable = false;
    AudioAutoTimeoutLock _l(mEnableLock);
    AudioAutoTimeoutLock _l2(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    ClosePCMDump();

    pcm_stop(mPcm);
    pcm_close(mPcm);
    mPcm = NULL;

#ifdef MTK_VOW_DCCALI_SUPPORT
    mDCCalEnable = false;
    AudioAutoTimeoutLock _lDC(mDCCalEnableLock);
    pthread_cond_signal(&mDCCal_Cond);

    CloseDCCalDump();

    if (mDCCalBuffer.pBufBase != NULL) { delete[] mDCCalBuffer.pBufBase; }
#endif

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}

void *AudioALSACaptureDataProviderNormal::readThread(void *arg)
{
    pthread_detach(pthread_self());

    status_t retval = NO_ERROR;
    AudioALSACaptureDataProviderNormal *pDataProvider = static_cast<AudioALSACaptureDataProviderNormal *>(arg);

    uint32_t open_index = pDataProvider->mOpenIndex;

    char nameset[32];
    sprintf(nameset, "%s%d", __FUNCTION__, pDataProvider->mCaptureDataProviderType);
    prctl(PR_SET_NAME, (unsigned long)nameset, 0, 0, 0);

#ifdef MTK_AUDIO_ADJUST_PRIORITY
    // force to set priority
    struct sched_param sched_p;
    sched_getparam(0, &sched_p);
    sched_p.sched_priority = RTPM_PRIO_AUDIO_RECORD + 1;
    if (0 != sched_setscheduler(0, SCHED_RR, &sched_p))
    {
        ALOGE("[%s] failed, errno: %d", __FUNCTION__, errno);
    }
    else
    {
        sched_p.sched_priority = RTPM_PRIO_AUDIO_RECORD + 1;
        sched_getparam(0, &sched_p);
        ALOGD("sched_setscheduler ok, priority: %d", sched_p.sched_priority);
    }
#endif
    ALOGD("+%s(), pid: %d, tid: %d, kReadBufferSize=0x%x, open_index=%d", __FUNCTION__, getpid(), gettid(), kReadBufferSize, open_index);


    // read raw data from alsa driver
    char linear_buffer[kReadBufferSize];
    uint32_t Read_Size = kReadBufferSize;
    uint32_t kReadBufferSize_new;
    while (pDataProvider->mEnable == true)
    {
        if (open_index != pDataProvider->mOpenIndex)
        {
            ALOGD("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, pDataProvider->mOpenIndex);
            break;
        }

        retval = pDataProvider->mEnableLock.lock_timeout(500);
        ASSERT(retval == NO_ERROR);
        if (pDataProvider->mEnable == false)
        {
            pDataProvider->mEnableLock.unlock();
            break;
        }

        ASSERT(pDataProvider->mPcm != NULL);
        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[0] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;

        if (pDataProvider->mCaptureDropSize > 0)
        {
            Read_Size = (pDataProvider->mCaptureDropSize > kReadBufferSize) ? kReadBufferSize : pDataProvider->mCaptureDropSize;
            int retval = pcm_read(pDataProvider->mPcm, linear_buffer, Read_Size);

            if (retval != 0)
            {
                ALOGE("%s(), pcm_read() drop error, retval = %d", __FUNCTION__, retval);
            }
            ALOGV("%s(), mCaptureDropSize = %d, Read_Size=%d", __FUNCTION__, pDataProvider->mCaptureDropSize, Read_Size);
            pDataProvider->mCaptureDropSize -= Read_Size;
            pDataProvider->mEnableLock.unlock();
            continue;
        }
        else
        {
            int retval = pcm_read(pDataProvider->mPcm, linear_buffer, kReadBufferSize);
            if (retval != 0)
            {
                ALOGE("%s(), pcm_read() error, retval = %d", __FUNCTION__, retval);
            }
        }
        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[1] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;

        //struct timespec tempTimeStamp;
        pDataProvider->GetCaptureTimeStamp(&pDataProvider->mStreamAttributeSource.Time_Info, kReadBufferSize);

#ifdef MTK_VOW_DCCALI_SUPPORT
        //copy data to DC Cal
        pDataProvider->copyCaptureDataToDCCalBuffer(linear_buffer, kReadBufferSize_new);
#endif

      // use ringbuf format to save buffer info
        pDataProvider->mPcmReadBuf.pBufBase = linear_buffer;
        pDataProvider->mPcmReadBuf.bufLen   = kReadBufferSize + 1; // +1: avoid pRead == pWrite
        pDataProvider->mPcmReadBuf.pRead    = linear_buffer;
        pDataProvider->mPcmReadBuf.pWrite   = linear_buffer + kReadBufferSize;
        pDataProvider->mEnableLock.unlock();
        pDataProvider->provideCaptureDataToAllClients(open_index);

        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[2] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;

        if (pDataProvider->mPCMDumpFile || btempDebug)
        {
            ALOGD("%s, latency_in_us,%1.6lf,%1.6lf,%1.6lf", __FUNCTION__, pDataProvider->timerec[0], pDataProvider->timerec[1], pDataProvider->timerec[2]);
        }
    }

    ALOGD("-%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    pthread_exit(NULL);
    return NULL;
}

#ifdef MTK_VOW_DCCALI_SUPPORT
void AudioALSACaptureDataProviderNormal::copyCaptureDataToDCCalBuffer(void *buffer, size_t size)
{
    size_t copysize = size;
    uint32_t freeSpace = RingBuf_getFreeSpace(&mDCCalBuffer);
    ALOGV("%s(), freeSpace(%u), dataSize(%u),mDCCalBufferFull=%d", __FUNCTION__, freeSpace, size, mDCCalBufferFull);

    if (mDCCalBufferFull == false)
    {
        if (freeSpace > 0)
        {
            if (freeSpace < size)
            {
                ALOGD("%s(), freeSpace(%u) < dataSize(%u), buffer full!!", __FUNCTION__, freeSpace, size);
                //ALOGD("%s before,pBase = 0x%x pWrite = 0x%x  bufLen = %d  pRead = 0x%x",__FUNCTION__,
                //mDCCalBuffer.pBufBase,mDCCalBuffer.pWrite, mDCCalBuffer.bufLen,mDCCalBuffer.pRead);

                RingBuf_copyFromLinear(&mDCCalBuffer, (char *)buffer, freeSpace);

                //ALOGD("%s after,pBase = 0x%x pWrite = 0x%x  bufLen = %d  pRead = 0x%x",__FUNCTION__,
                //mDCCalBuffer.pBufBase,mDCCalBuffer.pWrite, mDCCalBuffer.bufLen,mDCCalBuffer.pRead);
            }
            else
            {
                //ALOGD("%s before,pBase = 0x%x pWrite = 0x%x  bufLen = %d  pRead = 0x%x",__FUNCTION__,
                //mDCCalBuffer.pBufBase,mDCCalBuffer.pWrite, mDCCalBuffer.bufLen,mDCCalBuffer.pRead);

                RingBuf_copyFromLinear(&mDCCalBuffer, (char *)buffer, size);

                //ALOGD("%s after,pBase = 0x%x pWrite = 0x%x  bufLen = %d  pRead = 0x%x",__FUNCTION__,
                //mDCCalBuffer.pBufBase,mDCCalBuffer.pWrite, mDCCalBuffer.bufLen,mDCCalBuffer.pRead);
            }
        }
        else
        {
            mDCCalBufferFull = true;
            pthread_cond_signal(&mDCCal_Cond);
        }
    }
}

size_t AudioALSACaptureDataProviderNormal::CalulateDC(short *buffer , size_t size)
{
    //ALOGV("%s()+,Size(%u)", __FUNCTION__, size);
    int checksize = size >> 2;  //stereo, 16bits
    int count = checksize;
    int accumulateL = 0, accumulateR = 0;
    short DCL = 0, DCR = 0;

#if 0
    if (pDCCalFile != NULL)
    {
        fwrite(buffer, sizeof(char), size, pDCCalFile);
    }
#endif
    WriteDCCalDumpData((void *)buffer, size);

    while (count)
    {
        accumulateL += *(buffer);
        accumulateR += *(buffer + 1);
        buffer += 2;
        count--;
    }
    DCL = (short)(accumulateL / checksize);
    DCR = (short)(accumulateR / checksize);

    ALOGD("%s()- ,checksize(%d),accumulateL(%d),accumulateR(%d), DCL(%d), DCR(%d)", __FUNCTION__, checksize, accumulateL, accumulateR, DCL, DCR);
    return size;
}

void *AudioALSACaptureDataProviderNormal::DCCalThread(void *arg)
{
    prctl(PR_SET_NAME, (unsigned long)__FUNCTION__, 0, 0, 0);

    ALOGD("+%s(), pid: %d, tid: %d, kDCRReadBufferSize=%x", __FUNCTION__, getpid(), gettid(), kDCRReadBufferSize);

    status_t retval = NO_ERROR;
    AudioALSACaptureDataProviderNormal *pDataProvider = static_cast<AudioALSACaptureDataProviderNormal *>(arg);


    //char linear_buffer[kDCRReadBufferSize];
    char *plinear_buffer = new char[kDCRReadBufferSize];
    uint32_t Read_Size = kDCRReadBufferSize;
    while (pDataProvider->mDCCalEnable == true)
    {
        pthread_mutex_lock(&pDataProvider->mDCCal_Mutex);
        pthread_cond_wait(&pDataProvider->mDCCal_Cond, &pDataProvider->mDCCal_Mutex);
        //ALOGD("%s(), signal get", __FUNCTION__);

        retval = pDataProvider->mDCCalEnableLock.lock_timeout(300);
        ASSERT(retval == NO_ERROR);
        if (pDataProvider->mDCCalEnable == false)
        {
            pDataProvider->mDCCalEnableLock.unlock();
            pthread_mutex_unlock(&pDataProvider->mDCCal_Mutex);
            break;
        }

        if (pDataProvider->mDCCalBufferFull)
        {
            Read_Size = RingBuf_getDataCount(&pDataProvider->mDCCalBuffer);
            //ALOGD("%s(), Read_Size =%u, kDCRReadBufferSize=%u", __FUNCTION__,Read_Size,kDCRReadBufferSize);
            if (Read_Size > kDCRReadBufferSize)
            {
                Read_Size = kDCRReadBufferSize;
            }

            //ALOGD("%s,pBase = 0x%x pWrite = 0x%x  bufLen = %d  pRead = 0x%x",__FUNCTION__,
            //pDataProvider->mDCCalBuffer.pBufBase,pDataProvider->mDCCalBuffer.pWrite, pDataProvider->mDCCalBuffer.bufLen,pDataProvider->mDCCalBuffer.pRead);

            RingBuf_copyToLinear(plinear_buffer, &pDataProvider->mDCCalBuffer, Read_Size);
            //ALOGD("%s after copy,pBase = 0x%x pWrite = 0x%x  bufLen = %d  pRead = 0x%x",__FUNCTION__,
            //pDataProvider->mDCCalBuffer.pBufBase,pDataProvider->mDCCalBuffer.pWrite, pDataProvider->mDCCalBuffer.bufLen,pDataProvider->mDCCalBuffer.pRead);
            pDataProvider->CalulateDC((short *)plinear_buffer, Read_Size);

            pDataProvider->mDCCalBufferFull = false;
        }

        pDataProvider->mDCCalEnableLock.unlock();
        pthread_mutex_unlock(&pDataProvider->mDCCal_Mutex);
    }

    ALOGD("-%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    delete[] plinear_buffer;
    pthread_exit(NULL);
    return NULL;
}


void AudioALSACaptureDataProviderNormal::OpenDCCalDump()
{
    ALOGV("%s()", __FUNCTION__);
    char DCCalDumpFileName[128];
    sprintf(DCCalDumpFileName, "%s.pcm", "/sdcard/mtklog/audio_dump/DCCalData");

    mDCCalDumpFile = NULL;
    mDCCalDumpFile = AudioOpendumpPCMFile(DCCalDumpFileName, streamin_propty);

    if (mDCCalDumpFile != NULL)
    {
        ALOGD("%s, DCCalDumpFileName = %s", __FUNCTION__, DCCalDumpFileName);
    }
}

void AudioALSACaptureDataProviderNormal::CloseDCCalDump()
{
    if (mDCCalDumpFile)
    {
        AudioCloseDumpPCMFile(mDCCalDumpFile);
        ALOGD("%s()", __FUNCTION__);
    }
}

void  AudioALSACaptureDataProviderNormal::WriteDCCalDumpData(void *buffer , size_t size)
{
    if (mDCCalDumpFile)
    {
        //ALOGD("%s()", __FUNCTION__);
        AudioDumpPCMData((void *)buffer , size, mDCCalDumpFile);
    }
}
#else
void AudioALSACaptureDataProviderNormal::copyCaptureDataToDCCalBuffer(void *buffer, size_t size)
{
    ALOGE("%s() unsupport", __FUNCTION__);
}

size_t AudioALSACaptureDataProviderNormal::CalulateDC(short *buffer , size_t size)
{
    ALOGE("%s() unsupport", __FUNCTION__);
    return 0;
}

void *AudioALSACaptureDataProviderNormal::DCCalThread(void *arg)
{
    ALOGE("%s() unsupport", __FUNCTION__);
    return NULL;
}

void AudioALSACaptureDataProviderNormal::OpenDCCalDump()
{
    ALOGE("%s() unsupport", __FUNCTION__);
}

void AudioALSACaptureDataProviderNormal::CloseDCCalDump()
{
    ALOGE("%s() unsupport", __FUNCTION__);
}

void  AudioALSACaptureDataProviderNormal::WriteDCCalDumpData(void *buffer , size_t size)
{
    ALOGE("%s() unsupport", __FUNCTION__);
}

#endif

} // end of namespace android
