/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "../ssp.h"
#include <linux/wakelock.h>
#include <linux/pm_wakeup.h>

#define	VENDOR		"MAXIM"
#define	CHIP_ID		"MAX88920"

#define CANCELATION_FILE_PATH	"/efs/prox_cal"
#define LCD_LDI_FILE_PATH	"/sys/class/lcd/panel/window_type"

#define LINE_1		'4'
#define LINE_2		'2'

#define LDI_OTHERS	'0'
#define LDI_GRAY	'1'
#define LDI_WHITE	'2'

#if defined(CONFIG_MACH_JACTIVE_EUR)
#define DEFUALT_HIGH_THRESHOLD	45
#define DEFUALT_LOW_THRESHOLD	30
#define TBD_HIGH_THRESHOLD		45
#define TBD_LOW_THRESHOLD		30
#define WHITE_HIGH_THRESHOLD		45
#define WHITE_LOW_THRESHOLD		30
#else
#define DEFUALT_HIGH_THRESHOLD	60
#define DEFUALT_LOW_THRESHOLD	45
#define TBD_HIGH_THRESHOLD		60
#define TBD_LOW_THRESHOLD		45
#define WHITE_HIGH_THRESHOLD		60
#define WHITE_LOW_THRESHOLD		45
#endif
/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

/**
 * struct pmic8xxx_pwrkey - pmic8xxx pwrkey information
 * @key_press_irq: key press irq number
 * @pdata: platform data
 */
struct pmic8xxx_pwrkey {
	struct input_dev *pwr;
	int key_press_irq;
	u32	powerkey_state ;
	int key_release_irq;
	bool press;
	const struct pm8xxx_pwrkey_platform_data *pdata;
};

static bool call_in_progress = false;
static bool ischarging = false;
static int sensor_wake_options = 0;
static int sensor_wake_options_debug = 0;
static bool wakelock_held = false;
static struct wake_lock wakelock;
//struct wakeup_source *ws;
static DEFINE_MUTEX(prx_lock);
static struct pmic8xxx_pwrkey *sensorwake_pwrdev;
static struct delayed_work check_prox_val;
static struct delayed_work sensorwake_presspwr_work;
static bool trig_timer = false;
static unsigned char prox_val = 0;
static int prox_val_step_wave = 0;
static unsigned long  prox_val_step_wave_tout = 0;
static int prox_val_step_pocket = 0;
static unsigned long  prox_val_step_pocket_tout = 0;
static int prox_timer_length = 1000;
struct device *gdev;
static unsigned int wake_options_prox_max = 55;
#define PROX_BLOCKED		200
#define PROX_POCKET_LENGTH	10

static ssize_t prox_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t prox_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[PROXIMITY_RAW].prox[1],
		data->buf[PROXIMITY_RAW].prox[2],
		data->buf[PROXIMITY_RAW].prox[3]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[2] = { 1, 20};
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 2);
		data->bProximityRawEnabled = true;
	} else {
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 2);
		data->bProximityRawEnabled = false;
	}

	return size;
}

unsigned char get_proximity_rawdata(struct ssp_data *data)
{
	unsigned char uRowdata = 0;
	char chTempbuf[2] = { 1, 20};

	if (data->bProximityRawEnabled == false) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 2);
		msleep(200);
		uRowdata = data->buf[PROXIMITY_RAW].prox[0];
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 2);
	} else {
		uRowdata = data->buf[PROXIMITY_RAW].prox[0];
	}

	return uRowdata;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", get_proximity_rawdata(data));
}

static ssize_t proximity_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", get_proximity_rawdata(data));
}

static int get_proximity_threshold(struct ssp_data *data)
{
	if (data->uProxCanc <= (data->uProxLoThresh_default >> 1))
		return FAIL;

	data->uProxHiThresh = data->uProxHiThresh_default
		+ (data->uProxCanc - (data->uProxLoThresh_default >> 1));
	data->uProxLoThresh = data->uProxLoThresh_default
		+ (data->uProxCanc - (data->uProxLoThresh_default >> 1));

	return SUCCESS;
}

