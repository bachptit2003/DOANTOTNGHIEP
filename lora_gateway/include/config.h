#ifndef __CONFIG_H__
#define __CONFIG_H__

// LoRa Configuration
#define DEVICE_PATH         "/dev/loraSPI1.0"
#define FREQUENCY           433000000
#define TX_POWER            17
#define BANDWIDTH           125000
#define SPREADING_FACTOR    512   // SF9
#define MAX_PACKET_SIZE     255

// Timing Configuration
#define RX_POLL_INTERVAL    50
#define TX_WAIT_TIME        80
#define STATS_INTERVAL      30

// MQTT Configuration
#define MQTT_BROKER         "localhost"
#define MQTT_PORT           1883
#define MQTT_KEEPALIVE      60
#define MQTT_TOPIC_PREFIX   "lora/gateway"
#define MQTT_QOS            1

// Database Configuration
#define DB_PATH             "/home/debian/lora_gateway.db"

// LoRa IOCTL commands
#define LORA_IOC_MAGIC      '\x74'
#define LORA_SET_STATE      (_IOW(LORA_IOC_MAGIC,  0, int))
#define LORA_GET_STATE      (_IOR(LORA_IOC_MAGIC,  1, int))
#define LORA_SET_FREQUENCY  (_IOW(LORA_IOC_MAGIC,  2, int))
#define LORA_GET_FREQUENCY  (_IOR(LORA_IOC_MAGIC,  3, int))
#define LORA_SET_POWER      (_IOW(LORA_IOC_MAGIC,  4, int))
#define LORA_GET_POWER      (_IOR(LORA_IOC_MAGIC,  5, int))
#define LORA_SET_BANDWIDTH  (_IOW(LORA_IOC_MAGIC, 11, int))
#define LORA_GET_BANDWIDTH  (_IOR(LORA_IOC_MAGIC, 12, int))
#define LORA_SET_SPRFACTOR  (_IOW(LORA_IOC_MAGIC,  9, int))
#define LORA_GET_SPRFACTOR  (_IOR(LORA_IOC_MAGIC, 10, int))
#define LORA_GET_RSSI       (_IOR(LORA_IOC_MAGIC, 13, int))
#define LORA_GET_SNR        (_IOR(LORA_IOC_MAGIC, 14, int))
#define LORA_SET_LNAAGC     (_IOW(LORA_IOC_MAGIC,  8, int))

#define LORA_STATE_SLEEP    0
#define LORA_STATE_STANDBY  1
#define LORA_STATE_TX       2
#define LORA_STATE_RX       3

#endif