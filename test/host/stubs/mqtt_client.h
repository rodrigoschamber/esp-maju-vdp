#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- tipos basicos do ESP-IDF event loop que o MQTT depende -------------- */

typedef const char *esp_event_base_t;

typedef void (*esp_event_handler_t)(void *event_handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void *event_data);

/* ---- tipos e enums do cliente MQTT --------------------------------------- */

typedef struct esp_mqtt_client *esp_mqtt_client_handle_t;

typedef enum {
    MQTT_EVENT_ANY          = -1,
    MQTT_EVENT_ERROR        = 0,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_BEFORE_CONNECT,
    MQTT_EVENT_DELETED,
} esp_mqtt_event_id_t;

typedef enum {
    MQTT_ERROR_TYPE_NONE = 0,
    MQTT_ERROR_TYPE_TCP_TRANSPORT,
    MQTT_ERROR_TYPE_CONNECTION_REFUSED,
    MQTT_ERROR_TYPE_SUBSCRIBE_FAILED,
} esp_mqtt_error_type_t;

typedef enum {
    MQTT_CONNECTION_ACCEPTED = 0,
    MQTT_CONNECTION_REFUSE_PROTOCOL,
    MQTT_CONNECTION_REFUSE_ID_REJECTED,
    MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE,
    MQTT_CONNECTION_REFUSE_BAD_USERNAME,
    MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED,
} esp_mqtt_connect_return_code_t;

typedef struct {
    esp_err_t esp_tls_last_esp_err;
    int esp_tls_stack_err;
    int esp_tls_cert_verify_flags;
    esp_mqtt_error_type_t error_type;
    esp_mqtt_connect_return_code_t connect_return_code;
    int esp_transport_sock_errno;
} esp_mqtt_error_codes_t;

typedef struct {
    esp_mqtt_event_id_t     event_id;
    esp_mqtt_client_handle_t client;
    void                   *data;
    int                     data_len;
    int                     total_data_len;
    char                   *topic;
    int                     topic_len;
    int                     msg_id;
    int                     session_present;
    esp_mqtt_error_codes_t *error_handle;
    bool                    retain;
    int                     qos;
    bool                    dup;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;

/* ---- configuracao do cliente (sub-estruturas aninhadas do ESP-IDF v5.x) --- */

typedef struct {
    struct {
        struct {
            const char *uri;
            const char *host;
            uint32_t    port;
        } address;
        struct {
            esp_err_t (*crt_bundle_attach)(void *conf);
            const char *certificate;
        } verification;
    } broker;
    struct {
        const char *client_id;
        const char *username;
        struct {
            const char *password;
            const char *certificate;
            const char *key;
        } authentication;
    } credentials;
    struct {
        int  keepalive;
        bool disable_clean_session;
        int  reconnect_timeout_ms;
        int  network_timeout_ms;
    } session;
} esp_mqtt_client_config_t;

/* ---- API do cliente ------------------------------------------------------ */

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);

esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                          esp_mqtt_event_id_t event,
                                          esp_event_handler_t event_handler,
                                          void *event_handler_arg);

esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);

int esp_mqtt_client_publish(esp_mqtt_client_handle_t client,
                             const char *topic,
                             const char *data,
                             int len,
                             int qos,
                             int retain);

esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);

esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);
