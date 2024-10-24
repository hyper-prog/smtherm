/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#include "smtherm.h"
#include "therm.h"
#include "tools.h"

void read_config_item(char *item_line,struct SMTherm_Settings *smtc,struct SensorData *seda)
{
    char confs[128];

    if(item_line[0] == '#')
        return;

    h_strlcpy(confs,"SensorCount=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->sensor_count = dec;
        return;
    }

    h_strlcpy(confs,"LogLevel=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->loglevel = dec;
        return;
    }

    h_strlcpy(confs,"TcpPort=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->tcpport = dec;
        return;
    }

    h_strlcpy(confs,"SSEPort=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->sse_port = dec;
        return;
    }

    h_strlcpy(confs,"SSEHost=",256);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        h_strlcpy(smtc->sse_host,ltrim(item_line + strlen(confs)),128);
        return;
    }

    h_strlcpy(confs,"DefaultTargetTemp=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        float flt;
        if(sscanf(ltrim(item_line + strlen(confs)),"%f",&flt) == 1)
            set_target_temp_lowlevel(flt);
        return;
    }

    h_strlcpy(confs,"DefaultTargetSensor=",256);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        set_target_sensor_lowlevel(ltrim(item_line + strlen(confs)));
        return;
    }

    h_strlcpy(confs,"SensorPollTime=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->sensor_poll_time = dec;
        return;
    }

    h_strlcpy(confs,"LogFile=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        h_strlcpy(smtc->logfile,ltrim(item_line + strlen(confs)),128);
        return;
    }

    h_strlcpy(confs,"PidFile=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        h_strlcpy(smtc->pidfile,ltrim(item_line + strlen(confs)),128);
        return;
    }

    h_strlcpy(confs,"StateFile=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        h_strlcpy(smtc->statefile,ltrim(item_line + strlen(confs)),128);
        return;
    }

    h_strlcpy(confs,"NoDaemon=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        if(strcmp(ltrim(item_line + strlen(confs)),"yes") == 0)
            smtc->nodaemon = 1;
        else
            smtc->nodaemon = 0;
        return;
    }

    h_strlcpy(confs,"DefaultThermostatState=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        if(strcmp(ltrim(item_line + strlen(confs)),"on") == 0)
            set_thermostat_on_lowlevel(1);
        else
            set_thermostat_on_lowlevel(1);
        return;
    }

    h_strlcpy(confs,"HeaterSwitchMode=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        h_strlcpy(smtc->heater_switch_mode,ltrim(item_line + strlen(confs)),16);
        return;
    }

    h_strlcpy(confs,"HeaterSwitchGpio=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->heater_switch_gpio = dec;
        return;
    }

    h_strlcpy(confs,"HeaterSwitchShellyAddress=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        h_strlcpy(smtc->heater_switch_shelly_address,ltrim(item_line + strlen(confs)),64);
        return;
    }

    h_strlcpy(confs,"HeaterSwitchShellyInDeviceId=",128);
    if(strncmp(confs,item_line,strlen(confs)) == 0)
    {
        int dec;
        if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
            smtc->heater_switch_shelly_indeviceid = dec;
        return;
    }

    int ss;
    for(ss = 1 ; ss <= 9 ; ++ss)
    {
        snprintf(confs,128,"Sensor%dName=",ss);
        if(strncmp(confs,item_line,strlen(confs)) == 0)
        {
            h_strlcpy(seda[ss].name,ltrim(item_line + strlen(confs)),SENSOR_NAME_MAXLENGTH);
            return;
        }

        snprintf(confs,128,"Sensor%dType=",ss);
        if(strncmp(confs,item_line,strlen(confs)) == 0)
        {
            seda[ss].type = SENSOR_TYPE_UNKNOWN;
            if(strcmp(ltrim(item_line + strlen(confs)),"dht22") == 0)
                seda[ss].type = SENSOR_TYPE_DHT22;
            return;
        }

        snprintf(confs,128,"Sensor%dGpioPin=",ss);
        if(strncmp(confs,item_line,strlen(confs)) == 0)
        {
            int dec;
            if(sscanf(ltrim(item_line + strlen(confs)),"%d",&dec) == 1)
                seda[ss].gpio_pin = dec;
            return;
        }
    }
}

void read_config_file(char *cf,struct SMTherm_Settings *smtc,struct SensorData *seda)
{
    FILE *cffp = NULL;
    int bufferLength = 255;
    char buffer[bufferLength];

    toLog(2,"Read config file: %s\n",cf);
    cffp = fopen(cf,"r");
    if(cffp == NULL)
    {
        toLog(0,"Cannot open config file!\n");
        exit(1);
    }

    while(fgets(buffer, bufferLength, cffp))
    {
        char *trimmed = trim(buffer);
        if(strlen(trimmed) == 0)
            continue;
        if(trimmed[0] == '#')
            continue;

        read_config_item(trimmed,smtc,seda);
    }
    fclose(cffp);
}

void printconfig(struct SMTherm_Settings *smtc)
{
    toLog(2,"SensorCount: %d\n",smtc->sensor_count);
    toLog(2,"LogLevel: %d\n",smtc->loglevel);
    toLog(2,"NoDaemon: %d\n",smtc->nodaemon);
    toLog(2,"LogFile: %s\n",smtc->logfile);
    toLog(2,"PidFile: %s\n",smtc->pidfile);
    toLog(2,"TargetTemp: %f\n",get_target_temp_lowlevel());
    toLog(2,"TargetSensor: %s\n",get_target_sensor_lowlevel());
}
