//////////////////////////////////////////////////////////////////////////////
//
// Web Server Stuff
#include <Arduino.h>
#include "projectconfig.h"
#include "mywebserver.h"
#include "webserver.h"
#include "heatchannel.h"
#include <WiFi.h>
#include "outputs.h"
#include "tempsensors.h"
#include "tempfetcher.h"

extern int target_temp;

static const char statuspage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Boiler Control</title>
</head>
<body>
  <h2>Boiler Control</h2>
  %STATUS%
</body>
</html>
)rawliteral";

static time_t starttime;
static void getInterval(String &s, time_t d)
{
    // report 00h 00m or 00m 00s
    // hours only if >= 1hr
    if (d >= 3600) {
        s += String(int(d/3600)) + "h ";
    }
    // minutes if >= 1 minute
    if (d >= 60) {
        s += String(int((d%3600)/60)) + "m";
    }
    // seconds only if less than 1hr
    if (d < 3600) {
        if (d >= 60) {
            s += " ";
        }
        s += String(d%60) + "s";
    }
}

// Replaces placeholder with button section in your web page
static String statuspage_processor(const String& var){
  time_t now = time(NULL);
  String s;
  char ch;
  if(var == "STATUS"){
    int i;
    unsigned long t;
    s+="<p><table border=\"1\">";
    s+="<tr><td/>";
    for(i=0; i<num_heat_channels; ++i) {
        s +="<th align=\"center\">";
        s += channels[i].getName();
        s +="</th>";
    }
    s+="</tr><tr><th align=\"left\">Timer</th>";
    for(i=0; i<num_heat_channels; ++i) {
        s +="<td align=\"center\">";
        if (channels[i].getActive()) {
            // timer
            t = channels[i].getTimer();
            switch (t) {
                case CHANNEL_TIMER_OFF:
                    s += "Off";
                    // cooldown
                    t = channels[i].getCooldown();
                    switch (t) {
                        case CHANNEL_TIMER_OFF:
                            // cooldown off, say nothing
                            break;
                        case CHANNEL_TIMER_ON: // shouldn't happen
                            s += " (cooling)";
                            break;
                        default:
                            s += " (cooling ";
                            getInterval(s,t-now);
                            s+=")";
                    }
                    break;
                case CHANNEL_TIMER_ON:
                    s += "On";
                    break;
                default:
                    getInterval(s,t-now);
            }
        } else {
            s += "Inactive";
        }
        s +="</td>";
    }

    s+="</tr><tr><th align=\"left\">Target Temp</th>";
    for(i=0; i<num_heat_channels; ++i) {
        s +="<td align=\"center\">";
        if (channels[i].getActive()) {
            s+=String(channels[i].targetTemp());
            s+="C";
        }
        s +="</td>";
    }

    s+="</tr><tr><th align=\"left\">Base Warmup</th>";
    for(i=0; i<num_heat_channels; ++i) {
        s +="<td align=\"center\">";
        if (channels[i].getActive()) {
            getInterval(s,channels[i].getScheduler().getBaseWarmup());
        }
        s +="</td>";
    }

    s+="</tr><tr><th align=\"left\">Actions</th>";
    for(i=0; i<num_heat_channels; ++i) {
        s +="<td align=\"center\">";
        ch = '0'+i;
        if (channels[i].getActive()) {
            if (channels[i].targetTemp2() > -1) {
                s+="Warm: ";
                s+="<a href=\"heat?ch="; s+=ch ; s+="&q=3600&t=1\">+1h</a> ";
                s+="<a href=\"heat?ch="; s+=ch ; s+="&q=7200&t=1\">+2h</a> ";
                s+="<a href=\"heat?ch="; s+=ch ; s+="&q=10800&t=1\">+3h</a> ";
                s+="<a href=\"heat?ch="; s+=ch ; s+="&q="; s+= CHANNEL_TIMER_ON ; s+="&t=1\">On</a> ";
                s+="<br/>Hot: ";
            }
            s+="<a href=\"heat?ch="; s+=ch ; s+="&q=3600&t=2\">+1h</a> ";
            s+="<a href=\"heat?ch="; s+=ch ; s+="&q=7200&t=2\">+2h</a> ";
            s+="<a href=\"heat?ch="; s+=ch ; s+="&q=10800&t=2\">+3h</a> ";
            s+="<a href=\"heat?ch="; s+=ch ; s+="&q="; s+=CHANNEL_TIMER_ON ; s+="&t=2\">On</a><br/>";
            s+="<a href=\"heat?ch="; s+=ch ; s+="&q="; s+=CHANNEL_TIMER_OFF ; s+="\">Off</a> ";
            s+="<a href=\"config?name=chactive&id="; s+=ch ; s+="&value=0\">Disable</a> ";
        } else {
            s+="<a href=\"config?name=chactive&id=";
            s+=ch;
            s+="&value=1\">Activate</a>";
        }
        s +="</td>";
    }

    s+="</tr><tr><th align=\"left\">State</th>";
    for(i=0; i<num_heat_channels; ++i) {
        s +="<td align=\"center\">";
        s += (channels[i].getOutput()) ? "on" : "off";
        s += " ";
        s += (channels[i].getInputReady().pinstate()) ? "calls " : "";
        s += (channels[i].getInputSatisfied().pinstate()) ? "satisfied " : "";
        s += (channels[i].getSatisfied()) ? "SATon" : "SAToff";
        s +="</td>";
    }
    s+="</tr></table>";

    s+="<p><table border=\"1\">";
    s += "<tr><td colspan=\"2\"><a href=\"scheduler\">Schedules</a></td><tr>";
    s += "<tr><td>Boiler</td><td>";
    switch (o_boiler_state) {
        case OUTPUT_OFF: s+="off"; break;
        case OUTPUT_COOLING: s+="cycling"; break;
        case OUTPUT_HEATING: s+="heating"; break;
    }
    s += "</td></tr><tr><td>Pump</td><td>";
    switch (o_pump_state) {
        case OUTPUT_OFF: s+="off"; break;
        case OUTPUT_COOLING: s+="cooling"; break;
        case OUTPUT_HEATING: s+="running"; break;
    }
    for (i=0; i<num_temps; ++i) {
        s += "</td></tr><tr><td>";
        s += temperatures[i].getName();
        s += "</td><td>";
        int t = temperatures[i].getTemp();
        if (t != 999) {
            s += String(t)+"C ";
        } else {
            s += "--- ";
        }
    }
    s += "</td></tr><tr><td>Target Temp</td><td>";
    if (target_temp == 999) {
        s+= "---";
    } else {
        s+= String(target_temp);
    }
    s += "</td></tr><tr><td>WiFi SNR</td><td>" + String(WiFi.RSSI());
    s += "</td></tr><tr><td>Date</td><td>";
    struct tm timeinfo;
    getLocalTime(&timeinfo);
#define dt_len 21
    char dt[dt_len];
    ssize_t l = strftime(dt,dt_len,"%F %T",&timeinfo);
    s += dt;
    s += "</td></tr><tr><td>Uptime</td><td>";
    getInterval(s,now-starttime);
    s += "</td></tr></table>";
  }
  return s;
}

static void redirect_to_home (AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "OK");
  response->addHeader("Location","boiler");
  request->send(response);
}

static void web_set_heat (AsyncWebServerRequest *request) {
    String x;
    // GET /heat?ch=N&q=X where X is seconds to run
    if (request->hasParam("ch")) {
        x = request->getParam("ch")->value();
        int ch = x.toInt();
        if ((ch >= 0) && (ch <= num_heat_channels)) {
            if (request->hasParam("q")) {
                int i;
                // if there is a temp setting then enact that
                if (request->hasParam("t")) {
                    x = request->getParam("t")->value();
                    i = x.toInt();
                    channels[ch].setTargetTempBySetting(i);
                }
                x = request->getParam("q")->value();
                i = x.toInt();
                channels[ch].adjustTimer(i);
            }
        }
    }
    redirect_to_home(request);
}

void webserver_setup() {
    // Route web pages
    WS_init("boiler");
    server.on("/boiler", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", statuspage, statuspage_processor);
    });
    server.on("/heat", HTTP_GET, web_set_heat);
    starttime = time(NULL);
}
