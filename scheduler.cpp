//////////////////////////////////////////////////////////////////////////////
//
// Scheduler
#include <Arduino.h>
#include "projectconfig.h"
#include "mywebserver.h"
#include "scheduler.h"
#include "heatchannel.h"
#include <time.h>
#include <mysyslog.h>
#include "myconfig.h"

#ifdef SHORT_TIMES
// max time before running the pump for a short while
#define CIRCULATION_TIME 600 // s
// min runtime before running sludgebuster
#define SLUDGE_UPTIME 300000 // ms
#else
// max time before running the pump for a short while
#define CIRCULATION_TIME 86400 // s
// min runtime before running sludgebuster
#define SLUDGE_UPTIME (24*60*60*1000) // ms
#endif

// task to check scheduling
static TaskHandle_t schedtask_handle = NULL;

static const char schedpage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
<title>Boiler Scheduling</title>
<style>
.schedule {
    width: 122px;
    display: grid;
    grid-gap: 8px;
    grid-template-columns: 1fr 1fr 1fr 1fr 1fr;
}
.tab {
    overflow: hidden;
    border: 1px solid #ccc;
    background-color: #f1f1f1;
}
.tab button {
    background-color: inherit;
    float: left;
    border: none;
    outline: none;
    cursor: pointer;
    padding: 5px 16px;
    transition: 0.3s;
    font-size: 14px;
}
.tab button:hover {
    background-color: #ddd;
}
.tab button.active {
    background-color: #ccc;
}
.daycontent {
    display: none;
    padding: 6px 12px;
    border: 1px solid #ccc;
    border-top: none;
}
.timeslot {
    padding: 1px 1px;
    font-size: 12px;
    font-family: sans-serif;
    text-align: right;
}
.timebox {
    border-style: solid;
    border-width: thin;
    border-collapse: separate;
}
.cold{
  background-color: white;
}

.warm{
  background-color: orange;
}

.hot{
  background-color: red;
}
</style>
<script>
var selectedDay = 0;
var selectedCh = 0;
var selectedSchedule = 0;
// holds the currently displayed schedule
var currschedule=[];
// gives the click sequence for each channel
// TODO load dynamically
var possiblesettings=[ [ "cold", "hot" ], [ "cold","warm","hot" ] ];

async function loadSchedule() {
    // unix time is Sunday=0 but ui is Monday=0/Sunday=6
    var u = "schedulerset?ch="+selectedCh+"&day="+((selectedDay+1)%%7)
    var a = await fetch(u,{method:'get'})
    var b = await a.text()
    var c = b.split(' ').map(Number);
    if (c.length != 97) {
        console.log("Did not get 96 settings for the day, got "+String(c.length));
        return;
    }
    var sch = document.getElementById(selectedSchedule).getElementsByClassName("timebox");
    var i = 0;
    for (s in sch) {
        if (typeof(sch[s]) == "object") {
            sch[s].classList.remove(possiblesettings[selectedCh][sch[s].getAttribute("data-temp")]);
            sch[s].setAttribute("data-temp",c[i]);
            sch[s].classList.add(possiblesettings[selectedCh][c[i]]);
            ++i;
        }
    }
}

async function saveScheduleCheckbox(ch,day,e) {
    var u = "schedulerset?ch="+ch+"&day="+((day+1)%%7)+"&slot="+e.id+"&state="+e.getAttribute("data-temp");
    return await fetch(u,{method:'get'})
}

async function toggleCheckbox(element) {
    var st = element.getAttribute("data-temp");
    element.classList.remove(possiblesettings[selectedCh][st]);
    st = (parseInt(st) + 1) %% possiblesettings[selectedCh].length;
    element.setAttribute("data-temp",st);
    element.classList.add(possiblesettings[selectedCh][st]);
    return saveScheduleCheckbox(selectedCh,selectedDay,element);
}

