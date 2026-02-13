/*
 * Dub Siren Machine (tone() function version / v5)
 *
 * Features:
 * - Minimum frequency: 130Hz, upper limit: 2100Hz (clamped to 3000Hz for safety).
 * - Initial LFO waveform: Sine wave (changed from Square).
 * - Silence when POT3 (speed) is fully left (value <= 10).
 * - Volume sweep (20ms) for smooth ON/OFF transitions to reduce pop noise.
 * - Hysteresis for silence threshold to prevent chattering.
 * - digitalWrite(SPEAKER_PIN, LOW) after noTone() for pin stability.
 *
 * Refactoring Improvements:
 * - Modularized into functions: readPotValues, updateSilenceState, handleVolumeSweep, updateNormalOperation.
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
 */

#include <math.h>   // For sin()

// Configuration constants
struct PinConfig {
  static const int SPEAKER = 11;     // Audio output
  static const int PITCH_POT = A0;   // Base pitch potentiometer
  static const int AMP_POT = A1;     // LFO amplitude potentiometer
  static const int SPEED_POT = A2;   // LFO speed potentiometer
  static const int BUTTON = 4;       // Waveform select button
  static const int LED = 3;          // LFO state LED
};

struct AudioConfig {
  static const int SILENCE_THRESHOLD_LOW = 10;   // Enter silence below this
  static const int SILENCE_THRESHOLD_HIGH = 20;  // Exit silence above this
  static const long UPDATE_INTERVAL = 10;        // Update every 10ms (100Hz)
  static const int MIN_FREQ = 130;              // Minimum frequency
  static const int MAX_FREQ = 3000;             // Safety upper limit
  static const int BASE_FREQ_MIN = 130;         // Base frequency range
  static const int BASE_FREQ_MAX = 2100;
  static const float STEP_MIN = 0.01;           // LFO step range
  static const float STEP_MAX = 0.8;
  static const int AMP_MIN = 0;                 // LFO amplitude range
  static const int AMP_MAX = 600;
  static const unsigned long DEBOUNCE_DELAY = 200; // Button debounce time
  static const unsigned long SWEEP_DURATION = 20;  // Volume sweep time (ms)
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
  Waveform waveformType = SINE;     // Initial waveform (changed to SINE)
  float angle = 0.0;                  // LFO angle (radians)
  unsigned long lastUpdateTime = 0;   // Tone update timer
  unsigned long lastButtonPress = 0;  // Button debounce timer
  bool isSilent = true;               // Silent mode
  bool sweepActive = false;           // Volume sweep in progress
  unsigned long sweepStartTime = 0;   // Sweep start time
  float sweepFromAmp = 0.0;          // Starting amplitude
  float sweepToAmp = 0.0;            // Target amplitude
  bool isEnteringSound = false;       // True: OFF->ON, False: ON->OFF
} state;

// PotValues struct for reading POT values
struct PotValues {
  float baseFreq;
  float amplitude;
  float step;
  int speedValue;
};

// Read and cache POT values with smoothing
PotValues readPotValues() {
  static int lastSpeedValue = 0;
  int speedValue = analogRead(PinConfig::SPEED_POT);
  // 2-sample moving average for stability
  speedValue = (speedValue + lastSpeedValue) / 2;
  lastSpeedValue = speedValue;

  float baseFreq = map(analogRead(PinConfig::PITCH_POT), 0, 1023, AudioConfig::BASE_FREQ_MIN, AudioConfig::BASE_FREQ_MAX);
  float amplitude = map(analogRead(PinConfig::AMP_POT), 0, 1023, AudioConfig::AMP_MIN, AudioConfig::AMP_MAX);
  float step = map(speedValue, 0, 1023, AudioConfig::STEP_MIN * 1000, AudioConfig::STEP_MAX * 1000) / 1000.0;

  return {baseFreq, amplitude, step, speedValue};
}

void setup() {
  pinMode(PinConfig::SPEAKER, OUTPUT);
  pinMode(PinConfig::BUTTON, INPUT_PULLUP);
  pinMode(PinConfig::LED, OUTPUT);
  digitalWrite(PinConfig::SPEAKER, LOW);  // Ensure speaker pin is low
}

