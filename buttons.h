#include "displayconfig.h"
//////////////////////////////////////////////////////////////////
//
// button classes
class button_t {
    public:
        button_t(uint16_t a_x, uint16_t a_y,
                 uint16_t a_w, uint16_t a_h,
                 bool (*a_p)(int, time_t),
                 bool (*a_r)(int, time_t),
                 int a_i
                ) :
            m_x1(a_x-button_expand),
            m_x2(a_x+a_w+button_expand),
            m_y2(240-a_y+button_expand),
            m_y1(240-a_y-a_h-button_expand),
            m_presstime(0),
            m_lastnotifytime(0),
            m_pressed(a_p),
            m_released(a_r),
            m_handle(a_i),
            m_mustrelease(false)
        {
        };

        // notify current press state
        void checkPressed(uint16_t a_x, uint16_t a_y);
        void initialise();
    private:
        uint16_t m_x1,m_x2;
        uint16_t m_y1,m_y2;
        time_t m_presstime;
        time_t m_lastnotifytime;
        bool (*m_pressed)(int, time_t);
        bool (*m_released)(int, time_t);
        int m_handle;
        bool m_mustrelease;
};

extern button_t buttons[];
extern const size_t num_buttons;
extern bool check_touch();
