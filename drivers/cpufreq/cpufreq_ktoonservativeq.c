/*
 *  drivers/cpufreq/cpufreq_ktoonservative.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2009 Alexander Clouter <alex@digriz.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_CPU_DOWN_BLOCK_CYCLES		(11)
#define DEF_BOOST_CPU				(1134000)
#define DEF_BOOST_GPU				(450)
#define DEF_BOOST_HOLD_CYCLES			(22)
#define DEF_DISABLE_HOTPLUGGING			(0)
#define CPUS_AVAILABLE				num_possible_cpus()
static int hotplug_cpu_enable_up[] = { 0, 58, 68, 78 };
static int hotplug_cpu_enable_down[] = { 0, 35, 45, 55 };
static int hotplug_cpu_single_up[] = { 0, 0, 0, 0 };
static int hotplug_cpu_single_down[] = { 0, 0, 0, 0 };
static int hotplug_cpu_lockout[] = { 0, 0, 0, 0 };
static bool hotplug_flag_on = false;
static unsigned int Lcpu_hotplug_block_cycles = 0;
static bool hotplug_flag_off = false;
static bool disable_hotplugging_chrg_override;

void setExtraCores(unsigned int requested_freq);
unsigned int kt_freq_control[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static bool disable_hotplug_bt_active = false;
static unsigned int min_sampling_rate;
static unsigned int stored_sampling_rate = 45000;
static unsigned int Lcpu_down_block_cycles = 0;
static unsigned int Lcpu_up_block_cycles = 0;
static bool boostpulse_relayf = false;
static int boost_hold_cycles_cnt = 0;
static bool screen_is_on = true;

extern void ktoonservative_is_active(bool val);
extern void ktoonservative_is_activebd(bool val);
extern void ktoonservative_is_activepk(bool val);
extern void ktoonservative_is_activehk(bool val);
extern void boost_the_gpu(int freq, int cycles);

extern void apenable_auto_hotplug(bool state);
extern bool apget_enable_auto_hotplug(void);
static bool prev_apenable;
static bool hotplugInProgress = false;

//extern void kt_is_active_benabled_gpio(bool val);
extern void kt_is_active_benabled_touchkey(bool val);
//extern void kt_is_active_benabled_power(bool val);
extern unsigned int get_cable_state(void);
extern void ktoonservative_is_activechrg(bool val);

#define LATENCY_MULTIPLIER			(1000)
#define MIN_LATENCY_MULTIPLIER			(100)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(10)
#define TRANSITION_LATENCY_LIMIT		(10 * 1000 * 1000)

struct work_struct hotplug_offline_work;
struct work_struct hotplug_online_work;

static void do_dbs_timer(struct work_struct *work);

struct cpu_dbs_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
	unsigned int down_skip;
	unsigned int requested_freq;
	int cpu;
	unsigned int enable:1;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, cs_cpu_dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */

/*
 * dbs_mutex protects dbs_enable in governor start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

static struct workqueue_struct *dbs_wq;

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int sampling_rate_screen_off;
	unsigned int sampling_down_factor;
	unsigned int up_threshold;
	unsigned int up_threshold_hotplug_1;
	unsigned int up_threshold_hotplug_2;
	unsigned int up_threshold_hotplug_3;
	unsigned int down_threshold;
	unsigned int down_threshold_hotplug_1;
	unsigned int down_threshold_hotplug_2;
	unsigned int down_threshold_hotplug_3;
	unsigned int cpu_down_block_cycles;
	unsigned int cpu_hotplug_block_cycles;
	unsigned int touch_boost_cpu;
	unsigned int touch_boost_cpu_all_cores;
	unsigned int touch_boost_2nd_core;
	unsigned int touch_boost_3rd_core;
	unsigned int touch_boost_4th_core;
	unsigned int boost_2nd_core_on_button;
	unsigned int boost_3rd_core_on_button;
	unsigned int boost_4th_core_on_button;
	unsigned int lockout_2nd_core_hotplug;
	unsigned int lockout_3rd_core_hotplug;
	unsigned int lockout_4th_core_hotplug;
	//unsigned int touch_boost_gpu;
	unsigned int sync_extra_cores;
	unsigned int boost_hold_cycles;
	unsigned int disable_hotplugging;
	unsigned int disable_hotplugging_chrg;
	unsigned int disable_hotplug_bt;
	unsigned int no_extra_cores_screen_off;
	unsigned int ignore_nice;
	unsigned int freq_step;
} dbs_tuners_ins = {
	.up_threshold = 57,
	.up_threshold_hotplug_1 = 58,
	.up_threshold_hotplug_2 = 68,
	.up_threshold_hotplug_3 = 78,
	.down_threshold = 52,
	.down_threshold_hotplug_1 = 35,
	.down_threshold_hotplug_2 = 45,
	.down_threshold_hotplug_3 = 55,
	.cpu_down_block_cycles = DEF_CPU_DOWN_BLOCK_CYCLES,
	.cpu_hotplug_block_cycles = DEF_CPU_DOWN_BLOCK_CYCLES,
	.touch_boost_cpu = DEF_BOOST_CPU,
	.touch_boost_cpu_all_cores = 0,
	.touch_boost_2nd_core = 1,
	.touch_boost_3rd_core = 0,
	.touch_boost_4th_core = 0,
	.boost_2nd_core_on_button = 1,
	.boost_3rd_core_on_button = 0,
	.boost_4th_core_on_button = 0,
	.lockout_2nd_core_hotplug = 0,
	.lockout_3rd_core_hotplug = 0,
	.lockout_4th_core_hotplug = 0,
	//.touch_boost_gpu = DEF_BOOST_GPU,
	.sync_extra_cores = 0,
	.boost_hold_cycles = DEF_BOOST_HOLD_CYCLES,
	.disable_hotplugging = DEF_DISABLE_HOTPLUGGING,
	.disable_hotplugging_chrg = 0,
	.disable_hotplug_bt = 0,
	.no_extra_cores_screen_off = 1,
	.sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
	.sampling_rate_screen_off = 45000,
	.ignore_nice = 0,
	.freq_step = 5,
};

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu,
							u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, wall);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);

	return idle_time;
}

/* keep track of frequency transitions */
static int
dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		     void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpu_dbs_info_s *this_dbs_info = &per_cpu(cs_cpu_dbs_info,
							freq->cpu);

	struct cpufreq_policy *policy;

	if (!this_dbs_info->enable)
		return 0;

	policy = this_dbs_info->cur_policy;

	/*
	 * we only care if our internally tracked freq moves outside
	 * the 'valid' ranges of freqency available to us otherwise
	 * we do not change it
	*/
	if (this_dbs_info->requested_freq > policy->max
			|| this_dbs_info->requested_freq < policy->min)
		this_dbs_info->requested_freq = freq->new;

	return 0;
}

