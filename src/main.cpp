#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <string>
#include <functional>
#include <PubSubClient.h>
// #include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <LittleFS.h>

std::map<std::string, std::function<void(float)>> calibrationMap;

// MQ Topics
const char *TERM_CALIBRATE_TOPIC = "termometer/calibrate";

const char *TERM_STATUS_TOPIC = "termometer/status";

String ssid;

// adjustments for termometer readings
const int INTERVAL = 250;
const int THERM_PIN = 14;
const float referenceVoltage = 1.0;                                       // Adjust this based on your actual reference voltage if known
const float correctionFactor = 0.29 / (93.0 / 1023.0 * referenceVoltage); // Based on your earlier example
float tempCorrection = 1;
unsigned long lastSendTime = 0; // Stores last time the JSON was sent
const long interval = 5000;     // Interval at which to send JSON (1000 milliseconds or 1 second)

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(8001);

struct ConfigInfo
{
  int mqttPort;
  String mqttServer;
  String mqttUser;
  String mqttPassword;
  String ssid;
  String wifiPassword;
};

// platforIO complains with we don't declare beforehand
void connect(String mqttUser, String mqttPassword)
{

  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    // Connect with username and password
    if (client.connect("ESP8266Client-dev-test", mqttUser.c_str(), mqttPassword.c_str()))
    {
      Serial.println("MQTT client connected.");

      client.subscribe(TERM_CALIBRATE_TOPIC);

      Serial.print("MQTT client subscribed to: ");
      Serial.println(TERM_CALIBRATE_TOPIC);
    }
    else
    {
      Serial.print("failed, client state [");
      Serial.print(client.state());
      Serial.println("] try again in 5 seconds");
      delay(5000);
    }
  }
}

float checkTemperature()
{
  int adcValue = analogRead(A0);

  Serial.print("Raw reading: ");
  Serial.println(adcValue);

  float voltage = (adcValue / 1023.0 * referenceVoltage) * correctionFactor;
  float temperature = (voltage * 100.0) - tempCorrection; // Convert voltage to temperature

  return temperature;
}

/**
 *
 * Defines the MQ Callback function
 *
 */
void mqCallback(char *topic, byte *payload, unsigned int length)
{
  String valueString;
  for (unsigned i = 0; i < length; i++)
  {
    valueString += (char)payload[i];
  }
  Serial.print("payload receive from topic [");
  Serial.print(topic);
  Serial.print("] : ");
  Serial.println(valueString);

  calibrationMap[topic](valueString.toFloat());
}

void setupWifi(String ssid, String password)
{
  WiFi.begin(ssid, password);

  Serial.println("Waiting for WI-FI Connection");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to local network using IP Address:  ");
  Serial.println(WiFi.localIP());
}

void handleStatusPage()
{
  float temperature = checkTemperature();

  String html = "<!DOCTYPE html>"
                "<html lang='en'>"
                "<head>"
                "<meta charset='UTF-8'>"
                "<meta http-equiv='refresh' content='3'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>Status Page</title>"
                "<style>"
                "body { font-family: 'Arial', sans-serif; background-color: #282c34; color: #61dafb; }"
                "h1 { color: #ff8c00; }"
                "p { font-size: 1.2em; }"
                "</style>"
                "</head>"
                "<body>"
                "<h1>ESP8266 Status</h1>"
                "<p>System Uptime: " +
                String(millis() / 1000) + " seconds</p>"
                                          "<p>Operating Voltage: 3.3V</p>"
                                          "<p>WiFi SSID: " +
                ssid + "</p>"
                               "<p>MAC Address : " +
                String(WiFi.macAddress()) + "</p>"
                                            "<p>Signal Strength: " +
                String(WiFi.RSSI()) + " dBm</p>"
                                      "<p>Room Temperature: " +
                String(temperature) + " C</p>"
                                      "</body>"
                                      "</html>";

  server.send(200, "text/html", html);
}

void handleRoot()
{
  server.send(200, "text/html", "<h1>You are connected to ESP8266</h1>");
}

