# HAGIWO MOD1 Dub Siren

HAGIWO MOD1 ベースのダブサイレン用 Arduino Nano / ATmega328P スケッチです。

これは HAGIWO MOD1 ハードウェア向けの非公式ファームウェアです。HAGIWO MOD1 は HAGIWO さんによって設計されています。

現在のバージョン: `v1.0.0`

## 必要なもの

- HAGIWO MOD1 ハードウェア
- Arduino Nano / ATmega328P
- 通常利用には Arduino IDE、ビルド確認には PlatformIO

## クイックスタート

1. このリポジトリをダウンロード、または clone します。
2. Arduino IDE で `hagiwo-mod1-dub-siren.ino` を開きます。
3. `Arduino Nano` と `ATmega328P` を選択します。
4. シリアルポートを選択します。
5. `Upload` をクリックします。
6. 書き込みに失敗する場合は、`ATmega328P (Old Bootloader)` を試してください。

詳しい書き込み手順は `UPLOAD_STEPS_ja.md` を参照してください。

モジュールの操作方法と初回利用手順は `MANUAL_ja.md` を参照してください。

## 機能

- D11 / F4 の矩形波オーディオ出力
- A0 ピッチ制御
- A1 LFO 深さ制御
- A2 LFO 速度制御
- A3 / D17 ゲート入力: HIGH の間だけ発音
- D9 キル入力: HIGH の間ミュート
- D10 LFO ポーズ入力: HIGH の間 LFO の動きを停止
- D4 ボタン:
  - 短押しで LFO 波形を変更
  - 230 ms、つまり 0.23 秒以上の長押しで、押している間だけ手動発音
- D3 LED は LFO の動きを表示し、LFO 深さに応じたソフトウェア PWM 輝度で点灯

## コントロール

| 機能 | Arduino ピン | MOD1 ジャック / 操作部 | POT |
|---|---:|---:|---:|
| オーディオ出力 | D11 | F4 | - |
| ピッチ POT | A0 | - | POT1 |
| LFO 深さ POT | A1 | - | POT2 |
| LFO 速度 POT | A2 | - | POT3 |
| ゲート入力 | A3 / D17 | F1 | - |
| キル入力 | D9 | F2 | - |
| LFO ポーズ入力 | D10 | F3 | - |
| 波形 / 手動ゲートボタン | D4 | Button | - |
| LFO LED | D3 | LED | - |

## リポジトリ内容

- `README_ja.md`: 入口となるプロジェクト概要。
- `UPLOAD_STEPS_ja.md`: Arduino IDE と PlatformIO 向けの Arduino Nano 書き込み手順。
- `MANUAL_ja.md`: モジュール操作用の初回ユーザーマニュアル。
- `hagiwo-mod1-dub-siren_spec_ja.md`: 開発や改造向けの技術仕様。
- `hagiwo-mod1-dub-siren.ino`: Arduino スケッチ。
- `LICENSE`: MIT License 本文。
- `.gitignore`: ローカルのビルド出力やエディタファイルの除外ルール。
- `.gitattributes`: テキストファイルの改行ルール。

## 変更履歴

### v1.0.0 - 2026-07-05

- 初回公開リリース。
- HAGIWO MOD1 / Arduino Nano 向けの Timer1 ベース矩形波ダブサイレンファームウェア。
- ゲート入力、キル入力、LFO ポーズ入力、手動ゲートボタン、波形切替、LFO LED ソフトウェア PWM を含みます。

## クレジット

HAGIWO MOD1 は HAGIWO さんによって設計されています。HAGIWO さんの MOD シリーズハードウェアとソース例は、ライセンスフリー / CC0 ポリシーで公開されています。このリポジトリは、そのハードウェア向けの非公式ファームウェアです。

## ライセンス

このファームウェアとドキュメントは MIT License で公開されています。詳細は `LICENSE` を参照してください。

## 注意

MOD1 ハードウェア上では、D9 は A4 と F2 信号経路を共有し、D10 は A5 と F3 信号経路を共有します。このスケッチでは D9 と D10 を高インピーダンスのデジタル入力としてのみ使用します。
