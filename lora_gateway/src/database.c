/*
 * src/database.c - SQLite Database Operations
 * Handles all database initialization, insert, query, and maintenance
 * 
 * NOTE: This is PART 1 of 2 - Contains initialization and insert functions
 */

#include "database.h"
#include "gateway.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <cjson/cJSON.h>

/*====================================================================
 * DATABASE INITIALIZATION
 *====================================================================*/

int db_init(void) {
    int rc;
    char *err_msg = NULL;
    
    printf("\n╔═══════════════════════════════════╗\n");
    printf("║   Database Initialization         ║\n");
    printf("╚═══════════════════════════════════╝\n\n");
    
    // 1. Open database (auto-create if not exists)
    rc = sqlite3_open(DB_PATH, &db_state.db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Cannot open database: %s\n", 
                sqlite3_errmsg(db_state.db));
        return -1;
    }
    printf("✓ Database: %s\n", DB_PATH);
    
    // 2. Enable Write-Ahead Logging (better performance)
    sqlite3_exec(db_state.db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
    
    // 3. Create SENSOR_DATA table
    const char *sql_sensor = 
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER NOT NULL,"
        "node_id INTEGER NOT NULL,"
        "temperature REAL,"
        "humidity REAL,"
        "light INTEGER,"
        "soil_moisture INTEGER,"
        "rssi INTEGER,"
        "snr INTEGER"
        ");";
    
    rc = sqlite3_exec(db_state.db, sql_sensor, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Create sensor_data: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    printf("✓ Table: sensor_data\n");
    
    // 4. Create ACTUATOR_LOGS table
    const char *sql_actuator =
        "CREATE TABLE IF NOT EXISTS actuator_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER NOT NULL,"
        "node_id INTEGER NOT NULL,"
        "actuator TEXT NOT NULL,"
        "state INTEGER NOT NULL,"
        "trigger_type TEXT,"
        "trigger_value REAL"
        ");";
    
    rc = sqlite3_exec(db_state.db, sql_actuator, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Create actuator_logs: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    printf("✓ Table: actuator_logs\n");
    
    // 5. Create COMMAND_HISTORY table
    const char *sql_command =
        "CREATE TABLE IF NOT EXISTS command_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER NOT NULL,"
        "node_id INTEGER NOT NULL,"
        "command TEXT NOT NULL,"
        "value TEXT NOT NULL,"
        "source TEXT"
        ");";
    
    rc = sqlite3_exec(db_state.db, sql_command, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Create command_history: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    printf("✓ Table: command_history\n");
    
    // 6. Create GATEWAY_STATS table
    const char *sql_stats =
        "CREATE TABLE IF NOT EXISTS gateway_stats ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER NOT NULL,"
        "rx_count INTEGER,"
        "tx_count INTEGER,"
        "crc_errors INTEGER,"
        "json_errors INTEGER,"
        "auto_commands INTEGER"
        ");";
    
    rc = sqlite3_exec(db_state.db, sql_stats, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Create gateway_stats: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    printf("✓ Table: gateway_stats\n");
    
    // 7. Create INDEX for better query performance
    const char *sql_index = 
        "CREATE INDEX IF NOT EXISTS idx_time_node "
        "ON sensor_data(timestamp, node_id);";
    sqlite3_exec(db_state.db, sql_index, 0, 0, 0);
    printf("✓ Index: idx_time_node\n");
    
    // 8. Prepare statements (for faster inserts)
    const char *insert_sensor = 
        "INSERT INTO sensor_data (timestamp, node_id, temperature, humidity, "
        "light, soil_moisture, rssi, snr) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db_state.db, insert_sensor, -1, 
                           &db_state.stmt_sensor, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Prepare sensor statement failed\n");
        return -1;
    }
    
    const char *insert_actuator =
        "INSERT INTO actuator_logs (timestamp, node_id, actuator, state, "
        "trigger_type, trigger_value) VALUES (?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db_state.db, insert_actuator, -1, 
                           &db_state.stmt_actuator, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Prepare actuator statement failed\n");
        return -1;
    }
    
    const char *insert_command =
        "INSERT INTO command_history (timestamp, node_id, command, value, source) "
        "VALUES (?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db_state.db, insert_command, -1, 
                           &db_state.stmt_command, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Prepare command statement failed\n");
        return -1;
    }
    
    const char *insert_stats =
        "INSERT INTO gateway_stats (timestamp, rx_count, tx_count, crc_errors, "
        "json_errors, auto_commands) VALUES (?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db_state.db, insert_stats, -1, 
                           &db_state.stmt_stats, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "✗ Prepare stats statement failed\n");
        return -1;
    }
    
    printf("✓ Prepared statements ready\n");
    printf("✓ Database initialized!\n\n");
    
    db_state.last_backup_time = time(NULL);
    
    return 0;
}

