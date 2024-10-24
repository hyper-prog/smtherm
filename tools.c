/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

#include "tools.h"
#include "smtherm.h"

char *ltrim(char *s)
{
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s)
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

char *trim(char *s)
{
    return rtrim(ltrim(s));
}

size_t h_strlcpy(char *dest, const char *src, size_t size)
{
    size_t ret = strlen(src);

    if (size) {
        size_t len = (ret >= size) ? size - 1 : ret;
        memcpy(dest, src, len);
        dest[len] = '\0';
    }
    return ret;
}

pthread_mutex_t loglock;
time_t log_oldrawtime=1;
char   log_timebuf[80];

void toLog(int level, const char * format, ...)
{
    if(level <= loglevel())
    {
        if(nodaemon())
        {
            va_list args;
            va_start(args,format);
            vfprintf(stdout,format,args);
            va_end(args);
            fflush(stdout);
        }
        else
        {
            FILE *logf;

            pthread_mutex_lock(&loglock);
            logf = fopen(logfile(),"a");
            if(logf == NULL)
            {
                fprintf(stderr,"Error opening log file: %s\n",logfile());
                exit(1);
            }

            time_t rawtime;
            time(&rawtime);
            if(log_oldrawtime != rawtime)
            {
                struct tm timeinfo;
                localtime_r(&rawtime,&timeinfo);
                strftime(log_timebuf,80,"%F %T smtherm: ",&timeinfo);
                log_oldrawtime = rawtime;
            }

            va_list args;
            va_start(args,format);
            fputs(log_timebuf,logf);
            vfprintf(logf,format,args);
            va_end(args);
            fclose(logf);
            pthread_mutex_unlock(&loglock);
        }
    }
}
