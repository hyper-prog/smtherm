/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
* DHT22 read codes according to https://github.com/Qengineering/DHT22-Raspberry-Pi
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/
#ifndef DRND_H
#define DRND_H

struct ReadValues;

struct RndSensorDevice
{
    float last_temp;
    float last_hum;
    unsigned long int last_read_time;
    unsigned long int okread;
};


int               rnd_sensor_init(struct RndSensorDevice* sd);
struct ReadValues rnd_sensor_read(struct RndSensorDevice* sd);

#endif // DRND_H
