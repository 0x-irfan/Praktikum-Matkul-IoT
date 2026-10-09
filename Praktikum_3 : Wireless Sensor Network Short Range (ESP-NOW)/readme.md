# Praktikum 3 : Wireless Sensor Network Short Range (ESP-NOW)

![ESP32](https://img.shields.io/badge/Board-ESP32-blue)
![Protocol](https://img.shields.io/badge/Protocol-ESP--NOW-orange)
![Framework](https://img.shields.io/badge/Framework-Arduino-00979D)

Repositori ini berisi kumpulan program praktikum **komunikasi nirkabel jarak dekat antar ESP32 menggunakan ESP-NOW**. Praktikum dimulai dari langkah paling dasar (mengecek MAC address), lalu berlanjut ke berbagai pola komunikasi: **Two Ways (PtP)**, **One to Many**, **Many to One**, **Mesh**, dan **ESP-NOW + Web Server**.

Semua komunikasi dilakukan **tanpa router dan tanpa internet** (kecuali pada bagian web server, yang memakai Wi-Fi hanya untuk menampilkan halaman web). Pesan diketik melalui **Serial Monitor**, dikirim lewat udara, lalu ditampilkan di Serial Monitor perangkat lain.

---

## Tujuan Pembelajaran

Setelah menyelesaikan praktikum ini, kamu diharapkan mampu:

1. Menjelaskan apa itu ESP-NOW dan kapan protokol ini cocok dipakai pada Wireless Sensor Network.
2. Mengecek dan memakai **MAC address** ESP32 sebagai alamat tujuan komunikasi.
3. Membuat komunikasi **dua arah**, **satu ke banyak**, dan **banyak ke satu** antar ESP32.
4. Memahami cara kerja **jaringan mesh** sederhana (multi-hop).
5. Menggabungkan ESP-NOW dengan **web server** untuk memantau perangkat lewat browser.

---

## Apa itu ESP-NOW?

**ESP-NOW** adalah protokol komunikasi nirkabel buatan **Espressif** yang memungkinkan beberapa ESP32 saling berkirim data **secara langsung (peer-to-peer)** melalui radio 2,4 GHz. Tidak ada proses *connect* ke access point, tidak ada DHCP, dan tidak ada handshake yang panjang. Perangkat cukup mengetahui **MAC address** lawan bicaranya, lalu langsung mengirim data.

### Perbandingan dengan Wi-Fi biasa

| Aspek | Wi-Fi biasa (TCP/IP) | ESP-NOW |
|---|---|---|
| Butuh router / access point | Ya | **Tidak** |
| Proses koneksi (scan, autentikasi, DHCP) | Ya, memakan waktu | **Tidak ada** (*connectionless*) |
| Latensi | Relatif lebih tinggi | **Rendah** |
| Ukuran data per paket | Besar | **Maksimal 250 byte** |
| Jumlah peer | Banyak klien per router | Hingga 20 peer per perangkat (maksimal 10 bila terenkripsi) |
| Cocok untuk | Web, internet, streaming | Sensor, remote control, pesan singkat antar-board |

### Mengapa cocok untuk Wireless Sensor Network?

- **Hemat daya dan cepat**: tanpa proses koneksi, data bisa langsung dikirim.
- **Mandiri**: jaringan tetap bekerja walau tidak ada router atau internet.
- **Sederhana**: cukup beberapa fungsi API untuk mengirim dan menerima data.
- **Fleksibel**: bisa membentuk pola satu-ke-satu, satu-ke-banyak, banyak-ke-satu, hingga mesh.

### Istilah penting

| Istilah | Penjelasan |
|---|---|
| **MAC address** | Alamat unik 6 byte milik setiap board, contoh `CC:7B:5C:F2:2E:E0`. Dipakai ESP-NOW sebagai "alamat tujuan", mirip nomor telepon. |
| **Peer** | Perangkat lawan bicara yang **didaftarkan** terlebih dulu sebelum kita boleh mengirim data ke MAC-nya. |
| **Callback** | Fungsi yang **dipanggil otomatis** oleh sistem saat suatu kejadian terjadi (data selesai dikirim atau data masuk). |
| **Unicast** | Mengirim ke **satu** MAC tertentu. |
| **Broadcast** | Mengirim ke **semua** perangkat di sekitar memakai MAC `FF:FF:FF:FF:FF:FF`. |
| **Payload** | Isi data yang dikirim. Di praktikum ini berupa `struct` yang berisi teks. |
| **Channel** | Kanal frekuensi Wi-Fi. Perangkat yang saling berkomunikasi harus berada di channel yang sama. |

---

## Kebutuhan Perangkat dan Software

**Perangkat keras**

- 3 buah ESP32 (pada praktikum ini diberi peran **Master**, **Slave 1**, dan **Slave 2**)
- Kabel USB data
- Laptop/PC (idealnya bisa membuka beberapa Serial Monitor sekaligus)

**Perangkat lunak**

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x
- Board package **esp32 by Espressif Systems** (v2.x atau v3.x)
- Driver USB-serial (CP210x atau CH340, sesuai board)

> Tidak ada library tambahan yang perlu diinstal. `WiFi.h`, `esp_now.h`, dan `WebServer.h` sudah termasuk dalam core ESP32.

---

## Langkah Dasar: Cek MAC Address

Sebelum membuat komunikasi apa pun, kita harus tahu **MAC address setiap board**. Tanpa MAC yang benar, pesan tidak akan sampai ke tujuan.

### Mengapa harus cek MAC address?

ESP-NOW tidak memakai IP address. Satu-satunya cara menunjuk "kirim ke siapa" adalah lewat **MAC address**. Setiap board ESP32 punya MAC unik yang sudah tertanam di chip (disimpan di eFuse), sehingga **MAC board kamu pasti berbeda** dengan contoh di README ini.

### Langkah-langkah

1. Buka file [`Cek_MacAddr_ESP32.ino`](Cek_MacAddr_ESP32.ino) di Arduino IDE.
2. Pilih board (misalnya **ESP32 Dev Module**) dan port yang sesuai.
3. Klik **Upload**.
4. Buka **Serial Monitor** dengan baud rate **115200**.
5. Catat **MAC Wi-Fi STA**, karena itulah alamat yang dipakai ESP-NOW.
6. Ulangi untuk setiap board, lalu beri label fisik (misalnya tempel stiker "Master", "Slave 1", "Slave 2") agar tidak tertukar.

### Hasil Serial Monitor

| Master | Slave 1 | Slave 2 |
|:---:|:---:|:---:|
| ![Cek MAC Master](images/cek-mac-master.png) | ![Cek MAC Slave 1](images/cek-mac-slave1.png) | ![Cek MAC Slave 2](images/cek-mac-slave2.png) |
| *Master* | *Slave 1* | *Slave 2* |

### MAC address yang dipakai pada praktikum ini

| Node | Peran | MAC Address (Wi-Fi STA) |
|---|---|---|
| 0 | Master | `CC:7B:5C:F2:2E:E0` |
| 1 | Slave 1 | `A4:F0:0F:91:95:60` |
| 2 | Slave 2 | `20:9B:A9:B1:C4:80` |

### Menulis MAC address di dalam kode

Di program, MAC ditulis sebagai **array 6 byte** berformat heksadesimal. Caranya: tambahkan `0x` di depan setiap pasangan angka, lalu pisahkan dengan koma.

```text
CC:7B:5C:F2:2E:E0   ->   {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0}
```

```cpp
uint8_t masterMAC[] = {0xCC, 0x7B, 0x5C, 0xF2, 0x2E, 0xE0};
```

> **Catatan:** MAC dibaca langsung dari eFuse memakai `esp_read_mac(mac, ESP_MAC_WIFI_STA)`. Memanggil `WiFi.macAddress()` terlalu dini di `setup()` pada beberapa versi core bisa menghasilkan `00:00:00:00:00:00` karena stack Wi-Fi belum siap.

---

## Kerangka Dasar Program ESP-NOW

Semua pola komunikasi di bawah ini memakai langkah yang sama. Bedanya hanya pada **siapa mengirim, siapa menerima, dan siapa yang didaftarkan sebagai peer**.

```cpp
#include <WiFi.h>
#include <esp_now.h>

typedef struct {
  char text[200];          // payload: struct harus SAMA di pengirim dan penerima
} Pesan;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);                       // 1. nyalakan radio Wi-Fi mode Station
  esp_now_init();                            // 2. nyalakan ESP-NOW
  esp_now_register_send_cb(onDataSent);      // 3. daftarkan callback kirim
  esp_now_register_recv_cb(onDataRecv);      // 4. daftarkan callback terima

  esp_now_peer_info_t peerInfo = {};         // 5. daftarkan lawan bicara (peer)
  memcpy(peerInfo.peer_addr, macTujuan, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // 6. kirim data ke MAC tujuan
  esp_now_send(macTujuan, (uint8_t *)&pesan, sizeof(pesan));
}
```

| Langkah | Fungsi | Kegunaan |
|---|---|---|
| 1 | `WiFi.mode(WIFI_STA)` | Menyalakan radio Wi-Fi. ESP-NOW butuh radio ini aktif. |
| 2 | `esp_now_init()` | Menyalakan ESP-NOW. |
| 3 | `esp_now_register_send_cb()` | Menentukan fungsi yang dipanggil setelah pengiriman (sukses atau gagal). |
| 4 | `esp_now_register_recv_cb()` | Menentukan fungsi yang dipanggil saat data masuk. |
| 5 | `esp_now_add_peer()` | Mendaftarkan MAC tujuan. **Wajib** sebelum mengirim unicast ke MAC itu. |
| 6 | `esp_now_send()` | Mengirim data. |

> **Perbedaan versi core:** pada core ESP32 v3.x tanda tangan (*signature*) fungsi callback berbeda dibanding v2.x. Kode di repositori ini menanganinya dengan `#if ESP_ARDUINO_VERSION_MAJOR >= 3`, sehingga bisa dikompilasi di kedua versi.

---

## Pola Komunikasi

Berikut ringkasan lima pola yang dipraktikkan. Kode lengkap tiap pola ada di foldernya masing-masing.

| Pola | Arah data | Folder | Cocok untuk |
|---|---|---|---|
| Two Ways (PtP) | A &harr; B | [`Two Ways or PtP`](Two%20Ways%20or%20PtP/) | Dua perangkat saling berkirim pesan |
| One to Many | 1 pengirim &rarr; banyak penerima | [`OneToMany`](OneToMany/) | Perintah dari satu pusat ke banyak perangkat |
| Many to One | Banyak pengirim &rarr; 1 penerima | [`ManyToOne`](ManyToOne/) | Beberapa sensor melapor ke satu pusat data |
| Mesh | Semua node bisa kirim, terima, dan meneruskan | [`Mesh`](Mesh/) | Memperluas jangkauan lewat multi-hop |
| ESP-NOW + Web Server | ESP-NOW dan HTTP | [`ESPNOW-WebServer`](ESPNOW-WebServer/) | Memantau perangkat lewat browser |

### 1. Two Ways atau PtP (Point to Point)

Dua ESP32 saling mengirim **dan** menerima pesan. Inilah bentuk komunikasi paling dasar.

```mermaid
flowchart LR
    A[ESP32 A] <-->|ESP-NOW| B[ESP32 B]
```

**Cara kerja**

- Setiap board mendaftarkan **MAC lawan** sebagai peer.
- Pengiriman ditangani di `loop()`: teks yang diketik di Serial Monitor dikirim dengan `esp_now_send()`.
- Penerimaan ditangani di callback `onDataRecv()`: dipanggil otomatis saat ada data masuk, lalu pesan dicetak ke Serial Monitor.
- Karena kedua board punya kode kirim **dan** terima yang aktif bersamaan, komunikasi berlangsung dua arah.

**Konsep yang dipelajari:** peer, callback, struct sebagai payload, dan status pengiriman (`Terkirim` atau `GAGAL`).

### 2. One to Many (Satu ke Banyak)

Satu board (**Master**) mengirim pesan ke beberapa board (**Slave 1** dan **Slave 2**). Slave hanya menerima.

```mermaid
flowchart LR
    M[Master pengirim] --> S1[Slave 1]
    M --> S2[Slave 2]
```

**Cara kerja**

- Master mendaftarkan **dua peer** sekaligus (MAC Slave 1 dan MAC Slave 2).
- Teks yang diketik di Serial Monitor Master dikirim ke **semua slave**, atau ke slave tertentu dengan awalan:
  - `halo semua` &rarr; ke Slave 1 **dan** Slave 2
  - `1:halo` &rarr; hanya ke Slave 1
  - `2:halo` &rarr; hanya ke Slave 2
- Slave cukup menjalankan callback penerima dan menampilkan pesan dari Master.
- Master melihat status `Terkirim` atau `GAGAL` **per slave**, sehingga mudah diketahui slave mana yang tidak menerima.

**Konsep yang dipelajari:** mendaftarkan banyak peer, mengirim ke tujuan tertentu atau semua, dan membaca status per tujuan.

### 3. Many to One (Banyak ke Satu)

Beberapa board (**Slave 1** dan **Slave 2**) mengirim pesan ke satu board pusat (**Master**). Pola ini paling mirip dengan **sensor node yang melapor ke sink** pada Wireless Sensor Network.

```mermaid
flowchart LR
    S1[Slave 1] --> M[Master penerima]
    S2[Slave 2] --> M
```

**Cara kerja**

- Setiap slave mendaftarkan **MAC Master** sebagai peer, lalu mengirim pesan yang diketik di Serial Monitor-nya.
- Setiap pesan membawa **identitas pengirim** (ID atau MAC) di dalam `struct`.
- Master tidak perlu mendaftarkan peer untuk **menerima**. Callback `onDataRecv()` membaca **MAC pengirim** dari paket yang masuk, lalu menampilkannya, misalnya `[Slave 1 | A4:F0:...] halo master`.
- Satu kode yang sama dapat dipakai di kedua slave, cukup mengubah ID-nya.

**Konsep yang dipelajari:** mengenali pengirim dari MAC, serta menyertakan identitas di dalam payload.

### 4. Mesh

Setiap node dapat **mengirim, menerima, dan meneruskan (relay)** pesan. Jika dua node tidak saling terjangkau langsung, pesan dapat melompat melalui node lain (**multi-hop**).

```mermaid
flowchart LR
    S1[Slave 1] <--> M[Master] <--> S2[Slave 2]
```

Pada topologi di atas, Slave 1 dan Slave 2 tidak saling mendengar langsung. Pesan Slave 1 ke Slave 2 harus **lewat Master** (2 hop).

**Cara kerja (flooding)**

- Pesan dikirim secara **broadcast** (`FF:FF:FF:FF:FF:FF`), sehingga semua node di sekitar bisa menerimanya.
- Setiap pesan membawa **TTL** (sisa hop). Node yang menerima akan meneruskan pesan sambil mengurangi TTL, dan berhenti saat TTL habis.
- Setiap pesan punya **ID unik** (node asal + nomor urut). Pesan yang sudah pernah dilihat **tidak diproses atau diteruskan lagi**, sehingga tidak terjadi perulangan tanpa henti (*loop*).
- Pesan bisa ditujukan ke **semua node** atau ke **node tertentu**. Node yang bukan tujuan tetap meneruskannya.
- Penerima menampilkan asal pesan, node relay terakhir, dan jumlah hop, misalnya `[dari Slave 1 | lewat Master | hop 2] halo`.
- Satu kode yang sama dipakai di ketiga board. Peran node ditentukan otomatis dari MAC address-nya.
- Tersedia mode **simulasi topologi garis** untuk membuktikan multi-hop walau ketiga board berdekatan.

**Konsep yang dipelajari:** broadcast, TTL, deduplikasi pesan, relay, dan multi-hop.

> **Batasan:** ESP-NOW tidak punya routing bawaan, sehingga mesh di sini berbasis **flooding**. Cocok untuk jaringan kecil, tetapi kurang efisien untuk node yang banyak. Broadcast juga **tidak ada ACK**, jadi pesan bisa hilang bila terjadi tabrakan atau sinyal lemah.

### 5. ESP-NOW + Web Server

Setiap ESP32 tetap berkomunikasi lewat ESP-NOW, **sekaligus** menjalankan web server sehingga dapat dipantau dari browser HP atau laptop.

```mermaid
flowchart LR
    B[Browser] -->|HTTP port 80| E1[ESP32 Master]
    E1 <-->|ESP-NOW| E2[ESP32 Slave 1]
    E1 <-->|ESP-NOW| E3[ESP32 Slave 2]
    R[Router Wi-Fi] --- E1
```

**Fitur halaman web**

- **Informasi perangkat**: SSID, alamat IP, MAC, channel, uptime, memori bebas, serta jumlah pesan terkirim, diterima, dan diteruskan.
- **Status tetangga**: node lain yang terdengar, waktu terakhir terdengar, dan kekuatan sinyal (RSSI).
- **Terminal**: mencerminkan isi **Serial Monitor** secara real-time dan menyediakan kolom untuk mengirim pesan.

**Cara kerja**

- ESP32 memakai mode **`WIFI_AP_STA`**: sebagai Access Point (memancarkan Wi-Fi sendiri) dan/atau Station (tersambung ke router) pada saat yang sama.
- Halaman web dilayani oleh `WebServer` pada port 80. Browser mengambil data lewat *polling* ke beberapa alamat, misalnya `/info` (data perangkat), `/log` (isi terminal), dan `/send` (kirim pesan).
- Seluruh keluaran Serial disimpan juga ke **buffer log** sehingga bisa ditampilkan di terminal web.
- Tiap node bisa diberi **IP tersendiri** di jaringan router (misalnya akhiran `.101`, `.102`, dan `.103`), dan dapat diakses juga lewat nama seperti `esp32-master.local`.

**Hal penting: channel harus sama.** ESP-NOW memakai channel Wi-Fi yang sedang aktif. Bila node tersambung ke router, channel radio otomatis mengikuti router. Karena semua node tersambung ke router yang sama, channel mereka sama dan ESP-NOW tetap berfungsi. Jika memakai mode AP saja, semua node harus dipaksa ke channel yang sama.

**Konsep yang dipelajari:** mode Wi-Fi ganda, web server di mikrokontroler, hubungan channel Wi-Fi dengan ESP-NOW, serta IP statis dan mDNS.

> **Keamanan:** jangan menaruh **SSID dan password Wi-Fi asli** di repositori publik. Gunakan nilai contoh sebelum melakukan *commit*, atau simpan kredensial di file terpisah yang tidak ikut diunggah (misalnya lewat `.gitignore`).

---

## Pengaturan Serial Monitor

Semua program memakai pengaturan yang sama:

| Pengaturan | Nilai |
|---|---|
| Baud rate | **115200** |
| Line ending | **Newline** (atau *Both NL & CR*) |

Line ending harus *Newline* karena program membaca teks sampai tombol **Enter** ditekan.

---

## Urutan Belajar yang Disarankan

1. **Cek MAC address** semua board dan catat hasilnya.
2. **Two Ways (PtP)**: pahami dasar kirim, terima, peer, dan callback.
3. **One to Many**: belajar mengelola banyak peer.
4. **Many to One**: belajar mengenali pengirim dari MAC.
5. **Mesh**: pahami broadcast, TTL, dan relay.
6. **ESP-NOW + Web Server**: gabungkan semuanya dengan antarmuka web.

---

## Troubleshooting Umum

| Masalah | Kemungkinan penyebab | Solusi |
|---|---|---|
| MAC tercetak `00:00:00:00:00:00` | `WiFi.macAddress()` dipanggil sebelum stack Wi-Fi siap | Gunakan `esp_read_mac(mac, ESP_MAC_WIFI_STA)`. |
| Selalu `GAGAL terkirim` | MAC tujuan salah, board tujuan belum menyala atau belum menjalankan ESP-NOW, atau terlalu jauh | Cek MAC lagi, nyalakan kedua board, dekatkan jaraknya. |
| Pesan tidak muncul di board lawan | Peer belum didaftarkan, atau MAC tujuan tertukar | Periksa array MAC dan pastikan `esp_now_add_peer()` berhasil. |
| Teks tidak terkirim saat Enter ditekan | Line ending bukan *Newline* | Ganti ke **Newline**. |
| Karakter aneh di Serial Monitor | Baud rate tidak cocok | Atur ke **115200**. |
| Error kompilasi pada fungsi callback | Versi core ESP32 berbeda (v2.x dan v3.x) | Perbarui board package. Kode sudah memakai `#if` untuk kedua versi. |
| Pesan tidak terdengar saat web server aktif | Channel Wi-Fi tiap node berbeda | Pastikan semua node tersambung ke router yang sama, atau paksa channel yang sama pada mode AP. |
| Pesan terpotong | Panjang teks melebihi ukuran `text[]` | Perpendek pesan (total struct maksimal 250 byte). |

---

## Struktur Repositori

```text
Praktikum_3 : Wireless Sensor Network Short Range (ESP-NOW)
├── Cek_MacAddr_ESP32.ino    # Program untuk mengecek MAC address
├── Two Ways or PtP/         # Komunikasi dua arah
├── OneToMany/               # Satu pengirim ke banyak penerima
├── ManyToOne/               # Banyak pengirim ke satu penerima
├── Mesh/                    # Jaringan mesh (multi-hop, flooding)
├── ESPNOW-WebServer/        # ESP-NOW + web server
├── images/                  # Screenshot untuk README
└── readme.md                # Dokumen ini
```

---

## Referensi

- [Dokumentasi resmi ESP-NOW (Espressif)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [Arduino-ESP32 (GitHub)](https://github.com/espressif/arduino-esp32)
