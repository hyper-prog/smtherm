/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tools.h"
#include "conf.h"
#include "dht22.h"
#include "smtherm.h"
#include "comm.h"
#include "therm.h"

#define TMODE_ERROR  0
#define TMODE_SINGLE 1
#define TMODE_AVG    2
#define TMODE_DAV    3
#define TMODE_MIN    4
#define TMODE_MAX    5

struct ThermData therm;
pthread_mutex_t thermostat_lock;

void init_thermostat(void)
{
    therm.thermostat_on = 0;
    therm.target_temp = 20.0;
    therm.reference_temp = 20.0;
    therm.heating_on = 0;
    therm.last_target_temp = 20.0;
    therm.last_reference_temp = 20.0;
    therm.hyst = 0.25;
    therm.req_act = THERM_REQACT_NONE;
    therm.req_wait = 0;
    therm.req_notify = 0;
    h_strlcpy(therm.target_sensor,"",128);

    therm.emergencysaved_thermostat_on = -1;
    therm.emergencysaved_target_temp = -99;

    if(pthread_mutex_init(&thermostat_lock, NULL) != 0)
    {
        toLog(0,"Mutex init has failed!\n");
    }
}

int thermostat_read_saved_state(const char *statefile,int delete_file)
{
    FILE *sfp;
    if((sfp = fopen(statefile,"r")) != NULL)
    {
        char buffer[128],set[64];
        while(fgets(buffer, 128, sfp)  != NULL)
        {
           h_strlcpy(set,"tw:",64);
           if(strncmp(set,buffer,strlen(set)) == 0)
           {
               if(strcmp(trim(buffer + strlen(set)),"on") == 0)
                   therm.thermostat_on = 1;
               else
                   therm.thermostat_on = 0;
           }

           h_strlcpy(set,"tt:",64);
           if(strncmp(set,buffer,strlen(set)) == 0)
           {
               float f;
               if(sscanf(trim(buffer + strlen(set)),"%f",&f) == 1)
                   therm.target_temp = f;
           }

           h_strlcpy(set,"rt:",64);
           if(strncmp(set,buffer,strlen(set)) == 0)
           {
               float f;
               if(sscanf(trim(buffer + strlen(set)),"%f",&f) == 1)
                   therm.reference_temp = f;
           }

           h_strlcpy(set,"ltt:",64);
           if(strncmp(set,buffer,strlen(set)) == 0)
           {
               float f;
               if(sscanf(trim(buffer + strlen(set)),"%f",&f) == 1)
                   therm.last_target_temp = f;
           }

           h_strlcpy(set,"lrt:",64);
           if(strncmp(set,buffer,strlen(set)) == 0)
           {
               float f;
               if(sscanf(trim(buffer + strlen(set)),"%f",&f) == 1)
                   therm.last_reference_temp = f;
           }

           h_strlcpy(set,"heat:",64);
           if(strncmp(set,buffer,strlen(set)) == 0)
           {
               if(strcmp(trim(buffer + strlen(set)),"on") == 0)
                   therm.heating_on = 1;
               else
                   therm.heating_on = 0;
           }
        }
        fclose(sfp);

        if(delete_file)
        {
            if(unlink(statefile) != 0)
                toLog(1,"Warning: Cannot delete state file!\n");
        }
        return 1;
    }
    return 0;
}

void thermostat_emergency_write_state(const char *statefile)
{
    int need_save = 0;
    pthread_mutex_lock(&thermostat_lock);
    if(therm.emergencysaved_thermostat_on != therm.thermostat_on || therm.emergencysaved_target_temp != therm.target_temp)
    {
        need_save = 1;
        therm.emergencysaved_thermostat_on = therm.thermostat_on;
        therm.emergencysaved_target_temp = therm.target_temp;
    }
    pthread_mutex_unlock(&thermostat_lock);

    if(need_save)
        thermostat_write_state(statefile,0);
}

void thermostat_write_state(const char *statefile,int clean_shutdown)
{
    pthread_mutex_lock(&thermostat_lock);
    FILE *sfp;
    if((sfp = fopen(statefile,"w")) != NULL)
    {
        fprintf(sfp,"tw:%s\n",therm.thermostat_on ? "on" : "off");
        fprintf(sfp,"tt:%.1f\n",therm.target_temp);
        if(clean_shutdown)
        {
            fprintf(sfp,"rt:%.1f\n",therm.reference_temp);
            fprintf(sfp,"ltt:%.1f\n",therm.last_target_temp);
            fprintf(sfp,"lrt:%.1f\n",therm.last_reference_temp);
            fprintf(sfp,"heat:%s\n",therm.heating_on ? "on" : "off");
        }
        fprintf(sfp,"coff:%s\n",clean_shutdown ? "t" : "f");
        fclose(sfp);
    }
    pthread_mutex_unlock(&thermostat_lock);
}

