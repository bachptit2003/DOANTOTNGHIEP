# BeagleBone Black LoRa Gateway

IoT Gateway system using LoRa communication protocol for sensor networks with automatic control, MQTT integration, and SQLite database.

## 🎯 Features

- **LoRa Communication**: Bi-directional communication with sensor nodes
- **JSON Protocol**: Modern JSON-based data format
- **MQTT Integration**: Publish data to MQTT broker for web dashboards
- **SQLite Database**: Store sensor data, actuator logs, and command history
- **Auto Control**: Edge computing for automatic actuator control based on thresholds
- **Web Dashboard**: Real-time JSON output for web interfaces

## 📁 Project Structure

```
lora_gateway/
├── include/          # Header files
│   ├── types.h      # Data structures
│   ├── config.h     # Configuration constants
│   ├── lora.h       # LoRa driver interface
│   ├── mqtt.h       # MQTT client interface
│   ├── database.h   # Database interface
│   ├── json_parser.h# JSON parsing
│   ├── gateway.h    # Gateway core
│   ├── auto_control.h# Auto control logic
│   └── utils.h      # Utilities
│
├── src/             # Source files
│   ├── main.c       # Entry point
│   ├── lora.c       # LoRa implementation
│   ├── mqtt.c       # MQTT implementation
│   ├── database.c   # Database operations
│   ├── json_parser.c# JSON parsing
│   ├── gateway.c    # Gateway logic
│   ├── auto_control.c# Auto control
│   └── utils.c      # Utilities
│
├── scripts/         # Helper scripts
├── data/            # Runtime data
│   └── backups/    # Database backups
├── Makefile         # Build script
├── .gitignore      # Git ignore
└── README.md       # This file
```

## 🚀 Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/yourusername/lora_gateway.git
cd lora_gateway
```

### 2. Install Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    gcc \
    make \
    libcjson-dev \
    libmosquitto-dev \
    libsqlite3-dev
```

### 3. Build

```bash
make
```

### 4. Run

```bash
sudo ./bin/gateway
```

## 📝 Configuration

Edit `include/config.h` to customize:

```c
#define DEVICE_PATH         "/dev/loraSPI1.0"
#define FREQUENCY           433000000
#define TX_POWER            14
#define MQTT_BROKER         "localhost"
#define MQTT_PORT           1883
#define DB_PATH             "/home/debian/lora_gateway.db"
```

## 🎮 Commands

### Manual Control

```bash
fan 1 on          # Turn on fan for node 1
light 2 off       # Turn off light for node 2
pump 3 on         # Turn on pump for node 3
all 1 off         # Turn off all actuators for node 1
```

### Auto Control

```bash
auto 1 on                    # Enable auto mode for node 1
settemp 1 20.0 28.0         # Set temperature range [20-28°C]
setlight 1 200 800          # Set light range [200-800 lux]
setsoil 1 1500 3000         # Set soil moisture range
```

### Database

```bash
dbshow 1 10        # Show last 10 records for node 1
dbstats            # Show database statistics
dbclean 30         # Clean data older than 30 days
dbbackup           # Backup database
```

### System

```bash
status             # Show all nodes status
stats              # Show gateway statistics
help               # Show help
exit               # Exit gateway
```

## 📡 MQTT Topics

### Subscribe (Control)

- `lora/gateway/control/node{id}/{command}` - Individual control
- `lora/gateway/command` - Text commands
- `lora/gateway/db/query` - Database queries

### Publish (Data)

- `lora/gateway/nodes/node{id}` - Node sensor data
- `lora/gateway/stats` - Gateway statistics
- `lora/gateway/status` - Gateway online/offline
- `lora/gateway/db/response` - Database query responses

### Example MQTT Commands

```bash
# Control fan via MQTT
mosquitto_pub -t "lora/gateway/control/node1/fan" -m "on"

# Query database
mosquitto_pub -t "lora/gateway/db/query" -m '{"action":"get_latest","node_id":1,"limit":10,"request_id":"req_001"}'

# Text command
mosquitto_pub -t "lora/gateway/command" -m "fan 1 on"
```

## 🗄️ Database Schema

### sensor_data
- `id`, `timestamp`, `node_id`
- `temperature`, `humidity`, `light`, `soil_moisture`
- `rssi`, `snr`

### actuator_logs
- `id`, `timestamp`, `node_id`, `actuator`
- `state`, `trigger_type`, `trigger_value`

### command_history
- `id`, `timestamp`, `node_id`
- `command`, `value`, `source`

### gateway_stats
- `id`, `timestamp`
- `rx_count`, `tx_count`, `crc_errors`, `json_errors`, `auto_commands`

## �� Development

### Build Commands

```bash
make           # Build project
make clean     # Clean build files
make install   # Install to /usr/local/bin
make uninstall # Remove from system
make help      # Show help
```

### Adding New Features

1. Add header in `include/`
2. Implement in `src/`
3. Update `Makefile` if needed
4. Test thoroughly

## 📊 JSON Data Format

### Node to Gateway (Sensor Data)

```json
{
  "node": 1,
  "temp": 25.5,
  "hum": 60.2,
  "soil": 2500,
  "lux": 450,
  "act": {
    "pump": 0,
    "fan": 1,
    "light": 0
  }
}
```

### Gateway to Node (Command)

```json
{
  "node": 1,
  "cmd": "fan",
  "val": "on"
}
```

### Web Dashboard Output (`/tmp/gateway_data.json`)

```json
{
  "timestamp": "14:30:25",
  "unix_time": 1704123025,
  "nodes": {
    "node1": {
      "temp": 25.5,
      "humid": 60.2,
      "light": 450,
      "soil": 2500,
      "rssi": -45,
      "snr": 10,
      "actuators": {
        "fan": 1,
        "light": 0,
        "pump": 0
      },
      "auto_mode": true
    }
  },
  "gateway": {
    "rx_crc_error": 5,
    "json_parse_error": 0,
    "auto_commands": 12
  }
}
```

## 🐛 Troubleshooting

### LoRa Device Not Found

```bash
# Check device
ls -l /dev/loraSPI1.0

# Check permissions
sudo chmod 666 /dev/loraSPI1.0
```

### MQTT Connection Failed

```bash
# Check MQTT broker
sudo systemctl status mosquitto

# Start MQTT broker
sudo systemctl start mosquitto
```

### Database Permission Error

```bash
# Fix permissions
sudo chown debian:debian /home/debian/lora_gateway.db
sudo chmod 644 /home/debian/lora_gateway.db
```

## 📄 License

MIT License - See LICENSE file for details

## �� Authors

- Your Name - Initial work

## 🤝 Contributing

1. Fork the project
2. Create feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open Pull Request

## 📞 Contact

Your Name - your.email@example.com

Project Link: https://github.com/yourusername/lora_gateway
