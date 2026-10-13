// =========================================================================
//  TFT_eSPI Driver Configuration for 3.5" SPI TFT (ILI9488) + XPT2046
// =========================================================================
// Note: In this project, these settings are automatically passed via 
// build_flags in platformio.ini. This file is provided for reference.

#define USER_SETUP_LOADED 1

#define ILI9488_DRIVER 1

// ESP32 Hardware VSPI Pins
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_BL   27

// Touch screen pin
#define TOUCH_CS 21

// SPI frequencies
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
