#ifndef __AUTO_CONTROL_H__
#define __AUTO_CONTROL_H__

#include <stdint.h>

void check_auto_control(int node_id, float temp, float hum, 
                       uint16_t light, uint16_t soil);

#endif // __AUTO_CONTROL_H__