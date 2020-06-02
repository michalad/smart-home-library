#ifndef MQTT_GATEWAY_H
#define MQTT_GATEWAY_H

#include <Arduino.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <LinkedList.h>
#include <Bounce2.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <Datatypes.h>

class MqttGateway
{

private:
    InternetProps internetProps;
    MqttProps mqttProps;
    LinkedList<MqttItem> *mqttItems;
    LinkedList<DebouncerPin> *debouncers;
    LinkedList<DS18B20Temp> *tempSensors;
    EthernetClient *ethernetClient;
    PubSubClient *mqttClient;
    unsigned long lastRetry;
    unsigned long lastTemperatureRead;

    void initRelay(int executivePin);
    void setupButton(int buttonPin);
    void handleRelay(int relyNum, byte *payload, unsigned int length);
    String getCmdTopic(String topicBase);
    String getStateTopic(String topicBase);
    void connectToMqtt();
    const char *toCharArray(String string);
    boolean canReconnect();
    void buttonsLoop();
    void sendOnOff(String topicBase, int currentState);
    void sendTemperature(String topicBase, float temperature);
    void ds18b20Loop();
    const char *toPayload(const bool currentState);
    MqttItem findAssignedItem(const int buttonPin);
    boolean canReadTemperature();

public:
    MqttGateway(InternetProps internetProps,
                MqttProps mqttProps,
                EthernetClient *ethernetClient,
                PubSubClient *mqttClient,
                LinkedList<MqttItem> *mqttItems);
    void setup();
    void callback(char *topic, byte *payload, unsigned int length);
    void loop();
};

#endif