#include <ArduinoOTA.h>
#include <HX711.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include <cmath>

const int BUFFER_SIZE = 300;
const int SCALE_DOUT_PIN = D6;
const int SCALE_SCK_PIN = D5;

HX711 scale(SCALE_DOUT_PIN, SCALE_SCK_PIN);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const boolean static_ip = STATIC_IP;
IPAddress ip(IP);
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);

const char* mqtt_broker = MQTT_BROKER;
const char* mqtt_clientId = MQTT_CLIENTID;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

const char* mqtt_value_topic = MQTT_VALUE_TOPIC;
const char* mqtt_tare_topic = MQTT_TARE_TOPIC;
String availabilityBase = MQTT_CLIENTID;
String availabilitySuffix = "/availability";
String availabilityTopicStr = availabilityBase + availabilitySuffix;
const char* availabilityTopic = availabilityTopicStr.c_str();
const char* birthMessage = "online";
const char* lwtMessage = "offline";

float weight = 0.0;
float previous_weight = 10.0;
float temp_weight = 0.0;

WiFiClient espClient;
PubSubClient client(espClient);

// Wifi setup function

void setup_wifi() {

  ArduinoOTA.setHostname("bed scale");
  ArduinoOTA.begin();

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (static_ip) {
    WiFi.config(ip, gateway, subnet);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print(" WiFi connected - IP address: ");
  Serial.println(WiFi.localIP());
}


void publish_load_sensor_status() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  weight = scale.get_units(10);
  Serial.println(String(weight, 2));


  JsonObject& root = jsonBuffer.createObject();

  root["value"] = (String)weight;


  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish(mqtt_value_topic, buffer, true);
}

// Function that publishes birthMessage

void publish_birth_message() {
  // Publish the birthMessage
  Serial.print("Publishing birth message \"");
  Serial.print(birthMessage);
  Serial.print("\" to ");
  Serial.print(availabilityTopic);
  Serial.println("...");
  client.publish(availabilityTopic, birthMessage, true);
}

void triggerAction(String topic, String requestedAction) {
  if (topic == mqtt_tare_topic && requestedAction == "TARE") {
    Serial.print("Tare scale");
    scale.tare();
    weight = 0.0;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();

  String topicToProcess = topic;
  payload[length] = '\0';
  String payloadToProcess = (char*)payload;
  triggerAction(topicToProcess, payloadToProcess);
}

// Function that runs in loop() to connect/reconnect to the MQTT broker, and publish the current door statuses on connect

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    delay(0);
    // Attempt to connect
    if (client.connect(mqtt_clientId, mqtt_username, mqtt_password, availabilityTopic, 0, true, lwtMessage)) {
      Serial.println("Connected!");

      // Publish the birth message on connect/reconnect
      publish_birth_message();

      // Publish the current status on connect/reconnect to ensure status is synced with whatever happened while disconnected
      publish_load_sensor_status();

    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_broker, 1883);

  scale.set_scale(46268);
  scale.tare();
  weight = scale.get_units(10);
  previous_weight = weight;
  Serial.println(String(weight, 2));
}

void loop() {
  // Connect/reconnect to the MQTT broker and listen for messages
  ArduinoOTA.handle();
  delay(1000);
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  temp_weight = scale.get_units(10);  

  if (temp_weight < 0.5)
  {
    scale.tare();
    temp_weight = 0.0;
  }

  if (abs(temp_weight - previous_weight) > 1 || temp_weight == 0.0) {
    previous_weight = weight;
    weight = temp_weight;
    publish_load_sensor_status();
  }

}
