//////////////////////////////////////////////////////////////////////////////
//
// Scheduler
#pragma once

class HeatChannel;

class Scheduler {
    private:
        HeatChannel & mChannel;
        int mBaseWarmup;
        time_t mLastChange;
        unsigned char mSchedule[7][24][4];
    public:
        Scheduler (HeatChannel & aChannel, int setback);
        // returns null if all ok, error string otherwise
        const char * set(int d, const String & hm, bool state);
        const char * get(int d, const String & hm, bool &i);
        void checkSchedule(int d, int h, int m);
        time_t lastChange(); // time of last change, 0 if none pending
        void saveChanges();
        void readConfig();
};
extern void scheduler_setup();