/*====================================================================
 * DATABASE INSERT FUNCTIONS
 *====================================================================*/

int db_save_sensor_data(int node_id, float temp, float hum, 
                        uint16_t light, uint16_t soil, 
                        int32_t rssi, int32_t snr) {
    if (db_state.db == NULL || db_state.stmt_sensor == NULL) {
        return -1;
    }

    time_t now = time(NULL);
    
    sqlite3_reset(db_state.stmt_sensor);
    sqlite3_clear_bindings(db_state.stmt_sensor);
    
    sqlite3_bind_int64(db_state.stmt_sensor, 1, now);
    sqlite3_bind_int(db_state.stmt_sensor, 2, node_id);
    sqlite3_bind_double(db_state.stmt_sensor, 3, temp);
    sqlite3_bind_double(db_state.stmt_sensor, 4, hum);
    sqlite3_bind_int(db_state.stmt_sensor, 5, light);
    sqlite3_bind_int(db_state.stmt_sensor, 6, soil);
    sqlite3_bind_int(db_state.stmt_sensor, 7, rssi);
    sqlite3_bind_int(db_state.stmt_sensor, 8, snr);
    
    int rc = sqlite3_step(db_state.stmt_sensor);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "✗ DB insert sensor failed: %s\n", 
                sqlite3_errmsg(db_state.db));
        db_state.insert_errors++;
        return -1;
    }
    
    db_state.total_inserts++;
    return 0;
}

