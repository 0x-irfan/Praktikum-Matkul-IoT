#include <Arduino.h>
#define LED_PIN 2

const int intervals[] = {1000, 2000, 3000}; // ms
const int jumlahInterval = sizeof(intervals) / sizeof(intervals[0]);

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  for (int i = 0; i < jumlahInterval; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(intervals[i]);
    digitalWrite(LED_PIN, LOW);
    delay(intervals[i]);
  }
}


