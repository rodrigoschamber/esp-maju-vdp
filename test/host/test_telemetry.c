#include "unity.h"
#include "telemetry.h"
#include "vpd.h"
#include <stdbool.h>

/* --- backends mock --------------------------------------------------------- */

static bool     s_init_called;
static bool     s_send_called;
static bool     s_deinit_called;
static float    s_last_t;
static float    s_last_rh;
static esp_err_t s_init_ret;

static esp_err_t mock_init(void)
{
    s_init_called = true;
    return s_init_ret;
}

static void mock_send(float t, float rh, const vpd_result_t *v)
{
    (void)v;
    s_send_called = true;
    s_last_t  = t;
    s_last_rh = rh;
}

static void mock_deinit(void)
{
    s_deinit_called = true;
}

static const telemetry_backend_t s_mock = {
    .init   = mock_init,
    .send   = mock_send,
    .deinit = mock_deinit,
};

/* backend alternativo para testar a troca de implementacao */
static bool s_alt_send_called;

static esp_err_t alt_init(void) { return ESP_OK; }
static void alt_send(float t, float rh, const vpd_result_t *v)
{
    (void)t; (void)rh; (void)v;
    s_alt_send_called = true;
}
static void alt_deinit(void) {}

static const telemetry_backend_t s_alt = {
    .init   = alt_init,
    .send   = alt_send,
    .deinit = alt_deinit,
};

/* --- testes ---------------------------------------------------------------- */

void test_telemetry_init_dispatches(void)
{
    s_init_called = false;
    s_init_ret    = ESP_OK;
    TEST_ASSERT_EQUAL_INT(ESP_OK, s_mock.init());
    TEST_ASSERT_TRUE(s_init_called);
}

void test_telemetry_init_propagates_error(void)
{
    s_init_called = false;
    s_init_ret    = ESP_FAIL;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, s_mock.init());
    TEST_ASSERT_TRUE(s_init_called);
}

void test_telemetry_send_dispatches(void)
{
    s_send_called = false;
    vpd_result_t v = {0};
    s_mock.send(24.5f, 63.0f, &v);
    TEST_ASSERT_TRUE(s_send_called);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.5f, s_last_t);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 63.0f, s_last_rh);
}

void test_telemetry_deinit_dispatches(void)
{
    s_deinit_called = false;
    s_mock.deinit();
    TEST_ASSERT_TRUE(s_deinit_called);
}

void test_telemetry_swap_backend(void)
{
    /* Primeira chamada: backend mock. */
    s_send_called     = false;
    s_alt_send_called = false;
    vpd_result_t v = {0};

    const telemetry_backend_t *b = &s_mock;
    b->send(1.0f, 2.0f, &v);
    TEST_ASSERT_TRUE(s_send_called);
    TEST_ASSERT_FALSE(s_alt_send_called);

    /* Troca de backend: apenas alterar o ponteiro, sem tocar no restante. */
    s_send_called = false;
    b = &s_alt;
    b->send(1.0f, 2.0f, &v);
    TEST_ASSERT_FALSE(s_send_called);
    TEST_ASSERT_TRUE(s_alt_send_called);
}

void test_telemetry_backend_struct_populated(void)
{
    /* Garante que nenhum campo do backend ficou NULL. */
    TEST_ASSERT_NOT_NULL(s_mock.init);
    TEST_ASSERT_NOT_NULL(s_mock.send);
    TEST_ASSERT_NOT_NULL(s_mock.deinit);
}
