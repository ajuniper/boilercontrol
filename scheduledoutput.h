// simple class to request future change to output
#pragma once
class ScheduledOutput {
    private:
        time_t m_when;
        bool m_nextstate;
        bool m_currentstate;
    public:
        ScheduledOutput(bool state = false) :
            m_when(0),
            m_nextstate(state),
            m_currentstate(false)
        {}
        bool setState(bool s, time_t w=0) {
            if (s == m_currentstate) {
                // skip any pending change
                m_when = 0;
            } else if (w == 0) {
                // immediate change requested
                // we never update current state here, that only happens
                // in checkState
                m_when = 1;
                m_nextstate = s;
            } else if (m_when == 0) {
                // schedule change, none pending already
                m_when = w;
                m_nextstate = s;
            } else {
                // no action required, a change is already pending
            }
            return m_currentstate;
        }
        bool currentState() {
            return m_currentstate;
        }
        bool checkState(time_t now) {
            if ((m_when > 0) && (now > m_when)) {
                // time to change state
                m_when = 0;
                m_currentstate = m_nextstate;
            }
            return m_currentstate;
        }
        time_t nextChange() const {
            return m_when;
        }
        operator bool() const {
            return m_currentstate;
        }
};