void handleMessage()
{
  String blinkCountStr = server.arg("blink"); // Get the blink parameter from the URL as a String
  int blinkCount = blinkCountStr.toInt();     // Convert the String to an int -- Fixed: Added missing semicolon

  server.send(200, "text/plain", "Will blink LED " + String(blinkCount) + " times");

  for (int i = 0; i < blinkCount; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on (HIGH is the voltage level)
    delay(INTERVAL);                 // Wait for half a second
    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off by making the voltage LOW
    delay(INTERVAL);                 // Wait for half a second
  }
}

void setupWebserver()
{
  // Define routes
  server.on("/", handleRoot);
  server.on("/send", HTTP_GET, handleMessage);
  server.on("/status", HTTP_GET, handleStatusPage);

  server.begin();
  Serial.println("HTTP server started");
}

void listFilesInLittleFS()
{
  Serial.println("Listing files in LittleFS:");
  Dir dir = LittleFS.openDir("");
  while (dir.next())
  {
    Serial.print("File: ");
    Serial.print(dir.fileName());
    Serial.print(" - Size: ");
    Serial.println(dir.fileSize());
  }
}

ConfigInfo getConfiguration()
{
  if (!LittleFS.begin())
  {
    Serial.println("Failed to mount LittleFS");
    throw std::runtime_error("Failed to mount LittleFS");
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
    Serial.println("Failed to open the config file");
    throw std::runtime_error("Failed to open the config file");
  }

  Serial.println("Config file opened successfully.");
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  Serial.print("Read config file: ");
  Serial.println(buf.get());

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    throw std::runtime_error("Failed to read JSON file");
  }

  ConfigInfo configInfo;

  configInfo.mqttPort = doc["mqttPort"];
  configInfo.mqttServer = doc["mqttServer"].as<String>(); // Ensure type compatibility
  configInfo.mqttUser = doc["mqttUser"].as<String>();
  configInfo.mqttPassword = doc["mqttPassword"].as<String>();
  configInfo.ssid = doc["SSID"].as<String>();
  configInfo.wifiPassword = doc["wifiPassword"].as<String>();

  return configInfo;
}

void connectToMQ(String mqttHost, int mqttPort, String mqttUser, String mqttPassword)
{
  client.setServer(mqttHost.c_str(), mqttPort);
  client.setCallback(mqCallback);

  if (!client.connected())
  {
    connect(mqttUser.c_str(), mqttPassword.c_str());
  }
  client.loop();
}

void handleCalibrateTermometer(float newValue)
{
  tempCorrection = newValue;
  Serial.print("termemometer correction factor set to ");
  Serial.println(newValue);
}

void sendJsonToTopic(PubSubClient &mqttClient, const char *topic, const char *serializedJson)
{
  // Serialize JSON to string
  mqttClient.publish(topic, serializedJson);

  Serial.print("message published to topic [");
  Serial.print(topic);
  Serial.print("] : ");
  Serial.println(serializedJson);
}

void sendTermometerMessage(PubSubClient &mqttClient, const char *topic, String value)
{
  // Create a JSON object
  JsonDocument doc; // Adjust size based on your JSON structure
  doc["deviceId"] = "termometer";
  doc["temperature"] = value;

  char jsonBuffer[512];           // Adjust size based on your JSON structure
  serializeJson(doc, jsonBuffer); // Serialize the JSON object to a buffer

  sendJsonToTopic(mqttClient, TERM_STATUS_TOPIC, jsonBuffer);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  ConfigInfo configInfo = getConfiguration();

  setupWifi(configInfo.ssid, configInfo.wifiPassword);

  connectToMQ(configInfo.mqttServer, configInfo.mqttPort, configInfo.mqttUser, configInfo.mqttPassword);

  setupWebserver();

  ssid = configInfo.ssid;

  Serial.println("Getting Config..");

  calibrationMap[TERM_CALIBRATE_TOPIC] = handleCalibrateTermometer;
}

void loop()
{
  server.handleClient();

  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= interval)
  {
    float temperature = checkTemperature();
    sendTermometerMessage(client, TERM_STATUS_TOPIC, String(temperature));
    lastSendTime = currentTime; // Update the last send time
  }
}
