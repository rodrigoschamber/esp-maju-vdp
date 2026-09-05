#pragma once

#include "telemetry.h"
#include "esp_err.h"

esp_err_t telemetry_dispatch_init(const telemetry_backend_t *const *backends);

void telemetry_dispatch_send(const maju_reading_t *r);

void telemetry_dispatch_process_pending(void);

void telemetry_dispatch_deinit(void);
