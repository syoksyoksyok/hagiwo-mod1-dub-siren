/*
 * SPDX-License-Identifier: MIT
 *
 * HAGIWO MOD1 Dub Siren
 * Version: v1.0.0
 *
 * Arduino Nano / ATmega328P sketch that turns a HAGIWO MOD1 into a
 * square-wave dub siren.
 *
 * Copyright (c) 2026 syoksyoksyok
 *
 * Features:
 * - D11 / F4 square-wave audio output.
 * - A0 pitch, A1 LFO depth, A2 LFO speed.
 * - A3 / D17 gate input enables sound while HIGH.
 * - D9 kill input mutes audio while HIGH.
 * - D10 pauses the LFO while HIGH.
 * - D4 button tap changes LFO waveform.
 * - D4 button hold for 230ms or longer enables sound while held.
 * - D3 LED shows LFO speed and depth using software PWM.
 */

#include <avr/interrupt.h>
#include <math.h>

#define USE_TIMER1_AUDIO_ENGINE 1

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
  static const unsigned long AUDIO_EDGE_STABLE_MS = 3;
  static const unsigned int STOP_FADE_COMPARE = 63;
  static const uint8_t STOP_FADE_TICKS = 128;
  static const uint8_t STOP_FADE_MAX_DUTY = 128;
  static const uint8_t START_FADE_TICKS = 128;
  static const uint8_t START_FADE_MAX_DUTY = 128;
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
  bool audioActiveStable = false;
  bool audioActiveCandidate = false;
  unsigned long audioActiveCandidateSince = 0;
  bool lastAudioRequested = false;
  bool lastAudioActive = false;
  bool lastKillActive = false;
  uint8_t ledBrightness = 0;
} state;

// Smoothed control values used by the audio and LED update
struct PotValues {
  float baseFreq;
  float amplitude;
  float step;
};

#if USE_TIMER1_AUDIO_ENGINE
volatile bool audioEngineActive = false;
volatile bool audioStopPending = false;
volatile bool audioStartPending = false;
volatile bool audioStartFadeRequested = false;
volatile bool speakerOutputHigh = false;
volatile uint8_t audioStopFadeTicks = 0;
volatile uint8_t audioStopFadeAccumulator = 0;
volatile uint8_t audioStartFadeTicks = 0;
volatile uint8_t audioStartFadeAccumulator = 0;
volatile uint16_t audioCurrentCompare = 0;
volatile uint16_t audioPendingCompare = 0;

// Assumes the standard Arduino Nano 16MHz clock. Timer1 uses prescaler 8.
uint16_t frequencyToTimerCompare(int frequency) {
  if (frequency < AudioConfig::MIN_FREQ) frequency = AudioConfig::MIN_FREQ;
  if (frequency > AudioConfig::MAX_FREQ) frequency = AudioConfig::MAX_FREQ;
  return static_cast<uint16_t>((F_CPU / (2UL * 8UL * static_cast<unsigned long>(frequency))) - 1);
}

ISR(TIMER1_COMPA_vect) {
  if (!audioEngineActive) return;

  if (audioStopPending) {
    if (audioStopFadeTicks == 0) {
      audioEngineActive = false;
      audioStopPending = false;
      speakerOutputHigh = false;
      TIMSK1 &= ~_BV(OCIE1A);
      PORTB &= ~_BV(PB3);
      return;
    }

    uint8_t duty = (static_cast<uint16_t>(audioStopFadeTicks) * AudioConfig::STOP_FADE_MAX_DUTY) / AudioConfig::STOP_FADE_TICKS;
    uint16_t accumulator = static_cast<uint16_t>(audioStopFadeAccumulator) + duty;
    audioStopFadeAccumulator = static_cast<uint8_t>(accumulator);
    speakerOutputHigh = accumulator >= 256;
    if (speakerOutputHigh) {
      PORTB |= _BV(PB3);
    } else {
      PORTB &= ~_BV(PB3);
    }
    audioStopFadeTicks--;
    return;
  }

  if (audioStartPending) {
    if (audioStartFadeTicks >= AudioConfig::START_FADE_TICKS) {
      audioStartPending = false;
      speakerOutputHigh = false;
      PORTB &= ~_BV(PB3);
      OCR1A = audioCurrentCompare;
      TCNT1 = 0;
      return;
    }

    uint8_t duty = ((static_cast<uint16_t>(audioStartFadeTicks) + 1) * AudioConfig::START_FADE_MAX_DUTY) / AudioConfig::START_FADE_TICKS;
    uint16_t accumulator = static_cast<uint16_t>(audioStartFadeAccumulator) + duty;
    audioStartFadeAccumulator = static_cast<uint8_t>(accumulator);
    speakerOutputHigh = accumulator >= 256;
    if (speakerOutputHigh) {
      PORTB |= _BV(PB3);
    } else {
      PORTB &= ~_BV(PB3);
    }
    audioStartFadeTicks++;
    return;
  }

  PINB = _BV(PB3);
  speakerOutputHigh = !speakerOutputHigh;

  if (audioPendingCompare != audioCurrentCompare) {
    audioCurrentCompare = audioPendingCompare;
    OCR1A = audioCurrentCompare;
  }
}

void beginAudioEngine() {
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = frequencyToTimerCompare(AudioConfig::MIN_FREQ);
  TCCR1B = _BV(WGM12) | _BV(CS11);
  TIMSK1 = 0;
  interrupts();
}

