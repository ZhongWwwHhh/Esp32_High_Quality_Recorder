/* From: https://github.com/espressif/esp-idf/tree/v4.4.2/examples/peripherals/i2s/i2s_audio_recorder_sdcard */
/* This is the original statement */
/* I2S Digital Microphone Recording Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* Esp32_Mic
PCB design at: https://github.com/ZhongWwwHhh/Esp32_High_Quality_Recorder_PCB
support esp32 (esp32s2 and esp32s3 might can be supported, but I don't have those devices)
require esp-idf v4.4.2
firmware version: v2.0-beta.0
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"
#include "esp_timer.h"
#include "driver/adc.h"
#include "esp_sleep.h"

static const char *TAG = "Esp32_Mic";

// mode with battery
#define BATTERY_MODE

// GPIO for leds
#define STATE_LED1 CONFIG_STATE_LED1_IO
#define STATE_LED2 CONFIG_STATE_LED2_IO

// setting of i2s
#define SAMPLE_SIZE (CONFIG_I2S_BIT_SAMPLE * 1024)

// setting of adc for battery level
#define NO_OF_SAMPLES 3                             // Multisampling, set less to reduce occupancy, may be more inaccurate
static const adc_channel_t channel = ADC_CHANNEL_6; // GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;

// esp timer for battery level
static void check_battery_level(void *args);

// setting of sdmmc
#define SD_MOUNT_POINT "/sdcard"
sdmmc_card_t *card;

static int16_t i2s_readraw_buff[SAMPLE_SIZE];
size_t bytes_read;
const int WAVE_HEADER_SIZE = 44;

void mount_sdcard(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // sdmmc_card_t *card;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Set bus width to use:
    slot_config.width = 1;

    ESP_LOGI(TAG, "Mounting filesystem");

    ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    while (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        usleep(1000000);
        ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    }
    ESP_LOGI(TAG, "Filesystem mounted");
}

void record_wav(void)
{
    // Use POSIX and C standard library functions to work with files.
    ESP_LOGI(TAG, "Detect file");

    // detect files
    struct stat st;
    char filename_suffix[10];
    char filename_hole[100];
    for (short i = 1; i < 100; i++)
    {
        strcpy(filename_hole, SD_MOUNT_POINT);
        strcpy(filename_suffix, "");
        sprintf(filename_suffix, "%d", i);
        strcat(filename_hole, "/");
        strcat(filename_hole, CONFIG_FILENAME_PREFIX);
        strcat(filename_hole, filename_suffix);
        strcat(filename_hole, ".raw");
        if (stat(filename_hole, &st) != 0)
        {
            break;
        };
    }

    if (stat(filename_hole, &st) == 0)
    {
        ESP_LOGW(TAG, "Can't find a free filename. Please clean the SD card");
        gpio_set_level(STATE_LED1, 0);
        while (true)
        {
            gpio_set_level(STATE_LED2, 1);
            usleep(250000);
            gpio_set_level(STATE_LED2, 0);
            usleep(250000);
        }
    }

    // Create new WAV file
    ESP_LOGI(TAG, "Open %s", filename_hole);
    FILE *f = fopen(filename_hole, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file");
    }
    usleep(500000);

    gpio_set_level(STATE_LED1, 0);
    gpio_set_level(STATE_LED2, 1);

    // Start recording
    ESP_LOGI(TAG, "Recording");
    char led2_state = 1;
    char unsaved_time = 0; // if it is not enough, set it as short or int
    while (1)
    {
        unsaved_time = 0;
        while (unsaved_time <= 25)
        {
            // Read the RAW samples from the microphone
            i2s_read(CONFIG_I2S_CH, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 100);
            // Write the samples to the WAV file
            fwrite(i2s_readraw_buff, 1, bytes_read, f);
            unsaved_time++;
        }
        fflush(f);
        fsync(fileno(f));
        led2_state = !led2_state;
        gpio_set_level(STATE_LED2, led2_state); // blink led2 to identify running
    }
}

void init_microphone(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = CONFIG_I2S_SAMPLE_RATE,
        .bits_per_sample = CONFIG_I2S_BIT_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = true,
    };

    // Set the pinout configuration
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = CONFIG_I2S_BCK_IO,
        .ws_io_num = CONFIG_I2S_WS_IO,
        .data_out_num = CONFIG_I2S_DO_IO,
        .data_in_num = CONFIG_I2S_DI_IO,
    };

    // Call driver installation function before any I2S R/W operation.
    ESP_ERROR_CHECK(i2s_driver_install(CONFIG_I2S_CH, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(CONFIG_I2S_CH, &pin_config));
}

static void check_battery_level(void *args)
{
    ESP_LOGI(TAG, "Detect battery level");

    // Read and Multisampling
    uint32_t adc_reading = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
        adc_reading += adc1_get_raw((adc1_channel_t)channel);
    }
    adc_reading /= NO_OF_SAMPLES;
    ESP_LOGI(TAG, "Battery level is: %d", adc_reading);

#ifdef BATTERY_MODE
    if (adc_reading <= 2100)
    {
        ESP_LOGI(TAG, "Battery level low. Power off. Please charge");
        usleep(1000000);
        esp_deep_sleep_start();
        return;
    }
    else
    {
        ESP_LOGI(TAG, "Battery level is enough");
        return;
    }
#endif
}

void init_battery_adc(void)
{
    ESP_LOGI(TAG, "Init adc for battrey level");
    // Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);

    // Check first
    check_battery_level(NULL);

    ESP_LOGI(TAG, "Create timer to detect battery level every 6 mins");
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &check_battery_level,
        /* name is optional, but may help identify the timer when debugging */
        .name = "adc_timer",
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 600000000));
}

static void configure_led(void)
{
    gpio_reset_pin(STATE_LED1);
    gpio_reset_pin(STATE_LED2);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(STATE_LED1, GPIO_MODE_OUTPUT);
    gpio_set_direction(STATE_LED2, GPIO_MODE_OUTPUT);

    /* light led1 */
    gpio_set_level(STATE_LED1, 1);
    gpio_set_level(STATE_LED2, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Esp32 Mic Starting");
    // init led
    configure_led();

    // Init adc of battery level
    init_battery_adc();

    // Mount the SDCard for recording the audio file
    mount_sdcard();

    // Init the PDM digital microphone
    init_microphone();

    ESP_LOGI(TAG, "Starting recording");
    // Start Recording
    record_wav();
}