void free_thermostat(const char *statefile)
{
    thermostat_write_state(statefile,1);
    pthread_mutex_destroy(&thermostat_lock);
}

/* Calculate reference temperature according to the "DefaultTargetSensor" config value
 *  Returns the calculated reference temperature or -100.0 if cannot calulate it.
 */
float calc_reference_temp(float target)
{
    int i;
    int mode = TMODE_ERROR;
    float svs[MAX_SENSOR_COUNT];
    int   sc = 0;

    toLog(2,"Calculating reference temp...\n");
    if(strlen(therm.target_sensor) < 5)
        return -100.0;

    if(!strncmp(therm.target_sensor,"min:",4))
        mode = TMODE_MIN;
    if(!strncmp(therm.target_sensor,"max:",4))
        mode = TMODE_MAX;
    if(!strncmp(therm.target_sensor,"avg:",4))
        mode = TMODE_AVG;
    if(!strncmp(therm.target_sensor,"dav:",4))
        mode = TMODE_DAV;
    if(!strncmp(therm.target_sensor,"one:",4))
        mode = TMODE_SINGLE;

    char isensorname[SENSOR_NAME_MAXLENGTH];
    int  ci = 0;
    int l = strlen(therm.target_sensor);
    for(i = 4 ; i < (l + 1); i++)
    {
        if(therm.target_sensor[i] == ',' || therm.target_sensor[i] == '\0')
        {
            isensorname[ci] = '\0';
            int tsi = get_index_by_name(isensorname);
            if(tsi >= 0)
            {
                if(is_sensor_last_read_success(tsi))
                {
                    struct TempHum th;
                    th =  get_sensor_value(tsi);
                    svs[sc] = th.temp;
                    ++sc;
                    toLog(2,"Value of \"%s\" is %.1f\n",isensorname,th.temp);
                }
            }
            ci = 0;
        }
        else
        {
            isensorname[ci++] = therm.target_sensor[i];
        }
    }

    if(mode == TMODE_SINGLE && sc > 0)
    {
        return svs[0];
    }

    if(mode == TMODE_MIN && sc > 0)
    {
        float f = 99.0;
        for(i = 0 ; i < sc ; ++i)
            if(f > svs[i])
                f = svs[i];
        if(f < 99.0)
            return f;
    }

    if(mode == TMODE_MAX && sc > 0)
    {
        float f = 0.0;
        for(i = 0 ; i < sc ; ++i)
            if(f < svs[i])
                f = svs[i];
        if(f > 0.0)
            return f;
    }

    if(mode == TMODE_AVG && sc > 0)
    {
        float sum = 0.0;
        for(i = 0 ; i < sc ; ++i)
            sum += svs[i];
        return floor((sum / sc) * 10) / 10;
    }

    if(mode == TMODE_DAV && sc > 0)
    {
        float sum = 0.0;
        float count = 0.0;
        for(i = 0 ; i < sc ; ++i)
        {
            float dif = target - svs[i];
            if(dif < 0.5) {
                sum += svs[i];
                count += 1.0;
            } else if(dif < 1.0) {
                sum += svs[i] * 1.5;
                count += 1.5;
            } else if(dif < 1.5) {
                sum += svs[i] * 1.75;
                count += 1.75;
            } else {
                sum += svs[i] * 2.0;
                count += 2.0;
            }
        }
        return floor((sum / count) * 10) / 10;
    }
    return -100.0;
}

