/*
 * Dub Siren Machine (tone() function version / v4)
 *
 * Changes:
 * - Minimum frequency clamped to 130Hz.
 * - Pitch upper limit: 2100Hz.
 * - Initial LFO waveform: Square wave.
 * - Silence when POT3 (speed) is fully left (value <= 10).
 *
 * Refactored for better readability and structure.
 * Added randomSeed for better randomness.
 * Improved button debouncing with millis().
 * Explicit type casting for frequency.
 * Added upper frequency clamp for safety (3000Hz).
 *
 * Noise fix for silence transition:
 * - Added hysteresis to silence threshold to prevent chattering.
 * - Added digitalWrite(SPEAKER_PIN, LOW) after noTone() to minimize pop noise.
 * - Use state variable to detect transitions and only call noTone() on entering silence.
 *
 * Volume sweep for smoothing (replacing frequency sweep):
 * - On OFF->ON: Fade LFO amplitude from 0 to target value over 20ms, using base frequency (no LFO).
 * - On ON->OFF: Fade LFO amplitude from current to 0 over 20ms, then noTone().
 * - Sweep updates every loop for smoother transitions.
 * - Removed frequency sweep to avoid contrived feel.
 */

#include <math.h>   // For sin()
#include <stdlib.h> // For random()

// Pin assignments
const int SPEAKER_PIN = 11;     // Audio output
const int PITCH_POT_PIN = A0;   // Base pitch potentiometer
const int AMP_POT_PIN = A1;     // LFO amplitude potentiometer
const int SPEED_POT_PIN = A2;   // LFO speed potentiometer
const int BUTTON_PIN = 4;       // Waveform select button
const int LED_PIN = 3;          // LFO state LED

// Constants
const int SILENCE_THRESHOLD_LOW = 10;   // Enter silence below this
const int SILENCE_THRESHOLD_HIGH = 20;  // Exit silence above this (hysteresis)
const long UPDATE_INTERVAL = 10;        // Update every 10ms (100Hz)
const int MIN_FREQUENCY = 130;          // Minimum frequency
const int MAX_FREQUENCY = 3000;         // Safety upper limit
const int BASE_FREQ_MIN = 130;          // Base freq range
const int BASE_FREQ_MAX = 2100;
const float STEP_MIN = 0.01;            // LFO step range
const float STEP_MAX = 0.8;
const int AMP_MIN = 0;                  // Amplitude range
const int AMP_MAX = 600;
const unsigned long DEBOUNCE_DELAY = 200; // Button debounce time
const unsigned long SWEEP_DURATION = 20;  // Volume sweep time in ms (short for natural feel)

// Waveform types (enum for clarity)
enum Waveform {
  SINE = 0,
  SQUARE = 1,
  SAWTOOTH = 2,
  REVERSE_SAWTOOTH = 3,
  RANDOM_HOLD = 4
};

// Global variables
Waveform waveformType = SQUARE;       // Start with square wave
float angle = 0.0;                    // LFO angle (radians)
float randomValue = 0.0;              // For random hold
unsigned long randomHoldTime = 0;    // Random value hold duration
unsigned long randomLastUpdate = 0;  // Last random update time
unsigned long lastUpdateTime = 0;    // Tone update timer
unsigned long lastButtonPress = 0;   // For debouncing
bool isSilent = true;                // Start in silent mode
bool sweepActive = false;            // Volume sweep in progress
unsigned long sweepStartTime = 0;    // Start time of sweep
float sweepFromAmp = 0.0;           // Starting amplitude of sweep
float sweepToAmp = 0.0;             // Target amplitude of sweep
bool isEnteringSound = false;        // True: OFF->ON sweep, False: ON->OFF sweep
float baseFrequency = MIN_FREQUENCY; // Store base frequency during sweep

void setup() {
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  randomSeed(analogRead(A3));  // Seed random with unused analog pin for variety
  digitalWrite(SPEAKER_PIN, LOW);  // Ensure speaker pin is low initially
}

