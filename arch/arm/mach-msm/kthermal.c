#include "kthermal.h"

static struct delayed_work check_temp_workk;
static struct cpufreq_frequency_table *table;

static int limit_idx = 18;
static int limit_idx_low = 3;
static int limit_idx_high = 18;

extern void do_kthermal(unsigned int cpu, unsigned int freq);

static int thermal_get_freq_table(void)
{
	int ret = 0;
	int i = 0;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	
	table = cpufreq_frequency_get_table(0);
	if (table == NULL) {
		pr_debug("%s: error reading cpufreq table\n", KBUILD_MODNAME);
		ret = -EINVAL;
		goto fail;
	}

	while (table[i].frequency != CPUFREQ_TABLE_END)
	{
		//pr_alert("LOAD TABLE %d-%d-%d-%d\n", table[i].frequency, kmsm_thermal_info.minimum_throttle_mhz, limit_idx_low, limit_idx_high);
		if (kmsm_thermal_info.minimum_throttle_mhz == table[i].frequency)
			limit_idx_low = i;
		if (policy->user_policy.max == table[i].frequency)
			limit_idx_high = i;
		i++;
	}
	limit_idx = limit_idx_high;
	pr_alert("LOADED THERMAL TABLE: low=%d high=%d current limit = %d\n", limit_idx_low, limit_idx_high, limit_idx);
fail:
	return ret;
}

static void __cpuinit check_tempk(struct work_struct *work)
{
	unsigned int new_freq;
	struct tsens_device tsens_dev;
	long temp = 0;
	int ret = 0;
	
	tsens_dev.sensor_num = kmsm_thermal_info.sensor_id;
	ret = tsens_get_temp(&tsens_dev, &temp);
	if (ret) {
		pr_debug("%s: Unable to read TSENS sensor %d\n",
				KBUILD_MODNAME, tsens_dev.sensor_num);
		goto reschedule;
	}
	//pr_alert("CHECK TEMP %lu-%d-%d\n", temp, kmsm_thermal_info.temp_limit_degC_start, kmsm_thermal_info.temp_limit_degC_stop);
	kmsm_thermal_info.current_temp = temp;
	
	if (temp >= kmsm_thermal_info.temp_limit_degC_start)
	{
		unsigned int i;
		if (!kmsm_thermal_info.isthrottling)
		{
			//prev_freq = cpufreq_get(0);
			thermal_get_freq_table();
			pr_alert("START KTHROTTLING - current temp = %lu - set point = %d\n", temp, kmsm_thermal_info.temp_limit_degC_start);
		}
		kmsm_thermal_info.isthrottling = 1;
		//policy = cpufreq_cpu_get(0);
		//__cpufreq_driver_target(policy, 1296000, CPUFREQ_RELATION_H);
		limit_idx -= kmsm_thermal_info.freq_steps_while_throttling;
		if (limit_idx < limit_idx_low)
			limit_idx = limit_idx_low;
		for (i = 0; i < num_online_cpus(); i++)
		{
			//pr_alert("KTHROTTLING LOOP - current temp = %lu - set point = %d\n", temp, kmsm_thermal_info.temp_limit_degC_start);
			if (cpu_online(i) && cpufreq_get(i) != table[limit_idx].frequency)
			{
				//pr_alert("KTHROTTLING LOOP IN IF - current temp = %lu - set point = %d\n", temp, kmsm_thermal_info.temp_limit_degC_start);
				//policy = NULL;
				//policy = cpufreq_cpu_get(i);
				//if (policy != NULL)
				//	__cpufreq_driver_target(policy, 1296000, CPUFREQ_RELATION_H);
				new_freq = table[limit_idx].frequency;
				do_kthermal(i, new_freq);
			}
		}
	}
	else if (kmsm_thermal_info.isthrottling && temp > kmsm_thermal_info.temp_limit_degC_stop && temp < kmsm_thermal_info.temp_limit_degC_start)
	{
		unsigned int i;
		for (i = 0; i < num_online_cpus(); i++)
		{
			if (cpu_online(i) && cpufreq_get(i) != table[limit_idx].frequency)
			{
				new_freq = table[limit_idx].frequency;
				do_kthermal(i, new_freq);
			}
		}
	}
	else if (kmsm_thermal_info.isthrottling && temp <= kmsm_thermal_info.temp_limit_degC_stop)
	{
		unsigned int i;
		//policy = cpufreq_cpu_get(0);
		//if (prev_freq > 0)
		//	__cpufreq_driver_target(policy, prev_freq, CPUFREQ_RELATION_H);
		limit_idx += kmsm_thermal_info.freq_steps_while_throttling;
		if (limit_idx >= limit_idx_high)
		{
			limit_idx = limit_idx_high;
			kmsm_thermal_info.isthrottling = 0;
			do_kthermal(0, 0);
			pr_alert("STOP KTHROTTLING - current temp = %lu\n", temp);
		}
		for (i = 0; i < num_online_cpus(); i++)
		{
			if (cpu_online(i))
			{
				//policy = NULL;
				//policy = cpufreq_cpu_get(i);
				//if (prev_freq > 0 && policy != NULL)
				//	__cpufreq_driver_target(policy, prev_freq, CPUFREQ_RELATION_H);
				//do_thermal(i, prev_freq);
				new_freq = table[limit_idx].frequency;
				do_kthermal(i, new_freq);
			}
		}
	}

reschedule:
	schedule_delayed_work_on(0, &check_temp_workk,
			msecs_to_jiffies(kmsm_thermal_info.poll_speed));
}

