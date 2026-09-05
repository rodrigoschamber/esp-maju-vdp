#include "unity.h"
#include "i2c_stub.h"
#include "mlx90614.h"

/* PEC do SMBus (CRC-8, polinomio 0x07, init 0x00) — replica local para gerar vetores. */
static uint8_t pec8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* Monta os 3 bytes devolvidos pelo sensor (LSB, MSB, PEC) para um Read Word. */
static void build_word_rx(uint8_t addr, uint8_t cmd, uint16_t raw, uint8_t out[3])
{
    out[0] = (uint8_t)(raw & 0xFF);
    out[1] = (uint8_t)(raw >> 8);
    uint8_t frame[5] = {
        (uint8_t)(addr << 1), cmd, (uint8_t)((addr << 1) | 1), out[0], out[1],
    };
    out[2] = pec8(frame, sizeof(frame));
}

static mlx90614_handle_t make_handle(void)
{
    mlx90614_handle_t h = NULL;
    mlx90614_create((void *)0x1, MLX90614_ADDR_DEFAULT, 100000, &h);
    return h;
}

/* --- create / delete -------------------------------------------------------- */

void test_mlx90614_create_valid(void)
{
    mlx90614_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_create((void *)0x1, MLX90614_ADDR_DEFAULT, 100000, &h));
    TEST_ASSERT_NOT_NULL(h);
    mlx90614_delete(h);
}

void test_mlx90614_create_null_bus(void)
{
    mlx90614_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_create(NULL, MLX90614_ADDR_DEFAULT, 100000, &h));
    TEST_ASSERT_NULL(h);
}

void test_mlx90614_create_null_out(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_create((void *)0x1, MLX90614_ADDR_DEFAULT, 100000, NULL));
}

void test_mlx90614_delete_valid(void)
{
    mlx90614_handle_t h = make_handle();
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_delete(h));
}

void test_mlx90614_delete_null_handle(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_delete(NULL));
}

/* --- PEC -------------------------------------------------------------------- */

void test_mlx90614_pec_known_vectors(void)
{
    /* Vetores calculados externamente: fixam a definicao do CRC-8 SMBus. */
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x3ABB, rx);
    TEST_ASSERT_EQUAL_HEX8(0xBB, rx[0]);
    TEST_ASSERT_EQUAL_HEX8(0x3A, rx[1]);
    TEST_ASSERT_EQUAL_HEX8(0x78, rx[2]);

    build_word_rx(0x5A, 0x06, 0x3ABB, rx);
    TEST_ASSERT_EQUAL_HEX8(0x6E, rx[2]);

    build_word_rx(0x5A, 0x24, 0xFFFF, rx);
    TEST_ASSERT_EQUAL_HEX8(0xD6, rx[2]);
}

/* --- leitura de temperatura ------------------------------------------------- */

void test_mlx90614_read_object_typical(void)
{
    /* raw 0x3ABB = 15035 -> 300.70 K -> 27.55 C */
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x3ABB, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_read_object(h, &c));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 27.55f, c);
    TEST_ASSERT_EQUAL_size_t(1, i2c_stub_last_tx_len);
    TEST_ASSERT_EQUAL_HEX8(0x07, i2c_stub_last_tx[0]);
    mlx90614_delete(h);
}

void test_mlx90614_read_ambient_typical(void)
{
    uint8_t rx[3];
    build_word_rx(0x5A, 0x06, 0x3ABB, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_read_ambient(h, &c));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 27.55f, c);
    TEST_ASSERT_EQUAL_HEX8(0x06, i2c_stub_last_tx[0]);
    mlx90614_delete(h);
}

void test_mlx90614_read_object_min(void)
{
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x0000, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_read_object(h, &c));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -273.15f, c);
    mlx90614_delete(h);
}

void test_mlx90614_read_object_max(void)
{
    /* 0x7FFF = 32767 -> 655.34 K -> 382.19 C (maior valor sem flag de erro) */
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x7FFF, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_read_object(h, &c));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 382.19f, c);
    mlx90614_delete(h);
}

void test_mlx90614_read_object_bad_pec(void)
{
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x3ABB, rx);
    rx[2] ^= 0x01; /* corrompe o PEC */
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_CRC, mlx90614_read_object(h, &c));
    mlx90614_delete(h);
}

void test_mlx90614_read_object_error_flag(void)
{
    /* Bit 15 setado com PEC valido -> sensor sinalizou erro. */
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x8000, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_RESPONSE, mlx90614_read_object(h, &c));
    mlx90614_delete(h);
}

void test_mlx90614_pec_depends_on_address(void)
{
    /* Resposta gerada para 0x5A, mas o handle esta em 0x5B: o PEC nao pode bater. */
    uint8_t rx[3];
    build_word_rx(0x5A, 0x07, 0x3ABB, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = NULL;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_create((void *)0x1, 0x5B, 100000, &h));
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_CRC, mlx90614_read_object(h, &c));
    mlx90614_delete(h);
}

void test_mlx90614_read_i2c_error(void)
{
    i2c_stub_set_error(ESP_ERR_TIMEOUT);

    mlx90614_handle_t h = make_handle();
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_TIMEOUT, mlx90614_read_object(h, &c));
    i2c_stub_reset();
    mlx90614_delete(h);
}

void test_mlx90614_read_null_args(void)
{
    float c = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_read_object(NULL, &c));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_read_ambient(NULL, &c));

    mlx90614_handle_t h = make_handle();
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_read_object(h, NULL));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, mlx90614_read_emissivity(h, NULL));
    mlx90614_delete(h);
}

/* --- emissividade ----------------------------------------------------------- */

void test_mlx90614_read_emissivity_default(void)
{
    uint8_t rx[3];
    build_word_rx(0x5A, 0x24, 0xFFFF, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float e = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_read_emissivity(h, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, e);
    TEST_ASSERT_EQUAL_HEX8(0x24, i2c_stub_last_tx[0]);
    mlx90614_delete(h);
}

void test_mlx90614_read_emissivity_096(void)
{
    /* 0xF5C2 = 62914 -> 0.9600 */
    uint8_t rx[3];
    build_word_rx(0x5A, 0x24, 0xF5C2, rx);
    i2c_stub_set_rx(rx, sizeof(rx));

    mlx90614_handle_t h = make_handle();
    float e = 0.0f;
    TEST_ASSERT_EQUAL_INT(ESP_OK, mlx90614_read_emissivity(h, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.96f, e);
    mlx90614_delete(h);
}
