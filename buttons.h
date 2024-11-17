#include "displayconfig.h"
//////////////////////////////////////////////////////////////////
//
// button classes
class button_t {
    public:
        button_t(int a_x, int a_y,
                 int a_w, int a_h,
                 bool (*a_p)(int, time_t),
                 bool (*a_r)(int, time_t),
                 int a_i
                );

        // notify current press state
        void checkPressed(uint16_t a_x, uint16_t a_y);
        void initialise();
    private:
        uint16_t m_x1,m_x2;
        uint16_t m_y1,m_y2;
        time_t m_presstime;
        time_t m_lastnotifytime;
        int64_t m_presstime_us;
        bool (*m_pressed)(int, time_t);
        bool (*m_released)(int, time_t);
        int m_handle;
        bool m_mustrelease;
};

extern button_t buttons[];
extern const size_t num_buttons;
extern bool check_touch();
extern void buttons_init();
