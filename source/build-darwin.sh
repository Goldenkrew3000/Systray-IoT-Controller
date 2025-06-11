# PahoMQTTC Does not have pkg-config support on Darwin through homebrew... what the fuck why not
PAHO_LIB=/opt/homebrew/Cellar/libpaho-mqtt/1.3.14/lib
PAHO_INCL=/opt/homebrew/Cellar/libpaho-mqtt/1.3.14/include

gcc `pkg-config --cflags libcjson` -I $PAHO_INCL -L $PAHO_LIB -o main main.c config.c mqtt.c `pkg-config --libs libcjson` -lpaho-mqtt3c -g
