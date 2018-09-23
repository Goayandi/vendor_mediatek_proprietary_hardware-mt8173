#ifndef __IMFLLCORE_H__
#define __IMFLLCORE_H__

#include "MfllDefs.h"
#include "MfllTypes.h"
#include "IMfllCapturer.h"
#include "IMfllEventListener.h"
#include "IMfllEvents.h"
#include "IMfllMfb.h"
#include "IMfllExifInfo.h"

#include <utils/RefBase.h> // android::RefBase
#include <vector>
#include <string>

using android::sp;
using std::vector;

namespace mfll {

class IMfllCore : public android::RefBase {
public:
    /**
     *  Caller should always create instance via INTERFACE::createInstance method.
     *  And caller also have responsibility to destroy it by invoking destroyInstance.
     *  If caller uses android::sp to manager life cycle of instance who doesn't need to invoke
     *  destroyInstance to destroy instance.
     *
     *  @return             - An IMfllCore instance, caller has responsibility to manager it's lifetime.
     */
    static IMfllCore* createInstance(void);

    /**
     *  To destroy instance, but notice that, because MFLL core related classes inherit android::RefBase,
     *  life time is supposed to be managed by strong pointer. It means that the destructor of these classes
     *  will not be invoked if caller invoked destroyInstance. The destructor will be invoked until
     *  the strong reference counts to zero.
     */
    virtual void destroyInstance(void);

    /**
     *  Get EventType name in const char*
     *
     *  @param e            - EventType to be retrieved.
     *  @return             - Name of EventType.
     */
    static const char* getEventTypeName(enum EventType e);

public:
    /**
     *  Init MfllCore, if using MrpMode_BestPerformance, a thread will be create
     *  to allocate memory chunks, notice that, this thread will be joined while
     *  destroying MfllCore.
     *  @param cfg          - MfllConfig_t structure to describe usage
     *  @return             - Returns MfllErr_Ok if works
     */
    virtual enum MfllErr init (const MfllConfig_t &cfg) = 0;
    /**
     *  Run MFLL with specified MfllConfig_t structure from IMfllCore::init.
     *
     *  @return             - If ok returns MfllErr_Ok.
     *  @retval MfllErr_Shooted - This instance is out of date and cannot be triggered again.
     */
    virtual enum MfllErr doMfll() = 0;

    /**
     *  Tell MFLL to cancel this operation
     */
    virtual enum MfllErr doCancel() = 0;

    /**
     *  Get specified blending frame number.
     *
     *  @return             - Frame number to blend
     */
    virtual unsigned int getBlendFrameNum(void) = 0;

    /**
     *  Get the blended frame number
     *  @return             - Frame number of blended
     */
    virtual unsigned int getFrameBlendedNum(void) = 0;

    /**
     *  Get specified capture frame number.
     *
     *  @return             - Frame number to capture
     */
    virtual unsigned int getCaptureFrameNum(void) = 0;

    /**
     *  Get the number of captured frames
     *  @return             - Frame number of captured
     */
    virtual unsigned int getFrameCapturedNum(void) = 0;

    /**
     *  Get instance num of Memc object.
     *
     *  @return             - Number of Memc instance.
     */
    virtual unsigned int getMemcInstanceNum(void) = 0;

    /**
     *  After BSS has been applied, the buffer index will be changed.
     *  To use this function to get the original index by the new index
     */
    virtual unsigned int getIndexByNewIndex(const unsigned int &newIndex) = 0;

    /**
     *  Get MFLL core reversion
     *
     *  @return             - 8 digitals for representing aa.bbb.ccc
     *                        e.g.: v2.0.1, return value will be
     *                        0x02000001
     */
    virtual unsigned int getReversion(void) = 0;

    /**
     *  Get MFLL core reversion in std::string format
     *
     *  @return             - A std::string object contains reversion info.
     *                        e.g.: 2.0.1
     */
    virtual std::string getReversionString(void) = 0;

    /**
     *  Get sensor ID that MFLL is configured as.
     *
     *  @return             - Sensor ID
     *  @note               - Default it returns -1.
     */
    virtual int getSensorId(void) = 0;

    /**
     *  Retrieve if this MFLL instance shooted or notice
     *
     *  @return             - True or false
     *  @note               - This function is thread-safe.
     */
    virtual bool isShooted(void) = 0;

    /**
     *  Determine if MfllCore runs as full size MC
     *  @return             - True or false.
     *  @note               - This function works only if doMfll has been invoked.
     */
    virtual bool isFullSizeMc(void) = 0;

    /**
     *  Determine if MfllCore is using feed RAW mode
     *  @return             - True or false.
     *  @note               - This function works only if IMfllCore::init has
     *                        been invoked.
     */
    virtual bool isFeedRaw(void) = 0;

    /**
     *  To read RAW feeding configuration
     *  @param              - MfllFeedRawConfig_t reference to retrieve.
     *  @return             - MfllErr_Ok for read ok.
     */
    virtual enum MfllErr readFeedRawConfiguration(MfllFeedRawConfig_t *cfg) = 0;

    /**
     *  MFLL provides that event listener mechanism. You can register event listener
     *  to monitor event and pre-process or post-process operation for the event.
     *
     *  @param e            - A IMfllEventListener object with smart pointer protected
     *                        It means the lifetime of this object will be holded by
     *                        MfllCore.
     *  @return             - If ok returns MfllErr_Ok.
     *  @retval MfllErr_Shooted - The MFLL instance has shooted and cannot execute
     *                            this operation anymore.
     *  @note               - Operations in event may block MFLL flow.
     *                      - This operation is thread-safe.
     *                      - Lifetime will be extended or managed by strong pointer.
     */
    virtual enum MfllErr registerEventListener(const sp<IMfllEventListener> &e) = 0;

