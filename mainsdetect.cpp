//////////////////////////////////////////////////////////////////////////////
//
// Mains detection objects
#include <Arduino.h>
#include "projectconfig.h"
#include "mainsdetect.h"
#include <mysyslog.h>

// input processing stuff
// debounce interval
#define I_DEBOUNCE 100
// min distance from 0v to treat as "on"
#define INPUT_ON_LIMIT 250
// smoothing for mains frequency - only look for volts for max 1/2 cycle
#define MAINS_HZ 50
#define INPUT_ON_MILLIS (1000/(MAINS_HZ*2))

// for binary input testing, pull down to activate
// note no pullup on selected pins
#define MAINSDETECT_TESTMODE


MainsDetect::MainsDetect(int a_pin, const char * a_name1, const char * a_name2) :
    m_pin(a_pin),
    m_name1(a_name1),
    m_name2(a_name2),
    m_zerov(-1),
    m_debounce(0),
    m_pinstate(0),
    m_state(false),
    m_disabled(true)
{
    //calibrate();
}

// calibrate the zero volts level
void MainsDetect::calibrate() {
    m_disabled = true;
    if (m_pin == -1) { return; }
    char t2[100];
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
    sprintf(t2,"Av for pin %d is %d\n",m_pin,m_zerov);
    Serial.print(t2);
    if (m_zerov < 1500) {
        // not there, disable
        // TODO watch for pin coming back?
        sprintf(t2,"disabled pin %d zerov %d",m_pin,m_zerov);
        Serial.println(t2);
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
    if ((j < (m_zerov-INPUT_ON_LIMIT)) || (j > (m_zerov+INPUT_ON_LIMIT))) {
#endif
        // mains is on, reset counter, start debounce if required
        if (m_pinstate == 0) {
            // trigger change check after debounce
            m_debounce = millis();
        }
        m_pinstate = INPUT_ON_MILLIS;
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
        ((a_millinow - debounce) > I_DEBOUNCE)) {
        m_debounce = 0;
        bool x = (m_pinstate > 0);
        if (x != m_state) {
            // only action a change if it changed
            m_state = x;
            syslog.logf(LOG_DAEMON | LOG_INFO, "%s %s %s",m_name1,m_name2,m_state?"set":"cleared");
            return true;
        }
    }
    return false;
}
