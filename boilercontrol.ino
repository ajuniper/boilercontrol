#include <mywifi.h>
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

// task to update outputs
static TaskHandle_t outputtask_handle = NULL;

// last output state
bool o_pump_on = false;
bool o_boiler_on = false;
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

//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#include "heatchannel.h"

//////////////////////////////////////////////////////////////////////////////
//
// Web Server Stuff
#include "webserver.h"

static const char * handleConfigBoiler(const char * name, const String & id, int &value) {
    if (id == "time") {
        // all ok, save the value
        return NULL;
    } else if (id == "percent") {
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
    MyCfgRegisterInt("cycle", &handleConfigBoiler);

    mytime_setup(MY_TIMEZONE, PIN_RTC_CLK, PIN_RTC_DATA, PIN_RTC_RST);
    WIFI_init("boiler");
    SyslogInit("boiler");
    xTaskCreate(output_task, "outputs", 10000, NULL, 1, &outputtask_handle);
    webserver_setup();
    tempsensors_init();
    heatchannel_setup();

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
    bool last_pump = o_pump_on;
    bool last_boiler = o_boiler_on;
    bool this_boiler = o_boiler_on;
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

            // recalculate whether the boiler should be running or not
            // this is where we are cycling the boiler to temperature
            if (last_boiler == false) {
                // boiler is requested off
                this_boiler = false;
                boiler_cycle_time = 0;
                waittime = 3600 * 1000; // 1hr
                o_boiler_state = OUTPUT_OFF;

            } else if (boiler_cycle_time > now) {
                // boiler is already cycled off by time so nothing to do
                // just wait until the end of the cycle window unless
                // something changes in the mean time
                waittime = boiler_cycle_time - now;

            } else if ((boiler_cycle_time != 0) &&
                       (flow_temp > ((last_target_temp * hysteresis)/100))) {
                // boiler is cycled off by temperature so nothing to do
                // just wait a few seconds and check again unless
                // something changes in the mean time
                waittime = 10000;

            } else if (flow_temp > last_target_temp) {
                // boiler just went over temperature

                // read the cycling config
                int short_cycle = MyCfgGetInt("cycle","time", BOILER_SHORT_CYCLE);
                hysteresis = 100 - MyCfgGetInt("cycle","percent", 10);

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
                this_boiler = true;
                o_boiler_state = OUTPUT_HEATING;
                if (boiler_cycle_time != 0) {
                    // boiler just finished cycling
                    syslogf(LOG_DAEMON | LOG_WARNING, "Cycling boiler back on, temp now %dC",flow_temp);
                    boiler_cycle_time = 0;
                }
            }

            // TODO wait for mains to be at 0v
            // update the outputs
            digitalWrite(PIN_O_PUMP_ON, last_pump?RELAY_ON:RELAY_OFF);
            digitalWrite(PIN_O_BOILER_ON, this_boiler?RELAY_ON:RELAY_OFF);

            n = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( waittime) );
        } // end of wait for signal/timeout

        last_pump = o_pump_on;
        if (last_boiler != o_boiler_on) {
            if (o_boiler_on) {
                syslogf(LOG_DAEMON | LOG_WARNING, "Turning boiler on, temp now %dC",tempsensors_get("boiler.flow"));
            }
            last_boiler = o_boiler_on;
            boiler_cycle_time = 0;
        }
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
        o_pump_on = false;
        o_boiler_on = false;
        target_temp = 0;

        // see what is ready for action
        for (i=0; i<num_heat_channels; ++i) {
            if (channels[i].wantFire()) {
                calling |= true;
                syslogf(LOG_DAEMON|LOG_INFO,"%s is ready",channels[i].getName());
                if (channels[i].canFire()) {
                    // something is asking for heat
                    o_pump_on = true;
                    o_boiler_on = true;
                    // boiler state is controlled from output handler
                    int t = channels[i].targetTemp();
                    if (t > target_temp) { target_temp = t; }
                    syslogf(LOG_DAEMON | LOG_WARNING, "Run pump and boiler to %dC",target_temp);
                }
            }
        }
        // 999 is rendered as --- on the display
        if (target_temp == 0) { target_temp = 999; }
        if (calling) {
            // something is calling for heat

            // action the satisfied relays
            // if we get to this loop then something IS calling for fire
            // cool down is not possible if something is calling for fire
            for (i=0; i<num_heat_channels; ++i) {
                // we must set satisfied to indicate to not send heat to this channel
                // if the channel is satisfied or this channel is not asking for heat
                // (which can include cooldown too)
                channels[i].setSatisfied((!channels[i].canFire()) || channels[i].isSatisfied());
            }
            // TODO delay needed?

            // action the zone valves
            for (i=0; i<num_heat_channels; ++i) {
                channels[i].setOutput(channels[i].wantFire());
            }

            // update what the pump is doing
            o_pump_state = o_pump_on?OUTPUT_HEATING:OUTPUT_OFF;
            // boiler state is controlled by output task
        } else {
            // nothing asking for heat, what about cooldown?
            calling = false;
            for (i=0; i<num_heat_channels; ++i) {
                if (channels[i].wantCooldown()) {
                    calling |= true;
                    syslogf(LOG_DAEMON|LOG_INFO,"%s is ready for cooldown",channels[i].getName());
                    if (channels[i].canCooldown()) {
                        o_pump_on = true;
                        o_pump_state = OUTPUT_COOLING;
                        syslogf(LOG_DAEMON | LOG_WARNING, "Run pump for cooldown");
                    }
                }
            }

            if (calling) {
                // something is asking for cooldown
                // this ensures other ZVs are closed

                // action the satisfied relays
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setSatisfied(channels[i].isSatisfied());
                }
                // TODO delay needed?

                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setOutput(channels[i].wantCooldown());
                }
            } else {
                // nothing on
                o_pump_state = OUTPUT_OFF;

                // action the satisfied relays to off, not needed
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setSatisfied(false);
                }
                // TODO delay needed?

                // action the zone valves to off
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setOutput(false);
                }

                syslogf(LOG_DAEMON | LOG_WARNING, "Pump and boiler off");
            }
        }

        // set system outputs - via task to deal with mains sync
        // TODO should setting outputs be synchronised with setting outputs
        xTaskNotifyGive( outputtask_handle );
    }

    // wait a while
    delay(50);
}
