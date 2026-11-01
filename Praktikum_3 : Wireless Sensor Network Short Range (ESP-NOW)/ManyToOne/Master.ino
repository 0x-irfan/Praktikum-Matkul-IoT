/*
  ESP-NOW Many to One - MASTER (Penerima)
  Master  : cc:7b:5c:f2:2e:e0
  Slave 1 : a4:f0:0f:91:95:60
  Slave 2 : 20:9b:a9:b1:c4:80

  Master menerima pesan teks dari Slave 1 dan Slave 2,
  lalu menampilkannya di Serial Monitor (115200 baud).
*/

#include <WiFi.h>
#include <esp_now.h>

// MAC address slave
uint8_t slave1MAC[] = {0xA4, 0xF0, 0x0F, 0x91, 0x95, 0x60};
uint8_t slave2MAC[] = {0x20, 0x9B, 0xA9, 0xB1, 0xC4, 0x80};

// Struktur pesan (harus sama dengan di slave)
typedef struct {
  uint8_t id;        // ID pengirim (1 atau 2)
  char text[200];    // isi pesan
} Pesan;

Pesan pesanMasuk;

const char* namaPengirim(const uint8_t *mac) {
  if (memcmp(mac, slave1MAC, 6) == 0) return "Slave 1";
  if (memcmp(mac, slave2MAC, 6) == 0) return "Slave 2";
  return "Tidak dikenal";
}

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
  memcpy(&pesanMasuk, data, sizeof(Pesan));
  pesanMasuk.text[sizeof(pesanMasuk.text) - 1] = '\0';

  Serial.printf("[%s | %02X:%02X:%02X:%02X:%02X:%02X] %s\n",
                namaPengirim(mac),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                pesanMasuk.text);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  Serial.print("MAC Master: ");
  Serial.println(WiFi.macAddress());   // harus cc:7b:5c:f2:2e:e0

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Master siap menerima pesan...");
}

void loop() {
  // Master hanya menunggu pesan masuk (ditangani callback)
}
