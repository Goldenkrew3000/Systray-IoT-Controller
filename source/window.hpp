#ifndef _WINDOW_H
#define _WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif
int windowHandler_init();
#ifdef __cplusplus
}
#endif
void windowHandler_loop();
void windowHandler_drawDeviceList();
void windowHandler_handleDeviceControl();
void windowHandler_drawLightDeviceControl();
void windowHandler_drawSelectDevice();

#endif
