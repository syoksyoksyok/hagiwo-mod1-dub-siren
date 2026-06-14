# hagiwo-mod1-dub-siren.ino 仕様解析

対象ファイル: `hagiwo-mod1-dub-siren.ino`  
解析日: 2026-06-14

## 1. 概要

このスケッチは、Arduino の `tone()` 関数を使ってダブサイレン風の発振音を出すプログラムです。

3つのポテンショメータで基本ピッチ、LFO振幅、LFO速度を操作し、ボタンでLFO波形を切り替えます。LFOによって出力周波数を周期的に変調し、サイレン風のピッチ変化を作ります。

主な特徴は次の通りです。

- 出力は `tone()` による矩形波。
- 出力ピンは D11。
- LFO波形は4種類。
- 初期波形はサイン波。
- SPEED POT が左端付近のときは無音。
- 無音判定にはヒステリシスを使用。
- POT値には2サンプル移動平均による平滑化あり。
- 周波数は 130Hz から 3000Hz に制限。
- 通常動作時は 10ms ごとに周波数を更新。

## 2. ハードウェア仕様

| 用途 | Arduino ピン | コード上の定義 |
|---|---:|---|
| 音声出力 | D11 | `PinConfig::SPEAKER` |
| 基本ピッチ POT | A0 | `PinConfig::PITCH_POT` |
| LFO振幅 POT | A1 | `PinConfig::AMP_POT` |
| LFO速度 POT | A2 | `PinConfig::SPEED_POT` |
| 波形切替ボタン | D4 | `PinConfig::BUTTON` |
| LFO状態LED | D3 | `PinConfig::LED` |

ボタン入力は `INPUT_PULLUP` です。そのため、ボタンが押されていないときは HIGH、押されたときは LOW として扱われます。

## 3. 操作仕様

### 3.1 PITCH POT

`A0` の値を読み取り、基本周波数 `baseFreq` に変換します。

- 入力範囲: 0-1023
- 出力範囲: 130Hz-2100Hz

変換式は Arduino の `map()` を使っています。

```cpp
map(pitchValue, 0, 1023, 130, 2100)
```

### 3.2 AMP POT

`A1` の値を読み取り、LFOによる周波数変調幅 `amplitude` に変換します。

- 入力範囲: 0-1023
- 出力範囲: 0-600

単位は直接Hzとして扱われ、最終的に `baseFreq + lfoValue` の形で周波数に加算されます。

### 3.3 SPEED POT

`A2` の値を読み取り、LFOの進行量 `step` に変換します。

- 入力範囲: 0-1023
- 出力範囲: 0.01-0.8

この `step` は更新ごとに `state.angle` へ加算されます。更新周期は 10ms なので、値が大きいほどLFO周期が速くなります。

また、SPEED POT は無音制御にも使われます。

- `speedValue <= 10`: 無音へ移行
- `speedValue > 20`: 発音へ復帰

10 と 20 の間に幅を持たせることで、境界付近で発音と無音が細かく切り替わるチャタリングを防ぎます。

### 3.4 ボタン

ボタンを押すたびに波形が次の順番で切り替わります。

1. `SINE`
2. `SQUARE`
3. `SAWTOOTH`
4. `REVERSE_SAWTOOTH`
5. `SINE` に戻る

デバウンス時間は 200ms です。

## 4. 周波数生成仕様

通常動作時は次の流れで周波数を決めます。

1. POTから `baseFreq`, `amplitude`, `step`, `speedValue` を読む。
2. `step` を `state.angle` に加算する。
3. `state.angle` が `TWO_PI` 以上なら `TWO_PI` を引いて循環させる。
4. 現在の波形に応じて `lfoValue` を計算する。
5. `baseFreq + lfoValue` を `modulatedFreq` とする。
6. `modulatedFreq` を 130Hz-3000Hz に制限する。
7. `tone(D11, modulatedFreq)` で出力する。
8. `lfoValue > 0` のとき LED を点灯する。

## 5. LFO波形仕様

### 5.1 SINE

```cpp
sin(state.angle) * amplitude
```

サイン波で、`-amplitude` から `+amplitude` の範囲を滑らかに変化します。

### 5.2 SQUARE

コード上は「Smoothed square wave」とコメントされていますが、実装は完全な矩形波ではなく、区間ごとに固定値とサインカーブを組み合わせています。

- `0 <= angle < PI/2`: `+amplitude`
- `PI/2 <= angle < PI`: `amplitude * sin(...)`
- `PI <= angle < 1.5PI`: `-amplitude`
- `1.5PI <= angle < 2PI`: `-amplitude * sin(...)`

注意点として、区間境界で値が急に変わる箇所があります。完全に滑らかな波形ではありません。

