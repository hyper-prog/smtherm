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
#include "history.h"
#include "therm.h"
#include "tools.h"

#define MAX_VALUE_NAME_PAIRS 16
#define MAX_NAME_LENGTH      64
#define MAX_VALUE_LENGTH     256

struct ValueNamePairs
{
    char name[MAX_NAME_LENGTH + 1];
    char value[MAX_VALUE_LENGTH + 1];
};

struct ValueNamePairs pairs[MAX_VALUE_NAME_PAIRS];

int is_valid_input(char *input,struct ValueNamePairs *p) //returns the counts of name value pairs
{
    int i;
    int ci;
    int tc = 0;
    int state = 1; // 1:namestart 2:in_name 3:valuestart 4:in_value

    for(i = 0 ; i < MAX_VALUE_NAME_PAIRS ; ++i)
    {
        h_strlcpy(p[i].name,"",MAX_NAME_LENGTH);
        h_strlcpy(p[i].value,"",MAX_VALUE_LENGTH);
    }

    int l = strlen(input);
    for(i = 0 ; i < l ; ++i)
        if(!isalnum(input[i]) && input[i] != ':' && input[i] != '.' && input[i] != ';')
            return 0;
    ci = 0;
    for(i = 0 ; i < l ; ++i)
    {
        if(state == 1)
        {
            if(isalpha(input[i]))
            {
                state = 2;
                if(ci < MAX_NAME_LENGTH)
                {
                    p[tc].name[ci] = input[i];
                    ++ci;
                    p[tc].name[ci] = '\0';
                }
            }
            else
                return 0;
            continue;
        }
        if(state == 2)
        {
            if(isalnum(input[i]))
            {
                state = 2;
                if(ci < MAX_NAME_LENGTH)
                {
                    p[tc].name[ci] = input[i];
                    ++ci;
                    p[tc].name[ci] = '\0';
                }
            }
            else if(input[i] == ':')
            {
                state = 3;
                ci = 0;
            }
            else
                return 0;
            continue;
        }
        if(state == 3)
        {
            if(isalnum(input[i]))
            {
                state = 4;
                if(ci < MAX_VALUE_LENGTH)
                {
                    p[tc].value[ci] = input[i];
                    ++ci;
                    p[tc].value[ci] = '\0';
                }
            }
            else
                return 0;
            continue;
        }
        if(state == 4)
        {
            if(isalnum(input[i]) || input[i] == '.')
            {
                state = 4;
                if(ci < MAX_VALUE_LENGTH)
                {
                    p[tc].value[ci] = input[i];
                    ++ci;
                    p[tc].value[ci] = '\0';
                }
            }
            else if(input[i] == ';')
            {
                state = 1;
                if(tc + 1 >= MAX_VALUE_NAME_PAIRS)
                    return tc;
                ++tc;
                ci = 0;
            }
            else
                return 0;
            continue;
        }
    }
    if(state != 1)
        return 0;
    return tc;
}

char* get_value_for_name(char *name,struct ValueNamePairs *p)
{
    int i;
    for(i = 0 ; i < MAX_VALUE_NAME_PAIRS ; ++i)
        if(!strcmp(p[i].name,name))
            return p[i].value;
    return "";
}

char* get_minimal_json_string(char *input,int i)
{
    return "";
}

