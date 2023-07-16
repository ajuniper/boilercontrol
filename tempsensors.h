//////////////////////////////////////////////////////////////////
//
// temperature sensors

class tempsensor_t {
    public:
        tempsensor_t (const char * a_addr, const char * a_name) :
        m_addr(a_addr),
        m_name(a_name),
        m_temp(0)
        {
        }
        const char * getName() const { return m_name; }
        int getTemp() const;
        void setTemp(int t) { m_temp = t; }
    private:
        const char * m_addr;
        const char * m_name;
        int m_temp;
};
extern tempsensor_t temperatures[];
extern const size_t num_temps;
extern void tempsensors_init();