void setAudioFrequency(int frequency) {
  uint16_t compare = frequencyToTimerCompare(frequency);

  noInterrupts();
  bool wasStopPending = audioStopPending;
  bool startWithFade = audioStartFadeRequested && !wasStopPending;
  audioStartFadeRequested = false;
  audioPendingCompare = compare;
  audioStopPending = false;
  audioStopFadeTicks = 0;
  audioStopFadeAccumulator = 0;
  if (!audioEngineActive || wasStopPending) {
    PORTB &= ~_BV(PB3);
    speakerOutputHigh = false;
    audioCurrentCompare = compare;
    OCR1A = startWithFade ? AudioConfig::STOP_FADE_COMPARE : compare;
    TCNT1 = 0;
    audioStartPending = startWithFade;
    audioStartFadeTicks = 0;
    audioStartFadeAccumulator = 0;
    audioEngineActive = true;
    TIMSK1 |= _BV(OCIE1A);
  }
  interrupts();
}

void stopAudioOutput() {
  noInterrupts();
  if (audioEngineActive) {
    if (!audioStopPending) {
      audioStopPending = true;
      audioStopFadeTicks = AudioConfig::STOP_FADE_TICKS;
      audioStopFadeAccumulator = 0;
      OCR1A = AudioConfig::STOP_FADE_COMPARE;
      TCNT1 = 0;
    }
    interrupts();
    return;
  }
  audioEngineActive = false;
  audioStopPending = false;
  audioStopFadeTicks = 0;
  audioStopFadeAccumulator = 0;
  audioStartPending = false;
  audioStartFadeRequested = false;
  audioStartFadeTicks = 0;
  audioStartFadeAccumulator = 0;
  speakerOutputHigh = false;
  TIMSK1 &= ~_BV(OCIE1A);
  interrupts();
  digitalWrite(PinConfig::SPEAKER, LOW);
}

void stopAudioOutputImmediate() {
  noInterrupts();
  audioEngineActive = false;
  audioStopPending = false;
  audioStopFadeTicks = 0;
  audioStopFadeAccumulator = 0;
  audioStartPending = false;
  audioStartFadeRequested = false;
  audioStartFadeTicks = 0;
  audioStartFadeAccumulator = 0;
  speakerOutputHigh = false;
  TIMSK1 &= ~_BV(OCIE1A);
  interrupts();
  digitalWrite(PinConfig::SPEAKER, LOW);
}
#else
void beginAudioEngine() {
}

void setAudioFrequency(int frequency) {
  tone(PinConfig::SPEAKER, frequency);
}

void stopAudioOutput() {
  noTone(PinConfig::SPEAKER);
  digitalWrite(PinConfig::SPEAKER, LOW);
}

void stopAudioOutputImmediate() {
  stopAudioOutput();
}
#endif


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
  beginAudioEngine();
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
  bool rawAudioRequested = gateActive || state.buttonLongPressActive;
  if (rawAudioRequested == state.audioActiveStable) {
    state.audioActiveCandidate = rawAudioRequested;
    state.audioActiveCandidateSince = now;
  } else if (rawAudioRequested != state.audioActiveCandidate) {
    state.audioActiveCandidate = rawAudioRequested;
    state.audioActiveCandidateSince = now;
  } else if (now - state.audioActiveCandidateSince >= AudioConfig::AUDIO_EDGE_STABLE_MS) {
    state.audioActiveStable = rawAudioRequested;
  }
  bool audioRequested = state.audioActiveStable;
  bool audioActive = audioRequested && !killActive;
  bool killReleased = state.lastKillActive && !killActive;
  if (!audioActive && state.lastAudioActive) {
    if (killActive) {
      stopAudioOutputImmediate();
    } else {
      stopAudioOutput();
    }
  }

  if (audioRequested && !state.lastAudioRequested) {
    if (!killActive) {
      audioStartFadeRequested = true;
      float lfoValue = calculateLfoValue(pots.amplitude);
      int modulatedFreq = static_cast<int>(pots.baseFreq + lfoValue);
      if (modulatedFreq < AudioConfig::MIN_FREQ) modulatedFreq = AudioConfig::MIN_FREQ;
      if (modulatedFreq > AudioConfig::MAX_FREQ) modulatedFreq = AudioConfig::MAX_FREQ;
      setAudioFrequency(modulatedFreq);
    }
  }

  if (killReleased && audioRequested) {
    float lfoValue = calculateLfoValue(pots.amplitude);
    int modulatedFreq = static_cast<int>(pots.baseFreq + lfoValue);
    if (modulatedFreq < AudioConfig::MIN_FREQ) modulatedFreq = AudioConfig::MIN_FREQ;
    if (modulatedFreq > AudioConfig::MAX_FREQ) modulatedFreq = AudioConfig::MAX_FREQ;
    setAudioFrequency(modulatedFreq);
  }

  state.lastAudioRequested = audioRequested;
  state.lastAudioActive = audioActive;
  state.lastKillActive = killActive;

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

  setAudioFrequency(modulatedFreq);
}

// D3 hardware PWM uses Timer2 on ATmega328P, which can conflict with Arduino tone().
// Software PWM keeps the LED behavior identical for both audio engines.
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
      return state.angle < PI ? amplitude : -amplitude;
    case SAWTOOTH:
      // Linear slope from -amplitude to +amplitude
      return ((state.angle / TWO_PI) * 2 - 1) * amplitude;
    case REVERSE_SAWTOOTH:
      return (1.0 - (state.angle / TWO_PI)) * 2 * amplitude - amplitude;
    default:
      return 0.0;
  }
}
