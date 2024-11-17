//////////////////////////////////////////////////////////////////
//
// config for display layout and behaviour
#pragma once


#include "projectconfig.h"
#include "time.h"

#ifdef display_conns_on_left
// if connections are on left then
// display (0,0) is bottom right
// touch (0,0) is top right
#define display_rotation 3
#else
// if connections are on right then
// display (0,0) is top left
// touch (0,0) is bottom left
#define display_rotation 1
#endif
#define display_max_x 320
#define display_max_y 240

// coordinate values below assume
// display (0,0) is top left

// millisecods
#define touch_delay 25
// microseconds
#define touch_release_debounce 100000
#define fontnum 1
#define clock_font &FreeMonoBold18pt7b
#define clock_x 190
#define clock_y 1
#define clock_colour TFT_WHITE
#define button_expand 10
#define display_sleep 30
#define display_brightness 200

#define channel_font &FreeMonoBold12pt7b
#define channel_x 130
#define channel_y 50
#define channel_spacing 55
#define channel_icon_size 24
#define channel_colour TFT_CYAN
#define channel_colour_hot TFT_RED
#define channel_colour_warm TFT_ORANGE
#define channel_label_x 1
#define channel_label_w channel_x
#define channel_active_x (channel_x + 30)
#define channel_active_w channel_icon_size
#define channel_timer_x (channel_x + 86)
#define channel_timer_w channel_icon_size
#define channel_countdown_x 215
#define channel_countdown_w 98
#ifdef SHORT_TIMES
#define timer_increment 10
#else
#define timer_increment 3600
#endif

#define temp_font nullptr
#define temp_x 64
#define temp_y 210
#define temp_colour TFT_DARKGREY

#define ip_font nullptr
#define ip_x 320
#define ip_y 230
#define ip_colour TFT_DARKGREY

#define pump_x 296
#define pump_y 0
#define pump_w 24
#define pump_h 24
#define pump_colour TFT_RED
#define pump_colour2 TFT_BLUE

#define flame_x 270
#define flame_y 0
#define flame_w 24
#define flame_h 24
#define flame_colour TFT_RED
#define flame_colour2 TFT_BLUE

#define wifi_x 225
#define wifi_y 0
#define wifi_w 45
#define wifi_h 40
#define wifi_colour 0xffff

#define rssi_width 24
#define rssi_height 24
#define rssi_spacing 5
#define rssi_x 246
#define rssi_y 22
#define rssi_deg 45
#define rssi_width 1
#define rssi_direction 225
#define rssi_colour TFT_LIGHTGREY

#define heat_io_x 318
//#define heat_io_x 290
#define heat_io_size 2
