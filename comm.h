/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_COMM_HEADER
#define SMTHERM_COMM_HEADER

struct SMTherm_Settings;
struct SensorData;

void comm(char *input,char *output,int output_max_size);

#endif
