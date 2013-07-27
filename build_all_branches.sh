echo "Switch to AOSP 4.2.2"
git checkout aosp

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

echo "Switch to AOSP 4.2.2"
git checkout aosp

