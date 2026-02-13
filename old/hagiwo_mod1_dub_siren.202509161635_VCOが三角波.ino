/*
 * Arduino Nano ダブサイレンマシン (修正版 v17 - VCOノコギリ波)
 * VCO: ノコギリ波 (200Hz～)
 * LFO: サイン波、矩形波、ノコギリ波、逆コギリ波 (0.1Hz～100Hz)
 * LEDはLFO振幅に連動
 * オーディオ出力: D11
 * 修正点：
 * - ★VCOの波形をノコギリ波に変更
 * - POT1,2,3の指数カーブを緩やかに修正 (1.7)
 * - 最低周波数を200Hzに変更
 * - 最終段のハイパスフィルターを190Hzに調整
 * - POT3が左端（最小値）の時に発音を停止する機能
 * - LFOで上限を超える場合は動的に上限を拡張し、LFO波形を完全に保持
 * - ボタン短押しでLFO波形変更（発音中でも確実に動作）
 * - LED制御の競合問題を解決
 */

#include <math.h>

// Pin assignments
#define POT_FREQ    A0
#define POT_DEPTH   A1
#define POT_RATE    A2
#define RESERVED    A5
#define AUDIO_OUT   11
#define BUTTON      4
#define LED         3

// Audio parameter constants
#define MIN_FREQ    200.0 
#define BASE_MAX_FREQ    2200.0
#define ABSOLUTE_MAX_FREQ 12000.0
#define AUDIO_SAMPLE_RATE 31250.0

// POTサイレンス機能の閾値設定
#define POT_SILENCE_THRESHOLD 10

// ボタン設定
#define DEBOUNCE_DELAY 50

// LFO波形変更フィードバック設定
#define FEEDBACK_DURATION 200  // フィードバック時間（ms）

// LFO waveform types
enum LFOWaveform {
  LFO_SINE = 0,
  LFO_SQUARE,
  LFO_SAW,
  LFO_RAMP,
  LFO_COUNT
};

// Global variables
volatile LFOWaveform currentLFO = LFO_SQUARE;

// ボタン処理変数（短押し対応）
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// LFO波形変更フィードバック用変数
unsigned long lfoChangeTime = 0;
bool lfoFeedbackActive = false;
uint8_t feedbackPhase = 0;  // 点滅制御用

// Audio generation variables
volatile uint32_t vcoPhase = 0;
volatile uint32_t lfoPhase = 0;
volatile float vcoFreq = MIN_FREQ;
volatile float lfoFreqFloat = 1.0;
volatile float lfoDepthFloat = 0.25;
volatile uint8_t currentLFOValue = 128;
volatile float dynamicMaxFreq = BASE_MAX_FREQ;

// POTサイレンス機能の状態変数
volatile bool potSilenceActive = false;

// ハイパスフィルターの状態変数
const float hpf_alpha = 0.9632f; // 190Hz HPF @ 31.25kHz
volatile float hpf_last_in = 0.0f;
volatile float hpf_last_out = 0.0f;

// High-quality lookup tables
const uint8_t sineTable[256] PROGMEM = {
  128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,
  176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,216,
  218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
  245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
  255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
  245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
  218,216,213,211,209,206,204,201,199,196,193,191,188,185,182,179,
  176,174,171,168,165,162,159,156,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,99,96,93,90,87,84,81,
  79,76,73,70,67,64,62,59,56,54,51,49,46,44,42,39,
  37,35,33,31,29,27,25,23,21,20,18,17,15,14,12,11,
  10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,
  0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,
  10,11,12,14,15,17,18,20,21,23,25,27,29,31,33,35,
  37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,
  79,81,84,87,90,93,96,99,103,106,109,112,115,118,121,124
};

// ノコギリ波 生成
uint8_t generateSawtooth(uint32_t phase) {
  // 位相カウンターの上位8ビットをそのまま出力値とする (CPU負荷が最も軽い)
  return (phase >> 24);
}

// LFO waveform generation
uint8_t generateLFO(uint32_t phase, LFOWaveform waveform) {
  uint8_t index = (phase >> 24) & 0xFF;
  switch (waveform) {
    case LFO_SINE:
      return pgm_read_byte(&sineTable[index]);
    case LFO_SQUARE:
      return (index < 128) ? 0 : 255;
    case LFO_SAW:
      return index;
    case LFO_RAMP:
      return 255 - index;
    default:
      return 128;
  }
}

// 動的上限拡張システム
float calculateDynamicMaxFreq(float baseFreq, float depth) {
  float maxModulatedFreq = baseFreq * (1.0 + depth);
  float requiredMax = max(BASE_MAX_FREQ, maxModulatedFreq);
  if (requiredMax > ABSOLUTE_MAX_FREQ) {
    requiredMax = ABSOLUTE_MAX_FREQ;
  }
  return requiredMax;
}

// 拡張された範囲での変調計算
float calculateModulatedFreq(float baseFreq, uint8_t lfoValue, float depth, float maxFreq) {
  float normalizedLFO = ((float)lfoValue - 128.0) / 128.0;
  float modulation = normalizedLFO * depth;
  float modulatedFreq = baseFreq * (1.0 + modulation);
  if (modulatedFreq < MIN_FREQ) {
    modulatedFreq = MIN_FREQ;
  }
  if (modulatedFreq > maxFreq) {
    modulatedFreq = maxFreq;
  }
  return modulatedFreq;
}

