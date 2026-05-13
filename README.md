# Hệ Thống Điều Khiển Xe RC Từ Xa — ESP32-S3 + RoboRemo

Hệ thống nhúng ESP32-S3 (ESP-IDF) kết nối WiFi TCP với app RoboRemo để điều khiển
xe RC xăng/điện: 2 servo (ga, lái), ESC motor điện, cảm biến và phản hồi realtime.

---

## Cấu Trúc Thư Mục Dự Kiến

```
DIEU-KHIEN-RC-PCC/
├── main/
│   ├── CMakeLists.txt
│   ├── app_main.c                 # Khởi tạo và vòng lặp chính
│   ├── wifi/
│   │   └── wifi_manager.c         # AP mode + Station mode
│   ├── tcp_server/
│   │   └── tcp_server.c           # TCP socket server
│   ├── protocol/
│   │   └── roboremol_parser.c     # Parse lệnh từ RoboRemo
│   ├── servo/
│   │   └── servo_control.c        # PWM điều khiển servo lái + ga
│   ├── esc/
│   │   └── esc_control.c          # PWM điều khiển ESC motor
│   ├── encoder/
│   │   └── encoder_reader.c       # Đọc encoder (PCNT)
│   ├── sensors/
│   │   ├── distance_sensor.c      # Cảm biến khoảng cách
│   │   └── current_sensor.c       # Cảm biến dòng
│   └── feedback/
│       └── status_reporter.c      # Gửi dữ liệu realtime về app
├── components/                    # Thư viện dùng chung
├── CMakeLists.txt
├── sdkconfig
└── README.md
```

---

## Giao Thức RoboRemo

| Chiều | Định dạng | Ví dụ |
|-------|-----------|-------|
| App → ESP | `CMD:VALUE` | `SERVO1:1500` servo lái, `SERVO2:1000` servo ga, `ESC:180` tốc độ motor |
| ESP → App | `STATUS:k1=v1,k2=v2` | `STATUS:dist=45,speed=1200,current=2.5` |

---

## Bản Đồ Phát Triển

```
PHASE 0: NỀN TẢNG KẾT NỐI
│
├── WiFi TCP Server
│   ├── ESP32 chế độ AP (RoboRemo kết nối trực tiếp)
│   └── TCP socket lắng nghe lệnh
│
├── Giao thức RoboRemo
│   ├── Parse SERVO:<pwm_us> — servo lái (0°–180°)
│   ├── Parse SERVO2:<pwm_us> — servo ga (idle–full)
│   └── Parse ESC:<pwm_us> — ESC motor điện
│
└── Điều khiển cơ bản
    ├── Servo 1 (lái) — MCPWM
    ├── Servo 2 (ga) — MCPWM
    └── ESC (motor điện) — MCPWM
────────────────────────────────────────────────────

PHASE 1: CẢM BIẾN & REALTIME FEEDBACK
│
├── Encoder tốc độ
│   ├── PCNT module đọc xung encoder
│   └── Tính RPM / tốc độ tuyến tính
│
├── Cảm biến khoảng cách (HC-SR04 / VL53L0X)
│   └── Đo khoảng cách vật cản
│
├── Cảm biến dòng (ACS712 / INA219)
│   └── Giám sát dòng tiêu thụ
│
└── Status Reporter
    └── Gửi TCP `STATUS:...` RoboRemo mỗi 100ms
────────────────────────────────────────────────────

PHASE 2: NÂNG CAO
│
├── PID Speed Control
│   ├── Điều khiển kín vòng tốc độ motor
│   └── Đọc encoder → tính sai số → chỉnh PWM
│
├── Web Dashboard (tùy chọn)
│   ├── HTTP server trên ESP32
│   └── Giao diện web điều khiển + xem telemetry
│
└── OTA Firmware Update
    ├── OTA qua HTTP
    └── Rollback nếu lỗi
```

---

## Công Nghệ Sử Dụng

- **MCU:** ESP32-S3 (Xtensa LX7 dual-core)
- **Framework:** ESP-IDF v5.x
- **Kết nối:** WiFi 802.11 b/g/n — TCP socket
- **App điều khiển:** RoboRemo (Android)
- **PWM:** MCPWM peripheral (servo & ESC)
- **Encoder:** PCNT peripheral
- **Giao thức:** Custom text-based over TCP
