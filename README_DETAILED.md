# 🌾 LoRa Gateway - Hệ thống Nông Nghiệp Thông Minh

**ĐATN (Đồ Án Tốt Nghiệp)** - Hệ thống Gateway LoRa trên BeagleBone Black với JSON, MQTT, SQLite3 và Điều khiển Tự động

📅 **Phiên bản:** 1.0.0 | 🔧 **Trạng thái:** Active Development ✅

---

## 📋 Mục Lục

1. [Tổng Quan](#tổng-quan)
2. [Kiến Trúc Hệ Thống](#kiến-trúc-hệ-thống)
3. [Yêu Cầu Phần Cứng & Phần Mềm](#yêu-cầu-phần-cứng--phần-mềm)
4. [Hướng Dẫn Cài Đặt Chi Tiết](#hướng-dẫn-cài-đặt-chi-tiết)
5. [Cấu Hình Nâng Cao](#cấu-hình-nâng-cao)
6. [Hướng Dẫn Sử Dụng](#hướng-dẫn-sử-dụng)
7. [API MQTT Toàn Bộ](#api-mqtt-toàn-bộ)
8. [Schema & Truy Vấn Database](#schema--truy-vấn-database)
9. [Giám Sát & Tối Ưu Hóa](#giám-sát--tối-ưu-hóa)
10. [Xử Lý Lỗi Toàn Diện](#xử-lý-lỗi-toàn-diện)
11. [Phát Triển Thêm](#phát-triển-thêm)

---

## 🎯 Tổng Quan

### Mục Đích Dự Án

Phát triển hệ thống Gateway LoRa để:
- 📊 **Thu thập dữ liệu** từ các node cảm biến nông nghiệp
- 🎮 **Điều khiển thiết bị** (quạt, đèn, bơm) tại từng farm
- 🤖 **Tự động hóa** dựa trên ngưỡng (temperature, light, soil moisture)
- ☁️ **Tích hợp MQTT** để kết nối với web dashboard
- 💾 **Lưu trữ dữ liệu** lâu dài trong SQLite3
- 📱 **Cung cấp API** để các ứng dụng khác truy vấn

### Tính Năng Chính

✅ **JSON Format** - Dễ parse, tiết kiệm bandwidth  
✅ **MQTT Integration** - Pub/Sub, real-time control  
✅ **SQLite Database** - Lưu trữ 100k+ records  
✅ **Auto Control** - Điều khiển tự động dựa threshold  
✅ **CLI Interactive** - Điều khiển từ command line  
✅ **Database Backup** - Sao lưu tự động hàng ngày  
✅ **CRC Error Recovery** - Phục hồi lỗi truyền  
✅ **Graceful Shutdown** - Đóng an toàn  

### Cảm Biến Hỗ Trợ

| Cảm Biến | Thông Số | Đơn Vị |
|----------|---------|-------|
| 🌡️ Nhiệt độ | -40 đến +80 | °C |
| 💧 Độ ẩm không khí | 0 đến 100 | % |
| 🌞 Ánh sáng | 0 đến 65535 | Lux |
| 🌱 Độ ẩm đất | 0 đến 4095 | ADC |

### Điều Khiển (Actuators)

| Thiết Bị | Mô Tả | Điều Khiển |
|----------|------|-----------|
| 💨 Quạt | Làm mát không khí | ON/OFF |
| 💡 Đèn | Cung cấp ánh sáng | ON/OFF |
| 💦 Bơm | Tưới nước | ON/OFF |

---

## 🏗️ Kiến Trúc Hệ Thống

### Sơ Đồ Toàn Cục

```
┌─────────────────────────────────────────────────────────┐
│          BeagleBone Black Gateway (Linux ARM)           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │   LoRa Driver (/dev/loraSPI1.0)                  │   │
│  │   Freq: 433 MHz | SF9 | BW: 125 kHz             │   │
│  │   TX Power: 17 dBm | Max Range: 1 km            │   │
│  └────────────────┬─────────────────────────────────┘   │
│                   │                                     │
│    ┌──────────────▼──────────────┐                     │
│    │     gateway.c (Core)        │                     │
│    │ ┌─────────────────────────┐ │                     │
│    │ │ JSON Parser             │ │                     │
│    │ │ Auto Control Logic      │ │                     │
│    │ │ Interactive CLI Handler │ │                     │
│    │ └────────────┬────────────┘ │                     │
│    └──────────────┼──────────────┘                     │
│                   │                                     │
│    ┌──────────────┼──────────────┬──────────────────┐   │
│    ▼              ▼              ▼                  ▼   │
│ mqtt.c      database.c        lora.c            utils.c│
│ - Broker    - SQLite3         - I/O              - Logs │
│ - Callbacks - Queries         - Commands                │
│ - Publish   - Backup                                    │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │   Output Files                                   │   │
│  │ - /tmp/gateway_data.json (Real-time JSON)       │   │
│  │ - /home/debian/lora_gateway.db (SQLite)         │   │
│  │ - /home/debian/backups/*.db (Daily backups)     │   │
│  └──────────────────────────────────────────────────┘   │
│                                                         │
└─────────────────────────────────────────────────────────┘
         ▲                                    ▼
         │                                    │
    LoRa Nodes                    MQTT Broker
   (ESP32 x3)                   + Web Dashboard
   Node1-3
```

### Luồng Dữ Liệu Chi Tiết

```
1. NODE SEND (Every 10s):
   LoRa → Gateway
   JSON: {"node":1, "temp":25.3, "hum":65.2, ...}
   
2. GATEWAY RECEIVE:
   Parse JSON
   Save to SQLite
   Check Auto Control
   Publish to MQTT
   
3. GATEWAY PUBLISH:
   Topic: lora/gateway/nodes/node1
   Payload: {complete sensor data}
   
4. COMMANDS (From MQTT):
   Topic: lora/gateway/control/node1/fan
   Payload: "on"
   Gateway parses → Sends to Node via LoRa
   
5. DATABASE:
   sensor_data
   actuator_logs
   command_history
   gateway_stats
```

---

## 🖥️ Yêu Cầu Phần Cứng & Phần Mềm

### Hardware Requirements

#### Gateway (BeagleBone Black)
| Thành Phần | Chi Tiết |
|-----------|---------|
| 🔌 SoC | AM3358, ARM Cortex-A8, 1 GHz |
| 🧠 RAM | 512 MB DDR3 |
| 💾 Storage | 4 GB eMMC hoặc MicroSD |
| 📡 LoRa Module | SX1276, qua SPI `/dev/loraSPI1.0` |
| 🔌 Interfaces | SPI, UART, GPIO |

#### Node (ESP32)
| Thành Phần | Chi Tiết |
|-----------|---------|
| 🔌 SoC | ESP32, Dual Core, 240 MHz |
| 📡 LoRa | SX1276/SX1278 qua SPI |
| 📊 Cảm biến | DHT22 (Temp/Hum), LDR (Light), Soil (ADC) |
| 🔌 Actuators | MOSFET/Relay cho Fan, Light, Pump |

### Software Requirements

#### Bắt Buộc
```bash
OS: Debian 9/10/11 (ARM)
GCC: 4.9+ 
Git: 2.0+
Make: 3.8+
```

#### Libraries
```bash
libcjson-dev       (JSON parsing)
libmosquitto-dev   (MQTT client)
libsqlite3-dev     (Database)
build-essential    (Compiler tools)
```

---

## 📥 Hướng Dẫn Cài Đặt Chi Tiết

### Bước 1: Chuẩn Bị Hệ Thống (10 phút)

```bash
# 1. SSH vào BBB hoặc terminal local
ssh debian@192.168.7.2

# 2. Update package list
sudo apt-get update
sudo apt-get upgrade -y

# 3. Cài đặt dependencies
sudo apt-get install -y \
    git \
    build-essential \
    libcjson-dev \
    libmosquitto-dev \
    mosquitto \
    mosquitto-clients \
    libsqlite3-dev \
    sqlite3

# 4. Kiểm tra installation
gcc --version
sqlite3 --version
mosquitto --version
```

### Bước 2: Clone & Setup Repository (5 phút)

```bash
# 1. Clone project
cd /home/debian
sudo git clone https://github.com/bachptit2003/DOANTOTNGHIEP.git
cd DOANTOTNGHIEP/lora_gateway

# 2. Kiểm tra cấu trúc
ls -la
```

Output:
```
include/      - Header files
src/          - Source files
Makefile      - Build configuration
bin/          - Compiled binary (sau khi make)
```

### Bước 3: Load LoRa Driver (5 phút)

```bash
# 1. Biên dịch driver
cd ../driverlora
make

# 2. Load kernel module
sudo insmod lora.ko

# 3. Verify
ls -la /dev/loraSPI*
# Output: /dev/loraSPI1.0
```

### Bước 4: Biên Dịch Gateway (10 phút)

```bash
cd ../lora_gateway

# 1. Clean build
make clean

# 2. Compile
make

# 3. Check binary
ls -lh bin/gateway
# Output: -rwxr-xr-x 1 debian debian 250K
```

### Bước 5: Khởi Động Services (5 phút)

```bash
# 1. Start MQTT broker
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

# 2. Verify MQTT
mosquitto_sub -t "test" &
mosquitto_pub -t "test" -m "hello"

# 3. Create database directory
sudo mkdir -p /home/debian/backups
sudo chown debian:debian /home/debian/backups

# 4. Create database path
touch /home/debian/lora_gateway.db
sudo chown debian:debian /home/debian/lora_gateway.db
```

### Bước 6: Chạy Gateway (1 phút)

```bash
# 1. Run with sudo (cần access LoRa device)
sudo ./bin/gateway

# Expected output:
# ╔═══════════════════════════════════════════════════╗
# ║  BeagleBone Black LoRa Gateway - JSON MODE       ║
# ║  ✓ Receives JSON data from nodes                 ║
# ║  ✓ Sends JSON commands to nodes                  ║
# ╚═══════════════════════════════════════════════════╝
# 
# ✓ Device opened: /dev/loraSPI1.0
# ✓ Frequency: 0.433 MHz
# ✓ TX Power: 17 dBm
# ...
```

### Bước 7: Kiểm Tra Hoạt Động (2 phút)

```bash
# Terminal 1: Run gateway
sudo ./bin/gateway

# Terminal 2: Subscribe MQTT
mosquitto_sub -t "lora/gateway/#" -v

# Terminal 3: Test command
mosquitto_pub -t "lora/gateway/command" -m "status"

# Xem output trên Terminal 1:
# [14:30:45] MQTT RX: lora/gateway/command => status
```

---

## ⚙️ Cấu Hình Nâng Cao

### Tệp Cấu Hình Chính: `include/config.h`

```c
/* LoRa Configuration */
#define DEVICE_PATH         "/dev/loraSPI1.0"
#define FREQUENCY           433000000      // 433 MHz ISM band
#define TX_POWER            17             // 17 dBm max
#define BANDWIDTH           125000         // 125 kHz
#define SPREADING_FACTOR    512            // SF9 = 2^9

/* Timing */
#define RX_POLL_INTERVAL    50             // Poll every 50ms
#define TX_WAIT_TIME        80             // Wait 80ms after TX
#define STATS_INTERVAL      30             // Stats every 30s

/* MQTT */
#define MQTT_BROKER         "localhost"    // Broker IP
#define MQTT_PORT           1883           // Default port
#define MQTT_KEEPALIVE      60             // Keepalive 60s
#define MQTT_TOPIC_PREFIX   "lora/gateway" // Topic prefix

/* Database */
#define DB_PATH             "/home/debian/lora_gateway.db"
#define DB_BACKUP_DIR       "/home/debian/backups"
```

### Tối Ưu Hóa Thông Số LoRa

| Mục Đích | SF | BW | TX Power | Range | Battery |
|----------|----|----|----------|-------|---------|
| 📍 Gần (< 100m) | SF7 | 500k | 7 | 100m | ✓✓✓ |
| 🏘️ Trung bình (100-500m) | SF9 | 125k | 14 | 500m | ✓✓ |
| 🌾 Xa (> 500m) | SF11 | 125k | 17 | 2km | ✓ |

**Khuyến nghị cho nông nghiệp: SF9, 125kHz**

---

## 🚀 Hướng Dẫn Sử Dụng

### 1. Chế Độ Interactive CLI

```bash
$ sudo ./bin/gateway
Type 'help' for commands

gateway> help

╔═════════════════════════════════════╗
║      Available Commands (JSON)      ║
╚═════════════════════════════════════╝

MANUAL CONTROL (JSON Format):
  fan <node> <on|off>     - Control fan
  light <node> <on|off>   - Control light
  pump <node> <on|off>    - Control pump
  all <node> <on|off>     - Control all
  Example: fan 1 on
           → {"node":1,"cmd":"fan","val":"on"}

AUTO CONTROL:
  auto <node> <on|off>           - Enable/disable auto
  settemp <node> <min> <max>     - Set temp range
  setlight <node> <min> <max>    - Set light range (lux)
  setsoil <node> <min> <max>     - Set soil range (ADC)

MONITORING:
  status                  - Show all nodes
  stats                   - Show statistics

DATABASE:
  dbshow <node> [limit]   - Show recent data
  dbstats                 - Show database stats
  dbclean <days>          - Clean data older than N days
  dbbackup                - Backup database now

SYSTEM:
  help                    - Show this help
  exit                    - Exit gateway
```

### 2. Ví Dụ Sử Dụng Thực Tế

#### Scenario 1: Manual Control
```bash
gateway> fan 1 on
→ Sending JSON: {"node":1,"cmd":"fan","val":"on"}

gateway> light 2 off
→ Sending JSON: {"node":2,"cmd":"light","val":"off"}

gateway> all 3 off
→ Tắt tất cả thiết bị Node 3
```

#### Scenario 2: Auto Control
```bash
# Bật auto mode
gateway> auto 1 on
✓ Node 1 AUTO mode ON

# Đặt ngưỡng
gateway> settemp 1 20.0 28.0
✓ Node 1 temp: [20.0, 28.0]°C

gateway> setlight 1 200 800
✓ Node 1 light: [200, 800] lux

gateway> setsoil 1 1500 3000
✓ Node 1 soil: [1500, 3000]

# Gateway sẽ tự động:
# - Bật fan nếu T > 28°C
# - Tắt fan nếu T < 20°C
# - Bật đèn nếu Light < 200 lux
# - Bật bơm nếu Soil > 3000
```

#### Scenario 3: Monitoring
```bash
gateway> status

╔═════════════════════════════════════╗
║         Gateway Status (JSON)       ║
╚═════════════════════════════════════╝

Node 1:
  T:25.3°C H:65.2%  L:450 S:2100
  Actuators: Fan=ON Light=OFF Pump=ON
  Auto: ON, Last: 2s ago
  RX=1234 TX=56 RSSI=-95 dBm

Node 2:
  No data yet

Node 3:
  [similar format]
```

#### Scenario 4: Database
```bash
gateway> dbshow 1 20
╔═══════════════════════════════════════════════════════════╗
║  Node 1 - Last 20 Records                                ║
║ Time                 Temp   Hum   Light  Soil   RSSI      ║
║ 14:45:23  25.3°C  65.2%  450    2100   -95dBm ║
║ 14:45:13  25.2°C  65.0%  448    2105   -96dBm ║
...

gateway> dbstats
║ Sensor records:    3425
║ Actuator logs:     156
║ Command history:   89
║ Total inserts:     3670

gateway> dbclean 7
✓ Cleaned up 1200 old records (kept last 7 days)

gateway> dbbackup
✓ Database backed up: /home/debian/backups/lora_gateway_20240104_143045.db
```

---

## 📡 API MQTT Toàn Bộ

### 1. Dữ Liệu Sensor từ Node

**Topic:** `lora/gateway/nodes/node{N}`  
**Frequency:** Mỗi lần nhận dữ liệu từ Node (~ 10s)

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
    "fan": 1,
    "light": 0,
    "pump": 1
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

**Subscribe:**
```bash
mosquitto_sub -t "lora/gateway/nodes/+" -v
```

### 2. Lệnh Điều Khiển Structured

**Topic:** `lora/gateway/control/node{N}/{command}`  
**Payload:** `on` hoặc `off`

```bash
# Bật quạt Node 1
mosquitto_pub -t "lora/gateway/control/node1/fan" -m "on"

# Tắt đèn Node 2
mosquitto_pub -t "lora/gateway/control/node2/light" -m "off"

# Tắt tất cả Node 3
mosquitto_pub -t "lora/gateway/control/node3/all" -m "off"

# Bật auto Node 1
mosquitto_pub -t "lora/gateway/control/node1/auto" -m "on"
```

### 3. Lệnh Điều Khiển Text (Đơn Giản)

**Topic:** `lora/gateway/command`  
**Payload:** `<command> <node> <value>`

```bash
# Manual control
mosquitto_pub -t "lora/gateway/command" -m "fan 1 on"
mosquitto_pub -t "lora/gateway/command" -m "light 2 off"
mosquitto_pub -t "lora/gateway/command" -m "all 3 off"

# Auto control
mosquitto_pub -t "lora/gateway/command" -m "auto 1 on"

# Threshold settings
mosquitto_pub -t "lora/gateway/command" -m "settemp 1 20.0 28.0"
mosquitto_pub -t "lora/gateway/command" -m "setlight 1 200 800"
mosquitto_pub -t "lora/gateway/command" -m "setsoil 1 1500 3000"
```

### 4. Threshold Setting via Structured Topic

**Topic:** `lora/gateway/control/node{N}/threshold/{type}`  
**Payload:** `min,max`

```bash
# Nhiệt độ 20-28°C
mosquitto_pub -t "lora/gateway/control/node1/threshold/temp" -m "20.0,28.0"

# Ánh sáng 200-800 lux
mosquitto_pub -t "lora/gateway/control/node1/threshold/light" -m "200,800"

# Độ ẩm đất 1500-3000
mosquitto_pub -t "lora/gateway/control/node1/threshold/soil" -m "1500,3000"
```

### 5. Database Query via MQTT

**Topic:** `lora/gateway/db/query`  
**Payload:** JSON request

```json
{
  "action": "get_latest",
  "node_id": 1,
  "limit": 10,
  "request_id": "req_20240104_001"
}
```

**Response Topic:** `lora/gateway/db/response`

```bash
# Query latest 10 records
mosquitto_pub -t "lora/gateway/db/query" -m '{
  "action": "get_latest",
  "node_id": 1,
  "limit": 10,
  "request_id": "req1"
}'

# Query last 24 hours
mosquitto_pub -t "lora/gateway/db/query" -m '{
  "action": "get_range",
  "node_id": 1,
  "hours": 24,
  "request_id": "req2"
}'

# Get aggregate (AVG, MIN, MAX)
mosquitto_pub -t "lora/gateway/db/query" -m '{
  "action": "get_aggregate",
  "node_id": 1,
  "hours": 24,
  "request_id": "req3"
}'

# Actuator history
mosquitto_pub -t "lora/gateway/db/query" -m '{
  "action": "get_actuator_history",
  "node_id": 1,
  "limit": 20,
  "request_id": "req4"
}'

# Overall stats
mosquitto_pub -t "lora/gateway/db/query" -m '{
  "action": "get_stats",
  "request_id": "req5"
}'
```

### 6. Gateway Stats

**Topic:** `lora/gateway/stats`  
**Frequency:** Mỗi 30 giây

```json
{
  "timestamp": 1672531200,
  "rx_nodata": 0,
  "rx_crc_error": 5,
  "rx_crc_recovery": 4,
  "json_parse_error": 0,
  "auto_commands": 45,
  "mqtt_publish_count": 12450,
  "mqtt_error_count": 3
}
```

### 7. Gateway Status

**Topic:** `lora/gateway/status`  
**Payload:** `online` hoặc `offline` (Last Will Testament)

```bash
mosquitto_sub -t "lora/gateway/status"
# Output: online (khi gateway start)
# Output: offline (khi gateway crash/disconnect)
```

---

## 💾 Schema & Truy Vấn Database

### Bảng 1: sensor_data (Dữ liệu Cảm Biến)

```sql
CREATE TABLE sensor_data (
  id              INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp       INTEGER NOT NULL,           -- Unix time
  node_id         INTEGER NOT NULL,           -- 1-3
  temperature     REAL,                       -- °C
  humidity        REAL,                       -- %
  light           INTEGER,                    -- Lux
  soil_moisture   INTEGER,                    -- ADC 0-4095
  rssi            INTEGER,                    -- dBm (-120 to 0)
  snr             INTEGER                     -- dB
);

-- Index để tối ưu query
CREATE INDEX idx_time_node ON sensor_data(timestamp, node_id);
```

**Ví dụ Truy Vấn:**
```sql
-- 10 bản ghi gần nhất
SELECT * FROM sensor_data WHERE node_id = 1 
ORDER BY timestamp DESC LIMIT 10;

-- Dữ liệu trong 24h
SELECT * FROM sensor_data 
WHERE node_id = 1 AND timestamp > (strftime('%s', 'now') - 86400)
ORDER BY timestamp;

-- Thống kê hôm nay
SELECT 
  AVG(temperature) as avg_temp,
  MIN(temperature) as min_temp,
  MAX(temperature) as max_temp,
  COUNT(*) as count
FROM sensor_data
WHERE node_id = 1 AND timestamp > (strftime('%s', 'now') - 86400);

-- Export CSV
.mode csv
.output sensor_data.csv
SELECT datetime(timestamp, 'unixepoch'), temperature, humidity, light, soil_moisture 
FROM sensor_data WHERE node_id = 1;
.output stdout
```

### Bảng 2: actuator_logs (Log Thay Đổi Thiết Bị)

```sql
CREATE TABLE actuator_logs (
  id              INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp       INTEGER NOT NULL,
  node_id         INTEGER NOT NULL,
  actuator        TEXT NOT NULL,              -- 'fan', 'light', 'pump'
  state           INTEGER NOT NULL,           -- 0=OFF, 1=ON
  trigger_type    TEXT,                       -- 'AUTO', 'MANUAL', 'MQTT'
  trigger_value   REAL                        -- Temperature, Light, etc.
);
```

**Ví dụ Truy Vấn:**
```sql
-- Lịch sử bật/tắt quạt
SELECT datetime(timestamp, 'unixepoch'), actuator, state, trigger_type 
FROM actuator_logs 
WHERE node_id = 1 AND actuator = 'fan'
ORDER BY timestamp DESC LIMIT 20;

-- Lần gần nhất bật bơm
SELECT * FROM actuator_logs 
WHERE node_id = 1 AND actuator = 'pump' AND state = 1
ORDER BY timestamp DESC LIMIT 1;

-- Số lần auto vs manual
SELECT trigger_type, COUNT(*) as count 
FROM actuator_logs 
WHERE node_id = 1 
GROUP BY trigger_type;
```

### Bảng 3: command_history (Lịch Sử Lệnh)

```sql
CREATE TABLE command_history (
  id              INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp       INTEGER NOT NULL,
  node_id         INTEGER NOT NULL,
  command         TEXT NOT NULL,              -- 'fan', 'light', 'pump', 'auto'
  value           TEXT NOT NULL,              -- 'on', 'off', threshold values
  source          TEXT                        -- 'USER', 'MQTT', 'AUTO'
);
```

**Ví dụ Truy Vấn:**
```sql
-- Tất cả lệnh được gửi
SELECT datetime(timestamp, 'unixepoch'), command, value, source 
FROM command_history 
WHERE node_id = 1
ORDER BY timestamp DESC LIMIT 50;

-- Lệnh từ MQTT
SELECT * FROM command_history 
WHERE node_id = 1 AND source = 'MQTT'
ORDER BY timestamp DESC;
```

### Bảng 4: gateway_stats (Thống Kê Gateway)

```sql
CREATE TABLE gateway_stats (
  id              INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp       INTEGER NOT NULL,
  rx_count        INTEGER,                    -- Total RX
  tx_count        INTEGER,                    -- Total TX
  crc_errors      INTEGER,                    -- CRC failures
  json_errors     INTEGER,                    -- JSON parse failures
  auto_commands   INTEGER                     -- Auto control commands sent
);
```

**Ví dụ Truy Vấn:**
```sql
-- Thống kê cuối ngày
SELECT datetime(timestamp, 'unixepoch') as time, rx_count, tx_count, crc_errors
FROM gateway_stats
ORDER BY timestamp DESC LIMIT 1;

-- Xu hướng lỗi
SELECT 
  date(datetime(timestamp, 'unixepoch')) as date,
  SUM(crc_errors) as total_errors,
  SUM(auto_commands) as auto_actions
FROM gateway_stats
GROUP BY date
ORDER BY date DESC LIMIT 7;
```

### Backup & Export

```bash
# Interactive SQLite3
sqlite3 /home/debian/lora_gateway.db

# Dump sang file
sqlite3 /home/debian/lora_gateway.db ".dump" > backup.sql

# Export CSV
sqlite3 /home/debian/lora_gateway.db -header -csv \
  "SELECT datetime(timestamp, 'unixepoch'), temperature, humidity 
   FROM sensor_data WHERE node_id = 1" > data.csv

# Restore
sqlite3 /home/debian/lora_gateway.db < backup.sql
```

---

## 📊 Giám Sát & Tối Ưu Hóa

### Real-time Monitoring

```bash
# Terminal 1: Gateway
sudo ./bin/gateway

# Terminal 2: Watch MQTT
watch -n 1 'mosquitto_sub -t "lora/gateway/stats" -C 1 | jq'

# Terminal 3: Monitor DB
watch -n 5 'sqlite3 /home/debian/lora_gateway.db \
  "SELECT COUNT(*) as total, 
          COUNT(CASE WHEN timestamp > (strftime(\"%s\", \"now\") - 3600) THEN 1 END) as last_hour 
   FROM sensor_data"'

# Terminal 4: Check system
watch -n 5 'free -h && echo && top -b -n 1 | head -15'
```

### Performance Metrics

```bash
# Database size
du -sh /home/debian/lora_gateway.db

# Number of records
sqlite3 /home/debian/lora_gateway.db "SELECT COUNT(*) FROM sensor_data;"

# Disk usage
df -h /home/debian

# Process memory
ps aux | grep gateway | grep -v grep

# MQTT traffic
mosquitto_sub -t "lora/gateway/#" | wc -l
```

### Optimization Tips

1. **Database Cleanup** (tự động mỗi tuần):
   ```bash
   # Xóa dữ liệu cũi hơn 30 ngày
   gateway> dbclean 30
   ```

2. **Enable VACUUM** (mỗi tháng):
   ```bash
   sqlite3 /home/debian/lora_gateway.db "VACUUM;"
   ```

3. **Monitor Disk Space**:
   ```bash
   # ~10 records/node/min = ~14,400 records/day
   # ~1 KB/record = ~14 MB/day
   # Mỗi tháng: ~420 MB
   ```

4. **Tune LoRa Parameters**:
   - Reduce SF nếu gần → Faster TX, Less battery
   - Increase SF nếu xa → Better range, More power

---

## ⚠️ Xử Lý Lỗi Toàn Diện

### Lỗi LoRa

| Lỗi | Nguyên nhân | Giải pháp | Status |
|-----|-----------|----------|--------|
| `Failed to open /dev/loraSPI1.0` | Driver không load | `sudo insmod driverlora/lora.ko` | 🔴 Critical |
| `TX failed: Bad file descriptor` | Device bị đóng | Restart gateway | 🔴 Critical |
| `CRC error` | Nhiễu/khoảng cách xa | Giảm SF hoặc tăng TX power | 🟡 Warning |
| `JSON parse failed` | Dữ liệu hỏng | Kiểm tra node firmware | 🟡 Warning |
| `No data from node` | Node tắt/hỏng | Kiểm tra node pin, reset | 🔴 Critical |

**Recovery:**
```bash
# Restart LoRa driver
sudo rmmod lora.ko
sudo insmod driverlora/lora.ko

# Restart gateway
pkill gateway
sudo ./bin/gateway
```

### Lỗi MQTT

| Lỗi | Nguyên nhân | Giải pháp |
|-----|-----------|----------|
| `MQTT Connect failed` | Broker down | `sudo systemctl restart mosquitto` |
| `Cannot subscribe` | Permission denied | Check mosquitto ACL |
| `Publish timeout` | Network issue | Check WiFi/Ethernet |

```bash
# Kiểm tra broker
systemctl status mosquitto

# Kiểm tra port
netstat -tulpn | grep 1883

# Test connection
mosquitto_pub -h 127.0.0.1 -t test -m "hello"
```

### Lỗi Database

| Lỗi | Nguyên nhân | Giải pháp |
|-----|-----------|----------|
| `database is locked` | Process đang mở DB | Kill process cũ |
| `disk I/O error` | Disk đầy | `dbclean 7` |
| `cannot open database` | Permission denied | `sudo chown debian:debian` |

```bash
# Find process using DB
lsof /home/debian/lora_gateway.db

# Force close
pkill -9 gateway

# Check/repair
sqlite3 /home/debian/lora_gateway.db "PRAGMA integrity_check;"
```

### Lỗi Memory/CPU

```bash
# Monitor real-time
top -u debian

# Check memory leak
ps aux | grep gateway
# Memory bình thường: 5-15 MB

# Process limit
ulimit -v  # Virtual memory
ulimit -u  # Process count
```

---

## 🔧 Phát Triển Thêm

### Mở Rộng Sensor

Thêm cảm biến mới trong `src/gateway.c`:

```c
// Thêm vào node_data_t struct
typedef struct {
    // ... existing fields
    float co2_level;           // ppm
    uint16_t wind_speed;       // km/h
    // ...
} node_data_t;

// Parse JSON
if (cJSON_IsNumber(co2_json)) *co2 = co2_json->valuedouble;

// Database
db_save_sensor_data(..., temp, hum, light, soil, co2, wind, ...);
```

### Thêm Actuator Mới

```c
// Update struct
typedef struct {
    // ... existing
    uint8_t heater_state;      // New heater control
    uint8_t curtain_state;     // New curtain control
} actuator_state_t;

// Add command handler
else if (strcmp(command, "heater") == 0) {
    lora_send_command(node_id, "heater", value);
    // ...
}
```

### Advanced Auto Control

Implement PID controller:

```c
void pid_control(int node_id, float current, float target, float* output) {
    static float error_prev = 0, integral = 0;
    
    float error = target - current;
    integral += error * DT;
    float derivative = (error - error_prev) / DT;
    
    *output = Kp * error + Ki * integral + Kd * derivative;
    error_prev = error;
}
```

### Web Dashboard Integration

```bash
# Export real-time data
cat /tmp/gateway_data.json | curl -X POST \
  http://your-dashboard.com/api/sensor \
  -H "Content-Type: application/json" \
  -d @-

# Or via MQTT
mosquitto_sub -t "lora/gateway/nodes/+" | \
  jq -R 'fromjson | . as $data | @base64' | \
  curl -X POST http://your-api/sensor -d @-
```

---

## 📚 Tài Liệu Tham Khảo

- [BeagleBone Black](https://beagleboard.org/black)
- [Mosquitto MQTT](https://mosquitto.org/documentation/)
- [cJSON Library](https://github.com/DaveGamble/cJSON)
- [SQLite3](https://www.sqlite.org/docs.html)
- [LoRa Technology](https://lora-alliance.org/)

---

## 📋 Danh Sách File

```
DOANTOTNGHIEP/
├── README.md                    ← Bạn đang đọc
├── QUICKSTART.md               ← Hướng dẫn nhanh
├── LICENSE
├── .gitignore
│
├── driverlora/                 ← Kernel driver
│   ├── Makefile
│   ├── driver.c
│   ├── lora.c
│   └── lora.h
│
├── lora_gateway/               ← Gateway application
│   ├── Makefile
│   ├── include/
│   │   ├── types.h             ← Data structures
│   │   ├── gateway.h           ← Gateway functions
│   │   ├── mqtt.h              ← MQTT integration
│   │   ├── database.h          ← SQLite functions
│   │   ├── lora.h              ← LoRa wrapper
│   │   ├── config.h            ← Configuration
│   │   ├── auto_control.h      ← Auto control
│   │   ├── json_parser.h       ← JSON parsing
│   │   └── utils.h             ← Utilities
│   │
│   └── src/
│       ├── main.c              ← Entry point
│       ├── gateway.c           ← Core logic
│       ├── mqtt.c              ← MQTT callbacks
│       ├── database.c          ← DB operations
│       ├── lora.c              ← LoRa I/O
│       ├── auto_control.c      ← Auto logic
│       ├── json_parser.c       ← JSON parsing
│       └── utils.c             ← Utilities
│
└── node_cam_bien/              ← Node firmware
    └── nodecambien.ino         ← Arduino code
```

---

## 💬 Support & Contribute

**Report Issues:** https://github.com/bachptit2003/DOANTOTNGHIEP/issues  
**Pull Requests:** Welcome! 🎉

---

## 👨‍🎓 About

**ĐATN 2025-2026** - Smart Agriculture IoT System  
**Author:** Bach Tít  
**University:** [Your University]

---

**Last Updated:** January 4, 2026  
**Version:** 1.0.0 ✅  
**License:** MIT
