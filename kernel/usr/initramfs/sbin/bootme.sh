#!/sbin/busybox sh

stop
sync
echo "rebooting to recovery now"
sleep 2;
reboot recovery

