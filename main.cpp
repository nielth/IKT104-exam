#include "DevI2C.h"
#include "ThisThread.h"
// Mbed OS 6.10
#include "mbed.h"
#include "DFRobot_RGBLCD.h"
#include "PwmOut.h"
#include <HTS221Sensor.h>

#include "main.h"

#define CLOCK 0
#define ALARM 1
#define TEMP 2
#define CALENDAR 3
#define WEATHER 4

extern Data data; // Struct in header
struct tm *timeinfo;

DFRobot_RGBLCD lcd(16, 2, D14, D15);
// Left button
InterruptIn button1(D1, PullUp);
// middle button
InterruptIn button2(D2, PullUp);
// right button
InterruptIn button3(D4, PullUp);
PwmOut piezo(D5);
DevI2C i2c(PB_11, PB_10);
time_t seconds;
Timer debounce; 

Thread interface_thread;
Thread alarm_thread;

void disconnect_network();
void interface_();
void alarm();
void interface_toggle();
void hour_toggle();
void minute_toggle();

int main() {
while (true) {
    set_time(0);
    debounce.start();
    interface_thread.start(interface_);
    alarm_thread.start(alarm);

    button1.rise(&interface_toggle);
    button2.rise(&hour_toggle);
    button3.rise(&minute_toggle);
    piezo.period(0.001f);

    connect_network();
    get_clock();
    get_weather();
    disconnect_network();
        while (true) 
        {
            // Every hour, do an if statement-loop to get the current weather
            ThisThread::sleep_for(3600s);

            // Check each hour if it's day times saving and fetch new clock status from API
            if (timeinfo->tm_wday == 0 && timeinfo->tm_mon == 9 &&
                timeinfo->tm_hour == 3 && timeinfo->tm_mday >= 25 ||
                timeinfo->tm_wday == 0 && timeinfo->tm_mon == 2 &&
                timeinfo->tm_hour == 2 && timeinfo->tm_mday >= 25) {
                    break;
            }

            connect_network();
            get_weather();
            lcd.init();
            disconnect_network();
        }
    }
}

void interface_() {
    int i = 0;
    float temperature, humidity;
    DevI2C i2c(PB_11, PB_10);
    HTS221Sensor sensor(&i2c);
    sensor.init(nullptr);
    sensor.enable();
    const char *weekday[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                            "Thursday", "Friday", "Saturday"};

    while (true) {
        seconds = time(NULL);
        timeinfo = localtime(&seconds);
        time(&seconds);
        mktime(timeinfo);
        if (data.raw_clock != 0 && data.is_dst == 1) 
        timeinfo->tm_hour += 2;
        else if (data.raw_clock != 0 && data.is_dst == 0) 
        timeinfo->tm_hour++;

        if (data.raw_clock != 0 && timeinfo->tm_hour % 24 <= 2) {
            timeinfo->tm_mday += 1;
            if (timeinfo->tm_mday == 31) {
                timeinfo->tm_mon++;
                if (timeinfo->tm_mon == 11)
                timeinfo->tm_year++;
            }
        }
        if (data.raw_clock != 0 && i == 0) {
            set_time(data.raw_clock);
            i++;
        }
        if (data.interface == CLOCK) {
            lcd.setCursor(0, 0);
            lcd.printf("%02d:%02d:%02d ", timeinfo->tm_hour % 24, timeinfo->tm_min,
                        timeinfo->tm_sec);
            lcd.setCursor(0, 1);
            if (data.alarm_status == true)
                lcd.printf("Alarm is on        ");
            if (data.alarm_status == false)
                lcd.printf("Alarm is off       ");
        }

        else if (data.interface == ALARM) {
            lcd.setCursor(0, 0);
            lcd.printf("Alarm: %02d:%02d ", data.hour, data.minute);
        }

        else if (data.interface == TEMP) {
            sensor.get_temperature(&temperature);
            sensor.get_humidity(&humidity);
            lcd.setCursor(0, 0);
            lcd.printf("Temp: %.1f ", temperature);
            lcd.setCursor(0, 1);
            lcd.printf("Hum: %.1f ", humidity);
        }

        else if (data.interface == CALENDAR) {
            lcd.setCursor(0, 0);
            lcd.printf("%02d-%02d-%d", timeinfo->tm_mday, timeinfo->tm_mon + 1, 1900 + timeinfo->tm_year);
            lcd.setCursor(0, 1);
            lcd.printf("%s", weekday[timeinfo->tm_wday]);
        }

        else if (data.interface == WEATHER) {
            lcd.setCursor(0, 0);
            lcd.printf("Weather:");
            lcd.setCursor(0, 1);
            lcd.printf("%s", data.weather);
        }
        if (data.thread_status == false) {
            lcd.init();
            data.thread_status = true;
        }
    }
}

// Toggle between interfaces on LCD // STOP alarm (left button)
void interface_toggle() {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(debounce.elapsed_time()).count() > 200){    
        if (data.alarm_going_off == false){                                         // Change interface
            data.thread_status = false;
            data.interface++;
            if (data.interface == 5) 
                data.interface = 0;
        } else if (data.alarm_going_off == true && data.alarm_status == true) {     // Stops alarm functionality
            piezo.write(0.0f);           
            data.alarm_status = false;        
            data.alarm_going_off = false;
        }
        debounce.reset();  
    }
}

// Toggle hours on alarm // Snooze (middle button)
void hour_toggle() {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(debounce.elapsed_time()).count() > 200) {    
        if (data.alarm_going_off == false){
            if (data.interface == ALARM) data.hour++;
            if (data.interface == CLOCK) data.alarm_status = !data.alarm_status;
            if (data.hour == 24) data.hour = 0;
        } else {                                // Snoozes the active alarm
            piezo.write(0.0f);
            if (data.minute >= 55)
                data.hour++;
            data.minute += 5;
            data.minute = data.minute % 60;
            data.alarm_going_off = false;
        }
        debounce.reset();  
    }
}

// Toggle minutes on alarm (right button)
void minute_toggle() {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(debounce.elapsed_time()).count() > 200) {    
        if (data.interface == ALARM) data.minute++;
        if (data.minute == 60) data.minute = 0;
    }
    debounce.reset();  
}

// Toggle alarm function (the piezo)
void alarm() {
    while(true){
        if (seconds / 3600 != 0 && data.hour == timeinfo->tm_hour % 24 &&
            data.minute == timeinfo->tm_min && data.alarm_status == true &&
            data.interface != ALARM) {
            piezo.write(0.80f);
            data.alarm_going_off = true;
        }
    }
}