static void change_proximity_default_threshold(struct ssp_data *data)
{
	switch (data->chLcdLdi[1]) {
	case LDI_GRAY:
		data->uProxHiThresh_default = TBD_HIGH_THRESHOLD;
		data->uProxLoThresh_default = TBD_LOW_THRESHOLD;
		break;
	case LDI_WHITE:
		data->uProxHiThresh_default = WHITE_HIGH_THRESHOLD;
		data->uProxLoThresh_default = WHITE_LOW_THRESHOLD;
		break;
	case LDI_OTHERS:
		data->uProxHiThresh_default = DEFUALT_HIGH_THRESHOLD;
		data->uProxLoThresh_default = DEFUALT_LOW_THRESHOLD;
		break;
	default:
		data->uProxHiThresh_default = DEFUALT_HIGH_THRESHOLD;
		data->uProxLoThresh_default = DEFUALT_LOW_THRESHOLD;
		break;
	}
	data->uProxHiThresh = data->uProxHiThresh_default;
	data->uProxLoThresh = data->uProxLoThresh_default;
}

int proximity_open_lcd_ldi(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cancel_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(LCD_LDI_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cancel_filp)) {
		iRet = PTR_ERR(cancel_filp);
		if (iRet != -ENOENT)
			pr_err("[SSP]: %s - Can't open lcd ldi file\n",
				__func__);
		set_fs(old_fs);
		data->chLcdLdi[0] = 0;
		data->chLcdLdi[1] = 0;
		goto exit;
	}

	iRet = cancel_filp->f_op->read(cancel_filp,
		(u8 *)data->chLcdLdi, sizeof(u8) * 2, &cancel_filp->f_pos);
	if (iRet != (sizeof(u8) * 2)) {
		pr_err("[SSP]: %s - Can't read the lcd ldi data\n", __func__);
		iRet = -EIO;
	}

	ssp_dbg("[SSP]: %s - %c%c\n", __func__,
		data->chLcdLdi[0], data->chLcdLdi[1]);

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

exit:
	change_proximity_default_threshold(data);
	return iRet;
}

int proximity_open_calibration(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cancel_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cancel_filp)) {
		iRet = PTR_ERR(cancel_filp);
		if (iRet != -ENOENT)
			pr_err("[SSP]: %s - Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		goto exit;
	}

	iRet = cancel_filp->f_op->read(cancel_filp,
		(u8 *)&data->uProxCanc, sizeof(u8), &cancel_filp->f_pos);
	if (iRet != sizeof(u8)) {
		pr_err("[SSP]: %s - Can't read the cancel data\n", __func__);
		iRet = -EIO;
	}

	if (data->uProxCanc != 0) /*If there is an offset cal data. */
		get_proximity_threshold(data);

	pr_info("%s: proximity ps_canc = %d, ps_thresh hi - %d lo - %d\n",
		__func__, data->uProxCanc, data->uProxHiThresh,
		data->uProxLoThresh);

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

exit:
	set_proximity_threshold(data, data->uProxHiThresh, data->uProxLoThresh);

	return iRet;
}

