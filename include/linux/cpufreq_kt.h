#include <linux/cpufreq.h>
#include <linux/cpu.h>

extern bool ktoonservative_is_active;
extern bool call_in_progress;

extern void ktoonservative_screen_is_on(bool state);
extern void ktoonservative_boostpulse(bool boost_for_button);

extern void send_cable_state(unsigned int state);
extern void send_cable_state_kt(unsigned int state);

extern void set_gps_status(bool stat);
extern void set_call_in_progress(bool state);
extern void set_call_in_progress_prox(bool state);
extern void set_call_in_progress_scrn(bool state);

extern void set_bluetooth_state(unsigned int val);
extern void set_bluetooth_state_kt(bool val);

extern bool gkt_work_isinitd;
extern struct work_struct gkt_online_work;
extern struct workqueue_struct *gkt_wq;

extern void __ref gkt_online_work_fn(struct work_struct *work);
extern void gkt_work_init(void);
extern void gkt_boost_cpu_call(bool change_screen_state, bool boost_for_button);
