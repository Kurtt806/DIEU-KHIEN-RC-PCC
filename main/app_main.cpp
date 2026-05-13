#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "wifi_manager.h"
#include "tcp_server.h"

#define BOOT_BUTTON_GPIO    0
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

static void boot_button_cb(void *button_handle, void *usr_data) {
    ESP_LOGI(TAG, "BOOT long press -> Config AP");
    WifiManager::GetInstance().StartConfigAp();
}

static void init_boot_button(void) {
    button_config_t btn_cfg = {
        .long_press_time = LONG_PRESS_MS,
        .short_press_time = 200,
    };

    button_gpio_config_t gpio_cfg = {
        .gpio_num = BOOT_BUTTON_GPIO,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };

    button_handle_t btn_handle = NULL;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle));
    ESP_ERROR_CHECK(iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, NULL, boot_button_cb, NULL));
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

    init_boot_button();

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
