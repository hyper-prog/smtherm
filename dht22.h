/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
* DHT22 read codes according to https://github.com/Qengineering/DHT22-Raspberry-Pi
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/
#ifndef TDHT22_H
#define TDHT22_H

#define SENSOR_SENSE_MAX_DIFFTIME 180
#define SENSOR_SENSE_MAX_DIFFTEMP 5.0

struct ReadValues;

struct Dht22SensorDevice
{
    int fahrenheit;
    int wpi_pin;
    int max_try;
    int ar_delay;
    unsigned char hwdata[8];

    float last_lltemp;
    unsigned long int last_read_time;
    unsigned long int okread;
    unsigned long int checksumerror;
    unsigned long int falsedataerror;
};

void              init_sensors(void);
void              free_sensors(void);

int               dht22_sensor_init(struct Dht22SensorDevice* sd,int rpi_gpio_pin);
struct ReadValues dht22_sensor_read(struct Dht22SensorDevice* sd);

void              dht22_sensor_set_autoretry(struct Dht22SensorDevice* sd,int max_try);
void              dht22_sensor_set_autoretry_delay(struct Dht22SensorDevice* sd,int ar_delay);
void              dht22_sensor_set_fahrenheit(struct Dht22SensorDevice* sd);
struct ReadValues dht22_sensor_single_read(struct Dht22SensorDevice* sd);

#endif // TDHT22_H