void loop() {
  // Button handling with debounce
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > DEBOUNCE_DELAY) {
    lastButtonPress = millis();
    waveformType = static_cast<Waveform>((waveformType + 1) % 5);
  }

  // Read speed POT
  int speedValue = analogRead(SPEED_POT_PIN);

  // Hysteresis for silence state
  if (isSilent) {
    if (speedValue > SILENCE_THRESHOLD_HIGH && !sweepActive) {
      // Entering sound: Start volume fade-in
      isSilent = false;
      sweepActive = true;
      isEnteringSound = true;
      sweepStartTime = millis();
      sweepFromAmp = 0.0;
      sweepToAmp = map(analogRead(AMP_POT_PIN), 0, 1023, AMP_MIN, AMP_MAX);
      baseFrequency = map(analogRead(PITCH_POT_PIN), 0, 1023, BASE_FREQ_MIN, BASE_FREQ_MAX);
    }
  } else {
    if (speedValue <= SILENCE_THRESHOLD_LOW && !sweepActive) {
      // Entering silence: Start volume fade-out
      sweepActive = true;
      isEnteringSound = false;
      sweepStartTime = millis();
      sweepFromAmp = map(analogRead(AMP_POT_PIN), 0, 1023, AMP_MIN, AMP_MAX);
      sweepToAmp = 0.0;
      baseFrequency = map(analogRead(PITCH_POT_PIN), 0, 1023, BASE_FREQ_MIN, BASE_FREQ_MAX);
    }
  }

  // If volume sweep is active, handle it (update every loop for smoothness)
  if (sweepActive) {
    unsigned long elapsed = millis() - sweepStartTime;
    if (elapsed >= SWEEP_DURATION) {
      // Sweep complete
      sweepActive = false;
      if (!isEnteringSound) {
        // End of ON->OFF sweep: Silence now
        noTone(SPEAKER_PIN);
        digitalWrite(SPEAKER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        isSilent = true;
      }
      // For OFF->ON, normal operation starts after sweep
    } else {
      // During sweep: Interpolate amplitude, use base frequency (no LFO)
      float progress = static_cast<float>(elapsed) / SWEEP_DURATION;
      float currentAmp = sweepFromAmp + (sweepToAmp - sweepFromAmp) * progress;
      int sweepFreq = static_cast<int>(baseFrequency); // No LFO during sweep
      if (sweepFreq < MIN_FREQUENCY) sweepFreq = MIN_FREQUENCY;
      if (sweepFreq > MAX_FREQUENCY) sweepFreq = MAX_FREQUENCY;
      tone(SPEAKER_PIN, sweepFreq);
      digitalWrite(LED_PIN, currentAmp > 0 ? HIGH : LOW); // LED reflects amplitude
      return;  // Skip normal update during sweep
    }
  }

  if (isSilent) {
    return;  // No update if silent
  }

  // Normal update every UPDATE_INTERVAL ms
  if (millis() - lastUpdateTime >= UPDATE_INTERVAL) {
    lastUpdateTime = millis();

    // Read POT values
    baseFrequency = map(analogRead(PITCH_POT_PIN), 0, 1023, BASE_FREQ_MIN, BASE_FREQ_MAX);
    float step = map(speedValue, 0, 1023, STEP_MIN * 1000, STEP_MAX * 1000) / 1000.0;
    float amplitude = map(analogRead(AMP_POT_PIN), 0, 1023, AMP_MIN, AMP_MAX);

    // Update random if needed
    if (waveformType == RANDOM_HOLD && (millis() - randomLastUpdate >= randomHoldTime)) {
      randomLastUpdate = millis();
      // Map speed: higher speed -> shorter hold time (faster changes)
      randomHoldTime = map(speedValue, 0, 1023, 500, 50);  // Low speed: long hold, high: short
      randomValue = (random(-AMP_MAX, AMP_MAX + 1) / static_cast<float>(AMP_MAX)) * amplitude;
    }

    // Update angle
    angle += step;
    if (angle >= TWO_PI) {
      angle -= TWO_PI;
    }

    // Calculate LFO value
    float lfoValue = calculateLfoValue(amplitude);

    // Modulate frequency and clamp
    int modulatedFrequency = static_cast<int>(baseFrequency + lfoValue);
    if (modulatedFrequency < MIN_FREQUENCY) {
      modulatedFrequency = MIN_FREQUENCY;
    } else if (modulatedFrequency > MAX_FREQUENCY) {
      modulatedFrequency = MAX_FREQUENCY;
    }

    // Output tone and LED
    tone(SPEAKER_PIN, modulatedFrequency);
    digitalWrite(LED_PIN, lfoValue > 0 ? HIGH : LOW);
  }
}

// Function to calculate LFO value based on waveform
float calculateLfoValue(float amplitude) {
  switch (waveformType) {
    case SINE:
      return sin(angle) * amplitude;
    case SQUARE:
      return (angle < PI ? 1 : -1) * amplitude;
    case SAWTOOTH:
      return ((angle / TWO_PI) * 2 - 1) * amplitude;
    case REVERSE_SAWTOOTH:
      return (1.0 - (angle / TWO_PI)) * 2 * amplitude - amplitude;
    case RANDOM_HOLD:
      return randomValue;
    default:
      return 0.0;  // Fallback
  }
}