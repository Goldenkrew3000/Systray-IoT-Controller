# IoT Controller Darwin XNU CMake
# NOTE: This is separated from the Linux version because macOS is NOT working correctly with CMake for me

cmake_minimum_required(VERSION 4.0)
project(iot_controller)

# Include directories for Paho MQTT C, cJSON, and imgui
set(EXTRA_INCLUDES
    /opt/homebrew/Cellar/cjson/1.7.18/include
    /opt/homebrew/Cellar/cjson/1.7.18/include/cjson
    /opt/homebrew/Cellar/libpaho-mqtt/1.3.14/include
    /opt/homebrew/Cellar/glfw/3.4/include
    ${PROJECT_SOURCE_DIR}/imgui
    ${PROJECT_SOURCE_DIR}/imgui/backends
    /opt/homebrew/include
)

# Linker directories for the libraries specified above
set(EXTRA_LIB_DIRS
    /opt/homebrew/Cellar/cjson/1.7.18/lib
    /opt/homebrew/Cellar/libpaho-mqtt/1.3.14/lib
    /opt/homebrew/Cellar/glfw/3.4/lib
    /opt/homebrew/lib
)

# Source files
set(SOURCE_FILES    "main.c"
                    "config.c"
                    "mqtt.c"
                    "window.cpp"
                    "wait.c"

                    "imgui/imgui.cpp"
                    "imgui/imgui_draw.cpp"
                    "imgui/imgui_tables.cpp"
                    "imgui/imgui_widgets.cpp"
                    "imgui/backends/imgui_impl_glfw.cpp"
                    "imgui/backends/imgui_impl_opengl3.cpp"
)

add_executable(iot_controller ${SOURCE_FILES})

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    # Force enable ASan on macOS
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    
    # Needed for macOS specifically
    add_link_options(-fno-lto)
    
    message(STATUS "AddressSanitizer forced enabled for macOS")
endif()

target_include_directories(iot_controller PRIVATE
    ${CJSON_INCLUDE}
    ${EXTRA_INCLUDES}
)

target_link_directories(iot_controller PRIVATE
    /opt/homebrew/Cellar/cjson/1.7.18/lib
    /opt/homebrew/Cellar/libpaho-mqtt/1.3.14/lib
    /opt/homebrew/Cellar/glfw/3.4/lib
    /opt/homebrew/lib
)

find_package(OpenGL REQUIRED)

target_link_libraries(iot_controller PRIVATE OpenGL::GL -lglfw -lcjson -lpaho-mqtt3c)
