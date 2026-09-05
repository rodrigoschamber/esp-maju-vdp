#include "unity.h"
#include "i2c_stub.h"
#include <stdbool.h>

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
void test_vpd_calculate_leaf_typical(void);
void test_vpd_calculate_leaf_equals_air(void);
void test_vpd_calculate_wrapper_matches_leaf(void);

/* --- declaracoes dos testes (definidos em test_thermal.c) ------------------ */
void test_thermal_stats_fixture_frame(void);
void test_thermal_stats_constant_frame(void);
void test_thermal_select_both_ok_prefers_mlx(void);
void test_thermal_select_mlx_ok_amg_fail(void);
void test_thermal_select_mlx_fail_falls_back_to_amg_avg(void);
void test_thermal_select_both_fail_none_and_nan(void);
void test_thermal_select_mlx_implausible_falls_back(void);
void test_thermal_source_str_values(void);
void test_thermal_reset_clears_flags(void);

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

/* --- declaracoes dos testes (definidos em test_mlx90614.c) ----------------- */
void test_mlx90614_create_valid(void);
void test_mlx90614_create_null_bus(void);
void test_mlx90614_create_null_out(void);
void test_mlx90614_delete_valid(void);
void test_mlx90614_delete_null_handle(void);
void test_mlx90614_pec_known_vectors(void);
void test_mlx90614_read_object_typical(void);
void test_mlx90614_read_ambient_typical(void);
void test_mlx90614_read_object_min(void);
void test_mlx90614_read_object_max(void);
void test_mlx90614_read_object_bad_pec(void);
void test_mlx90614_read_object_error_flag(void);
void test_mlx90614_pec_depends_on_address(void);
void test_mlx90614_read_i2c_error(void);
void test_mlx90614_read_null_args(void);
void test_mlx90614_read_emissivity_default(void);
void test_mlx90614_read_emissivity_096(void);

/* --- declaracoes dos testes (definidos em test_amg8833.c) ------------------ */
void test_amg8833_create_valid(void);
void test_amg8833_create_null_bus(void);
void test_amg8833_create_null_out(void);
void test_amg8833_delete_valid(void);
void test_amg8833_delete_null_handle(void);
void test_amg8833_init_writes_registers(void);
void test_amg8833_init_i2c_error(void);
void test_amg8833_init_null_handle(void);
void test_amg8833_set_frame_rate(void);
void test_amg8833_thermistor_positive(void);
void test_amg8833_thermistor_negative(void);
void test_amg8833_frame_all_25c(void);
void test_amg8833_frame_mixed_values(void);
void test_amg8833_frame_ignores_upper_nibble(void);
void test_amg8833_frame_i2c_error(void);
void test_amg8833_frame_null_args(void);

/* --- declaracoes dos testes (definidos em test_telemetry.c) ---------------- */
void test_telemetry_init_dispatches(void);
void test_telemetry_init_propagates_error(void);
void test_telemetry_send_dispatches(void);
void test_telemetry_deinit_dispatches(void);
void test_telemetry_swap_backend(void);
void test_telemetry_backend_struct_populated(void);

/* --- declaracoes dos testes (definidos em test_fields.c) ------------------- */
void test_fields_all_valid_exact_string(void);
void test_fields_amg_invalid_omits_field6_to_8(void);
void test_fields_field5_omitted_when_nan(void);
void test_fields_buffer_too_small_returns_negative(void);
void test_fields_thermal_json_has_64_values(void);
void test_fields_thermal_json_null_when_mlx_fail(void);
void test_fields_thermal_json_buffer_too_small(void);

/* --- declaracoes dos testes (definidos em test_thingspeak.c) --------------- */
void test_thingspeak_backend_struct_populated(void);
void test_thingspeak_init_returns_ok(void);
void test_thingspeak_send_payload_fields(void);
void test_thingspeak_send_payload_ir_fields(void);
void test_thingspeak_send_omits_amg_fields_when_invalid(void);
void test_thingspeak_send_success_entry_id(void);
void test_thingspeak_send_empty_response_no_crash(void);
void test_thingspeak_send_http_error_no_crash(void);
void test_thingspeak_send_perform_fail_no_crash(void);
void test_thingspeak_send_init_null_no_crash(void);
void test_thingspeak_url_set_in_request(void);

/* controle do stub HTTP (definido em thingspeak_http_stub.c) */
void ts_stub_reset(void);

/* controle do stub MQTT (definido em mqtt_stub.c) */
void mqtt_stub_reset(void);

/* --- declaracoes dos testes (definidos em test_dispatch.c) ----------------- */
void test_dispatch_init_calls_each_backend_init(void);
void test_dispatch_send_routes_to_all_backends(void);
void test_dispatch_send_delivers_correct_values(void);
void test_dispatch_send_delivers_thermal_frame(void);
void test_dispatch_send_is_nonblocking_before_process(void);
void test_dispatch_slow_backend_does_not_prevent_other(void);
void test_dispatch_multiple_readings_buffered(void);
void test_dispatch_init_error_continues_other_backends(void);
void test_dispatch_deinit_resets_state(void);

