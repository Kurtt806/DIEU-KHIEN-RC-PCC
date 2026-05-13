#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "tcp_server.h"

#define BOOT_BUTTON_GPIO    GPIO_NUM_0
#define LONG_PRESS_MS       3000

static const char *TAG = "rc_car";

static void on_wifi_event(WifiEvent event) {
    switch (event) {
        case WifiEvent::Connected:
            ESP_LOGI(TAG, "WiFi connected! IP: %s",
                     WifiManager::GetInstance().GetIpAddress().c_str());
            tcp_server_start();
            break;
        case WifiEvent::Disconnected:
            ESP_LOGI(TAG, "WiFi disconnected");
            tcp_server_stop();
            break;
        case WifiEvent::ConfigModeEnter:
            ESP_LOGI(TAG, "Config AP mode started. Connect to RC_CAR-XXXX");
            break;
        default:
            break;
    }
}

static void boot_button_task(void *arg) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    TickType_t press_start = 0;
    bool was_pressed = false;

    while (1) {
        bool pressed = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);

        if (pressed && !was_pressed) {
            press_start = xTaskGetTickCount();
        }

        if (pressed && was_pressed) {
            TickType_t now = xTaskGetTickCount();
            if ((now - press_start) >= pdMS_TO_TICKS(LONG_PRESS_MS)) {
                ESP_LOGI(TAG, "BOOT long press -> Config AP");
                WifiManager::GetInstance().StartConfigAp();
                while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
        }

        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "RC Car starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS: erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS: initialized");

    xTaskCreate(boot_button_task, "btn_mon", 2048, NULL, 5, NULL);

    auto &wifi = WifiManager::GetInstance();

    WifiManagerConfig config;
    config.ssid_prefix = "RC_CAR";
    config.language = "vi-VN";

    wifi.SetEventCallback(on_wifi_event);
    wifi.Initialize(config);
    wifi.StartStation();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
