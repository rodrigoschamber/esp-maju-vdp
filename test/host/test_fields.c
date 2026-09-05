#include "unity.h"
#include "telemetry_fields.h"
#include "test_fixtures.h"
#include <math.h>
#include <string.h>

/* --- telemetry_format_fields ----------------------------------------------- */

void test_fields_all_valid_exact_string(void)
{
    char buf[256];
    maju_reading_t r = make_reading_ir();
    int len = telemetry_format_fields(buf, sizeof(buf), &r);

    TEST_ASSERT_EQUAL_STRING(
        "field1=25.00&field2=60.00&field3=1.234&field4=1.567"
        "&field5=22.83&field6=21.10&field7=23.00&field8=24.90",
        buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), len);
}

void test_fields_amg_invalid_omits_field6_to_8(void)
{
    char buf[256];
    maju_reading_t r = make_reading_ir();
    r.th.amg_ok = false;
    TEST_ASSERT_GREATER_THAN(0, telemetry_format_fields(buf, sizeof(buf), &r));

    TEST_ASSERT_NOT_NULL(strstr(buf, "&field5=22.83"));
    TEST_ASSERT_NULL(strstr(buf, "field6"));
    TEST_ASSERT_NULL(strstr(buf, "field7"));
    TEST_ASSERT_NULL(strstr(buf, "field8"));
}

void test_fields_field5_omitted_when_nan(void)
{
    char buf[256];
    maju_reading_t r = make_reading(25.0f, 60.0f, 1.234f, 1.567f);
    r.v.t_folha_c = NAN;
    TEST_ASSERT_GREATER_THAN(0, telemetry_format_fields(buf, sizeof(buf), &r));

    TEST_ASSERT_EQUAL_STRING("field1=25.00&field2=60.00&field3=1.234&field4=1.567", buf);
    TEST_ASSERT_NULL(strstr(buf, "nan"));
}

void test_fields_buffer_too_small_returns_negative(void)
{
    char buf[40];
    maju_reading_t r = make_reading_ir();
    TEST_ASSERT_EQUAL_INT(-1, telemetry_format_fields(buf, sizeof(buf), &r));
    TEST_ASSERT_EQUAL_INT(-1, telemetry_format_fields(NULL, 10, &r));
    TEST_ASSERT_EQUAL_INT(-1, telemetry_format_fields(buf, sizeof(buf), NULL));
}

/* --- telemetry_format_thermal_json ------------------------------------------ */

void test_fields_thermal_json_has_64_values(void)
{
    char buf[768];
    maju_reading_t r = make_reading_ir();
    int len = telemetry_format_thermal_json(buf, sizeof(buf), &r);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), len);

    const char *prefix = "{\"ts_ms\":123456,\"src\":\"mlx\",\"t_leaf\":22.83,\"mlx_tobj\":22.83,";
    TEST_ASSERT_EQUAL_STRING_LEN(prefix, buf, strlen(prefix));

    const char *px = strstr(buf, "\"px\":[");
    TEST_ASSERT_NOT_NULL(px);
    int commas = 0;
    for (const char *p = px; *p && *p != ']'; p++) {
        if (*p == ',') commas++;
    }
    TEST_ASSERT_EQUAL_INT(63, commas);
    TEST_ASSERT_NOT_NULL(strstr(px, "[21.10,21.64,"));   /* px[0], px[1] */
    TEST_ASSERT_EQUAL_STRING("]}", buf + len - 2);
}

void test_fields_thermal_json_null_when_mlx_fail(void)
{
    char buf[768];
    maju_reading_t r = make_reading_ir();
    r.th.mlx_ok = false;
    thermal_select_leaf(&r.th);
    TEST_ASSERT_GREATER_THAN(0, telemetry_format_thermal_json(buf, sizeof(buf), &r));

    TEST_ASSERT_NOT_NULL(strstr(buf, "\"src\":\"amg\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"t_leaf\":23.00"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"mlx_tobj\":null"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"mlx_ta\":null"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"therm\":25.10"));
}

void test_fields_thermal_json_buffer_too_small(void)
{
    char buf[128];
    maju_reading_t r = make_reading_ir();
    TEST_ASSERT_EQUAL_INT(-1, telemetry_format_thermal_json(buf, sizeof(buf), &r));
}
