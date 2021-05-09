#include "mbed.h"

// Header guard to protect us from declaring a variable two times
#ifndef SOME_HEADER_GUARD_WITH_UNIQUE_NAME
#define SOME_HEADER_GUARD_WITH_UNIQUE_NAME

// Shared variables across files goes here

struct Data {    
    // Print values
    int raw_clock = 0;
    bool is_dst = 0;
    int hour = 0;
    int minute = 0;
    float temperature;
    float humidity;
    char weather[50];

    // Inteface values
    int interface = 0;
    bool thread_status = false;
    bool alarm_status = false;
    bool alarm_going_off = false;
};

#endif

void disconnect_network();
void get_clock();
void get_weather();
void connect_network();
void connect_server(const char host_name[], const char api_message[], int REMOTE_PORT);
char* raw_json_function(int REMOTE_PORT);
char* json_parser(char *raw_json, char *key);