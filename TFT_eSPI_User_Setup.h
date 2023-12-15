/* Configuration file for TFT_eSPI
 * As per tips at the bottom of the TFT_eSPI readme
 * #include <../../boilercontrol/TFT_eSPI_User_Setup.h>
 */
#define ILI9341_DRIVER
#include <../../boilercontrol/pinconfig.h>

#define TFT_MISO PIN_SPI_MISO
#define TFT_MOSI PIN_SPI_MOSI
#define TFT_SCLK PIN_SPI_SCK
#define TFT_CS   PIN_DISPLAY_CS  // Chip select control pin
#define TFT_DC   PIN_DISPLAY_DC  // Data Command control pin
#define TFT_RST  -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST
#define TOUCH_CS PIN_TOUCH_CS     // Chip select pin (T_CS) of touch screen
#define SPI_FREQUENCY  40000000
#define USE_HSPI_PORT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts
#define LOAD_GLCD
