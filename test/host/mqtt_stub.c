#include "mqtt_stub.h"
#include <string.h>
#include <stdbool.h>

/* ---- estado controlavel pelos testes -------------------------------------- */

static bool               s_init_null       = false;
static bool               s_start_fail      = false;
static esp_err_t          s_register_ret    = ESP_OK; /* ESP_OK por padrao    */
static int                s_publish_ret     = 0;       /* >= 0 = sucesso       */
static bool               s_simulate_connect = true;  /* start() dispara CONNECTED */

/* ---- estado observavel pelos testes --------------------------------------- */

mqtt_stub_pub_t mqtt_stub_pubs[MQTT_STUB_MAX_PUBS];
char mqtt_stub_last_topic[128];
char mqtt_stub_last_payload[1024];
int  mqtt_stub_publish_count;

/* ---- estado interno do stub ----------------------------------------------- */

static esp_event_handler_t s_handler     = NULL;
static void               *s_handler_arg = NULL;

struct esp_mqtt_client {
    int placeholder;
};

static struct esp_mqtt_client s_client_instance;

/* ---- API de controle (usada pelos testes) --------------------------------- */

void mqtt_stub_reset(void)
{
    s_init_null            = false;
    s_start_fail           = false;
    s_register_ret         = ESP_OK;
    s_publish_ret          = 0;
    s_simulate_connect     = true;
    s_handler              = NULL;
    s_handler_arg          = NULL;
    memset(mqtt_stub_pubs, 0, sizeof(mqtt_stub_pubs));
    mqtt_stub_last_topic[0]   = '\0';
    mqtt_stub_last_payload[0] = '\0';
    mqtt_stub_publish_count   = 0;
}

void mqtt_stub_set_init_null(bool fail)        { s_init_null        = fail;  }
void mqtt_stub_set_start_fail(bool fail)       { s_start_fail       = fail;  }
void mqtt_stub_set_register_ret(esp_err_t ret) { s_register_ret     = ret;   }
void mqtt_stub_set_publish_ret(int ret)        { s_publish_ret      = ret;   }
void mqtt_stub_set_simulate_connect(bool sim)  { s_simulate_connect = sim;   }

/* Dispara o evento CONNECTED manualmente (util para testes avancos). */
void mqtt_stub_fire_connected(void)
{
    if (!s_handler) return;
    static esp_mqtt_event_t evt;
    evt.event_id    = MQTT_EVENT_CONNECTED;
    evt.error_handle = NULL;
    s_handler(s_handler_arg, NULL, (int32_t)MQTT_EVENT_CONNECTED, &evt);
}

/* Dispara o evento DISCONNECTED manualmente. */
void mqtt_stub_fire_disconnected(void)
{
    if (!s_handler) return;
    static esp_mqtt_event_t evt;
    evt.event_id    = MQTT_EVENT_DISCONNECTED;
    evt.error_handle = NULL;
    s_handler(s_handler_arg, NULL, (int32_t)MQTT_EVENT_DISCONNECTED, &evt);
}

/* Dispara um evento arbitrario com dados ja preenchidos pelo chamador. */
void mqtt_stub_fire_event(esp_mqtt_event_id_t id, esp_mqtt_event_t *event)
{
    if (!s_handler) return;
    s_handler(s_handler_arg, NULL, (int32_t)id, event);
}

/* ---- implementacoes stub -------------------------------------------------- */

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config)
{
    (void)config;
    if (s_init_null) return NULL;
    return &s_client_instance;
}

esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                          esp_mqtt_event_id_t event,
                                          esp_event_handler_t handler,
                                          void *arg)
{
    (void)client;
    (void)event;
    s_handler     = handler;
    s_handler_arg = arg;
    return s_register_ret;
}

esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client)
{
    (void)client;
    if (s_start_fail) return ESP_FAIL;
    if (s_simulate_connect) {
        mqtt_stub_fire_connected();
    }
    return ESP_OK;
}

int esp_mqtt_client_publish(esp_mqtt_client_handle_t client,
                             const char *topic,
                             const char *data,
                             int len,
                             int qos,
                             int retain)
{
    (void)client;
    (void)qos;
    (void)retain;

    mqtt_stub_pub_t *pub = &mqtt_stub_pubs[mqtt_stub_publish_count % MQTT_STUB_MAX_PUBS];

    strncpy(pub->topic, topic ? topic : "", sizeof(pub->topic) - 1);
    pub->topic[sizeof(pub->topic) - 1] = '\0';

    int copy = (len > 0 && len < (int)sizeof(pub->payload) - 1)
               ? len : (int)sizeof(pub->payload) - 1;
    if (data && copy > 0) {
        memcpy(pub->payload, data, copy);
    }
    pub->payload[copy] = '\0';
    pub->len = len;

    memcpy(mqtt_stub_last_topic, pub->topic, sizeof(mqtt_stub_last_topic));
    memcpy(mqtt_stub_last_payload, pub->payload, sizeof(mqtt_stub_last_payload));

    mqtt_stub_publish_count++;
    return s_publish_ret;
}

esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client)
{
    (void)client;
    return ESP_OK;
}

esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client)
{
    (void)client;
    s_handler     = NULL;
    s_handler_arg = NULL;
    return ESP_OK;
}
