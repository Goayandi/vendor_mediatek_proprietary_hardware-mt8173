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
#ifndef _N3D_HAL_H_
#define _N3D_HAL_H_

#include <vector>
#include <mtkcam/feature/stereo/hal/stereo_common.h>

using namespace StereoHAL;

const MUINT32 MAX_GEO_LEVEL  = 3;

struct HWFEFM_DATA
{
    MUINT16 *geoDataMain1[MAX_GEO_LEVEL];       //FE
    MUINT16 *geoDataMain2[MAX_GEO_LEVEL];       //FE
    MUINT16 *geoDataLeftToRight[MAX_GEO_LEVEL]; //FM
    MUINT16 *geoDataRightToLeft[MAX_GEO_LEVEL]; //FM

    HWFEFM_DATA()
        : geoDataMain1{NULL}
        , geoDataMain2{NULL}
        , geoDataLeftToRight{NULL}
        , geoDataRightToLeft{NULL}
    {
    }
};

struct SWFEFM_DATA
{
    MUINT8* geoSrcImgMain1[MAX_GEO_LEVEL];
    MUINT8* geoSrcImgMain2[MAX_GEO_LEVEL];

    SWFEFM_DATA()
        : geoSrcImgMain1{NULL}
        , geoSrcImgMain2{NULL}
    {
    }
};

struct EIS_DATA
{
    bool isON;
    MPoint eisOffset;
    MSize eisImgSize;

    EIS_DATA()
        : isON(false)
        , eisOffset()
        , eisImgSize()
    {
    }
};

struct N3D_HAL_INIT_PARAM
{
    ENUM_STEREO_SCENARIO    eScenario;
    MUINT8                  fefmRound;

    N3D_HAL_INIT_PARAM()
        : eScenario(eSTEREO_SCENARIO_UNKNOWN)
        , fefmRound(2)
    {
    }
};

struct N3D_HAL_PARAM_COMMON
{
    HWFEFM_DATA hwfefmData;
    IImageBuffer *rectifyImgMain1;    //warp_src_addr_main

    IImageBuffer *ccImage[2];   //0: main1, 1: main2

    MUINT32 magicNumber[2];     //0: main1, 1:main2
    MUINT32 requestNumber;

    N3D_HAL_PARAM_COMMON()
        : rectifyImgMain1(NULL)
        , ccImage{NULL}
        , magicNumber{0}
        , requestNumber(0)
    {
    }
};

struct N3D_HAL_PARAM : public N3D_HAL_PARAM_COMMON  //For Preview/VR
{
    EIS_DATA eisData;
    bool isAFTrigger;
    bool isDepthAFON;
    bool isDistanceMeasurementON;

    IImageBuffer *rectifyImgMain2;    //warp_src_addr_auxi

    N3D_HAL_PARAM()
        : N3D_HAL_PARAM_COMMON()
        , eisData()
        , isAFTrigger(false)
        , isDepthAFON(false)
        , isDistanceMeasurementON(false)
    {
    }
};

struct N3D_HAL_OUTPUT
{
    IImageBuffer *rectifyImgMain1;    //warp_dst_addr_main
    MUINT8 *maskMain1;          //warp_msk_addr_main    //point to m_main1Mask

    IImageBuffer *rectifyImgMain2;    //warp_dst_addr_auxi
    MUINT8 *maskMain2;          //warp_msk_addr_auxi

    MUINT8 *ldcMain1;           //warp_ldc_addr_main

    MFLOAT distance;            //MTK_STEREO_FEATURE_RESULT_DISTANCE, unit: meter

    MFLOAT convOffset;          //Convergence depth offset, output for GF HAL

    N3D_HAL_OUTPUT()
        : rectifyImgMain1(NULL)
        , maskMain1(NULL)
        , rectifyImgMain2(NULL)
        , maskMain2(NULL)
        , ldcMain1(NULL)
        , distance(0)
        , convOffset(0)
    {
    }
};

struct N3D_HAL_PARAM_CAPTURE : public N3D_HAL_PARAM_COMMON
{
    IImageBuffer *rectifyGBMain2;     //YV12, 2176x1152
    MUINT32 captureOrientation;

