#!/bin/bash

# renaming the trim build to uber build
echo "renaming the build name from TRIM to UBER";
sed 's/JB-SGII-PWR-CORE-TRIM/JB-SGII-PWR-CORE-UBER/g' arch/arm/configs/dorimanx_defconfig > dorimanx_defconfig;
#sed 's/JB-SGII-TRIM/JB-SGII-UBER/g' build_kernel.sh > build_kernel-updated.sh;

\cp dorimanx_defconfig arch/arm/configs/dorimanx_defconfig;
#\cp build_kernel-updated.sh build_kernel.sh;

echo "patching the kernel to block the TRIM Functionality";
patch -Rp1 --no-backup-if-mismatch < trim.patch;

cd ../initramfs3;
echo "patching the initramfs to block the TRIM Functionality";
patch -Rp1 --no-backup-if-mismatch < trim.patch;
cd ../Dorimanx-SG2-I9100-Kernel;


# 1.) load the ".config"                                                      #
echo "Loading config file";
bash ./load_config.sh;                                                      #
                                                                             #
# 2.) clean the sources                                                       #
echo "Cleaning kernel now";
bash ./clean-kernel.sh;                                                           #

# 3.) now you can build my kernel                                             #
bash ./build_kernel.sh                                                           #

# renaming the trim build to uber build
echo "renaming the build name from UBER to TRIM";
sed 's/JB-SGII-PWR-CORE-UBER/JB-SGII-PWR-CORE-TRIM/g' arch/arm/configs/dorimanx_defconfig > dorimanx_defconfig;
\cp dorimanx_defconfig arch/arm/configs/dorimanx_defconfig;
rm dorimanx_defconfig;

#sed 's/JB-SGII-UBER/JB-SGII-TRIM/g' build_kernel.sh > build_kernel-updated.sh;
#\cp build_kernel-updated.sh build_kernel.sh;
#rm build_kernel-updated.sh;

echo "Upatching the kernel to block the TRIM Functionality";
patch -p1 --no-backup-if-mismatch < trim.patch;
cd ../initramfs3;
echo "Unpatching the initramfs to block the TRIM Functionality";
patch -p1 --no-backup-if-mismatch < trim.patch;
cd ../Dorimanx-SG2-I9100-Kernel;
#                                                                             #
# Have fun and update me if something nice can be added to my source.         #
###############################################################################

