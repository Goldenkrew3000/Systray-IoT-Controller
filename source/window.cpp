/*
// IoT Controller
// Window Handler
// Goldenkrew3000 2025
// GPLv3
*/

#include <stdio.h>
#include <stdatomic.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "window.hpp"
extern "C" {
#include "mqtt.h"
#include "wait.h"
#include "states.h"
#include "config.h"
}

// Window objects
GLFWwindow* window = nullptr;
int display_width = 0;
int display_height = 0;
ImVec4 black_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
int deviceList_selectedItem = -1;
float halfChildHeight = 0;

// Devices
extern int deviceCount;
extern configPtr_device_t configPtr_devices[];

// Signal objects
atomic_int dispatchFlag = 0; // Main flag to activate the MQTT dispatch function
atomic_int dispatchType = 0; // Type of device
atomic_int dispatchAction = 0; // Action to perform
atomic_int dispatchDevice = -1; // Device index
_Atomic uint32_t dispatchContent = 0x0; // Payload to send, if any

static void windowHandler_glfw_error_callback(int error, const char* desc) {
    printf("GLFW Error: %d: %s\n", error, desc);
}

int windowHandler_init() {
    // NOTE: As far as I know, GLFW should use the error callback when an error hits, so I am not providing error messages to the console manually
    glfwSetErrorCallback(windowHandler_glfw_error_callback);

    if (!glfwInit()) {
        return 1;
    }

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create a window with graphical context
    window = glfwCreateWindow(640, 480, "IoT Controller", nullptr, nullptr);
    if (window == nullptr) {
        return 1;
    }
    glfwMakeContextCurrent(window);

    // Enable VSync
    glfwSwapInterval(1);

    // Disable window resizing
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Enable Keyboard and Gamepad controls
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup color mode
    ImGui::StyleColorsDark();

    // Setup Platform/Render backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // NOTE: LOAD FONTS HERE

    // Start the window loop
    // NOTE: Cannot put this into a separate thread due to the polling
    windowHandler_loop();

    return 0;
}

void windowHandler_loop() {
    // Get the viewport size
    // TODO move somewhere better
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Make window flags
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Set the size of the window to the size of the viewport (fullscreen)
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        // Actually draw the window
        {
            ImGui::Begin("IoT Controller", nullptr, windowFlags);
            windowHandler_drawDeviceList();
            ImGui::SameLine();

            ImGui::BeginChild("rightSide", ImVec2(0, 0), true);

            ImVec2 left_avail = ImGui::GetContentRegionAvail();
            halfChildHeight = left_avail.x * 0.473f; // HACK: Make 2 children aligned vertically
            
            //ImGui::BeginChild("UpSide", ImVec2(0, half_width), true);
            //ImGui::EndChild();
            //ImGui::BeginChild("DownSide", ImVec2(0, half_width), true);
            //ImGui::EndChild();

            windowHandler_handleDeviceControl();


            ImGui::EndChild();
            
            
            ImGui::End();
        }

        // Render frame
        ImGui::Render();
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(black_color.x * black_color.w, black_color.y * black_color.w, black_color.z * black_color.w, black_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return;
}

void windowHandler_drawDeviceList() {
    ImGui::BeginChild("devicePanel", ImVec2(150, 0), true);

    // Title
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "Devices");
    ImGui::Separator();

    // Add devices from the config file to the list
    for (int i = 0; i < deviceCount; i++) {
        // Set color to red or green depending whether device has been seen online
        if (configPtr_devices[i].online == 1) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        }

        if (ImGui::Selectable(configPtr_devices[i].prettyName, deviceList_selectedItem == i)) {
            deviceList_selectedItem = i;
            printf("INTERFACE: (Device list) Item %d selected.\n", deviceList_selectedItem);
        }

        // Reset color
        ImGui::PopStyleColor();

        if (deviceList_selectedItem) {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::EndChild();
}

void windowHandler_handleDeviceControl() {
    if (strcmp(configPtr_devices[deviceList_selectedItem].type, "light") == 0) {
        if (configPtr_devices[deviceList_selectedItem].online == 1) {
            windowHandler_drawLightDeviceControl();
            windowHandler_drawLightDeviceInfo();
        } else {
            windowHandler_drawDeviceOffline();
        }
    } else if (strcmp(configPtr_devices[deviceList_selectedItem].type, "powermon") == 0) {
        // NOTE Not implemented
        printf("NOTE: Not implemented.\n");
    } else {
        // No device selected
        windowHandler_drawSelectDevice();
    }
}

//int brightness = 0;
int brightnessOld = 0;
//int warmth = 0;
int warmthOld = 0;
//float lightColor[3] = { 0.0f, 0.0f, 0.0f };
void windowHandler_drawLightDeviceControl() {
    ImGui::BeginChild("deviceControl", ImVec2(0, halfChildHeight), true);

    // Title
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "OpenBK Light Device");
    ImGui::Separator();

    if (ImGui::Button("Turn light on")) {
        atomic_store(&dispatchType, FLAG_DISPATCH_TYPE_OPENBK_LIGHT);
        atomic_store(&dispatchAction, FLAG_DISPATCH_ACTION_OPENBK_LIGHT_POWER_ON);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&dispatchFlag);
    }

    ImGui::SameLine();

    if (ImGui::Button("Turn light off")) {
        atomic_store(&dispatchType, FLAG_DISPATCH_TYPE_OPENBK_LIGHT);
        atomic_store(&dispatchAction, FLAG_DISPATCH_ACTION_OPENBK_LIGHT_POWER_OFF);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&dispatchFlag);
    }

    if (ImGui::Button("Set to white mode")) {
        //
    }

    ImGui::SameLine();

    if (ImGui::Button("Set to color mode")) {
        //
    }

    ImGui::SliderInt("Brightness", &configPtr_devices[deviceList_selectedItem].brightness, 0, 100);
    if (configPtr_devices[deviceList_selectedItem].brightness != brightnessOld) {
        brightnessOld = configPtr_devices[deviceList_selectedItem].brightness;
        atomic_store(&dispatchType, FLAG_DISPATCH_TYPE_OPENBK_LIGHT);
        atomic_store(&dispatchAction, FLAG_DISPATCH_ACTION_OPENBK_LIGHT_BRIGHTNESS);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchContent, configPtr_devices[deviceList_selectedItem].brightness);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&dispatchFlag);
    }

    ImGui::SliderInt("Warmth", &configPtr_devices[deviceList_selectedItem].warmth, 0, 100);
    if (configPtr_devices[deviceList_selectedItem].warmth != warmthOld) {
        warmthOld = configPtr_devices[deviceList_selectedItem].warmth;
        atomic_store(&dispatchType, FLAG_DISPATCH_TYPE_OPENBK_LIGHT);
        atomic_store(&dispatchAction, FLAG_DISPATCH_ACTION_OPENBK_LIGHT_WARMTH);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchContent, configPtr_devices[deviceList_selectedItem].warmth);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&dispatchFlag);
    }

    ImGui::PushItemWidth(100);
    ImGui::ColorPicker3("ColorPicker", (float*)&configPtr_devices[deviceList_selectedItem].color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
    ImGui::PopItemWidth();
    
    ImGui::EndChild();
}