void loop() {
  // Handle button press (waveform selection)
  if (digitalRead(PinConfig::BUTTON) == LOW && millis() - state.lastButtonPress > AudioConfig::DEBOUNCE_DELAY) {
    state.lastButtonPress = millis();
    state.waveformType = static_cast<Waveform>((state.waveformType + 1) % 4); // 4 waveforms
  }

  // Read and cache POT values
  auto [baseFreq, amplitude, step, speedValue] = readPotValues();

  // Update silence state (hysteresis)
  updateSilenceState(speedValue, baseFreq, amplitude);

  // Handle volume sweep if active
  if (state.sweepActive) {
    handleVolumeSweep(baseFreq);
    return;  // Skip normal operation during sweep
  }

  // Normal operation (LFO modulation) if not silent
  if (!state.isSilent) {
    updateNormalOperation(baseFreq, amplitude, step);
  }
}

// Update silence state with hysteresis
void updateSilenceState(int speedValue, float baseFreq, float amplitude) {
  if (state.isSilent) {
    if (speedValue > AudioConfig::SILENCE_THRESHOLD_HIGH && !state.sweepActive) {
      // Start fade-in
      state.isSilent = false;
      state.sweepActive = true;
      state.isEnteringSound = true;
      state.sweepStartTime = millis();
      state.sweepFromAmp = 0.0;
      state.sweepToAmp = amplitude;
    }
  } else {
    if (speedValue <= AudioConfig::SILENCE_THRESHOLD_LOW && !state.sweepActive) {
      // Start fade-out
      state.sweepActive = true;
      state.isEnteringSound = false;
      state.sweepStartTime = millis();
      state.sweepFromAmp = amplitude;
      state.sweepToAmp = 0.0;
    }
  }
}

// Handle volume sweep
void handleVolumeSweep(float baseFreq) {
  unsigned long elapsed = millis() - state.sweepStartTime;
  if (elapsed >= AudioConfig::SWEEP_DURATION) {
    // Sweep complete
    state.sweepActive = false;
    if (!state.isEnteringSound) {
      // End of ON->OFF: Silence
      noTone(PinConfig::SPEAKER);
      digitalWrite(PinConfig::SPEAKER, LOW);
      digitalWrite(PinConfig::LED, LOW);
      state.isSilent = true;
    }
    return;
  }

  // Interpolate amplitude
  float progress = static_cast<float>(elapsed) / AudioConfig::SWEEP_DURATION;
  float currentAmp = state.sweepFromAmp + (state.sweepToAmp - state.sweepFromAmp) * progress;
  int freq = static_cast<int>(baseFreq);  // No LFO during sweep
  if (freq < AudioConfig::MIN_FREQ) freq = AudioConfig::MIN_FREQ;
  if (freq > AudioConfig::MAX_FREQ) freq = AudioConfig::MAX_FREQ;
  tone(PinConfig::SPEAKER, freq);
  digitalWrite(PinConfig::LED, currentAmp > 0 ? HIGH : LOW);
}

// Normal operation: Update LFO and output tone
void updateNormalOperation(float baseFreq, float amplitude, float step) {
  if (millis() - state.lastUpdateTime < AudioConfig::UPDATE_INTERVAL) return;
  state.lastUpdateTime = millis();

  // Update LFO angle
  state.angle += step;
  if (state.angle >= TWO_PI) state.angle -= TWO_PI;

  // Calculate LFO value
  float lfoValue = calculateLfoValue(amplitude);

  // Modulate frequency
  int modulatedFreq = static_cast<int>(baseFreq + lfoValue);
  if (modulatedFreq < AudioConfig::MIN_FREQ) modulatedFreq = AudioConfig::MIN_FREQ;
  if (modulatedFreq > AudioConfig::MAX_FREQ) modulatedFreq = AudioConfig::MAX_FREQ;

  // Output
  tone(PinConfig::SPEAKER, modulatedFreq);
  digitalWrite(PinConfig::LED, lfoValue > 0 ? HIGH : LOW);
}

// Calculate LFO value based on waveform
float calculateLfoValue(float amplitude) {
  switch (state.waveformType) {
    case SINE:
      return sin(state.angle) * amplitude;
    case SQUARE:
      return (state.angle < PI ? 1 : -1) * amplitude;
    case SAWTOOTH:
      return ((state.angle / TWO_PI) * 2 - 1) * amplitude;
    case REVERSE_SAWTOOTH:
      return (1.0 - (state.angle / TWO_PI)) * 2 * amplitude - amplitude;
    default:
      return 0.0;
  }
}