// POT3サイレンス機能チェック
bool checkPot3Silence(int ratePot) {
  return (ratePot <= POT_SILENCE_THRESHOLD);
}

// LFO波形変更処理
void changeLFOWaveform() {
  cli();
  currentLFO = (LFOWaveform)((currentLFO + 1) % LFO_COUNT);
  sei();
  
  // フィードバック開始
  lfoChangeTime = millis();
  lfoFeedbackActive = true;
  feedbackPhase = 0;
}

// 短押しボタン処理
void handleButton() {
  int reading = digitalRead(BUTTON);
  
  // デバウンス処理
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;
      
      // ボタンが押された瞬間（HIGH→LOW）
      if (buttonState == LOW) {
        changeLFOWaveform(); // 短押しで即座に波形変更
      }
    }
  }
  
  lastButtonState = reading;
}

// LFO波形変更フィードバック処理（点滅制御）
void handleLFOFeedback() {
  if (!lfoFeedbackActive) return;
  
  unsigned long elapsed = millis() - lfoChangeTime;
  if (elapsed >= FEEDBACK_DURATION) {
    lfoFeedbackActive = false;
    return;
  }
  
  // 50ms間隔で点滅（4回点滅）
  uint8_t blinkCycle = (elapsed / 50) % 2;
  feedbackPhase = (blinkCycle == 0) ? 255 : 0;
}

// Timer1 interrupt for audio generation (31.25kHz)
ISR(TIMER1_COMPA_vect) {
  if (potSilenceActive) {
    OCR2A = 128; // 無音（PWM中間値）
    return;
  }
  
  // VCO波形生成 (現在の位相に基づく)
  uint8_t vcoOut = generateSawtooth(vcoPhase);
  
  // LFO生成
  LFOWaveform safeLFO = currentLFO;
  currentLFOValue = generateLFO(lfoPhase, safeLFO);
  
  // 次のサンプルのための変調周波数を計算
  float modulatedFreq = calculateModulatedFreq(vcoFreq, currentLFOValue, lfoDepthFloat, dynamicMaxFreq);
  uint32_t vcoIncrement = (uint32_t)(modulatedFreq * 137438.95);
  uint32_t lfoIncrement = (uint32_t)(lfoFreqFloat * 137438.95);
  
  // 次のサンプルのために位相を更新
  vcoPhase += vcoIncrement;
  lfoPhase += lfoIncrement;
  
  // --- 最終段: 190Hz ハイパスフィルター ---
  float ac_input = (float)vcoOut - 128.0f; // DCオフセット除去
  hpf_last_out = hpf_alpha * (hpf_last_out + ac_input - hpf_last_in);
  hpf_last_in = ac_input; // 状態を更新
  
  // DCオフセットを戻し、8bit範囲にクランプ
  int16_t final_output = (int16_t)(hpf_last_out + 128.0f);
  if (final_output > 255) final_output = 255;
  if (final_output < 0) final_output = 0;

  // フィルター適用後のオーディオ出力
  OCR2A = (uint8_t)final_output;
}

void setup() {
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  pinMode(AUDIO_OUT, OUTPUT);
  digitalWrite(LED, LOW);

  // Timer setup
  cli();
  
  // Timer2 for PWM audio output
  TCCR2A = (1 << COM2A1) | (1 << WGM20);
  TCCR2B = (1 << CS20);
  OCR2A = 128;

  // Timer1 for audio interrupt (31.25kHz)
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS10);
  OCR1A = 511;
  TIMSK1 = (1 << OCIE1A);
  
  sei();
  
  delay(100);
}

void loop() {
  // ポテンショメータ読み取り
  int freqPot = analogRead(POT_FREQ);
  int depthPot = analogRead(POT_DEPTH);
  int ratePot = analogRead(POT_RATE);
  
  // POT3サイレンス機能チェック
  bool newPotSilenceState = checkPot3Silence(ratePot);

  // オーディオパラメータを安全に更新
  cli();
  potSilenceActive = newPotSilenceState;
  // POTがサイレンス状態でない場合のみパラメータを更新
  if (!potSilenceActive) {
    // POTの値を0.0-1.0に正規化し、指数カーブを適用
    // pow(x, 1.7) を使い、緩やかな凹型のカーブを作成
    float curvedFreq = pow(freqPot / 1023.0, 1.7);
    float curvedDepth = pow(depthPot / 1023.0, 1.7);
    float curvedRate = pow(ratePot / 1023.0, 1.7);
    
    vcoFreq = MIN_FREQ + curvedFreq * (BASE_MAX_FREQ - MIN_FREQ);
    lfoDepthFloat = curvedDepth * 3.0;
    lfoFreqFloat = 0.1 + curvedRate * 99.9;
    
    dynamicMaxFreq = calculateDynamicMaxFreq(vcoFreq, lfoDepthFloat);
  }
  sei();
  
  // 短押し対応ボタン処理
  handleButton();
  
  // LFO波形変更フィードバック処理
  handleLFOFeedback();
  
  // LED更新（フィードバック優先制御）
  if (lfoFeedbackActive) {
    // フィードバック中はフィードバック用LEDを優先
    analogWrite(LED, feedbackPhase);
  }
  else if (!potSilenceActive) {
    // 通常時はLFO値でLED制御
    analogWrite(LED, currentLFOValue);
  }
  else {
    // 無音時はLED消灯
    analogWrite(LED, 0);
  }
}