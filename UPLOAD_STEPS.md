# Upload Guide

This guide explains how to upload `hagiwo-mod1-dub-siren.ino` to an Arduino Nano / ATmega328P.

## Arduino IDE

1. Install Arduino IDE.
2. Download or clone this repository.
3. Open `hagiwo-mod1-dub-siren.ino` in Arduino IDE. If Arduino IDE asks to place the sketch in a folder, allow it. The sketch should be inside a folder named `hagiwo-mod1-dub-siren`.
4. Select `Tools` > `Board` > `Arduino AVR Boards` > `Arduino Nano`.
5. Select `Tools` > `Processor` > `ATmega328P`.
6. Connect the Arduino Nano by USB.
7. Select the serial port from `Tools` > `Port`.
8. Click `Upload`.

If upload fails, select `Tools` > `Processor` > `ATmega328P (Old Bootloader)` and upload again. Many older Nano-compatible boards need the old bootloader setting.

## Troubleshooting

- If no serial port appears, try another USB cable. Some USB cables are charge-only.
- Many Nano-compatible boards use a CH340/CH341 USB-serial chip. If the port does not appear, install the CH340/CH341 driver for your OS.
- If upload fails with `stk500_recv()` or a similar error, try `ATmega328P (Old Bootloader)`.
- Close Serial Monitor and any other apps using the same serial port before uploading.

## PlatformIO Build Check

PlatformIO is not required for normal users. It is useful when checking that the sketch compiles.

```powershell
pio ci hagiwo-mod1-dub-siren.ino --board nanoatmega328
```

## Notes

- The target board is Arduino Nano / ATmega328P.
- This repository does not require additional Arduino libraries.
- The output on D11 / F4 is a 5V square wave. Keep the connected mixer or amplifier volume low when testing for the first time.
