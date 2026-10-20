# MiniMotoNav

Simulator dashboard sepeda motor + navigasi untuk **LVGL 9**, dengan satu sumber UI yang
sama untuk **Windows (SDL2)** dan **ESP32-S3 N16R8 + ILI9341 320x240**.

Fokus utama: animasi halus 60 FPS, angka kecepatan besar yang bergulir, gauge putih-merah
dengan zona merah, panel RPM/throttle bergaya racing, peta vektor dengan rute dan instruksi
belok, serta tiga tema: race (default), siang, dan malam.

| Race - speedometer | Race - navigasi |
|---|---|
| ![Speedometer race](shots/race_speedo.png) | ![Navigasi race](shots/race_nav.png) |

| Siang - speedometer | Malam - speedometer |
|---|---|
| ![Speedometer siang](shots/day_speedo.png) | ![Speedometer malam](shots/night_speedo.png) |

| Siang - navigasi | Malam - navigasi |
|---|---|
| ![Navigasi siang](shots/day_nav.png) | ![Navigasi malam](shots/night_nav.png) |

![Panel kontrol](shots/controls.png)

## Fitur

- Speedometer full-custom: gauge 0-160 km/h dengan zona merah 120-160, cincin dalam,
  indikator GPS hijau, angka 3 digit dengan transisi geser/crossfade per digit, trip,
  serta bar throttle dan RPM putih-merah.
- Navigasi full-custom: peta vektor berputar mengikuti heading, rute dengan chevron,
  puck kendaraan, kartu instruksi belok, kompas, DIST/TIME/ETA.
- Tema race, siang, dan malam dengan transisi halus (bukan ganti warna mendadak).
- Telemetri simulasi: siklus skenario kota 124 detik, gearbox 6 percepatan, atau mode
  manual lewat panel kontrol.
- Overlay kontrol (slider + tombol) di `lv_layer_top()`, bisa dipakai dengan mouse.
- FPS dan render time nyata ditampilkan di layar navigasi (bisa disembunyikan).
- Screenshot dan benchmark frame time dari command line.

## Build simulator (Windows)

Prasyarat: CMake >= 3.20 dan Visual Studio 2022 Build Tools (MSVC x64).
LVGL 9.3.0 ada di `third_party/lvgl` sebagai submodule, SDL2 2.30.9 sudah ada di
`third_party/SDL2`. Jika clone dari GitHub, sertakan submodule:

