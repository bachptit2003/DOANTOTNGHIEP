/*
 * src/mqtt.c - MQTT Implementation
 * Handles MQTT connection, callbacks, and message handling
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <mosquitto.h>

#include "mqtt.h"
#include "database.h"
#include "gateway.h"
#include "utils.h"

/* External globals */
extern gateway_state_t gateway;
extern database_state_t db_state;

/*====================================================================
 * MQTT CALLBACK FUNCTIONS
 *====================================================================*/

void mqtt_on_connect(struct mosquitto *mosq, void *obj, int rc) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    if (rc == 0) {
        printf("[%s]  MQTT Connected to %s:%d\n", timestamp, MQTT_BROKER, MQTT_PORT);
        gateway.mqtt_connected = 1;
        
        // Publish online status
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/status", MQTT_TOPIC_PREFIX);
        mosquitto_publish(mosq, NULL, topic, 6, "online", MQTT_QOS, true);
        
        printf("[%s]  MQTT Subscribing to control topics...\n", timestamp);
        
        // Subscribe to control commands
        snprintf(topic, sizeof(topic), "%s/control/#", MQTT_TOPIC_PREFIX);
        mosquitto_subscribe(mosq, NULL, topic, MQTT_QOS);
        printf("[%s]  MQTT Subscribed: %s\n", timestamp, topic);
        
        // Subscribe to text commands
        snprintf(topic, sizeof(topic), "%s/command", MQTT_TOPIC_PREFIX);
        mosquitto_subscribe(mosq, NULL, topic, MQTT_QOS);
        printf("[%s]  MQTT Subscribed (TEXT): %s\n", timestamp, topic);
        
        // Subscribe to DB query topic
        snprintf(topic, sizeof(topic), "%s/db/query", MQTT_TOPIC_PREFIX);
        mosquitto_subscribe(mosq, NULL, topic, MQTT_QOS);
        printf("[%s]  MQTT Subscribed (DB): %s\n", timestamp, topic);
    } else {
        printf("[%s]  MQTT Connect failed: %s\n", timestamp, mosquitto_connack_string(rc));
        gateway.mqtt_connected = 0;
    }
}

void mqtt_on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    printf("[%s]   MQTT Disconnected (rc=%d)\n", timestamp, rc);
    gateway.mqtt_connected = 0;
    
    if (gateway.running && rc != 0) {
        printf("[%s]   MQTT Auto-reconnect in %d seconds...\n", 
               timestamp, MQTT_RECONNECT_INTERVAL);
    }
}

void mqtt_on_publish(struct mosquitto *mosq, void *obj, int mid) {
    gateway.mqtt_publish_count++;
}

