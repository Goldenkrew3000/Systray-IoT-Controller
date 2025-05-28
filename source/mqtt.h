#ifndef _MQTT_H
#define _MQTT_H
#include <MQTTClient.h>

int mqttHandler_init();
void mqttHandler_deinit();
int message_arrived_callback(void* context, char* topicName, int topicLen, MQTTClient_message* message);
void connection_lost_callback(void* context, char* cause);
int mqttHandler_processMessage(char* topic, char* content);

#endif
