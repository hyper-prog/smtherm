/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#ifndef SMTHERM_TOOLS_HEADER
#define SMTHERM_TOOLS_HEADER

#include <stdio.h>
#include <time.h>

char *ltrim(char *s);
char *rtrim(char *s);
char *trim(char *s);

size_t h_strlcpy(char *dest, const char *src, size_t size);

void init_log_mutex(void);
void toLog(int level, const char * format, ...);

#endif
