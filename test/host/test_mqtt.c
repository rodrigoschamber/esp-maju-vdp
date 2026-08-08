#include "unity.h"
#include "mqtt_client.h"
#include "telemetry_mqtt.h"
#include "telemetry_thingspeak.h"
#include "vpd.h"
#include "esp_err.h"
#include "wifi_env.h"
#include <string.h>
#include <stdbool.h>

/* declaradas em mqtt_stub.c */
extern char mqtt_stub_last_topic[128];
extern char mqtt_stub_last_payload[256];
extern int  mqtt_stub_publish_count;
void mqtt_stub_reset(void);
void mqtt_stub_set_init_null(bool fail);
void mqtt_stub_set_start_fail(bool fail);
void mqtt_stub_set_register_ret(esp_err_t ret);
void mqtt_stub_set_publish_ret(int ret);
void mqtt_stub_set_simulate_connect(bool sim);
void mqtt_stub_fire_disconnected(void);
void mqtt_stub_fire_event(esp_mqtt_event_id_t id, esp_mqtt_event_t *event);

/* declaradas em thingspeak_http_stub.c */
extern char ts_stub_last_payload[256];
void ts_stub_set_response(int status, esp_err_t perform_err, const char *body);

/* ---- helpers -------------------------------------------------------------- */

/* Inicializa o backend MQTT com stub simulando conexao bem-sucedida. */
static void init_connected(void)
{
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(true);
    mqtt_backend.init();
}

static vpd_result_t make_vpd(void)
{
    vpd_result_t v = {0};
    v.vpd_ar    = 1.234f;
    v.vpd_folha = 1.567f;
    return v;
}

/* ---- testes: estrutura do backend ---------------------------------------- */

void test_mqtt_backend_struct_populated(void)
{
    TEST_ASSERT_NOT_NULL(mqtt_backend.init);
    TEST_ASSERT_NOT_NULL(mqtt_backend.send);
    TEST_ASSERT_NOT_NULL(mqtt_backend.deinit);
}

/* ---- testes: init --------------------------------------------------------- */

void test_mqtt_init_returns_ok_when_connected(void)
{
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(true);
    TEST_ASSERT_EQUAL_INT(ESP_OK, mqtt_backend.init());
    mqtt_backend.deinit();
}

void test_mqtt_init_fails_when_client_null(void)
{
    mqtt_stub_reset();
    mqtt_stub_set_init_null(true);
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, mqtt_backend.init());
}

void test_mqtt_init_fails_when_start_fails(void)
{
    mqtt_stub_reset();
    mqtt_stub_set_start_fail(true);
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, mqtt_backend.init());
}

void test_mqtt_init_fails_when_register_fails(void)
{
    mqtt_stub_reset();
    mqtt_stub_set_register_ret(ESP_FAIL);
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, mqtt_backend.init());
}

/* ---- testes: send --------------------------------------------------------- */

void test_mqtt_send_publishes_to_correct_topic(void)
{
    init_connected();
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_TOPIC_ENV, mqtt_stub_last_topic);
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_contains_temperature(void)
{
    init_connected();
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "\"t\":25.00"));
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_contains_humidity(void)
{
    init_connected();
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "\"rh\":60.00"));
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_contains_vpd_fields(void)
{
    init_connected();
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "\"vpd_ar\":1.234"));
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "\"vpd_folha\":1.567"));
    mqtt_backend.deinit();
}

