#include "lora.h"
#include "config.h"
#include "utils.h"
#include "gateway.h"
#include 
#include 
#include 
#include 
#include 
#include <sys/ioctl.h>
#include 
#include <cjson/cJSON.h>

static int lora_fd = -1;

int lora_get_fd(void) {
    return lora_fd;
}

int lora_init(void) {
    uint32_t freq, bw, sf, agc, state;
    int32_t power;
    
    printf("\n╔═══════════════════════════════════╗\n");
    printf("║   Gateway Init (JSON Mode)          ║\n");
    printf("╚═══════════════════════════════════╝\n\n");
    
    lora_fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
    if (lora_fd < 0) {
        perror("Failed to open LoRa device");
        return -1;
    }
    printf("✓ Device opened: %s\n", DEVICE_PATH);
    
    state = LORA_STATE_STANDBY;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    
    freq = FREQUENCY;
    ioctl(lora_fd, LORA_SET_FREQUENCY, &freq);
    printf("✓ Frequency: %.3f MHz\n", freq / 1000000.0);
    
    power = TX_POWER;
    ioctl(lora_fd, LORA_SET_POWER, &power);
    printf("✓ TX Power: %d dBm\n", power);
    
    bw = BANDWIDTH;
    ioctl(lora_fd, LORA_SET_BANDWIDTH, &bw);
    printf("✓ Bandwidth: %.1f kHz\n", bw / 1000.0);
    
    sf = SPREADING_FACTOR;
    ioctl(lora_fd, LORA_SET_SPRFACTOR, &sf);
    printf("✓ Spreading Factor: %u\n", sf);
    
    agc = 1;
    ioctl(lora_fd, LORA_SET_LNAAGC, &agc);
    printf("✓ LNA AGC: enabled\n");
    
    state = LORA_STATE_RX;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    printf("✓ LoRa in RX mode\n\n");
    
    gateway.lora_fd = lora_fd;
    return 0;
}

void lora_cleanup(void) {
    if (lora_fd >= 0) {
        uint32_t state = LORA_STATE_SLEEP;
        ioctl(lora_fd, LORA_SET_STATE, &state);
        close(lora_fd);
        lora_fd = -1;
    }
}

int lora_send_command_text(const char *cmd_str) {
    uint32_t state;
    int ret;
    char timestamp[32];
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    state = LORA_STATE_STANDBY;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    
    ret = write(lora_fd, cmd_str, strlen(cmd_str));
    if (ret > 0) {
        printf("[%s] TX TEXT (%d bytes): %s\n", timestamp, ret, cmd_str);
    } else {
        printf("[%s] TX failed: %s\n", timestamp, strerror(errno));
    }
    
    usleep(TX_WAIT_TIME * 1000);
    
    state = LORA_STATE_RX;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    
    return ret;
}

int lora_send_command_json(int node_id, const char *cmd, const char *val) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "node", node_id);
    cJSON_AddStringToObject(json, "cmd", cmd);
    cJSON_AddStringToObject(json, "val", val);
    
    char *json_string = cJSON_PrintUnformatted(json);
    
    uint32_t state;
    int ret;
    char timestamp[32];
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    state = LORA_STATE_STANDBY;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    
    ret = write(lora_fd, json_string, strlen(json_string));
    if (ret > 0) {
        printf("[%s] TX JSON (%d bytes): %s\n", timestamp, ret, json_string);
    } else {
        printf("[%s] TX failed: %s\n", timestamp, strerror(errno));
    }
    
    free(json_string);
    cJSON_Delete(json);
    
    usleep(TX_WAIT_TIME * 1000);
    
    state = LORA_STATE_RX;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    
    return ret;
}

int lora_send_command(int node_id, const char *cmd, const char *val) {
    return lora_send_command_json(node_id, cmd, val);
}

int lora_read_packet(char *buffer, int max_len) {
    return read(lora_fd, buffer, max_len - 1);
}

void lora_clear_and_restart_rx(void) {
    uint32_t state;
    state = LORA_STATE_STANDBY;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
    state = LORA_STATE_RX;
    ioctl(lora_fd, LORA_SET_STATE, &state);
    usleep(10000);
}

int lora_get_rssi(void) {
    int32_t rssi;
    ioctl(lora_fd, LORA_GET_RSSI, &rssi);
    return rssi;
}

int lora_get_snr(void) {
    int32_t snr;
    ioctl(lora_fd, LORA_GET_SNR, &snr);
    return snr;
}