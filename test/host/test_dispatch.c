#include "unity.h"
#include "telemetry_dispatch.h"
#include "telemetry.h"
#include "test_fixtures.h"
#include <stdbool.h>
#include <string.h>

/* --- mocks ----------------------------------------------------------------- */

static int   s_a_init_count;
static int   s_a_send_count;
static float s_a_last_t;
static float s_a_last_rh;
static float s_a_last_px63;
static int   s_a_last_fonte;

static int   s_b_init_count;
static int   s_b_send_count;
static bool  s_b_send_blocks; /* simula backend lento */

static esp_err_t mock_a_init(void)  { s_a_init_count++; return ESP_OK; }
static void      mock_a_deinit(void) {}
static void      mock_a_send(const maju_reading_t *r)
{
    s_a_send_count++;
    s_a_last_t     = r->t_ar;
    s_a_last_rh    = r->rh;
    s_a_last_px63  = r->th.amg_px[63];
    s_a_last_fonte = (int)r->th.fonte;
}

static esp_err_t mock_b_init(void)  { s_b_init_count++; return ESP_OK; }
static void      mock_b_deinit(void) {}
static void      mock_b_send(const maju_reading_t *r)
{
    (void)r;
    s_b_send_count++;
    /* se s_b_send_blocks estiver ativo, simularia longa espera — aqui apenas conta */
}

/* backend que falha no init, usado em test_dispatch_init_error_continues_other_backends */
static esp_err_t failing_init(void)                    { return ESP_FAIL; }
static void      failing_send(const maju_reading_t *r) { (void)r; }
static void      failing_deinit(void)                  {}

static const telemetry_backend_t s_backend_a = {
    .init   = mock_a_init,
    .send   = mock_a_send,
    .deinit = mock_a_deinit,
};

static const telemetry_backend_t s_backend_b = {
    .init   = mock_b_init,
    .send   = mock_b_send,
    .deinit = mock_b_deinit,
};

static const telemetry_backend_t *const s_two_backends[] = {
    &s_backend_a,
    &s_backend_b,
    NULL,
};

static void reset_mocks(void)
{
    s_a_init_count = 0;
    s_a_send_count = 0;
    s_a_last_t     = 0.0f;
    s_a_last_rh    = 0.0f;
    s_a_last_px63  = 0.0f;
    s_a_last_fonte = -1;
    s_b_init_count = 0;
    s_b_send_count = 0;
    s_b_send_blocks = false;
}

static maju_reading_t make_r(float t, float rh)
{
    return make_reading(t, rh, 1.0f, 2.0f);
}

/* --- testes ---------------------------------------------------------------- */

void test_dispatch_init_calls_each_backend_init(void)
{
    reset_mocks();
    TEST_ASSERT_EQUAL_INT(ESP_OK, telemetry_dispatch_init(s_two_backends));
    TEST_ASSERT_EQUAL_INT(1, s_a_init_count);
    TEST_ASSERT_EQUAL_INT(1, s_b_init_count);
    telemetry_dispatch_deinit();
}

void test_dispatch_send_routes_to_all_backends(void)
{
    reset_mocks();
    telemetry_dispatch_init(s_two_backends);

    maju_reading_t r = make_r(25.0f, 60.0f);
    telemetry_dispatch_send(&r);
    telemetry_dispatch_process_pending();

    TEST_ASSERT_EQUAL_INT(1, s_a_send_count);
    TEST_ASSERT_EQUAL_INT(1, s_b_send_count);
    telemetry_dispatch_deinit();
}

void test_dispatch_send_delivers_correct_values(void)
{
    reset_mocks();
    telemetry_dispatch_init(s_two_backends);

    maju_reading_t r = make_r(22.5f, 55.0f);
    telemetry_dispatch_send(&r);
    telemetry_dispatch_process_pending();

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.5f, s_a_last_t);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 55.0f, s_a_last_rh);
    telemetry_dispatch_deinit();
}

