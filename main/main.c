
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define SAMPLE_RATE 16000
#define BUFFER_SIZE 256

static const char *TAG = "AUDIO_PLAYER";
static i2s_chan_handle_t tx_handle;

/* Embedded WAV file */
extern const uint8_t audio_start[] asm("_binary_depends_wav_start");
extern const uint8_t audio_end[]   asm("_binary_depends_wav_end");

static uint16_t read_u16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-S3 Audio Player...");

    /* Initialize I2S */
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

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
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "I2S initialized!");

    /* Read embedded WAV file */
    const uint8_t *wav = audio_start;
    size_t wav_size = audio_end - audio_start;

    size_t offset = 12;
    size_t data_offset = 0;
    size_t data_size = 0;

    uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t sample_rate = 0;

    /* Find WAV format and audio data chunks */
    while (offset + 8 <= wav_size) {
        uint32_t chunk_size = read_u32(wav + offset + 4);

        if (memcmp(wav + offset, "fmt ", 4) == 0) {
            if (chunk_size < 16 || offset + 8 + chunk_size > wav_size) {
                ESP_LOGE(TAG, "Invalid WAV format chunk");
                return;
            }

            audio_format = read_u16(wav + offset + 8);
            channels = read_u16(wav + offset + 10);
            sample_rate = read_u32(wav + offset + 12);
            bits_per_sample = read_u16(wav + offset + 22);
        }

        if (memcmp(wav + offset, "data", 4) == 0) {
            data_offset = offset + 8;
            data_size = chunk_size;
            break;
        }

        offset += 8 + chunk_size + (chunk_size & 1);
    }

    if (audio_format != 1 || channels != 1 ||
        bits_per_sample != 16 || sample_rate != SAMPLE_RATE ||
        data_offset == 0 || data_offset + data_size > wav_size) {

        ESP_LOGE(TAG, "Unsupported or invalid WAV file");
        return;
    }

  ESP_LOGI(TAG, "Playing depends.wav...");

    /* Convert mono samples to stereo for the amplifier */
    const int16_t *audio =
        (const int16_t *)(wav + data_offset);

    size_t total_samples = data_size / sizeof(int16_t);
    int16_t buffer[BUFFER_SIZE * 2];

    size_t position = 0;

    while (position < total_samples) {
        size_t count = total_samples - position;

        if (count > BUFFER_SIZE) {
            count = BUFFER_SIZE;
        }

        for (size_t i = 0; i < count; i++) {
            buffer[i * 2] = audio[position + i];
            buffer[i * 2 + 1] = audio[position + i];
        }

        size_t bytes_written = 0;

        ESP_ERROR_CHECK(
            i2s_channel_write(
                tx_handle,
                buffer,
                count * 2 * sizeof(int16_t),
                &bytes_written,
                portMAX_DELAY
            )
        );

        position += count;
    }

    ESP_LOGI(TAG, "Audio playback finished!");
}
