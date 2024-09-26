//////////////////////////////////////////////////////////////////////////////
//
// Scheduler
#pragma once

class HeatChannel;

class Scheduler {
    private:
        HeatChannel & mChannel;
        // scheduler adjustment configuration
        // base amount always added
        time_t mBaseWarmup;
        // gradually start earlier
        int mAdvanceStartTemp;
        float mAdvanceRate;
        int mAdvanceLimit;
        // optionally run hotter
        int mRunHotterTemp;
        // gradually run longer
        int mExtendStartTemp;
        float mExtendRate;
        int mExtendLimit;

        // when things have happened
        time_t mLastChange;
        time_t mLastSchedule;
        time_t mCancelledUntil;
        int mLastTarget;
        // holds 0/1/2
        char mSchedule[7][24][4];
        static const int num_days = 7;
        static const int num_hours = 24;
        static const int num_mins = 4;

    public:
        Scheduler (HeatChannel & aChannel, int setback);
        // returns null if all ok, error string otherwise
        const char * set(int d, const String & hm, char state);
        void set(int d, int h, int m, char state);
        const char * get(int d, const String & hm, char &i) const;
        char get(int d, int h, int m) const
        {
            return mSchedule[d][h][m];
        }

        void checkSchedule(int d, int h, int m);
        time_t lastChange(); // time of last change, 0 if none pending
        void saveChanges();
        void readConfig();
        time_t getBaseWarmup() const { return mBaseWarmup; }
        time_t getWarmup() const;
        time_t getExtend() const;
        bool getBoost() const;
        void setWarmup(int w) { mBaseWarmup = w; }
        void setAdvanceStartTemp(int i) { mAdvanceStartTemp = i; }
        void setAdvanceRate(float r) { mAdvanceRate = r; }
        void setAdvanceLimit(int l) { mAdvanceLimit = l; }
        void setBoostTemp(int b) { mRunHotterTemp = b; }
        void setExtendStartTemp(int i) { mExtendStartTemp = i; }
        void setExtendRate(float r) { mExtendRate = r; }
        void setExtendLimit(int l) { mExtendLimit = l; }
        void turnedOff(time_t endtime);
        void wakeUp();
        static const int num_slots = num_days * num_hours * num_mins;
};
extern void scheduler_setup();
