/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "ssid.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define LOW 0
#define HIGH 1

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

#define WEB_SERVER_2 "blynk-cloud.com"
#define WEB_URL_2 "http://blynk-cloud.com/b3e42dd400e84c5586f122328b83616f/notify"

static const char *TAG = "wifi";

#define HIGH 1
#define INT_PIN GPIO_NUM_32 //Interrupt GPIO pin
#define BATTERY_EN_PIN GPIO_NUM_14 //GPIO pin to enable battery level monitor

float battery = 0.0;

static const char *POST_BODY = "{\"body\":\"Washing done. Battery: %.2f%%\"}";

static const char *POST_REQUEST = "POST /b3e42dd400e84c5586f122328b83616f/notify HTTP/1.0\r\n"
    "Host: "WEB_SERVER_2"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    "%s";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

float adc1_get_value(adc1_channel_t adc_channel) {
    float adc_values[3] = {};

    for (int i = 0; i < 3; i++) {
        vTaskDelay(10);
        adc_values[i] = adc1_get_voltage(adc_channel);
    }

    printf("ADC1: %f\n", adc_values[0]);
    printf("ADC2: %f\n", adc_values[1]);
    printf("ADC3: %f\n", adc_values[2]);

    return (adc_values[0] + adc_values[1] + adc_values[2]) / 3.0;
}

float battery_percentage() {
    //Required for battery monitoring
    adc1_config_width(ADC_WIDTH_12Bit);
    // adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);
    gpio_set_direction(BATTERY_EN_PIN, GPIO_MODE_OUTPUT);
    //We have to enable pin 14 to turn on the voltage divider
    //This saves power when we are sleeping
    gpio_set_level(BATTERY_EN_PIN, HIGH);
    float adc_value = adc1_get_value(ADC1_CHANNEL_7);
    gpio_set_level(BATTERY_EN_PIN, LOW);
    float adc_voltage = adc_value * 3.3 / 4095;
    printf("adc_voltage: %f\n", adc_voltage);
    //voltage divider uses 100k / 330k ohm resistors
    //4.3V -> 3.223, 2.4 -> 1.842
    float expected_max = 4.3*330/(100+330);
    float expected_min = 2.8*330/(100+330);
    float battery_level = (adc_voltage-expected_min)/(expected_max-expected_min);
    // float battery_voltage = adc_voltage * 2;
    return battery_level * 100.0;
}

void initialise_wifi()
{
    vTaskDelay(10000);
    battery = battery_percentage();
    printf("Got battery percentage %.2f\n", battery);

    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    printf("Starting wifi\n");
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


void http_get_task()
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int socket;

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        printf("Waiting for connection to AP\n");
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");
        printf("Connected to AP\n");

        int err = getaddrinfo(WEB_SERVER_2, "80", &hints, &res);

        if(err != 0 || res == NULL) {
            printf("DNS lookup failed\n");
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));
        printf("DNS lookup succeeded. IP=%s\n", inet_ntoa(*addr));

        socket = socket(res->ai_family, res->ai_socktype, 0);
        if(socket < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            printf("Failed to allocate\n");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket\r\n");
        printf("... allocated socket\n");

        if(connect(socket, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            printf("... socket connect failed errno\n");
            close(socket);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        printf("... connected\n");
        freeaddrinfo(res);


        char *post_body;
        asprintf(&post_body, POST_BODY, battery);

        char *post_request;
        asprintf(&post_request, POST_REQUEST, strlen(post_body), post_body);

        printf("Sending POST request:\n");
        printf(post_request);
        printf("\n");

        if (write(socket, post_request, strlen(post_request)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            printf("... socket send failed\n");
            close(socket);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");
        printf("... socket send success\n");

        /* Read HTTP response */
        char* response = NULL;
        char recv_buf[64];
        int responselen = 0;
        int r = 0;

        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(socket, recv_buf, sizeof(recv_buf)-1);
            for (int i = 0; i < 64; i++) {
              printf("%d\n", recv_buf[i]);
            }
            responselen += r;
            response = realloc(response, responselen+1);
            strcat(response, recv_buf);
        } while(r > 0);

        printf("Response length: %d\n", responselen);
        printf("Actual length: %d\n", strlen(response));
        printf("Response: %s\n", response);
        printf("Find: %d\n", strncmp("HTTP/1.1 200 OK", response, responselen));
        if (strncmp("HTTP/1.1 200 OK", response, responselen) == 0) {
            printf("Notification successful");
        }

        free(response);

        printf("\n");
        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
        printf("... done reading from socket. Last read return=%d errno=%d\n", r, errno);
        close(socket);

        // Retry
        vTaskDelay(5000);
    }
}
