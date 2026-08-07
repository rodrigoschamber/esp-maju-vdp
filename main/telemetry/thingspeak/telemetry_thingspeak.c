#include <stdio.h>
#include <stdlib.h>
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
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "vpd.h"
#include "wifi_env.h"
#include "telemetry_thingspeak.h"

static const char *TAG = "thingspeak";

/* Buffer acumulado via HTTP_EVENT_ON_DATA; perform() já consumiu o body. */
static char s_resp_buf[32];
static int  s_resp_len;

static esp_err_t ts_http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy = evt->data_len;
        if (s_resp_len + copy >= (int)sizeof(s_resp_buf) - 1)
            copy = (int)sizeof(s_resp_buf) - 1 - s_resp_len;
        if (copy > 0) {
            memcpy(s_resp_buf + s_resp_len, evt->data, copy);
            s_resp_len += copy;
            s_resp_buf[s_resp_len] = '\0';
        }
    }
    return ESP_OK;
}

static esp_err_t ts_init(void)
{
    return ESP_OK;
}

static void ts_send(float t, float rh, const vpd_result_t *v)
{
    if (!MAJU_THINGSPEAK_ENABLE_ENV) {
        return;
    }

    char payload[192];
    int len = snprintf(payload,
                       sizeof(payload),
                       "api_key=%s&field1=%.2f&field2=%.2f&field3=%.3f&field4=%.3f",
                       MAJU_THINGSPEAK_WRITE_API_KEY_ENV,
                       t,
                       rh,
                       v->vpd_ar,
                       v->vpd_folha);
    if (len <= 0 || len >= (int)sizeof(payload)) {
        ESP_LOGE(TAG, "Payload ThingSpeak excedeu o limite do buffer.");
        return;
    }

    s_resp_buf[0] = '\0';
    s_resp_len    = 0;

    esp_http_client_config_t cfg = {
        .url               = MAJU_THINGSPEAK_URL_ENV,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler     = ts_http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Falha ao criar cliente HTTP para ThingSpeak.");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, payload, len);

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao enviar ThingSpeak: %s", esp_err_to_name(err));
    } else if (status != 200) {
        ESP_LOGE(TAG, "ThingSpeak retornou HTTP %d (resp=%s)", status,
                 s_resp_len > 0 ? s_resp_buf : "sem corpo");
    } else {
        long entry_id = strtol(s_resp_buf, NULL, 10);
        if (entry_id > 0) {
            ESP_LOGI(TAG, "ThingSpeak atualizado com sucesso (entry_id=%ld)", entry_id);
        } else {
            ESP_LOGW(TAG, "ThingSpeak respondeu sem confirmar entry_id (resp=%s)",
                     s_resp_len > 0 ? s_resp_buf : "vazio");
        }
    }

    esp_http_client_cleanup(client);
}

static void ts_deinit(void)
{
}

const telemetry_backend_t thingspeak_backend = {
    .init   = ts_init,
    .send   = ts_send,
    .deinit = ts_deinit,
};
