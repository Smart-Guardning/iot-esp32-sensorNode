const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);
const webPort = 3000;

// MQTT 브로커 설정
const mqttBrokerUrl = 'mqtt://localhost:1883';
const mqttClient = mqtt.connect(mqttBrokerUrl);

// MQTT 클라이언트 설정
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe('smartfarm/#', (err) => {
        if (err) {
            console.error('Failed to subscribe to topic', err);
        } else {
            console.log('Subscribed to topic: smartfarm/#');
        }
    });
});

mqttClient.on('message', (topic, message) => {
    console.log(`Message received on topic ${topic}: ${message.toString()}`);
    io.emit('mqttMessage', { topic, message: message.toString() });
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
});

server.listen(webPort, () => {
    console.log(`Web server started and listening on port ${webPort}`);
});
