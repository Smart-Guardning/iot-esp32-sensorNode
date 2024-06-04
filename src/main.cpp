#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// WiFi 설정
const char* ssid = "NEOS";
const char* password = "neosvr1998";

// MQTT 브로커 설정
const char* mqtt_broker_ip = "192.168.1.243";
const int mqtt_port = 1883;
const char* nodeID = "smartSensor-01";

// 센서 핀 설정
#define SOIL_MOISTURE_PIN 34
#define WATER_LEVEL_PIN 33
#define DHT_PIN 21
#define RELAY_PIN 26
#define LED_PIN 5 // LED 핀 설정

DHT dht(DHT_PIN, DHT11);

WiFiClient espClient;
PubSubClient client(espClient);

int targetMoisture = 500; // 기본 목표 습도
bool watering = false;    // 물주기 상태를 나타내는 변수
unsigned long wateringStartTime = 0; // 물주기 시작 시간

// MQTT 연결 함수
void connectToMQTT() {
    client.setServer(mqtt_broker_ip, mqtt_port);
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect(nodeID)) {
            Serial.println("connected");
            String commandTopic = String("smartfarm/commands/") + nodeID;
            client.subscribe(commandTopic.c_str());
            Serial.print("Subscribed to: ");
            Serial.println(commandTopic.c_str());
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
    float temp = dht.readTemperature(false, true);
    float humidity = dht.readHumidity();
    int waterpipe = digitalRead(RELAY_PIN); // 릴레이 핀의 현재 상태 읽기
    int error_code = 0; // 0: 정상, 1: 오류 DHT11 센서 오류, 2: 오류 수위 센서 오류, 3: 오류 토양 습도 센서 오류

    String tempStr = isnan(temp) ? "null" : String(temp);
    String humidityStr = isnan(humidity) ? "null" : String(humidity);

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
    Serial.println(tempStr);
    Serial.print("Humidity: ");
    Serial.println(humidityStr);
    Serial.print("Waterpipe: ");
    Serial.println(waterpipe);
    Serial.print("Error Code: ");
    Serial.println(error_code);

    // MQTT 메시지 전송
    String payload = "{";
    payload += "\"node_id\": \"" + String(nodeID) + "\",";
    payload += "\"soil_moisture\": " + String(soilMoistureValue) + ",";
    payload += "\"water_level\": " + String(waterLevelValue) + ",";
    payload += "\"temperature\": " + tempStr + ",";
    payload += "\"humidity\": " + humidityStr + ",";
    payload += "\"waterpipe\": " + String(waterpipe) + ",";
    payload += "\"error_code\": " + String(error_code);
    payload += "}";

    String topic = String("smartfarm/sensor/") + nodeID;
    client.publish(topic.c_str(), payload.c_str());
    
    // 목표 습도와 현재 습도 비교하여 물주기 제어
    if (!watering && soilMoistureValue < targetMoisture) {
        watering = true;
        wateringStartTime = millis();
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH); // 물펌프가 작동할 때 LED 켜기
        Serial.println("RELAY_PIN set to HIGH for watering");
    }

    // 물주기 시간이 3초가 넘었는지 확인
    if (watering && millis() - wateringStartTime >= 3000) {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW); // 물펌프가 작동하지 않을 때 LED 끄기
        Serial.println("RELAY_PIN set to LOW after 3 seconds");
        watering = false;
    }
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

    if (incoming.startsWith("TARGET_MOISTURE")) {
        targetMoisture = incoming.substring(15).toInt();
        Serial.print("New target moisture set to: ");
        Serial.println(targetMoisture);
    } else if (incoming == "WATER_ON") {
        watering = true;
        wateringStartTime = millis();
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH); // 물펌프가 작동할 때 LED 켜기
        Serial.println("RELAY_PIN set to HIGH");
    } else if (incoming == "WATER_OFF") {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW); // 물펌프가 작동하지 않을 때 LED 끄기
        Serial.println("RELAY_PIN set to LOW");
        watering = false;
    } else {
        Serial.println("Unknown command received");
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(WATER_LEVEL_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT); // LED 핀 설정
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW); // 초기 상태에서 LED 끄기

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
