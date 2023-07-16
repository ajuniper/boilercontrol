#include <WiFi.h>
#include <AsyncTCP.h>
#include "time.h"
#include <WiFiUdp.h>
#include <mysyslog.h>
#include "pinconfig.h"
#include "display.h"
#include "tempsensors.h"
#include "mytime.h"

// task to update outputs
static TaskHandle_t outputtask_handle = NULL;

// last output state
bool o_pump_on = false;
bool o_boiler_on = false;

#include <my_secrets.h>
const char* ssid     = MY_WIFI_SSID;
const char* password = MY_WIFI_PASSWORD;

// syslog stuff
const char * syslog_name = "boiler";

//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#include "heatchannel.h"

//////////////////////////////////////////////////////////////////////////////
//
// Web Server Stuff
#include "webserver.h"

//////////////////////////////////////////////////////////////////////////////
//
// setup

void setup() {
    // put your setup code here, to run once:
    // Serial port for debugging purposes
    Serial.begin(115200);
    mytime_setup(MY_TIMEZONE, PIN_RTC_CLK, PIN_RTC_DATA, PIN_RTC_RST);

    xTaskCreate(wifi_task, "wifi", 10000, NULL, 1, NULL);
    xTaskCreate(output_task, "outputs", 10000, NULL, 1, &outputtask_handle);
    heatchannel_setup();
    webserver_setup();

    // start the display update task
    display_init();
}

//////////////////////////////////////////////////////////////////////////////
//
// wifi management
// separate task to not stall boot
static void wifi_task (void*)
{
    // Connect to Wi-Fi
    WiFi.setHostname("boiler");
    WiFi.begin(ssid, password);

    // wait for wifi to be ready
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.println("Connecting to WiFi..");
    }

    // Init and get the network time
    configTime(0, 0, MY_NTP_SERVER1, MY_NTP_SERVER2, MY_NTP_SERVER3);

    // Print ESP Local IP Address
    Serial.println(WiFi.localIP());
    syslog.logf(LOG_DAEMON | LOG_WARNING, "started");
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
    while(1) {
        // TODO wait for mains to be at 0v
        // update the outputs
        digitalWrite(PIN_O_PUMP_ON, o_pump_on?RELAY_ON:RELAY_OFF);
        digitalWrite(PIN_O_BOILER_ON, o_boiler_on?RELAY_ON:RELAY_OFF);

        // wait for signal
        uint32_t n = 0;
        while (n == 0) {
            n = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( 60000 ) );
        }
        Serial.println("output change");
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
            syslog.logf(LOG_DAEMON|LOG_INFO,"%s triggered change",channels[i].getName());
        }
    }

    if (changed) {
        bool calling = false;
        o_pump_on = false;
        o_boiler_on = false;

        // see what is ready for action
        for (i=0; i<num_heat_channels; ++i) {
            if (channels[i].wantFire()) {
                calling |= true;
                syslog.logf(LOG_DAEMON|LOG_INFO,"%s is ready",channels[i].getName());
                if (channels[i].canFire()) {
                    // something is asking for heat
                    o_pump_on = true;
                    o_boiler_on = true;
                    syslog.logf(LOG_DAEMON | LOG_WARNING, "Run pump and boiler");
                }
            }
        }
        if (calling) {

            // action the satisfied relays
            for (i=0; i<num_heat_channels; ++i) {
                channels[i].setSatisfied(channels[i].isSatisfied());
            }
            // TODO delay needed?

            // action the zone valves
            for (i=0; i<num_heat_channels; ++i) {
                channels[i].setOutput(channels[i].wantFire());
            }
        } else {
            // nothing asking for heat, what about cooldown?
            calling = false;
            for (i=0; i<num_heat_channels; ++i) {
                if (channels[i].wantCooldown()) {
                    calling |= true;
                    syslog.logf(LOG_DAEMON|LOG_INFO,"%s is ready for cooldown",channels[i].getName());
                    if (channels[i].canCooldown()) {
                        o_pump_on = true;
                        syslog.logf(LOG_DAEMON | LOG_WARNING, "Run pump for cooldown");
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

                // action the satisfied relays to off, not needed
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setSatisfied(false);
                }
                // TODO delay needed?

                // action the zone valves to off
                for (i=0; i<num_heat_channels; ++i) {
                    channels[i].setOutput(false);
                }

                syslog.logf(LOG_DAEMON | LOG_WARNING, "Pump and boiler off");
            }
        }

        // set system outputs - via task to deal with mains sync
        Serial.println("request output change");
        xTaskNotifyGive( outputtask_handle );
    }

    // wait a while
    delay(50);
}
