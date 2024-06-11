#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <esp_sleep.h>
#include <algorithm>
#include <Preferences.h>

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
#define BATTERY_PIN 35

DHT dht(DHT_PIN, DHT22);

WiFiClient espClient;
PubSubClient client(espClient);

// 설정값 저장을 위한 Preferences 객체 생성
Preferences preferences;

bool autoWatering = false;              // 자동 물주기 설정
int targetMoisture = 2500;              // 기본 목표 습도
bool watering = false;                  // 물주기 상태를 나타내는 변수
bool isSleeping = false;                // Deep Sleep 모드 설정
unsigned long wateringStartTime = 0;    // 물주기 시작 시간
unsigned long wateringDuration = 3000;  // 물주기 시간 (기본 3초)
unsigned long measurementInterval = 30; // 측정 주기 (기본 30분)

int soilMoistureValue;
int waterLevelValue;

// 함수 선언
void connectToMQTT();
void connectToWiFi();
void collectSensorData();
void callback(char *topic, byte *payload, unsigned int length);
void processSerialInput();
void goToSleep();
template <typename T>
T getMedianValue(T *values, size_t size);
void sendMQTTSettings();
float readBatteryLevel();
void printCurrentSettings();
void checkAutoWatering();
void loadSettings();
void saveSettings();

void setup()
{
    Serial.begin(115200);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(WATER_LEVEL_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT); // LED 핀 설정
    pinMode(BATTERY_PIN, INPUT);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW); // 초기 상태에서 LED 끄기
    loadSettings();
    dht.begin();
    connectToWiFi();
    client.setCallback(callback);
    Serial.println("Starting MQTT connection");
    connectToMQTT();
}

void loop()
{
    if (!client.connected())
    {
        Serial.println("MQTT client not connected, reconnecting...");
        connectToMQTT();
    }

    client.loop();

    if (!watering)
    { // 펌프가 가동 중이 아닐 때만 센서 데이터 수집
        collectSensorData();
    }

    processSerialInput();
    printCurrentSettings();

    // 자동 물주기 기능 체크
    checkAutoWatering();

    if (isSleeping)
    {
        goToSleep();
    }
    delay(3000);
}