    N3D_HAL_PARAM_CAPTURE()
        : N3D_HAL_PARAM_COMMON()
        , rectifyGBMain2(NULL)
        , captureOrientation(0)
    {
    }
};

struct N3D_HAL_PARAM_CAPTURE_SWFE : public N3D_HAL_PARAM_COMMON
{
    SWFEFM_DATA *swfeData;

    N3D_HAL_PARAM_CAPTURE_SWFE()
        : N3D_HAL_PARAM_COMMON()
        , swfeData(NULL)
    {
    }
};

struct N3D_HAL_OUTPUT_CAPTURE : public N3D_HAL_OUTPUT
{
    //Main1 is output by DepthMap_Node
    IImageBuffer *jpsImgMain2;  //1080 YV12 main2 image with warpping

    //For de-noise
    MFLOAT  *warpingMatrix;     //User need to alloc this array, size from StereoSettingProvider::getMaxWarpingMatrixBufferSizeInBytes()
    MUINT32 warpingMatrixSize; //Count of element in warpingMatrix

    N3D_HAL_OUTPUT_CAPTURE()
        : N3D_HAL_OUTPUT()
        , jpsImgMain2(NULL)
        , warpingMatrix(NULL)
        , warpingMatrixSize(0)
    {
    }
};

struct RUN_LENGTH_DATA
{
    //Data is always 0/255
    MUINT32 offset;
    MUINT32 len;

    RUN_LENGTH_DATA(MUINT32 o, MUINT32 l) {
        offset = o;
        len = l;
    }
};

class N3D_HAL
{
public:
    /**
     * \brief Create a new instance
     * \details Create instance, callers should delete them after used
     *
     * \param n3dInitParam Init parameter
     * \return A new instance
     */
    static N3D_HAL *createInstance(N3D_HAL_INIT_PARAM &n3dInitParam);

    /**
     * \brief Default destructor
     * \details Default destructor, callers should delete them after used
     */
    virtual ~N3D_HAL() {}

    /**
     * \brief Run N3D HAL in preview/record scenario
     * \details Run N3D HAL in preview/record scenario
     *
     * \param n3dParams Input parameter
     * \param n3dOutput Output parameter
     *
     * \return True if success
     */
    virtual bool N3DHALRun(N3D_HAL_PARAM &n3dParams, N3D_HAL_OUTPUT &n3dOutput) = 0;

    /**
     * \brief Run N3D HAL in capture scenario
     * \details Run N3D HAL in capture scenario
     *
     * \param n3dParams Input parameter
     * \param n3dOutput Output parameter
     *
     * \return True if success
     */
    virtual bool N3DHALRun(N3D_HAL_PARAM_CAPTURE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput) = 0;

    /**
     * \brief Run N3D HAL with SWFE for all scenario. Not functional yet.
     * \details Run N3D HAL with SWFE for all scenario. Not functional yet.
     *
     * \param n3dParams Input parameter
     * \param n3dOutput Output parameter
     *
     * \return True if success
     */
    virtual bool N3DHALRun(N3D_HAL_PARAM_CAPTURE_SWFE &n3dParams, N3D_HAL_OUTPUT_CAPTURE &n3dOutput) = 0;

    /**
     * \brief Get stereo extra data in JSON format
     * \details Stereo extra data consists of parameters of stereo, such as JPS size, capture orientation, etc
     *          It also contains the mask for warpping. The mask is encoded with enhanced run-length-encoding(ERLE).
     *          The ERLE describes the valid area in the format [offset, length],
     *          e.g. [123, 1920] means the valid data starts from offset 123 with length 1920.
     *          User just needs to memset this area to 0xFF( ::memset(mask+123, 0xFF, 1920); ),
     *          then he can decode and get the warpping mask.
     * \return Stereo extra data in JSON format
     *
     * \see JSON_Util
     */
    virtual char *getStereoExtraData() = 0;

protected:
    N3D_HAL() {}
};

#endif  // _N3D_HAL_H_