For EdgeTx. https://github.com/EdgeTX/edgetx.github.io/wiki/EdgeTX-Installation-with-the-Bootloader#flashing-the-edgetx-bootloader-and-firmware
1. Start docker desktop
2. Run `run.sh` from git bash
3. Grab firmware from `build-output\arm-none-eabi\firmware.bin`
4. Put `*.bin` into `/Firmware` folder on radio sd card
5. Turn on radio and open SD card explorer app to firmware folder
6. Select appropiate file either `Flash bootloader` or `Flash [Internal|External] Multi`
7. Reboot to booloader mode (hold T1 and T4 to center whilst powering on)
8. Write the new firmware
9. Reboot

For Multi Module https://www.multi-module.org/using-the-module/firmware-updates
1. Grap the latest `*stm-serial-aetr-v*.bin` from https://downloads.multi-module.org/?rmtx16s
2. Put `*.bin` into `/Firmware` folder on radio sd card
3. Turn on radio and open SD card explorer app to firmware folder
4. Select appropiate file and `Flash [Internal|External] Multi`
5. Reboot
