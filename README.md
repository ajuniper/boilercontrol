# Boiler Controller

ESP32 based heating and hot water controller with LCD touchscreen, RTC, web page, temperature sensors for boiler cycling.

Being created to try to make an aging oil boiler more efficient with different target temperatures for different circuits, with setback based on outside temperature, with cooldown cycles to use the heat left in the system when the schedule is completed.
More description, photos and screenshots coming soon.

## URLs

All requests are `GET` requests (yuk!)

* `/` and `/boiler` gives current status and management controls
* `/temperatures` reports current DS18B20 readings
* `/api?id=XXX` reports current temperature for sensor named `XXX`
* `/remap?id=XXX&to=YYY` sets name of sensor `XXX` to `YYY` (where `XXX` is typically sensor MAC address)
* `/fake?id=XXX&temp=t.tt` creates fake sensor `XXX` with value `t.tt` (used for testing)
* `/scheduler` gives html page for setting schedules
* `/schedulerset?ch=X&day=D&slot=H:MM&state=01` for updating individual slots in schedules, day = 0-6=M-S, state = 0 or 1
* `/heat?ch=N&q=X` set timer for channel, where X is seconds to run (0=off, -1=forever)
* `/heat?ch=N&a=X` set channel active or inactive
* `/targettemp?ch=N` reports current target temperature for channel
* `/targettemp?ch=N&temp=X` sets target temperature for channel
* `/update` gives page for doing OTA updates
* `/update` use __POST__ request with body containing new firmware.bin
* `/reboot` reboots the system

## LittleFS filesystem files

* `/chactive.N` contains the active/inactive setting for each channel
* `/warmup.N` contains configured setback time for channel (seconds)
* `/sched.N` contains schedule for channel
* `/targettemp.N` contains the target temperature for the channel
* `/28-XXXXXXXXXXXX` contains the name for the sensor with given MAC address

## Expected sensor names

Need to associate specific DS18B20 with given name via the `/remap` URL above.

* `boiler.flow` is the sensor on the output from the boiler
* `boiler.rethw` is the sensor on the return from the hot water tank
* `boiler.retmain` is the sensor on the return from the main heating circuit
* `boiler.retext` is the sensor on the return from the alternate heating circuit
