#include <linux/wakelock.h>
#include <linux/miscdevice.h>

extern struct ssp_data *main_prox_data;
#define FACTORY_DATA_MAX	64
struct sensor_value {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		struct {
			u16 r;
			u16 g;
			u16 b;
			u16 w;
		};
		u8 prox[4];
		s16 data[9];
		s32 pressure[3];
		u8 sig_motion;
		u8 step_det;
		u32 step_diff;
	};
};

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
	FACTORY_MODE,
	REMOVE_LIBRARY,
	ADD_LIBRARY,
};

/* SENSOR_TYPE */
enum {
	ACCELEROMETER_SENSOR = 0,
	GYROSCOPE_SENSOR,
	GEOMAGNETIC_SENSOR,
	PRESSURE_SENSOR,
	GESTURE_SENSOR,
	PROXIMITY_SENSOR,
	TEMPERATURE_HUMIDITY_SENSOR,
	LIGHT_SENSOR,
	PROXIMITY_RAW,
	GEOMAGNETIC_RAW,
	ORIENTATION_SENSOR,
	SIG_MOTION_SENSOR,
	STEP_DETECTOR,
	STEP_COUNTER,
	SENSOR_MAX,
};

struct calibraion_data {
	s16 x;
	s16 y;
	s16 z;
};

struct hw_offset_data {
	char x;
	char y;
	char z;
};

struct ssp_data {
	struct input_dev *acc_input_dev;
	struct input_dev *gyro_input_dev;
	struct input_dev *pressure_input_dev;
	struct input_dev *light_input_dev;
	struct input_dev *prox_input_dev;
	struct input_dev *temp_humi_input_dev;
	struct input_dev *mag_input_dev;
	struct input_dev *gesture_input_dev;
	struct input_dev *sig_motion_input_dev;
	struct input_dev *step_det_input_dev;
	struct input_dev *step_cnt_input_dev;
	struct i2c_client *client;
	struct wake_lock ssp_wake_lock;
	struct miscdevice akmd_device;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct work_struct work_debug;
	struct calibraion_data accelcal;
	struct calibraion_data gyrocal;
	struct hw_offset_data magoffset;
	struct sensor_value buf[SENSOR_MAX];
	struct device *sen_dev;
	struct device *mcu_device;
	struct device *acc_device;
	struct device *gyro_device;
	struct device *mag_device;
	struct device *prs_device;
	struct device *prox_device;
	struct device *light_device;
	struct delayed_work work_firmware;
	struct device *ges_device;
	struct device *temphumidity_device;

	bool bSspShutdown;
	bool bMcuIRQTestSuccessed;
	bool bAccelAlert;
	bool bProximityRawEnabled;
	bool bGeomagneticRawEnabled;
	bool bBarcodeEnabled;
	bool bBinaryChashed;
	bool bProbeIsDone;

	unsigned char uProxCanc;
	unsigned char uProxHiThresh;
	unsigned char uProxLoThresh;
	unsigned char uProxHiThresh_default;
	unsigned char uProxLoThresh_default;
	unsigned int uIr_Current;
	unsigned char uFuseRomData[3];
	unsigned char uFactorydata[FACTORY_DATA_MAX];
	char *pchLibraryBuf;
	char chLcdLdi[2];
	int iIrq;
	int iLibraryLength;
	int aiCheckStatus[SENSOR_MAX];

	unsigned int uIrqFailCnt;
	unsigned int uSsdFailCnt;
	unsigned int uResetCnt;
	unsigned int uInstFailCnt;
	unsigned int uTimeOutCnt;
	unsigned int uIrqCnt;
	unsigned int uBusyCnt;
	unsigned int uMissSensorCnt;

	unsigned int uGyroDps;
	unsigned int uSensorState;
	unsigned int uCurFirmRev;
	unsigned int uFactoryProxAvg[4];
	unsigned int uFactorydataReady;
	s32 iPressureCal;
	u64 step_count_total;
	atomic_t aSensorEnable;
	int64_t adDelayBuf[SENSOR_MAX];

	int (*wakeup_mcu)(void);
	int (*check_mcu_ready)(void);
	int (*check_mcu_busy)(void);
	int (*set_mcu_reset)(int);
	int (*read_chg)(void);
	void (*get_sensor_data[SENSOR_MAX])(char *, int *,
		struct sensor_value *);
	void (*report_sensor_data[SENSOR_MAX])(struct ssp_data *,
		struct sensor_value *);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	struct ssp_sensorhub_data *hub_data;
#endif
	int ap_rev;
	int ssp_changes;
	int accel_position;
	int mag_position;
	int fw_dl_state;
#ifdef CONFIG_SENSORS_SSP_SHTC1
	char *comp_engine_ver;
	char *comp_engine_ver2;
	struct mutex cp_temp_adc_lock;
#endif
};

