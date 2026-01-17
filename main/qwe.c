#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"

#define IR_GPIO     18
#define BUTTON_GPIO 0

#define SIRC_HDR_US   2400
#define SIRC_1_US     1200
#define SIRC_0_US     600
#define SIRC_GAP_US   600

static rmt_channel_handle_t rmt_tx;
static rmt_encoder_handle_t copy_enc;

static void send_sirc12(uint8_t address, uint8_t command)
{
    rmt_symbol_word_t symbols[1 + 12 * 2];
    int i = 0;

    symbols[i++] = (rmt_symbol_word_t){
        .level0 = 1, .duration0 = SIRC_HDR_US,
        .level1 = 0, .duration1 = SIRC_GAP_US
    };

    uint16_t frame = (address << 7) | command;

    for (int b = 0; b < 12; b++) {
        uint32_t on_time = (frame & (1 << b)) ? SIRC_1_US : SIRC_0_US;
        symbols[i++] = (rmt_symbol_word_t){
            .level0 = 1, .duration0 = on_time,
            .level1 = 0, .duration1 = SIRC_GAP_US
        };
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0
    };

    rmt_transmit(rmt_tx, copy_enc, symbols,
                 sizeof(symbols), &tx_cfg);
}

void app_main(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&btn);

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = IR_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    rmt_new_tx_channel(&tx_cfg, &rmt_tx);

    rmt_carrier_config_t carrier = {
        .frequency_hz = 40000,
        .duty_cycle = 0.33f,
    };
    rmt_apply_carrier(rmt_tx, &carrier);

    rmt_copy_encoder_config_t enc_cfg = {};
    rmt_new_copy_encoder(&enc_cfg, &copy_enc);

    rmt_enable(rmt_tx);

    while (1) {
        if (gpio_get_level(BUTTON_GPIO) == 0) {
            send_sirc12(0x04, 0x15);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
