#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ti_extamp"

#include <fcntl.h>
#include <cutils/log.h>

#include "mtk_tas2555_interface.h"

static int fd = -1;

static int chk_tas2555()
{
    if (fd < 0)
        return MTK_TAS2555_Init();
    else
        return fd;
}

int MTK_TAS2555_Init(void)
{
    fd = open(TI_DRV2555_I2CDEVICE, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        ALOGW("Can't open i2c bus:%s\n", TI_DRV2555_I2CDEVICE);

    return fd;
}

void MTK_TAS2555_SpeakerOn(void)
{
    int ret = 0;
    unsigned char buf[2] = {TIAUDIO_CMD_SPEAKER, 1};

    if (chk_tas2555() < 0)
        return;
    ret = write(fd, buf, 2);
}

void MTK_TAS2555_SpeakerOff(void)
{
    int ret = 0;
    unsigned char buf[2] = {TIAUDIO_CMD_SPEAKER, 0};

    if (chk_tas2555() < 0)
        return;
    ret = write(fd, buf, 2);
}

void MTK_TAS2555_SetSampleRate(int samplerate)
{
    int ret = 0;
    unsigned char buf[5] = {TIAUDIO_CMD_SAMPLERATE, 0};

    if (chk_tas2555() < 0)
        return;
    buf[1] = (unsigned char)((samplerate & 0xff000000)>>24);
    buf[2] = (unsigned char)((samplerate & 0x00ff0000)>>16);
    buf[3] = (unsigned char)((samplerate & 0x0000ff00)>>8);
    buf[4] = (unsigned char)((samplerate & 0x000000ff));
    ret = write(fd, buf, 5);
}

EXPORT_SYMBOL(MTK_TAS2555_Init);
EXPORT_SYMBOL(MTK_TAS2555_SpeakerOn);
EXPORT_SYMBOL(MTK_TAS2555_SpeakerOff);
EXPORT_SYMBOL(MTK_TAS2555_SetSampleRate);
