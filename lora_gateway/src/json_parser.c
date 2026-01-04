/*
 * src/json_parser.c - JSON Parsing Implementation
 * Handles parsing incoming JSON data and generating output
 */

#include "json_parser.h"
#include "gateway.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>

/*====================================================================
 * JSON PARSING - Parse incoming sensor data from nodes
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
    
    if (*node_id < 1 || *node_id > MAX_NODES) {
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
    
    // Format: "node:1,temp:25.5,hum:60.2,soil:2500,lux:450,rssi:-45"
    int matched = sscanf(data, "node:%d,temp:%f,hum:%f,soil:%d,lux:%d,rssi:%d",
                        &id, &t, &h, &s, &l, &rssi);
    
    if (matched >= 5 && id >= 1 && id <= MAX_NODES) {
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
 * JSON OUTPUT - Generate JSON file for web dashboard
 *====================================================================*/

void output_json_to_file(void) {
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
    
    // Create nodes object
    cJSON *nodes = cJSON_CreateObject();
    
    for (int i = 0; i < MAX_NODES; i++) {
        if (gateway.nodes[i].last_update > 0) {
            char node_key[16];
            snprintf(node_key, sizeof(node_key), "node%d", i+1);
            
            cJSON *node = cJSON_CreateObject();
            
            // Sensor data
            cJSON_AddNumberToObject(node, "temp", gateway.nodes[i].temperature);
            cJSON_AddNumberToObject(node, "humid", gateway.nodes[i].humidity);
            cJSON_AddNumberToObject(node, "light", gateway.nodes[i].light);
            cJSON_AddNumberToObject(node, "soil", gateway.nodes[i].soil_moisture);
            
            // Signal quality
            cJSON_AddNumberToObject(node, "rssi", gateway.nodes[i].last_rssi);
            cJSON_AddNumberToObject(node, "snr", gateway.nodes[i].last_snr);
            
            // Statistics
            cJSON_AddNumberToObject(node, "rx_count", gateway.nodes[i].rx_count);
            cJSON_AddNumberToObject(node, "tx_count", gateway.nodes[i].tx_count);
            cJSON_AddNumberToObject(node, "last_update", (double)gateway.nodes[i].last_update);
            
            // Actuators
            cJSON *actuators = cJSON_CreateObject();
            cJSON_AddNumberToObject(actuators, "fan", gateway.nodes[i].actuators.fan_state);
            cJSON_AddNumberToObject(actuators, "light", gateway.nodes[i].actuators.light_state);
            cJSON_AddNumberToObject(actuators, "pump", gateway.nodes[i].actuators.pump_state);
            cJSON_AddItemToObject(node, "actuators", actuators);
            
            // Auto mode status
            cJSON_AddBoolToObject(node, "auto_mode", gateway.nodes[i].thresholds.enabled);
            
            // Thresholds (if auto mode enabled)
            if (gateway.nodes[i].thresholds.enabled) {
                cJSON *thresholds = cJSON_CreateObject();
                
                cJSON *temp_th = cJSON_CreateObject();
                cJSON_AddNumberToObject(temp_th, "min", gateway.nodes[i].thresholds.temp_min);
                cJSON_AddNumberToObject(temp_th, "max", gateway.nodes[i].thresholds.temp_max);
                cJSON_AddItemToObject(thresholds, "temp", temp_th);
                
                cJSON *light_th = cJSON_CreateObject();
                cJSON_AddNumberToObject(light_th, "min", gateway.nodes[i].thresholds.light_min);
                cJSON_AddNumberToObject(light_th, "max", gateway.nodes[i].thresholds.light_max);
                cJSON_AddItemToObject(thresholds, "light", light_th);
                
                cJSON *soil_th = cJSON_CreateObject();
                cJSON_AddNumberToObject(soil_th, "min", gateway.nodes[i].thresholds.soil_min);
                cJSON_AddNumberToObject(soil_th, "max", gateway.nodes[i].thresholds.soil_max);
                cJSON_AddItemToObject(thresholds, "soil", soil_th);
                
                cJSON_AddItemToObject(node, "thresholds", thresholds);
            }
            
            cJSON_AddItemToObject(nodes, node_key, node);
        }
    }
    
    cJSON_AddItemToObject(root, "nodes", nodes);
    
    // Gateway statistics
    cJSON *gw_stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(gw_stats, "rx_nodata", gateway.rx_nodata);
    cJSON_AddNumberToObject(gw_stats, "rx_crc_error", gateway.rx_crc_error);
    cJSON_AddNumberToObject(gw_stats, "rx_crc_recovery", gateway.rx_crc_recovery);
    cJSON_AddNumberToObject(gw_stats, "json_parse_error", gateway.json_parse_error);
    cJSON_AddNumberToObject(gw_stats, "auto_commands", gateway.auto_commands);
    cJSON_AddNumberToObject(gw_stats, "mqtt_connected", gateway.mqtt_connected);
    cJSON_AddNumberToObject(gw_stats, "mqtt_publish_count", gateway.mqtt_publish_count);
    cJSON_AddNumberToObject(gw_stats, "mqtt_error_count", gateway.mqtt_error_count);
    
    cJSON_AddItemToObject(root, "gateway", gw_stats);
    
    // Pretty print JSON to file
    char *json_string = cJSON_Print(root);
    fprintf(fp, "%s", json_string);
    
    free(json_string);
    cJSON_Delete(root);
    fclose(fp);
}