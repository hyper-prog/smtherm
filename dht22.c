/*************************************************************************************
* SMTherm - Smart Manageable Thermostat daemon
* DHT22 read codes according to https://github.com/Qengineering/DHT22-Raspberry-Pi
*
*  Peter Deak (C) hyper80@gmail.com , GPL v2
**************************************************************************************/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <wiringPi.h>

#include "dht22.h"
#include "smtherm.h"
#include "tools.h"

#define MAX_TIMINGS 85                  // Takes 84 state changes to transmit data

int rpigpio_to_wiringpi_pin[][2] = {
    {2,8},
    {3,9},
    {4,7},
    {17,0},
    {27,2},
    {22,3},
    {10,12},
    {9,13},
    {11,14},
    {0,30},
    {5,21},
    {6,22},
    {13,23},
    {19,24},
    {26,25},
    {14,15},
    {15,16},
    {18,1},
    {23,4},
    {24,5},
    {25,6},
    {8,10},
    {7,11},
    {1,31},
    {12,26},
    {16,27},
    {20,28},
    {21,29},
    {-1,-1}
};

pthread_spinlock_t sensor_lock;

void init_sensors(void)
{
    if(pthread_spin_init(&sensor_lock, PTHREAD_PROCESS_PRIVATE) != 0)
    {
        fprintf(stderr,"Spinlock init has failed!\n");
    }

    int init_try = 0;
    int init_ok = ( wiringPiSetup() >= 0 );
    while(!init_ok)
    {
        delay(1000);

        init_ok = ( wiringPiSetup() >= 0 );

        ++init_try;
        if(init_try >= 5)
            break;
    }
}

void free_sensors(void)
{
    pthread_spin_destroy(&sensor_lock);
}

static int durn(struct timespec t1, struct timespec t2)
{
    return (((t2.tv_sec-t1.tv_sec)*1000000) + ((t2.tv_nsec-t1.tv_nsec)/1000)); // elapsed microsecs
}

int dht22_sensor_init(struct Dht22SensorDevice* sd,int rpi_gpio_pin)
{
    int i;

    sd->wpi_pin = -1;
    for(i = 0 ; ; ++i)
    {
        if(rpigpio_to_wiringpi_pin[i][0] == rpi_gpio_pin)
        {
            sd->wpi_pin = rpigpio_to_wiringpi_pin[i][1];
            break;
        }
        if(rpigpio_to_wiringpi_pin[i][0] == -1)
            return 0;
    }
    //printf("WiringPi pin %d from %d\n",sd->wpi_pin,rpi_gpio_pin);

    sd->fahrenheit = 0;
    sd->max_try = 12;
    sd->ar_delay = 1000;

    pinMode(sd->wpi_pin,INPUT);

    sd->last_lltemp      = 0.0;
    sd->last_read_time   = 0;
    sd->okread           = 0;
    sd->c2okread         = 0;
    sd->checksumerror    = 0;
    sd->c2checksumerror  = 0;
    sd->falsedataerror   = 0;
    sd->c2falsedataerror = 0;

    return 1;
}

void dht22_sensor_set_autoretry(struct Dht22SensorDevice* sd,int max_try)
{
    sd->max_try = max_try;
}

void dht22_sensor_set_autoretry_delay(struct Dht22SensorDevice* sd,int ar_delay)
{
    sd->ar_delay = ar_delay;
}

void dht22_sensor_set_fahrenheit(struct Dht22SensorDevice* sd)
{
    sd->fahrenheit = 1;
}

struct ReadValues dht22_sensor_read(struct Dht22SensorDevice* sd)
{
    int try;
    struct ReadValues rv;
    rv.valid = 0;
    for(try = 0 ; try < sd->max_try ; ++try)
    {
        delay(50);
        rv = dht22_sensor_single_read(sd);
        if(rv.valid)
        {
            time_t now = time(NULL);
            if(sd->last_read_time > 0) //If this is not the first read
            {
                if(now - sd->last_read_time < SENSOR_SENSE_MAX_DIFFTIME)
                {
                    if(fabs(rv.temp - sd->last_lltemp) < SENSOR_SENSE_MAX_DIFFTEMP || rv.hum > 100)
                    {
                        //Full ok values
                        //fprintf(stderr,"*** Full Ok read(pin:%d): temp: %.1f\n",sd->wpi_pin,rv.temp);
                        sd->okread++;
                        sd->c2okread++;
                        sd->last_lltemp    = rv.temp;
                        sd->last_read_time = now;
                        return rv;
                    }
                    else
                    {
                        //Too much temperature changes, go retry
                        //fprintf(stderr,"*** Too muck temparature changes, error(pin:%d): temp: %.1f lasttemp: %.1f\n",sd->wpi_pin,rv.temp,sd->last_lltemp);
                        rv.valid = 0;
                        sd->last_lltemp    = rv.temp;
                        sd->last_read_time = now;
                        sd->falsedataerror++;
                        sd->c2falsedataerror++;
                    }
                }
                else
                {
                    //Cannot apply sense check because too much time difference, go retry
                    //fprintf(stderr,"*** Too much time diff to check (pin:%d): temp: %.1f\n",sd->wpi_pin,rv.temp);
                    rv.valid = 0;
                    sd->okread++;
                    sd->c2okread++;
                    sd->last_lltemp    = rv.temp;
                    sd->last_read_time = now;
                }
            }
            else
            {
                //Cannot apply sense check because this is the first read, jump next read
                //fprintf(stderr,"*** First (OK) read(pin:%d): temp: %.1f\n",sd->wpi_pin,rv.temp);
                sd->okread++;
                sd->c2okread++;
                sd->last_lltemp    = rv.temp;
                sd->last_read_time = now;
            }
        }
        else
        {
            //fprintf(stderr,"*** Checksum error(pin:%d)\n",sd->wpi_pin);
            sd->checksumerror++;
            sd->c2checksumerror++;
        }

        delay(sd->ar_delay);
    }
    //fprintf(stderr,"*** Giving up(pin:%d)\n",sd->wpi_pin);
    return rv;
}