```powershell
git clone --recursive https://github.com/hnftxx/MiniMotoNav.git
```

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\MiniMotoNav.exe
```

`SDL2.dll` disalin otomatis ke folder executable oleh CMake.

### Argumen

| Argumen | Fungsi |
|---|---|
| `--mode=nav` / `--mode=speedo` | Mulai di layar navigasi / speedometer |
| `--theme=race` / `day` / `night` | Mulai dengan tema race (default), siang, atau malam |
| `--controls` | Buka panel kontrol saat start |
| `--scale=N` | Skala jendela 1-6 (default 3 = 960x720) |
| `--novsync` | Jalankan tanpa batas 60 FPS (untuk uji beban) |
| `--no-fps` | Sembunyikan penghitung FPS |
| `--bench=N` | Ukur N frame, cetak avg/p95/max frame + render time |
| `--shot=file.bmp` | Simpan screenshot setelah warm-up |
| `--speed=N` | Kecepatan untuk screenshot (default 78 km/h) |
| `--warmup=N` | Jumlah frame sebelum screenshot (default 150) |
| `--help` | Daftar argumen |

Contoh:

```powershell
.\build\Release\MiniMotoNav.exe --bench=600 --mode=nav --theme=race
.\build\Release\MiniMotoNav.exe --shot=shots\race_nav.bmp --warmup=180 --mode=nav --theme=race
ffmpeg -y -i shots\race_nav.bmp shots\race_nav.png
```

### Tombol keyboard

| Tombol | Aksi |
|---|---|
| `Esc` | Keluar |
| `Space` | Jalan / pause |
| `N` | Ganti speedometer / navigasi |
| `T` | Ganti tema: race -> siang -> malam -> race |
| `C` | Buka / tutup panel kontrol |
| `F` | Tampilkan / sembunyikan FPS (di navigasi) |
| `R` | Reset simulasi |
| `Up` / `Down` | Naik/turun kecepatan manual (5 km/h) |
| `Left` / `Right` | Turun/naik throttle (5%) |
| `W` / `S` | Naik/turun RPM (250) |

## Hasil pengukuran simulator

Diukur pada build MSVC Release, 320x240, dengan `--bench` yang memaksa satu render penuh
per frame (di luar vsync):

| Layar | Rata-rata render | p95 render | Rata-rata frame (uncapped) |
|---|---|---|---|
| Speedometer | 0.77 ms | 1.03 ms | 0.97 ms |
| Navigasi | 1.03 ms | 1.45 ms | 1.25 ms |

Loop interaktif dipacu tetap 60 FPS. Indikator FPS (layar navigasi) menunjukkan 60-61 saat aktif.
Angka di atas adalah hasil PC, **bukan** klaim performa ESP32.

## Struktur proyek

```
src/app/          telemetri simulasi, state aplikasi, tipe performa
src/ui/           speedometer, navigasi, panel kontrol, drawing helpers
src/ui/fonts/     font Montserrat ExtraBold hasil konversi lv_font_conv
src/platform/     entry point ESP32 (TFT_eSPI)
sim/              SDL2 platform + main simulator
tests/            unit test telemetri (CTest)
include/          User_Setup.h untuk TFT_eSPI
lv_conf.h         konfigurasi LVGL bersama
CMakeLists.txt    build simulator + test
platformio.ini    build firmware ESP32-S3
third_party/      LVGL (submodule) + SDL2 prebuilt
```

## Target ESP32-S3 N16R8

### Perangkat

- ESP32-S3 DevKitC-1 N16R8 (16 MB flash, 8 MB PSRAM)
- ILI9341 2.8" 320x240 SPI
- Kabel jumper, backlight panel ke GPIO7 (atau 3V3)

### Wiring default (`include/User_Setup.h`)

| ILI9341 | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | GPIO10 |
| RESET | GPIO8 |
| DC | GPIO9 |
| MOSI / SDI | GPIO11 |
| SCK | GPIO12 |
| MISO / SDO | GPIO13 (opsional) |
| LED / BL | GPIO7 |

Ubah pin di `include/User_Setup.h` jika wiring berbeda. File itu di-*force include*
oleh `platformio.ini`, jadi TFT_eSPI dan firmware memakai definisi yang sama.
Jika panel menampilkan warna terbalik, aktifkan `TFT_INVERSION_ON` di file tersebut.
Landscape diatur oleh `tft.setRotation(1)` di `src/platform/esp32_main.cpp`
(gunakan `3` bila gambar terbalik).

### Build & upload

```powershell
pip install platformio
pio run -e esp32s3
pio run -e esp32s3 -t upload
pio device monitor
```

Konfigurasi memakai LVGL 9.3.0 + TFT_eSPI dari registry PlatformIO, buffer parsial
320x40 baris, dan flush manual RGB565 (`pushColors(..., true)`).

Status build terakhir (PlatformIO 6.2.0, Arduino core 3.3.12, `pio run -e esp32s3`):

| Memori | Terpakai | Kapasitas | Persentase |
|---|---|---|---|
| RAM statis | 50.304 B | 327.680 B | 15,4% |
| Flash (app) | 681.508 B | 6.553.600 B | 10,4% |

Angka di atas adalah hasil kompilasi, bukan hasil pengukuran di perangkat.

### Catatan performa hardware

Firmware ini **belum diuji pada perangkat fisik**. Jangan menganggap 60 FPS di ESP32
sudah tercapai sebelum diukur. Yang bisa dilakukan setelah flash:

1. Penghitung FPS hanya tampil di layar navigasi. Perangkat belum punya input, jadi untuk melihatnya sementara ubah kode ke layar navigasi.
2. Jika di bawah 60 FPS, turunkan `kBufferLines` (mis. 20), naikkan
   `LV_DEF_REFR_PERIOD` ke 16, atau kurangi jumlah segmen gradien gauge (96).
3. Pastikan SPI 40 MHz stabil. Turunkan ke 27 MHz bila muncul artefak.
4. Pastikan PSRAM OPI aktif bila memakai board N16R8 (tidak wajib untuk kode ini,
   karena buffer render berada di RAM internal).

## Model telemetri

- Mode otomatis: target kecepatan mengikuti siklus 124 detik (34, 78, 48, 96, 62, 122,
  70 km/h) dengan slew target 18 km/h/s dan batas akselerasi -16/+10.5 km/h/s.
- Gearbox 6 percepatan: RPM dihitung dari window kecepatan tiap gear.
- Mode manual: slider kecepatan 0-160, throttle 0-100%, RPM 800-9000.
- Trip meter terintegrasi dari kecepatan; clock mulai 14:32 dan maju 1 menit per
  60 detik simulasi.
- Navigasi memakai loop rute sintetis + grid jalan deterministik; bukan GPS nyata.

## Keterbatasan

- Belum ada input touch pada target ESP32 (panel kontrol hanya di simulator).
- Belum ada GPS/IMU nyata; semua data disimulasikan.
- Rute navigasi adalah demo geometris, bukan data peta/OSRM.
- Performa hardware belum diverifikasi.

## Lisensi komponen pihak ketiga

- LVGL 9.3.0 - MIT
- SDL2 2.30.9 - zlib
- Montserrat (SIL Open Font License 1.1), dikonversi ke font LVGL.