static int proximity_store_cancelation(struct ssp_data *data, int iCalCMD)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cancel_filp = NULL;

	if (iCalCMD) {
		data->uProxCanc = get_proximity_rawdata(data);
		get_proximity_threshold(data);
	} else {
		data->uProxHiThresh = data->uProxHiThresh_default;
		data->uProxLoThresh = data->uProxLoThresh_default;
		data->uProxCanc = 0;
	}

	set_proximity_threshold(data, data->uProxHiThresh, data->uProxLoThresh);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(cancel_filp)) {
		pr_err("%s: Can't open cancelation file\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cancel_filp);
		return iRet;
	}

	iRet = cancel_filp->f_op->write(cancel_filp, (u8 *)&data->uProxCanc,
		sizeof(u8), &cancel_filp->f_pos);
	if (iRet != sizeof(u8)) {
		pr_err("%s: Can't write the cancel data to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

static ssize_t proximity_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	unsigned char uProxCanc = data->uProxCanc;

	if (uProxCanc > (data->uProxLoThresh_default >> 1))
		uProxCanc = uProxCanc - (data->uProxLoThresh_default >> 1);
	else
		uProxCanc = 0;

	ssp_dbg("[SSP]: uProxThresh : hi : %u lo : %u, uProxCanc = %u\n",
		data->uProxHiThresh, data->uProxLoThresh, uProxCanc);

	return sprintf(buf, "%u,%u,%u\n", uProxCanc, data->uProxHiThresh,
		data->uProxLoThresh);
}

static ssize_t proximity_cancel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iCalCMD = 0, iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* calibrate cancelation value */
		iCalCMD = 1;
	else if (sysfs_streq(buf, "0")) /* reset cancelation value */
		iCalCMD = 0;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	iRet = proximity_store_cancelation(data, iCalCMD);
	if (iRet < 0) {
		pr_err("[SSP]: - %s proximity_store_cancelation() failed\n",
			__func__);
		return iRet;
	}

	ssp_dbg("[SSP]: %s - %u\n", __func__, iCalCMD);
	return size;
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: uProxThresh = hi - %u, lo - %u\n",
		data->uProxHiThresh, data->uProxLoThresh);

	return sprintf(buf, "%u,%u\n", data->uProxHiThresh,
		data->uProxLoThresh);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u8 uNewThresh;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtou8(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrtoint failed.(%d)\n", __func__, iRet);
	else {
		data->uProxHiThresh = uNewThresh;
		set_proximity_threshold(data, data->uProxHiThresh,
			data->uProxLoThresh);
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u, lo - %u\n",
		__func__, data->uProxHiThresh, data->uProxLoThresh);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: uProxThresh = hi - %u, lo - %u\n",
		data->uProxHiThresh, data->uProxLoThresh);

	return sprintf(buf, "%u,%u\n", data->uProxHiThresh,
		data->uProxLoThresh);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u8 uNewThresh;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtou8(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrtoint failed.(%d)\n", __func__, iRet);
	else {
		data->uProxLoThresh = uNewThresh;
		set_proximity_threshold(data, data->uProxHiThresh,
			data->uProxLoThresh);
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u, lo - %u\n",
		__func__, data->uProxHiThresh, data->uProxLoThresh);

	return size;
}

static ssize_t sensor_wake_options_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = sprintf(buf, "%d\n", sensor_wake_options);
	return ret;
}

static ssize_t sensor_wake_options_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	unsigned int val = 0;
	gdev = dev;
	retval = sscanf(buf, "%d", &val);
	if (retval != 0 && val >= 0 && val <= 6)
		sensor_wake_options = val;
	return size;
}

static ssize_t sensor_wake_options_debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = sprintf(buf, "%d\n", sensor_wake_options_debug);
	return ret;
}

static ssize_t sensor_wake_options_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	unsigned int val = 0;

	retval = sscanf(buf, "%d", &val);
	if (retval != 0 && val >= 0 && val <= 1)
		sensor_wake_options_debug = val;
	return size;
}

static ssize_t barcode_emul_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bBarcodeEnabled);
}

static ssize_t barcode_emul_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		set_proximity_barcode_enable(data, true);
	else
		set_proximity_barcode_enable(data, false);

	return size;
}

static DEVICE_ATTR(vendor, S_IRUGO, prox_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, prox_name_show, NULL);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, proximity_raw_data_show, NULL);
static DEVICE_ATTR(barcode_emul_en, S_IRUGO | S_IWUSR | S_IWGRP,
	barcode_emul_enable_show, barcode_emul_enable_store);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(sensor_wake_options, S_IRUGO | S_IWUSR | S_IWGRP,
	sensor_wake_options_show, sensor_wake_options_store);
