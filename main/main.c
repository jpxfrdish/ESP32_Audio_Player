
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define SAMPLE_RATE 16000
#define TONE_FREQ   440
#define AMPLITUDE   2500
#define BUFFER_SIZE 256

static const char *TAG = "AUDIO_PLAYER";
static i2s_chan_handle_t tx_handle;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-S3 Audio Player...");

    // Configure I2S channel
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    // Configure I2S standard mode
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_15,
            .ws   = GPIO_NUM_16,
            .dout = GPIO_NUM_17,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(
        i2s_channel_init_std_mode(tx_handle, &std_cfg)
    );

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "I2S initialized successfully!");
    ESP_LOGI(TAG, "Playing 440 Hz test tone...");

    int16_t samples[BUFFER_SIZE * 2];
    float phase = 0.0f;
    const float phase_step =
        2.0f * 3.14159265f * TONE_FREQ / SAMPLE_RATE;

    while (1) {
        // Generate a quiet stereo sine wave
        for (int i = 0; i < BUFFER_SIZE; i++) {
            int16_t sample = (int16_t)(AMPLITUDE * sinf(phase));

            samples[i * 2] = sample;
            samples[i * 2 + 1] = sample;

            phase += phase_step;

            if (phase >= 2.0f * 3.14159265f) {
                phase -= 2.0f * 3.14159265f;
            }
        }

        size_t bytes_written = 0;

        ESP_ERROR_CHECK(
            i2s_channel_write(
                tx_handle,
                samples,
                sizeof(samples),
                &bytes_written,
                portMAX_DELAY
            )
        );
    }
}