function clickChannel(evt, chNum) {
    let i, chcontent, chlinks;
    chcontent = document.getElementsByClassName("chcontent");
    for (i = 0; i < chcontent.length; i++) {
        chcontent[i].style.display = "none";
    }
    chlinks = document.getElementsByClassName("chlinks");
    for (i = 0; i < chlinks.length; i++) {
        chlinks[i].className = chlinks[i].className.replace(" active", "");
    }
    evt.currentTarget.className += " active";
    selectedCh = chNum;
    selectedSchedule = "s" + selectedCh+"."+selectedDay;
    document.getElementById("c"+selectedCh).style.display = "block";
    document.getElementById(selectedSchedule).style.display = "block";
    loadSchedule();
}

function clickDay(evt, daynum) {
    let i, daycontent, daylinks;
    daycontent = document.getElementsByClassName("daycontent");
    for (i = 0; i < daycontent.length; i++) {
        daycontent[i].style.display = "none";
    }
    daylinks = document.getElementsByClassName("daylinks");
    for (i = 0; i < daylinks.length; i++) {
        daylinks[i].className = daylinks[i].className.replace(" active", "");
    }
    evt.className += " active";
    selectedDay = daynum;
    selectedSchedule = "s" + selectedCh+"."+selectedDay;
    document.getElementById(selectedSchedule).style.display = "block";
    loadSchedule();
}

function clickDone() {
    location.href="/";
}

async function copyForwards() {
    selectedDay+=1;
    if (selectedDay >= 7) { selectedDay -= 7; }
    let f = document.getElementById(selectedSchedule);
    f = f.getElementsByTagName("input");
    let t = document.getElementById("s"+selectedCh+"."+selectedDay);
    t = t.getElementsByTagName("input");

    for(v in f) {
        tt = t[v].getAttribute("data-temp");
        ff = f[v].getAttribute("data-temp");
        if (tt != ff) {
            t[v].setAttribute("data-temp",ff);
            await toggleCheckbox(t[v]);
        }
    }
    clickDay(document.getElementById("days").children[selectedDay],selectedDay);
}

</script>
</head>
<body>
%CHANNELTABS%
<script>
let i,j, prototype
const days = ["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"];
let dt = document.getElementById("days");
for(i=0;i<days.length;++i) {
    var s = document.createElement("button");
    s.setAttribute("class", "daylinks");    
    if (i==0) { s.setAttribute("class", "daylinks active"); }
    s.setAttribute("onclick", "clickDay(this, "+i+")");
    s.innerHTML=days[i];
    dt.appendChild(s);
}
prototype = document.createElement("div");
prototype.setAttribute("class","daycontent");
var pr2 = document.createElement("div");
pr2.setAttribute("class","schedule");
prototype.appendChild(pr2);
for(i=0;i<24;++i) {
    var l = document.createElement("label");
    var c;
    l.innerHTML=""+i+":00";
    l.setAttribute("class","timeslot");
    pr2.appendChild(l);
    for (j=0; j<60; j+=15) {
        l = document.createElement("label");
        l.setAttribute("data-temp",0);
        l.setAttribute("class","timebox cold");
        l.setAttribute("onclick", "toggleCheckbox(this)");
        if (j==0){
            l.setAttribute("id",""+i+":00");
        } else {
            l.setAttribute("id",""+i+":"+j);
        }
        pr2.appendChild(l);
    }
}
let chcontent = document.getElementsByClassName("chcontent");
for (i = 0; i < chcontent.length; i++) {
    for (j=0; j<days.length; ++j) {
        chcontent[i].style.display = "none";
        let d = prototype.cloneNode(true);
        d.id = "s"+i+"."+j;
        chcontent[i].appendChild(d);
    }
}
selectedSchedule = "s" + selectedCh+"."+selectedDay;
document.getElementById("c"+selectedCh).style.display = "block";
document.getElementById(selectedSchedule).style.display = "block";
loadSchedule();
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
static String schedpage_processor(const String& var){
  //Serial.println(var);
  if(var == "CHANNELTABS"){
    String s;
    s+="<div id=\"channels\" class=\"tab\">";
    int i;
    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        s+="<button id=\"ct"+String(i)+"\" class=\"chlinks";
        s+=(i==0)?" active":"";
        s+="\" onclick=\"clickChannel(event, ";
        s+=String(i);
        s+=")\">";
        s += channels[i].getName();
        s+="</button>";
    }
    s+="<button id=\"ct3\" class=\"chlinks\" onclick=\"clickDone()\">Done</button><button id=\"ct4\" class=\"chlinks\" onclick=\"copyForwards()\">Copy to Next</button>";

    s+="</div><div id=\"days\" class=\"tab\"></div>";

    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        s+="<div id=\"c"+String(i)+"\" class=\"chcontent\"></div>";
    }

    return s;
  }
  return String();
}