void mqtt_on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    printf("[%s]  MQTT RX: %s => %s\n", timestamp, msg->topic, (char*)msg->payload);
    
    char *topic = msg->topic;
    char *payload = (char*)msg->payload;
    
    // Handle database queries
    if (strstr(topic, "/db/query") != NULL) {
        if (db_state.db == NULL) {
            printf("[%s]   Database not available\n", timestamp);
            
            cJSON *error = cJSON_CreateObject();
            cJSON_AddBoolToObject(error, "success", false);
            cJSON_AddStringToObject(error, "error", "Database not available");
            char *error_str = cJSON_PrintUnformatted(error);
            
            char response_topic[128];
            snprintf(response_topic, sizeof(response_topic), "%s/db/response", MQTT_TOPIC_PREFIX);
            mosquitto_publish(mosq, NULL, response_topic, strlen(error_str), error_str, MQTT_QOS, false);
            
            free(error_str);
            cJSON_Delete(error);
            return;
        }
        
        cJSON *request = cJSON_Parse(payload);
        if (request == NULL) {
            printf("[%s]   Invalid JSON query\n", timestamp);
            return;
        }
        
        cJSON *action_json = cJSON_GetObjectItem(request, "action");
        cJSON *request_id_json = cJSON_GetObjectItem(request, "request_id");
        
        if (!cJSON_IsString(action_json)) {
            cJSON_Delete(request);
            return;
        }
        
        const char *action = action_json->valuestring;
        const char *request_id = cJSON_IsString(request_id_json) ? 
                                  request_id_json->valuestring : "unknown";
        
        printf("[%s]   DB Query: %s (ID: %s)\n", timestamp, action, request_id);
        
        char *data_str = NULL;
        
        if (strcmp(action, "get_latest") == 0) {
            cJSON *node_json = cJSON_GetObjectItem(request, "node_id");
            cJSON *limit_json = cJSON_GetObjectItem(request, "limit");
            
            int node_id = cJSON_IsNumber(node_json) ? node_json->valueint : 0;
            int limit = cJSON_IsNumber(limit_json) ? limit_json->valueint : 10;
            
            data_str = db_query_latest_sensors(node_id, limit);
        }
        else if (strcmp(action, "get_range") == 0) {
            cJSON *node_json = cJSON_GetObjectItem(request, "node_id");
            cJSON *hours_json = cJSON_GetObjectItem(request, "hours");
            
            int node_id = cJSON_IsNumber(node_json) ? node_json->valueint : 1;
            int hours = cJSON_IsNumber(hours_json) ? hours_json->valueint : 24;
            
            data_str = db_query_range_sensors(node_id, hours);
        }
        else if (strcmp(action, "get_aggregate") == 0) {
            cJSON *node_json = cJSON_GetObjectItem(request, "node_id");
            cJSON *hours_json = cJSON_GetObjectItem(request, "hours");
            
            int node_id = cJSON_IsNumber(node_json) ? node_json->valueint : 1;
            int hours = cJSON_IsNumber(hours_json) ? hours_json->valueint : 24;
            
            data_str = db_query_aggregate(node_id, hours);
        }
        else if (strcmp(action, "get_actuator_history") == 0) {
            cJSON *node_json = cJSON_GetObjectItem(request, "node_id");
            cJSON *limit_json = cJSON_GetObjectItem(request, "limit");
            
            int node_id = cJSON_IsNumber(node_json) ? node_json->valueint : 1;
            int limit = cJSON_IsNumber(limit_json) ? limit_json->valueint : 20;
            
            data_str = db_query_actuator_history(node_id, limit);
        }
        else if (strcmp(action, "get_stats") == 0) {
            data_str = db_query_stats();
        }
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", data_str != NULL);
        cJSON_AddStringToObject(response, "request_id", request_id);
        cJSON_AddStringToObject(response, "action", action);
        
        if (data_str != NULL) {
            cJSON *data_json = cJSON_Parse(data_str);
            if (data_json) {
                cJSON_AddItemToObject(response, "data", data_json);
            }
            free(data_str);
        } else {
            cJSON_AddStringToObject(response, "error", "Query failed");
        }
        
        char *response_str = cJSON_PrintUnformatted(response);
        char response_topic[128];
        snprintf(response_topic, sizeof(response_topic), "%s/db/response", MQTT_TOPIC_PREFIX);
        
        mosquitto_publish(mosq, NULL, response_topic, strlen(response_str), 
                         response_str, MQTT_QOS, false);
        
        printf("[%s]   DB Response sent (%zu bytes)\n", timestamp, strlen(response_str));
        
        free(response_str);
        cJSON_Delete(response);
        cJSON_Delete(request);
        
        return;
    }
    
    // Handle text commands
    if (strstr(topic, "/command") != NULL) {
        printf("[%s]  MQTT TEXT CMD: %s\n", timestamp, payload);
        
        int node_id;
        char val[32];
        float val1, val2;
        
        if (sscanf(payload, "fan %d %s", &node_id, val) == 2) {
            if (node_id >= 1 && node_id <= 3) {
                int node_idx = node_id - 1;
                if (!gateway.nodes[node_idx].thresholds.enabled) {
                    lora_send_command(node_id, "fan", val);
                    gateway.nodes[node_idx].actuators.fan_state = (strcmp(val, "on") == 0);
                    gateway.nodes[node_idx].tx_count++;
                } else {
                    printf("[%s]   Node %d is in AUTO mode\n", timestamp, node_id);
                }
            }
        }
        else if (sscanf(payload, "light %d %s", &node_id, val) == 2) {
            if (node_id >= 1 && node_id <= 3) {
                int node_idx = node_id - 1;
                if (!gateway.nodes[node_idx].thresholds.enabled) {
                    lora_send_command(node_id, "light", val);
                    gateway.nodes[node_idx].actuators.light_state = (strcmp(val, "on") == 0);
                    gateway.nodes[node_idx].tx_count++;
                } else {
                    printf("[%s]   Node %d is in AUTO mode\n", timestamp, node_id);
                }
            }
        }
        else if (sscanf(payload, "pump %d %s", &node_id, val) == 2) {
            if (node_id >= 1 && node_id <= 3) {
                int node_idx = node_id - 1;
                if (!gateway.nodes[node_idx].thresholds.enabled) {
                    lora_send_command(node_id, "pump", val);
                    gateway.nodes[node_idx].actuators.pump_state = (strcmp(val, "on") == 0);
                    gateway.nodes[node_idx].tx_count++;
                } else {
                    printf("[%s]   Node %d is in AUTO mode\n", timestamp, node_id);
                }
            }
        }
        else if (sscanf(payload, "all %d %s", &node_id, val) == 2) {
            if (node_id >= 1 && node_id <= 3) {
                int node_idx = node_id - 1;
                if (!gateway.nodes[node_idx].thresholds.enabled) {
                    lora_send_command(node_id, "all", val);
                    int state = (strcmp(val, "on") == 0);
                    gateway.nodes[node_idx].actuators.fan_state = state;
                    gateway.nodes[node_idx].actuators.light_state = state;
                    gateway.nodes[node_idx].actuators.pump_state = state;
                    gateway.nodes[node_idx].tx_count++;
                } else {
                    printf("[%s]   Node %d is in AUTO mode\n", timestamp, node_id);
                }
            }
        }
        else if (sscanf(payload, "auto %d %s", &node_id, val) == 2) {
            if (node_id >= 1 && node_id <= 3) {
                int node_idx = node_id - 1;
                int enable = (strcmp(val, "on") == 0);
                gateway.nodes[node_idx].thresholds.enabled = enable;
                printf("[%s]   Node %d AUTO mode %s\n", 
                       timestamp, node_id, enable ? "ENABLED" : "DISABLED");
                
                if (!enable) {
                    lora_send_command(node_id, "all", "off");
                    gateway.nodes[node_idx].actuators.fan_state = 0;
                    gateway.nodes[node_idx].actuators.light_state = 0;
                    gateway.nodes[node_idx].actuators.pump_state = 0;
                    gateway.nodes[node_idx].tx_count++;
                }
            }
        }
        else if (sscanf(payload, "settemp %d %f %f", &node_id, &val1, &val2) == 3) {
            if (node_id >= 1 && node_id <= 3) {
                gateway.nodes[node_id-1].thresholds.temp_min = val1;
                gateway.nodes[node_id-1].thresholds.temp_max = val2;
                printf("[%s]   Node %d temp threshold: [%.1f, %.1f]°C\n", 
                       timestamp, node_id, val1, val2);
            }
        }
        else if (sscanf(payload, "setlight %d %f %f", &node_id, &val1, &val2) == 3) {
            if (node_id >= 1 && node_id <= 3) {
                gateway.nodes[node_id-1].thresholds.light_min = (uint16_t)val1;
                gateway.nodes[node_id-1].thresholds.light_max = (uint16_t)val2;
                printf("[%s]   Node %d light threshold: [%u, %u] lux\n", 
                       timestamp, node_id, (uint16_t)val1, (uint16_t)val2);
            }
        }
        else if (sscanf(payload, "setsoil %d %f %f", &node_id, &val1, &val2) == 3) {
            if (node_id >= 1 && node_id <= 3) {
                gateway.nodes[node_id-1].thresholds.soil_min = (uint16_t)val1;
                gateway.nodes[node_id-1].thresholds.soil_max = (uint16_t)val2;
                printf("[%s]   Node %d soil threshold: [%u, %u]\n", 
                       timestamp, node_id, (uint16_t)val1, (uint16_t)val2);
            }
        }
        else {
            printf("[%s]   Unknown TEXT command: %s\n", timestamp, payload);
        }
        
        return;
    }
    
    // Handle structured control commands
    int node_id = 0;
    char command[32] = {0};
    char value[32] = {0};
    
    if (sscanf(topic, "lora/gateway/control/node%d/%s", &node_id, command) == 2) {
        strncpy(value, payload, sizeof(value) - 1);
        
        printf("[%s]  MQTT CMD: Node%d %s=%s\n", timestamp, node_id, command, value);
        
        if (node_id >= 1 && node_id <= 3) {
            int node_idx = node_id - 1;
            
            if (gateway.nodes[node_idx].thresholds.enabled && 
                strcmp(command, "auto") != 0) {
                printf("[%s]   Node %d is in AUTO mode, ignoring manual command\n", 
                       timestamp, node_id);
                return;
            }
            
            if (strcmp(command, "fan") == 0) {
                lora_send_command(node_id, "fan", value);
                gateway.nodes[node_idx].actuators.fan_state = (strcmp(value, "on") == 0);
                gateway.nodes[node_idx].tx_count++;
                db_log_command(node_id, "fan", value, "MQTT");
            }
            else if (strcmp(command, "light") == 0) {
                lora_send_command(node_id, "light", value);
                gateway.nodes[node_idx].actuators.light_state = (strcmp(value, "on") == 0);
                gateway.nodes[node_idx].tx_count++;
                db_log_command(node_id, "light", value, "MQTT");
            }
            else if (strcmp(command, "pump") == 0) {
                lora_send_command(node_id, "pump", value);
                gateway.nodes[node_idx].actuators.pump_state = (strcmp(value, "on") == 0);
                gateway.nodes[node_idx].tx_count++;
                db_log_command(node_id, "pump", value, "MQTT");
            }
            else if (strcmp(command, "all") == 0) {
                lora_send_command(node_id, "all", value);
                int state = (strcmp(value, "on") == 0);
                gateway.nodes[node_idx].actuators.fan_state = state;
                gateway.nodes[node_idx].actuators.light_state = state;
                gateway.nodes[node_idx].actuators.pump_state = state;
                gateway.nodes[node_idx].tx_count++;
                db_log_command(node_id, "all", value, "MQTT");
            }
            else if (strcmp(command, "auto") == 0) {
                int enable = (strcmp(value, "on") == 0);
                gateway.nodes[node_idx].thresholds.enabled = enable;
                printf("[%s]   Node %d AUTO mode %s\n", 
                       timestamp, node_id, enable ? "ENABLED" : "DISABLED");
                
                if (!enable) {
                    lora_send_command(node_id, "all", "off");
                    gateway.nodes[node_idx].actuators.fan_state = 0;
                    gateway.nodes[node_idx].actuators.light_state = 0;
                    gateway.nodes[node_idx].actuators.pump_state = 0;
                }
            }
        }
    }
    // Handle threshold settings
    else if (sscanf(topic, "lora/gateway/control/node%d/threshold/%s", 
                    &node_id, command) == 2) {
        if (node_id >= 1 && node_id <= 3) {
            int node_idx = node_id - 1;
            float min_val, max_val;
            
            if (sscanf(payload, "%f,%f", &min_val, &max_val) == 2) {
                if (strcmp(command, "temp") == 0) {
                    gateway.nodes[node_idx].thresholds.temp_min = min_val;
                    gateway.nodes[node_idx].thresholds.temp_max = max_val;
                    printf("[%s]   Node %d temp threshold: [%.1f, %.1f]°C\n", 
                           timestamp, node_id, min_val, max_val);
                }
                else if (strcmp(command, "light") == 0) {
                    gateway.nodes[node_idx].thresholds.light_min = (uint16_t)min_val;
                    gateway.nodes[node_idx].thresholds.light_max = (uint16_t)max_val;
                    printf("[%s]   Node %d light threshold: [%u, %u] lux\n", 
                           timestamp, node_id, (uint16_t)min_val, (uint16_t)max_val);
                }
                else if (strcmp(command, "soil") == 0) {
                    gateway.nodes[node_idx].thresholds.soil_min = (uint16_t)min_val;
                    gateway.nodes[node_idx].thresholds.soil_max = (uint16_t)max_val;
                    printf("[%s]   Node %d soil threshold: [%u, %u]\n", 
                           timestamp, node_id, (uint16_t)min_val, (uint16_t)max_val);
                }
            }
        }
    }
}