int db_log_actuator_change(int node_id, const char *actuator, int state, 
                           const char *trigger_type, float trigger_value) {
    if (db_state.db == NULL) return -1;
    
    time_t now = time(NULL);
    
    sqlite3_reset(db_state.stmt_actuator);
    sqlite3_clear_bindings(db_state.stmt_actuator);
    
    sqlite3_bind_int64(db_state.stmt_actuator, 1, now);
    sqlite3_bind_int(db_state.stmt_actuator, 2, node_id);
    sqlite3_bind_text(db_state.stmt_actuator, 3, actuator, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(db_state.stmt_actuator, 4, state);
    sqlite3_bind_text(db_state.stmt_actuator, 5, trigger_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(db_state.stmt_actuator, 6, trigger_value);
    
    int rc = sqlite3_step(db_state.stmt_actuator);
    if (rc != SQLITE_DONE) {
        db_state.insert_errors++;
        return -1;
    }
    
    return 0;
}

int db_log_command(int node_id, const char *cmd, const char *val, 
                   const char *source) {
    if (db_state.db == NULL) return -1;
    
    time_t now = time(NULL);
    
    sqlite3_reset(db_state.stmt_command);
    sqlite3_clear_bindings(db_state.stmt_command);
    
    sqlite3_bind_int64(db_state.stmt_command, 1, now);
    sqlite3_bind_int(db_state.stmt_command, 2, node_id);
    sqlite3_bind_text(db_state.stmt_command, 3, cmd, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(db_state.stmt_command, 4, val, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(db_state.stmt_command, 5, source, -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(db_state.stmt_command);
    if (rc != SQLITE_DONE) {
        db_state.insert_errors++;
        return -1;
    }
    
    return 0;
}

int db_save_gateway_stats(void) {
    if (db_state.db == NULL) return -1;
    
    time_t now = time(NULL);
    uint32_t total_rx = 0, total_tx = 0;
    
    for (int i = 0; i < MAX_NODES; i++) {
        total_rx += gateway.nodes[i].rx_count;
        total_tx += gateway.nodes[i].tx_count;
    }
    
    sqlite3_reset(db_state.stmt_stats);
    sqlite3_clear_bindings(db_state.stmt_stats);
    
    sqlite3_bind_int64(db_state.stmt_stats, 1, now);
    sqlite3_bind_int(db_state.stmt_stats, 2, total_rx);
    sqlite3_bind_int(db_state.stmt_stats, 3, total_tx);
    sqlite3_bind_int(db_state.stmt_stats, 4, gateway.rx_crc_error);
    sqlite3_bind_int(db_state.stmt_stats, 5, gateway.json_parse_error);
    sqlite3_bind_int(db_state.stmt_stats, 6, gateway.auto_commands);
    
    int rc = sqlite3_step(db_state.stmt_stats);
    if (rc != SQLITE_DONE) {
        return -1;
    }
    
    return 0;
}

/*====================================================================
 * DATABASE CLEANUP
 *====================================================================*/

void db_cleanup(void) {
    if (db_state.stmt_sensor) {
        sqlite3_finalize(db_state.stmt_sensor);
    }
    if (db_state.stmt_actuator) {
        sqlite3_finalize(db_state.stmt_actuator);
    }
    if (db_state.stmt_command) {
        sqlite3_finalize(db_state.stmt_command);
    }
    if (db_state.stmt_stats) {
        sqlite3_finalize(db_state.stmt_stats);
    }
    if (db_state.db) {
        sqlite3_close(db_state.db);
        printf("✓ Database closed\n");
    }
}

/*
 * src/database.c - PART 2/2
 * Query and Maintenance Functions
 * 
 * COPY THIS CONTENT AND APPEND TO database.c (Part 1)
 * Remove the "CONTINUED IN PART 2" comment and add this content
 */

/*====================================================================
 * DATABASE QUERY FUNCTIONS - For MQTT/Web Interface
 *====================================================================*/

char* db_query_latest_sensors(int node_id, int limit) {
    char sql[512];
    
    if (node_id > 0) {
        snprintf(sql, sizeof(sql), 
            "SELECT "
            "datetime(timestamp, 'unixepoch', 'localtime') as time, "
            "temperature, humidity, light, soil_moisture, rssi, snr "
            "FROM sensor_data WHERE node_id = %d "
            "ORDER BY timestamp DESC LIMIT %d;", 
            node_id, limit);
    } else {
        snprintf(sql, sizeof(sql), 
            "SELECT "
            "node_id, "
            "datetime(timestamp, 'unixepoch', 'localtime') as time, "
            "temperature, humidity, light, soil_moisture, rssi, snr "
            "FROM sensor_data "
            "ORDER BY timestamp DESC LIMIT %d;", 
            limit);
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    cJSON *root = cJSON_CreateArray();
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *item = cJSON_CreateObject();
        
        int col = 0;
        if (node_id == 0) {
            cJSON_AddNumberToObject(item, "node_id", sqlite3_column_int(stmt, col++));
        }
        
        cJSON_AddStringToObject(item, "time", 
            (const char*)sqlite3_column_text(stmt, col++));
        cJSON_AddNumberToObject(item, "temperature", 
            sqlite3_column_double(stmt, col++));
        cJSON_AddNumberToObject(item, "humidity", 
            sqlite3_column_double(stmt, col++));
        cJSON_AddNumberToObject(item, "light", 
            sqlite3_column_int(stmt, col++));
        cJSON_AddNumberToObject(item, "soil_moisture", 
            sqlite3_column_int(stmt, col++));
        cJSON_AddNumberToObject(item, "rssi", 
            sqlite3_column_int(stmt, col++));
        cJSON_AddNumberToObject(item, "snr", 
            sqlite3_column_int(stmt, col++));
        
        cJSON_AddItemToArray(root, item);
    }
    
    sqlite3_finalize(stmt);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

char* db_query_range_sensors(int node_id, int hours) {
    time_t start_time = time(NULL) - (hours * 3600);
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
        "SELECT "
        "datetime(timestamp, 'unixepoch', 'localtime') as time, "
        "temperature, humidity, light, soil_moisture, rssi, snr "
        "FROM sensor_data WHERE node_id = %d AND timestamp >= %ld "
        "ORDER BY timestamp ASC;", 
        node_id, start_time);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    cJSON *root = cJSON_CreateArray();
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *item = cJSON_CreateObject();
        
        cJSON_AddStringToObject(item, "time", 
            (const char*)sqlite3_column_text(stmt, 0));
        cJSON_AddNumberToObject(item, "temperature", 
            sqlite3_column_double(stmt, 1));
        cJSON_AddNumberToObject(item, "humidity", 
            sqlite3_column_double(stmt, 2));
        cJSON_AddNumberToObject(item, "light", 
            sqlite3_column_int(stmt, 3));
        cJSON_AddNumberToObject(item, "soil_moisture", 
            sqlite3_column_int(stmt, 4));
        cJSON_AddNumberToObject(item, "rssi", 
            sqlite3_column_int(stmt, 5));
        cJSON_AddNumberToObject(item, "snr", 
            sqlite3_column_int(stmt, 6));
        
        cJSON_AddItemToArray(root, item);
    }
    
    sqlite3_finalize(stmt);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

char* db_query_aggregate(int node_id, int hours) {
    time_t start_time = time(NULL) - (hours * 3600);
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
        "SELECT "
        "AVG(temperature) as avg_temp, "
        "MIN(temperature) as min_temp, "
        "MAX(temperature) as max_temp, "
        "AVG(humidity) as avg_hum, "
        "MIN(humidity) as min_hum, "
        "MAX(humidity) as max_hum, "
        "AVG(light) as avg_light, "
        "MIN(light) as min_light, "
        "MAX(light) as max_light, "
        "AVG(soil_moisture) as avg_soil, "
        "MIN(soil_moisture) as min_soil, "
        "MAX(soil_moisture) as max_soil, "
        "COUNT(*) as record_count "
        "FROM sensor_data WHERE node_id = %d AND timestamp >= %ld;", 
        node_id, start_time);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    cJSON *root = cJSON_CreateObject();
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON_AddNumberToObject(root, "avg_temp", sqlite3_column_double(stmt, 0));
        cJSON_AddNumberToObject(root, "min_temp", sqlite3_column_double(stmt, 1));
        cJSON_AddNumberToObject(root, "max_temp", sqlite3_column_double(stmt, 2));
        cJSON_AddNumberToObject(root, "avg_hum", sqlite3_column_double(stmt, 3));
        cJSON_AddNumberToObject(root, "min_hum", sqlite3_column_double(stmt, 4));
        cJSON_AddNumberToObject(root, "max_hum", sqlite3_column_double(stmt, 5));
        cJSON_AddNumberToObject(root, "avg_light", sqlite3_column_double(stmt, 6));
        cJSON_AddNumberToObject(root, "min_light", sqlite3_column_int(stmt, 7));
        cJSON_AddNumberToObject(root, "max_light", sqlite3_column_int(stmt, 8));
        cJSON_AddNumberToObject(root, "avg_soil", sqlite3_column_double(stmt, 9));
        cJSON_AddNumberToObject(root, "min_soil", sqlite3_column_int(stmt, 10));
        cJSON_AddNumberToObject(root, "max_soil", sqlite3_column_int(stmt, 11));
        cJSON_AddNumberToObject(root, "record_count", sqlite3_column_int(stmt, 12));
    }
    
    sqlite3_finalize(stmt);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

char* db_query_actuator_history(int node_id, int limit) {
    char sql[512];
    snprintf(sql, sizeof(sql), 
        "SELECT "
        "datetime(timestamp, 'unixepoch', 'localtime') as time, "
        "actuator, state, trigger_type, trigger_value "
        "FROM actuator_logs WHERE node_id = %d "
        "ORDER BY timestamp DESC LIMIT %d;", 
        node_id, limit);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    cJSON *root = cJSON_CreateArray();
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *item = cJSON_CreateObject();
        
        cJSON_AddStringToObject(item, "time", 
            (const char*)sqlite3_column_text(stmt, 0));
        cJSON_AddStringToObject(item, "actuator", 
            (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(item, "state", 
            sqlite3_column_int(stmt, 2));
        cJSON_AddStringToObject(item, "trigger_type", 
            (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddNumberToObject(item, "trigger_value", 
            sqlite3_column_double(stmt, 4));
        
        cJSON_AddItemToArray(root, item);
    }
    
    sqlite3_finalize(stmt);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

char* db_query_stats(void) {
    cJSON *root = cJSON_CreateObject();
    
    const char *sql = "SELECT COUNT(*) FROM sensor_data";
    sqlite3_stmt *stmt;
    
    sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON_AddNumberToObject(root, "total_sensors", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    
    sql = "SELECT COUNT(*) FROM actuator_logs";
    sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON_AddNumberToObject(root, "total_actuators", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    
    sql = "SELECT COUNT(*) FROM command_history";
    sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON_AddNumberToObject(root, "total_commands", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    
    sql = "SELECT node_id, datetime(MAX(timestamp), 'unixepoch', 'localtime') as last_update, "
          "COUNT(*) as count FROM sensor_data GROUP BY node_id";
    sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    
    cJSON *nodes = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *node = cJSON_CreateObject();
        cJSON_AddNumberToObject(node, "node_id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(node, "last_update", 
            (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddNumberToObject(node, "record_count", sqlite3_column_int(stmt, 2));
        cJSON_AddItemToArray(nodes, node);
    }
    cJSON_AddItemToObject(root, "nodes", nodes);
    sqlite3_finalize(stmt);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

/*====================================================================
 * DATABASE DISPLAY FUNCTIONS - For console output
 *====================================================================*/

void db_show_recent_data(int node_id, int limit) {
    char sql[512];
    snprintf(sql, sizeof(sql), 
        "SELECT datetime(timestamp, 'unixepoch', 'localtime'), "
        "temperature, humidity, light, soil_moisture, rssi "
        "FROM sensor_data WHERE node_id = %d "
        "ORDER BY timestamp DESC LIMIT %d;", 
        node_id, limit);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_state.db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Query failed: %s\n", sqlite3_errmsg(db_state.db));
        return;
    }
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Node %d - Last %d Records                                 ║\n", 
           node_id, limit);
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Time                 Temp   Hum   Light  Soil   RSSI      ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *time = (const char *)sqlite3_column_text(stmt, 0);
        double temp = sqlite3_column_double(stmt, 1);
        double hum = sqlite3_column_double(stmt, 2);
        int light = sqlite3_column_int(stmt, 3);
        int soil = sqlite3_column_int(stmt, 4);
        int rssi = sqlite3_column_int(stmt, 5);
        
        printf("║ %s  %.1f°C %.1f%%  %-5d  %-5d  %ddBm  ║\n",
               time, temp, hum, light, soil, rssi);
        count++;
    }
    
    if (count == 0) {
        printf("║                     No data found                         ║\n");
    }
    
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    sqlite3_finalize(stmt);
}

void db_show_statistics(void) {
    const char *sql_count = 
        "SELECT "
        "(SELECT COUNT(*) FROM sensor_data) as sensor_count,"
        "(SELECT COUNT(*) FROM actuator_logs) as actuator_count,"
        "(SELECT COUNT(*) FROM command_history) as command_count;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db_state.db, sql_count, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Statistics query failed\n");
        return;
    }
    
    printf("\n╔═══════════════════════════════════╗\n");
    printf("║   Database Statistics             ║\n");
    printf("╠═══════════════════════════════════╣\n");
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int sensor_count = sqlite3_column_int(stmt, 0);
        int actuator_count = sqlite3_column_int(stmt, 1);
        int command_count = sqlite3_column_int(stmt, 2);
        
        printf("║ Sensor records:    %8d      ║\n", sensor_count);
        printf("║ Actuator logs:     %8d      ║\n", actuator_count);
        printf("║ Command history:   %8d      ║\n", command_count);
    }
    
    printf("║ Total inserts:     %8u      ║\n", db_state.total_inserts);
    printf("║ Insert errors:     %8u      ║\n", db_state.insert_errors);
    printf("╚═══════════════════════════════════╝\n\n");
    
    sqlite3_finalize(stmt);
}

/*====================================================================
 * DATABASE MAINTENANCE
 *====================================================================*/

int db_cleanup_old_data(int days_to_keep) {
    time_t cutoff_time = time(NULL) - (days_to_keep * 24 * 3600);
    
    char sql[256];
    snprintf(sql, sizeof(sql), 
             "DELETE FROM sensor_data WHERE timestamp < %ld;", cutoff_time);
    
    int rc = sqlite3_exec(db_state.db, sql, 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cleanup failed: %s\n", sqlite3_errmsg(db_state.db));
        return -1;
    }
    
    int deleted = sqlite3_changes(db_state.db);
    printf("✓ Cleaned up %d old records (kept last %d days)\n", deleted, days_to_keep);
    
    sqlite3_exec(db_state.db, "VACUUM;", 0, 0, 0);
    
    return deleted;
}

int db_backup(void) {
    char backup_path[256];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(backup_path, sizeof(backup_path), 
             "/home/debian/backups/lora_gateway_%Y%m%d_%H%M%S.db", t);
    
    system("mkdir -p /home/debian/backups");
    
    sqlite3 *backup_db;
    int rc = sqlite3_open(backup_path, &backup_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot create backup\n");
        return -1;
    }
    
    sqlite3_backup *backup_obj = sqlite3_backup_init(backup_db, "main", 
                                                      db_state.db, "main");
    if (backup_obj) {
        sqlite3_backup_step(backup_obj, -1);
        sqlite3_backup_finish(backup_obj);
        printf("✓ Database backed up: %s\n", backup_path);
    }
    
    sqlite3_close(backup_db);
    db_state.last_backup_time = now;
    
    return 0;
}