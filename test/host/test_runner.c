#include "unity.h"
#include "i2c_stub.h"

/* --- declaracoes dos testes (definidos em test_vpd.c) ---------------------- */
void test_vpd_svp_at_0c(void);
void test_vpd_svp_at_20c(void);
void test_vpd_svp_at_25c(void);
void test_vpd_svp_at_30c(void);
void test_vpd_calculate_typical(void);
void test_vpd_calculate_zero_offset(void);
void test_vpd_calculate_100_humidity(void);
void test_vpd_calculate_avp_field(void);
void test_vpd_classificar_baixa(void);
void test_vpd_classificar_propagacao(void);
void test_vpd_classificar_vegetativo(void);
void test_vpd_classificar_floracao(void);
void test_vpd_classificar_alta(void);
void test_vpd_faixa_str_all_non_null(void);

/* --- declaracoes dos testes (definidos em test_sht3x.c) -------------------- */
void test_sht3x_create_valid(void);
void test_sht3x_create_null_bus(void);
void test_sht3x_create_null_out(void);
void test_sht3x_delete_valid(void);
void test_sht3x_delete_null_handle(void);
void test_sht3x_measure_converts_values(void);
void test_sht3x_measure_min_values(void);
void test_sht3x_measure_max_values(void);
void test_sht3x_measure_bad_temperature_crc(void);
void test_sht3x_measure_bad_humidity_crc(void);
void test_sht3x_measure_i2c_error(void);
void test_sht3x_measure_null_handle(void);
void test_sht3x_read_status_valid(void);
void test_sht3x_read_status_bad_crc(void);
void test_sht3x_soft_reset_valid(void);
void test_sht3x_soft_reset_null_handle(void);

/* --- declaracoes dos testes (definidos em test_telemetry.c) ---------------- */
void test_telemetry_init_dispatches(void);
void test_telemetry_init_propagates_error(void);
void test_telemetry_send_dispatches(void);
void test_telemetry_deinit_dispatches(void);
void test_telemetry_swap_backend(void);
void test_telemetry_backend_struct_populated(void);

/* --- setUp/tearDown globais ------------------------------------------------- */

void setUp(void)
{
    i2c_stub_reset();
}

void tearDown(void) {}

/* --- ponto de entrada ------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();

    /* domain/vpd */
    RUN_TEST(test_vpd_svp_at_0c);
    RUN_TEST(test_vpd_svp_at_20c);
    RUN_TEST(test_vpd_svp_at_25c);
    RUN_TEST(test_vpd_svp_at_30c);
    RUN_TEST(test_vpd_calculate_typical);
    RUN_TEST(test_vpd_calculate_zero_offset);
    RUN_TEST(test_vpd_calculate_100_humidity);
    RUN_TEST(test_vpd_calculate_avp_field);
    RUN_TEST(test_vpd_classificar_baixa);
    RUN_TEST(test_vpd_classificar_propagacao);
    RUN_TEST(test_vpd_classificar_vegetativo);
    RUN_TEST(test_vpd_classificar_floracao);
    RUN_TEST(test_vpd_classificar_alta);
    RUN_TEST(test_vpd_faixa_str_all_non_null);

    /* hal/sht3x */
    RUN_TEST(test_sht3x_create_valid);
    RUN_TEST(test_sht3x_create_null_bus);
    RUN_TEST(test_sht3x_create_null_out);
    RUN_TEST(test_sht3x_delete_valid);
    RUN_TEST(test_sht3x_delete_null_handle);
    RUN_TEST(test_sht3x_measure_converts_values);
    RUN_TEST(test_sht3x_measure_min_values);
    RUN_TEST(test_sht3x_measure_max_values);
    RUN_TEST(test_sht3x_measure_bad_temperature_crc);
    RUN_TEST(test_sht3x_measure_bad_humidity_crc);
    RUN_TEST(test_sht3x_measure_i2c_error);
    RUN_TEST(test_sht3x_measure_null_handle);
    RUN_TEST(test_sht3x_read_status_valid);
    RUN_TEST(test_sht3x_read_status_bad_crc);
    RUN_TEST(test_sht3x_soft_reset_valid);
    RUN_TEST(test_sht3x_soft_reset_null_handle);

    /* telemetry (Strategy pattern) */
    RUN_TEST(test_telemetry_init_dispatches);
    RUN_TEST(test_telemetry_init_propagates_error);
    RUN_TEST(test_telemetry_send_dispatches);
    RUN_TEST(test_telemetry_deinit_dispatches);
    RUN_TEST(test_telemetry_swap_backend);
    RUN_TEST(test_telemetry_backend_struct_populated);

    return UNITY_END();
}
