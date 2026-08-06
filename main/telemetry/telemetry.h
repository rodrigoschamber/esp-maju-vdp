#pragma once

#include "esp_err.h"
#include "vpd.h"

/* Backend de telemetria — substitua a implementacao sem tocar no app_main. */
typedef struct {
    esp_err_t (*init)(void);
    void      (*send)(float t, float rh, const vpd_result_t *v);
    void      (*deinit)(void);
} telemetry_backend_t;
