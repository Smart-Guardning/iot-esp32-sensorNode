#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// WiFi 설정
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// MQTT 브로커 설정
const char* mqtt_server = "your_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* mqtt_user = "your_MQTT_USER";
const char* mqtt_password = "your_MQTT_PASSWORD";

// 센서 핀 설정
#define SOIL_MOISTURE_PIN 34
#define WATER_LEVEL_PIN 35
#define DHT_PIN 14
#define RELAY_PIN 27

DHT dht(DHT_PIN, DHT11);

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT 연결 함수
void connectToMQTT() {
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
            Serial.println("connected");
            client.subscribe("smartfarm/commands");
        } else {
            Serial.print("failed with state ");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

// WiFi 연결 함수
void connectToWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

// 센서 데이터 수집 함수
void collectSensorData() {
    int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
    int waterLevelValue = analogRead(WATER_LEVEL_PIN);
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temp) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    Serial.print("Soil Moisture: ");
    Serial.println(soilMoistureValue);
    Serial.print("Water Level: ");
    Serial.println(waterLevelValue);
    Serial.print("Temperature: ");
    Serial.println(temp);
    Serial.print("Humidity: ");
    Serial.println(humidity);

    // MQTT 메시지 전송
    String payload = "{";
    payload += "\"soil_moisture\": " + String(soilMoistureValue) + ",";
    payload += "\"water_level\": " + String(waterLevelValue) + ",";
    payload += "\"temperature\": " + String(temp) + ",";
    payload += "\"humidity\": " + String(humidity);
    payload += "}";

    client.publish("smartfarm/sensor", payload.c_str());
}

// MQTT 메시지 콜백 함수
void callback(char* topic, byte* payload, unsigned int length) {
    String incoming = "";
    for (int i = 0; i < length; i++) {
        incoming += (char)payload[i];
    }

    Serial.print("Message received: ");
    Serial.println(incoming);

    if (incoming == "WATER_ON") {
        digitalWrite(RELAY_PIN, HIGH);
    } else if (incoming == "WATER_OFF") {
        digitalWrite(RELAY_PIN, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(WATER_LEVEL_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    dht.begin();

    connectToWiFi();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    connectToMQTT();
}

void loop() {
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();

    collectSensorData();

    delay(2000);
}
