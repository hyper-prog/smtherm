/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "history.h"
#include "smtherm.h"
#include "tools.h"

struct OneSensorHistory
{
    int    index;
    int    active;
    char   name[SENSOR_NAME_MAXLENGTH];

    unsigned short int *temparray;
    unsigned char   *humarray;
};

struct HeaterActionHistory
{
    unsigned long int when;
    int what;
};

int history_sensor_count;

int wpos;
struct OneSensorHistory history[MAX_SENSOR_COUNT];
unsigned int timetable[HISTORY_MAX_ITEMCOUNT];
time_t startTime;
time_t lastHistorySave;

int hsw_wpos;
struct HeaterActionHistory hsw_history[HEATERSW_HISTORY_MAX_ITEMCOUNT];

pthread_mutex_t hlock;

int init_history_database(void)
{
    toLog(2,"Init history database...\n");
    pthread_mutex_lock(&hlock);
    wpos = 0;
    startTime = time(NULL);
    history_sensor_count = 0;
    lastHistorySave = 0;
    int i;

    for(i = 0 ; i < HISTORY_MAX_ITEMCOUNT ; ++i)
        timetable[i] = 0;

    for(i = 0 ; i < max_sensor_count() ; ++i)
    {
        history[i].index = 0;
        history[i].active = 0;
        history[i].temparray = NULL;
        history[i].humarray = NULL;
    }

    for(i = 0 ; i < max_sensor_count() ; ++i)
    {
        if(is_sensor_active(i))
        {
            history[history_sensor_count].index = i;
            history[history_sensor_count].active = 1;
            h_strlcpy(history[history_sensor_count].name,get_sensor_name(i),SENSOR_NAME_MAXLENGTH);
            history[history_sensor_count].temparray = (unsigned short int *)malloc(sizeof(unsigned short int) * HISTORY_MAX_ITEMCOUNT);
            history[history_sensor_count].humarray = (unsigned char *)malloc(sizeof(unsigned char) * HISTORY_MAX_ITEMCOUNT);
            int ii;
            for(ii = 0 ; ii < HISTORY_MAX_ITEMCOUNT ; ++ii)
            {
                history[history_sensor_count].temparray[ii] = 0;
                history[history_sensor_count].humarray[ii] = 0;
            }

            ++history_sensor_count;
        }
    }

    hsw_wpos = 0;
    for(i = 0 ; i < HEATERSW_HISTORY_MAX_ITEMCOUNT ; ++i)
    {
        hsw_history[i].when = 0;
        hsw_history[i].what = 0;
    }

    pthread_mutex_unlock(&hlock);
    toLog(2,"History init done.\n");
    return 0;
}

int history_store_sensors(void)
{
    int i;

    toLog(2,"History store...\n");
    time_t now;
    now = time(NULL);

    if(lastHistorySave != 0 && (lastHistorySave + (60 * HISTORY_STORE_PERIOD_MIN)) > now)
    {
        return history_sensor_count;
    }

    pthread_mutex_lock(&hlock);
    lastHistorySave = now;
    toLog(2,"Determine store pos...\n");
    if(wpos >= HISTORY_MAX_ITEMCOUNT)
        wpos = 0;
    toLog(2,"Store timestamp...\n");
    timetable[wpos] = (unsigned int)(now - startTime);
    toLog(2,"Store sensor values...\n");
    for(i = 0 ; i < history_sensor_count ; ++i)
    {
        struct TempHum th = get_sensor_value(history[i].index);
        history[i].temparray[wpos] = (unsigned short int) ((th.temp + 50) * 10);
        history[i].humarray[wpos] = (unsigned char) th.hum;
        toLog(4,"--- Stored values to wpos:%d time:%d temp:%d hum:%d\n",wpos,timetable[wpos],history[i].temparray[wpos],history[i].humarray[wpos]);
    }
    wpos++;
    toLog(3,"History store done. (wpos:%d)\n",wpos);
    pthread_mutex_unlock(&hlock);
    return history_sensor_count;
}

void hsw_history_store(int what)
{
    time_t now;
    now = time(NULL);

    pthread_mutex_lock(&hlock);
    hsw_history[hsw_wpos].when = now;
    hsw_history[hsw_wpos].what = what;
    ++hsw_wpos;
    if(hsw_wpos >= HEATERSW_HISTORY_MAX_ITEMCOUNT)
        hsw_wpos = 0;
    pthread_mutex_unlock(&hlock);
}

