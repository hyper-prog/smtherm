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
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wiringPi.h>
#include <curl/curl.h>

#include "tools.h"
#include "conf.h"
#include "therm.h"
#include "dht22.h"
#include "smtherm.h"
#include "comm.h"
#include "history.h"

struct SMTherm_Settings smt_settings;
struct SensorData smt_sensors[MAX_SENSOR_COUNT];

int server_socket = 0;

void zero_conf(void)
{
    smt_settings.loglevel = 0;
    smt_settings.nodaemon = 0;
    smt_settings.tcpport = 5017;
    smt_settings.sensor_poll_time = 60;
    smt_settings.sse_port = 0;
    h_strlcpy(smt_settings.sse_host,"127.0.0.1",128);
    h_strlcpy(smt_settings.pidfile,"",128);
    h_strlcpy(smt_settings.logfile,"/var/log/smtherm.log",128);
    h_strlcpy(smt_settings.statefile,"/var/run/smtherm.data",128);
    h_strlcpy(smt_settings.heater_switch_mode,"none",16);

    smt_settings.heater_switch_gpio = 0;
    h_strlcpy(smt_settings.heater_switch_mode,"none",16);
    h_strlcpy(smt_settings.heater_switch_shelly_address,"",64);
    smt_settings.heater_switch_shelly_indeviceid = 0;

    int i;
    for(i = 1 ; i < MAX_SENSOR_COUNT ; ++i)
    {
        smt_sensors[i].active = 0;
        smt_sensors[i].last_read_success = 0;
        h_strlcpy(smt_sensors[i].name,"-",128);
        smt_sensors[i].type = SENSOR_TYPE_UNKNOWN;
        smt_sensors[i].gpio_pin = -1;
        smt_sensors[i].rtime = 0;
        smt_sensors[i].temp = 0.0;
        smt_sensors[i].hum = 0.0;
        smt_sensors[i].lowlevel_data = NULL;
    }
}

int loglevel(void)
{
    return smt_settings.loglevel;
}

int nodaemon(void)
{
    return smt_settings.nodaemon;
}

char *logfile(void)
{
    return smt_settings.logfile;
}

int max_sensor_count(void)
{
    return MAX_SENSOR_COUNT;
}

int is_sensor_active(int index)
{
    if(index < 0 || index > MAX_SENSOR_COUNT)
        return 0;
    return (int)(smt_sensors[index].active);
}

int is_sensor_last_read_success(int index)
{
    if(index < 0 || index > MAX_SENSOR_COUNT)
        return 0;
    return (int)(smt_sensors[index].last_read_success);
}

const char * get_sensor_name(int index)
{
    return smt_sensors[index].name;
}

int get_index_by_name(const char *n)
{
    int i;
    for(i = 1 ; i < MAX_SENSOR_COUNT ; ++i)
        if(smt_sensors[i].active && !strcmp(smt_sensors[i].name,n))
            return i;
    return -1;
}

struct TempHum get_sensor_value(int index)
{
    struct TempHum th;
    smt_sensors[index].adv_rtime = time(NULL);
    smt_sensors[index].adv_temp = smt_sensors[index].temp;
    smt_sensors[index].adv_hum = smt_sensors[index].hum;
    th.temp = smt_sensors[index].temp;
    th.hum = smt_sensors[index].hum;
    return th;
}

int get_sensor_statistics(char *buffer,int buffer_length,int index)
{
    if(!is_sensor_active(index) || smt_sensors[index].lowlevel_data == NULL)
    {
        snprintf(buffer,buffer_length,"Error getting statistics of sensor: %s\n",smt_sensors[index].name);
        return 1;
    }
    struct SensorDevice *sd = (struct SensorDevice *)smt_sensors[index].lowlevel_data;
    time_t now = time(NULL);

    snprintf(buffer,buffer_length,"{\"name\": \"%s\",\"temp\": %.1f,\"lastok\": \"%s\",\"hum\": %.0f,\"lastread\": %lu,\"okread\": %lu,\"crcerror\": %lu,\"insense\": %lu}",
                smt_sensors[index].name,
                smt_sensors[index].temp,
                smt_sensors[index].last_read_success ? "yes" : "no",
                smt_sensors[index].hum,
                now - sd->last_read_time,
                sd->okread,
                sd->checksumerror,
                sd->falsedataerror);
    return 0;
}

void printversion(void)
{
    printf("%s\n"
           "Version: %s\n"
           "Compiled: %s\n"
           "Author: Peter Deak (hyper80@gmail.com)\n"
           "License: GPL\n",LONGNAME,VERSION, __DATE__);
}

void printhelp(void)
{
    printf("%s\n"
           "  Usage: smtherm <configfile> [-nodaemon]\n",LONGNAME);
}

void beforeExit(void)
{
    if(server_socket != 0)
    {
        int true = 1;
        setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int));
        close(server_socket);
    }

    free_thermostat(smt_settings.statefile);

    if(strlen(smt_settings.pidfile) > 0)
        if(unlink(smt_settings.pidfile) != 0)
            toLog(0,"Warning: Cannot delete pid file!\n");

    free_sensors();
    free_history_database();
}

