/*
 * src/auto_control.c - Auto Control Logic
 * Edge computing - automatically control actuators based on thresholds
 */

#include "auto_control.h"
#include "gateway.h"
#include "lora.h"
#include "database.h"
#include "utils.h"
#include "config.h"
#include <stdio.h>
#include <unistd.h>

/*====================================================================
 * EDGE COMPUTING - AUTO CONTROL WITH DATABASE LOGGING
 * Automatically control actuators based on threshold
 * Log every change to database
 *====================================================================*/

void check_auto_control(int node_id, float temp, float hum, 
                       uint16_t light, uint16_t soil) {
    if (node_id < 1 || node_id > MAX_NODES) return;
    
    int node_idx = node_id - 1;
    node_data_t *node = &gateway.nodes[node_idx];
    threshold_config_t *th = &node->thresholds;
    
    // If auto mode is disabled, do nothing
    if (!th->enabled) return;
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    // ═══════════════════════════════════════════════════════════
    //  FAN CONTROL - Temperature-based
    // ═══════════════════════════════════════════════════════════
    int temp_outside = (temp < th->temp_min) || (temp > th->temp_max);
    
    if (temp_outside && node->actuators.fan_state == 0) {
        // Temperature out of range → TURN FAN ON
        printf("[%s] [AUTO] Node %d: Temp %.1f°C OUT [%.1f,%.1f] → FAN ON\n",
               timestamp, node_id, temp, th->temp_min, th->temp_max);
        
        lora_send_command(node_id, "fan", "on");
        node->actuators.fan_state = 1;
        node->tx_count++;
        gateway.auto_commands++;
        
        // Log to database
        db_log_actuator_change(node_id, "fan", 1, "AUTO", temp);
        
        usleep(500000);  // 500ms delay
    } 
    else if (!temp_outside && node->actuators.fan_state == 1) {
        // Temperature back to normal → TURN FAN OFF
        printf("[%s] [AUTO] Node %d: Temp %.1f°C IN [%.1f,%.1f] → FAN OFF\n",
               timestamp, node_id, temp, th->temp_min, th->temp_max);
        
        lora_send_command(node_id, "fan", "off");
        node->actuators.fan_state = 0;
        node->tx_count++;
        gateway.auto_commands++;
        
        // Log to database
        db_log_actuator_change(node_id, "fan", 0, "AUTO", temp);
        
        usleep(500000);
    }
    
    // ═══════════════════════════════════════════════════════════
    //  LIGHT CONTROL - Light intensity-based
    // ═══════════════════════════════════════════════════════════
    int light_outside = (light < th->light_min) || (light > th->light_max);
    
    if (light_outside && node->actuators.light_state == 0) {
        // Light out of range → TURN LIGHT ON
        printf("[%s] [AUTO] Node %d: Light %u OUT [%u,%u] → LIGHT ON\n",
               timestamp, node_id, light, th->light_min, th->light_max);
        
        lora_send_command(node_id, "light", "on");
        node->actuators.light_state = 1;
        node->tx_count++;
        gateway.auto_commands++;
        
        // Log to database
        db_log_actuator_change(node_id, "light", 1, "AUTO", (float)light);
        
        usleep(500000);
    } 
    else if (!light_outside && node->actuators.light_state == 1) {
        // Light back to normal → TURN LIGHT OFF
        printf("[%s] [AUTO] Node %d: Light %u IN [%u,%u] → LIGHT OFF\n",
               timestamp, node_id, light, th->light_min, th->light_max);
        
        lora_send_command(node_id, "light", "off");
        node->actuators.light_state = 0;
        node->tx_count++;
        gateway.auto_commands++;
        
        // Log to database
        db_log_actuator_change(node_id, "light", 0, "AUTO", (float)light);
        
        usleep(500000);
    }
    
    // ═══════════════════════════════════════════════════════════
    //  PUMP CONTROL - Soil moisture-based
    // ═══════════════════════════════════════════════════════════
    int soil_outside = (soil < th->soil_min) || (soil > th->soil_max);
    
    if (soil_outside && node->actuators.pump_state == 0) {
        // Soil moisture out of range → TURN PUMP ON
        printf("[%s] [AUTO] Node %d: Soil %u OUT [%u,%u] → PUMP ON\n",
               timestamp, node_id, soil, th->soil_min, th->soil_max);
        
        lora_send_command(node_id, "pump", "on");
        node->actuators.pump_state = 1;
        node->tx_count++;
        gateway.auto_commands++;
        
        // Log to database
        db_log_actuator_change(node_id, "pump", 1, "AUTO", (float)soil);
        
        usleep(500000);
    } 
    else if (!soil_outside && node->actuators.pump_state == 1) {
        // Soil moisture back to normal → TURN PUMP OFF
        printf("[%s] [AUTO] Node %d: Soil %u IN [%u,%u] → PUMP OFF\n",
               timestamp, node_id, soil, th->soil_min, th->soil_max);
        
        lora_send_command(node_id, "pump", "off");
        node->actuators.pump_state = 0;
        node->tx_count++;
        gateway.auto_commands++;
        
        // Log to database
        db_log_actuator_change(node_id, "pump", 0, "AUTO", (float)soil);
        
        usleep(500000);
    }
}