// TFT_eSPI setup for GODSPEED.
// ESP32-S3 DevKitC-1 N16R8 + ILI9341 2.8" 320x240 SPI panel.
//
// If your wiring differs, change the pins here. This file is force-included by
// platformio.ini; for Arduino IDE copy it into the TFT_eSPI library folder.
//
// Backlight is driven by TFT_BL. Set TFT_BL to -1 and TFT_BACKLIGHT_ON to HIGH
// if your panel's LED pin is tied to 3V3.

#ifndef GODSPEED_USER_SETUP_H
#define GODSPEED_USER_SETUP_H

#define USER_SETUP_INFO "GODSPEED ILI9341"

#define ILI9341_DRIVER

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// ESP32-S3 FSPI pins. Keep MISO free if the panel has no SDO.
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8
#define TFT_BL 7
#define TFT_BACKLIGHT_ON HIGH

// TFT_eSPI fonts are not used: LVGL renders all text with its own fonts.
// Keeping them off avoids the FS/SPIFFS dependency pulled in by SMOOTH_FONT.

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

// If the panel shows inverted colours, add:
// #define TFT_INVERSION_ON

#endif
