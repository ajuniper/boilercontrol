//////////////////////////////////////////////////////////////////////////////
//
// Mains detection objects
#include <Arduino.h>
#include "projectconfig.h"
#include "mainsdetect.h"
#include <mysyslog.h>
#include "myconfig.h"
#include <map>

// input processing stuff
// debounce interval
#define I_DEBOUNCE 100
// min distance from 0v to treat as "on"
#define INPUT_ON_LIMIT 350
// smoothing for mains frequency - only look for volts for max 1/2 cycle
#define MAINS_HZ 50
#define INPUT_ON_MILLIS (1000/(MAINS_HZ*2))

std::map<int, MainsDetect*> detectors;

static const char * cfg_set_threshold(const char * name, const String & id, int &value) {
    int i = id.toInt();
    std::map<int, MainsDetect*>::iterator j = detectors.find(i);
    if (j == detectors.end()) {
        return "Unknown pin";
    }
    if (strcmp(name, "mainsthrsh") == 0) {
        j->second->setThreshold(value);
    } else if (strcmp(name, "mainsticks") == 0) {
        j->second->setTicks(value);
    } else if (strcmp(name, "mainsdbnc") == 0) {
        j->second->setDebounceT(value);
    } else {
        return "Invalid selector";
    }
    return NULL;
}

static bool need_initialise = true;
static void initialise() {
    need_initialise = false;
    MyCfgRegisterInt("mainsthrsh",&cfg_set_threshold);
    MyCfgRegisterInt("mainsticks",&cfg_set_threshold);
    MyCfgRegisterInt("mainsdbnc",&cfg_set_threshold);
}

MainsDetect::MainsDetect(int a_pin, const char * a_name1, const char * a_name2) :
    m_pin(a_pin),
    m_name1(a_name1),
    m_name2(a_name2),
    m_zerov(-1),
    m_debounce(0),
    m_pinstate(0),
    m_state(false),
    m_disabled(true),
    m_threshold(INPUT_ON_LIMIT),
    m_ticks(INPUT_ON_MILLIS),
    m_debounce_t(I_DEBOUNCE)
{
    //calibrate();
    detectors[a_pin] = this;
}

// calibrate the zero volts level
void MainsDetect::calibrate() {
    m_disabled = true;
    if (m_pin == -1) { return; }
    char t2[100];

    if (need_initialise) {
        initialise();
    }
    m_threshold = MyCfgGetInt("mainsthrsh", String(m_pin), INPUT_ON_LIMIT);
    m_ticks = MyCfgGetInt("mainsticks", String(m_pin), INPUT_ON_MILLIS);
    m_debounce_t = MyCfgGetInt("mainsdbnc", String(m_pin), I_DEBOUNCE);

#ifdef MAINSDETECT_TESTMODE
    m_disabled = false;
    pinMode(m_pin, INPUT);
    sprintf(t2,"input pin %d in digital mode",m_pin);
    Serial.println(t2);
#else
    // calculate the average voltage to determine base level
    int a;
    int j;
    a=0;
    // twice as first pass gives bogus results
    for(j=0; j<(1000/MAINS_HZ); ++j) {
        a+=analogRead(m_pin);
    }
    a=0;
    for(j=0; j<(1000/MAINS_HZ); ++j) {
        a+=analogRead(m_pin);
    }
    // calculate the average
    m_zerov=a/j;
    //sprintf(t2,"Av for pin %d is %d\n",m_pin,m_zerov);
    //Serial.print(t2);
    if (m_zerov < 1500) {
        // not there, disable
        // TODO watch for pin coming back?
        //sprintf(t2,"disabled pin %d zerov %d",m_pin,m_zerov);
        //Serial.println(t2);
    } else {
        m_disabled = false;
    }
#endif
}

// refresh the state of the pin (every millisecond)
void MainsDetect::checkstate() {
    if (m_disabled) { return; }

#ifdef MAINSDETECT_TESTMODE
    // test mode, just do digital read
    // pull DOWN to activate
    if (digitalRead(m_pin)==0) {
#else
    int j = analogRead(m_pin);
    if ((j < (m_zerov-m_threshold)) || (j > (m_zerov+m_threshold))) {
#endif
        // mains is on, reset counter, start debounce if required
        if (m_pinstate == 0) {
            // trigger change check after debounce
            m_debounce = millis();
        }
        m_pinstate = m_ticks;
    } else {
        // mains is off, decrement counter
        if (m_pinstate > 0) {
            m_pinstate--;
            if (m_pinstate == 0) {
                // trigger change check after debounce
                m_debounce = millis();
            }
        }
    }
}

// check for debounce expiry and report if changed
bool MainsDetect::updatepin(unsigned long a_millinow) {
    // sets changed to true if value changed
    if (m_pin == -1) { return false; }
    unsigned long debounce;
    if (((debounce = m_debounce) > 0) &&
        ((a_millinow - debounce) > m_debounce_t)) {
        m_debounce = 0;
        bool x = (m_pinstate > 0);
        if (x != m_state) {
            // only action a change if it changed
            m_state = x;
            syslogf(LOG_DAEMON | LOG_INFO, "%s %s input %s",m_name1,m_name2,m_state?"set":"cleared");
            return true;
        }
    }
    return false;
}