static void sched_set(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = nullptr;
    String x,y;
    String z("Invalid Parameters");
    int rc = 400;
    // GET /sched_set?ch=X&day=D&slot=H:MM&state=012
    // GET /sched_set?ch=X&day=D&slot=H:MM
    // GET /sched_set?ch=X&day=D
    // d=0-6 M-S
    if (!request->hasParam("ch")) {
        z = "Channel missing";
    } else if (!request->hasParam("day")) {
        z = "Day missing";
    } else {
        // get common parameters
        x = request->getParam("ch")->value();
        int ch = x.toInt();
        if ((ch >= 0) && (ch < num_heat_channels)) {
            x = request->getParam("day")->value();
            int j = x.toInt();
            const char * err;

            if (!request->hasParam("slot")) {
                // no slot given so return the whole days worth
                rc = 200;
                z = "";
                int h;
                int m;
                for (h=0; h<24; ++h) {
                    for (m=0; m<4; ++m) {
                        z += String(channels[ch].getScheduler().get(j,h,m));
                        z += " ";
                    }
                }

            } else if (!request->hasParam("state")) {
                // serve current state of scheduler slot
                rc = 200;
                unsigned char i = 0;
                if ((err = channels[ch].getScheduler().get(j,request->getParam("slot")->value(),i)) == NULL) {
                    z = String(i);
                } else {
                    z = err;
                }

            } else {
                // setting the specific slot
                x = request->getParam("state")->value();
                unsigned char i = x.toInt();
                if ((err = channels[ch].getScheduler().set(j,request->getParam("slot")->value(),i)) == NULL) {
                    z = "Channel ";
                    z+=channels[ch].getName();
                    z+=" day ";
                    z+=String(j);
                    z+=" slot ";
                    z+=request->getParam("slot")->value();
                    z+=" set to ";
                    z+=x;
                    rc = 200;
                } else {
                    z = err;
                }
            }
        } else {
            z = "Invalid channel "+x;
        }
    }
    response = request->beginResponse(rc, "text/plain", z);
    response->addHeader("Connection", "close");
    request->send(response);
}

static void warmup_set (AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = nullptr;
    String x,y;
    // GET /targettemp?ch=N
    // GET /targettemp?ch=N&temp=X where X is new target temp
    if (!request->hasParam("ch") && request->hasParam("t")) {
        response = request->beginResponse(400, "text/plain", "channel or time missing");
    } else {
        x = request->getParam("ch")->value();
        int ch = x.toInt();
        if ((ch >= 0) && (ch <= num_heat_channels)) {
            if (request->hasParam("t")) {
                x = request->getParam("t")->value();
                int i = x.toInt();
                channels[ch].getScheduler().setWarmup(i);
                y = "Setting base warmup time for  ";
                y+=channels[ch].getName();
                y+=" to ";
                y+=x;
                response = request->beginResponse(200, "text/plain", y);

                String s = "warmup.";
                s += ch;
                if (prefs.putInt(s.c_str(), i) != 4) {
                    syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to save %s",s.c_str());
                }
            } else {
                y = "Base warmup time for ";
                y+=channels[ch].getName();
                y+=" is ";
                y+= String(channels[ch].getScheduler().getBaseWarmup());
                response = request->beginResponse(200, "text/plain", y);
            }
        } else {
            x = "Invalid channel "+x;
            response = request->beginResponse(400, "text/plain", x);
        }
    }
    response->addHeader("Connection", "close");
    request->send(response);
}

