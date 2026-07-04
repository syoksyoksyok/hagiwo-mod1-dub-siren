/*
 * SPDX-License-Identifier: MIT
 *
 * HAGIWO MOD1 Dub Siren
 *
 * Arduino Nano / ATmega328P sketch that turns a HAGIWO MOD1 into a
 * tone()-based dub siren.
 *
 * Copyright (c) 2026 syoksyoksyok
 *
 * Features:
 * - D11 / F4 tone output.
 * - A0 pitch, A1 LFO depth, A2 LFO speed.
 * - A3 / D17 gate input enables sound while HIGH.
 * - D9 kill input mutes audio while HIGH.
 * - D10 pauses the LFO while HIGH.
 * - D4 button tap changes LFO waveform.
 * - D4 button hold for 0.23 seconds or longer enables sound while held.
 * - D3 LED shows LFO speed and depth using software PWM.
 */

#include <math.h>

// Hardware and behavior configuration
struct PinConfig {
  static const int SPEAKER = 11;
  static const int PITCH_POT = A0;
  static const int AMP_POT = A1;
  static const int SPEED_POT = A2;
  static const int GATE_INPUT = A3;
  static const int KILL_INPUT = 9;
  static const int LFO_PAUSE_INPUT = 10;
  static const int BUTTON = 4;
  static const int LED = 3;
};

struct AudioConfig {
  static const long UPDATE_INTERVAL = 10;
  static const int MIN_FREQ = 130;
  static const int MAX_FREQ = 3000;
  static const int BASE_FREQ_MIN = 130;
  static const int BASE_FREQ_MAX = 2100;
  static constexpr float STEP_MIN = 0.01;
  static constexpr float STEP_MAX = 0.8;
  static const int AMP_MIN = 0;
  static const int AMP_MAX = 600;
  static const unsigned long DEBOUNCE_DELAY = 30;
  static const unsigned long LONG_PRESS_DURATION = 230;
  static const unsigned int LED_PWM_PERIOD_US = 4096;
};

enum Waveform {
  SINE = 0,
  SQUARE = 1,
  SAWTOOTH = 2,
  REVERSE_SAWTOOTH = 3
};

struct State {
  Waveform waveformType = SINE;
  float angle = 0.0;
  unsigned long lastUpdateTime = 0;
  unsigned long buttonPressStart = 0;
  bool buttonWasPressed = false;
  bool buttonLongPressActive = false;
  bool lastAudioActive = false;
  uint8_t ledBrightness = 0;
} state;

// Smoothed control values used by the audio and LED update
struct PotValues {
  float baseFreq;
  float amplitude;
  float step;
};

// Read controls once per loop and smooth the ADC values
PotValues readPotValues() {
  // 2-sample moving average for basic ADC noise reduction
  static int lastPitchValue = 0;
  static int lastAmpValue = 0;
  static int lastSpeedValue = 0;

  int pitchValue = (analogRead(PinConfig::PITCH_POT) + lastPitchValue) / 2;
  lastPitchValue = pitchValue;

  int ampValue = (analogRead(PinConfig::AMP_POT) + lastAmpValue) / 2;
  lastAmpValue = ampValue;

  int speedValue = (analogRead(PinConfig::SPEED_POT) + lastSpeedValue) / 2;
  lastSpeedValue = speedValue;

  float baseFreq = map(pitchValue, 0, 1023, AudioConfig::BASE_FREQ_MIN, AudioConfig::BASE_FREQ_MAX);
  float amplitude = map(ampValue, 0, 1023, AudioConfig::AMP_MIN, AudioConfig::AMP_MAX);
  float step = map(speedValue, 0, 1023, AudioConfig::STEP_MIN * 1000, AudioConfig::STEP_MAX * 1000) / 1000.0;

  return {baseFreq, amplitude, step};
}

void setup() {
  pinMode(PinConfig::SPEAKER, OUTPUT);
  pinMode(PinConfig::BUTTON, INPUT_PULLUP);
  pinMode(PinConfig::GATE_INPUT, INPUT);
  pinMode(PinConfig::KILL_INPUT, INPUT);
  pinMode(PinConfig::LFO_PAUSE_INPUT, INPUT);
  pinMode(PinConfig::LED, OUTPUT);
  digitalWrite(PinConfig::SPEAKER, LOW);
}

