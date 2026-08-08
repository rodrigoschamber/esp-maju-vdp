#include <stdio.h>
#include <string.h>

/* Evita que o IntelliSense no macOS siga headers Mach-O no contexto ESP32. */
#if defined(__INTELLISENSE__)
#ifdef __APPLE__
#undef __APPLE__
#endif
#ifdef __MACH__
#undef __MACH__
#endif
#endif

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

#include "vpd.h"
#include "wifi_env.h"
#include "telemetry_mqtt.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;
static volatile bool            s_connected;

static void mqtt_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "MQTT conectado ao broker.");
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT desconectado; aguardando reconexao automatica.");
        break;

    case MQTT_EVENT_ERROR:
        if (event->error_handle) {
            switch (event->error_handle->error_type) {
            case MQTT_ERROR_TYPE_TCP_TRANSPORT:
                ESP_LOGE(TAG, "MQTT erro de transporte (errno=%d, tls_err=0x%x)",
                         event->error_handle->esp_transport_sock_errno,
                         event->error_handle->esp_tls_last_esp_err);
                break;
            case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
                ESP_LOGE(TAG, "MQTT conexao recusada pelo broker (codigo=%d)",
                         event->error_handle->connect_return_code);
                break;
            default:
                ESP_LOGE(TAG, "MQTT erro: tipo=%d", event->error_handle->error_type);
                break;
            }
        }
        break;

    default:
        break;
    }
}

static esp_err_t mqtt_init(void)
{
    if (!MAJU_MQTT_ENABLE_ENV) {
        return ESP_OK;
    }

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address = {
                .uri = MAJU_MQTT_BROKER_URI_ENV,
            },
            .verification = {
                .crt_bundle_attach = esp_crt_bundle_attach,
            },
        },
        .credentials = {
            .client_id = MAJU_MQTT_CLIENT_ID_ENV,
            .username  = MAJU_MQTT_USERNAME_ENV,
            .authentication = {
                .password = MAJU_MQTT_PASSWORD_ENV,
            },
        },
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Falha ao criar cliente MQTT.");
        return ESP_FAIL;
    }

    esp_err_t err = esp_mqtt_client_register_event(s_client,
                                                    MQTT_EVENT_ANY,
                                                    mqtt_event_handler,
                                                    NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar evento MQTT: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return err;
    }

    err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar cliente MQTT: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return err;
    }

    ESP_LOGI(TAG, "MQTT iniciado: broker=%s, topico=%s",
             MAJU_MQTT_BROKER_URI_ENV, MAJU_MQTT_TOPIC_ENV);
    return ESP_OK;
}

static void mqtt_send(float t, float rh, const vpd_result_t *v)
{
    if (!MAJU_MQTT_ENABLE_ENV) {
        return;
    }

    if (s_client == NULL || !s_connected) {
        ESP_LOGW(TAG, "MQTT nao conectado; descartando leitura.");
        return;
    }

    char payload[128];
    int len = snprintf(payload, sizeof(payload),
                       "{\"t\":%.2f,\"rh\":%.2f,\"vpd_ar\":%.3f,\"vpd_folha\":%.3f}",
                       t, rh, v->vpd_ar, v->vpd_folha);
    if (len <= 0 || len >= (int)sizeof(payload)) {
        ESP_LOGE(TAG, "Payload MQTT excedeu o limite do buffer.");
        return;
    }

    /* QoS 0: fire-and-forget; nao bloqueia enquanto o ThingSpeak processa. */
    int msg_id = esp_mqtt_client_publish(s_client, MAJU_MQTT_TOPIC_ENV,
                                         payload, len, 0, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Falha ao publicar MQTT (msg_id=%d).", msg_id);
    } else {
        ESP_LOGI(TAG, "MQTT publicado: topico=%s", MAJU_MQTT_TOPIC_ENV);
    }
}

static void mqtt_deinit(void)
{
    if (s_client == NULL) {
        return;
    }
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client    = NULL;
    s_connected = false;
}

const telemetry_backend_t mqtt_backend = {
    .init   = mqtt_init,
    .send   = mqtt_send,
    .deinit = mqtt_deinit,
};
