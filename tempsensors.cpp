#include <Arduino.h>
//////////////////////////////////////////////////////////////////
//
// temperature sensors
#include "displayconfig.h"
#include "tempsensors.h"

tempsensor_t temperatures[] = {
    { "aaa", "Flow" },
    { "bbb", "HWret" },
    { "ccc", "MainRet" },
    { "ddd", "ExtRet" }
};
const size_t num_temps = (sizeof(temperatures)/sizeof(temperatures[0]));
