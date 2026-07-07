# User Manual

This is a quick user manual for first-time users of a HAGIWO MOD1 running this sketch.

## What It Does

This sketch outputs a square-wave siren sound from D11 / F4 while a gate signal is HIGH or while the built-in button is held. A0 controls the base pitch, A1 controls the LFO depth, and A2 controls the LFO speed. The LFO modulates the pitch, and the D3 LED shows the same LFO movement with brightness based on the LFO depth.

## Connections

| MOD1 signal | Arduino pin | Role |
|---|---:|---|
| F4 | D11 | Audio output |
| POT1 | A0 | Pitch |
| POT2 | A1 | LFO depth |
| POT3 | A2 | LFO speed |
| F1 | A3 / D17 | Gate input |
| F2 | D9 | Kill input |
| F3 | D10 | LFO pause input |
| Button | D4 | Waveform select / manual gate |
| LED | D3 | LFO indicator |

Connect F4 / D11 to a mixer or the next module in the signal chain. The output is not a line-level audio signal; it is a 5V square wave from the Arduino. When connecting it to external audio equipment, use a mixer input or attenuator and start with the volume low. Do not connect it directly to headphones or passive speakers.

External signals connected to F1, F2, or F3 must stay within the 0V to 5V range and must be positive voltage only. Signals that include negative voltage, such as some Eurorack signals, or signals above 5V can damage the circuit if connected directly. Always scale external signals to the proper range, and make sure GND is shared when connecting external equipment.

## Basic Operation

- POT1 / A0: Changes the base pitch.
- POT2 / A1: Changes how deeply the LFO modulates the pitch. This also affects the depth of the LED brightness movement.
- POT3 / A2: Changes the LFO speed. The LFO does not stop at the far-left position; it keeps moving very slowly.
- F1 / A3 / D17: Enables sound while HIGH.
- F2 / D9: Force-mutes the output while HIGH. This has priority over the gate and button.
- F3 / D10: Pauses LFO movement while HIGH.
- Button / D4: A short press under 0.23 seconds changes the LFO waveform. Holding it for 0.23 seconds or longer manually enables sound while held.
- LED / D3: Shows the LFO speed and depth.

The LFO waveform changes in this order: SINE, SQUARE, SAWTOOTH, REVERSE SAWTOOTH.

## Initial Test

1. Set POT1, POT2, and POT3 near the center position.
2. Connect F4 / D11 to a mixer or amplifier, and start with the volume low.
3. Hold the button for 0.23 seconds or longer and confirm that sound is produced only while the button is held.
4. Turn POT1 and confirm that the pitch changes.
5. Turn POT2 and POT3 and confirm that the pitch movement and LED movement change.
6. Press the button briefly and confirm that the LFO waveform changes.
7. Send a 5V gate to F1 / A3 and confirm that sound is produced only while the gate is HIGH.
8. Send HIGH to F2 / D9 to confirm mute behavior, and send HIGH to F3 / D10 to confirm LFO pause behavior.

## Troubleshooting

- Confirm that the output is taken from F4 / D11.
- If the F1 / A3 gate is LOW, hold the button for 0.23 seconds or longer and check whether manual sound output works.
- If F2 / D9 is HIGH, the output is force-muted.
- Confirm that the mixer or amplifier input volume is not turned down.
- Confirm that the MOD1 and Arduino Nano are receiving proper 5V and GND.
