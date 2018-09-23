#ifndef __MFLLTYPES_H__
#define __MFLLTYPES_H__

#include "MfllDefs.h"

// LINUX
#include <pthread.h> // pthread_mutex_t
#include <sys/prctl.h> // prctl(PR_SET_NAME)

// STL
#include <string> // std::string
#include <map> // std::map

namespace mfll {

    /* Mfll error code */
    enum MfllErr
    {
        MfllErr_Ok = 0,
        MfllErr_Shooted,
        MfllErr_AlreadyExist,
        MfllErr_NotInited,
        MfllErr_BadArgument,
        MfllErr_IllegalBlendFrameNum,
        MfllErr_IllegalCaptureFrameNum,
        MfllErr_NullPointer,
        MfllErr_NotImplemented,
        MfllErr_NotSupported,
        /* This error code indicates to instance creation is failed */
        MfllErr_CreateInstanceFailed,
        /* Load image failed */
        MfllErr_LoadImageFailed,
        /* Save image failed */
        MfllErr_SaveImageFailed,
        /* Others error will be categoried here */
        MfllErr_UnexpectedError,
        /* indicates to size only */
        MfllErr_Size,
    };

    /**
     *  Mfll mode, bit map
     *    - bit 0: Zsd on or off
     *    - bit 1: is MFB
     *    - bit 2: is AIS
     *    - bit 3: is Single Capturer
     * */
    enum MfllMode
    {
        MfllMode_NormalMfll         = 0x02,
        MfllMode_ZsdMfll            = 0x03,
        MfllMode_NormalAis          = 0x04,
        MfllMode_ZsdAis             = 0x05,
        MfllMode_NormalSingleFrame  = 0x08,
        MfllMode_ZsdSingleFrame     = 0x09,
        /* Bits */
        MfllMode_Bit_Zsd            = 0x00,
        MfllMode_Bit_Mfll           = 0x01,
        MfllMode_Bit_Ais            = 0x02,
        MfllMode_Bit_SingleFrame    = 0x03,
    };

    /**
     *  An enumeration describes buffers that MFLL core uses
     */
    enum MfllBuffer
    {
        /* Captured raw, amount is: (capturedFrameNum - 1) */
        MfllBuffer_Raw = 0,
        /* QSize yuv, converted from captured raw, amount is: (capturedFrameNum - 1) */
        MfllBuffer_QYuv,
        /* Full yuv, converted from captured raw, amount is: (capturedFrameNum - 1) */
        MfllBuffer_FullSizeYuv, // new one
        /* Postview yuv */
        MfllBuffer_PostviewYuv,
        /* Get the base YUV frame of the current stage */
        MfllBuffer_BaseYuv,
        /* Get the reference frame of the current stage */
        MfllBuffer_ReferenceYuv,
        /* Get the golden frame of the current stage */
        MfllBuffer_GoldenYuv,
        /* Get the blending(output) frame of the current stage */
        MfllBuffer_BlendedYuv,
        /* Get the mixed output(final output) frame of current stage */
        MfllBuffer_MixedYuv,
        /* Get the input weighting table of the current stage */
        MfllBuffer_WeightingIn,
        /* Get the output weighting table of the current stage */
        MfllBuffer_WeightingOut,
        /* Memc working buffer, amount is: (blendingFrameNum -1) */
        MfllBuffer_AlgorithmWorking, // Memc working buffer, with index

        /* size */
        MfllBuffer_Size
    };

    /* Image format that Mfll will invoke */
    enum ImageFormat
    {
        ImageFormat_Yuy2 = 0,   // YUV422:  size in byte = 2 x @ImageSensor
        ImageFormat_Raw10,      // RAW10:   size in byte = 1.25 x @ImageSensor
        ImageFormat_Raw8,       // for weighting table, size = 1 x @ImageSensor
        ImageFormat_Yv16,       // YUV422: size in byte = 2 x @ImageSensor
        ImageFormat_Y8,         // Y8: size in byte = 1 x @ImageSensor
        ImageFormat_Nv21,       // NV21:
        /* size */
        ImageFormat_Size
    };

    /**
     *  MFNR may have chance to apply SWNR or HWNR.
     */
    enum NoiseReductionType
    {
        NoiseReductionType_None = 0, // No NR applied
        NoiseReductionType_SWNR, // Applied SWNR after MFNR
        NoiseReductionType_HWNR, // Applied HWNR after MFNR
        /* size */
        NoiseReductionType_Size
    };

    /* describes MFLL RAW to YUV stage. */
    enum YuvStage
    {
        YuvStage_RawToYuy2 = 0, // Stage 1
        YuvStage_RawToYv16,     // Stage 1
        YuvStage_BaseYuy2,      // Stage 1
        YuvStage_GoldenYuy2,    // Stage 3
        /* size */
        YuvStage_Size
    };

