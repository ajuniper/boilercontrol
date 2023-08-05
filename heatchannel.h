//////////////////////////////////////////////////////////////////////////////
//
// Heat channel objects
#pragma once
#include "mainsdetect.h"
#include "scheduler.h"

class HeatChannel {
private:
    int m_id;
    const char * const m_name;
    int m_pin_zv;
    int m_pin_zv_satisfied;
    MainsDetect m_ready;
    MainsDetect m_satisfied;
    int m_target_temp;
    bool m_enabled;
    bool m_active;
    int m_cooldown_duration;
    int m_endtime;
    int m_cooldown_time;
    bool m_zv_output;
    bool m_zv_satisfied_output;
    bool m_changed;
    time_t m_lastTime;
    Scheduler m_scheduler;

    int m_y; // y display position

    void drawActive() const;
    void drawTimer() const;
    void drawCountdown() const;
public:
    HeatChannel(
        int a_id,
        const char * a_name,
	int a_pin_o_zv,
	int a_pin_o_zv_satisfied,
	int a_pin_i_zv_ready,
	int a_pin_i_zv_satisfied,
        int a_target_temp,
	bool a_enabled,
	int a_cooldown_duration,
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
    void setActive(bool a) {
        if (!a) {
            m_endtime = 0;
            m_cooldown_time = 0;
        }
        m_active = a;
        m_changed = true;
    };
    time_t getTimer() { return m_endtime; }
    unsigned long getCooldown() { return m_cooldown_time; }
    bool getOutput() { return m_enabled && m_zv_output; }
    bool getSatisfied() { return m_enabled && m_zv_satisfied_output; }
    MainsDetect & getInputReady() { return m_ready; }
    MainsDetect & getInputSatisfied() { return m_satisfied; }
    void adjustTimer(int dt);
    // set the output state to the zv
    void setOutput(bool state);
    void setSatisfied(bool state);
    int targetTemp() const { return m_target_temp; }
    void setTargetTemp(int target);

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
    void drawIO(int row, int oncolour, bool state) const;
    void readConfig();
    time_t lastTime() { return m_lastTime; }
    Scheduler & getScheduler() { return m_scheduler; }
};

extern const size_t num_heat_channels;
extern HeatChannel channels[];
extern void heatchannel_setup();
