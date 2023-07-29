//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#include <Arduino.h>
#include "pinconfig.h"
#include "heatchannel.h"
#include "displayconfig.h"
#include "display.h"
#include <LittleFS.h>

#include <Syslog.h>
extern Syslog syslog;

//#define HEAT_COOLDOWN 600
#define HEAT_COOLDOWN 30

HeatChannel::HeatChannel(
    const char * a_name,
    int a_pin_o_zv,
    int a_pin_o_zv_satisfied,
    int a_pin_i_zv_ready,
    int a_pin_i_zv_satisfied,
    int a_target_temp,
    bool a_enabled,
    int a_cooldown_duration,
    int a_y
) : m_name(a_name),
    m_pin_zv(a_pin_o_zv),
    m_pin_zv_satisfied(a_pin_o_zv_satisfied),
    m_ready(a_pin_i_zv_ready,a_name,"ready"),
    m_satisfied(a_pin_i_zv_satisfied,a_name,"satisfied"),
    m_target_temp(a_target_temp),
    m_enabled(a_enabled),
    m_active(false),
    m_cooldown_duration(a_cooldown_duration),
    m_endtime(0),
    m_cooldown_time(0),
    m_zv_output(false),
    m_zv_satisfied_output(false),
    m_changed(true), // trigger output setup first time around
    m_y(a_y)
{
    if (m_pin_zv > -1) {
        digitalWrite(m_pin_zv, RELAY_OFF);
        pinMode(m_pin_zv, OUTPUT);
    }
    if (m_pin_zv_satisfied > -1) {
        digitalWrite(m_pin_zv_satisfied, RELAY_OFF);
        pinMode(m_pin_zv_satisfied, OUTPUT);
    }
    if (LittleFS.begin()) {
        String s = "targettemp.";
        s += m_name;
        fs::File f = LittleFS.open(s, "r");
        if (f) {
            String y;
            while (f.available()) {
                y = f.readString();
            }
            m_target_temp = y.toInt();
            f.close();
        }
        LittleFS.end();
    }
}

// update timer states
// returns true if something changed
bool HeatChannel::updateTimers(time_t now, unsigned long millinow) {
    bool ret = m_changed;
    m_changed = false;
    if (m_endtime > 0) {
        if (now > m_endtime) {
            // time to turn off
            m_endtime = 0;
            const char * c = "";
            if (m_cooldown_duration > 0) {
                m_cooldown_time = now + m_cooldown_duration;
                c = ", starting cooldown";
            }
            syslog.logf(LOG_DAEMON|LOG_INFO,"%s turning off%s",m_name,c);
            ret = true;
        }
    }
    if (m_cooldown_time > 0) {
        if (now > m_cooldown_time) {
            // time to turn off
            m_cooldown_time = 0;
            syslog.logf(LOG_DAEMON|LOG_INFO,"%s cooldown finished",m_name);
            ret = true;
        }
    }
    if (m_enabled) {
        ret |= m_ready.updatepin(millinow);
        ret |= m_satisfied.updatepin(millinow);
    }
    return ret;
}
void HeatChannel::adjustTimer(int dt) {
    m_changed = true;
    switch (dt) {
        case 0:
            // turn off
            if (m_endtime != 0) {
                if (m_cooldown_duration > 0) {
                    m_cooldown_time = time(NULL) + m_cooldown_duration;
                }
                m_endtime = 0;
            } else {
                // channel is already off, cancel cooldown
                m_cooldown_time = 0;
            }
            break;
        case -1:
            // turn on
            m_endtime = dt;
            m_cooldown_time = 0;
            break;
        default:
            // turn on for given seconds
            if (m_endtime == 0) {
                // currently off so calculate absolute off time
                m_endtime = time(NULL) + dt;
            } else if (m_endtime == -1) {
                // noop, already fixed on
            } else if (m_endtime > 0) {
                m_endtime += dt;
            }
            m_cooldown_time = 0;
    }
    syslog.logf(LOG_DAEMON | LOG_INFO, "%s set to %d",m_name,m_endtime);
}
// set the output state to the zv
void HeatChannel::setOutput(bool state) {
    if (m_enabled == true && m_pin_zv > -1) {
        if ((state == false) && (m_pin_zv_satisfied > -1)) {
            // if the channel is off then it is satisfied
            //digitalWrite(m_pin_zv_satisfied,RELAY_ON);
        }
        digitalWrite(m_pin_zv,state?RELAY_ON:RELAY_OFF);
        if (m_zv_output != state) {
            syslog.logf(LOG_DAEMON | LOG_INFO, "%s output set to %s",m_name,state?"on":"off");
        }
        m_zv_output = state;
    }
}
void HeatChannel::setSatisfied(bool state) {
    if (m_enabled == true && m_pin_zv_satisfied > -1) {
        digitalWrite(m_pin_zv_satisfied,state?RELAY_ON:RELAY_OFF);
        if (m_zv_satisfied_output != state) {
            syslog.logf(LOG_DAEMON | LOG_INFO, "%s satisfied set to %s",m_name,state?"on":"off");
        }
        m_zv_satisfied_output = state;
    }
}