    /**
     *  MFLL provides an event classes, these are event that MFLL will invoked.
     *  If not specified param1 or param2, it will be 0.
     */
    enum EventType
    {
        EventType_Init = 0,
        EventType_AllocateRawBuffer, /* param1: integer, index of buffer */
        EventType_AllocateQyuvBuffer, /* param1: integer, index of buffer */
        EventType_AllocateYuvBase,
        EventType_AllocateYuvGolden,
        EventType_AllocateYuvWorking,
        EventType_AllocateYuvMcWorking,
        EventType_AllocateYuvMixing,
        EventType_AllocateWeighting, /* param1: integer, index of weighting table */
        EventType_AllocateMemc, /* param1: integer, index of memc working buffer */
        EventType_Capture, /* invoke when start capturing frame and all frames are captured */
        EventType_CaptureRaw, /* param1: integer, index of captured RAW */
        EventType_CaptureYuvQ, /* param1: integer, index of captured Q size Yuv */
        EventType_CaptureEis, /* param1: integer, index of captured Eis info */
        EventType_Bss, /* param1: reference of an integer, represents number of frame to do BSS */
        EventType_EncodeYuvBase,
        EventType_EncodeYuvGolden,
        EventType_MotionEstimation, /* param1: integer, index of buffer */
        EventType_MotionCompensation,
        EventType_Blending,
        EventType_Mixing,
        EventType_Destroy,
        /* size */
        EventType_Size,
    };

    enum MemcMode
    {
        MemcMode_Sequential = 0,
        MemcMode_Parallel,
        /* size */
        MemcMode_Size
    };

    /* RWB sensor support mode */
    enum RwbMode
    {
        RwbMode_None = 0,
        RebMode_Mdp,
        RwbMode_GPU,
        /* size */
        RwbMode_Size
    };

    /* Memory Reduce Plan mode */
    enum MrpMode
    {
        MrpMode_BestPerformance = 0,
        MrpMode_Balance,
        /* size */
        MrpMode_Size
    };
//
//-----------------------------------------------------------------------------
//
    typedef struct MfllCoreDbgInfo
    {
        unsigned int frameCapture;
        unsigned int frameBlend;
        unsigned int iso; // saves iso to capture
        unsigned int exp; // saves exposure to capture
        unsigned int ori_iso;
        unsigned int ori_exp;
        unsigned int width; // processing image size
        unsigned int height;
        unsigned int bss_enable; // is bss applied
        unsigned int memc_skip; // bits indicates to skipped frame.
        unsigned int shot_mode;

        MfllCoreDbgInfo () noexcept
        {
            frameCapture = 4;
            frameBlend = 4;
            iso = 0;
            exp = 0;
            ori_iso = 0;
            ori_exp = 0;
            width = 0;
            height = 0;
            bss_enable = 0;
            memc_skip = 0;
            shot_mode = 0;
        }
    } MfllCoreDbgInfo_t;

    typedef struct MfllRect
    {
        int x;
        int y;
        int w;
        int h;

        MfllRect() noexcept : x(0), y(0), w(0), h(0) {}
        MfllRect(int x, int y, int w, int h) noexcept : x(x), y(y), w(w), h(h) {}
        size_t size() const noexcept { return static_cast<size_t>(w * h); }
    } MfllRect_t;

    /* sync object */
    typedef struct MfllSyncObj
    {
        pthread_mutex_t trigger;
        pthread_mutex_t done;
        MfllSyncObj(void) noexcept
            : trigger(PTHREAD_MUTEX_INITIALIZER)
            , done(PTHREAD_MUTEX_INITIALIZER)
        {
        }
    } MfllSyncObj_t;

    typedef struct MfllMotionVector
    {
        int x;
        int y;

        MfllMotionVector(void) noexcept
            : x (0), y (0) {}
    } MfllMotionVector_t;

    typedef struct MfllEventStatus
    {
        int ignore;
        enum MfllErr err;

        MfllEventStatus(void) noexcept
            : ignore(0)
            , err(MfllErr_Ok) {}
    } MfllEventStatus_t;