### 5.3 SAWTOOTH

```cpp
((state.angle / TWO_PI) * 2 - 1) * amplitude
```

`-amplitude` から `+amplitude` へ直線的に上がる波形です。

### 5.4 REVERSE_SAWTOOTH

```cpp
(1.0 - (state.angle / TWO_PI)) * 2 * amplitude - amplitude
```

`+amplitude` から `-amplitude` へ直線的に下がる波形です。

## 6. 状態管理仕様

状態は `State state` に集約されています。

| メンバー | 役割 |
|---|---|
| `waveformType` | 現在のLFO波形 |
| `angle` | LFOの現在位相 |
| `lastUpdateTime` | 通常動作更新の前回時刻 |
| `lastButtonPress` | ボタン押下の前回時刻 |
| `isSilent` | 現在無音状態かどうか |
| `sweepActive` | 発音/無音移行中かどうか |
| `sweepStartTime` | 移行開始時刻 |
| `sweepFromAmp` | 移行開始時の振幅 |
| `sweepToAmp` | 移行終了時の振幅 |
| `isEnteringSound` | 発音へ入る移行かどうか |

## 7. 無音・復帰仕様

起動直後は `state.isSilent = true` なので無音状態から始まります。

発音へ移行する条件:

```cpp
state.isSilent == true
speedValue > 20
sweepActive == false
```

無音へ移行する条件:

```cpp
state.isSilent == false
speedValue <= 10
sweepActive == false
```

移行時間は `SWEEP_DURATION = 20ms` です。ただし、`tone()` 自体には音量制御機能がないため、このスケッチの sweep は実際の音量を連続的に変化させていません。

現在の実装で sweep 中に行っていることは次の通りです。

- `baseFreq` で `tone()` を出す。
- `currentAmp > 0` のとき LED を点灯する。
- 無音移行完了時に `noTone()` を呼ぶ。
- 無音移行完了時にスピーカーピンを LOW にする。

## 8. 全行解析

### 1-34行目: ファイルヘッダコメント

このスケッチの目的、特徴、過去の修正履歴、ノイズ低減変更の説明です。

- 1行目: 複数行コメント開始。
- 2行目: プログラム名とバージョン説明。
- 4-10行目: 主要機能の説明。
- 12-17行目: リファクタリング内容の説明。
- 19-22行目: 2025-09-16 の修正履歴。
- 24-29行目: 2025-09-16 の仕様変更履歴。
- 31-33行目: 2025-10-01 のノイズ低減変更。
- 34行目: 複数行コメント終了。

コメント上は「Volume sweep」とありますが、実装上は `tone()` の音量を直接変えていないため、実際の音量フェードではありません。

### 36行目: 数学関数の読み込み

```cpp
#include <math.h>
```

`sin()` を使うために `math.h` を読み込んでいます。LFOのサイン波計算と、SQUARE波形内の補間に使われます。

### 38-46行目: ピン定義

`PinConfig` 構造体にピン番号をまとめています。

- 39行目: `PinConfig` 定義開始。
- 40行目: 音声出力は D11。
- 41行目: 基本ピッチPOTは A0。
- 42行目: LFO振幅POTは A1。
- 43行目: LFO速度POTは A2。
- 44行目: 波形切替ボタンは D4。
- 45行目: LEDは D3。
- 46行目: `PinConfig` 定義終了。

### 48-62行目: 音声・制御パラメータ定義

`AudioConfig` 構造体に、音声生成と操作に関する定数をまとめています。

- 49行目: 無音へ入るしきい値。SPEED POT が 10 以下で無音移行。
- 50行目: 無音から出るしきい値。SPEED POT が 20 より大きいと発音移行。
- 51行目: 通常動作の更新周期。10ms。
- 52行目: 最低出力周波数。130Hz。
- 53行目: 最大出力周波数。3000Hz。
- 54行目: 基本周波数の最小値。130Hz。
- 55行目: 基本周波数の最大値。2100Hz。
- 56行目: LFO位相ステップ最小値。0.01。
- 57行目: LFO位相ステップ最大値。0.8。
- 58行目: LFO振幅最小値。0。
- 59行目: LFO振幅最大値。600。
- 60行目: ボタンのデバウンス時間。200ms。
- 61行目: 発音/無音移行時間。20ms。
- 62行目: `AudioConfig` 定義終了。

### 64-70行目: 波形列挙型

`Waveform` enum でLFO波形を定義しています。

- 66行目: `SINE = 0`
- 67行目: `SQUARE = 1`
- 68行目: `SAWTOOTH = 2`
- 69行目: `REVERSE_SAWTOOTH = 3`

ボタン押下時は `(state.waveformType + 1) % 4` で循環します。

### 72-84行目: グローバル状態