static DEVICE_ATTR(sensor_wake_options_debug, S_IRUGO | S_IWUSR | S_IWGRP,
	sensor_wake_options_debug_show, sensor_wake_options_debug_store);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_state,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_prox_cal,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_barcode_emul_en,
	&dev_attr_sensor_wake_options,
	&dev_attr_sensor_wake_options_debug,
	NULL,
};

void set_call_in_progress_prox(bool state)
{
	call_in_progress = state;
	if (sensor_wake_options_debug) pr_alert("CALL IN PROGRESS: %d\n", state);
}

void check_prox_value_trig(bool trig)
{
	if (sensor_wake_options && trig && !call_in_progress)
	{
		if (main_prox_data != NULL)
		{
			if (!main_prox_data->bProximityRawEnabled)
			{
				char chTempbuf[2] = { 1, 20};
				send_instruction(main_prox_data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 2);
				main_prox_data->bProximityRawEnabled = true;
			}
		}
		schedule_delayed_work_on(0, &check_prox_val, msecs_to_jiffies(prox_timer_length));
	}
	else if (sensor_wake_options && !trig)
	{
		if (main_prox_data != NULL)
		{
			if (main_prox_data->bProximityRawEnabled)
			{
				char chTempbuf[2] = { 1, 20};
				send_instruction(main_prox_data, REMOVE_SENSOR, PROXIMITY_RAW, chTempbuf, 2);
				main_prox_data->bProximityRawEnabled = false;
			}
		}
		prox_val_step_wave = 0;
		prox_val_step_wave_tout = 0;
		prox_val_step_pocket = 0;
		prox_val_step_pocket_tout = 0;
	}
	trig_timer = trig;
}

void sensorwake_setdev(struct pmic8xxx_pwrkey *input_device) {
	sensorwake_pwrdev = input_device;
	return;
}
extern irqreturn_t pwrkey_press_irq(int irq, void *_pwrkey);
extern irqreturn_t pwrkey_release_irq(int irq, void *_pwrkey);

static void sensorwake_presspwr(struct work_struct *work) //(struct work_struct *screenwake_presspwr_work)
{
	if (sensor_wake_options_debug) pr_alert("SENSOR 2 WAKE - POWER BUTTON FUNC");
	/*input_event(sensorwake_pwrdev, EV_KEY, KEY_POWER, 1);
	msleep(100);
	input_event(sensorwake_pwrdev, EV_SYN, 0, 0);
	msleep(500);
	input_event(sensorwake_pwrdev, EV_KEY, KEY_POWER, 0);
	msleep(100);
	input_event(sensorwake_pwrdev, EV_SYN, 0, 0);
	msleep(500);*/
	//wake_lock_timeout(&wakelock, msecs_to_jiffies(10000));
	//pm_wakeup_event(gdev, 2000);
	//input_report_key(sensorwake_pwrdev->pwr, KEY_POWER, 1);
	//input_sync(sensorwake_pwrdev->pwr);
	//msleep(500);
	//input_report_key(sensorwake_pwrdev->pwr, KEY_POWER, 0);
	//input_sync(sensorwake_pwrdev->pwr);
	pwrkey_press_irq(sensorwake_pwrdev->key_press_irq, sensorwake_pwrdev);
	msleep(200);
	pwrkey_release_irq(sensorwake_pwrdev->key_release_irq, sensorwake_pwrdev);
	//wake_unlock(&wakelock);
	//mutex_unlock(&prx_lock);
}
//static DECLARE_WORK(sensorwake_presspwr_work, sensorwake_presspwr);

void pwr_trig_fsensor(void)
{
	if (!call_in_progress)
	{
		//wake_lock_timeout(&wakelock, msecs_to_jiffies(10000));
		//__pm_stay_awake(ws);
		prox_val_step_wave = 0;
		prox_val_step_wave_tout = 0;
		prox_val_step_pocket = 0;
		prox_val_step_pocket_tout = 0;
		//if (mutex_trylock(&prx_lock)) 
		//	schedule_work(&sensorwake_presspwr_work);
		//wake_lock(&wakelock);
		//msleep(100);
		schedule_delayed_work_on(0, &sensorwake_presspwr_work, msecs_to_jiffies(200));
	}
}