void do_thermostat_in(void)
{
    float rt = calc_reference_temp(therm.target_temp);

    if(rt == -100.0) //cannot calculate reference temp (eg: cannot read sensors)
    {
        //If heating is on switch off, to prevent forever heating on sensor error.
        if(therm.heating_on)
        {
             therm.heating_on = 0;
             therm.req_act = THERM_REQACT_SWOFF;
             therm.req_wait = 0;
             therm.req_notify = 1;
        }
        return;
    }
    therm.last_reference_temp = therm.reference_temp;
    therm.reference_temp = rt;

    if(therm.thermostat_on)
    {
        int last_heating_on = therm.heating_on;
        if(therm.heating_on)
        {
            if(therm.reference_temp < therm.target_temp + therm.hyst)
            {
                therm.heating_on = 1;
            }
            else
            {
                therm.heating_on = 0;
            }
        }
        else
        {
            if(therm.reference_temp <= therm.target_temp - therm.hyst)
            {
                therm.heating_on = 1;
            }
            else
            {
                therm.heating_on = 0;
            }
        }

        if(last_heating_on != therm.heating_on)
        {
            if(therm.heating_on)
                therm.req_act = THERM_REQACT_SWON;
            else
                therm.req_act = THERM_REQACT_SWOFF;
            therm.req_wait = 0;
            therm.req_notify = 1;
        }
    }
    else //thermostat off
    {
         if(therm.heating_on)
         {
             therm.heating_on = 0;

             therm.req_act = THERM_REQACT_SWOFF;
             therm.req_wait = 0;
             therm.req_notify = 1;
         }
    }

    if(!therm.thermostat_on)
    {
        therm.heating_on = 0;
    }

    if(therm.last_reference_temp != therm.reference_temp)
    {
        therm.req_notify = 1;
    }
}

void thermostat_ext_lock(const char *from)
{
    pthread_mutex_lock(&thermostat_lock);
}

void thermostat_ext_unlock(const char *from)
{
    pthread_mutex_unlock(&thermostat_lock);
}

void do_thermostat(void)
{
    pthread_mutex_lock(&thermostat_lock);
    do_thermostat_in();
    pthread_mutex_unlock(&thermostat_lock);
}

int is_notify_required(void)
{
    int nr;
    pthread_mutex_lock(&thermostat_lock);
    nr = therm.req_notify;
    pthread_mutex_unlock(&thermostat_lock);
    return nr;
}

void clear_notify_required(void)
{
    pthread_mutex_lock(&thermostat_lock);
    therm.req_notify = 0;
    pthread_mutex_unlock(&thermostat_lock);
}

int is_req_heater_action(void)
{
    int ra = THERM_REQACT_NONE;
    pthread_mutex_lock(&thermostat_lock);
    if(therm.req_act != THERM_REQACT_NONE)
    {
        if(therm.req_wait >= THERM_REQWAIT_THRES)
        {
            ra = therm.req_act;
        }
        else
        {
            therm.req_wait++;
        }
    }
    pthread_mutex_unlock(&thermostat_lock);
    return ra;
}

void clear_req_heater_action(void)
{
    pthread_mutex_lock(&thermostat_lock);
    therm.req_act = THERM_REQACT_NONE;
    therm.req_wait = 0;
    pthread_mutex_unlock(&thermostat_lock);
}

void set_target_temp(float tt)
{
    pthread_mutex_lock(&thermostat_lock);
    therm.last_target_temp = therm.target_temp;
    therm.target_temp = tt;
    pthread_mutex_unlock(&thermostat_lock);

    if(therm.last_target_temp != therm.target_temp)
    {
        do_thermostat();
        do_sse_notify_if_required();
    }
}

void set_thermostat_on(int on)
{
    pthread_mutex_lock(&thermostat_lock);
    therm.thermostat_on = on;
    pthread_mutex_unlock(&thermostat_lock);

    do_thermostat();
    do_sse_notify_if_required();
}

void do_sse_notify_if_required(void)
{
    if(is_notify_required())
    {
        clear_notify_required();
        send_sse_message("thermostat=ThermostatDataChanged");
    }
}

void set_target_temp_lowlevel(float tt)
{
    therm.target_temp = tt;
}

void set_target_sensor_lowlevel(const char *s)
{
    h_strlcpy(therm.target_sensor,s,128);
}

void set_thermostat_on_lowlevel(int o)
{
    therm.thermostat_on = o;
}

float get_target_temp_lowlevel(void)
{
    return therm.target_temp;
}

const char* get_target_sensor_lowlevel(void)
{
    return therm.target_sensor;
}

void set_hystersis_lowlevel(float h)
{
    therm.hyst = h;
}

int get_thermostat_working()
{
    return therm.thermostat_on;
}

float get_target_temp(void)
{
    return therm.target_temp;
}

float get_reference_temp(void)
{
    return therm.reference_temp;
}

int get_heating_state(void)
{
    return therm.heating_on;
}

