#pragma once
#include "esp_err.h"

/* Versoes minimas para compilacao no host — sem logging de falha.
 * (void)(tag) evita o aviso de TAG nao utilizado nos drivers. */
#define ESP_RETURN_ON_FALSE(cond, err, tag, msg, ...) \
    do { (void)(tag); if (!(cond)) return (err); } while (0)

#define ESP_RETURN_ON_ERROR(expr, tag, msg, ...) \
    do { (void)(tag); esp_err_t _ret = (expr); if (_ret != ESP_OK) return _ret; } while (0)
