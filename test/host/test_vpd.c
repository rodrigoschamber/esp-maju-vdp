#include "unity.h"
#include "vpd.h"

/* --- vpd_svp_kpa ----------------------------------------------------------- */

void test_vpd_svp_at_0c(void)
{
    /* SVP(0) = 0.6108 * exp(0) = 0.6108 kPa */
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.6108f, vpd_svp_kpa(0.0f));
}

void test_vpd_svp_at_20c(void)
{
    /* Valor de referencia: ~2.338 kPa */
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 2.338f, vpd_svp_kpa(20.0f));
}

void test_vpd_svp_at_25c(void)
{
    /* Valor de referencia: ~3.169 kPa */
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 3.169f, vpd_svp_kpa(25.0f));
}

void test_vpd_svp_at_30c(void)
{
    /* Valor de referencia: ~4.243 kPa */
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 4.243f, vpd_svp_kpa(30.0f));
}

/* --- vpd_calculate --------------------------------------------------------- */

void test_vpd_calculate_typical(void)
{
    /* 25 C, 60% UR, offset 2 C → folha a 23 C */
    vpd_result_t v;
    vpd_calculate(25.0f, 60.0f, 2.0f, &v);

    TEST_ASSERT_FLOAT_WITHIN(0.005f, 3.169f, v.svp_ar);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 1.901f, v.avp);           /* 3.169 * 0.60 */
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 1.268f, v.vpd_ar);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,  23.0f,  v.t_folha_c);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 2.809f, v.svp_folha);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,  0.908f, v.vpd_folha);
}

void test_vpd_calculate_zero_offset(void)
{
    /* Sem offset: vpd_folha deve ser igual ao vpd_ar. */
    vpd_result_t v;
    vpd_calculate(25.0f, 50.0f, 0.0f, &v);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, v.vpd_ar, v.vpd_folha);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, v.t_folha_c);
}

void test_vpd_calculate_100_humidity(void)
{
    /* UR = 100%: AVP = SVP, VPD = 0. */
    vpd_result_t v;
    vpd_calculate(25.0f, 100.0f, 0.0f, &v);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v.vpd_ar);
}

void test_vpd_calculate_avp_field(void)
{
    /* AVP = SVP_ar * (UR/100). */
    vpd_result_t v;
    vpd_calculate(20.0f, 70.0f, 0.0f, &v);

    float expected_avp = vpd_svp_kpa(20.0f) * 0.70f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_avp, v.avp);
}

/* --- vpd_classificar ------------------------------------------------------- */

void test_vpd_classificar_baixa(void)
{
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_BAIXA, vpd_classificar(0.0f));
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_BAIXA, vpd_classificar(0.39f));
}

void test_vpd_classificar_propagacao(void)
{
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_PROPAGACAO, vpd_classificar(0.40f));
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_PROPAGACAO, vpd_classificar(0.79f));
}

void test_vpd_classificar_vegetativo(void)
{
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_VEGETATIVO, vpd_classificar(0.80f));
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_VEGETATIVO, vpd_classificar(1.19f));
}

void test_vpd_classificar_floracao(void)
{
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_FLORACAO, vpd_classificar(1.20f));
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_FLORACAO, vpd_classificar(1.60f));
}

void test_vpd_classificar_alta(void)
{
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_ALTA, vpd_classificar(1.61f));
    TEST_ASSERT_EQUAL_INT(VPD_FAIXA_ALTA, vpd_classificar(3.00f));
}

/* --- vpd_faixa_str --------------------------------------------------------- */

void test_vpd_faixa_str_all_non_null(void)
{
    TEST_ASSERT_NOT_NULL(vpd_faixa_str(VPD_FAIXA_BAIXA));
    TEST_ASSERT_NOT_NULL(vpd_faixa_str(VPD_FAIXA_PROPAGACAO));
    TEST_ASSERT_NOT_NULL(vpd_faixa_str(VPD_FAIXA_VEGETATIVO));
    TEST_ASSERT_NOT_NULL(vpd_faixa_str(VPD_FAIXA_FLORACAO));
    TEST_ASSERT_NOT_NULL(vpd_faixa_str(VPD_FAIXA_ALTA));
}
