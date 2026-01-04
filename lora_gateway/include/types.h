#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <time.h>
#include <sqlite3.h>
#include <mosquitto.h>

#define MAX_NODES 3

typedef struct {
    uint8_t fan_state;
    uint8_t light_state;
    uint8_t pump_state;
} actuator_state_t;

typedef struct {
    uint8_t enabled;
    float temp_min;
    float temp_max;
    uint16_t light_min;
    uint16_t light_max;
    uint16_t soil_min;
    uint16_t soil_max;
} threshold_config_t;

typedef struct {
    float temperature;
    float humidity;
    uint16_t light;
    uint16_t soil_moisture;
    time_t last_update;
    
    actuator_state_t actuators;
    threshold_config_t thresholds;
    
    uint32_t rx_count;
    uint32_t tx_count;
    int32_t last_rssi;
    int32_t last_snr;
} node_data_t;

typedef struct {
    int lora_fd;
    volatile int running;
    
    node_data_t nodes[MAX_NODES];
    
    uint32_t rx_nodata;
    uint32_t rx_crc_error;
    uint32_t rx_crc_recovery;
    uint32_t rx_other_error;
    uint32_t auto_commands;
    uint32_t json_parse_error;
    
    uint64_t loop_count;
    time_t last_stats_time;
    
    struct mosquitto *mqtt;
    int mqtt_connected;
    uint32_t mqtt_publish_count;
    uint32_t mqtt_error_count;
} gateway_state_t;

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *stmt_sensor;
    sqlite3_stmt *stmt_actuator;
    sqlite3_stmt *stmt_command;
    sqlite3_stmt *stmt_stats;
    
    uint32_t total_inserts;
    uint32_t insert_errors;
    time_t last_backup_time;
} database_state_t;

#endif // __TYPES_H__