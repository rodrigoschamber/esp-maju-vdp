#include "unity.h"
#include "i2c_stub.h"
#include "amg8833.h"
#include <string.h>

static amg8833_handle_t make_handle(void)
{
    amg8833_handle_t h = NULL;
    amg8833_create((void *)0x1, AMG8833_ADDR_HIGH, 100000, &h);
    return h;
}

/* Preenche um frame de 128 bytes (little-endian) com o mesmo valor bruto em todos os pixels. */
static void fill_frame(uint8_t out[128], uint16_t raw)
{
    for (int i = 0; i < AMG8833_PIXELS; i++) {
        out[2 * i]     = (uint8_t)(raw & 0xFF);
        out[2 * i + 1] = (uint8_t)(raw >> 8);
    }
}

static void set_pixel(uint8_t frame[128], int idx, uint16_t raw)
{
    frame[2 * idx]     = (uint8_t)(raw & 0xFF);
    frame[2 * idx + 1] = (uint8_t)(raw >> 8);
}

/* --- create / delete -------------------------------------------------------- */

void test_amg8833_create_valid(void)
{
    amg8833_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_create((void *)0x1, AMG8833_ADDR_HIGH, 100000, &h));
    TEST_ASSERT_NOT_NULL(h);
    amg8833_delete(h);
}

void test_amg8833_create_null_bus(void)
{
    amg8833_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_create(NULL, AMG8833_ADDR_HIGH, 100000, &h));
    TEST_ASSERT_NULL(h);
}

void test_amg8833_create_null_out(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_create((void *)0x1, AMG8833_ADDR_HIGH, 100000, NULL));
}

void test_amg8833_delete_valid(void)
{
    amg8833_handle_t h = make_handle();
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_delete(h));
}

void test_amg8833_delete_null_handle(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_delete(NULL));
}

/* --- init ------------------------------------------------------------------- */

void test_amg8833_init_writes_registers(void)
{
    amg8833_handle_t h = make_handle();
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_init(h));

    /* PCTL=normal, RST=initial reset, FPSC=10 fps, INTC=off, nessa ordem. */
    TEST_ASSERT_EQUAL_size_t(4, i2c_stub_tx_count);
    const uint8_t expected[4][2] = {
        { 0x00, 0x00 }, { 0x01, 0x3F }, { 0x02, 0x00 }, { 0x03, 0x00 },
    };
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_size_t(2, i2c_stub_tx_log_len[i]);
        TEST_ASSERT_EQUAL_HEX8(expected[i][0], i2c_stub_tx_log[i][0]);
        TEST_ASSERT_EQUAL_HEX8(expected[i][1], i2c_stub_tx_log[i][1]);
    }
    amg8833_delete(h);
}

void test_amg8833_init_i2c_error(void)
{
    i2c_stub_set_error(ESP_ERR_TIMEOUT);

    amg8833_handle_t h = make_handle();
    TEST_ASSERT_EQUAL_INT(ESP_ERR_TIMEOUT, amg8833_init(h));
    /* Para na primeira escrita que falhou. */
    TEST_ASSERT_EQUAL_size_t(1, i2c_stub_tx_count);
    i2c_stub_reset();
    amg8833_delete(h);
}

void test_amg8833_init_null_handle(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_init(NULL));
}

void test_amg8833_set_frame_rate(void)
{
    amg8833_handle_t h = make_handle();
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_set_frame_rate(h, true));
    TEST_ASSERT_EQUAL_HEX8(0x02, i2c_stub_last_tx[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, i2c_stub_last_tx[1]);
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_set_frame_rate(h, false));
    TEST_ASSERT_EQUAL_HEX8(0x00, i2c_stub_last_tx[1]);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_set_frame_rate(NULL, false));
    amg8833_delete(h);
}

/* --- termistor -------------------------------------------------------------- */

void test_amg8833_thermistor_positive(void)
{
    /* raw 0x0190 = 400 -> 400 * 0.0625 = 25.0 C */
    uint8_t rx[2] = { 0x90, 0x01 };
    i2c_stub_set_rx(rx, sizeof(rx));

    amg8833_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_read_thermistor(h, &c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, c);
    TEST_ASSERT_EQUAL_size_t(1, i2c_stub_last_tx_len);
    TEST_ASSERT_EQUAL_HEX8(0x0E, i2c_stub_last_tx[0]);
    amg8833_delete(h);
}

