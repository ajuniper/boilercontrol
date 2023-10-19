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

extern bool o_pump_on;
extern bool o_boiler_on;
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

// Replaces placeholder with button section in your web page
static String statuspage_processor(const String& var){
  time_t now = time(NULL);
  String s;
  char ch;
  if(var == "STATUS"){
    int i;
    unsigned long t;
    s+="<p><table border=\"1\">";
    s+="<tr><th align=\"left\">Name</th><th align=\"left\">Timer</th><th align=\"left\">Target<br/>Temp</th><th align=\"left\">Base<br/>Warmup</th><th align=\"left\">Actions</th><th align=\"left\">State</th></tr>";
    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        ch = '0'+i;
        // name
        s += "<tr><th align=\"left\">";
        s += channels[i].getName();
        // timer
        s += "</th><td>";
        if (!channels[i].getActive()) {
            s+="Inactive";
            // empty cell for target temp and warmup time
            s+="</td><td>";
            s+="</td><td>";
        } else {
            // timer
            t = channels[i].getTimer();
            switch (t) {
                case 0:
                    s += "Off";
                    // cooldown
                    t = channels[i].getCooldown();
                    switch (t) {
                        case 0:
                            // cooldown off, say nothing
                            break;
                        case -1: // shouldn't happen
                            s += " (cooling)";
                            break;
                        default:
                            s += " (cooling ";
                            s+= String((t-now)/60);
                            s+=")";
                    }
                    break;
                case -1:
                    s += "On";
                    break;
                default: {
                    unsigned long d = t-now;
                    if (d < 60) {
                        s += String(d) + "s";
                    } else {
                        if (d > 3600) {
                            s += String(int(d/3600)) + "h ";
                        }
                        s += String(int((d%3600)/60)) + "m";
                    }
                }
            }
            // target temperature
            s+="</td><td>";
            s+=String(channels[i].targetTemp());
            s+="</td><td>";
            s+=String(channels[i].getScheduler().getBaseWarmup());

        } // end not inactive
        // actions
        s+="</td><td>";
        if (!channels[i].getActive()) {
            s+="<a href=\"/heat?ch=";
            s+=ch;
            s+="&a=1\">Activate</A>";
            // empty cell for output states
            s += "</td><td>";
        } else {
            s+="<a href=\"/heat?ch="; s+=ch ; s+="&q=3600\">+1h</a> ";
            s+="<a href=\"/heat?ch="; s+=ch ; s+="&q=7200\">+2h</a> ";
            s+="<a href=\"/heat?ch="; s+=ch ; s+="&q=10800\">+3h</a> ";
            s+="<a href=\"/heat?ch="; s+=ch ; s+="&q=-1\">On</a> ";
            s+="<a href=\"/heat?ch="; s+=ch ; s+="&q=0\">Off</a> ";
            s+="<a href=\"/heat?ch="; s+=ch ; s+="&a=0\">Disable</a> ";

            // output states
            s += "</td><td>";
            s += (channels[i].getOutput()) ? "on" : "off";
            s += " ";
            s += (channels[i].getInputReady().pinstate()) ? "calls " : "";
            s += (channels[i].getInputSatisfied().pinstate()) ? "satisfied " : "";
            s += (channels[i].getSatisfied()) ? "SATon" : "SAToff";
        }
        s+="</td></tr>";
    }

    s+="</table><p><table border=\"1\">";
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
    s += "</td></tr><tr><td>Target Temp</td><td>" + String(target_temp);
    s += "</td></tr><tr><td>WiFi SNR</td><td>" + String(WiFi.RSSI());
    s += "</td></tr><tr><td>Date</td><td>";
    struct tm timeinfo;
    getLocalTime(&timeinfo);
#define dt_len 21
    char dt[dt_len];
    ssize_t l = strftime(dt,dt_len,"%F %T",&timeinfo);
    s += dt;
    s += "</td></tr></table>";
  }
  return s;
}

static void redirect_to_home (AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "OK");
  response->addHeader("Location","/boiler");
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
                x = request->getParam("q")->value();
                int i = x.toInt();
                channels[ch].adjustTimer(i);
            }
            if (request->hasParam("a")) {
                x = request->getParam("a")->value();
                int i = x.toInt();
                channels[ch].setActive(i);
            }
        }
    }
    redirect_to_home(request);
}

static void web_set_target (AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = nullptr;
    String x,y;
    // GET /targettemp?ch=N
    // GET /targettemp?ch=N&temp=X where X is new target temp
    if (!request->hasParam("ch") && request->hasParam("temp")) {
        response = request->beginResponse(400, "text/plain", "Sensor id missing");
    } else {
        x = request->getParam("ch")->value();
        int ch = x.toInt();
        if ((ch >= 0) && (ch <= num_heat_channels)) {
            if (request->hasParam("temp")) {
                x = request->getParam("temp")->value();
                int i = x.toInt();
                channels[ch].setTargetTemp(i);
                y = "Setting target temperature ";
                y+=channels[ch].getName();
                y+=" to ";
                y+=x;
                response = request->beginResponse(200, "text/plain", y);
            } else {
                y = "Target temperature ";
                y+=channels[ch].getName();
                y+=" is ";
                y+= String(channels[ch].targetTemp());
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

void webserver_setup() {
    // Route web pages
    WS_init("/boiler");
    server.on("/boiler", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", statuspage, statuspage_processor);
    });
    server.on("/heat", HTTP_GET, web_set_heat);
    server.on("/targettemp", HTTP_GET, web_set_target);
}