void ischarging_relay(bool status)
{
	ischarging = status;
}
void prox_max_relay(unsigned int val)
{
	wake_options_prox_max = val;
}

static void check_prox_value(struct work_struct *work)
{
	unsigned char prox = main_prox_data->buf[PROXIMITY_RAW].prox[0];
	
	//Wave 2 wake
	if (sensor_wake_options == 1 || (sensor_wake_options == 2 && ischarging) || sensor_wake_options == 5 || (sensor_wake_options == 6 && ischarging))
	{
		if (prox_val_step_wave == 0)
		{
			if (prox_val <= wake_options_prox_max && prox > prox_val+30)
			{
				prox_val_step_wave = 1;
				if (!prox_val_step_wave_tout)
					prox_val_step_wave_tout = jiffies_to_msecs(jiffies);
				prox_timer_length = 50;
			}
		}
		else if (prox_val_step_wave == 1 || prox_val_step_wave == 3)
		{
			if (prox > PROX_BLOCKED)
				prox_val_step_wave++;
		}
		else if (prox_val_step_wave == 2 || prox_val_step_wave == 4)
		{
			if (prox <= wake_options_prox_max)
			{
				if (prox_val_step_wave == 2)
					prox_val_step_wave = 3;
				if (prox_val_step_wave == 4)
				{
					if (sensor_wake_options_debug) pr_alert("WAVE 2 WAKE - POWER BUTTON CALLING");
					pwr_trig_fsensor();
				}
			}
		}

		prox_val = prox;
		if (jiffies_to_msecs(jiffies) - prox_val_step_wave_tout > 4000)
		{
			prox_val_step_wave_tout = 0;
			prox_val_step_wave = 0;
			prox_timer_length = 1000;
		}
	}
	
	//Pocket 2 wake
	if (sensor_wake_options == 3 || (sensor_wake_options == 4 && ischarging) || sensor_wake_options == 5 || (sensor_wake_options == 6 && ischarging))
	{
		if (prox_val_step_pocket == 0)
		{
			if (prox > PROX_BLOCKED)
			{
				prox_val_step_pocket = 1;
				//if (!prox_val_step_pocket_tout)
				prox_val_step_pocket_tout = jiffies_to_msecs(jiffies);
			}
		}
		else if (prox_val_step_pocket == 1)
		{
			if (prox <= wake_options_prox_max)
			{
				if (jiffies_to_msecs(jiffies) - prox_val_step_pocket_tout >= 10000)
				{
					if (sensor_wake_options_debug) pr_alert("POCKET 2 WAKE - POWER BUTTON CALLING");
					pwr_trig_fsensor();
				}
				else
				{
					prox_val_step_pocket = 0;
					prox_val_step_pocket_tout = 0;
				}
			}
		}
	}
	if (sensor_wake_options_debug) pr_alert("SENSOR 2 WAKE - Prox:%d ProxPrevious:%d W2W:%d P2W:%d W2WTimeout:%ld P2WTimeout:%ld\n", prox, prox_val, prox_val_step_wave, prox_val_step_pocket, jiffies_to_msecs(jiffies) - prox_val_step_wave_tout, jiffies_to_msecs(jiffies) - prox_val_step_pocket_tout);

	if (trig_timer && !call_in_progress)
		schedule_delayed_work_on(0, &check_prox_val, msecs_to_jiffies(prox_timer_length));
}

void initialize_prox_factorytest(struct ssp_data *data)
{
	sensors_register(data->prox_device, data,
		prox_attrs, "proximity_sensor");
	main_prox_data = data;
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "ktsensor_wake");
	INIT_DELAYED_WORK(&check_prox_val, check_prox_value);
	INIT_DELAYED_WORK(&sensorwake_presspwr_work, sensorwake_presspwr);
	//ws = wakeup_source_register("kt_wakeup_func");		
}

void remove_prox_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->prox_device, prox_attrs);
}
