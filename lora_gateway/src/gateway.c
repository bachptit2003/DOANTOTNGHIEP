/*
 * src/gateway.c - Gateway Core Logic
 * Main gateway processing, packet handling, and interactive mode
 */

#include "gateway.h"
#include "mqtt.h"
#include "database.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <cjson/cJSON.h>

/* External globals */
extern gateway_state_t gateway;
extern database_state_t db_state;

/* LoRa IOCTL commands */
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

#define DEVICE_PATH         "/dev/loraSPI1.0"
#define FREQUENCY           433000000
#define TX_POWER            17
#define BANDWIDTH           125000
#define SPREADING_FACTOR    512
#define MAX_PACKET_SIZE     255
#define RX_POLL_INTERVAL    50
#define TX_WAIT_TIME        80
#define STATS_INTERVAL      30

/*====================================================================
 * JSON PARSING - NEW FUNCTION
 *====================================================================*/

int parse_json_sensor_data(const char *data, int *node_id, float *temp, 
                          float *hum, uint16_t *soil, uint16_t *lux,
                          actuator_state_t *actuators) {
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        gateway.json_parse_error++;
        return 0;
    }
    
    // Extract node ID
    cJSON *node = cJSON_GetObjectItem(json, "node");
    if (!cJSON_IsNumber(node)) {
        cJSON_Delete(json);
        return 0;
    }
    *node_id = node->valueint;
    
    if (*node_id < 1 || *node_id > 3) {
        cJSON_Delete(json);
        return 0;
    }
    
    // Extract sensor values
    cJSON *temp_json = cJSON_GetObjectItem(json, "temp");
    cJSON *hum_json = cJSON_GetObjectItem(json, "hum");
    cJSON *soil_json = cJSON_GetObjectItem(json, "soil");
    cJSON *lux_json = cJSON_GetObjectItem(json, "lux");
    
    if (cJSON_IsNumber(temp_json)) *temp = (float)temp_json->valuedouble;
    if (cJSON_IsNumber(hum_json)) *hum = (float)hum_json->valuedouble;
    if (cJSON_IsNumber(soil_json)) *soil = (uint16_t)soil_json->valueint;
    if (cJSON_IsNumber(lux_json)) *lux = (uint16_t)lux_json->valueint;
    
    // Extract actuator states (optional)
    cJSON *act = cJSON_GetObjectItem(json, "act");
    if (act != NULL) {
        cJSON *pump = cJSON_GetObjectItem(act, "pump");
        cJSON *fan = cJSON_GetObjectItem(act, "fan");
        cJSON *light = cJSON_GetObjectItem(act, "light");
        
        if (cJSON_IsNumber(pump)) actuators->pump_state = pump->valueint;
        if (cJSON_IsNumber(fan)) actuators->fan_state = fan->valueint;
        if (cJSON_IsNumber(light)) actuators->light_state = light->valueint;
    }
    
    cJSON_Delete(json);
    return 1;
}

/*====================================================================
 * FALLBACK: OLD TEXT FORMAT PARSER
 *====================================================================*/

int parse_text_sensor_data(const char *data, int *node_id, float *temp, 
                          float *hum, uint16_t *soil, uint16_t *lux) {
    int id, rssi;
    float t, h;
    int s, l;
    
    int matched = sscanf(data, "node:%d,temp:%f,hum:%f,soil:%d,lux:%d,rssi:%d",
                        &id, &t, &h, &s, &l, &rssi);
    
    if (matched >= 5 && id >= 1 && id <= 3) {
        *node_id = id;
        *temp = t;
        *hum = h;
        *soil = (uint16_t)s;
        *lux = (uint16_t)l;
        return 1;
    }
    
    return 0;
}

/*====================================================================
 * LORA FUNCTIONS
 *====================================================================*/

