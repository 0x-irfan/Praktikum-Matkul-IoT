# Praktikum 1: I/O (Software in Loop) — ESP32

Repositori ini berisi hasil **Praktikum 1 Mata Kuliah IoT** tentang *Input/Output* pada ESP32 dengan metode **Software in Loop**, yaitu program dan rangkaian diuji lewat simulator **[Wokwi](https://wokwi.com)** tanpa perlu hardware asli.

Ada 4 program: LED blink dengan interval waktu, LED running kiri ke kanan, LED running kanan ke kiri, dan LED yang dikontrol pakai tombol.

---

## Tujuan Praktikum

1. Memahami cara mengontrol **output digital** (LED) pada ESP32.
2. Memahami perbedaan **`delay()`** dan **`millis()`** dalam mengatur waktu.
3. Membuat pola LED berjalan (*running LED*) menggunakan **array dan perulangan**.
4. Membaca **input digital** (push button) dan mengontrol output berdasarkan input tersebut.

---

## Daftar Program

### Program 1 — LED Blink Interval 1, 2, 3 Detik

**Soal:** Konfigurasikan ESP32 dengan satu LED. Buat LED menyala dengan interval 1 detik, 2 detik, dan 3 detik menggunakan `delay()` dan `millis()`.

#### 🔹 Versi `delay()`

**File:** [`1a_led_delay.ino`](./1a_led_delay.ino)

**"**
Bayangin kamu lagi masak mie terus disuruh nunggu 3 menit. Kalau pakai `delay()`, kamu cuma bengong di depan panci selama 3 menit, nggak boleh ngapa-ngapain. Begitu juga ESP32: pas `delay()` jalan, dia "diem" dan nggak bisa ngerjain hal lain. Di program ini LED nyala lalu mati dengan jeda 1 detik, terus 2 detik, terus 3 detik, dan diulang terus. Daftar jedanya disimpan di array `intervals[]`, jadi kodenya cukup ditulis sekali lalu diulang pakai `for`. Cocok buat program sederhana, tapi kurang oke kalau ESP32 harus ngerjain banyak hal sekaligus.
**"**
**🎥 Video:**

<!-- Upload video 1a ke sini (drag & drop file .mp4 saat edit README di GitHub) -->
VIDEO_1A

#### 🔹 Versi `millis()`

**File:** [`1b_led_millis.ino`](./1b_led_millis.ino)

**"**
Nah, kalau `millis()` itu kayak masak mie sambil pasang timer di HP. Kamu tetap bebas main game, scroll, atau ngerjain tugas, dan cuma ngecek timer sesekali: "udah 3 menit belum?". `millis()` itu penghitung waktu ESP32 sejak dinyalakan (satuan milidetik). Programnya ngecek terus: kalau selisih waktu sekarang dan waktu terakhir LED berubah sudah sama dengan interval, LED di-*toggle* (nyala jadi mati, mati jadi nyala). Hasilnya sama persis dengan versi `delay()`, tapi ESP32 nggak "ketahan", jadi bisa sambil baca tombol atau sensor.
**"**
**🎥 Video:**

<!-- Upload video 1b ke sini -->
VIDEO_1B

---

### Program 2 — LED Running Kiri ke Kanan

**Soal:** Konfigurasikan ESP32 dengan 3 LED. Buat LED menyala *running* dari kiri ke kanan secara looping.

**File:** [`2_led_running_kiri_ke_kanan.ino`](./2_led_running_kiri_ke_kanan.ino)

**"**
Ini kayak lampu *running text* atau lampu sein mobil yang jalan. Pin ketiga LED dikumpulin di satu array `ledPins[]`, lalu `for` ngecek satu per satu dari LED paling kiri (indeks 0) sampai paling kanan (indeks 2). Tiap LED dinyalain sebentar, dimatiin, lalu giliran LED sebelahnya. Karena ada di dalam `loop()`, setelah LED kanan selesai, dia langsung balik lagi ke kiri, jadi muternya nggak berhenti. Pakai array + `for` bikin kode lebih pendek daripada nulis `digitalWrite` satu-satu.
**"**
**🎥 Video:**

<!-- Upload video program 2 ke sini -->
VIDEO_2

---

### Program 3 — LED Running Kanan ke Kiri

**Soal:** Konfigurasikan ESP32 dengan 3 LED. Buat LED menyala *running* dari kanan ke kiri secara looping.

**File:** [`3_led_running_kanan_ke_kiri.ino`](./3_led_running_kanan_ke_kiri.ino)

**"**
Konsepnya sama persis kayak Program 2, bedanya arah jalannya dibalik. Kalau tadi `for` ngitung maju dari 0 ke 2, sekarang `for`-nya ngitung mundur dari indeks terakhir (2) ke 0. Jadi yang nyala duluan LED paling kanan, lalu geser ke tengah, lalu ke kiri, dan diulang terus. Jadi cukup ubah arah hitungan di `for`, nggak perlu nulis ulang semuanya.
**"**
**🎥 Video:**

<!-- Upload video program 3 ke sini -->
VIDEO_3

---

### Program 4 — LED + 3 Button

**Soal:** Konfigurasikan ESP32 dengan 3 LED dan 3 Button.

| Tombol | Aksi |
|--------|------|
| Button 1 | LED menyala **blink** |
| Button 2 | LED **running kiri → kanan** (looping) |
| Button 3 | LED **running kanan → kiri** (looping) |

**File:** [`4_led_button_mode.ino`](./4_led_button_mode.ino)

**"**
Di sini semua program sebelumnya digabung jadi satu, dan kamu yang pegang kendali lewat tombol. Caranya, ada variabel `mode` yang tugasnya "ngingetin" ESP32 lagi di mode apa: blink, running kiri-kanan, atau running kanan-kiri. Pas tombol ditekan, program cuma ngubah isi `mode`, nggak langsung nyalain LED. Setelah itu `switch-case` yang milih pola LED sesuai `mode`-nya. Tombolnya pakai `INPUT_PULLUP`, jadi kondisi normalnya HIGH dan berubah jadi LOW pas ditekan. Timing-nya pakai `millis()` (bukan `delay()`), makanya tombol tetap langsung responsif walau LED lagi jalan, dan kamu bisa ganti mode kapan aja.
**"**
**🎥 Video:**

<!-- Upload video program 4 ke sini -->
VIDEO_4
