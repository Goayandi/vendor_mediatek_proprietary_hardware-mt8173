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
#ifndef _STEREO_AREA_H_
#define _STEREO_AREA_H_

#include <stdint.h>
#include <math.h>
#include <mtkcam/def/common.h>
//#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Format.h>
#include "stereo_setting_provider.h"

using namespace NSCam;

const MSize  MSIZE_ZERO(0, 0);
const MPoint MPOINT_ZERO(0, 0);

namespace StereoHAL {

enum ENUM_IMAGE_RATIO_ALIGNMENT
{
    E_KEEP_AREA_SIZE,   //(old width * old height) ~= (new width * new height)
    E_KEEP_WIDTH,       //Keep width, change height
    E_KEEP_HEIGHT,      //Keep height, change width
};

/**
 * \brief Describes the stereo area
 * \details Stereo area consists of three parts: size, padding and start point.
 *          The area can have two rectagles, just like:
 *          ┌───────────────┐
 *          │ ┌───────────┐ │
 *          │ │           │ │
 *          │ │           │ │
 *          │ └───────────┘ │
 *          └───────────────┘
 *          Size: The outter size of the area
 *          Start point: The top-left position of the inner rect related to the outter rect
 *          Padding: The size of the outter rect - the size of the inner rect,
 *                   which meas it's the sum of the spaces.
 */
struct StereoArea {
    MSize size;
    MSize padding;
    MPoint startPt;

    /**
     * \brief Default constructor
     */
    inline StereoArea()
            : size()
            , padding()
            , startPt()
            {
            }

    /**
     * \brief Construct StereoArea with flatten parameteres
     * \details Construct StereoArea with flatten parameteres
     *
     * \param w Width of size
     * \param h Height of size
     * \param paddingX Horizontal padding of the area, default is 0
     * \param paddingY Vertical padding of the area, default is 0
     * \param startX X position of the start point of content, default is 0
     * \param startY Y position of the start point of content, default is 0
     */
    inline StereoArea(MUINT32 w, MUINT32 h, MUINT32 paddingX=0, MUINT32 paddingY=0, MUINT32 startX = 0, MUINT32 startY = 0)
            : size(w, h)
            , padding(paddingX, paddingY)
            , startPt(startX, startY)
            {
            }

    /**
     * \brief Construct StereoArea with structured parameteres
     * \details Construct StereoArea with structured parameteres
     *
     * \param sz Size of the area
     * \param p Padding of the area
     * \param pt Start point of content
     */
    inline StereoArea(MSize sz, MSize pad=MSIZE_ZERO, MPoint pt = MPOINT_ZERO)
            : size(sz)
            , padding(pad)
            , startPt(pt)
            {
            }

    /**
     * \brief Assign operator, it does deep copy
     * \details Assign operator, it does deep copy
     *
     * \param rhs Source area
     */
    inline StereoArea &operator=(const StereoArea &rhs)
            {
                size = rhs.size;
                padding = rhs.padding;
                startPt = rhs.startPt;

                return *this;
            }

    /**
     * \brief Compare operator
     * \details Compares two areas
     *
     * \param rhs Compared area
     * \return True if all data is the same
     */
    inline bool operator==(const StereoArea &rhs) const
            {
                if(size != rhs.size) return false;
                if(padding != rhs.padding) return false;
                if(startPt != rhs.startPt) return false;

                return true;
            }

    /**
     * \brief Compare operator
     * \details Compares two areas
     *
     * \param rhs Compared area
     * \return True if all data is different
     */
    inline bool operator!=(const StereoArea &rhs) const
            {
                if(size != rhs.size) return true;
                if(padding != rhs.padding) return true;
                if(startPt != rhs.startPt) return true;

                return false;
            }

    /**
     * \brief Construct with another area
     * \details Construct with another area, deep copies the data from source
     *
     * \param rhs Construct source
     */
    inline StereoArea(const StereoArea &rhs)
            : size(rhs.size)
            , padding(rhs.padding)
            , startPt(rhs.startPt)
            {
            }

    /**
     * \brief Default type convertor to MSize
     * \return size of the area
     */
    inline operator MSize() const {
        return size;
    }

