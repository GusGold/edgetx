#! /bin/sh

rm -r /src/build-output # Clear directory
mkdir /src/build-output
cd /src/build-output

# Custom options defined here: radio\src\CMakeLists.txt
# For what was included in the firmware, review build-output\arm-none-eabi\CMakeCache.txt
# -DHELI=ON - Heli on
# -DDEFAULT_MODE=2 - Mode 2, 
# -DPCB=X10 -DPCBREV=TX16S - PCB based on x10, tx16s specifically (https://github.com/EdgeTX/edgetx/blob/bc8087cbf17cd214d128cf169142800522078fec/tools/build-radiomaster.py#L17-L20)
# -DCMAKE_BUILD_TYPE=Release - Non debug
cmake -DHELI=ON -DDEFAULT_MODE=2 -DPCB=X10 -DPCBREV=TX16S -DCMAKE_BUILD_TYPE=Release ../

make configure
make -C arm-none-eabi -j$(nproc) firmware
