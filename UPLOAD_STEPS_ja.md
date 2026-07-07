# 書き込みガイド

このガイドでは、`hagiwo-mod1-dub-siren.ino` を Arduino Nano / ATmega328P に書き込む方法を説明します。

## Arduino IDE

1. Arduino IDE をインストールします。
2. このリポジトリをダウンロード、または clone します。
3. Arduino IDE で `hagiwo-mod1-dub-siren.ino` を開きます。Arduino IDE からスケッチをフォルダに入れるよう求められた場合は、許可してください。スケッチは `hagiwo-mod1-dub-siren` という名前のフォルダ内に配置されます。
4. `Tools` > `Board` > `Arduino AVR Boards` > `Arduino Nano` を選択します。
5. `Tools` > `Processor` > `ATmega328P` を選択します。
6. Arduino Nano を USB で接続します。
7. `Tools` > `Port` からシリアルポートを選択します。
8. `Upload` をクリックします。

書き込みに失敗する場合は、`Tools` > `Processor` > `ATmega328P (Old Bootloader)` を選択して、もう一度書き込んでください。古い Nano 互換ボードの多くは old bootloader 設定が必要です。

## トラブルシューティング

- シリアルポートが表示されない場合は、別の USB ケーブルを試してください。充電専用の USB ケーブルもあります。
- Nano 互換ボードの多くは CH340 / CH341 USB シリアルチップを使用しています。ポートが表示されない場合は、使用している OS 向けの CH340 / CH341 ドライバをインストールしてください。
- `stk500_recv()` または類似のエラーで書き込みに失敗する場合は、`ATmega328P (Old Bootloader)` を試してください。
- 書き込み前に、Serial Monitor や同じシリアルポートを使用している他のアプリを閉じてください。

## PlatformIO ビルド確認

通常利用では PlatformIO は不要です。スケッチがコンパイルできるか確認するときに便利です。

```powershell
pio ci hagiwo-mod1-dub-siren.ino --board nanoatmega328
```

## 注意

- 対象ボードは Arduino Nano / ATmega328P です。
- このリポジトリは追加の Arduino ライブラリを必要としません。
- D11 / F4 の出力は 5V 矩形波です。初めてテストするときは、接続したミキサーやアンプの音量を小さくしてください。