void windowHandler_drawSelectDevice() {
    ImGui::BeginChild("deviceControl", ImVec2(0, 0), true);

    // Title
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "No device selected");
    ImGui::Separator();

    ImGui::Text("Please select a device");

    ImGui::EndChild();
}

void windowHandler_drawDeviceOffline() {
    ImGui::BeginChild("deviceControl", ImVec2(0, 0), true);

    // Title
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "Device is offline");
    ImGui::Separator();

    ImGui::Text("The selected device is offline");

    ImGui::EndChild();
}

void windowHandler_drawLightDeviceInfo() {
    ImGui::BeginChild("deviceInfo", ImVec2(0, halfChildHeight), true);

    ImGui::Text("Uptime: %s", configPtr_devices[deviceList_selectedItem].deviceState.uptime);
    ImGui::Text("MQTT Messages: %d\n", configPtr_devices[deviceList_selectedItem].deviceState.mqttCount);
    if (SHOW_MODE == 1) {
        ImGui::Text("Wifi Information: %s (Channel: %d, Mode: %s)",
        configPtr_devices[deviceList_selectedItem].deviceState.wifi_ssid,
        configPtr_devices[deviceList_selectedItem].deviceState.wifi_channel,
        configPtr_devices[deviceList_selectedItem].deviceState.wifi_mode);
        ImGui::Text("Wifi BSSID: %s", configPtr_devices[deviceList_selectedItem].deviceState.wifi_bssid);
    } else {
        ImGui::Text("Wifi Information:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Redacted");
        ImGui::SameLine();
        ImGui::Text("(Channel: %d, Mode: %s)",
            configPtr_devices[deviceList_selectedItem].deviceState.wifi_channel,
            configPtr_devices[deviceList_selectedItem].deviceState.wifi_mode);
        ImGui::Text("Wifi BSSID:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Redacted");
    }
    
    ImGui::Text("Wifi Signal: %d%% (%d / %d)\n",
        100 * (configPtr_devices[deviceList_selectedItem].deviceState.wifi_signal - RSSI_MIN) / (RSSI_MAX - RSSI_MIN),
        configPtr_devices[deviceList_selectedItem].deviceState.wifi_signal,
        configPtr_devices[deviceList_selectedItem].deviceState.wifi_rssi);

    ImGui::Text("Color: %s -- HSB Color: %s -- Dimmer: %d", configPtr_devices[deviceList_selectedItem].deviceState.color, configPtr_devices[deviceList_selectedItem].deviceState.hsbcolor, configPtr_devices[deviceList_selectedItem].deviceState.dimmer);

    ImGui::EndChild();
}
