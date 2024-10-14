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
#include "tempsensors.h"
#include "outputs.h"

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
%POSSIBLESETTINGS%

async function loadSchedule() {
    // unix time is Sunday=0 but ui is Monday=0/Sunday=6
    var u = "schedset?ch="+selectedCh+"&day="+((selectedDay+1)%%7)
    var a = await fetch(u,{method:'get'})
    var b = await a.text()
    var c = b.split(' ').map(Number);
    if (c.length != 97) {
        console.log("Did not get 96 settings for the day, got "+String(c.length));
        return;
    }
    var sch = document.getElementById(selectedSchedule).getElementsByClassName("timebox");
    var i = 0;
    for (s=0; s<sch.length; ++s) {
        sch[s].classList.remove(possiblesettings[selectedCh][sch[s].getAttribute("data-temp")]);
        sch[s].setAttribute("data-temp",c[i]);
        sch[s].classList.add(possiblesettings[selectedCh][c[i]]);
        ++i;
    }
}

async function saveScheduleCheckbox(ch,day,e) {
    var u = "schedset?ch="+ch+"&day="+((day+1)%%7)+"&slot="+e.id+"&state="+e.getAttribute("data-temp");
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
    location.href="boiler";
}

async function copyForwards() {
    selectedDay+=1;
    if (selectedDay >= 7) { selectedDay -= 7; }
    let f = document.getElementById(selectedSchedule);
    f = f.getElementsByClassName("timebox");
    let t = document.getElementById("s"+selectedCh+"."+selectedDay);
    t = t.getElementsByClassName("timebox");

    for(v=0; v<f.length; ++v) {
        tt = t[v].getAttribute("data-temp");
        ff = f[v].getAttribute("data-temp");
        if (tt != ff) {
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
i = (((new Date()).getDay())+6)%7;
clickDay(document.getElementById("days").children[i],i);
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
static String schedpage_processor(const String& var){
  //Serial.println(var);
  if(var == "POSSIBLESETTINGS") {
    // var possiblesettings=[ [ "cold", "hot" ], [ "cold","warm","hot" ] ];
    int i;
    int j;
    String s("var possiblesettings=[");
    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        if (i>0) { s += ","; }
        // off(cold) is always available
        s += "[\"cold\"";
        if (channels[i].targetTemp2() > -1) {
            s += ",\"warm\"";
        }
        // on(hot) is always available
        s += ",\"hot\"]";
    }
    s+="];";
    return s;
  } else if(var == "CHANNELTABS"){
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
                        // convert to character
                        z += ((char)(channels[ch].getScheduler().get(j,h,m)+48));
                        z += " ";
                    }
                }

            } else if (!request->hasParam("state")) {
                // we have a slot but no new state for it, so just
                // serve current state of scheduler slot
                rc = 200;
                char i = 0;
                if ((err = channels[ch].getScheduler().get(j,request->getParam("slot")->value(),i)) == NULL) {
                    z = ((char)(i+48));
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

Scheduler::Scheduler(HeatChannel & aChannel, int aSetback) :
    mChannel(aChannel),
    mBaseWarmup(aSetback),

    mAdvanceStartTemp(12),
    mAdvanceRate(0), // disabled by default
    mAdvanceLimit(0),

    mRunHotterTemp(0),

    mExtendStartTemp(4),
    mExtendRate(0), // disabled by default
    mExtendLimit(0),
    mSludgeInterval(CIRCULATION_TIME),

    mLastChange(0),
    mLastSchedule(0),
    mCancelledUntil(0),
    mLastTarget(-1)
{
    memset(mSchedule, 0, sizeof(mSchedule));
}

void Scheduler::readConfig()
{
    // read saved setback config
    mBaseWarmup = MyCfgGetInt("wuBase", String(mChannel.getId()), mBaseWarmup);
    mAdvanceStartTemp = MyCfgGetInt("wuThrsh", String(mChannel.getId()), mAdvanceStartTemp);
    mAdvanceRate = MyCfgGetFloat("wuScale", String(mChannel.getId()), mAdvanceRate);
    mAdvanceLimit = MyCfgGetInt("wuLimit", String(mChannel.getId()), mAdvanceLimit);
    mRunHotterTemp = MyCfgGetInt("bstThrsh", String(mChannel.getId()), mRunHotterTemp);
    mExtendStartTemp = MyCfgGetInt("orThrsh", String(mChannel.getId()), mExtendStartTemp);
    mExtendRate = MyCfgGetFloat("orScale", String(mChannel.getId()), mExtendRate);
    mExtendLimit = MyCfgGetInt("orLimit", String(mChannel.getId()), mExtendLimit);
    mSludgeInterval = MyCfgGetInt("circtime", String(mChannel.getId()), mSludgeInterval);

    // read saved schedule (holds chars '0','1','2')
    String s = MyCfgGetString("schedule", String(mChannel.getId()), String());
    if (s.length() != sizeof(mSchedule)) {
        syslogf(LOG_DAEMON|LOG_ERR,"Failed to read %s",s.c_str());
    } else {
        int i,j,k,l=0;
        for (i=0; i<7; ++i) {
            for (j=0; j<24; ++j) {
                for (k=0; k<4; ++k) {
                    // mSchedule always holds 0/1/2
                    mSchedule[i][j][k] = s[l]&3;
                    ++l;
                }
            }
        }
    }
}

// factor in some adjustments
// basewarmup + 
// advance start time
//   if temp < t1 then add min(((temp - t1) * m),limit)
time_t Scheduler::getWarmup() const {
    time_t w = mBaseWarmup;
    int temp = tempsensors_get("forecast.low");
    if (temp < mAdvanceStartTemp) {
        int adj = round(((float)(mAdvanceStartTemp - temp)) * mAdvanceRate);
        if (adj > mAdvanceLimit) {
            adj = mAdvanceLimit;
        }
        w += adj;
    }
    return w;
}

// factor in some adjustments
// run longer
//   if temp < t3 then extend duration by min(((temp - t3) * m),limit)
time_t Scheduler::getExtend() const {
    int adj = 0;
    int temp = tempsensors_get("forecast.low");
    if (temp < mExtendStartTemp) {
        adj = round(((float)(mExtendStartTemp - temp)) * mExtendRate);
        if (adj > mExtendLimit) {
            adj = mExtendLimit;
        }
    }
    return adj;
}

// factor in some adjustments
// run hotter
//   if temp < t2 then use "hot" not "warm"
bool Scheduler::getBoost() const {
    int temp = tempsensors_get("forecast.low");
    return (temp <= mRunHotterTemp);
}

void Scheduler::checkSchedule(int d, int h, int m)
{
    // calculate current warmup time
    time_t t = getWarmup();
    time_t w = t;

    struct tm w2;
    time_t now = time(NULL);
    time_t starttime = now;
    time_t c = 0;
    int targettemp=-1;

    // if anything between now and warmup is scheduled on then we should go on
    // if (starttime + warmup) is scheduled on
    time_t t2 = t;
    do {
        localtime_r(&starttime, &w2);
        if (mSchedule[w2.tm_wday][w2.tm_hour][w2.tm_min/15] != 0) {
            // we have found a slot which is on within the warmup window
            // so drop in to the next loop
            // remember the first temperature setting to use
            targettemp = mSchedule[w2.tm_wday][w2.tm_hour][w2.tm_min/15];
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

    // grab the current run end time
    time_t currTimer = mChannel.getTimer();

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

        // add any temperature compensation on to the end of our duration
        c = getExtend();
        t += c;

        // work out when we would switch things off
        t2 = t + now;

        // tell the system to do its stuff
        bool changed = (t > 0);

        if (currTimer > 0) {
            // channel is already running with a timed end period
            // determine if we are permitted to change the run time
            if (currTimer == t2) {
                // same run time as we are going to set up this time
                // no action required
                changed = false;
            } else if (currTimer == mLastSchedule) {
                // same run time as we set up last time
                // but looks like something changed
                // we can shorten the time because it's one we set

                // set our delta to take that into account
                // (this can be negative)
                t = (t2 - currTimer);

            } else {
                // channel has some run time but we didn't set that so we
                // cannot shorten it
                if (currTimer > t2) {
                    // channel already has more runtime than we were to add
                    // so we do nothing
                    t = 0;
                    changed = false;
                } else {
                    // set our run time to extend by the amount called for
                    // to turn off at scheduled time
                    t = (t2 - currTimer);
                }
            }
        } else {
            // channel is currently not running
            if ((now < mCancelledUntil) && (mLastSchedule == mCancelledUntil)) {
                // user cancelled a preceding request so we do not start a new request
                // until that time has passed, unless we decide we want to finish at
                // a different time
                t = 0;
                changed = false;
            }
        }
        if ((t > 5) || (t < -5)) {
            // fall thru, we must action it
        } else {
            // small change, ignore
            changed = false;
        }

        // turn it up if it is cold
        if (getBoost()) { targettemp = 2; }

        // take action if required
        if (changed) {
            // remember the expiry time for when we would turn the channel off
            // this gates us trying to turn the channel back on again
            mLastSchedule = t2;

            // tell the world
            syslogf(LOG_DAEMON|LOG_WARNING, "Scheduler starts %s for %d seconds (warmup %d extend %d), setting %d",mChannel.getName(),t,w,c,targettemp);
            mChannel.setTargetTempBySetting(targettemp);
            mLastTarget = targettemp;
            mChannel.adjustTimer(t2);
        } else {
            // no need to change the scheduling time

            // check to see if the target temperature needs adjusting
            if (targettemp != mLastTarget) {
                mChannel.setTargetTempBySetting(targettemp);
                mLastTarget = targettemp;
                syslogf(LOG_DAEMON|LOG_WARNING, "Scheduler adjusts %s setting %d",mChannel.getName(),targettemp);
            }
        }
    } else {
        // schedule says we do not need to run

        // if we just shortened the schedule then we may need to turn the
        // boiler off
        if ((currTimer > 0) && (currTimer == mLastSchedule)) {
            // boiler is running because the scheduler turned it on
            // therefore we must turn it off
            mChannel.setTargetTempBySetting(0);
            mChannel.adjustTimer(CHANNEL_TIMER_OFF);
            syslogf(LOG_DAEMON|LOG_WARNING, "Scheduler stops %s, schedule shortened to off",mChannel.getName());
        }

        // sludge stopper
        // if channel is off and >24h since last ran (or up for long enough)
        // check this here before resetting mLastSchedule otherwise the sludge
        // buster triggers before the main task stops the burner
        if ((mSludgeInterval > 0) &&
            (currTimer <= now) &&
            (mLastSchedule == 0) &&
            ((mChannel.lastTime() != 0) || (millis() > SLUDGE_UPTIME)) &&
            ((now - mChannel.lastTime()) > mSludgeInterval)) {
            // we can only run the sludge buster if all channels are inactive
            if (o_boiler_state == OUTPUT_OFF) {
                syslogf(LOG_DAEMON|LOG_WARNING, "Scheduler run %s sludge buster",mChannel.getName());
                mChannel.adjustTimer(CHANNEL_TIMER_SLUDGE);
            }
        }

        // channel is now off
        mLastSchedule = 0;
        mLastTarget = 0;

    }
}

// called by heat channel to say the user cancelled the current request
// if it's one we had requested then we must remember not to request again
// until the current slot has passed
void Scheduler::turnedOff(time_t endtime)
{
    if (endtime == mLastSchedule) {
        // the request was one of ours
        // remember to not start again for a while
        mCancelledUntil = endtime;
    }
}

// called by the heat channel to trigger a schedule recalculation
// e.g. if channel got turned on
void Scheduler::wakeUp()
{
    xTaskNotifyGive( schedtask_handle );
}

time_t Scheduler::lastChange()
{
    // returns time of last config change, 0 if none pending
    return mLastChange;
}

void Scheduler::saveChanges()
{
    if (mLastChange == 0) { return; }
    mLastChange = 0;
    String s;
    int i,j,k;
    char c[2];
    c[1] = 0;
    // save to preferences (holds chars '0','1','2')
    for(i=0; i<7; ++i) {
        for(j=0; j<24; ++j) {
            for(k=0; k<4; ++k) {
                c[0] =(mSchedule[i][j][k]+48);
                s+=c;
            }
        }
    }
    MyCfgPutString("schedule",String(mChannel.getId()),s);
    syslogf(LOG_DAEMON|LOG_ERR,"Saved schedule for %s",mChannel.getName());
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

            // check channel for pending save
            u = ch.getScheduler().lastChange();

            // pick the newest pending save
            if ((u != 0) && (u > t)) { t = u; }

            // skip if channel is disabled
            if (!ch.getActive()) { continue; }

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

// always returns 0/1/2
const char * Scheduler::get(int d, const String & hm, char & state) const
{
    int h;
    int m;
    const char * ret = extractTimeslot(d, hm, h, m);
    if (ret != NULL) { return ret; }
    state = mSchedule[d][h][m];
    return NULL;
}

// accepts 0/1/2 as numeric or ascii
void Scheduler::set(int d, int h, int m, char state)
{
    if (state == '0' || state == '1' || state == '2') {
        mSchedule[d][h][m] = state-48;
    } else if (state == 0 || state == 1 || state == 2) {
        mSchedule[d][h][m] = state;
    } else {
        return;
    }
    mLastChange = time(NULL);
    xTaskNotifyGive( schedtask_handle );
}
const char * Scheduler::set(int d, const String & hm, char state)
{
    int h;
    int m;
    const char * ret = extractTimeslot(d, hm, h, m);
    if (ret != NULL) { return ret; }
    set(d,h,m,state);
    return NULL;
}

static const char * cfg_set_warmup(const char * name, const String & id, int &value) {
    int i = id.toInt();
    if ((i < 0) || (i >= num_heat_channels)) {
        return "Invalid channel";
    } else if (strcmp(name, "wuBase") == 0) {
        channels[i].getScheduler().setWarmup(value);
    } else if (strcmp(name, "wuThrsh") == 0) {
        channels[i].getScheduler().setAdvanceStartTemp(value);
    } else if (strcmp(name, "wuLimit") == 0) {
        channels[i].getScheduler().setAdvanceLimit(value);
    } else if (strcmp(name, "bstThrsh") == 0) {
        channels[i].getScheduler().setBoostTemp(value);
    } else if (strcmp(name, "orThrsh") == 0) {
        channels[i].getScheduler().setExtendStartTemp(value);
    } else if (strcmp(name, "orLimit") == 0) {
        channels[i].getScheduler().setExtendLimit(value);
    } else if (strcmp(name, "circtime") == 0) {
        channels[i].getScheduler().setSludgeInterval(value);
    } else {
        return "Invalid selector";
    }
    return NULL;
}

static const char * cfg_set_warmup_float(const char * name, const String & id, float &value) {
    int i = id.toInt();
    if ((i < 0) || (i >= num_heat_channels)) {
        return "Invalid channel";
    } else if (strcmp(name, "wuScale") == 0) {
        channels[i].getScheduler().setAdvanceRate(value);
    } else if (strcmp(name, "orScale") == 0) {
        channels[i].getScheduler().setExtendRate(value);
    } else {
        return "Invalid selector";
    }
    return NULL;
}

static const char * cfg_set_schedule(const char * name, const String & id, String &s) {
    int ch = id.toInt();
    if ((ch < 0) || (ch >= num_heat_channels)) {
        return "Invalid channel";
    } else if (s.length() != Scheduler::num_slots) {
        return "Invalid schedule";
    } else {
        int i,j,k,l=0;
        for (i=0; i<7; ++i) {
            for (j=0; j<24; ++j) {
                for (k=0; k<4; ++k) {
                    //syslogf("set ch %d day %d hh %d mm %d to offs %d value %d\n",ch,i,j,k,l,s[l]);
                    channels[ch].getScheduler().set(i,j,k,s[l]);
                    ++l;
                }
            }
        }
        return NULL;
    }
}

void scheduler_setup() {
    int i;
    for (i=0; i<num_heat_channels; ++i) {
        channels[i].getScheduler().readConfig();
    }
    server.on("/scheduler", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", schedpage, schedpage_processor);
    });
    server.on("/schedset", HTTP_GET, sched_set);
    xTaskCreate(scheduler_run, "scheduler", 10000, NULL, 1, &schedtask_handle);

    MyCfgRegisterInt("wuBase",&cfg_set_warmup);
    MyCfgRegisterInt("wuThrsh",&cfg_set_warmup);
    MyCfgRegisterFloat("wuScale",&cfg_set_warmup_float);
    MyCfgRegisterInt("wuLimit",&cfg_set_warmup);

    MyCfgRegisterInt("bstThrsh",&cfg_set_warmup);

    MyCfgRegisterInt("orThrsh",&cfg_set_warmup);
    MyCfgRegisterFloat("orScale",&cfg_set_warmup_float);
    MyCfgRegisterInt("orLimit",&cfg_set_warmup);
    MyCfgRegisterInt("circtime",&cfg_set_warmup);

    MyCfgRegisterString("schedule",&cfg_set_schedule);
}
