#include <Arduino.h>
#include "projectconfig.h"
#include "pinconfig.h"
#include "display.h"
#include <WiFi.h>

TFT_eSPI tft = TFT_eSPI();

#include "pump.h"
#include "flame.h"

#include "buttons.h"
#include "tempsensors.h"
#include "heatchannel.h"
#include "outputs.h"
#include "mysyslog.h"

bool display_needs_reset = false;

//////////////////////////////////////////////////////////////////
//
// update clock and IP

int last_min = -1;

static void print_clock() {
    struct tm timeinfo;
#define dt_len 21
    char dt[dt_len];
    getLocalTime(&timeinfo);
    tft.setFreeFont(clock_font);
    tft.setTextColor(clock_colour,TFT_BLACK,true);
    if (timeinfo.tm_min != last_min) {
        // update the whole time string if the minute changes
        last_min = timeinfo.tm_min;
        strftime(dt,dt_len,"%a %H %M",&timeinfo);
        if (timeinfo.tm_sec % 2) {
            dt[6]=':';
        }

        tft.drawString(dt,clock_x,clock_y,fontnum);
    } else {
        // just update the colon
        if (timeinfo.tm_sec % 2) {
            dt[0]=':';
            dt[1]=0;
            tft.drawString(dt,clock_x-47,clock_y,fontnum);
        } else {
            // bodge around library bug which does not draw a single space
            dt[0]=' ';
            dt[1]=' ';
            dt[2]=0;
            tft.drawString(dt,clock_x-40,clock_y,fontnum);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        snprintf(dt,dt_len,"%16s",WiFi.localIP().toString().c_str());
    } else {
        snprintf(dt,dt_len,"%16s","---.---.---.---");
    }

    tft.setFreeFont(ip_font);
    tft.setTextColor(ip_colour,TFT_BLACK);
    tft.drawString(dt,ip_x,ip_y,1);
}

//////////////////////////////////////////////////////////////////
//
// output icons

static void draw_outputs() {

    int pc;
    int bc;

    switch (o_boiler_state) {
        case OUTPUT_OFF: bc = TFT_DARKGREY; break;
        case OUTPUT_COOLING: bc = flame_colour2; break;
        case OUTPUT_HEATING: bc = flame_colour; break;
    }
    switch (o_pump_state) {
        case OUTPUT_OFF: pc = TFT_DARKGREY; break;
        case OUTPUT_COOLING: pc = pump_colour2; break;
        case OUTPUT_HEATING: pc = pump_colour; break;
    }

    tft.drawBitmap(pump_x, pump_y, pump, pump_w, pump_h,pc);
    tft.drawBitmap(flame_x, flame_y, flame, flame_w, flame_h, bc);
}

//////////////////////////////////////////////////////////////////
//
// RSSI icon

static void draw_rssi() {
    int s = WiFi.RSSI();

    // always present
    tft.fillCircle(rssi_x, rssi_y, 2, rssi_colour);
    tft.drawSmoothArc(rssi_x,rssi_y,
                      1*rssi_spacing,1*rssi_spacing+rssi_width,
                      rssi_direction-rssi_deg,
                      rssi_direction+rssi_deg,
                      (s>-105)?rssi_colour:TFT_BLACK,TFT_BLACK);
    tft.drawSmoothArc(rssi_x,rssi_y,
                      2*rssi_spacing,2*rssi_spacing+rssi_width,
                      rssi_direction-rssi_deg,
                      rssi_direction+rssi_deg,
                      (s>-90)?rssi_colour:TFT_BLACK,TFT_BLACK);
    tft.drawSmoothArc(rssi_x,rssi_y,
                      3*rssi_spacing,3*rssi_spacing+rssi_width,
                      rssi_direction-rssi_deg,
                      rssi_direction+rssi_deg,
                      (s>-78)?rssi_colour:TFT_BLACK,TFT_BLACK);
    tft.drawSmoothArc(rssi_x,rssi_y,
                      4*rssi_spacing,4*rssi_spacing+rssi_width,
                      rssi_direction-rssi_deg,
                      rssi_direction+rssi_deg,
                      (s>-60)?rssi_colour:TFT_BLACK,TFT_BLACK);
}

static void draw_temperatures() {
    int x = temp_x;
    int i;
    char m[12];
    tft.setFreeFont(temp_font);
    tft.setTextColor(temp_colour,TFT_BLACK);
    for (i=0; i<num_temps; ++i) {
        int t = temperatures[i].getTemp();
        if (t == 999) {
            sprintf(m," ---");
        } else {
            // fix up bogus readings
            if (t < -99) t = -99;
            sprintf(m,"%3dC",t);
        }
        tft.drawString(m,x,temp_y,1);
        x+=temp_x;
    }
}

//////////////////////////////////////////////////////////////////
//
// main display handling

// time of last display update, limit update rate
static time_t lastsec = 0;

// update display components
static void update_display() {
    print_clock();
    draw_outputs();
    draw_rssi();
    draw_temperatures();

    int i;
    for(i=0; i<num_heat_channels; ++i) {
        channels[i].updateDisplay();
    }
}

// IRQ task to wake display
static TaskHandle_t displaytask_handle = NULL;

static void IRAM_ATTR display_irq() {
    detachInterrupt(digitalPinToInterrupt(PIN_TOUCH_IRQ));
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR( displaytask_handle, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

// called when no recent touches
static void wait_for_touch() {
    // To store the touch coordinates
    uint16_t t_x = 0, t_y = 0;
    // display off
    ledcWrite(0, 0);
    // enable IRQ
    attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_IRQ), display_irq, FALLING);
    // wait for signal
    uint32_t n = 0;
    while (n == 0) {
        n = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( 60000 ) );
    }

    // update the display then light it up
    update_display();
    ledcWrite(0, display_brightness);
    // wait for touch to be released before continuing
    while (tft.getTouch(&t_x,&t_y)) {
        delay(touch_delay);
    }
}