void test_dispatch_send_delivers_thermal_frame(void)
{
    /* O frame de 64 pixels e a fonte da folha atravessam a fila intactos. */
    reset_mocks();
    telemetry_dispatch_init(s_two_backends);

    maju_reading_t r = make_reading_ir();
    telemetry_dispatch_send(&r);
    telemetry_dispatch_process_pending();

    TEST_ASSERT_EQUAL_INT(LEAF_SRC_MLX, s_a_last_fonte);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, r.th.amg_px[63], s_a_last_px63);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.90f, s_a_last_px63);
    telemetry_dispatch_deinit();
}

void test_dispatch_send_is_nonblocking_before_process(void)
{
    /* dispatch_send deve encaminhar para filas sem chamar send() ainda. */
    reset_mocks();
    telemetry_dispatch_init(s_two_backends);

    maju_reading_t r = make_r(25.0f, 60.0f);
    telemetry_dispatch_send(&r);

    /* Sem process_pending, send() nao deve ter sido chamado. */
    TEST_ASSERT_EQUAL_INT(0, s_a_send_count);
    TEST_ASSERT_EQUAL_INT(0, s_b_send_count);
    telemetry_dispatch_deinit();
}

void test_dispatch_slow_backend_does_not_prevent_other(void)
{
    /*
     * Mesmo que o backend B seja lento (s_b_send_blocks), o backend A deve
     * receber a mensagem independentemente — ambos sao enfileirados antes de
     * qualquer send() ser chamado.
     */
    reset_mocks();
    s_b_send_blocks = true;
    telemetry_dispatch_init(s_two_backends);

    maju_reading_t r = make_r(25.0f, 60.0f);
    telemetry_dispatch_send(&r);

    /* Ambos foram enfileirados; process drena na ordem, mas A sempre recebe. */
    telemetry_dispatch_process_pending();
    TEST_ASSERT_EQUAL_INT(1, s_a_send_count);
    TEST_ASSERT_EQUAL_INT(1, s_b_send_count);
    telemetry_dispatch_deinit();
}

void test_dispatch_multiple_readings_buffered(void)
{
    reset_mocks();
    telemetry_dispatch_init(s_two_backends);

    maju_reading_t r1 = make_r(1.0f, 10.0f);
    maju_reading_t r2 = make_r(2.0f, 20.0f);
    maju_reading_t r3 = make_r(3.0f, 30.0f);
    telemetry_dispatch_send(&r1);
    telemetry_dispatch_send(&r2);
    telemetry_dispatch_send(&r3);

    /* Nenhum send() chamado ate aqui. */
    TEST_ASSERT_EQUAL_INT(0, s_a_send_count);

    telemetry_dispatch_process_pending();
    TEST_ASSERT_EQUAL_INT(3, s_a_send_count);
    TEST_ASSERT_EQUAL_INT(3, s_b_send_count);
    telemetry_dispatch_deinit();
}

void test_dispatch_init_error_continues_other_backends(void)
{
    /* Um backend com init() que falha nao deve impedir os demais. */
    static const telemetry_backend_t bad = {
        .init   = failing_init,
        .send   = failing_send,
        .deinit = failing_deinit,
    };

    reset_mocks();
    static const telemetry_backend_t *const backends[] = { &bad, &s_backend_a, NULL };
    /* Deve retornar ESP_OK (init de bad falha mas nao aborta o dispatch). */
    TEST_ASSERT_EQUAL_INT(ESP_OK, telemetry_dispatch_init(backends));

    maju_reading_t r = make_r(5.0f, 50.0f);
    telemetry_dispatch_send(&r);
    telemetry_dispatch_process_pending();

    TEST_ASSERT_EQUAL_INT(1, s_a_send_count);
    telemetry_dispatch_deinit();
}

void test_dispatch_deinit_resets_state(void)
{
    reset_mocks();
    telemetry_dispatch_init(s_two_backends);
    telemetry_dispatch_deinit();

    /* Apos deinit, nao deve haver state residual — reinit deve funcionar. */
    reset_mocks();
    TEST_ASSERT_EQUAL_INT(ESP_OK, telemetry_dispatch_init(s_two_backends));
    TEST_ASSERT_EQUAL_INT(1, s_a_init_count);
    telemetry_dispatch_deinit();
}
