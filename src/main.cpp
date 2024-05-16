#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <string>
#include <functional>
#include <PubSubClient.h>
#include <WiFiClientSecure.h> 

std::map<std::string, std::function<void(float)>> calibrationMap;

const char* ssid = "SSID";
const char* password = "PWD";

// MQ Credentials
const char* mqtt_user = "USER";
const char* mqtt_password = "PWD";

// MQ Server info
const char* mqtt_server = "SERVER";
const int mqtt_port = 8883;

// MQ Topics
const char* TERM_CALIBRATE_TOPIC = "dev_test/termometer/calibrate";

// adjustments for termometer readings
const int INTERVAL = 250;
const int THERM_PIN = 14;
const float referenceVoltage = 1.0;// Adjust this based on your actual reference voltage if known
const float correctionFactor = 0.29 / (93.0 / 1023.0 * referenceVoltage); // Based on your earlier example

float tempCorrection = 5;

// platforIO complains with we don't declare beforehand
void handleRoot();
void handleMessage();
void handleStatusPage();
void handleCalibrateTermometer(float value);
void tryConnectMq();

WiFiClientSecure espClient;

PubSubClient client(espClient);

ESP8266WebServer server(80);

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Connect with username and password
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      
      client.subscribe(TERM_CALIBRATE_TOPIC);

      Serial.println("subscribed to: ");
      Serial.println(TERM_CALIBRATE_TOPIC);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/**
 * 
 * Defines the MQ Callback function
 * 
*/
void mqCallback(char* topic, byte* payload, unsigned int length) {
  String valueString;
  for (int i = 0; i < length; i++) {
    valueString += (char)payload[i];
  }

  Serial.print("payload: ");
  Serial.println(valueString);

  calibrationMap[topic](valueString.toFloat());

}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  //pinMode(THERM_PIN, INPUT);

  calibrationMap[TERM_CALIBRATE_TOPIC] = handleCalibrateTermometer;

  espClient.setInsecure(); // This is now valid

  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqCallback);

  // Define routes
  server.on("/", handleRoot);
  server.on("/send", HTTP_GET, handleMessage);

  server.on("/status", HTTP_GET, handleStatusPage);

  server.begin();
  Serial.println("HTTP server started");

}

void loop() {
  server.handleClient();
  tryConnectMq();
}

void handleRoot() {
  server.send(200, "text/html", "<h1>You are connected to ESP8266</h1>");
}

void handleMessage() {
  String blinkCountStr = server.arg("blink"); // Get the blink parameter from the URL as a String
  int blinkCount = blinkCountStr.toInt(); // Convert the String to an int -- Fixed: Added missing semicolon

  server.send(200, "text/plain", "Will blink LED " + String(blinkCount) + " times");

  for (int i = 0; i < blinkCount; i++) {
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED on (HIGH is the voltage level)
    delay(INTERVAL);                       // Wait for half a second
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED off by making the voltage LOW
    delay(INTERVAL);                       // Wait for half a second
  }
}

void tryConnectMq() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  
}

void handleCalibrateTermometer(float newValue) {
  tempCorrection = newValue;
  Serial.print("termemometer correction factor set to " );
  Serial.println(newValue);
}

void handleStatusPage() {

  int adcValue = analogRead(A0);
  float voltage = (adcValue / 1023.0 * referenceVoltage) * correctionFactor;

  Serial.print("voltage: ");
  Serial.println(voltage);

  float temperature = (voltage * 100.0) - tempCorrection;   // Convert voltage to temperature

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
                "<p>System Uptime: " + String(millis() / 1000) + " seconds</p>"
                "<p>Operating Voltage: 3.3V</p>"
                "<p>WiFi SSID: " + String(ssid) + "</p>"
                "<p>MAC Address : " + String(WiFi.macAddress()) + "</p>"
                "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>"
                "<p>Room Temperature: " + String(temperature) + " C</p>"
                "</body>"
                "</html>";

  server.send(200, "text/html", html);

}
