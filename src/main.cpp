#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// WiFi 설정
const char* ssid = "test";
const char* password = "test";

// MQTT 브로커 설정
const char* mqtt_broker_ip = "192.168.1.190";
const int mqtt_port = 1883;
const char* nodeID = "smartSensor-01";

// 센서 핀 설정
#define SOIL_MOISTURE_PIN 34
#define WATER_LEVEL_PIN 33
#define DHT_PIN 21
#define RELAY_PIN 27

DHT dht(DHT_PIN, DHT11);

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT 연결 함수
void connectToMQTT() {
    client.setServer(mqtt_broker_ip, mqtt_port);
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect(nodeID)) {
            Serial.println("connected");
            String commandTopic = String("smartfarm/commands/") + nodeID;
            client.subscribe(commandTopic.c_str());
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
    int waterpipe = 0; // 0: 물펌프 멈춤, 1: 물펌프 작동
    int error_code = 0; // 0: 정상, 1: 오류 DH11 센서 오류, 2: 오류 수위 센서 오류, 3: 오류 토양 습도 센서 오류

    if (isnan(temp) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        error_code = 1;
    }

    if (waterLevelValue < 0 || waterLevelValue > 4095) {
        Serial.println("Failed to read from water level sensor!");
        error_code = 2;
    }

    if (soilMoistureValue < 0 || soilMoistureValue > 4095) {
        Serial.println("Failed to read from soil moisture sensor!");
        error_code = 3;
    }

    Serial.print("Node ID: ");
    Serial.println(nodeID);
    Serial.print("Soil Moisture: ");
    Serial.println(soilMoistureValue);
    Serial.print("Water Level: ");
    Serial.println(waterLevelValue);
    Serial.print("Temperature: ");
    Serial.println(temp);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    Serial.print("Waterpipe: ");
    Serial.println(waterpipe);
    Serial.print("Error Code: ");
    Serial.println(error_code);

    // MQTT 메시지 전송
    String payload = "{";
    payload += "\"node_id\": \"" + String(nodeID) + "\",";
    payload += "\"soil_moisture\": " + String(soilMoistureValue) + ",";
    payload += "\"water_level\": " + String(waterLevelValue) + ",";
    payload += "\"temperature\": " + String(temp) + ",";
    payload += "\"humidity\": " + String(humidity) + ",";
    payload += "\"waterpipe\": " + String(waterpipe) + ",";
    payload += "\"error_code\": " + String(error_code);
    payload += "}";

    String topic = String("smartfarm/sensor/") + nodeID;
    client.publish(topic.c_str(), payload.c_str());
}

// MQTT 메시지 콜백 함수
void callback(char* topic, byte* payload, unsigned int length) {
    String incoming = "";
    for (int i = 0; i < length; i++) {
        incoming += (char)payload[i];
    }

    Serial.print("Message received on topic ");
    Serial.print(topic);
    Serial.print(": ");
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

    client.setCallback(callback);

    // 디버깅 메시지 추가
    Serial.println("Starting MQTT connection");
    connectToMQTT();
}

void loop() {
    if (!client.connected()) {
        // 디버깅 메시지 추가
        Serial.println("MQTT client not connected, reconnecting...");
        connectToMQTT();
    }

    client.loop();

    collectSensorData();

    delay(2000);
}
