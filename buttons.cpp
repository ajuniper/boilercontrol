#include <Arduino.h>
//////////////////////////////////////////////////////////////////
//
// button classes
#include "buttons.h"
#include "display.h"
#include "heatchannel.h"

void button_t::initialise()
{
#if 0
    // draw rectangle to identify touch area
    tft.drawRect(m_x1-1,
                 240-m_y1-1,
                 2+m_x2-m_x1,
                 2+m_y1-m_y2,
                 TFT_BLUE);
#endif
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
        channels[a_h].setActive(!channels[a_h].getActive());
        return true;
    }
    return false;
}

static bool handle_release_active(int a_h, time_t a_duration)
{
    if (!channels[a_h].getEnabled()) { return false; }
    if (a_duration < 2) {
        channels[a_h].setActive(!channels[a_h].getActive());
    }
    return false;
}

static bool handle_press_timer(int a_h, time_t a_duration)
{
    if (!channels[a_h].getActive()) { return false; }
    // no op for short presses, action long press here
    if (a_duration > 2) {
        if (channels[a_h].getTimer() == 0) {
            channels[a_h].adjustTimer(-1);
        } else {
            channels[a_h].adjustTimer(0);
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
        if (channels[a_h].getTimer() == -1) {
            // short press when fixed on means turn off
            channels[a_h].adjustTimer(0);
        } else {
            // else short press means add a bit of time
            channels[a_h].adjustTimer(timer_increment);
        }
    }
    return false;
}

button_t buttons[] = {
    { channel_label_x, channel_y + 0*channel_spacing,
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 0 },
    { channel_label_x, channel_y + 1*channel_spacing,
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 1 },
    { channel_label_x, channel_y + 2*channel_spacing,
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 2 },

    { channel_active_x, channel_y + 0*channel_spacing,
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 0 },
    { channel_active_x, channel_y + 1*channel_spacing,
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 1 },
    { channel_active_x, channel_y + 2*channel_spacing,
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 2 },

    { channel_timer_x, channel_y + 0*channel_spacing,
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 0 },
    { channel_timer_x, channel_y + 1*channel_spacing,
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 1 },
    { channel_timer_x, channel_y + 2*channel_spacing,
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 2 }

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