/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_THERM_HEADER
#define SMTHERM_THERM_HEADER

#define THERM_REQACT_NONE  0
#define THERM_REQACT_SWON  1
#define THERM_REQACT_SWOFF 2

#define THERM_REQWAIT_THRES  1

struct ThermData
{
    int   thermostat_on;
    float target_temp;
    float reference_temp;
    char  target_sensor[256];
    int   heating_on;
    float hyst;

    float last_target_temp;
    float last_reference_temp;

    int   req_act;
    int   req_wait;
    int   req_notify;

    int   emergencysaved_thermostat_on;
    float emergencysaved_target_temp;

};

void init_thermostat(void);
int  thermostat_read_saved_state(const char *statefile,int delete_file);
void thermostat_write_state(const char *statefile,int clean_shutdown);
void thermostat_emergency_write_state(const char *statefile);
void free_thermostat(const char *statefile);

void thermostat_ext_lock(const char *from);
void thermostat_ext_unlock(const char *from);

float calc_reference_temp(float target);

void  do_thermostat(void);

void  set_target_temp_lowlevel(float tt);
void  set_target_sensor_lowlevel(const char *s);
void  set_thermostat_on_lowlevel(int o);
void  set_hystersis_lowlevel(float h);
float get_target_temp_lowlevel(void);
const char* get_target_sensor_lowlevel(void);

void  set_target_temp(float tt);
void  set_thermostat_on(int on);

int   get_thermostat_working(void);
float get_target_temp(void);
float get_reference_temp(void);

int   get_heating_state(void);

void  do_sse_notify_if_required(void);
int   is_notify_required(void);
void  clear_notify_required(void);

int   is_req_heater_action(void);
void  clear_req_heater_action(void);

#endif