    /**
     * \brief Get content size of the area
     * \details Get content size of the area
     * \return The size of content
     */
    inline MSize contentSize() const {
        return (size - padding);
    }

    /**
     * \brief Get product area
     *
     * \param RATIO Multiplier
     */
    inline StereoArea &operator*=(const MFLOAT RATIO) {
        size.w *= RATIO;
        size.h *= RATIO;
        padding.w *= RATIO;
        padding.h *= RATIO;
        startPt.x *= RATIO;
        startPt.y *= RATIO;

        return *this;
    }

    /**
     * \brief Get new product area
     *
     * \param RATIO Multiplier
     * \return New product area
     */
    inline StereoArea operator *(const MFLOAT RATIO) const {
        StereoArea newArea( *this );
        newArea *= RATIO;

        return newArea;
    }

    /**
     * \brief Apply rotation with module orientation
     * \details As default we will centralize the content
     *
     * \param CENTRALIZE_CONTENT (default)true if you want to centralize content, padding will not change
     *                           false if you really want to rotate size, padding and start point
     * \return Rotated area
     * \see StereoSettingProvider::getModuleRotation()
     */
    inline StereoArea &rotatedByModule(const bool CENTRALIZE_CONTENT=true) {
        if(CENTRALIZE_CONTENT) {
            switch(StereoSettingProvider::getModuleRotation())
            {
            case eRotate_0:
            case eRotate_180:
            default:
                break;
            case eRotate_90:
            case eRotate_270:
                {
                    //Only content rotates, padding does not change
                    MSize szContent = contentSize();
                    StereoArea rotatedArea(szContent.h+padding.w, szContent.w+padding.h, padding.w, padding.h);
                    rotatedArea.startPt.x = padding.w/2;
                    rotatedArea.startPt.y = padding.h/2;
                    *this = rotatedArea;
                }
                break;
            }
        } else {
            switch(StereoSettingProvider::getModuleRotation())
            {
            case eRotate_0:
            default:
                break;
            case eRotate_180:
                {
                    StereoArea rotatedArea(*this);
                    rotatedArea.startPt.x = size.w - contentSize().w - startPt.x;
                    rotatedArea.startPt.y = size.h - contentSize().h - startPt.y;
                    *this = rotatedArea;
                }
                break;
            case eRotate_90:
                {
                    StereoArea rotatedArea(size.h, size.w, padding.h, padding.w);
                    rotatedArea.startPt.x = size.h - contentSize().h - startPt.y;
                    rotatedArea.startPt.y = startPt.x;
                    *this = rotatedArea;
                }
                break;
            case eRotate_270:
                {
                    StereoArea rotatedArea(size.h, size.w, padding.h, padding.w);
                    rotatedArea.startPt.x = startPt.y;
                    rotatedArea.startPt.y = size.w - contentSize().w - startPt.x;
                    *this = rotatedArea;
                }
                break;
            }
        }

        return *this;
    }

    /**
     * \brief add extra padding in height
     * \details add extra padding to the height of the area
     *
     * \param extendRatio
     * \return Padded area
     * \see StereoSettingProvider::addPaddingToHeight()
     */
    inline StereoArea &addPaddingToHeight(float extendRatio) {

        int paddingHeight = (*this).size.h*extendRatio;

        (*this).startPt.x = 0;
        (*this).startPt.y = 0;

        (*this).padding.w = 0;
        (*this).padding.h = paddingHeight;

        (*this).size.h = (*this).size.h + paddingHeight;

        return *this;
    }

    /**
     * \brief Apply 16-align to content size. This will change overall size if needed.
     *        16-align meas its size is multiple of 16, e.g. 1920x1088
     * \details Sometimes user needs 16-aligned size for further process
     * \return 16-aligned area
     */
    inline StereoArea &apply16AlignToContent() {
        MSize szContent = contentSize();
        szContent.w = ((szContent.w+15)>>4)<<4;
        szContent.h = ((szContent.h+15)>>4)<<4;
        size = szContent + padding;
        startPt.x = padding.w>>1;
        startPt.y = padding.h>>1;

        return *this;
    }

