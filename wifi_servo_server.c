// wifi_servo_server.c

#include "wifi_led_server.h" //! změnit jmeno

//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
//#include "sdkconfig.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/ledc.h"
//#include "driver/gpio.h"
//#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

#define WIFI_SSID      "ESP32-RoboticArm"
#define WIFI_PASS      "RoboticArm123"
#define MAX_STA_CONN   4                      // max stable connections

#define SERVO_GPIO         GPIO_NUM_18        // output pin for servo
#define SERVO_PWM_FREQ     50                 // 50 Hz for servo
#define SERVO_PWM_RES      LEDC_TIMER_16_BIT  // 16bitové rozlišení
#define SERVO_PWM_CHANNEL  LEDC_CHANNEL_0

static const char *TAG = "wifi_servo_server";

// HTML file for compilator
extern const uint8_t spage_html_start[] asm("_binary_spage_html_start");
extern const uint8_t spage_html_end[]   asm("_binary_spage_html_end");

static esp_err_t root_get_handler(httpd_req_t *req) // Paste HTML file to website
{
    size_t html_len = spage_html_end - spage_html_start;
    httpd_resp_send(req, (const char *)spage_html_start, html_len);
    return ESP_OK;
}

static esp_err_t servo_get_handler(httpd_req_t *req) // Servo set angle from website
{
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char angle_str[8];
        if (httpd_query_key_value(query, "angle", angle_str, sizeof(angle_str)) == ESP_OK) {
            int angle = atoi(angle_str);
            if (angle >= 0 && angle <= 180) {
                // přepočet úhlu na duty cycle
                uint32_t duty_min = (uint32_t)(0.05 * 65535);  // ~1 ms
                uint32_t duty_max = (uint32_t)(0.10 * 65535);  // ~2 ms
                uint32_t duty = duty_min + ((duty_max - duty_min) * angle) / 180;

                ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL, duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL);

                ESP_LOGI(TAG, "Servo set to angle: %d° (duty: %u)", angle, duty);
            } else {
                ESP_LOGW(TAG, "Angle out of range: %d", angle);
            }
        }
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t servo = {
            .uri       = "/servo",
            .method    = HTTP_GET,
            .handler   = servo_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &servo);
    }

    return server;
}

static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // AP set
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP was created: SSID: %s, pass: %s", WIFI_SSID, WIFI_PASS);
}

void wifi_led_server_start(void)
{
    // Inicializace PWM pro servo
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = SERVO_PWM_RES,
        .freq_hz = SERVO_PWM_FREQ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = SERVO_PWM_CHANNEL,
        .duty       = 0,
        .gpio_num   = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);

    wifi_init_softap();
    start_webserver();
    ESP_LOGI(TAG, "HTML size: %d b", spage_html_end - spage_html_start);
}
