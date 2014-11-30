#!/sbin/busybox sh

/sbin/busybox mount -o remount,rw /;

DATA=`/sbin/blkid /dev/block/mmcblk0p29 | grep "f2fs"`
CACHE=`/sbin/blkid /dev/block/mmcblk0p18 | grep "f2fs"`
SYSTEM=`/sbin/blkid /dev/block/mmcblk0p16 | grep "f2fs"`

if [ "${CACHE}" != "" ]; then
	/sbin/busybox sed -i 's,#CACHE_ISF2FS,,' /fstab.build;
else
	/sbin/busybox sed -i 's,#CACHE_ISEXT4,,' /fstab.build;
fi;
if [ "${DATA}" != "" ]; then
	/sbin/busybox sed -i 's,#DATA_ISF2FS,,' /fstab.build;
else
	/sbin/busybox sed -i 's,#DATA_ISEXT4,,' /fstab.build;
fi;
if [ "${SYSTEM}" != "" ]; then
	/sbin/busybox sed -i 's,#SYS_ISF2FS,,' /fstab.build;
else
	/sbin/busybox sed -i 's,#SYS_ISEXT4,,' /fstab.build;
fi;

/sbin/busybox mv /fstab.qcom /fstab.orig;
/sbin/busybox mv /fstab.build /fstab.qcom;

