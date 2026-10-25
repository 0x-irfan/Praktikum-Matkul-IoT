#include <Arduino.h>
const int ledPins[]    = {12, 13, 14};
const int buttonPins[] = {25, 26, 27};
const int jumlahLed    = sizeof(ledPins) / sizeof(ledPins[0]);

const unsigned long stepDelay = 200; // ms, kecepatan blink/running

int mode = 0;          // 0 = off, 1 = blink, 2 = kiri->kanan, 3 = kanan->kiri
int step = 0;
bool blinkState = false;
unsigned long previousMillis = 0;

void setup() {
  for (int i = 0; i < jumlahLed; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  for (int i = 0; i < jumlahLed; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void allOff() {
  for (int i = 0; i < jumlahLed; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

void bacaTombol() {
  // Tombol aktif LOW (karena INPUT_PULLUP)
  if (digitalRead(buttonPins[0]) == LOW && mode != 1) {
    mode = 1; step = 0; allOff();
  } else if (digitalRead(buttonPins[1]) == LOW && mode != 2) {
    mode = 2; step = 0; allOff();
  } else if (digitalRead(buttonPins[2]) == LOW && mode != 3) {
    mode = 3; step = 0; allOff();
  }
}

void loop() {
  bacaTombol();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= stepDelay) {
    previousMillis = currentMillis;

    switch (mode) {
      case 1: // blink semua LED
        blinkState = !blinkState;
        for (int i = 0; i < jumlahLed; i++) {
          digitalWrite(ledPins[i], blinkState);
        }
        break;

      case 2: // running kiri -> kanan
        allOff();
        digitalWrite(ledPins[step], HIGH);
        step = (step + 1) % jumlahLed;
        break;

      case 3: // running kanan -> kiri
        allOff();
        digitalWrite(ledPins[jumlahLed - 1 - step], HIGH);
        step = (step + 1) % jumlahLed;
        break;

      default:
        allOff();
        break;
    }
  }
}


