/********************************************************************************************
 *     LEGAL DISCLAIMER
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION,
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/
#ifndef _STEREO_SIZE_PROVIDER_H_
#define _STEREO_SIZE_PROVIDER_H_

#include <cutils/atomic.h>
#include <utils/Mutex.h>
//#include <imageio/ispio_utility.h> //change header file from utility.h to inormalpipe.h for ip-based
#include <mtkcam/drv/iopipe/CamIO/INormalPipe.h>
#include "stereo_common.h"
#include "pass2_size_data.h"
#include "stereo_setting_provider.h"

#define STEREO_SIZE_PROVIDER_DEBUG

#ifdef STEREO_SIZE_PROVIDER_DEBUG    // Enable debug log.

#undef __func__
#define __func__ __FUNCTION__

#define SIZE_PROVIDER_LOGD(fmt, arg...)    ALOGD("[%s]" fmt, __func__, ##arg)
#define SIZE_PROVIDER_LOGI(fmt, arg...)    ALOGI("[%s]" fmt, __func__, ##arg)
#define SIZE_PROVIDER_LOGW(fmt, arg...)    ALOGW("[%s] WRN(%5d):" fmt, __func__, __LINE__, ##arg)
#define SIZE_PROVIDER_LOGE(fmt, arg...)    ALOGE("[%s] %s ERROR(%5d):" fmt, __func__,__FILE__, __LINE__, ##arg)

#else   // Disable debug log.
#define SIZE_PROVIDER_LOGD(a,...)
#define SIZE_PROVIDER_LOGI(a,...)
#define SIZE_PROVIDER_LOGW(a,...)
#define SIZE_PROVIDER_LOGE(a,...)
#endif  // STEREO_SIZE_PROVIDER_DEBUG

enum ENUM_BUFFER_NAME
{
    //N3D Output
    E_MV_Y,
    E_MASK_M_Y,
    E_SV_Y,
    E_MASK_S_Y,
    E_LDC,

    //N3D before MDP for capture
    E_MV_Y_LARGE,
    E_MASK_M_Y_LARGE,
    E_SV_Y_LARGE,
    E_MASK_S_Y_LARGE,

    //DPE Output
    E_DMP_H,
    E_CFM_H,
    E_RESPO,

    //OCC Output
    E_MY_S,
    E_DMH,

    //WMF Output
    E_DMW,
    E_DEPTH_MAP,

    //Output
    E_DMG,
    E_DMBG,

    //Bokeh Output
    E_BOKEH_WROT, //VSDOF image
    E_BOKEH_WDMA, //Clean image
    E_BOKEH_3DNR,

    //BMDeNoise
    E_BM_PREPROCESS_FULLRAW_CROP_1,
    E_BM_PREPROCESS_FULLRAW_CROP_2,
    E_BM_PREPROCESS_W_1,
    E_BM_PREPROCESS_W_2,
    E_BM_PREPROCESS_MFBO_1,
    E_BM_PREPROCESS_MFBO_2,
    E_BM_PREPROCESS_MFBO_FINAL_1,
    E_BM_PREPROCESS_MFBO_FINAL_2,
    E_BM_DENOISE_HAL_OUT,
    E_BM_DENOISE_HAL_OUT_ROT_BACK,
};

//using namespace NSImageio;
using namespace NSImageio::NSIspio;
using namespace StereoHAL;

class StereoSizeProvider
{
public:
    /**
     * \brief Get instance of size provider
     * \detail Size provider is implemented as singleton
     *
     * \return Instance of size provider
     */
    static StereoSizeProvider *getInstance();

    /**
     * \brief Get pass1 related sizes
     *
     * \param sensor Sensor to get size
     * \param format Format of output image
     * \param port Output port of pass1
     * \param scenario The scenario to get image
     * \param tgCropRect Output TG crop rectangle
     * \param outSize Output size
     * \param strideInBytes Output stride
     * \return true if success
     */
    bool getPass1Size( ENUM_STEREO_SENSOR sensor,
                       EImageFormat format,
                       EPortIndex port,
                       ENUM_STEREO_SCENARIO scenario,
                       MRect &tgCropRect,
                       MSize &outSize,
                       MUINT32 &strideInBytes ) const;

    /**
     * \brief Get pass1 active array cropping rectangle
     * \details AP is working on the coodination of active array
     *
     * \param sensor Sensor to active array
     * \param cropRect Output active array cropping rectangle
     *
     * \return true if suceess
     */
    bool getPass1ActiveArrayCrop(ENUM_STEREO_SENSOR sensor, MRect &cropRect);

    /**
     * \brief Get Pass2-A related size
     * \details The size will be affected by module orientation and image ratio
     *
     * \param round The round of Pass2-A
     * \param eScenario Stereo scenario
     * \param pass2SizeInfo Output size info
     * \return true if success
     */
    bool getPass2SizeInfo(ENUM_PASS2_ROUND round, ENUM_STEREO_SCENARIO eScenario, Pass2SizeInfo &pass2SizeInfo);

    //For rests, will rotate according to module orientation and ratio inside
    /**
     * \brief Get buffer size besides Pass2-A,
     * \details The size will be affected by module orientation and image ratio
     *
     * \param eName Buffer name unum
     * \param eScenario Stereo scenario
     *
     * \return The area of each buffer
     */
    StereoArea getBufferSize(ENUM_BUFFER_NAME eName, ENUM_STEREO_SCENARIO eScenario = eSTEREO_SCENARIO_UNKNOWN) const;

    /**
     * \brief Get side-by-side(SBS) image(a.k.a. JPS) size
     * \details JPS size = ( single image size + padding ) * 2
     * \return SBS image size
     */
    MSize getSBSImageSize();    //For capture only

    /**
     * \brief Get capture image size
     * \details Capture size may change in runtime, this API can set/get the capture size set in AP
     * \return Size of captured image
     */
    MSize captureImageSize() { return m_captureSize; }



public:
    //For updating sizes when user called StereoSettingProvider::setStereoPrifile()
    friend class StereoSettingProvider;

protected:
    StereoSizeProvider();
    virtual ~StereoSizeProvider() {};

private:
    bool __updateBMDeNoiseSizes();
    bool __getCenterCrop(MSize &srcSize, MRect &rCrop);
    void __updateCaptureImageSize();

private:
    static Mutex            mLock;
    MSize                   m_captureSize;


    MSize                   m_BMDeNoiseAlgoSize_main1;
    MSize                   m_BMDeNoiseAlgoSize_main2;
    MSize                   m_BMDeNoiseFullRawSize_main1;
    MSize                   m_BMDeNoiseFullRawSize_main2;
    MRect                   m_BMDeNoiseFullRawCropSize_main1;
    MRect                   m_BMDeNoiseFullRawCropSize_main2;
};

#endif
