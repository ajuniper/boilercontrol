//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#pragma once
#include "mainsdetect.h"
#include "scheduler.h"
#include "scheduledoutput.h"

class HeatChannel {
private:
    int m_id;
    const char * const m_name;
    int m_pin_zv;
    int m_pin_zv_satisfied;
    MainsDetect m_ready;
    MainsDetect m_satisfied;
    int m_target_temp;
    int m_target_temp1;
    int m_target_temp2;
    bool m_enabled;
    bool m_active;
    int m_cooldown_duration;
    int m_endtime;
    int m_cooldown_time;
    int m_cooldown_mintemp;
    int m_cooldown_target;
    int m_sludge_duration;
    ScheduledOutput m_zv_output;
    ScheduledOutput m_zv_satisfied_output;
    bool m_changed;
    time_t m_lastTime;
    Scheduler m_scheduler;
    time_t m_suspendUntil;
    int m_suspend_duration;

    int m_y; // y display position

    void drawActive() const;
    void drawTimer() const;
    void drawCountdown() const;
    // record that the channel needs to be suspended (if it is not already)
    void needSuspend();

public:
    HeatChannel(
        int a_id,
        const char * a_name,
	int a_pin_o_zv,
	int a_pin_o_zv_satisfied,
	int a_pin_i_zv_ready,
	int a_pin_i_zv_satisfied,
        int a_target_temp1,
        int a_target_temp2,
	bool a_enabled,
	int a_cooldown_duration,
        int a_cooldown_mintemp,
        int a_y,
        int a_setback
    );

    // update timer states
    // returns true if something changed
    bool updateTimers(time_t now, unsigned long millinow);
    int getId() const { return m_id; }
    const char * getName() { return m_name; }
    bool getEnabled() { return m_enabled; }
    bool getActive() const { return m_enabled && m_active; };
    void setActive(bool a, bool updateConfig=false);
    time_t getTimer() { return m_endtime; }
    unsigned long getCooldown() { return m_cooldown_time; }
    bool getOutput() { return m_enabled && m_zv_output; }
    bool getSatisfied() { return m_enabled && m_zv_satisfied_output; }
    MainsDetect & getInputReady() { return m_ready; }
    MainsDetect & getInputSatisfied() { return m_satisfied; }
    void adjustTimer(int dt);
    // set the output state to the zv
    // returns true if state changes
    bool setOutput(bool state, time_t when = 0);
    bool setSatisfied(bool state, time_t when = 0);
    // update the output pin as required, and
    // return true if change is scheduled, false otherwise
    bool setOutputPin(time_t now);
    bool setSatisfiedPin(time_t now);
    // returns current target temp
    int targetTemp() const { return m_target_temp; }
    // return configured target temperatures
    // targettemp2 is optional and set to -1 if not supported
    int targetTemp1() const { return m_target_temp1; }
    int targetTemp2() const { return m_target_temp2; }
    void setTargetTemp1(int target) { m_target_temp1 = target; }
    void setTargetTemp2(int target) { m_target_temp2 = target; }
    void setTargetTemp(int target);
    void setTargetTempBySetting(int target); /* 0/1/2 */
    void setCooldownRuntime(int t) { m_cooldown_duration = t; }
    void setCooldownMintemp(int t) { m_cooldown_mintemp = t; }
    void setSludgeRuntime(int t) { m_sludge_duration = t; }
    void setSuspendTime(int t) { m_suspend_duration = t; }

    // should this channel be running?
    bool wantFire() const;
    // is the channel satisfied
    bool isSatisfied() const;
    // is the channel asking for a cooldown period?
    // i.e. ready for pump only
    bool wantCooldown() const;
    bool canCooldown() const;
    // is the channel ready for boiler and pump to run?
    bool canFire() const;

    // display handling for this channel
    void initDisplay();
    void updateDisplay();
    void drawName(bool warm) const; // F=hot T=warm
    void drawIO(int row, int oncolour, bool state) const;
    void readConfig();
    time_t lastTime() { return m_lastTime; }
    Scheduler & getScheduler() { return m_scheduler; }

    // returns true if channel is executing a change and pump/boiler
    // should be suspended
    bool isChanging(time_t now) { return m_suspendUntil > now;}
};

// values for use with adjustTimer
#define CHANNEL_TIMER_OFF 0
#define CHANNEL_TIMER_ON -1
#define CHANNEL_TIMER_SLUDGE -2

#define num_heat_channels 2
extern HeatChannel channels[];
extern void heatchannel_setup();