/*====================================================================
 * MQTT INITIALIZATION
 *====================================================================*/

int mqtt_init() {
    int rc;
    
    printf("\n╔════════════════════════════════════╗\n");
    printf("║   MQTT Initialization              ║\n");
    printf("╚════════════════════════════════════╝\n\n");
    
    mosquitto_lib_init();
    
    gateway.mqtt = mosquitto_new("lora_gateway", true, NULL);
    if (!gateway.mqtt) {
        printf(" Failed to create MQTT instance\n");
        return -1;
    }
    
    mosquitto_connect_callback_set(gateway.mqtt, mqtt_on_connect);
    mosquitto_disconnect_callback_set(gateway.mqtt, mqtt_on_disconnect);
    mosquitto_publish_callback_set(gateway.mqtt, mqtt_on_publish);
    mosquitto_message_callback_set(gateway.mqtt, mqtt_on_message);
    
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/status", MQTT_TOPIC_PREFIX);
    mosquitto_will_set(gateway.mqtt, topic, 7, "offline", MQTT_QOS, true);
    
    printf(" Connecting to MQTT broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);
    rc = mosquitto_connect(gateway.mqtt, MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE);
    
    if (rc != MOSQ_ERR_SUCCESS) {
        printf(" MQTT connect failed: %s\n", mosquitto_strerror(rc));
        return -1;
    }
    
    rc = mosquitto_loop_start(gateway.mqtt);
    if (rc != MOSQ_ERR_SUCCESS) {
        printf(" MQTT loop start failed: %s\n", mosquitto_strerror(rc));
        return -1;
    }
    
    printf(" MQTT initialized\n\n");
    
    sleep(1);
    
    return 0;
}

/*====================================================================
 * MQTT PUBLISH FUNCTIONS
 *====================================================================*/

void mqtt_publish_node_data(int node_id) {
    if (!gateway.mqtt_connected || node_id < 1 || node_id > 3) {
        return;
    }
    
    int node_idx = node_id - 1;
    node_data_t *node = &gateway.nodes[node_idx];
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "node_id", node_id);
    cJSON_AddNumberToObject(root, "timestamp", (double)node->last_update);
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors, "temperature", node->temperature);
    cJSON_AddNumberToObject(sensors, "humidity", node->humidity);
    cJSON_AddNumberToObject(sensors, "light", node->light);
    cJSON_AddNumberToObject(sensors, "soil_moisture", node->soil_moisture);
    cJSON_AddItemToObject(root, "sensors", sensors);
    
    cJSON *actuators = cJSON_CreateObject();
    cJSON_AddNumberToObject(actuators, "fan", node->actuators.fan_state);
    cJSON_AddNumberToObject(actuators, "light", node->actuators.light_state);
    cJSON_AddNumberToObject(actuators, "pump", node->actuators.pump_state);
    cJSON_AddItemToObject(root, "actuators", actuators);
    
    cJSON *signal = cJSON_CreateObject();
    cJSON_AddNumberToObject(signal, "rssi", node->last_rssi);
    cJSON_AddNumberToObject(signal, "snr", node->last_snr);
    cJSON_AddItemToObject(root, "signal", signal);
    
    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "rx_count", node->rx_count);
    cJSON_AddNumberToObject(stats, "tx_count", node->tx_count);
    cJSON_AddItemToObject(root, "stats", stats);
    
    cJSON_AddBoolToObject(root, "auto_mode", node->thresholds.enabled);
    
    char *json_string = cJSON_PrintUnformatted(root);
    
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/nodes/node%d", MQTT_TOPIC_PREFIX, node_id);
    
    int rc = mosquitto_publish(gateway.mqtt, NULL, topic, 
                              strlen(json_string), json_string, 
                              MQTT_QOS, false);
    
    if (rc != MOSQ_ERR_SUCCESS) {
        gateway.mqtt_error_count++;
    }
    
    free(json_string);
    cJSON_Delete(root);
}

void mqtt_publish_gateway_stats() {
    if (!gateway.mqtt_connected) {
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
    cJSON_AddNumberToObject(root, "rx_nodata", gateway.rx_nodata);
    cJSON_AddNumberToObject(root, "rx_crc_error", gateway.rx_crc_error);
    cJSON_AddNumberToObject(root, "rx_crc_recovery", gateway.rx_crc_recovery);
    cJSON_AddNumberToObject(root, "json_parse_error", gateway.json_parse_error);
    cJSON_AddNumberToObject(root, "auto_commands", gateway.auto_commands);
    cJSON_AddNumberToObject(root, "mqtt_publish_count", gateway.mqtt_publish_count);
    cJSON_AddNumberToObject(root, "mqtt_error_count", gateway.mqtt_error_count);
    
    char *json_string = cJSON_PrintUnformatted(root);
    
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/stats", MQTT_TOPIC_PREFIX);
    
    mosquitto_publish(gateway.mqtt, NULL, topic, 
                     strlen(json_string), json_string, 
                     MQTT_QOS, false);
    
    free(json_string);
    cJSON_Delete(root);
}
