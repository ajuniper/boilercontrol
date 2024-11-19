#include <Arduino.h>
//////////////////////////////////////////////////////////////////
//
// temperature sensors
#include "projectconfig.h"
#include "pinconfig.h"
#include "displayconfig.h"
#include "tempsensors.h"
#include "tempreporter.h"
#include "mywebserver.h"
#include "tempfetcher.h"

extern int target_temp;

tempsensor_t temperatures[] = {
    // addr, name(label)
    // xxxxxxxxxxxx.tr - max 12 characters due to prefs library
    { "targettemp","Target", &target_temp },
    { "forecast.low", "Forecast", &forecast_low_temp },
    { "boiler.flow", "Flow" },
    { "boiler.rethw", "HWret" },
    { "boiler.retht", "HtRet" }
};
const size_t num_temps = (sizeof(temperatures)/sizeof(temperatures[0]));

void tempsensors_init() {
    TR_init(server);
    TF_init();
}
int tempsensor_t::getTemp() const {
    if (m_value != NULL) {
        return *m_value;
    } else {
        return int(TR_get(m_addr)+0.5);
    }
} 
int tempsensors_get(const char * name) {
    size_t i;
    int t = -999;
    for (i=0; i<num_temps; ++i) {
        if (strcmp(temperatures[i].getAddr(),name) == 0) {
            t = temperatures[i].getTemp();
            if (t == 999) { t = -999; }
            break;
        }
    }
    return t;
}

