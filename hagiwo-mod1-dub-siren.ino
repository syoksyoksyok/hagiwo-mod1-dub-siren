/*
 * Dub Siren Machine (8-bit PWM audio version)
 *
 * Features:
 * - 8-bit PWM audio output on D11 / F4.
 * - PWM carrier: 62.5kHz on Timer2.
 * - Audio sample update: 15.625kHz on Timer1.
 * - Minimum frequency: 130Hz, upper limit: 2100Hz (clamped to 3000Hz for safety).
 * - Initial LFO waveform: Sine wave.
 * - Silence when POT3 (speed) is fully left (value <= 10).
 * - Real output-level sweep (20ms) for smooth ON/OFF transitions.
 * - Hysteresis for silence threshold to prevent chattering.
 * - Integer/fixed-point control path; no float or sin() in runtime code.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define AUDIO_PHASE_INC_PER_HZ 274878UL

// Configuration constants
struct PinConfig {
  static const uint8_t SPEAKER = 11;       // Audio output: OC2A / D11 / F4
  static const uint8_t PITCH_POT = A0;     // Base pitch potentiometer
  static const uint8_t AMP_POT = A1;       // LFO amplitude potentiometer
  static const uint8_t SPEED_POT = A2;     // LFO speed potentiometer
  static const uint8_t BUTTON = 4;         // Waveform select button
  static const uint8_t LED = 3;            // LFO state LED
};

struct AudioConfig {
  static const uint16_t SILENCE_THRESHOLD_LOW = 10;   // Enter silence below this
  static const uint16_t SILENCE_THRESHOLD_HIGH = 20;  // Exit silence above this
  static const uint16_t CONTROL_UPDATE_INTERVAL = 10; // Control update every 10ms
  static const uint16_t MIN_FREQ = 130;
  static const uint16_t MAX_FREQ = 3000;
  static const uint16_t BASE_FREQ_MIN = 130;
  static const uint16_t BASE_FREQ_MAX = 2100;
  static const uint16_t LFO_DEPTH_MAX_HZ = 600;
  static const uint16_t LFO_STEP_MIN = 104;            // 0.01 rad/update in uint16 phase
  static const uint16_t LFO_STEP_MAX = 8344;           // 0.8 rad/update in uint16 phase
  static const uint8_t PWM_CENTER = 128;
  static const uint8_t PWM_MAX_LEVEL = 255;
  static const unsigned long DEBOUNCE_DELAY = 200;
  static const unsigned long SWEEP_DURATION = 20;
};

// Waveform types for pitch LFO
enum Waveform {
  SINE = 0,
  SQUARE = 1,
  SAWTOOTH = 2,
  REVERSE_SAWTOOTH = 3
};

// 8-bit sine table used by both the audio oscillator and LFO.
const uint8_t sineTable[256] PROGMEM = {
  128, 131, 134, 137, 140, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174,
  177, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 206, 209, 211, 213, 216,
  218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240, 241, 243, 244,
  245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
  255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
  245, 244, 243, 241, 240, 239, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
  218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 191, 188, 185, 182, 179,
  177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 140, 137, 134, 131,
  128, 125, 122, 119, 116, 112, 109, 106, 103, 100,  97,  94,  91,  88,  85,  82,
   79,  77,  74,  71,  68,  65,  63,  60,  57,  55,  52,  50,  47,  45,  43,  40,
   38,  36,  34,  32,  30,  28,  26,  24,  22,  21,  19,  17,  16,  15,  13,  12,
   11,  10,   8,   7,   6,   6,   5,   4,   3,   3,   2,   2,   2,   1,   1,   1,
    1,   1,   1,   1,   2,   2,   2,   3,   3,   4,   5,   6,   6,   7,   8,  10,
   11,  12,  13,  15,  16,  17,  19,  21,  22,  24,  26,  28,  30,  32,  34,  36,
   38,  40,  43,  45,  47,  50,  52,  55,  57,  60,  63,  65,  68,  71,  74,  77,
   79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 116, 119, 122, 125
};

struct State {
  Waveform waveformType = SINE;
  uint16_t lfoPhase = 0;
  unsigned long lastUpdateTime = 0;
  unsigned long lastButtonPress = 0;
  bool isSilent = true;
  bool sweepActive = false;
  unsigned long sweepStartTime = 0;
  uint8_t sweepFromLevel = 0;
  uint8_t sweepToLevel = 0;
  bool isEnteringSound = false;
} state;

struct PotValues {
  uint16_t baseFreq;
  uint16_t lfoDepthHz;
  uint16_t lfoStep;
  uint16_t speedValue;
};

volatile uint32_t audioPhase = 0;
volatile uint32_t audioPhaseIncrement = 0;
volatile uint8_t audioLevel = 0;
volatile bool audioEnabled = false;

void setupPwmAudio();
void setupSampleTimer();
void setAudioFrequency(uint16_t frequency);
void setAudioLevel(uint8_t level);
void setAudioEnabled(bool enabled);
PotValues readPotValues();
void updateSilenceState(uint16_t speedValue);
void handleVolumeSweep(uint16_t baseFreq);
void updateNormalOperation(uint16_t baseFreq, uint16_t lfoDepthHz, uint16_t lfoStep);
int16_t calculateLfoValue(uint16_t lfoDepthHz);

ISR(TIMER1_COMPA_vect) {
  if (!audioEnabled || audioLevel == 0) {
    OCR2A = AudioConfig::PWM_CENTER;
    return;
  }

  audioPhase += audioPhaseIncrement;
  uint8_t sample = pgm_read_byte(&sineTable[audioPhase >> 24]);
  int16_t centered = (int16_t)sample - AudioConfig::PWM_CENTER;
  int16_t scaled = (centered * audioLevel) >> 8;
  OCR2A = AudioConfig::PWM_CENTER + scaled;
}

void setup() {
  pinMode(PinConfig::SPEAKER, OUTPUT);
  pinMode(PinConfig::BUTTON, INPUT_PULLUP);
  pinMode(PinConfig::LED, OUTPUT);
  digitalWrite(PinConfig::LED, LOW);

  setupPwmAudio();
  setupSampleTimer();
  setAudioEnabled(false);
}

void loop() {
  if (digitalRead(PinConfig::BUTTON) == LOW && millis() - state.lastButtonPress > AudioConfig::DEBOUNCE_DELAY) {
    state.lastButtonPress = millis();
    state.waveformType = static_cast<Waveform>((state.waveformType + 1) & 0x03);
  }

  PotValues pots = readPotValues();
  updateSilenceState(pots.speedValue);

  if (state.sweepActive) {
    handleVolumeSweep(pots.baseFreq);
    return;
  }

  if (!state.isSilent) {
    updateNormalOperation(pots.baseFreq, pots.lfoDepthHz, pots.lfoStep);
  }
}

void setupPwmAudio() {
  cli();
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  OCR2A = AudioConfig::PWM_CENTER;
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20); // Fast PWM on OC2A / D11
  TCCR2B = _BV(CS20);                             // No prescaler: 62.5kHz carrier
  sei();
}

void setupSampleTimer() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 1023;                  // 16MHz / (1023 + 1) = 15.625kHz
  TCCR1B = _BV(WGM12) | _BV(CS10);
  TIMSK1 = _BV(OCIE1A);
  sei();
}

void setAudioFrequency(uint16_t frequency) {
  if (frequency < AudioConfig::MIN_FREQ) frequency = AudioConfig::MIN_FREQ;
  if (frequency > AudioConfig::MAX_FREQ) frequency = AudioConfig::MAX_FREQ;

  uint32_t phaseIncrement = (uint32_t)frequency * AUDIO_PHASE_INC_PER_HZ;
  noInterrupts();
  audioPhaseIncrement = phaseIncrement;
  interrupts();
}

void setAudioLevel(uint8_t level) {
  noInterrupts();
  audioLevel = level;
  interrupts();
}

void setAudioEnabled(bool enabled) {
  noInterrupts();
  audioEnabled = enabled;
  if (!enabled) {
    audioLevel = 0;
    OCR2A = AudioConfig::PWM_CENTER;
  }
  interrupts();
}

PotValues readPotValues() {
  static bool initialized = false;
  static uint16_t pitchSmooth = 0;
  static uint16_t ampSmooth = 0;
  static uint16_t speedSmooth = 0;

  uint16_t pitchRaw = analogRead(PinConfig::PITCH_POT);
  uint16_t ampRaw = analogRead(PinConfig::AMP_POT);
  uint16_t speedRaw = analogRead(PinConfig::SPEED_POT);

  if (!initialized) {
    pitchSmooth = pitchRaw;
    ampSmooth = ampRaw;
    speedSmooth = speedRaw;
    initialized = true;
  } else {
    pitchSmooth = (pitchSmooth + pitchRaw) >> 1;
    ampSmooth = (ampSmooth + ampRaw) >> 1;
    speedSmooth = (speedSmooth + speedRaw) >> 1;
  }

  uint16_t baseFreq = AudioConfig::BASE_FREQ_MIN +
    (((uint32_t)pitchSmooth * (AudioConfig::BASE_FREQ_MAX - AudioConfig::BASE_FREQ_MIN)) >> 10);
  uint16_t lfoDepthHz = ((uint32_t)ampSmooth * AudioConfig::LFO_DEPTH_MAX_HZ) >> 10;
  uint16_t lfoStep = AudioConfig::LFO_STEP_MIN +
    (((uint32_t)speedSmooth * (AudioConfig::LFO_STEP_MAX - AudioConfig::LFO_STEP_MIN)) >> 10);

  return {baseFreq, lfoDepthHz, lfoStep, speedSmooth};
}

void updateSilenceState(uint16_t speedValue) {
  if (state.isSilent) {
    if (speedValue > AudioConfig::SILENCE_THRESHOLD_HIGH && !state.sweepActive) {
      state.isSilent = false;
      state.sweepActive = true;
      state.isEnteringSound = true;
      state.sweepStartTime = millis();
      state.sweepFromLevel = 0;
      state.sweepToLevel = AudioConfig::PWM_MAX_LEVEL;
      setAudioEnabled(true);
    }
  } else {
    if (speedValue <= AudioConfig::SILENCE_THRESHOLD_LOW && !state.sweepActive) {
      state.sweepActive = true;
      state.isEnteringSound = false;
      state.sweepStartTime = millis();
      state.sweepFromLevel = AudioConfig::PWM_MAX_LEVEL;
      state.sweepToLevel = 0;
    }
  }
}

void handleVolumeSweep(uint16_t baseFreq) {
  setAudioFrequency(baseFreq);

  unsigned long elapsed = millis() - state.sweepStartTime;
  if (elapsed >= AudioConfig::SWEEP_DURATION) {
    state.sweepActive = false;
    if (state.isEnteringSound) {
      setAudioLevel(AudioConfig::PWM_MAX_LEVEL);
      digitalWrite(PinConfig::LED, HIGH);
    } else {
      setAudioEnabled(false);
      digitalWrite(PinConfig::LED, LOW);
      state.isSilent = true;
    }
    return;
  }

  uint8_t level;
  if (state.sweepToLevel >= state.sweepFromLevel) {
    level = state.sweepFromLevel +
      (((uint16_t)(state.sweepToLevel - state.sweepFromLevel) * elapsed) / AudioConfig::SWEEP_DURATION);
  } else {
    level = state.sweepFromLevel -
      (((uint16_t)(state.sweepFromLevel - state.sweepToLevel) * elapsed) / AudioConfig::SWEEP_DURATION);
  }

  setAudioLevel(level);
  digitalWrite(PinConfig::LED, level > 0 ? HIGH : LOW);
}

void updateNormalOperation(uint16_t baseFreq, uint16_t lfoDepthHz, uint16_t lfoStep) {
  if (millis() - state.lastUpdateTime < AudioConfig::CONTROL_UPDATE_INTERVAL) return;
  state.lastUpdateTime = millis();

  state.lfoPhase += lfoStep;
  int16_t lfoValue = calculateLfoValue(lfoDepthHz);

  int16_t modulatedFreq = (int16_t)baseFreq + lfoValue;
  if (modulatedFreq < (int16_t)AudioConfig::MIN_FREQ) modulatedFreq = AudioConfig::MIN_FREQ;
  if (modulatedFreq > (int16_t)AudioConfig::MAX_FREQ) modulatedFreq = AudioConfig::MAX_FREQ;

  setAudioFrequency((uint16_t)modulatedFreq);
  setAudioLevel(AudioConfig::PWM_MAX_LEVEL);
  digitalWrite(PinConfig::LED, lfoValue > 0 ? HIGH : LOW);
}

int16_t calculateLfoValue(uint16_t lfoDepthHz) {
  int16_t lfo = 0; // -128..127 nominal
  uint8_t index = state.lfoPhase >> 8;

  switch (state.waveformType) {
    case SINE:
      lfo = (int16_t)pgm_read_byte(&sineTable[index]) - 128;
      break;
    case SQUARE:
      if (state.lfoPhase < 0x4000) {
        lfo = 127;
      } else if (state.lfoPhase < 0x8000) {
        uint8_t transitionIndex = (state.lfoPhase - 0x4000) >> 7;
        lfo = (int16_t)pgm_read_byte(&sineTable[transitionIndex]) - 128;
      } else if (state.lfoPhase < 0xC000) {
        lfo = -127;
      } else {
        uint8_t transitionIndex = (state.lfoPhase - 0xC000) >> 7;
        lfo = -((int16_t)pgm_read_byte(&sineTable[transitionIndex]) - 128);
      }
      break;
    case SAWTOOTH:
      lfo = (int16_t)index - 128;
      break;
    case REVERSE_SAWTOOTH:
      lfo = 127 - (int16_t)index;
      break;
    default:
      lfo = 0;
      break;
  }

  return (lfo * (int16_t)lfoDepthHz) >> 7;
}
