#include "AudioALSACaptureDataProviderBTCVSD.h"

#include <pthread.h>

#include <sys/prctl.h>

#include "AudioType.h"

#include "WCNChipController.h"

#include "AudioBTCVSDDef.h"
#include "AudioBTCVSDControl.h"
#include "AudioALSADriverUtility.h"

#define LOG_TAG "AudioALSACaptureDataProviderBTCVSD"

namespace android
{

static bool mBTMode_Open;
/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderBTCVSD *AudioALSACaptureDataProviderBTCVSD::mAudioALSACaptureDataProviderBTCVSD = NULL;
AudioALSACaptureDataProviderBTCVSD *AudioALSACaptureDataProviderBTCVSD::getInstance()
{
    AudioLock mGetInstanceLock;
    AudioAutoTimeoutLock _l(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderBTCVSD == NULL)
    {
        mAudioALSACaptureDataProviderBTCVSD = new AudioALSACaptureDataProviderBTCVSD();
    }
    ASSERT(mAudioALSACaptureDataProviderBTCVSD != NULL);
    return mAudioALSACaptureDataProviderBTCVSD;
}

AudioALSACaptureDataProviderBTCVSD::AudioALSACaptureDataProviderBTCVSD() :
    mWCNChipController(WCNChipController::GetInstance()),
    mAudioBTCVSDControl(AudioBTCVSDControl::getInstance()),
    mMixer(AudioALSADriverUtility::getInstance()->getMixer()),
    mBTIrqReceived(false),
    hReadThread(0),
    mReadBufferSize(0),
    mBliSrc(NULL),
    mBliSrcOutputBuffer(NULL),
#ifndef MTK_SUPPORT_BTCVSD_ALSA
    mFd2(mAudioBTCVSDControl->getFd())
#else
    mFd2(0)
#endif
{
    ALOGD("%s()", __FUNCTION__);

    mCaptureDataProviderType = CAPTURE_PROVIDER_BT_CVSD;
}

AudioALSACaptureDataProviderBTCVSD::~AudioALSACaptureDataProviderBTCVSD()
{
    ALOGD("%s()", __FUNCTION__);
}


status_t AudioALSACaptureDataProviderBTCVSD::open()
{
    ALOGD("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class attach
    AudioAutoTimeoutLock _l(mEnableLock);

    ASSERT(mEnable == false);

    // config attribute (will used in client SRC/Enh/... later)
    mStreamAttributeSource.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeSource.audio_channel_mask = AUDIO_CHANNEL_IN_MONO;
    mStreamAttributeSource.num_channels = android_audio_legacy::AudioSystem::popCount(mStreamAttributeSource.audio_channel_mask);
    mStreamAttributeSource.sample_rate = mWCNChipController->GetBTCurrentSamplingRateNumber();

    // Reset frames readed counter
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;

    if (mAudioBTCVSDControl->BT_SCO_isWideBand() == true)
    {
        mReadBufferSize = MSBC_PCM_FRAME_BYTE * 6 * 2; // 16k mono->48k stereo
    }
    else
    {
        mReadBufferSize = SCO_RX_PCM8K_BUF_SIZE * 12 * 2; // 8k mono->48k stereo
    }

    mBTMode_Open = mAudioBTCVSDControl->BT_SCO_isWideBand();
    initBliSrc();
    ALOGD("%s(), audio_format = %d, audio_channel_mask=%x, num_channels=%d, sample_rate=%d", __FUNCTION__,
          mStreamAttributeSource.audio_format, mStreamAttributeSource.audio_channel_mask, mStreamAttributeSource.num_channels, mStreamAttributeSource.sample_rate);

    OpenPCMDump(LOG_TAG);

    // enable bt cvsd driver
#ifdef MTK_SUPPORT_BTCVSD_ALSA
#if 0
	int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmBTCVSDCapture);
	int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmBTCVSDCapture);

	struct pcm_params *params;
	params = pcm_params_get(cardindex, pcmindex,  PCM_OUT);
	if (params == NULL)
	{
		ALOGD("Device does not exist.\n");
	}

