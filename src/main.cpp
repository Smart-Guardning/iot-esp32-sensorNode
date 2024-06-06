#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <esp_sleep.h>
#include <algorithm>

// WiFi 설정 (기본값, 시리얼 통신으로 설정 가능)
char ssid[50] = "NEOS";
char password[50] = "neosvr1998";

// MQTT 브로커 설정 (기본값, 시리얼 통신으로 설정 가능)
char mqtt_broker_ip[50] = "192.168.1.243";
int mqtt_port = 1884;
char nodeID[50] = "smartSensor-01";

// 센서 핀 설정
#define SOIL_MOISTURE_PIN 34
#define WATER_LEVEL_PIN 33
#define DHT_PIN 21
#define RELAY_PIN 26
#define LED_PIN 5 // LED 핀 설정

DHT dht(DHT_PIN, DHT11);

WiFiClient espClient;
PubSubClient client(espClient);

bool autoWatering = false; // 자동 물주기 설정
int targetMoisture = 2500; // 기본 목표 습도
bool watering = false;    // 물주기 상태를 나타내는 변수
bool isSleeping = false;   // Deep Sleep 모드 설정
unsigned long wateringStartTime = 0; // 물주기 시작 시간
unsigned long wateringDuration = 3000; // 물주기 시간 (기본 3초)
unsigned long measurementInterval = 30; // 측정 주기 (기본 30분)

// 함수 선언
void connectToMQTT();
void connectToWiFi();
void collectSensorData();
void callback(char* topic, byte* payload, unsigned int length);
void processSerialInput();
void goToSleep();
template<typename T> T getMedianValue(T* values, size_t size);

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
    Serial.println("Starting MQTT connection");
    connectToMQTT();
}

void loop() {
    if (!client.connected()) {
        Serial.println("MQTT client not connected, reconnecting...");
        connectToMQTT();
    }

    client.loop();

    if (!watering) { // 펌프가 가동 중이 아닐 때만 센서 데이터 수집
        collectSensorData();
    }

    processSerialInput();

    if (isSleeping) {
        goToSleep();
    }
    delay(3000);
}

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
    const int numReadings = 5;
    int soilMoistureValues[numReadings];
    int waterLevelValues[numReadings];
    float tempValues[numReadings];
    float humidityValues[numReadings];

    for (int i = 0; i < numReadings; i++) {
        soilMoistureValues[i] = analogRead(SOIL_MOISTURE_PIN);
        waterLevelValues[i] = analogRead(WATER_LEVEL_PIN);
        tempValues[i] = dht.readTemperature();
        humidityValues[i] = dht.readHumidity();
        delay(50); // 각 측정 사이에 약간의 지연을 추가하여 안정화
    }

    int soilMoistureValue = getMedianValue(soilMoistureValues, numReadings);
    int waterLevelValue = getMedianValue(waterLevelValues, numReadings);
    float temp = getMedianValue(tempValues, numReadings);
    float humidity = getMedianValue(humidityValues, numReadings);
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

    if (soilMoistureValue < 0 || soilMoistureValue > 9999) {
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

    // MQTT 메시지 전송 (펌프가 가동 중이 아닐 때만 전송)
    if (!watering) {
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
    }

    // 자동 물주기 기능이 켜져 있는지 확인
    if (autoWatering) {
        // 목표 습도와 현재 습도 비교하여 물주기 제어
        if (!watering && soilMoistureValue < targetMoisture) {
            watering = true;
            wateringStartTime = millis();
            digitalWrite(RELAY_PIN, HIGH);
            digitalWrite(LED_PIN, HIGH); // 물펌프가 작동할 때 LED 켜기
            Serial.println("RELAY_PIN set to HIGH for watering");
        }

        // 물주기 시간이 설정된 시간을 넘었는지 확인
        if (watering && millis() - wateringStartTime >= wateringDuration) {
            digitalWrite(RELAY_PIN, LOW);
            digitalWrite(LED_PIN, LOW); // 물펌프가 작동하지 않을 때 LED 끄기
            Serial.println("RELAY_PIN set to LOW after watering duration");
            watering = false;
        }
    }
}

