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
#ifndef _PROFILE_UTIL_H_
#define _PROFILE_UTIL_H_

#include <mtkcam/feature/stereo/hal/stereo_setting_provider.h>
#include <string.h>
#include <mtkcam/utils/std/Trace.h>
#include <sstream>  //For ostringstream
#include <string>

//To enable/disable met log, modify vsdof_common.mk: HAL_MET_PROFILE := true
#ifdef MET_USER_EVENT_SUPPORT
#include <met_tag.h>
#endif

/**
 * \brief Profile utility to analyze performance by log
 * \details Usage:
 *          1. Create a ProfileUtil instance with TAG and MESSAGE
 *          2. Call beginProfile() to start profiling
 *          3. Call endProfile() to end profiling and print log.
 *             The log will be in format: [Benchmark]<TAG>::<MESSAGE>: ooo.xxx sec
 *          4. Enable/Disable log: setprop debug.STEREO.log.Profile [1|0], default is 0
 *
 *          Sample code:
 *          ProfileUtil profile(LOG_TAG, "xxx");
 *          profile.beginProfile();
 *          profile.endProfile();
 */
class ProfileUtil
{
public:
    /**
     * \brief Constructor, this won't begin profile until beginProfile()
     * \details Derived classes should implement the same interface
     *
     * \param TAG The tag in prefix, usally use LOG_TAG
     * \param MESSAGE The message in prefix
     */
    ProfileUtil(const char *TAG, const char *MESSAGE)
        : PROFILE_ENABLED(StereoSettingProvider::isProfileLogEnabled())
#ifdef MET_USER_EVENT_SUPPORT
        , mMetTag(NULL)
#endif
    {
        mTag = std::string(TAG);
        setMessage(MESSAGE);

#ifdef MET_USER_EVENT_SUPPORT
        if(PROFILE_ENABLED) {
            std::string metTag = _getPrefix();
            mMetTag = new char[metTag.length()+1];
            ::memcpy(mMetTag, metTag.c_str(), metTag.length());
            mMetTag[metTag.length()] = 0;
            met_tag_init();
        }
#endif
    }

    /**
     * \brief Destructor
     */
    virtual ~ProfileUtil()
    {
#ifdef MET_USER_EVENT_SUPPORT
        if(PROFILE_ENABLED) {
            met_tag_uninit();
        }

        if(mMetTag) {
            delete [] mMetTag;
        }
#endif
    }

    /**
     * \brief Begin profile, this includes log and ftrace
     */
    void beginProfile()
    {
        if(!PROFILE_ENABLED) {
            return;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_start);
        CAM_TRACE_BEGIN( _getPrefix().c_str() );
#ifdef MET_USER_EVENT_SUPPORT
        met_tag_start( 0, mMetTag );
#endif
    }

    /**
     * \brief End profile and print log
     * \details If the property debug.STEREO.log.Profile is set to 1,
     *          we will print the duration
     */
    void endProfile()
    {
        if(!PROFILE_ENABLED) {
            return;
        }

#ifdef MET_USER_EVENT_SUPPORT
        met_tag_end( 0, mMetTag );
#endif
        CAM_TRACE_END();
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        t_result = timeDiff(t_start, t_end);
        ALOGD("[Benchmark]%s: %lu.%.9lu sec", _getPrefix().c_str(), t_result.tv_sec, t_result.tv_nsec);
    }

    /**
     * \brief Set message
     * \details If called after beginProfile(), this won't change ftrace title
     *          Howerver the log will be affected, e.g. you can change message accroding to the result
     *
     * \param MESSAGE The message of the profile
     */
    void setMessage(const char *MESSAGE)
    {
        mMessage = std::string(MESSAGE);
    }

    /**
     * \brief Append message text
     * \details If called after beginProfile(), this won't change ftrace title.
     *          Howerver the log will be affected, e.g. you can append output of a function to the log
     *
     * \param MESSAGE The message to append
     */
    void appendMessage(const char *MESSAGE)
    {
        if(NULL != MESSAGE &&
           strlen(MESSAGE) > 0)
        {
            mMessage += std::string(MESSAGE);
        }
    }

protected:
    std::string _getPrefix() {
        std::string prefix;
        if(mTag.length() > 0) {
            if( mMessage.length() > 0 ) {
                std::ostringstream stringStream;
                stringStream << mTag << "::" << mMessage;
                prefix = stringStream.str();
            } else {
                return mTag;
            }
        } else if( mMessage.length() > 0 ) {
            return mMessage;
        }

        return prefix;
    }

private:
    const bool  PROFILE_ENABLED;
    std::string  mTag;
    std::string  mMessage;
#ifdef MET_USER_EVENT_SUPPORT
    char    *mMetTag;
#endif

    struct timespec t_start, t_end, t_result;
};

/**
 * \brief Automatically profile performance with variable life cycle
 * \details Usage & sample code:
 *          AutoProfileUtil profile(LOG_TAG, "xxxx");
 *          or
 *          AutoProfileUtil profile(LOG_TAG, __func__);
 *
 *          The log will be in format: [Benchmark]<TAG>::<MESSAGE>: ooo.xxx sec
 *          To enable/disable log: setprop debug.STEREO.log.Profile [1|0], default is 0
 * \see ProfileUtil
 */
class AutoProfileUtil : public ProfileUtil
{
public:
    /**
     * \brief Constructor, begin profile immediately
     *
     * \param TAG The tag in prefix, usally use LOG_TAG
     * \param MESSAGE The message in prefix
     */
    AutoProfileUtil(const char *TAG, const char *MESSAGE) : ProfileUtil(TAG, MESSAGE)
    {
        beginProfile();
    }

    /**
     * \brief Destructor, end profile
     */
    ~AutoProfileUtil()
    {
        endProfile();
    }
};

#endif