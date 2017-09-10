echo
echo "Setup"
echo
branch=$(git symbolic-ref --short HEAD)
branch_name=$(git rev-parse --abbrev-ref HEAD)
last_commit=$(git rev-parse --verify --short=8 HEAD)
export LOCALVERSION="-Armonia-Kernel-${branch_name}/${last_commit}"
mkdir -p out
export ARCH=arm64
export SUBARCH=arm64
make O=out clean
make O=out mrproper

echo
echo "Issue Build Commands"
echo
export CROSS_COMPILE="$HOME"/Android-dev/toolchains/aosp-gcc/aarch64-linux-android-4.9/bin/aarch64-linux-android-

echo
echo "Set DEFCONFIG"
echo 
make O=out ARCH=arm64 gts4lwifi_eur_open_defconfig


echo
echo "let's build"
echo 
make O=out -j$(nproc --all) CONFIG_DEBUG_SECTION_MISMATCH=y


echo
echo "copying new files"
echo
cp ./out/arch/arm64/boot/Image.gz-dtb ./AnyKernel3/
cd ./AnyKernel3/
zip -r9 Armonia-kernel-SM-T830"$version"-"$branch"-"$last_commit".zip * -x .git README.md *placeholder

cd ..
echo THE END