// 시리얼 입력 처리 함수
void processSerialInput() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.startsWith("SSID:")) {
            input.remove(0, 5);
            input.toCharArray(ssid, sizeof(ssid));
            Serial.print("SSID set to: ");
            Serial.println(ssid);
        } else if (input.startsWith("PASSWORD:")) {
            input.remove(0, 9);
            input.toCharArray(password, sizeof(password));
            Serial.print("Password set to: ");
            Serial.println(password);
        } else if (input.startsWith("MQTT_BROKER:")) {
            input.remove(0, 12);
            input.toCharArray(mqtt_broker_ip, sizeof(mqtt_broker_ip));
            Serial.print("MQTT Broker IP set to: ");
            Serial.println(mqtt_broker_ip);
        } else if (input.startsWith("MQTT_PORT:")) {
            input.remove(0, 10);
            mqtt_port = input.toInt();
            Serial.print("MQTT Broker Port set to: ");
            Serial.println(mqtt_port);
        } else if (input.startsWith("NODEID:")) {
            input.remove(0, 7);
            input.toCharArray(nodeID, sizeof(nodeID));
            Serial.print("Node ID set to: ");
            Serial.println(nodeID);
        } else if (input.startsWith("AUTO_WATER:")) {
            input.remove(0, 11);
            autoWatering = (input == "ON");
            Serial.print("Auto Watering set to: ");
            Serial.println(autoWatering ? "ON" : "OFF");
        } else if (input.startsWith("TARGET_MOISTURE:")) {
            input.remove(0, 15);
            targetMoisture = input.toInt();
            Serial.print("Target Moisture set to: ");
            Serial.println(targetMoisture);
        } else if (input.startsWith("WATER_DURATION:")) {
            input.remove(0, 15);
            wateringDuration = input.toInt();
            Serial.print("Watering Duration set to: ");
            Serial.println(wateringDuration);
        } else if (input.startsWith("MEASUREMENT_INTERVAL:")) {
            input.remove(0, 20);
            measurementInterval = input.toInt();
            Serial.print("Measurement Interval set to: ");
            Serial.println(measurementInterval);
        } else if (input.startsWith("SLEEP:")) {
            input.remove(0, 6);
            isSleeping = (input == "ON");
            Serial.print("Sleep Mode set to: ");
            Serial.println(isSleeping ? "ON" : "OFF");
        }
    }
}

// Deep Sleep 모드로 전환 함수
void goToSleep() {
    Serial.println("Going to sleep...");
    esp_sleep_enable_timer_wakeup(measurementInterval * 1000000);
    esp_deep_sleep_start();
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
    } else if (incoming.startsWith("AUTO_WATER")) {
        if (incoming.substring(10) == "ON") {
            autoWatering = true;
            Serial.println("Automatic watering enabled");
        } else if (incoming.substring(10) == "OFF") {
            autoWatering = false;
            Serial.println("Automatic watering disabled");
        }
    } else if (incoming.startsWith("WATER_DURATION")) {
        wateringDuration = incoming.substring(14).toInt();
        Serial.print("New watering duration set to: ");
        Serial.println(wateringDuration);
    } else if (incoming.startsWith("MEASUREMENT_INTERVAL")) {
        measurementInterval = incoming.substring(20).toInt();
        Serial.print("New measurement interval set to: ");
        Serial.println(measurementInterval);
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
    } else if (incoming.startsWith("SLEEP:")) {
        if (incoming.substring(6) == "ON") {
            isSleeping = true;
            Serial.println("Sleep mode enabled");
        } else if (incoming.substring(6) == "OFF") {
            isSleeping = false;
            Serial.println("Sleep mode disabled");
        }
    } else {
        Serial.println("Unknown command received");
    }
}

// 중간 값 필터 함수
template<typename T>
T getMedianValue(T* values, size_t size) {
    std::sort(values, values + size);
    if (size % 2 == 0) {
        return (values[size / 2 - 1] + values[size / 2]) / 2;
    } else {
        return values[size / 2];
    }
}