int lora_init() {
    uint32_t freq, bw, sf, agc, state;
    int32_t power;
    
    printf("\n╔═════════════════════════════════════╗\n");
    printf("║   Gateway Init (JSON Mode)          ║\n");
    printf("╚═════════════════════════════════════╝\n\n");
    
    gateway.lora_fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
    if (gateway.lora_fd < 0) {
        perror("Failed to open LoRa device");
        return -1;
    }
    printf("✓ Device opened: %s\n", DEVICE_PATH);
    
    state = LORA_STATE_STANDBY;
    ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    
    freq = FREQUENCY;
    ioctl(gateway.lora_fd, LORA_SET_FREQUENCY, &freq);
    printf("✓ Frequency: %.3f MHz\n", freq / 1000000.0);
    
    power = TX_POWER;
    ioctl(gateway.lora_fd, LORA_SET_POWER, &power);
    printf("✓ TX Power: %d dBm\n", power);
    
    bw = BANDWIDTH;
    ioctl(gateway.lora_fd, LORA_SET_BANDWIDTH, &bw);
    printf("✓ Bandwidth: %.1f kHz\n", bw / 1000.0);
    
    sf = SPREADING_FACTOR;
    ioctl(gateway.lora_fd, LORA_SET_SPRFACTOR, &sf);
    printf("✓ Spreading Factor: %u\n", sf);
    
    agc = 1;
    ioctl(gateway.lora_fd, LORA_SET_LNAAGC, &agc);
    printf("✓ LNA AGC: enabled\n");
    
    state = LORA_STATE_RX;
    ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
    printf("✓ LoRa in RX mode\n\n");
    
    return 0;
}

// Send command as TEXT
int lora_send_command_text(const char *cmd_str) {
    uint32_t state;
    int ret;
    char timestamp[32];
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    state = LORA_STATE_STANDBY;
    ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    
    ret = write(gateway.lora_fd, cmd_str, strlen(cmd_str));
    if (ret > 0) {
        printf("[%s] TX TEXT (%d bytes): %s\n", timestamp, ret, cmd_str);
    } else {
        printf("[%s] TX failed: %s\n", timestamp, strerror(errno));
    }
    
    usleep(TX_WAIT_TIME * 1000);
    
    state = LORA_STATE_RX;
    ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
    
    return ret;
}

// Send command as JSON
int lora_send_command_json(int node_id, const char *cmd, const char *val) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "node", node_id);
    cJSON_AddStringToObject(json, "cmd", cmd);
    cJSON_AddStringToObject(json, "val", val);
    
    char *json_string = cJSON_PrintUnformatted(json);
    
    int ret;
    char timestamp[32];
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    ret = write(gateway.lora_fd, json_string, strlen(json_string));
    
    if (ret > 0) {
        printf("[%s] ✓ TX (%d bytes): %s\n", timestamp, ret, json_string);
    } else {
        printf("[%s] ✗ TX failed: %s\n", timestamp, strerror(errno));
    }
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// Wrapper: Use JSON by default
int lora_send_command(int node_id, const char *cmd, const char *val) {
    return lora_send_command_json(node_id, cmd, val);
}

void lora_clear_and_restart_rx() {
    uint32_t state;
    state = LORA_STATE_STANDBY;
    ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    state = LORA_STATE_RX;
    ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
}

/*====================================================================
 * JSON OUTPUT - WEB DASHBOARD READY
 *====================================================================*/