    /* Mfll bypass option */
    typedef struct MfllBypassOption
    {
        unsigned int bypassAllocRawBuffer[MFLL_MAX_FRAMES];
        unsigned int bypassAllocQyuvBuffer[MFLL_MAX_FRAMES];
        unsigned int bypassAllocYuvBase;
        unsigned int bypassAllocYuvGolden;
        unsigned int bypassAllocYuvWorking;
        unsigned int bypassAllocYuvMcWorking;
        unsigned int bypassAllocYuvMixing;
        unsigned int bypassAllocWeighting[2];
        unsigned int bypassAllocMemc[MFLL_MAX_FRAMES];
        unsigned int bypassAllocPostview;
        unsigned int bypassCapture;
        unsigned int bypassBss;
        unsigned int bypassEncodeYuvBase;
        unsigned int bypassEncodeYuvGolden;
        unsigned int bypassMotionEstimation[MFLL_MAX_FRAMES];
        unsigned int bypassMotionCompensation[MFLL_MAX_FRAMES];
        unsigned int bypassBlending[MFLL_MAX_FRAMES];
        unsigned int bypassMixing;
        MfllBypassOption(void) noexcept
            : bypassAllocRawBuffer{0}
            , bypassAllocQyuvBuffer{0}
            , bypassAllocYuvBase(0)
            , bypassAllocYuvGolden(0)
            , bypassAllocYuvWorking(0)
            , bypassAllocYuvMcWorking(0)
            , bypassAllocYuvMixing(0)
            , bypassAllocWeighting{0}
            , bypassAllocMemc{0}
            , bypassAllocPostview(0)
            , bypassCapture(0)
            , bypassBss(0)
            , bypassEncodeYuvBase(0)
            , bypassEncodeYuvGolden(0)
            , bypassMotionEstimation{0}
            , bypassMotionCompensation{0}
            , bypassBlending{0}
            , bypassMixing(0)
        {}
    } MfllBypassOption_t;

    /**
     *  Capture parameter for MFNR
     */
    typedef struct MfllConfig
    {
        int             sensor_id; // opened sensor id
        int             iso; // info of adjuested ISO, which is now applied
        int             exp; // info of adjuested EXP, which is now applied
        int             original_iso; // info of the origianl ISO
        int             original_exp;
        int             blend_num;
        int             capture_num;
        int             full_size_mc;
        enum MfllMode   mfll_mode;
        enum RwbMode    rwb_mode;
        enum MemcMode   memc_mode;
        enum MrpMode    mrp_mode;
        enum NoiseReductionType post_nr_type;

        MfllConfig(void) noexcept
        {
            sensor_id = -1;
            iso = 0;
            exp = 0;
            original_iso = 0;
            original_exp = 0;
            blend_num = 4;
            capture_num = 4;
            full_size_mc = 0;
            mfll_mode = static_cast<enum MfllMode>(0);
            rwb_mode = static_cast<enum RwbMode>(RwbMode_None);
            memc_mode = static_cast<enum MemcMode>(MemcMode_Parallel);
            mrp_mode = static_cast<enum MrpMode>(MrpMode_BestPerformance);
            post_nr_type = static_cast<enum NoiseReductionType>(NoiseReductionType_None);
        }
    } MfllConfig_t;

    // for MFNR core library 2
    struct MfllConfig2 : public MfllConfig_t
    {
        int             yuv_sensor;
        MfllConfig2() noexcept
            : MfllConfig_t()
            , yuv_sensor(0)
        {
        }
    };

    typedef struct MfllStrategyConfig
    {
        int     iso;
        int     exp;
        int     original_iso;
        int     original_exp;
        int     frameCapture;
        int     frameBlend;
        int     enableMfb;
        int     isAis;
        int     isFullSizeMc;
        MfllStrategyConfig(void) noexcept
            : iso(0)
            , exp(0)
            , original_iso(0)
            , original_exp(0)
            , frameCapture(4)
            , frameBlend(4)
            , enableMfb(0)
            , isAis(0)
            , isFullSizeMc(0)
        {}
    } MfllStrategyConfig_t;

    typedef struct MfllFeedRawConfig
    {
        std::string version;
        int width;
        int height;
        int frameNum;
        std::map<std::string, std::string> param;
        MfllFeedRawConfig() noexcept
            : width(0), height(0), frameNum(0) {}
    } MfllFeedRawConfig_t;

}; /* namespace mfll */

/******************************************************************************
 * Utilities
 ******************************************************************************/
namespace mfll {

/**
 *  Check if the mode is ZSD or note.
 *  @param m            Mode to check
 *  @return             If the mode is ZSD mode, returns true
 */
inline bool isZsdMode(const enum MfllMode &m) noexcept
{
    return m & (1 << MfllMode_Bit_Zsd); // see MfllMode, bit 0 represents if it's ZSD mode or not
}

#define MFLL_THREAD_NAME(x) mfll::setThreadName(x)
inline void setThreadName(const char* name)
{
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
}

}; /* namespace mfll */
#endif /* __MFLLTYPES_H__ */
