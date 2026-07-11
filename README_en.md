# HAGIWO MOD1 Dub Siren
An Arduino Nano / ATmega328P sketch for a HAGIWO MOD1-based dub siren.
This firmware is designed for HAGIWO MOD1 hardware.
The HAGIWO MOD1 was designed by HAGIWO.

Current version: `v1.0.0`

## Requirements
- HAGIWO MOD1 hardware
- Arduino Nano / ATmega328P
- Arduino IDE for normal use, and PlatformIO for build verification

## Upload
1. Download or clone this repository.
2. Open `hagiwo-mod1-dub-siren.ino` in the Arduino IDE.
3. Select `Arduino Nano` and `ATmega328P`.
4. Select the serial port.
5. Click `Upload`.
6. If the upload fails, try `ATmega328P (Old Bootloader)`.

For detailed upload instructions and troubleshooting, see `UPLOAD_STEPS_en.md`.

## Features
This sketch turns the HAGIWO MOD1 into a dub siren machine.

While the onboard button is held down, or while a HIGH signal is applied to the F1 jack (A3 / D17), a square-wave siren sound is output from the F4 jack (D11).
POT1 controls the VCO base pitch, POT2 controls the LFO depth, and POT3 (A2) controls the LFO speed.
The LFO modulates the pitch, and the LED indicates the LFO movement with brightness variation based on the LFO depth.

## Control / Function Reference
| Panel label | Arduino pin | Function |
|---|---:|---|
| POT1 | A0 | VCO pitch |
| POT2 | A1 | LFO depth |
| POT3 | A2 | LFO speed |
| F1 jack | A3 / D17 | Gate input (sound output) |
| F2 jack | D9 | Kill input |
| F3 jack | D10 | LFO pause input |
| F4 jack | D11 | Audio output |
| Button | D4 | Short press: waveform selection / Long press: manual sound output |
| LED | D3 | LFO indicator |

Connect the F4 jack (D11) to a mixer or downstream module.
The output is not line-level audio; it is a 5V square wave from the Arduino.
When connecting it to external audio equipment, use a mixer input or attenuator and begin with the volume set low.
Do not connect headphones or passive speakers directly.

External signals connected to F1, F2, and F3 must remain within the 0V to 5V range and must not contain negative voltages.
Directly connecting signals that contain negative voltages, such as some Eurorack signals, or signals that exceed 5V may damage the circuit.
Always condition external signals to the appropriate range and use a common GND when connecting external equipment.

## Basic Operation
- POT1 (A0): Adjusts the base pitch.
- POT2 (A1): Adjusts the depth of LFO pitch modulation. It also affects the depth of the LED brightness variation.
- POT3 (A2): Adjusts the LFO speed. Even near the fully counterclockwise position, the LFO does not stop and continues moving very slowly.
- F1 jack (A3 / D17): Produces sound only while the input is HIGH.
- F2 jack (D9): Forces the output to mute while the input is HIGH. This takes priority over the gate and button.
- F3 jack (D10): Pauses the LFO movement while the input is HIGH.
- Button (D4): A short press of less than 0.23 seconds changes the LFO waveform. Holding it for 0.23 seconds or longer manually produces sound for as long as the button remains pressed.
- LED (D3): Indicates the LFO speed and depth.

The LFO waveforms cycle in the following order: SINE, SQUARE, SAWTOOTH, and REVERSE SAWTOOTH.


## Troubleshooting
- Make sure the output is being taken from the F4 jack (D11).
- If the gate at the F1 jack (A3 / D17) is LOW, hold the button for at least 0.23 seconds and check whether manual sound output works.
- If the F2 jack (D9) is HIGH, the output is forcibly muted.
- Check that the input level on the mixer or amplifier is not turned down.
- Check that 5V and GND are correctly supplied to both the MOD1 and the Arduino Nano.
- For upload problems, see `UPLOAD_STEPS_en.md`.

## Documentation
- `UPLOAD_STEPS_en.md`: Arduino Nano upload instructions for the Arduino IDE and PlatformIO.
- `hagiwo-mod1-dub-siren_spec_en.md`: Technical specifications for development and modification.
- `hagiwo-mod1-dub-siren.ino`: Arduino sketch.
- `LICENSE`: MIT License text.

## Changelog
### v1.0.0 - 2026-07-05
- Initial public release.
- Timer1-based square-wave dub siren firmware for the HAGIWO MOD1 / Arduino Nano.
- Includes gate input, kill input, LFO pause input, manual gate button, waveform switching, and software-PWM LFO LED indication.

## Credits
The HAGIWO MOD1 was designed by HAGIWO.
HAGIWO's MOD-series hardware and source examples are made available under HAGIWO's stated license-free / CC0 policy.
This repository contains unofficial firmware for that hardware.

## License
This firmware and its documentation are released under the MIT License. See `LICENSE` for details.

## Notes
On the MOD1 hardware, the F2 jack (D9) shares a signal path with A4, and the F3 jack (D10) shares a signal path with A5.
This sketch uses D9 and D10 only as high-impedance digital inputs.