// is the channel satisfied
bool HeatChannel::isSatisfied() const {
    // satisfied if
    // - no timer is active
    // - not ready for flow
    // - satisfied is signalled
    return ((wantFire() == false) && (wantCooldown() == false)) ||
           (m_ready.pinstate() == false) || // TODO? assume satisfied
           (m_satisfied.pinstate() == true);
}
// should this channel be running?
bool HeatChannel::wantFire() const {
    // signal the zone valve if
    // - channel is enabled and
    // - channel is active and
    // - timer is active
    //return m_enabled && m_active && ((m_endtime != 0) || (m_cooldown_time != 0));
    return m_enabled && m_active && (m_endtime != 0);
}
// is the channel ready for boiler and pump to run?
bool HeatChannel::canFire() const {
    // channel can fire the boiler if:
    // - the channel wants fire
    // - not satisfied
    // - zone valve is ready
    return wantFire() &&
           //(m_cooldown_time == 0) && // implicitly means timer calls
           (m_satisfied.pinstate() == false) &&
           (m_ready.pinstate() == true);
}
// is the channel asking for a cooldown period?
// i.e. ready for pump only
bool HeatChannel::wantCooldown() const {
    // channel is asking for cooldown cycle if;
    // - the channel is enabled and
    // - the channel is active
    // - the heat timer is not running
    // - the cooldown timer is running
    // - satisfied is not signalled
    return m_enabled && m_active &&
           (m_endtime == 0) && (m_cooldown_time != 0) &&
           (m_satisfied.pinstate() == false);
}
bool HeatChannel::canCooldown() const {
    // channel can cooldown if
    // - the channel wants cooldown, and
    // - the channel is not satisfied, and
    // - the zone valve is ready
    return wantCooldown() &&
           (m_satisfied.pinstate() == false) &&
           (m_ready.pinstate() == true);
}

HeatChannel channels[] = {
    HeatChannel("Hot Water",
                PIN_O_HW_CALL,
                PIN_O_HW_SATISFIED,
                PIN_I_HW_CALLS,
                PIN_I_HW_SATISFIED,
                65, // target temp
                true,
                HEAT_COOLDOWN,
                channel_y),
    HeatChannel("Heating",
                PIN_O_HEAT_CALL,
                -1,
                PIN_I_ZV1_READY,
                -1,
                55, // target temp
                true,
                HEAT_COOLDOWN,
                channel_y+channel_spacing),

    HeatChannel("Kitchen",
                PIN_O_ZV2_CALL,
                -1,
                PIN_I_ZV2_READY,
                -1,
                55, // target temp
                false,
                HEAT_COOLDOWN,
                channel_y+channel_spacing+channel_spacing)
};
const size_t num_heat_channels = sizeof(channels)/sizeof(channels[0]);

//////////////////////////////////////////////////////////////////////////////
//
// task for input pins to set debounce flag

static void input_watch(void *) {
    delay(1000);
    int i;
    for (i=0; i<num_heat_channels; ++i) {
        channels[i].getInputReady().calibrate();
        channels[i].getInputSatisfied().calibrate();
    }
    while(1) {
        delay(1);
        int i;
        for (i=0; i<num_heat_channels; ++i) {
            channels[i].getInputReady().checkstate();
            channels[i].getInputSatisfied().checkstate();
        }
    }
}

