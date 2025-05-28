#ifndef _CONFIG_H
#define _CONFIG_H
#include <cjson/cJSON.h>

typedef struct {
    char* mode;
    char* prettyName;
    char* name;
    char* type;
} configPtr_device_t;

int configHandler_read();
int configHandler_checkExists(cJSON* obj, const char* root, const char* name);
int configHandler_callocSuccess(char* callocPtr);
void configHandler_freeConfigObjects();

#endif
