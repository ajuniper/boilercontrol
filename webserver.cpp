//////////////////////////////////////////////////////////////////////////////
//
// Web Server Stuff
#include <Arduino.h>
#include "mywebserver.h"
#include "webserver.h"
#include "heatchannel.h"
#include <WiFi.h>
#include "outputs.h"

extern bool o_pump_on;
extern bool o_boiler_on;

static const char statuspage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Boiler Control</title>
</head>
<body>
  <h2>Boiler Control</h2>
  %STATUS%
  <A HREF="/heat?ch=0&q=3600">Hot Water 1 hour</A><BR/>
  <A HREF="/heat?ch=0&q=-1">Hot Water On</A><BR/>
  <A HREF="/heat?ch=0&q=0">Hot Water Off</A><P/>
  <A HREF="/heat?ch=1&q=3600">Heating 1 hour</A><BR/>
  <A HREF="/heat?ch=1&q=7200">Heating 2 hours</A><BR/>
  <A HREF="/heat?ch=1&q=10800">Heating 3 hours</A><BR/>
  <A HREF="/heat?ch=1&q=-1">Heating On</A><BR/>
  <A HREF="/heat?ch=1&q=0">Heating Off</A><P/>
  <hr/>
  %STATUS2%
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
static String statuspage_processor(const String& var){
  time_t now = time(NULL);
  //Serial.println(var);
  if(var == "STATUS"){
    String s = "<p/>";
    int i;
    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        s += channels[i].getName();
        s += ": ";
        unsigned long t = channels[i].getTimer();
        switch (t) {
            case 0:
                s += "Off";
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
        t = channels[i].getCooldown();
        switch (t) {
            case 0:
                // cooldown off, say nothing
                break;
            case -1: // shouldn't happen
                s += "cooldown on";
                break;
            default:
                s += "cooldown ";
                s+= String((t-now)/60);
        }
        s+=" target temp ";
        s+=String(channels[i].targetTemp());
        s+="<br/>";
    }

    s+="<p/>";

    for(i=0; i<num_heat_channels; ++i) {
        if (!channels[i].getEnabled()) { continue ; }
        s += channels[i].getName();
        s += ": ";
        s += (channels[i].getOutput()) ? "on" : "off";
        s += " ";
        s += (channels[i].getInputReady().pinstate()) ? "calls" : "";
        s += (channels[i].getInputSatisfied().pinstate()) ? "satisfied" : "";
        s += " ";
        s += (channels[i].getSatisfied()) ? "SATon" : "SAToff";
        s+="<br/>";
    }
    
    s += "System: pump ";
    switch (o_boiler_state) {
        case OUTPUT_OFF: s+="off"; break;
        case OUTPUT_COOLING: s+="cycling"; break;
        case OUTPUT_HEATING: s+="heating"; break;
    }
    s += " boiler ";
    switch (o_pump_state) {
        case OUTPUT_OFF: s+="off"; break;
        case OUTPUT_COOLING: s+="cycling"; break;
        case OUTPUT_HEATING: s+="heating"; break;
    }
    return s;
  } else if(var == "STATUS"){
    String s = "<p/>";
    s += "WiFi SNR: " + String(WiFi.RSSI()) + "<br/>";
    s += "Date: ";
    struct tm timeinfo;
    getLocalTime(&timeinfo);
#define dt_len 21
    char dt[dt_len];
    ssize_t l = strftime(dt,dt_len,"%F %T",&timeinfo);
    s += dt;
    return s;
  }
  return String();
}

static void redirect_to_home (AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "OK");
  response->addHeader("Location","/boiler");
  request->send(response);
}

static void web_set_heat (AsyncWebServerRequest *request) {
    String x;
    // GET /heat?ch=N&q=X where X is seconds to run
    if (request->hasParam("q") && request->hasParam("ch")) {
        x = request->getParam("ch")->value();
        int ch = x.toInt();
        if ((ch >= 0) && (ch <= num_heat_channels)) {
            x = request->getParam("q")->value();
            int i = x.toInt();
            channels[ch].adjustTimer(i);
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
                y+=x;
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
    server.on("/targettemp", HTTP_GET, web_set_heat);
}
