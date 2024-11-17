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
#include <mysyslog.h>

// if conns are on left then (0,0) is bottom right
// and we must reverse the coordinates
#ifdef display_conns_on_left
// if connections are on left then
// display (0,0) is bottom right
// touch (0,0) is top right
#define x_t2d(_x) (display_max_x - _x)
#define y_t2d(_y) (_y)
#else
// if connections are on right then
// display (0,0) is top left
// touch (0,0) is bottom left
#define x_t2d(_x) (_x)
#define y_t2d(_y) (display_max_y - _y)
#endif

// are we asking for hot or warm heat?
static bool selected_temperatures[num_heat_channels];

extern bool display_needs_reset;

// remember if the display was pressed or not last time thru
static bool last_pressed = false;

// should we show button regions?
static int show_buttons = 0;

void button_t::initialise()
{
    if (show_buttons) {
        // draw rectangle to identify touch area
        int x = min(x_t2d(m_x1),x_t2d(m_x2));
        int y = min(y_t2d(m_y1),y_t2d(m_y2));
        int x_w = 2+m_x2-m_x1;
        int y_h = 2+m_y2-m_y1;
        if (show_buttons > 1) {
            tft.fillRect(x, y, x_w, y_h, TFT_BLUE);
        } else {
            tft.drawRect(x, y, x_w, y_h, TFT_BLUE);
        }
        syslogf("button at (%d..%d,%d..%d) handle %d",m_x1,m_x2,m_y1,m_y2,m_handle);
    }

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
        // record when the button was last seen pressed
        m_presstime_us = esp_timer_get_time();

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
            // delay release a while in case it's not really released
            if (esp_timer_get_time() > (m_presstime_us + touch_release_debounce)) {
                // notify release just once
                (*m_released)(m_handle, now - m_presstime);
                m_presstime = 0;
                m_lastnotifytime = 0;
                m_mustrelease = false;
            }
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

// passed coordinates assume (0,0) is top left
// y coordinate is bottom left of rectange since that is how text works
// we must flip that to align with touch (0,0) bottom left
// and then deal with display rotation too
button_t::button_t(int a_x, int a_y,
         int a_w, int a_h,
         bool (*a_p)(int, time_t),
         bool (*a_r)(int, time_t),
         int a_i
        ) :
    m_presstime(0),
    m_presstime_us(0),
    m_lastnotifytime(0),
    m_pressed(a_p),
    m_released(a_r),
    m_handle(a_i),
    m_mustrelease(false)
{
    int x1 = x_t2d(max(0,a_x-button_expand));
    int x2 = x_t2d(min(display_max_x,a_x+a_w+button_expand));
    m_x1 = min(x1,x2);
    m_x2 = max(x1,x2);

    int y1 = y_t2d(max(0,a_y - button_expand));
    int y2 = y_t2d(min(display_max_y,a_y + a_h + button_expand));
    m_y1 = min(y1,y2);
    m_y2 = max(y1,y2);
};

// coordinates assume (0,0) is top left and give the lower left corner location
button_t buttons[] = {
    { channel_label_x,
      channel_y + 0*channel_spacing,
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 0 },
    { channel_label_x,
      channel_y + 1*channel_spacing,
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 1 },
    /*
    { channel_label_x,
      channel_y + 2*channel_spacing,
      channel_label_w, channel_icon_size,
      &handle_press_label, &handle_release_label, 2 },
     */

    { channel_active_x,
      channel_y + 0*channel_spacing,
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 0 },
    { channel_active_x,
      channel_y + 1*channel_spacing,
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 1 },
    /*
    { channel_active_x,
      channel_y + 2*channel_spacing,
      channel_active_w, channel_icon_size,
      &handle_press_active, &handle_release_active, 2 },
     */

    { channel_timer_x,
      channel_y + 0*channel_spacing,
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 0 },
    { channel_timer_x,
      channel_y + 1*channel_spacing,
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 1 },
    /*
    { channel_timer_x,
      channel_y + 2*channel_spacing,
      channel_timer_w, channel_icon_size,
      &handle_press_timer, &handle_release_timer, 2 },
     */

    // invisible button bottom left to reset display
    { 0, 215, 25, 25, &handle_dr_press, &handle_dr_release, 0 }
};
const size_t num_buttons = (sizeof(buttons)/sizeof(buttons[0]));

bool check_touch() {
    // To store the touch coordinates, initialise as out of range
    uint16_t t_x = 65535, t_y = 65535;
    bool pressed = tft.getTouch(&t_x,&t_y);

    if ((pressed) && (show_buttons > 0)) {
        tft.drawPixel(x_t2d(t_x), y_t2d(t_y), TFT_CYAN);
        if (show_buttons > 2) {
            syslogf("display touch at %d,%d",t_x,t_y);
        }
    }

    int i;
    for(i=0; i<num_buttons; ++i) {
        buttons[i].checkPressed(t_x, t_y);
    }
    bool ret = (last_pressed != pressed);
    last_pressed = pressed;
    return ret;
}

static const char * cfg_set_showbuttons(const char * name, const String & id, int &value) {
    // ignore ID
    show_buttons = value;
    display_needs_reset = true;
    return NULL;
}
void buttons_init() {
    MyCfgRegisterInt("showbutt",&cfg_set_showbuttons);
    show_buttons = MyCfgGetInt("showbutt", String("0"), show_buttons);
}