void output_json_to_file() {
    FILE *fp = fopen("/tmp/gateway_data.json", "w");
    if (!fp) {
        perror("Failed to open JSON file");
        return;
    }
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "timestamp", timestamp);
    cJSON_AddNumberToObject(root, "unix_time", (double)time(NULL));
    
    // Nodes array
    cJSON *nodes = cJSON_CreateObject();
    
    for (int i = 0; i < 3; i++) {
        if (gateway.nodes[i].last_update > 0) {
            char node_key[16];
            snprintf(node_key, sizeof(node_key), "node%d", i+1);
            
            cJSON *node = cJSON_CreateObject();
            cJSON_AddNumberToObject(node, "temp", gateway.nodes[i].temperature);
            cJSON_AddNumberToObject(node, "humid", gateway.nodes[i].humidity);
            cJSON_AddNumberToObject(node, "light", gateway.nodes[i].light);
            cJSON_AddNumberToObject(node, "soil", gateway.nodes[i].soil_moisture);
            cJSON_AddNumberToObject(node, "rssi", gateway.nodes[i].last_rssi);
            cJSON_AddNumberToObject(node, "snr", gateway.nodes[i].last_snr);
            cJSON_AddNumberToObject(node, "rx_count", gateway.nodes[i].rx_count);
            cJSON_AddNumberToObject(node, "tx_count", gateway.nodes[i].tx_count);
            cJSON_AddNumberToObject(node, "last_update", (double)gateway.nodes[i].last_update);
            
            // Actuators
            cJSON *actuators = cJSON_CreateObject();
            cJSON_AddNumberToObject(actuators, "fan", gateway.nodes[i].actuators.fan_state);
            cJSON_AddNumberToObject(actuators, "light", gateway.nodes[i].actuators.light_state);
            cJSON_AddNumberToObject(actuators, "pump", gateway.nodes[i].actuators.pump_state);
            cJSON_AddItemToObject(node, "actuators", actuators);
            
            // Auto mode
            cJSON_AddBoolToObject(node, "auto_mode", gateway.nodes[i].thresholds.enabled);
            
            cJSON_AddItemToObject(nodes, node_key, node);
        }
    }
    
    cJSON_AddItemToObject(root, "nodes", nodes);
    
    // Gateway stats
    cJSON *gw_stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(gw_stats, "rx_nodata", gateway.rx_nodata);
    cJSON_AddNumberToObject(gw_stats, "rx_crc_error", gateway.rx_crc_error);
    cJSON_AddNumberToObject(gw_stats, "rx_crc_recovery", gateway.rx_crc_recovery);
    cJSON_AddNumberToObject(gw_stats, "json_parse_error", gateway.json_parse_error);
    cJSON_AddNumberToObject(gw_stats, "auto_commands", gateway.auto_commands);
    cJSON_AddItemToObject(root, "gateway", gw_stats);
    
    // Pretty print JSON
    char *json_string = cJSON_Print(root);
    fprintf(fp, "%s", json_string);
    
    free(json_string);
    cJSON_Delete(root);
    fclose(fp);
}

/*====================================================================
 * EDGE COMPUTING - AUTO CONTROL WITH DATABASE LOGGING
 *====================================================================*/