// MQTT 연결 함수
void connectToMQTT()
{
    client.setServer(mqtt_broker_ip, mqtt_port);
    while (!client.connected())
    {
        Serial.print("Connecting to MQTT...");
        if (client.connect(nodeID))
        {
            Serial.println("connected");
            String commandTopic = String("smartfarm/commands/") + nodeID;
            client.subscribe(commandTopic.c_str());
            Serial.print("Subscribed to: ");
            Serial.println(commandTopic.c_str());
        }
        else
        {
            Serial.print("failed with state ");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

// WiFi 연결 함수
void connectToWiFi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

// 센서 데이터 수집 함수
void collectSensorData()
{
    const int numReadings = 5;
    int soilMoistureValues[numReadings];
    int waterLevelValues[numReadings];
    float tempValues[numReadings];
    float humidityValues[numReadings];

    for (int i = 0; i < numReadings; i++)
    {
        soilMoistureValues[i] = analogRead(SOIL_MOISTURE_PIN);
        waterLevelValues[i] = analogRead(WATER_LEVEL_PIN);
        tempValues[i] = dht.readTemperature();
        humidityValues[i] = dht.readHumidity();
        delay(50); // 각 측정 사이에 약간의 지연을 추가하여 안정화
    }

    soilMoistureValue = getMedianValue(soilMoistureValues, numReadings);
    waterLevelValue = getMedianValue(waterLevelValues, numReadings);
    float temp = getMedianValue(tempValues, numReadings);
    float humidity = getMedianValue(humidityValues, numReadings);
    int waterpipe = digitalRead(RELAY_PIN);  // 릴레이 핀의 현재 상태 읽기
    int error_code = 0;                      // 0: 정상, 1: 오류 DHT11 센서 오류, 2: 오류 수위 센서 오류, 3: 오류 토양 습도 센서 오류
    float batteryLevel = readBatteryLevel(); // 배터리 레벨 읽기

    String tempStr = isnan(temp) ? "null" : String(temp);
    String humidityStr = isnan(humidity) ? "null" : String(humidity);

    if (isnan(temp) || isnan(humidity))
    {
        Serial.println("Failed to read from DHT sensor!");
        error_code = 1;
    }

    if (waterLevelValue < 0 || waterLevelValue > 4095)
    {
        Serial.println("Failed to read from water level sensor!");
        error_code = 2;
    }

    if (soilMoistureValue < 0 || soilMoistureValue > 9999)
    {
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
    Serial.print("Battery Level: ");
    Serial.println(batteryLevel);
    Serial.print("Error Code: ");
    Serial.println(error_code);

    // MQTT 메시지 전송 (펌프가 가동 중이 아닐 때만 전송)
    if (!watering)
    {
        String payload = "{";
        payload += "\"node_id\": \"" + String(nodeID) + "\",";
        payload += "\"soil_moisture\": " + String(soilMoistureValue) + ",";
        payload += "\"water_level\": " + String(waterLevelValue) + ",";
        payload += "\"temperature\": " + tempStr + ",";
        payload += "\"humidity\": " + humidityStr + ",";
        payload += "\"waterpipe\": " + String(waterpipe) + ",";
        payload += "\"battery_level\": " + String(batteryLevel) + ",";
        payload += "\"error_code\": " + String(error_code);
        payload += "}";

        String topic = String("smartfarm/sensor/") + nodeID;
        client.publish(topic.c_str(), payload.c_str());
    }
}

// 자동 물주기 함수
void checkAutoWatering() {
    if (autoWatering) {
        // 목표 습도와 현재 습도 비교하여 물주기 제어
        if (!watering && soilMoistureValue > targetMoisture && waterLevelValue > 200) {
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

// 배터리 레벨 읽기 함수
float readBatteryLevel()
{
    return analogRead(BATTERY_PIN) / 4096.0 * 7.445;
}

// 시리얼 입력 처리 함수
void processSerialInput()
{
    if (Serial.available() > 0)
    {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.startsWith("SSID:"))
        {
            input.remove(0, 5);
            input.toCharArray(ssid, sizeof(ssid));
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("PASSWORD:"))
        {
            input.remove(0, 9);
            input.toCharArray(password, sizeof(password));
            Serial.print("Password set to: ");
            Serial.println(password);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("MQTT_BROKER:"))
        {
            input.remove(0, 12);
            input.toCharArray(mqtt_broker_ip, sizeof(mqtt_broker_ip));
            Serial.print("MQTT Broker IP set to: ");
            Serial.println(mqtt_broker_ip);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("MQTT_PORT:"))
        {
            input.remove(0, 10);
            mqtt_port = input.toInt();
            Serial.print("MQTT Broker Port set to: ");
            Serial.println(mqtt_port);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("NODEID:"))
        {
            input.remove(0, 7);
            input.toCharArray(nodeID, sizeof(nodeID));
            Serial.print("Node ID set to: ");
            Serial.println(nodeID);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("AUTO_WATER:"))
        {
            input.remove(0, 11);
            autoWatering = (input == "ON");
            Serial.print("Auto Watering set to: ");
            Serial.println(autoWatering ? "ON" : "OFF");
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("TARGET_MOISTURE:"))
        {
            input.remove(0, 15);
            targetMoisture = input.toInt();
            Serial.print("Target Moisture set to: ");
            Serial.println(targetMoisture);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("WATER_DURATION:"))
        {
            input.remove(0, 15);
            wateringDuration = input.toInt();
            Serial.print("Watering Duration set to: ");
            Serial.println(wateringDuration);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("MEASUREMENT_INTERVAL:"))
        {
            input.remove(0, 20);
            measurementInterval = input.toInt();
            Serial.print("Measurement Interval set to: ");
            Serial.println(measurementInterval);
            sendMQTTSettings();
            saveSettings();
        }
        else if (input.startsWith("SLEEP:"))
        {
            input.remove(0, 6);
            isSleeping = (input == "ON");
            Serial.print("Sleep Mode set to: ");
            Serial.println(isSleeping ? "ON" : "OFF");
            sendMQTTSettings();
            saveSettings();
        }
    }
}

// 설정값을 MQTT로 전송하는 함수
void sendMQTTSettings()
{
    String payload = "{";
    payload += "\"node_id\": \"" + String(nodeID) + "\",";
    payload += "\"ssid\": \"" + String(ssid) + "\",";
    payload += "\"password\": \"" + String(password) + "\",";
    payload += "\"mqtt_broker\": \"" + String(mqtt_broker_ip) + "\",";
    payload += "\"mqtt_port\": " + String(mqtt_port) + ",";
    payload += "\"auto_watering\": " + String(autoWatering ? "ON" : "OFF") + ",";
    payload += "\"target_moisture\": " + String(targetMoisture) + ",";
    payload += "\"watering_duration\": " + String(wateringDuration) + ",";
    payload += "\"measurement_interval\": " + String(measurementInterval) + ",";
    payload += "\"sleep_mode\": " + String(isSleeping ? "ON" : "OFF");
    payload += "}";

    String topic = String("smartfarm/settings/") + nodeID;
    client.publish(topic.c_str(), payload.c_str());
}

// Deep Sleep 모드로 전환 함수
void goToSleep()
{
    Serial.println("Going to sleep...");
    esp_sleep_enable_timer_wakeup(measurementInterval * 1000000);
    esp_deep_sleep_start();
}

// MQTT 메시지 콜백 함수
void callback(char *topic, byte *payload, unsigned int length)
{
    String incoming = "";
    for (int i = 0; i < length; i++)
    {
        incoming += (char)payload[i];
    }

    Serial.print("Message received on topic ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(incoming);

    if (incoming.startsWith("TARGET_MOISTURE:"))
    {
        String valueStr = incoming.substring(16); 
        targetMoisture = valueStr.toInt();        
        Serial.print("New target moisture set to: ");
        Serial.println(targetMoisture);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("AUTO_WATER:"))
    {
        autoWatering = (incoming.substring(11) == "ON");
        Serial.print("Auto Watering set to: ");
        Serial.println(autoWatering ? "ON" : "OFF");
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("WATER_DURATION:"))
    {
        String valueStr = incoming.substring(15); 
        wateringDuration = valueStr.toInt();
        Serial.print("Watering Duration set to: ");
        Serial.println(wateringDuration);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("MEASUREMENT_INTERVAL:"))
    {
        String valueStr = incoming.substring(21); 
        measurementInterval = valueStr.toInt(); 
        Serial.print("Measurement Interval set to: ");
        Serial.println(measurementInterval);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("SLEEP:"))
    {
        isSleeping = (incoming.substring(6) == "ON");
        Serial.print("Sleep Mode set to: ");
        Serial.println(isSleeping ? "ON" : "OFF");
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("SSID:"))
    {
        incoming.remove(0, 5);
        incoming.toCharArray(ssid, sizeof(ssid));
        Serial.print("SSID set to: ");
        Serial.println(ssid);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("PASSWORD:"))
    {
        incoming.remove(0, 9);
        incoming.toCharArray(password, sizeof(password));
        Serial.print("Password set to: ");
        Serial.println(password);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("MQTT_BROKER:"))
    {
        incoming.remove(0, 12);
        incoming.toCharArray(mqtt_broker_ip, sizeof(mqtt_broker_ip));
        Serial.print("MQTT Broker IP set to: ");
        Serial.println(mqtt_broker_ip);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("MQTT_PORT:"))
    {
        mqtt_port = incoming.substring(10).toInt();
        Serial.print("MQTT Broker Port set to: ");
        Serial.println(mqtt_port);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming.startsWith("NODEID:"))
    {
        incoming.remove(0, 7);
        incoming.toCharArray(nodeID, sizeof(nodeID));
        Serial.print("Node ID set to: ");
        Serial.println(nodeID);
        sendMQTTSettings();
        saveSettings();
    }
    else if (incoming == "WATER_ON")
    {
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        Serial.println("RELAY_PIN set to HIGH for manual watering");
    }
    else if (incoming == "WATER_OFF")
    {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        Serial.println("RELAY_PIN set to LOW for manual watering");
    }
    else
    {
        Serial.println("Unknown command received");
    }
}

// 중간 값 필터 함수
template <typename T>
T getMedianValue(T *values, size_t size)
{
    std::sort(values, values + size);
    if (size % 2 == 0)
    {
        return (values[size / 2 - 1] + values[size / 2]) / 2;
    }
    else
    {
        return values[size / 2];
    }
}

// 현재 세팅 출력
void printCurrentSettings()
{
    Serial.println("Current Settings:");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("MQTT Broker IP: ");
    Serial.println(mqtt_broker_ip);
    Serial.print("MQTT Port: ");
    Serial.println(mqtt_port);
    Serial.print("Node ID: ");
    Serial.println(nodeID);
    Serial.print("Auto Watering: ");
    Serial.println(autoWatering ? "ON" : "OFF");
    Serial.print("Target Moisture: ");
    Serial.println(targetMoisture);
    Serial.print("Watering Duration: ");
    Serial.println(wateringDuration);
    Serial.print("Measurement Interval: ");
    Serial.println(measurementInterval);
    Serial.print("Sleep Mode: ");
    Serial.println(isSleeping ? "ON" : "OFF");
}

// 설정값 저장 함수
void saveSettings()
{
    preferences.begin("settings", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putString("mqtt_broker_ip", mqtt_broker_ip);
    preferences.putInt("mqtt_port", mqtt_port);
    preferences.putString("nodeID", nodeID);
    preferences.putBool("autoWatering", autoWatering);
    preferences.putInt("targetMoisture", targetMoisture);
    preferences.putInt("wateringDuration", wateringDuration);
    preferences.putInt("measurementInterval", measurementInterval);
    preferences.putBool("isSleeping", isSleeping);
    preferences.end();
}

// 설정값 불러오기 함수
void loadSettings()
{
    preferences.begin("settings", true);
    String stored_ssid = preferences.getString("ssid", ssid); // 두 번째 인자는 기본값
    String stored_password = preferences.getString("password", password);
    String stored_mqtt_broker_ip = preferences.getString("mqtt_broker_ip", mqtt_broker_ip);
    int stored_mqtt_port = preferences.getInt("mqtt_port", mqtt_port);
    String stored_nodeID = preferences.getString("nodeID", nodeID);
    bool stored_autoWatering = preferences.getBool("autoWatering", autoWatering);
    int stored_targetMoisture = preferences.getInt("targetMoisture", targetMoisture);
    int stored_wateringDuration = preferences.getInt("wateringDuration", wateringDuration);
    int stored_measurementInterval = preferences.getInt("measurementInterval", measurementInterval);
    bool stored_isSleeping = preferences.getBool("isSleeping", isSleeping);
    preferences.end();

    // 불러온 값을 현재 변수에 저장
    stored_ssid.toCharArray(ssid, sizeof(ssid));
    stored_password.toCharArray(password, sizeof(password));
    stored_mqtt_broker_ip.toCharArray(mqtt_broker_ip, sizeof(mqtt_broker_ip));
    mqtt_port = stored_mqtt_port;
    stored_nodeID.toCharArray(nodeID, sizeof(nodeID));
    autoWatering = stored_autoWatering;
    targetMoisture = stored_targetMoisture;
    wateringDuration = stored_wateringDuration;
    measurementInterval = stored_measurementInterval;
    isSleeping = stored_isSleeping;
}