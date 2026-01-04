#ifndef __LORA_H__
#define __LORA_H__

#include <stdint.h>

int lora_init(void);
int lora_send_command(int node_id, const char *cmd, const char *val);
int lora_send_command_json(int node_id, const char *cmd, const char *val);
int lora_send_command_text(const char *cmd_str);
void lora_clear_and_restart_rx(void);

#endif // __LORA_H__

#endif