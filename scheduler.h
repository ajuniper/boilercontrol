//////////////////////////////////////////////////////////////////////////////
//
// Scheduler
#pragma once

class HeatChannel;

class Scheduler {
    private:
        HeatChannel & mChannel;
        time_t mBaseWarmup;
        time_t mLastChange;
        unsigned char mSchedule[7][24][4];
    public:
        Scheduler (HeatChannel & aChannel, int setback);
        // returns null if all ok, error string otherwise
        const char * set(int d, const String & hm, bool state);
        const char * get(int d, const String & hm, bool &i);
        unsigned char get(int d, int h, int m)
        {
            return mSchedule[d][h][m];
        }

        void checkSchedule(int d, int h, int m);
        time_t lastChange(); // time of last change, 0 if none pending
        void saveChanges();
        void readConfig();
        time_t getBaseWarmup() const { return mBaseWarmup; }
        time_t getWarmup() const;
        void setWarmup(time_t t) { mBaseWarmup = t; }
};
extern void scheduler_setup();