void test_amg8833_thermistor_negative(void)
{
    /* raw 0x08C8: bit 11 = sinal, magnitude 200 -> -12.5 C */
    uint8_t rx[2] = { 0xC8, 0x08 };
    i2c_stub_set_rx(rx, sizeof(rx));

    amg8833_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_read_thermistor(h, &c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -12.5f, c);
    amg8833_delete(h);
}

/* --- frame ------------------------------------------------------------------ */

void test_amg8833_frame_all_25c(void)
{
    uint8_t raw[128];
    fill_frame(raw, 0x0064);                       /* 100 * 0.25 = 25.0 C */
    uint8_t therm[2] = { 0x90, 0x01 };             /* 25.0 C */
    i2c_stub_push_rx(raw, sizeof(raw));
    i2c_stub_push_rx(therm, sizeof(therm));

    amg8833_handle_t h = make_handle();
    amg8833_frame_t f;
    memset(&f, 0, sizeof(f));
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_read_frame(h, &f));
    for (int i = 0; i < AMG8833_PIXELS; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, f.pixels[i]);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, f.thermistor_c);

    /* Primeira escrita pediu 0x80 (T01L); a segunda, 0x0E (termistor). */
    TEST_ASSERT_EQUAL_size_t(2, i2c_stub_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x80, i2c_stub_tx_log[0][0]);
    TEST_ASSERT_EQUAL_HEX8(0x0E, i2c_stub_tx_log[1][0]);
    amg8833_delete(h);
}

void test_amg8833_frame_mixed_values(void)
{
    uint8_t raw[128];
    fill_frame(raw, 0x0000);
    set_pixel(raw, 1,  0x0064);   /* 25.0 C */
    set_pixel(raw, 2,  0x07FF);   /* 511.75 C (maximo positivo) */
    set_pixel(raw, 3,  0x0FF0);   /* -16 * 0.25 = -4.0 C (complemento de dois) */
    set_pixel(raw, 63, 0x00C8);   /* 50.0 C */
    uint8_t therm[2] = { 0x00, 0x00 };
    i2c_stub_push_rx(raw, sizeof(raw));
    i2c_stub_push_rx(therm, sizeof(therm));

    amg8833_handle_t h = make_handle();
    amg8833_frame_t f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_read_frame(h, &f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f,   0.0f,  f.pixels[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  25.0f,  f.pixels[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 511.75f, f.pixels[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  -4.0f,  f.pixels[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,   0.0f,  f.pixels[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  50.0f,  f.pixels[63]);
    amg8833_delete(h);
}

void test_amg8833_frame_ignores_upper_nibble(void)
{
    /* Bits 15..12 nao fazem parte do valor: 0xF064 deve dar 25.0 C. */
    uint8_t raw[128];
    fill_frame(raw, 0xF064);
    uint8_t therm[2] = { 0x00, 0x00 };
    i2c_stub_push_rx(raw, sizeof(raw));
    i2c_stub_push_rx(therm, sizeof(therm));

    amg8833_handle_t h = make_handle();
    amg8833_frame_t f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, amg8833_read_frame(h, &f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, f.pixels[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, f.pixels[63]);
    amg8833_delete(h);
}

void test_amg8833_frame_i2c_error(void)
{
    i2c_stub_set_error(ESP_ERR_TIMEOUT);

    amg8833_handle_t h = make_handle();
    amg8833_frame_t f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_TIMEOUT, amg8833_read_frame(h, &f));
    i2c_stub_reset();
    amg8833_delete(h);
}

void test_amg8833_frame_null_args(void)
{
    amg8833_frame_t f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_read_frame(NULL, &f));

    amg8833_handle_t h = make_handle();
    float c;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_read_frame(h, NULL));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_read_thermistor(h, NULL));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, amg8833_read_thermistor(NULL, &c));
    amg8833_delete(h);
}
