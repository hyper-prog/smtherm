/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_THERM_HEADER
#define SMTHERM_THERM_HEADER

struct ThermData
{
    int   thermostat_on;
    float target_temp;
    float reference_temp;
    char  target_sensor[256];
    int   heating_on;
    float hist;

    float last_target_temp;
    float last_reference_temp;
};

void init_thermostat(void);
void thermostat_read_saved_state(const char *statefile);
void free_thermostat(const char *statefile);

void thermostat_ext_lock();
void thermostat_ext_unlock();

float calc_reference_temp(void);

int   do_thermostat(void);

int   switch_heater(int state);

void  set_target_temp_lowlevel(float tt);
void  set_target_sensor_lowlevel(const char *s);
void  set_thermostat_on_lowlevel(int o);
float get_target_temp_lowlevel(void);
const char* get_target_sensor_lowlevel(void);

void  set_target_temp(float tt);
void  set_thermostat_on(int on);

int   get_thermostat_working(void);
float get_target_temp(void);
float get_reference_temp(void);

int   get_heating_state(void);

#endif
