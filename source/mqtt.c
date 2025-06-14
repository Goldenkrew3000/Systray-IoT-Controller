/*
// IoT Controller
// MQTT Handler
// Goldenkrew3000 2025
// GPLv3
*/

#include "mqtt.h"
#include "config.h"
#include "wait.h"
#include "states.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <MQTTClient.h>
#include <cjson/cJSON.h>

// MQTT Settings
extern char* configPtr_mqtt_broker;
extern char* configPtr_mqtt_port;
extern char* configPtr_mqtt_clientName;
extern char* configPtr_mqtt_username;
extern char* configPtr_mqtt_password;
char MQTT_Address[128];

// Devices
extern int deviceCount;
extern configPtr_device_t configPtr_devices[];

// Window signal objects
extern atomic_int dispatchFlag;
extern atomic_int dispatchType;
extern atomic_int dispatchAction;
extern atomic_int dispatchDevice;
extern _Atomic uint32_t dispatchContent;

#define ARRAY_MAX_DEPTH 3
MQTTClient client;
static int rc = 0;
static int json_rc = 0;

int mqttHandler_init() {
    printf("%s +\n", __func__);

    // Form MQTT broker address
    // TODO add some sort of buffer overflow handling
    snprintf(MQTT_Address, 128, "tcp://%s:%s", configPtr_mqtt_broker, configPtr_mqtt_port);

    // Create MQTT client
    if ((rc = MQTTClient_create(&client, MQTT_Address, configPtr_mqtt_clientName, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("ERROR: Could not create MQTT client (%s).\n", MQTTClient_strerror(rc));
        return 1;
    }

    // Set callbacks
    if ((rc = MQTTClient_setCallbacks(client, NULL, connection_lost_callback, message_arrived_callback, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("ERROR: Could not set MQTT callbacks (%s).\n", MQTTClient_strerror(rc));
        MQTTClient_destroy(&client);
        return 1;
    }

    // Connect to broker and perform login
    MQTTClient_connectOptions mqtt_options = MQTTClient_connectOptions_initializer;
    mqtt_options.keepAliveInterval = 20;
    mqtt_options.cleansession = 1;
    mqtt_options.username = configPtr_mqtt_username;
    mqtt_options.password = configPtr_mqtt_password;

    if ((rc = MQTTClient_connect(client, &mqtt_options)) != MQTTCLIENT_SUCCESS) {
        printf("ERROR: Could not connect to MQTT broker (%s).\n", MQTTClient_strerror(rc));
        MQTTClient_destroy(&client);
        return 1;
    }
    printf("Connected to MQTT broker.\n");

    // Subscribe to # (Root topic)
    if ((rc = MQTTClient_subscribe(client, "#", 1)) != MQTTCLIENT_SUCCESS) {
        printf("ERROR: Could not subscribe to MQTT broker\n");
        MQTTClient_disconnect(client, 10000);
        MQTTClient_destroy(&client);
        return 1;
    }

    return 0;
}

void mqttHandler_deinit() {
    printf("%s +\n", __func__);

    MQTTClient_unsubscribe(client, "#");
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

int message_arrived_callback(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    printf("Topic: %s -- Content: %.*s\n", topicName, message->payloadlen, (char*)message->payload);
    
    // Make a copy of the topic and content to send to the processMessage function
    char* topic_ptr = NULL;
    char* content_ptr = NULL;
    topic_ptr = strdup(topicName);
    content_ptr = strdup(message->payload);
    if (topic_ptr == NULL) {
        printf("ERROR: Could not malloc memory for topic copy.\n");
    }
    if (content_ptr == NULL) {
        printf("ERROR: Could not malloc memory for content copy.\n");
    }
    if (topic_ptr != NULL && content_ptr != NULL) {
        mqttHandler_processMessage(topic_ptr, content_ptr);
    } else {
        if (topic_ptr != NULL) { free(topic_ptr); }
        if (content_ptr != NULL) { free(content_ptr); }
    }
}

void connection_lost_callback(void* context, char* cause) {
    printf("WARNING: Connection to MQTT broker lost, cause: %s\n", cause);
}

int mqttHandler_processMessage(char* topic, char* content) {
    /* Info
    // deviceName/ --> Status Update
    // cmnd/ --> Command sent (from here or other device)
    */

    // TODO: Reform this, many memory leaks

    // Split string into array, sliced by '/'
    // TODO handle freeing etc
    char* token = NULL;
    char* array[ARRAY_MAX_DEPTH] = { NULL };
    int split_idx = 0;
    token = strtok(topic, "/");
    while (token != NULL) {
        if (split_idx == ARRAY_MAX_DEPTH) {
            printf("OVERFLOW WARNING!!\n"); // TODO
            break;
        }
        array[split_idx++] = token;
        token = strtok(NULL, "/");
    }


    // Check for general response
    for (int i = 0; i < deviceCount; i++) {
        if (strcmp(array[0], configPtr_devices[i].name) == 0) {
            printf("Device %s sent a general status update.\n", configPtr_devices[i].prettyName);
            if (strcmp(array[1], "connected") == 0) {
                // Device sent a connection status update
                if (strcmp(content, "online") == 0) {
                    // Device is online
                    configPtr_devices[i].online = 1;
                }
            }
        }
    }

    // Check for state/status response
    if (strcmp(array[0], "stat") == 0) {
        // Received state/status command response
        // Find which device sent the response
        for (int i = 0; i < deviceCount; i++) {
            if (strcmp(array[1], configPtr_devices[i].name) == 0) {
                // Find whether the response is from a state or status command
                if (strcmp(array[2], "RESULT") == 0) {
                    // Device sent a State response
                    printf("Device %s sent a State response.\n", configPtr_devices[i].prettyName);
                    mqttHandler_processStateResponse(content, i);
                } else if (strcmp(array[2], "STATUS") == 0) {
                    // Device sent a Status response
                    printf("Device %s sent a Status response.\n", configPtr_devices[i].prettyName);
                    mqttHandler_processStatusResponse(content);
                }
            }
        }
        
    }

processMessage_cleanup:
    free(topic);
    free(content);
}

// MQTT Command Dispatcher Thread
void* mqttHandler_commandDispatcher(void*) {
    while (1 == 1) {
        while (atomic_load(&dispatchFlag) == 0) {
            waitHandler_wait(&dispatchFlag);
        }

        printf("Performing action. Device %d, Type %d, Action %d, Content: 0x%.4x\n",
            atomic_load(&dispatchDevice), atomic_load(&dispatchType), atomic_load(&dispatchAction), atomic_load(&dispatchContent));

        if (atomic_load(&dispatchType) == FLAG_DISPATCH_TYPE_OPENBK_LIGHT) {
            // Dispatch request is for an OpenBK Light
            if (atomic_load(&dispatchAction) == FLAG_DISPATCH_ACTION_OPENBK_LIGHT_POWER_ON) {
                // Turn on light
                mqttHandler_sendOpenBKLightCommand(atomic_load(&dispatchDevice), "led_enableAll", 1, 1);
            } else if (atomic_load(&dispatchAction) == FLAG_DISPATCH_ACTION_OPENBK_LIGHT_POWER_OFF) {
                // Turn off light
                mqttHandler_sendOpenBKLightCommand(atomic_load(&dispatchDevice), "led_enableAll", 1, 0);
            } else if (atomic_load(&dispatchAction) == FLAG_DISPATCH_ACTION_OPENBK_LIGHT_BRIGHTNESS) {
                // Brightness change
                mqttHandler_sendOpenBKLightCommand(atomic_load(&dispatchDevice), "led_dimmer", 1, atomic_load(&dispatchContent));
            }
        }
        
        // Reset atomic contents/flag
        atomic_store(&dispatchType, 0);
        atomic_store(&dispatchAction, 0);
        atomic_store(&dispatchContent, 0x0);
        atomic_store(&dispatchDevice, -1);
        atomic_store(&dispatchFlag, 0);
    }
}

// Send an OpenBK Light Command over MQTT
int mqttHandler_sendOpenBKLightCommand(int device, char* cmnd, int useContent, uint32_t content) {
    // Form command and payload
    char* topic = NULL;
    char* payload = NULL;
    asprintf(&topic, "cmnd/%s/%s", configPtr_devices[device].name, cmnd);
    asprintf(&payload, "%d", content);
    printf("Topic: %s\n", topic);
    printf("Payload: %s\n", payload);

    // Send MQTT message
    MQTTClient_message obj = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    if (useContent == 1) {
        obj.payload = payload;
        obj.payloadlen = strlen(payload);
    }
	obj.qos = 0;
	obj.retained = 0;
    MQTTClient_publishMessage(client, topic, &obj, &token); // TODO handle response?

    // Free objects
    if (topic != NULL) { free(topic); }
    if (payload != NULL) { free(payload); }
}

int mqttHandler_processStateResponse(char* content, int device) {
    // Clean the deviceState struct
    mqttHandler_cleanState(device);

    // Parse JSON
    cJSON* jobj_state = cJSON_Parse(content);
    if (jobj_state == NULL) {
        const char* jerr_ptr = cJSON_GetErrorPtr();
        if (jerr_ptr != NULL) {
            printf("ERROR: Parsing JSON returned error: (%s)\n", jerr_ptr);
        } else {
            printf("ERROR: Parsing JSON returned unknown error.\n");
        }
        goto processStateResponse_cleanup_fail;
    }

    cJSON* jobj_uptime = cJSON_GetObjectItemCaseSensitive(jobj_state, "Uptime");
    cJSON* jobj_mqttCount = cJSON_GetObjectItemCaseSensitive(jobj_state, "MqttCount");
    cJSON* jobj_dimmer = cJSON_GetObjectItemCaseSensitive(jobj_state, "Dimmer");
    cJSON* jobj_color = cJSON_GetObjectItemCaseSensitive(jobj_state, "Color");
    cJSON* jobj_hsbcolor = cJSON_GetObjectItemCaseSensitive(jobj_state, "HSBColor");
    cJSON* jobj_channel = cJSON_GetObjectItemCaseSensitive(jobj_state, "Channel");
    cJSON* jobj_power = cJSON_GetObjectItemCaseSensitive(jobj_state, "POWER");
    cJSON* jobj_wifi_root = cJSON_GetObjectItemCaseSensitive(jobj_state, "Wifi");

    json_rc = configHandler_checkExists(jobj_uptime, "root", "Uptime");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_mqttCount, "root", "MqttCount");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_dimmer, "root", "Dimmer");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_color, "root", "Color");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_hsbcolor, "root", "HSBColor");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_channel, "root", "Channel");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_power, "root", "POWER");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_wifi_root, "root", "Wifi");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }

    cJSON* jobj_wifi_ssid = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "SSId");
    cJSON* jobj_wifi_bssid = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "BSSId");
    cJSON* jobj_wifi_channel = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "Channel");
    cJSON* jobj_wifi_mode = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "Mode");
    cJSON* jobj_wifi_rssi = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "RSSI");
    cJSON* jobj_wifi_signal = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "Signal");

    json_rc = configHandler_checkExists(jobj_wifi_ssid, "Wifi", "SSId");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_wifi_bssid, "Wifi", "BSSId");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_wifi_channel, "Wifi", "Channel");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_wifi_mode, "Wifi", "Mode");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_wifi_rssi, "Wifi", "RSSI");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    json_rc = configHandler_checkExists(jobj_wifi_signal, "Wifi", "Signal");
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }

    configPtr_devices[device].deviceState.uptime = strdup(jobj_uptime->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.uptime);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    configPtr_devices[device].deviceState.color = strdup(jobj_color->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.color);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    configPtr_devices[device].deviceState.hsbcolor = strdup(jobj_hsbcolor->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.hsbcolor);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    configPtr_devices[device].deviceState.power = strdup(jobj_power->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.power);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    configPtr_devices[device].deviceState.wifi_ssid = strdup(jobj_wifi_ssid->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.wifi_ssid);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    configPtr_devices[device].deviceState.wifi_bssid = strdup(jobj_wifi_bssid->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.wifi_bssid);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }
    configPtr_devices[device].deviceState.wifi_mode = strdup(jobj_wifi_mode->valuestring);
    json_rc = configHandler_callocSuccess(configPtr_devices[device].deviceState.wifi_mode);
    if (json_rc == 1) { goto processStateResponse_cleanup_fail; }

    configPtr_devices[device].deviceState.mqttCount = jobj_mqttCount->valueint;
    configPtr_devices[device].deviceState.dimmer = jobj_dimmer->valueint;
    configPtr_devices[device].deviceState.wifi_channel = jobj_wifi_channel->valueint;
    configPtr_devices[device].deviceState.wifi_rssi = jobj_wifi_rssi->valueint;
    configPtr_devices[device].deviceState.wifi_signal = jobj_wifi_signal->valueint;

    // Update states
    configPtr_devices[device].brightness = configPtr_devices[device].deviceState.dimmer;
    //configPtr_devices[device].color[0] = configPtr_devices[device].deviceState.dimmer;

    goto processStateResponse_cleanup_success;

