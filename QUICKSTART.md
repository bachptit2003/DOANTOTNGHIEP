# 🚀 Quick Start Guide

## Cài đặt nhanh trên BeagleBone Black

### 1. Chuẩn bị
```bash
# SSH vào BBB
ssh debian@192.168.1.XXX

# Update hệ thống
sudo apt-get update
sudo apt-get upgrade -y
```

### 2. Cài đặt thư viện
```bash
sudo apt-get install -y \
    libcjson-dev \
    libmosquitto-dev \
    libsqlite3-dev \
    build-essential \
    git \
    mosquitto \
    mosquitto-clients
```

### 3. Clone & Build
```bash
cd /home/debian
sudo git clone https://github.com/your-username/DATN.git
cd DATN/lora_gateway
sudo make clean
sudo make
```

### 4. Chạy Gateway
```bash
sudo ./bin/gateway
```

### 5. Kiểm tra MQTT
```bash
# Terminal khác
mosquitto_sub -t "lora/gateway/#" -v
```

---

## Các lệnh hay dùng

### Build & Run
```bash
cd /home/debian/DATN/lora_gateway
make                    # Build
make clean              # Clean
make install            # Install system-wide (tùy chọn)
```

### Interactive Commands
```
help              # Trợ giúp
status            # Xem trạng thái
fan 1 on          # Bật quạt node 1
auto 1 on         # Bật auto mode
settemp 1 20 28   # Đặt ngưỡng nhiệt độ
dbshow 1 10       # Xem 10 bản ghi
exit              # Thoát
```

### MQTT Commands
```bash
# Bật quạt node 1
mosquitto_pub -t "lora/gateway/control/node1/fan" -m "on"

# Truy vấn database
mosquitto_pub -t "lora/gateway/db/query" -m '{
  "action": "get_latest",
  "node_id": 1,
  "limit": 10
}'

# Theo dõi dữ liệu
mosquitto_sub -t "lora/gateway/nodes/node1"
```

---

## Cấu trúc thư mục

```
DATN/
├── README.md                  # Hướng dẫn chi tiết
├── QUICKSTART.md             # File này
├── driverlora/               # LoRa driver
│   ├── driver.c
│   ├── lora.c
│   ├── lora.h
│   └── Makefile
├── lora_gateway/             # Gateway chính
│   ├── Makefile
│   ├── README.md
│   ├── include/              # Header files
│   │   ├── auto_control.h
│   │   ├── config.h
│   │   ├── database.h
│   │   ├── gateway.h
│   │   ├── json_parser.h
│   │   ├── lora.h
│   │   ├── mqtt.h
│   │   ├── types.h
│   │   └── utils.h
│   ├── src/                  # Source files
│   │   ├── auto_control.c
│   │   ├── database.c
│   │   ├── gateway.c
│   │   ├── json_parser.c
│   │   ├── lora.c
│   │   ├── main.c
│   │   ├── mqtt.c
│   │   └── utils.c
│   ├── obj/                  # Build objects (generated)
│   └── bin/                  # Binary (generated)
└── node cam bien/            # Node code
    └── nodecambien.ino
```

---

## Mô tả từng phần

### `include/types.h`
Định nghĩa tất cả các struct dữ liệu:
- `node_data_t` - Dữ liệu của mỗi node
- `actuator_state_t` - Trạng thái điều khiển
- `gateway_state_t` - Trạng thái gateway
- `database_state_t` - Trạng thái database

### `src/gateway.c`
Phần lõi:
- Xử lý gói tin JSON/Text từ node
- Gửi lệnh JSON tới node
- Điều khiển tự động dựa trên ngưỡng
- Interactive CLI

### `src/mqtt.c`
Kết nối MQTT:
- Nhận lệnh từ MQTT broker
- Publish dữ liệu sensor
- Xử lý truy vấn database

### `src/database.c`
SQLite database:
- Lưu dữ liệu sensor
- Lưu log điều khiển
- Truy vấn & thống kê

---

## Thường gặp

**Q: Không kết nối được MQTT?**
A: Kiểm tra mosquitto: `sudo systemctl status mosquitto`

**Q: LoRa module không tìm thấy?**
A: Kiểm tra `/dev/loraSPI1.0` tồn tại

**Q: Database bị khóa?**
A: Kill gateway cũ: `pkill -f "gateway"`

**Q: Quên lệnh?**
A: Gõ `help` trong interactive mode

---

## Liên kết hữu ích

- [LoRa Documentation](https://lora-alliance.org/)
- [MQTT Spec](https://mqtt.org/)
- [SQLite Docs](https://sqlite.org/docs.html)
- [BeagleBone Black Docs](https://beagleboard.org/black)

---

**Chúc mừng bắt đầu!** 🎉
