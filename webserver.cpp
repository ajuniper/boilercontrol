//////////////////////////////////////////////////////////////////////////////
//
// Web Server Stuff
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "webserver.h"
#include "heatchannel.h"
#include <Syslog.h>

extern bool o_pump_on;
extern bool o_boiler_on;
extern Syslog syslog;

// Create AsyncWebServer object on port 80
static AsyncWebServer server(80);

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

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
            default:
                s+= String((t-now)/60);
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
    
    s += "System: ";
    s += o_pump_on?"pump ":"";
    s += o_boiler_on?"boiler":"";
    return s;
  }
  return String();
}

static void redirect_to_home (AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "OK");
  response->addHeader("Location","/");
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

static void serve_update_get(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<html><body><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form><p/><a href=\"/reboot\">Reboot</a><p/></body></html>");
}
static void serve_update_post(AsyncWebServerRequest *request) {
    shouldReboot = !Update.hasError();
    syslog.logf(LOG_DAEMON|LOG_INFO,"Update starting");
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
}
static void serve_update_post_body(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index){
        Serial.printf("Update Start: %s\n", filename.c_str());
        syslog.logf(LOG_DAEMON|LOG_INFO,"Update Start: %s", filename.c_str());
        //Update.runAsync(true);
        //if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
        if(!Update.begin())
        {
            Update.printError(Serial);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update failed: %s",Update.errorString());
        }
    }
    if(!Update.hasError()){
        if(Update.write(data, len) != len){
            Update.printError(Serial);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update failed: %s",Update.errorString());
        }
    }
    if(final){
        if(Update.end(true)){
            Serial.printf("Update Success: %uB\n", index+len);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update success");
        } else {
            Update.printError(Serial);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update failed: %s",Update.errorString());
        }
    }
}

static void serve_reboot_get(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<html><body>Rebooting... bye bye...</body></html>");
    syslog.logf(LOG_DAEMON|LOG_CRIT,"Restarting");
    Serial.printf("Restarting");
    delay(1000);
    // TODO shut down boiler etc. first
    ESP.restart();
}

void webserver_setup() {
    // Route web pages
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", statuspage, statuspage_processor);
    });
    server.on("/heat", HTTP_GET, web_set_heat);

    // firmware updates
    // https://github.com/me-no-dev/ESPAsyncWebServer#file-upload-handling
    server.on("/update", HTTP_GET, serve_update_get);
    server.on("/update", HTTP_POST, serve_update_post, serve_update_post_body);
    server.on("/reboot", HTTP_GET, serve_reboot_get);
    server.begin();
}
