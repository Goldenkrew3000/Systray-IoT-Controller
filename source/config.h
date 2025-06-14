#ifndef _CONFIG_H
#define _CONFIG_H
#include <cjson/cJSON.h>

typedef struct {
    char* uptime;
    int mqttCount;
    int dimmer;
    char* color;
    char* hsbcolor;
    char* power;
    char* wifi_ssid;
    char* wifi_bssid;
    int wifi_channel;
    char* wifi_mode;
    int wifi_rssi;
    int wifi_signal;
} mqttHandler_state_t;

typedef struct {
    char* mode; // TODO
    char* prettyName;
    char* name;
    char* type; // TODO
    int online;
    mqttHandler_state_t deviceState;
} configPtr_device_t;

int configHandler_read();
int configHandler_checkExists(cJSON* obj, const char* root, const char* name);
int configHandler_callocSuccess(char* callocPtr);
void configHandler_freeConfigObjects();

#endif
