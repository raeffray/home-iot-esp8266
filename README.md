
# Playground with a nodemcu ESP8266 wifi

This is just a playground, connecting a ESP8266 to a local mqtt server and publishing and subscribing to topics

## Next step
Make my Roomba great again. It's so crappy right now..

## Using sub exec from the container

```
docker exec mosquitto-server mosquitto_sub -h localhost -p 1883 -t "dev_test/termometer/status" -u "USER" -P "PASSWORD"
```

## Configuring

create a json file at: `data/config.json`

```
{
    "mqttServer": "MQTT_SERVER",
    "mqttPort": MQTT_PORT,
    "mqttUser": "MQTT_USER",
    "mqttPassword": "MQTT_PASSWORD",
    "SSID": "WIFI_SSID",
    "wifiPassword": "WIFI_PASSWORD"
}
```

### resources:
https://arduino.esp8266.com/stable/package_esp8266com_index.json