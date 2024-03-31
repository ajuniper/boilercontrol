# Boiler Controller

ESP32 based heating and hot water controller with LCD touchscreen, RTC, web page, temperature sensors for boiler cycling.

Being created to try to make an aging oil boiler more efficient with different target temperatures for different circuits, with setback based on outside temperature, with cooldown cycles to use the heat left in the system when the schedule is completed.
More description, photos and screenshots coming soon.

## URLs

All requests are `GET` requests (yuk!)

* `/` and `/boiler` gives current status and management controls
* `/scheduler` gives html page for setting schedules
* `/schedulerset?ch=X&day=D&slot=H:MM&state=01` for updating individual slots in schedules, day = 0-6=M-S, state = 0 or 1
* `/schedulerset?ch=X&day=D&slot=H:MM` retrieve given schedule slot
* `/schedulerset?ch=X&day=D` retrieve schedule for day
* `/heat?ch=N&q=X` set timer for channel, where X is seconds to run (0=off, -1=forever)
* `/config?name=chactive&id=N&value=X` set channel active or inactive
* `/config?name=warmup&id=N&value=X` set base warmup time for channel N to X seconds
* `/config?name=targettemp&id=N` reports current target temperature for channel N
* `/config?name=targettemp&id=N&value=X` sets target temperature for channel N
* `/config?name=cycle&id=time&value=X` sets min off time for cycling boiler (default 120 seconds)
* `/config?name=cycle&id=percent&value=X` sets percentage which flow must drop to end cycle (default 10%)

Provided by the updater library:
* `/update` gives page for doing OTA updates
* `/update` use __POST__ request with body containing new firmware.bin
* `/reboot` reboots the system

Provided by tempreporter library:
* `/temperatures` reports current DS18B20 readings
* `/api?id=XXX` reports current temperature for sensor named `XXX`
* `/config?name=remap&id=XXX&value=YYY` sets name of sensor `XXX` to `YYY` (where `XXX` is typically sensor MAC address)
* `/fake?id=XXX&temp=t.tt` creates fake sensor `XXX` with value `t.tt` (used for testing)
* `/config?name=pin&id=(ds18b20|dht11)[&value=N]` sets or gets the pin associated with the different sensors (reboot after to take effect)
* `/config?name=tr&id=X&value=T` sets enable reporting flag for temperature channel X where X is name or addr, value 0 or 1

## Expected sensor names

Need to associate specific DS18B20 with given name via the `/remap` URL above.
Max 12 characters due to preferences library limits.

* `boiler.flow` is the sensor on the output from the boiler
* `boiler.rethw` is the sensor on the return from the hot water tank
* `boiler.retht` is the sensor on the return from the main heating circuit

## Preferences objects

pin.ds18b20
pin.dht11
remap.[sensorID]
temprep.poll
temprep.submit

wifi.hostname
wifi.ssid
wifi.password

targettemp.[ch]
chactive.[ch]
warmup.[ch]
schedule.[ch]

## Scheduler web page DOM objects

* channels tabs
* days tabs
* c0 c1 (chcontent)
  * s0.0 s0.1 s0.2 ... (daycontent)
    * form (schedule)
      * 00:00 (timeslot)
      * label
        * checkbox value="hh:mm"
      * label
        * checkbox value="hh:mm"
      * label
        * checkbox value="hh:mm"
...
