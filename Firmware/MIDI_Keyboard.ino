#include <Arduino.h>
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI("MIDI Keyboard");

const byte keyPins[] = {4, 5, 6, 13, 12, 11, 10};
const byte drumPins[] = {7, 8, 9};
const byte switchPins[] = {3, 43, 44};
const byte sliderPins[] = {1, 2};

const byte keyNotes[] = {60, 62, 64, 65, 67, 69, 71};
const byte drumNotes[] = {36, 38, 42};

const byte keyCount = 7;
const byte drumCount = 3;
const byte switchCount = 3;

const byte keyboardChannel = 1;
const byte drumChannel = 10;
const byte noteVelocity = 100;

const byte touchPressPercent = 130;
const byte touchReleasePercent = 115;
const unsigned long drumNoteLength = 120;
const unsigned long buttonDebounceTime = 25;
const unsigned long sliderUpdateTime = 20;

unsigned long touchBaseline[keyCount + drumCount];
bool touchIsActive[keyCount + drumCount] = {};
int activeKeyNotes[keyCount] = {};
unsigned long drumStartedAt[drumCount] = {};
bool drumIsPlaying[drumCount] = {};

int octaveShift = 0;

bool buttonLastReading[switchCount] = {HIGH, HIGH, HIGH};
bool buttonState[switchCount] = {HIGH, HIGH, HIGH};
unsigned long buttonChangedAt[switchCount] = {};

int lastSliderValues[2] = {-1, -1};
unsigned long lastSliderUpdate = 0;

void calibrateTouchPads() {
  for (byte pad = 0; pad < keyCount + drumCount; pad++) {
    unsigned long total = 0;
    byte pin;

    if (pad < keyCount) {
      pin = keyPins[pad];
    } else {
      pin = drumPins[pad - keyCount];
    }

    for (byte reading = 0; reading < 30; reading++) {
      total += touchRead(pin);
      delay(10);
    }

    touchBaseline[pad] = total / 30;
  }
}

bool readTouchPad(byte pad, byte pin) {
  unsigned long reading = touchRead(pin);
  unsigned long pressLevel = touchBaseline[pad] * touchPressPercent / 100;
  unsigned long releaseLevel = touchBaseline[pad] * touchReleasePercent / 100;

  if (!touchIsActive[pad] && reading > pressLevel) {
    touchIsActive[pad] = true;
  }

  if (touchIsActive[pad] && reading < releaseLevel) {
    touchIsActive[pad] = false;
  }

  return touchIsActive[pad];
}

void updateKeyPads() {
  for (byte key = 0; key < keyCount; key++) {
    bool isTouched = readTouchPad(key, keyPins[key]);

    if (isTouched && activeKeyNotes[key] == 0) {
      int note = keyNotes[key] + octaveShift * 12;
      activeKeyNotes[key] = note;
      MIDI.noteOn(note, noteVelocity, keyboardChannel);
    }

    if (!isTouched && activeKeyNotes[key] != 0) {
      MIDI.noteOff(activeKeyNotes[key], 0, keyboardChannel);
      activeKeyNotes[key] = 0;
    }
  }
}

void startDrumNotes() {
  for (byte drum = 0; drum < drumCount; drum++) {
    byte pad = keyCount + drum;
    bool isTouched = readTouchPad(pad, drumPins[drum]);

    if (isTouched && !drumIsPlaying[drum]) {
      MIDI.noteOn(drumNotes[drum], noteVelocity, drumChannel);
      drumStartedAt[drum] = millis();
      drumIsPlaying[drum] = true;
    }
  }
}

void stopDrumNotes() {
  for (byte drum = 0; drum < drumCount; drum++) {
    if (drumIsPlaying[drum] && millis() - drumStartedAt[drum] >= drumNoteLength) {
      MIDI.noteOff(drumNotes[drum], 0, drumChannel);
      drumIsPlaying[drum] = false;
    }
  }
}

void updateSwitches() {
  for (byte button = 0; button < switchCount; button++) {
    bool reading = digitalRead(switchPins[button]);

    if (reading != buttonLastReading[button]) {
      buttonLastReading[button] = reading;
      buttonChangedAt[button] = millis();
    }

    if (millis() - buttonChangedAt[button] >= buttonDebounceTime && reading != buttonState[button]) {
      buttonState[button] = reading;

      if (buttonState[button] == LOW) {
        if (button == 0 && octaveShift > -2) {
          octaveShift--;
        }

        if (button == 1 && octaveShift < 2) {
          octaveShift++;
        }

        if (button == 2) {
          MIDI.controlChange(64, 127, keyboardChannel);
        }
      }

      if (buttonState[button] == HIGH && button == 2) {
        MIDI.controlChange(64, 0, keyboardChannel);
      }
    }
  }
}

void updateSliders() {
  if (millis() - lastSliderUpdate < sliderUpdateTime) {
    return;
  }

  lastSliderUpdate = millis();

  for (byte slider = 0; slider < 2; slider++) {
    int reading = analogRead(sliderPins[slider]);
    int midiValue = map(reading, 0, 4095, 0, 127);

    if (lastSliderValues[slider] < 0 || abs(midiValue - lastSliderValues[slider]) >= 2) {
      lastSliderValues[slider] = midiValue;

      if (slider == 0) {
        MIDI.controlChange(7, midiValue, keyboardChannel);
      } else {
        MIDI.controlChange(1, midiValue, keyboardChannel);
      }
    }
  }
}

void setup() {
  analogReadResolution(12);

  for (byte button = 0; button < switchCount; button++) {
    pinMode(switchPins[button], INPUT_PULLUP);
  }

  calibrateTouchPads();

  MIDI.begin();
  USB.begin();
}

void loop() {
  updateSwitches();
  updateKeyPads();
  startDrumNotes();
  stopDrumNotes();
  updateSliders();
}
