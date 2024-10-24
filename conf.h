/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_CONF_HEADER
#define SMTHERM_CONF_HEADER

struct SMTherm_Settings;
struct SensorData;

void read_config_file(char *cf,struct SMTherm_Settings *smtc,struct SensorData *seda);
void printconfig(struct SMTherm_Settings *smtc);

#endif