	// HW pcm config
	mConfig.channels = mStreamAttributeSource.num_channels;
	mConfig.rate = mStreamAttributeSource.sample_rate;
	mConfig.period_count = 2;
	mConfig.period_size = 1024;//(mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count)) / ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);
	mConfig.format = PCM_FORMAT_S16_LE;//transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);
	mConfig.start_threshold = 0;
	mConfig.stop_threshold = 0;
	mConfig.silence_threshold = 0;
	ALOGD("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d, pcmindex=%d",
		  __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format, pcmindex);

	// open pcm driver
	openPcmDriver(pcmindex);
	//openPcmDriver(25);
#else
	memset(&mConfig, 0, sizeof(mConfig));
	mConfig.channels = mStreamAttributeSource.num_channels;
	mConfig.rate = mStreamAttributeSource.sample_rate;
	mConfig.period_size = 1024;
	mConfig.period_count = 2;
	mConfig.format = PCM_FORMAT_S16_LE;
	mConfig.start_threshold = 0;
	mConfig.stop_threshold = 0;
	mConfig.silence_threshold = 0;

	ASSERT(mPcm == NULL);
    int pcmIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmBTCVSDCapture);
    int cardIdx = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmBTCVSDCapture);
	mPcm = pcm_open(cardIdx, pcmIdx, PCM_IN, &mConfig);
	ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
	mAudioBTCVSDControl->BT_SCO_RX_Begin(mFd2);
	pcm_start(mPcm);
#endif
#else
    mAudioBTCVSDControl->BT_SCO_RX_Begin(mFd2);
