//////////////////////////////////////////////////////////////////////////////
//
// Mains detection objects
class MainsDetect {
private:
    // pin number
    int m_pin;
    // channel name and usage name
    const char * m_name1;
    const char * m_name2;
    // average voltage value
    int m_zerov;
    // end time of debounce period
    unsigned long m_debounce;
    // leaky bucket of pin state
    int m_pinstate;
    // reported state
    bool m_state;
    // if pin is disabled due to bad averaging
    bool m_disabled;
    // the distance from 0v that will be treated as mains present
    int m_threshold;
    // number of ticks to go before declaring off
    // needs configurable in case half cycle from MPZV has gaps
    int m_ticks;
    // how long to debounce for
    unsigned long m_debounce_t;

public:
    MainsDetect(int a_pin, const char * a_name1, const char * a_name2);

    // calibrate the zero volts level
    void calibrate();

    // refresh the state of the pin (every millisecond)
    void checkstate();

    // check for debounce expiry and report if changed
    bool updatepin(unsigned long a_millinow);

    // report debounced pin state
    bool pinstate() const { return m_state; }

    // update the configured threshold
    void setThreshold(int t) { m_threshold = t; }
    void setTicks(int t) { m_ticks = t; }
    void setDebounceT(int t) { m_debounce_t = t; }
};
