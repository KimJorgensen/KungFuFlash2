# Kung Fu Flash 2 Firmware
The initial firmware installation can be done via the SWD interface (J1) using a ST-Link V2 programmer or via the USB port.
The Kung Fu Flash 2 cartridge should not be connected to the Commodore 64 before this process has been completed.

## SWD (Recommended)
Connect the ST-Link V2 programmer to J1 (3.3V, GND, SWDIO, and SWCLK) and connect BOOT0 to 3.3V.

Rename the `KungFuFlash_v2.xx.upd` file to `KungFuFlash_v2.xx.bin` and use the STM32CubeProgrammer from ST to program the firmware.

All wires to J1 should be disconnected once the firmware has been successfully installed.

## USB
Add a jumper to short BOOT0 and +3.3V on J1. This will ensure that the board is started in DFU mode.

Use [dfu-util](http://dfu-util.sourceforge.net/) to install the firmware:

`dfu-util -a 0 -s 0x08000000 -D KungFuFlash_v2.xx.upd`

The jumper should be removed after the firmware has been installed.

## Diagnostic
The diagnostic tool is started by pressing the menu button for 2 seconds (until the LED turns off) and is intended to debug stability problems on some C64 models.
For reference the phi2 clock frequency should be around 985248 Hz for PAL and around 1022727 Hz for NTSC.

The diagnostic tool allows adjusting a phi2 clock offset to address any stability issues.
Some C64 breadbin/longboards may require a small negative phi2 offset and C64C/shortboards may require a small positive offset.

In the diagnostic tool the following buttons on the cartridge can be used:

* `Special`: Decrease offset
* `Menu`: Increase offset
* `Special`, hold for 2 seconds: Save offset and exit
* `Menu`, hold for 2 seconds: Reset offset (without saving)
* `Special` + `Menu`, hold for 2 seconds: Reset and save offset. This combo will also work without starting the diagnostic tool first
* `Reset`: Exit diagnostic tool without saving offset

The phi2 offset is saved to the hidden `.KFF2.cfg` file and is reset if the file is not found on the SD card.
