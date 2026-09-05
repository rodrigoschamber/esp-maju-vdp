/*
 * Leituras infravermelhas e escolha da temperatura da folha.
 */
#include <math.h>
#include <string.h>
#include "thermal.h"

static bool plausible(float c)
{
    return isfinite(c) && c >= THERMAL_LEAF_MIN_C && c <= THERMAL_LEAF_MAX_C;
}

void thermal_reset(thermal_t *th)
{
    if (th == NULL) {
        return;
    }
    memset(th, 0, sizeof(*th));
    th->fonte     = LEAF_SRC_NONE;
    th->t_folha_c = NAN;
}

void thermal_stats(const float px[THERMAL_PIXELS], float *min_c, float *max_c, float *avg_c)
{
    float mn = px[0];
    float mx = px[0];
    float sum = 0.0f;

    for (int i = 0; i < THERMAL_PIXELS; i++) {
        if (px[i] < mn) mn = px[i];
        if (px[i] > mx) mx = px[i];
        sum += px[i];
    }

    if (min_c) *min_c = mn;
    if (max_c) *max_c = mx;
    if (avg_c) *avg_c = sum / (float)THERMAL_PIXELS;
}

leaf_source_t thermal_select_leaf(thermal_t *th)
{
    if (th->mlx_ok && plausible(th->mlx_tobj_c)) {
        th->fonte     = LEAF_SRC_MLX;
        th->t_folha_c = th->mlx_tobj_c;
    } else if (th->amg_ok && plausible(th->amg_avg_c)) {
        th->fonte     = LEAF_SRC_AMG;
        th->t_folha_c = th->amg_avg_c;
    } else {
        th->fonte     = LEAF_SRC_NONE;
        th->t_folha_c = NAN;
    }
    return th->fonte;
}

const char *thermal_source_str(leaf_source_t src)
{
    switch (src) {
    case LEAF_SRC_MLX: return "mlx";
    case LEAF_SRC_AMG: return "amg";
    default:           return "none";
    }
}
