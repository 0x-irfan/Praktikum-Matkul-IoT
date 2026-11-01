/*
  ESP-NOW Many to One - SLAVE (Pengirim)
  Satu kode untuk Slave 1 dan Slave 2.
  Ubah SLAVE_ID menjadi 1 atau 2 sebelum upload.

  Ketik teks di Serial Monitor (115200 baud, newline aktif),
  tekan Enter -> pesan dikirim ke Master.
*/

#include <WiFi.h>
#include <esp_now.h>

// ====== UBAH DI SINI: 1 untuk Slave 1, 2 untuk Slave 2 ======
#define SLAVE_ID 1
// =============================================================

// MAC address master
uint8_t masterMAC[] = {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0};

// Struktur pesan (harus sama dengan di master)
typedef struct {
  uint8_t id;
  char text[200];
} Pesan;

Pesan pesanKirim;
esp_now_peer_info_t peerInfo;

// Callback status pengiriman
// Core v3.3+ memakai wifi_tx_info_t, versi lebih lama memakai MAC address
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "-> Terkirim" : "-> GAGAL terkirim");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  Serial.print("MAC Slave ");
  Serial.print(SLAVE_ID);
  Serial.print(": ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  // Daftarkan master sebagai peer
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, masterMAC, 6);
  peerInfo.channel = 0;      // 0 = ikut channel saat ini (sama di semua board)
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Gagal menambahkan peer master");
    return;
  }

  Serial.println("Ketik pesan lalu tekan Enter untuk mengirim ke Master:");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    pesanKirim.id = SLAVE_ID;
    input.toCharArray(pesanKirim.text, sizeof(pesanKirim.text));

    Serial.print("Kirim: ");
    Serial.println(pesanKirim.text);

    esp_err_t hasil = esp_now_send(masterMAC, (uint8_t *)&pesanKirim, sizeof(pesanKirim));
    if (hasil != ESP_OK) {
      Serial.println("Error saat mengirim data");
    }
  }
}