Scheduler::Scheduler(HeatChannel & aChannel, int aSetback) :
    mChannel(aChannel),
    mBaseWarmup(aSetback),
    mLastChange(0),
    mLastSchedule(0)
{
    memset(mSchedule, 0, sizeof(mSchedule));
}

void Scheduler::readConfig()
{
    // read saved setback config
    String s = "warmup.";
    s+=mChannel.getId();
    mBaseWarmup = prefs.getInt(s.c_str(), mBaseWarmup);

    // read saved schedule
    s="sched.";
    s+=mChannel.getId();
    if (prefs.getBytes(s.c_str(), (unsigned char *)mSchedule,sizeof(mSchedule)) != sizeof(mSchedule)) {
        syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to read %s",s.c_str());
    }
}

time_t Scheduler::getWarmup() const {
    // TODO factor in some adjustments
    return mBaseWarmup;
}

void Scheduler::checkSchedule(int d, int h, int m)
{
    // calculate current warmup time
    time_t t = getWarmup();

    struct tm w2;
    time_t now = time(NULL);
    time_t starttime = now;
    time_t c = 0;

    // do not keep calculating until any current cycle is completed
    if (mLastSchedule > now) {
        return;
    }

    // if anything between now and warmup is scheduled on then we should go on
    // if (starttime + warmup) is scheduled on
    time_t t2 = t;
    do {
        localtime_r(&starttime, &w2);
        if (mSchedule[w2.tm_wday][w2.tm_hour][w2.tm_min/15] != 0) {
            // we have found a slot which is on within the warmup window
            // so drop in to the next loop
            break;
        }

        if (c == 0) {
            // first pass, align to the next 15 minute slot
            c = min(((time_t)900) - (starttime % 900),t2);
        } else {
            // other passes, move by 15 mins or max warmup
            c = min(((time_t)900), t2);
        }
        starttime += c;
        t2 -= c;
        // go round the loop again until we reach the end of the
        // warmup cycle, and then one more pass.
        // t2 > 0 means we have not reached the end of the warmup interval
        // on the pass where t2 goes to 0, c will be >0
        // then after that c goes to 0
    } while ((t2 > 0) || (c > 0));

    c = 0; // number of 15 minute intervals
    // did we find a start slot within the warmup window?
    // if we did, work out how long that interval is for
    // TODO there's a bug here if schedule says on and off and on
    // within the warmup window
    // limit on c is to prevent infinite loop if everything is on
    while ((mSchedule[w2.tm_wday][w2.tm_hour][w2.tm_min/15] != 0) &&
           (c < 672)) {

        // calculate end time
        ++c;

        // bump time breakdown onwards by a 15 minute slot
        w2.tm_min+=15;
        if (w2.tm_min >= 60) {
            w2.tm_min-=60;
            ++w2.tm_hour;
            if (w2.tm_hour >= 24) {
                w2.tm_hour -= 24;
                ++w2.tm_wday;
                if (w2.tm_wday >= 7) {
                    w2.tm_wday -= 7;
                }
            }
        }
    }

    // schedule says we need to turn on
    if (c > 0) {
        // set runtime
        t = (c * 15 * 60);
        if (starttime == now) {
            // round down in case we are already in the specified slot
            // (e.g. boot time or if warmup time is not a multiple of 15 minutes)
            // only the case if we found an enabled slot in the first
            // pass which means we are in the slot already
            t -= (now % 900);
        } else {
            // add time from now to scheduled start
            t += (starttime-now);
        }

        // remember the expiry time for when we would turn the channel off
        // this gates us trying to turn the channel back on again
        mLastSchedule = t + now;

        // tell the system to do its stuff
        c = mChannel.getTimer();
        if (c > 0) {
            // channel already has some run time, adjust our delta
            // to take that into account
            if ((c - now) > t) {
                // channel already has more runtime than we were to add
                // so we do nothing
                t = 0;
            } else {
                t -= (c - now);
            }
        }
        // take action if required
        if (t > 0) {
            syslog.logf(LOG_DAEMON|LOG_WARNING, "Scheduler starts %s for %d seconds",mChannel.getName(),t);
            mChannel.adjustTimer(t);
        }

    } else if ((mChannel.getTimer() <= now) &&
               ((mChannel.lastTime() != 0) || (millis() > SLUDGE_UPTIME)) &&
               ((now - mChannel.lastTime()) > CIRCULATION_TIME)) {
        // if >24h since last run
        // set timer for (now-1) / -2
        // sludge stopper
        syslog.logf(LOG_DAEMON|LOG_WARNING, "Scheduler run %s sludge buster",mChannel.getName());
        mChannel.adjustTimer(-2);
    }
}