void test_mqtt_send_increments_publish_count(void)
{
    init_connected();
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_EQUAL_INT(1, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_send_skips_when_not_connected(void)
{
    /* Inicializa sem simular evento CONNECTED → s_connected permanece false. */
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(false);
    mqtt_backend.init();

    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_send_skips_after_disconnect(void)
{
    init_connected();
    mqtt_stub_fire_disconnected(); /* simula perda de conexao */

    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_send_handles_publish_failure_no_crash(void)
{
    init_connected();
    mqtt_stub_set_publish_ret(-1); /* simula falha de publicacao */
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    /* nao deve travar; publish_count ainda incrementa (a funcao foi chamada) */
    TEST_ASSERT_EQUAL_INT(1, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_error_tcp_transport_no_crash(void)
{
    /* Simula o evento MQTT_EVENT_ERROR com tipo TCP_TRANSPORT; nao deve travar. */
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(false);
    mqtt_backend.init();

    static esp_mqtt_error_codes_t err_codes = {
        .error_type              = MQTT_ERROR_TYPE_TCP_TRANSPORT,
        .esp_transport_sock_errno = 202,
        .esp_tls_last_esp_err    = 0,
    };
    static esp_mqtt_event_t err_evt = {
        .event_id    = MQTT_EVENT_ERROR,
        .error_handle = &err_codes,
    };
    mqtt_stub_fire_event(MQTT_EVENT_ERROR, &err_evt);
    mqtt_backend.deinit();
}

void test_mqtt_error_connection_refused_no_crash(void)
{
    /* Simula o evento MQTT_EVENT_ERROR com tipo CONNECTION_REFUSED. */
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(false);
    mqtt_backend.init();

    static esp_mqtt_error_codes_t err_codes = {
        .error_type          = MQTT_ERROR_TYPE_CONNECTION_REFUSED,
        .connect_return_code = MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED,
    };
    static esp_mqtt_event_t err_evt = {
        .event_id    = MQTT_EVENT_ERROR,
        .error_handle = &err_codes,
    };
    mqtt_stub_fire_event(MQTT_EVENT_ERROR, &err_evt);
    mqtt_backend.deinit();
}

/* ---- testes: deinit ------------------------------------------------------- */

void test_mqtt_deinit_idempotent(void)
{
    /* Chamar deinit duas vezes nao deve travar. */
    init_connected();
    mqtt_backend.deinit();
    mqtt_backend.deinit();
}

void test_mqtt_deinit_clears_connected_state(void)
{
    init_connected();
    mqtt_backend.deinit();

    /* Apos deinit, send deve ser ignorado. */
    mqtt_stub_reset();
    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
}

/* ---- testes: isolamento entre backends ------------------------------------ */

void test_mqtt_failure_does_not_block_thingspeak(void)
{
    /* MQTT falha na publicacao, mas ThingSpeak deve funcionar normalmente. */
    init_connected();
    mqtt_stub_set_publish_ret(-1);
    ts_stub_set_response(200, ESP_OK, "99");
    ts_stub_last_payload[0] = '\0';

    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);       /* MQTT: falha na publicacao  */
    thingspeak_backend.send(25.0f, 60.0f, &v); /* ThingSpeak: deve funcionar */

    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field1=25.00"));
    mqtt_backend.deinit();
}

void test_thingspeak_failure_does_not_affect_mqtt(void)
{
    /* ThingSpeak retorna erro HTTP, mas MQTT deve publicar normalmente. */
    init_connected();
    ts_stub_set_response(500, ESP_OK, "error");

    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);       /* MQTT: publica              */
    thingspeak_backend.send(25.0f, 60.0f, &v); /* ThingSpeak: retorna 500    */

    TEST_ASSERT_EQUAL_INT(1, mqtt_stub_publish_count);
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_TOPIC_ENV, mqtt_stub_last_topic);
    mqtt_backend.deinit();
}

void test_mqtt_unavailable_thingspeak_still_sends(void)
{
    /* Cliente MQTT nunca conecta (broker indisponivel); ThingSpeak funciona. */
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(false); /* sem evento CONNECTED */
    mqtt_backend.init();
    ts_stub_set_response(200, ESP_OK, "7");

    vpd_result_t v = make_vpd();
    mqtt_backend.send(25.0f, 60.0f, &v);       /* descartado: nao conectado  */
    thingspeak_backend.send(25.0f, 60.0f, &v); /* deve enviar normalmente    */

    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "api_key="));
    mqtt_backend.deinit();
}
