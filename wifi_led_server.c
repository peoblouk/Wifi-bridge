// wifi_led_server.c

#include "wifi_led_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include <string.h>

#define WIFI_SSID      "ESP32-RoboticArm"
#define WIFI_PASS      "RoboticArm123"
#define MAX_STA_CONN   4
#define LED_GPIO       GPIO_NUM_38

static const char *TAG = "wifi_led_server";

static const char *html_page =
    "<html><head><title>ESP32 RoboticArm</title></head>"
    "<body><h1>ESP32 LED Control</h1>"
    "<form action=\"/led\" method=\"get\">"
    "<button name=\"state\" value=\"on\" type=\"submit\">LED ON</button>"
    "<button name=\"state\" value=\"off\" type=\"submit\">LED OFF</button>"
    "</form></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t led_get_handler(httpd_req_t *req)
{
    char param[32];
    if (httpd_req_get_url_query_str(req, param, sizeof(param)) == ESP_OK) {
        char value[8];
        if (httpd_query_key_value(param, "state", value, sizeof(value)) == ESP_OK) {
            if (strcmp(value, "on") == 0) {
                gpio_set_level(LED_GPIO, 1);
            } else if (strcmp(value, "off") == 0) {
                gpio_set_level(LED_GPIO, 0);
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

        httpd_uri_t led = {
            .uri       = "/led",
            .method    = HTTP_GET,
            .handler   = led_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &led);
    }

    return server;
}

static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
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

    ESP_LOGI(TAG, "WiFi AP vytvořen: SSID: %s, heslo: %s", WIFI_SSID, WIFI_PASS);
}

void wifi_led_server_start(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    wifi_init_softap();
    start_webserver();
}