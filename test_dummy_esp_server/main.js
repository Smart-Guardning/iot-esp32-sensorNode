////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////                                           ////////////////////////
/////////////////////////  센서 노드 없을때 테스트를 하기 위한 코드    ////////////////////////
/////////////////////////                                           ////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

const mqtt = require('mqtt');

// MQTT 브로커 설정
const mqttBrokerUrl = 'mqtt://192.168.1.190:1883'; // Mosquitto 브로커 IP 주소
const mqttClient = mqtt.connect(mqttBrokerUrl);

// 센서 데이터 생성 함수
function generateSensorData() {
    const nodeID = 'smart-sensor-ZERO'; // 더미 노드 ID
    const soilMoisture = Math.floor(Math.random() * 1024); // 0-1023 사이의 랜덤 값
    const waterLevel = Math.floor(Math.random() * 1024);   // 0-1023 사이의 랜덤 값
    const temperature = (Math.random() * 30 + 10).toFixed(2); // 10-40도 사이의 랜덤 값
    const humidity = (Math.random() * 50 + 30).toFixed(2);    // 30-80% 사이의 랜덤 값

    return {
        node_id: nodeID,
        soil_moisture: soilMoisture,
        water_level: waterLevel,
        temperature: temperature,
        humidity: humidity
    };
}

// MQTT 연결 및 데이터 발행
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');

    // 주기적으로 센서 데이터 발행 (2초마다)
    setInterval(() => {
        const sensorData = generateSensorData();
        const payload = JSON.stringify(sensorData);

        mqttClient.publish('smartfarm/sensor', payload, {}, (err) => {
            if (err) {
                console.error('Failed to publish sensor data', err);
            } else {
                console.log('Sensor data published:', payload);
            }
        });
    }, 2000);
});

mqttClient.on('error', (err) => {
    console.error('MQTT connection error:', err);
});