/* --- declaracoes dos testes (definidos em test_mqtt.c) --------------------- */
void test_mqtt_backend_struct_populated(void);
void test_mqtt_init_returns_ok_when_connected(void);
void test_mqtt_init_fails_when_client_null(void);
void test_mqtt_init_fails_when_start_fails(void);
void test_mqtt_init_fails_when_register_fails(void);
void test_mqtt_send_publishes_to_correct_topic(void);
void test_mqtt_send_payload_contains_temperature(void);
void test_mqtt_send_payload_contains_humidity(void);
void test_mqtt_send_payload_contains_vpd_fields(void);
void test_mqtt_send_increments_publish_count(void);
void test_mqtt_send_skips_when_not_connected(void);
void test_mqtt_send_skips_after_disconnect(void);
void test_mqtt_send_handles_publish_failure_no_crash(void);
void test_mqtt_send_payload_contains_ir_fields(void);
void test_mqtt_send_payload_src_none_without_ir(void);
void test_mqtt_send_payload_src_amg_on_fallback(void);
void test_mqtt_send_no_thermal_frame_when_amg_fail(void);
void test_mqtt_send_publishes_thermal_frame_on_separate_topic(void);
void test_mqtt_thermal_frame_skipped_when_not_connected(void);
void test_mqtt_deinit_idempotent(void);
void test_mqtt_deinit_clears_connected_state(void);
void test_mqtt_failure_does_not_block_thingspeak(void);
void test_thingspeak_failure_does_not_affect_mqtt(void);
void test_mqtt_unavailable_thingspeak_still_sends(void);
void test_mqtt_error_tcp_transport_no_crash(void);
void test_mqtt_error_connection_refused_no_crash(void);

/* --- setUp/tearDown globais ------------------------------------------------- */

