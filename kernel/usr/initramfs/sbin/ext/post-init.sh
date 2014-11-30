#!/system/bin/sh

#Set Max Mhz for GPU
echo 450000000 > /sys/devices/platform/kgsl-3d0.0/kgsl/kgsl-3d0/max_gpuclk

#Set Max Mhz speed and booted flag to set Super Max
echo 1890000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq;
echo 1 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_booted;

echo 1 > /proc/sys/net/ipv4/tcp_tw_recycle;
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse;
echo 1024 > /proc/sys/kernel/random/read_wakeup_threshold;
echo 2048 > /proc/sys/kernel/random/write_wakeup_threshold;
echo 256000000 > /proc/sys/kernel/shmmax;
echo 1024000 > /proc/sys/net/core/rmem_max;
echo 1024000 > /proc/sys/net/core/wmem_max;

# General Tweaks, thanks to Osmosis and Malaroths for most of this
echo 512 > /sys/block/mmcblk0/queue/nr_requests;
echo 256 > /sys/block/mmcblk0/queue/read_ahead_kb;
echo 2 > /sys/block/mmcblk0/queue/rq_affinity;
echo 0 > /sys/block/mmcblk0/queue/nomerges;
echo 0 > /sys/block/mmcblk0/queue/rotational;
echo 0 > /sys/block/mmcblk0/queue/add_random;
echo 0 > /sys/block/mmcblk0/queue/iostats;

#VM tweaks
echo 20 > /proc/sys/vm/vfs_cache_pressure;
echo 8192 > /proc/sys/vm/min_free_kbytes
echo 0 > /proc/sys/vm/swappiness;
echo 70 > /proc/sys/vm/dirty_ratio;
echo 20000 > /proc/sys/vm/dirty_writeback_centisecs;
echo 20000 > /proc/sys/vm/dirty_expire_centisecs;
echo 50 > /proc/sys/vm/dirty_background_ratio;

#Disable debug
echo "0" > /sys/module/kernel/parameters/initcall_debug;
echo "0" > /sys/module/earlysuspend/parameters/debug_mask;
echo "0" > /sys/module/alarm/parameters/debug_mask;
echo "0" > /sys/module/alarm_dev/parameters/debug_mask;
echo "0" > /sys/module/binder/parameters/debug_mask;
echo "0" > /sys/module/xt_qtaguid/parameters/debug_mask;

# Cache Tweaks, thanks to brees75 for this stuff
echo 2048 > /sys/devices/virtual/bdi/0:18/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/0:19/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/1:0/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:1/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:2/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:3/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:4/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:5/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:6/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:7/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:8/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:9/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:10/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:11/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:12/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:13/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:14/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/1:15/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/7:0/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:1/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:2/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:3/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:4/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:5/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:6/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:7/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:8/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:9/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:10/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:11/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:12/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:13/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:14/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:15/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:16/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:17/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:18/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:19/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:20/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:21/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:22/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:23/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:24/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:25/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:26/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:27/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:28/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:29/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:30/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:31/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:32/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:33/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:34/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:35/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:36/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:37/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/7:38/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/179:0/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/179:8/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/179:16/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/253:0/read_ahead_kb
echo 2048 > /sys/devices/virtual/bdi/254:0/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:1/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:2/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:3/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:4/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:5/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:6/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:7/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:8/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:9/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:10/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:11/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:12/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:13/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:14/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:15/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:16/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:17/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:18/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:19/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:20/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:21/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:22/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:23/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:24/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:25/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:26/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:27/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:28/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:29/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:30/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:31/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:32/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:33/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:34/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:35/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:36/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:37/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/254:38/read_ahead_kb
echo 256 > /sys/devices/virtual/bdi/default/read_ahead_kb

echo "Running Post-Init Script"

#Check for forcing Samsung MTP
if [ -f /data/.ktoonsez/force_samsung_mtp ];
then
  # BackUp old post-init log
  echo "Forcing Samsung MTP"
  echo 1 > /sys/devices/system/cpu/cpufreq/ktoonsez/enable_google_mtp;
fi

echo $(date) END of post-init.sh
