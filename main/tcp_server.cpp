#include "tcp_server.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "roboremo.h"

static const char *TAG = "tcp_server";

#define TCP_PORT        9870    // cổng RoboRemo
#define BUF_SIZE        256     // buffer đọc dữ liệu từ socket

static TaskHandle_t s_server_task = nullptr;  // handle task TCP server
static int s_listen_fd = -1;                  // fd socket lắng nghe
static int s_client_fd = -1;                  // fd socket client (RoboRemo)

// Gửi heartbeat "alive 1\n" – nếu send lỗi thì thoát client loop
static bool send_heartbeat(int fd) {
    char buf[64];
    int len = roboremo_get_heartbeat(buf, sizeof(buf));
    if (len > 0) {
        if (send(fd, buf, len, 0) < 0) return false;
    }
    return true;
}

// Gửi status "STATUS:...\n" – nếu send lỗi thì thoát client loop
static bool send_status(int fd) {
    char buf[128];
    int len = roboremo_get_status(buf, sizeof(buf));
    if (len > 0) {
        if (send(fd, buf, len, 0) < 0) return false;
    }
    return true;
}

// Task TCP server: lắng nghe, accept, đọc lệnh RoboRemo, delegate cho roboremo module
static void tcp_server_task(void *arg) {
    struct sockaddr_in server_addr{}, client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    char rx_buf[BUF_SIZE];
    int line_pos = 0;
    TickType_t last_heartbeat = xTaskGetTickCount();
    TickType_t last_status = xTaskGetTickCount();

    // Tạo socket TCP
    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        s_listen_fd = -1;
        vTaskDelete(nullptr);
        return;
    }

    // Cho phép reuse địa chỉ (tránh lỗi bind khi restart)
    int opt = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Cấu hình địa chỉ server: 0.0.0.0:9870
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

    ESP_LOGI(TAG, "Listening on port %d", TCP_PORT);

    // Vòng lặp accept – thoát khi socket lắng nghe bị đóng
    while (true) {
        s_client_fd = accept(s_listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (s_client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        struct sockaddr_in *cli = (struct sockaddr_in *)&client_addr;
        ESP_LOGI(TAG, "Client: %s:%d",
                 inet_ntoa(cli->sin_addr), ntohs(cli->sin_port));

        line_pos = 0;
        last_heartbeat = xTaskGetTickCount();
        last_status = xTaskGetTickCount();

        // Vòng lặp xử lý từng client
        while (true) {
            // Kiểm tra watchdog trước mỗi lần select
            roboremo_tick();

            TickType_t now = xTaskGetTickCount();

            // Gửi heartbeat và status định kỳ – nếu lỗi thì ngắt kết nối
            if ((now - last_heartbeat) >= pdMS_TO_TICKS(ROBOREMO_HEARTBEAT_INTERVAL_MS)) {
                if (!send_heartbeat(s_client_fd)) break;
                last_heartbeat = now;
            }

            if ((now - last_status) >= pdMS_TO_TICKS(ROBOREMO_STATUS_INTERVAL_MS)) {
                if (!send_status(s_client_fd)) break;
                last_status = now;
            }

            // Poll socket với timeout 50ms
            fd_set readfds;
            struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 };
            FD_ZERO(&readfds);
            FD_SET(s_client_fd, &readfds);

            int sel = select(s_client_fd + 1, &readfds, nullptr, nullptr, &tv);
            if (sel < 0) break;
            if (sel == 0) continue;

            // Đọc dữ liệu, nối tiếp vào buffer (xử lý gói tin bị chia nhỏ)
            int len = recv(s_client_fd, rx_buf + line_pos, sizeof(rx_buf) - line_pos - 1, 0);
            if (len <= 0) break;

            rx_buf[line_pos + len] = '\0';

            // Tách dòng theo \n và xử lý từng lệnh
            char *p = rx_buf;
            while (*p) {
                char *nl = strchr(p, '\n');
                if (!nl) break;

                size_t cmd_len = nl - p;
                *nl = '\0';
                roboremo_process(p, cmd_len);
                p = nl + 1;
            }

            // Giữ phần dư chưa có \n cho lần đọc sau
            line_pos = (int)strlen(p);
            if (line_pos > 0 && p != rx_buf) {
                memmove(rx_buf, p, line_pos);
            }
        }

        ESP_LOGI(TAG, "Client disconnected");
        close(s_client_fd);
        s_client_fd = -1;
    }

    // Dọn dẹp khi task kết thúc
    close(s_listen_fd);
    s_listen_fd = -1;
    s_client_fd = -1;
    s_server_task = nullptr;
    vTaskDelete(nullptr);
}

// Tạo task TCP server (chỉ tạo nếu chưa chạy)
void tcp_server_start(void) {
    if (s_server_task) {
        ESP_LOGW(TAG, "Already running");
        return;
    }
    BaseType_t ret = xTaskCreate(tcp_server_task, "tcp_server", 4096, nullptr, 5, &s_server_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        s_server_task = nullptr;
    }
}

// Yêu cầu dừng: đóng socket, task sẽ tự detect lỗi và self-delete
void tcp_server_stop(void) {
    if (s_client_fd >= 0) {
        close(s_client_fd);
        s_client_fd = -1;
    }
    if (s_listen_fd >= 0) {
        close(s_listen_fd);
        s_listen_fd = -1;
    }
}
