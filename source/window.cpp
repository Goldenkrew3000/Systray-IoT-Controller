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
#include "config.h"
extern "C" {
#include "mqtt.h"
#include "wait.h"
}

// Window objects
GLFWwindow* window = nullptr;
int display_width = 0;
int display_height = 0;
ImVec4 black_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
int deviceList_selectedItem = -1;

// Devices
extern int deviceCount;
extern configPtr_device_t configPtr_devices[];

// Signal objects
atomic_int buttonCmd = 0;
atomic_int flag = 0;

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
    window = glfwCreateWindow(800, 600, "IoT Controller", nullptr, nullptr);
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
            windowHandler_handleDeviceControl();
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
        if (ImGui::Selectable(configPtr_devices[i].prettyName, deviceList_selectedItem == i)) {
            deviceList_selectedItem = i;
            printf("INTERFACE: (Device list) Item %d selected.\n", deviceList_selectedItem);
        }

        if (deviceList_selectedItem) {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::EndChild();
}

void windowHandler_handleDeviceControl() {
    if (strcmp(configPtr_devices[deviceList_selectedItem].type, "light") == 0) {
        //
        windowHandler_drawLightDeviceControl();
    } else if (strcmp(configPtr_devices[deviceList_selectedItem].type, "powermon") == 0) {
        // NOTE Not implemented
        printf("NOTE: Not implemented.\n");
    } else {
        // No device selected
        windowHandler_drawSelectDevice();
    }
}

int brightness = 0;
int brightnessOld = 0;
void windowHandler_drawLightDeviceControl() {
    ImGui::BeginChild("deviceControl", ImVec2(0, 0), true);

    // Title
    ImGui::TextColored(ImVec4(1, 0, 1, 1), "Light Device");
    ImGui::Separator();

    if (ImGui::Button("Turn lignt on")) {
        atomic_store(&dispatchAction, 1);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&flag);
    }

    if (ImGui::Button("Turn light off")) {
        atomic_store(&dispatchAction, 2);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&flag);
    }

    ImGui::SliderInt("int", &brightness, 0, 100);
    if (brightness != brightnessOld) {
        brightnessOld = brightness;
        printf("Brightness slider changed: %d\n", brightness);

        atomic_store(&dispatchType, 1);
        atomic_store(&dispatchAction, 3);
        atomic_store(&dispatchContent, brightness);
        atomic_store(&dispatchDevice, deviceList_selectedItem);
        atomic_store(&dispatchFlag, 1);
        waitHandler_wake(&flag);
    }
    


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
