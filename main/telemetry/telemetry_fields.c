/*
 * Formatacao comum dos payloads de telemetria.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>

#include "telemetry_fields.h"

/* Acrescenta texto formatado em buf[len..]; devolve false se nao couber. */
static bool append(char *buf, size_t size, int *len, const char *fmt, ...)
{
    if (*len < 0 || (size_t)*len >= size) {
        return false;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *len, size - (size_t)*len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= size - (size_t)*len) {
        return false;
    }
    *len += n;
    return true;
}

static bool append_float_or_null(char *buf, size_t size, int *len, bool ok, float v)
{
    if (ok && isfinite(v)) {
        return append(buf, size, len, "%.2f", v);
    }
    return append(buf, size, len, "null");
}

int telemetry_format_fields(char *buf, size_t size, const maju_reading_t *r)
{
    if (buf == NULL || size == 0 || r == NULL) {
        return -1;
    }
    buf[0] = '\0';
    int len = 0;

    if (!append(buf, size, &len, "field1=%.2f&field2=%.2f&field3=%.3f&field4=%.3f",
                r->t_ar, r->rh, r->v.vpd_ar, r->v.vpd_folha)) {
        return -1;
    }

    if (isfinite(r->v.t_folha_c)) {
        if (!append(buf, size, &len, "&field5=%.2f", r->v.t_folha_c)) {
            return -1;
        }
    }

    if (r->th.amg_ok) {
        if (!append(buf, size, &len, "&field6=%.2f&field7=%.2f&field8=%.2f",
                    r->th.amg_min_c, r->th.amg_avg_c, r->th.amg_max_c)) {
            return -1;
        }
    }

    return len;
}

int telemetry_format_thermal_json(char *buf, size_t size, const maju_reading_t *r)
{
    if (buf == NULL || size == 0 || r == NULL) {
        return -1;
    }
    buf[0] = '\0';
    int len = 0;
    const thermal_t *th = &r->th;

    if (!append(buf, size, &len, "{\"ts_ms\":%lu,\"src\":\"%s\",\"t_leaf\":",
                (unsigned long)r->ts_ms, thermal_source_str(th->fonte))) {
        return -1;
    }
    if (!append_float_or_null(buf, size, &len, th->fonte != LEAF_SRC_NONE, th->t_folha_c)) {
        return -1;
    }

    if (!append(buf, size, &len, ",\"mlx_tobj\":")) return -1;
    if (!append_float_or_null(buf, size, &len, th->mlx_ok, th->mlx_tobj_c)) return -1;
    if (!append(buf, size, &len, ",\"mlx_ta\":")) return -1;
    if (!append_float_or_null(buf, size, &len, th->mlx_ok, th->mlx_ta_c)) return -1;

    if (!append(buf, size, &len, ",\"therm\":")) return -1;
    if (!append_float_or_null(buf, size, &len, th->amg_ok, th->amg_therm_c)) return -1;
    if (!append(buf, size, &len, ",\"min\":")) return -1;
    if (!append_float_or_null(buf, size, &len, th->amg_ok, th->amg_min_c)) return -1;
    if (!append(buf, size, &len, ",\"avg\":")) return -1;
    if (!append_float_or_null(buf, size, &len, th->amg_ok, th->amg_avg_c)) return -1;
    if (!append(buf, size, &len, ",\"max\":")) return -1;
    if (!append_float_or_null(buf, size, &len, th->amg_ok, th->amg_max_c)) return -1;

    if (!append(buf, size, &len, ",\"px\":[")) return -1;
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        if (!append(buf, size, &len, i == 0 ? "%.2f" : ",%.2f", th->amg_px[i])) {
            return -1;
        }
    }
    if (!append(buf, size, &len, "]}")) return -1;

    return len;
}
