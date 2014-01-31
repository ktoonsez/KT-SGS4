echo "Switch to AOSP 4.4"
git checkout aosp4.4

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl.sh

echo "Switch to AOSP 4.3"
git checkout aosp4.3

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl.sh

echo "Switch to Touchjizz 4.2.2"
git checkout tw

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl.sh

echo "Switch to Touchjizz 4.3"
git checkout tw4.3

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl.sh

echo "Switch to Touchjizz GE 4.3"
git checkout twge4.3

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_spr.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_tmo.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl.sh

echo "Switch to Touchjizz GE 4.4"
git checkout twge4.4

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intl.sh
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_vzw.sh

echo "Switch to Touchjizz 4.2-jactive"
git checkout tw-active
echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_intlja.sh

echo "Switch to AOSP 4.4"
git checkout aosp4.4