static struct notifier_block dbs_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier
};

void set_bluetooth_state_kt(bool val)
{
	if (val == true && dbs_tuners_ins.disable_hotplug_bt == 1)
	{
		disable_hotplug_bt_active = true;
		if (num_online_cpus() < 2)
		{
			int cpu;
			for (cpu = 1; cpu < CPUS_AVAILABLE; cpu++)
			{
				if (!cpu_online(cpu))
					hotplug_cpu_single_up[cpu] = 1;
			}
			if (!hotplugInProgress)
				queue_work_on(0, dbs_wq, &hotplug_online_work);
		}
	}
	else
		disable_hotplug_bt_active = false;
}

void send_cable_state_kt(unsigned int state)
{
	int cpu;
	if (state && dbs_tuners_ins.disable_hotplugging_chrg)
	{
		disable_hotplugging_chrg_override = true;
		for (cpu = 1; cpu < CPUS_AVAILABLE; cpu++)
			hotplug_cpu_single_up[cpu] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_online_work);
	}
	else
	{
		disable_hotplugging_chrg_override = false;
	}
}

/************************** sysfs interface ************************/
static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}
define_one_global_ro(sampling_rate_min);

static ssize_t show_touch_boost_cpu(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", dbs_tuners_ins.touch_boost_cpu / 1000);
}

static ssize_t show_touch_boost_cpu_all_cores(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", dbs_tuners_ins.touch_boost_cpu_all_cores);
}

static ssize_t show_sync_extra_cores(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", dbs_tuners_ins.sync_extra_cores);
}

