# Project Status

Terakhir diperbarui: 2026-10-21

## Ringkasan

MiniMotoNav menyatukan satu UI LVGL 9 untuk simulator Windows (SDL2) dan firmware
ESP32-S3 N16R8 + ILI9341 320x240. Empat mode aktif: speedometer, navigasi, drag strip,
dan GPS clock. Simulator dan firmware memakai `src/app` + `src/ui` yang sama.

## Status build

| Target | Status | Keterangan |
|---|---|---|
| Simulator MSVC Release | Lulus | `cmake --build build --config Release` |
| Unit test CTest | Lulus 4/4 | `telemetry_tests`, `drag_tests`, `perf_tests`, `command_tests` |
| Firmware `pio run -e esp32s3` | Lulus | PlatformIO 6.2.0, Arduino 3.3.12 |

Firmware terakhir:

| Memori | Terpakai | Kapasitas | Persentase |
|---|---|---|---|
| RAM statis | 59.712 B | 327.680 B | 18,2% |
| Flash (app) | 741.228 B | 6.553.600 B | 11,3% |

## Yang sudah diimplementasikan

- Speedometer: gauge 0-160, zona merah, digit animasi, trip, throttle/RPM, redraw
  selektif per area.
- Navigasi: peta vektor, rute, chevron, puck, instruksi, kompas, DIST/TIME/ETA,
  overlay FPS PC vs ESP32, redraw peta dibatasi 30 FPS.
- Drag: countdown, timer monotonic, split, finish terinterpolasi, pause/resume/cancel,
  PB/riwayat, acceleration test, deteksi data GPS tidak valid, preset target
  100/200/400/1000/2000 m di panel simulator.
- GPS clock: jam, kompas, altitude, speed, trip, akurasi, redraw selektif.
- Tema race/siang/malam dengan transisi; warna background layar hanya diubah saat
  tema berganti.
- Model performa ESP32 deterministik: profil conservative/balanced/optimistic/custom,
  SPI, buffer parsial, biaya CPU, jitter, opsi DMA overlap.
- Command parser bersama + antrean FIFO; simulator memakai CLI/keyboard, firmware
  memakai tombol dan perintah serial.
- Firmware mengukur FPS refresh, durasi flush SPI, piksel/rect dirty, render time,
  worst render, dan heap per detik.

## Benchmark singkat

`--bench=600 --novsync --profile=balanced` (detail di README):

| Layar | Dirty px / rects | Model 40 MHz |
|---|---|---|
| Speedometer | 24.325 / 3,0 | 11,9 ms / 60 FPS |
| Navigasi | 76.800 / 6,0 | 35,6 ms / 28 FPS |
| GPS clock | 12.377 / 2,0 | 6,5 ms / 60 FPS |
| Drag idle | 16.377 / 2,0 | 8,3 ms / 60 FPS |

Angka ESP32 adalah model, bukan pengukuran perangkat.

## Belum diverifikasi / keterbatasan

- Tidak ada perangkat ESP32 fisik pada sesi ini. Stabilitas panel, integritas SPI
  40 MHz, deteksi PSRAM, FPS nyata, dan perilaku GPS belum diukur.
- Klaim 60 FPS di ESP32 tidak boleh dibuat sebelum pengukuran serial.
- Satu frame penuh 320x240 RGB565 pada 40 MHz membutuhkan sekitar 30,7 ms transfer;
  navigasi full-frame karena itu tidak mungkin 60 FPS tanpa memperkecil area atau
  menaikkan SPI.
- Jalur DMA TFT_eSPI belum diaktifkan karena belum bisa divalidasi tanpa hardware.
  Simulator hanya memodelkan DMA.
- Timing drag berorientasi closed course; GPS ponsel tidak boleh diklaim akurat
  sampai milidetik.
- Tidak ada touchscreen di target; kontrol drag hanya di simulator, firmware memakai
  perintah serial.
- PB/riwayat drag masih in-memory.
- Rute navigasi adalah demo geometris, bukan peta/OSRM nyata.

## Langkah verifikasi hardware berikutnya

1. Flash `firmware.bin`, buka serial 115200, pastikan boot melaporkan `psram=8MB`.
2. Rekam `[stats]` minimal 60 detik per mode; catat FPS, `spi`, `dirty`, heap.
3. Uji stabilitas SPI 40 MHz; jika ada artefak, turunkan ke 27 MHz.
4. Uji semua tombol dan perintah serial, termasuk `target`, `split`, `countdown`.
5. Baru setelah itu dokumentasikan FPS fisik; jangan memakai angka model sebagai hasil.
