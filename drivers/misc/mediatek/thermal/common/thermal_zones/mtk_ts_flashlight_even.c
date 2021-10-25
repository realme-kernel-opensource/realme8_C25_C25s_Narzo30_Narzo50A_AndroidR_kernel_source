// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
 /**************************************************************************
 * Copyright (c) 2020-2030 OPPO Mobile Comm Corp.,Ltd. All Rights Reserved.
 * VENDOR_EDIT
 * File                   : mtk_ts_flashlight.cpp
 * Description     : file to set flashlight ntc as a mtk thermal zone
 * -------------Rivision History-----------------------------------------
 * <version>      <date>            <author>                                      <Modify.desc>
 * 1.0                  2020-6-5      lifeiping@Camera.Drvier              add new file
 * OPPO Mobile Comm Proprietary and Confidential.
 *************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include "tzbatt_initcfg.h"
#include <linux/power_supply.h>
#include <linux/delay.h>

/* ************************************ */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static struct thermal_zone_device *thz_dev;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip = 0;
static char g_bind0[20] = { 0 };
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

#define mtk_flashlight_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define TEMPERATURE_TBL_SIZE 166

extern int even_temperature_enable(bool On);
int get_flashlight_temperature(void)
{
    int temp = 0;

    even_temperature_enable(true);
	msleep(30);

    temp = get_FlashLightNTC_temp();
    printk("[%s] T_flashlight temp = %d\n", __func__, temp);

    even_temperature_enable(false);

    return temp;
}

//static int pre_temp_idx = 70;  //start from 30 'c

static int mtk_flashlight_get_temp(struct thermal_zone_device *thermal, int *t)
{

	*t = get_flashlight_temperature();

	printk("[%s] T_flashlight real_temp, %d\n", __func__, *t);

	thermal->polling_delay = 0;  //no-auto polling

	return 0;
}

static int mtk_flashlight_bind(struct thermal_zone_device *thermal,
			     struct thermal_cooling_device *cdev)
{
	return 0;
}

static int mtk_flashlight_unbind(struct thermal_zone_device *thermal,
			       struct thermal_cooling_device *cdev)
{
	return 0;
}

static int mtk_flashlight_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtk_flashlight_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtk_flashlight_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtk_flashlight_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtk_flashlight_get_crit_temp(struct thermal_zone_device *thermal,
				      int *temperature)
{
	*temperature = mtk_flashlight_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtk_flashlight_dev_ops = {
	.bind = mtk_flashlight_bind,
	.unbind = mtk_flashlight_unbind,
	.get_temp = mtk_flashlight_get_temp,
	.get_mode = mtk_flashlight_get_mode,
	.set_mode = mtk_flashlight_set_mode,
	.get_trip_type = mtk_flashlight_get_trip_type,
	.get_trip_temp = mtk_flashlight_get_trip_temp,
	.get_crit_temp = mtk_flashlight_get_crit_temp,
};

static int mtk_flashlight_read(struct seq_file *m, void *v)
{

	seq_printf(m,
		"[mtk_flashlight_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

	seq_printf(m,
		"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[4], trip_temp[5], trip_temp[6],
		trip_temp[7], trip_temp[8], trip_temp[9]);

	seq_printf(m,
		"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

	seq_printf(m,
		"g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
		g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

	seq_printf(m,
		"g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
		g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

	seq_printf(m,
		"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

	seq_printf(m,
		"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtk_flashlight_register_thermal(void);
static void mtk_flashlight_unregister_thermal(void);

static ssize_t mtk_flashlight_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	return -EINVAL;
}

static int mtk_flashlight_register_thermal(void)
{
	printk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtkflashlight", num_trip,
						NULL, &mtk_flashlight_dev_ops,
						0, 0, 0, interval * 1000);

	return 0;
}

static void mtk_flashlight_unregister_thermal(void)
{
	printk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtk_flashlight_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_flashlight_read, NULL);
}

static const struct file_operations mtkts_flashlight_fops = {
	.owner = THIS_MODULE,
	.open = mtk_flashlight_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_flashlight_write,
	.release = single_release,
};

static int __init mtk_flashlight_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtk_flashlight_dir = NULL;

	printk("[%s]\n", __func__);

	mtk_flashlight_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtk_flashlight_dir) {
		printk("%s mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzflashlight", 0664, mtk_flashlight_dir,
							&mtkts_flashlight_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
	mtk_flashlight_register_thermal();

	return 0;
}

static void __exit mtk_flashlight_exit(void)
{
        printk("[%s]\n", __func__);

        mtk_flashlight_unregister_thermal();
}
module_init(mtk_flashlight_init);
module_exit(mtk_flashlight_exit);
