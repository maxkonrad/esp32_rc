#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/rmt_tx.h"
#include "driver/gpio.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "nvs_flash.h"

// --------------------------------------------------
static const char *TAG = "SONY_IR";

// ---------------- CONFIG ----------------
#define RMT_TX_PIN GPIO_NUM_18
#define WIFI_SSID  "****"
#define WIFI_PASS  "****"

// ---------------- RMT HANDLES ----------------
typedef struct {
    rmt_channel_handle_t tx_chan;
    rmt_encoder_handle_t encoder;
} rmt_handles_t;

static rmt_handles_t g_rmt = {0};
static QueueHandle_t ir_queue;

// --------------------------------------------------
// SONY SIRC PACKET BUILDER
// --------------------------------------------------
static int sony_build_packet(uint16_t data, rmt_symbol_word_t *buf)
{
    int i = 0;

    // Header: 2400us ON, 600us OFF
    buf[i++] = (rmt_symbol_word_t){.duration0 = 2400, .level0 = 1, .duration1 = 600, .level1 = 0};

    // 15 bit LSB first
    for (int b = 0; b < 15; b++) {
        if (data & (1 << b))
            buf[i++] = (rmt_symbol_word_t){.duration0 = 1200, .level0 = 1, .duration1 = 600, .level1 = 0};
        else
            buf[i++] = (rmt_symbol_word_t){.duration0 = 600, .level0 = 1, .duration1 = 600, .level1 = 0};
    }
    return i;
}

// --------------------------------------------------
// IR TASK
// --------------------------------------------------
static void ir_task(void *arg)
{
    uint16_t cmd;
    rmt_symbol_word_t packet[16];

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags.eot_level = 0,
    };

    while (1) {
        if (xQueueReceive(ir_queue, &cmd, portMAX_DELAY)) {

            int len = sony_build_packet(cmd, packet);

            rmt_enable(g_rmt.tx_chan);

            for (int i = 0; i < 3; i++) {
                rmt_transmit(
                    g_rmt.tx_chan,
                    g_rmt.encoder,
                    packet,
                    len * sizeof(rmt_symbol_word_t),
                    &tx_cfg
                );
                rmt_tx_wait_all_done(g_rmt.tx_chan, portMAX_DELAY);
                vTaskDelay(pdMS_TO_TICKS(45));
            }

            rmt_disable(g_rmt.tx_chan);
        }
    }
}

// --------------------------------------------------
// HTTP HANDLER
// --------------------------------------------------
static esp_err_t ir_http_handler(httpd_req_t *req)
{
    uint16_t cmd = (uint16_t)(uintptr_t)req->user_ctx;
    ESP_LOGI(TAG, "Request received for command: 0x%04X", cmd);
    xQueueSend(ir_queue, &cmd, 0);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// --------------------------------------------------
// RMT INIT
// --------------------------------------------------
static void init_rmt_tx(void)
{
    rmt_tx_channel_config_t tx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RMT_TX_PIN,
        .resolution_hz = 1000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &g_rmt.tx_chan));

    rmt_carrier_config_t carrier = {
        .frequency_hz = 40000,
        .duty_cycle = 0.33,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(g_rmt.tx_chan, &carrier));

    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&enc_cfg, &g_rmt.encoder));
}

// --------------------------------------------------
// WIFI
// --------------------------------------------------
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &wifi_event_handler, NULL);

    wifi_config_t wifi = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi);
    esp_wifi_start();

    esp_wifi_set_max_tx_power(40); // ~10 dBm
}

// --------------------------------------------------
// HTTP SERVER
// --------------------------------------------------
static httpd_handle_t start_http(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t srv;
    httpd_start(&srv, &cfg);

#define URI(u, c) { .uri=u, .method=HTTP_GET, .handler=ir_http_handler, .user_ctx=(void*)(uintptr_t)c }

    httpd_uri_t uris[] = {
        URI("/power",       0x2215),
        URI("/vol_up",      0x2212),
        URI("/vol_down",    0x2213),
        URI("/play",        0x3232),
        URI("/pause",       0x3239),
        URI("/stop",        0x3238),
        URI("/ff",          0x3231),
        URI("/rew",         0x3230),
    };

    for (int i = 0; i < sizeof(uris)/sizeof(uris[0]); i++)
        httpd_register_uri_handler(srv, &uris[i]);

    return srv;
}

// --------------------------------------------------
// APP MAIN
// --------------------------------------------------
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_rmt_tx();

    ir_queue = xQueueCreate(8, sizeof(uint16_t));
    xTaskCreatePinnedToCore(ir_task, "ir_task", 4096, NULL, 10, NULL, 1);

    wifi_init_sta();
    start_http();

    ESP_LOGI(TAG, "System ready");
}