/* cpufreq_ktoonservative Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(sampling_rate_screen_off, sampling_rate_screen_off);
show_one(sampling_down_factor, sampling_down_factor);
show_one(up_threshold, up_threshold);
show_one(up_threshold_hotplug_1, up_threshold_hotplug_1);
show_one(up_threshold_hotplug_2, up_threshold_hotplug_2);
show_one(up_threshold_hotplug_3, up_threshold_hotplug_3);
show_one(down_threshold, down_threshold);
show_one(down_threshold_hotplug_1, down_threshold_hotplug_1);
show_one(down_threshold_hotplug_2, down_threshold_hotplug_2);
show_one(down_threshold_hotplug_3, down_threshold_hotplug_3);
show_one(cpu_down_block_cycles, cpu_down_block_cycles);
show_one(cpu_hotplug_block_cycles, cpu_hotplug_block_cycles);
show_one(touch_boost_2nd_core, touch_boost_2nd_core);
show_one(touch_boost_3rd_core, touch_boost_3rd_core);
show_one(touch_boost_4th_core, touch_boost_4th_core);
show_one(boost_2nd_core_on_button, boost_2nd_core_on_button);
show_one(boost_3rd_core_on_button, boost_3rd_core_on_button);
show_one(boost_4th_core_on_button, boost_4th_core_on_button);
show_one(lockout_2nd_core_hotplug, lockout_2nd_core_hotplug);
show_one(lockout_3rd_core_hotplug, lockout_3rd_core_hotplug);
show_one(lockout_4th_core_hotplug, lockout_4th_core_hotplug);
//show_one(touch_boost_gpu, touch_boost_gpu);
show_one(boost_hold_cycles, boost_hold_cycles);
show_one(disable_hotplugging, disable_hotplugging);
show_one(disable_hotplugging_chrg, disable_hotplugging_chrg);
show_one(disable_hotplug_bt, disable_hotplug_bt);
show_one(no_extra_cores_screen_off, no_extra_cores_screen_off);
show_one(ignore_nice_load, ignore_nice);
show_one(freq_step, freq_step);

static ssize_t store_sampling_down_factor(struct kobject *a,
					  struct attribute *b,
					  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;

	dbs_tuners_ins.sampling_down_factor = input;
	return count;
}

static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	dbs_tuners_ins.sampling_rate = max(input, min_sampling_rate);
	stored_sampling_rate = max(input, min_sampling_rate);
	return count;
}

static ssize_t store_sampling_rate_screen_off(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	dbs_tuners_ins.sampling_rate_screen_off = max(input, min_sampling_rate);
	return count;
}

static ssize_t store_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 ||
			input <= dbs_tuners_ins.down_threshold)
		return -EINVAL;

	dbs_tuners_ins.up_threshold = input;
	return count;
}

static ssize_t store_up_threshold_hotplug_1(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 ||
			input <= dbs_tuners_ins.down_threshold_hotplug_1)
		return -EINVAL;

	dbs_tuners_ins.up_threshold_hotplug_1 = input;
	hotplug_cpu_enable_up[1] = input;
	return count;
}

static ssize_t store_up_threshold_hotplug_2(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 ||
			input <= dbs_tuners_ins.down_threshold_hotplug_2)
		return -EINVAL;

	dbs_tuners_ins.up_threshold_hotplug_2 = input;
	hotplug_cpu_enable_up[2] = input;
	return count;
}

static ssize_t store_up_threshold_hotplug_3(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 ||
			input <= dbs_tuners_ins.down_threshold_hotplug_3)
		return -EINVAL;

	dbs_tuners_ins.up_threshold_hotplug_3 = input;
	hotplug_cpu_enable_up[3] = input;
	return count;
}

static ssize_t store_down_threshold(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= dbs_tuners_ins.up_threshold)
		return -EINVAL;

	dbs_tuners_ins.down_threshold = input;
	return count;
}

static ssize_t store_down_threshold_hotplug_1(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= dbs_tuners_ins.up_threshold_hotplug_1)
		return -EINVAL;

	dbs_tuners_ins.down_threshold_hotplug_1 = input;
	hotplug_cpu_enable_down[1] = input;
	return count;
}

static ssize_t store_down_threshold_hotplug_2(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= dbs_tuners_ins.up_threshold_hotplug_2)
		return -EINVAL;

	dbs_tuners_ins.down_threshold_hotplug_2 = input;
	hotplug_cpu_enable_down[2] = input;
	return count;
}

static ssize_t store_down_threshold_hotplug_3(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= dbs_tuners_ins.up_threshold_hotplug_3)
		return -EINVAL;

	dbs_tuners_ins.down_threshold_hotplug_3 = input;
	hotplug_cpu_enable_down[3] = input;
	return count;
}

static ssize_t store_cpu_down_block_cycles(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (input < 0)
		return -EINVAL;

	dbs_tuners_ins.cpu_down_block_cycles = input;
	return count;
}

static ssize_t store_cpu_hotplug_block_cycles(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (input < 0)
		return -EINVAL;

	dbs_tuners_ins.cpu_hotplug_block_cycles = input;
	return count;
}

static ssize_t store_touch_boost_cpu(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input * 1000 > GLOBALKT_MAX_FREQ_LIMIT)
		input = GLOBALKT_MAX_FREQ_LIMIT;
	if (input * 1000 < 0)
		input = 0;
	dbs_tuners_ins.touch_boost_cpu = input * 1000;
	return count;
}

static ssize_t store_touch_boost_cpu_all_cores(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret, i;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input != 0 && input != 1)
		input = 1;
	dbs_tuners_ins.touch_boost_cpu_all_cores = input;

	if (dbs_tuners_ins.sync_extra_cores == 0 && dbs_tuners_ins.touch_boost_cpu_all_cores == 0)
	{
		for (i = 0; i < CPUS_AVAILABLE; i++)
			kt_freq_control[i] = 0;
	}
	return count;
}

static ssize_t store_sync_extra_cores(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret, i;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input != 0 && input != 1)
		input = 1;
	dbs_tuners_ins.sync_extra_cores = input;
	
	if (dbs_tuners_ins.sync_extra_cores == 0 && dbs_tuners_ins.touch_boost_cpu_all_cores == 0)
	{
		for (i = 0; i < CPUS_AVAILABLE; i++)
			kt_freq_control[i] = 0;
	}
	return count;
}

static ssize_t store_touch_boost_2nd_core(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.touch_boost_2nd_core = input;
	return count;
}

static ssize_t store_touch_boost_3rd_core(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.touch_boost_3rd_core = input;
	return count;
}

static ssize_t store_touch_boost_4th_core(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.touch_boost_4th_core = input;
	return count;
}

static ssize_t store_lockout_2nd_core_hotplug(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret, cpu;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1 && input != 2)
		input = 0;

	dbs_tuners_ins.lockout_2nd_core_hotplug = input;
	hotplug_cpu_lockout[1] = input;
	if (input == 1)
	{
		hotplug_cpu_single_up[1] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_online_work);
	}
	else if (input == 2)
	{
		hotplug_cpu_single_down[1] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_offline_work);
	}
	return count;
}

static ssize_t store_lockout_3rd_core_hotplug(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret, cpu;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1 && input != 2)
		input = 0;

	dbs_tuners_ins.lockout_3rd_core_hotplug = input;
	hotplug_cpu_lockout[2] = input;
	if (input == 1)
	{
		hotplug_cpu_single_up[2] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_online_work);
	}
	else if (input == 2)
	{
		hotplug_cpu_single_down[2] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_offline_work);
	}
	return count;
}

static ssize_t store_lockout_4th_core_hotplug(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret, cpu;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1 && input != 2)
		input = 0;

	dbs_tuners_ins.lockout_4th_core_hotplug = input;
	hotplug_cpu_lockout[3] = input;
	if (input == 1)
	{
		hotplug_cpu_single_up[3] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_online_work);
	}
	else if (input == 2)
	{
		hotplug_cpu_single_down[3] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_offline_work);
	}
	return count;
}

/*static ssize_t store_touch_boost_gpu(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 100 && input != 160 && input != 266 && input != 350 && input != 400 && input != 450 && input != 533 && input != 612)
		input = 0;

	dbs_tuners_ins.touch_boost_gpu = input;
	return count;
}*/

