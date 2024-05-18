const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);
const webPort = 3000;

// MQTT 브로커 설정
const mqttBrokerUrl = 'mqtt://192.168.1.190:1883'; // MQTT 브로커 주소
const mqttClient = mqtt.connect(mqttBrokerUrl);

// 현재 구독 중인 노드 ID 저장
let nodes = new Set();

// MQTT 클라이언트 설정
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe('smartfarm/sensor/#', (err) => {
        if (err) {
            console.error('Failed to subscribe to topic', err);
        } else {
            console.log('Subscribed to topic: smartfarm/sensor/#');
        }
    });
});

mqttClient.on('message', (topic, message) => {
    console.log(`Message received on topic ${topic}: ${message.toString()}`);
    const payload = JSON.parse(message.toString());
    const nodeID = payload.node_id;

    if (!nodes.has(nodeID)) {
        nodes.add(nodeID);
        io.emit('updateNodes', Array.from(nodes));
    }

    io.emit('mqttMessage', { nodeID, topic, message: message.toString() });
});

mqttClient.on('error', (err) => {
    console.error('MQTT connection error:', err);
});

// Express 서버 설정
app.use(express.static('public'));

app.get('/', (req, res) => {
    res.sendFile(__dirname + '/public/index.html');
});

io.on('connection', (socket) => {
    console.log('Socket.io client connected:', socket.id);

    socket.on('disconnect', () => {
        console.log('Socket.io client disconnected:', socket.id);
    });

    // 클라이언트 연결 시 현재 노드 목록 전송
    socket.emit('updateNodes', Array.from(nodes));
});

server.listen(webPort, () => {
    console.log(`Web server started and listening on port ${webPort}`);
});
