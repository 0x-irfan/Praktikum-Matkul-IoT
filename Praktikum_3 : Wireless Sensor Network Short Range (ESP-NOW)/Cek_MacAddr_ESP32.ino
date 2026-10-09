/*
  CEK MAC ADDRESS ESP32 UNIVERSAL
  Upload ke board mana pun, buka Serial Monitor (115200 baud).

  Yang ditampilkan:
  - MAC Wi-Fi Station (STA)  -> ini yang dipakai ESP-NOW
  - MAC Wi-Fi Access Point (AP)
  - MAC Bluetooth
  - Format array C++ siap salin: {0xCC, 0x7B, ...}
  - Informasi chip
*/

#include <WiFi.h>
#include <esp_system.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp_mac.h>
#endif

void cetakMac(const char *label, const uint8_t *mac) {
  Serial.printf("%-22s: %02X:%02X:%02X:%02X:%02X:%02X\n",
                label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void tampilkanInfo() {
  uint8_t sta[6], ap[6], bt[6];

  // Dibaca langsung dari eFuse, selalu valid (tidak bergantung status Wi-Fi)
  esp_read_mac(sta, ESP_MAC_WIFI_STA);
  esp_read_mac(ap, ESP_MAC_WIFI_SOFTAP);
  esp_read_mac(bt, ESP_MAC_BT);

  Serial.println();
  Serial.println("========== INFO MAC ADDRESS ==========");
  cetakMac("MAC Wi-Fi STA (ESP-NOW)", sta);
  cetakMac("MAC Wi-Fi AP", ap);
  cetakMac("MAC Bluetooth", bt);

  Serial.printf("Format array           : {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}\n",
                sta[0], sta[1], sta[2], sta[3], sta[4], sta[5]);
  Serial.printf("Chip                   : %s rev %d, %d core\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  Serial.println("======================================");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  tampilkanInfo();
  Serial.println("Info diulang tiap 5 detik. Kirim karakter apa saja untuk menampilkan sekarang.");
}

void loop() {
  static uint32_t terakhir = 0;

  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    tampilkanInfo();
    terakhir = millis();
  }

  if (millis() - terakhir >= 5000) {
    terakhir = millis();
    tampilkanInfo();
  }
}
