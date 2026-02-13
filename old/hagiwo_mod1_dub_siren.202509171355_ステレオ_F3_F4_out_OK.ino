/*
 * Stereo Dub Siren Machine (Timer-based version / v7)
 *
 * FIXED: Arduino's tone() function can only output on one pin at a time.
 * This version uses Timer1 interrupt for true stereo output.
 *
 * Features:
 * - STEREO OUTPUT: Pin 10 = L channel, Pin 11 = R channel
 * - Timer1-based square wave generation for both channels
 * - Minimum frequency: 130Hz, upper limit: 2100Hz (clamped to 3000Hz for safety)
 * - Initial LFO waveform: Sine wave
 * - Silence when POT3 (speed) is fully left (value <= 10)
 * - Volume sweep (20ms) for smooth ON/OFF transitions to reduce pop noise
 * - Hysteresis for silence threshold to prevent chattering
 * - Stereo panning control with phase shift between L/R channels
 *
 * Timer Implementation:
 * - Timer1 generates interrupts at audio frequencies
 * - Each channel has independent frequency control
 * - Square wave output (suitable for 8-bit MCU limitations)
 * - Phase offset between channels creates stereo width effect
 */

#include <math.h>   // For sin()

// Configuration constants
struct PinConfig {
  static const int SPEAKER_L = 10;    // Left channel audio output (OC1B)
  static const int SPEAKER_R = 11;    // Right channel audio output (OC1A)
  static const int PITCH_POT = A0;    // Base pitch potentiometer
  static const int AMP_POT = A1;      // LFO amplitude potentiometer
  static const int SPEED_POT = A2;    // LFO speed potentiometer
  static const int BUTTON = 4;        // Waveform select button
  static const int LED = 3;           // LFO state LED
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
  static const float STEREO_PHASE_OFFSET = PI / 4; // Phase difference between L/R (45 degrees)
  static const long F_CPU_FREQ = 16000000L;     // Arduino Uno CPU frequency
};

// Waveform types
enum Waveform {
  SINE = 0,
  SQUARE = 1,
  SAWTOOTH = 2,
  REVERSE_SAWTOOTH = 3
};

// Global state
struct State {
  Waveform waveformType = SINE;       // Initial waveform
  float angle = 0.0;                  // LFO angle (radians)
  unsigned long lastUpdateTime = 0;   // Tone update timer
  unsigned long lastButtonPress = 0;  // Button debounce timer
  bool isSilent = true;               // Silent mode
  bool sweepActive = false;           // Volume sweep in progress
  unsigned long sweepStartTime = 0;   // Sweep start time
  float sweepFromAmp = 0.0;          // Starting amplitude
  float sweepToAmp = 0.0;            // Target amplitude
  bool isEnteringSound = false;       // True: OFF->ON, False: ON->OFF
  
  // Audio generation variables
  volatile bool audioEnabled = false;
  volatile unsigned int freqL = 440;  // Left channel frequency
  volatile unsigned int freqR = 440;  // Right channel frequency
  volatile unsigned long counterL = 0; // Left channel phase accumulator
  volatile unsigned long counterR = 0; // Right channel phase accumulator
  volatile unsigned long phaseIncL = 0; // Left channel phase increment
  volatile unsigned long phaseIncR = 0; // Right channel phase increment
} state;

// PotValues struct for reading POT values
struct PotValues {
  float baseFreq;
  float amplitude;
  float step;
  int speedValue;
};

// Timer1 interrupt service routine for audio generation
ISR(TIMER1_COMPA_vect) {
  if (!state.audioEnabled) {
    digitalWrite(PinConfig::SPEAKER_L, LOW);
    digitalWrite(PinConfig::SPEAKER_R, LOW);
    return;
  }
  
  // Update phase accumulators
  state.counterL += state.phaseIncL;
  state.counterR += state.phaseIncR;
  
  // Generate square waves (toggle on overflow)
  digitalWrite(PinConfig::SPEAKER_L, (state.counterL & 0x80000000L) ? HIGH : LOW);
  digitalWrite(PinConfig::SPEAKER_R, (state.counterR & 0x80000000L) ? HIGH : LOW);
}

