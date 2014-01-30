#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/msm_tsens.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/msm_tsens.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <mach/cpufreq.h>

struct kmsm_thermal_data {
	uint32_t sensor_id;
	int isthrottling;
	uint32_t poll_speed;
	uint32_t temp_limit_degC_start;
	uint32_t temp_limit_degC_stop;
	uint32_t freq_steps_while_throttling;
	uint32_t minimum_throttle_mhz;
	uint32_t core_limit_temp_degC;
	uint32_t core_temp_hysteresis_degC;
	uint32_t core_control_mask;
	long current_temp;
};

static struct kmsm_thermal_data kmsm_thermal_info = {
	.sensor_id = 0,
	.isthrottling = false,
	.poll_speed = 1000,
	.temp_limit_degC_start = 70,
	.temp_limit_degC_stop = 67,
	.freq_steps_while_throttling = 1,
	.minimum_throttle_mhz = 918000,
	.current_temp = 0,
};
