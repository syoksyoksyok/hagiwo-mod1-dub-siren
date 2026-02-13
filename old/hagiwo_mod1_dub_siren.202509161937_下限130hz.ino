/*
 * ダブサイレンマシン (tone()関数バージョン / v4)
 *
 * ★ 最低周波数を130Hzに変更。
 * ピッチ上限は2100Hz。
 * 起動時のLFO波形は矩形波。
 * POT3を左に回し切るとnoTone()で消音する機能。
 */

#include <math.h>   // sin()関数を使用するためにインクルード
#include <stdlib.h> // random()関数を使用するためにインクルード

// --- ピンアサイン (現在の設定を維持) ---
const int speakerPin = 11;      // オーディオ出力ピン
const int pitchPotPin = A0;       // ピッチ(基準周波数)を調整するポテンショメータ
const int ampPotPin = A1;         // LFOの振れ幅を調整するポテンショメータ
const int speedPotPin = A2;       // LFOの速度を調整するポテンショメータ
const int buttonPin = 4;          // LFO波形を選択するボタン
const int ledPin = 3;             // LFOの状態を示すLED

// --- グローバル変数 ---
const int potSilenceThreshold = 10; // POTをオフと判定する閾値
float angle = 0.0;                  // LFOの角度（ラジアン）
float step = 0.0;                   // 角度の増加量
float amplitude = 0.0;              // LFOの振れ幅
int waveformType = 1;               // 起動時のLFO波形を矩形波(1)に設定
float randomValue = 0.0;            // サンプル＆ホールド用のランダム値
unsigned long randomHoldTime = 0;   // ランダム値の保持時間
unsigned long randomLastUpdate = 0;   // ランダム値の最後の更新時間

// tone()更新用タイマー
unsigned long lastUpdateTime = 0;
const long updateInterval = 10;     // 10ミリ秒ごと（1秒間に100回）に更新

void setup() {
  pinMode(speakerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // --- ボタン処理 ---
  if (digitalRead(buttonPin) == LOW) {
    delay(200);
    waveformType = (waveformType + 1) % 5;
  }

  // --- POT3のサイレンス機能チェック ---
  int speedValue = analogRead(speedPotPin);
  if (speedValue <= potSilenceThreshold) {
    noTone(speakerPin);
    digitalWrite(ledPin, LOW);
  } else {
    // --- 10ミリ秒ごとに処理を実行 ---
    if (millis() - lastUpdateTime >= updateInterval) {
      lastUpdateTime = millis();

      // --- ポテンショメータ読み取りとマッピング ---
      int pitchValue = analogRead(pitchPotPin);
      // ★ピッチの下限を130Hzに変更
      float frequency = map(pitchValue, 0, 1023, 130, 2100);

      // speedValueは既に読み取り済み
      step = map(speedValue, 0, 1023, 10, 800) / 1000.0; 

      int ampValue = analogRead(ampPotPin);
      amplitude = map(ampValue, 0, 1023, 0, 600);

      // --- LFO計算 ---
      if (waveformType == 4 && (millis() - randomLastUpdate >= randomHoldTime)) {
        randomLastUpdate = millis();
        randomHoldTime = map(speedValue, 1023, 0, 50, 500);
        randomValue = (random(-600, 601) / 600.0) * amplitude;
      }

      angle += step;
      if (angle >= TWO_PI) {
        angle -= TWO_PI;
      }

      float lfoValue;
      switch (waveformType) {
        case 0: // サイン波
          lfoValue = sin(angle) * amplitude;
          break;
        case 1: // 矩形波
          lfoValue = (angle < PI ? 1 : -1) * amplitude;
          break;
        case 2: // ノコギリ波
          lfoValue = ((angle / TWO_PI) * 2 - 1) * amplitude;
          break;
        case 3: // ノコギリ波（下降）
          lfoValue = (1.0 - (angle / TWO_PI)) * 2 * amplitude - amplitude;
          break;
        case 4: // ランダム波（サンプル＆ホールド）
          lfoValue = randomValue;
          break;
      }

      // --- 発音とLED表示 ---
      int modulatedFrequency = frequency + lfoValue;
      
      // ★安全装置(ハイパスフィルターの代替)：周波数が130Hz未満にならないようにする
      if (modulatedFrequency < 130) {
        modulatedFrequency = 130;
      }

      tone(speakerPin, modulatedFrequency);

      // LFOの正負に合わせてLEDを点滅させる
      if (lfoValue > 0) {
        digitalWrite(ledPin, HIGH);
      } else {
        digitalWrite(ledPin, LOW);
      }
    }
  }
}