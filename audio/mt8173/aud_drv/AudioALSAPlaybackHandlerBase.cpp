#define LOG_TAG "AudioALSAPlaybackHandlerBase"

#include "AudioALSAPlaybackHandlerBase.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioUtility.h"
#include "AudioMTKFilter.h"
#include "MtkAudioComponent.h"


//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif


namespace android
{

static const uint32_t kAudioSoundCardIndex = 0;

#ifdef MTK_HIRES_192K_AUDIO_SUPPORT
static const uint32_t kMaxPcmDriverBufferSize = 0x20000; // 128k
static const uint32_t kBliSrcOutputBufferSize = 0x18000; // 96k
static const uint32_t kPcmDriverBufferSize    = 0x10000; // 64k
#else
static const uint32_t kMaxPcmDriverBufferSize = 0x10000; // 64k
static const uint32_t kBliSrcOutputBufferSize = 0x8000;  // 32k
static const uint32_t kPcmDriverBufferSize    = 0x6000;  // 24k
#endif

uint32_t AudioALSAPlaybackHandlerBase::mDumpFileNum = 0;

AudioALSAPlaybackHandlerBase::AudioALSAPlaybackHandlerBase(const stream_attribute_t *stream_attribute_source) :
    mPlaybackHandlerType(PLAYBACK_HANDLER_BASE),
    mHardwareResourceManager(AudioALSAHardwareResourceManager::getInstance()),
    mStreamAttributeSource(stream_attribute_source),
    mPcm(NULL),
    mComprStream(NULL),
    mStreamCbk(NULL),
    mCbkCookie(NULL),
    mAudioFilterManagerHandler(NULL),
    mPostProcessingOutputBuffer(NULL),
    mPostProcessingOutputBufferSize(0),
    mBliSrc(NULL),
    mBliSrcOutputBuffer(NULL),
    mBitConverter(NULL),
    mBitConverterOutputBuffer(NULL),
    mdataPendingOutputBuffer(NULL),
    mdataPendingTempBuffer(NULL),
    mdataPendingOutputBufferSize(0),
    mdataPendingRemindBufferSize(0),
    mPCMDumpFile(NULL),
    mIdentity(0xFFFFFFFF)
{
    ALOGV("%s()", __FUNCTION__);

    memset(&mConfig, 0, sizeof(mConfig));
    memset(&mStreamAttributeTarget, 0, sizeof(mStreamAttributeTarget));
    memset(&mComprConfig, 0, sizeof(mComprConfig));
}


AudioALSAPlaybackHandlerBase::~AudioALSAPlaybackHandlerBase()
{
    ALOGV("%s()", __FUNCTION__);
}


status_t AudioALSAPlaybackHandlerBase::ListPcmDriver(const unsigned int card,const unsigned int device)
{
    struct pcm_params *params;
    unsigned int min,max ;
    params = pcm_params_get(card, device,  PCM_OUT);
    if (params == NULL)
    {
        ALOGW("Device does not exist.\n");
    }
    min = pcm_params_get_min(params, PCM_PARAM_RATE);
    max = pcm_params_get_max(params, PCM_PARAM_RATE);
    ALOGV("        Rate:\tmin=%uHz\tmax=%uHz\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_CHANNELS);
    max = pcm_params_get_max(params, PCM_PARAM_CHANNELS);
    ALOGV("    Channels:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_SAMPLE_BITS);
    max = pcm_params_get_max(params, PCM_PARAM_SAMPLE_BITS);
    ALOGV(" Sample bits:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_PERIOD_SIZE);
    max = pcm_params_get_max(params, PCM_PARAM_PERIOD_SIZE);
    ALOGV(" Period size:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_PERIODS);
    max = pcm_params_get_max(params, PCM_PARAM_PERIODS);
    ALOGV("Period count:\tmin=%u\t\tmax=%u\n", min, max);
    max = pcm_params_get_max(params, PCM_PARAM_BUFFER_SIZE);
    ALOGV("PCM_PARAM_BUFFER_SIZE :\t max=%u\t\n", max);
    max = pcm_params_get_max(params, PCM_PARAM_BUFFER_BYTES);
    ALOGV("PCM_PARAM_BUFFER_BYTES :\t max=%u\t\n", max);
    pcm_params_free(params);

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::openPcmDriver(const unsigned int card, const unsigned int device)
{
    ALOGV("+%s(), pcm device = %d", __FUNCTION__, device);

    ASSERT(mPcm == NULL);
    mPcm = pcm_open(card, device, PCM_OUT | PCM_MONOTONIC, &mConfig);
    if (mPcm == NULL)
    {
        ALOGE("%s(), mPcm == NULL!!", __FUNCTION__);
    }
    else if (pcm_is_ready(mPcm) == false)
    {
        ALOGE("%s(), pcm_is_ready(%p) == false due to %s, close pcm.", __FUNCTION__, mPcm, pcm_get_error(mPcm));
        pcm_close(mPcm);
        mPcm = NULL;
    }

    ALOGV("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    ASSERT(mPcm != NULL);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::closePcmDriver()
{
    ALOGV("+%s(), mPcm = %p", __FUNCTION__, mPcm);

    if (mPcm != NULL)
    {
        pcm_stop(mPcm);
        pcm_close(mPcm);
        mPcm = NULL;
    }

    ALOGV("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::openComprDriver(const unsigned int device)
{
    ALOGD("+%s(), compr device = %d", __FUNCTION__, device);
    ASSERT(mComprStream == NULL);
    mComprStream = compress_open(AudioALSADeviceParser::getInstance()->GetCardIndex(),
                                 device, COMPRESS_IN, &mComprConfig);
    if (mComprStream == NULL)
    {
        ALOGE("%s(), mComprStream == NULL!!", __FUNCTION__);
    }
    else if (is_compress_ready(mComprStream) == false)
    {
        ALOGE("%s(), compress device open fail:%s", __FUNCTION__, compress_get_error(mComprStream));
        compress_close(mComprStream);
        mComprStream = NULL;
    }
    ALOGD("-%s(), mComprStream = %p", __FUNCTION__, mComprStream);
    ASSERT(mComprStream != NULL);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::closeComprDriver()
{
    ALOGD("+%s(), mComprStream = %p", __FUNCTION__, mComprStream);

    if(mComprStream!=NULL)
    {
        //close compress driver
        compress_stop(mComprStream);
        compress_close(mComprStream);
        mComprStream = NULL;
    }

    ALOGD("-%s(), mComprStream = %p", __FUNCTION__, mComprStream);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::setComprCallback(stream_callback_t StreamCbk, void *CbkCookie)
{
    mStreamCbk = StreamCbk;
    mCbkCookie = CbkCookie;
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::doStereoToMonoConversionIfNeed(void *buffer, size_t bytes)
{
    if (!mHardwareResourceManager->isStereoSpeaker())
    {
        if (mStreamAttributeSource->output_devices & AUDIO_DEVICE_OUT_SPEAKER)
        {
            if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT)
            {
                int32_t *Sample = (int32_t *)buffer;
                while (bytes > 0)
                {
                    int32_t averageValue = ((*Sample) >> 1) + ((*(Sample + 1)) >> 1);
                    *Sample++ = averageValue;
                    *Sample++ = averageValue;
                    bytes -= 8;
                }
            }
            else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT)
            {
                int16_t *Sample = (int16_t *)buffer;
                while (bytes > 0)
                {
                    int16_t averageValue = ((*Sample) >> 1) + ((*(Sample + 1)) >> 1);
                    *Sample++ = averageValue;
                    *Sample++ = averageValue;
                    bytes -= 4;
                }
            }
        }
    }
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::setScreenState(bool mode, size_t buffer_size, size_t reduceInterruptSize, bool bforce)
{
    (void)mode;
    (void)buffer_size;
    (void)reduceInterruptSize;
    (void)bforce;
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::getHardwareBufferInfo(time_info_struct_t *HWBuffer_Time_Info)
{
    status_t ret = UNKNOWN_ERROR;

    if (mPcm)
    {
        if (pcm_get_htimestamp(mPcm, &HWBuffer_Time_Info->frameInfo_get, &HWBuffer_Time_Info->timestamp_get) == 0)
        {
            size_t hw_buf_size_frame = pcm_bytes_to_frames(mPcm, mStreamAttributeTarget.buffer_size);
            //remaining frames in hardware buffer
            HWBuffer_Time_Info->buffer_per_time = hw_buf_size_frame;
            ret = NO_ERROR;
            ALOGVV("%s frameInfo_get = %d hw_buf_size_frame = %d buffer_per_time = %d\n",
                   __FUNCTION__, HWBuffer_Time_Info->frameInfo_get, hw_buf_size_frame, HWBuffer_Time_Info->buffer_per_time);
        }
        else
        {
            ALOGV("%s pcm_get_htimestamp fail %s\n", __FUNCTION__, pcm_get_error(mPcm));
        }
    }
    else if (mPlaybackHandlerType == PLAYBACK_HANDLER_VOICE)
    {
        //for background sound no pcm handler case
        clock_gettime(CLOCK_MONOTONIC, &HWBuffer_Time_Info->timestamp_get);
        ret = NO_ERROR;
    }
    else
    {
        ALOGE("%s unexpected condition\n", __FUNCTION__);
    }

    return ret;
}

status_t AudioALSAPlaybackHandlerBase::get_timeStamp(unsigned long *frames, unsigned int *samplerate)
{
    if(mComprStream == NULL)
    {
        ALOGE("%s(), mComprStream NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

	if(compress_get_tstamp(mComprStream, frames, samplerate) == 0)
    {
        ALOGV("%s(), frames:%lu, samplerate:%u", __FUNCTION__, *frames, *samplerate);
        return NO_ERROR;
    }
    else
    {
        ALOGE("%s get_tstamp fail %s\n", __FUNCTION__, compress_get_error(mComprStream));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

playback_handler_t AudioALSAPlaybackHandlerBase::getPlaybackHandlerType()
{
    return mPlaybackHandlerType;
}

status_t AudioALSAPlaybackHandlerBase::setFilterMng(AudioMTKFilterManager *pFilterMng)
{
    (void)pFilterMng;
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBase::initPostProcessing()
{
    // init post processing
    mPostProcessingOutputBufferSize = mStreamAttributeSource->buffer_size;
    mPostProcessingOutputBuffer = new char[mPostProcessingOutputBufferSize];
    ASSERT(mPostProcessingOutputBuffer != NULL);

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::deinitPostProcessing()
{
    // deinit post processing
    if (mPostProcessingOutputBuffer)
    {
        delete [] mPostProcessingOutputBuffer;
        mPostProcessingOutputBuffer = NULL;
        mPostProcessingOutputBufferSize = 0;
    }

    if (mAudioFilterManagerHandler)
    {
        mAudioFilterManagerHandler->stop();
        mAudioFilterManagerHandler = NULL;
    }

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::doPostProcessing(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes)
{
    // bypass downlink filter while DMNR tuning
#ifdef DOWNLINK_LOW_LATENCY
    if (mAudioFilterManagerHandler && mStreamAttributeSource->BesRecord_Info.besrecord_dmnr_tuningEnable == false && mStreamAttributeSource->bBypassPostProcessDL == false && !(mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST))
#else
    if (mAudioFilterManagerHandler && mStreamAttributeSource->BesRecord_Info.besrecord_dmnr_tuningEnable == false && mStreamAttributeSource->bBypassPostProcessDL == false)
#endif
    {
        mAudioFilterManagerHandler->start(mFirstDataWrite);
        uint32_t outputSize = mAudioFilterManagerHandler->process(pInBuffer, inBytes, mPostProcessingOutputBuffer, mPostProcessingOutputBufferSize);
        if (outputSize == 0)
        {
            *ppOutBuffer = pInBuffer;
            *pOutBytes = inBytes;
        }
        else
        {
            *ppOutBuffer = mPostProcessingOutputBuffer;
            *pOutBytes = outputSize;
        }
    }
    else // bypass
    {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }

    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::initBliSrc()
{
    // init BLI SRC if need
    if (mStreamAttributeSource->sample_rate  != mStreamAttributeTarget.sample_rate  ||
        mStreamAttributeSource->num_channels != mStreamAttributeTarget.num_channels)
    {
        ALOGV("%s(), sample_rate: %d => %d, num_channels: %d => %d, mStreamAttributeSource->audio_format: 0x%x", __FUNCTION__,
              mStreamAttributeSource->sample_rate,  mStreamAttributeTarget.sample_rate,
              mStreamAttributeSource->num_channels, mStreamAttributeTarget.num_channels,
              mStreamAttributeSource->audio_format);

        SRC_PCM_FORMAT src_pcm_format = SRC_IN_Q1P15_OUT_Q1P15;
        if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT)
        {
            src_pcm_format = SRC_IN_Q1P31_OUT_Q1P31;
        }
        else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT)
        {
            src_pcm_format = SRC_IN_Q1P15_OUT_Q1P15;
        }
        else
        {
            ALOGE("%s(), not support mStreamAttributeSource->audio_format(0x%x) SRC!!", __FUNCTION__, mStreamAttributeSource->audio_format);
        }

        mBliSrc = newMtkAudioSrc(
            mStreamAttributeSource->sample_rate, mStreamAttributeSource->num_channels,
            mStreamAttributeTarget.sample_rate,  mStreamAttributeTarget.num_channels,
            src_pcm_format);
        ASSERT(mBliSrc != NULL);
        mBliSrc->open();

        mBliSrcOutputBuffer = new char[kBliSrcOutputBufferSize];
        ASSERT(mBliSrcOutputBuffer != NULL);
    }

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::deinitBliSrc()
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

status_t AudioALSAPlaybackHandlerBase::doBliSrc(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes)
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

        ALOGVV("%s(), num_raw_data_left = %u, num_converted_data = %u",
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

pcm_format AudioALSAPlaybackHandlerBase::transferAudioFormatToPcmFormat(const audio_format_t audio_format) const
{
    pcm_format retval = PCM_FORMAT_S16_LE;

    switch (audio_format)
    {
        case AUDIO_FORMAT_PCM_8_BIT:
        {
            retval = PCM_FORMAT_S8;
            break;
        }
        case AUDIO_FORMAT_PCM_16_BIT:
        {
            retval = PCM_FORMAT_S16_LE;
            break;
        }
        case AUDIO_FORMAT_PCM_8_24_BIT:
        {
            retval = PCM_FORMAT_S24_LE;
            break;
        }
        case AUDIO_FORMAT_PCM_32_BIT:
        {
            retval = PCM_FORMAT_S32_LE;
            break;
        }
        default:
        {
            ALOGW("No such audio format(0x%x)!! Use AUDIO_FORMAT_PCM_16_BIT(0x%x) instead", audio_format, PCM_FORMAT_S16_LE);
            retval = PCM_FORMAT_S16_LE;
            break;
        }
    }

    ALOGV("%s(), audio_format(0x%x) => pcm_format(0x%x)", __FUNCTION__, audio_format, retval);
    return retval;
}

status_t AudioALSAPlaybackHandlerBase::initBitConverter()
{
    // init bit converter if need
    if (mStreamAttributeSource->audio_format != mStreamAttributeTarget.audio_format)
    {
        BCV_PCM_FORMAT bcv_pcm_format = BCV_IN_Q1P31_OUT_Q1P15;
        if ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) || (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_8_24_BIT))
        {
            if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT)
            {
                bcv_pcm_format = BCV_IN_Q1P31_OUT_Q1P15;
            }
            else if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
            {
                bcv_pcm_format = BCV_IN_Q1P31_OUT_Q9P23;
            }
        }
        else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT)
        {
            if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_32_BIT)
            {
                bcv_pcm_format = BCV_IN_Q1P15_OUT_Q1P31;
            }
            else if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
            {
                bcv_pcm_format = BCV_IN_Q1P15_OUT_Q9P23;
            }
        }
        else if(mStreamAttributeSource->audio_format == AUDIO_FORMAT_MP3)   //doug for tunneling
        {
            if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT)
            {
                return NO_ERROR;
            }
            else if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
            {
                bcv_pcm_format = BCV_IN_Q1P15_OUT_Q9P23;
            }
        }
        else
        {
            ASSERT(0);
        }

        ALOGV("%s(), audio_format: 0x%x => 0x%x, bcv_pcm_format = 0x%x",
              __FUNCTION__, mStreamAttributeSource->audio_format, mStreamAttributeTarget.audio_format, bcv_pcm_format);

        if (mStreamAttributeSource->num_channels > 2)
        {
            mBitConverter = newMtkAudioBitConverter(
                mStreamAttributeSource->sample_rate,
                2,
                bcv_pcm_format);
        }
        else
        {
            mBitConverter = newMtkAudioBitConverter(
                mStreamAttributeSource->sample_rate,
                mStreamAttributeSource->num_channels,
                bcv_pcm_format);
        }

        ASSERT(mBitConverter != NULL);
        mBitConverter->open();
        mBitConverter->resetBuffer();

        mBitConverterOutputBuffer = new char[kMaxPcmDriverBufferSize];
        ASSERT(mBitConverterOutputBuffer != NULL);
    }

    ALOGV("%s(), mBitConverter = %p, mBitConverterOutputBuffer = %p", __FUNCTION__, mBitConverter, mBitConverterOutputBuffer);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::deinitBitConverter()
{
    // deinit bit converter if need
    if (mBitConverter != NULL)
    {
        mBitConverter->close();
        deleteMtkAudioBitConverter(mBitConverter);
        mBitConverter = NULL;
    }

    if (mBitConverterOutputBuffer != NULL)
    {
        delete[] mBitConverterOutputBuffer;
        mBitConverterOutputBuffer = NULL;
    }

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::doBitConversion(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes)
{
    if (mBitConverter != NULL)
    {
        *pOutBytes = kPcmDriverBufferSize;
        mBitConverter->process(pInBuffer, &inBytes, (void *)mBitConverterOutputBuffer, pOutBytes);
        *ppOutBuffer = mBitConverterOutputBuffer;
    }
    else
    {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }

    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}

// we assue that buufer should write as 64 bytes align , so only src handler is create,
// will cause output buffer is not 64 bytes align
status_t AudioALSAPlaybackHandlerBase::initDataPending()
{
    ALOGV("mBliSrc = %p",mBliSrc);
    if(mBliSrc != NULL)
    {
        mdataPendingOutputBufferSize = (1024*128) + dataAlignedSize;// here nned to cover max write buffer size
        mdataPendingOutputBuffer = new char[mdataPendingOutputBufferSize];
        mdataPendingTempBuffer  = new char[dataAlignedSize];
        ASSERT(mdataPendingOutputBufferSize != 0);
    }
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::DeinitDataPending()
{
    ALOGV("DeinitDataPending");
    if(mdataPendingOutputBuffer != NULL)
    {
        delete[] mdataPendingOutputBuffer;
        mdataPendingOutputBuffer = NULL;
    }
    ALOGV("delete mdataPendingTempBuffer");
    if(mdataPendingTempBuffer != NULL)
    {
        delete[] mdataPendingTempBuffer ;
        mdataPendingTempBuffer = NULL;
    }
    mdataPendingOutputBufferSize = 0;
    mdataPendingRemindBufferSize = 0;
    return NO_ERROR;
}

// we assue that buufer should write as 64 bytes align , so only src handler is create,
// will cause output buffer is not 64 bytes align
status_t AudioALSAPlaybackHandlerBase::dodataPending(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes)
{
    char *DataPointer = (char*)mdataPendingOutputBuffer;
    char *DatainputPointer = (char*)pInBuffer;
    uint32 TotalBufferSize  =inBytes+mdataPendingRemindBufferSize;
    uint32 tempRemind = TotalBufferSize %dataAlignedSize;
    uint32 TotalOutputSize = TotalBufferSize - tempRemind;
    uint32 TotalOutputCount = TotalOutputSize;
    if (mBliSrc != NULL )  // do data pending
     {
         ALOGV("inBytes = %d mdataPendingRemindBufferSize = %d TotalOutputSize = %d",inBytes,mdataPendingRemindBufferSize,TotalOutputSize);
         if(mdataPendingRemindBufferSize != 0) // deal previous remaind buffer
         {
             memcpy((void*)DataPointer,(void*)mdataPendingTempBuffer,mdataPendingRemindBufferSize);
             DataPointer += mdataPendingRemindBufferSize;
             TotalOutputCount -= mdataPendingRemindBufferSize;
         }

         //deal with input buffer
         memcpy((void*)DataPointer,pInBuffer,TotalOutputCount);
         DataPointer+= TotalOutputCount;
         DatainputPointer += TotalOutputCount;
         TotalOutputCount =0;

         // update pointer and data count
         *ppOutBuffer = mdataPendingOutputBuffer;
         *pOutBytes = TotalOutputSize;

         ALOGV("tempRemind = %d pOutBytes = %d",tempRemind,*pOutBytes);

         // deal with remind buffer
         memcpy((void*)mdataPendingTempBuffer,(void*)DatainputPointer,tempRemind);
         mdataPendingRemindBufferSize = tempRemind;

     }
     else
     {
         *ppOutBuffer = pInBuffer;
         *pOutBytes = inBytes;
     }

     ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
     return NO_ERROR;
}

void AudioALSAPlaybackHandlerBase::OpenPCMDump(const char *class_name)
{
    ALOGV("%s()", __FUNCTION__);
    String8 dump_file_name;
    dump_file_name.appendFormat("%s.%d.%s.%d_%dch_format(0x%x).pcm", streamout, mDumpFileNum, class_name,
                                mStreamAttributeTarget.sample_rate, mStreamAttributeTarget.num_channels,
                                mStreamAttributeTarget.audio_format);

    mPCMDumpFile = AudioOpendumpPCMFile(dump_file_name.string(), streamout_propty);
    if (mPCMDumpFile != NULL)
    {
        ALOGV("%s DumpFileName = %s", __FUNCTION__, dump_file_name.string());

        mDumpFileNum++;
        mDumpFileNum %= MAX_DUMP_NUM;
    }
}

void AudioALSAPlaybackHandlerBase::ClosePCMDump()
{
    ALOGV("%s()", __FUNCTION__);
    if (mPCMDumpFile)
    {
        AudioCloseDumpPCMFile(mPCMDumpFile);
    }
}

void  AudioALSAPlaybackHandlerBase::WritePcmDumpData(const void *buffer, ssize_t bytes)
{
    if (mPCMDumpFile)
    {
        AudioDumpPCMData((void *)buffer , bytes, mPCMDumpFile);
    }
}

status_t AudioALSAPlaybackHandlerBase::initNLEProcessing()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBase::deinitNLEProcessing()
{
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBase::doNLEProcessing(void *pInBuffer __unused, size_t inBytes __unused)
{
    return INVALID_OPERATION;
}

} // end of namespace android
