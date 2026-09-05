#include "unity.h"
#include "mqtt_stub.h"
#include "telemetry_mqtt.h"
#include "telemetry_thingspeak.h"
#include "test_fixtures.h"
#include "esp_err.h"
#include "wifi_env.h"
#include <string.h>
#include <stdbool.h>

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

static maju_reading_t make_r(void)
{
    return make_reading(25.0f, 60.0f, 1.234f, 1.567f);
}

/* Conta quantos valores ha entre "px":[ e ] (numero de virgulas + 1). */
static int count_px_values(const char *json)
{
    const char *p = strstr(json, "\"px\":[");
    if (!p) return -1;
    p += 6;
    const char *end = strchr(p, ']');
    if (!end) return -1;
    int n = 1;
    for (; p < end; p++) {
        if (*p == ',') n++;
    }
    return n;
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

/* ---- testes: send (escalares) -------------------------------------------- */

void test_mqtt_send_publishes_to_correct_topic(void)
{
    init_connected();
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_TOPIC_ENV, mqtt_stub_last_topic);
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_contains_temperature(void)
{
    init_connected();
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "field1=25.00"));
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_contains_humidity(void)
{
    init_connected();
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "field2=60.00"));
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_contains_vpd_fields(void)
{
    init_connected();
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "field3=1.234"));
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "field4=1.567"));
    mqtt_backend.deinit();
}

void test_mqtt_send_increments_publish_count(void)
{
    init_connected();
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_EQUAL_INT(1, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_send_skips_when_not_connected(void)
{
    /* Inicializa sem simular evento CONNECTED → s_connected permanece false. */
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(false);
    mqtt_backend.init();

    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_send_skips_after_disconnect(void)
{
    init_connected();
    mqtt_stub_fire_disconnected(); /* simula perda de conexao */

    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

void test_mqtt_send_handles_publish_failure_no_crash(void)
{
    init_connected();
    mqtt_stub_set_publish_ret(-1); /* simula falha de publicacao */
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    /* nao deve travar; publish_count ainda incrementa (a funcao foi chamada) */
    TEST_ASSERT_EQUAL_INT(1, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

/* ---- testes: send (campos IR e fonte) ------------------------------------ */

void test_mqtt_send_payload_contains_ir_fields(void)
{
    init_connected();
    maju_reading_t r = make_reading_ir();
    mqtt_backend.send(&r);
    /* Com AMG valido ha duas publicacoes; a primeira e a dos escalares. */
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_TOPIC_ENV, mqtt_stub_pubs[0].topic);
    TEST_ASSERT_EQUAL_STRING(
        "field1=25.00&field2=60.00&field3=1.234&field4=1.567"
        "&field5=22.83&field6=21.10&field7=23.00&field8=24.90&src=mlx",
        mqtt_stub_pubs[0].payload);
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_src_none_without_ir(void)
{
    init_connected();
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_last_payload, "&src=none"));
    mqtt_backend.deinit();
}

void test_mqtt_send_payload_src_amg_on_fallback(void)
{
    init_connected();
    maju_reading_t r = make_reading_ir();
    r.th.mlx_ok = false;
    thermal_select_leaf(&r.th);
    r.v.t_folha_c = r.th.t_folha_c;
    mqtt_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_pubs[0].payload, "field5=23.00"));
    TEST_ASSERT_NOT_NULL(strstr(mqtt_stub_pubs[0].payload, "&src=amg"));
    mqtt_backend.deinit();
}

/* ---- testes: frame termico ------------------------------------------------ */

void test_mqtt_send_no_thermal_frame_when_amg_fail(void)
{
    init_connected();
    maju_reading_t r = make_reading_ir();
    r.th.amg_ok = false;
    mqtt_backend.send(&r);
    TEST_ASSERT_EQUAL_INT(1, mqtt_stub_publish_count);
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_TOPIC_ENV, mqtt_stub_last_topic);
    mqtt_backend.deinit();
}

void test_mqtt_send_publishes_thermal_frame_on_separate_topic(void)
{
    init_connected();
    maju_reading_t r = make_reading_ir();
    mqtt_backend.send(&r);

    TEST_ASSERT_EQUAL_INT(2, mqtt_stub_publish_count);
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_THERMAL_TOPIC_ENV, mqtt_stub_last_topic);
    TEST_ASSERT_EQUAL_STRING(MAJU_MQTT_THERMAL_TOPIC_ENV, mqtt_stub_pubs[1].topic);

    const char *json = mqtt_stub_last_payload;
    TEST_ASSERT_EQUAL_CHAR('{', json[0]);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"ts_ms\":123456"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"src\":\"mlx\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"t_leaf\":22.83"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"mlx_tobj\":22.83"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"mlx_ta\":24.90"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"therm\":25.10"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"min\":21.10"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"avg\":23.00"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"max\":24.90"));
    TEST_ASSERT_EQUAL_INT(64, count_px_values(json));
    TEST_ASSERT_EQUAL_CHAR('}', json[strlen(json) - 1]);
    mqtt_backend.deinit();
}

void test_mqtt_thermal_frame_skipped_when_not_connected(void)
{
    mqtt_stub_reset();
    mqtt_stub_set_simulate_connect(false);
    mqtt_backend.init();

    maju_reading_t r = make_reading_ir();
    mqtt_backend.send(&r);
    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    mqtt_backend.deinit();
}

/* ---- testes: eventos de erro ---------------------------------------------- */

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
    maju_reading_t r = make_r();
    mqtt_backend.send(&r);
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

    maju_reading_t r = make_r();
    mqtt_backend.send(&r);       /* MQTT: falha na publicacao  */
    thingspeak_backend.send(&r); /* ThingSpeak: deve funcionar */

    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field1=25.00"));
    mqtt_backend.deinit();
}

void test_thingspeak_failure_does_not_affect_mqtt(void)
{
    /* ThingSpeak retorna erro HTTP, mas MQTT deve publicar normalmente. */
    init_connected();
    ts_stub_set_response(500, ESP_OK, "error");

    maju_reading_t r = make_r();
    mqtt_backend.send(&r);       /* MQTT: publica              */
    thingspeak_backend.send(&r); /* ThingSpeak: retorna 500    */

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

    maju_reading_t r = make_r();
    mqtt_backend.send(&r);       /* descartado: nao conectado  */
    thingspeak_backend.send(&r); /* deve enviar normalmente    */

    TEST_ASSERT_EQUAL_INT(0, mqtt_stub_publish_count);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "api_key="));
    mqtt_backend.deinit();
}
