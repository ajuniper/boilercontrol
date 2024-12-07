#include <mywifi.h>
#include "projectconfig.h"
#include <AsyncTCP.h>
#include "time.h"
#include <WiFiUdp.h>
#include <mysyslog.h>
#include "pinconfig.h"
#include "display.h"
#include "tempsensors.h"
#include "mytime.h"
#include "outputs.h"
#include <tempreporter.h>
#include <mywebserver.h>
#include <webupdater.h>
#include <myconfig.h>
#include "scheduledoutput.h"
#include "buttons.h"
#include <mysystem.h>

// task to update outputs
static TaskHandle_t outputtask_handle = NULL;

// last output state
ScheduledOutput o_pump_on;
ScheduledOutput o_boiler_on;
t_output_state o_boiler_state;
t_output_state o_pump_state;
int target_temp = 999;

#include <my_secrets.h>
const char* ssid     = MY_WIFI_SSID;
const char* password = MY_WIFI_PASSWORD;

// if we cycle the boiler off at its target temperature
// then we can't start it again for this period
#ifdef SHORT_TIMES
#define BOILER_SHORT_CYCLE 30
#else
#define BOILER_SHORT_CYCLE 120
#endif
#define BOILER_RELAY_WAIT 3

//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#include "heatchannel.h"

//////////////////////////////////////////////////////////////////////////////
//
// Web Server Stuff
#include "webserver.h"