void get_history_data(char *buffer,int buffersize,const char *sensorname,int startoffset,int count)
{
    int i;
    toLog(2,"Search:%s\n",sensorname);
    toLog(2,"history_sensor_count:%d\n",history_sensor_count);
    pthread_mutex_lock(&hlock);
    snprintf(buffer,buffersize,"{}");
    for(i = 0 ; i < history_sensor_count ; ++i)
    {
        if(!strcmp(history[i].name,sensorname))
        {
            toLog(2,"Found, wpos:%d startoffset:%d count:%d\n",wpos,startoffset,count);
            int offset = 0;
            int ic = 0;
            char lb[64];
            int s_wpos = 0,e_wpos = wpos - 1;

            s_wpos = ((wpos - 1) - startoffset) - count;
            if(s_wpos < 0)
                s_wpos = HISTORY_MAX_ITEMCOUNT + s_wpos;
            e_wpos = (wpos - 1) - startoffset;
            if(e_wpos < 0)
                e_wpos = HISTORY_MAX_ITEMCOUNT + e_wpos;

            toLog(2,"Get sensor data: s_wpos:%d e_wpos:%d, timetable[s_wpos]: %u, timetable[e_wpos]: %u \n",s_wpos,e_wpos,timetable[s_wpos],timetable[e_wpos]);

            snprintf(lb,64,"{\"st\":%lu,\"d\":[",(unsigned long)startTime);
            snprintf(buffer+offset,buffersize-offset,"%s",lb);
            offset += strlen(lb);

            while(1)
            {
                if(offset + HISTORY_BUFFEREND_SAFETYLENGTH > buffersize)
                    break;

                if(s_wpos >= HISTORY_MAX_ITEMCOUNT)
                    s_wpos = 0;

                if(history[i].temparray[s_wpos] > 0 && (timetable[s_wpos] > 0 || ic == 0))
                {
                    float tval;
                    tval = ((double)(history[i].temparray[s_wpos]) / 10) - 50;

                    snprintf(lb,64,"%s[%u,%.1f,%u]",
                        ic == 0 ? "" : "," ,
                        timetable[s_wpos],
                        tval,
                        history[i].humarray[s_wpos]);

                    snprintf(buffer+offset,buffersize-offset,"%s",lb);
                    offset += strlen(lb);
                    ++ic;
                }

                if(s_wpos == e_wpos)
                    break;

                s_wpos++;
            }
            snprintf(lb,64,"]}");
            snprintf(buffer+offset,buffersize-offset,"%s",lb);
            offset += strlen(lb);
        }
    }
    pthread_mutex_unlock(&hlock);
}

void get_hsw_history_data(char *buffer,int buffersize)
{
    pthread_mutex_lock(&hlock);

    int offset = 0;
    char lb[64];

    snprintf(lb,64,"{\"hswhist\":[");
    snprintf(buffer+offset,buffersize-offset,"%s",lb);
    offset += strlen(lb);

    int r,first = 1;
    for(r = 0 ; r < HEATERSW_HISTORY_MAX_ITEMCOUNT ; r++)
    {
        if(hsw_history[(hsw_wpos + r) % HEATERSW_HISTORY_MAX_ITEMCOUNT].when != 0 &&
           hsw_history[(hsw_wpos + r) % HEATERSW_HISTORY_MAX_ITEMCOUNT].what != 0 )
        {
            snprintf(lb,64,"%s[%lu,%d]",
                           first ? "" : ",",
                           hsw_history[(hsw_wpos + r) % HEATERSW_HISTORY_MAX_ITEMCOUNT].when,
                           hsw_history[(hsw_wpos + r) % HEATERSW_HISTORY_MAX_ITEMCOUNT].what);
            snprintf(buffer+offset,buffersize-offset,"%s",lb);
            offset += strlen(lb);
            first = 0;
        }
    }

    snprintf(lb,64,"]}");
    snprintf(buffer+offset,buffersize-offset,"%s",lb);
    offset += strlen(lb);

    pthread_mutex_unlock(&hlock);
}

int free_history_database(void)
{
    int i;
    pthread_mutex_lock(&hlock);
    for(i = 0 ; i < history_sensor_count ; ++i)
        if(history[history_sensor_count].active)
        {
            if(history[history_sensor_count].temparray != NULL)
                free(history[history_sensor_count].temparray);
            if(history[history_sensor_count].humarray != NULL)
                free(history[history_sensor_count].humarray);
        }
    pthread_mutex_unlock(&hlock);
    return 0;
}
