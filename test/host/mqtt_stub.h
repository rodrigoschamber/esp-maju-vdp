#pragma once
#include <stdbool.h>
#include "mqtt_client.h"

#define MQTT_STUB_MAX_PUBS 4

typedef struct {
    char topic[128];
    char payload[1024];
    int  len;
} mqtt_stub_pub_t;

/* --- estado observavel --------------------------------------------------- */
extern mqtt_stub_pub_t mqtt_stub_pubs[MQTT_STUB_MAX_PUBS];  /* [0] = 1a publicacao apos reset */
extern char mqtt_stub_last_topic[128];
extern char mqtt_stub_last_payload[1024];
extern int  mqtt_stub_publish_count;

/* --- controle ------------------------------------------------------------- */
void mqtt_stub_reset(void);
void mqtt_stub_set_init_null(bool fail);
void mqtt_stub_set_start_fail(bool fail);
void mqtt_stub_set_register_ret(esp_err_t ret);
void mqtt_stub_set_publish_ret(int ret);
void mqtt_stub_set_simulate_connect(bool sim);
void mqtt_stub_fire_connected(void);
void mqtt_stub_fire_disconnected(void);
void mqtt_stub_fire_event(esp_mqtt_event_id_t id, esp_mqtt_event_t *event);
