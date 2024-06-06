////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////                                           ////////////////////////
/////////////////////////  센서 노드 없을때 테스트를 하기 위한 코드    ////////////////////////
/////////////////////////                                           ////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

const mqtt = require('mqtt');

// MQTT 브로커 설정
const mqttBrokerUrl = 'mqtt://localhost:1884'; // Mosquitto 브로커 IP 주소
const mqttClient = mqtt.connect(mqttBrokerUrl);

let autoWatering = false; // 자동 물주기 설정
let targetMoisture = 2500; // 기본 목표 습도
let watering = false;    // 물주기 상태를 나타내는 변수
let isSleeping = false;   // Deep Sleep 모드 설정
let wateringDuration = 3000; // 물주기 시간 (기본 3초)
let measurementInterval = 30; // 측정 주기 (기본 30분)
const nodeID = 'smartSensor-ZERO'; // 더미 노드 ID

// 센서 데이터 생성 함수
function generateSensorData() {
    const soilMoisture = Math.floor(Math.random() * 1024); // 0-1023 사이의 랜덤 값
    const waterLevel = Math.floor(Math.random() * 1024);   // 0-1023 사이의 랜덤 값
    const temperature = (Math.random() * 30 + 10).toFixed(2); // 10-40도 사이의 랜덤 값
    const humidity = (Math.random() * 50 + 30).toFixed(2);    // 30-80% 사이의 랜덤 값
    // 릴레이 스위치 상태
    const waterpipe = watering ? 1 : 0; // 0-1 사이의 랜덤 값
    const errorCode = Math.floor(Math.random() * 2); // 0-1 사이의 랜덤 값

    return {
        node_id: nodeID,
        soil_moisture: soilMoisture,
        water_level: waterLevel,
        temperature: temperature,
        humidity: humidity,
        waterpipe: waterpipe,
        error_code: errorCode,
    };
}

// 설정값을 MQTT로 전송하는 함수
function sendMQTTSettings() {
    const payload = JSON.stringify({
        node_id: nodeID,
        auto_watering: autoWatering ? "ON" : "OFF",
        target_moisture: targetMoisture,
        watering_duration: wateringDuration,
        measurement_interval: measurementInterval,
        sleep_mode: isSleeping ? "ON" : "OFF"
    });

    const topic = `smartfarm/settings/${nodeID}`;
    mqttClient.publish(topic, payload, {}, (err) => {
        if (err) {
            console.error('Failed to publish settings', err);
        } else {
            console.log(`Settings published to ${topic}: ${payload}`);
        }
    });
}

// MQTT 연결 및 데이터 발행
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');

    // 주기적으로 센서 데이터 발행 (3초마다)
    setInterval(() => {
        if (!watering) { // 펌프가 작동 중이 아닐 때만 데이터 발행
            const sensorData = generateSensorData();
            const payload = JSON.stringify(sensorData);
            const topic = `smartfarm/sensor/${sensorData.node_id}`; // 고유한 주제 사용

            mqttClient.publish(topic, payload, {}, (err) => {
                if (err) {
                    console.error('Failed to publish sensor data', err);
                } else {
                    console.log(`Sensor data published to ${topic}: ${payload}`);
                }
            });
        }
    }, 3000);

    // 초기에 설정값을 전송
    sendMQTTSettings();
});

mqttClient.on('error', (err) => {
    console.error('MQTT connection error:', err);
});