void sigint_handler(int sig)
{
    beforeExit();
    toLog(0,"Received INT/TERM signal, Exiting...\n");
    exit(0);
}

void attach_signal_handler(void)
{
    struct sigaction sa;

    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        toLog(0,"Error, Cannot set signal handler: sigaction() failure. Exiting...\n");
        beforeExit();
        exit(1);
    }
    if(sigaction(SIGTERM, &sa, NULL) == -1)
    {
        toLog(0,"Error, Cannot set signal handler: sigaction() failure. Exiting...\n");
        beforeExit();
        exit(1);
    }
}

void init_sensor_devices(void)
{
    int i;
    for(i = 0 ; i < MAX_SENSOR_COUNT ; ++i)
    {
        if(strlen(smt_sensors[i].name) > 0 &&
           strcmp(smt_sensors[i].name,"-") != 0 &&
           smt_sensors[i].type != SENSOR_TYPE_UNKNOWN &&
           smt_sensors[i].gpio_pin > -1 )
        {
            smt_sensors[i].active = 0;
            if(smt_sensors[i].type == SENSOR_TYPE_DHT22)
            {
                smt_sensors[i].lowlevel_data = (void *)malloc(sizeof(struct SensorDevice));
                dht22_sensor_init((struct SensorDevice *)smt_sensors[i].lowlevel_data,smt_sensors[i].gpio_pin);
                smt_sensors[i].active = 1;
            }
        }
    }
}

void read_single_sensor(int index)
{
    if(smt_sensors[index].active)
    {
        if(smt_sensors[index].type == SENSOR_TYPE_DHT22 && smt_sensors[index].lowlevel_data != NULL)
        {
            struct ReadValues v = dht22_sensor_read((struct SensorDevice *)smt_sensors[index].lowlevel_data);
            if(v.valid)
            {
                thermostat_ext_lock();

                smt_sensors[index].last_read_success = 1;

                smt_sensors[index].last_rtime = smt_sensors[index].rtime;
                smt_sensors[index].last_temp = smt_sensors[index].temp;
                smt_sensors[index].last_hum = smt_sensors[index].hum;

                smt_sensors[index].rtime = time(NULL);
                smt_sensors[index].temp = v.temp;
                smt_sensors[index].hum = v.hum;

                thermostat_ext_unlock();
            }
            else
            {
                thermostat_ext_lock();
                smt_sensors[index].last_read_success = 0;
                thermostat_ext_unlock();
            }
        }
    }
}

void read_all_sensors(void)
{
    int i;
    for(i = 0 ; i < MAX_SENSOR_COUNT ; ++i)
        read_single_sensor(i);
}

int is_sensor_value_changed(void)
{
    int i;
    for(i = 0 ; i < MAX_SENSOR_COUNT ; ++i)
        if(smt_sensors[i].active && smt_sensors[i].last_read_success)
            if(smt_sensors[i].last_temp != smt_sensors[i].temp || smt_sensors[i].last_hum != smt_sensors[i].hum)
                return 1;
    return 0;
}

void print_valid_sensors(void)
{
    int i;
    for(i = 0 ; i < MAX_SENSOR_COUNT ; ++i)
    {
        if(smt_sensors[i].active)
        {
            printf("%s = %.1f C , %.0f %%\n",smt_sensors[i].name,smt_sensors[i].temp,smt_sensors[i].hum);
        }
    }
}

void * sensor_reader_thread_fnc(void *arg)
{
    toLog(1,"Sensor reader thread: init sensors...\n");
    init_sensors();
    init_sensor_devices();
    init_history_database();
    toLog(1,"Sensor reader thread: start readings...\n");
    while(1)
    {
        read_all_sensors();
        if(smt_settings.loglevel > 2)
        {
            printf("----------------------------\n");
            print_valid_sensors();
        }
        if(is_sensor_value_changed())
        {
            do_thermostat();
            send_sse_message("sensors=SensorDataChanged");
        }

        history_store_sensors();

        sleep(smt_settings.sensor_poll_time);
    }
    return NULL;
}

void * communication_thread_fnc(void *arg)
{
    toLog(1,"Communication thread: init...\n");
    struct sockaddr_in addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1)
    {
        toLog(0,"Error creating communication socket!\n");
        beforeExit();
        exit(1);
    }

    addr.sin_port = htons(smt_settings.tcpport);
    addr.sin_addr.s_addr = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;

    if(bind(server_socket, (struct sockaddr *)&addr,sizeof(struct sockaddr_in) ) == -1)
    {
         if( errno == EADDRINUSE )
         {
            // handle port already open case
            toLog(0,"Error: Can't bind tcp port %d: already in use!\n",smt_settings.tcpport);
            beforeExit();
            exit(1);
         }
         printf("Error: Can't bind tcp port!\n");
         beforeExit();
         exit(1);
    }

    toLog(1,"Successfully bound to port %d\n", smt_settings.tcpport);

    listen(server_socket,8);

    int client_socket;
    char in[256];
    char out[TCP_OUTPUT_BUFFER_SIZE];
    while(1)
    {
        memset(in, 0, 256);
        client_socket = accept(server_socket, NULL, NULL);
        recv(client_socket, in, 255, 0);
        comm(trim(in),out,TCP_OUTPUT_BUFFER_SIZE);
        send(client_socket, out, strlen(out), 0);
        close(client_socket);
    }
    return NULL;
}

