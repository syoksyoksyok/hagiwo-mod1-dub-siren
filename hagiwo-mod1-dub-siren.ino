/*
 * Dub Siren Machine (tone() function version / v5)
 *
 * Features:
 * - Minimum frequency: 130Hz, upper limit: 2100Hz (clamped to 3000Hz for safety).
 * - Initial LFO waveform: Sine wave (changed from Square).
 * - POT3 controls only LFO speed; it no longer mutes the output.
 * - A3 / D17 gate input enables sound while HIGH and stops sound while LOW.
 * - LED blinks at LFO speed with brightness controlled by LFO depth.
 *
 * Refactoring Improvements:
 * - Modularized into functions: readPotValues, updateNormalOperation, calculateLfoValue.
 * - Grouped constants into PinConfig and AudioConfig for clarity.
 * - Reduced global variables, cached POT values to minimize analogRead().
 * - Simplified state management with clear transitions.
 * - Added POT value smoothing for stability.
 *
 * Previous Fixes (2025-09-16):
 * - Moved PotValues struct definition before readPotValues to fix "does not name a type" error.
 * - Corrected analogRead(PinConfig::SPEAKER) to PinConfig::SPEED_POT in readPotValues.
 * - Swapped RANDOM_HOLD and SINE in Waveform enum.
 *
 * Changes (2025-09-16):
 * - Removed RANDOM_HOLD waveform and related logic/variables.
 * - Updated Waveform enum to include only SINE, SQUARE, SAWTOOTH, REVERSE_SAWTOOTH.
 * - Removed randomSeed and stdlib.h dependency.
 * - Updated comments to reflect removal of random waveform.
 * - Changed initial waveform to SINE.
 *
 * Software-side Noise Reduction (2025-10-01):
 * - Enhanced smoothing for all POT values.
 * - Adjusted SQUARE and SAWTOOTH waveforms to reduce abrupt frequency changes.
 */

#include <math.h>   // For sin()

// Configuration constants
struct PinConfig {
  static const int SPEAKER = 11;       // Audio output
  static const int PITCH_POT = A0;     // Base pitch potentiometer
  static const int AMP_POT = A1;       // LFO amplitude potentiometer
  static const int SPEED_POT = A2;     // LFO speed potentiometer
  static const int GATE_INPUT = A3;    // External gate input on A3 / D17
  static const int BUTTON = 4;         // Waveform select button
  static const int LED = 3;            // LFO state LED
};

struct AudioConfig {
  static const long UPDATE_INTERVAL = 10;      // Update every 10ms (100Hz)
  static const int MIN_FREQ = 130;             // Minimum frequency
  static const int MAX_FREQ = 3000;            // Safety upper limit
  static const int BASE_FREQ_MIN = 130;        // Base frequency range
  static const int BASE_FREQ_MAX = 2100;
  static constexpr float STEP_MIN = 0.01;      // LFO step range
  static constexpr float STEP_MAX = 0.8;
  static const int AMP_MIN = 0;                // LFO amplitude range
  static const int AMP_MAX = 600;
  static const unsigned long DEBOUNCE_DELAY = 200;  // Button debounce time
  static const unsigned int LED_PWM_PERIOD_US = 4096;  // Software PWM period for D3
};

// Waveform types (Removed RANDOM_HOLD)
enum Waveform {
  SINE = 0,
  SQUARE = 1,
  SAWTOOTH = 2,
  REVERSE_SAWTOOTH = 3
};

// Global state
struct State {
  Waveform waveformType = SINE;
  float angle = 0.0;
  unsigned long lastUpdateTime = 0;
  unsigned long lastButtonPress = 0;
  bool lastGateActive = false;
  uint8_t ledBrightness = 0;
} state;

// PotValues struct for reading POT values
struct PotValues {
  float baseFreq;
  float amplitude;
  float step;
};

// Read and cache POT values with smoothing
PotValues readPotValues() {
  // Enhanced smoothing for all POT values using a 2-sample moving average
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
  pinMode(PinConfig::LED, OUTPUT);
  digitalWrite(PinConfig::SPEAKER, LOW);
}

void loop() {
  if (digitalRead(PinConfig::BUTTON) == LOW && millis() - state.lastButtonPress > AudioConfig::DEBOUNCE_DELAY) {
    state.lastButtonPress = millis();
    state.waveformType = static_cast<Waveform>((state.waveformType + 1) % 4);
  }

  PotValues pots = readPotValues();

  bool gateActive = digitalRead(PinConfig::GATE_INPUT) == HIGH;
  if (!gateActive && state.lastGateActive) {
    noTone(PinConfig::SPEAKER);
    digitalWrite(PinConfig::SPEAKER, LOW);
  }

  if (gateActive && !state.lastGateActive) {
    state.angle = 0.0;
    state.lastUpdateTime = 0;
  }
  state.lastGateActive = gateActive;

  updateNormalOperation(pots.baseFreq, pots.amplitude, pots.step, gateActive);
  updateLedPwm();
}

void updateNormalOperation(float baseFreq, float amplitude, float step, bool audioEnabled) {
  if (millis() - state.lastUpdateTime < AudioConfig::UPDATE_INTERVAL) return;
  state.lastUpdateTime = millis();

  state.angle += step;
  if (state.angle >= TWO_PI) state.angle -= TWO_PI;

  float lfoValue = calculateLfoValue(amplitude);
  uint8_t depthBrightness = static_cast<uint8_t>(map(static_cast<int>(amplitude), AudioConfig::AMP_MIN, AudioConfig::AMP_MAX, 0, 255));
  state.ledBrightness = lfoValue > 0 ? depthBrightness : 0;

  if (!audioEnabled) return;

  int modulatedFreq = static_cast<int>(baseFreq + lfoValue);
  if (modulatedFreq < AudioConfig::MIN_FREQ) modulatedFreq = AudioConfig::MIN_FREQ;
  if (modulatedFreq > AudioConfig::MAX_FREQ) modulatedFreq = AudioConfig::MAX_FREQ;

  tone(PinConfig::SPEAKER, modulatedFreq);
}

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

// Calculate LFO value based on waveform
float calculateLfoValue(float amplitude) {
  switch (state.waveformType) {
    case SINE:
      return sin(state.angle) * amplitude;
    case SQUARE:
      // Smoothed square wave: using a sinusoidal curve for transitions
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
      // Adjusted sawtooth: linear slope from -amplitude to +amplitude
      return ((state.angle / TWO_PI) * 2 - 1) * amplitude;
    case REVERSE_SAWTOOTH:
      return (1.0 - (state.angle / TWO_PI)) * 2 * amplitude - amplitude;
    default:
      return 0.0;
  }
}
