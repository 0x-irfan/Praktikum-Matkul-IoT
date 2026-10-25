#include <Arduino.h>
#define LED_PIN 2

const unsigned long intervals[] = {1000, 2000, 3000}; // ms
const int jumlahInterval = sizeof(intervals) / sizeof(intervals[0]);

int idx = 0;
bool ledState = false;
unsigned long previousMillis = 0;

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= intervals[idx]) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);

    // pindah ke interval berikutnya setelah 1 siklus ON-OFF selesai
    if (!ledState) {
      idx = (idx + 1) % jumlahInterval;
    }
  }
}



