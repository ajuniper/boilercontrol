//////////////////////////////////////////////////////////////////////////////
//
// Time setup
#include <Arduino.h>
#include "pinconfig.h"
#include "mytime.h"

#include "esp_sntp.h"
#include "time.h"
#include <Wire.h>
#include <ErriezDS1302.h>
#include <errno.h>
#include <string.h>
#include <my_secrets.h>

#define PIN_RTC_CLK 0
#define PIN_RTC_DATA 4
#define PIN_RTC_RST 5
ErriezDS1302 rtc = ErriezDS1302(PIN_RTC_CLK, PIN_RTC_DATA, PIN_RTC_RST);

static bool have_rtc = false;

static void timeSyncCallback(struct timeval *tv)
{
    // reset the RTC based on NTP
    rtc.setEpoch(tv->tv_sec);
}

void mytime_setup()
{
    setenv("TZ", MY_TIMEZONE, 1);
    // Initialize I2C
    Wire.begin();
    Wire.setClock(100000);

    // Initialize RTC
    int i = 0;
    while (i++ < 3) {
        if (rtc.begin()) {
            have_rtc = true;
            break;
        }
        delay(1000);
    }
    if (have_rtc == false) {
        Serial.println("No RTC!");
    } else {
        sntp_set_time_sync_notification_cb(timeSyncCallback);
        if (!rtc.isRunning()) {
            // Enable oscillator - time not trustworthy
            Serial.println("RTC is not running yet");
            rtc.clockEnable(true);
        } else {
            Serial.println("RTC is running");
            // get date from RTC and set cpu clock
            struct timespec now;
            now.tv_sec = rtc.getEpoch();
            now.tv_nsec = 0;
            if (clock_settime(CLOCK_REALTIME,&now)) {
                Serial.println("Failed to set time from RTC");
                Serial.println(strerror(errno));
            }
        }
    }

    // resync every hour
    sntp_set_sync_interval(3600*000);

    // SNTP gets started when Wifi connects
    //configTime(0, 0, MY_NTP_SERVER1, MY_NTP_SERVER2, MY_NTP_SERVER3);
}
