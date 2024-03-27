/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "driver/gpio.h"
#include "sdcard.h"
#include "board.h"

static const char *TAG = "SDCARD";
int g_gpio = -1;
// ThucND
#ifdef CONFIG_SD_CARD_USING
// This define Is used in assistant speaker
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  2
#define PIN_NUM_CS   14
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN    1

#endif //SPI_DMA_CHAN
#else
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   2
#endif
#define MAX_FILE_TO_OPEN  5
//ThucND END
static void sdmmc_card_print_info(const sdmmc_card_t *card)
{
    ESP_LOGD(TAG, "Name: %s\n", card->cid.name);
    ESP_LOGD(TAG, "Type: %s\n", (card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGD(TAG, "Speed: %s\n", (card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
    ESP_LOGD(TAG, "Size: %lluMB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    ESP_LOGD(TAG, "CSD: ver=%d, sector_size=%d, capacity=%d read_bl_len=%d\n",
             card->csd.csd_ver,
             card->csd.sector_size, card->csd.capacity, card->csd.read_block_len);
    ESP_LOGD(TAG, "SCR: sd_spec=%d, bus_width=%d\n", card->scr.sd_spec, card->scr.bus_width);
}

int8_t get_sdcard_open_file_num_max(void)
{
    return MAX_FILE_TO_OPEN;
}

esp_err_t sdcard_mount(const char *base_path, periph_sdcard_mode_t mode)
{
    if (mode >= SD_MODE_MAX) {
        ESP_LOGE(TAG, "PLease select the correct sd mode: 1-line SD mode, 4-line SD mode or SPI mode!, current mode is %d", mode);
        return ESP_FAIL;
    }
    esp_err_t ret;
//ThucND begin

#ifndef CONFIG_SD_CARD_USING
    
    sdmmc_card_t *card = NULL;
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = get_sdcard_open_file_num_max()
    };

    if (mode != SD_MODE_SPI) {
        ESP_LOGI(TAG, "Using 1-line SD mode, 4-line SD mode,  base path=%s", base_path);
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.gpio_cd = g_gpio;
        slot_config.width = mode & 0X01;

        gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);  
        gpio_set_pull_mode(GPIO_NUM_2,  GPIO_PULLUP_ONLY);   
        gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);

        if (mode == SD_MODE_4_LINE) {
            gpio_set_pull_mode(GPIO_NUM_4,  GPIO_PULLUP_ONLY);
            gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY);
        }

        ret = esp_vfs_fat_sdmmc_mount(base_path, &host, &slot_config, &mount_config, &card);
    } else {
        ESP_LOGI(TAG, "Using SPI mode, base path=%s", base_path);
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
        slot_config.gpio_miso = PIN_NUM_MISO;
        slot_config.gpio_mosi = PIN_NUM_MOSI;
        slot_config.gpio_sck  = PIN_NUM_CLK;
        slot_config.gpio_cs   = PIN_NUM_CS;

        ret = esp_vfs_fat_sdmmc_mount(base_path, &host, &slot_config, &mount_config, &card);
    }

#else
    
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    ESP_LOGI(TAG, "Initializing SD card");
    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    if (mode != SD_MODE_SPI) {
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;

    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    }
    else {

    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    //host.max_freq_khz = 1000; //do not use 4000 because it will return 0x109 err
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK && (ret != ESP_ERR_INVALID_STATE)) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    gpio_config_t io_config = {};
    //out put no pull
    // io_config.pin_bit_mask = GPIO_OUTPUT_SEL;
    // io_config.mode = GPIO_MODE_OUTPUT;
    // io_config.intr_type = GPIO_INTR_DISABLE;
    // io_config.pull_down_en = 0;
    // io_config.pull_up_en = 1;
    // gpio_config (&io_config);
    // io_config.pin_bit_mask = GPIO_INPUT_SEL;
    // io_config.mode = GPIO_MODE_INPUT;
    // io_config.intr_type = GPIO_INTR_DISABLE;
    // io_config.pull_down_en = 0;
    // io_config.pull_up_en = 0;
    // gpio_config (&io_config);
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(card);
    }
#endif
//ThucND end
    switch (ret) {
        case ESP_OK:
            // Card has been initialized, print its properties
            sdmmc_card_print_info(card);
            ESP_LOGI(TAG, "CID name %s!\n", card->cid.name);
            break;

        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(TAG, "File system already mounted");
            break;

        case ESP_FAIL:
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
            break;

        default:
            ESP_LOGE(TAG, "Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.", ret);
            break;
    }

    return ret;

}

esp_err_t sdcard_unmount(void)
{
    esp_err_t ret = esp_vfs_fat_sdmmc_unmount();

    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "File system not mounted");
    }
    return ret;
}

bool sdcard_is_exist()
{
    if (g_gpio >= 0) {
        //ESP_LOGI (TAG, "gpio get lv %d, %d\r\n", g_gpio, gpio_get_level(g_gpio));
        return (gpio_get_level(g_gpio) == 0x00);
    } else {
        return true;
    }
    return false;
}

int IRAM_ATTR sdcard_read_detect_pin(void)
{
    if (g_gpio >= 0) {
        return gpio_get_level(g_gpio);
    } else {
        return -1;
    }
    return 0;
}

esp_err_t sdcard_destroy()
{
    if (g_gpio >= 0) {
        return gpio_isr_handler_remove(g_gpio);
    }
    return ESP_OK;
}

esp_err_t sdcard_init(int card_detect_pin, void (*detect_intr_handler)(void *), void *isr_context)
{
    esp_err_t ret = ESP_OK;
    if (card_detect_pin >= 0) {
        // gpio_set_direction(card_detect_pin, GPIO_MODE_INPUT);
        // if (detect_intr_handler) {
        //     gpio_set_intr_type(card_detect_pin, GPIO_INTR_ANYEDGE);
        //     gpio_isr_handler_add(card_detect_pin, detect_intr_handler, isr_context);
        //     gpio_intr_enable(card_detect_pin);
        // }
        // gpio_pullup_en(card_detect_pin);
    }
    g_gpio = card_detect_pin;
    return ret;
}
