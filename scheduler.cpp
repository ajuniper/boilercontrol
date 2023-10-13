//////////////////////////////////////////////////////////////////////////////
//
// Scheduler
#include <Arduino.h>
#include "mywebserver.h"
#include "scheduler.h"
#include "heatchannel.h"
#include <time.h>
#include <LittleFS.h>
#include <mysyslog.h>

static const char schedpage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
<title>Boiler Scheduling</title>
<style>
.schedule {
width: 60px;
display: grid;
grid-gap: 0px;
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
padding: 3px 3px;
font-size: 12px;
font-family: sans-serif;
text-align: right;
}
</style>
<script>
var selectedDay = 0;
var selectedCh = 0;
var selectedSchedule = 0;
function toggleCheckbox(element) {
var xhr = new XMLHttpRequest();
var url = "/schedulerset?ch="+selectedCh+"&day="+selectedDay+"&slot="+element.value+"&state=";
if(element.checked){ url+="1"; } else {url+="0"};
xhr.open("GET", url, true);
xhr.send();
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
}
function clickDone() {
location.href="boilercontrol";
}
function copyForwards() {
selectedDay+=1;
if (selectedDay >= 7) { selectedDay -= 7; }
let f = document.getElementById(selectedSchedule);
f = f.getElementsByTagName("input");
let t = document.getElementById("s"+selectedCh+"."+selectedDay);
t = t.getElementsByTagName("input");
for(v in f) {
if (t[v].checked != f[v].checked) {
t[v].checked = f[v].checked;
toggleCheckbox(t[v]);
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
var pr2 = document.createElement("form");
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
c = document.createElement("input");
c.setAttribute("type","checkbox");
if (j==0){
c.setAttribute("value",""+i+":00");
} else {
c.setAttribute("value",""+i+":"+j);
}
c.setAttribute("onclick", "toggleCheckbox(this)");
l.appendChild(c);
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
        s+=(i==0)?"active":"";
        s+="\" onclick=\"clickChannel(event, 0)\">";
        s += channels[i].getName();
        s+="</button>";
    }
    s+="<button id=\"ct3\" class=\"chlinks\" onclick=\"clickDone()\">Done</button><button id=\"ct4\" class=\"chlinks\" onclick=\"copyForwards()\">Copy to Next</button>";

    s+="</div><div id=\"days\" class=\"tab\"></div>";

    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        s+="<div id=\"c"+String(i)+" class=\"chcontent\"></div>";
    }

    return s;
  }
  return String();
}

static void sched_set(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = nullptr;
    String x,y;
    String z("Invalid Paremeters");
    int rc = 400;
    // GET /sched_set?ch=X&day=D&slot=H:MM&state=01
    // d=0-6 M-S
    if (request->hasParam("ch") &&
        request->hasParam("day") &&
        request->hasParam("slot") &&
        request->hasParam("state")) {
        x = request->getParam("ch")->value();
        int ch = x.toInt();
        if ((ch >= 0) && (ch <= num_heat_channels)) {
            x = request->getParam("state")->value();
            int i = x.toInt();
            x = request->getParam("day")->value();
            int j = x.toInt();
            x = request->getParam("slot")->value();
            if (channels[ch].getScheduler().set(j,x,i)) {
                y = "Channel ";
                y+=channels[ch].getName();
                y+=" day ";
                y+=String(j);
                y+=" slot ";
                y+=x;
                if (i==0) {
                    y+=" cleared";
                } else {
                    y+=" set";
                }
                response = request->beginResponse(200, "text/plain", y);
            }
        } else {
            z = "Invalid channel "+x;
        }
    } else {
        z = "Parameter missing";
    }
    response = request->beginResponse(rc, "text/plain", z);
    response->addHeader("Connection", "close");
    request->send(response);
}

Scheduler::Scheduler(HeatChannel & aChannel, int aSetback) :
    mChannel(aChannel),
    mBaseWarmup(aSetback),
    mLastChange(0)
{
    memset(mSchedule, 0, sizeof(mSchedule));
}