static ssize_t store_boost_hold_cycles(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input < 0)
		return -EINVAL;

	dbs_tuners_ins.boost_hold_cycles = input;
	return count;
}

static ssize_t store_disable_hotplugging(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret, cpu;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.disable_hotplugging = input;
	if (input == 1)
	{
		for (cpu = 1; cpu < CPUS_AVAILABLE; cpu++)
			hotplug_cpu_single_up[cpu] = 1;
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_online_work);
	}
	return count;
}

static ssize_t store_disable_hotplugging_chrg(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input, c_state;
	int ret, cpu;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.disable_hotplugging_chrg = input;
	c_state = get_cable_state();
	send_cable_state_kt(c_state);
		
	return count;
}

static ssize_t store_no_extra_cores_screen_off(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.no_extra_cores_screen_off = input;
	return count;
}

static ssize_t store_boost_2nd_core_on_button(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.boost_2nd_core_on_button = input;
	if (dbs_tuners_ins.boost_2nd_core_on_button == 1)
	{
		//kt_is_active_benabled_gpio(true);
		kt_is_active_benabled_touchkey(true);
		//kt_is_active_benabled_power(true);
	}

	return count;
}

static ssize_t store_boost_3rd_core_on_button(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.boost_3rd_core_on_button = input;
	if (dbs_tuners_ins.boost_3rd_core_on_button == 1)
	{
		//kt_is_active_benabled_gpio(true);
		kt_is_active_benabled_touchkey(true);
		//kt_is_active_benabled_power(true);
	}

	return count;
}

static ssize_t store_boost_4th_core_on_button(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.boost_4th_core_on_button = input;
	if (dbs_tuners_ins.boost_4th_core_on_button == 1)
	{
		//kt_is_active_benabled_gpio(true);
		kt_is_active_benabled_touchkey(true);
		//kt_is_active_benabled_power(true);
	}

	return count;
}

static ssize_t store_disable_hotplug_bt(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input != 0 && input != 1)
		input = 0;

	dbs_tuners_ins.disable_hotplug_bt = input;
	return count;
}

static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == dbs_tuners_ins.ignore_nice) /* nothing to do */
		return count;

	dbs_tuners_ins.ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(cs_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&dbs_info->prev_cpu_wall);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
	}
	return count;
}

static ssize_t store_freq_step(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 100)
		input = 100;

	/* no need to test here if freq_step is zero as the user might actually
	 * want this, they would be crazy though :) */
	dbs_tuners_ins.freq_step = input;
	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(sampling_rate_screen_off);
define_one_global_rw(sampling_down_factor);
define_one_global_rw(up_threshold);
define_one_global_rw(up_threshold_hotplug_1);
define_one_global_rw(up_threshold_hotplug_2);
define_one_global_rw(up_threshold_hotplug_3);
define_one_global_rw(down_threshold);
define_one_global_rw(down_threshold_hotplug_1);
define_one_global_rw(down_threshold_hotplug_2);
define_one_global_rw(down_threshold_hotplug_3);
define_one_global_rw(cpu_down_block_cycles);
define_one_global_rw(cpu_hotplug_block_cycles);
define_one_global_rw(touch_boost_cpu);
define_one_global_rw(touch_boost_cpu_all_cores);
define_one_global_rw(touch_boost_2nd_core);
define_one_global_rw(touch_boost_3rd_core);
define_one_global_rw(touch_boost_4th_core);
define_one_global_rw(boost_2nd_core_on_button);
define_one_global_rw(boost_3rd_core_on_button);
define_one_global_rw(boost_4th_core_on_button);
define_one_global_rw(lockout_2nd_core_hotplug);
define_one_global_rw(lockout_3rd_core_hotplug);
define_one_global_rw(lockout_4th_core_hotplug);
//define_one_global_rw(touch_boost_gpu);
define_one_global_rw(sync_extra_cores);
define_one_global_rw(boost_hold_cycles);
define_one_global_rw(disable_hotplugging);
define_one_global_rw(disable_hotplugging_chrg);
define_one_global_rw(disable_hotplug_bt);
define_one_global_rw(no_extra_cores_screen_off);
define_one_global_rw(ignore_nice_load);
define_one_global_rw(freq_step);

