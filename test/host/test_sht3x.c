#include "unity.h"
#include "i2c_stub.h"
#include "sht3x.h"

/* CRC-8 Sensirion (polinomio 0x31, init 0xFF) — replica local para gerar vetores. */
static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

static void build_measure_rx(uint16_t raw_t, uint16_t raw_rh, uint8_t out[6])
{
    out[0] = (uint8_t)(raw_t >> 8);
    out[1] = (uint8_t)(raw_t & 0xFF);
    out[2] = crc8(&out[0], 2);
    out[3] = (uint8_t)(raw_rh >> 8);
    out[4] = (uint8_t)(raw_rh & 0xFF);
    out[5] = crc8(&out[3], 2);
}

/* helper: cria handle reutilizando o bus stub */
static sht3x_handle_t make_handle(void)
{
    sht3x_handle_t h = NULL;
    sht3x_create((void *)0x1, SHT3X_ADDR_LOW, 100000, &h);
    return h;
}

/* --- sht3x_create / sht3x_delete ------------------------------------------ */

void test_sht3x_create_valid(void)
{
    sht3x_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_create((void *)0x1, SHT3X_ADDR_LOW, 100000, &h));
    TEST_ASSERT_NOT_NULL(h);
    sht3x_delete(h);
}

void test_sht3x_create_null_bus(void)
{
    sht3x_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, sht3x_create(NULL, SHT3X_ADDR_LOW, 100000, &h));
    TEST_ASSERT_NULL(h);
}

void test_sht3x_create_null_out(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, sht3x_create((void *)0x1, SHT3X_ADDR_LOW, 100000, NULL));
}

void test_sht3x_delete_valid(void)
{
    sht3x_handle_t h = make_handle();
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_delete(h));
}

void test_sht3x_delete_null_handle(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, sht3x_delete(NULL));
}

/* --- sht3x_measure_single -------------------------------------------------- */

void test_sht3x_measure_converts_values(void)
{
    /* raw_t = 26214 (0x6666) → T ≈ 25.0 C
     * raw_rh = 32768 (0x8000) → RH ≈ 50.0 % */
    uint8_t rx[6];
    build_measure_rx(0x6666, 0x8000, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    float t = 0.0f, rh = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_measure_single(h, SHT3X_REP_HIGH, &t, &rh));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, t);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, rh);
    sht3x_delete(h);
}

void test_sht3x_measure_min_values(void)
{
    /* raw = 0 → T = -45 C, RH = 0 % */
    uint8_t rx[6];
    build_measure_rx(0x0000, 0x0000, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    float t = 0.0f, rh = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_measure_single(h, SHT3X_REP_HIGH, &t, &rh));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -45.0f, t);
    TEST_ASSERT_FLOAT_WITHIN(0.1f,   0.0f, rh);
    sht3x_delete(h);
}

void test_sht3x_measure_max_values(void)
{
    /* raw = 0xFFFF → T = 130 C, RH = 100 % */
    uint8_t rx[6];
    build_measure_rx(0xFFFF, 0xFFFF, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    float t = 0.0f, rh = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_measure_single(h, SHT3X_REP_HIGH, &t, &rh));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 130.0f, t);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, rh);
    sht3x_delete(h);
}

void test_sht3x_measure_bad_temperature_crc(void)
{
    uint8_t rx[6];
    build_measure_rx(0x6666, 0x8000, rx);
    rx[2] ^= 0xFF; /* corrompe o CRC da temperatura */
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    float t, rh;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_CRC, sht3x_measure_single(h, SHT3X_REP_HIGH, &t, &rh));
    sht3x_delete(h);
}

void test_sht3x_measure_bad_humidity_crc(void)
{
    uint8_t rx[6];
    build_measure_rx(0x6666, 0x8000, rx);
    rx[5] ^= 0xFF; /* corrompe o CRC da umidade */
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    float t, rh;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_CRC, sht3x_measure_single(h, SHT3X_REP_HIGH, &t, &rh));
    sht3x_delete(h);
}

void test_sht3x_measure_i2c_error(void)
{
    i2c_stub_set_error(ESP_ERR_TIMEOUT);

    sht3x_handle_t h = make_handle();
    float t, rh;
    /* O timeout e retornado ao tentar enviar o comando de medicao. */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, sht3x_measure_single(h, SHT3X_REP_HIGH, &t, &rh));
    i2c_stub_reset();
    sht3x_delete(h);
}

void test_sht3x_measure_null_handle(void)
{
    float t, rh;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, sht3x_measure_single(NULL, SHT3X_REP_HIGH, &t, &rh));
}

/* --- sht3x_read_status ----------------------------------------------------- */

void test_sht3x_read_status_valid(void)
{
    uint8_t rx[3] = { 0x80, 0x10, 0 };
    rx[2] = crc8(rx, 2);
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    uint16_t status = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_read_status(h, &status));
    TEST_ASSERT_EQUAL_HEX16(0x8010, status);
    sht3x_delete(h);
}

void test_sht3x_read_status_bad_crc(void)
{
    uint8_t rx[3] = { 0x80, 0x10, 0xAB }; /* CRC incorreto */
    i2c_stub_set_rx(rx, sizeof(rx));

    sht3x_handle_t h = make_handle();
    uint16_t status = 0;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_CRC, sht3x_read_status(h, &status));
    sht3x_delete(h);
}

/* --- sht3x_soft_reset ------------------------------------------------------ */

void test_sht3x_soft_reset_valid(void)
{
    sht3x_handle_t h = make_handle();
    TEST_ASSERT_EQUAL_INT(ESP_OK, sht3x_soft_reset(h));
    sht3x_delete(h);
}

void test_sht3x_soft_reset_null_handle(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, sht3x_soft_reset(NULL));
}
