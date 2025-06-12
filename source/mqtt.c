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
extern atomic_int buttonCmd;
extern atomic_int flag;

extern atomic_int dispatchFlag;
extern atomic_int dispatchType;
extern atomic_int dispatchAction;
extern atomic_int dispatchDevice;
extern _Atomic uint32_t dispatchContent;

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

void* mqttHandler_commandDispatcher(void*) {
    while (1 == 1) {
        while (atomic_load(&dispatchFlag) == 0) {
            waitHandler_wait(&dispatchFlag);
        }

        // Perform requested action
        //printf("Button: %d\n", atomic_load(&buttonCmd));
        //mqttHandler_sendCommand();
        //atomic_store(&buttonCmd, 0);
        
        printf("Performing action. Device %d, Type %d, Action %d, Content: 0x%.4x\n",
            atomic_load(&dispatchDevice), atomic_load(&dispatchType), atomic_load(&dispatchAction), atomic_load(&dispatchContent));

        if (atomic_load(&dispatchType) == FLAG_DISPATCH_TYPE_OPENBK_LIGHT) {
            // Dispatch request is for an OpenBK Light
            if (atomic_load(&dispatchAction) == FLAG_DISPATCH_ACTION_OPENBK_LIGHT_POWER_ON) {
                // Turn on light
            } else if (atomic_load(&dispatchAction) == FLAG_DISPATCH_ACTION_OPENBK_LIGHT_POWER_OFF) {
                // Turn off light
            } else if (atomic_load(&dispatchAction) == FLAG_DISPATCH_ACTION_OPENBK_LIGHT_BRIGHTNESS) {
                // Brightness change
                printf("here");
                char* topic = NULL;
                char* payload = NULL;
                asprintf(topic, "cmnd/%s/%s", configPtr_devices[device].name, cmnd);
                asprintf(payload, "%d", content);
                //mqttHandler_sendOpenBKLightCommand(atomic_load(&dispatchDevice), "led_dimmer", atomic_load(&dispatchContent));
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

int mqttHandler_sendOpenBKLightCommand(int device, char* cmnd, uint32_t content) {
    // TODO Add a heap of protection against double frees etc
    // Form command and payload
    char* topic = NULL;
    char* payload = NULL;
    asprintf(topic, "cmnd/%s/%s", configPtr_devices[device].name, cmnd);
    asprintf(payload, "%d", content);
    printf("Topic: %s\n", topic);
    printf("Payload: %s\n", payload);

    // Send MQTT message
    /*MQTTClient_message obj = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;
    obj.payload = payload;
	obj.payloadlen = strlen(payload);
	obj.qos = 0;
	obj.retained = 0;
    MQTTClient_publishMessage(client, topic, &obj, &token);
    */

    // Free objects
    free(topic);
    free(payload);

    //char* payload = "0";
    /*
	char* payload = NULL;
    asprintf(&payload, "%d", atomic_load(&buttonCmd)); // Allocates automatically
    MQTTClient_message plobj = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;
	plobj.payload = payload;
	plobj.payloadlen = strlen(payload);
	plobj.qos = 0;
	plobj.retained = 0;
	MQTTClient_publishMessage(client, "cmnd/windowsideLight/led_enableAll", &plobj, &token);
    free(payload);
    */
}
