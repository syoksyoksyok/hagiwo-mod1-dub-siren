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

## Build / Upload

The sketch targets Arduino Nano / ATmega328P.

### Arduino IDE upload

1. Install Arduino IDE.
2. Open `hagiwo-mod1-dub-siren.ino`.
3. Select `Tools` > `Board` > `Arduino AVR Boards` > `Arduino Nano`.
4. Select `Tools` > `Processor` > `ATmega328P`.
5. Select the serial port from `Tools` > `Port`.
6. Click `Upload`.

If upload fails, select `Tools` > `Processor` > `ATmega328P (Old Bootloader)` and upload again. Many older Nano-compatible boards need the old bootloader setting.

### PlatformIO check

```powershell
pio ci hagiwo-mod1-dub-siren.ino --board nanoatmega328
```

## First-time user manual

このスケッチを書き込んだ HAGIWO MOD1 を初めて使う人向けの簡易マニュアルです。

### できること

ゲート信号、または本体ボタンの長押しで D11 / F4 から矩形波のサイレン音を出します。A0 で基本ピッチ、A1 で LFO の深さ、A2 で LFO の速さを調整します。LFO はピッチを揺らし、D3 の LED は同じ LFO の速さと深さを明るさで表示します。

### 接続

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

F4 / D11 をミキサーや後段モジュールへ接続します。出力は Arduino の 5V 矩形波です。ライン入力や外部機材へ入れる場合は、必要に応じて音量を下げてください。F1、F2、F3 に外部信号を入れる場合は 0V から 5V の範囲にしてください。ユーロラックなどの高い電圧を直接入れないでください。

### 基本操作

- POT1 / A0: 音の基本ピッチを変えます。
- POT2 / A1: LFO がピッチを揺らす深さを変えます。LED の明るさ変化の深さにも反映されます。
- POT3 / A2: LFO の速さを変えます。左端付近でも LFO は止まらず、非常に遅い動きになります。
- F1 / A3 / D17: HIGH の間だけ発音します。
- F2 / D9: HIGH の間は強制ミュートします。ゲートやボタンより優先されます。
- F3 / D10: HIGH の間は LFO の進行を一時停止します。
- Button / D4: 0.23 秒未満の短押しで LFO 波形を切り替えます。0.23 秒以上押し続けると、押している間だけ手動で発音します。
- LED / D3: LFO の速さと深さを表示します。

LFO 波形は、SINE、SQUARE、SAWTOOTH、REVERSE SAWTOOTH の順に切り替わります。

### 最初の動作確認

1. POT1、POT2、POT3 を中央付近にします。
2. F4 / D11 をミキサーやアンプへ接続し、音量を小さめにします。
3. ボタンを 0.23 秒以上押し続けて、押している間だけ音が出ることを確認します。
4. POT1 を回してピッチが変わることを確認します。
5. POT2 と POT3 を回して、音の揺れと LED の動きが変わることを確認します。
6. ボタンを短く押して、LFO 波形が切り替わることを確認します。
7. F1 / A3 に 5V ゲートを入れて、ゲートが HIGH の間だけ音が出ることを確認します。
8. F2 / D9 に HIGH を入れてミュート、F3 / D10 に HIGH を入れて LFO 一時停止を確認します。

### 音が出ないとき

- F4 / D11 から出力を取っているか確認してください。
- F1 / A3 のゲートが LOW の場合、ボタンを 0.23 秒以上押して手動発音できるか確認してください。
- F2 / D9 が HIGH になっていると強制ミュートされます。
- ミキサーやアンプの入力音量が下がっていないか確認してください。
- MOD1 と Arduino Nano に正しく 5V と GND が供給されているか確認してください。

## Repository contents

- `hagiwo-mod1-dub-siren.ino`: Arduino sketch.
- `hagiwo-mod1-dub-siren_spec.md`: detailed behavior and hardware mapping notes.
- `LICENSE`: MIT License text.
- `CHANGELOG.md`: release history.

## Credits

HAGIWO MOD1 is designed by HAGIWO / ハギヲさん. HAGIWO's MOD series hardware and source examples are published with a license-free / CC0 policy. This repository contains an unofficial firmware for that hardware.

## License

This firmware and documentation are released under the MIT License. See `LICENSE` for details.

## Notes

D9 shares the F2 signal path with A4, and D10 shares the F3 signal path with A5 on the MOD1 hardware. This sketch uses D9 and D10 only as high-impedance digital inputs.
