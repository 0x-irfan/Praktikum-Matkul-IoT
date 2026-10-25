#include <Arduino.h>
const int ledPins[] = {12, 13, 14};
const int jumlahLed = sizeof(ledPins) / sizeof(ledPins[0]);
const int delayTime = 200; // ms

void setup() {
  for (int i = 0; i < jumlahLed; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
}

void loop() {
  for (int i = 0; i < jumlahLed; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(delayTime);
    digitalWrite(ledPins[i], LOW);
  }
}


