#ifndef __MQTT_H__
#define __MQTT_H__

#include <mosquitto.h>
#include "types.h"

/* MQTT Configuration */
#define MQTT_BROKER         "localhost"
#define MQTT_PORT           1883
#define MQTT_KEEPALIVE      60
#define MQTT_TOPIC_PREFIX   "lora/gateway"
#define MQTT_QOS            1
#define MQTT_TOPIC_CONTROL  "lora/gateway/control/"
#define MQTT_RECONNECT_INTERVAL  5
#define MQTT_MAX_RECONNECT_ATTEMPTS 10

/* MQTT Functions */
int mqtt_init(void);
void mqtt_cleanup(void);

/* Callbacks */
void mqtt_on_connect(struct mosquitto *mosq, void *obj, int rc);
void mqtt_on_disconnect(struct mosquitto *mosq, void *obj, int rc);
void mqtt_on_publish(struct mosquitto *mosq, void *obj, int mid);
void mqtt_on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);

/* Publishing functions */
void mqtt_publish_node_data(int node_id);
void mqtt_publish_gateway_stats(void);

#endif // __MQTT_H__