#include "tcp_server.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "wifi_manager.h"

static const char *TAG = "tcp_server";

#define TCP_PORT        9870
#define BUF_SIZE        256
#define TX_INTERVAL_MS  100

static TaskHandle_t s_server_task = nullptr;
static int s_listen_fd = -1;
static int s_client_fd = -1;

struct roboremo_cmd {
    char cmd[16];
    int value;
};

static bool parse_roboremo_cmd(const char *line, roboremo_cmd *out) {
    const char *colon = strchr(line, ':');
    if (!colon) return false;

    size_t cmd_len = colon - line;
    if (cmd_len >= sizeof(out->cmd)) return false;

    strncpy(out->cmd, line, cmd_len);
    out->cmd[cmd_len] = '\0';
    out->value = atoi(colon + 1);
    return true;
}

static void handle_command(const roboremo_cmd *cmd) {
    if (strcmp(cmd->cmd, "SERVO1") == 0) {
        ESP_LOGI(TAG, "Servo 1 (steering): %d us", cmd->value);
    } else if (strcmp(cmd->cmd, "SERVO2") == 0) {
        ESP_LOGI(TAG, "Servo 2 (throttle): %d us", cmd->value);
    } else if (strcmp(cmd->cmd, "ESC") == 0) {
        ESP_LOGI(TAG, "ESC motor: %d", cmd->value);
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd->cmd);
    }
}

static void send_status(int fd) {
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "STATUS:rssi=%d\n",
                       WifiManager::GetInstance().GetRssi());
    send(fd, buf, len, 0);
}

static void tcp_server_task(void *arg) {
    struct sockaddr_in server_addr{}, client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    char rx_buf[BUF_SIZE];
    int line_pos = 0;
    TickType_t last_tx = xTaskGetTickCount();

    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        s_listen_fd = -1;
        vTaskDelete(nullptr);
        return;
    }

    int opt = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Bind failed: errno %d", errno);
        close(s_listen_fd);
        s_listen_fd = -1;
        vTaskDelete(nullptr);
        return;
    }

    if (listen(s_listen_fd, 1) < 0) {
        ESP_LOGE(TAG, "Listen failed: errno %d", errno);
        close(s_listen_fd);
        s_listen_fd = -1;
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (true) {
        s_client_fd = accept(s_listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (s_client_fd < 0) {
            if (errno == EINTR) continue;
            ESP_LOGE(TAG, "Accept failed: errno %d", errno);
            break;
        }

        struct sockaddr_in *cli = (struct sockaddr_in *)&client_addr;
        ESP_LOGI(TAG, "Client connected: %s:%d",
                 inet_ntoa(cli->sin_addr), ntohs(cli->sin_port));

        line_pos = 0;
        last_tx = xTaskGetTickCount();

        while (true) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_tx) >= pdMS_TO_TICKS(TX_INTERVAL_MS)) {
                send_status(s_client_fd);
                last_tx = now;
            }

            fd_set readfds;
            struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 };
            FD_ZERO(&readfds);
            FD_SET(s_client_fd, &readfds);

            int sel = select(s_client_fd + 1, &readfds, nullptr, nullptr, &tv);
            if (sel < 0) break;
            if (sel == 0) continue;

            int len = recv(s_client_fd, rx_buf + line_pos, sizeof(rx_buf) - line_pos - 1, 0);
            if (len <= 0) break;

            rx_buf[line_pos + len] = '\0';
            char *p = rx_buf;
            while (*p) {
                char *nl = strchr(p, '\n');
                if (!nl) break;

                *nl = '\0';
                roboremo_cmd cmd;
                if (parse_roboremo_cmd(p, &cmd)) {
                    handle_command(&cmd);
                } else {
                    ESP_LOGW(TAG, "Invalid command: %s", p);
                }
                p = nl + 1;
            }

            line_pos = (int)strlen(p);
            if (line_pos > 0 && p != rx_buf) {
                memmove(rx_buf, p, line_pos);
            }
        }

        ESP_LOGI(TAG, "Client disconnected");
        close(s_client_fd);
        s_client_fd = -1;
    }

    close(s_listen_fd);
    s_listen_fd = -1;
    s_client_fd = -1;
    s_server_task = nullptr;
    vTaskDelete(nullptr);
}

void tcp_server_start(void) {
    if (s_server_task) {
        ESP_LOGW(TAG, "TCP server already running");
        return;
    }
    xTaskCreate(tcp_server_task, "tcp_server", 4096, nullptr, 5, &s_server_task);
    ESP_LOGI(TAG, "TCP server task created");
}

void tcp_server_stop(void) {
    if (s_client_fd >= 0) {
        close(s_client_fd);
        s_client_fd = -1;
    }
    if (s_listen_fd >= 0) {
        close(s_listen_fd);
        s_listen_fd = -1;
    }
    if (s_server_task) {
        vTaskDelete(s_server_task);
        s_server_task = nullptr;
    }
    ESP_LOGI(TAG, "TCP server stopped");
}
