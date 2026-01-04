#ifndef __JSON_PARSER_H__
#define __JSON_PARSER_H__

#include <stdint.h>
#include "types.h"

int parse_json_sensor_data(const char *data, int *node_id, float *temp, 
                          float *hum, uint16_t *soil, uint16_t *lux,
                          actuator_state_t *actuators);

int parse_text_sensor_data(const char *data, int *node_id, float *temp, 
                          float *hum, uint16_t *soil, uint16_t *lux);

void output_json_to_file(void);

#endif // __JSON_PARSER_H__