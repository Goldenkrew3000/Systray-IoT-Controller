/*
// IoT Controller
// Entry Point
// Goldenkrew3000 2025
// GPLv3
*/


#include <stdio.h>
#include <stdlib.h>
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

    windowHandler_start();

    exit(EXIT_SUCCESS);
}