static ssize_t show_isthrottling(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kmsm_thermal_info.isthrottling);
}

static ssize_t show_poll_speed(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kmsm_thermal_info.poll_speed);
}

static ssize_t store_poll_speed(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int value = 0;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;
	
	if (value < 100)
		value = 100;
	kmsm_thermal_info.poll_speed = value;
	return count;
}

static ssize_t show_current_temp(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", kmsm_thermal_info.current_temp);
}

static ssize_t show_temp_limit_degC_start(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kmsm_thermal_info.temp_limit_degC_start);
}

static ssize_t store_temp_limit_degC_start(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int value = 0;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;
	
	if (value < kmsm_thermal_info.temp_limit_degC_stop)
		value = kmsm_thermal_info.temp_limit_degC_stop + 1;
		
	kmsm_thermal_info.temp_limit_degC_start = value;
	return count;
}

static ssize_t show_temp_limit_degC_stop(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kmsm_thermal_info.temp_limit_degC_stop);
}

static ssize_t store_temp_limit_degC_stop(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int value = 0;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	if (value > kmsm_thermal_info.temp_limit_degC_start)
		value = kmsm_thermal_info.temp_limit_degC_start - 1;

	kmsm_thermal_info.temp_limit_degC_stop = value;
	return count;
}

static ssize_t show_freq_steps_while_throttling(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kmsm_thermal_info.freq_steps_while_throttling);
}

static ssize_t store_freq_steps_while_throttling(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int value = 0;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	if (value < 0 || value > 5)
		value = 1;
	kmsm_thermal_info.freq_steps_while_throttling = value;
	return count;
}

static ssize_t show_minimum_throttle_mhz(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kmsm_thermal_info.minimum_throttle_mhz);
}

static ssize_t store_minimum_throttle_mhz(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int value = 0;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	if (value <= 0)
		value = 378000;
		
	kmsm_thermal_info.minimum_throttle_mhz = value;
	return count;
}

static struct global_attr isthrottling_attr = __ATTR(isthrottling, 0444, show_isthrottling, NULL);
static struct global_attr poll_speed_attr = __ATTR(poll_speed, 0666, show_poll_speed, store_poll_speed);
static struct global_attr current_temp_attr = __ATTR(current_temp, 0444, show_current_temp, NULL);
static struct global_attr temp_limit_degC_start_attr = __ATTR(temp_limit_degC_start, 0666, show_temp_limit_degC_start, store_temp_limit_degC_start);
static struct global_attr temp_limit_degC_stop_attr = __ATTR(temp_limit_degC_stop, 0666, show_temp_limit_degC_stop, store_temp_limit_degC_stop);
static struct global_attr freq_steps_while_throttling_attr = __ATTR(freq_steps_while_throttling, 0666, show_freq_steps_while_throttling, store_freq_steps_while_throttling);
static struct global_attr minimum_throttle_mhz_attr = __ATTR(minimum_throttle_mhz, 0666, show_minimum_throttle_mhz, store_minimum_throttle_mhz);

static struct attribute *kthermal_attributes[] = {
	&isthrottling_attr.attr,
	&poll_speed_attr.attr,
	&current_temp_attr.attr,
	&temp_limit_degC_start_attr.attr,
	&temp_limit_degC_stop_attr.attr,
	&freq_steps_while_throttling_attr.attr,
	&minimum_throttle_mhz_attr.attr,
	NULL,
};

static struct attribute_group kthermal_attr_group = {
	.attrs = kthermal_attributes,
	.name = "kthermal",
};

static int __init start_kthermal(void)
{
	int rc;
	rc = sysfs_create_group(cpufreq_global_kobject,
				&kthermal_attr_group);

	pr_alert("START KTHERMAL\n");
	INIT_DELAYED_WORK(&check_temp_workk, check_tempk);
	schedule_delayed_work_on(0, &check_temp_workk, msecs_to_jiffies(60000));
	
	return 0;
}
late_initcall(start_kthermal);
