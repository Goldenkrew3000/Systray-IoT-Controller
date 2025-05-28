# Systray-IoT-Controller
A simple GTK based app that controls IoT devices from a Unix Systray

# NOTICE
While this is a very simple application, it is not currently finished. <br>

## Why does this exist
I consider Home Assistant to be a very bloated service to run on a network, and I wanted a simple way to control IoT devices (Such as lights running OpenBK firmware) directly from the Systray of a Unix operating system. <br>

## What does it run on
This should run on any Unix/Unix-like operating system (Although I am only supporting GlibC/Musl Linux, and OpenBSD). <br>
I DO NOT know if this runs under a Windows environment, and WILL NOT (I am not a Windows user, and do not plan to support it in any way). <br>

## Dependencies
- CMake
- CJson
- Paho MQTT C
- GTK4
- (TODO Some libappindicator or something)