void check_auto_control(int node_id, float temp, float hum, uint16_t light, uint16_t soil) {
    if (node_id < 1 || node_id > 3) return;
    
    int node_idx = node_id - 1;
    node_data_t *node = &gateway.nodes[node_idx];
    threshold_config_t *th = &node->thresholds;
    
    if (!th->enabled) return;
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    // FAN CONTROL - Temperature-based
    int temp_outside = (temp < th->temp_min) || (temp > th->temp_max);
    
    if (temp_outside && node->actuators.fan_state == 0) {
        printf("[%s] [AUTO] Node %d: Temp %.1f°C OUT [%.1f,%.1f] → FAN ON\n",
               timestamp, node_id, temp, th->temp_min, th->temp_max);
        
        lora_send_command(node_id, "fan", "on");
        node->actuators.fan_state = 1;
        node->tx_count++;
        gateway.auto_commands++;
        
        db_log_actuator_change(node_id, "fan", 1, "AUTO", temp);
        
        usleep(500000);
    } 
    else if (!temp_outside && node->actuators.fan_state == 1) {
        printf("[%s] [AUTO] Node %d: Temp %.1f°C IN [%.1f,%.1f] → FAN OFF\n",
               timestamp, node_id, temp, th->temp_min, th->temp_max);
        
        lora_send_command(node_id, "fan", "off");
        node->actuators.fan_state = 0;
        node->tx_count++;
        gateway.auto_commands++;
        
        db_log_actuator_change(node_id, "fan", 0, "AUTO", temp);
        
        usleep(500000);
    }
    
    // LIGHT CONTROL - Light intensity-based
    int light_outside = (light < th->light_min) || (light > th->light_max);
    
    if (light_outside && node->actuators.light_state == 0) {
        printf("[%s] [AUTO] Node %d: Light %u OUT [%u,%u] → LIGHT ON\n",
               timestamp, node_id, light, th->light_min, th->light_max);
        
        lora_send_command(node_id, "light", "on");
        node->actuators.light_state = 1;
        node->tx_count++;
        gateway.auto_commands++;
        
        db_log_actuator_change(node_id, "light", 1, "AUTO", (float)light);
        
        usleep(500000);
    } 
    else if (!light_outside && node->actuators.light_state == 1) {
        printf("[%s] [AUTO] Node %d: Light %u IN [%u,%u] → LIGHT OFF\n",
               timestamp, node_id, light, th->light_min, th->light_max);
        
        lora_send_command(node_id, "light", "off");
        node->actuators.light_state = 0;
        node->tx_count++;
        gateway.auto_commands++;
        
        db_log_actuator_change(node_id, "light", 0, "AUTO", (float)light);
        
        usleep(500000);
    }
    
    // PUMP CONTROL - Soil moisture-based
    int soil_outside = (soil < th->soil_min) || (soil > th->soil_max);
    
    if (soil_outside && node->actuators.pump_state == 0) {
        printf("[%s] [AUTO] Node %d: Soil %u OUT [%u,%u] → PUMP ON\n",
               timestamp, node_id, soil, th->soil_min, th->soil_max);
        
        lora_send_command(node_id, "pump", "on");
        node->actuators.pump_state = 1;
        node->tx_count++;
        gateway.auto_commands++;
        
        db_log_actuator_change(node_id, "pump", 1, "AUTO", (float)soil);
        
        usleep(500000);
    } 
    else if (!soil_outside && node->actuators.pump_state == 1) {
        printf("[%s] [AUTO] Node %d: Soil %u IN [%u,%u] → PUMP OFF\n",
               timestamp, node_id, soil, th->soil_min, th->soil_max);
        
        lora_send_command(node_id, "pump", "off");
        node->actuators.pump_state = 0;
        node->tx_count++;
        gateway.auto_commands++;
        
        db_log_actuator_change(node_id, "pump", 0, "AUTO", (float)soil);
        
        usleep(500000);
    }
}

/*====================================================================
 * PROCESS SENSOR PACKET - JSON AWARE
 *====================================================================*/

void process_sensor_packet(const char *data, int len) {
    int node_id;
    float temp = 0.0, hum = 0.0;
    uint16_t soil = 0, lux = 0;
    actuator_state_t actuators = {0};
    char timestamp[32];
    int32_t rssi, snr;
    
    // Try JSON first
    int success = parse_json_sensor_data(data, &node_id, &temp, &hum, 
                                        &soil, &lux, &actuators);
    
    // Fallback to text format
    if (!success) {
        success = parse_text_sensor_data(data, &node_id, &temp, &hum, &soil, &lux);
        if (!success) return;
    }
    
    if (node_id < 1 || node_id > 3) return;
    
    int node_idx = node_id - 1;
    
    get_timestamp(timestamp, sizeof(timestamp));
    ioctl(gateway.lora_fd, LORA_GET_RSSI, &rssi);
    ioctl(gateway.lora_fd, LORA_GET_SNR, &snr);
    
    printf("[%s] RX Node %d: T=%.1f°C H=%.1f%% L=%u S=%u [RSSI:%d SNR:%d]\n",
           timestamp, node_id, temp, hum, lux, soil, rssi, snr);
    
    gateway.nodes[node_idx].temperature = temp;
    gateway.nodes[node_idx].humidity = hum;
    gateway.nodes[node_idx].light = lux;
    gateway.nodes[node_idx].soil_moisture = soil;
    gateway.nodes[node_idx].last_update = time(NULL);
    gateway.nodes[node_idx].rx_count++;
    gateway.nodes[node_idx].last_rssi = rssi;
    gateway.nodes[node_idx].last_snr = snr;
    
    db_save_sensor_data(node_id, temp, hum, lux, soil, rssi, snr);
    
    // Update actuator states if present
    gateway.nodes[node_idx].actuators = actuators;
    
    output_json_to_file();
    mqtt_publish_node_data(node_id);
    check_auto_control(node_id, temp, hum, lux, soil);
}