// time of last change of touch
static time_t last_touch = 0;

static void display_reset() {
    // display IRQ is an input
    pinMode(PIN_TOUCH_IRQ, INPUT_PULLUP);

    // set up display
    tft.init();
    // connectors on right
    tft.setRotation(display_rotation);
    tft.fillScreen(TFT_BLACK);
    // text is right justified
    tft.setTextDatum(TR_DATUM);
    tft.setTextSize(1);

    // PWM channel freq resolution
    ledcSetup(0, 5000, 8);
    // add pin to PWM channel
    ledcAttachPin(PIN_DISPLAY_LED,0);
    // default brightness
    ledcWrite(0, display_brightness);

    // draw channels
    int i;
    for(i=0;i<num_heat_channels;++i) {
        channels[i].initDisplay();
    }
    // draw buttons
    for(i=0; i<num_buttons; ++i) {
        buttons[i].initialise();
    }
    // temperature labels
    char m[14];
    int x = temp_x;
    tft.setFreeFont(temp_font);
    tft.setTextColor(temp_colour,TFT_BLACK);
    for (i=0; i<num_temps; ++i) {
        sprintf(m,"%10s",temperatures[i].getName());
        tft.drawString(m,x,temp_y-10,1);
        x+=temp_x;
    }
    last_touch = 0;
    display_needs_reset = false;
}

// main display update task
static void displaytask(void*) {
    while(1) {
        time_t now = time(NULL);

        if (display_needs_reset) {
            syslogf("Resetting display");
            display_reset();
            delay(1000);
            continue;
        }

        // wait for a valid time to be set
        if (last_touch == 0) {
            if (now > 100000) {
                last_touch = now;
            } else {
                delay(100);
                continue;
            }
        }

        // no touches so stop showing until touch
        if ((now - last_touch) > display_sleep) {
            wait_for_touch();
            now = time(NULL);
            last_touch = now;
        }

        // handle any touches
        bool touched = check_touch();
        if (touched) {
            last_touch = time(NULL);
        }
        // if display touch changed or >1s passes then update
        if (touched || (lastsec != now)) {
            update_display();
            lastsec = now;
        }

        // wait a while
        delay(touch_delay);
    }
}

void display_init() {

    display_reset();
    xTaskCreate(displaytask, "displaytask", 10000, NULL, 1, &displaytask_handle);   
}
