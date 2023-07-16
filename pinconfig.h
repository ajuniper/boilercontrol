#ifndef __PINCONFIG_H__
#define __PINCONFIG_H__
// input only: 36 39 34 35
// output only: 12
// available: 2 4 5 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33

// required IO:
// 3(4)x mains detect - 36 39 34 35
// 4(6)x mains control - 32 33 25 26 27 14
// display CS+DC+LED(PWM) - 23 22 21
// touch   CS+IRQ - 19 18 
// RTC     CLK, RST, DATA - 5 17 16
// 3x SPI - 4 2 15
// 1wire - 13

// spare - 12 (output only)

// Boiler Control Connections
#define PIN_I_HW_SATISFIED 36
#define PIN_I_HW_CALLS 39
#define PIN_I_ZV1_READY 34
#define PIN_I_ZV2_READY 35

#define PIN_O_HW_CALL 32
#define PIN_O_HEAT_CALL 33
#define PIN_O_PUMP_ON 25
#define PIN_O_BOILER_ON 26
#define PIN_O_ZV2_CALL 27
#define PIN_O_HW_SATISFIED 14

// display
#define PIN_DISPLAY_CS 23
#define PIN_DISPLAY_DC 22
#define PIN_DISPLAY_LED 21

// touchscreen
#define PIN_TOUCH_CS 19
#define PIN_TOUCH_IRQ 18

// DS1320 RTC
#define PIN_RTC_CLK 5
#define PIN_RTC_RST 17
#define PIN_RTC_DATA 16

// SPI common
#define PIN_SPI_MOSI 4
#define PIN_SPI_MISO 2
#define PIN_SPI_SCK 15

// Onewire for temperature sensors
#define PIN_1WIRE 13

// relay outputs are inveted
#define RELAY_ON LOW
#define RELAY_OFF HIGH

#endif
