# Hướng Dẫn Cài Đặt RoboRemo Cho Xe RC ESP32-S3

## 1. Cài App RoboRemo

Tải từ [Google Play](https://play.google.com/store/apps/details?id=com.hardcodedjoy.roboremofree) (bản Free).

---

## 2. Cài Giao Diện Điều Khiển

### Cách 1: Import file interface (nhanh nhất)

1. Copy file `rc-car.interface` vào điện thoại
2. Mở RoboRemo → menu (☰) → **Interface** → **Import**
3. Chọn file `rc-car.interface`

### Cách 2: Tạo thủ công trong app

Mở RoboRemo → menu → **Interface** → **Create**. Thêm từng widget:

#### Nút STOP (khẩn cấp)
| Thuộc tính | Giá trị |
|---|---|
| Type | **Button** |
| ID | `stop` |
| Command | `s` |
| X / Y | 20 / 10 |
| W / H | 60 / 15 |
| Color | `#FF4444` |

#### Nút FORWARD
| Thuộc tính | Giá trị |
|---|---|
| Type | **Button** |
| ID | `forward` |
| Command | `f` |
| X / Y | 33 / 28 |
| W / H | 34 / 15 |
| Color | `#4CAF50` |

#### Nút BACKWARD
| Thuộc tính | Giá trị |
|---|---|
| Type | **Button** |
| ID | `backward` |
| Command | `b` |
| X / Y | 33 / 46 |
| W / H | 34 / 15 |
| Color | `#F44336` |

#### Nút LEFT
| Thuộc tính | Giá trị |
|---|---|
| Type | **Button** |
| ID | `left` |
| Command | `l` |
| X / Y | 5 / 33 |
| W / H | 25 / 18 |
| Color | `#FF9800` |

#### Nút RIGHT
| Thuộc tính | Giá trị |
|---|---|
| Type | **Button** |
| ID | `right` |
| Command | `r` |
| X / Y | 70 / 33 |
| W / H | 25 / 18 |
| Color | `#FF9800` |

#### Nút CENTER (lái thẳng)
| Thuộc tính | Giá trị |
|---|---|
| Type | **Button** |
| ID | `center` |
| Command | `c` |
| X / Y | 33 / 64 |
| W / H | 34 / 12 |
| Color | `#2196F3` |

#### Thanh trượt SERVO1 (lái)
| Thuộc tính | Giá trị |
|---|---|
| Type | **Slider** |
| ID | `servo1` |
| Text/Prefix | `SERVO1:` |
| X / Y | 0 / 78 |
| W / H | 100 / 6 |
| Min / Max | 1000 / 2000 |

#### Thanh trượt SERVO2 (ga)
| Thuộc tính | Giá trị |
|---|---|
| Type | **Slider** |
| ID | `servo2` |
| Text/Prefix | `SERVO2:` |
| X / Y | 0 / 86 |
| W / H | 100 / 6 |
| Min / Max | 1000 / 2000 |

#### Thanh trượt ESC (motor)
| Thuộc tính | Giá trị |
|---|---|
| Type | **Slider** |
| ID | `esc` |
| Text/Prefix | `ESC:` |
| X / Y | 0 / 94 |
| W / H | 100 / 6 |
| Min / Max | 0 / 255 |

#### Đèn báo kết nối (Alive)
| Thuộc tính | Giá trị |
|---|---|
| Type | **Level** |
| ID | `alive` |
| Text | `alive` |
| X / Y | 0 / 0 |
| W / H | 50 / 7 |
| Color | `#00FF00` |
| Font size | 12 |

> ⚙️ Vào **Properties** của widget `alive`, set **on timeout** = 700ms. Đèn sẽ tự tắt khi mất kết nối.

#### Hiển thị RSSI
| Thuộc tính | Giá trị |
|---|---|
| Type | **Level** |
| ID | `rssi` |
| Text | `RSSI:` |
| X / Y | 50 / 0 |
| W / H | 50 / 7 |
| Color | `#FFFFFF` |
| Font size | 12 |

---

## 3. Kết Nối Với Xe

1. **Bật nguồn ESP32-S3**
   - Nếu chưa cấu hình WiFi: ESP phát AP tên `RC_CAR-XXXX`
   - Kết nối WiFi phone vào `RC_CAR-XXXX`, vào `192.168.4.1` để cấu hình

2. **Mở RoboRemo** → menu → **Connect** → **TCP**
   - **Host**: địa chỉ IP của ESP (VD: `192.168.4.1` ở chế độ AP, hoặc IP router cấp)
   - **Port**: `9870`
   - Nhấn **Connect**

3. Khi kết nối thành công, đèn **alive** sẽ sáng xanh.

---

## 4. Điều Khiển

| Nút | Chức năng |
|---|---|
| ⬆ FORWARD | Tiến |
| ⬇ BACKWARD | Lùi |
| ⬅ LEFT | Rẽ trái |
| ➡ RIGHT | Rẽ phải |
| ⏹ STOP | Dừng ga |
| 🔵 CENTER | Lái thẳng |
| SERVO1 slider | Tinh chỉnh góc lái (1000–2000µs) |
| SERVO2 slider | Tinh chỉnh ga (1000–2000µs) |
| ESC slider | Tinh chỉnh tốc độ motor (0–255) |

---

## 5. Giao Thức TCP

ESP32 gửi **heartbeat** `alive 1\n` mỗi 500ms. Mất tín hiệu >500ms → xe tự động dừng (safety).

ESP32 gửi **status** `STATUS:rssi=-45\n` mỗi 100ms (hiển thị cường độ WiFi).

---

## 6. Cấu Hình WiFi (Captive Portal)

Khi ESP chưa có WiFi:
1. Kết nối phone vào WiFi `RC_CAR-XXXX`
2. Trình duyệt tự động mở hoặc vào `192.168.4.1`
3. Chọn WiFi nhà, nhập mật khẩu → **Save**
4. ESP sẽ khởi động lại và kết nối

Muốn vào lại chế độ Config AP: **nhấn giữ nút BOOT trên board ~3 giây**.

---

## 7. Xử Lý Sự Cố

| Vấn đề | Cách xử lý |
|---|---|
| Không thấy WiFi `RC_CAR-XXXX` | Nhấn giữ nút **BOOT** 3s để vào Config AP |
| Kết nối TCP thất bại | Kiểm tra IP và port (9870), phone cùng mạng với ESP |
| Đèn alive không sáng | Kiểm tra heartbeat: ESP gửi `alive 1\n` mỗi 500ms |
| Xe không phản hồi lệnh | Kiểm log serial (`idf.py monitor`) |
