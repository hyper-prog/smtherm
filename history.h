/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_HISTORY_HEADER
#define SMTHERM_HISTORY_HEADER

#define HISTORY_MAX_ITEMCOUNT          1024
#define HISTORY_STORE_PERIOD_MIN       5
#define HISTORY_BUFFEREND_SAFETYLENGTH 26

#define HEATERSW_HISTORY_MAX_ITEMCOUNT 64

int init_history_database(void);

int history_store_sensors(void);
void hsw_history_store(int what);

int free_history_database(void);

void get_history_data(char *buffer,int buffersize,const char *sensorname,int startoffset,int count);
void get_hsw_history_data(char *buffer,int buffersize);

#endif
