////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////                                           ////////////////////////
/////////////////////////  센서 노드 없을때 테스트를 하기 위한 코드    ////////////////////////
/////////////////////////                                           ////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

const mqtt = require('mqtt');

// MQTT 브로커 설정
const mqttBrokerUrl = 'mqtt://localhost:1884'; // Mosquitto 브로커 IP 주소
const mqttClient = mqtt.connect(mqttBrokerUrl);

// 센서 데이터 생성 함수
function generateSensorData() {
    const nodeID = 'smartSensor-ZERO'; // 더미 노드 ID
    const soilMoisture = Math.floor(Math.random() * 1024); // 0-1023 사이의 랜덤 값
    const waterLevel = Math.floor(Math.random() * 1024);   // 0-1023 사이의 랜덤 값
    const temperature = (Math.random() * 30 + 10).toFixed(2); // 10-40도 사이의 랜덤 값
    const humidity = (Math.random() * 50 + 30).toFixed(2);    // 30-80% 사이의 랜덤 값
    // 릴레이 스위치 상태
    const waterpipe = Math.floor(Math.random() * 2); // 0-1 사이의 랜덤 값
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

// MQTT 연결 및 데이터 발행
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');

    // 주기적으로 센서 데이터 발행 (3초마다)
    setInterval(() => {
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
    }, 3000);
});

mqttClient.on('error', (err) => {
    console.error('MQTT connection error:', err);
});
