/*
 * ダブサイレンマシン (tone()関数バージョン / 安定化・LED点滅版)
 *
 * ★ tone()とanalogWrite()のタイマー競合を解決し、低周波でのノイズを完全に解消。
 * ★ LFOの振幅に合わせ、LEDが点滅するように変更。
 * LFOで基準周波数を変調し、ボタンで5種類の波形を選択可能。
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
float angle = 0.0;                // LFOの角度（ラジアン）
float step = 0.0;                 // 角度の増加量
float amplitude = 0.0;            // LFOの振れ幅
int waveformType = 0;             // LFO波形タイプ (0:サイン, 1:矩形, 2:ノコギリ, 3:逆ノコギリ, 4:ランダム)
float randomValue = 0.0;          // サンプル＆ホールド用のランダム値
unsigned long randomHoldTime = 0; // ランダム値の保持時間
unsigned long randomLastUpdate = 0; // ランダム値の最後の更新時間

// tone()更新用タイマー
unsigned long lastUpdateTime = 0;
const long updateInterval = 10;   // 10ミリ秒ごと（1秒間に100回）に更新

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

  // --- 10ミリ秒ごとに処理を実行 ---
  if (millis() - lastUpdateTime >= updateInterval) {
    lastUpdateTime = millis();

    // --- ポテンショメータ読み取りとマッピング ---
    int pitchValue = analogRead(pitchPotPin);
    float frequency = map(pitchValue, 0, 1023, 80, 2000);

    int speedValue = analogRead(speedPotPin);
    step = map(speedValue, 0, 1023, 10, 500) / 1000.0; 

    int ampValue = analogRead(ampPotPin);
    amplitude = map(ampValue, 0, 1023, 0, 500);

    // --- LFO計算 ---
    if (waveformType == 4 && (millis() - randomLastUpdate >= randomHoldTime)) {
      randomLastUpdate = millis();
      randomHoldTime = map(speedValue, 1023, 0, 50, 500);
      randomValue = (random(-500, 501) / 500.0) * amplitude;
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
    
    if (modulatedFrequency < 80) {
      modulatedFrequency = 80;
    }

    tone(speakerPin, modulatedFrequency);

    // ★ LFOの正負に合わせてLEDを点滅させる (タイマー競合の回避)
    if (lfoValue > 0) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
  }
}