`State` 構造体と、そのインスタンス `state` を定義しています。

- 74行目: 初期波形は `SINE`。
- 75行目: LFO位相は 0.0 から開始。
- 76行目: 通常更新の前回時刻。
- 77行目: ボタン押下の前回時刻。
- 78行目: 起動直後は無音状態。
- 79行目: 起動直後は移行処理なし。
- 80行目: sweep開始時刻。
- 81行目: sweep開始振幅。
- 82行目: sweep終了振幅。
- 83行目: 発音へ入る移行かどうか。
- 84行目: `state` インスタンス生成。

### 86-92行目: POT読み取り結果構造体

`PotValues` は `readPotValues()` の戻り値です。

- 88行目: 基本周波数。
- 89行目: LFO振幅。
- 90行目: LFO位相ステップ。
- 91行目: 平滑化後のSPEED POT値。

### 94-115行目: POT読み取りと平滑化

`readPotValues()` は3つのPOTを読み、平滑化して制御値へ変換します。

- 95行目: 関数開始。
- 97-99行目: 前回値を `static` 変数として保持。
- 101行目: PITCH POT を読み、前回値と平均化。
- 102行目: PITCH の前回値を更新。
- 104行目: AMP POT を読み、前回値と平均化。
- 105行目: AMP の前回値を更新。
- 107行目: SPEED POT を読み、前回値と平均化。
- 108行目: SPEED の前回値を更新。
- 110行目: PITCH値を 130-2100Hz へ変換。
- 111行目: AMP値を 0-600 へ変換。
- 112行目: SPEED値を 0.01-0.8 へ変換。
- 114行目: 変換結果を `PotValues` として返す。
- 115行目: 関数終了。

注意点として、初回の `lastPitchValue`, `lastAmpValue`, `lastSpeedValue` は 0 です。そのため、起動直後の1回目の読み取りは実値の約半分になります。

### 117-122行目: setup()

起動時のピン初期化です。

- 118行目: スピーカーピン D11 を出力に設定。
- 119行目: ボタン D4 をプルアップ入力に設定。
- 120行目: LED D3 を出力に設定。
- 121行目: スピーカーピンを LOW にして安定化。

### 124-141行目: loop()

メインループです。

- 125行目: ボタンがLOW、かつ前回押下から200msを超えていれば押下として扱う。
- 126行目: ボタン押下時刻を更新。
- 127行目: 波形を次へ進める。
- 130行目: POT値を読み取り、構造化束縛で変数に展開。
- 131行目: SPEED POT に基づいて無音/発音状態を更新。
- 133-136行目: sweep中なら `handleVolumeSweep()` を実行してこのループを終了。
- 138-140行目: 無音でなければ通常動作を更新。

注意点として、130行目の構造化束縛は C++17 機能です。Arduino環境によってはコンパイルできない可能性があります。

### 143-162行目: 無音状態更新

`updateSilenceState()` は SPEED POT 値から無音/発音移行を開始する関数です。

- 143行目: 関数定義。`baseFreq` は渡されていますが、この関数内では使われていません。
- 144行目: 現在が無音状態か判定。
- 145行目: 無音状態で、SPEED値が20を超え、sweep中でなければ発音移行開始。
- 146行目: 無音状態を解除。
- 147行目: sweepを有効化。
- 148行目: 発音へ入る移行であることを記録。
- 149行目: sweep開始時刻を記録。
- 150行目: sweep開始振幅を 0 に設定。
- 151行目: sweep終了振幅を現在のAMP値に設定。
- 153行目: 現在が無音でない場合の処理へ。
- 154行目: SPEED値が10以下で、sweep中でなければ無音移行開始。
- 155行目: sweepを有効化。
- 156行目: 無音へ入る移行であることを記録。
- 157行目: sweep開始時刻を記録。
- 158行目: sweep開始振幅を現在のAMP値に設定。
- 159行目: sweep終了振幅を 0 に設定。

### 164-184行目: sweep処理

`handleVolumeSweep()` は発音開始または無音化の移行処理です。

- 165行目: sweep開始からの経過時間を計算。
- 166行目: 経過時間が20ms以上なら移行完了。
- 167行目: sweepを無効化。
- 168行目: 無音へ入る移行だったか判定。
- 169行目: 無音移行なら `noTone()` で発音停止。
- 170行目: スピーカーピンを LOW にする。
- 171行目: LEDを消灯。
- 172行目: 無音状態に設定。
- 174行目: 移行完了後、関数終了。
- 177行目: 0.0-1.0 の進行率を計算。
- 178行目: sweep中の現在振幅を線形補間で計算。
- 179行目: 出力周波数を基本周波数に設定。
- 180行目: 最低周波数 130Hz に制限。
- 181行目: 最大周波数 3000Hz に制限。
- 182行目: `tone()` で発音。
- 183行目: `currentAmp > 0` ならLED点灯。

