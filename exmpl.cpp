

void handleCalibrate() {
  if (server.hasArg("plain")) {
    String postBody = server.arg("plain");
    Serial.print("Received POST data: ");
    Serial.println(postBody);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, postBody);

    if (error) {
      server.send(500, "text/plain", "Bad JSON");
      return;
    }

    std::string deviceToCalibrate = doc["device"];
    float value = doc["value"];

    Serial.println("Parsed JSON:");
    //Serial.println("deviceToCalibrate: " + deviceToCalibrate);
    Serial.println("value: " + String(value));

    //client.publish("dev_test/termometer/calibrate", String(value));
    client.publish("dev_test/termometer/calibrate", String(value).c_str());

    calibrationMap["termometer"](value);

    server.send(200, "application/json", "{\"response\":\"Received and parsed JSON\"}");
  } else {
    server.send(400, "text/plain", "No JSON data");
  }

}