int main(int argi,char **argc)
{
    if(argi <= 1)
    {
        printhelp();
        return 0;
    }

    zero_conf();
    init_thermostat();

    int p;
    for(p = 1 ; p < argi ; ++p)
    {
        if(!strcmp(argc[p],"-h") || !strcmp(argc[p],"-help"))
        {
            printhelp();
            return 0;
        }

        if(!strcmp(argc[p],"-v") || !strcmp(argc[p],"-V") || !strcmp(argc[p],"-version"))
        {
            printversion();
            return 0;
        }

        if(!strcmp(argc[p],"-nodaemon"))
        {
            smt_settings.nodaemon = 1;
            continue;
        }

        if(argc[p][0] != '-')
        {
            read_config_file(argc[p],&smt_settings,smt_sensors);
            continue;
        }
    }

    thermostat_read_saved_state(smt_settings.statefile);

    if(smt_settings.loglevel > 1)
    {
        printconfig(&smt_settings);
    }

    if(strlen(smt_settings.pidfile) > 0)
    {
        FILE *pidf = NULL;
        if((pidf = fopen(smt_settings.pidfile,"w")) != NULL)
        {
            fprintf(pidf,"%d",getpid());
            fclose(pidf);
        }
        else
            toLog(0,"Error, Cannot open pid file!\n");
    }

    attach_signal_handler();

    if(!smt_settings.nodaemon)
    {
        toLog(1,"Started, Entering daemon mode...\n");

        if(daemon(0,0) == -1)
        {
            toLog(0,"Error, daemon() failure, Exiting...\n");
            beforeExit();
            exit(1);
        }
        toLog(2,"Entered daemon mode.\n");
    }
    else
    {
        toLog(2,"\nWARNING: Daemon mode is disabled by -nodaemon switch!\n"
                "All messages written to the standard output!\n"
                "THE HASSES DOES NOT USE THE LOG FILE!\n\n");
    }
    toLog(0,"Start...\n");

    if(!strcmp(smt_settings.heater_switch_mode,"gpio"))
    {
        pinMode(smt_settings.heater_switch_gpio, OUTPUT);
        digitalWrite(smt_settings.heater_switch_gpio, LOW);
    }

    pthread_t sensor_reader_thread;
    pthread_t communication_thread;

    pthread_create(&sensor_reader_thread, NULL, sensor_reader_thread_fnc, NULL);
    sleep(2);
    pthread_create(&communication_thread, NULL, communication_thread_fnc, NULL);

    pthread_join(sensor_reader_thread, NULL);
    pthread_join(communication_thread, NULL);
    return(0);
}

int send_sse_message(const char *message)
{
    if(smt_settings.sse_port == 0)
        return 1;

    toLog(2,"SSE message...\n");
    struct sockaddr_in sseaddr;

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        toLog(1,"Error creating SSE socket!\n");
        return 1;
    }
    bzero(&sseaddr, sizeof(sseaddr));

    sseaddr.sin_family = AF_INET;
    sseaddr.sin_addr.s_addr = inet_addr(smt_settings.sse_host);
    sseaddr.sin_port = htons(smt_settings.sse_port);

    if(connect(sockfd, (struct sockaddr *)&sseaddr, sizeof(sseaddr)) != 0)
    {
         toLog(1,"Error: Can't connect to SSE server!\n");
         return 1;
    }

    toLog(2,"Successfully connected to sse, send: \"%s\"\n",message);

    send(sockfd, message, strlen(message), 0);
    close(sockfd);
    toLog(2,"Closed sse channel\n");
    return 0;
}

int switch_heater(int state)
{
    if(!strcmp(smt_settings.heater_switch_mode,"gpio"))
    {
        toLog(2,"Set gpio(%d) %s\n",smt_settings.heater_switch_gpio,state ? "HIGH" : "LOW");
        digitalWrite(smt_settings.heater_switch_gpio, state ? HIGH : LOW);
        return 1;
    }

    if(!strcmp(smt_settings.heater_switch_mode,"shelly"))
    {
        CURL *curl;
        CURLcode res;
        curl = curl_easy_init();
        if(curl)
        {
            char callstring[128];
            snprintf(callstring,128,"http://%s/rpc/Switch.Set?id=%d&on=%s",
                  smt_settings.heater_switch_shelly_address,
                  smt_settings.heater_switch_shelly_indeviceid,
                  state ? "true" : "false"
                );
            curl_easy_setopt(curl, CURLOPT_URL, callstring);
            toLog(2,"Call(Shelly) %s\n",callstring);
            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
                toLog(0, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
            curl_easy_cleanup(curl);
        }
        return 1;
    }
    return 0;

}