実装上、178行目の `currentAmp` は音量には使われません。`tone()` は周波数指定のみで、振幅指定がないためです。

### 186-201行目: 通常動作

`updateNormalOperation()` はLFOによる周波数変調と出力を行います。

- 187行目: 前回更新から10ms未満なら何もしない。
- 188行目: 更新時刻を保存。
- 190行目: LFO位相 `angle` に `step` を加算。
- 191行目: `angle` が `TWO_PI` 以上なら一周分戻す。
- 193行目: 現在波形に応じたLFO値を計算。
- 195行目: 基本周波数にLFO値を足して出力周波数を作る。
- 196行目: 最低周波数 130Hz に制限。
- 197行目: 最大周波数 3000Hz に制限。
- 199行目: `tone()` で出力。
- 200行目: LFO値が正ならLED点灯、0以下なら消灯。

### 203-231行目: LFO値計算

`calculateLfoValue()` は現在の波形に応じて `-amplitude` から `+amplitude` 付近の値を返します。

- 204行目: 関数開始。
- 205行目: 現在の波形で分岐。
- 206-207行目: SINE波形。`sin(angle) * amplitude`。
- 208-222行目: SQUARE波形。固定値とサイン補間を組み合わせた変化。
- 223-225行目: SAWTOOTH波形。`-amplitude` から `+amplitude` へ上昇。
- 226-227行目: REVERSE_SAWTOOTH波形。`+amplitude` から `-amplitude` へ下降。
- 228-229行目: 想定外の波形値なら 0.0 を返す。
- 230-231行目: switch と関数の終了。

## 9. コンパイル互換性の注意点

### 9.1 C++17 構造化束縛

次の行は C++17 の構文です。

```cpp
auto [baseFreq, amplitude, step, speedValue] = readPotValues();
```

Arduino IDE / ボードパッケージの設定によっては C++17 が有効でなく、コンパイルエラーになる可能性があります。

互換性を優先する場合は、次のように書く方が安全です。

```cpp
PotValues pots = readPotValues();
float baseFreq = pots.baseFreq;
float amplitude = pots.amplitude;
float step = pots.step;
int speedValue = pots.speedValue;
```

### 9.2 static const float

`AudioConfig` 内の `STEP_MIN` と `STEP_MAX` は `static const float` です。

```cpp
static const float STEP_MIN = 0.01;
static const float STEP_MAX = 0.8;
```

Arduino AVR系のコンパイル設定では、クラス内 `static const float` の扱いで問題が出る可能性があります。環境によっては `constexpr` またはグローバル定数にした方が安全です。

### 9.3 map() の戻り値

Arduino の `map()` は整数演算を行い、戻り値は `long` です。

このコードでは `STEP_MIN * 1000` と `STEP_MAX * 1000` を使って 10-800 に変換してから 1000.0 で割ることで、小数のステップ値を作っています。

## 10. 実装上の注意点・改善候補

### 10.1 sweep は音量フェードではない

コメントでは音量スイープと説明されていますが、実際には音量制御されていません。`tone()` には音量指定がないためです。

本当にポップノイズ低減のためのフェードを行うなら、PWM出力、外部VCA、DAC、またはタイマ割り込みによる波形生成など別方式が必要です。

### 10.2 SQUARE波形は境界で不連続

SQUARE波形は「Smoothed」とコメントされていますが、区間境界で急変する箇所があります。

例えば `angle` が `PI/2` に到達した直後、戻り値が `+amplitude` から `0` へ急変します。急な周波数変化を避けたい場合は、波形設計を見直す余地があります。

### 10.3 updateSilenceState() の未使用引数

`updateSilenceState(int speedValue, float baseFreq, float amplitude)` の `baseFreq` は未使用です。動作には影響しませんが、警告や可読性の観点では削除できます。

### 10.4 初回POT読み取りが半分になる

平滑化用の前回値が 0 で初期化されているため、初回読み取りは実値の約半分になります。起動直後の反応を正確にしたい場合は、初回だけ実読み取り値をそのまま使う処理が考えられます。

## 11. 現在のGitHubとの差分確認

確認時点で、ローカルの `main` と GitHub の `main` は同じコミットでした。

- ローカル `HEAD`: `fa6189c Add files via upload`
- ローカル `origin/main`: `fa6189c`
- GitHub `refs/heads/main`: `fa6189c5c5c0e7a388ff29c82d101451970104d8`
- `hagiwo-mod1-dub-siren.ino` に差分なし

この仕様書ファイルは新規作成物であり、元の `.ino` は変更していません。