// Initialize Timer1 for audio generation
void setupTimer1() {
  // Disable interrupts during setup
  cli();
  
  // Set Timer1 to CTC mode (Clear Timer on Compare Match)
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  
  // Set compare match register for desired frequency (44.1kHz sample rate)
  OCR1A = AudioConfig::F_CPU_FREQ / 44100 - 1;
  
  // Turn on CTC mode
  TCCR1B |= (1 << WGM12);
  
  // Set CS10 bit for no prescaler
  TCCR1B |= (1 << CS10);
  
  // Enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  
  // Enable interrupts
  sei();
}

// Set frequency for both channels
void setFrequencies(unsigned int freqL, unsigned int freqR) {
  // Calculate phase increments (32-bit fixed point)
  // phaseInc = (freq * 2^32) / sampleRate
  state.phaseIncL = ((unsigned long)freqL << 16) / 44100 * 65536UL;
  state.phaseIncR = ((unsigned long)freqR << 16) / 44100 * 65536UL;
  
  state.freqL = freqL;
  state.freqR = freqR;
}

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
  pinMode(PinConfig::SPEAKER_L, OUTPUT);
  pinMode(PinConfig::SPEAKER_R, OUTPUT);
  pinMode(PinConfig::BUTTON, INPUT_PULLUP);
  pinMode(PinConfig::LED, OUTPUT);
  digitalWrite(PinConfig::SPEAKER_L, LOW);
  digitalWrite(PinConfig::SPEAKER_R, LOW);
  
  // Initialize timer-based audio generation
  setupTimer1();
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

// Handle volume sweep (stereo version)
void handleVolumeSweep(float baseFreq) {
  unsigned long elapsed = millis() - state.sweepStartTime;
  if (elapsed >= AudioConfig::SWEEP_DURATION) {
    // Sweep complete
    state.sweepActive = false;
    if (!state.isEnteringSound) {
      // End of ON->OFF: Silence both channels
      state.audioEnabled = false;
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
  
  // Output same frequency to both channels during sweep
  setFrequencies(freq, freq);
  state.audioEnabled = true;
  digitalWrite(PinConfig::LED, currentAmp > 0 ? HIGH : LOW);
}

// Normal operation: Update LFO and output tone (stereo version)
void updateNormalOperation(float baseFreq, float amplitude, float step) {
  if (millis() - state.lastUpdateTime < AudioConfig::UPDATE_INTERVAL) return;
  state.lastUpdateTime = millis();

  // Update LFO angle
  state.angle += step;
  if (state.angle >= TWO_PI) state.angle -= TWO_PI;

  // Calculate LFO values for both channels
  float lfoValueL = calculateLfoValue(amplitude, state.angle);                                    // Left channel
  float lfoValueR = calculateLfoValue(amplitude, state.angle + AudioConfig::STEREO_PHASE_OFFSET); // Right channel with phase offset

  // Modulate frequency for both channels
  int modulatedFreqL = static_cast<int>(baseFreq + lfoValueL);
  int modulatedFreqR = static_cast<int>(baseFreq + lfoValueR);
  
  // Clamp frequencies
  if (modulatedFreqL < AudioConfig::MIN_FREQ) modulatedFreqL = AudioConfig::MIN_FREQ;
  if (modulatedFreqL > AudioConfig::MAX_FREQ) modulatedFreqL = AudioConfig::MAX_FREQ;
  if (modulatedFreqR < AudioConfig::MIN_FREQ) modulatedFreqR = AudioConfig::MIN_FREQ;
  if (modulatedFreqR > AudioConfig::MAX_FREQ) modulatedFreqR = AudioConfig::MAX_FREQ;

  // Output to both channels
  setFrequencies(modulatedFreqL, modulatedFreqR);
  state.audioEnabled = true;
  
  // LED indicates average LFO activity
  digitalWrite(PinConfig::LED, (lfoValueL + lfoValueR) > 0 ? HIGH : LOW);
}

// Calculate LFO value based on waveform (with angle parameter for stereo phase offset)
float calculateLfoValue(float amplitude, float angle) {
  // Normalize angle to 0-2π range
  while (angle >= TWO_PI) angle -= TWO_PI;
  while (angle < 0) angle += TWO_PI;
  
  switch (state.waveformType) {
    case SINE:
      return sin(angle) * amplitude;
    case SQUARE:
      return (angle < PI ? 1 : -1) * amplitude;
    case SAWTOOTH:
      return ((angle / TWO_PI) * 2 - 1) * amplitude;
    case REVERSE_SAWTOOTH:
      return (1.0 - (angle / TWO_PI)) * 2 * amplitude - amplitude;
    default:
      return 0.0;
  }
}