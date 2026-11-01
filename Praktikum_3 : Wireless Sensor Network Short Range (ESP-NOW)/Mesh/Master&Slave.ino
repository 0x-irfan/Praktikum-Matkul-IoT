/*
  ESP-NOW MESH (Flooding Multi-hop) - 3 Node
  Node 0 (Master)  : cc:7b:5c:f2:2e:e0
  Node 1 (Slave 1) : a4:f0:0f:91:95:60
  Node 2 (Slave 2) : 20:9b:a9:b1:c4:80

  Satu kode yang sama di-upload ke ketiga ESP32.
  Peran node ditentukan otomatis dari MAC address board.

  Cara kerja:
  - Setiap node bisa MENGIRIM dan MENERIMA.
  - Pesan dikirim secara broadcast, lalu node yang menerima akan
    meneruskan (relay) ke node lain selama TTL belum habis.
  - ID pesan (asal + nomor urut) dipakai agar pesan yang sama
    tidak diproses/diteruskan berulang kali (anti-loop).

  Serial Monitor (115200 baud, line ending "Newline"):
    "halo semua"  -> ke semua node
    "0:halo"      -> hanya ke Master
    "1:halo"      -> hanya ke Slave 1
    "2:halo"      -> hanya ke Slave 2
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp_mac.h>
#endif

// ================= PENGATURAN =================
#define SIMULASI_TOPOLOGI 0   // 1 = simulasi topologi garis: Slave1 - Master - Slave2
                              //     (Slave 1 dan Slave 2 tidak saling mendengar langsung,
                              //      pesan harus lewat Master -> bukti multi-hop)
#define TTL_AWAL 3            // batas maksimum hop
// ==============================================

#define JUMLAH_NODE 3
#define SEMUA 0xFF

const uint8_t NODE_MAC[JUMLAH_NODE][6] = {
  {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0},   // 0 Master
  {0xA4, 0xF0, 0x0F, 0x91, 0x95, 0x60},   // 1 Slave 1
  {0x20, 0x9B, 0xA9, 0xB1, 0xC4, 0x80}    // 2 Slave 2
};
const char* NODE_NAMA[JUMLAH_NODE] = {"Master", "Slave 1", "Slave 2"};

uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Struktur pesan mesh
typedef struct __attribute__((packed)) {
  uint8_t  origin;    // ID node asal (0..2)
  uint8_t  dest;      // ID node tujuan, atau SEMUA
  uint16_t seq;       // nomor urut pesan dari node asal
  uint8_t  ttl;       // sisa hop
  char     text[180]; // isi pesan
} MeshMsg;

// Paket yang diterima (disimpan dulu di buffer, diproses di loop)
typedef struct {
  uint8_t mac[6];     // MAC pengirim langsung (hop sebelumnya)
  MeshMsg msg;
} Paket;

#define RX_BUF 8
Paket rxBuf[RX_BUF];
volatile uint8_t rxHead = 0, rxTail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Cache anti-duplikat
#define SEEN_N 32
struct { uint8_t origin; uint16_t seq; bool valid; } seen[SEEN_N];
uint8_t seenIdx = 0;

int myId = -1;
uint16_t mySeq = 0;

// ---------- Fungsi bantu ----------
int idDariMac(const uint8_t *mac) {
  for (int i = 0; i < JUMLAH_NODE; i++) {
    if (memcmp(mac, NODE_MAC[i], 6) == 0) return i;
  }
  return -1;
}

bool sudahDilihat(uint8_t origin, uint16_t seq) {
  for (int i = 0; i < SEEN_N; i++) {
    if (seen[i].valid && seen[i].origin == origin && seen[i].seq == seq) return true;
  }
  return false;
}

void tandaiDilihat(uint8_t origin, uint16_t seq) {
  seen[seenIdx] = {origin, seq, true};
  seenIdx = (seenIdx + 1) % SEEN_N;
}

// Simulasi topologi: node hanya "mendengar" tetangga tertentu
bool bolehDengar(int dariId) {
#if SIMULASI_TOPOLOGI
  if (myId == 0) return true;   // Master mendengar semua
  return dariId == 0;           // Slave hanya mendengar Master
#else
  return true;
#endif
}

void kirimBroadcast(MeshMsg &m) {
  esp_err_t r = esp_now_send(BROADCAST_MAC, (uint8_t *)&m, sizeof(m));
  if (r != ESP_OK) Serial.println("   Error saat mengirim data");
}

// ---------- Callback terima ----------
// Core v3.x : (const esp_now_recv_info_t *info, const uint8_t *data, int len)
// Core v2.x : (const uint8_t *mac, const uint8_t *data, int len)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(MeshMsg)) return;

  portENTER_CRITICAL(&mux);
  uint8_t next = (rxHead + 1) % RX_BUF;
  if (next != rxTail) {
    memcpy(rxBuf[rxHead].mac, mac, 6);
    memcpy(&rxBuf[rxHead].msg, data, sizeof(MeshMsg));
    rxHead = next;
  }
  portEXIT_CRITICAL(&mux);
}

// ---------- Proses paket masuk ----------
void prosesPaket(Paket &p) {
  MeshMsg &m = p.msg;
  m.text[sizeof(m.text) - 1] = '\0';

  int dariId = idDariMac(p.mac);
  if (dariId < 0) return;                    // bukan node kita
  if (!bolehDengar(dariId)) return;          // simulasi topologi
  if (m.origin >= JUMLAH_NODE) return;
  if (m.origin == myId) return;              // pesan sendiri yang kembali
  if (sudahDilihat(m.origin, m.seq)) return; // duplikat
  tandaiDilihat(m.origin, m.seq);

  bool untukSaya = (m.dest == SEMUA || m.dest == myId);

  if (untukSaya) {
    int hop = TTL_AWAL - m.ttl + 1;
    Serial.printf("[dari %s | lewat %s | hop %d] %s\n",
                  NODE_NAMA[m.origin], NODE_NAMA[dariId], hop, m.text);
  }

  // Teruskan (relay) jika bukan khusus untuk saya dan TTL masih ada
  if (m.dest != myId && m.ttl > 1) {
    m.ttl--;
    delay(random(5, 30));   // jeda acak agar relay tidak tabrakan
    kirimBroadcast(m);
    Serial.printf("   (meneruskan pesan dari %s)\n", NODE_NAMA[m.origin]);
  }
}

// ---------- Kirim pesan baru dari Serial ----------
void kirimPesan(uint8_t dest, const String &isi) {
  MeshMsg m;
  memset(&m, 0, sizeof(m));
  m.origin = myId;
  m.dest = dest;
  m.seq = ++mySeq;
  m.ttl = TTL_AWAL;
  isi.toCharArray(m.text, sizeof(m.text));

  tandaiDilihat(m.origin, m.seq);   // supaya tidak diproses ulang bila kembali
  kirimBroadcast(m);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);

  // Baca MAC langsung dari eFuse (WiFi.macAddress() bisa 00:00:... jika
  // dipanggil sebelum stack Wi-Fi siap)
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  myId = idDariMac(mac);

  Serial.printf("MAC board ini: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (myId < 0) {
    Serial.println("MAC tidak ada di daftar NODE_MAC! Periksa alamat MAC.");
    while (true) delay(1000);
  }
  Serial.printf("Peran: Node %d (%s)\n", myId, NODE_NAMA[myId]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW gagal diinisialisasi");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onDataRecv);

  // Peer broadcast
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Gagal menambahkan peer broadcast");
    while (true) delay(1000);
  }

  randomSeed(esp_random());

#if SIMULASI_TOPOLOGI
  Serial.println("Mode SIMULASI topologi garis: Slave1 - Master - Slave2");
#endif
  Serial.println("Ketik pesan lalu Enter. Format: 'teks' (semua), '0:teks', '1:teks', '2:teks'");
}

void loop() {
  // 1) Proses semua paket yang masuk
  while (rxTail != rxHead) {
    Paket p;
    portENTER_CRITICAL(&mux);
    memcpy(&p, &rxBuf[rxTail], sizeof(Paket));
    rxTail = (rxTail + 1) % RX_BUF;
    portEXIT_CRITICAL(&mux);
    prosesPaket(p);
  }

  // 2) Baca teks dari Serial Monitor
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    uint8_t dest = SEMUA;
    String isi = input;

    if (input.length() >= 2 && input[1] == ':' && input[0] >= '0' && input[0] < '0' + JUMLAH_NODE) {
      dest = input[0] - '0';
      isi = input.substring(2);
      isi.trim();
    }

    if (dest == myId) {
      Serial.println("Tujuan adalah node ini sendiri, pesan tidak dikirim.");
      return;
    }

    if (dest == SEMUA) Serial.println("Kirim ke semua: " + isi);
    else Serial.printf("Kirim ke %s: %s\n", NODE_NAMA[dest], isi.c_str());

    kirimPesan(dest, isi);
  }
}
