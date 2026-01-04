# LoRa Gateway - Hệ thống Thu Thập Dữ Liệu Nông Nghiệp Thông Minh

## 📋 Mục Lục
1. [Giới thiệu](#giới-thiệu)
2. [Kiến trúc hệ thống](#kiến-trúc-hệ-thống)
3. [Yêu cầu phần cứng](#yêu-cầu-phần-cứng)
4. [Cài đặt](#cài-đặt)
5. [Cấu hình](#cấu-hình)
6. [Sử dụng](#sử-dụng)
7. [API MQTT](#api-mqtt)
8. [Database](#database)
9. [Xử lý lỗi](#xử-lý-lỗi)
10. [Troubleshooting](#troubleshooting)

---

## 🎯 Giới Thiệu

Đây là một hệ thống Gateway LoRa chạy trên **BeagleBone Black (BBB)** để thu thập dữ liệu từ các node cảm biến nông nghiệp thông qua giao thức LoRa.

### Tính năng chính:
- ✅ **Nhận dữ liệu JSON** từ các node LoRa
- ✅ **Gửi lệnh điều khiển** JSON tới các node
- ✅ **MQTT Integration** - Kết nối với dashboard web
- ✅ **SQLite Database** - Lưu trữ dữ liệu thời gian thực
- ✅ **Auto Control Mode** - Tự động điều khiển dựa trên ngưỡng
- ✅ **Interactive CLI** - Giao diện dòng lệnh

### Cảm biến hỗ trợ:
- 🌡️ Nhiệt độ (°C)
- 💧 Độ ẩm không khí (%)
- 🌞 Cường độ ánh sáng (Lux)
- 🌱 Độ ẩm đất (ADC)

### Điều khiển (Actuators):
- 💨 Quạt (Fan)
- 💡 Đèn (Light)
- 💦 Bơm nước (Pump)

---

## 🏗️ Kiến Trúc Hệ Thống

```
┌─────────────────────────────────────────────────┐
│         BeagleBone Black (Gateway)              │
│  ┌──────────────────────────────────────────┐   │
│  │ LoRa Driver (/dev/loraSPI1.0)            │   │
│  │ Frequency: 433 MHz | SF9 | BW 125 kHz   │   │
│  └──────────────────────────────────────────┘   │
│           ▲                          ▼           │
│           │                          │           │
│  ┌────────┴──────────┬────────────────┴──────┐  │
│  │   gateway.c       │    mqtt.c              │  │
│  │ - JSON Parser     │ - MQTT Broker          │  │
│  │ - Command Handler │ - Topic Management     │  │
│  │ - Auto Control    │ - DB Queries           │  │
│  └────────┬──────────┴────────────────┬──────┘  │
│           │                           │         │
│  ┌────────▼──────────────────────────▼──────┐  │
│  │     database.c (SQLite3)                 │  │
│  │ - sensor_data                            │  │
│  │ - actuator_logs                          │  │
│  │ - command_history                        │  │
│  │ - gateway_stats                          │  │
│  └──────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
         ▲                              ▼
         │                              │
    LoRa Nodes              MQTT Broker + Web
   (Node 1-3)              Dashboard
```

---

## 🖥️ Yêu Cầu Phần Cứng

### Gateway (BeagleBone Black):
- CPU: ARM Cortex-A8, 1 GHz
- RAM: 512 MB
- OS: Debian 9.x - 11.x
- LoRa Module: SX1276 qua SPI
- Thiết bị: `/dev/loraSPI1.0`

### Nodes (ESP32):
- LoRa Module: SX1276/SX1278
- Cảm biến: DHT22, LDR, Soil Moisture
- Điều khiển: MOSFET/Relay

---

## 📦 Cài Đặt

### 1. Cài đặt các thư viện phụ thuộc:

```bash
sudo apt-get update
sudo apt-get install -y \
    libcjson-dev \
    libmosquitto-dev \
    libsqlite3-dev \
    build-essential \
    git
```

### 2. Clone repository:

```bash
cd /home/debian
git clone https://github.com/your-username/DATN.git
cd DATN/lora_gateway
```

### 3. Biên dịch:

```bash
make clean
make
```

### 4. Cấu hình MQTT broker (nếu chưa có):

```bash
sudo apt-get install -y mosquitto mosquitto-clients
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

### 5. Chạy Gateway:

```bash
sudo ./bin/gateway
```

---

## ⚙️ Cấu Hình

### Thay đổi thông số trong `include/config.h`:

```c
// LoRa Settings
#define FREQUENCY           433000000   // 433 MHz
#define TX_POWER            17          // 17 dBm
#define BANDWIDTH           125000      // 125 kHz
#define SPREADING_FACTOR    512         // SF9

// Timing
#define RX_POLL_INTERVAL    50          // 50ms
#define TX_WAIT_TIME        80          // 80ms
#define STATS_INTERVAL      30          // 30s

// MQTT
#define MQTT_BROKER         "localhost" // IP của MQTT broker
#define MQTT_PORT           1883
```

---

## 🚀 Sử Dụng

### Chế độ Interactive CLI:

**Lệnh cơ bản:**
```
help                 - Hiển thị trợ giúp
status               - Trạng thái tất cả nodes
stats                - Thống kê LoRa

ĐIỀU KHIỂN THỦ CÔNG:
fan <node> <on|off>      - Bật/tắt quạt
light <node> <on|off>    - Bật/tắt đèn
pump <node> <on|off>     - Bật/tắt bơm
all <node> <on|off>      - Bật/tắt tất cả

ĐIỀU KHIỂN TỰ ĐỘNG:
auto <node> <on|off>           - Bật/tắt auto mode
settemp <node> <min> <max>     - Đặt ngưỡng nhiệt độ
setlight <node> <min> <max>    - Đặt ngưỡng ánh sáng
setsoil <node> <min> <max>     - Đặt ngưỡng độ ẩm đất

DATABASE:
dbshow <node> [limit]   - Hiển thị N bản ghi gần nhất
dbstats                 - Thống kê cơ sở dữ liệu
dbclean <days>          - Xóa dữ liệu cũ hơn N ngày
dbbackup                - Sao lưu database

exit                 - Thoát chương trình
```

**Ví dụ:**
```bash
# Bật quạt Node 1
fan 1 on

# Tắt tất cả thiết bị Node 2
all 2 off

# Bật auto mode cho Node 1 với ngưỡng:
auto 1 on
settemp 1 20.0 28.0    # Nhiệt độ: 20-28°C
setlight 1 200 800     # Ánh sáng: 200-800 lux
setsoil 1 1500 3000    # Độ ẩm: 1500-3000

# Xem 20 bản ghi gần nhất
dbshow 1 20
```

---

## 📡 API MQTT

### Dữ liệu từ Node → Gateway → MQTT

**Topic:** `lora/gateway/nodes/node{N}`

```json
{
  "node_id": 1,
  "timestamp": 1672531200,
  "sensors": {
    "temperature": 25.3,
    "humidity": 65.2,
    "light": 450,
    "soil_moisture": 2100
  },
  "actuators": {
    "fan": 0,
    "light": 1,
    "pump": 0
  },
  "signal": {
    "rssi": -95,
    "snr": 7
  },
  "stats": {
    "rx_count": 1234,
    "tx_count": 56
  },
  "auto_mode": true
}
```

### Lệnh điều khiển → MQTT → Gateway

**Topic:** `lora/gateway/control/node{N}/{command}`  
**Payload:** `on` hoặc `off`

```bash
# Bật quạt Node 1
mosquitto_pub -t "lora/gateway/control/node1/fan" -m "on"

# Tắt đèn Node 2
mosquitto_pub -t "lora/gateway/control/node2/light" -m "off"
```

**Hoặc dùng lệnh TEXT:**  
**Topic:** `lora/gateway/command`  
**Payload:** `fan 1 on`

```bash
mosquitto_pub -t "lora/gateway/command" -m "fan 1 on"
```

### Truy vấn Database qua MQTT

**Topic:** `lora/gateway/db/query`  
**Payload:**
```json
{
  "action": "get_latest",
  "node_id": 1,
  "limit": 10,
  "request_id": "req_123"
}
```

**Response Topic:** `lora/gateway/db/response`

---

## 💾 Database

### Cấu trúc bảng:

#### `sensor_data`
```sql
id              INTEGER PRIMARY KEY
timestamp       INTEGER (Unix time)
node_id         INTEGER (1-3)
temperature     REAL (°C)
humidity        REAL (%)
light           INTEGER (Lux)
soil_moisture   INTEGER (ADC)
rssi            INTEGER (dBm)
snr             INTEGER (dB)
```

#### `actuator_logs`
```sql
id              INTEGER PRIMARY KEY
timestamp       INTEGER
node_id         INTEGER
actuator        TEXT (fan, light, pump)
state           INTEGER (0/1)
trigger_type    TEXT (AUTO, MANUAL, MQTT)
trigger_value   REAL
```

#### `command_history`
```sql
id              INTEGER PRIMARY KEY
timestamp       INTEGER
node_id         INTEGER
command         TEXT
value           TEXT
source          TEXT (USER, MQTT, AUTO)
```

#### `gateway_stats`
```sql
id              INTEGER PRIMARY KEY
timestamp       INTEGER
rx_count        INTEGER
tx_count        INTEGER
crc_errors      INTEGER
json_errors     INTEGER
auto_commands   INTEGER
```

### Vị trí file:
```
/home/debian/lora_gateway.db      (Database chính)
/home/debian/backups/             (Thư mục backup)
/tmp/gateway_data.json            (JSON output cho web)
```

---

## ⚠️ Xử Lý Lỗi

### Lỗi LoRa:

| Lỗi | Nguyên nhân | Giải pháp |
|-----|------------|----------|
| `Failed to open /dev/loraSPI1.0` | Driver không load | `sudo modprobe` hoặc khởi động lại |
| `CRC error` | Nhiễu tín hiệu | Giảm spreading factor hoặc tăng TX power |
| `TX failed` | Không có dữ liệu | Kiểm tra node đó có đang chạy không |

### Lỗi MQTT:

| Lỗi | Nguyên nhân | Giải pháp |
|-----|------------|----------|
| `MQTT connect failed` | Broker không sẵn sàng | `sudo systemctl restart mosquitto` |
| `Cannot open database` | Permission denied | `sudo chown debian:debian /home/debian/` |

### Lỗi Database:

| Lỗi | Nguyên nhân | Giải pháp |
|-----|------------|----------|
| `Insert errors` | Disk đầy | `dbclean 7` để xóa dữ liệu cũ |
| `Cannot open DB` | File bị khóa | Kill process cũ, xóa `.db-wal` |

---

## 🔧 Troubleshooting

### Gateway không nhận dữ liệu từ Node

1. **Kiểm tra LoRa module:**
   ```bash
   ls -la /dev/loraSPI*
   ```

2. **Kiểm tra thông số:**
   - Frequency, SF, BW của node phải giống gateway
   - Khoảng cách < 1 km (open space)

3. **Xem log:**
   ```bash
   journalctl -u lora_gateway -f
   ```

### MQTT không kết nối

```bash
# Kiểm tra broker
mosquitto_sub -t "lora/gateway/#" -v

# Kiểm tra firewall
sudo ufw allow 1883
```

### Database phát triển quá nhanh

```bash
# Xóa dữ liệu cũi hơn 7 ngày
dbclean 7

# Hoặc tạo backup rồi xóa
dbbackup
```

---

## 📊 Giám Sát

### Xem thống kê real-time:

```bash
mosquitto_sub -t "lora/gateway/stats"
```

### Output JSON:

```bash
cat /tmp/gateway_data.json | jq
```

### Theo dõi DB:

```bash
sudo sqlite3 /home/debian/lora_gateway.db
sqlite> SELECT COUNT(*) FROM sensor_data;
sqlite> SELECT * FROM sensor_data ORDER BY timestamp DESC LIMIT 5;
```

---

## 👥 Đóng Góp

Để báo cáo lỗi hoặc đề xuất tính năng, vui lòng tạo GitHub Issue.

---

## 📄 Giấy Phép

MIT License - xem file LICENSE để chi tiết

---

## 📧 Liên Hệ

- **Email:** your-email@example.com
- **GitHub:** [@your-username](https://github.com/your-username)

---

**Cập nhật lần cuối:** Tháng 1 năm 2026