/*====================================================================
 * INTERACTIVE MODE - USER COMMANDS
 *====================================================================*/

void print_help() {
    printf("\n╔═════════════════════════════════════╗\n");
    printf("║      Available Commands (JSON)      ║\n");
    printf("╚═════════════════════════════════════╝\n\n");
    printf("MANUAL CONTROL (JSON Format):\n");
    printf("  fan <node> <on|off>     - Control fan\n");
    printf("  light <node> <on|off>   - Control light\n");
    printf("  pump <node> <on|off>    - Control pump\n");
    printf("  all <node> <on|off>     - Control all\n");
    printf("  Example: fan 1 on  →  {\"node\":1,\"cmd\":\"fan\",\"val\":\"on\"}\n");
    printf("\n");
    printf("AUTO CONTROL:\n");
    printf("  auto <node> <on|off>    - Enable/disable auto\n");
    printf("  settemp <node> <min> <max>    - Set temp range\n");
    printf("  setlight <node> <min> <max>   - Set light range\n");
    printf("  setsoil <node> <min> <max>    - Set soil range\n");
    printf("\n");
    printf("MONITORING:\n");
    printf("  status                  - Show all nodes\n");
    printf("  stats                   - Show statistics\n");
    printf("\n");
    printf("DATABASE:\n");
    printf("  dbshow <node> [limit]   - Show recent data\n");
    printf("  dbstats                 - Show database stats\n");
    printf("  dbclean <days>          - Clean old data (keep N days)\n");
    printf("  dbbackup                - Backup database now\n");
    printf("\n");
    printf("SYSTEM:\n");
    printf("  help                    - Show this help\n");
    printf("  exit                    - Exit gateway\n\n");
}

void print_status() {
    time_t now = time(NULL);
    
    printf("\n╔═════════════════════════════════════╗\n");
    printf("║         Gateway Status (JSON)       ║\n");
    printf("╚═════════════════════════════════════╝\n\n");
    
    for (int i = 0; i < 3; i++) {
        node_data_t *node = &gateway.nodes[i];
        if (node->last_update == 0) {
            printf("Node %d: No data yet\n\n", i + 1);
            continue;
        }
        
        int age = (int)(now - node->last_update);
        
        printf("Node %d:\n", i + 1);
        printf("  T:%.1f°C H:%.1f%% L:%u S:%u\n", 
               node->temperature, node->humidity, node->light, node->soil_moisture);
        printf("  Actuators: Fan=%s Light=%s Pump=%s\n",
               node->actuators.fan_state ? "ON" : "OFF",
               node->actuators.light_state ? "ON" : "OFF",
               node->actuators.pump_state ? "ON" : "OFF");
        printf("  Auto: %s, Last: %ds ago\n", 
               node->thresholds.enabled ? "ON" : "OFF", age);
        printf("  RX=%u TX=%u RSSI=%d dBm\n\n",
               node->rx_count, node->tx_count, node->last_rssi);
    }
}

