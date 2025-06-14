/*
// IoT Controller
// Config Handler
// Goldenkrew3000 2025
// GPLv3
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <cjson/cJSON.h>

#define MAX_DEVICES 8

const char* configFilename = "config.json";
char* configPtr = NULL;
static int rc = 0;

// Config pointers to be used globally
char* configPtr_mqtt_broker = NULL;
char* configPtr_mqtt_port = NULL;
char* configPtr_mqtt_clientName = NULL;
char* configPtr_mqtt_username = NULL;
char* configPtr_mqtt_password = NULL;
int deviceCount = 0;
configPtr_device_t configPtr_devices[MAX_DEVICES];

int configHandler_read() {
    printf("%s +\n", __func__);

    // Initialize pointers in devices struct array to null (Otherwise the free() step will SIGSEGV)
    for (int i = 0; i < MAX_DEVICES; i++) {
        configPtr_devices[i].mode = NULL;
        configPtr_devices[i].prettyName = NULL;
        configPtr_devices[i].name = NULL;
        configPtr_devices[i].type = NULL;
    }

    // Open config file
    FILE* fp_configFile = fopen(configFilename, "r");
    if (fp_configFile == NULL) {
        printf("ERROR: Could not open config file.\n");
        return 1;
    }

    // Determine size of config file and malloc
    fseek(fp_configFile, 0, SEEK_END);
    long fs_configFile = ftell(fp_configFile);
    fseek(fp_configFile, 0, SEEK_SET);
    configPtr = (char*)calloc(fs_configFile + 1, sizeof(char));
    if (configPtr == NULL) {
        printf("ERROR: Could not allocate %ld bytes on heap.\n", fs_configFile + 1);
        return 1;
    }

    // Read in config file
    fread(configPtr, 1, fs_configFile + 1, fp_configFile);
    fclose(fp_configFile);

    // Parse JSON
    cJSON* jobj_configFile = cJSON_Parse(configPtr);
    if (jobj_configFile == NULL) {
        const char* jerr_ptr = cJSON_GetErrorPtr();
        if (jerr_ptr != NULL) {
            printf("ERROR: Parsing JSON returned error: (%s).\n", jerr_ptr);
        } else {
            printf("ERROR: Parsing JSON returned unknown error.\n");
        }
        cJSON_Delete(jobj_configFile);
        free(configPtr);
        return 1;
    }

    // Fetch MQTT config
    cJSON* jobj_mqtt_root = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "mqtt");
    if (jobj_mqtt_root == NULL) {
        printf("ERROR: 'mqtt' field does not exist in JSON.\n");
        cJSON_Delete(jobj_configFile);
        free(configPtr);
        return 1;
    }

    cJSON* jobj_mqtt_broker = cJSON_GetObjectItemCaseSensitive(jobj_mqtt_root, "broker");
    cJSON* jobj_mqtt_port = cJSON_GetObjectItemCaseSensitive(jobj_mqtt_root, "port");
    cJSON* jobj_mqtt_clientName = cJSON_GetObjectItemCaseSensitive(jobj_mqtt_root, "clientName");
    cJSON* jobj_mqtt_username = cJSON_GetObjectItemCaseSensitive(jobj_mqtt_root, "username");
    cJSON* jobj_mqtt_password = cJSON_GetObjectItemCaseSensitive(jobj_mqtt_root, "password");

    rc = configHandler_checkExists(jobj_mqtt_broker, "mqtt", "broker");
    if (rc == 1) { goto configHandler_cleanup_fail; }
    rc = configHandler_checkExists(jobj_mqtt_port, "mqtt", "port");
    if (rc == 1) { goto configHandler_cleanup_fail; }
    rc = configHandler_checkExists(jobj_mqtt_clientName, "mqtt", "clientName");
    if (rc == 1) { goto configHandler_cleanup_fail; }
    rc = configHandler_checkExists(jobj_mqtt_username, "mqtt", "username");
    if (rc == 1) { goto configHandler_cleanup_fail; }
    rc = configHandler_checkExists(jobj_mqtt_password, "mqtt", "password");
    if (rc == 1) { goto configHandler_cleanup_fail; }

    // TODO Probably change this to strdup since it would simplify this a fair bit
    configPtr_mqtt_broker = (char*)calloc(strlen(jobj_mqtt_broker->valuestring) + 1, sizeof(char));
    rc = configHandler_callocSuccess(configPtr_mqtt_broker);
    if (rc == 1) { goto configHandler_cleanup_fail; }
    configPtr_mqtt_port = (char*)calloc(strlen(jobj_mqtt_port->valuestring) + 1, sizeof(char));
    rc = configHandler_callocSuccess(configPtr_mqtt_port);
    if (rc == 1) { goto configHandler_cleanup_fail; }
    configPtr_mqtt_clientName = (char*)calloc(strlen(jobj_mqtt_clientName->valuestring) + 1, sizeof(char));
    rc = configHandler_callocSuccess(configPtr_mqtt_clientName);
    if (rc == 1) { goto configHandler_cleanup_fail; }
    configPtr_mqtt_username = (char*)calloc(strlen(jobj_mqtt_username->valuestring) + 1, sizeof(char));
    rc = configHandler_callocSuccess(configPtr_mqtt_username);
    if (rc == 1) { goto configHandler_cleanup_fail; }
    configPtr_mqtt_password = (char*)calloc(strlen(jobj_mqtt_password->valuestring) + 1, sizeof(char));
    rc = configHandler_callocSuccess(configPtr_mqtt_password);
    if (rc == 1) { goto configHandler_cleanup_fail; }

    strcpy(configPtr_mqtt_broker, jobj_mqtt_broker->valuestring);
    strcpy(configPtr_mqtt_port, jobj_mqtt_port->valuestring);
    strcpy(configPtr_mqtt_clientName, jobj_mqtt_clientName->valuestring);
    strcpy(configPtr_mqtt_username, jobj_mqtt_username->valuestring);
    strcpy(configPtr_mqtt_password, jobj_mqtt_password->valuestring);

    // Fetch amount of devices
    cJSON* jobj_devices_root = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "devices");
    if (jobj_devices_root == NULL) {
        printf("ERROR: 'devices' array does not exist in JSON.\n");
        goto configHandler_cleanup_fail;
    }

    deviceCount = cJSON_GetArraySize(jobj_devices_root);
    if (deviceCount > MAX_DEVICES) {
        printf("ERROR: Device count is over the maximum allowed (%d).\n", MAX_DEVICES);
        goto configHandler_cleanup_fail;
    }

    // Fill device struct with device information
    for (int i = 0; i < deviceCount; i++) {
        cJSON* jobj_devices_device = cJSON_GetArrayItem(jobj_devices_root, i);
        if (jobj_devices_device == NULL) {
            printf("ERROR: Could not fetch index %d of 'devices' array in JSON.\n", i);
            goto configHandler_cleanup_fail;
        }

        cJSON* jobj_devices_device_mode = cJSON_GetObjectItemCaseSensitive(jobj_devices_device, "mode");
        cJSON* jobj_devices_device_prettyName = cJSON_GetObjectItemCaseSensitive(jobj_devices_device, "prettyName");
        cJSON* jobj_devices_device_name = cJSON_GetObjectItemCaseSensitive(jobj_devices_device, "name");
        cJSON* jobj_devices_device_type = cJSON_GetObjectItemCaseSensitive(jobj_devices_device, "type");

        rc = configHandler_checkExists(jobj_devices_device_mode, "mode", "devices");
        if (rc == 1) { goto configHandler_cleanup_fail; }
        rc = configHandler_checkExists(jobj_devices_device_prettyName, "prettyName", "devices");
        if (rc == 1) { goto configHandler_cleanup_fail; }
        rc = configHandler_checkExists(jobj_devices_device_name, "name", "devices");
        if (rc == 1) { goto configHandler_cleanup_fail; }
        rc = configHandler_checkExists(jobj_devices_device_type, "type", "devices");
        if (rc == 1) { goto configHandler_cleanup_fail; }

        configPtr_devices[i].mode = strdup(jobj_devices_device_mode->valuestring);
        rc = configHandler_callocSuccess(configPtr_devices[i].mode);
        if (rc == 1) { goto configHandler_cleanup_fail; }
        configPtr_devices[i].prettyName = strdup(jobj_devices_device_prettyName->valuestring);
        rc = configHandler_callocSuccess(configPtr_devices[i].prettyName);
        if (rc == 1) { goto configHandler_cleanup_fail; }
        configPtr_devices[i].name = strdup(jobj_devices_device_name->valuestring);
        rc = configHandler_callocSuccess(configPtr_devices[i].name);
        if (rc == 1) { goto configHandler_cleanup_fail; }
        configPtr_devices[i].type = strdup(jobj_devices_device_type->valuestring);
        rc = configHandler_callocSuccess(configPtr_devices[i].type);
        if (rc == 1) { goto configHandler_cleanup_fail; }
    }

    // Initialize the device state variables
    for (int i = 0; i < deviceCount; i++) {
        configPtr_devices[i].online = 0;
        configPtr_devices[i].deviceState.uptime = NULL;
        configPtr_devices[i].deviceState.color = NULL;
        configPtr_devices[i].deviceState.hsbcolor = NULL;
        configPtr_devices[i].deviceState.power = NULL;
        configPtr_devices[i].deviceState.wifi_ssid = NULL;
        configPtr_devices[i].deviceState.wifi_bssid = NULL;
        configPtr_devices[i].deviceState.wifi_mode = NULL;
        configPtr_devices[i].deviceState.mqttCount = 0;
        configPtr_devices[i].deviceState.dimmer = 0;
        configPtr_devices[i].deviceState.wifi_channel = 0;
        configPtr_devices[i].deviceState.wifi_rssi = 0;
        configPtr_devices[i].deviceState.wifi_signal = 0;
    }
    
    // Free objects
    goto configHandler_cleanup_success;
configHandler_cleanup_fail:
    cJSON_Delete(jobj_configFile);
    free(configPtr);
    configHandler_freeConfigObjects();
    return 1;
configHandler_cleanup_success:
    cJSON_Delete(jobj_configFile);
    free(configPtr);
    return 0;
}

int configHandler_checkExists(cJSON* obj, const char* root, const char* name) {
    if (obj == NULL) {
        printf("ERROR: '%s' field does not contain item '%s' in JSON.\n", root, name);
        return 1;
    }
    return 0;
}

int configHandler_callocSuccess(char* callocPtr) {
    if (callocPtr == NULL) {
        printf("ERROR: Could not allocate memory on the heap.\n");
        return 1;
    }
    return 0;
}

void configHandler_freeConfigObjects() {
    printf("%s +\n", __func__);

    // Free MQTT objects
    if (configPtr_mqtt_broker != NULL) { free(configPtr_mqtt_broker); }
    if (configPtr_mqtt_port != NULL) { free(configPtr_mqtt_port); }
    if (configPtr_mqtt_clientName != NULL) { free(configPtr_mqtt_clientName); }
    if (configPtr_mqtt_username != NULL) { free(configPtr_mqtt_username); }
    if (configPtr_mqtt_password != NULL) { free(configPtr_mqtt_password); }

    // Free device struct objects
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (configPtr_devices[i].mode != NULL) { free(configPtr_devices[i].mode); }
        if (configPtr_devices[i].prettyName != NULL) { free(configPtr_devices[i].prettyName); }
        if (configPtr_devices[i].name != NULL) { free(configPtr_devices[i].name); }
        if (configPtr_devices[i].type != NULL) { free(configPtr_devices[i].type); }
    }
}