void setUp(void)
{
    i2c_stub_reset();
    ts_stub_reset();
    mqtt_stub_reset();
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
    RUN_TEST(test_vpd_calculate_leaf_typical);
    RUN_TEST(test_vpd_calculate_leaf_equals_air);
    RUN_TEST(test_vpd_calculate_wrapper_matches_leaf);

    /* domain/thermal */
    RUN_TEST(test_thermal_stats_fixture_frame);
    RUN_TEST(test_thermal_stats_constant_frame);
    RUN_TEST(test_thermal_select_both_ok_prefers_mlx);
    RUN_TEST(test_thermal_select_mlx_ok_amg_fail);
    RUN_TEST(test_thermal_select_mlx_fail_falls_back_to_amg_avg);
    RUN_TEST(test_thermal_select_both_fail_none_and_nan);
    RUN_TEST(test_thermal_select_mlx_implausible_falls_back);
    RUN_TEST(test_thermal_source_str_values);
    RUN_TEST(test_thermal_reset_clears_flags);

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

    /* hal/mlx90614 */
    RUN_TEST(test_mlx90614_create_valid);
    RUN_TEST(test_mlx90614_create_null_bus);
    RUN_TEST(test_mlx90614_create_null_out);
    RUN_TEST(test_mlx90614_delete_valid);
    RUN_TEST(test_mlx90614_delete_null_handle);
    RUN_TEST(test_mlx90614_pec_known_vectors);
    RUN_TEST(test_mlx90614_read_object_typical);
    RUN_TEST(test_mlx90614_read_ambient_typical);
    RUN_TEST(test_mlx90614_read_object_min);
    RUN_TEST(test_mlx90614_read_object_max);
    RUN_TEST(test_mlx90614_read_object_bad_pec);
    RUN_TEST(test_mlx90614_read_object_error_flag);
    RUN_TEST(test_mlx90614_pec_depends_on_address);
    RUN_TEST(test_mlx90614_read_i2c_error);
    RUN_TEST(test_mlx90614_read_null_args);
    RUN_TEST(test_mlx90614_read_emissivity_default);
    RUN_TEST(test_mlx90614_read_emissivity_096);

    /* hal/amg8833 */
    RUN_TEST(test_amg8833_create_valid);
    RUN_TEST(test_amg8833_create_null_bus);
    RUN_TEST(test_amg8833_create_null_out);
    RUN_TEST(test_amg8833_delete_valid);
    RUN_TEST(test_amg8833_delete_null_handle);
    RUN_TEST(test_amg8833_init_writes_registers);
    RUN_TEST(test_amg8833_init_i2c_error);
    RUN_TEST(test_amg8833_init_null_handle);
    RUN_TEST(test_amg8833_set_frame_rate);
    RUN_TEST(test_amg8833_thermistor_positive);
    RUN_TEST(test_amg8833_thermistor_negative);
    RUN_TEST(test_amg8833_frame_all_25c);
    RUN_TEST(test_amg8833_frame_mixed_values);
    RUN_TEST(test_amg8833_frame_ignores_upper_nibble);
    RUN_TEST(test_amg8833_frame_i2c_error);
    RUN_TEST(test_amg8833_frame_null_args);

    /* telemetry (Strategy pattern) */
    RUN_TEST(test_telemetry_init_dispatches);
    RUN_TEST(test_telemetry_init_propagates_error);
    RUN_TEST(test_telemetry_send_dispatches);
    RUN_TEST(test_telemetry_deinit_dispatches);
    RUN_TEST(test_telemetry_swap_backend);
    RUN_TEST(test_telemetry_backend_struct_populated);

    /* telemetry/fields */
    RUN_TEST(test_fields_all_valid_exact_string);
    RUN_TEST(test_fields_amg_invalid_omits_field6_to_8);
    RUN_TEST(test_fields_field5_omitted_when_nan);
    RUN_TEST(test_fields_buffer_too_small_returns_negative);
    RUN_TEST(test_fields_thermal_json_has_64_values);
    RUN_TEST(test_fields_thermal_json_null_when_mlx_fail);
    RUN_TEST(test_fields_thermal_json_buffer_too_small);

    /* telemetry/thingspeak */
    RUN_TEST(test_thingspeak_backend_struct_populated);
    RUN_TEST(test_thingspeak_init_returns_ok);
    RUN_TEST(test_thingspeak_send_payload_fields);
    RUN_TEST(test_thingspeak_send_payload_ir_fields);
    RUN_TEST(test_thingspeak_send_omits_amg_fields_when_invalid);
    RUN_TEST(test_thingspeak_send_success_entry_id);
    RUN_TEST(test_thingspeak_send_empty_response_no_crash);
    RUN_TEST(test_thingspeak_send_http_error_no_crash);
    RUN_TEST(test_thingspeak_send_perform_fail_no_crash);
    RUN_TEST(test_thingspeak_send_init_null_no_crash);
    RUN_TEST(test_thingspeak_url_set_in_request);

    /* telemetry/mqtt */
    RUN_TEST(test_mqtt_backend_struct_populated);
    RUN_TEST(test_mqtt_init_returns_ok_when_connected);
    RUN_TEST(test_mqtt_init_fails_when_client_null);
    RUN_TEST(test_mqtt_init_fails_when_start_fails);
    RUN_TEST(test_mqtt_init_fails_when_register_fails);
    RUN_TEST(test_mqtt_send_publishes_to_correct_topic);
    RUN_TEST(test_mqtt_send_payload_contains_temperature);
    RUN_TEST(test_mqtt_send_payload_contains_humidity);
    RUN_TEST(test_mqtt_send_payload_contains_vpd_fields);
    RUN_TEST(test_mqtt_send_increments_publish_count);
    RUN_TEST(test_mqtt_send_skips_when_not_connected);
    RUN_TEST(test_mqtt_send_skips_after_disconnect);
    RUN_TEST(test_mqtt_send_handles_publish_failure_no_crash);
    RUN_TEST(test_mqtt_send_payload_contains_ir_fields);
    RUN_TEST(test_mqtt_send_payload_src_none_without_ir);
    RUN_TEST(test_mqtt_send_payload_src_amg_on_fallback);
    RUN_TEST(test_mqtt_send_no_thermal_frame_when_amg_fail);
    RUN_TEST(test_mqtt_send_publishes_thermal_frame_on_separate_topic);
    RUN_TEST(test_mqtt_thermal_frame_skipped_when_not_connected);
    RUN_TEST(test_mqtt_deinit_idempotent);
    RUN_TEST(test_mqtt_deinit_clears_connected_state);
    RUN_TEST(test_mqtt_failure_does_not_block_thingspeak);
    RUN_TEST(test_thingspeak_failure_does_not_affect_mqtt);
    RUN_TEST(test_mqtt_unavailable_thingspeak_still_sends);
    RUN_TEST(test_mqtt_error_tcp_transport_no_crash);
    RUN_TEST(test_mqtt_error_connection_refused_no_crash);

    /* telemetry/dispatch */
    RUN_TEST(test_dispatch_init_calls_each_backend_init);
    RUN_TEST(test_dispatch_send_routes_to_all_backends);
    RUN_TEST(test_dispatch_send_delivers_correct_values);
    RUN_TEST(test_dispatch_send_delivers_thermal_frame);
    RUN_TEST(test_dispatch_send_is_nonblocking_before_process);
    RUN_TEST(test_dispatch_slow_backend_does_not_prevent_other);
    RUN_TEST(test_dispatch_multiple_readings_buffered);
    RUN_TEST(test_dispatch_init_error_continues_other_backends);
    RUN_TEST(test_dispatch_deinit_resets_state);

    return UNITY_END();
}
