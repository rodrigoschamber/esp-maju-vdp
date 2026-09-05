/*
 * Fixtures compartilhadas pelos testes de telemetria (maju_reading_t).
 */
#pragma once

#include <string.h>
#include "telemetry.h"

/* Leitura "classica": so SHT35 + VPD. th zerado (mlx_ok = amg_ok = false, fonte NONE). */
static inline maju_reading_t make_reading(float t, float rh, float vpd_ar, float vpd_folha)
{
    maju_reading_t r;
    memset(&r, 0, sizeof(r));
    r.t_ar        = t;
    r.rh          = rh;
    r.v.vpd_ar    = vpd_ar;
    r.v.vpd_folha = vpd_folha;
    r.v.t_folha_c = t - 2.0f;
    r.th.fonte    = LEAF_SRC_NONE;
    return r;
}

/* Frame de referencia: px[i] = 21.10 + (i % 8) * 0.542857 -> min 21.10, max 24.90, media 23.00 */
static inline void fixture_fill_frame(float px[THERMAL_PIXELS])
{
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        px[i] = 21.10f + (float)(i % 8) * 0.542857f;
    }
}

/* Leitura completa com MLX90614 e AMG8833 validos; fonte MLX, t_folha 22.83. */
static inline maju_reading_t make_reading_ir(void)
{
    maju_reading_t r = make_reading(25.0f, 60.0f, 1.234f, 1.567f);
    r.ts_ms = 123456;

    r.th.mlx_ok      = true;
    r.th.mlx_tobj_c  = 22.83f;
    r.th.mlx_ta_c    = 24.90f;
    r.th.amg_ok      = true;
    r.th.amg_therm_c = 25.10f;
    fixture_fill_frame(r.th.amg_px);
    thermal_stats(r.th.amg_px, &r.th.amg_min_c, &r.th.amg_max_c, &r.th.amg_avg_c);
    thermal_select_leaf(&r.th);

    r.v.t_folha_c = r.th.t_folha_c;
    return r;
}
