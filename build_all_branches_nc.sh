echo "Switch to AOSP 4.2.2"
git checkout aosp

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
./build_spr.sh
./build_tmo.sh
./build_vzw.sh
./build_intl.sh

echo "Switch to AOSP 4.3"
git checkout aosp4.3

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
./build_spr.sh
./build_tmo.sh
./build_vzw.sh
./build_intl.sh

echo "Switch to Touchjizz 4.2.2"
git checkout tw

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
./build_spr.sh
./build_tmo.sh
./build_vzw.sh
./build_intl.sh

echo "Switch to Touchjizz 4.3"
git checkout tw4.3

echo "Make Clean"
make clean
echo "Make Mrproper"
make mrproper
./build_att.sh
./build_spr.sh
./build_tmo.sh
./build_vzw.sh
./build_intl.sh

echo "Switch to AOSP 4.2.2"
git checkout aosp

