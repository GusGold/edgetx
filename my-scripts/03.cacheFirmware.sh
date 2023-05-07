#! /bin/sh

cd ../build-output/arm-none-eabi

EDGETX_VERSION=$(sed -nr "s/^#define VERSION[ ]+\"([0-9\.]+)\"$/\1/p" radio/src/stamp.h)
GIT_COMMIT=$(sed -nr "s/^#define GIT_STR[ ]+\"([0-9a-f]+)\"$/\1/p" radio/src/stamp.h)

cp firmware.bin "custom $EDGETX_VERSION $GIT_COMMIT.bin"
cp firmware.bin "../../my-firmware/custom $EDGETX_VERSION $GIT_COMMIT.bin"