gcc `pkg-config --cflags gtk+-3.0 libcjson` -o main main.c config.c mqtt.c `pkg-config --libs gtk+-3.0 libcjson` -g -lpaho-mqtt3c
