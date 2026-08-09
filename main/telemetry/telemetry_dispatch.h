#pragma once

#include "telemetry.h"
#include "vpd.h"
#include "esp_err.h"

esp_err_t telemetry_dispatch_init(const telemetry_backend_t *const *backends);

void telemetry_dispatch_send(float t, float rh, const vpd_result_t *v);

void telemetry_dispatch_process_pending(void);

void telemetry_dispatch_deinit(void);
