mkdir $(pwd)/out
make -C $(pwd) O=$(pwd)/out VARIANT_DEFCONFIG=jf_tmo_defconfig jf_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -C $(pwd) O=$(pwd)/out
cp $(pwd)/out/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage
