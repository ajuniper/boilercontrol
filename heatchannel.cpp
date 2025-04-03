//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#include <Arduino.h>
#include "projectconfig.h"
#include "pinconfig.h"
#include "heatchannel.h"
#include "displayconfig.h"
#include "display.h"
#include "myconfig.h"
#include <ESPAsyncWebServer.h>
#include "tempsensors.h"

#include <mysyslog.h>

#ifdef SHORT_TIMES
#define HEAT_COOLDOWN 30
#define HEAT_SETBACK 60
#define HEAT_SUSPEND 5
#else
#define HEAT_COOLDOWN 600
#define HEAT_SETBACK (45*60)
#define HEAT_SUSPEND 15
#endif

HeatChannel::HeatChannel(
    int a_id,
    const char * a_name,
    int a_pin_o_zv,
    int a_pin_o_zv_satisfied,
    int a_pin_i_zv_ready,
    int a_pin_i_zv_satisfied,
    int a_target_temp1,
    int a_target_temp2,
    bool a_enabled,
    int a_cooldown_duration,
    int a_cooldown_mintemp,
    int a_y,
    int a_setback
) : m_id(a_id),
    m_name(a_name),
    m_pin_zv(a_pin_o_zv),
    m_pin_zv_satisfied(a_pin_o_zv_satisfied),
    m_ready(a_pin_i_zv_ready,a_name,"ready"),
    m_satisfied(a_pin_i_zv_satisfied,a_name,"satisfied"),
    m_target_temp1(a_target_temp1),
    m_target_temp2(a_target_temp2),
    m_enabled(a_enabled),
    m_active(false),
    m_cooldown_duration(a_cooldown_duration),
    m_cooldown_mintemp(a_cooldown_mintemp),
    m_cooldown_target(0),
    m_sludge_duration(120),
    m_endtime(0),
    m_cooldown_time(0),
    m_zv_output(false),
    m_zv_satisfied_output(false),
    m_changed(true), // trigger output setup first time around
    m_scheduler(*this, a_setback),
    m_suspendUntil(0),
    m_suspend_duration(HEAT_SUSPEND),
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
}

// update timer states
// returns true if something changed
// timer or inputs
bool HeatChannel::updateTimers(time_t now, unsigned long millinow) {
    bool ret = m_changed;
    m_changed = false;
    if ((m_endtime > 0) || (m_endtime == CHANNEL_TIMER_SLUDGE)) {
        if (now > m_endtime) {
            // time to turn off
            const char * c = "";
            if (m_endtime == CHANNEL_TIMER_SLUDGE) {
                if (m_sludge_duration > 0) {
                    m_cooldown_target = -99999;
                    m_cooldown_time = now + m_sludge_duration;
                    c = ", starting sludgebuster";
                }
            } else {
                if (m_cooldown_duration > 0) {
                    m_cooldown_target = m_cooldown_mintemp;
                    m_cooldown_time = now + m_cooldown_duration;
                    c = ", starting cooldown";
                }
            }
            m_endtime = 0;
            m_lastTime = now;
            syslogf(LOG_DAEMON|LOG_INFO,"%s turning off%s",m_name,c);
            ret = true;
        }
    }
    if (m_cooldown_time > 0) {
        if (now > m_cooldown_time) {
            // time to turn off
            m_cooldown_time = 0;
            m_lastTime = time(NULL);
            syslogf(LOG_DAEMON|LOG_INFO,"%s cooldown finished",m_name);
            ret = true;
        } else if (tempsensors_get("boiler.flow") < m_cooldown_target) {
            // cooldown flow is low enough, turn off
            m_cooldown_time = 0;
            m_lastTime = time(NULL);
            syslogf(LOG_DAEMON|LOG_INFO,"%s cooldown finished early",m_name);
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
    switch (dt) {
        case CHANNEL_TIMER_OFF:
            // turn off
            if (m_endtime != 0) {
                // channel is running so set end time to
                // a moment ago
                m_scheduler.turnedOff(m_endtime);
                m_endtime = time(NULL)-1;
            } else if (m_cooldown_time != 0) {
                // channel is already off, cancel cooldown
                m_cooldown_time = time(NULL)-1;
            }
            break;
        case CHANNEL_TIMER_ON:
            // turn on
            m_cooldown_time = 0;
            m_endtime = dt;
            break;
        case CHANNEL_TIMER_SLUDGE:
            // run cooldown cycle to circulate sludge
            // TODO set m_endtime to 1/2 and check for this in updateTimers
            m_cooldown_time = 0;
            m_endtime = dt;
            break;
        default:
            // turn on for given seconds
            m_cooldown_time = 0;
            if (dt < 1000000000) {
                // delta
                if ((m_endtime == 0) || (m_endtime == CHANNEL_TIMER_SLUDGE)) {
                    // currently off so calculate absolute off time
                    m_endtime = time(NULL) + dt;
                } else if (m_endtime == CHANNEL_TIMER_ON) {
                    // noop, already fixed on
                } else {
                    // at some arbitrary value, add the extra time
                    m_endtime += dt;
                }
            } else {
                // absolute value, just save it
                m_endtime = dt;
            }
    }
    syslogf(LOG_DAEMON | LOG_INFO, "%s set to %d",m_name,m_endtime);
    m_changed = true;
    // update the last activity time here to hold off the sludge buster otherwise the sludge
    // buster triggers before the main task stops the burner
    m_lastTime = time(NULL);
}

// set the output state to the zv
// returns true if state changes
bool HeatChannel::setOutput(bool state, time_t when) {
    if ((m_enabled == false ) || (m_pin_zv == -1)) {
        return false;
    }
    bool changed = false;
    if (state != m_zv_output.setState(state,when)) {
        syslogf(LOG_DAEMON | LOG_INFO, "%s request output set to %s at %d",m_name,state?"on":"off",when);
        changed = true;
        needSuspend();
    }
    return changed;
}
bool HeatChannel::setSatisfied(bool state, time_t when) {
    if ((m_enabled == false ) || (m_pin_zv_satisfied == -1)) {
        return false;
    }
    bool changed = false;
    if (state != m_zv_satisfied_output.setState(state,when)) {
        syslogf(LOG_DAEMON | LOG_INFO, "%s request satisfied set to %s at %d",m_name,state?"on":"off",when);
        changed = true;
        needSuspend();
    }
    return changed;
}

// update the output pin as required, and
// returns true if change is scheduled, false otherwise
bool HeatChannel::setOutputPin(time_t now) {
    if ((m_enabled == false ) || (m_pin_zv == -1)) {
        return false;
    }

    bool curr = m_zv_output.currentState();
    bool nextstate = m_zv_output.checkState(now);
    digitalWrite(m_pin_zv, nextstate?RELAY_ON:RELAY_OFF);
    if (nextstate != curr) {
        syslogf(LOG_DAEMON | LOG_INFO, "%s output set to %s",m_name,nextstate?"on":"off");
    }
    return (m_zv_output.nextChange() != 0);
}

bool HeatChannel::setSatisfiedPin(time_t now) {
    if ((m_enabled == false ) || (m_pin_zv_satisfied == -1)) {
        return false;
    }

    bool curr = m_zv_satisfied_output.currentState();
    bool nextstate = m_zv_satisfied_output.checkState(now);
    digitalWrite(m_pin_zv_satisfied, nextstate?RELAY_ON:RELAY_OFF);
    if (nextstate != curr) {
        syslogf(LOG_DAEMON | LOG_INFO, "%s satisfied set to %s",m_name,nextstate?"on":"off");
    }
    return (m_zv_satisfied_output.nextChange() != 0);
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
    return m_enabled && m_active && ((m_endtime > 0) || (m_endtime == CHANNEL_TIMER_ON));
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

void HeatChannel::readConfig()
{
    m_target_temp1 = MyCfgGetInt("tgttmp",String(m_id), m_target_temp1);
    m_target_temp2 = MyCfgGetInt("tgttmp2",String(m_id), m_target_temp2);
    m_active = MyCfgGetInt("chactive",String(m_id), m_active);
    m_cooldown_duration = MyCfgGetInt("coolrun",String(m_id), m_cooldown_duration);
    m_cooldown_mintemp = MyCfgGetInt("cooltmp",String(m_id), m_cooldown_mintemp);
    m_sludge_duration = MyCfgGetInt("circrun",String(m_id), m_sludge_duration);
    m_suspend_duration = MyCfgGetInt("suspend",String(m_id), m_suspend_duration);
}

HeatChannel channels[num_heat_channels] = {
    HeatChannel(0, "Hot Water",
                PIN_O_HW_CALL,
                PIN_O_HW_SATISFIED,
                PIN_I_HW_CALLS,
                PIN_I_HW_SATISFIED,
                65, -1, // target temps (warm not supported)
                true,
                HEAT_COOLDOWN,
                60,
                channel_y,
                0),

    HeatChannel(1, "Heating",
                PIN_O_HEAT_CALL,
                -1,
                PIN_I_ZV1_READY,
                -1,
                65, 50, // target temp
                true,
                HEAT_COOLDOWN,
                45,
                channel_y+channel_spacing,
                HEAT_SETBACK),

#if 0
    HeatChannel(2, "Kitchen",
                PIN_O_ZV2_CALL,
                -1,
                PIN_I_ZV2_READY,
                -1,
                50, -1, // target temp (warm not supported)
                false,
                HEAT_COOLDOWN,
                channel_y+channel_spacing+channel_spacing,
                HEAT_SETBACK)
#endif
};

static const char * cfg_set_targettemp(const char * name, const String & id, int &value) {
    int ch = id.toInt();
    if ((ch >= 0) && (ch <= num_heat_channels)) {
        if (strcmp(name, "tgttmp2") == 0) {
            channels[ch].setTargetTemp2(value);
        } else if (strcmp(name, "tgttmp") == 0) {
            channels[ch].setTargetTemp1(value);
        } else if (strcmp(name, "coolrun") == 0) {
            channels[ch].setCooldownRuntime(value);
        } else if (strcmp(name, "cooltmp") == 0) {
            channels[ch].setCooldownMintemp(value);
        } else if (strcmp(name, "circrun") == 0) {
            channels[ch].setSludgeRuntime(value);
        } else if (strcmp(name, "suspend") == 0) {
            channels[ch].setSuspendTime(value);
        } else {
            return "Invalid setting";
        }
        return NULL;
    } else {
        return "Invalid channel";
    }
}

static const char * cfg_set_active(const char * name, const String & id, int &value) {
    int ch = id.toInt();
    if ((ch >= 0) && (ch <= num_heat_channels)) {
        channels[ch].setActive(value);
        return NULL;
    } else {
        return "Invalid channel";
    }
}

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
    int i;
    for (i=0; i<num_heat_channels; ++i) {
        channels[i].readConfig();
    }
    scheduler_setup();
    xTaskCreate(input_watch, "inputwatch", 10000, NULL, 1, NULL);   
    MyCfgRegisterInt("tgttmp",&cfg_set_targettemp);
    MyCfgRegisterInt("tgttmp2",&cfg_set_targettemp);
    MyCfgRegisterInt("coolrun",&cfg_set_targettemp);
    MyCfgRegisterInt("cooltmp",&cfg_set_targettemp);
    MyCfgRegisterInt("circrun",&cfg_set_targettemp);
    MyCfgRegisterInt("chactive",&cfg_set_active);
    MyCfgRegisterInt("suspend",&cfg_set_targettemp);
}

void HeatChannel::drawName(bool warm) const {
    if (m_enabled) {
        tft.setFreeFont(channel_font);
        if (warm) {
            tft.setTextColor(channel_colour_warm,TFT_BLACK);
        } else {
            tft.setTextColor(channel_colour_hot,TFT_BLACK);
        }
        tft.drawString(m_name,channel_x,m_y,fontnum);
    }
}

void HeatChannel::initDisplay() {
    drawName(false);
    updateDisplay();
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
    if (m_endtime == CHANNEL_TIMER_ON) {
        tft.fillCircle(x,y,channel_icon_size/2,TFT_GREEN);
    } else {
        // treat sludge as off
        tft.fillCircle(x,y,channel_icon_size/2,TFT_BLACK);
        tft.drawCircle(x,y,channel_icon_size/2,(m_endtime>0)?TFT_GREEN:TFT_DARKGREY);
        tft.drawFastVLine(x,y-(channel_icon_size/2)+4,(channel_icon_size/2)-4,(m_endtime>0)?TFT_GREEN:TFT_DARKGREY);
        tft.drawFastHLine(x,y,(channel_icon_size/2)-6,(m_endtime>0)?TFT_GREEN:TFT_DARKGREY);
    }
}

void HeatChannel::drawCountdown() const {
    tft.setFreeFont(channel_font);
    char tt[10];
    const char * t;
    tft.setTextColor(TFT_DARKGREY,TFT_BLACK);
    t="  Off";
    switch(m_endtime) {
        case CHANNEL_TIMER_ON:
            tft.setTextColor(channel_colour,TFT_BLACK);
            t="   On";
            break;

        // treat sludge as off
        case CHANNEL_TIMER_SLUDGE:
        case 0:
            // already set up
            break;

        default:
            if (m_endtime > time(NULL)) {
                tft.setTextColor(channel_colour,TFT_BLACK);
                int remaining = m_endtime - time(NULL) + 59;
                sprintf(tt,"%2d:%02d",(remaining/3600),((remaining/60)%60));
                t=tt;
            }
    }
    tft.drawString(t,channel_countdown_x + channel_countdown_w,m_y,fontnum);
}

void HeatChannel::setTargetTemp(int target) {
    m_target_temp = target;
    m_changed = true;
}

void HeatChannel::setTargetTempBySetting(int target) {
    /* 0/1/2 */
    if ((m_target_temp2 > -1) && (target == 1)) {
        // channel supports 2 "on" temperatures
        // "warm" is requested
        m_target_temp = m_target_temp2;
    } else if (target == 0) {
        m_target_temp = 0;
    } else {
        // target temperature is always #1
        m_target_temp = m_target_temp1;
    }
    m_changed = true;
}

void HeatChannel::setActive(bool a, bool updateConfig) {
    if (!a) {
        m_endtime = 0;
        m_cooldown_time = 0;
    }
    m_active = a;
    m_changed = true;
    m_scheduler.wakeUp();
    if (updateConfig) {
        MyCfgPutInt("chactive", String(m_id), a);
    }
};

void HeatChannel::needSuspend() {
    if (m_suspendUntil > 0) {
        // already suspended, take no action
        return;
    }
    m_suspendUntil = time(NULL) + m_suspend_duration;
}
