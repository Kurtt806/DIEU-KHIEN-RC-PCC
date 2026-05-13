#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "tcp_server.h"

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

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "RC Car starting...");

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