void Scheduler::readConfig()
{
    // read saved setback config
    String s = "/warmup.";
    s+=mChannel.getId();
    fs::File f = LittleFS.open(s, "r");
    if (f) {
        String y;
        while (f.available()) {
            y = f.readString();
        }
        mBaseWarmup = y.toInt();
        f.close();
    } else {
        syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to open warmup file");
    }

    // read saved schedule
    s="/sched.";
    s+=mChannel.getId();
    f = LittleFS.open(s, "r");
    if (f) {
        int r = f.read((unsigned char *)mSchedule,sizeof(mSchedule));
        if (r != sizeof(mSchedule)) {
            syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to read all bytes");
        }
        f.close();
    } else {
        syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to open schedule file");
    }
}

void Scheduler::checkSchedule(int d, int h, int m)
{
    // calculate current warmup time
    int t = mBaseWarmup;
    // TODO factor in some adjustments

    struct tm w2;
    time_t now = time(NULL);
    int c = 0; // number of 15 minute intervals

    // if (now + warmup) is scheduled on
    now += t;
    localtime_r(&now, &w2);
    while (mSchedule[w2.tm_wday][w2.tm_hour][w2.tm_min/15] != 0) {
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
    if (c > 0) {
        // set timer
        t += (c * 15 * 60);
        mChannel.adjustTimer(t);
    } else if ((time(NULL) - mChannel.lastTime()) > 86400) {
        // if >24h since last run
        // set timer for (now-1) / -2
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
    if (mLastChange != 0) { return; }
    String s="/sched.";
    s+=mChannel.getId();
    fs::File f = LittleFS.open(s, "w");
    if (f) {
        int written = f.write((const unsigned char *)mSchedule,sizeof(mSchedule));
        if (written != sizeof(mSchedule)) {
            syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to write all bytes");
        }
        f.close();
    } else {
        syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to open schedule file for writing");
    }
    mLastChange = 0;
}

static void scheduler_run(void *)
{
    time_t t,u;
    int i,j;
    int d,h,m;
    struct tm now;
    delay(5000);
    while(1) {
        // sleep to start of next minute
        t = time(NULL);
        t = t%60;
        t = 60-t;
        delay(t*1000);

        // for each channel
        t = 0;
        u = time(NULL);
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
            if (ch.getTimer() > 0) { continue; }

            // process this channel
            ch.getScheduler().checkSchedule(now.tm_wday,now.tm_hour,now.tm_min);
        }

        // if pending save older than 10s then save changes
        u = time(NULL);
        if ((t > 0) && ((u-t) > 10)) {
            if (LittleFS.begin()) {
                for(i=0; i<num_heat_channels; ++i) {
                    channels[i].getScheduler().saveChanges();
                }
                LittleFS.end();
            } else {
                syslog.logf(LOG_DAEMON|LOG_ERR,"Failed to start littlefs");
            }
        }
    }
}

bool Scheduler::set(int d, String & hm, bool state)
{
    // validate inputs
    if (d < 0 || d > 6) { return false; }
    // unix time is Sunday=0 but ui is Monday=0
    d = (d+1)%7;
    // extract h
    int h = hm.toInt();
    if (h < 0 || h>23) { return false; }
    // extract m
    int m = hm.indexOf(":");
    if (m < 0) { return false; }
    m = hm.substring(m+1).toInt();
    if (m != 0 && m != 15 && m != 30 && m != 45) { return false; }
    // convert to quarter hours
    m = m/15;
    mSchedule[d][h][m] = state;
    mLastChange = time(NULL);
    return true;
}

void scheduler_setup() {
    // called with LittleFS already running
    int i;
    for (i=0; i<num_heat_channels; ++i) {
        channels[i].getScheduler().readConfig();
    }
    server.on("/scheduler", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", schedpage, schedpage_processor);
    });
    server.on("/schedulerset", HTTP_GET, sched_set);
    xTaskCreate(scheduler_run, "scheduler", 10000, NULL, 1, NULL);   
}
