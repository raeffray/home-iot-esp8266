#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <string>
#include <functional>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>

std::map<std::string, std::function<void(float)>> calibrationMap;

// MQ Topics
const char *TERM_CALIBRATE_TOPIC = "dev_test/termometer/calibrate";

const char *TERM_STATUS_TOPIC = "dev_test/termometer/status";

const char *mqttUser;
const char *mqttPassword;
const char *ssid;

// adjustments for termometer readings
const int INTERVAL = 250;
const int THERM_PIN = 14;
const float referenceVoltage = 1.0;                                       // Adjust this based on your actual reference voltage if known
const float correctionFactor = 0.29 / (93.0 / 1023.0 * referenceVoltage); // Based on your earlier example

float tempCorrection = 5;

unsigned long lastSendTime = 0; // Stores last time the JSON was sent
const long interval = 5000;     // Interval at which to send JSON (1000 milliseconds or 1 second)

// platforIO complains with we don't declare beforehand
void handleRoot();
void handleMessage();
void handleStatusPage();
void handleCalibrateTermometer(float value);
void tryConnectMq();
float checkTemperature();
void sendJsonToTopic(PubSubClient &mqttClient, const char *topic, const char *serializedJson);
void sendTermometerMessage(PubSubClient &mqttClient, const char *topic, String value);

WiFiClientSecure espClient;

PubSubClient client(espClient);

ESP8266WebServer server(80);

void reconnect()
{

  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    Serial.print("user: ");
    Serial.println(mqttUser);
    
    // Connect with username and password
    if (client.connect("ESP8266Client-dev-test", mqttUser, mqttPassword))
    {
      Serial.println("MQTT client connected.");

      client.subscribe(TERM_CALIBRATE_TOPIC);

      Serial.println("MQTT client subscribed to: ");
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

void setupWifi(String ssid, String password) {
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void setupMq(const char* mqHost, const int mqPort){
  espClient.setInsecure();
  client.setServer(mqHost,mqPort);
  client.setCallback(mqCallback);
}

void setupWebserver(){
  // Define routes
  server.on("/", handleRoot);
  server.on("/send", HTTP_GET, handleMessage);

  server.on("/status", HTTP_GET, handleStatusPage);

  server.begin();
  Serial.println("HTTP server started");
}

void setup()
{
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(THERM_PIN, INPUT);

  //   if (!LittleFS.begin()) {
  //       Serial.println("An error occurred while mounting LittleFS");
  //       return;
  //   } else {
  //       Serial.println("Mounted LittleFS successfully");
  //   }

  // File configFile = LittleFS.open("/config.json", "r");  // Open a file for writing

  // if (configFile) {
  //  size_t size = configFile.size();
  //  std::unique_ptr<char[]> buf(new char[size]);
  //  configFile.readBytes(buf.get(), size);

  JsonDocument doc;
  // deserializeJson(doc, buf.get());

  // const char* mqttServer = doc["mqttServer"];
  // const int mqttPort = doc["mqttPort"];
  // mqttUser = doc["mqttUser"];
  // mqttPassword = doc["mqttPassword"];
  // ssid = doc["SSID"];
  // const char* wifiPassword = doc["wifiPassword"];

  const char *mqttServer = "ZZZ";
  const int mqttPort = 8883;
  mqttUser = "ZZZ";
  mqttPassword = "ZZZ";
  ssid = "ZZZ-2G";
  const char *wifiPassword = "ZZZ";

  setupWifi(ssid, wifiPassword);
  setupMq(mqttServer, mqttPort);
  setupWebserver();
  
  // defines a stratucture to map device and its calibration function
  calibrationMap[TERM_CALIBRATE_TOPIC] = handleCalibrateTermometer;
    //} else {
  //  Serial.println("Config file not loaded");
  //}
}

void loop()
{
  server.handleClient();
  tryConnectMq();

  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= interval)
  {
    float temperature = checkTemperature();
    sendTermometerMessage(client, TERM_STATUS_TOPIC, String(temperature));
    lastSendTime = currentTime; // Update the last send time
  }
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

void tryConnectMq()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}

void handleCalibrateTermometer(float newValue)
{
  tempCorrection = newValue;
  Serial.print("termemometer correction factor set to ");
  Serial.println(newValue);
}

float checkTemperature()
{
  int adcValue = analogRead(A0);

  float voltage = (adcValue / 1023.0 * referenceVoltage) * correctionFactor;
  float temperature = (voltage * 100.0) - tempCorrection; // Convert voltage to temperature

  return temperature;
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
                String(ssid) + "</p>"
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

void sendTermometerMessage(PubSubClient &mqttClient, const char *topic, String value){
  // Create a JSON object
  JsonDocument doc; // Adjust size based on your JSON structure
  doc["deviceId"] = "termometer";
  doc["temperature"] = value;

  char jsonBuffer[512];           // Adjust size based on your JSON structure
  serializeJson(doc, jsonBuffer); // Serialize the JSON object to a buffer

  sendJsonToTopic(mqttClient, TERM_STATUS_TOPIC, jsonBuffer);

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