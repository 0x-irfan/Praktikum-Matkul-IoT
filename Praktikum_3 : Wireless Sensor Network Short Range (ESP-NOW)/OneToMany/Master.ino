/*
  ESP-NOW One to Many - MASTER (Pengirim)
  Master  : cc:7b:5c:f2:2e:e0
  Slave 1 : a4:f0:0f:91:95:60
  Slave 2 : 20:9b:a9:b1:c4:80

  Ketik teks di Serial Monitor (115200 baud, newline aktif) lalu Enter:
    "halo semua"   -> dikirim ke Slave 1 DAN Slave 2
    "1:halo"       -> dikirim hanya ke Slave 1
    "2:halo"       -> dikirim hanya ke Slave 2
*/

#include <WiFi.h>
#include <esp_now.h>

uint8_t slave1MAC[] = {0xA4, 0xF0, 0x0F, 0x91, 0x95, 0x60};
uint8_t slave2MAC[] = {0x20, 0x9B, 0xA9, 0xB1, 0xC4, 0x80};

// Struktur pesan (harus sama dengan di slave)
typedef struct {
  char text[200];
} Pesan;

Pesan pesanKirim;

// Callback status pengiriman
// Core v3.3+ memakai wifi_tx_info_t, versi lebih lama memakai MAC address
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  const uint8_t *mac = info->des_addr;
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  const char *nama = "?";
  if (memcmp(mac, slave1MAC, 6) == 0) nama = "Slave 1";
  else if (memcmp(mac, slave2MAC, 6) == 0) nama = "Slave 2";

  Serial.printf("   -> %s: %s\n", nama,
                status == ESP_NOW_SEND_SUCCESS ? "Terkirim" : "GAGAL");
}

bool tambahPeer(uint8_t *mac) {
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
}

void kirim(uint8_t *mac, const String &isi) {
  memset(&pesanKirim, 0, sizeof(pesanKirim));
  isi.toCharArray(pesanKirim.text, sizeof(pesanKirim.text));
  esp_err_t hasil = esp_now_send(mac, (uint8_t *)&pesanKirim, sizeof(pesanKirim));
  if (hasil != ESP_OK) Serial.println("   Error saat mengirim data");
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

  esp_now_register_send_cb(onDataSent);

  if (!tambahPeer(slave1MAC)) Serial.println("Gagal menambahkan Slave 1");
  if (!tambahPeer(slave2MAC)) Serial.println("Gagal menambahkan Slave 2");

  Serial.println("Ketik pesan lalu Enter.");
  Serial.println("Format: 'teks' (ke semua), '1:teks' (Slave 1), '2:teks' (Slave 2)");
}

void loop() {
  if (!Serial.available()) return;

  String input = Serial.readStringUntil('\n');
  input.trim();
  if (input.length() == 0) return;

  if (input.startsWith("1:")) {
    String isi = input.substring(2);
    isi.trim();
    Serial.println("Kirim ke Slave 1: " + isi);
    kirim(slave1MAC, isi);
  } else if (input.startsWith("2:")) {
    String isi = input.substring(2);
    isi.trim();
    Serial.println("Kirim ke Slave 2: " + isi);
    kirim(slave2MAC, isi);
  } else {
    Serial.println("Kirim ke semua slave: " + input);
    kirim(slave1MAC, input);
    kirim(slave2MAC, input);
  }
}
