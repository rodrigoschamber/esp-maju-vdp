#include "esp_http_client.h"
#include <string.h>
#include <stdbool.h>

/* --- stub state (controlled by tests) -------------------------------------- */
static int       s_status      = 200;
static esp_err_t s_perform_err = ESP_OK;
static char      s_resp_body[64];
static bool      s_init_null   = false;

/* --- observable state (inspected by tests) --------------------------------- */
char ts_stub_last_payload[256];
char ts_stub_last_url[128];

/* --- control API ----------------------------------------------------------- */

void ts_stub_set_response(int status, esp_err_t perform_err, const char *body)
{
    s_status      = status;
    s_perform_err = perform_err;
    strncpy(s_resp_body, body ? body : "", sizeof(s_resp_body) - 1);
    s_resp_body[sizeof(s_resp_body) - 1] = '\0';
}

void ts_stub_set_init_null(bool fail)
{
    s_init_null = fail;
}

void ts_stub_reset(void)
{
    s_status               = 200;
    s_perform_err          = ESP_OK;
    strcpy(s_resp_body, "0");
    s_init_null            = false;
    ts_stub_last_payload[0] = '\0';
    ts_stub_last_url[0]     = '\0';
}

/* --- internal handle ------------------------------------------------------- */

struct esp_http_client {
    http_event_handle_cb event_handler;
};

static struct esp_http_client s_handle;

/* --- stub implementations -------------------------------------------------- */

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg)
{
    if (s_init_null) return NULL;
    s_handle.event_handler = cfg ? cfg->event_handler : NULL;
    if (cfg && cfg->url)
        strncpy(ts_stub_last_url, cfg->url, sizeof(ts_stub_last_url) - 1);
    return &s_handle;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,
                                     const char *k, const char *v)
{
    (void)c; (void)k; (void)v;
    return ESP_OK;
}

esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t c,
                                         const char *data, int len)
{
    int copy = len < (int)sizeof(ts_stub_last_payload) - 1
               ? len : (int)sizeof(ts_stub_last_payload) - 1;
    memcpy(ts_stub_last_payload, data, copy);
    ts_stub_last_payload[copy] = '\0';
    (void)c;
    return ESP_OK;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t c)
{
    /* Deliver body via event handler, mirroring real esp_http_client behaviour. */
    if (s_perform_err == ESP_OK && c->event_handler && s_resp_body[0] != '\0') {
        esp_http_client_event_t evt = {
            .event_id = HTTP_EVENT_ON_DATA,
            .client   = c,
            .data     = s_resp_body,
            .data_len = (int)strlen(s_resp_body),
        };
        c->event_handler(&evt);
    }
    return s_perform_err;
}

int esp_http_client_get_status_code(esp_http_client_handle_t c)
{
    (void)c;
    return s_status;
}

int esp_http_client_read_response(esp_http_client_handle_t c, char *buf, int len)
{
    /* Body already consumed by event handler — matches real perform() behaviour. */
    (void)c; (void)buf; (void)len;
    return 0;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c)
{
    (void)c;
    return ESP_OK;
}
