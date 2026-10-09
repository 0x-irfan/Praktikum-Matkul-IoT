/*
  ESP-NOW TWO WAY (DUA ARAH) - DENGAN DAFTAR MAC
  Dua ESP32 saling kirim dan terima pesan teks lewat Serial Monitor.

  Master  : cc:7b:5c:f2:2e:e0   (node 0)
  Slave 1 : a4:f0:0f:91:95:60   (node 1)
  Slave 2 : 20:9b:a9:b1:c4:80   (node 2)

  Satu kode yang sama dipakai di kedua board.
  Atur PEER_NODE = node LAWAN BICARA sebelum upload. Contoh:
    - Upload ke Master  -> PEER_NODE 1  (lawannya Slave 1)
    - Upload ke Slave 1 -> PEER_NODE 0  (lawannya Master)
    - Upload ke Slave 2 -> PEER_NODE 0  (lawannya Master)

  Serial Monitor: 115200 baud, line ending "Newline".
  Ketik pesan lalu Enter -> dikirim ke lawan.
  Pesan dari lawan otomatis tampil lengkap dengan nama pengirimnya.
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp_mac.h>
#endif

// ====== UBAH DI SINI: node lawan bicara (0 = Master, 1 = Slave 1, 2 = Slave 2) ======
#define PEER_NODE 1
// =====================================================================================

#define JUMLAH_NODE 3

const uint8_t NODE_MAC[JUMLAH_NODE][6] = {
  {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0},   // 0 Master
  {0xA4, 0xF0, 0x0F, 0x91, 0x95, 0x60},   // 1 Slave 1
  {0x20, 0x9B, 0xA9, 0xB1, 0xC4, 0x80}    // 2 Slave 2
};
const char *NODE_NAMA[JUMLAH_NODE] = {"Master", "Slave 1", "Slave 2"};

// Struktur pesan (harus sama di kedua board)
typedef struct {
  char text[200];
} Pesan;

Pesan pesanKirim, pesanMasuk;
const uint8_t *macPeer = NODE_MAC[PEER_NODE];

// Cari nama node dari MAC address
const char *namaDariMac(const uint8_t *mac) {
  for (int i = 0; i < JUMLAH_NODE; i++) {
    if (memcmp(mac, NODE_MAC[i], 6) == 0) return NODE_NAMA[i];
  }
  return "Tidak dikenal";
}

// Callback status pengiriman
// Core v3.3+ memakai wifi_tx_info_t, versi lebih lama memakai MAC address
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "   -> Terkirim" : "   -> GAGAL terkirim");
}

// Callback saat data diterima
// Core v3.x : (const esp_now_recv_info_t *info, const uint8_t *data, int len)
// Core v2.x : (const uint8_t *mac, const uint8_t *data, int len)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(Pesan)) return;
  memcpy(&pesanMasuk, data, sizeof(Pesan));
  pesanMasuk.text[sizeof(pesanMasuk.text) - 1] = '\0';

  Serial.printf("[%s] %s\n", namaDariMac(mac), pesanMasuk.text);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);

  // Baca MAC langsung dari eFuse (WiFi.macAddress() bisa 00:00:... bila terlalu dini)
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("MAC board ini : %02X:%02X:%02X:%02X:%02X:%02X (%s)\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], namaDariMac(mac));
  Serial.printf("Lawan bicara  : %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
                NODE_NAMA[PEER_NODE],
                macPeer[0], macPeer[1], macPeer[2], macPeer[3], macPeer[4], macPeer[5]);

  if (memcmp(mac, macPeer, 6) == 0) {
    Serial.println("PERINGATAN: PEER_NODE sama dengan board ini! Ubah PEER_NODE ke node lawan.");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi");
    while (true) delay(1000);
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Daftarkan lawan bicara sebagai peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, macPeer, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Gagal menambahkan peer");
    while (true) delay(1000);
  }

  Serial.println("Siap. Ketik pesan lalu Enter untuk mengirim:");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    memset(&pesanKirim, 0, sizeof(pesanKirim));
    input.toCharArray(pesanKirim.text, sizeof(pesanKirim.text));

    Serial.print("[Saya] ");
    Serial.println(pesanKirim.text);

    esp_err_t hasil = esp_now_send(macPeer, (uint8_t *)&pesanKirim, sizeof(pesanKirim));
    if (hasil != ESP_OK) Serial.println("   Error saat mengirim data");
  }
}