    /**
     *  Remove registered event from MFLL.
     *  @param *e           - Identification of IMfllEventListener.
     *  @return             - If ok returns MfllErr_Ok
     *  @retval MfllErr_Shooted - The MFLL instance has shooted and cannot execute
     *                            this operation anymore.
     *  @note               - removing events will travelling all registered events
     *                        and find the specified event and remove it. It means
     *                        this operation is high cost, make sure you really
     *                        want to remove event or just can ignore event.
     *                      - This operation is thread-safe.
     */
    virtual enum MfllErr removeEventListener(IMfllEventListener *e) = 0;

    /**
     *  User can set bypass option before calling doMfll.
     *  @param b            - Bypass option instance to describe the bypass operations
     *  @return             - If applied returns MfllErr_Ok.
     *                        If is doing MFLL, this function doesn't work and returns MfllErr_Shooted.
     */
    virtual enum MfllErr setBypassOption(const MfllBypassOption_t &b) = 0;

    /**
     *  To tell MfllCore to set bypass option for single frame cpature mode.
     *  @return             This medoth will always return MfllErr_Ok but change the behavior
     *                      of MfllCore
     *  @note               This method only updates partial options of bypass option, if the
     *                      method "setBypassOption" has been invoked after invoked this method,
     *                      all the bypass setting for single frame capture will be lost.
     *
     *                      And MfllCore will set a flag as single frame capture because the
     *                      ISP setting or control flow will be different
     */
    virtual enum MfllErr setBypassOptionAsSingleFrame() = 0;

    /**
     *  Set capture resolution to MFLL
     *  @param width        - Width of capture image
     *  @param height       - Height of capture image
     *  @return             - If ok returns MfllErr_Ok
     *  @retval MfllErr_Shooted - The MFLL instance has shooted and cannot execute
     *                            this operation anymore
     *  @note               - This operation is thread-safe.
     */
    virtual enum MfllErr setCaptureResolution(unsigned int width, unsigned int height) = 0;

    /**
     *  User may also set capturer to MfllCore. If capturer hasn't been set, MfllCore will
     *  invoke IMfllCapturer::createInstance to create IMfllCapturer instance when need.
     *
     *  @param capturer     - A strong pointer contains reference of IMfllCapturer instance.
     *  @return             - If ok returns MfllErr_Ok
     *  @note               - This operation is thread-safe
     *                        Lifetime of capturer should be handled by android::sp.
     */
    virtual enum MfllErr setCapturer(const sp<IMfllCapturer> &capturer) = 0;

    /**
     *  User may also set a MFB processor to MfllCore. If MFB processor hasn't been set,
     *  MfllCore will invoke IMfllMfb::createInstance to create IMfllMfb instance when need.
     *
     *  @param mfb          - A strong pointer contains reference of IMfllMfb instance.
     *  @return             - If ok returns MfllErr_Ok
     *  @note               - This operation is thread-safe
     */
    virtual enum MfllErr setMfb(const sp<IMfllMfb> &mfb) = 0;

    /**
     *  To register a postview output buffer for MfllCore.
     *  Caller can set postview buffer at anytime but before encoding base YUV, because
     *  MfllCore will encode postview YUV during golden YUV.
     *  @param              - A buffer to contain the postview yuv
     *  @param              - Source crop region, base on full size RAW
     *  @sa                 MfllBuffer_PostviewYuv
     *  @sa                 EventType_EncodeYuvBase
     */
    virtual enum MfllErr setPostviewBuffer(
            const sp<IMfllImageBuffer>&     buffer,
            const MfllRect_t&               srcCropRgn
            ) = 0;

    /**
     *  Returns IMfllMfb instance that IMfllCore is using.
     *
     *  @return             - If there's no IMfllMfb is using, sp<IMfllMfb> contains NULL
     */
    virtual sp<IMfllMfb> getMfb(void) = 0;

    /**
     *  To get IMfllImageBuffer instance in android::sp<IMfllImageBuffer> type.
     *
     *  @param s            - Buffer to release.
     *  @param index        - If the selected buffer is not only just a buffer,
     *                        index will be the index of the serial buffers.
     *  @return             - If this function is fail returns sp<T> which contains NULL pointer.
     *  @sa                 - enum MfllBuffer
     */
    virtual sp<IMfllImageBuffer> retrieveBuffer(const enum MfllBuffer &s, int index = 0) = 0;

    /**
     *  To release buffer using decStrong rather than directly release. When the
     *  reference count of the IMfllImageBuffer instance is counted to 0, it will
     *  be released.
     *
     *  @param s            - Buffer to release.
     *  @param index        - If the selected buffer is not only just a buffer,
     *                        index will be the index of the serial buffers.
     *  @return             - Returns MfllErr_Ok if ok
     *  @sa                 - enum MfllBuffer
     */
    virtual enum MfllErr releaseBuffer(const enum MfllBuffer &s, int index = 0) = 0;

    /**
     *  Update exif info to IMfllExifInfo container
     *  @param key          - Key value is always with type "unsigned int"
     *  @param value        - 32 bits data
     */
    virtual enum MfllErr updateExifInfo(unsigned int key, uint32_t value) = 0;

    /**
     *  Retrieve exif info container
     *  @return             - an unmutable IMfllExifInfo container reference
     */
    virtual const sp<IMfllExifInfo>& getExifContainer() = 0;

protected:
    virtual ~IMfllCore(void) {};

}; /* class IMfllCore */
}; /* namespace mfll */
#endif