    /**
     * \brief Apply 8-align to content size. This will change overall size if needed.
     *        8-align meas its size is multiple of 8, e.g. 1980x1088
     * \details Sometimes user needs 8-aligned size for further process
     * \return 8-aligned area
     */
    inline StereoArea &apply8AlignToContent() {
        MSize szContent = contentSize();
        szContent.w = (((szContent.w + 7)>>3)<<3);
        szContent.h = (((szContent.h + 7)>>3)<<3);
        size = szContent + padding;

        return *this;
    }

    /**
     * \brief Apply 2-align to content size. This will change overall size if needed.
     *        2-align meas its size is multiple of 2, e.g. 1920x1088
     * \details Sometimes user needs 2-aligned size for further process
     * \return 2-aligned area
     */
    inline StereoArea &apply2AlignToContent() {
        MSize szContent = contentSize();
        szContent.w += (szContent.w & 1);
        szContent.h += (szContent.h & 1);
        size = szContent + padding;
        startPt.x = padding.w>>1;
        startPt.y = padding.h>>1;

        return *this;
    }

    /**
     * \brief Apply 16-align to size
     *        16-align meas its size is multiple of 16, e.g. 1920x1088
     * \details Sometimes user needs 16-aligned size for further process
     * \return 16-aligned area
     */
    inline StereoArea &apply16Align() {
        if(padding == MSIZE_ZERO) {
            return apply16AlignToContent();
        }

        MSize szContent = contentSize();
        size.w = ((size.w+15)>>4)<<4;
        size.h = ((size.h+15)>>4)<<4;

        if(MSIZE_ZERO != padding) {
            padding = size - szContent;
            startPt.x = padding.w>>1;
            startPt.y = padding.h>>1;
        }

        return *this;
    }

    /**
     * \brief Apply 8-align to size
     *        8-align meas its size is multiple of 8, e.g. 480x272
     * \details Sometimes user needs 8-aligned size for further process
     * \return 8-aligned area
     */
    inline StereoArea &apply8Align() {
        MSize szContent = contentSize();
        size.w = ((size.w+7)>>3)<<3;
        size.h = ((size.h+7)>>3)<<3;

        if(MSIZE_ZERO != padding) {
            padding = size - szContent;
            startPt.x = padding.w>>1;
            startPt.y = padding.h>>1;
        }

        return *this;
    }

    /**
     * \brief Apply 8 align to outter width
     * \details Content size will not change, padding.w and startPt.x may change
     * \return Area with 8-aligned outter width
     */
    inline StereoArea &apply8AlignToWidth() {
        MSize szContent = contentSize();
        size.w = ((size.w+7)>>3)<<3;

        if(MSIZE_ZERO != padding) {
            padding = size - szContent;
            startPt.x = padding.w>>1;
        }

        return *this;
    }

    /**
     * \brief Apply 8 align to content width
     * \details Padding will not change, size.w and startPt.x may change
     * \return Area with 8-aligned content width
     */
    inline StereoArea &apply8AlignToContentWidth() {
        MSize szContent = contentSize();
        szContent.w = ((szContent.w+7)>>3)<<3;
        size.w = szContent.w + padding.w;
        startPt.x = padding.w>>1;

        return *this;
    }

    /**
     * \brief Apply 2-align to size, 2-align meas it's an even number
     * \details Picture size must be even
     * \return 2-aligned area
     */
    inline StereoArea &apply2Align() {
        this->apply2AlignToContent();
        MSize szContent = contentSize();
        if(MSIZE_ZERO == padding) {
            size = szContent;
        } else {
            size = szContent + padding;
            size.w += (size.w & 1);
            size.h += (size.h & 1);

            padding = size - szContent;
            startPt.x = padding.w>>1;
            startPt.y = padding.h>>1;
        }

        return *this;
    }