void interactive_mode() {
    char rx_buffer[MAX_PACKET_SIZE + 1];
    char input[256];
    int node_id;
    float val1, val2;
    char arg1[64];
    fd_set readfds;
    struct timeval tv;
    int ret;
    int flags;
    
    printf("\n╔═════════════════════════════════════╗\n");
    printf("║   Gateway - JSON Command Mode       ║\n");
    printf("║   Commands sent as JSON packets     ║\n");
    printf("╚═════════════════════════════════════╝\n");
    printf("\nType 'help' for commands\n\n");
    
    // Set stdin non-blocking
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    gateway.loop_count = 0;
    gateway.last_stats_time = time(NULL);
    
    while (gateway.running) {
        gateway.loop_count++;
        
        // Read sensor data
        ret = read(gateway.lora_fd, rx_buffer, sizeof(rx_buffer) - 1);
        
        if (ret > 0) {
            rx_buffer[ret] = '\0';
            process_sensor_packet(rx_buffer, ret);
            
            ret = read(gateway.lora_fd, rx_buffer, sizeof(rx_buffer) - 1);
            if (ret > 0) {
                rx_buffer[ret] = '\0';
                process_sensor_packet(rx_buffer, ret);
            }
        } 
        else if (ret < 0) {
            if (errno == ENODATA) {
                gateway.rx_nodata++;
            }
            else if (errno == EBADMSG) {
                gateway.rx_crc_error++;
                if (gateway.rx_crc_error % 10 == 1) {
                    printf("CRC error (count: %u) - Recovering...\n", gateway.rx_crc_error);
                }
                lora_clear_and_restart_rx();
                ret = read(gateway.lora_fd, rx_buffer, sizeof(rx_buffer) - 1);
                if (ret > 0) {
                    rx_buffer[ret] = '\0';
                    process_sensor_packet(rx_buffer, ret);
                    gateway.rx_crc_recovery++;
                }
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                gateway.rx_other_error++;
            }
        }
        
        // Print stats
        time_t now = time(NULL);
        if (now - gateway.last_stats_time >= STATS_INTERVAL) {
            uint32_t total_rx = gateway.nodes[0].rx_count + 
                               gateway.nodes[1].rx_count + 
                               gateway.nodes[2].rx_count;
            
            printf("\n[STATS] Loops: %lu/%ds, RX: %u, JSON_ERR: %u, CRC: %u\n",
                   (unsigned long)gateway.loop_count, STATS_INTERVAL,
                   total_rx, gateway.json_parse_error, gateway.rx_crc_error);
            
            gateway.loop_count = 0;
            gateway.rx_nodata = 0;
            gateway.last_stats_time = now;
            mqtt_publish_gateway_stats();
            db_save_gateway_stats();
        }
        
        // Check user input
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                if (strlen(input) == 0) continue;
                
                if (strcmp(input, "exit") == 0) {
                    gateway.running = 0;
                    break;
                }
                else if (strcmp(input, "help") == 0) {
                    print_help();
                }
                else if (strcmp(input, "status") == 0) {
                    print_status();
                }
                else if (strcmp(input, "stats") == 0) {
                    printf("\nRX NODATA: %u\n", gateway.rx_nodata);
                    printf("RX CRC Errors: %u\n", gateway.rx_crc_error);
                    printf("RX CRC Recoveries: %u (%.1f%%)\n",
                           gateway.rx_crc_recovery,
                           gateway.rx_crc_error > 0 ? 
                           (100.0 * gateway.rx_crc_recovery / gateway.rx_crc_error) : 0.0);
                    printf("JSON Parse Errors: %u\n", gateway.json_parse_error);
                    printf("Auto Commands: %u\n\n", gateway.auto_commands);
                }
                
                // DATABASE COMMANDS
                else if (sscanf(input, "dbshow %d %d", &node_id, (int*)&val1) == 2) {
                    if (node_id >= 1 && node_id <= 3) {
                        db_show_recent_data(node_id, (int)val1);
                    }
                }
                else if (sscanf(input, "dbshow %d", &node_id) == 1) {
                    if (node_id >= 1 && node_id <= 3) {
                        db_show_recent_data(node_id, 10);
                    }
                }
                else if (strcmp(input, "dbstats") == 0) {
                    db_show_statistics();
                }
                else if (sscanf(input, "dbclean %d", &node_id) == 1) {
                    if (node_id > 0 && node_id <= 365) {
                        db_cleanup_old_data(node_id);
                    } else {
                        printf("Usage: dbclean <days>  (1-365)\n");
                    }
                }
                else if (strcmp(input, "dbbackup") == 0) {
                    db_backup();
                }
                
                // MANUAL CONTROL
                else if (sscanf(input, "fan %d %s", &node_id, arg1) == 2) {
                    if (node_id >= 1 && node_id <= 3) {
                        if (!gateway.nodes[node_id-1].thresholds.enabled) {
                            printf("→ Sending JSON: {\"node\":%d,\"cmd\":\"fan\",\"val\":\"%s\"}\n", 
                                   node_id, arg1);
                            lora_send_command(node_id, "fan", arg1);
                            gateway.nodes[node_id-1].actuators.fan_state = (strcmp(arg1, "on") == 0);
                            gateway.nodes[node_id-1].tx_count++;
                            
                            db_log_command(node_id, "fan", arg1, "USER");
                            db_log_actuator_change(node_id, "fan", 
                                (strcmp(arg1, "on") == 0) ? 1 : 0, 
                                "MANUAL", 0.0);
                        } else {
                            printf("⚠️  Node %d is in AUTO mode\n", node_id);
                        }
                    }
                }
                else if (sscanf(input, "light %d %s", &node_id, arg1) == 2) {
                    if (node_id >= 1 && node_id <= 3) {
                        if (!gateway.nodes[node_id-1].thresholds.enabled) {
                            printf("→ Sending JSON: {\"node\":%d,\"cmd\":\"light\",\"val\":\"%s\"}\n", 
                                   node_id, arg1);
                            lora_send_command(node_id, "light", arg1);
                            gateway.nodes[node_id-1].actuators.light_state = (strcmp(arg1, "on") == 0);
                            gateway.nodes[node_id-1].tx_count++;
                            
                            db_log_command(node_id, "light", arg1, "USER");
                            db_log_actuator_change(node_id, "light", 
                                (strcmp(arg1, "on") == 0) ? 1 : 0, 
                                "MANUAL", 0.0);
                        } else {
                            printf("⚠️  Node %d is in AUTO mode\n", node_id);
                        }
                    }
                }
                else if (sscanf(input, "pump %d %s", &node_id, arg1) == 2) {
                    if (node_id >= 1 && node_id <= 3) {
                        if (!gateway.nodes[node_id-1].thresholds.enabled) {
                            printf("→ Sending JSON: {\"node\":%d,\"cmd\":\"pump\",\"val\":\"%s\"}\n", 
                                   node_id, arg1);
                            lora_send_command(node_id, "pump", arg1);
                            gateway.nodes[node_id-1].actuators.pump_state = (strcmp(arg1, "on") == 0);
                            gateway.nodes[node_id-1].tx_count++;
                            
                            db_log_command(node_id, "pump", arg1, "USER");
                            db_log_actuator_change(node_id, "pump", 
                                (strcmp(arg1, "on") == 0) ? 1 : 0, 
                                "MANUAL", 0.0);
                        } else {
                            printf("⚠️  Node %d is in AUTO mode\n", node_id);
                        }
                    }
                }
                else if (sscanf(input, "all %d %s", &node_id, arg1) == 2) {
                    if (node_id >= 1 && node_id <= 3) {
                        if (!gateway.nodes[node_id-1].thresholds.enabled) {
                            printf("→ Sending JSON: {\"node\":%d,\"cmd\":\"all\",\"val\":\"%s\"}\n", 
                                   node_id, arg1);
                            lora_send_command(node_id, "all", arg1);
                            int state = (strcmp(arg1, "on") == 0);
                            gateway.nodes[node_id-1].actuators.fan_state = state;
                            gateway.nodes[node_id-1].actuators.light_state = state;
                            gateway.nodes[node_id-1].actuators.pump_state = state;
                            gateway.nodes[node_id-1].tx_count++;
                            
                            db_log_command(node_id, "all", arg1, "USER");
                            db_log_actuator_change(node_id, "fan", state, "MANUAL", 0.0);
                            db_log_actuator_change(node_id, "light", state, "MANUAL", 0.0);
                            db_log_actuator_change(node_id, "pump", state, "MANUAL", 0.0);
                        } else {
                            printf("⚠️  Node %d is in AUTO mode\n", node_id);
                        }
                    }
                }
                
                // AUTO CONTROL
                else if (sscanf(input, "auto %d %s", &node_id, arg1) == 2) {
                    if (node_id >= 1 && node_id <= 3) {
                        int enable = (strcmp(arg1, "on") == 0);
                        gateway.nodes[node_id-1].thresholds.enabled = enable;
                        printf("✓ Node %d AUTO mode %s\n", node_id, enable ? "ON" : "OFF");
                        
                        db_log_command(node_id, "auto", arg1, "USER");
                        
                        if (!enable) {
                            lora_send_command(node_id, "all", "off");
                            gateway.nodes[node_id-1].actuators.fan_state = 0;
                            gateway.nodes[node_id-1].actuators.light_state = 0;
                            gateway.nodes[node_id-1].actuators.pump_state = 0;
                            gateway.nodes[node_id-1].tx_count++;
                            
                            db_log_command(node_id, "all", "off", "USER");
                            db_log_actuator_change(node_id, "fan", 0, "MANUAL", 0.0);
                            db_log_actuator_change(node_id, "light", 0, "MANUAL", 0.0);
                            db_log_actuator_change(node_id, "pump", 0, "MANUAL", 0.0);
                        }
                    }
                }
                else if (sscanf(input, "settemp %d %f %f", &node_id, &val1, &val2) == 3) {
                    if (node_id >= 1 && node_id <= 3) {
                        gateway.nodes[node_id-1].thresholds.temp_min = val1;
                        gateway.nodes[node_id-1].thresholds.temp_max = val2;
                        printf("✓ Node %d temp: [%.1f, %.1f]°C\n", node_id, val1, val2);
                        
                        char val_str[64];
                        snprintf(val_str, sizeof(val_str), "%.1f,%.1f", val1, val2);
                        db_log_command(node_id, "settemp", val_str, "USER");
                    }
                }
                else if (sscanf(input, "setlight %d %f %f", &node_id, &val1, &val2) == 3) {
                    if (node_id >= 1 && node_id <= 3) {
                        gateway.nodes[node_id-1].thresholds.light_min = (uint16_t)val1;
                        gateway.nodes[node_id-1].thresholds.light_max = (uint16_t)val2;
                        printf("✓ Node %d light: [%u, %u] lux\n", node_id, (uint16_t)val1, (uint16_t)val2);
                        
                        char val_str[64];
                        snprintf(val_str, sizeof(val_str), "%u,%u", (uint16_t)val1, (uint16_t)val2);
                        db_log_command(node_id, "setlight", val_str, "USER");
                    }
                }
                else if (sscanf(input, "setsoil %d %f %f", &node_id, &val1, &val2) == 3) {
                    if (node_id >= 1 && node_id <= 3) {
                        gateway.nodes[node_id-1].thresholds.soil_min = (uint16_t)val1;
                        gateway.nodes[node_id-1].thresholds.soil_max = (uint16_t)val2;
                        printf("✓ Node %d soil: [%u, %u]\n", node_id, (uint16_t)val1, (uint16_t)val2);
                        
                        char val_str[64];
                        snprintf(val_str, sizeof(val_str), "%u,%u", (uint16_t)val1, (uint16_t)val2);
                        db_log_command(node_id, "setsoil", val_str, "USER");
                    }
                }
                else {
                    printf("Unknown command. Type 'help'\n");
                }
            }
        }
        
        usleep(RX_POLL_INTERVAL * 1000);
    }
    
    fcntl(STDIN_FILENO, F_SETFL, flags);
}
