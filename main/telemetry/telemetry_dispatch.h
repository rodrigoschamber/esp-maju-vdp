#pragma once

#include "telemetry.h"
#include "vpd.h"
#include "esp_err.h"

/*
 * Inicializa um dispatch de telemetria com independência de timing:
 * cria uma fila e uma task FreeRTOS dedicada para cada backend do array
 * terminado em NULL, e chama backend->init() para cada um.
 */
esp_err_t telemetry_dispatch_init(const telemetry_backend_t *const *backends);

/* Enfileira a leitura em todas as filas de backend sem bloquear. */
void telemetry_dispatch_send(float t, float rh, const vpd_result_t *v);

/* Drena as filas chamando backend->send() para cada item pendente (uso em testes). */
void telemetry_dispatch_process_pending(void);

/* Para todas as tasks, esvazia as filas e chama backend->deinit(). */
void telemetry_dispatch_deinit(void);
