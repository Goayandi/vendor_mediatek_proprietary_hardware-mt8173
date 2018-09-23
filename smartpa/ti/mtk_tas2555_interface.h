#ifndef _MTK_TAS2555_INTERFACE_H
#define _MTK_TAS2555_INTERFACE_H


#ifdef __cplusplus
extern "C" {
#endif

#define	TIAUDIO_CMD_REG_WITE            1
#define	TIAUDIO_CMD_REG_READ            2
#define	TIAUDIO_CMD_DEBUG_ON            3
#define	TIAUDIO_CMD_PROGRAM             4
#define	TIAUDIO_CMD_CONFIGURATION       5
#define	TIAUDIO_CMD_FW_TIMESTAMP        6
#define	TIAUDIO_CMD_CALIBRATION         7
#define	TIAUDIO_CMD_SAMPLERATE          8
#define	TIAUDIO_CMD_BITRATE             9
#define	TIAUDIO_CMD_DACVOLUME           10
#define	TIAUDIO_CMD_SPEAKER             11

#define	TAS2555_MAGIC_NUMBER    0x32353535	/* '2555' */

#define	SMARTPA_SPK_DAC_VOLUME              _IOWR(TAS2555_MAGIC_NUMBER, 1, unsigned long)
#define	SMARTPA_SPK_POWER_ON                _IOWR(TAS2555_MAGIC_NUMBER, 2, unsigned long)
#define	SMARTPA_SPK_POWER_OFF               _IOWR(TAS2555_MAGIC_NUMBER, 3, unsigned long)
#define	SMARTPA_SPK_SWITCH_PROGRAM          _IOWR(TAS2555_MAGIC_NUMBER, 4, unsigned long)
#define	SMARTPA_SPK_SWITCH_CONFIGURATION    _IOWR(TAS2555_MAGIC_NUMBER, 5, unsigned long)
#define	SMARTPA_SPK_SWITCH_CALIBRATION      _IOWR(TAS2555_MAGIC_NUMBER, 6, unsigned long)
#define	SMARTPA_SPK_SET_SAMPLERATE          _IOWR(TAS2555_MAGIC_NUMBER, 7, unsigned long)
#define	SMARTPA_SPK_SET_BITRATE             _IOWR(TAS2555_MAGIC_NUMBER, 8, unsigned long)

int MTK_TAS2555_Init(void);
void MTK_TAS2555_SpeakerOn(void);
void MTK_TAS2555_SpeakerOff(void);
void MTK_TAS2555_SetSampleRate(int samplerate);

#define TI_DRV2555_I2CDEVICE "/dev/tas2555s" //dev node provides the i2c read/write interface.

#ifdef __cplusplus
}
#endif

#endif //_MTK_TAS2555_INTERFACE_H

