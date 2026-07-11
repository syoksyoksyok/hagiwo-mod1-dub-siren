# Upload Guide
This guide explains how to upload `hagiwo-mod1-dub-siren.ino` to an Arduino Nano / ATmega328P.

## Arduino IDE
1. Install the Arduino IDE.
2. Download or clone this repository.
3. Open `hagiwo-mod1-dub-siren.ino` in the Arduino IDE.
   If the Arduino IDE asks to place the sketch in a folder, allow it.
   The sketch will be placed in a folder named `hagiwo-mod1-dub-siren`.
4. Select `Tools` > `Board` > `Arduino AVR Boards` > `Arduino Nano`.
5. Select `Tools` > `Processor` > `ATmega328P`.
6. Connect the Arduino Nano via USB.
7. Select the serial port from `Tools` > `Port`.
8. Click `Upload`.

If the upload fails, select `Tools` > `Processor` > `ATmega328P (Old Bootloader)` and try uploading again.
Many older Nano-compatible boards require the old bootloader setting.

## Troubleshooting
- If the serial port does not appear, try a different USB cable. Some USB cables are for charging only.
- Many Nano-compatible boards use a CH340 / CH341 USB-to-serial chip.
  If the port does not appear, install the CH340 / CH341 driver for your operating system.
- If the upload fails with `stk500_recv()` or a similar error, try `ATmega328P (Old Bootloader)`.
- Before uploading, close the Serial Monitor and any other application using the same serial port.
- If uploading still fails, disconnect and reconnect the USB cable, try another USB cable, or change the timing of pressing the Arduino Nano reset button, then try again.
- If the problem remains unresolved, copy the Arduino IDE error message and ask an AI assistant or another support resource about it.

## PlatformIO Build Check
PlatformIO is not required for normal use. It is useful for checking whether the sketch compiles.

```powershell
pio ci hagiwo-mod1-dub-siren.ino --board nanoatmega328
```

## Notes
- The target board is the Arduino Nano / ATmega328P.
- This repository does not require any additional Arduino libraries.
- The D11 / F4 output is a 5V square wave. When testing it for the first time, start with the connected mixer or amplifier volume set low.
