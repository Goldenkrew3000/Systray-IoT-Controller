/*
// IoT Controller
// MQTT Handler
// Goldenkrew3000 2025
// GPLv3
*/

#include "mqtt.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <MQTTClient.h>

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

MQTTClient client;
static int rc = 0;

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

    // Split string so only the root exists
    char* token = NULL;
    char* topic_cpy = topic;
    token = strsep(&topic_cpy, "/");
    if (token == NULL) {
        printf("ERROR: Something went wrong with strsep.\n");
        goto processMessage_cleanup;
    }

    // Check if the message is from a defined device
    for (int i = 0; i < deviceCount; i++) {
        if (strcmp(token, configPtr_devices[i].name) == 0) {
            printf("Device %s has sent a status update.\n", configPtr_devices[i].prettyName);
        }
    }

processMessage_cleanup:
    free(topic);
    free(content);
}
