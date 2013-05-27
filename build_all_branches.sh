echo "Switch to AOSP 4.2.2"
git checkout aosp

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att_aosp.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr_aosp.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo_aosp.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw_aosp.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl_aosp.sh

echo "Switch to Touchjizz 4.2.2"
git checkout tw

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att_tw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr_tw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo_tw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw_tw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl_tw.sh

echo "Switch to AOSP 4.2.2"
git checkout aosp

