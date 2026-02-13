/*
 * ダブサイレンマシン (tone()関数バージョン)
 *
 * 現在のピンアサインを維持しつつ、参照プログラムを基にtone()関数で発音するように再構築。
 * LFO（低周波オシレーター）で基準周波数を変調してサイレン音を生成します。
 * ボタンでLFO波形を5種類（サイン波、矩形波、ノコギリ波、逆ノコギリ波、ランダム）から選択できます。
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
float ledState = 0.0;             // LEDの状態用変数
float randomValue = 0.0;          // サンプル＆ホールド用のランダム値
unsigned long randomHoldTime = 0; // ランダム値の保持時間
unsigned long randomLastUpdate = 0; // ランダム値の最後の更新時間

void setup() {
  pinMode(speakerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); // ボタンのピンをプルアップ入力モードに設定
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // --- ボタン処理 ---
  // ボタンが押されたら波形タイプを切り替える
  if (digitalRead(buttonPin) == LOW) {
    delay(200); // チャタリング（短時間のON/OFF）防止
    waveformType = (waveformType + 1) % 5; // 波形タイプを0から4の範囲で循環させる
  }

  // --- ポテンショメータ読み取りとマッピング ---
  // POT1: ピッチ (基準周波数)
  int pitchValue = analogRead(pitchPotPin);
  float frequency = map(pitchValue, 0, 1023, 50, 2000); // 50Hzから2000Hzの範囲に変換

  // POT3: LFOスピード
  int speedValue = analogRead(speedPotPin);
  step = map(speedValue, 0, 1023, 1, 50) / 1000.0; // LFOの1ステップあたりの角度変化量を設定

  // POT2: LFO振れ幅
  int ampValue = analogRead(ampPotPin);
  amplitude = map(ampValue, 0, 1023, 0, 500); // 周波数の変化幅を0から500に設定

  // --- LFO計算 ---
  // ランダム波（サンプル＆ホールド）用の値更新
  if (waveformType == 4 && (millis() - randomLastUpdate >= randomHoldTime)) {
    randomLastUpdate = millis();
    randomHoldTime = map(speedValue, 1023, 0, 50, 500); // LFOスピードが速いほど、ランダム値の更新も速くする
    randomValue = (random(-500, 501) / 500.0) * amplitude; // -1から1の範囲の乱数を生成し、振れ幅を適用
  }

  // LFOの角度を更新
  angle += step;
  if (angle >= TWO_PI) { // 角度が2π（360度）を超えたら0に戻す
    angle -= TWO_PI;
  }

  // 選択された波形タイプに基づいてLFOの値を計算
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
  // LFOの値で基準周波数を変調
  int modulatedFrequency = frequency + lfoValue;
  
  // 安全装置：周波数が負の値にならないようにする
  if (modulatedFrequency < 1) {
    modulatedFrequency = 1;
  }

  // tone()関数で発音
  tone(speakerPin, modulatedFrequency);

  // LFOの動きに合わせてLEDの明るさを変更
  ledState = (lfoValue + amplitude) / (2 * amplitude); // LFOの値を0から1の範囲に正規化
  analogWrite(ledPin, int(ledState * 255));
}