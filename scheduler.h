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
        time_t mLastSchedule;
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
        void setWarmup(time_t t) { mBaseWarmup = t; }
        static const int num_slots = num_days * num_hours * num_mins;
};
extern void scheduler_setup();
