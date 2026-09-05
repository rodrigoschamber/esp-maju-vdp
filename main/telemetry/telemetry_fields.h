/*
 * Formatacao comum dos payloads de telemetria (ThingSpeak e MQTT).
 *
 * Mapeamento dos campos:
 *   field1 = T ar (C)          field5 = T folha usada (C)
 *   field2 = UR (%)            field6 = AMG8833 minimo (C)
 *   field3 = VPD ar (kPa)      field7 = AMG8833 media (C)
 *   field4 = VPD folha (kPa)   field8 = AMG8833 maximo (C)
 *
 * Campos sem valor valido sao omitidos (ThingSpeak rejeita "nan").
 */
#pragma once

#include <stddef.h>
#include "telemetry.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Escreve "field1=..&field2=..&...&field8=.." (sem api_key nem src).
 *
 * @return Tamanho escrito (sem o terminador) ou -1 se nao couber em buf.
 */
int telemetry_format_fields(char *buf, size_t size, const maju_reading_t *r);

/**
 * @brief Escreve o frame termico do AMG8833 em JSON:
 *        {"ts_ms":..,"src":"mlx","t_leaf":..,"mlx_tobj":..,"mlx_ta":..,"therm":..,
 *         "min":..,"avg":..,"max":..,"px":[64 valores]}
 *        Valores indisponiveis saem como null.
 *
 * @return Tamanho escrito (sem o terminador) ou -1 se nao couber em buf.
 */
int telemetry_format_thermal_json(char *buf, size_t size, const maju_reading_t *r);

#ifdef __cplusplus
}
#endif
