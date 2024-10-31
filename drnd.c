/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
* DHT22 read codes according to https://github.com/Qengineering/DHT22-Raspberry-Pi
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "drnd.h"
#include "smtherm.h"

struct ReadValues rnd_sensor_read(struct RndSensorDevice* sd);

int rnd_sensor_init(struct RndSensorDevice* sd)
{
    sd->last_temp      = 20.0 + (rand()%100 - 50)/10;
    sd->last_hum       = 50.0 + (rand()%100 - 50)/10;
    sd->last_read_time = 0;
    sd->okread         = 0;
    return 1;
}

struct ReadValues rnd_sensor_read(struct RndSensorDevice* sd)
{
    struct ReadValues rv;

    sd->last_temp = sd->last_temp + (rand()%60 - 30)/10;
    sd->last_hum  = sd->last_hum + (rand()%60 - 30)/10;

    sd->last_read_time = time(NULL);
    sd->okread++;

    rv.temp = sd->last_temp;
    rv.hum  = sd->last_hum;
    rv.valid = 1;
    return rv;
}

//End code.
