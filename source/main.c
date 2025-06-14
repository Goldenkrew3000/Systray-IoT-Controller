/*
// IoT Controller
// Entry Point
// Goldenkrew3000 2025
// GPLv3
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "window.hpp"
#include "mqtt.h"
#include "config.h"

static int rc = 0;

int main() {
    printf("IoT Controller\n");
    printf("Goldenkrew3000 2025\n");

    // Read config file
    rc = configHandler_read();
    if (rc != 0) {
        printf("MAIN: Ran into critical error, exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Connect to the MQTT broker
    rc = mqttHandler_init();
    if (rc != 0) {
        printf("MAIN: Ran into a critical error, exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Start a thread to initially request device information about all the devices specified in the config
    // NOTE: At this point, no modification is taking place on the variables, so no mutex's needed
    pthread_t thr_device_info;
    pthread_create(&thr_device_info, NULL, mqttHandler_initDeviceInfo, NULL);

    // Start the MQTT command dispatcher thread
    pthread_t thr_mqtt_cmd_dispatcher;
    pthread_create(&thr_mqtt_cmd_dispatcher, NULL, mqttHandler_commandDispatcher, NULL);

    // Start the window (Has to be on the main thread)
    windowHandler_init();

    exit(EXIT_SUCCESS);
}