static struct attribute *dbs_attributes[] = {
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&sampling_rate_screen_off.attr,
	&sampling_down_factor.attr,
	&up_threshold.attr,
	&up_threshold_hotplug_1.attr,
	&up_threshold_hotplug_2.attr,
	&up_threshold_hotplug_3.attr,
	&down_threshold.attr,
	&down_threshold_hotplug_1.attr,
	&down_threshold_hotplug_2.attr,
	&down_threshold_hotplug_3.attr,
	&cpu_down_block_cycles.attr,
	&cpu_hotplug_block_cycles.attr,
	&touch_boost_cpu.attr,
	&touch_boost_cpu_all_cores.attr,
	&touch_boost_2nd_core.attr,
	&touch_boost_3rd_core.attr,
	&touch_boost_4th_core.attr,
	&boost_2nd_core_on_button.attr,
	&boost_3rd_core_on_button.attr,
	&boost_4th_core_on_button.attr,
	&lockout_2nd_core_hotplug.attr,
	&lockout_3rd_core_hotplug.attr,
	&lockout_4th_core_hotplug.attr,
	//&touch_boost_gpu.attr,
	&sync_extra_cores.attr,
	&boost_hold_cycles.attr,
	&disable_hotplugging.attr,
	&disable_hotplugging_chrg.attr,
	&disable_hotplug_bt.attr,
	&no_extra_cores_screen_off.attr,
	&ignore_nice_load.attr,
	&freq_step.attr,
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "ktoonservativeq",
};

/************************** sysfs end ************************/

