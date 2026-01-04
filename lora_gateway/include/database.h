#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <sqlite3.h>
#include "types.h"

/* Database file location */
#define DB_PATH "/home/debian/lora_gateway.db"
#define DB_BACKUP_DIR "/home/debian/backups"

/* Database Functions - Initialization */
int db_init(void);
void db_cleanup(void);

/* Database Functions - Save Data */
int db_save_sensor_data(int node_id, float temp, float hum, 
                        uint16_t light, uint16_t soil, 
                        int32_t rssi, int32_t snr);
int db_log_actuator_change(int node_id, const char *actuator, int state, 
                           const char *trigger_type, float trigger_value);
int db_log_command(int node_id, const char *cmd, const char *val, 
                   const char *source);
int db_save_gateway_stats(void);

/* Database Functions - Query (returns JSON strings) */
char* db_query_latest_sensors(int node_id, int limit);
char* db_query_range_sensors(int node_id, int hours);
char* db_query_aggregate(int node_id, int hours);
char* db_query_actuator_history(int node_id, int limit);
char* db_query_stats(void);

/* Database Functions - Display (console output) */
void db_show_recent_data(int node_id, int limit);
void db_show_statistics(void);

/* Database Maintenance */
int db_cleanup_old_data(int days_to_keep);
int db_backup(void);

#endif // __DATABASE_H__