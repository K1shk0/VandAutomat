#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>

#define WIFI_SSID "IoT_H3/4"
#define WIFI_PASSWORD "98806829"
#define MQTT_HOST "wilsons.local"
#define MQTT_PORT 1883

#define RELAY_PIN 13
#define FLOATTER_PIN 33
#define MOISTURE_SENSOR_PIN 34
#define FLOATTER2_PIN 25
#define RED_LED_PIN 19
#define BLUE_LED_PIN 21
#define YELLOW_LED_PIN 22
#define GREEN_LED_PIN 23

#define MOISTURE_THRESHOLD 2800

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
#define MQTT_PUB_STATE1 "lly/autovander/hvile"
#define MQTT_PUB_STATE2 "lly/autovander/tilstand"

// Relay is ACTIVE LOW
#define PUMP_ON LOW
#define PUMP_OFF HIGH

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      configTime(0, 0, "pool.ntp.org");

setenv(
  "TZ",
  "CET-1CEST,M3.5.0,M10.5.0/3",
  1
);

tzset();
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}


void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

String getTimestamp() {
  struct tm timeInfo;

  if (!getLocalTime(&timeInfo)) {
    return "TIME_ERROR";
  }

  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%S",
    &timeInfo
  );

  return String(buffer);
}

void publishState(String message) {
  String fullMessage =
    getTimestamp() + " | " + message;

  mqttClient.publish(
    MQTT_PUB_STATE2,
    1,
    true,
    fullMessage.c_str()
  );
}

void setup() {

  Serial.begin(115200);

  // Pump
  pinMode(RELAY_PIN, OUTPUT);

  // Sensors
  pinMode(FLOATTER_PIN, INPUT_PULLDOWN);
  pinMode(FLOATTER2_PIN, INPUT_PULLDOWN);
  pinMode(MOISTURE_SENSOR_PIN, INPUT);

  // LEDs
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Pump starts OFF
  digitalWrite(RELAY_PIN, PUMP_OFF);

  // LEDs start OFF
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  Serial.println("Garden system started!");

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  connectToWifi();
}

void loop() {

  // -------------------------
  // Read sensors
  // -------------------------

  int moistureValue = analogRead(MOISTURE_SENSOR_PIN);

  int floatter1 = digitalRead(FLOATTER_PIN);
  int floatter2 = digitalRead(FLOATTER2_PIN);

  // -------------------------
  // Print sensor values
  // -------------------------

  Serial.println("-------------------------");

  Serial.print("Moisture value: ");
  Serial.println(moistureValue);

  Serial.print("Float sensor 1: ");
  Serial.println(floatter1);

  Serial.print("Float sensor 2: ");
  Serial.println(floatter2);

  // -------------------------
  // GREEN LED
  // Soil has enough moisture
  // -------------------------

  if (moistureValue <= MOISTURE_THRESHOLD) {

    digitalWrite(GREEN_LED_PIN, HIGH);

    Serial.println("Soil: WET");
    Serial.println("GREEN LED: ON");

  } else if (moistureValue > MOISTURE_THRESHOLD) {

    digitalWrite(GREEN_LED_PIN, LOW);

    Serial.println("Soil: DRY");

    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_STATE2, 1, true, String(moistureValue).c_str());
  }

  // -------------------------
  // RED LED
  // Float 1:
  // 0 = enough water
  // 1 = not enough water
  // -------------------------

  if (floatter1 == 1) {

    digitalWrite(RED_LED_PIN, HIGH);

    Serial.println("Float 1: NOT enough water");
    Serial.println("RED LED: ON");
    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_STATE2, 1, true, String(floatter1).c_str());

  } else if (floatter1 == 0) {

    digitalWrite(RED_LED_PIN, LOW);

    Serial.println("Float 1: Enough water");

    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_STATE2, 1, true, String(floatter1).c_str());
  }

  // -------------------------
  // YELLOW LED
  // Float 2:
  // 0 = water level OK
  // 1 = overflow risk
  // -------------------------

  if (floatter2 == 1) {

    digitalWrite(YELLOW_LED_PIN, HIGH);

    Serial.println("Float 2: OVERFLOW RISK");
    Serial.println("YELLOW LED: ON");

  } else if (floatter2 == 0) {

    digitalWrite(YELLOW_LED_PIN, LOW);

    Serial.println("Float 2: Water level OK");
    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_STATE2, 1, true, String(floatter2).c_str());
  }


  if (moistureValue > MOISTURE_THRESHOLD && floatter1 == 0 && floatter2 == 0
  ) {

    // Pump ON
    digitalWrite(RELAY_PIN, PUMP_ON);

    // Blue LED ON
    digitalWrite(BLUE_LED_PIN, HIGH);

    Serial.println("PUMP: ON");
    Serial.println("BLUE LED: ON");

  } else if (moistureValue <= MOISTURE_THRESHOLD || floatter1 == 1 || floatter2 == 1) {

    // Pump OFF
    digitalWrite(RELAY_PIN, PUMP_OFF);

    // Blue LED OFF
    digitalWrite(BLUE_LED_PIN, LOW);

    Serial.println("PUMP: OFF");

    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_STATE2, 1, true, String(moistureValue).c_str());
  }


  delay(500);
}