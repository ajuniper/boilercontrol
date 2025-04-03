// system output states
#pragma once
#include "scheduledoutput.h"

typedef enum t_output_state { OUTPUT_OFF, OUTPUT_SUSPEND, OUTPUT_COOLING, OUTPUT_HEATING };

extern t_output_state o_boiler_state;
extern t_output_state o_pump_state;

extern ScheduledOutput o_boiler_on;
extern ScheduledOutput o_pump_on;
