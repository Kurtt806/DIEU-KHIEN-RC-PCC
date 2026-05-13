#ifndef ROBOREMO_H
#define ROBOREMO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROBOREMO_HEARTBEAT_INTERVAL_MS  500  // chu kỳ gửi alive heartbeat
#define ROBOREMO_CONTACT_LOST_MS        500  // timeout mất kết nối -> safety stop
#define ROBOREMO_STATUS_INTERVAL_MS     100  // chu kỳ gửi status (RSSI...)

// Callback cho các actuator dạng analog (servo, ESC)
typedef void (*roboremo_actuator_cb_t)(int value);

// Callback cho các actuator dạng digital (forward/backward/left/right)
typedef void (*roboremo_digital_cb_t)(bool active);

// Callback cho việc lấy dữ liệu status (inject từ app_main, giúp roboremo không phụ thuộc module khác)
typedef int (*roboremo_status_cb_t)(char *buf, size_t size);

// Cấu trúc chứa tất cả callback cho roboremo
typedef struct {
    roboremo_actuator_cb_t on_servo1;   // SERVO1: lái (PWM 1000-2000us)
    roboremo_actuator_cb_t on_servo2;   // SERVO2: ga (PWM 1000-2000us)
    roboremo_actuator_cb_t on_esc;      // ESC: motor (0-255)
    roboremo_digital_cb_t  on_forward;  // f: tiến
    roboremo_digital_cb_t  on_backward; // b: lùi
    roboremo_digital_cb_t  on_left;     // l: rẽ trái
    roboremo_digital_cb_t  on_right;    // r: rẽ phải
    roboremo_status_cb_t   on_get_status; // callback inject dữ liệu status (NULL = không gửi)
} roboremo_callbacks_t;

// Khởi tạo module roboremo với các callback điều khiển
void roboremo_init(const roboremo_callbacks_t *cbs);

// Xử lý một dòng lệnh nhận từ app (bỏ \n, null-terminated)
void roboremo_process(const char *line, size_t len);

// Lấy chuỗi heartbeat "alive 1\n" để gửi xuống app
int  roboremo_get_heartbeat(char *buf, size_t size);

// Lấy chuỗi status "STATUS:...\n" để gửi xuống app
int  roboremo_get_status(char *buf, size_t size);

// Tick kiểm tra watchdog – nếu mất tín hiệu >500ms thì tự động dừng xe
void roboremo_tick(void);

// Reset watchdog mỗi khi nhận được lệnh hợp lệ từ app
void roboremo_reset_watchdog(void);

#ifdef __cplusplus
}
#endif

#endif
