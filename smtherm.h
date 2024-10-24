/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_SMTHERM_HEADER
#define SMTHERM_SMTHERM_HEADER

#define NAME "smtherm"
#define LONGNAME "Simple managed thermostat daemon"

#define VERSION "0.1"

#define SENSOR_TYPE_UNKNOWN    0
#define SENSOR_TYPE_DHT22      1

#define MAX_SENSOR_COUNT       9
#define SENSOR_NAME_MAXLENGTH  32

#define TCP_OUTPUT_BUFFER_SIZE 20480

struct SensorData
{
    char   active;
    char   last_read_success;
    char   name[SENSOR_NAME_MAXLENGTH];
    int    type;
    int    gpio_pin;

    //current measured data
    time_t rtime;
    float  temp;
    float  hum;
    //last measured data
    time_t last_rtime;
    float  last_temp;
    float  last_hum;
    //last advertised data
    time_t adv_rtime;
    float  adv_temp;
    float  adv_hum;

    void   *lowlevel_data;
};

struct TempHum
{
    float  temp;
    float  hum;
};

struct SMTherm_Settings
{
    int   sensor_count;
    int   loglevel;
    int   nodaemon;
    int   tcpport;
    int   sensor_poll_time;
    char  logfile[128];
    char  pidfile[128];
    char  statefile[128];
    char  sse_host[128];
    int   sse_port;

    char  heater_switch_mode[16]; // "gpio","shelly"
    int   heater_switch_gpio;
    char  heater_switch_shelly_address[64];
    int   heater_switch_shelly_indeviceid;

};

int loglevel(void);
int nodaemon(void);
char *logfile(void);

float get_target_temp(void);
float get_reference_temp(void);
void  set_target_temp(float tt);
int   get_heating_state(void);
int   get_index_by_name(const char *n);

int   max_sensor_count(void);
int   is_sensor_active(int index);
int   is_sensor_last_read_success(int index);
const char * get_sensor_name(int index);
struct TempHum get_sensor_value(int index);
int   get_sensor_statistics(char *buffer,int buffer_length,int index);

int   send_sse_message(const char *message);

int   switch_heater(int state);

#endif
