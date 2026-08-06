#pragma once
#include "FreeRTOS.h"

/* No-op: testes de host nao esperam por hardware. */
static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