static const char * handleConfigBoiler(const char * name, const String & id, int &value) {
    if (id == "cyct") {
        // all ok, save the value
        return NULL;
    } else if (id == "cycpc") {
        // all ok, save the value
        return NULL;
    } else if (id == "wait") {
        // all ok, save the value
        return NULL;
    } else {
        return "boiler config not recognised";
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// setup

void setup() {
    // put your setup code here, to run once:
    // Serial port for debugging purposes
    Serial.begin(115200);
    Serial.println("Starting...");
    MyCfgInit(true);
    MyCfgRegisterInt("boiler", &handleConfigBoiler);

    mytime_setup(MY_TIMEZONE, PIN_RTC_CLK, PIN_RTC_DATA, PIN_RTC_RST);
    WIFI_init("boiler");
    SyslogInit("boiler");
    xTaskCreate(output_task, "outputs", 10000, NULL, 1, &outputtask_handle);
    webserver_setup();
    SYS_init();
    tempsensors_init();
    heatchannel_setup();
    buttons_init();

    // start the display update task
    display_init();

    // firmware updates
    UD_init(server);
}

//////////////////////////////////////////////////////////////////////////////
//
// output control
// separate task to permit mains cycle sync

static void output_task (void *)
{

    // set up digital IO - set output state before setting mode to avoid boot time glitches
    digitalWrite(PIN_O_PUMP_ON, RELAY_OFF);
    digitalWrite(PIN_O_BOILER_ON, RELAY_OFF);
    pinMode(PIN_O_PUMP_ON, OUTPUT);
    pinMode(PIN_O_BOILER_ON, OUTPUT);

    delay(1000);

    // cache the requested output settings
    // only update when we are signalled
    bool last_boiler = false;
    bool this_boiler = false;
    bool this_pump = false;
    int last_target_temp = target_temp;
    time_t boiler_cycle_time = 0;
    int hysteresis = 100 - 10;

    while(1) {
        // wait for signal
        uint32_t n = 0;
        while (n == 0) {
            TickType_t waittime = 5*1000; // 5s
            time_t now = time(NULL);
            int flow_temp = tempsensors_get("boiler.flow");

            this_boiler = o_boiler_on.checkState(now);
            if (last_boiler != this_boiler) {
                if (this_boiler == true) {
                    syslogf(LOG_DAEMON | LOG_WARNING, "Boiler can fire, temp now %dC",tempsensors_get("boiler.flow"));
                }
                last_boiler = this_boiler;
                boiler_cycle_time = 0;
            }

            // recalculate whether the boiler should be running or not
            // this is where we are cycling the boiler to temperature
            if (this_boiler == false) {
                // boiler is requested off
                boiler_cycle_time = 0;
                waittime = 3600 * 1000; // 1hr
                o_boiler_state = OUTPUT_OFF;

            } else if (boiler_cycle_time > now) {
                // boiler is already cycled off by time so nothing to do
                // just wait until the end of the cycle window unless
                // something changes in the mean time
                waittime = (boiler_cycle_time - now) * 1000;
                this_boiler = false;

            } else if ((boiler_cycle_time != 0) &&
                       (flow_temp > ((last_target_temp * hysteresis)/100))) {
                // boiler is cycled off by temperature so nothing to do
                // just wait a few seconds and check again unless
                // something changes in the mean time
                waittime = 10000;
                this_boiler = false;

            } else if (flow_temp > last_target_temp) {
                // boiler just went over temperature

                // read the cycling config
                int short_cycle = MyCfgGetInt("boiler","cyct", BOILER_SHORT_CYCLE);
                hysteresis = 100 - MyCfgGetInt("boiler","cycpc", 10);

                syslogf(LOG_DAEMON | LOG_WARNING, "Cycling boiler off, flow temp %dC is greater than target %dC fire again at %dC",flow_temp,last_target_temp,((last_target_temp * hysteresis)/100));

                // start a new cycle period
                this_boiler = false;

                // the earliest time the boiler can fire again
                boiler_cycle_time = now + short_cycle;

                // how long to wait before looking again
                waittime = short_cycle * 1000;

                // we are cycling the boiler
                o_boiler_state = OUTPUT_COOLING;

            } else {
                // boiler should be on
                o_boiler_state = OUTPUT_HEATING;
                if (boiler_cycle_time != 0) {
                    // boiler just finished cycling
                    syslogf(LOG_DAEMON | LOG_WARNING, "Cycling boiler back on, temp now %dC",flow_temp);
                    boiler_cycle_time = 0;
                }
            }

            // calculate what the pump should be doing
            if (o_pump_on.checkState(now)) {
                this_pump = true;
                // update what the pump is doing
                // if boiler is on or cycling then we are heating
                o_pump_state = (o_boiler_state == OUTPUT_OFF)?OUTPUT_COOLING:OUTPUT_HEATING;
            } else {
                // pump is not on
                this_pump = false;
                o_pump_state = OUTPUT_OFF;
            }

            // update the outputs
            digitalWrite(PIN_O_PUMP_ON, this_pump?RELAY_ON:RELAY_OFF);
            digitalWrite(PIN_O_BOILER_ON, this_boiler?RELAY_ON:RELAY_OFF);

            int i;
            for (i=0; i<num_heat_channels; ++i) {
                if (channels[i].setOutputPin(now)) {
                    waittime = min((TickType_t(500)), waittime);
                }
                if (channels[i].setSatisfiedPin(now)) {
                    waittime = min((TickType_t(500)), waittime);
                }
            }

            // if change is coming then be ready for it
            if (o_boiler_on.nextChange() > 0) {
                waittime = min((TickType_t(500)), waittime);
            }
            if (o_pump_on.nextChange() > 0) {
                waittime = min((TickType_t(500)), waittime);
            }
            // wait for something to change, either demand state or
            // calculated time period elapsed
            n = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( waittime) );

            // n==0 indicates timeout, else signalled
        } // end of wait for signal/timeout

        // drop out of loop if signalled that demand state changed
        last_target_temp = target_temp;
        if (last_target_temp == 999) { last_target_temp = 0; }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// main loop

void loop() {
    time_t now = time(NULL);
    unsigned long millinow = millis();
    bool changed = false;
    int i;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // check timers
    for(i=0; i<num_heat_channels; ++i) {
        if (channels[i].updateTimers(now,millinow)) {
            changed |= true;
            syslogf(LOG_DAEMON|LOG_INFO,"%s triggered change",channels[i].getName());
        }
    }

    if (changed) {
        bool calling = false;
        target_temp = 0;

        // cache the interval between relay changes
        time_t relaywait = MyCfgGetInt("boiler","wait", BOILER_RELAY_WAIT);

        // see what is ready for action
        for (i=0; i<num_heat_channels; ++i) {
            if (channels[i].wantFire()) {
                calling |= true;
            }
        }
        // 999 is rendered as --- on the display
        if (target_temp == 0) { target_temp = 999; }
        if (calling) {
            // something is calling for heat
            int tt = 0;

            // action the satisfied relays
            // if we get to this loop then something IS calling for fire
            // cool down is not possible if something is calling for fire
            for (i=0; i<num_heat_channels; ++i) {
                // we must set satisfied to indicate to not send heat to this channel
                // if the channel is satisfied or this channel is not asking for heat
                // (which can include cooldown too)
                // these changes are start-of-sequence so happen immediately
                channels[i].setSatisfied((!channels[i].canFire()) || channels[i].isSatisfied());
                // action the zone valves
                channels[i].setOutput(channels[i].wantFire());
            }

            // turn the pump and boiler on as required
            for (i=0; i<num_heat_channels; ++i) {
                if (channels[i].wantFire()) {
                    syslogf(LOG_DAEMON|LOG_INFO,"%s is ready",channels[i].getName());
                    // have to wait for ack from zv before firing
                    if (channels[i].canFire()) {
                        // something is asking for heat
                        tt = max(tt, channels[i].targetTemp());
                        // give the pump and boiler time for the zv to be in place
                        o_pump_on.setState(true, now+relaywait);
                        o_boiler_on.setState(true,now+relaywait+relaywait);
                        syslogf(LOG_DAEMON | LOG_WARNING, "Run pump and boiler to %dC",tt);
                    }
                }
            }
            target_temp = tt;

        } else {
            // nothing asking for heat, what about cooldown?
            calling = false;
            for (i=0; i<num_heat_channels; ++i) {
                if (channels[i].wantCooldown()) {
                    calling |= true;
                }
            }

            if (calling) {
                // something is asking for cooldown
                // this ensures other ZVs are closed

                // action the satisfied relays immediately
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setSatisfied(channels[i].isSatisfied());
                }

                // set the zone valves immediately
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setOutput(channels[i].wantCooldown());
                }

                // and set the pump/boiler
                // boiler defaults to off anyway, pump should already be on
                for (i=0; i<num_heat_channels; ++i) {
                    if (channels[i].wantCooldown()) {
                        syslogf(LOG_DAEMON|LOG_INFO,"%s is ready for cooldown",channels[i].getName());
                    }
                }

                syslogf(LOG_DAEMON | LOG_WARNING, "Run pump for cooldown");
                o_boiler_on.setState(false);
                o_pump_on.setState(true,now+relaywait);
            } else {
                // nothing on

                o_boiler_on.setState(false);
                // give the boiler a chance to stop before stopping the pump
                o_pump_on.setState(false,now+relaywait);

                // action the satisfied relays to off, not needed
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setSatisfied(false,now+relaywait+relaywait);
                }

                // action the zone valves to off
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setOutput(false,now+relaywait+relaywait);
                }

                syslogf(LOG_DAEMON | LOG_WARNING, "Pump and boiler off");
            }
        }

        // set system outputs - via task to deal with mains sync
        // TODO should setting outputs be synchronised with setting outputs
        xTaskNotifyGive( outputtask_handle );

    } // end of something changed

    // wait a while
    delay(50);
}
