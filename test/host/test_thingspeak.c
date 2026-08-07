#include "unity.h"
#include "telemetry_thingspeak.h"
#include "vpd.h"
#include "esp_err.h"
#include <string.h>
#include <stdbool.h>

/* declared in thingspeak_http_stub.c */
extern char ts_stub_last_payload[256];
extern char ts_stub_last_url[128];
void ts_stub_set_response(int status, esp_err_t perform_err, const char *body);
void ts_stub_set_init_null(bool fail);

static vpd_result_t make_vpd(void)
{
    vpd_result_t v = {0};
    v.vpd_ar    = 1.234f;
    v.vpd_folha = 1.567f;
    return v;
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
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "api_key="));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field1=25.00"));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field2=60.00"));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field3=1.234"));
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_payload, "field4=1.567"));
}

void test_thingspeak_send_success_entry_id(void)
{
    /* HTTP 200 with valid entry_id → success log path, must not crash. */
    ts_stub_set_response(200, ESP_OK, "42");
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
}

void test_thingspeak_send_empty_response_no_crash(void)
{
    /* HTTP 200 but no body → entry_id = 0 → warning path. */
    ts_stub_set_response(200, ESP_OK, "");
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
}

void test_thingspeak_send_http_error_no_crash(void)
{
    ts_stub_set_response(400, ESP_OK, "error");
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
}

void test_thingspeak_send_perform_fail_no_crash(void)
{
    ts_stub_set_response(0, ESP_FAIL, "");
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
}

void test_thingspeak_send_init_null_no_crash(void)
{
    ts_stub_set_init_null(true);
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
    ts_stub_set_init_null(false);
}

void test_thingspeak_url_set_in_request(void)
{
    ts_stub_set_response(200, ESP_OK, "1");
    vpd_result_t v = make_vpd();
    thingspeak_backend.send(25.0f, 60.0f, &v);
    TEST_ASSERT_NOT_NULL(strstr(ts_stub_last_url, "thingspeak.com"));
}
