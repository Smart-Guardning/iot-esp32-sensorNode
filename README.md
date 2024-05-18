# iot-esp32-sensorNode
smartguardning의 노드 펌웨어 및 테스트 코드가 있다.

## 구조
프로젝트는 `src`, `test_dummy_esp_server`, `test_server` 로 나눌 수 있다.

### src
ESP32를 위해 작성된 코드이다.
Arduino, WiFi, PubSubClient(MQTT), Adafruit_Sensor, DHT 라이브러리를 사용 한다.

토양 습도 센서, 수위 센서, 공기 중 습도 & 온도 센서(DHT11), 릴레이를 사용하여, 센싱한 데이터를 MQTT 프로토콜로 통신한다.
```
    // MQTT 메시지 전송
    String payload = "{";
    payload += "\"soil_moisture\": " + String(soilMoistureValue) + ",";
    payload += "\"water_level\": " + String(waterLevelValue) + ",";
    payload += "\"temperature\": " + String(temp) + ",";
    payload += "\"humidity\": " + String(humidity) + ",";
    payload += "\"waterpipe\": " + String(waterpipe) + ",";
    payload += "\"error_code\": " + String(error_code);
    payload += "}";

    String topic = String("smartfarm/sensor/") + nodeID;
    client.publish(topic.c_str(), payload.c_str());
```
위의 코드는 토픽 `smartfarm/sensor/[nodeID]`에 `{"node_id":"[노드ID]","soil_moisture":[토양 습도],"water_level":[수위],"temperature":"[온도]","humidity":"[습도]","waterpipe":[0 or 1],"error_code":[0 ~ 3]}`를 전송한다.

시리얼 통신을 통해 `WiFi 설정`, `MQTT 연결 설정`을 할 수 있다.

platform.io를 사용해 `빌드` 및 `업로드`

### test_dummy_esp_server
ESP32 하드웨어를 사용할 수 없을때, 하드웨어의 통신을 모방하는 MQTT 퍼블리셔 이다.

- `dummy[번호].js`: 실행 시키면 고유한 ID를 가지고 mqtt 통신을 시작한다.
    - mqttBrokerUrl 를 수정하여, 해당 주소의 브로커에 접속 할 수 있도록한다.
    - src의 MQTT 통신을 모방한다.

`node ./dummy[번호].js` 로 시작

### test_server
MQTT 웹 라즈베리파이에 있는 모스키토 MQTT 브로커와 통신해, MQTT 클라이언트로 작동한다.
이를 웹 소켓을 이용해 실시간으로 웹 클라이언트로 전송하며, 웹 인터페이스에서 실시간으로 센서 데이터를 모니터링 한다.

Espress, Scoket.io, mqtt, http 모듈사용

`node ./main.js` 로 시작