static void dbs_check_cpu(struct cpu_dbs_info_s *this_dbs_info)
{
	unsigned int load = 0;
	unsigned int max_load = 0;
	unsigned int freq_target;
	int cpu;
	struct cpufreq_policy *policy;
	unsigned int j;

	policy = this_dbs_info->cur_policy;

	if (boostpulse_relayf)
	{
		if (stored_sampling_rate != 0 && screen_is_on)
			dbs_tuners_ins.sampling_rate = stored_sampling_rate;
		this_dbs_info->down_skip = 0;

		if (boost_hold_cycles_cnt >= dbs_tuners_ins.boost_hold_cycles)
		{
			boostpulse_relayf = false;
			boost_hold_cycles_cnt = 0;
			if (dbs_tuners_ins.sync_extra_cores == 0)
			{
				for (cpu = 0; cpu < CPUS_AVAILABLE; cpu++)
					kt_freq_control[cpu] = 0;
			}
			goto boostcomplete;
		}
		boost_hold_cycles_cnt++;

		if (dbs_tuners_ins.touch_boost_cpu_all_cores && policy->cpu == 0)
		{
			for (cpu = 1; cpu < CPUS_AVAILABLE; cpu++)
			{
				if (&trmlpolicy[cpu] != NULL)
				{
					if (cpu_online(cpu))
					{
						if (trmlpolicy[cpu].cur < dbs_tuners_ins.touch_boost_cpu)
						{
							//__cpufreq_driver_target(&trmlpolicy[cpu], dbs_tuners_ins.touch_boost_cpu,
							//	CPUFREQ_RELATION_H);
							kt_freq_control[cpu] = dbs_tuners_ins.touch_boost_cpu;
							//pr_alert("BOOST EXTRA CPUs: %d\n", cpu);
						}
					}
				}
			}
		}
		
		/* if we are already at full speed then break out early */
		if (this_dbs_info->requested_freq == policy->max || policy->cur > dbs_tuners_ins.touch_boost_cpu || this_dbs_info->requested_freq > dbs_tuners_ins.touch_boost_cpu)
			return;
		
		this_dbs_info->requested_freq = dbs_tuners_ins.touch_boost_cpu;
		__cpufreq_driver_target(policy, this_dbs_info->requested_freq,
			CPUFREQ_RELATION_H);
boostcomplete:
		return;
	}
	
	/*
	 * Every sampling_rate, we check, if current idle time is less
	 * than 20% (default), then we try to increase frequency
	 * Every sampling_rate*sampling_down_factor, we check, if current
	 * idle time is more than 80%, then we try to decrease frequency
	 *
	 * Any frequency increase takes it to the maximum frequency.
	 * Frequency reduction happens at minimum steps of
	 * 5% (default) of maximum frequency
	 */

	/* Get Absolute Load */
	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info_s *j_dbs_info;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;

		j_dbs_info = &per_cpu(cs_cpu_dbs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - j_dbs_info->prev_cpu_wall);
		j_dbs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;

		if (dbs_tuners_ins.ignore_nice) {
			u64 cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] -
					 j_dbs_info->prev_cpu_nice;
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
		if (load > max_load)
			max_load = load;
		//max_load += load;
		//pr_alert("LOAD CHECK2: %d-%d", load, max_load);
	}
	//max_load = max_load / num_online_cpus();
	/*
	 * break out if we 'cannot' reduce the speed as the user might
	 * want freq_step to be zero
	 */
	if (dbs_tuners_ins.freq_step == 0)
		return;
	
	if (policy->cpu == 0)
	{
		for (cpu = 1; cpu < CPUS_AVAILABLE; cpu++)
		{
			if (max_load >= hotplug_cpu_enable_up[cpu] && (!cpu_online(cpu)) && hotplug_cpu_lockout[cpu] != 2)
			{
				if (Lcpu_hotplug_block_cycles > dbs_tuners_ins.cpu_hotplug_block_cycles)
				{
					hotplug_cpu_single_up[cpu] = 1;
					hotplug_flag_on = true;
					Lcpu_hotplug_block_cycles = 0;
				}
				Lcpu_hotplug_block_cycles++;
				break;
			}
			else if (max_load <= hotplug_cpu_enable_down[CPUS_AVAILABLE - cpu] && (cpu_online(CPUS_AVAILABLE - cpu)) && hotplug_cpu_lockout[CPUS_AVAILABLE - cpu] != 1)
			{
				hotplug_cpu_single_down[CPUS_AVAILABLE - cpu] = 1;
				hotplug_flag_off = true;
				break;
			}
		}
		//pr_alert("LOAD CHECK: %d-%d-%d-%d-%d-%d-%d\n", max_load, hotplug_cpu_single_up[1], hotplug_cpu_single_up[2], hotplug_cpu_single_up[3], hotplug_cpu_enable_up[1], hotplug_cpu_enable_up[2], hotplug_cpu_enable_up[3]);
	
		/* Check for frequency increase is greater than hotplug value */
		//CPUS_AVAILABLE
		if (hotplug_flag_on) {
			if (policy->cur > (policy->min * 2))
			{
				if (Lcpu_up_block_cycles > dbs_tuners_ins.cpu_down_block_cycles && (dbs_tuners_ins.no_extra_cores_screen_off == 0 || (dbs_tuners_ins.no_extra_cores_screen_off == 1 && screen_is_on)))
				{
					hotplug_flag_on = false;
					if (!hotplugInProgress && policy->cpu == 0)
						queue_work_on(policy->cpu, dbs_wq, &hotplug_online_work);
					Lcpu_up_block_cycles = 0;
				}
				Lcpu_up_block_cycles++;
			}
		}
	}

	/* Check for frequency increase */
	if (max_load > dbs_tuners_ins.up_threshold) {
		this_dbs_info->down_skip = 0;

		/* if we are already at full speed then break out early */
		if (this_dbs_info->requested_freq == policy->max)
			return;

		freq_target = (dbs_tuners_ins.freq_step * policy->max) / 100;

		/* max freq cannot be less than 100. But who knows.... */
		if (unlikely(freq_target == 0))
			freq_target = 5;

		this_dbs_info->requested_freq += freq_target;
		if (this_dbs_info->requested_freq > policy->max)
			this_dbs_info->requested_freq = policy->max;

		__cpufreq_driver_target(policy, this_dbs_info->requested_freq, CPUFREQ_RELATION_H);
		if (dbs_tuners_ins.sync_extra_cores && policy->cpu == 0)
			setExtraCores(this_dbs_info->requested_freq);
		return;
	}
	
	if (policy->cpu == 0 && hotplug_flag_off && !dbs_tuners_ins.disable_hotplugging && !disable_hotplugging_chrg_override && disable_hotplug_bt_active == false) {
		if (num_online_cpus() > 1)
		{
			if (Lcpu_down_block_cycles > dbs_tuners_ins.cpu_down_block_cycles)
			{
				hotplug_flag_off = false;
				if (!hotplugInProgress && policy->cpu == 0)
					queue_work_on(policy->cpu, dbs_wq, &hotplug_offline_work);
				Lcpu_down_block_cycles = 0;
			}
			Lcpu_down_block_cycles++;
		}
	}
	/*
	 * The optimal frequency is the frequency that is the lowest that
	 * can support the current CPU usage without triggering the up
	 * policy. To be safe, we focus 10 points under the threshold.
	 */
	if (max_load < (dbs_tuners_ins.down_threshold - 10)) {
		freq_target = (dbs_tuners_ins.freq_step * policy->max) / 100;

		this_dbs_info->requested_freq -= freq_target;
		if (this_dbs_info->requested_freq < policy->min)
			this_dbs_info->requested_freq = policy->min;

		/*
		 * if we cannot reduce the frequency anymore, break out early
		 */
		if (policy->cur == policy->min)
			return;

		__cpufreq_driver_target(policy, this_dbs_info->requested_freq, CPUFREQ_RELATION_H);
		if (dbs_tuners_ins.sync_extra_cores && policy->cpu == 0)
			setExtraCores(this_dbs_info->requested_freq);
		return;
	}
}

void setExtraCores(unsigned int requested_freq)
{
	unsigned int cpu;
	for (cpu = 1; cpu < CPUS_AVAILABLE; cpu++)
	{
		if (&trmlpolicy[cpu] != NULL)
		{
			if (cpu_online(cpu))
			{
				//__cpufreq_driver_target(&trmlpolicy[cpu], requested_freq, CPUFREQ_RELATION_H);
				kt_freq_control[cpu] = requested_freq;
				//pr_alert("BOOST EXTRA CPUs: %d\n", cpu);
			}
		}
	}
}