void comm(char *input,char *output,int output_max_size)
{
    int tc = is_valid_input(input,pairs);
    if(tc == 0)
    {
        snprintf(output,output_max_size,"Error-1");
        return;
    }

    char *cmd = get_value_for_name("cmd",pairs);
    if(strlen(cmd) == 0)
    {
        snprintf(output,output_max_size,"Error-2");
        return;
    }

    if(!strcmp(cmd,"qtt"))
    {
        snprintf(output,output_max_size,"{\"working\": \"%s\",\"target_temp\":%.1f,\"reference_temp\":%.1f,\"heating_state\":\"%s\"}",
                     get_thermostat_working() ? "on" : "off",
                     get_target_temp(),
                     get_reference_temp(),
                     get_heating_state() ? "on" : "off");
        return;
    }

    if(!strcmp(cmd,"stt"))
    {
        char *ttempstr = get_value_for_name("ttemp",pairs);
        if(strlen(ttempstr) == 0)
        {
            snprintf(output,output_max_size,"Error\n");
            return;
        }
        float ttf;
        if(sscanf(ttempstr,"%f",&ttf) == 1)
        {
            set_target_temp(ttf);
            snprintf(output,output_max_size,"{\"set\":\"ok\"}");
            return;
        }
        snprintf(output,output_max_size,"Error-4");
        return;
    }

    if(!strcmp(cmd,"sll"))
    {
        char *loglevelstr = get_value_for_name("loglevel",pairs);
        if(strlen(loglevelstr) == 0)
        {
            snprintf(output,output_max_size,"Error\n");
            return;
        }
        int ll;
        if(sscanf(loglevelstr,"%d",&ll) == 1)
        {
            set_loglevel(ll);
            snprintf(output,output_max_size,"{\"set\":\"ok\"}");
            return;
        }
        snprintf(output,output_max_size,"Error-4");
        return;
    }

    if(!strcmp(cmd,"stw"))
    {
        char *ttempstr = get_value_for_name("work",pairs);
        if(strlen(ttempstr) == 0)
        {
            snprintf(output,output_max_size,"Error\n");
            return;
        }
        if(!strcmp(ttempstr,"on"))
        {
            set_thermostat_on(1);
            snprintf(output,output_max_size,"{\"set\":\"ok\"}");
            return;
        }

        if(!strcmp(ttempstr,"off"))
        {
            set_thermostat_on(0);
            snprintf(output,output_max_size,"{\"set\":\"ok\"}");
            return;
        }
        snprintf(output,output_max_size,"Error-4");
        return;
    }

    if(!strcmp(cmd,"qas"))
    {
        snprintf(output,output_max_size,"{\"sensors\": [");
        int ms = max_sensor_count();
        int i,ol;
        int oc = 0;
        for( i = 0 ; i < ms ; ++i)
        {
            if(is_sensor_active(i))
            {
                const char *name = get_sensor_name(i);
                struct TempHum th = get_sensor_value(i);
                ol = strlen(output);
                snprintf(output+ol,output_max_size-ol,"%s{\"name\": \"%s\",\"temp\": %.1f,\"hum\": %.0f}",oc == 0 ? "" : ",",name,th.temp,th.hum);
                ++oc;
            }
        }
        ol = strlen(output);
        snprintf(output+ol,output_max_size-ol,"]}");
        return;
    }

    if(!strcmp(cmd,"qct"))
    {
        snprintf(output,output_max_size,"{\"version\":\"%s\", \"compiledate\":\"%s\", \"compiletime\":\"%s\"}",
                 VERSION,__DATE__,__TIME__);
        return;
    }

    if(!strcmp(cmd,"rct"))
    {
        reset_sensor_counters();
        snprintf(output,output_max_size,"{\"reset\":\"ok\"}");
        return;
    }

    if(!strcmp(cmd,"qhis") && tc == 4)
    {
        char *sensorname = get_value_for_name("sn",pairs);
        char *offsetstr = get_value_for_name("off",pairs);
        char *lengthstr = get_value_for_name("len",pairs);
        int offset = atoi(offsetstr);
        int length = atoi(lengthstr);
        get_history_data(output,output_max_size,sensorname,offset,length);
        return;
    }

    if(!strcmp(cmd,"qhshis") && tc == 1)
    {
        get_hsw_history_data(output,output_max_size);
        return;
    }

    if(!strcmp(cmd,"qstat") && tc == 2)
    {
        char *sensorname = get_value_for_name("sn",pairs);
        get_sensor_statistics(output,output_max_size,get_index_by_name(sensorname));
        return;
    }

    snprintf(output,output_max_size,"Error-0");
    return;
}