#endif

    // create reading thread
    mEnable = true;
    int ret = pthread_create(&hReadThread, NULL, AudioALSACaptureDataProviderBTCVSD::readThread, (void *)this);
    if (ret != 0)
    {
        ALOGE("%s() create thread fail!!", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t AudioALSACaptureDataProviderBTCVSD::close()
{
    ALOGD("%s()", __FUNCTION__);
    ASSERT(mClientLock.tryLock() != 0); // lock by base class detach

    mEnable = false;
    AudioAutoTimeoutLock _l(mEnableLock);

    ClosePCMDump();

    mAudioBTCVSDControl->BT_SCO_RX_End(mFd2);

#ifdef MTK_SUPPORT_BTCVSD_ALSA
    pcm_stop(mPcm);
    pcm_close(mPcm);
    mPcm = NULL;
#endif
    return NO_ERROR;
}


void *AudioALSACaptureDataProviderBTCVSD::readThread(void *arg)
{
    pthread_detach(pthread_self());

    prctl(PR_SET_NAME, (unsigned long)__FUNCTION__, 0, 0, 0);

#ifdef MTK_AUDIO_ADJUST_PRIORITY
    #define RTPM_PRIO_AUDIO_RECORD (77)
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
    ALOGD("+%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());



    status_t retval = NO_ERROR;
    AudioALSACaptureDataProviderBTCVSD *pDataProvider = static_cast<AudioALSACaptureDataProviderBTCVSD *>(arg);

    uint32_t open_index = pDataProvider->mOpenIndex;

    // read raw data from alsa driver
    uint32_t read_size = 0;
    char linear_buffer[MSBC_PCM_FRAME_BYTE * 6 * 2]; // fixed at size for WB
    while (pDataProvider->mEnable == true)
    {
        if (open_index != pDataProvider->mOpenIndex)
        {
            ALOGD("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, pDataProvider->mOpenIndex);
            break;
        }

        retval = pDataProvider->mEnableLock.lock_timeout(300);
        ASSERT(retval == NO_ERROR);
        if (pDataProvider->mEnable == false)
        {
            ALOGE("%s(), pDataProvider->mEnable == false", __FUNCTION__);
            pDataProvider->mEnableLock.unlock();
            break;
        }

        read_size = pDataProvider->readDataFromBTCVSD(linear_buffer);
        if (read_size == 0)
        {
            ALOGE("%s(), read_size == 0", __FUNCTION__);
            pDataProvider->mEnableLock.unlock();
            continue;
        }

        // SRC
        void *pBufferAfterBliSrc = NULL;
        uint32_t bytesAfterBliSrc = 0;
        pDataProvider->doBliSrc(linear_buffer, read_size, &pBufferAfterBliSrc, &bytesAfterBliSrc);

        // use ringbuf format to save buffer info
        pDataProvider->mPcmReadBuf.pBufBase = (char *)pBufferAfterBliSrc;
        pDataProvider->mPcmReadBuf.bufLen   = bytesAfterBliSrc + 1; // +1: avoid pRead == pWrite
        pDataProvider->mPcmReadBuf.pRead    = (char *)pBufferAfterBliSrc;
        pDataProvider->mPcmReadBuf.pWrite   = (char *)pBufferAfterBliSrc + bytesAfterBliSrc;
        pDataProvider->mEnableLock.unlock();

        pDataProvider->provideCaptureDataToAllClients(open_index);
    }

    pDataProvider->deinitBliSrc();

    pDataProvider->mBTIrqReceived = false;

    ALOGD("-%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    pthread_exit(NULL);
    return NULL;
}


uint32_t AudioALSACaptureDataProviderBTCVSD::readDataFromBTCVSD(void *linear_buffer)
{
    ALOGV("+%s()", __FUNCTION__);

    uint8_t *cvsd_raw_data = mAudioBTCVSDControl->BT_SCO_RX_GetCVSDTempInBuf();
#ifdef MTK_SUPPORT_BTCVSD_ALSA
    ASSERT(mPcm != NULL);

    // make sure receiving bt irq before start reading
    if (!mBTIrqReceived)
    {
        pcm_prepare(mPcm);

        struct mixer_ctl *ctl;
        unsigned int num_values, i;
        int index = 0;
        ctl = mixer_get_ctl_by_name(mMixer, "btcvsd_rx_irq_received");
        num_values = mixer_ctl_get_num_values(ctl);
        for (i = 0; i < num_values; i++)
        {
            index = mixer_ctl_get_value(ctl, i);
            ALOGV("%s(), btcvsd_rx_irq_received, i = %d, index = %d", __FUNCTION__, i, index);
        }
        mBTIrqReceived = index != 0;
    }

    if (mBTIrqReceived)
    {
        int retval = pcm_read(mPcm, cvsd_raw_data, BTSCO_CVSD_RX_TEMPINPUTBUF_SIZE);
        if (retval != 0)
        {
            ALOGE("%s(), pcm_read() error, retval = %d, fail due to %s", __FUNCTION__, retval, pcm_get_error(mPcm));
            usleep(60 * 1000);  // BTSCO_CVSD_RX_TEMPINPUTBUF_SIZE will transfer to 60ms data, NB: 16 * 3.75ms/WB: 8 * 7.5ms
            memset(linear_buffer, 0, MSBC_PCM_FRAME_BYTE * 6 * 2);
            // pretend read 60ms data
            return mAudioBTCVSDControl->BT_SCO_isWideBand() ? 0.06 * 16000 * 2 : 0.06 * 8000 * 2;
        }
        // check if timeout during read bt data
        struct mixer_ctl *ctl = mixer_get_ctl_by_name(mMixer, "btcvsd_rx_timeout");
        int index = mixer_ctl_get_value(ctl, 0);
        bool rx_timeout = index != 0;
        ALOGV("%s(), btcvsd_rx_timeout, rx_timeout %d, index %d", __FUNCTION__, rx_timeout, index);
        if (rx_timeout)
        {
            ALOGE("%s(), rx_timeout %d, index %d, return mute data", __FUNCTION__, rx_timeout, index);
            memset(linear_buffer, 0, MSBC_PCM_FRAME_BYTE * 6 * 2);
            // pretend read 60ms data
            return mAudioBTCVSDControl->BT_SCO_isWideBand() ? 0.06 * 16000 * 2 : 0.06 * 8000 * 2;
        }
    }
    else
    {
        ALOGW("%s(), mBTIrqReceived = %d", __FUNCTION__, mBTIrqReceived);
        usleep(22.5 * 1000);    // bt irq interval is 22.5ms
        memset(linear_buffer, 0, MSBC_PCM_FRAME_BYTE * 6 * 2);
        return mAudioBTCVSDControl->BT_SCO_isWideBand() ? 0.0225 * 16000 * 2 : 0.0225 * 8000 * 2;
    }
#else
    uint32_t raw_data_size = ::read(mFd2, cvsd_raw_data, BTSCO_CVSD_RX_TEMPINPUTBUF_SIZE);
    ALOGV("%s(), cvsd_raw_data = %p, raw_data_size = %d", __FUNCTION__, cvsd_raw_data, raw_data_size);

    if (raw_data_size == 0)
    {
        ALOGE("%s(), raw_data_size == 0", __FUNCTION__);
        return 0;
    }
#endif

    uint8_t *inbuf = mAudioBTCVSDControl->BT_SCO_RX_GetCVSDInBuf();
    uint32_t insize = SCO_RX_PLC_SIZE;

    uint8_t *outbuf = NULL;
    uint32_t outsize = 0;
    if (mAudioBTCVSDControl->BT_SCO_isWideBand() == true)
    {
        outbuf = mAudioBTCVSDControl->BT_SCO_RX_GetMSBCOutBuf();
        outsize = MSBC_PCM_FRAME_BYTE;
    }
    else
    {
        outbuf = mAudioBTCVSDControl->BT_SCO_RX_GetCVSDOutBuf();
        outsize = SCO_RX_PCM8K_BUF_SIZE;
    }

    uint8_t *workbuf = mAudioBTCVSDControl->BT_SCO_RX_GetCVSDWorkBuf();
    uint32_t workbufsize = SCO_RX_PCM64K_BUF_SIZE;


    uint8_t packetvalid = 0;
    uint32_t total_read_size = 0;
    uint32_t bytes = BTSCO_CVSD_RX_INBUF_SIZE;
    do
    {
        memcpy(inbuf, cvsd_raw_data, SCO_RX_PLC_SIZE);
        cvsd_raw_data += SCO_RX_PLC_SIZE;

        packetvalid = *cvsd_raw_data; // parser packet valid info for each 30-byte packet
        //packetvalid = 1; // force packvalid to 1 for test
        cvsd_raw_data += BTSCO_CVSD_PACKET_VALID_SIZE;

        insize = SCO_RX_PLC_SIZE;
        
        if(mBTMode_Open != mAudioBTCVSDControl->BT_SCO_isWideBand())
        {            
            ALOGD("BTSCO change mode after RX_Begin!!!");
            mAudioBTCVSDControl->BT_SCO_RX_End(mFd2);
            mAudioBTCVSDControl->BT_SCO_RX_Begin(mFd2); 
            mBTMode_Open = mAudioBTCVSDControl->BT_SCO_isWideBand();                    

            if (mAudioBTCVSDControl->BT_SCO_isWideBand() == true)
            {
                mReadBufferSize = MSBC_PCM_FRAME_BYTE * 6 * 2; // 16k mono->48k stereo
            }
            else
            {
                mReadBufferSize = SCO_RX_PCM8K_BUF_SIZE * 12 * 2; // 8k mono->48k stereo
            }

            // enable bli src when needed
            initBliSrc();

            return 0;
        }

        outsize = (mAudioBTCVSDControl->BT_SCO_isWideBand() == true) ? MSBC_PCM_FRAME_BYTE : SCO_RX_PCM8K_BUF_SIZE;
        ALOGV("btsco_process_RX_CVSD/MSBC(+), insize = %d, outsize = %d, packetvalid = %u", insize, outsize, packetvalid);

        if (mAudioBTCVSDControl->BT_SCO_isWideBand() == true)
        {
            mAudioBTCVSDControl->btsco_process_RX_MSBC(inbuf, &insize, outbuf, &outsize, workbuf, workbufsize, packetvalid);
        }
        else
        {
            mAudioBTCVSDControl->btsco_process_RX_CVSD(inbuf, &insize, outbuf, &outsize, workbuf, workbufsize, packetvalid);
        }
        inbuf += SCO_RX_PLC_SIZE;
        bytes -= insize;
        ALOGV("btsco_process_RX_CVSD/MSBC(-), insize = %d, outsize = %d, bytes = %d", insize, outsize, bytes);


        if (outsize > 0)
        {
            if (total_read_size + outsize <= mReadBufferSize)
            {
                memcpy(linear_buffer, outbuf, outsize);
            }
            else
            {
                ALOGE("%s(), total_read_size %u + outsize %u > mReadBufferSize %u, bytes %u", __FUNCTION__, total_read_size, outsize, mReadBufferSize, bytes);
                ASSERT(total_read_size + outsize <= mReadBufferSize);
                if (total_read_size < mReadBufferSize)
                {
                    outsize = mReadBufferSize - total_read_size;
                    memcpy(linear_buffer, outbuf, outsize);
                }
                else
                {
                    break;
                }
            }
            linear_buffer = ((char *)linear_buffer) + outsize;
            total_read_size += outsize;
        }
    }
    while (bytes > 0 && total_read_size < mReadBufferSize);


    ALOGV("+%s(), total_read_size = %u", __FUNCTION__, total_read_size);
    return total_read_size;
}

static const uint32_t kBliSrcOutputBufferSize = 0x10000;  // 64k
status_t AudioALSACaptureDataProviderBTCVSD::initBliSrc()
{
    ALOGD("%s(), bt band = %d, mStreamAttributeSource.sample_rate = %u mBliSrc = %p", __FUNCTION__,
          mAudioBTCVSDControl->BT_SCO_isWideBand(), mStreamAttributeSource.sample_rate, mBliSrc);

    bool needSrc = false;
    // bt band and rate is not match, need src
    if ((mAudioBTCVSDControl->BT_SCO_isWideBand() && mStreamAttributeSource.sample_rate != 16000) ||
        (!mAudioBTCVSDControl->BT_SCO_isWideBand() && mStreamAttributeSource.sample_rate != 8000))
    {
        needSrc = true;

    }

    // always recreate blisrc
    if (mBliSrc)
    {
        deinitBliSrc();
    }

    // init BLI SRC if need
    if (!mBliSrc && needSrc)
    {
        SRC_PCM_FORMAT src_pcm_format = SRC_IN_Q1P15_OUT_Q1P15;

        mBliSrc = newMtkAudioSrc(
            mAudioBTCVSDControl->BT_SCO_isWideBand() ? 16000 : 8000, 1,
            mStreamAttributeSource.sample_rate,  mStreamAttributeSource.num_channels,
            src_pcm_format);

        ASSERT(mBliSrc != NULL);
        mBliSrc->open();

        mBliSrcOutputBuffer = new char[kBliSrcOutputBufferSize];
        ASSERT(mBliSrcOutputBuffer != NULL);
    }

    return NO_ERROR;
}


status_t AudioALSACaptureDataProviderBTCVSD::deinitBliSrc()
{
    // deinit BLI SRC if need
    if (mBliSrc != NULL)
    {
        mBliSrc->close();
        deleteMtkAudioSrc(mBliSrc);
        mBliSrc = NULL;
    }

    if (mBliSrcOutputBuffer != NULL)
    {
        delete[] mBliSrcOutputBuffer;
        mBliSrcOutputBuffer = NULL;
    }

    return NO_ERROR;
}


status_t AudioALSACaptureDataProviderBTCVSD::doBliSrc(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes)
{
    if (mBliSrc == NULL) // No need SRC
    {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }
    else
    {
        char *p_read = (char *)pInBuffer;
        uint32_t num_raw_data_left = inBytes;
        uint32_t num_converted_data = kBliSrcOutputBufferSize; // max convert num_free_space

        uint32_t consumed = num_raw_data_left;
        mBliSrc->process((int16_t *)p_read, &num_raw_data_left,
                         (int16_t *)mBliSrcOutputBuffer, &num_converted_data);
        consumed -= num_raw_data_left;
        p_read += consumed;

        ALOGV("%s(), num_raw_data_left = %u, num_converted_data = %u",
              __FUNCTION__, num_raw_data_left, num_converted_data);

        if (num_raw_data_left > 0)
        {
            ALOGW("%s(), num_raw_data_left(%u) > 0", __FUNCTION__, num_raw_data_left);
            ASSERT(num_raw_data_left == 0);
        }

        *ppOutBuffer = mBliSrcOutputBuffer;
        *pOutBytes = num_converted_data;
    }

    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}

} // end of namespace android
