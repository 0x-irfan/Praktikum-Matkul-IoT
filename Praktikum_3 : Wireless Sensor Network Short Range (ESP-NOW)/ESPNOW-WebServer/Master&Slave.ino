/*
  ESP-NOW MESH + WI-FI ROUTER (IP TERSENDIRI) + WEB SERVER - 3 Node
  Node 0 (Master)  : cc:7b:5c:f2:2e:e0
  Node 1 (Slave 1) : a4:f0:0f:91:95:60
  Node 2 (Slave 2) : 20:9b:a9:b1:c4:80

  Satu kode yang sama di-upload ke ketiga ESP32 (peran ditentukan dari MAC).

  Setiap node:
  - Tersambung ke Wi-Fi router "Kontrakan 2,4" dan mendapat IP sendiri:
      Master  -> IP berakhiran .101  (esp32-master.local)
      Slave 1 -> IP berakhiran .102  (esp32-slave1.local)
      Slave 2 -> IP berakhiran .103  (esp32-slave2.local)
      (tiga angka pertama otomatis mengikuti subnet router, lihat MODE_IP)
  - Tetap memancarkan Access Point cadangan (SSID ESP32-Master dst.,
      http://192.168.4.1) bila AKTIFKAN_AP = 1
  - Halaman web (alamat IP di atas) berisi:
      * informasi perangkat (SSID, IP, MAC, channel, uptime, heap, dll.)
      * status tetangga mesh (terakhir terdengar, RSSI)
      * terminal yang mencerminkan Serial Monitor + kolom kirim pesan
  - Tetap berkomunikasi mesh lewat ESP-NOW (flooding multi-hop)

  Serial Monitor (115200 baud, line ending "Newline"):
    "halo semua"  -> ke semua node
    "0:halo" / "1:halo" / "2:halo" -> ke node tertentu

  Tidak perlu library tambahan (WebServer sudah bawaan core ESP32).
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_now.h>
#include <esp_system.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp_mac.h>
#endif

// ================= PENGATURAN =================
#define SIMULASI_TOPOLOGI 0   // 1 = simulasi topologi garis: Slave1 - Master - Slave2
#define TTL_AWAL 3            // batas maksimum hop

// --- Wi-Fi router ---
#define WIFI_SSID "Kontrakan 2,4"
#define WIFI_PASS "K0NTR4K4N"

// Mode IP:
//   0 = DHCP (IP dari router, bisa berubah)
//   1 = IP statis OTOMATIS: tiga angka pertama mengikuti subnet router (/24),
//       angka terakhir diambil dari IP_AKHIR di bawah (disarankan)
//   2 = IP statis MANUAL: isi IP_MANUAL, GATEWAY_MANUAL, SUBNET_MANUAL di bawah
#define MODE_IP 1

// --- Access Point cadangan ---
#define AKTIFKAN_AP 1         // 1 = AP tetap aktif, 0 = nonaktif
#define WIFI_CHANNEL 1        // channel AP bila router belum tersambung
                              // (setelah tersambung, channel otomatis ikut router,
                              //  sehingga ESP-NOW tetap satu channel di semua node)
#define AP_PASS "12345678"    // minimal 8 karakter
#define HB_MS 5000            // interval heartbeat (ms)
// ==============================================

#define JUMLAH_NODE 3
#define SEMUA 0xFF
#define HEARTBEAT 0xFE

const uint8_t NODE_MAC[JUMLAH_NODE][6] = {
  {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0},   // 0 Master
  {0xA4, 0xF0, 0x0F, 0x91, 0x95, 0x60},   // 1 Slave 1
  {0x20, 0x9B, 0xA9, 0xB1, 0xC4, 0x80}    // 2 Slave 2
};
const char *NODE_NAMA[JUMLAH_NODE] = {"Master", "Slave 1", "Slave 2"};
const char *NODE_SSID[JUMLAH_NODE] = {"ESP32-Master", "ESP32-Slave1", "ESP32-Slave2"};

uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Hostname (akses lewat http://esp32-master.local dst.)
const char *NODE_HOST[JUMLAH_NODE] = {"esp32-master", "esp32-slave1", "esp32-slave2"};

// MODE_IP 1: angka terakhir IP tiap node (pilih di luar rentang DHCP router)
const uint8_t IP_AKHIR[JUMLAH_NODE] = {101, 102, 103};

// MODE_IP 2: isi sesuai jaringan router kamu
const IPAddress IP_MANUAL[JUMLAH_NODE] = {
  IPAddress(192, 168, 1, 101),   // Master
  IPAddress(192, 168, 1, 102),   // Slave 1
  IPAddress(192, 168, 1, 103)    // Slave 2
};
const IPAddress GATEWAY_MANUAL(192, 168, 1, 1);
const IPAddress SUBNET_MANUAL(255, 255, 255, 0);
const IPAddress DNS_MANUAL(8, 8, 8, 8);

// Struktur pesan mesh
typedef struct __attribute__((packed)) {
  uint8_t  origin;    // ID node asal (0..2)
  uint8_t  dest;      // ID node tujuan, SEMUA, atau HEARTBEAT
  uint16_t seq;       // nomor urut pesan dari node asal
  uint8_t  ttl;       // sisa hop
  char     text[180]; // isi pesan
} MeshMsg;

// Paket yang diterima (disimpan dulu di buffer, diproses di loop)
typedef struct {
  uint8_t mac[6];     // MAC pengirim langsung (hop sebelumnya)
  int8_t  rssi;       // kekuatan sinyal (0 = tidak tersedia)
  MeshMsg msg;
} Paket;

#define RX_BUF 8
Paket rxBuf[RX_BUF];
volatile uint8_t rxHead = 0, rxTail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Cache anti-duplikat
#define SEEN_N 32
typedef struct { uint8_t origin; uint16_t seq; bool valid; } SeenEntry;
SeenEntry seen[SEEN_N];
uint8_t seenIdx = 0;

// Log untuk terminal web (mencerminkan Serial Monitor)
#define LOG_N 80
#define LOG_LEN 160
char logBuf[LOG_N][LOG_LEN];
uint32_t logSeq = 0;   // jumlah total baris yang pernah dicatat

// Status node
int myId = -1;
uint16_t mySeq = 0;
uint8_t staMac[6], apMac[6];
uint32_t txCount = 0, rxCount = 0, relayCount = 0;
bool heard[JUMLAH_NODE] = {false, false, false};
uint32_t lastHeardMs[JUMLAH_NODE] = {0, 0, 0};
int8_t lastRssi[JUMLAH_NODE] = {0, 0, 0};

WebServer server(80);
bool wifiOk = false;   // status koneksi ke router

// ================= HALAMAN WEB =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="id"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Mesh</title>
<style>
:root{--bg:#0f1419;--card:#18202a;--bd:#263241;--tx:#e6edf3;--mut:#8b9bb0;--ac:#3ea6ff;--ok:#3fb950;--bad:#f85149}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 system-ui,-apple-system,Segoe UI,sans-serif}
header{padding:16px 18px;border-bottom:1px solid var(--bd);display:flex;align-items:center;gap:12px;flex-wrap:wrap}
h1{font-size:19px;margin:0}
.badge{background:var(--ac);color:#04121f;border-radius:999px;padding:2px 10px;font-size:12px;font-weight:600}
.st{margin-left:auto;color:var(--mut);font-size:13px}
main{max-width:980px;margin:0 auto;padding:16px;display:grid;gap:18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}
.card{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:12px}
.k{color:var(--mut);font-size:12px}
.v{font-size:16px;font-weight:600;word-break:break-all;margin-top:2px}
h2{font-size:13px;margin:0 0 8px;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.box{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:12px;overflow-x:auto}
table{width:100%;border-collapse:collapse}
th,td{text-align:left;padding:7px 8px;border-bottom:1px solid var(--bd);font-size:14px;white-space:nowrap}
th{color:var(--mut);font-weight:500}
tr:last-child td{border-bottom:0}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:var(--bad)}
.dot.on{background:var(--ok)}
#term{background:#000;color:#7ee787;font:13px/1.5 ui-monospace,Menlo,Consolas,monospace;height:320px;overflow:auto;padding:10px;border-radius:8px;white-space:pre-wrap;word-break:break-word}
.row{display:flex;gap:8px;margin-top:8px;flex-wrap:wrap;align-items:center}
select,input,button{background:#0b1016;color:var(--tx);border:1px solid var(--bd);border-radius:8px;padding:9px 10px;font:inherit}
input[type=text]{flex:1;min-width:140px}
button{background:var(--ac);color:#04121f;border:0;font-weight:600;cursor:pointer}
button.sec{background:#263241;color:var(--tx)}
label{color:var(--mut);font-size:13px;display:flex;align-items:center;gap:6px}
</style></head><body>
<header>
  <h1 id="nm">ESP32 Mesh</h1><span class="badge" id="role">-</span>
  <span class="st"><span class="dot" id="conn"></span><span id="cs">menghubungkan...</span></span>
</header>
<main>
  <section><h2>Informasi Perangkat</h2><div class="grid" id="cards"></div></section>
  <section><h2>Tetangga Mesh</h2>
    <div class="box"><table>
      <thead><tr><th>Node</th><th>MAC</th><th>Status</th><th>Terakhir terdengar</th><th>RSSI</th></tr></thead>
      <tbody id="nb"></tbody>
    </table></div>
  </section>
  <section><h2>Terminal (Serial Monitor)</h2>
    <div id="term"></div>
    <div class="row">
      <select id="to"><option value="all">Semua node</option></select>
      <input type="text" id="msg" placeholder="Ketik pesan lalu Enter..." maxlength="170" autocomplete="off">
      <button id="snd">Kirim</button>
    </div>
    <div class="row">
      <button class="sec" id="clr">Bersihkan</button>
      <label><input type="checkbox" id="auto" checked> Auto-scroll</label>
    </div>
  </section>
</main>
<script>
const $=id=>document.getElementById(id);
const term=$('term');
let after=0, built=false;

function fmtUp(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return (h?h+'j ':'')+m+'m '+(s%60)+'d';}
function card(k,v){return '<div class="card"><div class="k">'+k+'</div><div class="v">'+v+'</div></div>';}
function setConn(ok){
  $('conn').className='dot'+(ok?' on':'');
  $('cs').textContent=ok?'terhubung':'terputus';
}

async function info(){
  try{
    const d=await (await fetch('/info')).json();
    $('nm').textContent=d.name;
    $('role').textContent='Node '+d.id;
    document.title=d.name+' - ESP32 Mesh';
    $('cards').innerHTML=
      card('Wi-Fi router',d.wok?d.wssid:'tidak terhubung')+card('IP LAN (web server)',d.wok?d.wip:'-')+
      card('Gateway',d.wok?d.wgw:'-')+card('Sinyal ke router',d.wok?d.wrssi+' dBm':'-')+
      card('Hostname',d.host+'.local')+card('AP cadangan',d.apon?d.ssid+' ('+d.ip+')':'nonaktif')+
      card('MAC Wi-Fi (STA)',d.stamac)+card('MAC AP',d.apmac)+
      card('Channel',d.ch)+card('Klien terhubung ke AP',d.cl)+
      card('Uptime',fmtUp(d.up))+card('Heap bebas',(d.heap/1024).toFixed(1)+' KB')+
      card('Pesan dikirim',d.tx)+card('Pesan diterima',d.rx)+
      card('Pesan diteruskan',d.rl)+card('TTL / Simulasi',d.ttl+' hop / '+(d.sim?'aktif':'mati'));
    if(!built){
      d.nb.forEach(n=>{const o=document.createElement('option');o.value=n.id;o.textContent=n.n;$('to').appendChild(o);});
      built=true;
    }
    $('nb').innerHTML=d.nb.map(n=>{
      const on=n.ago>=0&&n.ago<=15;
      const st=n.ago<0?'belum terdengar':(on?'terhubung':'tidak terdengar');
      return '<tr><td>'+n.n+'</td><td>'+n.mac+'</td><td><span class="dot'+(on?' on':'')+'"></span>'+st+
             '</td><td>'+(n.ago<0?'-':n.ago+' d lalu')+'</td><td>'+(n.rssi?n.rssi+' dBm':'-')+'</td></tr>';
    }).join('');
    setConn(true);
  }catch(e){setConn(false);}
}

async function logPoll(){
  try{
    const t=await (await fetch('/log?after='+after)).text();
    const L=t.split('\n');
    const seq=parseInt(L[0]);
    if(isNaN(seq))return;
    if(seq<after)term.textContent='';           // perangkat restart
    const lines=L.slice(1).filter(x=>x.length);
    if(lines.length){
      term.textContent+=lines.join('\n')+'\n';
      if(term.textContent.length>24000)term.textContent=term.textContent.slice(-16000);
      if($('auto').checked)term.scrollTop=term.scrollHeight;
    }
    after=seq;
  }catch(e){}
}

async function kirim(){
  const msg=$('msg').value.trim();
  if(!msg)return;
  $('msg').value='';
  try{
    await fetch('/send',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:new URLSearchParams({to:$('to').value,msg:msg})});
  }catch(e){}
  logPoll();
}

$('snd').onclick=kirim;
$('msg').addEventListener('keydown',e=>{if(e.key==='Enter')kirim();});
$('clr').onclick=()=>{term.textContent='';};
info();logPoll();
setInterval(info,2000);
setInterval(logPoll,700);
</script></body></html>)rawliteral";

// ================= LOG / SERIAL =================
void logAdd(const char *s) {
  char *dst = logBuf[logSeq % LOG_N];
  strncpy(dst, s, LOG_LEN - 1);
  dst[LOG_LEN - 1] = '\0';
  for (char *p = dst; *p; p++) {
    if (*p == '\n' || *p == '\r') *p = ' ';
  }
  logSeq++;
}

// Cetak ke Serial Monitor sekaligus catat untuk terminal web
void catat(const char *fmt, ...) {
  char tmp[LOG_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  Serial.println(tmp);
  logAdd(tmp);
}

// ================= FUNGSI BANTU =================
int idDariMac(const uint8_t *mac) {
  for (int i = 0; i < JUMLAH_NODE; i++) {
    if (memcmp(mac, NODE_MAC[i], 6) == 0) return i;
  }
  return -1;
}

String macToStr(const uint8_t *mac) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(b);
}

bool sudahDilihat(uint8_t origin, uint16_t seq) {
  for (int i = 0; i < SEEN_N; i++) {
    if (seen[i].valid && seen[i].origin == origin && seen[i].seq == seq) return true;
  }
  return false;
}

void tandaiDilihat(uint8_t origin, uint16_t seq) {
  seen[seenIdx].origin = origin;
  seen[seenIdx].seq = seq;
  seen[seenIdx].valid = true;
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
  if (r != ESP_OK) catat("   Error saat mengirim data");
}

void kirimHeartbeat() {
  MeshMsg m;
  memset(&m, 0, sizeof(m));
  m.origin = myId;
  m.dest = HEARTBEAT;
  m.ttl = 1;
  esp_now_send(BROADCAST_MAC, (uint8_t *)&m, sizeof(m));
}

// ================= CALLBACK TERIMA =================
// Core v3.x : (const esp_now_recv_info_t *info, const uint8_t *data, int len)
// Core v2.x : (const uint8_t *mac, const uint8_t *data, int len)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
  int8_t rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  int8_t rssi = 0;
#endif
  if (len != sizeof(MeshMsg)) return;

  portENTER_CRITICAL(&mux);
  uint8_t next = (rxHead + 1) % RX_BUF;
  if (next != rxTail) {
    memcpy(rxBuf[rxHead].mac, mac, 6);
    rxBuf[rxHead].rssi = rssi;
    memcpy(&rxBuf[rxHead].msg, data, sizeof(MeshMsg));
    rxHead = next;
  }
  portEXIT_CRITICAL(&mux);
}

// ================= PROSES PAKET MASUK =================
void prosesPaket(Paket &p) {
  MeshMsg &m = p.msg;
  m.text[sizeof(m.text) - 1] = '\0';

  int dariId = idDariMac(p.mac);
  if (dariId < 0) return;                    // bukan node kita
  if (!bolehDengar(dariId)) return;          // simulasi topologi
  if (m.origin >= JUMLAH_NODE) return;
  if (m.origin == myId) return;              // pesan sendiri yang kembali

  // Catat tetangga langsung (hop sebelumnya) untuk tabel di web
  heard[dariId] = true;
  lastHeardMs[dariId] = millis();
  lastRssi[dariId] = p.rssi;

  if (m.dest == HEARTBEAT) return;           // heartbeat tidak diproses lebih lanjut
  if (sudahDilihat(m.origin, m.seq)) return; // duplikat
  tandaiDilihat(m.origin, m.seq);

  bool untukSaya = (m.dest == SEMUA || m.dest == myId);

  if (untukSaya) {
    rxCount++;
    int hop = TTL_AWAL - m.ttl + 1;
    catat("[dari %s | lewat %s | hop %d] %s",
          NODE_NAMA[m.origin], NODE_NAMA[dariId], hop, m.text);
  }

  // Teruskan (relay) jika bukan khusus untuk saya dan TTL masih ada
  if (m.dest != myId && m.ttl > 1) {
    m.ttl--;
    delay(random(5, 30));   // jeda acak agar relay tidak tabrakan
    kirimBroadcast(m);
    relayCount++;
    catat("   (meneruskan pesan dari %s)", NODE_NAMA[m.origin]);
  }
}

// ================= KIRIM PESAN =================
void kirimKeTujuan(uint8_t dest, const String &isi) {
  if (dest != SEMUA && dest >= JUMLAH_NODE) {
    catat("Tujuan tidak valid.");
    return;
  }
  if (dest == myId) {
    catat("Tujuan adalah node ini sendiri, pesan tidak dikirim.");
    return;
  }

  if (dest == SEMUA) catat("Kirim ke semua: %s", isi.c_str());
  else catat("Kirim ke %s: %s", NODE_NAMA[dest], isi.c_str());

  MeshMsg m;
  memset(&m, 0, sizeof(m));
  m.origin = myId;
  m.dest = dest;
  m.seq = ++mySeq;
  m.ttl = TTL_AWAL;
  isi.toCharArray(m.text, sizeof(m.text));

  tandaiDilihat(m.origin, m.seq);   // supaya tidak diproses ulang bila kembali
  kirimBroadcast(m);
  txCount++;
}

// Format dari Serial Monitor: "teks", "0:teks", "1:teks", "2:teks"
void prosesInputSerial(String input) {
  input.trim();
  if (input.length() == 0) return;

  uint8_t dest = SEMUA;
  String isi = input;

  if (input.length() >= 2 && input[1] == ':' && input[0] >= '0' && input[0] < '0' + JUMLAH_NODE) {
    dest = input[0] - '0';
    isi = input.substring(2);
    isi.trim();
  }
  if (isi.length() == 0) return;
  kirimKeTujuan(dest, isi);
}

// ================= HANDLER WEB =================
void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleInfo() {
  String j = "{";
  j += "\"name\":\"" + String(NODE_NAMA[myId]) + "\",";
  j += "\"id\":" + String(myId) + ",";
  j += "\"ssid\":\"" + String(NODE_SSID[myId]) + "\",";
  j += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  bool wok = (WiFi.status() == WL_CONNECTED);
  j += "\"wok\":" + String(wok ? 1 : 0) + ",";
  j += "\"wssid\":\"" + String(WIFI_SSID) + "\",";
  j += "\"wip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"wgw\":\"" + WiFi.gatewayIP().toString() + "\",";
  j += "\"wrssi\":" + String(wok ? WiFi.RSSI() : 0) + ",";
  j += "\"host\":\"" + String(NODE_HOST[myId]) + "\",";
  j += "\"apon\":" + String(AKTIFKAN_AP) + ",";
  j += "\"apmac\":\"" + macToStr(apMac) + "\",";
  j += "\"stamac\":\"" + macToStr(staMac) + "\",";
  j += "\"ch\":" + String(WiFi.channel()) + ",";
  j += "\"cl\":" + String(WiFi.softAPgetStationNum()) + ",";
  j += "\"up\":" + String(millis() / 1000) + ",";
  j += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"tx\":" + String(txCount) + ",";
  j += "\"rx\":" + String(rxCount) + ",";
  j += "\"rl\":" + String(relayCount) + ",";
  j += "\"ttl\":" + String(TTL_AWAL) + ",";
  j += "\"sim\":" + String(SIMULASI_TOPOLOGI) + ",";
  j += "\"nb\":[";
  bool first = true;
  for (int i = 0; i < JUMLAH_NODE; i++) {
    if (i == myId) continue;
    if (!first) j += ",";
    first = false;
    long ago = heard[i] ? (long)((millis() - lastHeardMs[i]) / 1000) : -1;
    j += "{\"id\":" + String(i) + ",\"n\":\"" + String(NODE_NAMA[i]) + "\",\"mac\":\"" +
         macToStr(NODE_MAC[i]) + "\",\"ago\":" + String(ago) +
         ",\"rssi\":" + String((int)lastRssi[i]) + "}";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// Baris pertama = nomor urut terakhir, sisanya baris log baru
void handleLog() {
  uint32_t after = (uint32_t)server.arg("after").toInt();
  uint32_t oldest = (logSeq > LOG_N) ? (logSeq - LOG_N) : 0;
  if (after < oldest || after > logSeq) after = oldest;

  String out;
  out.reserve(512);
  out += String(logSeq);
  out += "\n";
  for (uint32_t i = after; i < logSeq; i++) {
    out += logBuf[i % LOG_N];
    out += "\n";
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", out);
}

void handleSend() {
  String to = server.arg("to");
  String msg = server.arg("msg");
  msg.trim();
  if (msg.length() == 0) {
    server.send(400, "text/plain", "pesan kosong");
    return;
  }
  uint8_t dest = (to == "all" || to.length() == 0) ? SEMUA : (uint8_t)to.toInt();
  kirimKeTujuan(dest, msg);
  server.send(200, "text/plain", "ok");
}

// ================= KONEKSI WI-FI ROUTER =================
bool tungguWiFi(uint32_t ms) {
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < ms) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

void sambungkanWiFi() {
  WiFi.setHostname(NODE_HOST[myId]);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

#if MODE_IP == 2
  // IP statis manual
  WiFi.config(IP_MANUAL[myId], GATEWAY_MANUAL, SUBNET_MANUAL, DNS_MANUAL);
#endif

  catat("Menyambung ke Wi-Fi \"%s\" ...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  bool ok = tungguWiFi(20000);

#if MODE_IP == 1
  if (ok) {
    // IP statis otomatis: ikut subnet router, hanya angka terakhir yang diganti
    IPAddress gw = WiFi.gatewayIP();
    IPAddress mask = WiFi.subnetMask();
    IPAddress dns = WiFi.dnsIP();

    if (mask != IPAddress(255, 255, 255, 0)) {
      catat("Subnet router bukan /24 (%s), tetap memakai DHCP.", mask.toString().c_str());
    } else {
      IPAddress target(gw[0], gw[1], gw[2], IP_AKHIR[myId]);
      if (WiFi.localIP() != target) {
        catat("Mengatur IP statis %s ...", target.toString().c_str());
        WiFi.disconnect();
        delay(300);
        WiFi.config(target, gw, mask, dns);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        ok = tungguWiFi(20000);

        if (!ok || WiFi.localIP() != target) {
          // Gagal (mis. IP bentrok) -> kembali ke DHCP
          catat("IP statis gagal, kembali ke DHCP.");
          WiFi.disconnect();
          delay(300);
          WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
          WiFi.begin(WIFI_SSID, WIFI_PASS);
          ok = tungguWiFi(20000);
        }
      }
    }
  }
#endif

  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);   // agar ESP-NOW dan web server tetap responsif
  wifiOk = ok;

  if (ok) {
    catat("Wi-Fi terhubung | IP: %s | Gateway: %s | Sinyal: %d dBm | Channel: %d",
          WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
          WiFi.RSSI(), WiFi.channel());
  } else {
    catat("Gagal tersambung ke Wi-Fi, akan dicoba lagi otomatis.");
  }
}

// ================= SETUP & LOOP =================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(1000);

  WiFi.mode(WIFI_AP_STA);

  // Baca MAC langsung dari eFuse (WiFi.macAddress() bisa 00:00:... bila terlalu dini)
  esp_read_mac(staMac, ESP_MAC_WIFI_STA);
  esp_read_mac(apMac, ESP_MAC_WIFI_SOFTAP);
  myId = idDariMac(staMac);

  catat("MAC board ini: %s", macToStr(staMac).c_str());

  if (myId < 0) {
    catat("MAC tidak ada di daftar NODE_MAC! Periksa alamat MAC.");
    while (true) delay(1000);
  }
  catat("Peran: Node %d (%s)", myId, NODE_NAMA[myId]);

  // Sambung ke Wi-Fi router (channel radio otomatis ikut router,
  // sama di semua node, sehingga ESP-NOW tetap saling terdengar)
  sambungkanWiFi();

#if AKTIFKAN_AP
  // Access Point cadangan (tetap bisa diakses walau router mati)
  int chAp = (WiFi.status() == WL_CONNECTED) ? WiFi.channel() : WIFI_CHANNEL;
  if (WiFi.softAP(NODE_SSID[myId], AP_PASS, chAp)) {
    catat("AP cadangan aktif - SSID: %s | Password: %s | http://%s",
          NODE_SSID[myId], AP_PASS, WiFi.softAPIP().toString().c_str());
  } else {
    catat("Gagal memulai Access Point");
  }
#endif

  if (esp_now_init() != ESP_OK) {
    catat("ESP-NOW gagal diinisialisasi");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onDataRecv);

  // Peer broadcast
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0;            // 0 = ikut channel saat ini (channel AP)
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    catat("Gagal menambahkan peer broadcast");
    while (true) delay(1000);
  }

  randomSeed(esp_random());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/send", HTTP_POST, handleSend);
  server.begin();
  if (MDNS.begin(NODE_HOST[myId])) {
    MDNS.addService("http", "tcp", 80);
    catat("mDNS aktif: http://%s.local", NODE_HOST[myId]);
  }
  if (WiFi.status() == WL_CONNECTED) {
    catat("Web server berjalan. Buka: http://%s", WiFi.localIP().toString().c_str());
  } else {
    catat("Web server berjalan (router belum tersambung, pakai AP cadangan).");
  }

#if SIMULASI_TOPOLOGI
  catat("Mode SIMULASI topologi garis: Slave1 - Master - Slave2");
#endif
  catat("Ketik pesan lalu Enter. Format: 'teks' (semua), '0:teks', '1:teks', '2:teks'");
}

void loop() {
  server.handleClient();

  // Pantau koneksi router, sambung ulang bila terputus
  static uint32_t lastWifiCheck = 0;
  if (millis() - lastWifiCheck >= 10000) {
    lastWifiCheck = millis();
    bool now = (WiFi.status() == WL_CONNECTED);
    if (now != wifiOk) {
      wifiOk = now;
      if (now) catat("Wi-Fi tersambung kembali | IP: %s", WiFi.localIP().toString().c_str());
      else catat("Wi-Fi router terputus, mencoba menyambung ulang...");
    }
    if (!now) WiFi.reconnect();
  }

  // 1) Proses semua paket yang masuk
  while (rxTail != rxHead) {
    Paket p;
    portENTER_CRITICAL(&mux);
    memcpy(&p, &rxBuf[rxTail], sizeof(Paket));
    rxTail = (rxTail + 1) % RX_BUF;
    portEXIT_CRITICAL(&mux);
    prosesPaket(p);
  }

  // 2) Heartbeat berkala (untuk status tetangga di web)
  static uint32_t lastHb = 0;
  if (millis() - lastHb >= HB_MS) {
    lastHb = millis();
    kirimHeartbeat();
  }

  // 3) Baca teks dari Serial Monitor
  if (Serial.available()) {
    prosesInputSerial(Serial.readStringUntil('\n'));
  }
}
