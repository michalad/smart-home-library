#include "MqttGateway.h"

#define RELAY_ON 0
#define RELAY_OFF 1

MqttGateway::MqttGateway(InternetProps internetProps,
                         MqttProps mqttProps,
                         EthernetClient *ethernetClient,
                         PubSubClient *mqttClient,
                         LinkedList<MqttItem> *mqttItems)
{
  Serial.begin(115200);
  this->internetProps = internetProps;
  this->mqttProps = mqttProps;
  this->mqttItems = mqttItems;
  this->ethernetClient = ethernetClient;
  this->mqttClient = mqttClient;
  this->lastRetry = millis();
  this->lastTemperatureRead = millis();
  this->debouncers = new LinkedList<DebouncerPin>();
  this->tempSensors = new LinkedList<DS18B20Temp>();
}

void MqttGateway::setup()
{
  for (int item = 0; item < mqttItems->size(); item++)
  {
    MqttItem currentItem = mqttItems->get(item);
    if (currentItem.deviceType == ON_OFF)
    {
      initRelay(currentItem.executivePin);
      setupButton(currentItem.inputPin);
    }
    if (currentItem.deviceType == DS18B20)
    {
      DallasTemperature *tempSensor = new DallasTemperature(new OneWire(currentItem.inputPin));
      tempSensor->begin();
      tempSensors->add({tempSensor, -127.0f, currentItem.topicBase});
    }
  }

  Ethernet.begin(internetProps.mac, internetProps.ip);
  delay(3000);
  mqttClient->setServer(mqttProps.mqttServer, mqttProps.mqttPort);
  connectToMqtt();
}

void MqttGateway::loop()
{
  if (!mqttClient->connected())
  {
    if (canReconnect())
    {
      Serial.println("Connection to mqtt lost, reconnecting in 60 seconds...");
      connectToMqtt();
      lastRetry = millis();
    }
  }
  mqttClient->loop();
  buttonsLoop();
  ds18b20Loop();
}

void MqttGateway::initRelay(int executivePin)
{
  if (executivePin > -1)
  {
    digitalWrite(executivePin, RELAY_OFF);
    pinMode(executivePin, OUTPUT);
  }
}

void MqttGateway::setupButton(int buttonPin)
{
  if (buttonPin > -1)
  {
    pinMode(buttonPin, INPUT_PULLUP);
    Bounce *debouncer = new Bounce();
    debouncer->attach(buttonPin);
    debouncer->interval(100);
    debouncers->add({buttonPin, debouncer});
  }
}

void MqttGateway::buttonsLoop()
{
  for (int index = 0; index < debouncers->size(); index++)
  {
    DebouncerPin debouncerPin = debouncers->get(index);
    Bounce *debouncer = debouncerPin.debouncer;
    if (debouncer->update())
    {
      int value = debouncer->read();
      if (value == LOW)
      {
        MqttItem assignedItem = findAssignedItem(debouncerPin.pin);
        if (assignedItem.executivePin > -1)
        {
          int newValue = !digitalRead(assignedItem.executivePin);
          digitalWrite(assignedItem.executivePin, newValue);
          sendOnOff(assignedItem.topicBase, newValue);
        }
      }
    }
  }
}

void MqttGateway::ds18b20Loop()
{
  if (canReadTemperature())
  {
    for (int index = 0; index < tempSensors->size(); index++)
    {
      DS18B20Temp temperature = tempSensors->get(index);
      DallasTemperature *tempSensor = temperature.sensor;

      tempSensor->requestTemperatures();
      Serial.println("Reading temperature...");
      float newTemp = tempSensor->getTempCByIndex(0);

      int roundedNewTemp = (int)round(newTemp * 10);
      int roundedOldTem = (int)round(temperature.lastTemp * 10);

      if (newTemp > -127 && newTemp < 85 && roundedNewTemp != roundedOldTem)
      {
        Serial.println("Updating temperature");
        Serial.println(tempSensor->getTempCByIndex(0), 1);
        temperature.lastTemp = newTemp;
        tempSensors->set(index, temperature);
        sendTemperature(temperature.topicBase, newTemp);
      }

      lastTemperatureRead = millis();
    }
  }
}

MqttItem MqttGateway::findAssignedItem(const int buttonPin)
{
  for (int item = 0; item < mqttItems->size(); item++)
  {
    MqttItem currentItem = mqttItems->get(item);
    if (currentItem.inputPin == buttonPin)
    {
      return currentItem;
    }
  }
  return {-1};
}

void MqttGateway::connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  if (mqttClient->connect((char *)mqttProps.deviceName.c_str(), mqttProps.mqttUser, mqttProps.mqttPassword))
  {
    for (int item = 0; item < mqttItems->size(); item++)
    {
      MqttItem currentItem = mqttItems->get(item);
      if (currentItem.deviceType == ON_OFF)
      {
        mqttClient->subscribe(toCharArray(getCmdTopic(currentItem.topicBase)));
      }
    }
    Serial.println("Connected");
  }
  else
  {
    Serial.print("failed with state ");
    Serial.println(mqttClient->state());
    delay(1000);
  }
}

void MqttGateway::sendOnOff(String topicBase, int currentState)
{
  mqttClient->publish(toCharArray(getStateTopic(topicBase)), toPayload(currentState), true);
}

void MqttGateway::sendTemperature(String topicBase, float temperature)
{
  String payload = String("{\"temperature\": ")+ String(temperature)+ String(" }");
  mqttClient->publish(toCharArray(topicBase), toCharArray(payload), true);
}

const char *MqttGateway::toPayload(const bool currentState)
{
  if (currentState == RELAY_OFF)
  {
    return "off";
  }
  return "on";
}

void MqttGateway::callback(char *topic, byte *payload, unsigned int length)
{

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  for (int item = 0; item < mqttItems->size(); item++)
  {
    MqttItem currentItem = mqttItems->get(item);
    if (currentItem.deviceType == ON_OFF)
    {
      if (String(topic) == getCmdTopic(currentItem.topicBase))
      {
        handleRelay(currentItem.executivePin, payload, length);
      }
    }
  }
  Serial.println("-----------------------");
}

String MqttGateway::getCmdTopic(String topicBase)
{
  return topicBase + "/cmd";
}

String MqttGateway::getStateTopic(String topicBase)
{
  return topicBase + "/state";
}

void MqttGateway::handleRelay(int executivePin, byte *payload, unsigned int length)
{
  if (!strncmp((char *)payload, "off", length))
  {
    digitalWrite(executivePin, RELAY_OFF);
  }
  else if (!strncmp((char *)payload, "on", length))
  {
    digitalWrite(executivePin, RELAY_ON);
  }
}

const char *MqttGateway::toCharArray(String string)
{
  return (char *)string.c_str();
}

boolean MqttGateway::canReadTemperature()
{
  return millis() - lastTemperatureRead > 5000;
}

boolean MqttGateway::canReconnect()
{
  return millis() - lastRetry > 60000;
}