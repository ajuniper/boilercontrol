#include <Arduino.h>
#include "projectconfig.h"
//////////////////////////////////////////////////////////////////
//
// button classes
#include "pinconfig.h"
#include "buttons.h"
#include "display.h"
#include "heatchannel.h"
#include "myconfig.h"

// if conns are on left then (0,0) is bottom right
// and we must reverse the coordinates
#ifdef display_conns_on_left
#define ch_x(_x,_w) ((display_max_x)-(_x)-(_w))
#define ch_y(_y,_h) ((display_max_y)-(_y)-(_h))
#else
#define ch_x(_x,_w) (_x)
#define ch_y(_y,_h) (_y)
#endif

// are we asking for hot or warm heat?
static bool selected_temperatures[num_heat_channels];

extern bool display_needs_reset;

void button_t::initialise()
{
#if 0
    // draw rectangle to identify touch area
    int x_w = 2+m_x2-m_x1;
    int y_h = 2+m_y1-m_y2;
    tft.drawRect(ch_x(m_x1-1,x_w),
                 ch_y(240-m_y1-1,y_h),
                 x_w,
                 y_h,
                 TFT_BLUE);
#endif
    int i;
    for(i=0;i<num_heat_channels;++i) {
        selected_temperatures[i]=false;
    }
};

void button_t::checkPressed(uint16_t a_x, uint16_t a_y)
{
    time_t now = time(NULL);
    if ((a_x >= m_x1) && (a_x <= m_x2) &&
        (a_y >= m_y1) && (a_y <= m_y2)) {
        // pressed
        if (m_presstime == 0) {
            // record press time
            m_presstime = now;
        }
        // call all the time it is pressed once per second
        if (!m_mustrelease) {
            if (m_lastnotifytime != now) {
                m_mustrelease = (*m_pressed)(m_handle, now - m_presstime);
                m_lastnotifytime = now;
            }
        }
    } else {
        // not pressed, but was it before?
        if (m_presstime != 0) {
            // notify release just once
            (*m_released)(m_handle, now - m_presstime);
            m_presstime = 0;
            m_lastnotifytime = 0;
            m_mustrelease = false;
        }
    }
}

static bool handle_press_label(int a_h, time_t a_duration)
{
    // no op
    if (!channels[a_h].getEnabled()) { return false; }
    if (channels[a_h].targetTemp2() < 0) {
        // "warm" not available on this channel
    } else {
        // toggle temperature and update label
        selected_temperatures[a_h] = !selected_temperatures[a_h];
        channels[a_h].drawName(selected_temperatures[a_h]);
    }
    return false;
}

static bool handle_release_label(int a_h, time_t a_duration)
{
    // no op
    if (!channels[a_h].getEnabled()) { return false; }
    return false;
}

static bool handle_press_active(int a_h, time_t a_duration)
{
    if (!channels[a_h].getEnabled()) { return false; }
    if (a_duration > 1) {
        bool a = !channels[a_h].getActive();
        channels[a_h].setActive(a,true);
        return true;
    }
    return false;
}

static bool handle_release_active(int a_h, time_t a_duration)
{
    if (!channels[a_h].getEnabled()) { return false; }
    if (a_duration < 2) {
        channels[a_h].setActive(!channels[a_h].getActive(),true);
    }
    return false;
}

static bool handle_press_timer(int a_h, time_t a_duration)
{
    if (!channels[a_h].getActive()) { return false; }
    // no op for short presses, action long press here
    if (a_duration > 2) {
        time_t t = channels[a_h].getTimer();
        if ((t == 0) || (t == CHANNEL_TIMER_SLUDGE)) {
            channels[a_h].setTargetTempBySetting(selected_temperatures[a_h]?1:2);
            channels[a_h].adjustTimer(CHANNEL_TIMER_ON);
        } else {
            channels[a_h].adjustTimer(CHANNEL_TIMER_OFF);
        }
        return true;
    }
    return false;
}

static bool handle_release_timer(int a_h, time_t a_duration)
{
    if (!channels[a_h].getActive()) { return false; }
    // handle short presses on release
    if (a_duration < 3) {
        if (channels[a_h].getTimer() == CHANNEL_TIMER_ON) {
            // short press when fixed on means turn off
            channels[a_h].adjustTimer(CHANNEL_TIMER_OFF);
        } else {
            // else short press means add a bit of time
            channels[a_h].setTargetTempBySetting(selected_temperatures[a_h]?1:2);
            channels[a_h].adjustTimer(timer_increment);
        }
    }
    return false;
}

static bool handle_dr_press(int , time_t )
{
    return false;
}

static bool handle_dr_release (int , time_t a_duration)
{
    if (a_duration >= 5) {
        display_needs_reset = true;
    }
    return false;
}

button_t buttons[] = {
    { ch_x(channel_label_x,channel_label_w),
      ch_y(channel_y + 0*channel_spacing,channel_icon_size),
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 0 },
    { ch_x(channel_label_x,channel_label_w),
      ch_y(channel_y + 1*channel_spacing,channel_icon_size),
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 1 },
    /*
    { ch_x(channel_label_x,channel_label_w),
      ch_y(channel_y + 2*channel_spacing,channel_icon_size),
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 2 },
     */

    { ch_x(channel_active_x,channel_active_w),
      ch_y(channel_y + 0*channel_spacing,channel_icon_size),
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 0 },
    { ch_x(channel_active_x,channel_active_w),
      ch_y(channel_y + 1*channel_spacing,channel_icon_size),
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 1 },
    /*
    { ch_x(channel_active_x,channel_active_w),
      ch_y(channel_y + 2*channel_spacing,channel_icon_size),
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 2 },
     */

    { ch_x(channel_timer_x,channel_timer_w),
      ch_y(channel_y + 0*channel_spacing,channel_icon_size),
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 0 },
    { ch_x(channel_timer_x,channel_timer_w),
      ch_y(channel_y + 1*channel_spacing,channel_icon_size),
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 1 },
    /*
    { ch_x(channel_timer_x,channel_timer_w),
      ch_y(channel_y + 2*channel_spacing,channel_icon_size),
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 2 },
     */

    // invisible button bottom left to reset display
    { ch_x(0,25), ch_y(215,25), 25, 25, &handle_dr_press, &handle_dr_release, 0 }
};
const size_t num_buttons = (sizeof(buttons)/sizeof(buttons[0]));

// remember if the display was pressed or not last time thru
static bool last_pressed = false;

bool check_touch() {
    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    bool pressed = tft.getTouch(&t_x,&t_y);

    int i;
    for(i=0; i<num_buttons; ++i) {
        buttons[i].checkPressed(t_x, t_y);
    }
    bool ret = (last_pressed != pressed);
    last_pressed = pressed;
    return ret;
}
