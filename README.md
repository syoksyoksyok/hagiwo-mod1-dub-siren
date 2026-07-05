# HAGIWO MOD1 Dub Siren

Arduino Nano / ATmega328P sketch for a HAGIWO MOD1 based dub siren.

This is an unofficial firmware for the HAGIWO MOD1 hardware. HAGIWO MOD1 is designed by HAGIWO / ハギヲさん.

Current version: `v1.0.0`

## Requirements

- HAGIWO MOD1 hardware
- Arduino Nano / ATmega328P
- Arduino IDE for normal users, or PlatformIO for build checks

## Quick start

1. Download or clone this repository.
2. Open `hagiwo-mod1-dub-siren.ino` in Arduino IDE.
3. Select `Arduino Nano` and `ATmega328P`.
4. Select the serial port.
5. Click `Upload`.
6. If upload fails, try `ATmega328P (Old Bootloader)`.

For detailed upload steps, see `UPLOAD_STEPS.md`.

For module operation and first-time use, see `MANUAL.md`.

## Features

- D11 / F4 square-wave audio output
- A0 pitch control
- A1 LFO depth control
- A2 LFO speed control
- A3 / D17 gate input: sound is enabled while HIGH
- D9 kill input: mutes audio while HIGH
- D10 LFO pause input: pauses LFO movement while HIGH
- D4 button:
  - short press changes LFO waveform
  - hold for 0.23 seconds or longer to manually enable sound while held
- D3 LED shows LFO movement with software PWM brightness based on LFO depth

## Controls

| Function | Pin |
|---|---:|
| Audio output | D11 |
| Pitch pot | A0 |
| LFO depth pot | A1 |
| LFO speed pot | A2 |
| Gate input | A3 / D17 |
| Kill input | D9 |
| LFO pause input | D10 |
| Waveform / manual gate button | D4 |
| LFO LED | D3 |

## Repository contents

- `README.md`: entry point and project overview.
- `UPLOAD_STEPS.md`: Arduino Nano upload steps for Arduino IDE and PlatformIO.
- `MANUAL.md`: first-time user manual for module operation.
- `hagiwo-mod1-dub-siren_spec.md`: technical specification for development and modification.
- `hagiwo-mod1-dub-siren.ino`: Arduino sketch.
- `LICENSE`: MIT License text.
- `.gitignore`: local build output and editor file ignore rules.
- `.gitattributes`: text file line-ending rules.

## Changelog

### v1.0.0 - 2026-07-05

- Initial public release.
- Timer1-based square-wave dub siren firmware for HAGIWO MOD1 / Arduino Nano.
- Includes gate input, kill input, LFO pause input, manual gate button, waveform switching, and LFO LED software PWM.

## Credits

HAGIWO MOD1 is designed by HAGIWO / ハギヲさん. HAGIWO's MOD series hardware and source examples are published with a license-free / CC0 policy. This repository contains an unofficial firmware for that hardware.

## License

This firmware and documentation are released under the MIT License. See `LICENSE` for details.

## Notes

D9 shares the F2 signal path with A4, and D10 shares the F3 signal path with A5 on the MOD1 hardware. This sketch uses D9 and D10 only as high-impedance digital inputs.
