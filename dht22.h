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

#define DHT22M_GPIO_CONF_FILE "/sys/class/dht22m/gpiolist"

struct ReadValues;

struct Dht22SensorDevice
{
    int reader_code;
    int fahrenheit;
    int rpi_gpio_pin;
    int wpi_pin;
    int max_try;
    int ar_delay;
    unsigned char hwdata[8];
    char devicefile[16];

    float last_lltemp;
    unsigned long int last_read_time;
    unsigned long int okread;
    unsigned long int c2okread;
    unsigned long int checksumerror;
    unsigned long int c2checksumerror;
    unsigned long int falsedataerror;
    unsigned long int c2falsedataerror;
};

void              init_sensors(void);
void              init_wiringPi(void);
int               init_configure_dht22m(void);
void              free_sensors(void);

int               dht22_sensor_init(struct Dht22SensorDevice* sd,int rpi_gpio_pin,int reader_code);
struct ReadValues dht22_sensor_read(struct Dht22SensorDevice* sd);

void              dht22_sensor_set_autoretry_maxtry(struct Dht22SensorDevice* sd,int max_try);
void              dht22_sensor_set_autoretry_delay(struct Dht22SensorDevice* sd,int ar_delay);
void              dht22_sensor_set_fahrenheit(struct Dht22SensorDevice* sd);
struct ReadValues dht22_sensor_single_read(struct Dht22SensorDevice* sd);
void              dht22_sensor_reset_counters(struct Dht22SensorDevice* sd);

#endif // TDHT22_H