void loop() {
  unsigned long now = millis();
  bool buttonPressed = digitalRead(PinConfig::BUTTON) == LOW;

  if (buttonPressed && !state.buttonWasPressed) {
    state.buttonWasPressed = true;
    state.buttonPressStart = now;
    state.buttonLongPressActive = false;
  } else if (buttonPressed && !state.buttonLongPressActive && now - state.buttonPressStart >= AudioConfig::LONG_PRESS_DURATION) {
    state.buttonLongPressActive = true;
  } else if (!buttonPressed && state.buttonWasPressed) {
    unsigned long pressDuration = now - state.buttonPressStart;
    if (!state.buttonLongPressActive && pressDuration >= AudioConfig::DEBOUNCE_DELAY && pressDuration < AudioConfig::LONG_PRESS_DURATION) {
      state.waveformType = static_cast<Waveform>((state.waveformType + 1) % 4);
    }
    state.buttonWasPressed = false;
    state.buttonLongPressActive = false;
  }

  PotValues pots = readPotValues();

  bool gateActive = digitalRead(PinConfig::GATE_INPUT) == HIGH;
  bool killActive = digitalRead(PinConfig::KILL_INPUT) == HIGH;
  bool lfoPaused = digitalRead(PinConfig::LFO_PAUSE_INPUT) == HIGH;
  bool audioRequested = gateActive || state.buttonLongPressActive;
  bool audioActive = audioRequested && !killActive;
  if (!audioActive && state.lastAudioActive) {
    noTone(PinConfig::SPEAKER);
    digitalWrite(PinConfig::SPEAKER, LOW);
  }

  if (audioActive && !state.lastAudioActive) {
    state.angle = 0.0;
    state.lastUpdateTime = 0;
  }
  state.lastAudioActive = audioActive;

  updateNormalOperation(pots.baseFreq, pots.amplitude, pots.step, audioActive, lfoPaused);
  updateLedPwm();
}

void updateNormalOperation(float baseFreq, float amplitude, float step, bool audioEnabled, bool lfoPaused) {
  if (millis() - state.lastUpdateTime < AudioConfig::UPDATE_INTERVAL) return;
  state.lastUpdateTime = millis();

  if (!lfoPaused) {
    state.angle += step;
    if (state.angle >= TWO_PI) state.angle -= TWO_PI;
  }

  float lfoValue = calculateLfoValue(amplitude);
  uint8_t depthBrightness = static_cast<uint8_t>(map(static_cast<int>(amplitude), AudioConfig::AMP_MIN, AudioConfig::AMP_MAX, 0, 255));
  if (amplitude <= 0.0 || depthBrightness == 0) {
    state.ledBrightness = 0;
  } else {
    float ledLevel = (lfoValue + amplitude) / (2.0 * amplitude);
    if (ledLevel < 0.0) ledLevel = 0.0;
    if (ledLevel > 1.0) ledLevel = 1.0;
    state.ledBrightness = static_cast<uint8_t>(ledLevel * depthBrightness);
  }

  if (!audioEnabled) return;

  int modulatedFreq = static_cast<int>(baseFreq + lfoValue);
  if (modulatedFreq < AudioConfig::MIN_FREQ) modulatedFreq = AudioConfig::MIN_FREQ;
  if (modulatedFreq > AudioConfig::MAX_FREQ) modulatedFreq = AudioConfig::MAX_FREQ;

  tone(PinConfig::SPEAKER, modulatedFreq);
}

// D3 hardware PWM uses Timer2 on ATmega328P, which can conflict with tone().
// Use software PWM so the LFO LED can fade while D11 outputs tone().
void updateLedPwm() {
  static bool ledPinHigh = false;

  if (state.ledBrightness == 0) {
    if (ledPinHigh) digitalWrite(PinConfig::LED, LOW);
    ledPinHigh = false;
    return;
  }

  if (state.ledBrightness >= 255) {
    if (!ledPinHigh) digitalWrite(PinConfig::LED, HIGH);
    ledPinHigh = true;
    return;
  }

  unsigned long phase = micros() % AudioConfig::LED_PWM_PERIOD_US;
  unsigned long onTime = ((unsigned long)AudioConfig::LED_PWM_PERIOD_US * state.ledBrightness) / 255;
  bool ledOn = phase < onTime;
  if (ledOn != ledPinHigh) {
    digitalWrite(PinConfig::LED, ledOn ? HIGH : LOW);
    ledPinHigh = ledOn;
  }
}

// Calculate the current LFO offset
float calculateLfoValue(float amplitude) {
  switch (state.waveformType) {
    case SINE:
      return sin(state.angle) * amplitude;
    case SQUARE:
      // Square-like wave with curved transition sections
      if (state.angle < PI) {
        if (state.angle < PI / 2) {
          return amplitude;
        } else {
          return amplitude * sin((state.angle - PI / 2) * 2);
        }
      } else {
        if (state.angle < PI * 1.5) {
          return -amplitude;
        } else {
          return -amplitude * sin((state.angle - PI * 1.5) * 2);
        }
      }
    case SAWTOOTH:
      // Linear slope from -amplitude to +amplitude
      return ((state.angle / TWO_PI) * 2 - 1) * amplitude;
    case REVERSE_SAWTOOTH:
      return (1.0 - (state.angle / TWO_PI)) * 2 * amplitude - amplitude;
    default:
      return 0.0;
  }
}