void dht22_startpuls(int wpi_pin)
{
    pinMode(wpi_pin, OUTPUT);
    digitalWrite(wpi_pin, HIGH);
    delayMicroseconds(5);
    digitalWrite(wpi_pin, LOW);
    delayMicroseconds(1100);
    digitalWrite(wpi_pin, HIGH);
    delayMicroseconds(10);
    pinMode(wpi_pin, INPUT);
    delayMicroseconds(20);
}

void dht22_end(int wpi_pin)
{
    pinMode(wpi_pin, OUTPUT);
    digitalWrite(wpi_pin, HIGH);
}

struct ReadValues dht22_sensor_single_read_in(struct Dht22SensorDevice* sd)
{
    struct ReadValues rv;

    rv.valid = 0;
    rv.temp  = 0.0;
    rv.hum   = 0.0;

    struct timespec st, cur;
    int toggles   = 0;
    int bit_cnt   = 0;
    int lrs       = HIGH;
    int lastState = HIGH;
    unsigned int waitForChange = 0;

    sd->hwdata[0] = sd->hwdata[1] = sd->hwdata[2] = sd->hwdata[3] = sd->hwdata[4] = 0;

    // Collect data from sensor
    dht22_startpuls(sd->wpi_pin);
    clock_gettime(CLOCK_REALTIME, &cur);
    for(toggles = 0 ; (toggles < MAX_TIMINGS) && (waitForChange < 250) ; ++toggles)
    {
        st = cur;
        for(waitForChange = 0; ((lrs = digitalRead(sd->wpi_pin)) == lastState) && (waitForChange < 255) ; ++waitForChange)
        {
            delayMicroseconds(1);
        }
        clock_gettime(CLOCK_REALTIME, &cur);
        lastState = lrs;

        if((toggles > 2) && (toggles % 2 == 0))
        {
            sd->hwdata[ bit_cnt / 8 ] <<= 1;
            if(durn(st,cur) > 48)
                sd->hwdata[ bit_cnt/8 ] |= 0x00000001;
            ++bit_cnt;
        }
    }
    dht22_end(sd->wpi_pin);

    // Interpret 40 bits. (Five elements of 8 bits each)  Last element is a checksum.
    if((bit_cnt >= 40) && (sd->hwdata[4] == ((sd->hwdata[0] + sd->hwdata[1] + sd->hwdata[2] + sd->hwdata[3]) & 0xFF)) )
    {
        rv.valid = 1;
        rv.hum   = (float)((sd->hwdata[0] << 8) + sd->hwdata[1]) / 10.0;
        rv.temp  = (float)(((sd->hwdata[2] & 0x7F) << 8) + sd->hwdata[3]) / 10.0;
        if(sd->hwdata[2] & 0x80) // Negative Sign Bit on.
        {
            rv.temp  *= -1.0;
        }
        if(sd->fahrenheit)
        {
            rv.temp *= 1.8;
            rv.temp += 32.0;
        }
    }
    else // Data Bad, return invalid
    {
        rv.valid= 0;
        rv.hum  = 0.0;
        rv.temp = 0.0;
    }

    if(rv.hum > 100)
        rv.valid = 0;
    return rv;
}

struct ReadValues dht22_sensor_single_read(struct Dht22SensorDevice* sd)
{
    struct ReadValues rv;
    pthread_spin_lock(&sensor_lock);
    rv = dht22_sensor_single_read_in(sd);
    pthread_spin_unlock(&sensor_lock);
    return rv;
}

void dht22_sensor_reset_counters(struct Dht22SensorDevice* sd)
{
    pthread_spin_lock(&sensor_lock);
    sd->c2okread         = 0;
    sd->c2checksumerror  = 0;
    sd->c2falsedataerror = 0;
    pthread_spin_unlock(&sensor_lock);
}

//End code.
