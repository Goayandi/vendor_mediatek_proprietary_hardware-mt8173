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
#ifndef _TUNING_PROVIDER_H_
#define _TUNING_PROVIDER_H_

#include <mtkcam/drv/def/dpecommon.h>
#include <isp_reg.h>
#include <vector>

enum ENUM_CLEAR_REGION  //for GF
{
    E_CLEAR_REGION_SMALL  = 0,
    E_CLEAR_REGION_MEDIUM = 1,
    E_CLEAR_REGION_LARGE  = 2,
};

enum ENUM_BOKEH_STRENGTH
{
    E_BOKEH_STRENGTH_WEAK,
    E_BOKEH_STRENGTH_NORMAL,
    E_BOKEH_STRENGTH_STRONG
};

enum ENUM_FM_DIRECTION
{
    E_FM_R_TO_L,
    E_FM_L_TO_R
};

using namespace NSCam::NSIoPipe;

class StereoTuningProvider
{
public:
    /**
     * \brief Get tuning info of DPE
     * \details If user set debug.STEREO.tuning.dpe to 1,
     *          we will dump tuning parameters to /sdcard/tuning/dpe_params in JSON format.
     *          User can edit this file to change tuning parameter and take effect in runtime.
     *
     * \param tuningBuffer Tuning parameters will be saved in the buffer
     * \return true if success
     */
    static bool getDPETuningInfo(DVEConfig *tuningBuffer);

    /**
     * \brief Get tuning info of WMF
     * \details If user set debug.STEREO.tuning.wmf to 1,
     *          we will dump tuning parameters to /sdcard/tuning/wmf_params in JSON format.
     *          User can edit this file to change tuning parameter and take effect in runtime.
     *
     * \param size Output WMF filter size, default is 5x5
     * \param tbliBuffer Output of tuning buffer
     *
     * \return true if success
     */
    static bool getWMFTuningInfo(WMFEFILTERSIZE &size, void *tbliBuffer);

    /**
     * \brief Get GF clear region info
     * \details GF algo will use this table to calculate clear range
     *
     * \param table_size The count of element in the table
     * \param clearRangeTable Clear range table
     *
     * \return [description]
     */
    static bool getGFTuningInfo(std::vector<MINT32> &tuningTable);

    /**
     * \brief Get tuning info of Bokeh
     * \details If user set debug.STEREO.tuning.bokeh to 1,
     *          we will dump tuning parameters to /sdcard/tuning/bokeh_params in JSON format.
     *          User can edit this file to change tuning parameter and take effect in runtime.
     *
     * \param tuningBuffer Ouptut of tuning buffer, type: dip_x_reg_t*
     * \param eBokehStrength Strength of bokeh
     *
     * \return true if success
     */
    static bool getBokehTuningInfo(void *tuningBuffer, ENUM_BOKEH_STRENGTH eBokehStrength=E_BOKEH_STRENGTH_NORMAL);

    /**
     * \brief Get tuning info of FM
     *
     * \param direction FM direction, left to right or right to left.
     *                  Note that sensor relative position may affect the result
     * \param fmSizeInfo Output FM size data
     * \param fmThInfo Output FM tuning data
     * \return true if success
     */
    static bool getFMTuningInfo(ENUM_FM_DIRECTION direction, DIP_X_REG_FM_SIZE &fmSizeInfo, DIP_X_REG_FM_TH_CON0 &fmThInfo);

    /**
     * \brief Get tuning info of FE
     *
     * \param isp_reg Tuning info will be saved here
     * \param BLOCK_SIZE FE block size, we support block size of 8 and 16 now
     *
     * \return true if success
     * \see StereoSettingProvider::fefmBlockSize
     */
    static bool getFETuningInfo(dip_x_reg_t *isp_reg, const int BLOCK_SIZE);

protected:
    static void initDPETuningInfo(DVEConfig *tuningBuffer);
    static void initBokehTuningInfo(dip_x_reg_t *tuning, ENUM_BOKEH_STRENGTH eBokehStrength);
};

#endif