processStateResponse_cleanup_fail:
    cJSON_Delete(jobj_state);
    mqttHandler_cleanState(device);
    return 1;
processStateResponse_cleanup_success:
    cJSON_Delete(jobj_state);
    return 0;
}

void mqttHandler_cleanState(int device) {
    printf("%s +\n", __func__);
    if (configPtr_devices[device].deviceState.uptime != NULL) { free(configPtr_devices[device].deviceState.uptime); }
    if (configPtr_devices[device].deviceState.color != NULL) { free(configPtr_devices[device].deviceState.color); }
    if (configPtr_devices[device].deviceState.hsbcolor != NULL) { free(configPtr_devices[device].deviceState.hsbcolor); }
    if (configPtr_devices[device].deviceState.power != NULL) { free(configPtr_devices[device].deviceState.power); }
    if (configPtr_devices[device].deviceState.wifi_ssid != NULL) { free(configPtr_devices[device].deviceState.wifi_ssid); }
    if (configPtr_devices[device].deviceState.wifi_bssid != NULL) { free(configPtr_devices[device].deviceState.wifi_bssid); }
    if (configPtr_devices[device].deviceState.wifi_mode != NULL) { free(configPtr_devices[device].deviceState.wifi_mode); }
    configPtr_devices[device].deviceState.mqttCount = 0;
    configPtr_devices[device].deviceState.dimmer = 0;
    configPtr_devices[device].deviceState.wifi_channel = 0;
    configPtr_devices[device].deviceState.wifi_rssi = 0;
    configPtr_devices[device].deviceState.wifi_signal = 0;
}

int mqttHandler_processStatusResponse(char* content) {
    printf("\n\n\nStatus Content: %s\n\n\n", content);
}

// This thread's only purpose is to run once on startup, sending an MQTT request for the state of each device configured in the config
void* mqttHandler_initDeviceInfo(void*) {
    printf("%s +\n", __func__);

    for (int i = 0; i < deviceCount; i++) {
        // Create topic
        char* topic = NULL;
        asprintf(&topic, "cmnd/%s/state", configPtr_devices[i].name);

        // Send MQTT message
        MQTTClient_message obj = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;
        obj.qos = 0;
        obj.retained = 0;
        MQTTClient_publishMessage(client, topic, &obj, &token); // TODO handle response?

        // Cleanup
        if (topic != NULL) { free(topic); } else { 
            printf("Non-critical ERROR: Something went wrong in mqttHandler_initDeviceInfo, could not allocate memory.\n");
        }
    }
}
