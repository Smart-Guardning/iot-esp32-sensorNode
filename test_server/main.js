const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const SerialPort = require('serialport');
const Readline = require('@serialport/parser-readline');
const listSerialPorts = require('@serialport/list');
const aedes = require('aedes')();
const net = require('net');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);
const webPort = 3333;

// aedes MQTT 브로커 설정
const mqttBroker = net.createServer(aedes.handle);
const mqttPort = 1883;

mqttBroker.listen(mqttPort, function () {
    console.log(`Aedes MQTT broker started and listening on port ${mqttPort}`);
});

// 시리얼 포트 설정
let serialPort;
let parser;

// 현재 구독 중인 노드 ID 저장
let nodes = new Set();

// Express 서버 설정
app.use(express.static('public'));

app.get('/', (req, res) => {
    res.sendFile(__dirname + '/public/index.html');
});

app.get('/serialPorts', async (req, res) => {
    const ports = await listSerialPorts();
    res.json(ports);
});

io.on('connection', (socket) => {
    console.log('Socket.io client connected:', socket.id);

    socket.on('disconnect', () => {
        console.log('Socket.io client disconnected:', socket.id);
    });

    // 클라이언트 연결 시 현재 노드 목록 전송
    socket.emit('updateNodes', Array.from(nodes));

    // 시리얼 포트 선택
    socket.on('selectSerialPort', async (portPath) => {
        if (serialPort && serialPort.isOpen) {
            await serialPort.close();
        }

        serialPort = new SerialPort(portPath, { baudRate: 115200 });
        parser = serialPort.pipe(new Readline({ delimiter: '\n' }));

        parser.on('data', (data) => {
            console.log('Data from Arduino:', data);
        });

        serialPort.on('open', () => {
            console.log('Serial port opened:', portPath);
        });

        serialPort.on('close', () => {
            console.log('Serial port closed');
        });

        serialPort.on('error', (err) => {
            console.error('Serial port error:', err);
        });
    });

    // 워터펌프 제어 명령 수신
    socket.on('controlWaterPump', (data) => {
        const { nodeID, command } = data;
        const commandTopic = `smartfarm/commands/${nodeID}`;
        aedes.publish({ topic: commandTopic, payload: command }, (err) => {
            if (err) {
                console.error('Failed to publish command', err);
            } else {
                console.log(`Command ${command} sent to node ${nodeID}`);
            }
        });
    });

    // 설정 명령 수신
    socket.on('updateSettings', (data) => {
        const { ssid, password, mqttBroker, mqttPort, nodeID, autoWater, targetMoisture, waterDuration } = data;
        if (ssid) serialPort.write(`SSID:${ssid}\n`);
        if (password) serialPort.write(`PASSWORD:${password}\n`);
        if (mqttBroker) serialPort.write(`MQTT_BROKER:${mqttBroker}\n`);
        if (mqttPort) serialPort.write(`MQTT_PORT:${mqttPort}\n`);
        if (nodeID) serialPort.write(`NODEID:${nodeID}\n`);
        if (autoWater) serialPort.write(`AUTO_WATER:${autoWater}\n`);
        if (targetMoisture) serialPort.write(`TARGET_MOISTURE:${targetMoisture}\n`);
        if (waterDuration) serialPort.write(`WATER_DURATION:${waterDuration}\n`);
        console.log('Settings updated via serial port');
    });
});

server.listen(webPort, () => {
    console.log(`Web server started and listening on port ${webPort}`);
});
