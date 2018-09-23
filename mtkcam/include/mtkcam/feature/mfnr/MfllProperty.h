#ifndef __MFLLFPROPERTY_H__
#define __MFLLFPROPERTY_H__

#include <utils/RefBase.h>
// STL
#include <mutex>

#define MFLL_PROPERTY_ON        1
#define MFLL_PROPERTY_OFF       0
#define MFLL_PROPERTY_DEFAULT   -1

namespace mfll {
    typedef enum tagProperty {
        Property_ForceMfll = 0,
        Property_CaptureNum,
        Property_BlendNum,
        Property_Iso,
        Property_Exposure,
        Property_Mrp,
        Property_Rwb,
        Property_FullSizeMc,
        Property_Bss,
        Property_ForceGmvZero,
        Property_FeedRaw,

        Property_DumpAll,
        Property_DumpRaw,
        Property_DumpYuv,
        Property_DumpMfb,
        Property_DumpMix,
        Property_DumpJpeg,
        Property_DumpPostview,
        Property_DumpExif,

        Property_Size
    } Property_t;

    const char* const PropertyString[Property_Size] = {
        "mediatek.mfll.force",
        "mediatek.mfll.capture_num",
        "mediatek.mfll.blend_num",
        "mediatek.mfll.iso",
        "mediatek.mfll.exp",
        "mediatek.mfll.mrp",
        "mediatek.mfll.rwb",
        "mediatek.mfll.full_size_mc",
        "mediatek.mfll.bss",
        "mediatek.mfll.force_gmv_zero",
        "mediatek.mfll.feed_raw",

        "mediatek.mfll.dump.all",
        "mediatek.mfll.dump.raw",
        "mediatek.mfll.dump.yuv",
        "mediatek.mfll.dump.mfb",
        "mediatek.mfll.dump.mixer",
        "mediatek.mfll.dump.jpeg",
        "mediatek.mfll.dump.postview",
        "mediatek.mfll.dump.exif"
    };

/**
 *  Property Read/Write/Wait
 *
 *  This interface provides a mechanism to read property and with these properties,
 *  some features might be for enabled or not.
 *
 *  All property will be read while creating, and after created, all property
 *  is retrieved from memory. If caller want the property from device realtime,
 *  invoke IMfllProperty::readProperty to get it.
 *
 *  @note This is a thread-safe class
 */
class MfllProperty : public android::RefBase {
public:
    MfllProperty(void);
    virtual ~MfllProperty(void);

/* interface */
public:
    /* To read property from device directly*/
    static int readProperty(const Property_t &t);

    /* To check if force on MFNR */
    static bool isForceMfll(void);

    /* Check if force full size MC, -1 is not to set, use default */
    static int getFullSizeMc(void);

    /* To get frame capture number */
    static int getCaptureNum(void);

    /* To get frame blend number */
    static int getBlendNum(void);

    /* To get force exposure */
    static int getExposure(void);

    /* To get force iso */
    static int getIso(void);

    /* To get if disable BSS (returns 0 for disable) */
    static int getBss(void);

    /* To get if force GMV to zero */
    static int getForceGmvZero(void);

/**
 *  Dump information will be available after MFNR core has been inited
 */
public:
    inline bool isDumpRaw(void)
    { return m_propValue[Property_DumpRaw] == MFLL_PROPERTY_ON ? true : false; }

    inline bool isDumpYuv(void)
    { return m_propValue[Property_DumpYuv] == MFLL_PROPERTY_ON ? true : false; }

    inline bool isDumpMfb(void)
    { return m_propValue[Property_DumpMfb] == MFLL_PROPERTY_ON ? true : false; }

    inline bool isDumpMix(void)
    { return m_propValue[Property_DumpMix] == MFLL_PROPERTY_ON ? true : false; }

    inline bool isDumpJpeg(void)
    { return m_propValue[Property_DumpJpeg] == MFLL_PROPERTY_ON ? true : false; }

    inline bool isDumpPostview(void)
    { return m_propValue[Property_DumpPostview] == MFLL_PROPERTY_ON ? true : false; }

    inline bool isDumpExif(void)
    { return m_propValue[Property_DumpExif] == MFLL_PROPERTY_ON ? true : false; }

public:
    /**
     *  To check if any dump property is specified
     *  @return             Ture for yes.
     */
    bool isDump(void);

    /**
     *  Get the cached value of the specified property
     *  @param t            Propery to get
     */
    int getProperty(const Property_t &t);

    /**
     *  Set the value to the specified property to device and cached memory
     *  @param t            Propery to set
     *  @param v            Value to set
     */
    void setProperty(const Property_t &t , const int &v);

private:
    std::mutex m_mutex; // for operation thread-safe
    int m_propValue[Property_Size]; // saves status of this property

}; /* class MfllProperty */
}; /* namespace mfll */
#endif//__MFLLFPROPERTY_H__
