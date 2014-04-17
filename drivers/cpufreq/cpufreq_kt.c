#include <linux/cpufreq_kt.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>

bool gkt_work_isinitd = false;
struct work_struct gkt_online_work;
struct workqueue_struct *gkt_wq;

void __ref gkt_online_work_fn(struct work_struct *work)
{
	int cpu, ret;
	struct cpufreq_policy new_policy;
	for_each_possible_cpu(cpu) {
		if (likely(!cpu_online(cpu) && (cpu)))
		{
			cpu_up(cpu);
			if (likely(cpu_online(cpu)))
			{
				ret = cpufreq_get_policy(&new_policy, cpu);
				if (!ret)
					__cpufreq_driver_target(&new_policy, 810000, CPUFREQ_RELATION_H);
			}
		}
	}
}

void gkt_work_init(void)
{
	if (!gkt_work_isinitd)
	{
		pr_alert("KTGlobal WORK INIT\n");
		gkt_wq = alloc_workqueue("gkt_wq", WQ_HIGHPRI, 0);
		if (!gkt_wq) {
			printk(KERN_ERR "Failed to create gkt_wq workqueue\n");
		}
		INIT_WORK(&gkt_online_work, gkt_online_work_fn);
		gkt_work_isinitd = true;
	}
}

void gkt_boost_cpu_call(bool change_screen_state, bool boost_for_button)
{
	if (ktoonservative_is_active)
	{
		//pr_alert("KTGlobal WORK CALL - Ktoonservative mode\n");
		if (change_screen_state)
			ktoonservative_screen_is_on(true);
		else
			ktoonservative_boostpulse(boost_for_button);
	}
	else
	{
		//pr_alert("KTGlobal WORK CALL - Others mode\n");
		if (!gkt_work_isinitd)
			gkt_work_init();
		queue_work_on(0, gkt_wq, &gkt_online_work);
	}
}

