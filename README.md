# HAGIWO MOD1 Dub Siren

Arduino Nano / ATmega328P sketch for a HAGIWO MOD1 based dub siren.

## Features

- D11 / F4 tone output
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

## Build

The sketch targets Arduino Nano / ATmega328P.

PlatformIO check example:

```powershell
pio ci hagiwo-mod1-dub-siren.ino --board nanoatmega328
```

Arduino IDE can also compile the `.ino` directly for Arduino Nano / ATmega328P.

## Notes

D9 shares the F2 signal path with A4, and D10 shares the F3 signal path with A5 on the MOD1 hardware. This sketch uses D9 and D10 only as high-impedance digital inputs.

The `old/` directory contains historical snapshots kept for reference.