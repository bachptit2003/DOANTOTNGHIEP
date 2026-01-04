#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include "types.h"

/* Gateway state (global) */
extern gateway_state_t gateway;
extern database_state_t db_state;

/* LORA Functions */
int lora_init(void);
int lora_send_command(int node_id, const char *cmd, const char *val);
int lora_send_command_json(int node_id, const char *cmd, const char *val);
int lora_send_command_text(const char *cmd_str);
void lora_clear_and_restart_rx(void);

/* Sensor data processing */
int parse_json_sensor_data(const char *data, int *node_id, float *temp, 
                          float *hum, uint16_t *soil, uint16_t *lux,
                          actuator_state_t *actuators);
int parse_text_sensor_data(const char *data, int *node_id, float *temp, 
                          float *hum, uint16_t *soil, uint16_t *lux);
void process_sensor_packet(const char *data, int len);
void check_auto_control(int node_id, float temp, float hum, uint16_t light, uint16_t soil);

/* Interactive mode */
void interactive_mode(void);
void print_help(void);
void print_status(void);

/* Output functions */
void output_json_to_file(void);
void get_timestamp(char *buf, size_t len);

#endif // __GATEWAY_H__