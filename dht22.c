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

int  dht22m_device_count = 0;
char dht22m_gpiolist_string[64];
int  dht22m_gpiolist_string_pan;

pthread_mutex_t sensor_lock;

void init_sensors(void)
{
    if(pthread_mutex_init(&sensor_lock, NULL) != 0)
    {
        toLog(0,"Sensor mutex init has failed!\n");
    }

    dht22m_device_count = 0;
    h_strlcpy(dht22m_gpiolist_string,"",64);
    dht22m_gpiolist_string_pan = 0;
}

void init_wiringPi(void)
{
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

int init_configure_dht22m(void)
{
    toLog(3,"DHT22M config, send \"%s\" to \"%sd\" file...\n",dht22m_gpiolist_string,DHT22M_GPIO_CONF_FILE);
    FILE *sysfile;
    if((sysfile = fopen(DHT22M_GPIO_CONF_FILE,"w")) == NULL)
    {
        toLog(1,"Cannot write \"%sd\", is dht22m modul loaded?\n",DHT22M_GPIO_CONF_FILE);
        return 1;
    }
    fprintf(sysfile,"%s",dht22m_gpiolist_string);
    fclose(sysfile);
    delay(2000);
    return 0;
}

void free_sensors(void)
{
    pthread_mutex_destroy(&sensor_lock);
}

static int durn(struct timespec t1, struct timespec t2)
{
    return (((t2.tv_sec-t1.tv_sec)*1000000) + ((t2.tv_nsec-t1.tv_nsec)/1000)); // elapsed microsecs
}

int dht22_sensor_init(struct Dht22SensorDevice* sd,int rpi_gpio_pin,int reader_code)
{
    int i;

    sd->reader_code = reader_code;
    if(reader_code == SENSOR_READERCODE_DEFAULT)
        sd->reader_code = SENSOR_READERCODE_DHT22_INTERNAL;

    sd->rpi_gpio_pin = rpi_gpio_pin;
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
    sd->ar_delay = 2200;

    if(sd->reader_code == SENSOR_READERCODE_DHT22_INTERNAL)
    {
        pinMode(sd->wpi_pin,INPUT);
        h_strlcpy(sd->devicefile,"",16);
    }

    if(sd->reader_code == SENSOR_READERCODE_DHT22_MKERNEL)
    {
        snprintf(sd->devicefile,16,"/dev/dht22m%d",dht22m_device_count);
        if(dht22m_device_count > 0)
        {
            dht22m_gpiolist_string_pan +=
                    snprintf(dht22m_gpiolist_string + dht22m_gpiolist_string_pan,
                             64 - dht22m_gpiolist_string_pan,
                             " ");
        }
        dht22m_gpiolist_string_pan +=
                snprintf(dht22m_gpiolist_string + dht22m_gpiolist_string_pan,
                         64 - dht22m_gpiolist_string_pan,
                         "%d",sd->rpi_gpio_pin);
        ++dht22m_device_count;
    }

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

void dht22_sensor_set_autoretry_maxtry(struct Dht22SensorDevice* sd,int max_try)
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

struct ReadValues dht22m_sensor_single_read(struct Dht22SensorDevice* sd)
{
    char buffer[64];
    char *ptr;
    struct ReadValues rv;

    rv.valid = 0;
    rv.temp  = 0.0;
    rv.hum   = 0.0;

    FILE *devf;
    if((devf = fopen(sd->devicefile,"r")) == NULL)
    {
        toLog(0,"Cannot open %s file!\n",sd->devicefile);
        return rv;
    }
    ptr = fgets(buffer,sizeof(buffer),devf);
    fclose(devf);

    if(ptr == NULL)
    {
        toLog(0,"Cannot read any data from %s file!\n",sd->devicefile);
        return rv;
    }

    trim(buffer);
    toLog(4,"Read \"%s\" from \"%s\"\n",buffer,sd->devicefile);

    /* Possible answers:
         Ok;TEMPERATURE;HUMIDITY
         ChecksumError, ReadTooSoon, NotRead, IOError
     Since I can't collect the types of these errors I only distinct the success
     and failed reads.    */
    if(buffer[0] == 'O' && buffer[1] == 'k' && buffer[2] == ';')
    {
        if(sscanf(buffer + 3,"%f;%f",&rv.temp,&rv.hum) == 2)
        {
            toLog(4,"Success read %.1f C and %.1f%%\n",rv.temp,rv.hum);
            rv.valid = 1;
            return rv;
        }
    }
    return rv;
}

struct ReadValues dht22_sensor_single_read(struct Dht22SensorDevice* sd)
{
    struct ReadValues rv;

    pthread_mutex_lock(&sensor_lock);
    if(sd->reader_code == SENSOR_READERCODE_DHT22_INTERNAL)
    {
        rv = dht22_sensor_single_read_in(sd);
    }
    if(sd->reader_code == SENSOR_READERCODE_DHT22_MKERNEL)
    {
        rv = dht22m_sensor_single_read(sd);
    }
    pthread_mutex_unlock(&sensor_lock);
    return rv;
}

void dht22_sensor_reset_counters(struct Dht22SensorDevice* sd)
{
    pthread_mutex_lock(&sensor_lock);
    sd->c2okread         = 0;
    sd->c2checksumerror  = 0;
    sd->c2falsedataerror = 0;
    pthread_mutex_unlock(&sensor_lock);
}

//End code.
