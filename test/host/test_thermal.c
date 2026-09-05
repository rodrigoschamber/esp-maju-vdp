#include "unity.h"
#include "thermal.h"
#include <math.h>
#include <string.h>

/* Frame de referencia: px[i] = 21.10 + (i % 8) * 0.542857 -> min 21.10, max 24.90, media 23.00 */
static void fill_fixture_frame(float px[THERMAL_PIXELS])
{
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        px[i] = 21.10f + (float)(i % 8) * 0.542857f;
    }
}

static void make_ir_ok(thermal_t *th)
{
    thermal_reset(th);
    th->mlx_ok     = true;
    th->mlx_tobj_c = 22.83f;
    th->mlx_ta_c   = 24.90f;
    th->amg_ok     = true;
    th->amg_therm_c = 25.10f;
    fill_fixture_frame(th->amg_px);
    thermal_stats(th->amg_px, &th->amg_min_c, &th->amg_max_c, &th->amg_avg_c);
}

/* --- thermal_stats ---------------------------------------------------------- */

void test_thermal_stats_fixture_frame(void)
{
    float px[THERMAL_PIXELS];
    fill_fixture_frame(px);

    float mn = 0, mx = 0, avg = 0;
    thermal_stats(px, &mn, &mx, &avg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.10f, mn);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.90f, mx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.00f, avg);
}

void test_thermal_stats_constant_frame(void)
{
    float px[THERMAL_PIXELS];
    for (int i = 0; i < THERMAL_PIXELS; i++) px[i] = 25.0f;

    float mn = 0, mx = 0, avg = 0;
    thermal_stats(px, &mn, &mx, &avg);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, mn);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, mx);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, avg);

    /* Ponteiros NULL sao aceitos. */
    thermal_stats(px, NULL, NULL, &avg);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, avg);
}

/* --- thermal_select_leaf ---------------------------------------------------- */

void test_thermal_select_both_ok_prefers_mlx(void)
{
    thermal_t th;
    make_ir_ok(&th);

    TEST_ASSERT_EQUAL_INT(LEAF_SRC_MLX, thermal_select_leaf(&th));
    TEST_ASSERT_EQUAL_INT(LEAF_SRC_MLX, th.fonte);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.83f, th.t_folha_c);
}

void test_thermal_select_mlx_ok_amg_fail(void)
{
    thermal_t th;
    make_ir_ok(&th);
    th.amg_ok = false;

    TEST_ASSERT_EQUAL_INT(LEAF_SRC_MLX, thermal_select_leaf(&th));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.83f, th.t_folha_c);
}

void test_thermal_select_mlx_fail_falls_back_to_amg_avg(void)
{
    thermal_t th;
    make_ir_ok(&th);
    th.mlx_ok = false;

    TEST_ASSERT_EQUAL_INT(LEAF_SRC_AMG, thermal_select_leaf(&th));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.00f, th.t_folha_c);
}

void test_thermal_select_both_fail_none_and_nan(void)
{
    thermal_t th;
    make_ir_ok(&th);
    th.mlx_ok = false;
    th.amg_ok = false;

    TEST_ASSERT_EQUAL_INT(LEAF_SRC_NONE, thermal_select_leaf(&th));
    TEST_ASSERT_TRUE(isnan(th.t_folha_c));
}

void test_thermal_select_mlx_implausible_falls_back(void)
{
    thermal_t th;
    make_ir_ok(&th);

    th.mlx_tobj_c = 382.19f;   /* fundo de escala: lente tampada / objeto quente */
    TEST_ASSERT_EQUAL_INT(LEAF_SRC_AMG, thermal_select_leaf(&th));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.00f, th.t_folha_c);

    th.mlx_tobj_c = -273.15f;  /* raw 0 */
    TEST_ASSERT_EQUAL_INT(LEAF_SRC_AMG, thermal_select_leaf(&th));

    th.mlx_tobj_c = NAN;
    TEST_ASSERT_EQUAL_INT(LEAF_SRC_AMG, thermal_select_leaf(&th));

    /* AMG tambem implausivel -> NONE */
    th.amg_avg_c = 250.0f;
    TEST_ASSERT_EQUAL_INT(LEAF_SRC_NONE, thermal_select_leaf(&th));
}

/* --- thermal_source_str / thermal_reset ------------------------------------ */

void test_thermal_source_str_values(void)
{
    TEST_ASSERT_EQUAL_STRING("mlx",  thermal_source_str(LEAF_SRC_MLX));
    TEST_ASSERT_EQUAL_STRING("amg",  thermal_source_str(LEAF_SRC_AMG));
    TEST_ASSERT_EQUAL_STRING("none", thermal_source_str(LEAF_SRC_NONE));
    TEST_ASSERT_EQUAL_STRING("none", thermal_source_str((leaf_source_t)99));
}

void test_thermal_reset_clears_flags(void)
{
    thermal_t th;
    make_ir_ok(&th);
    thermal_select_leaf(&th);
    TEST_ASSERT_TRUE(th.mlx_ok);

    thermal_reset(&th);
    TEST_ASSERT_FALSE(th.mlx_ok);
    TEST_ASSERT_FALSE(th.amg_ok);
    TEST_ASSERT_EQUAL_INT(LEAF_SRC_NONE, th.fonte);
    TEST_ASSERT_TRUE(isnan(th.t_folha_c));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, th.amg_px[63]);

    thermal_reset(NULL); /* nao deve travar */
}
