#include "roboremo.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "roboremo";

static roboremo_callbacks_t s_cbs;           // lưu các callback điều khiển
static int64_t s_last_cmd_us = 0;            // thời điểm nhận lệnh gần nhất (µs)

// Khởi tạo: copy callback, gán mốc watchdog = hiện tại
void roboremo_init(const roboremo_callbacks_t *cbs) {
    if (cbs) s_cbs = *cbs;
    s_last_cmd_us = esp_timer_get_time();
}

// Reset watchdog – gọi mỗi khi có lệnh hợp lệ từ app
void roboremo_reset_watchdog(void) {
    s_last_cmd_us = esp_timer_get_time();
}

// Tick watchdog: nếu quá 500ms không có lệnh thì tắt toàn bộ actuator
void roboremo_tick(void) {
    if ((esp_timer_get_time() - s_last_cmd_us) > ROBOREMO_CONTACT_LOST_MS * 1000) {
        if (s_cbs.on_forward)  s_cbs.on_forward(false);
        if (s_cbs.on_backward) s_cbs.on_backward(false);
        if (s_cbs.on_left)     s_cbs.on_left(false);
        if (s_cbs.on_right)    s_cbs.on_right(false);
        if (s_cbs.on_esc)      s_cbs.on_esc(0);        // tắt motor
        if (s_cbs.on_servo1)   s_cbs.on_servo1(1500);  // lái về trung tâm
        if (s_cbs.on_servo2)   s_cbs.on_servo2(1500);  // ga về idle
    }
}

// Xử lý lệnh 1 ký tự: f/b/s/l/r/c
static void dispatch_single_char(char ch) {
    switch (ch) {
        case 'f': // Forward: tắt backward, bật forward
            if (s_cbs.on_backward) s_cbs.on_backward(false);
            if (s_cbs.on_forward)  s_cbs.on_forward(true);
            break;
        case 'b': // Backward: tắt forward, bật backward
            if (s_cbs.on_forward)  s_cbs.on_forward(false);
            if (s_cbs.on_backward) s_cbs.on_backward(true);
            break;
        case 's': // Stop: tắt cả forward + backward
            if (s_cbs.on_forward)  s_cbs.on_forward(false);
            if (s_cbs.on_backward) s_cbs.on_backward(false);
            break;
        case 'l': // Left: tắt right, bật left
            if (s_cbs.on_right) s_cbs.on_right(false);
            if (s_cbs.on_left)  s_cbs.on_left(true);
            break;
        case 'r': // Right: tắt left, bật right
            if (s_cbs.on_left)  s_cbs.on_left(false);
            if (s_cbs.on_right) s_cbs.on_right(true);
            break;
        case 'c': // Center: tắt cả left + right
            if (s_cbs.on_left)  s_cbs.on_left(false);
            if (s_cbs.on_right) s_cbs.on_right(false);
            break;
        default:
            ESP_LOGW(TAG, "Unknown char cmd: '%c'", ch);
            break;
    }
}

// Xử lý lệnh dạng "TÊN:GIÁ_TRỊ" (SERVO1, SERVO2, ESC)
static void dispatch_ext_cmd(const char *name, int value) {
    if (strcmp(name, "SERVO1") == 0) {
        if (s_cbs.on_servo1) s_cbs.on_servo1(value);
    } else if (strcmp(name, "SERVO2") == 0) {
        if (s_cbs.on_servo2) s_cbs.on_servo2(value);
    } else if (strcmp(name, "ESC") == 0) {
        if (s_cbs.on_esc) s_cbs.on_esc(value);
    } else {
        ESP_LOGW(TAG, "Unknown ext cmd: %s", name);
    }
}

// Xử lý một dòng lệnh hoàn chỉnh (đã bỏ \n)
// len = độ dài dòng (có thể != strlen nếu dòng chứa null)
void roboremo_process(const char *line, size_t len) {
    if (!line || len == 0) return;

    roboremo_reset_watchdog();

    // Lệnh 1 ký tự: f/b/s/l/r/c
    if (len == 1) {
        dispatch_single_char(line[0]);
        return;
    }

    // Lệnh dạng "TÊN:GIÁ_TRỊ" – tìm dấu ':'
    const char *colon = (const char *)memchr(line, ':', len);
    if (!colon) {
        ESP_LOGW(TAG, "Invalid cmd (no colon): %s", line);
        return;
    }

    // Tách tên lệnh (phần trước dấu :)
    size_t name_len = colon - line;
    char name[16];
    if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
    memcpy(name, line, name_len);
    name[name_len] = '\0';

    int value = atoi(colon + 1);
    dispatch_ext_cmd(name, value);
}

// Lấy chuỗi heartbeat "alive 1\n" để duy trì kết nối RoboRemo
int roboremo_get_heartbeat(char *buf, size_t size) {
    return snprintf(buf, size, "alive 1\n");
}

// Lấy chuỗi status thông qua callback (inject từ app_main)
// Nếu không có callback → trả về 0 (không gửi gì)
int roboremo_get_status(char *buf, size_t size) {
    if (s_cbs.on_get_status) {
        return s_cbs.on_get_status(buf, size);
    }
    return 0;
}
