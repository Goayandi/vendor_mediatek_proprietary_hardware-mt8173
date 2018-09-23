/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
** unit test for fpu_float fpu_double
**
** Author: <cy_huang@richtek.com>
**
** -------------------------------------------------------------------------*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "rt_extamp_intf.h"

int main(int argc, const char **argv)
{
	struct SmartPa SmartPa;

	printf("%s ++\n", __func__);
	memset(&SmartPa, 0, sizeof(struct SmartPa));
	mtk_smartpa_init(&SmartPa);
	if (SmartPa.ops.init(NULL) < 0)
		goto out_main;
	if (SmartPa.ops.speakerOn(NULL) < 0)
		goto out_main;
	if (SmartPa.ops.speakerOff() < 0)
		goto out_main;
	if (SmartPa.ops.deinit() < 0)
		goto out_main;
	printf("%s --\n", __func__);
	return 0;
out_main:
	return -EINVAL;
}
