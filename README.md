# Boiler Controller

ESP32 based heating and hot water controller with LCD touchscreen, RTC, web page, temperature sensors for boiler cycling.

Being created to try to make an aging oil boiler more efficient with different target temperatures for different circuits, with setback based on outside temperature, with cooldown cycles to use the heat left in the system when the schedule is completed.
More description, photos and screenshots coming soon.

## URLs

All requests are `GET` requests (yuk!)

* `/` and `/boiler` gives current status and management controls

* `/scheduler` gives html page for setting schedules
* `/schedset?ch=X&day=D&slot=H:MM&state=01` for updating individual slots in schedules, day = 0-6=M-S, state = 0 or 1
* `/schedset?ch=X&day=D&slot=H:MM` retrieve given schedule slot
* `/schedset?ch=X&day=D` retrieve schedule for day
* `/config?name=schedule&id=N&value=X` set schedule for channel N, must be 96 0/1/2 characters

* `/heat?ch=N&q=X` manual timer for channel, where X is seconds to run (0=off, -1=forever)
* `/config?name=chactive&id=N&value=X` set channel active or inactive

* `/config?name=wuBase&id=N&value=X` set time channel starts before requested on time
* `/config?name=wuThrsh&id=N&value=X' set temp below which channel N will start early
* '/config?name=wuScale&id=N&value=X' set float scaling factor to calculate advance time from amount below threshold
* '/config?name=wuLimit&id=N&value=X' set max amount channel can advance by

* '/config?name=bstThrsh&id=N&value=X' set temp below which channel N will switch to boosted temperature

* `/config?name=orThrsh&id=N&value=X' set temp below which channel N will finish late
* '/config?name=orScale&id=N&value=X' set float scaling factor to calculate overrun time from amount below threshold
* '/config?name=orLimit&id=N&value=X' set max amount channel can extend by

* `/config?name=tgttmp&id=N` reports current target hot temperature for channel N
* `/config?name=tgttmp&id=N&value=X` sets target hot temperature for channel N
* `/config?name=tgttmp2&id=N` reports current target warm temperature for channel N
* `/config?name=tgttmp2&id=N&value=X` sets target warm temperature for channel N
* `/config?name=cooltmp&id=N&value=X` sets min cooldown temperature for channel N
* `/config?name=coolrun&id=N&valueX` sets the cooldown run time for channel N (default 120, 0 = off)
* `/config?name=circtime&id=N&valueX` sets the sludge buster max time to sit idle in seconds (default 86400 = 1 day, 0 = off)
* `/config?name=circrun&id=N&valueX` sets the sludge buster run time (default 120, 0 = off)
* `/config?name=mainsthrsh&id=N&valueX` sets the mains detection threshold for pin N (default 350, pins 34, 35 ,36, 39 )

* `/config?name=boiler&id=cyct&value=X` sets min off time for cycling boiler (default 120 seconds)
* `/config?name=boiler&id=cycpc&value=X` sets percentage which flow must drop to end cycle (default 10%)
* `/config?name=boiler&id=wait&value=X` sets time in S to wait after setting each relay (3 seconds)

* `/config?name=showbutt&id=0&value=X` enable or disable showing button regions, 1=regions+touch, 2=regionsboxed+touch, 3=2+syslog touch

Provided by the updater library:
* `/update` gives page for doing OTA updates
* `/update` use __POST__ request with body containing new firmware.bin
* `/reboot` reboots the system

Provided by tempreporter library:
* `/temperatures` reports current DS18B20 readings
* `/api?id=XXX` reports current temperature for sensor named `XXX`
* `/config?name=trremap&id=X&value=AAA+NNN+DIS` sets name of sensor `AAA` to `NNN` (where `XXX` is typically sensor MAC address), optionally disables
* `/fake?id=XXX&temp=t.tt` creates fake sensor `XXX` with value `t.tt` (used for testing)
* `/config?name=trpin&id=(18b20|dht11)[&value=N]` sets or gets the pin associated with the different sensors (reboot after to take effect)

Provided by the tempfetcher library
* `/config?name=fcst&id=rate&value=X` sets the minutes between fetching forecasts
* `/config?name=fcst&id=ahead&value=X` sets the number of hours ahead to look for lowest temperature
* `/config?name=fcst&id=behind&value=X` sets the number of hours behind to look for lowest temperature
* `/config?name=weather&id=url&value=x` sets the URL to retrieve weather forecasts from

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

## configuring temperature sensors
```
curl -s 'http://boiler/config?name=trremap&id=0&value=28-ff641f79b51c46+boiler.flow+1'
curl -s 'http://boiler/config?name=trremap&id=1&value=28-ff641f79b7e8c2+boiler.rethw+1'
curl -s 'http://boiler/config?name=trremap&id=2&value=28-ff641f79b7d4df+boiler.retht+1'
```

## My House Settings

### Boiler

Extend zone valve duration a little beyond default
```
boiler.cyct = 120
boiler.cycpc = 10
boiler.wait = 5
fcst.ahead = 5
```

### Hot Water

No warm up time to be added

```
wuBase = 0
```
Advance if 0 degrees in forecast by up to 15 minutes
```
wuThrsh = 0
wuScale = 180
wuLimit = 900
```
No boost or overrun
```
bstThrsh = -100
orThrsh = -100
orScale = 0
orLimit = 0
```
Target temperature 65 degrees, only one run temperature
```
tgttmp = 65
tgttmp2 = -1
```
Only run cooldown cycle for 3 minutes or until less than 60 degrees
```
cooltmp = 60
coolrun = 180
```
Circulate daily for a minute for sludge buster
```
circtime = 86400
circrun = 60
```

### Heating

Heating always needs 45min warmup
```
wuBase = 2700
```
Extend heating by up to an hour if forecast less than 8 degrees (extra 4 minutes per degree)
```
wuThrsh = 8
wuScale = 240
wuLimit = 3600
```
Force heating hot if less than -3 degrees forecast
```
bstThrsh = -3
```
Extend heating by up to half an hour if forecast less than -3 (extra 3 minutes per degree)
```
orThrsh = -3
orScale = 180
orLimit = 1800
```
Temperatures are 50 and 65 degrees
```
tgttmp = 65
tgttmp2 = 50
```
Overrun until boiler temp is 45 or for 15 minutes
```
cooltmp = 45
coolrun = 900
```
Circulate daily for 2 minutes for sludge buster
```
circtime = 86400
circrun = 120
```

## Config backup

```
curl -s 'http://boiler/configlist' | while read a b ; do [[ $a = myconfig/* ]] || continue ; c="${a/./\&id=}" ; c="${c/myconfig\//http:\/\/boiler/config?name=}" ; v=$(curl -s "$c") ; echo "curl -s \"$c&value=$v\"" ; done

```