    /**
     * \brief Apply ratio to content size, padding will not change, and outter size will be content size + padding
     * \details No 16-align or 2-align guaranteed, if original size has the similar ratio, it does nothing.
     *
     * \param ratio Usally pass StereoSettingProvider::imageRatio()
     * \param alignment Alignment to keep when ratio changes
     * \return Area with ratio changed
     */
    inline StereoArea &applyRatio(ENUM_STEREO_RATIO ratio, ENUM_IMAGE_RATIO_ALIGNMENT alignment=E_KEEP_AREA_SIZE) {
        const float MAX_ERROR = 0.02f;
        MSize szContent = contentSize();
        bool isSizeChange = false;
        int n, m;

        //width ratio from 16:9 to 4:3, i.e. 0.866f
        int contentMagic1 = 866;
        int contentMagic2 = 1000;

        //16:9->4:3 padding should be 3/4 due to display size change(1920->1440)
        int paddingMagic1 = 3;
        int paddingMagic2 = 4;

        if(eRatio_16_9 == ratio) {
            if(fabs(1.0f - szContent.w * 9.0f / 16.0f / szContent.h) > MAX_ERROR) {
                //Originally is not 16:9, change height to be
                n = 9;
                m = 16;

                int tmp = contentMagic1;
                contentMagic1 = contentMagic2;
                contentMagic2 = tmp;

                tmp = paddingMagic1;
                paddingMagic1 = paddingMagic2;
                paddingMagic2 = tmp;

                isSizeChange = true;
            }
        } else {
            if(fabs(1.0f - szContent.w * 3.0f / 4.0f / szContent.h) > MAX_ERROR) {
                //Originally is not 4:3, change height to be
                n = 3;
                m = 4;
                isSizeChange = true;
            }
        }

        if(isSizeChange) {
            switch(alignment) {
                case E_KEEP_AREA_SIZE:
                default:
                    {
                        // float sizeBase = sqrt(szContent.w*szContent.h/(n*m));
                        // szContent.w = (int)(sizeBase * m);
                        // szContent.h = (int)(sizeBase * n);
                        szContent.w = szContent.w * contentMagic1 / contentMagic2;
                        szContent.h = szContent.w * n / m;

                        padding.w = padding.w * paddingMagic1 / paddingMagic2;
                        padding.h = padding.h * paddingMagic1 / paddingMagic2;

                        startPt.x = padding.w/2;
                        startPt.y = padding.h/2;
                    }
                    break;

                case E_KEEP_WIDTH:
                    szContent.h = (szContent.w * n / m);
                    break;

                case E_KEEP_HEIGHT:
                    {
                        szContent.w = szContent.h * m / n ;
                        padding.w = padding.w * paddingMagic1 / paddingMagic2;
                        padding.h = padding.h * paddingMagic1 / paddingMagic2;

                        startPt.x = padding.w/2;
                        startPt.y = padding.h/2;
                    }
                    break;
            }

            size = szContent + padding;
        }

        return *this;
    }

    /**
     * \brief Apply ratio change to size.w, padding.w and startPt.x
     * \details Height won't change
     *
     * \param ratio Ratio to change
     * \return Area with width changed
     */
    inline StereoArea &applyWidthRatio(const float ratio=1.0f)
    {
        if(1.0f == ratio ||
           ratio < 0.0f)
        {
            return *this;
        }

        if(0 == padding.w) {
            size.w = (int)(size.w*ratio) & ~1;
        } else {
            MSize szContent = contentSize();
            size.w = (int)(size.w*ratio) & ~1;

            szContent.w = (int)(szContent.w * ratio);
            padding.w = size.w - szContent.w;
            startPt.x = (int)(startPt.x * ratio);
        }

        return *this;
    }

    /**
     * \brief Double size.w, padding.w and startPt.x
     *
     * \return Area with doubled width
     */
    inline StereoArea &applyDoubleWidth()
    {
        return applyWidthRatio(2.0f);
    }

private:
    /**
     * \brief Print area, mostly for debugging
     */
    inline void print() {
#ifdef GTEST
        printf("[StereoArea]Size(%dx%d), Padding(%dx%d), StartPt(%d, %d)\n",
                size.w, size.h, padding.w, padding.h, startPt.x, startPt.y);
#else
        printf("[StereoArea]Size(%dx%d), Padding(%dx%d), StartPt(%d, %d)",
               size.w, size.h, padding.w, padding.h, startPt.x, startPt.y);
#endif
    }
};

const StereoArea STEREO_AREA_ZERO;

}
#endif