void check_boost_cores_up(bool dec1, bool dec2, bool dec3)
{
	bool got_boost_core = false;

	if (!cpu_online(1) && dec1 && hotplug_cpu_lockout[1] != 2)
	{
		hotplug_cpu_single_up[1] = 1;
		got_boost_core = true;
	}
	if (!cpu_online(2) && dec2 && hotplug_cpu_lockout[2] != 2)
	{
		hotplug_cpu_single_up[2] = 1;
		got_boost_core = true;
	}
	if (!cpu_online(3) && dec3 && hotplug_cpu_lockout[3] != 2)
	{
		hotplug_cpu_single_up[3] = 1;
		got_boost_core = true;
	}
	if (got_boost_core)
	{
		if (!hotplugInProgress)
			queue_work_on(0, dbs_wq, &hotplug_online_work);
	}
}

void screen_is_on_relay_kt(bool state)
{
	screen_is_on = state;
	if (state == true)
	{
		if (stored_sampling_rate > 0)
			dbs_tuners_ins.sampling_rate = stored_sampling_rate; //max(input, min_sampling_rate);

		check_boost_cores_up(dbs_tuners_ins.boost_2nd_core_on_button, dbs_tuners_ins.boost_3rd_core_on_button, dbs_tuners_ins.boost_4th_core_on_button);
				
		//pr_alert("SCREEN_IS_ON1: %d-%d\n", dbs_tuners_ins.sampling_rate, stored_sampling_rate);
	}
	else
	{
		stored_sampling_rate = dbs_tuners_ins.sampling_rate;
		dbs_tuners_ins.sampling_rate = dbs_tuners_ins.sampling_rate_screen_off;
		//pr_alert("SCREEN_IS_ON2: %d-%d\n", dbs_tuners_ins.sampling_rate, stored_sampling_rate);
	}
	
}

void boostpulse_relay_kt(void)
{
	if (!boostpulse_relayf)
	{
		bool got_boost_core = false;

		if (dbs_tuners_ins.touch_boost_2nd_core == 0 && dbs_tuners_ins.touch_boost_3rd_core == 0 && dbs_tuners_ins.touch_boost_4th_core == 0 && dbs_tuners_ins.touch_boost_cpu == 0) // && dbs_tuners_ins.touch_boost_gpu == 0)
			return;
		/*if (dbs_tuners_ins.touch_boost_gpu > 0)
		{
			int bpc = (dbs_tuners_ins.boost_hold_cycles / 2);
			if (dbs_tuners_ins.boost_hold_cycles > 0)
				boost_the_gpu(dbs_tuners_ins.touch_boost_gpu, bpc);
			else
				boost_the_gpu(dbs_tuners_ins.touch_boost_gpu, 0);
		}*/
		check_boost_cores_up(dbs_tuners_ins.touch_boost_2nd_core, dbs_tuners_ins.touch_boost_3rd_core, dbs_tuners_ins.touch_boost_4th_core);
			
		boostpulse_relayf = true;
		boost_hold_cycles_cnt = 0;
		//dbs_tuners_ins.sampling_rate = min_sampling_rate;
		//pr_info("BOOSTPULSE RELAY KT");
	}
	else
	{
		/*if (dbs_tuners_ins.touch_boost_gpu > 0)
		{
			int bpc = (dbs_tuners_ins.boost_hold_cycles / 2);
			if (dbs_tuners_ins.boost_hold_cycles > 0)
				boost_the_gpu(dbs_tuners_ins.touch_boost_gpu, bpc);
			else
				boost_the_gpu(dbs_tuners_ins.touch_boost_gpu, 0);
		}*/
		boost_hold_cycles_cnt = 0;
	}
}

static void __cpuinit hotplug_offline_work_fn(struct work_struct *work)
{
	int cpu;
	//pr_info("ENTER OFFLINE");
	for_each_online_cpu(cpu) {
		if (likely(cpu_online(cpu) && (cpu))) {
			if (hotplug_cpu_single_down[cpu])
			{
				hotplug_cpu_single_down[cpu] = 0;
				cpu_down(cpu);
			}
			//pr_info("auto_hotplug: CPU%d down.\n", cpu);
		}
	}
	hotplugInProgress = false;
}

static void __cpuinit hotplug_online_work_fn(struct work_struct *work)
{
	int cpu;
	//pr_info("ENTER ONLINE");
	for_each_possible_cpu(cpu) {
		if (likely(!cpu_online(cpu) && (cpu))) {
			if (hotplug_cpu_single_up[cpu])
			{
				hotplug_cpu_single_up[cpu] = 0;
				cpu_up(cpu);
			}
			//pr_info("auto_hotplug: CPU%d up.\n", cpu);
		}
	}
	hotplugInProgress = false;
}

static void do_dbs_timer(struct work_struct *work)
{
	struct cpu_dbs_info_s *dbs_info =
		container_of(work, struct cpu_dbs_info_s, work.work);
	unsigned int cpu = dbs_info->cpu;

	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	delay -= jiffies % delay;

	mutex_lock(&dbs_info->timer_mutex);

	dbs_check_cpu(dbs_info);

	queue_delayed_work_on(cpu, dbs_wq, &dbs_info->work, delay);
	mutex_unlock(&dbs_info->timer_mutex);
}

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	delay -= jiffies % delay;

	dbs_info->enable = 1;
	INIT_DELAYED_WORK_DEFERRABLE(&dbs_info->work, do_dbs_timer);
	queue_delayed_work_on(dbs_info->cpu, dbs_wq, &dbs_info->work, delay);
}

