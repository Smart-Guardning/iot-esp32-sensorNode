#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define DHT_PIN GPIO_NUM_4
#define SOIL_MOISTURE_PIN ADC1_CHANNEL_0
#define WATER_LEVEL_PIN GPIO_NUM_5
#define RELAY_PIN GPIO_NUM_18
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define DEVICE_NAME "ESP32"
#define MAX_TIMINGS 85
#define OPTIMA_SOIL_MOISTURE 500

static esp_mqtt_client_handle_t mqtt_client;

// DHT11 온습도 센서 데이터 읽기 함수
void read_dht11(float *temperature, float *humidity) {
    uint8_t laststate = 1;
    uint8_t counter = 0;
    uint8_t j = 0, i;
    uint8_t data[5];
    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    // 18ms 신호 전송
    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_PIN, 0);
    vTaskDelay(18 / portTICK_PERIOD_MS);
    // 20-40us 대기
    gpio_set_level(DHT_PIN, 1);
    vTaskDelay(40 / portTICK_PERIOD_MS);
    // 센서로부터 80us 신호 수신
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);

    // DHT11 센서로부터 데이터 수신
    for (i = 0; i < MAX_TIMINGS; i++) {
        counter = 0;
        while (gpio_get_level(DHT_PIN) == laststate) {
            counter++;
            esp_rom_delay_us(1);
            if (counter == 255) {
                break;
            }
        }
        laststate = gpio_get_level(DHT_PIN);
        if (counter == 255) {
            break;
        }

        // 처음 3개의 신호는 무시
        if ((i >= 4) && (i % 2 == 0)) {
            // 40비트 데이터 중 5바이트 읽기
            data[j / 8] <<= 1;
            if (counter > 16) {
                data[j / 8] |= 1;
            }
            j++;
        }
    }

    // 데이터 검증 및 온습도 값 계산
    if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
        *humidity = (float)((data[0] << 8) + data[1]) / 10;
        if (*humidity > 100) {
            *humidity = data[0];    // DHT11
        }
        *temperature = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;
        if (*temperature > 125) {
            *temperature = data[2];  // DHT11
        }
        if (data[2] & 0x80) {
            *temperature = -*temperature;
        }
    }
}

// 아날로그 센서 데이터 읽기 함수
int read_analog_sensor(adc1_channel_t channel) {
    int adc_value = adc1_get_raw(channel);
    return adc_value;
}

// 수위 센서 데이터 읽기 함수
int read_water_level() {
    int water_level = gpio_get_level(WATER_LEVEL_PIN);
    return water_level;
}

// MQTT 데이터 전송 함수
void mqtt_publish_data(const char *topic, const char *data) {
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, 0, 1, 0);
    ESP_LOGI("MQTT", "Sent publish successful, msg_id=%d", msg_id);
}

// 릴레이 스위치 제어 함수
void control_relay(bool state) {
    gpio_set_level(RELAY_PIN, state);
}

void app_main() {
    // Wi-Fi 및 MQTT 초기화 코드
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    } // Wi-Fi 및 MQTT 초기화
    ESP_ERROR_CHECK(ret); 
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = { // Wi-Fi 설정
        .sta = {
            .ssid = WIFI_SSID, // Wi-Fi SSID
            .password = WIFI_PASSWORD // Wi-Fi 비밀번호
        }
    };
    // MQTT 브로커 주소 설정
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());


    // 센서 및 릴레이 초기화
    esp_rom_gpio_pad_select_gpio(DHT_PIN); // DHT11 센서
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SOIL_MOISTURE_PIN, ADC_ATTEN_DB_12);
    esp_rom_gpio_pad_select_gpio(WATER_LEVEL_PIN);
    gpio_set_direction(WATER_LEVEL_PIN, GPIO_MODE_INPUT);
    esp_rom_gpio_pad_select_gpio(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        // 온습도 센서 데이터 읽기
        float temperature, humidity;
        read_dht11(&temperature, &humidity);

        // 흙 습도 센서 데이터 읽기
        int soil_moisture = read_analog_sensor(SOIL_MOISTURE_PIN);

        // 수위 센서 데이터 읽기
        int water_level = read_water_level();

        // MQTT로 데이터 전송
        char temperature_data[10], humidity_data[10], soil_moisture_data[10], water_level_data[10];
        sprintf(temperature_data, "%.2f", temperature);
        sprintf(humidity_data, "%.2f", humidity);
        sprintf(soil_moisture_data, "%d", soil_moisture);
        sprintf(water_level_data, "%d", water_level);
        mqtt_publish_data("/sensor/temperature", temperature_data);
        mqtt_publish_data("/sensor/humidity", humidity_data);
        mqtt_publish_data("/sensor/soil_moisture", soil_moisture_data);
        mqtt_publish_data("/sensor/water_level", water_level_data);

        // 릴레이 스위치 제어
        bool relay_state = (soil_moisture < 500) && (water_level > 100);
        control_relay(relay_state);

        vTaskDelay(pdMS_TO_TICKS(1000));  // 1초 간격으로 데이터 전송 및 제어
    }
}