// channels initialiser
void heatchannel_setup() {
  xTaskCreate(input_watch, "inputwatch", 10000, NULL, 1, NULL);   
}

void HeatChannel::initDisplay() {
    if (m_enabled) {
        tft.setFreeFont(channel_font);
        tft.setTextColor(channel_colour,TFT_BLACK);
        tft.drawString(m_name,channel_x,m_y,fontnum);
        updateDisplay();
    }
}

void HeatChannel::updateDisplay() {
    if (!m_enabled) { return; }
    drawActive();
    drawTimer();
    drawCountdown();
}

void HeatChannel::drawIO(int row, int oncolour, bool state) const
{
    //tft.drawPixel(heat_io_x,m_y+row,state?oncolour:TFT_DARKGREY);
    //tft.fillRect(heat_io_x,y+(row*2),2,2,state?oncolour:TFT_DARKGREY);
    tft.fillRect(heat_io_x,
                 m_y+(row*(1+heat_io_size)),
                 heat_io_size,
                 heat_io_size,
                 state?oncolour:TFT_DARKGREY);
#if 0
    tft.fillRect(heat_io_x+((heat_io_size+1) * row),
                 y+channel_spacing-heat_io_size,
                 heat_io_size,
                 heat_io_size,
                 state?oncolour:TFT_DARKGREY);
#endif
}

void HeatChannel::drawActive() const {
    int x=channel_active_x + (channel_icon_size/2);
    int y=m_y + (channel_icon_size/2);
    tft.fillCircle(x,y,channel_icon_size/2,m_active?TFT_GREEN:TFT_RED);
    if (!m_active) {
        tft.fillRect(x-10,y-3,channel_icon_size-4,6,TFT_WHITE);
    }

    // single pixels to show IO state
    drawIO(0,TFT_YELLOW,m_zv_output);
    drawIO(1,TFT_GREEN,m_ready.pinstate());
    drawIO(2,TFT_RED,m_satisfied.pinstate());
    drawIO(3,TFT_RED,m_zv_satisfied_output);
    drawIO(4,TFT_ORANGE,canFire());
    drawIO(5,TFT_BLUE,wantCooldown());
}

void HeatChannel::drawTimer() const {
    int x=channel_timer_x + (channel_icon_size/2);
    int y=m_y + (channel_icon_size/2);
    if (m_endtime == -1) {
        tft.fillCircle(x,y,channel_icon_size/2,TFT_GREEN);
    } else {
        tft.fillCircle(x,y,channel_icon_size/2,TFT_BLACK);
        tft.drawCircle(x,y,channel_icon_size/2,(m_endtime!=0)?TFT_GREEN:TFT_DARKGREY);
        tft.drawFastVLine(x,y-(channel_icon_size/2)+4,(channel_icon_size/2)-4,(m_endtime!=0)?TFT_GREEN:TFT_DARKGREY);
        tft.drawFastHLine(x,y,(channel_icon_size/2)-6,(m_endtime!=0)?TFT_GREEN:TFT_DARKGREY);
    }
}

void HeatChannel::drawCountdown() const {
    tft.setFreeFont(channel_font);
    char tt[10];
    const char * t;
    tft.setTextColor(TFT_DARKGREY,TFT_BLACK);
    t="  Off";
    switch(m_endtime) {
        case -1:
            tft.setTextColor(channel_colour,TFT_BLACK);
            t="   On";
            break;
        case 0:
            // already set up
            break;
        default:
            if (m_endtime > time(NULL)) {
                tft.setTextColor(channel_colour,TFT_BLACK);
                int remaining = m_endtime - time(NULL) + 59;
                sprintf(tt,"%2d:%02d",(remaining/3600),(remaining/60));
                t=tt;
            }
    }
    tft.drawString(t,channel_countdown_x + channel_countdown_w,m_y,fontnum);
}


void HeatChannel::setTargetTemp(int target) {
    m_target_temp = target;
    if (LittleFS.begin()) {
        String s = "targettemp.";
        s += m_name;
        fs::File f = LittleFS.open(s, "w");
        if (f) {
            f.print(target);
            f.close();
        }
        LittleFS.end();
    }
}
