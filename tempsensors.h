//////////////////////////////////////////////////////////////////
//
// temperature sensors

class tempsensor_t {
    public:
        tempsensor_t (const char * a_addr, const char * a_name, int * a_value = NULL) :
        m_addr(a_addr),
        m_name(a_name),
        m_value(a_value),
        m_temp(0)
        {
        }
        const char * getName() const { return m_name; }
        const char * getAddr() const { return m_addr; }
        int getTemp() const;
        void setTemp(int t) { m_temp = t; }
    private:
        const char * m_addr;
        const char * m_name;
        int * m_value;
        int m_temp;
};
extern tempsensor_t temperatures[];
extern const size_t num_temps;
extern void tempsensors_init();
// get a reading given a name
extern int tempsensors_get(const char * name);
