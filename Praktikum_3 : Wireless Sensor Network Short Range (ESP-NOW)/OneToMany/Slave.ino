/*
  ESP-NOW One to Many - SLAVE (Penerima)
  Kode yang sama dipakai untuk Slave 1 dan Slave 2.
  Pesan dari master ditampilkan di Serial Monitor (115200 baud).
*/

#include <WiFi.h>
#include <esp_now.h>

// MAC address master
uint8_t masterMAC[] = {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0};

// Struktur pesan (harus sama dengan di master)
typedef struct {
  char text[200];
} Pesan;

Pesan pesanMasuk;

// Callback saat data diterima
// Core ESP32 v3.x : (const esp_now_recv_info_t *info, const uint8_t *data, int len)
// Core ESP32 v2.x : (const uint8_t *mac, const uint8_t *data, int len)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(Pesan)) return;
  if (memcmp(mac, masterMAC, 6) != 0) return;   // hanya terima dari master

  memcpy(&pesanMasuk, data, sizeof(Pesan));
  pesanMasuk.text[sizeof(pesanMasuk.text) - 1] = '\0';

  Serial.print("[Master] ");
  Serial.println(pesanMasuk.text);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  Serial.print("MAC board ini: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Slave siap menerima pesan dari Master...");
}

void loop() {
  // Menunggu pesan masuk (ditangani callback)
}
