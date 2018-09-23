#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <tinyalsa/asoundlib.h>
#include "rt_extamp_intf.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rt_extamp"

#define param_path "/system/etc/rt5509_param"
#define dev_path "/sys/bus/platform/devices/rt5509_param.0/prop_param"

static int rt_extamp_init(struct SmartPa *smart_pa)
{
	char data[4096] = {0};
	int size;
	int i_fd, o_fd;

	printf("%s() begin\n", __func__);
	ALOGD("%s() begin", __func__);
	i_fd = open(param_path, O_RDONLY);
	if (i_fd < 0) {
		printf("%s() cannot open param file\n", __func__);
		ALOGD("%s() cannot open param file\n", __func__);
		goto out_init;
	}
	size = read(i_fd, data, 4096);
	if (size < 0) {
		printf("%s() cannot read param file\n", __func__);
		ALOGD("%s() cannot read param file\n", __func__);
	}
	close(i_fd);
	if (size < 0)
		goto out_init;
	o_fd = open(dev_path, O_WRONLY);
	if (o_fd < 0) {
		printf("%s() cannot open device path\n", __func__);
		ALOGD("%s() cannot open device path\n", __func__);
		goto out_init;
	}
	if (write(o_fd, data, size) < 0) {
		printf("%s() cannot write device path\n", __func__);
		ALOGD("%s() cannot write device path\n", __func__);
	}
	close(o_fd);
	printf("%s() end\n", __func__);
	ALOGD("%s() end\n", __func__);
	return 0;
out_init:
	ALOGD("%s() cannot open file\n", __func__);
	return -EINVAL;
}

static int rt_extamp_deinit()
{
	printf("%s\n", __func__);
	return 0;
}

static int rt_extamp_speakerOn(struct SmartPaRuntime *runtime)
{
	struct mixer *mixer;
	struct mixer_ctl *mixer_ctl;
	int ret = 0;

	printf("%s() begin\n", __func__);
	ALOGD("%s() begin", __func__);
	mixer = mixer_open(0);
	if (!mixer) {
		printf("mixer_open fail\n");
		ALOGE("Error: mixer_oepn fail");
		return -EINVAL;
	}
	mixer_ctl = mixer_get_ctl_by_name(mixer, "I2SDO Mux");
	if (!mixer_ctl) {
		printf("get ctl I2SDO Mux fail\n");
		ALOGE("Error: cannot get I2SDO Mux mixer_ctl");
		goto out_spk_on;
	}
	ret = mixer_ctl_set_enum_by_string(mixer_ctl, "I2SDOR/L");
	if (ret < 0) {
		printf("set ctl I2SDO Mux fail\n");
		ALOGE("Error: cannot set I2SDO Mux mixer_ctl");
		goto out_spk_on;
	}
	mixer_ctl = mixer_get_ctl_by_name(mixer, "Ext_DO_Enable");
	if (!mixer_ctl) {
		printf("get ctl Ext_DO_Enable fail\n");
		ALOGE("Error: cannot get Ext_DO_Enable mixer_ctl");
		goto out_spk_on;
	}
	ret = mixer_ctl_set_value(mixer_ctl, 0, 1);
	if (ret < 0) {
		printf("set ctl Ext_DO_Enable fail\n");
		ALOGE("Error: cannot set Ext_DO_Enable mixer_ctl");
		goto out_spk_on;
	}
out_spk_on:
	mixer_close(mixer);
	printf("%s() end\n", __func__);
	ALOGD("%s() end", __func__);
	return ret;
}

static int rt_extamp_speakerOff()
{
	struct mixer *mixer;
	struct mixer_ctl *mixer_ctl;
	int ret = 0;

	printf("%s() begin\n", __func__);
	ALOGD("%s() begin", __func__);
	mixer = mixer_open(0);
	if (!mixer) {
		printf("mixer_open fail\n");
		ALOGE("Error: mixer_oepn fail");
		return -EINVAL;
	}
	mixer_ctl = mixer_get_ctl_by_name(mixer, "Ext_DO_Enable");
	if (!mixer_ctl) {
		printf("get ctl Ext_DO_Enable fail\n");
		ALOGE("Error: cannot get Ext_DO_Enable mixer_ctl");
		goto out_spk_on;
	}
	ret = mixer_ctl_set_value(mixer_ctl, 0, 0);
	if (ret < 0) {
		printf("set ctl Ext_DO_Enable fail\n");
		ALOGE("Error: cannot set Ext_DO_Enable mixer_ctl");
		goto out_spk_on;
	}
out_spk_on:
	mixer_close(mixer);
	printf("%s() end\n", __func__);
	ALOGD("%s() end", __func__);
	return ret;
}

int mtk_smartpa_init(struct SmartPa *smart_pa)
{
	printf("%s\n", __func__);
	smart_pa->ops.init = rt_extamp_init;
	smart_pa->ops.deinit = rt_extamp_deinit;
	smart_pa->ops.speakerOn = rt_extamp_speakerOn;
	smart_pa->ops.speakerOff = rt_extamp_speakerOff;
	return 0;
}
