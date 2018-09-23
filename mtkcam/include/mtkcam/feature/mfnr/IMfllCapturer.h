#ifndef __IMFLLCAPTURER_H__
#define __IMFLLCAPTURER_H__

#include "MfllDefs.h"
#include "MfllTypes.h"
#include "IMfllEvents.h"
#include "IMfllImageBuffer.h"

#include <utils/RefBase.h> // android::RefBase
#include <vector>

/* give a symbol here avoid nested include */
namespace mfll {
    class IMfllCore;
};

namespace mfll {
/**
 *  MfllCapturer has responsibility to provide YUV and QYUV frames for MfllCore.
 *  Usually, capturer should accept RAW buffers from outside, so here we provide
 *  an interface: IMfllCapturer::queueFullSizeRaw to queue full size RAW to
 *  capturer which should be thread-safe function.
 *
 *  Furthurmore, IMfllCapturer has responsibility to invoke
 *  MfllCore::doAllocRawBuffer
 *  MfllCore::doAllocQyuvBuffer
 *  while capturing in the implementation
 */
class IMfllCapturer : public android::RefBase {
public:
    /**
     *  Create Instance
     *
     *  @retrun                     - Instance of IMfllCapture
     */
    static IMfllCapturer* createInstance(void);

    /**
     *  Destroy instance using android::RefBase::decStrong(void*)
     */
    virtual void destroyInstance(void);

public:
    /**
     *  Capture frames with synchronized call. The caller thread will be blocked until
     *  frame capturing has been done.
     *
     *  @param[out] raws             - A container will contain captured frames if returns MfllErr_Ok.
     *                                 Caller have to prepare IMfllImageBuffer with the same width and
     *                                 height to capture.
     *  @param[out] qyuvs            - A container will contain captured framees in QYUV format.
     *  @param[out] gmvs             - A container will contain global motion vector information.
     *  @param[out] status           - A container contains results of every frame. If failed, the status
     *                                 will be non-zero
     *  @return                      - If succeed returns MfllErr_Ok.
     *  @note                        - After invoked this method and returns Ok, the frame number will
     *                                 be updated. Invoke getCapturedFrameNum to retrieve frame number.
     *                                 All vector size is supposed to be the same but size of parameter
     *                                 "gmvs" should be (n-1).
     */
    virtual enum MfllErr captureFrames(
        unsigned int captureFrameNum,
        std::vector< sp<IMfllImageBuffer>* >    &raws,
        std::vector< sp<IMfllImageBuffer>* >    &qyuvs,
        std::vector< MfllMotionVector_t* >      &gmvs,
        std::vector<int>                        &status
    ) = 0;

    /**
     *  Capture frames with synchronized call. The caller thread will be blocked until
     *  frame capturing has been done.
     *  @param[out] raws
     *  @param[out] yuvs
     *  @param[out] qyuvs
     *  @param[out] gmvs
     *  @param[out] status           - A container contains results of every frame. If failed, the status
     *                                 will be non-zero
     *  @return
     *  @note
     */
    virtual enum MfllErr captureFrames(
        unsigned int captureFrameNum,
        std::vector< sp<IMfllImageBuffer>* >    &raws,
        std::vector< sp<IMfllImageBuffer>* >    &yuvs,
        std::vector< sp<IMfllImageBuffer>* >    &qyuvs,
        std::vector< MfllMotionVector_t* >      &gmvs,
        std::vector<int>                        &status
    ) = 0;

    /**
     *  Retrieve captured frame number.
     *
     *  @return                 - This method will be avaiable when frames are captured, or retruns
     *                            0 as initial state.
     */
    virtual unsigned int getCapturedFrameNum(void) = 0;

    /**
     *  Give reference of IMfllCore
     *  @param c                - IMfllCore address
     */
    virtual void setMfllCore(IMfllCore *c) = 0;

    /**
     *  Tell MfllMfb what mode is using now
     *  @param mode             - Mode to tell, default is MfllMode_NormalMfll
     */
    virtual void setShotMode(const enum MfllMode &mode) = 0;

    /**
     *  Tell MfllMfb the post NR type is using
     *  @param nrType           - NoiseReductionType
     */
    virtual void setPostNrType(const enum NoiseReductionType &nrType) = 0;

    /**
     *  MfllCapturer will dispatch events therefore the IMfllEvents is necessary to be set.
     *  Without setting event dispatcher, the related events won't be sent.
     *
     *  @param e                 - A strong pointer of IMfllEvents reference.
     *  @return                  - If succeed returns MfllErr_Ok.
     */
    virtual enum MfllErr registerEventDispatcher(const sp<IMfllEvents> &e) = 0;

    /**
     *  To queue full size RAW to capturer if the RAW buffer is provided from
     *  other module.
     *
     *  @param img              - IMfllImageBuffer contains full size RAW buffer
     *  @return                 - If succeed returns MfllErr_Ok.
     */
    virtual enum MfllErr queueFullSizeRaw(sp<IMfllImageBuffer> &img) = 0;

    /**
     *  To queue GMV (Global Motion Vector) to capturer if the GMV is provided
     *  from other module.
     *  @param mv               - Gloval motion vector
     *  @return                 - If succeed returns MfllErr_Ok.
     */
    virtual enum MfllErr queueGmv(const MfllMotionVector_t &mv) = 0;

protected:
    virtual ~IMfllCapturer(void){};
};
}; /* namespace mfll */
#endif//__IMFLLCAPTURER_H__

