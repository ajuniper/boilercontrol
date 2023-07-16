#include <Arduino.h>
//////////////////////////////////////////////////////////////////
//
// temperature sensors
#include "pinconfig.h"
#include "displayconfig.h"
#include "tempsensors.h"
#include "tempreporter.h"
#include "mywebserver.h"

tempsensor_t temperatures[] = {
    { "boiler.flow", "Flow" },
    { "boiler.rethw", "HWret" },
    { "boiler.retmain", "MainRet" },
    { "boiler.retext", "ExtRet" }
};
const size_t num_temps = (sizeof(temperatures)/sizeof(temperatures[0]));

void tempsensors_init() {
    TR_init(server, PIN_1WIRE);
}
int tempsensor_t::getTemp() const {
    return int(TR_get(m_name)+0.5);
} 