time_t Scheduler::lastChange()
{
    // returns time of last change, 0 if none pending
    return mLastChange;
}

void Scheduler::saveChanges()
{
    if (mLastChange == 0) { return; }
    String s="/sched.";
    s+=mChannel.getId();
    if (prefs.putBytes(s.c_str(), (unsigned char *)mSchedule,sizeof(mSchedule)) != sizeof(mSchedule)) {
        syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to write %s",s.c_str());
    }
    mLastChange = 0;
}

static void scheduler_run(void *)
{
    time_t t,u,v;
    int i,j;
    int d,h,m;
    struct tm now;
    delay(5000);
    while(1) {
        // for each channel
        t = 0;
        v = u = time(NULL);
        localtime_r(&u,&now);

        for(i=0; i<num_heat_channels; ++i) {
            HeatChannel &ch(channels[i]);

            // if channel is enabled
            if (!ch.getActive()) { continue; }

            // check channel for pending save
            u = ch.getScheduler().lastChange();

            // pick the newest pending save
            if ((u != 0) && (u > t)) { t = u; }

            // skip if already running
            //if (ch.getTimer() > 0) { continue; }

            // process this channel
            ch.getScheduler().checkSchedule(now.tm_wday,now.tm_hour,now.tm_min);
        }

        // if pending save older than 10s then save changes
        u = time(NULL);
        if ((t > 0) && ((u-t) > 10)) {
            for(i=0; i<num_heat_channels; ++i) {
                channels[i].getScheduler().saveChanges();
            }
        }

        // sleep to start of next minute
        v = v%60;
        v = 60-v;
        //delay(v*1000);
        j = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( v*1000) );
        if (j > 0) {
            // somebody changed something
            // wait a couple of seconds and run scheduler
            delay(5000);
        }
    }
}

static const char * extractTimeslot(int d, const String & hm, int &h, int &m)
{
    // validate inputs
    if (d < 0 || d > 6) { return "Invalid day"; }
    // extract h
    h = hm.toInt();
    if (h < 0 || h>23) { return "Invalid hour"; }
    // extract m
    m = hm.indexOf(":");
    if (m < 0) { return "No colon found"; }
    m = hm.substring(m+1).toInt();
    if (m != 0 && m != 15 && m != 30 && m != 45) { return "Invalid minute"; }
    // convert to quarter hours
    m = m/15;
    return NULL;
}

const char * Scheduler::get(int d, const String & hm, unsigned char & state)
{
    int h;
    int m;
    const char * ret = extractTimeslot(d, hm, h, m);
    if (ret != NULL) { return ret; }
    state = mSchedule[d][h][m];
    return NULL;
}

const char * Scheduler::set(int d, const String & hm, unsigned char state)
{
    int h;
    int m;
    const char * ret = extractTimeslot(d, hm, h, m);
    if (ret != NULL) { return ret; }
    mSchedule[d][h][m] = state;
    mLastChange = time(NULL);
    xTaskNotifyGive( schedtask_handle );
    return NULL;
}

void scheduler_setup() {
    int i;
    for (i=0; i<num_heat_channels; ++i) {
        channels[i].getScheduler().readConfig();
    }
    server.on("/scheduler", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", schedpage, schedpage_processor);
    });
    server.on("/schedulerset", HTTP_GET, sched_set);
    server.on("/warmup", HTTP_GET, warmup_set);
    xTaskCreate(scheduler_run, "scheduler", 10000, NULL, 1, &schedtask_handle);
}
