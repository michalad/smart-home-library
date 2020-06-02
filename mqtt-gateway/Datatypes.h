struct InternetProps
{
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    byte *mac;
};

struct MqttProps
{
    const char *mqttServer;
    int mqttPort;
    const char *mqttUser;
    const char *mqttPassword;
    String deviceName;
};

enum DeviceType {
  ON_OFF,
  DS18B20
};

struct MqttItem 
{
    int executivePin;
    int inputPin;
    String topicBase;
    DeviceType deviceType;
};

struct DebouncerPin 
{
    int pin;
    Bounce *debouncer;
};

struct DS18B20Temp 
{
    DallasTemperature *sensor;
    float lastTemp;
    String topicBase;
};
