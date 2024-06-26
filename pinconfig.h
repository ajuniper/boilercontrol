#ifndef __PINCONFIG_H__
#define __PINCONFIG_H__
// input only: 36 39 34 35
// output only: 12
// available: 2 4 5 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33

// required IO:
// 3(4)x mains detect - 36 39 34 35
// 4(6)x mains control - 32 33 25 26 27 14
// display CS+DC+LED(PWM)
// touch   CS+IRQ
// RTC     CLK, RST, DATA
// 3x SPI
// 1wire

// spare - 12 (output only, cannot be pulled high)

// Boiler Control Connections
#define PIN_I_HW_SATISFIED 35
#define PIN_I_HW_CALLS 34
#define PIN_I_ZV1_READY 39
#define PIN_I_ZV2_READY 36

#define PIN_O_HW_CALL 32
#define PIN_O_HW_SATISFIED 33
#define PIN_O_HEAT_CALL 25
#define PIN_O_PUMP_ON 26
#define PIN_O_BOILER_ON 27
#define PIN_O_ZV2_CALL 14

// display
// see also ../libraries/TFT_eSPI//User_Setup.h
//#define PIN_DISPLAY_CS 15
//#define PIN_DISPLAY_DC 2
//#define PIN_DISPLAY_LED 17
#define PIN_DISPLAY_CS 23
#define PIN_DISPLAY_DC 22
#define PIN_DISPLAY_LED 21

// touchscreen
//#define PIN_TOUCH_CS 18
//#define PIN_TOUCH_IRQ 19
#define PIN_TOUCH_CS 19
#define PIN_TOUCH_IRQ 18

// DS1320 RTC
//#define PIN_RTC_CLK 23
//#define PIN_RTC_RST 22
//#define PIN_RTC_DATA 21
#define PIN_RTC_CLK 5
#define PIN_RTC_RST 16
#define PIN_RTC_DATA 17

// SPI common
#define PIN_SPI_MOSI 4
//#define PIN_SPI_MISO 5
//#define PIN_SPI_SCK 16
#define PIN_SPI_MISO 2
#define PIN_SPI_SCK 15

// Onewire for temperature sensors
#define PIN_1WIRE 13

// relay outputs are inverted
#define RELAY_ON LOW
#define RELAY_OFF HIGH

#endif
