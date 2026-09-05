#include "unity.h"
#include "telemetry_thingspeak.h"
#include "test_fixtures.h"
#include "esp_err.h"
#include <string.h>
#include <stdbool.h>

/* declared in thingspeak_http_stub.c */
extern char ts_stub_last_payload[256];
extern char ts_stub_last_url[128];
void ts_stub_set_response(int status, esp_err_t perform_err, const char *body);
void ts_stub_set_init_null(bool fail);

static maju_reading_t make_r(void)
{
    return make_reading(25.0f, 60.0f, 1.234f, 1.567f);
}

void test_thingspeak_backend_struct_populated(void)
{
    TEST_ASSERT_NOT_NULL(thingspeak_backend.init);
    TEST_ASSERT_NOT_NULL(thingspeak_backend.send);
    TEST_ASSERT_NOT_NULL(thingspeak_backend.deinit);
}

void test_thingspeak_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, thingspeak_backend.init());
}

void test_thingspeak_send_payload_fields(void)
{
    ts_stub_set_response(200, ESP_OK, "42");
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "api_key="));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field1=25.00"));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field2=60.00"));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field3=1.234"));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field4=1.567"));
}

void test_thingspeak_send_payload_ir_fields(void)
{
    ts_stub_set_response(200, ESP_OK, "42");
    maju_reading_t r = make_reading_ir();
    thingspeak_backend.send(&r);
    TEST_ASSERT_EQUAL_STRING(
        "api_key=TEST_KEY_1234567&field1=25.00&field2=60.00&field3=1.234&field4=1.567"
        "&field5=22.83&field6=21.10&field7=23.00&field8=24.90",
        ts_stub_last_payload);
    /* ThingSpeak nao recebe o indicador de fonte (parametro desconhecido). */
    TEST_ASSERT_NULL(strstr(ts_stub_last_payload, "src="));
}

void test_thingspeak_send_omits_amg_fields_when_invalid(void)
{
    ts_stub_set_response(200, ESP_OK, "42");
    maju_reading_t r = make_reading_ir();
    r.th.amg_ok = false;
    thingspeak_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field5=22.83"));
    TEST_ASSERT_NULL(strstr(ts_stub_last_payload, "field6="));
    TEST_ASSERT_NULL(strstr(ts_stub_last_payload, "field7="));
    TEST_ASSERT_NULL(strstr(ts_stub_last_payload, "field8="));
    TEST_ASSERT_NULL(strstr(ts_stub_last_payload, "nan"));
}

void test_thingspeak_send_success_entry_id(void)
{
    /* HTTP 200 with valid entry_id → success log path, must not crash. */
    ts_stub_set_response(200, ESP_OK, "42");
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
}

void test_thingspeak_send_empty_response_no_crash(void)
{
    /* HTTP 200 but no body → entry_id = 0 → warning path. */
    ts_stub_set_response(200, ESP_OK, "");
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
}

void test_thingspeak_send_http_error_no_crash(void)
{
    ts_stub_set_response(400, ESP_OK, "error");
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
}

void test_thingspeak_send_perform_fail_no_crash(void)
{
    ts_stub_set_response(0, ESP_FAIL, "");
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
}

void test_thingspeak_send_init_null_no_crash(void)
{
    ts_stub_set_init_null(true);
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
    ts_stub_set_init_null(false);
}

void test_thingspeak_url_set_in_request(void)
{
    ts_stub_set_response(200, ESP_OK, "1");
    maju_reading_t r = make_r();
    thingspeak_backend.send(&r);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_url, "thingspeak.com"));
}
