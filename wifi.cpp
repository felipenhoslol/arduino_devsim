#include <WiFiS3.h>
#include <ArduinoMqttClient.h>

char ssid[] = "ECE207N04SN01 7439";
char pass[] = "07jZ-095";

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "192.168.1.100";
int portaMQTT = 1883;