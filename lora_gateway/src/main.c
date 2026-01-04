/*
 * BeagleBone Black LoRa Gateway - JSON FORMAT
 * 
 * Receives JSON data from nodes for easy web dashboard integration
 * Uses cJSON library for parsing
 * 
 * Install: sudo apt-get install libcjson-dev libmosquitto-dev libsqlite3-dev
 * Compile: make
 * Run: sudo ./gateway
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <cjson/cJSON.h>
#include <mosquitto.h>
#include <sqlite3.h>

#include "gateway.h"
#include "mqtt.h"
#include "database.h"
#include "lora.h"
#include "utils.h"

/* Global state */
gateway_state_t gateway = {0};
database_state_t db_state = {0};

/* Forward declarations */
void shutdown_timeout_handler(int sig);
void signal_handler(int sig);

void signal_handler(int sig) {
    static int force_quit = 0;
    
    if (force_quit) {
        printf("\n[FORCE] Force quitting...\n");
        exit(1);
    }
    printf("\n\n[SIGNAL] Shutting down...\n");
    gateway.running = 0;
    force_quit = 1;
}

void shutdown_timeout_handler(int sig) {
    printf("\n[TIMEOUT] Shutdown timeout - force exit!\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║  BeagleBone Black LoRa Gateway - JSON MODE       ║\n");
    printf("║  ✓ Receives JSON data from nodes                 ║\n");
    printf("║  ✓ Sends JSON commands to nodes                  ║\n");
    printf("║  ✓ Outputs /tmp/gateway_data.json for web        ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Set default thresholds
    for (int i = 0; i < 3; i++) {  // MAX_NODES = 3
        gateway.nodes[i].thresholds.enabled = 0;
        gateway.nodes[i].thresholds.temp_min = 20.0;
        gateway.nodes[i].thresholds.temp_max = 28.0;
        gateway.nodes[i].thresholds.light_min = 200;
        gateway.nodes[i].thresholds.light_max = 800;
        gateway.nodes[i].thresholds.soil_min = 1500;
        gateway.nodes[i].thresholds.soil_max = 3000;
    }
    
    gateway.running = 1;
    gateway.rx_crc_recovery = 0;
    
    if (lora_init() < 0) {
        return 1;
    }
    if (mqtt_init() < 0) {
        printf("  Warning: MQTT init failed, continuing without MQTT\n");
        gateway.mqtt = NULL;
    }
    if (db_init() < 0) {
        printf("  Warning: Database init failed, continuing without DB\n");
        db_state.db = NULL;
    }
    
    printf("✓ Gateway ready!\n");
    printf("✓ JSON mode active\n");
    printf("✓ Waiting for nodes...\n\n");
    
    printf("Example commands:\n");
    printf("  fan 1 on      → {\"node\":1,\"cmd\":\"fan\",\"val\":\"on\"}\n");
    printf("  light 2 off   → {\"node\":2,\"cmd\":\"light\",\"val\":\"off\"}\n");
    printf("  pump 3 on     → {\"node\":3,\"cmd\":\"pump\",\"val\":\"on\"}\n\n");
    
    interactive_mode();
    
    printf("\n╔═════════════════════════════════════╗\n");
    printf("║          Shutting Down              ║\n");
    printf("╚═════════════════════════════════════╝\n\n");
    
    signal(SIGALRM, shutdown_timeout_handler);
    alarm(5);  // 5 second timeout
    
    db_cleanup();
    
    if (gateway.lora_fd >= 0) {
        uint32_t state = LORA_STATE_SLEEP;
        ioctl(gateway.lora_fd, LORA_SET_STATE, &state);
        
        // MQTT Cleanup - WITH TIMEOUT
        if (gateway.mqtt) {
            printf("✓ Cleaning up MQTT...\n");
            
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/status", MQTT_TOPIC_PREFIX);
            mosquitto_publish(gateway.mqtt, NULL, topic, 7, "offline", MQTT_QOS, false);
            
            printf("  Stopping MQTT loop...\n");
            int rc = mosquitto_loop_stop(gateway.mqtt, true);
            if (rc != MOSQ_ERR_SUCCESS) {
                printf("  Warning: mosquitto_loop_stop failed: %s\n", mosquitto_strerror(rc));
            }
            
            usleep(200000);
            
            printf("  Disconnecting...\n");
            mosquitto_disconnect(gateway.mqtt);
            mosquitto_destroy(gateway.mqtt);
            mosquitto_lib_cleanup();
            
            printf("✓ MQTT cleaned up\n");
        }
        close(gateway.lora_fd);
    }
    
    alarm(0);
    
    printf("✓ Gateway stopped\n\n");
    
    // Final statistics
    printf("━━━ FINAL STATISTICS ━━━\n");
    for (int i = 0; i < 3; i++) {
        printf("Node %d: RX=%u, TX=%u\n", 
               i+1, 
               gateway.nodes[i].rx_count, 
               gateway.nodes[i].tx_count);
    }
    printf("RX CRC errors: %u\n", gateway.rx_crc_error);
    printf("JSON parse errors: %u\n", gateway.json_parse_error);
    printf("Auto commands sent: %u\n\n", gateway.auto_commands);
    
    return 0;
}