static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info)
{
	dbs_info->enable = 0;
	cancel_delayed_work_sync(&dbs_info->work);
	cancel_work_sync(&hotplug_offline_work);
	cancel_work_sync(&hotplug_online_work);
}

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;

	this_dbs_info = &per_cpu(cs_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		ktoonservative_is_active(true);
		ktoonservative_is_activebd(true);
		ktoonservative_is_activepk(true);
		ktoonservative_is_activehk(true);
		ktoonservative_is_activechrg(true);
		if (dbs_tuners_ins.boost_2nd_core_on_button == 1 || dbs_tuners_ins.boost_3rd_core_on_button == 1 || dbs_tuners_ins.boost_4th_core_on_button == 1)
    		{
      			//kt_is_active_benabled_gpio(true);
		      	kt_is_active_benabled_touchkey(true);
      			//kt_is_active_benabled_power(true);
    		}
		
		prev_apenable = apget_enable_auto_hotplug();
		apenable_auto_hotplug(false);
		
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);

		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(cs_cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;

			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&j_dbs_info->prev_cpu_wall);
			if (dbs_tuners_ins.ignore_nice) {
				j_dbs_info->prev_cpu_nice =
						kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			}
		}
		this_dbs_info->cpu = cpu;
		this_dbs_info->down_skip = 0;
		this_dbs_info->requested_freq = policy->cur;

		mutex_init(&this_dbs_info->timer_mutex);
		dbs_enable++;
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 1) {
			unsigned int latency;
			/* policy latency is in nS. Convert it to uS first */
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;

			rc = sysfs_create_group(cpufreq_global_kobject,
						&dbs_attr_group);
			if (rc) {
				mutex_unlock(&dbs_mutex);
				return rc;
			}

			min_sampling_rate = (MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10)) / 20;
			/* Bring kernel and HW constraints together */
			min_sampling_rate = max(min_sampling_rate,
					MIN_LATENCY_MULTIPLIER * latency);
			dbs_tuners_ins.sampling_rate = 45000;
				//max((min_sampling_rate * 20),
				    //latency * LATENCY_MULTIPLIER);

			cpufreq_register_notifier(
					&dbs_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}
		mutex_unlock(&dbs_mutex);

		dbs_timer_init(this_dbs_info);

		break;

	case CPUFREQ_GOV_STOP:
		ktoonservative_is_active(false);
		ktoonservative_is_activebd(false);
		ktoonservative_is_activepk(false);
		ktoonservative_is_activehk(false);
		ktoonservative_is_activechrg(false);
    		//kt_is_active_benabled_gpio(false);
    		kt_is_active_benabled_touchkey(false);
    		//kt_is_active_benabled_power(false);
		
		apenable_auto_hotplug(prev_apenable);
		
		dbs_timer_exit(this_dbs_info);

		mutex_lock(&dbs_mutex);
		dbs_enable--;
		mutex_destroy(&this_dbs_info->timer_mutex);

		/*
		 * Stop the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 0)
			cpufreq_unregister_notifier(
					&dbs_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);

		mutex_unlock(&dbs_mutex);
		if (!dbs_enable)
			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);

		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&this_dbs_info->timer_mutex);
		if (policy->max < this_dbs_info->cur_policy->cur)
		{
			__cpufreq_driver_target(this_dbs_info->cur_policy, policy->max, CPUFREQ_RELATION_H);
		}
		else if (policy->min > this_dbs_info->cur_policy->cur)
		{
			__cpufreq_driver_target(this_dbs_info->cur_policy, policy->min, CPUFREQ_RELATION_L);
		}
		dbs_check_cpu(this_dbs_info);
		mutex_unlock(&this_dbs_info->timer_mutex);

		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_KTOONSERVATIVEQ
static
#endif
struct cpufreq_governor cpufreq_gov_ktoonservative = {
	.name			= "ktoonservativeq",
	.governor		= cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	dbs_wq = alloc_workqueue("ktoonservativeq_dbs_wq", WQ_HIGHPRI, 0);
	if (!dbs_wq) {
		printk(KERN_ERR "Failed to create ktoonservativeq_dbs_wq workqueue\n");
		return -EFAULT;
	}

	INIT_WORK(&hotplug_offline_work, hotplug_offline_work_fn);
	INIT_WORK(&hotplug_online_work, hotplug_online_work_fn);
	
	return cpufreq_register_governor(&cpufreq_gov_ktoonservative);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_ktoonservative);
	destroy_workqueue(dbs_wq);
}

MODULE_AUTHOR("Alexander Clouter <alex@digriz.org.uk>");
MODULE_DESCRIPTION("'cpufreq_ktoonservativeq' - A dynamic cpufreq governor for "
		"Low Latency Frequency Transition capable processors "
		"optimised for use in a battery environment");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_KTOONSERVATIVEQ
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
