#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "vpd.h"
#include "thermal.h"

/** Leitura completa de um ciclo, entregue a cada backend de telemetria. */
typedef struct {
    uint32_t     ts_ms;   /*!< Instante da leitura (ms desde o boot) */
    float        t_ar;    /*!< Temperatura do ar (SHT35), C */
    float        rh;      /*!< Umidade relativa (SHT35), % */
    vpd_result_t v;       /*!< VPD do ar e da folha */
    thermal_t    th;      /*!< Leituras infravermelhas (MLX90614 / AMG8833) */
} maju_reading_t;

typedef struct {
    esp_err_t (*init)(void);
    void      (*send)(const maju_reading_t *r);
    void      (*deinit)(void);
} telemetry_backend_t;
